

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
#include "libvex_emnote.h"
#include "libvex_guest_ppc32.h"
#include "libvex_guest_ppc64.h"

#include "main_util.h"
#include "main_globals.h"
#include "guest_generic_bb_to_IR.h"
#include "guest_ppc_defs.h"



static VexEndness host_endness;

static const UChar* guest_code;

static Addr64 guest_CIA_bbstart;

static Addr64 guest_CIA_curr_instr;

static IRSB* irsb;

static Bool mode64 = False;

static void* fnptr_to_fnentry( const VexAbiInfo* vbi, void* f )
{
   if (vbi->host_ppc_calls_use_fndescrs) {
      HWord* fdescr = (HWord*)f;
      return (void*)(fdescr[0]);
   } else {
      
      return f;
   }
}

#define SIGN_BIT  0x8000000000000000ULL
#define SIGN_MASK 0x7fffffffffffffffULL
#define SIGN_BIT32  0x80000000
#define SIGN_MASK32 0x7fffffff



#define DIP(format, args...)           \
   if (vex_traceflags & VEX_TRACE_FE)  \
      vex_printf(format, ## args)

#define DIS(buf, format, args...)      \
   if (vex_traceflags & VEX_TRACE_FE)  \
      vex_sprintf(buf, format, ## args)



#define offsetofPPCGuestState(_x) \
   (mode64 ? offsetof(VexGuestPPC64State, _x) : \
             offsetof(VexGuestPPC32State, _x))

#define OFFB_CIA         offsetofPPCGuestState(guest_CIA)
#define OFFB_IP_AT_SYSCALL offsetofPPCGuestState(guest_IP_AT_SYSCALL)
#define OFFB_SPRG3_RO    offsetofPPCGuestState(guest_SPRG3_RO)
#define OFFB_LR          offsetofPPCGuestState(guest_LR)
#define OFFB_CTR         offsetofPPCGuestState(guest_CTR)
#define OFFB_XER_SO      offsetofPPCGuestState(guest_XER_SO)
#define OFFB_XER_OV      offsetofPPCGuestState(guest_XER_OV)
#define OFFB_XER_CA      offsetofPPCGuestState(guest_XER_CA)
#define OFFB_XER_BC      offsetofPPCGuestState(guest_XER_BC)
#define OFFB_FPROUND     offsetofPPCGuestState(guest_FPROUND)
#define OFFB_DFPROUND    offsetofPPCGuestState(guest_DFPROUND)
#define OFFB_VRSAVE      offsetofPPCGuestState(guest_VRSAVE)
#define OFFB_VSCR        offsetofPPCGuestState(guest_VSCR)
#define OFFB_EMNOTE      offsetofPPCGuestState(guest_EMNOTE)
#define OFFB_CMSTART     offsetofPPCGuestState(guest_CMSTART)
#define OFFB_CMLEN       offsetofPPCGuestState(guest_CMLEN)
#define OFFB_NRADDR      offsetofPPCGuestState(guest_NRADDR)
#define OFFB_NRADDR_GPR2 offsetofPPCGuestState(guest_NRADDR_GPR2)
#define OFFB_TFHAR       offsetofPPCGuestState(guest_TFHAR)
#define OFFB_TEXASR      offsetofPPCGuestState(guest_TEXASR)
#define OFFB_TEXASRU     offsetofPPCGuestState(guest_TEXASRU)
#define OFFB_TFIAR       offsetofPPCGuestState(guest_TFIAR)



#define IFIELD( insn, idx, len ) ((insn >> idx) & ((1<<len)-1))

static UChar ifieldOPC( UInt instr ) {
   return toUChar( IFIELD( instr, 26, 6 ) );
}

static UInt ifieldOPClo10 ( UInt instr) {
   return IFIELD( instr, 1, 10 );
}

static UInt ifieldOPClo9 ( UInt instr) {
   return IFIELD( instr, 1, 9 );
}

static UInt ifieldOPClo8 ( UInt instr) {
   return IFIELD( instr, 1, 8 );
}

static UInt ifieldOPClo5 ( UInt instr) {
   return IFIELD( instr, 1, 5 );
}

static UChar ifieldRegDS( UInt instr ) {
   return toUChar( IFIELD( instr, 21, 5 ) );
}

static UChar ifieldRegXT ( UInt instr )
{
  UChar upper_bit = toUChar (IFIELD (instr, 0, 1));
  UChar lower_bits = toUChar (IFIELD (instr, 21, 5));
  return (upper_bit << 5) | lower_bits;
}

static inline UChar ifieldRegXS ( UInt instr )
{
  return ifieldRegXT ( instr );
}

static UChar ifieldRegA ( UInt instr ) {
   return toUChar( IFIELD( instr, 16, 5 ) );
}

static UChar ifieldRegXA ( UInt instr )
{
  UChar upper_bit = toUChar (IFIELD (instr, 2, 1));
  UChar lower_bits = toUChar (IFIELD (instr, 16, 5));
  return (upper_bit << 5) | lower_bits;
}

static UChar ifieldRegB ( UInt instr ) {
   return toUChar( IFIELD( instr, 11, 5 ) );
}

static UChar ifieldRegXB ( UInt instr )
{
  UChar upper_bit = toUChar (IFIELD (instr, 1, 1));
  UChar lower_bits = toUChar (IFIELD (instr, 11, 5));
  return (upper_bit << 5) | lower_bits;
}

static UChar ifieldRegC ( UInt instr ) {
   return toUChar( IFIELD( instr, 6, 5 ) );
}

static UChar ifieldRegXC ( UInt instr )
{
  UChar upper_bit = toUChar (IFIELD (instr, 3, 1));
  UChar lower_bits = toUChar (IFIELD (instr, 6, 5));
  return (upper_bit << 5) | lower_bits;
}

static UChar ifieldBIT10 ( UInt instr ) {
   return toUChar( IFIELD( instr, 10, 1 ) );
}

static UChar ifieldBIT1 ( UInt instr ) {
   return toUChar( IFIELD( instr, 1, 1 ) );
}

static UChar ifieldBIT0 ( UInt instr ) {
   return toUChar( instr & 0x1 );
}

static UInt ifieldUIMM16 ( UInt instr ) {
   return instr & 0xFFFF;
}

static UInt ifieldUIMM26 ( UInt instr ) {
   return instr & 0x3FFFFFF;
}

static UChar ifieldDM ( UInt instr ) {
   return toUChar( IFIELD( instr, 8, 2 ) );
}

static inline UChar ifieldSHW ( UInt instr )
{
  return ifieldDM ( instr );
}


typedef enum {
    PPC_GST_CIA,    
    PPC_GST_LR,     
    PPC_GST_CTR,    
    PPC_GST_XER,    
    PPC_GST_CR,     
    PPC_GST_FPSCR,  
    PPC_GST_VRSAVE, 
    PPC_GST_VSCR,   
    PPC_GST_EMWARN, 
    PPC_GST_CMSTART,
    PPC_GST_CMLEN,  
    PPC_GST_IP_AT_SYSCALL, 
    PPC_GST_SPRG3_RO, 
    PPC_GST_TFHAR,  
    PPC_GST_TFIAR,  
    PPC_GST_TEXASR, 
    PPC_GST_TEXASRU, 
    PPC_GST_MAX
} PPC_GST;

#define MASK_FPSCR_RN   0x3ULL  
#define MASK_FPSCR_DRN  0x700000000ULL 
#define MASK_VSCR_VALID 0x00010001



static UInt float_to_bits ( Float f )
{
   union { UInt i; Float f; } u;
   vassert(4 == sizeof(UInt));
   vassert(4 == sizeof(Float));
   vassert(4 == sizeof(u));
   u.f = f;
   return u.i;
}



static UInt MASK32( UInt begin, UInt end )
{
   UInt m1, m2, mask;
   vassert(begin < 32);
   vassert(end < 32);
   m1   = ((UInt)(-1)) << begin;
   m2   = ((UInt)(-1)) << end << 1;
   mask = m1 ^ m2;
   if (begin > end) mask = ~mask;  
   return mask;
}

static ULong MASK64( UInt begin, UInt end )
{
   ULong m1, m2, mask;
   vassert(begin < 64);
   vassert(end < 64);
   m1   = ((ULong)(-1)) << begin;
   m2   = ((ULong)(-1)) << end << 1;
   mask = m1 ^ m2;
   if (begin > end) mask = ~mask;  
   return mask;
}

static Addr64 nextInsnAddr( void )
{
   return guest_CIA_curr_instr + 4;
}



static void stmt ( IRStmt* st )
{
   addStmtToIRSB( irsb, st );
}

static IRTemp newTemp ( IRType ty )
{
   vassert(isPlausibleIRType(ty));
   return newIRTemp( irsb->tyenv, ty );
}


static UChar extend_s_5to8 ( UChar x )
{
   return toUChar((((Int)x) << 27) >> 27);
}

static UInt extend_s_8to32( UChar x )
{
   return (UInt)((((Int)x) << 24) >> 24);
}

static UInt extend_s_16to32 ( UInt x )
{
   return (UInt)((((Int)x) << 16) >> 16);
}

static ULong extend_s_16to64 ( UInt x )
{
   return (ULong)((((Long)x) << 48) >> 48);
}

static ULong extend_s_26to64 ( UInt x )
{
   return (ULong)((((Long)x) << 38) >> 38);
}

static ULong extend_s_32to64 ( UInt x )
{
   return (ULong)((((Long)x) << 32) >> 32);
}

static UInt getUIntPPCendianly ( const UChar* p )
{
   UInt w = 0;
   if (host_endness == VexEndnessBE) {
       w = (w << 8) | p[0];
       w = (w << 8) | p[1];
       w = (w << 8) | p[2];
       w = (w << 8) | p[3];
   } else {
       w = (w << 8) | p[3];
       w = (w << 8) | p[2];
       w = (w << 8) | p[1];
       w = (w << 8) | p[0];
   }
   return w;
}



static void assign ( IRTemp dst, IRExpr* e )
{
   stmt( IRStmt_WrTmp(dst, e) );
}

static void store ( IRExpr* addr, IRExpr* data )
{
   IRType tyA = typeOfIRExpr(irsb->tyenv, addr);
   vassert(tyA == Ity_I32 || tyA == Ity_I64);

   if (host_endness == VexEndnessBE)
      stmt( IRStmt_Store(Iend_BE, addr, data) );
   else
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

static IRExpr* qop ( IROp op, IRExpr* a1, IRExpr* a2, 
                              IRExpr* a3, IRExpr* a4 )
{
   return IRExpr_Qop(op, a1, a2, a3, a4);
}

static IRExpr* mkexpr ( IRTemp tmp )
{
   return IRExpr_RdTmp(tmp);
}

static IRExpr* mkU8 ( UChar i )
{
   return IRExpr_Const(IRConst_U8(i));
}

static IRExpr* mkU16 ( UInt i )
{
   return IRExpr_Const(IRConst_U16(i));
}

static IRExpr* mkU32 ( UInt i )
{
   return IRExpr_Const(IRConst_U32(i));
}

static IRExpr* mkU64 ( ULong i )
{
   return IRExpr_Const(IRConst_U64(i));
}

static IRExpr* mkV128 ( UShort i )
{
   vassert(i == 0 || i == 0xffff);
   return IRExpr_Const(IRConst_V128(i));
}

static IRExpr* load ( IRType ty, IRExpr* addr )
{
   if (host_endness == VexEndnessBE)
      return IRExpr_Load(Iend_BE, ty, addr);
   else
      return IRExpr_Load(Iend_LE, ty, addr);
}

static IRStmt* stmt_load ( IRTemp result,
                           IRExpr* addr, IRExpr* storedata )
{
   if (host_endness == VexEndnessBE)
      return IRStmt_LLSC(Iend_BE, result, addr, storedata);
   else
      return IRStmt_LLSC(Iend_LE, result, addr, storedata);
}

static IRExpr* mkOR1 ( IRExpr* arg1, IRExpr* arg2 )
{
   vassert(typeOfIRExpr(irsb->tyenv, arg1) == Ity_I1);
   vassert(typeOfIRExpr(irsb->tyenv, arg2) == Ity_I1);
   return unop(Iop_32to1, binop(Iop_Or32, unop(Iop_1Uto32, arg1), 
                                          unop(Iop_1Uto32, arg2)));
}

static IRExpr* mkAND1 ( IRExpr* arg1, IRExpr* arg2 )
{
   vassert(typeOfIRExpr(irsb->tyenv, arg1) == Ity_I1);
   vassert(typeOfIRExpr(irsb->tyenv, arg2) == Ity_I1);
   return unop(Iop_32to1, binop(Iop_And32, unop(Iop_1Uto32, arg1), 
                                           unop(Iop_1Uto32, arg2)));
}

static void expand8Ux16( IRExpr* vIn,
                          IRTemp* vEvn, IRTemp* vOdd )
{
   IRTemp ones8x16 = newTemp(Ity_V128);

   vassert(typeOfIRExpr(irsb->tyenv, vIn) == Ity_V128);
   vassert(vEvn && *vEvn == IRTemp_INVALID);
   vassert(vOdd && *vOdd == IRTemp_INVALID);
   *vEvn = newTemp(Ity_V128);
   *vOdd = newTemp(Ity_V128);

   assign( ones8x16, unop(Iop_Dup8x16, mkU8(0x1)) );
   assign( *vOdd, binop(Iop_MullEven8Ux16, mkexpr(ones8x16), vIn) );
   assign( *vEvn, binop(Iop_MullEven8Ux16, mkexpr(ones8x16), 
                        binop(Iop_ShrV128, vIn, mkU8(8))) );
}

static void expand8Sx16( IRExpr* vIn,
                          IRTemp* vEvn, IRTemp* vOdd )
{
   IRTemp ones8x16 = newTemp(Ity_V128);

   vassert(typeOfIRExpr(irsb->tyenv, vIn) == Ity_V128);
   vassert(vEvn && *vEvn == IRTemp_INVALID);
   vassert(vOdd && *vOdd == IRTemp_INVALID);
   *vEvn = newTemp(Ity_V128);
   *vOdd = newTemp(Ity_V128);

   assign( ones8x16, unop(Iop_Dup8x16, mkU8(0x1)) );
   assign( *vOdd, binop(Iop_MullEven8Sx16, mkexpr(ones8x16), vIn) );
   assign( *vEvn, binop(Iop_MullEven8Sx16, mkexpr(ones8x16), 
                        binop(Iop_ShrV128, vIn, mkU8(8))) );
}

static void expand16Ux8( IRExpr* vIn,
                          IRTemp* vEvn, IRTemp* vOdd )
{
   IRTemp ones16x8 = newTemp(Ity_V128);

   vassert(typeOfIRExpr(irsb->tyenv, vIn) == Ity_V128);
   vassert(vEvn && *vEvn == IRTemp_INVALID);
   vassert(vOdd && *vOdd == IRTemp_INVALID);
   *vEvn = newTemp(Ity_V128);
   *vOdd = newTemp(Ity_V128);

   assign( ones16x8, unop(Iop_Dup16x8, mkU16(0x1)) );
   assign( *vOdd, binop(Iop_MullEven16Ux8, mkexpr(ones16x8), vIn) );
   assign( *vEvn, binop(Iop_MullEven16Ux8, mkexpr(ones16x8), 
                        binop(Iop_ShrV128, vIn, mkU8(16))) );
}

static void expand16Sx8( IRExpr* vIn,
                          IRTemp* vEvn, IRTemp* vOdd )
{
   IRTemp ones16x8 = newTemp(Ity_V128);

   vassert(typeOfIRExpr(irsb->tyenv, vIn) == Ity_V128);
   vassert(vEvn && *vEvn == IRTemp_INVALID);
   vassert(vOdd && *vOdd == IRTemp_INVALID);
   *vEvn = newTemp(Ity_V128);
   *vOdd = newTemp(Ity_V128);

   assign( ones16x8, unop(Iop_Dup16x8, mkU16(0x1)) );
   assign( *vOdd, binop(Iop_MullEven16Sx8, mkexpr(ones16x8), vIn) );
   assign( *vEvn, binop(Iop_MullEven16Sx8, mkexpr(ones16x8), 
                       binop(Iop_ShrV128, vIn, mkU8(16))) );
}

static void breakV128to4xF64( IRExpr* t128,
                              
                              IRTemp* t3, IRTemp* t2,
                              IRTemp* t1, IRTemp* t0 )
{
   IRTemp hi64 = newTemp(Ity_I64);
   IRTemp lo64 = newTemp(Ity_I64);

   vassert(typeOfIRExpr(irsb->tyenv, t128) == Ity_V128);
   vassert(t0 && *t0 == IRTemp_INVALID);
   vassert(t1 && *t1 == IRTemp_INVALID);
   vassert(t2 && *t2 == IRTemp_INVALID);
   vassert(t3 && *t3 == IRTemp_INVALID);
   *t0 = newTemp(Ity_F64);
   *t1 = newTemp(Ity_F64);
   *t2 = newTemp(Ity_F64);
   *t3 = newTemp(Ity_F64);

   assign( hi64, unop(Iop_V128HIto64, t128) );
   assign( lo64, unop(Iop_V128to64,   t128) );
   assign( *t3,
           unop( Iop_F32toF64,
                 unop( Iop_ReinterpI32asF32,
                       unop( Iop_64HIto32, mkexpr( hi64 ) ) ) ) );
   assign( *t2,
           unop( Iop_F32toF64,
                 unop( Iop_ReinterpI32asF32, unop( Iop_64to32, mkexpr( hi64 ) ) ) ) );
   assign( *t1,
           unop( Iop_F32toF64,
                 unop( Iop_ReinterpI32asF32,
                       unop( Iop_64HIto32, mkexpr( lo64 ) ) ) ) );
   assign( *t0,
           unop( Iop_F32toF64,
                 unop( Iop_ReinterpI32asF32, unop( Iop_64to32, mkexpr( lo64 ) ) ) ) );
}


static void breakV128to4x64S( IRExpr* t128,
                              
                              IRTemp* t3, IRTemp* t2,
                              IRTemp* t1, IRTemp* t0 )
{
   IRTemp hi64 = newTemp(Ity_I64);
   IRTemp lo64 = newTemp(Ity_I64);

   vassert(typeOfIRExpr(irsb->tyenv, t128) == Ity_V128);
   vassert(t0 && *t0 == IRTemp_INVALID);
   vassert(t1 && *t1 == IRTemp_INVALID);
   vassert(t2 && *t2 == IRTemp_INVALID);
   vassert(t3 && *t3 == IRTemp_INVALID);
   *t0 = newTemp(Ity_I64);
   *t1 = newTemp(Ity_I64);
   *t2 = newTemp(Ity_I64);
   *t3 = newTemp(Ity_I64);

   assign( hi64, unop(Iop_V128HIto64, t128) );
   assign( lo64, unop(Iop_V128to64,   t128) );
   assign( *t3, unop(Iop_32Sto64, unop(Iop_64HIto32, mkexpr(hi64))) );
   assign( *t2, unop(Iop_32Sto64, unop(Iop_64to32,   mkexpr(hi64))) );
   assign( *t1, unop(Iop_32Sto64, unop(Iop_64HIto32, mkexpr(lo64))) );
   assign( *t0, unop(Iop_32Sto64, unop(Iop_64to32,   mkexpr(lo64))) );
}

static void breakV128to4x64U ( IRExpr* t128,
                               
                               IRTemp* t3, IRTemp* t2,
                               IRTemp* t1, IRTemp* t0 )
{
   IRTemp hi64 = newTemp(Ity_I64);
   IRTemp lo64 = newTemp(Ity_I64);

   vassert(typeOfIRExpr(irsb->tyenv, t128) == Ity_V128);
   vassert(t0 && *t0 == IRTemp_INVALID);
   vassert(t1 && *t1 == IRTemp_INVALID);
   vassert(t2 && *t2 == IRTemp_INVALID);
   vassert(t3 && *t3 == IRTemp_INVALID);
   *t0 = newTemp(Ity_I64);
   *t1 = newTemp(Ity_I64);
   *t2 = newTemp(Ity_I64);
   *t3 = newTemp(Ity_I64);

   assign( hi64, unop(Iop_V128HIto64, t128) );
   assign( lo64, unop(Iop_V128to64,   t128) );
   assign( *t3, unop(Iop_32Uto64, unop(Iop_64HIto32, mkexpr(hi64))) );
   assign( *t2, unop(Iop_32Uto64, unop(Iop_64to32,   mkexpr(hi64))) );
   assign( *t1, unop(Iop_32Uto64, unop(Iop_64HIto32, mkexpr(lo64))) );
   assign( *t0, unop(Iop_32Uto64, unop(Iop_64to32,   mkexpr(lo64))) );
}

static void breakV128to4x32( IRExpr* t128,
                              
                              IRTemp* t3, IRTemp* t2,
                              IRTemp* t1, IRTemp* t0 )
{
   IRTemp hi64 = newTemp(Ity_I64);
   IRTemp lo64 = newTemp(Ity_I64);

   vassert(typeOfIRExpr(irsb->tyenv, t128) == Ity_V128);
   vassert(t0 && *t0 == IRTemp_INVALID);
   vassert(t1 && *t1 == IRTemp_INVALID);
   vassert(t2 && *t2 == IRTemp_INVALID);
   vassert(t3 && *t3 == IRTemp_INVALID);
   *t0 = newTemp(Ity_I32);
   *t1 = newTemp(Ity_I32);
   *t2 = newTemp(Ity_I32);
   *t3 = newTemp(Ity_I32);

   assign( hi64, unop(Iop_V128HIto64, t128) );
   assign( lo64, unop(Iop_V128to64,   t128) );
   assign( *t3, unop(Iop_64HIto32, mkexpr(hi64)) );
   assign( *t2, unop(Iop_64to32,   mkexpr(hi64)) );
   assign( *t1, unop(Iop_64HIto32, mkexpr(lo64)) );
   assign( *t0, unop(Iop_64to32,   mkexpr(lo64)) );
}

static IRExpr* mkV128from32( IRTemp t3, IRTemp t2,
                               IRTemp t1, IRTemp t0 )
{
   return
      binop( Iop_64HLtoV128,
             binop(Iop_32HLto64, mkexpr(t3), mkexpr(t2)),
             binop(Iop_32HLto64, mkexpr(t1), mkexpr(t0))
   );
}


static IRExpr* mkQNarrow64Sto32 ( IRExpr* t64 )
{
   IRTemp hi32 = newTemp(Ity_I32);
   IRTemp lo32 = newTemp(Ity_I32);

   vassert(typeOfIRExpr(irsb->tyenv, t64) == Ity_I64);

   assign( hi32, unop(Iop_64HIto32, t64));
   assign( lo32, unop(Iop_64to32,   t64));

   return IRExpr_ITE(
             
             binop(Iop_CmpEQ32, mkexpr(hi32),
                   binop( Iop_Sar32, mkexpr(lo32), mkU8(31))),
             
             mkexpr(lo32),
             
             binop(Iop_Add32, mkU32(0x7FFFFFFF),
                   binop(Iop_Shr32, mkexpr(hi32), mkU8(31))));
}

static IRExpr* mkQNarrow64Uto32 ( IRExpr* t64 )
{
   IRTemp hi32 = newTemp(Ity_I32);
   IRTemp lo32 = newTemp(Ity_I32);

   vassert(typeOfIRExpr(irsb->tyenv, t64) == Ity_I64);

   assign( hi32, unop(Iop_64HIto32, t64));
   assign( lo32, unop(Iop_64to32,   t64));

   return IRExpr_ITE(
            
            binop(Iop_CmpEQ32, mkexpr(hi32), mkU32(0)),
            
            mkexpr(lo32),
            
            mkU32(0xFFFFFFFF));
}

static IRExpr* mkV128from4x64S ( IRExpr* t3, IRExpr* t2,
                                 IRExpr* t1, IRExpr* t0 )
{
   vassert(typeOfIRExpr(irsb->tyenv, t3) == Ity_I64);
   vassert(typeOfIRExpr(irsb->tyenv, t2) == Ity_I64);
   vassert(typeOfIRExpr(irsb->tyenv, t1) == Ity_I64);
   vassert(typeOfIRExpr(irsb->tyenv, t0) == Ity_I64);
   return binop(Iop_64HLtoV128,
                binop(Iop_32HLto64,
                      mkQNarrow64Sto32( t3 ),
                      mkQNarrow64Sto32( t2 )),
                binop(Iop_32HLto64,
                      mkQNarrow64Sto32( t1 ),
                      mkQNarrow64Sto32( t0 )));
}

static IRExpr* mkV128from4x64U ( IRExpr* t3, IRExpr* t2,
                                 IRExpr* t1, IRExpr* t0 )
{
   vassert(typeOfIRExpr(irsb->tyenv, t3) == Ity_I64);
   vassert(typeOfIRExpr(irsb->tyenv, t2) == Ity_I64);
   vassert(typeOfIRExpr(irsb->tyenv, t1) == Ity_I64);
   vassert(typeOfIRExpr(irsb->tyenv, t0) == Ity_I64);
   return binop(Iop_64HLtoV128,
                binop(Iop_32HLto64,
                      mkQNarrow64Uto32( t3 ),
                      mkQNarrow64Uto32( t2 )),
                binop(Iop_32HLto64,
                      mkQNarrow64Uto32( t1 ),
                      mkQNarrow64Uto32( t0 )));
}

#define MK_Iop_MullOdd8Ux16( expr_vA, expr_vB ) \
      binop(Iop_MullEven8Ux16, \
            binop(Iop_ShrV128, expr_vA, mkU8(8)), \
            binop(Iop_ShrV128, expr_vB, mkU8(8)))

#define MK_Iop_MullOdd8Sx16( expr_vA, expr_vB ) \
      binop(Iop_MullEven8Sx16, \
            binop(Iop_ShrV128, expr_vA, mkU8(8)), \
            binop(Iop_ShrV128, expr_vB, mkU8(8)))

#define MK_Iop_MullOdd16Ux8( expr_vA, expr_vB ) \
      binop(Iop_MullEven16Ux8, \
            binop(Iop_ShrV128, expr_vA, mkU8(16)), \
            binop(Iop_ShrV128, expr_vB, mkU8(16)))

#define MK_Iop_MullOdd32Ux4( expr_vA, expr_vB ) \
      binop(Iop_MullEven32Ux4, \
            binop(Iop_ShrV128, expr_vA, mkU8(32)), \
            binop(Iop_ShrV128, expr_vB, mkU8(32)))

#define MK_Iop_MullOdd16Sx8( expr_vA, expr_vB ) \
      binop(Iop_MullEven16Sx8, \
            binop(Iop_ShrV128, expr_vA, mkU8(16)), \
            binop(Iop_ShrV128, expr_vB, mkU8(16)))

#define MK_Iop_MullOdd32Sx4( expr_vA, expr_vB ) \
      binop(Iop_MullEven32Sx4, \
            binop(Iop_ShrV128, expr_vA, mkU8(32)), \
            binop(Iop_ShrV128, expr_vB, mkU8(32)))


static IRExpr*  mk64lo32Sto64 ( IRExpr* src )
{
   vassert(typeOfIRExpr(irsb->tyenv, src) == Ity_I64);
   return unop(Iop_32Sto64, unop(Iop_64to32, src));
}

static IRExpr*  mk64lo32Uto64 ( IRExpr* src )
{
   vassert(typeOfIRExpr(irsb->tyenv, src) == Ity_I64);
   return unop(Iop_32Uto64, unop(Iop_64to32, src));
}

static IROp mkSzOp ( IRType ty, IROp op8 )
{
   Int adj;
   vassert(ty == Ity_I8  || ty == Ity_I16 ||
           ty == Ity_I32 || ty == Ity_I64);
   vassert(op8 == Iop_Add8   || op8 == Iop_Sub8   || op8 == Iop_Mul8 ||
           op8 == Iop_Or8    || op8 == Iop_And8   || op8 == Iop_Xor8 ||
           op8 == Iop_Shl8   || op8 == Iop_Shr8   || op8 == Iop_Sar8 ||
           op8 == Iop_CmpEQ8 || op8 == Iop_CmpNE8 ||
           op8 == Iop_Not8 );
   adj = ty==Ity_I8 ? 0 : (ty==Ity_I16 ? 1 : (ty==Ity_I32 ? 2 : 3));
   return adj + op8;
}

static Addr64 mkSzAddr ( IRType ty, Addr64 addr )
{
   vassert(ty == Ity_I32 || ty == Ity_I64);
   return ( ty == Ity_I64 ?
            (Addr64)addr :
            (Addr64)extend_s_32to64( toUInt(addr) ) );
}

static IRExpr* mkSzImm ( IRType ty, ULong imm64 )
{
   vassert(ty == Ity_I32 || ty == Ity_I64);
   return ty == Ity_I64 ? mkU64(imm64) : mkU32((UInt)imm64);
}

static IRConst* mkSzConst ( IRType ty, ULong imm64 )
{
   vassert(ty == Ity_I32 || ty == Ity_I64);
   return ( ty == Ity_I64 ?
            IRConst_U64(imm64) :
            IRConst_U32((UInt)imm64) );
}

static IRExpr* mkSzExtendS16 ( IRType ty, UInt imm16 )
{
   vassert(ty == Ity_I32 || ty == Ity_I64);
   return ( ty == Ity_I64 ?
            mkU64(extend_s_16to64(imm16)) :
            mkU32(extend_s_16to32(imm16)) );
}

static IRExpr* mkSzExtendS32 ( IRType ty, UInt imm32 )
{
   vassert(ty == Ity_I32 || ty == Ity_I64);
   return ( ty == Ity_I64 ?
            mkU64(extend_s_32to64(imm32)) :
            mkU32(imm32) );
}

static IRExpr* mkNarrowTo8 ( IRType ty, IRExpr* src )
{
   vassert(ty == Ity_I32 || ty == Ity_I64);
   return ty == Ity_I64 ? unop(Iop_64to8, src) : unop(Iop_32to8, src);
}

static IRExpr* mkNarrowTo16 ( IRType ty, IRExpr* src )
{
   vassert(ty == Ity_I32 || ty == Ity_I64);
   return ty == Ity_I64 ? unop(Iop_64to16, src) : unop(Iop_32to16, src);
}

static IRExpr* mkNarrowTo32 ( IRType ty, IRExpr* src )
{
   vassert(ty == Ity_I32 || ty == Ity_I64);
   return ty == Ity_I64 ? unop(Iop_64to32, src) : src;
}

static IRExpr* mkWidenFrom8 ( IRType ty, IRExpr* src, Bool sined )
{
   IROp op;
   vassert(ty == Ity_I32 || ty == Ity_I64);
   if (sined) op = (ty==Ity_I32) ? Iop_8Sto32 : Iop_8Sto64;
   else       op = (ty==Ity_I32) ? Iop_8Uto32 : Iop_8Uto64;
   return unop(op, src);
}

static IRExpr* mkWidenFrom16 ( IRType ty, IRExpr* src, Bool sined )
{
   IROp op;
   vassert(ty == Ity_I32 || ty == Ity_I64);
   if (sined) op = (ty==Ity_I32) ? Iop_16Sto32 : Iop_16Sto64;
   else       op = (ty==Ity_I32) ? Iop_16Uto32 : Iop_16Uto64;
   return unop(op, src);
}

static IRExpr* mkWidenFrom32 ( IRType ty, IRExpr* src, Bool sined )
{
   vassert(ty == Ity_I32 || ty == Ity_I64);
   if (ty == Ity_I32)
      return src;
   return (sined) ? unop(Iop_32Sto64, src) : unop(Iop_32Uto64, src);
}


static Int integerGuestRegOffset ( UInt archreg )
{
   vassert(archreg < 32);
   
   
   
   

   switch (archreg) {
   case  0: return offsetofPPCGuestState(guest_GPR0);
   case  1: return offsetofPPCGuestState(guest_GPR1);
   case  2: return offsetofPPCGuestState(guest_GPR2);
   case  3: return offsetofPPCGuestState(guest_GPR3);
   case  4: return offsetofPPCGuestState(guest_GPR4);
   case  5: return offsetofPPCGuestState(guest_GPR5);
   case  6: return offsetofPPCGuestState(guest_GPR6);
   case  7: return offsetofPPCGuestState(guest_GPR7);
   case  8: return offsetofPPCGuestState(guest_GPR8);
   case  9: return offsetofPPCGuestState(guest_GPR9);
   case 10: return offsetofPPCGuestState(guest_GPR10);
   case 11: return offsetofPPCGuestState(guest_GPR11);
   case 12: return offsetofPPCGuestState(guest_GPR12);
   case 13: return offsetofPPCGuestState(guest_GPR13);
   case 14: return offsetofPPCGuestState(guest_GPR14);
   case 15: return offsetofPPCGuestState(guest_GPR15);
   case 16: return offsetofPPCGuestState(guest_GPR16);
   case 17: return offsetofPPCGuestState(guest_GPR17);
   case 18: return offsetofPPCGuestState(guest_GPR18);
   case 19: return offsetofPPCGuestState(guest_GPR19);
   case 20: return offsetofPPCGuestState(guest_GPR20);
   case 21: return offsetofPPCGuestState(guest_GPR21);
   case 22: return offsetofPPCGuestState(guest_GPR22);
   case 23: return offsetofPPCGuestState(guest_GPR23);
   case 24: return offsetofPPCGuestState(guest_GPR24);
   case 25: return offsetofPPCGuestState(guest_GPR25);
   case 26: return offsetofPPCGuestState(guest_GPR26);
   case 27: return offsetofPPCGuestState(guest_GPR27);
   case 28: return offsetofPPCGuestState(guest_GPR28);
   case 29: return offsetofPPCGuestState(guest_GPR29);
   case 30: return offsetofPPCGuestState(guest_GPR30);
   case 31: return offsetofPPCGuestState(guest_GPR31);
   default: break;
   }
   vpanic("integerGuestRegOffset(ppc,be)"); 
}

static IRExpr* getIReg ( UInt archreg )
{
   IRType ty = mode64 ? Ity_I64 : Ity_I32;
   vassert(archreg < 32);
   return IRExpr_Get( integerGuestRegOffset(archreg), ty );
}

static void putIReg ( UInt archreg, IRExpr* e )
{
   IRType ty = mode64 ? Ity_I64 : Ity_I32;
   vassert(archreg < 32);
   vassert(typeOfIRExpr(irsb->tyenv, e) == ty );
   stmt( IRStmt_Put(integerGuestRegOffset(archreg), e) );
}


static Int floatGuestRegOffset ( UInt archreg )
{
   vassert(archreg < 32);
   
   if (host_endness == VexEndnessLE) {
      switch (archreg) {
         case  0: return offsetofPPCGuestState(guest_VSR0) + 8;
         case  1: return offsetofPPCGuestState(guest_VSR1) + 8;
         case  2: return offsetofPPCGuestState(guest_VSR2) + 8;
         case  3: return offsetofPPCGuestState(guest_VSR3) + 8;
         case  4: return offsetofPPCGuestState(guest_VSR4) + 8;
         case  5: return offsetofPPCGuestState(guest_VSR5) + 8;
         case  6: return offsetofPPCGuestState(guest_VSR6) + 8;
         case  7: return offsetofPPCGuestState(guest_VSR7) + 8;
         case  8: return offsetofPPCGuestState(guest_VSR8) + 8;
         case  9: return offsetofPPCGuestState(guest_VSR9) + 8;
         case 10: return offsetofPPCGuestState(guest_VSR10) + 8;
         case 11: return offsetofPPCGuestState(guest_VSR11) + 8;
         case 12: return offsetofPPCGuestState(guest_VSR12) + 8;
         case 13: return offsetofPPCGuestState(guest_VSR13) + 8;
         case 14: return offsetofPPCGuestState(guest_VSR14) + 8;
         case 15: return offsetofPPCGuestState(guest_VSR15) + 8;
         case 16: return offsetofPPCGuestState(guest_VSR16) + 8;
         case 17: return offsetofPPCGuestState(guest_VSR17) + 8;
         case 18: return offsetofPPCGuestState(guest_VSR18) + 8;
         case 19: return offsetofPPCGuestState(guest_VSR19) + 8;
         case 20: return offsetofPPCGuestState(guest_VSR20) + 8;
         case 21: return offsetofPPCGuestState(guest_VSR21) + 8;
         case 22: return offsetofPPCGuestState(guest_VSR22) + 8;
         case 23: return offsetofPPCGuestState(guest_VSR23) + 8;
         case 24: return offsetofPPCGuestState(guest_VSR24) + 8;
         case 25: return offsetofPPCGuestState(guest_VSR25) + 8;
         case 26: return offsetofPPCGuestState(guest_VSR26) + 8;
         case 27: return offsetofPPCGuestState(guest_VSR27) + 8;
         case 28: return offsetofPPCGuestState(guest_VSR28) + 8;
         case 29: return offsetofPPCGuestState(guest_VSR29) + 8;
         case 30: return offsetofPPCGuestState(guest_VSR30) + 8;
         case 31: return offsetofPPCGuestState(guest_VSR31) + 8;
         default: break;
      }
   } else {
      switch (archreg) {
         case  0: return offsetofPPCGuestState(guest_VSR0);
         case  1: return offsetofPPCGuestState(guest_VSR1);
         case  2: return offsetofPPCGuestState(guest_VSR2);
         case  3: return offsetofPPCGuestState(guest_VSR3);
         case  4: return offsetofPPCGuestState(guest_VSR4);
         case  5: return offsetofPPCGuestState(guest_VSR5);
         case  6: return offsetofPPCGuestState(guest_VSR6);
         case  7: return offsetofPPCGuestState(guest_VSR7);
         case  8: return offsetofPPCGuestState(guest_VSR8);
         case  9: return offsetofPPCGuestState(guest_VSR9);
         case 10: return offsetofPPCGuestState(guest_VSR10);
         case 11: return offsetofPPCGuestState(guest_VSR11);
         case 12: return offsetofPPCGuestState(guest_VSR12);
         case 13: return offsetofPPCGuestState(guest_VSR13);
         case 14: return offsetofPPCGuestState(guest_VSR14);
         case 15: return offsetofPPCGuestState(guest_VSR15);
         case 16: return offsetofPPCGuestState(guest_VSR16);
         case 17: return offsetofPPCGuestState(guest_VSR17);
         case 18: return offsetofPPCGuestState(guest_VSR18);
         case 19: return offsetofPPCGuestState(guest_VSR19);
         case 20: return offsetofPPCGuestState(guest_VSR20);
         case 21: return offsetofPPCGuestState(guest_VSR21);
         case 22: return offsetofPPCGuestState(guest_VSR22);
         case 23: return offsetofPPCGuestState(guest_VSR23);
         case 24: return offsetofPPCGuestState(guest_VSR24);
         case 25: return offsetofPPCGuestState(guest_VSR25);
         case 26: return offsetofPPCGuestState(guest_VSR26);
         case 27: return offsetofPPCGuestState(guest_VSR27);
         case 28: return offsetofPPCGuestState(guest_VSR28);
         case 29: return offsetofPPCGuestState(guest_VSR29);
         case 30: return offsetofPPCGuestState(guest_VSR30);
         case 31: return offsetofPPCGuestState(guest_VSR31);
         default: break;
      }
   }
   vpanic("floatGuestRegOffset(ppc)"); 
}

static IRExpr* getFReg ( UInt archreg )
{
   vassert(archreg < 32);
   return IRExpr_Get( floatGuestRegOffset(archreg), Ity_F64 );
}

static void putFReg ( UInt archreg, IRExpr* e )
{
   vassert(archreg < 32);
   vassert(typeOfIRExpr(irsb->tyenv, e) == Ity_F64);
   stmt( IRStmt_Put(floatGuestRegOffset(archreg), e) );
}

static IRExpr* getDReg(UInt archreg) {
   IRExpr *e;
   vassert( archreg < 32 );
   e = IRExpr_Get( floatGuestRegOffset( archreg ), Ity_D64 );
   return e;
}
static IRExpr* getDReg32(UInt archreg) {
   IRExpr *e;
   vassert( archreg < 32 );
   e = IRExpr_Get( floatGuestRegOffset( archreg ), Ity_D32 );
   return e;
}

static IRExpr *getDReg_pair(UInt archreg) {
   IRExpr *high = getDReg( archreg );
   IRExpr *low = getDReg( archreg + 1 );

   return binop( Iop_D64HLtoD128, high, low );
}

static void putDReg32(UInt archreg, IRExpr* e) {
   vassert( archreg < 32 );
   vassert( typeOfIRExpr(irsb->tyenv, e) == Ity_D32 );
   stmt( IRStmt_Put( floatGuestRegOffset( archreg ), e ) );
}

static void putDReg(UInt archreg, IRExpr* e) {
   vassert( archreg < 32 );
   vassert( typeOfIRExpr(irsb->tyenv, e) == Ity_D64 );
   stmt( IRStmt_Put( floatGuestRegOffset( archreg ), e ) );
}

static void putDReg_pair(UInt archreg, IRExpr *e) {
   IRTemp low = newTemp( Ity_D64 );
   IRTemp high = newTemp( Ity_D64 );

   vassert( archreg < 32 );
   vassert( typeOfIRExpr(irsb->tyenv, e) == Ity_D128 );

   assign( low, unop( Iop_D128LOtoD64, e ) );
   assign( high, unop( Iop_D128HItoD64, e ) );

   stmt( IRStmt_Put( floatGuestRegOffset( archreg ), mkexpr( high ) ) );
   stmt( IRStmt_Put( floatGuestRegOffset( archreg + 1 ), mkexpr( low ) ) );
}

static Int vsxGuestRegOffset ( UInt archreg )
{
   vassert(archreg < 64);
   switch (archreg) {
   case  0: return offsetofPPCGuestState(guest_VSR0);
   case  1: return offsetofPPCGuestState(guest_VSR1);
   case  2: return offsetofPPCGuestState(guest_VSR2);
   case  3: return offsetofPPCGuestState(guest_VSR3);
   case  4: return offsetofPPCGuestState(guest_VSR4);
   case  5: return offsetofPPCGuestState(guest_VSR5);
   case  6: return offsetofPPCGuestState(guest_VSR6);
   case  7: return offsetofPPCGuestState(guest_VSR7);
   case  8: return offsetofPPCGuestState(guest_VSR8);
   case  9: return offsetofPPCGuestState(guest_VSR9);
   case 10: return offsetofPPCGuestState(guest_VSR10);
   case 11: return offsetofPPCGuestState(guest_VSR11);
   case 12: return offsetofPPCGuestState(guest_VSR12);
   case 13: return offsetofPPCGuestState(guest_VSR13);
   case 14: return offsetofPPCGuestState(guest_VSR14);
   case 15: return offsetofPPCGuestState(guest_VSR15);
   case 16: return offsetofPPCGuestState(guest_VSR16);
   case 17: return offsetofPPCGuestState(guest_VSR17);
   case 18: return offsetofPPCGuestState(guest_VSR18);
   case 19: return offsetofPPCGuestState(guest_VSR19);
   case 20: return offsetofPPCGuestState(guest_VSR20);
   case 21: return offsetofPPCGuestState(guest_VSR21);
   case 22: return offsetofPPCGuestState(guest_VSR22);
   case 23: return offsetofPPCGuestState(guest_VSR23);
   case 24: return offsetofPPCGuestState(guest_VSR24);
   case 25: return offsetofPPCGuestState(guest_VSR25);
   case 26: return offsetofPPCGuestState(guest_VSR26);
   case 27: return offsetofPPCGuestState(guest_VSR27);
   case 28: return offsetofPPCGuestState(guest_VSR28);
   case 29: return offsetofPPCGuestState(guest_VSR29);
   case 30: return offsetofPPCGuestState(guest_VSR30);
   case 31: return offsetofPPCGuestState(guest_VSR31);
   case 32: return offsetofPPCGuestState(guest_VSR32);
   case 33: return offsetofPPCGuestState(guest_VSR33);
   case 34: return offsetofPPCGuestState(guest_VSR34);
   case 35: return offsetofPPCGuestState(guest_VSR35);
   case 36: return offsetofPPCGuestState(guest_VSR36);
   case 37: return offsetofPPCGuestState(guest_VSR37);
   case 38: return offsetofPPCGuestState(guest_VSR38);
   case 39: return offsetofPPCGuestState(guest_VSR39);
   case 40: return offsetofPPCGuestState(guest_VSR40);
   case 41: return offsetofPPCGuestState(guest_VSR41);
   case 42: return offsetofPPCGuestState(guest_VSR42);
   case 43: return offsetofPPCGuestState(guest_VSR43);
   case 44: return offsetofPPCGuestState(guest_VSR44);
   case 45: return offsetofPPCGuestState(guest_VSR45);
   case 46: return offsetofPPCGuestState(guest_VSR46);
   case 47: return offsetofPPCGuestState(guest_VSR47);
   case 48: return offsetofPPCGuestState(guest_VSR48);
   case 49: return offsetofPPCGuestState(guest_VSR49);
   case 50: return offsetofPPCGuestState(guest_VSR50);
   case 51: return offsetofPPCGuestState(guest_VSR51);
   case 52: return offsetofPPCGuestState(guest_VSR52);
   case 53: return offsetofPPCGuestState(guest_VSR53);
   case 54: return offsetofPPCGuestState(guest_VSR54);
   case 55: return offsetofPPCGuestState(guest_VSR55);
   case 56: return offsetofPPCGuestState(guest_VSR56);
   case 57: return offsetofPPCGuestState(guest_VSR57);
   case 58: return offsetofPPCGuestState(guest_VSR58);
   case 59: return offsetofPPCGuestState(guest_VSR59);
   case 60: return offsetofPPCGuestState(guest_VSR60);
   case 61: return offsetofPPCGuestState(guest_VSR61);
   case 62: return offsetofPPCGuestState(guest_VSR62);
   case 63: return offsetofPPCGuestState(guest_VSR63);
   default: break;
   }
   vpanic("vsxGuestRegOffset(ppc)"); 
}

static Int vectorGuestRegOffset ( UInt archreg )
{
   vassert(archreg < 32);
   
   switch (archreg) {
   case  0: return offsetofPPCGuestState(guest_VSR32);
   case  1: return offsetofPPCGuestState(guest_VSR33);
   case  2: return offsetofPPCGuestState(guest_VSR34);
   case  3: return offsetofPPCGuestState(guest_VSR35);
   case  4: return offsetofPPCGuestState(guest_VSR36);
   case  5: return offsetofPPCGuestState(guest_VSR37);
   case  6: return offsetofPPCGuestState(guest_VSR38);
   case  7: return offsetofPPCGuestState(guest_VSR39);
   case  8: return offsetofPPCGuestState(guest_VSR40);
   case  9: return offsetofPPCGuestState(guest_VSR41);
   case 10: return offsetofPPCGuestState(guest_VSR42);
   case 11: return offsetofPPCGuestState(guest_VSR43);
   case 12: return offsetofPPCGuestState(guest_VSR44);
   case 13: return offsetofPPCGuestState(guest_VSR45);
   case 14: return offsetofPPCGuestState(guest_VSR46);
   case 15: return offsetofPPCGuestState(guest_VSR47);
   case 16: return offsetofPPCGuestState(guest_VSR48);
   case 17: return offsetofPPCGuestState(guest_VSR49);
   case 18: return offsetofPPCGuestState(guest_VSR50);
   case 19: return offsetofPPCGuestState(guest_VSR51);
   case 20: return offsetofPPCGuestState(guest_VSR52);
   case 21: return offsetofPPCGuestState(guest_VSR53);
   case 22: return offsetofPPCGuestState(guest_VSR54);
   case 23: return offsetofPPCGuestState(guest_VSR55);
   case 24: return offsetofPPCGuestState(guest_VSR56);
   case 25: return offsetofPPCGuestState(guest_VSR57);
   case 26: return offsetofPPCGuestState(guest_VSR58);
   case 27: return offsetofPPCGuestState(guest_VSR59);
   case 28: return offsetofPPCGuestState(guest_VSR60);
   case 29: return offsetofPPCGuestState(guest_VSR61);
   case 30: return offsetofPPCGuestState(guest_VSR62);
   case 31: return offsetofPPCGuestState(guest_VSR63);
   default: break;
   }
   vpanic("vextorGuestRegOffset(ppc)"); 
}

static IRExpr* getVReg ( UInt archreg )
{
   vassert(archreg < 32);
   return IRExpr_Get( vectorGuestRegOffset(archreg), Ity_V128 );
}

static void putVReg ( UInt archreg, IRExpr* e )
{
   vassert(archreg < 32);
   vassert(typeOfIRExpr(irsb->tyenv, e) == Ity_V128);
   stmt( IRStmt_Put(vectorGuestRegOffset(archreg), e) );
}

static IRExpr* getVSReg ( UInt archreg )
{
   vassert(archreg < 64);
   return IRExpr_Get( vsxGuestRegOffset(archreg), Ity_V128 );
}

static void putVSReg ( UInt archreg, IRExpr* e )
{
   vassert(archreg < 64);
   vassert(typeOfIRExpr(irsb->tyenv, e) == Ity_V128);
   stmt( IRStmt_Put(vsxGuestRegOffset(archreg), e) );
}


static Int guestCR321offset ( UInt cr )
{
   switch (cr) {
   case 0: return offsetofPPCGuestState(guest_CR0_321 );
   case 1: return offsetofPPCGuestState(guest_CR1_321 );
   case 2: return offsetofPPCGuestState(guest_CR2_321 );
   case 3: return offsetofPPCGuestState(guest_CR3_321 );
   case 4: return offsetofPPCGuestState(guest_CR4_321 );
   case 5: return offsetofPPCGuestState(guest_CR5_321 );
   case 6: return offsetofPPCGuestState(guest_CR6_321 );
   case 7: return offsetofPPCGuestState(guest_CR7_321 );
   default: vpanic("guestCR321offset(ppc)");
   }
} 

static Int guestCR0offset ( UInt cr )
{
   switch (cr) {
   case 0: return offsetofPPCGuestState(guest_CR0_0 );
   case 1: return offsetofPPCGuestState(guest_CR1_0 );
   case 2: return offsetofPPCGuestState(guest_CR2_0 );
   case 3: return offsetofPPCGuestState(guest_CR3_0 );
   case 4: return offsetofPPCGuestState(guest_CR4_0 );
   case 5: return offsetofPPCGuestState(guest_CR5_0 );
   case 6: return offsetofPPCGuestState(guest_CR6_0 );
   case 7: return offsetofPPCGuestState(guest_CR7_0 );
   default: vpanic("guestCR3offset(ppc)");
   }
}

typedef enum {
   _placeholder0,
   _placeholder1,
   _placeholder2,
   BYTE,
   HWORD,
   WORD,
   DWORD
} _popcount_data_type;

static IRTemp gen_POPCOUNT ( IRType ty, IRTemp src, _popcount_data_type data_type )
{
   Int shift[6];
   _popcount_data_type idx, i;
   IRTemp mask[6];
   IRTemp old = IRTemp_INVALID;
   IRTemp nyu = IRTemp_INVALID;

   vassert(ty == Ity_I64 || ty == Ity_I32);

   if (ty == Ity_I32) {

      for (idx = 0; idx < WORD; idx++) {
         mask[idx]  = newTemp(ty);
         shift[idx] = 1 << idx;
      }
      assign(mask[0], mkU32(0x55555555));
      assign(mask[1], mkU32(0x33333333));
      assign(mask[2], mkU32(0x0F0F0F0F));
      assign(mask[3], mkU32(0x00FF00FF));
      assign(mask[4], mkU32(0x0000FFFF));
      old = src;
      for (i = 0; i < data_type; i++) {
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

   vassert(mode64);

   for (i = 0; i < DWORD; i++) {
      mask[i] = newTemp( Ity_I64 );
      shift[i] = 1 << i;
   }
   assign( mask[0], mkU64( 0x5555555555555555ULL ) );
   assign( mask[1], mkU64( 0x3333333333333333ULL ) );
   assign( mask[2], mkU64( 0x0F0F0F0F0F0F0F0FULL ) );
   assign( mask[3], mkU64( 0x00FF00FF00FF00FFULL ) );
   assign( mask[4], mkU64( 0x0000FFFF0000FFFFULL ) );
   assign( mask[5], mkU64( 0x00000000FFFFFFFFULL ) );
   old = src;
   for (i = 0; i < data_type; i++) {
      nyu = newTemp( Ity_I64 );
      assign( nyu,
              binop( Iop_Add64,
                     binop( Iop_And64, mkexpr( old ), mkexpr( mask[i] ) ),
                     binop( Iop_And64,
                            binop( Iop_Shr64, mkexpr( old ), mkU8( shift[i] ) ),
                            mkexpr( mask[i] ) ) ) );
      old = nyu;
   }
   return nyu;
}

static IRTemp gen_vpopcntd_mode32 ( IRTemp src1, IRTemp src2 )
{
   Int i, shift[6];
   IRTemp mask[6];
   IRTemp old = IRTemp_INVALID;
   IRTemp nyu1 = IRTemp_INVALID;
   IRTemp nyu2 = IRTemp_INVALID;
   IRTemp retval = newTemp(Ity_I64);

   vassert(!mode64);

   for (i = 0; i < WORD; i++) {
      mask[i]  = newTemp(Ity_I32);
      shift[i] = 1 << i;
   }
   assign(mask[0], mkU32(0x55555555));
   assign(mask[1], mkU32(0x33333333));
   assign(mask[2], mkU32(0x0F0F0F0F));
   assign(mask[3], mkU32(0x00FF00FF));
   assign(mask[4], mkU32(0x0000FFFF));
   old = src1;
   for (i = 0; i < WORD; i++) {
      nyu1 = newTemp(Ity_I32);
      assign(nyu1,
             binop(Iop_Add32,
                   binop(Iop_And32,
                         mkexpr(old),
                         mkexpr(mask[i])),
                   binop(Iop_And32,
                         binop(Iop_Shr32, mkexpr(old), mkU8(shift[i])),
                         mkexpr(mask[i]))));
      old = nyu1;
   }

   old = src2;
   for (i = 0; i < WORD; i++) {
      nyu2 = newTemp(Ity_I32);
      assign(nyu2,
             binop(Iop_Add32,
                   binop(Iop_And32,
                         mkexpr(old),
                         mkexpr(mask[i])),
                   binop(Iop_And32,
                         binop(Iop_Shr32, mkexpr(old), mkU8(shift[i])),
                         mkexpr(mask[i]))));
      old = nyu2;
   }
   assign(retval, unop(Iop_32Uto64, binop(Iop_Add32, mkexpr(nyu1), mkexpr(nyu2))));
   return retval;
}


static IRExpr*  ROTL ( IRExpr* src,
                                          IRExpr* rot_amt )
{
   IRExpr *mask, *rot;
   vassert(typeOfIRExpr(irsb->tyenv,rot_amt) == Ity_I8);

   if (typeOfIRExpr(irsb->tyenv,src) == Ity_I64) {
      
      mask = binop(Iop_And8, rot_amt, mkU8(63));
      rot  = binop(Iop_Or64,
                binop(Iop_Shl64, src, mask),
                binop(Iop_Shr64, src, binop(Iop_Sub8, mkU8(64), mask)));
   } else {
      
      mask = binop(Iop_And8, rot_amt, mkU8(31));
      rot  = binop(Iop_Or32,
                binop(Iop_Shl32, src, mask),
                binop(Iop_Shr32, src, binop(Iop_Sub8, mkU8(32), mask)));
   }
   return IRExpr_ITE( binop(Iop_CmpNE8, mask, mkU8(0)),
                       rot,
                       src);
}

static IRExpr* ea_rA_idxd ( UInt rA, UInt rB )
{
   IRType ty = mode64 ? Ity_I64 : Ity_I32;
   vassert(rA < 32);
   vassert(rB < 32);
   return binop(mkSzOp(ty, Iop_Add8), getIReg(rA), getIReg(rB));
}

static IRExpr* ea_rA_simm ( UInt rA, UInt simm16 )
{
   IRType ty = mode64 ? Ity_I64 : Ity_I32;
   vassert(rA < 32);
   return binop(mkSzOp(ty, Iop_Add8), getIReg(rA),
                mkSzExtendS16(ty, simm16));
}

static IRExpr* ea_rAor0 ( UInt rA )
{
   IRType ty = mode64 ? Ity_I64 : Ity_I32;
   vassert(rA < 32);
   if (rA == 0) {
      return mkSzImm(ty, 0);
   } else {
      return getIReg(rA);
   }
}

static IRExpr* ea_rAor0_idxd ( UInt rA, UInt rB )
{
   vassert(rA < 32);
   vassert(rB < 32);
   return (rA == 0) ? getIReg(rB) : ea_rA_idxd( rA, rB );
}

static IRExpr* ea_rAor0_simm ( UInt rA, UInt simm16 )
{
   IRType ty = mode64 ? Ity_I64 : Ity_I32;
   vassert(rA < 32);
   if (rA == 0) {
      return mkSzExtendS16(ty, simm16);
   } else {
      return ea_rA_simm( rA, simm16 );
   }
}


static IRExpr* addr_align( IRExpr* addr, UChar align )
{
   IRType ty = mode64 ? Ity_I64 : Ity_I32;
   Long mask;
   switch (align) {
   case 1:  return addr;                    
   case 2:  mask = ((Long)-1) << 1; break;  
   case 4:  mask = ((Long)-1) << 2; break;  
   case 16: mask = ((Long)-1) << 4; break;  
   default:
      vex_printf("addr_align: align = %u\n", align);
      vpanic("addr_align(ppc)");
   }

   vassert(typeOfIRExpr(irsb->tyenv,addr) == ty);
   return binop( mkSzOp(ty, Iop_And8), addr, mkSzImm(ty, mask) );
}


static void gen_SIGBUS_if_misaligned ( IRTemp addr, UChar align )
{
   vassert(align == 2 || align == 4 || align == 8 || align == 16);
   if (mode64) {
      vassert(typeOfIRTemp(irsb->tyenv, addr) == Ity_I64);
      stmt(
         IRStmt_Exit(
            binop(Iop_CmpNE64,
                  binop(Iop_And64, mkexpr(addr), mkU64(align-1)),
                  mkU64(0)),
            Ijk_SigBUS,
            IRConst_U64( guest_CIA_curr_instr ), OFFB_CIA
         )
      );
   } else {
      vassert(typeOfIRTemp(irsb->tyenv, addr) == Ity_I32);
      stmt(
         IRStmt_Exit(
            binop(Iop_CmpNE32,
                  binop(Iop_And32, mkexpr(addr), mkU32(align-1)),
                  mkU32(0)),
            Ijk_SigBUS,
            IRConst_U32( guest_CIA_curr_instr ), OFFB_CIA
         )
      );
   }
}


static void make_redzone_AbiHint ( const VexAbiInfo* vbi, 
                                   IRTemp nia, const HChar* who )
{
   Int szB = vbi->guest_stack_redzone_size;
   if (0) vex_printf("AbiHint: %s\n", who);
   vassert(szB >= 0);
   if (szB > 0) {
      if (mode64) {
         vassert(typeOfIRTemp(irsb->tyenv, nia) == Ity_I64);
         stmt( IRStmt_AbiHint( 
                  binop(Iop_Sub64, getIReg(1), mkU64(szB)), 
                  szB,
                  mkexpr(nia)
         ));
      } else {
         vassert(typeOfIRTemp(irsb->tyenv, nia) == Ity_I32);
         stmt( IRStmt_AbiHint( 
                  binop(Iop_Sub32, getIReg(1), mkU32(szB)), 
                  szB,
                  mkexpr(nia)
         ));
      }
   }
}




static void putCR321 ( UInt cr, IRExpr* e )
{
   vassert(cr < 8);
   vassert(typeOfIRExpr(irsb->tyenv, e) == Ity_I8);
   stmt( IRStmt_Put(guestCR321offset(cr), e) );
}

static void putCR0 ( UInt cr, IRExpr* e )
{
   vassert(cr < 8);
   vassert(typeOfIRExpr(irsb->tyenv, e) == Ity_I8);
   stmt( IRStmt_Put(guestCR0offset(cr), e) );
}

static IRExpr*  getCR0 ( UInt cr )
{
   vassert(cr < 8);
   return IRExpr_Get(guestCR0offset(cr), Ity_I8);
}

static IRExpr*  getCR321 ( UInt cr )
{
   vassert(cr < 8);
   return IRExpr_Get(guestCR321offset(cr), Ity_I8);
}

static IRExpr*  getCRbit ( UInt bi )
{
   UInt n   = bi / 4;
   UInt off = bi % 4;
   vassert(bi < 32);
   if (off == 3) {
      
      return binop(Iop_And32, unop(Iop_8Uto32, getCR0(n)), mkU32(1));
   } else {
      
      return binop( Iop_And32, 
                    binop( Iop_Shr32, 
                           unop(Iop_8Uto32, getCR321(n)),
                           mkU8(toUChar(3-off)) ),
                    mkU32(1) );
   }
}

static void putCRbit ( UInt bi, IRExpr* bit )
{
   UInt    n, off;
   IRExpr* safe;
   vassert(typeOfIRExpr(irsb->tyenv,bit) == Ity_I32);
   safe = binop(Iop_And32, bit, mkU32(1));
   n   = bi / 4;
   off = bi % 4;
   vassert(bi < 32);
   if (off == 3) {
      
      putCR0(n, unop(Iop_32to8, safe));
   } else {
      off = 3 - off;
      vassert(off == 1 || off == 2 || off == 3);
      putCR321(
         n,
         unop( Iop_32to8,
               binop( Iop_Or32,
                      
                      binop(Iop_And32, unop(Iop_8Uto32, getCR321(n)),
                                       mkU32(~(1 << off))),
                      
                      binop(Iop_Shl32, safe, mkU8(toUChar(off)))
               )
         )
      );
   }
}


static
IRExpr*  getCRbit_anywhere ( UInt bi, Int* where )
{
   UInt n   = bi / 4;
   UInt off = bi % 4;
   vassert(bi < 32);
   if (off == 3) {
      
      *where = 0;
      return binop(Iop_And32, unop(Iop_8Uto32, getCR0(n)), mkU32(1));
   } else {
      
      *where = 3-off;
      return binop( Iop_And32, 
                    unop(Iop_8Uto32, getCR321(n)),
                    mkU32(1 << (3-off)) );
   }
}

static IRExpr* getXER_SO ( void );
static void set_CR0 ( IRExpr* result )
{
   vassert(typeOfIRExpr(irsb->tyenv,result) == Ity_I32 ||
           typeOfIRExpr(irsb->tyenv,result) == Ity_I64);
   if (mode64) {
      putCR321( 0, unop(Iop_64to8,
                        binop(Iop_CmpORD64S, result, mkU64(0))) );
   } else {
      putCR321( 0, unop(Iop_32to8,
                        binop(Iop_CmpORD32S, result, mkU32(0))) );
   }
   putCR0( 0, getXER_SO() );
}


static void set_AV_CR6 ( IRExpr* result, Bool test_all_ones )
{
   IRTemp v0 = newTemp(Ity_V128);
   IRTemp v1 = newTemp(Ity_V128);
   IRTemp v2 = newTemp(Ity_V128);
   IRTemp v3 = newTemp(Ity_V128);
   IRTemp rOnes  = newTemp(Ity_I8);
   IRTemp rZeros = newTemp(Ity_I8);

   vassert(typeOfIRExpr(irsb->tyenv,result) == Ity_V128);

   assign( v0, result );
   assign( v1, binop(Iop_ShrV128, result, mkU8(32)) );
   assign( v2, binop(Iop_ShrV128, result, mkU8(64)) );
   assign( v3, binop(Iop_ShrV128, result, mkU8(96)) );

   assign( rZeros, unop(Iop_1Uto8,
       binop(Iop_CmpEQ32, mkU32(0xFFFFFFFF),
             unop(Iop_Not32,
                  unop(Iop_V128to32,
                       binop(Iop_OrV128,
                             binop(Iop_OrV128, mkexpr(v0), mkexpr(v1)),
                             binop(Iop_OrV128, mkexpr(v2), mkexpr(v3))))
                  ))) );

   if (test_all_ones) {
      assign( rOnes, unop(Iop_1Uto8,
         binop(Iop_CmpEQ32, mkU32(0xFFFFFFFF),
               unop(Iop_V128to32,
                    binop(Iop_AndV128,
                          binop(Iop_AndV128, mkexpr(v0), mkexpr(v1)),
                          binop(Iop_AndV128, mkexpr(v2), mkexpr(v3)))
                    ))) );
      putCR321( 6, binop(Iop_Or8,
                         binop(Iop_Shl8, mkexpr(rOnes),  mkU8(3)),
                         binop(Iop_Shl8, mkexpr(rZeros), mkU8(1))) );
   } else {
      putCR321( 6, binop(Iop_Shl8, mkexpr(rZeros), mkU8(1)) );
   }
   putCR0( 6, mkU8(0) );
} 




static void putXER_SO ( IRExpr* e )
{
   IRExpr* so;
   vassert(typeOfIRExpr(irsb->tyenv, e) == Ity_I8);
   so = binop(Iop_And8, e, mkU8(1));
   stmt( IRStmt_Put( OFFB_XER_SO, so ) );
}

static void putXER_OV ( IRExpr* e )
{
   IRExpr* ov;
   vassert(typeOfIRExpr(irsb->tyenv, e) == Ity_I8);
   ov = binop(Iop_And8, e, mkU8(1));
   stmt( IRStmt_Put( OFFB_XER_OV, ov ) );
}

static void putXER_CA ( IRExpr* e )
{
   IRExpr* ca;
   vassert(typeOfIRExpr(irsb->tyenv, e) == Ity_I8);
   ca = binop(Iop_And8, e, mkU8(1));
   stmt( IRStmt_Put( OFFB_XER_CA, ca ) );
}

static void putXER_BC ( IRExpr* e )
{
   IRExpr* bc;
   vassert(typeOfIRExpr(irsb->tyenv, e) == Ity_I8);
   bc = binop(Iop_And8, e, mkU8(0x7F));
   stmt( IRStmt_Put( OFFB_XER_BC, bc ) );
}

static IRExpr*  getXER_SO ( void )
{
   return IRExpr_Get( OFFB_XER_SO, Ity_I8 );
}

static IRExpr*  getXER_SO32 ( void )
{
   return binop( Iop_And32, unop(Iop_8Uto32, getXER_SO()), mkU32(1) );
}

static IRExpr*  getXER_OV ( void )
{
   return IRExpr_Get( OFFB_XER_OV, Ity_I8 );
}

static IRExpr*  getXER_OV32 ( void )
{
   return binop( Iop_And32, unop(Iop_8Uto32, getXER_OV()), mkU32(1) );
}

static IRExpr*  getXER_CA32 ( void )
{
   IRExpr* ca = IRExpr_Get( OFFB_XER_CA, Ity_I8 );
   return binop( Iop_And32, unop(Iop_8Uto32, ca ), mkU32(1) );
}

static IRExpr*  getXER_BC ( void )
{
   return IRExpr_Get( OFFB_XER_BC, Ity_I8 );
}

static IRExpr*  getXER_BC32 ( void )
{
   IRExpr* bc = IRExpr_Get( OFFB_XER_BC, Ity_I8 );
   return binop( Iop_And32, unop(Iop_8Uto32, bc), mkU32(0x7F) );
}



static void set_XER_OV_32( UInt op, IRExpr* res,
                           IRExpr* argL, IRExpr* argR )
{
   IRTemp  t64;
   IRExpr* xer_ov;
   vassert(op < PPCG_FLAG_OP_NUMBER);
   vassert(typeOfIRExpr(irsb->tyenv,res)  == Ity_I32);
   vassert(typeOfIRExpr(irsb->tyenv,argL) == Ity_I32);
   vassert(typeOfIRExpr(irsb->tyenv,argR) == Ity_I32);

#  define INT32_MIN 0x80000000

#  define XOR2(_aa,_bb) \
      binop(Iop_Xor32,(_aa),(_bb))

#  define XOR3(_cc,_dd,_ee) \
      binop(Iop_Xor32,binop(Iop_Xor32,(_cc),(_dd)),(_ee))

#  define AND3(_ff,_gg,_hh) \
      binop(Iop_And32,binop(Iop_And32,(_ff),(_gg)),(_hh))

#define NOT(_jj) \
      unop(Iop_Not32, (_jj))

   switch (op) {
   case  PPCG_FLAG_OP_ADD:
   case  PPCG_FLAG_OP_ADDE:
      
      
      xer_ov 
         = AND3( XOR3(argL,argR,mkU32(-1)),
                 XOR2(argL,res),
                 mkU32(INT32_MIN) );
      
      xer_ov 
         = binop(Iop_Shr32, xer_ov, mkU8(31) );
      break;
      
   case  PPCG_FLAG_OP_DIVW:
      
      xer_ov
         = mkOR1(
              mkAND1( 
                 binop(Iop_CmpEQ32, argL, mkU32(INT32_MIN)),
                 binop(Iop_CmpEQ32, argR, mkU32(-1)) 
              ),
              binop(Iop_CmpEQ32, argR, mkU32(0) ) 
           );
      xer_ov 
         = unop(Iop_1Uto32, xer_ov);
      break;
      
   case  PPCG_FLAG_OP_DIVWU:
      
      xer_ov 
         = unop(Iop_1Uto32, binop(Iop_CmpEQ32, argR, mkU32(0)));
      break;
      
   case  PPCG_FLAG_OP_MULLW:
      t64 = newTemp(Ity_I64);
      assign( t64, binop(Iop_MullS32, argL, argR) );
      xer_ov 
         = binop( Iop_CmpNE32,
                  unop(Iop_64HIto32, mkexpr(t64)),
                  binop( Iop_Sar32, 
                         unop(Iop_64to32, mkexpr(t64)), 
                         mkU8(31))
                  );
      xer_ov
         = unop(Iop_1Uto32, xer_ov);
      break;
      
   case  PPCG_FLAG_OP_NEG:
      
      xer_ov
         = unop( Iop_1Uto32, 
                 binop(Iop_CmpEQ32, argL, mkU32(INT32_MIN)) );
      break;
      
   case  PPCG_FLAG_OP_SUBF:
   case  PPCG_FLAG_OP_SUBFC:
   case  PPCG_FLAG_OP_SUBFE:
      
      xer_ov 
         = AND3( XOR3(NOT(argL),argR,mkU32(-1)),
                 XOR2(NOT(argL),res),
                 mkU32(INT32_MIN) );
      
      xer_ov 
         = binop(Iop_Shr32, xer_ov, mkU8(31) );
      break;
      
   case PPCG_FLAG_OP_DIVWEU:
      xer_ov
               = binop( Iop_Or32,
                        unop( Iop_1Uto32, binop( Iop_CmpEQ32, argR, mkU32( 0 ) ) ),
                        unop( Iop_1Uto32, binop( Iop_CmpLT32U, argR, argL ) ) );
      break;

   case PPCG_FLAG_OP_DIVWE:

      xer_ov = binop( Iop_Or32,
                      unop( Iop_1Uto32, binop( Iop_CmpEQ32, argR, mkU32( 0 ) ) ),
                      unop( Iop_1Uto32, mkAND1( binop( Iop_CmpEQ32, res, mkU32( 0 ) ),
                              mkAND1( binop( Iop_CmpNE32, argL, mkU32( 0 ) ),
                                      binop( Iop_CmpNE32, argR, mkU32( 0 ) ) ) ) ) );
      break;



   default: 
      vex_printf("set_XER_OV: op = %u\n", op);
      vpanic("set_XER_OV(ppc)");
   }
   
   
   putXER_OV( unop(Iop_32to8, xer_ov) );

   
   putXER_SO( binop(Iop_Or8, getXER_SO(), getXER_OV()) );

#  undef INT32_MIN
#  undef AND3
#  undef XOR3
#  undef XOR2
#  undef NOT
}

static void set_XER_OV_64( UInt op, IRExpr* res,
                           IRExpr* argL, IRExpr* argR )
{
   IRExpr* xer_ov;
   vassert(op < PPCG_FLAG_OP_NUMBER);
   vassert(typeOfIRExpr(irsb->tyenv,res)  == Ity_I64);
   vassert(typeOfIRExpr(irsb->tyenv,argL) == Ity_I64);
   vassert(typeOfIRExpr(irsb->tyenv,argR) == Ity_I64);

#  define INT64_MIN 0x8000000000000000ULL

#  define XOR2(_aa,_bb) \
      binop(Iop_Xor64,(_aa),(_bb))

#  define XOR3(_cc,_dd,_ee) \
      binop(Iop_Xor64,binop(Iop_Xor64,(_cc),(_dd)),(_ee))

#  define AND3(_ff,_gg,_hh) \
      binop(Iop_And64,binop(Iop_And64,(_ff),(_gg)),(_hh))

#define NOT(_jj) \
      unop(Iop_Not64, (_jj))

   switch (op) {
   case  PPCG_FLAG_OP_ADD:
   case  PPCG_FLAG_OP_ADDE:
      
      
      xer_ov 
         = AND3( XOR3(argL,argR,mkU64(-1)),
                 XOR2(argL,res),
                 mkU64(INT64_MIN) );
      
      xer_ov 
         = unop(Iop_64to1, binop(Iop_Shr64, xer_ov, mkU8(63)));
      break;
      
   case  PPCG_FLAG_OP_DIVW:
      
      xer_ov
         = mkOR1(
              mkAND1( 
                 binop(Iop_CmpEQ64, argL, mkU64(INT64_MIN)),
                 binop(Iop_CmpEQ64, argR, mkU64(-1)) 
              ),
              binop(Iop_CmpEQ64, argR, mkU64(0) ) 
           );
      break;

   case  PPCG_FLAG_OP_DIVWU:
      
      xer_ov 
         = binop(Iop_CmpEQ64, argR, mkU64(0));
      break;
      
   case  PPCG_FLAG_OP_MULLW: {
      xer_ov 
         = binop( Iop_CmpNE32,
                  unop(Iop_64HIto32, res),
                  binop( Iop_Sar32, 
                         unop(Iop_64to32, res), 
                         mkU8(31))
                  );
      break;
   }
      
   case  PPCG_FLAG_OP_NEG:
      
      xer_ov
         = binop(Iop_CmpEQ64, argL, mkU64(INT64_MIN));
      break;
      
   case  PPCG_FLAG_OP_SUBF:
   case  PPCG_FLAG_OP_SUBFC:
   case  PPCG_FLAG_OP_SUBFE:
      
      xer_ov 
         = AND3( XOR3(NOT(argL),argR,mkU64(-1)),
                 XOR2(NOT(argL),res),
                 mkU64(INT64_MIN) );
      
      xer_ov 
         = unop(Iop_64to1, binop(Iop_Shr64, xer_ov, mkU8(63)));
      break;
      
   case PPCG_FLAG_OP_DIVDE:

      xer_ov
                  = mkOR1( binop( Iop_CmpEQ64, argR, mkU64( 0 ) ),
                           mkAND1( binop( Iop_CmpEQ64, res, mkU64( 0 ) ),
                                   mkAND1( binop( Iop_CmpNE64, argL, mkU64( 0 ) ),
                                           binop( Iop_CmpNE64, argR, mkU64( 0 ) ) ) ) );
      break;

   case PPCG_FLAG_OP_DIVDEU:
     
     xer_ov = mkOR1( binop( Iop_CmpEQ64, argR, mkU64( 0 ) ),
                         binop( Iop_CmpLE64U, argR, argL ) );
     break;

   case  PPCG_FLAG_OP_MULLD: {
      IRTemp  t128;
      t128 = newTemp(Ity_I128);
      assign( t128, binop(Iop_MullS64, argL, argR) );
      xer_ov 
         = binop( Iop_CmpNE64,
                  unop(Iop_128HIto64, mkexpr(t128)),
                  binop( Iop_Sar64,
                         unop(Iop_128to64, mkexpr(t128)),
                         mkU8(63))
                  );
      break;
   }
      
   default: 
      vex_printf("set_XER_OV: op = %u\n", op);
      vpanic("set_XER_OV(ppc64)");
   }
   
   
   putXER_OV( unop(Iop_1Uto8, xer_ov) );

   
   putXER_SO( binop(Iop_Or8, getXER_SO(), getXER_OV()) );

#  undef INT64_MIN
#  undef AND3
#  undef XOR3
#  undef XOR2
#  undef NOT
}

static void set_XER_OV ( IRType ty, UInt op, IRExpr* res,
                         IRExpr* argL, IRExpr* argR )
{
   if (ty == Ity_I32)
      set_XER_OV_32( op, res, argL, argR );
   else
      set_XER_OV_64( op, res, argL, argR );
}




static void set_XER_CA_32 ( UInt op, IRExpr* res,
                            IRExpr* argL, IRExpr* argR, IRExpr* oldca )
{
   IRExpr* xer_ca;
   vassert(op < PPCG_FLAG_OP_NUMBER);
   vassert(typeOfIRExpr(irsb->tyenv,res)   == Ity_I32);
   vassert(typeOfIRExpr(irsb->tyenv,argL)  == Ity_I32);
   vassert(typeOfIRExpr(irsb->tyenv,argR)  == Ity_I32);
   vassert(typeOfIRExpr(irsb->tyenv,oldca) == Ity_I32);


   switch (op) {
   case  PPCG_FLAG_OP_ADD:
      
      xer_ca
         = unop(Iop_1Uto32, binop(Iop_CmpLT32U, res, argL));
      break;
      
   case  PPCG_FLAG_OP_ADDE:
      
      xer_ca 
         = mkOR1( 
              binop(Iop_CmpLT32U, res, argL),
              mkAND1( 
                 binop(Iop_CmpEQ32, oldca, mkU32(1)),
                 binop(Iop_CmpEQ32, res, argL) 
              ) 
           );
      xer_ca 
         = unop(Iop_1Uto32, xer_ca);
      break;
      
   case  PPCG_FLAG_OP_SUBFE:
      
      xer_ca 
         = mkOR1( 
              binop(Iop_CmpLT32U, res, argR),
              mkAND1( 
                 binop(Iop_CmpEQ32, oldca, mkU32(1)),
                 binop(Iop_CmpEQ32, res, argR) 
              ) 
           );
      xer_ca 
         = unop(Iop_1Uto32, xer_ca);
      break;
      
   case  PPCG_FLAG_OP_SUBFC:
   case  PPCG_FLAG_OP_SUBFI:
      
      xer_ca
         = unop(Iop_1Uto32, binop(Iop_CmpLE32U, res, argR));
      break;
      
   case  PPCG_FLAG_OP_SRAW:
      
      xer_ca
         = binop(
              Iop_And32,
              binop(Iop_Sar32, argL, mkU8(31)),
              binop( Iop_And32,
                     argL,
                     binop( Iop_Sub32,
                            binop(Iop_Shl32, mkU32(1),
                                             unop(Iop_32to8,argR)),
                            mkU32(1) )
                     )
              );
      xer_ca 
         = IRExpr_ITE(
              
              binop(Iop_CmpLT32U, mkU32(31), argR),
              
              binop(Iop_Shr32, argL, mkU8(31)),
              
              unop(Iop_1Uto32, binop(Iop_CmpNE32, xer_ca, mkU32(0)))
           );
      break;

   case  PPCG_FLAG_OP_SRAWI:
      xer_ca
         = binop(
              Iop_And32,
              binop(Iop_Sar32, argL, mkU8(31)),
              binop( Iop_And32,
                     argL,
                     binop( Iop_Sub32,
                            binop(Iop_Shl32, mkU32(1),
                                             unop(Iop_32to8,argR)),
                            mkU32(1) )
                     )
              );
      xer_ca 
         = unop(Iop_1Uto32, binop(Iop_CmpNE32, xer_ca, mkU32(0)));
      break;
      
   default: 
      vex_printf("set_XER_CA: op = %u\n", op);
      vpanic("set_XER_CA(ppc)");
   }

   
   putXER_CA( unop(Iop_32to8, xer_ca) );
}

static void set_XER_CA_64 ( UInt op, IRExpr* res,
                            IRExpr* argL, IRExpr* argR, IRExpr* oldca )
{
   IRExpr* xer_ca;
   vassert(op < PPCG_FLAG_OP_NUMBER);
   vassert(typeOfIRExpr(irsb->tyenv,res)   == Ity_I64);
   vassert(typeOfIRExpr(irsb->tyenv,argL)  == Ity_I64);
   vassert(typeOfIRExpr(irsb->tyenv,argR)  == Ity_I64);
   vassert(typeOfIRExpr(irsb->tyenv,oldca) == Ity_I64);


   switch (op) {
   case  PPCG_FLAG_OP_ADD:
      
      xer_ca
         = unop(Iop_1Uto32, binop(Iop_CmpLT64U, res, argL));
      break;
      
   case  PPCG_FLAG_OP_ADDE:
      
      xer_ca 
         = mkOR1( 
              binop(Iop_CmpLT64U, res, argL),
              mkAND1( 
                 binop(Iop_CmpEQ64, oldca, mkU64(1)),
                 binop(Iop_CmpEQ64, res, argL) 
                 ) 
              );
      xer_ca 
         = unop(Iop_1Uto32, xer_ca);
      break;
      
   case  PPCG_FLAG_OP_SUBFE:
      
      xer_ca 
         = mkOR1( 
              binop(Iop_CmpLT64U, res, argR),
              mkAND1( 
                 binop(Iop_CmpEQ64, oldca, mkU64(1)),
                 binop(Iop_CmpEQ64, res, argR) 
              ) 
           );
      xer_ca 
         = unop(Iop_1Uto32, xer_ca);
      break;
      
   case  PPCG_FLAG_OP_SUBFC:
   case  PPCG_FLAG_OP_SUBFI:
      
      xer_ca
         = unop(Iop_1Uto32, binop(Iop_CmpLE64U, res, argR));
      break;
      
      
   case  PPCG_FLAG_OP_SRAW:
         

      xer_ca
         = binop(
              Iop_And64,
              binop(Iop_Sar64, argL, mkU8(31)),
              binop( Iop_And64,
                     argL,
                     binop( Iop_Sub64,
                            binop(Iop_Shl64, mkU64(1),
                                             unop(Iop_64to8,argR)),
                            mkU64(1) )
              )
           );
      xer_ca 
         = IRExpr_ITE(
              
              binop(Iop_CmpLT64U, mkU64(31), argR),
              
              unop(Iop_64to32, binop(Iop_Shr64, argL, mkU8(63))),
              
              unop(Iop_1Uto32, binop(Iop_CmpNE64, xer_ca, mkU64(0)))
          );
      break;
      
   case  PPCG_FLAG_OP_SRAWI:

      xer_ca
         = binop(
              Iop_And64,
              binop(Iop_Sar64, argL, mkU8(31)),
              binop( Iop_And64,
                     argL,
                     binop( Iop_Sub64,
                            binop(Iop_Shl64, mkU64(1),
                                             unop(Iop_64to8,argR)),
                            mkU64(1) )
              )
           );
      xer_ca 
         = unop(Iop_1Uto32, binop(Iop_CmpNE64, xer_ca, mkU64(0)));
      break;
      

   case  PPCG_FLAG_OP_SRAD:
         

      xer_ca
         = binop(
              Iop_And64,
              binop(Iop_Sar64, argL, mkU8(63)),
              binop( Iop_And64,
                     argL,
                     binop( Iop_Sub64,
                            binop(Iop_Shl64, mkU64(1),
                                             unop(Iop_64to8,argR)),
                            mkU64(1) )
              )
           );
      xer_ca 
         = IRExpr_ITE(
              
              binop(Iop_CmpLT64U, mkU64(63), argR),
              
              unop(Iop_64to32, binop(Iop_Shr64, argL, mkU8(63))),
              
              unop(Iop_1Uto32, binop(Iop_CmpNE64, xer_ca, mkU64(0)))
           );
      break;


   case  PPCG_FLAG_OP_SRADI:

      xer_ca
         = binop(
              Iop_And64,
              binop(Iop_Sar64, argL, mkU8(63)),
              binop( Iop_And64,
                     argL,
                     binop( Iop_Sub64,
                            binop(Iop_Shl64, mkU64(1),
                                             unop(Iop_64to8,argR)),
                            mkU64(1) )
              )
           );
      xer_ca 
         = unop(Iop_1Uto32, binop(Iop_CmpNE64, xer_ca, mkU64(0)));
      break;

   default: 
      vex_printf("set_XER_CA: op = %u\n", op);
      vpanic("set_XER_CA(ppc64)");
   }

   
   putXER_CA( unop(Iop_32to8, xer_ca) );
}

static void set_XER_CA ( IRType ty, UInt op, IRExpr* res,
                         IRExpr* argL, IRExpr* argR, IRExpr* oldca )
{
   if (ty == Ity_I32)
      set_XER_CA_32( op, res, argL, argR, oldca );
   else
      set_XER_CA_64( op, res, argL, argR, oldca );
}




static IRExpr*  getGST ( PPC_GST reg )
{
   IRType ty = mode64 ? Ity_I64 : Ity_I32;
   switch (reg) {
   case PPC_GST_SPRG3_RO:
      return IRExpr_Get( OFFB_SPRG3_RO, ty );

   case PPC_GST_CIA: 
      return IRExpr_Get( OFFB_CIA, ty );

   case PPC_GST_LR: 
      return IRExpr_Get( OFFB_LR, ty );

   case PPC_GST_CTR: 
      return IRExpr_Get( OFFB_CTR, ty );

   case PPC_GST_VRSAVE: 
      return IRExpr_Get( OFFB_VRSAVE, Ity_I32 );

   case PPC_GST_VSCR:
      return binop(Iop_And32, IRExpr_Get( OFFB_VSCR,Ity_I32 ),
                              mkU32(MASK_VSCR_VALID));

   case PPC_GST_CR: {
      
#     define FIELD(_n)                                               \
         binop(Iop_Shl32,                                            \
               unop(Iop_8Uto32,                                      \
                    binop(Iop_Or8,                                   \
                          binop(Iop_And8, getCR321(_n), mkU8(7<<1)), \
                          binop(Iop_And8, getCR0(_n), mkU8(1))       \
                    )                                                \
               ),                                                    \
               mkU8(4 * (7-(_n)))                                    \
         )
      return binop(Iop_Or32,
                   binop(Iop_Or32,
                         binop(Iop_Or32, FIELD(0), FIELD(1)),
                         binop(Iop_Or32, FIELD(2), FIELD(3))
                         ),
                   binop(Iop_Or32,
                         binop(Iop_Or32, FIELD(4), FIELD(5)),
                         binop(Iop_Or32, FIELD(6), FIELD(7))
                         )
                   );
#     undef FIELD
   }

   case PPC_GST_XER:
      return binop(Iop_Or32,
                   binop(Iop_Or32,
                         binop( Iop_Shl32, getXER_SO32(), mkU8(31)),
                         binop( Iop_Shl32, getXER_OV32(), mkU8(30))),
                   binop(Iop_Or32,
                         binop( Iop_Shl32, getXER_CA32(), mkU8(29)),
                         getXER_BC32()));

   case PPC_GST_TFHAR:
      return IRExpr_Get( OFFB_TFHAR, ty );

   case PPC_GST_TEXASR:
      return IRExpr_Get( OFFB_TEXASR, ty );

   case PPC_GST_TEXASRU:
      return IRExpr_Get( OFFB_TEXASRU, ty );

   case PPC_GST_TFIAR:
      return IRExpr_Get( OFFB_TFIAR, ty );

   default:
      vex_printf("getGST(ppc): reg = %u", reg);
      vpanic("getGST(ppc)");
   }
}

static IRExpr*  getGST_masked ( PPC_GST reg, UInt mask )
{
   IRTemp val = newTemp(Ity_I32);
   vassert( reg < PPC_GST_MAX );
    
   switch (reg) {

   case PPC_GST_FPSCR: {

      if (mask & MASK_FPSCR_RN) {
         assign( val, unop( Iop_8Uto32, IRExpr_Get( OFFB_FPROUND, Ity_I8 ) ) );
      } else {
         assign( val, mkU32(0x0) );
      }
      break;
   }

   default:
      vex_printf("getGST_masked(ppc): reg = %u", reg);
      vpanic("getGST_masked(ppc)");
   }

   if (mask != 0xFFFFFFFF) {
      return binop(Iop_And32, mkexpr(val), mkU32(mask));
   } else {
      return mkexpr(val);
   }
}

static IRExpr* getGST_masked_upper(PPC_GST reg, ULong mask) {
   IRExpr * val;
   vassert( reg < PPC_GST_MAX );

   switch (reg) {

   case PPC_GST_FPSCR: {
      if (mask & MASK_FPSCR_DRN) {
         val = binop( Iop_And32,
                      unop( Iop_8Uto32, IRExpr_Get( OFFB_DFPROUND, Ity_I8 ) ),
                      unop( Iop_64HIto32, mkU64( mask ) ) );
      } else {
         val = mkU32( 0x0ULL );
      }
      break;
   }

   default:
      vex_printf( "getGST_masked_upper(ppc): reg = %u", reg );
      vpanic( "getGST_masked_upper(ppc)" );
   }
   return val;
}


static IRExpr*  getGST_field ( PPC_GST reg, UInt fld )
{
   UInt shft, mask;

   vassert( fld < 8 );
   vassert( reg < PPC_GST_MAX );
   
   shft = 4*(7-fld);
   mask = 0xF<<shft;

   switch (reg) {
   case PPC_GST_XER:
      vassert(fld ==7);
      return binop(Iop_Or32,
                   binop(Iop_Or32,
                         binop(Iop_Shl32, getXER_SO32(), mkU8(3)),
                         binop(Iop_Shl32, getXER_OV32(), mkU8(2))),
                   binop(      Iop_Shl32, getXER_CA32(), mkU8(1)));
      break;

   default:
      if (shft == 0)
         return getGST_masked( reg, mask );
      else
         return binop(Iop_Shr32,
                      getGST_masked( reg, mask ),
                      mkU8(toUChar( shft )));
   }
}

static void putGST ( PPC_GST reg, IRExpr* src )
{
   IRType ty     = mode64 ? Ity_I64 : Ity_I32;
   IRType ty_src = typeOfIRExpr(irsb->tyenv,src );
   vassert( reg < PPC_GST_MAX );
   switch (reg) {
   case PPC_GST_IP_AT_SYSCALL: 
      vassert( ty_src == ty );
      stmt( IRStmt_Put( OFFB_IP_AT_SYSCALL, src ) );
      break;
   case PPC_GST_CIA: 
      vassert( ty_src == ty );
      stmt( IRStmt_Put( OFFB_CIA, src ) );
      break;
   case PPC_GST_LR: 
      vassert( ty_src == ty );
      stmt( IRStmt_Put( OFFB_LR, src ) );
      break;
   case PPC_GST_CTR: 
      vassert( ty_src == ty );
      stmt( IRStmt_Put( OFFB_CTR, src ) );
      break;
   case PPC_GST_VRSAVE: 
      vassert( ty_src == Ity_I32 );
      stmt( IRStmt_Put( OFFB_VRSAVE,src));
      break;
   case PPC_GST_VSCR:
      vassert( ty_src == Ity_I32 );
      stmt( IRStmt_Put( OFFB_VSCR,
                        binop(Iop_And32, src,
                              mkU32(MASK_VSCR_VALID)) ) );
      break;
   case PPC_GST_XER:
      vassert( ty_src == Ity_I32 );
      putXER_SO( unop(Iop_32to8, binop(Iop_Shr32, src, mkU8(31))) );
      putXER_OV( unop(Iop_32to8, binop(Iop_Shr32, src, mkU8(30))) );
      putXER_CA( unop(Iop_32to8, binop(Iop_Shr32, src, mkU8(29))) );
      putXER_BC( unop(Iop_32to8, src) );
      break;
      
   case PPC_GST_EMWARN:
      vassert( ty_src == Ity_I32 );
      stmt( IRStmt_Put( OFFB_EMNOTE,src) );
      break;
      
   case PPC_GST_CMSTART: 
      vassert( ty_src == ty );
      stmt( IRStmt_Put( OFFB_CMSTART, src) );
      break;
      
   case PPC_GST_CMLEN: 
      vassert( ty_src == ty );
      stmt( IRStmt_Put( OFFB_CMLEN, src) );
      break;
      
   case PPC_GST_TEXASR:
      vassert( ty_src == Ity_I64 );
      stmt( IRStmt_Put( OFFB_TEXASR, src ) );
      break;

   case PPC_GST_TEXASRU:
      vassert( ty_src == Ity_I32 );
      stmt( IRStmt_Put( OFFB_TEXASRU, src ) );
      break;

   case PPC_GST_TFIAR:
      vassert( ty_src == Ity_I64 );
      stmt( IRStmt_Put( OFFB_TFIAR, src ) );
      break;
   case PPC_GST_TFHAR:
      vassert( ty_src == Ity_I64 );
      stmt( IRStmt_Put( OFFB_TFHAR, src ) );
      break;
   default:
      vex_printf("putGST(ppc): reg = %u", reg);
      vpanic("putGST(ppc)");
   }
}

static void putGST_masked ( PPC_GST reg, IRExpr* src, ULong mask )
{
   IRType ty = mode64 ? Ity_I64 : Ity_I32;
   vassert( reg < PPC_GST_MAX );
   vassert( typeOfIRExpr( irsb->tyenv,src ) == Ity_I64 );

   switch (reg) {
   case PPC_GST_FPSCR: {
      if (mask & MASK_FPSCR_RN) {
         stmt( 
            IRStmt_Put(
               OFFB_FPROUND,
               unop(
                  Iop_32to8,
                  binop(
                     Iop_Or32, 
                     binop(
                        Iop_And32,
                        unop(Iop_64to32, src),
                        mkU32(MASK_FPSCR_RN & mask)
                     ),
                     binop(
                        Iop_And32, 
                        unop(Iop_8Uto32, IRExpr_Get(OFFB_FPROUND,Ity_I8)),
                        mkU32(MASK_FPSCR_RN & ~mask)
                     )
                  )
               )
            )
         );
      }
      if (mask & MASK_FPSCR_DRN) {
         stmt( 
            IRStmt_Put(
               OFFB_DFPROUND,
               unop(
                  Iop_32to8,
                  binop(
                     Iop_Or32, 
                     binop(
                        Iop_And32,
                        unop(Iop_64HIto32, src),
                        mkU32((MASK_FPSCR_DRN & mask) >> 32)
                     ),
                     binop(
                        Iop_And32, 
                        unop(Iop_8Uto32, IRExpr_Get(OFFB_DFPROUND,Ity_I8)),
                        mkU32((MASK_FPSCR_DRN & ~mask) >> 32)
                     )
                  )
               )
            )
         );
      }

      if (mask & 0xFC) {  
         VexEmNote ew = EmWarn_PPCexns;

         putGST( PPC_GST_EMWARN, mkU32(ew) );
         stmt( 
            IRStmt_Exit(
               binop(Iop_CmpNE32, mkU32(ew), mkU32(EmNote_NONE)),
               Ijk_EmWarn,
               mkSzConst( ty, nextInsnAddr()), OFFB_CIA ));
      }

      
      break;
   }

   default:
      vex_printf("putGST_masked(ppc): reg = %u", reg);
      vpanic("putGST_masked(ppc)");
   }
}

static void putGST_field ( PPC_GST reg, IRExpr* src, UInt fld )
{
   UInt shft;
   ULong mask;

   vassert( typeOfIRExpr(irsb->tyenv,src ) == Ity_I32 );
   vassert( fld < 16 );
   vassert( reg < PPC_GST_MAX );
   
   if (fld < 8)
      shft = 4*(7-fld);
   else
      shft = 4*(15-fld);
   mask = 0xF;
   mask = mask << shft;

   switch (reg) {
   case PPC_GST_CR:
      putCR0  (fld, binop(Iop_And8, mkU8(1   ), unop(Iop_32to8, src)));
      putCR321(fld, binop(Iop_And8, mkU8(7<<1), unop(Iop_32to8, src)));
      break;

   default:
      {
         IRExpr * src64 = unop( Iop_32Uto64, src );

         if (shft == 0) {
            putGST_masked( reg, src64, mask );
         } else {
            putGST_masked( reg,
                           binop( Iop_Shl64, src64, mkU8( toUChar( shft ) ) ),
                           mask );
         }
      }
   }
}


#define NONZERO_FRAC_MASK 0x000fffffffffffffULL
#define FP_FRAC_PART(x) binop( Iop_And64, \
                               mkexpr( x ), \
                               mkU64( NONZERO_FRAC_MASK ) )

static IRExpr * fp_exp_part_sp(IRTemp src)
{
   return binop( Iop_And32,
                 binop( Iop_Shr32, mkexpr( src ), mkU8( 23 ) ),
                 mkU32( 0xff ) );
}

static IRExpr * fp_exp_part(IRTemp src, Bool sp)
{
   IRExpr * exp;
   if (sp)
      return fp_exp_part_sp(src);

   if (!mode64)
      exp = binop( Iop_And32, binop( Iop_Shr32, unop( Iop_64HIto32,
                                                      mkexpr( src ) ),
                                     mkU8( 20 ) ), mkU32( 0x7ff ) );
   else
      exp = unop( Iop_64to32,
                  binop( Iop_And64,
                         binop( Iop_Shr64, mkexpr( src ), mkU8( 52 ) ),
                         mkU64( 0x7ff ) ) );
   return exp;
}

static IRExpr * is_Inf_sp(IRTemp src)
{
   IRTemp frac_part = newTemp(Ity_I32);
   IRExpr * Inf_exp;

   assign( frac_part, binop( Iop_And32, mkexpr(src), mkU32(0x007fffff)) );
   Inf_exp = binop( Iop_CmpEQ32, fp_exp_part( src, True  ), mkU32( 0xff ) );
   return mkAND1( Inf_exp, binop( Iop_CmpEQ32, mkexpr( frac_part ), mkU32( 0 ) ) );
}


static IRExpr * is_Inf(IRTemp src, Bool sp)
{
   IRExpr * Inf_exp, * hi32, * low32;
   IRTemp frac_part;

   if (sp)
      return is_Inf_sp(src);

   frac_part = newTemp(Ity_I64);
   assign( frac_part, FP_FRAC_PART(src) );
   Inf_exp = binop( Iop_CmpEQ32, fp_exp_part( src, False   ), mkU32( 0x7ff ) );
   hi32 = unop( Iop_64HIto32, mkexpr( frac_part ) );
   low32 = unop( Iop_64to32, mkexpr( frac_part ) );
   return mkAND1( Inf_exp, binop( Iop_CmpEQ32, binop( Iop_Or32, low32, hi32 ),
                                  mkU32( 0 ) ) );
}

static IRExpr * is_Zero_sp(IRTemp src)
{
   IRTemp sign_less_part = newTemp(Ity_I32);
   assign( sign_less_part, binop( Iop_And32, mkexpr( src ), mkU32( SIGN_MASK32 ) ) );
   return binop( Iop_CmpEQ32, mkexpr( sign_less_part ), mkU32( 0 ) );
}

static IRExpr * is_Zero(IRTemp src, Bool sp)
{
   IRExpr * hi32, * low32;
   IRTemp sign_less_part;
   if (sp)
      return is_Zero_sp(src);

   sign_less_part = newTemp(Ity_I64);

   assign( sign_less_part, binop( Iop_And64, mkexpr( src ), mkU64( SIGN_MASK ) ) );
   hi32 = unop( Iop_64HIto32, mkexpr( sign_less_part ) );
   low32 = unop( Iop_64to32, mkexpr( sign_less_part ) );
   return binop( Iop_CmpEQ32, binop( Iop_Or32, low32, hi32 ),
                              mkU32( 0 ) );
}

static IRExpr * is_NaN(IRTemp src)
{
   IRExpr * NaN_exp, * hi32, * low32;
   IRTemp frac_part = newTemp(Ity_I64);

   assign( frac_part, FP_FRAC_PART(src) );
   hi32 = unop( Iop_64HIto32, mkexpr( frac_part ) );
   low32 = unop( Iop_64to32, mkexpr( frac_part ) );
   NaN_exp = binop( Iop_CmpEQ32, fp_exp_part( src, False  ),
                    mkU32( 0x7ff ) );

   return mkAND1( NaN_exp, binop( Iop_CmpNE32, binop( Iop_Or32, low32, hi32 ),
                                               mkU32( 0 ) ) );
}

static IRExpr * is_NaN_32(IRTemp src)
{
#define NONZERO_FRAC_MASK32 0x007fffffULL
#define FP_FRAC_PART32(x) binop( Iop_And32, \
                                 mkexpr( x ), \
                                 mkU32( NONZERO_FRAC_MASK32 ) )

   IRExpr * frac_part = FP_FRAC_PART32(src);
   IRExpr * exp_part = binop( Iop_And32,
                              binop( Iop_Shr32, mkexpr( src ), mkU8( 23 ) ),
                              mkU32( 0x0ff ) );
   IRExpr * NaN_exp = binop( Iop_CmpEQ32, exp_part, mkU32( 0xff ) );

   return mkAND1( NaN_exp, binop( Iop_CmpNE32, frac_part, mkU32( 0 ) ) );
}

static IRExpr * handle_SNaN_to_QNaN_32(IRExpr * src)
{
#define SNAN_MASK32 0x00400000
   IRTemp tmp = newTemp(Ity_I32);
   IRTemp mask = newTemp(Ity_I32);
   IRTemp is_SNAN = newTemp(Ity_I1);

   vassert( typeOfIRExpr(irsb->tyenv, src ) == Ity_I32 );
   assign(tmp, src);

   
   assign( is_SNAN,
           mkAND1( is_NaN_32( tmp ),
                   binop( Iop_CmpEQ32,
                          binop( Iop_And32, mkexpr( tmp ),
                                 mkU32( SNAN_MASK32 ) ),
                          mkU32( 0 ) ) ) );
   
   assign ( mask, binop( Iop_And32,
                         unop( Iop_1Sto32, mkexpr( is_SNAN ) ),
                         mkU32( SNAN_MASK32 ) ) );
   return binop( Iop_Or32, mkexpr( mask ), mkexpr( tmp) );
}


static IRTemp getNegatedResult(IRTemp intermediateResult)
{
   ULong signbit_mask = 0x8000000000000000ULL;
   IRTemp signbit_32 = newTemp(Ity_I32);
   IRTemp resultantSignbit = newTemp(Ity_I1);
   IRTemp negatedResult = newTemp(Ity_I64);
   assign( signbit_32, binop( Iop_Shr32,
                          unop( Iop_64HIto32,
                                 binop( Iop_And64, mkexpr( intermediateResult ),
                                        mkU64( signbit_mask ) ) ),
                                 mkU8( 31 ) ) );
   assign( resultantSignbit,
        unop( Iop_Not1,
              binop( Iop_CmpEQ32,
                     binop( Iop_Xor32,
                            mkexpr( signbit_32 ),
                            unop( Iop_1Uto32, is_NaN( intermediateResult ) ) ),
                     mkU32( 1 ) ) ) );

   assign( negatedResult,
        binop( Iop_Or64,
               binop( Iop_And64,
                      mkexpr( intermediateResult ),
                      mkU64( ~signbit_mask ) ),
               binop( Iop_32HLto64,
                      binop( Iop_Shl32,
                             unop( Iop_1Uto32, mkexpr( resultantSignbit ) ),
                             mkU8( 31 ) ),
                      mkU32( 0 ) ) ) );

   return negatedResult;
}

static IRTemp getNegatedResult_32(IRTemp intermediateResult)
{
   UInt signbit_mask = 0x80000000;
   IRTemp signbit_32 = newTemp(Ity_I32);
   IRTemp resultantSignbit = newTemp(Ity_I1);
   IRTemp negatedResult = newTemp(Ity_I32);
   assign( signbit_32, binop( Iop_Shr32,
                                 binop( Iop_And32, mkexpr( intermediateResult ),
                                        mkU32( signbit_mask ) ),
                                 mkU8( 31 ) ) );
   assign( resultantSignbit,
        unop( Iop_Not1,
              binop( Iop_CmpEQ32,
                     binop( Iop_Xor32,
                            mkexpr( signbit_32 ),
                            unop( Iop_1Uto32, is_NaN_32( intermediateResult ) ) ),
                     mkU32( 1 ) ) ) );

   assign( negatedResult,
           binop( Iop_Or32,
                  binop( Iop_And32,
                         mkexpr( intermediateResult ),
                         mkU32( ~signbit_mask ) ),
                  binop( Iop_Shl32,
                         unop( Iop_1Uto32, mkexpr( resultantSignbit ) ),
                         mkU8( 31 ) ) ) );

   return negatedResult;
}


static ULong generate_TMreason( UInt failure_code,
                                             UInt persistant,
                                             UInt nest_overflow,
                                             UInt tm_exact )
{
   ULong tm_err_code =
     ( (ULong) 0) << (63-6)   
     | ( (ULong) persistant) << (63-7)     
     | ( (ULong) 0) << (63-8)   
     | ( (ULong) nest_overflow) << (63-9)   
     | ( (ULong) 0) << (63-10)  
     | ( (ULong) 0) << (63-11)  
     | ( (ULong) 0) << (63-12)  
     | ( (ULong) 0) << (63-13)  
     | ( (ULong) 0) << (63-14)  
     | ( (ULong) 0) << (63-15)  
     | ( (ULong) 0) << (63-16)  
     | ( (ULong) 0) << (63-30)  
     | ( (ULong) 0) << (63-31)  
     | ( (ULong) 0) << (63-32)  
     | ( (ULong) 0) << (63-33)  
     | ( (ULong) 0) << (63-35)  
     | ( (ULong) 0) << (63-36)  
     | ( (ULong) tm_exact) << (63-37)  
     | ( (ULong) 0) << (63-38)  
     | ( (ULong) 0) << (63-51)  
     | ( (ULong) 0) << (63-63);  

     return tm_err_code;
}

static void storeTMfailure( Addr64 err_address, ULong tm_reason,
                            Addr64 handler_address )
{
   putGST( PPC_GST_TFIAR,   mkU64( err_address ) );
   putGST( PPC_GST_TEXASR,  mkU64( tm_reason ) );
   putGST( PPC_GST_TEXASRU, mkU32( 0 ) );
   putGST( PPC_GST_TFHAR,   mkU64( handler_address ) );
}


static Bool dis_int_arith ( UInt theInstr )
{
   
   UChar opc1    = ifieldOPC(theInstr);
   UChar rD_addr = ifieldRegDS(theInstr);
   UChar rA_addr = ifieldRegA(theInstr);
   UInt  uimm16  = ifieldUIMM16(theInstr);
   UChar rB_addr = ifieldRegB(theInstr);
   UChar flag_OE = ifieldBIT10(theInstr);
   UInt  opc2    = ifieldOPClo9(theInstr);
   UChar flag_rC = ifieldBIT0(theInstr);

   Long   simm16 = extend_s_16to64(uimm16);
   IRType ty     = mode64 ? Ity_I64 : Ity_I32;
   IRTemp rA     = newTemp(ty);
   IRTemp rB     = newTemp(ty);
   IRTemp rD     = newTemp(ty);

   Bool do_rc = False;

   assign( rA, getIReg(rA_addr) );
   assign( rB, getIReg(rB_addr) );         

   switch (opc1) {
   
   case 0x0C: 
      DIP("addic r%u,r%u,%d\n", rD_addr, rA_addr, (Int)simm16);
      assign( rD, binop( mkSzOp(ty, Iop_Add8), mkexpr(rA),
                         mkSzExtendS16(ty, uimm16) ) );
      set_XER_CA( ty, PPCG_FLAG_OP_ADD, 
                  mkexpr(rD), mkexpr(rA), mkSzExtendS16(ty, uimm16),
                  mkSzImm(ty, 0) );
      break;
    
   case 0x0D: 
      DIP("addic. r%u,r%u,%d\n", rD_addr, rA_addr, (Int)simm16);
      assign( rD, binop( mkSzOp(ty, Iop_Add8), mkexpr(rA),
                         mkSzExtendS16(ty, uimm16) ) );
      set_XER_CA( ty, PPCG_FLAG_OP_ADD, 
                  mkexpr(rD), mkexpr(rA), mkSzExtendS16(ty, uimm16),
                  mkSzImm(ty, 0) );
      do_rc = True;  
      flag_rC = 1;
      break;

   case 0x0E: 
      
      
      if ( rA_addr == 0 ) {
         DIP("li r%u,%d\n", rD_addr, (Int)simm16);
         assign( rD, mkSzExtendS16(ty, uimm16) );
      } else {
         DIP("addi r%u,r%u,%d\n", rD_addr, rA_addr, (Int)simm16);
         assign( rD, binop( mkSzOp(ty, Iop_Add8), mkexpr(rA),
                            mkSzExtendS16(ty, uimm16) ) );
      }
      break;

   case 0x0F: 
      
      if ( rA_addr == 0 ) {
         DIP("lis r%u,%d\n", rD_addr, (Int)simm16);
         assign( rD, mkSzExtendS32(ty, uimm16 << 16) );
      } else {
         DIP("addis r%u,r%u,0x%x\n", rD_addr, rA_addr, (Int)simm16);
         assign( rD, binop( mkSzOp(ty, Iop_Add8), mkexpr(rA),
                            mkSzExtendS32(ty, uimm16 << 16) ) );
      }
      break;

   case 0x07: 
      DIP("mulli r%u,r%u,%d\n", rD_addr, rA_addr, (Int)simm16);
      if (mode64)
         assign( rD, unop(Iop_128to64,
                          binop(Iop_MullS64, mkexpr(rA),
                                mkSzExtendS16(ty, uimm16))) );
      else
         assign( rD, unop(Iop_64to32,
                          binop(Iop_MullS32, mkexpr(rA),
                                mkSzExtendS16(ty, uimm16))) );
      break;

   case 0x08: 
      DIP("subfic r%u,r%u,%d\n", rD_addr, rA_addr, (Int)simm16);
      
      assign( rD, binop( mkSzOp(ty, Iop_Sub8),
                         mkSzExtendS16(ty, uimm16),
                         mkexpr(rA)) );
      set_XER_CA( ty, PPCG_FLAG_OP_SUBFI, 
                  mkexpr(rD), mkexpr(rA), mkSzExtendS16(ty, uimm16),
                  mkSzImm(ty, 0) );
      break;

   
   case 0x1F:
      do_rc = True;    
      
      switch (opc2) {
      case 0x10A: 
         DIP("add%s%s r%u,r%u,r%u\n",
             flag_OE ? "o" : "", flag_rC ? ".":"",
             rD_addr, rA_addr, rB_addr);
         assign( rD, binop( mkSzOp(ty, Iop_Add8),
                            mkexpr(rA), mkexpr(rB) ) );
         if (flag_OE) {
            set_XER_OV( ty, PPCG_FLAG_OP_ADD,
                        mkexpr(rD), mkexpr(rA), mkexpr(rB) );
         }
         break;

      case 0x00A: 
         DIP("addc%s%s r%u,r%u,r%u\n",
             flag_OE ? "o" : "", flag_rC ? ".":"",
             rD_addr, rA_addr, rB_addr);
         assign( rD, binop( mkSzOp(ty, Iop_Add8),
                            mkexpr(rA), mkexpr(rB)) );
         set_XER_CA( ty, PPCG_FLAG_OP_ADD, 
                     mkexpr(rD), mkexpr(rA), mkexpr(rB),
                     mkSzImm(ty, 0) );
         if (flag_OE) {
            set_XER_OV( ty, PPCG_FLAG_OP_ADD, 
                        mkexpr(rD), mkexpr(rA), mkexpr(rB) );
         }
         break;
         
      case 0x08A: { 
         IRTemp old_xer_ca = newTemp(ty);
         DIP("adde%s%s r%u,r%u,r%u\n",
             flag_OE ? "o" : "", flag_rC ? ".":"",
             rD_addr, rA_addr, rB_addr);
         
         assign( old_xer_ca, mkWidenFrom32(ty, getXER_CA32(), False) );
         assign( rD, binop( mkSzOp(ty, Iop_Add8), mkexpr(rA),
                            binop( mkSzOp(ty, Iop_Add8),
                                   mkexpr(rB), mkexpr(old_xer_ca))) );
         set_XER_CA( ty, PPCG_FLAG_OP_ADDE, 
                     mkexpr(rD), mkexpr(rA), mkexpr(rB),
                     mkexpr(old_xer_ca) );
         if (flag_OE) {
            set_XER_OV( ty, PPCG_FLAG_OP_ADDE, 
                        mkexpr(rD), mkexpr(rA), mkexpr(rB) );
         }
         break;
      }

      case 0x0EA: { 
         IRTemp old_xer_ca = newTemp(ty);
         IRExpr *min_one;
         if (rB_addr != 0) {
            vex_printf("dis_int_arith(ppc)(addme,rB_addr)\n");
            return False;
         }
         DIP("addme%s%s r%u,r%u,r%u\n",
             flag_OE ? "o" : "", flag_rC ? ".":"",
             rD_addr, rA_addr, rB_addr);
         
         
         assign( old_xer_ca, mkWidenFrom32(ty, getXER_CA32(), False) );
         min_one = mkSzImm(ty, (Long)-1);
         assign( rD, binop( mkSzOp(ty, Iop_Add8), mkexpr(rA),
                            binop( mkSzOp(ty, Iop_Add8),
                                   min_one, mkexpr(old_xer_ca)) ));
         set_XER_CA( ty, PPCG_FLAG_OP_ADDE,
                     mkexpr(rD), mkexpr(rA), min_one,
                     mkexpr(old_xer_ca) );
         if (flag_OE) {
            set_XER_OV( ty, PPCG_FLAG_OP_ADDE, 
                        mkexpr(rD), mkexpr(rA), min_one );
         }
         break;
      }

      case 0x0CA: { 
         IRTemp old_xer_ca = newTemp(ty);
         if (rB_addr != 0) {
            vex_printf("dis_int_arith(ppc)(addze,rB_addr)\n");
            return False;
         }
         DIP("addze%s%s r%u,r%u,r%u\n",
             flag_OE ? "o" : "", flag_rC ? ".":"",
             rD_addr, rA_addr, rB_addr);
         
         
         assign( old_xer_ca, mkWidenFrom32(ty, getXER_CA32(), False) );
         assign( rD, binop( mkSzOp(ty, Iop_Add8),
                            mkexpr(rA), mkexpr(old_xer_ca)) );
         set_XER_CA( ty, PPCG_FLAG_OP_ADDE, 
                     mkexpr(rD), mkexpr(rA), mkSzImm(ty, 0), 
                     mkexpr(old_xer_ca) );
         if (flag_OE) {
            set_XER_OV( ty, PPCG_FLAG_OP_ADDE, 
                        mkexpr(rD), mkexpr(rA), mkSzImm(ty, 0) );
         }
         break;
      }

      case 0x1EB: 
         DIP("divw%s%s r%u,r%u,r%u\n",
             flag_OE ? "o" : "", flag_rC ? ".":"",
             rD_addr, rA_addr, rB_addr);
         if (mode64) {
            IRExpr* dividend = mk64lo32Sto64( mkexpr(rA) );
            IRExpr* divisor  = mk64lo32Sto64( mkexpr(rB) );
            assign( rD, mk64lo32Uto64( binop(Iop_DivS64, dividend,
                                                         divisor) ) );
            if (flag_OE) {
               set_XER_OV( ty, PPCG_FLAG_OP_DIVW, 
                           mkexpr(rD), dividend, divisor );
            }
         } else {
            assign( rD, binop(Iop_DivS32, mkexpr(rA), mkexpr(rB)) );
            if (flag_OE) {
               set_XER_OV( ty, PPCG_FLAG_OP_DIVW, 
                           mkexpr(rD), mkexpr(rA), mkexpr(rB) );
            }
         }
         break;

      case 0x1CB: 
         DIP("divwu%s%s r%u,r%u,r%u\n",
             flag_OE ? "o" : "", flag_rC ? ".":"",
             rD_addr, rA_addr, rB_addr);
         if (mode64) {
            IRExpr* dividend = mk64lo32Uto64( mkexpr(rA) );
            IRExpr* divisor  = mk64lo32Uto64( mkexpr(rB) );
            assign( rD, mk64lo32Uto64( binop(Iop_DivU64, dividend,
                                                         divisor) ) );
            if (flag_OE) {
               set_XER_OV( ty, PPCG_FLAG_OP_DIVWU, 
                           mkexpr(rD), dividend, divisor );
            }
         } else {
            assign( rD, binop(Iop_DivU32, mkexpr(rA), mkexpr(rB)) );
            if (flag_OE) {
               set_XER_OV( ty, PPCG_FLAG_OP_DIVWU, 
                           mkexpr(rD), mkexpr(rA), mkexpr(rB) );
            }
         }
         
         break;

      case 0x04B: 
         if (flag_OE != 0) {
            vex_printf("dis_int_arith(ppc)(mulhw,flag_OE)\n");
            return False;
         }
         DIP("mulhw%s r%u,r%u,r%u\n", flag_rC ? ".":"",
             rD_addr, rA_addr, rB_addr);
         if (mode64) {
            assign( rD, binop(Iop_Sar64,
                           binop(Iop_Mul64,
                                 mk64lo32Sto64( mkexpr(rA) ),
                                 mk64lo32Sto64( mkexpr(rB) )),
                              mkU8(32)) );
         } else {
            assign( rD, unop(Iop_64HIto32,
                             binop(Iop_MullS32,
                                   mkexpr(rA), mkexpr(rB))) );
         }
         break;

      case 0x00B: 
         if (flag_OE != 0) {
            vex_printf("dis_int_arith(ppc)(mulhwu,flag_OE)\n");
            return False;
         }
         DIP("mulhwu%s r%u,r%u,r%u\n", flag_rC ? ".":"",
             rD_addr, rA_addr, rB_addr);
         if (mode64) {
            assign( rD, binop(Iop_Sar64,
                           binop(Iop_Mul64,
                                 mk64lo32Uto64( mkexpr(rA) ),
                                 mk64lo32Uto64( mkexpr(rB) ) ),
                              mkU8(32)) );
         } else {
            assign( rD, unop(Iop_64HIto32, 
                             binop(Iop_MullU32,
                                   mkexpr(rA), mkexpr(rB))) );
         }
         break;
         
      case 0x0EB: 
         DIP("mullw%s%s r%u,r%u,r%u\n",
             flag_OE ? "o" : "", flag_rC ? ".":"",
             rD_addr, rA_addr, rB_addr);
         if (mode64) {
            IRExpr *a = unop(Iop_64to32, mkexpr(rA) );
            IRExpr *b = unop(Iop_64to32, mkexpr(rB) );
            assign( rD, binop(Iop_MullS32, a, b) );
            if (flag_OE) {
               set_XER_OV( ty, PPCG_FLAG_OP_MULLW, 
                           mkexpr(rD),
                           unop(Iop_32Uto64, a), unop(Iop_32Uto64, b) );
            }
         } else {
            assign( rD, unop(Iop_64to32,
                             binop(Iop_MullU32,
                                   mkexpr(rA), mkexpr(rB))) );
            if (flag_OE) {
               set_XER_OV( ty, PPCG_FLAG_OP_MULLW, 
                           mkexpr(rD), mkexpr(rA), mkexpr(rB) );
            }
         }
         break;

      case 0x068: 
         if (rB_addr != 0) {
            vex_printf("dis_int_arith(ppc)(neg,rB_addr)\n");
            return False;
         }
         DIP("neg%s%s r%u,r%u\n",
             flag_OE ? "o" : "", flag_rC ? ".":"",
             rD_addr, rA_addr);
         
         assign( rD, binop( mkSzOp(ty, Iop_Add8),
                            unop( mkSzOp(ty, Iop_Not8), mkexpr(rA) ),
                            mkSzImm(ty, 1)) );
         if (flag_OE) {
            set_XER_OV( ty, PPCG_FLAG_OP_NEG, 
                        mkexpr(rD), mkexpr(rA), mkexpr(rB) );
         }
         break;

      case 0x028: 
         DIP("subf%s%s r%u,r%u,r%u\n",
             flag_OE ? "o" : "", flag_rC ? ".":"",
             rD_addr, rA_addr, rB_addr);
         
         assign( rD, binop( mkSzOp(ty, Iop_Sub8),
                            mkexpr(rB), mkexpr(rA)) );
         if (flag_OE) {
            set_XER_OV( ty, PPCG_FLAG_OP_SUBF, 
                        mkexpr(rD), mkexpr(rA), mkexpr(rB) );
         }
         break;

      case 0x008: 
         DIP("subfc%s%s r%u,r%u,r%u\n",
             flag_OE ? "o" : "", flag_rC ? ".":"",
             rD_addr, rA_addr, rB_addr);
         
         assign( rD, binop( mkSzOp(ty, Iop_Sub8),
                            mkexpr(rB), mkexpr(rA)) );
         set_XER_CA( ty, PPCG_FLAG_OP_SUBFC, 
                     mkexpr(rD), mkexpr(rA), mkexpr(rB),
                     mkSzImm(ty, 0) );
         if (flag_OE) {
            set_XER_OV( ty, PPCG_FLAG_OP_SUBFC, 
                        mkexpr(rD), mkexpr(rA), mkexpr(rB) );
         }
         break;
         
      case 0x088: {
         IRTemp old_xer_ca = newTemp(ty);
         DIP("subfe%s%s r%u,r%u,r%u\n",
             flag_OE ? "o" : "", flag_rC ? ".":"",
             rD_addr, rA_addr, rB_addr);
         
         assign( old_xer_ca, mkWidenFrom32(ty, getXER_CA32(), False) );
         assign( rD, binop( mkSzOp(ty, Iop_Add8),
                            unop( mkSzOp(ty, Iop_Not8), mkexpr(rA)),
                            binop( mkSzOp(ty, Iop_Add8),
                                   mkexpr(rB), mkexpr(old_xer_ca))) );
         set_XER_CA( ty, PPCG_FLAG_OP_SUBFE, 
                     mkexpr(rD), mkexpr(rA), mkexpr(rB), 
                     mkexpr(old_xer_ca) );
         if (flag_OE) {
            set_XER_OV( ty, PPCG_FLAG_OP_SUBFE, 
                        mkexpr(rD), mkexpr(rA), mkexpr(rB) );
         }
         break;
      }

      case 0x0E8: { 
         IRTemp old_xer_ca = newTemp(ty);
         IRExpr *min_one;
         if (rB_addr != 0) {
            vex_printf("dis_int_arith(ppc)(subfme,rB_addr)\n");
            return False;
         }
         DIP("subfme%s%s r%u,r%u\n",
             flag_OE ? "o" : "", flag_rC ? ".":"",
             rD_addr, rA_addr);
         
         
         assign( old_xer_ca, mkWidenFrom32(ty, getXER_CA32(), False) );
         min_one = mkSzImm(ty, (Long)-1);
         assign( rD, binop( mkSzOp(ty, Iop_Add8),
                            unop( mkSzOp(ty, Iop_Not8), mkexpr(rA)),
                            binop( mkSzOp(ty, Iop_Add8),
                                   min_one, mkexpr(old_xer_ca))) );
         set_XER_CA( ty, PPCG_FLAG_OP_SUBFE,
                     mkexpr(rD), mkexpr(rA), min_one,
                     mkexpr(old_xer_ca) );
         if (flag_OE) {
            set_XER_OV( ty, PPCG_FLAG_OP_SUBFE, 
                        mkexpr(rD), mkexpr(rA), min_one );
         }
         break;
      }

      case 0x0C8: { 
         IRTemp old_xer_ca = newTemp(ty);
         if (rB_addr != 0) {
            vex_printf("dis_int_arith(ppc)(subfze,rB_addr)\n");
            return False;
         }
         DIP("subfze%s%s r%u,r%u\n",
             flag_OE ? "o" : "", flag_rC ? ".":"",
             rD_addr, rA_addr);
         
         
         assign( old_xer_ca, mkWidenFrom32(ty, getXER_CA32(), False) );
         assign( rD, binop( mkSzOp(ty, Iop_Add8),
                           unop( mkSzOp(ty, Iop_Not8),
                                 mkexpr(rA)), mkexpr(old_xer_ca)) );
         set_XER_CA( ty, PPCG_FLAG_OP_SUBFE,
                     mkexpr(rD), mkexpr(rA), mkSzImm(ty, 0), 
                     mkexpr(old_xer_ca) );
         if (flag_OE) {
            set_XER_OV( ty, PPCG_FLAG_OP_SUBFE,
                        mkexpr(rD), mkexpr(rA), mkSzImm(ty, 0) );
         }
         break;
      }


      
      case 0x49:  
         if (flag_OE != 0) {
            vex_printf("dis_int_arith(ppc)(mulhd,flagOE)\n");
            return False;
         }
         DIP("mulhd%s r%u,r%u,r%u\n", flag_rC ? ".":"",
             rD_addr, rA_addr, rB_addr);
         assign( rD, unop(Iop_128HIto64, 
                          binop(Iop_MullS64,
                                mkexpr(rA), mkexpr(rB))) );

         break;

      case 0x9:   
         if (flag_OE != 0) {
            vex_printf("dis_int_arith(ppc)(mulhdu,flagOE)\n");
            return False;
         }
         DIP("mulhdu%s r%u,r%u,r%u\n", flag_rC ? ".":"",
             rD_addr, rA_addr, rB_addr);
         assign( rD, unop(Iop_128HIto64, 
                          binop(Iop_MullU64,
                                mkexpr(rA), mkexpr(rB))) );
         break;

      case 0xE9:  
         DIP("mulld%s%s r%u,r%u,r%u\n",
             flag_OE ? "o" : "", flag_rC ? ".":"",
             rD_addr, rA_addr, rB_addr);
         assign( rD, binop(Iop_Mul64, mkexpr(rA), mkexpr(rB)) );
         if (flag_OE) {
            set_XER_OV( ty, PPCG_FLAG_OP_MULLD, 
                        mkexpr(rD), mkexpr(rA), mkexpr(rB) );
         }
         break;

      case 0x1E9: 
         DIP("divd%s%s r%u,r%u,r%u\n",
             flag_OE ? "o" : "", flag_rC ? ".":"",
             rD_addr, rA_addr, rB_addr);
         assign( rD, binop(Iop_DivS64, mkexpr(rA), mkexpr(rB)) );
         if (flag_OE) {
            set_XER_OV( ty, PPCG_FLAG_OP_DIVW, 
                        mkexpr(rD), mkexpr(rA), mkexpr(rB) );
         }
         break;

      case 0x1C9: 
         DIP("divdu%s%s r%u,r%u,r%u\n",
             flag_OE ? "o" : "", flag_rC ? ".":"",
             rD_addr, rA_addr, rB_addr);
         assign( rD, binop(Iop_DivU64, mkexpr(rA), mkexpr(rB)) );
         if (flag_OE) {
            set_XER_OV( ty, PPCG_FLAG_OP_DIVWU, 
                        mkexpr(rD), mkexpr(rA), mkexpr(rB) );
         }
         break;
         

      case 0x18B: 
      {
         IRTemp res = newTemp(Ity_I32);
         IRExpr * dividend, * divisor;
         DIP("divweu%s%s r%u,r%u,r%u\n",
             flag_OE ? "o" : "", flag_rC ? ".":"",
                                         rD_addr, rA_addr, rB_addr);
         if (mode64) {
            dividend = unop( Iop_64to32, mkexpr( rA ) );
            divisor = unop( Iop_64to32, mkexpr( rB ) );
            assign( res, binop( Iop_DivU32E, dividend, divisor ) );
            assign( rD, binop( Iop_32HLto64, mkU32( 0 ), mkexpr( res ) ) );
         } else {
            dividend = mkexpr( rA );
            divisor =  mkexpr( rB );
            assign( res, binop( Iop_DivU32E, dividend, divisor ) );
            assign( rD, mkexpr( res) );
         }

         if (flag_OE) {
            set_XER_OV_32( PPCG_FLAG_OP_DIVWEU,
                           mkexpr(res), dividend, divisor );
         }
         break;
      }

      case 0x1AB: 
      {

         IRTemp res = newTemp(Ity_I32);
         IRExpr * dividend, * divisor;
         DIP("divwe%s%s r%u,r%u,r%u\n",
             flag_OE ? "o" : "", flag_rC ? ".":"",
                                         rD_addr, rA_addr, rB_addr);
         if (mode64) {
            dividend = unop( Iop_64to32, mkexpr( rA ) );
            divisor = unop( Iop_64to32, mkexpr( rB ) );
            assign( res, binop( Iop_DivS32E, dividend, divisor ) );
            assign( rD, binop( Iop_32HLto64, mkU32( 0 ), mkexpr( res ) ) );
         } else {
            dividend = mkexpr( rA );
            divisor =  mkexpr( rB );
            assign( res, binop( Iop_DivS32E, dividend, divisor ) );
            assign( rD, mkexpr( res) );
         }

         if (flag_OE) {
            set_XER_OV_32( PPCG_FLAG_OP_DIVWE,
                           mkexpr(res), dividend, divisor );
         }
         break;
      }


      case 0x1A9: 
         DIP("divde%s%s r%u,r%u,r%u\n",
             flag_OE ? "o" : "", flag_rC ? ".":"",
             rD_addr, rA_addr, rB_addr);
         assign( rD, binop(Iop_DivS64E, mkexpr(rA), mkexpr(rB)) );
         if (flag_OE) {
            set_XER_OV_64( PPCG_FLAG_OP_DIVDE, mkexpr( rD ),
                           mkexpr( rA ), mkexpr( rB ) );
         }
         break;

      case 0x189: 
        
        DIP("divdeu%s%s r%u,r%u,r%u\n",
            flag_OE ? "o" : "", flag_rC ? ".":"",
            rD_addr, rA_addr, rB_addr);
        assign( rD, binop(Iop_DivU64E, mkexpr(rA), mkexpr(rB)) );
        if (flag_OE) {
           set_XER_OV_64( PPCG_FLAG_OP_DIVDEU, mkexpr( rD ),
                          mkexpr( rA ), mkexpr( rB ) );
        }
        break;

      default:
         vex_printf("dis_int_arith(ppc)(opc2)\n");
         return False;
      }
      break;

   default:
      vex_printf("dis_int_arith(ppc)(opc1)\n");
      return False;
   }

   putIReg( rD_addr, mkexpr(rD) );

   if (do_rc && flag_rC) {
      set_CR0( mkexpr(rD) );
   }
   return True;
}



static Bool dis_int_cmp ( UInt theInstr )
{
   
   UChar opc1    = ifieldOPC(theInstr);
   UChar crfD    = toUChar( IFIELD( theInstr, 23, 3 ) );
   UChar b22     = toUChar( IFIELD( theInstr, 22, 1 ) );
   UChar flag_L  = toUChar( IFIELD( theInstr, 21, 1 ) );
   UChar rA_addr = ifieldRegA(theInstr);
   UInt  uimm16  = ifieldUIMM16(theInstr);
   UChar rB_addr = ifieldRegB(theInstr);
   UInt  opc2    = ifieldOPClo10(theInstr);
   UChar b0      = ifieldBIT0(theInstr);

   IRType ty = mode64 ? Ity_I64 : Ity_I32;
   IRExpr *a = getIReg(rA_addr);
   IRExpr *b;

   if (!mode64 && flag_L==1) {  
      vex_printf("dis_int_cmp(ppc)(flag_L)\n");
      return False;
   }
   
   if (b22 != 0) {
      vex_printf("dis_int_cmp(ppc)(b22)\n");
      return False;
   }
   
   switch (opc1) {
   case 0x0B: 
      DIP("cmpi cr%u,%u,r%u,%d\n", crfD, flag_L, rA_addr,
          (Int)extend_s_16to32(uimm16));
      b = mkSzExtendS16( ty, uimm16 );
      if (flag_L == 1) {
         putCR321(crfD, unop(Iop_64to8, binop(Iop_CmpORD64S, a, b)));
      } else {
         a = mkNarrowTo32( ty, a );
         b = mkNarrowTo32( ty, b );
         putCR321(crfD, unop(Iop_32to8, binop(Iop_CmpORD32S, a, b)));
      }
      putCR0( crfD, getXER_SO() );
      break;
      
   case 0x0A: 
      DIP("cmpli cr%u,%u,r%u,0x%x\n", crfD, flag_L, rA_addr, uimm16);
      b = mkSzImm( ty, uimm16 );
      if (flag_L == 1) {
         putCR321(crfD, unop(Iop_64to8, binop(Iop_CmpORD64U, a, b)));
      } else {
         a = mkNarrowTo32( ty, a );
         b = mkNarrowTo32( ty, b );
         putCR321(crfD, unop(Iop_32to8, binop(Iop_CmpORD32U, a, b)));
      }
      putCR0( crfD, getXER_SO() );
      break;
      
   
   case 0x1F:
      if (b0 != 0) {
         vex_printf("dis_int_cmp(ppc)(0x1F,b0)\n");
         return False;
      }
      b = getIReg(rB_addr);

      switch (opc2) {
      case 0x000: 
         DIP("cmp cr%u,%u,r%u,r%u\n", crfD, flag_L, rA_addr, rB_addr);
         if (rA_addr == rB_addr)
            a = b = typeOfIRExpr(irsb->tyenv,a) == Ity_I64
                    ? mkU64(0)  : mkU32(0);
         if (flag_L == 1) {
            putCR321(crfD, unop(Iop_64to8, binop(Iop_CmpORD64S, a, b)));
         } else {
            a = mkNarrowTo32( ty, a );
            b = mkNarrowTo32( ty, b );
            putCR321(crfD, unop(Iop_32to8,binop(Iop_CmpORD32S, a, b)));
         }
         putCR0( crfD, getXER_SO() );
         break;
         
      case 0x020: 
         DIP("cmpl cr%u,%u,r%u,r%u\n", crfD, flag_L, rA_addr, rB_addr);
         if (rA_addr == rB_addr)
            a = b = typeOfIRExpr(irsb->tyenv,a) == Ity_I64
                    ? mkU64(0)  : mkU32(0);
         if (flag_L == 1) {
            putCR321(crfD, unop(Iop_64to8, binop(Iop_CmpORD64U, a, b)));
         } else {
            a = mkNarrowTo32( ty, a );
            b = mkNarrowTo32( ty, b );
            putCR321(crfD, unop(Iop_32to8, binop(Iop_CmpORD32U, a, b)));
         }
         putCR0( crfD, getXER_SO() );
         break;

      default:
         vex_printf("dis_int_cmp(ppc)(opc2)\n");
         return False;
      }
      break;
      
   default:
      vex_printf("dis_int_cmp(ppc)(opc1)\n");
      return False;
   }
   
   return True;
}


static Bool dis_int_logic ( UInt theInstr )
{
   
   UChar opc1    = ifieldOPC(theInstr);
   UChar rS_addr = ifieldRegDS(theInstr);
   UChar rA_addr = ifieldRegA(theInstr);
   UInt  uimm16  = ifieldUIMM16(theInstr);
   UChar rB_addr = ifieldRegB(theInstr);
   UInt  opc2    = ifieldOPClo10(theInstr);
   UChar flag_rC = ifieldBIT0(theInstr);
   
   IRType ty     = mode64 ? Ity_I64 : Ity_I32;
   IRTemp rS     = newTemp(ty);
   IRTemp rA     = newTemp(ty);
   IRTemp rB     = newTemp(ty);
   IRExpr* irx;
   Bool do_rc    = False;

   assign( rS, getIReg(rS_addr) );
   assign( rB, getIReg(rB_addr) );
   
   switch (opc1) {
   case 0x1C: 
      DIP("andi. r%u,r%u,0x%x\n", rA_addr, rS_addr, uimm16);
      assign( rA, binop( mkSzOp(ty, Iop_And8), mkexpr(rS),
                         mkSzImm(ty, uimm16)) );
      do_rc = True;  
      flag_rC = 1;
      break;
      
   case 0x1D: 
      DIP("andis r%u,r%u,0x%x\n", rA_addr, rS_addr, uimm16);
      assign( rA, binop( mkSzOp(ty, Iop_And8), mkexpr(rS),
                         mkSzImm(ty, uimm16 << 16)) );
      do_rc = True;  
      flag_rC = 1;
      break;

   case 0x18: 
      DIP("ori r%u,r%u,0x%x\n", rA_addr, rS_addr, uimm16);
      assign( rA, binop( mkSzOp(ty, Iop_Or8), mkexpr(rS),
                         mkSzImm(ty, uimm16)) );
      break;

   case 0x19: 
      DIP("oris r%u,r%u,0x%x\n", rA_addr, rS_addr, uimm16);
      assign( rA, binop( mkSzOp(ty, Iop_Or8), mkexpr(rS),
                         mkSzImm(ty, uimm16 << 16)) );
      break;

   case 0x1A: 
      DIP("xori r%u,r%u,0x%x\n", rA_addr, rS_addr, uimm16);
      assign( rA, binop( mkSzOp(ty, Iop_Xor8), mkexpr(rS),
                         mkSzImm(ty, uimm16)) );
      break;

   case 0x1B: 
      DIP("xoris r%u,r%u,0x%x\n", rA_addr, rS_addr, uimm16);
      assign( rA, binop( mkSzOp(ty, Iop_Xor8), mkexpr(rS),
                         mkSzImm(ty, uimm16 << 16)) );
      break;

   
   case 0x1F:
      do_rc = True; 

      switch (opc2) {
      case 0x01C: 
         DIP("and%s r%u,r%u,r%u\n",
             flag_rC ? ".":"", rA_addr, rS_addr, rB_addr);
         assign(rA, binop( mkSzOp(ty, Iop_And8),
                           mkexpr(rS), mkexpr(rB)));
         break;
         
      case 0x03C: 
         DIP("andc%s r%u,r%u,r%u\n",
             flag_rC ? ".":"", rA_addr, rS_addr, rB_addr);
         assign(rA, binop( mkSzOp(ty, Iop_And8), mkexpr(rS),
                           unop( mkSzOp(ty, Iop_Not8),
                                 mkexpr(rB))));
         break;
         
      case 0x01A: { 
         IRExpr* lo32;
         if (rB_addr!=0) {
            vex_printf("dis_int_logic(ppc)(cntlzw,rB_addr)\n");
            return False;
         }
         DIP("cntlzw%s r%u,r%u\n",
             flag_rC ? ".":"", rA_addr, rS_addr);
         
         
         lo32 = mode64 ? unop(Iop_64to32, mkexpr(rS)) : mkexpr(rS);
         
         
         irx =  binop(Iop_CmpNE32, lo32, mkU32(0));
         assign(rA, mkWidenFrom32(ty,
                         IRExpr_ITE( irx,
                                     unop(Iop_Clz32, lo32),
                                     mkU32(32)),
                         False));

         
         break;
      }
         
      case 0x11C: 
         DIP("eqv%s r%u,r%u,r%u\n",
             flag_rC ? ".":"", rA_addr, rS_addr, rB_addr);
         assign( rA, unop( mkSzOp(ty, Iop_Not8),
                           binop( mkSzOp(ty, Iop_Xor8),
                                  mkexpr(rS), mkexpr(rB))) );
         break;

      case 0x3BA: 
         if (rB_addr!=0) {
            vex_printf("dis_int_logic(ppc)(extsb,rB_addr)\n");
            return False;
         }
         DIP("extsb%s r%u,r%u\n",
             flag_rC ? ".":"", rA_addr, rS_addr);
         if (mode64)
            assign( rA, unop(Iop_8Sto64, unop(Iop_64to8, mkexpr(rS))) );
         else
            assign( rA, unop(Iop_8Sto32, unop(Iop_32to8, mkexpr(rS))) );
         break;

      case 0x39A: 
         if (rB_addr!=0) {
            vex_printf("dis_int_logic(ppc)(extsh,rB_addr)\n");
            return False;
         }
         DIP("extsh%s r%u,r%u\n",
             flag_rC ? ".":"", rA_addr, rS_addr);
         if (mode64)
            assign( rA, unop(Iop_16Sto64,
                             unop(Iop_64to16, mkexpr(rS))) );
         else
            assign( rA, unop(Iop_16Sto32,
                             unop(Iop_32to16, mkexpr(rS))) );
         break;

      case 0x1DC: 
         DIP("nand%s r%u,r%u,r%u\n",
             flag_rC ? ".":"", rA_addr, rS_addr, rB_addr);
         assign( rA, unop( mkSzOp(ty, Iop_Not8),
                           binop( mkSzOp(ty, Iop_And8),
                                  mkexpr(rS), mkexpr(rB))) );
         break;
         
      case 0x07C: 
         DIP("nor%s r%u,r%u,r%u\n",
             flag_rC ? ".":"", rA_addr, rS_addr, rB_addr);
         assign( rA, unop( mkSzOp(ty, Iop_Not8),
                           binop( mkSzOp(ty, Iop_Or8),
                                  mkexpr(rS), mkexpr(rB))) );
         break;

      case 0x1BC: 
         if ((!flag_rC) && rS_addr == rB_addr) {
            DIP("mr r%u,r%u\n", rA_addr, rS_addr);
            assign( rA, mkexpr(rS) );
         } else {
            DIP("or%s r%u,r%u,r%u\n",
                flag_rC ? ".":"", rA_addr, rS_addr, rB_addr);
            assign( rA, binop( mkSzOp(ty, Iop_Or8),
                               mkexpr(rS), mkexpr(rB)) );
         }
         break;

      case 0x19C: 
         DIP("orc%s r%u,r%u,r%u\n",
             flag_rC ? ".":"", rA_addr, rS_addr, rB_addr);
         assign( rA, binop( mkSzOp(ty, Iop_Or8), mkexpr(rS),
                            unop(mkSzOp(ty, Iop_Not8), mkexpr(rB))));
         break;
         
      case 0x13C: 
         DIP("xor%s r%u,r%u,r%u\n",
             flag_rC ? ".":"", rA_addr, rS_addr, rB_addr);
         assign( rA, binop( mkSzOp(ty, Iop_Xor8),
                            mkexpr(rS), mkexpr(rB)) );
         break;


      
      case 0x3DA: 
         if (rB_addr!=0) {
            vex_printf("dis_int_logic(ppc)(extsw,rB_addr)\n");
            return False;
         }
         DIP("extsw%s r%u,r%u\n", flag_rC ? ".":"", rA_addr, rS_addr);
         assign(rA, unop(Iop_32Sto64, unop(Iop_64to32, mkexpr(rS))));
         break;

      case 0x03A: 
         if (rB_addr!=0) {
            vex_printf("dis_int_logic(ppc)(cntlzd,rB_addr)\n");
            return False;
         }
         DIP("cntlzd%s r%u,r%u\n",
             flag_rC ? ".":"", rA_addr, rS_addr);
         
         irx =  binop(Iop_CmpNE64, mkexpr(rS), mkU64(0));
         assign(rA, IRExpr_ITE( irx,
                                unop(Iop_Clz64, mkexpr(rS)),
                                mkU64(64) ));
         
         break;

      case 0x1FC: 
         DIP("cmpb r%u,r%u,r%u\n", rA_addr, rS_addr, rB_addr);

         if (mode64)
            assign( rA, unop( Iop_V128to64,
                              binop( Iop_CmpEQ8x16,
                                     binop( Iop_64HLtoV128, mkU64(0), mkexpr(rS) ),
                                     binop( Iop_64HLtoV128, mkU64(0), mkexpr(rB) )
                                     )) );
         else
            assign( rA, unop( Iop_V128to32,
                              binop( Iop_CmpEQ8x16,
                                     unop( Iop_32UtoV128, mkexpr(rS) ),
                                     unop( Iop_32UtoV128, mkexpr(rB) )
                                     )) );
         break;

      case 0x2DF: { 
         IRTemp frB = newTemp(Ity_F64);
         DIP("mftgpr r%u,fr%u\n", rS_addr, rB_addr);

         assign( frB, getFReg(rB_addr));  
         if (mode64)
            assign( rA, unop( Iop_ReinterpF64asI64, mkexpr(frB)) );
         else
            assign( rA, unop( Iop_64to32, unop( Iop_ReinterpF64asI64, mkexpr(frB))) );

         putIReg( rS_addr, mkexpr(rA));
         return True;
      }

      case 0x25F: { 
         IRTemp frA = newTemp(Ity_F64);
         DIP("mffgpr fr%u,r%u\n", rS_addr, rB_addr);

         if (mode64)
            assign( frA, unop( Iop_ReinterpI64asF64, mkexpr(rB)) );
         else
            assign( frA, unop( Iop_ReinterpI64asF64, unop( Iop_32Uto64, mkexpr(rB))) );

         putFReg( rS_addr, mkexpr(frA));
         return True;
      }
      case 0x1FA: 
      {
    	  DIP("popcntd r%u,r%u\n", rA_addr, rS_addr);
    	  IRTemp result = gen_POPCOUNT(ty, rS, DWORD);
    	  putIReg( rA_addr, mkexpr(result) );
    	  return True;
      }
      case 0x17A: 
      {
         DIP("popcntw r%u,r%u\n", rA_addr, rS_addr);
         if (mode64) {
            IRTemp resultHi, resultLo;
            IRTemp argLo = newTemp(Ity_I32);
            IRTemp argHi = newTemp(Ity_I32);
            assign(argLo, unop(Iop_64to32, mkexpr(rS)));
            assign(argHi, unop(Iop_64HIto32, mkexpr(rS)));
            resultLo = gen_POPCOUNT(Ity_I32, argLo, WORD);
            resultHi = gen_POPCOUNT(Ity_I32, argHi, WORD);
            putIReg( rA_addr, binop(Iop_32HLto64, mkexpr(resultHi), mkexpr(resultLo)));
         } else {
            IRTemp result = gen_POPCOUNT(ty, rS, WORD);
            putIReg( rA_addr, mkexpr(result) );
         }
         return True;
      }
      case 0x7A: 
      {
         DIP("popcntb r%u,r%u\n", rA_addr, rS_addr);

         if (mode64) {
            IRTemp resultHi, resultLo;
            IRTemp argLo = newTemp(Ity_I32);
            IRTemp argHi = newTemp(Ity_I32);
            assign(argLo, unop(Iop_64to32, mkexpr(rS)));
            assign(argHi, unop(Iop_64HIto32, mkexpr(rS)));
            resultLo = gen_POPCOUNT(Ity_I32, argLo, BYTE);
            resultHi = gen_POPCOUNT(Ity_I32, argHi, BYTE);
            putIReg( rA_addr, binop(Iop_32HLto64, mkexpr(resultHi),
                                    mkexpr(resultLo)));
         } else {
            IRTemp result = gen_POPCOUNT(ty, rS, BYTE);
            putIReg( rA_addr, mkexpr(result) );
         }
         return True;
      }
       case 0x0FC: 
       {
 #define BPERMD_IDX_MASK 0x00000000000000FFULL
 #define BPERMD_BIT_MASK 0x8000000000000000ULL
          int i;
          IRExpr * rS_expr = mkexpr(rS);
          IRExpr * res = binop(Iop_And64, mkU64(0), mkU64(0));
          DIP("bpermd r%u,r%u,r%u\n", rA_addr, rS_addr, rB_addr);
          for (i = 0; i < 8; i++) {
             IRTemp idx_tmp = newTemp( Ity_I64 );
             IRTemp perm_bit = newTemp( Ity_I64 );
             IRTemp idx = newTemp( Ity_I8 );
             IRTemp idx_LT64 = newTemp( Ity_I1 );
             IRTemp idx_LT64_ity64 = newTemp( Ity_I64 );

             assign( idx_tmp,
                     binop( Iop_And64, mkU64( BPERMD_IDX_MASK ), rS_expr ) );
             assign( idx_LT64,
                           binop( Iop_CmpLT64U, mkexpr( idx_tmp ), mkU64( 64 ) ) );
             assign( idx,
                           binop( Iop_And8,
                                  unop( Iop_1Sto8,
                                        mkexpr(idx_LT64) ),
                                  unop( Iop_64to8, mkexpr( idx_tmp ) ) ) );
             assign( idx_LT64_ity64,
                           unop( Iop_32Uto64, unop( Iop_1Uto32, mkexpr(idx_LT64 ) ) ) );
             assign( perm_bit,
                           binop( Iop_And64,
                                  mkexpr( idx_LT64_ity64 ),
                                  binop( Iop_Shr64,
                                         binop( Iop_And64,
                                                mkU64( BPERMD_BIT_MASK ),
                                                binop( Iop_Shl64,
                                                       mkexpr( rB ),
                                                       mkexpr( idx ) ) ),
                                         mkU8( 63 ) ) ) );
             res = binop( Iop_Or64,
                                res,
                                binop( Iop_Shl64,
                                       mkexpr( perm_bit ),
                                       mkU8( i ) ) );
             rS_expr = binop( Iop_Shr64, rS_expr, mkU8( 8 ) );
          }
          putIReg(rA_addr, res);
          return True;
       }

      default:
         vex_printf("dis_int_logic(ppc)(opc2)\n");
         return False;
      }
      break;
      
   default:
      vex_printf("dis_int_logic(ppc)(opc1)\n");
      return False;
   }

   putIReg( rA_addr, mkexpr(rA) );

   if (do_rc && flag_rC) {
      set_CR0( mkexpr(rA) );
   }
   return True;
}

static Bool dis_int_parity ( UInt theInstr )
{
   
   UChar opc1    = ifieldOPC(theInstr);
   UChar rS_addr = ifieldRegDS(theInstr);
   UChar rA_addr = ifieldRegA(theInstr);
   UChar rB_addr = ifieldRegB(theInstr);
   UInt  opc2    = ifieldOPClo10(theInstr);
   UChar b0      = ifieldBIT0(theInstr);
   IRType ty     = mode64 ? Ity_I64 : Ity_I32;

   IRTemp rS     = newTemp(ty);
   IRTemp rA     = newTemp(ty);
   IRTemp iTot1  = newTemp(Ity_I32);
   IRTemp iTot2  = newTemp(Ity_I32);
   IRTemp iTot3  = newTemp(Ity_I32);
   IRTemp iTot4  = newTemp(Ity_I32);
   IRTemp iTot5  = newTemp(Ity_I32);
   IRTemp iTot6  = newTemp(Ity_I32);
   IRTemp iTot7  = newTemp(Ity_I32);
   IRTemp iTot8  = newTemp(Ity_I32);
   IRTemp rS1    = newTemp(ty);
   IRTemp rS2    = newTemp(ty);
   IRTemp rS3    = newTemp(ty);
   IRTemp rS4    = newTemp(ty);
   IRTemp rS5    = newTemp(ty);
   IRTemp rS6    = newTemp(ty);
   IRTemp rS7    = newTemp(ty);
   IRTemp iHi    = newTemp(Ity_I32);
   IRTemp iLo    = newTemp(Ity_I32);
   IROp to_bit   = (mode64 ? Iop_64to1 : Iop_32to1);
   IROp shr_op   = (mode64 ? Iop_Shr64 : Iop_Shr32);

   if (opc1 != 0x1f || rB_addr || b0) {
      vex_printf("dis_int_parity(ppc)(0x1F,opc1:rB|b0)\n");
      return False;
   }

   assign( rS, getIReg(rS_addr) );

   switch (opc2) {
   case 0xba:  
      DIP("prtyd r%u,r%u\n", rA_addr, rS_addr);
      assign( iTot1, unop(Iop_1Uto32, unop(to_bit, mkexpr(rS))) );
      assign( rS1, binop(shr_op, mkexpr(rS), mkU8(8)) );
      assign( iTot2, binop(Iop_Add32,
                           unop(Iop_1Uto32, unop(to_bit, mkexpr(rS1))),
                           mkexpr(iTot1)) );
      assign( rS2, binop(shr_op, mkexpr(rS1), mkU8(8)) );
      assign( iTot3, binop(Iop_Add32,
                           unop(Iop_1Uto32, unop(to_bit, mkexpr(rS2))),
                           mkexpr(iTot2)) );
      assign( rS3, binop(shr_op, mkexpr(rS2), mkU8(8)) );
      assign( iTot4, binop(Iop_Add32,
                           unop(Iop_1Uto32, unop(to_bit, mkexpr(rS3))),
                           mkexpr(iTot3)) );
      if (mode64) {
         assign( rS4, binop(shr_op, mkexpr(rS3), mkU8(8)) );
         assign( iTot5, binop(Iop_Add32,
                              unop(Iop_1Uto32, unop(to_bit, mkexpr(rS4))),
                              mkexpr(iTot4)) );
         assign( rS5, binop(shr_op, mkexpr(rS4), mkU8(8)) );
         assign( iTot6, binop(Iop_Add32,
                              unop(Iop_1Uto32, unop(to_bit, mkexpr(rS5))),
                              mkexpr(iTot5)) );
         assign( rS6, binop(shr_op, mkexpr(rS5), mkU8(8)) );
         assign( iTot7, binop(Iop_Add32,
                              unop(Iop_1Uto32, unop(to_bit, mkexpr(rS6))),
                              mkexpr(iTot6)) );
         assign( rS7, binop(shr_op, mkexpr(rS6), mkU8(8)) );
         assign( iTot8, binop(Iop_Add32,
                              unop(Iop_1Uto32, unop(to_bit, mkexpr(rS7))),
                              mkexpr(iTot7)) );
         assign( rA, unop(Iop_32Uto64,
                          binop(Iop_And32, mkexpr(iTot8), mkU32(1))) );
      } else
         assign( rA, mkexpr(iTot4) );

      break;
   case 0x9a:  
      assign( iTot1, unop(Iop_1Uto32, unop(to_bit, mkexpr(rS))) );
      assign( rS1, binop(shr_op, mkexpr(rS), mkU8(8)) );
      assign( iTot2, binop(Iop_Add32,
                           unop(Iop_1Uto32, unop(to_bit, mkexpr(rS1))),
                           mkexpr(iTot1)) );
      assign( rS2, binop(shr_op, mkexpr(rS1), mkU8(8)) );
      assign( iTot3, binop(Iop_Add32,
                           unop(Iop_1Uto32, unop(to_bit, mkexpr(rS2))),
                           mkexpr(iTot2)) );
      assign( rS3, binop(shr_op, mkexpr(rS2), mkU8(8)) );
      assign( iTot4, binop(Iop_Add32,
                           unop(Iop_1Uto32, unop(to_bit, mkexpr(rS3))),
                           mkexpr(iTot3)) );
      assign( iLo, unop(Iop_1Uto32, unop(Iop_32to1, mkexpr(iTot4) )) );

      if (mode64) {
         assign( rS4, binop(shr_op, mkexpr(rS3), mkU8(8)) );
         assign( iTot5, unop(Iop_1Uto32, unop(to_bit, mkexpr(rS4))) );
         assign( rS5, binop(shr_op, mkexpr(rS4), mkU8(8)) );
         assign( iTot6, binop(Iop_Add32,
                              unop(Iop_1Uto32, unop(to_bit, mkexpr(rS5))),
                              mkexpr(iTot5)) );
         assign( rS6, binop(shr_op, mkexpr(rS5), mkU8(8)) );
         assign( iTot7, binop(Iop_Add32,
                              unop(Iop_1Uto32, unop(to_bit, mkexpr(rS6))),
                              mkexpr(iTot6)) );
         assign( rS7, binop(shr_op, mkexpr(rS6), mkU8(8)));
         assign( iTot8, binop(Iop_Add32,
                              unop(Iop_1Uto32, unop(to_bit, mkexpr(rS7))),
                              mkexpr(iTot7)) );
         assign( iHi, binop(Iop_And32, mkU32(1), mkexpr(iTot8)) ),
            assign( rA, binop(Iop_32HLto64, mkexpr(iHi), mkexpr(iLo)) );
      } else
         assign( rA, binop(Iop_Or32, mkU32(0), mkexpr(iLo)) );
      break;
   default:
      vex_printf("dis_int_parity(ppc)(opc2)\n");
      return False;
   }

   putIReg( rA_addr, mkexpr(rA) );

   return True;
}


static Bool dis_int_rot ( UInt theInstr )
{
   
   UChar opc1    = ifieldOPC(theInstr);
   UChar rS_addr = ifieldRegDS(theInstr);
   UChar rA_addr = ifieldRegA(theInstr);
   UChar rB_addr = ifieldRegB(theInstr);
   UChar sh_imm  = rB_addr;
   UChar MaskBeg = toUChar( IFIELD( theInstr, 6, 5 ) );
   UChar MaskEnd = toUChar( IFIELD( theInstr, 1, 5 ) );
   UChar msk_imm = toUChar( IFIELD( theInstr, 5, 6 ) );
   UChar opc2    = toUChar( IFIELD( theInstr, 2, 3 ) );
   UChar b1      = ifieldBIT1(theInstr);
   UChar flag_rC = ifieldBIT0(theInstr);

   IRType ty     = mode64 ? Ity_I64 : Ity_I32;
   IRTemp rS     = newTemp(ty);
   IRTemp rA     = newTemp(ty);
   IRTemp rB     = newTemp(ty);
   IRTemp rot    = newTemp(ty);
   IRExpr *r;
   UInt   mask32;
   ULong  mask64;

   assign( rS, getIReg(rS_addr) );
   assign( rB, getIReg(rB_addr) );

   switch (opc1) {
   case 0x14: {
      
      DIP("rlwimi%s r%u,r%u,%d,%d,%d\n", flag_rC ? ".":"",
          rA_addr, rS_addr, sh_imm, MaskBeg, MaskEnd);
      if (mode64) {
         
         
         mask64 = MASK64(31-MaskEnd, 31-MaskBeg);
         r = ROTL( unop(Iop_64to32, mkexpr(rS) ), mkU8(sh_imm) );
         r = unop(Iop_32Uto64, r);
         assign( rot, binop(Iop_Or64, r,
                            binop(Iop_Shl64, r, mkU8(32))) );
         assign( rA,
            binop(Iop_Or64,
                  binop(Iop_And64, mkexpr(rot), mkU64(mask64)),
                  binop(Iop_And64, getIReg(rA_addr), mkU64(~mask64))) );
      }
      else {
         
         mask32 = MASK32(31-MaskEnd, 31-MaskBeg);
         r = ROTL(mkexpr(rS), mkU8(sh_imm));
         assign( rA,
            binop(Iop_Or32,
                  binop(Iop_And32, mkU32(mask32), r),
                  binop(Iop_And32, getIReg(rA_addr), mkU32(~mask32))) );
      }
      break;
   }

   case 0x15: {
      
      vassert(MaskBeg < 32);
      vassert(MaskEnd < 32);
      vassert(sh_imm  < 32);

      if (mode64) {
         IRTemp rTmp = newTemp(Ity_I64);
         mask64 = MASK64(31-MaskEnd, 31-MaskBeg);
         DIP("rlwinm%s r%u,r%u,%d,%d,%d\n", flag_rC ? ".":"",
             rA_addr, rS_addr, sh_imm, MaskBeg, MaskEnd);
         
         
         r = ROTL( unop(Iop_64to32, mkexpr(rS) ), mkU8(sh_imm) );
         r = unop(Iop_32Uto64, r);
         assign( rTmp, r );
         r = NULL;
         assign( rot, binop(Iop_Or64, mkexpr(rTmp),
                            binop(Iop_Shl64, mkexpr(rTmp), mkU8(32))) );
         assign( rA, binop(Iop_And64, mkexpr(rot), mkU64(mask64)) );
      }
      else {
         if (MaskBeg == 0 && sh_imm+MaskEnd == 31) {
            DIP("slwi%s r%u,r%u,%d\n", flag_rC ? ".":"",
                rA_addr, rS_addr, sh_imm);
            assign( rA, binop(Iop_Shl32, mkexpr(rS), mkU8(sh_imm)) );
         }
         else if (MaskEnd == 31 && sh_imm+MaskBeg == 32) {
            DIP("srwi%s r%u,r%u,%d\n", flag_rC ? ".":"",
                rA_addr, rS_addr, MaskBeg);
            assign( rA, binop(Iop_Shr32, mkexpr(rS), mkU8(MaskBeg)) );
         }
         else {
            
            mask32 = MASK32(31-MaskEnd, 31-MaskBeg);
            DIP("rlwinm%s r%u,r%u,%d,%d,%d\n", flag_rC ? ".":"",
                rA_addr, rS_addr, sh_imm, MaskBeg, MaskEnd);
            
            assign( rA, binop(Iop_And32,
                              ROTL(mkexpr(rS), mkU8(sh_imm)), 
                              mkU32(mask32)) );
         }
      }
      break;
   }

   case 0x17: {
      
      DIP("rlwnm%s r%u,r%u,r%u,%d,%d\n", flag_rC ? ".":"",
          rA_addr, rS_addr, rB_addr, MaskBeg, MaskEnd);
      if (mode64) {
         mask64 = MASK64(31-MaskEnd, 31-MaskBeg);
         
         r = ROTL( unop(Iop_64to32, mkexpr(rS)),
                   unop(Iop_64to8, mkexpr(rB)) );
         r = unop(Iop_32Uto64, r);
         assign(rot, binop(Iop_Or64, r, binop(Iop_Shl64, r, mkU8(32))));
         assign( rA, binop(Iop_And64, mkexpr(rot), mkU64(mask64)) );
      } else {
         mask32 = MASK32(31-MaskEnd, 31-MaskBeg);
         
         
         assign( rA, binop(Iop_And32,
                           ROTL(mkexpr(rS),
                                unop(Iop_32to8, mkexpr(rB))),
                           mkU32(mask32)) );
      }
      break;
   }

   
   case 0x1E: {
      msk_imm = ((msk_imm & 1) << 5) | (msk_imm >> 1);
      sh_imm |= b1 << 5;

      vassert( msk_imm < 64 );
      vassert( sh_imm < 64 );

      switch (opc2) {
      case 0x4: {
         
         r = ROTL( mkexpr(rS), unop(Iop_64to8, mkexpr(rB)) );

         if (b1 == 0) { 
            DIP("rldcl%s r%u,r%u,r%u,%u\n", flag_rC ? ".":"",
                rA_addr, rS_addr, rB_addr, msk_imm);
            
            mask64 = MASK64(0, 63-msk_imm);
            assign( rA, binop(Iop_And64, r, mkU64(mask64)) );
            break;
         } else {       
            DIP("rldcr%s r%u,r%u,r%u,%u\n", flag_rC ? ".":"",
                rA_addr, rS_addr, rB_addr, msk_imm);
            mask64 = MASK64(63-msk_imm, 63);
            assign( rA, binop(Iop_And64, r, mkU64(mask64)) );
            break;
         }
         break;
      }
      case 0x2: 
         DIP("rldic%s r%u,r%u,%u,%u\n", flag_rC ? ".":"",
             rA_addr, rS_addr, sh_imm, msk_imm);
         r = ROTL(mkexpr(rS), mkU8(sh_imm));
         mask64 = MASK64(sh_imm, 63-msk_imm);
         assign( rA, binop(Iop_And64, r, mkU64(mask64)) );
         break;
         
         
      case 0x0: 
         if (mode64
             && sh_imm + msk_imm == 64 && msk_imm >= 1 && msk_imm <= 63) {
            DIP("srdi%s r%u,r%u,%u\n",
                flag_rC ? ".":"", rA_addr, rS_addr, msk_imm);
            assign( rA, binop(Iop_Shr64, mkexpr(rS), mkU8(msk_imm)) );
         } else {
            DIP("rldicl%s r%u,r%u,%u,%u\n", flag_rC ? ".":"",
                rA_addr, rS_addr, sh_imm, msk_imm);
            r = ROTL(mkexpr(rS), mkU8(sh_imm));
            mask64 = MASK64(0, 63-msk_imm);
            assign( rA, binop(Iop_And64, r, mkU64(mask64)) );
         }
         break;
         
      case 0x1: 
         if (mode64 
             && sh_imm + msk_imm == 63 && sh_imm >= 1 && sh_imm <= 63) {
            DIP("sldi%s r%u,r%u,%u\n",
                flag_rC ? ".":"", rA_addr, rS_addr, sh_imm);
            assign( rA, binop(Iop_Shl64, mkexpr(rS), mkU8(sh_imm)) );
         } else {
            DIP("rldicr%s r%u,r%u,%u,%u\n", flag_rC ? ".":"",
                rA_addr, rS_addr, sh_imm, msk_imm);
            r = ROTL(mkexpr(rS), mkU8(sh_imm));
            mask64 = MASK64(63-msk_imm, 63);
            assign( rA, binop(Iop_And64, r, mkU64(mask64)) );
         }
         break;
         
      case 0x3: { 
         IRTemp rA_orig = newTemp(ty);
         DIP("rldimi%s r%u,r%u,%u,%u\n", flag_rC ? ".":"",
             rA_addr, rS_addr, sh_imm, msk_imm);
         r = ROTL(mkexpr(rS), mkU8(sh_imm));
         mask64 = MASK64(sh_imm, 63-msk_imm);
         assign( rA_orig, getIReg(rA_addr) );
         assign( rA, binop(Iop_Or64,
                           binop(Iop_And64, mkU64(mask64),  r),
                           binop(Iop_And64, mkU64(~mask64),
                                            mkexpr(rA_orig))) );
         break;
      }
      default:
         vex_printf("dis_int_rot(ppc)(opc2)\n");
         return False;
      }
      break;         
   }

   default:
      vex_printf("dis_int_rot(ppc)(opc1)\n");
      return False;
   }

   putIReg( rA_addr, mkexpr(rA) );

   if (flag_rC) {
      set_CR0( mkexpr(rA) );
   }
   return True;
}


static Bool dis_int_load ( UInt theInstr )
{
   
   UChar opc1     = ifieldOPC(theInstr);
   UChar rD_addr  = ifieldRegDS(theInstr);
   UChar rA_addr  = ifieldRegA(theInstr);
   UInt  uimm16   = ifieldUIMM16(theInstr);
   UChar rB_addr  = ifieldRegB(theInstr);
   UInt  opc2     = ifieldOPClo10(theInstr);
   UChar b1       = ifieldBIT1(theInstr);
   UChar b0       = ifieldBIT0(theInstr);

   Int     simm16 = extend_s_16to32(uimm16);
   IRType  ty     = mode64 ? Ity_I64 : Ity_I32;
   IRTemp  EA     = newTemp(ty);
   IRExpr* val;

   switch (opc1) {
   case 0x1F: 
      assign( EA, ea_rAor0_idxd( rA_addr, rB_addr ) );
      break;
   case 0x38: 
              
      simm16 = simm16 & 0xFFFFFFF0;
      assign( EA, ea_rAor0_simm( rA_addr, simm16  ) );
      break;
   case 0x3A: 
              
      simm16 = simm16 & 0xFFFFFFFC;
      assign( EA, ea_rAor0_simm( rA_addr, simm16  ) );
      break;
   default:   
      assign( EA, ea_rAor0_simm( rA_addr, simm16  ) );
      break;
   }

   switch (opc1) {
   case 0x22: 
      DIP("lbz r%u,%d(r%u)\n", rD_addr, (Int)simm16, rA_addr);
      val = load(Ity_I8, mkexpr(EA));
      putIReg( rD_addr, mkWidenFrom8(ty, val, False) );
      break;
      
   case 0x23: 
      if (rA_addr == 0 || rA_addr == rD_addr) {
         vex_printf("dis_int_load(ppc)(lbzu,rA_addr|rD_addr)\n");
         return False;
      }
      DIP("lbzu r%u,%d(r%u)\n", rD_addr, (Int)simm16, rA_addr);
      val = load(Ity_I8, mkexpr(EA));
      putIReg( rD_addr, mkWidenFrom8(ty, val, False) );
      putIReg( rA_addr, mkexpr(EA) );
      break;
      
   case 0x2A: 
      DIP("lha r%u,%d(r%u)\n", rD_addr, (Int)simm16, rA_addr);
      val = load(Ity_I16, mkexpr(EA));
      putIReg( rD_addr, mkWidenFrom16(ty, val, True) );
      break;

   case 0x2B: 
      if (rA_addr == 0 || rA_addr == rD_addr) {
         vex_printf("dis_int_load(ppc)(lhau,rA_addr|rD_addr)\n");
         return False;
      }
      DIP("lhau r%u,%d(r%u)\n", rD_addr, (Int)simm16, rA_addr);
      val = load(Ity_I16, mkexpr(EA));
      putIReg( rD_addr, mkWidenFrom16(ty, val, True) );
      putIReg( rA_addr, mkexpr(EA) );
      break;
      
   case 0x28: 
      DIP("lhz r%u,%d(r%u)\n", rD_addr, (Int)simm16, rA_addr);
      val = load(Ity_I16, mkexpr(EA));
      putIReg( rD_addr, mkWidenFrom16(ty, val, False) );
      break;
      
   case 0x29: 
      if (rA_addr == 0 || rA_addr == rD_addr) {
         vex_printf("dis_int_load(ppc)(lhzu,rA_addr|rD_addr)\n");
         return False;
      }
      DIP("lhzu r%u,%d(r%u)\n", rD_addr, (Int)simm16, rA_addr);
      val = load(Ity_I16, mkexpr(EA));
      putIReg( rD_addr, mkWidenFrom16(ty, val, False) );
      putIReg( rA_addr, mkexpr(EA) );
      break;

   case 0x20: 
      DIP("lwz r%u,%d(r%u)\n", rD_addr, (Int)simm16, rA_addr);
      val = load(Ity_I32, mkexpr(EA));
      putIReg( rD_addr, mkWidenFrom32(ty, val, False) );
      break;
      
   case 0x21: 
      if (rA_addr == 0 || rA_addr == rD_addr) {
         vex_printf("dis_int_load(ppc)(lwzu,rA_addr|rD_addr)\n");
         return False;
      }
      DIP("lwzu r%u,%d(r%u)\n", rD_addr, (Int)simm16, rA_addr);
      val = load(Ity_I32, mkexpr(EA));
      putIReg( rD_addr, mkWidenFrom32(ty, val, False) );
      putIReg( rA_addr, mkexpr(EA) );
      break;
      
   
   case 0x1F:
      if (b0 != 0) {
         vex_printf("dis_int_load(ppc)(Ox1F,b0)\n");
         return False;
      }

      switch (opc2) {
      case 0x077: 
         DIP("lbzux r%u,r%u,r%u\n", rD_addr, rA_addr, rB_addr);
         if (rA_addr == 0 || rA_addr == rD_addr) {
            vex_printf("dis_int_load(ppc)(lwzux,rA_addr|rD_addr)\n");
            return False;
         }
         val = load(Ity_I8, mkexpr(EA));
         putIReg( rD_addr, mkWidenFrom8(ty, val, False) );
         putIReg( rA_addr, mkexpr(EA) );
         break;
         
      case 0x057: 
         DIP("lbzx r%u,r%u,r%u\n", rD_addr, rA_addr, rB_addr);
         val = load(Ity_I8, mkexpr(EA));
         putIReg( rD_addr, mkWidenFrom8(ty, val, False) );
         break;
         
      case 0x177: 
         if (rA_addr == 0 || rA_addr == rD_addr) {
            vex_printf("dis_int_load(ppc)(lhaux,rA_addr|rD_addr)\n");
            return False;
         }
         DIP("lhaux r%u,r%u,r%u\n", rD_addr, rA_addr, rB_addr);
         val = load(Ity_I16, mkexpr(EA));
         putIReg( rD_addr, mkWidenFrom16(ty, val, True) );
         putIReg( rA_addr, mkexpr(EA) );
         break;
         
      case 0x157: 
         DIP("lhax r%u,r%u,r%u\n", rD_addr, rA_addr, rB_addr);
         val = load(Ity_I16, mkexpr(EA));
         putIReg( rD_addr, mkWidenFrom16(ty, val, True) );
         break;
         
      case 0x137: 
         if (rA_addr == 0 || rA_addr == rD_addr) {
            vex_printf("dis_int_load(ppc)(lhzux,rA_addr|rD_addr)\n");
            return False;
         }
         DIP("lhzux r%u,r%u,r%u\n", rD_addr, rA_addr, rB_addr);
         val = load(Ity_I16, mkexpr(EA));
         putIReg( rD_addr, mkWidenFrom16(ty, val, False) );
         putIReg( rA_addr, mkexpr(EA) );
         break;
         
      case 0x117: 
         DIP("lhzx r%u,r%u,r%u\n", rD_addr, rA_addr, rB_addr);
         val = load(Ity_I16, mkexpr(EA));
         putIReg( rD_addr, mkWidenFrom16(ty, val, False) );
         break;

      case 0x037: 
         if (rA_addr == 0 || rA_addr == rD_addr) {
            vex_printf("dis_int_load(ppc)(lwzux,rA_addr|rD_addr)\n");
            return False;
         }
         DIP("lwzux r%u,r%u,r%u\n", rD_addr, rA_addr, rB_addr);
         val = load(Ity_I32, mkexpr(EA));
         putIReg( rD_addr, mkWidenFrom32(ty, val, False) );
         putIReg( rA_addr, mkexpr(EA) );
         break;
         
      case 0x017: 
         DIP("lwzx r%u,r%u,r%u\n", rD_addr, rA_addr, rB_addr);
         val = load(Ity_I32, mkexpr(EA));
         putIReg( rD_addr, mkWidenFrom32(ty, val, False) );
         break;


      
      case 0x035: 
         if (rA_addr == 0 || rA_addr == rD_addr) {
            vex_printf("dis_int_load(ppc)(ldux,rA_addr|rD_addr)\n");
            return False;
         }
         DIP("ldux r%u,r%u,r%u\n", rD_addr, rA_addr, rB_addr);
         putIReg( rD_addr, load(Ity_I64, mkexpr(EA)) );
         putIReg( rA_addr, mkexpr(EA) );
         break;

      case 0x015: 
         DIP("ldx r%u,r%u,r%u\n", rD_addr, rA_addr, rB_addr);
         putIReg( rD_addr, load(Ity_I64, mkexpr(EA)) );
         break;

      case 0x175: 
         if (rA_addr == 0 || rA_addr == rD_addr) {
            vex_printf("dis_int_load(ppc)(lwaux,rA_addr|rD_addr)\n");
            return False;
         }
         DIP("lwaux r%u,r%u,r%u\n", rD_addr, rA_addr, rB_addr);
         putIReg( rD_addr,
                  unop(Iop_32Sto64, load(Ity_I32, mkexpr(EA))) );
         putIReg( rA_addr, mkexpr(EA) );
         break;

      case 0x155: 
         DIP("lwax r%u,r%u,r%u\n", rD_addr, rA_addr, rB_addr);
         putIReg( rD_addr,
                  unop(Iop_32Sto64, load(Ity_I32, mkexpr(EA))) );
         break;

      default:
         vex_printf("dis_int_load(ppc)(opc2)\n");
         return False;
      }
      break;

   case 0x3A:
      switch ((b1<<1) | b0) {
      case 0x0: 
         DIP("ld r%u,%d(r%u)\n", rD_addr, simm16, rA_addr);
         putIReg( rD_addr, load(Ity_I64, mkexpr(EA)) );
         break;

      case 0x1: 
         if (rA_addr == 0 || rA_addr == rD_addr) {
            vex_printf("dis_int_load(ppc)(ldu,rA_addr|rD_addr)\n");
            return False;
         }
         DIP("ldu r%u,%d(r%u)\n", rD_addr, simm16, rA_addr);
         putIReg( rD_addr, load(Ity_I64, mkexpr(EA)) );
         putIReg( rA_addr, mkexpr(EA) );
         break;

      case 0x2: 
         DIP("lwa r%u,%d(r%u)\n", rD_addr, simm16, rA_addr);
         putIReg( rD_addr,
                  unop(Iop_32Sto64, load(Ity_I32, mkexpr(EA))) );
         break;

      default:
         vex_printf("dis_int_load(ppc)(0x3A, opc2)\n");
         return False;
      }
      break;

   case 0x38: {
      IRTemp  high = newTemp(ty);
      IRTemp  low  = newTemp(ty);
      
      DIP("lq r%u,%d(r%u)\n", rD_addr, simm16, rA_addr);
      
      if (mode64) {
         if (host_endness == VexEndnessBE) {
            assign(high, load(ty, mkexpr( EA ) ) );
            assign(low, load(ty, binop( Iop_Add64,
                                        mkexpr( EA ),
                                        mkU64( 8 ) ) ) );
	 } else {
            assign(low, load(ty, mkexpr( EA ) ) );
            assign(high, load(ty, binop( Iop_Add64,
                                         mkexpr( EA ),
                                         mkU64( 8 ) ) ) );
	 }
      } else {
         assign(high, load(ty, binop( Iop_Add32,
                                      mkexpr( EA ),
                                      mkU32( 4 ) ) ) );
         assign(low, load(ty, binop( Iop_Add32,
                                      mkexpr( EA ),
                                      mkU32( 12 ) ) ) );
      }
      gen_SIGBUS_if_misaligned( EA, 16 );
      putIReg( rD_addr,  mkexpr( high) );
      putIReg( rD_addr+1,  mkexpr( low) );
      break;
   }
   default:
      vex_printf("dis_int_load(ppc)(opc1)\n");
      return False;
   }
   return True;
}



static Bool dis_int_store ( UInt theInstr, const VexAbiInfo* vbi )
{
   
   UChar opc1    = ifieldOPC(theInstr);
   UInt  rS_addr = ifieldRegDS(theInstr);
   UInt  rA_addr = ifieldRegA(theInstr);
   UInt  uimm16  = ifieldUIMM16(theInstr);
   UInt  rB_addr = ifieldRegB(theInstr);
   UInt  opc2    = ifieldOPClo10(theInstr);
   UChar b1      = ifieldBIT1(theInstr);
   UChar b0      = ifieldBIT0(theInstr);

   Int    simm16 = extend_s_16to32(uimm16);
   IRType ty     = mode64 ? Ity_I64 : Ity_I32;
   IRTemp rS     = newTemp(ty);
   IRTemp rB     = newTemp(ty);
   IRTemp EA     = newTemp(ty);
   
   assign( rB, getIReg(rB_addr) );
   assign( rS, getIReg(rS_addr) );
   
   switch (opc1) {
   case 0x1F: 
      assign( EA, ea_rAor0_idxd( rA_addr, rB_addr ) );
      break;
   case 0x3E: 
              
      simm16 = simm16 & 0xFFFFFFFC;
   default:   
      assign( EA, ea_rAor0_simm( rA_addr, simm16  ) );
      break;
   }

   switch (opc1) {
   case 0x26: 
      DIP("stb r%u,%d(r%u)\n", rS_addr, simm16, rA_addr);
      store( mkexpr(EA), mkNarrowTo8(ty, mkexpr(rS)) );
      break;
       
   case 0x27: 
      if (rA_addr == 0 ) {
         vex_printf("dis_int_store(ppc)(stbu,rA_addr)\n");
         return False;
      }
      DIP("stbu r%u,%d(r%u)\n", rS_addr, simm16, rA_addr);
      putIReg( rA_addr, mkexpr(EA) );
      store( mkexpr(EA), mkNarrowTo8(ty, mkexpr(rS)) );
      break;

   case 0x2C: 
      DIP("sth r%u,%d(r%u)\n", rS_addr, simm16, rA_addr);
      store( mkexpr(EA), mkNarrowTo16(ty, mkexpr(rS)) );
      break;
      
   case 0x2D: 
      if (rA_addr == 0) {
         vex_printf("dis_int_store(ppc)(sthu,rA_addr)\n");
         return False;
      }
      DIP("sthu r%u,%d(r%u)\n", rS_addr, simm16, rA_addr);
      putIReg( rA_addr, mkexpr(EA) );
      store( mkexpr(EA), mkNarrowTo16(ty, mkexpr(rS)) );
      break;

   case 0x24: 
      DIP("stw r%u,%d(r%u)\n", rS_addr, simm16, rA_addr);
      store( mkexpr(EA), mkNarrowTo32(ty, mkexpr(rS)) );
      break;

   case 0x25: 
      if (rA_addr == 0) {
         vex_printf("dis_int_store(ppc)(stwu,rA_addr)\n");
         return False;
      }
      DIP("stwu r%u,%d(r%u)\n", rS_addr, simm16, rA_addr);
      putIReg( rA_addr, mkexpr(EA) );
      store( mkexpr(EA), mkNarrowTo32(ty, mkexpr(rS)) );
      break;
      
   
   case 0x1F:
      if (b0 != 0) {
         vex_printf("dis_int_store(ppc)(0x1F,b0)\n");
         return False;
      }

      switch (opc2) {
      case 0x0F7: 
         if (rA_addr == 0) {
            vex_printf("dis_int_store(ppc)(stbux,rA_addr)\n");
            return False;
         }
         DIP("stbux r%u,r%u,r%u\n", rS_addr, rA_addr, rB_addr);
         putIReg( rA_addr, mkexpr(EA) );
         store( mkexpr(EA), mkNarrowTo8(ty, mkexpr(rS)) );
         break;
         
      case 0x0D7: 
         DIP("stbx r%u,r%u,r%u\n", rS_addr, rA_addr, rB_addr);
         store( mkexpr(EA), mkNarrowTo8(ty, mkexpr(rS)) );
         break;
         
      case 0x1B7: 
         if (rA_addr == 0) {
            vex_printf("dis_int_store(ppc)(sthux,rA_addr)\n");
            return False;
         }
         DIP("sthux r%u,r%u,r%u\n", rS_addr, rA_addr, rB_addr);
         putIReg( rA_addr, mkexpr(EA) );
         store( mkexpr(EA), mkNarrowTo16(ty, mkexpr(rS)) );
         break;
         
      case 0x197: 
         DIP("sthx r%u,r%u,r%u\n", rS_addr, rA_addr, rB_addr);
         store( mkexpr(EA), mkNarrowTo16(ty, mkexpr(rS)) );
         break;
         
      case 0x0B7: 
         if (rA_addr == 0) {
            vex_printf("dis_int_store(ppc)(stwux,rA_addr)\n");
            return False;
         }
         DIP("stwux r%u,r%u,r%u\n", rS_addr, rA_addr, rB_addr);
         putIReg( rA_addr, mkexpr(EA) );
         store( mkexpr(EA), mkNarrowTo32(ty, mkexpr(rS)) );
         break;

      case 0x097: 
         DIP("stwx r%u,r%u,r%u\n", rS_addr, rA_addr, rB_addr);
         store( mkexpr(EA), mkNarrowTo32(ty, mkexpr(rS)) );
         break;
         

      
      case 0x0B5: 
         if (rA_addr == 0) {
            vex_printf("dis_int_store(ppc)(stdux,rA_addr)\n");
            return False;
         }
         DIP("stdux r%u,r%u,r%u\n", rS_addr, rA_addr, rB_addr);
         putIReg( rA_addr, mkexpr(EA) );
         store( mkexpr(EA), mkexpr(rS) );
         break;

      case 0x095: 
         DIP("stdx r%u,r%u,r%u\n", rS_addr, rA_addr, rB_addr);
         store( mkexpr(EA), mkexpr(rS) );
         break;

      default:
         vex_printf("dis_int_store(ppc)(opc2)\n");
         return False;
      }
      break;

   case 0x3E:
      switch ((b1<<1) | b0) {
      case 0x0: 
         if (!mode64)
            return False;

         DIP("std r%u,%d(r%u)\n", rS_addr, simm16, rA_addr);
         store( mkexpr(EA), mkexpr(rS) );
         break;

      case 0x1: 
         if (!mode64)
            return False;

         DIP("stdu r%u,%d(r%u)\n", rS_addr, simm16, rA_addr);
         putIReg( rA_addr, mkexpr(EA) );
         store( mkexpr(EA), mkexpr(rS) );
         break;

      case 0x2: { 
         IRTemp EA_hi = newTemp(ty);
         IRTemp EA_lo = newTemp(ty);
         DIP("stq r%u,%d(r%u)\n", rS_addr, simm16, rA_addr);

         if (mode64) {
            if (host_endness == VexEndnessBE) {

               
               assign( EA_hi, ea_rAor0_simm( rA_addr, simm16 ) );

               
               assign( EA_lo, ea_rAor0_simm( rA_addr, simm16+8 ) );
	    } else {
               
               assign( EA_hi, ea_rAor0_simm( rA_addr, simm16+8 ) );

               
               assign( EA_lo, ea_rAor0_simm( rA_addr, simm16 ) );
	    }
         } else {
            
            assign( EA_hi, ea_rAor0_simm( rA_addr, simm16+4 ) );

            
            assign( EA_lo, ea_rAor0_simm( rA_addr, simm16+12 ) );
         }
         store( mkexpr(EA_hi), mkexpr(rS) );
         store( mkexpr(EA_lo), getIReg( rS_addr+1 ) );
         break;
      }
      default:
         vex_printf("dis_int_load(ppc)(0x3A, opc2)\n");
         return False;
      }
      break;

   default:
      vex_printf("dis_int_store(ppc)(opc1)\n");
      return False;
   }
   return True;
}



static Bool dis_int_ldst_mult ( UInt theInstr )
{
   
   UChar opc1     = ifieldOPC(theInstr);
   UChar rD_addr  = ifieldRegDS(theInstr);
   UChar rS_addr  = rD_addr;
   UChar rA_addr  = ifieldRegA(theInstr);
   UInt  uimm16   = ifieldUIMM16(theInstr);

   Int     simm16 = extend_s_16to32(uimm16);
   IRType  ty     = mode64 ? Ity_I64 : Ity_I32;
   IROp    mkAdd  = mode64 ? Iop_Add64 : Iop_Add32;
   IRTemp  EA     = newTemp(ty);
   UInt    r      = 0;
   UInt    ea_off = 0;
   IRExpr* irx_addr;

   assign( EA, ea_rAor0_simm( rA_addr, simm16 ) );

   switch (opc1) {
   case 0x2E: 
      if (rA_addr >= rD_addr) {
         vex_printf("dis_int_ldst_mult(ppc)(lmw,rA_addr)\n");
         return False;
      }
      DIP("lmw r%u,%d(r%u)\n", rD_addr, simm16, rA_addr);
      for (r = rD_addr; r <= 31; r++) {
         irx_addr = binop(mkAdd, mkexpr(EA), mode64 ? mkU64(ea_off) : mkU32(ea_off));
         putIReg( r, mkWidenFrom32(ty, load(Ity_I32, irx_addr ),
                                       False) );
         ea_off += 4;
      }
      break;
      
   case 0x2F: 
      DIP("stmw r%u,%d(r%u)\n", rS_addr, simm16, rA_addr);
      for (r = rS_addr; r <= 31; r++) {
         irx_addr = binop(mkAdd, mkexpr(EA), mode64 ? mkU64(ea_off) : mkU32(ea_off));
         store( irx_addr, mkNarrowTo32(ty, getIReg(r)) );
         ea_off += 4;
      }
      break;
      
   default:
      vex_printf("dis_int_ldst_mult(ppc)(opc1)\n");
      return False;
   }
   return True;
}



static 
void generate_lsw_sequence ( IRTemp tNBytes,   
                             IRTemp EA,        
                             Int    rD,        
                             Int    maxBytes ) 
{
   Int     i, shift = 24;
   IRExpr* e_nbytes = mkexpr(tNBytes);
   IRExpr* e_EA     = mkexpr(EA);
   IRType  ty       = mode64 ? Ity_I64 : Ity_I32;

   vassert(rD >= 0 && rD < 32);
   rD--; if (rD < 0) rD = 31;

   for (i = 0; i < maxBytes; i++) {
      
      stmt( IRStmt_Exit( binop(Iop_CmpLT32U, e_nbytes, mkU32(i+1)),
                         Ijk_Boring, 
                         mkSzConst( ty, nextInsnAddr()), OFFB_CIA ));
      
      if ((i % 4) == 0) {
         rD++; if (rD == 32) rD = 0;
         putIReg(rD, mkSzImm(ty, 0));
         shift = 24;
      }
      
      vassert(shift == 0 || shift == 8 || shift == 16 || shift == 24);
      putIReg( 
         rD, 
         mkWidenFrom32(
            ty, 
            binop(
               Iop_Or32, 
               mkNarrowTo32(ty, getIReg(rD)),
               binop(
                  Iop_Shl32, 
                  unop(
                     Iop_8Uto32, 
                     load( Ity_I8,
                           binop( mkSzOp(ty,Iop_Add8),
                                  e_EA, mkSzImm(ty,i)))
                  ), 
                  mkU8(toUChar(shift))
               )
            ),
            False
	 ) 
      ); 
      shift -= 8;
   }
}

static 
void generate_stsw_sequence ( IRTemp tNBytes,   
                              IRTemp EA,        
                              Int    rS,        
                              Int    maxBytes ) 
{
   Int     i, shift = 24;
   IRExpr* e_nbytes = mkexpr(tNBytes);
   IRExpr* e_EA     = mkexpr(EA);
   IRType  ty       = mode64 ? Ity_I64 : Ity_I32;

   vassert(rS >= 0 && rS < 32);
   rS--; if (rS < 0) rS = 31;

   for (i = 0; i < maxBytes; i++) {
      
      stmt( IRStmt_Exit( binop(Iop_CmpLT32U, e_nbytes, mkU32(i+1)),
                         Ijk_Boring, 
                         mkSzConst( ty, nextInsnAddr() ), OFFB_CIA ));
      
      if ((i % 4) == 0) {
         rS++; if (rS == 32) rS = 0;
         shift = 24;
      }
      
      vassert(shift == 0 || shift == 8 || shift == 16 || shift == 24);
      store(
            binop( mkSzOp(ty,Iop_Add8), e_EA, mkSzImm(ty,i)),
            unop( Iop_32to8,
                  binop( Iop_Shr32,
                         mkNarrowTo32( ty, getIReg(rS) ),
                         mkU8( toUChar(shift) )))
      );
      shift -= 8;
   }
}

static Bool dis_int_ldst_str ( UInt theInstr, Bool* stopHere )
{
   
   UChar opc1     = ifieldOPC(theInstr);
   UChar rD_addr  = ifieldRegDS(theInstr);
   UChar rS_addr  = rD_addr;
   UChar rA_addr  = ifieldRegA(theInstr);
   UChar rB_addr  = ifieldRegB(theInstr);
   UChar NumBytes = rB_addr;
   UInt  opc2     = ifieldOPClo10(theInstr);
   UChar b0       = ifieldBIT0(theInstr);

   IRType ty      = mode64 ? Ity_I64 : Ity_I32;
   IRTemp t_EA    = newTemp(ty);
   IRTemp t_nbytes = IRTemp_INVALID;

   *stopHere = False;

   if (opc1 != 0x1F || b0 != 0) {
      vex_printf("dis_int_ldst_str(ppc)(opc1)\n");
      return False;
   }

   switch (opc2) {
   case 0x255: 
      DIP("lswi r%u,r%u,%d\n", rD_addr, rA_addr, NumBytes);
      assign( t_EA, ea_rAor0(rA_addr) );
      if (NumBytes == 8 && !mode64) {
         
         
         putIReg( rD_addr,          
                  load(Ity_I32, mkexpr(t_EA)) );
         putIReg( (rD_addr+1) % 32, 
                  load(Ity_I32,
                       binop(Iop_Add32, mkexpr(t_EA), mkU32(4))) );
      } else {
         t_nbytes = newTemp(Ity_I32);
         assign( t_nbytes, mkU32(NumBytes==0 ? 32 : NumBytes) );
         generate_lsw_sequence( t_nbytes, t_EA, rD_addr, 32 );
         *stopHere = True;
      }
      return True;

   case 0x215: 
      if (rD_addr == rA_addr || rD_addr == rB_addr)
         return False;
      if (rD_addr == 0 && rA_addr == 0)
         return False;
      DIP("lswx r%u,r%u,r%u\n", rD_addr, rA_addr, rB_addr);
      t_nbytes = newTemp(Ity_I32);
      assign( t_EA, ea_rAor0_idxd(rA_addr,rB_addr) );
      assign( t_nbytes, unop( Iop_8Uto32, getXER_BC() ) );
      generate_lsw_sequence( t_nbytes, t_EA, rD_addr, 128 );
      *stopHere = True;
      return True;

   case 0x2D5: 
      DIP("stswi r%u,r%u,%d\n", rS_addr, rA_addr, NumBytes);
      assign( t_EA, ea_rAor0(rA_addr) );
      if (NumBytes == 8 && !mode64) {
         
         
         store( mkexpr(t_EA),
                getIReg(rD_addr) );
         store( binop(Iop_Add32, mkexpr(t_EA), mkU32(4)),
                getIReg((rD_addr+1) % 32) );
      } else {
         t_nbytes = newTemp(Ity_I32);
         assign( t_nbytes, mkU32(NumBytes==0 ? 32 : NumBytes) );
         generate_stsw_sequence( t_nbytes, t_EA, rD_addr, 32 );
         *stopHere = True;
      }
      return True;

   case 0x295: 
      DIP("stswx r%u,r%u,r%u\n", rS_addr, rA_addr, rB_addr);
      t_nbytes = newTemp(Ity_I32);
      assign( t_EA, ea_rAor0_idxd(rA_addr,rB_addr) );
      assign( t_nbytes, unop( Iop_8Uto32, getXER_BC() ) );
      generate_stsw_sequence( t_nbytes, t_EA, rS_addr, 128 );
      *stopHere = True;
      return True;

   default:
      vex_printf("dis_int_ldst_str(ppc)(opc2)\n");
      return False;
   }
   return True;
}



static IRExpr*  branch_ctr_ok( UInt BO )
{
   IRType ty = mode64 ? Ity_I64 : Ity_I32;
   IRTemp ok = newTemp(Ity_I32);

   if ((BO >> 2) & 1) {     
      assign( ok, mkU32(0xFFFFFFFF) );
   } else {
      if ((BO >> 1) & 1) {  
         assign( ok, unop( Iop_1Sto32,
                           binop( mkSzOp(ty, Iop_CmpEQ8),
                                  getGST( PPC_GST_CTR ),
                                  mkSzImm(ty,0))) );
      } else {              
         assign( ok, unop( Iop_1Sto32,
                           binop( mkSzOp(ty, Iop_CmpNE8),
                                  getGST( PPC_GST_CTR ),
                                  mkSzImm(ty,0))) );
      }
   }
   return mkexpr(ok);
}



static IRExpr*  branch_cond_ok( UInt BO, UInt BI )
{
   Int where;
   IRTemp res   = newTemp(Ity_I32);
   IRTemp cr_bi = newTemp(Ity_I32);
   
   if ((BO >> 4) & 1) {
      assign( res, mkU32(1) );
   } else {
      
      
      
      assign( cr_bi, getCRbit_anywhere( BI, &where ) );

      if ((BO >> 3) & 1) {
         
         assign( res, mkexpr(cr_bi) );
      } else {
         assign( res, binop(Iop_Xor32, mkexpr(cr_bi),
                                       mkU32(1<<where)) );
      }
   }
   return mkexpr(res);
}


static Bool dis_branch ( UInt theInstr, 
                         const VexAbiInfo* vbi,
                         DisResult* dres,
                         Bool (*resteerOkFn)(void*,Addr),
                         void* callback_opaque )
{
   UChar opc1    = ifieldOPC(theInstr);
   UChar BO      = ifieldRegDS(theInstr);
   UChar BI      = ifieldRegA(theInstr);
   UInt  BD_u16  = ifieldUIMM16(theInstr) & 0xFFFFFFFC; 
   UChar b11to15 = ifieldRegB(theInstr);
   UInt  opc2    = ifieldOPClo10(theInstr);
   UInt  LI_u26  = ifieldUIMM26(theInstr) & 0xFFFFFFFC; 
   UChar flag_AA = ifieldBIT1(theInstr);
   UChar flag_LK = ifieldBIT0(theInstr);

   IRType   ty        = mode64 ? Ity_I64 : Ity_I32;
   Addr64   tgt       = 0;
   Int      BD        = extend_s_16to32(BD_u16);
   IRTemp   do_branch = newTemp(Ity_I32);
   IRTemp   ctr_ok    = newTemp(Ity_I32);
   IRTemp   cond_ok   = newTemp(Ity_I32);
   IRExpr*  e_nia     = mkSzImm(ty, nextInsnAddr());
   IRConst* c_nia     = mkSzConst(ty, nextInsnAddr());
   IRTemp   lr_old    = newTemp(ty);

   
   if (theInstr == 0x429F0005) {
      DIP("bcl 0x%x, 0x%x (a.k.a mr lr,cia+4)\n", BO, BI);
      putGST( PPC_GST_LR, e_nia );
      return True;
   }

       
   dres->whatNext = Dis_StopHere;
   vassert(dres->jk_StopHere == Ijk_INVALID);

   switch (opc1) {
   case 0x12: 
      if (flag_AA) {
         tgt = mkSzAddr( ty, extend_s_26to64(LI_u26) );
      } else {
         tgt = mkSzAddr( ty, guest_CIA_curr_instr +
                             (Long)extend_s_26to64(LI_u26) );
      }
      if (mode64) {
         DIP("b%s%s 0x%llx\n",
             flag_LK ? "l" : "", flag_AA ? "a" : "", tgt);
      } else {
         DIP("b%s%s 0x%x\n",
             flag_LK ? "l" : "", flag_AA ? "a" : "", (Addr32)tgt);
      }

      if (flag_LK) {
         putGST( PPC_GST_LR, e_nia );
         if (vbi->guest_ppc_zap_RZ_at_bl
             && vbi->guest_ppc_zap_RZ_at_bl( (ULong)tgt) ) {
            IRTemp t_tgt = newTemp(ty);
            assign(t_tgt, mode64 ? mkU64(tgt) : mkU32(tgt) );
            make_redzone_AbiHint( vbi, t_tgt,
                                  "branch-and-link (unconditional call)" );
         }
      }

      if (resteerOkFn( callback_opaque, tgt )) {
         dres->whatNext   = Dis_ResteerU;
         dres->continueAt = tgt;
      } else {
         dres->jk_StopHere = flag_LK ? Ijk_Call : Ijk_Boring; ;
         putGST( PPC_GST_CIA, mkSzImm(ty, tgt) );
      }
      break;
      
   case 0x10: 
      DIP("bc%s%s 0x%x, 0x%x, 0x%x\n",
          flag_LK ? "l" : "", flag_AA ? "a" : "", BO, BI, BD);
      
      if (!(BO & 0x4)) {
         putGST( PPC_GST_CTR,
                 binop(mkSzOp(ty, Iop_Sub8),
                       getGST( PPC_GST_CTR ), mkSzImm(ty, 1)) );
      }

      assign( ctr_ok,  branch_ctr_ok( BO ) );
      assign( cond_ok, branch_cond_ok( BO, BI ) );
      assign( do_branch,
              binop(Iop_And32, mkexpr(cond_ok), mkexpr(ctr_ok)) );

      if (flag_AA) {
         tgt = mkSzAddr(ty, extend_s_16to64(BD_u16));
      } else {
         tgt = mkSzAddr(ty, guest_CIA_curr_instr +
                            (Long)extend_s_16to64(BD_u16));
      }
      if (flag_LK)
         putGST( PPC_GST_LR, e_nia );
      
      stmt( IRStmt_Exit(
               binop(Iop_CmpNE32, mkexpr(do_branch), mkU32(0)),
               flag_LK ? Ijk_Call : Ijk_Boring,
               mkSzConst(ty, tgt), OFFB_CIA ) );

      dres->jk_StopHere = Ijk_Boring;
      putGST( PPC_GST_CIA, e_nia );
      break;
      
   case 0x13:
      if ((b11to15 & ~3) != 0) {
         vex_printf("dis_int_branch(ppc)(0x13,b11to15)(%d)\n", (Int)b11to15);
         return False;
      }

      switch (opc2) {
      case 0x210: 
         if ((BO & 0x4) == 0) { 
            vex_printf("dis_int_branch(ppc)(bcctr,BO)\n");
            return False;
         }
         DIP("bcctr%s 0x%x, 0x%x\n", flag_LK ? "l" : "", BO, BI);
         
         assign( cond_ok, branch_cond_ok( BO, BI ) );

         assign( lr_old, addr_align( getGST( PPC_GST_CTR ), 4 ));

         if (flag_LK)
            putGST( PPC_GST_LR, e_nia );
         
         stmt( IRStmt_Exit(
                  binop(Iop_CmpEQ32, mkexpr(cond_ok), mkU32(0)),
                  Ijk_Boring,
                  c_nia, OFFB_CIA ));

         if (flag_LK && vbi->guest_ppc_zap_RZ_at_bl) {
            make_redzone_AbiHint( vbi, lr_old,
                                  "b-ctr-l (indirect call)" );
	 }

         dres->jk_StopHere = flag_LK ? Ijk_Call : Ijk_Boring;;
         putGST( PPC_GST_CIA, mkexpr(lr_old) );
         break;
         
      case 0x010: { 
         Bool vanilla_return = False;
         if ((BO & 0x14 ) == 0x14 && flag_LK == 0) {
            DIP("blr\n");
            vanilla_return = True;
         } else {
            DIP("bclr%s 0x%x, 0x%x\n", flag_LK ? "l" : "", BO, BI);
         }

         if (!(BO & 0x4)) {
            putGST( PPC_GST_CTR,
                    binop(mkSzOp(ty, Iop_Sub8),
                          getGST( PPC_GST_CTR ), mkSzImm(ty, 1)) );
         }
         
         
         assign( ctr_ok,  branch_ctr_ok( BO ) );
         assign( cond_ok, branch_cond_ok( BO, BI ) );
         assign( do_branch,
                 binop(Iop_And32, mkexpr(cond_ok), mkexpr(ctr_ok)) );
         
         assign( lr_old, addr_align( getGST( PPC_GST_LR ), 4 ));

         if (flag_LK)
            putGST( PPC_GST_LR,  e_nia );

         stmt( IRStmt_Exit(
                  binop(Iop_CmpEQ32, mkexpr(do_branch), mkU32(0)),
                  Ijk_Boring,
                  c_nia, OFFB_CIA ));

         if (vanilla_return && vbi->guest_ppc_zap_RZ_at_blr) {
            make_redzone_AbiHint( vbi, lr_old,
                                  "branch-to-lr (unconditional return)" );
         }

         dres->jk_StopHere = Ijk_Ret;  
         putGST( PPC_GST_CIA, mkexpr(lr_old) );
         break;
      }
      default:
         vex_printf("dis_int_branch(ppc)(opc2)\n");
         return False;
      }
      break;
      
   default:
      vex_printf("dis_int_branch(ppc)(opc1)\n");
      return False;
   }
   
   return True;
}



static Bool dis_cond_logic ( UInt theInstr )
{
   
   UChar opc1      = ifieldOPC(theInstr);
   UChar crbD_addr = ifieldRegDS(theInstr);
   UChar crfD_addr = toUChar( IFIELD(theInstr, 23, 3) );
   UChar crbA_addr = ifieldRegA(theInstr);
   UChar crfS_addr = toUChar( IFIELD(theInstr, 18, 3) );
   UChar crbB_addr = ifieldRegB(theInstr);
   UInt  opc2      = ifieldOPClo10(theInstr);
   UChar b0        = ifieldBIT0(theInstr);

   IRTemp crbD     = newTemp(Ity_I32);
   IRTemp crbA     = newTemp(Ity_I32);
   IRTemp crbB     = newTemp(Ity_I32);

   if (opc1 != 19 || b0 != 0) {
      vex_printf("dis_cond_logic(ppc)(opc1)\n");
      return False;
   }

   if (opc2 == 0) {  
      if (((crbD_addr & 0x3) != 0) ||
          ((crbA_addr & 0x3) != 0) || (crbB_addr != 0)) {
         vex_printf("dis_cond_logic(ppc)(crbD|crbA|crbB != 0)\n");
         return False;
      }
      DIP("mcrf cr%u,cr%u\n", crfD_addr, crfS_addr);
      putCR0(   crfD_addr, getCR0(  crfS_addr) );
      putCR321( crfD_addr, getCR321(crfS_addr) );
   } else {
      assign( crbA, getCRbit(crbA_addr) );
      if (crbA_addr == crbB_addr)
         crbB = crbA;
      else
         assign( crbB, getCRbit(crbB_addr) );

      switch (opc2) {
      case 0x101: 
         DIP("crand crb%d,crb%d,crb%d\n", crbD_addr, crbA_addr, crbB_addr);
         assign( crbD, binop(Iop_And32, mkexpr(crbA), mkexpr(crbB)) );
         break;
      case 0x081: 
         DIP("crandc crb%d,crb%d,crb%d\n", crbD_addr, crbA_addr, crbB_addr);
         assign( crbD, binop(Iop_And32, 
                             mkexpr(crbA),
                             unop(Iop_Not32, mkexpr(crbB))) );
         break;
      case 0x121: 
         DIP("creqv crb%d,crb%d,crb%d\n", crbD_addr, crbA_addr, crbB_addr);
         assign( crbD, unop(Iop_Not32,
                            binop(Iop_Xor32, mkexpr(crbA), mkexpr(crbB))) );
         break;
      case 0x0E1: 
         DIP("crnand crb%d,crb%d,crb%d\n", crbD_addr, crbA_addr, crbB_addr);
         assign( crbD, unop(Iop_Not32,
                            binop(Iop_And32, mkexpr(crbA), mkexpr(crbB))) );
         break;
      case 0x021: 
         DIP("crnor crb%d,crb%d,crb%d\n", crbD_addr, crbA_addr, crbB_addr);
         assign( crbD, unop(Iop_Not32,
                            binop(Iop_Or32, mkexpr(crbA), mkexpr(crbB))) );
         break;
      case 0x1C1: 
         DIP("cror crb%d,crb%d,crb%d\n", crbD_addr, crbA_addr, crbB_addr);
         assign( crbD, binop(Iop_Or32, mkexpr(crbA), mkexpr(crbB)) );
         break;
      case 0x1A1: 
         DIP("crorc crb%d,crb%d,crb%d\n", crbD_addr, crbA_addr, crbB_addr);
         assign( crbD, binop(Iop_Or32, 
                             mkexpr(crbA),
                             unop(Iop_Not32, mkexpr(crbB))) );
         break;
      case 0x0C1: 
         DIP("crxor crb%d,crb%d,crb%d\n", crbD_addr, crbA_addr, crbB_addr);
         assign( crbD, binop(Iop_Xor32, mkexpr(crbA), mkexpr(crbB)) );
         break;
      default:
         vex_printf("dis_cond_logic(ppc)(opc2)\n");
         return False;
      }

      putCRbit( crbD_addr, mkexpr(crbD) );
   }
   return True;
}



static Bool do_trap ( UChar TO, 
                      IRExpr* argL0, IRExpr* argR0, Addr64 cia )
{
   IRTemp argL, argR;
   IRExpr *argLe, *argRe, *cond, *tmp;

   Bool    is32bit = typeOfIRExpr(irsb->tyenv, argL0 ) == Ity_I32;

   IROp    opAND     = is32bit ? Iop_And32     : Iop_And64;
   IROp    opOR      = is32bit ? Iop_Or32      : Iop_Or64;
   IROp    opCMPORDS = is32bit ? Iop_CmpORD32S : Iop_CmpORD64S;
   IROp    opCMPORDU = is32bit ? Iop_CmpORD32U : Iop_CmpORD64U;
   IROp    opCMPNE   = is32bit ? Iop_CmpNE32   : Iop_CmpNE64;
   IROp    opCMPEQ   = is32bit ? Iop_CmpEQ32   : Iop_CmpEQ64;
   IRExpr* const0    = is32bit ? mkU32(0)      : mkU64(0);
   IRExpr* const2    = is32bit ? mkU32(2)      : mkU64(2);
   IRExpr* const4    = is32bit ? mkU32(4)      : mkU64(4);
   IRExpr* const8    = is32bit ? mkU32(8)      : mkU64(8);

   const UChar b11100 = 0x1C;
   const UChar b00111 = 0x07;

   if (is32bit) {
      vassert( typeOfIRExpr(irsb->tyenv, argL0) == Ity_I32 );
      vassert( typeOfIRExpr(irsb->tyenv, argR0) == Ity_I32 );
   } else {
      vassert( typeOfIRExpr(irsb->tyenv, argL0) == Ity_I64 );
      vassert( typeOfIRExpr(irsb->tyenv, argR0) == Ity_I64 );
      vassert( mode64 );
   }

   if ((TO & b11100) == b11100 || (TO & b00111) == b00111) {
      stmt( IRStmt_Exit( 
               binop(opCMPEQ, const0, const0), 
               Ijk_SigTRAP,
               mode64 ? IRConst_U64(cia) : IRConst_U32((UInt)cia),
               OFFB_CIA
      ));
      return True; 
   }

   if (is32bit) {
      argL = newTemp(Ity_I32);
      argR = newTemp(Ity_I32);
   } else {
      argL = newTemp(Ity_I64);
      argR = newTemp(Ity_I64);
   }

   assign( argL, argL0 );
   assign( argR, argR0 );

   argLe = mkexpr(argL);
   argRe = mkexpr(argR);

   cond = const0;
   if (TO & 16) { 
      tmp = binop(opAND, binop(opCMPORDS, argLe, argRe), const8);
      cond = binop(opOR, tmp, cond);
   }
   if (TO & 8) { 
      tmp = binop(opAND, binop(opCMPORDS, argLe, argRe), const4);
      cond = binop(opOR, tmp, cond);
   }
   if (TO & 4) { 
      tmp = binop(opAND, binop(opCMPORDS, argLe, argRe), const2);
      cond = binop(opOR, tmp, cond);
   }
   if (TO & 2) { 
      tmp = binop(opAND, binop(opCMPORDU, argLe, argRe), const8);
      cond = binop(opOR, tmp, cond);
   }
   if (TO & 1) { 
      tmp = binop(opAND, binop(opCMPORDU, argLe, argRe), const4);
      cond = binop(opOR, tmp, cond);
   }
   stmt( IRStmt_Exit( 
            binop(opCMPNE, cond, const0), 
            Ijk_SigTRAP,
            mode64 ? IRConst_U64(cia) : IRConst_U32((UInt)cia),
            OFFB_CIA
   ));
   return False; 
}

static Bool dis_trapi ( UInt theInstr,
                        DisResult* dres )
{
   
   UChar  opc1    = ifieldOPC(theInstr);
   UChar  TO      = ifieldRegDS(theInstr);
   UChar  rA_addr = ifieldRegA(theInstr);
   UInt   uimm16  = ifieldUIMM16(theInstr);
   ULong  simm16  = extend_s_16to64(uimm16);
   Addr64 cia     = guest_CIA_curr_instr;
   IRType ty      = mode64 ? Ity_I64 : Ity_I32;
   Bool   uncond  = False;

   switch (opc1) {
   case 0x03: 
      uncond = do_trap( TO, 
                        mode64 ? unop(Iop_64to32, getIReg(rA_addr)) 
                               : getIReg(rA_addr),
                        mkU32( (UInt)simm16 ),
                        cia );
      if (TO == 4) {
         DIP("tweqi r%u,%d\n", (UInt)rA_addr, (Int)simm16);
      } else {
         DIP("tw%di r%u,%d\n", (Int)TO, (UInt)rA_addr, (Int)simm16);
      }
      break;
   case 0x02: 
      if (!mode64)
         return False;
      uncond = do_trap( TO, getIReg(rA_addr), mkU64( (ULong)simm16 ), cia );
      if (TO == 4) {
         DIP("tdeqi r%u,%d\n", (UInt)rA_addr, (Int)simm16);
      } else {
         DIP("td%di r%u,%d\n", (Int)TO, (UInt)rA_addr, (Int)simm16);
      }
      break;
   default:
      return False;
   }

   if (uncond) {
      putGST( PPC_GST_CIA, mkSzImm( ty, nextInsnAddr() ));
      dres->jk_StopHere = Ijk_Boring;
      dres->whatNext    = Dis_StopHere;
   }

   return True;
}

static Bool dis_trap ( UInt theInstr,
                        DisResult* dres )
{
   
   UInt   opc2    = ifieldOPClo10(theInstr);
   UChar  TO      = ifieldRegDS(theInstr);
   UChar  rA_addr = ifieldRegA(theInstr);
   UChar  rB_addr = ifieldRegB(theInstr);
   Addr64 cia     = guest_CIA_curr_instr;
   IRType ty      = mode64 ? Ity_I64 : Ity_I32;
   Bool   uncond  = False;

   if (ifieldBIT0(theInstr) != 0)
      return False;

   switch (opc2) {
   case 0x004: 
      uncond = do_trap( TO, 
                        mode64 ? unop(Iop_64to32, getIReg(rA_addr)) 
                               : getIReg(rA_addr),
                        mode64 ? unop(Iop_64to32, getIReg(rB_addr)) 
                               : getIReg(rB_addr),
                        cia );
      if (TO == 4) {
         DIP("tweq r%u,r%u\n", (UInt)rA_addr, (UInt)rB_addr);
      } else {
         DIP("tw%d r%u,r%u\n", (Int)TO, (UInt)rA_addr, (UInt)rB_addr);
      }
      break;
   case 0x044: 
      if (!mode64)
         return False;
      uncond = do_trap( TO, getIReg(rA_addr), getIReg(rB_addr), cia );
      if (TO == 4) {
         DIP("tdeq r%u,r%u\n", (UInt)rA_addr, (UInt)rB_addr);
      } else {
         DIP("td%d r%u,r%u\n", (Int)TO, (UInt)rA_addr, (UInt)rB_addr);
      }
      break;
   default:
      return False;
   }

   if (uncond) {
      putGST( PPC_GST_CIA, mkSzImm( ty, nextInsnAddr() ));
      dres->jk_StopHere = Ijk_Boring;
      dres->whatNext    = Dis_StopHere;
   }

   return True;
}


static Bool dis_syslink ( UInt theInstr, 
                          const VexAbiInfo* abiinfo, DisResult* dres )
{
   IRType ty = mode64 ? Ity_I64 : Ity_I32;

   if (theInstr != 0x44000002) {
      vex_printf("dis_syslink(ppc)(theInstr)\n");
      return False;
   }

   
   DIP("sc\n");

   putGST( PPC_GST_IP_AT_SYSCALL, getGST( PPC_GST_CIA ) );

   putGST( PPC_GST_CIA, mkSzImm( ty, nextInsnAddr() ));

   dres->whatNext    = Dis_StopHere;
   dres->jk_StopHere = Ijk_Sys_syscall;
   return True;
}


static Bool dis_memsync ( UInt theInstr )
{
   
   UChar opc1    = ifieldOPC(theInstr);
   UInt  b11to25 = IFIELD(theInstr, 11, 15);
   UChar flag_L  = ifieldRegDS(theInstr);
   UInt  b11to20 = IFIELD(theInstr, 11, 10);
   UChar rD_addr = ifieldRegDS(theInstr);
   UChar rS_addr = rD_addr;
   UChar rA_addr = ifieldRegA(theInstr);
   UChar rB_addr = ifieldRegB(theInstr);
   UInt  opc2    = ifieldOPClo10(theInstr);
   UChar b0      = ifieldBIT0(theInstr);

   IRType ty     = mode64 ? Ity_I64 : Ity_I32;
   IRTemp EA     = newTemp(ty);

   assign( EA, ea_rAor0_idxd( rA_addr, rB_addr ) );

   switch (opc1) {
   
   case 0x13:   
      if (opc2 != 0x096) {
         vex_printf("dis_memsync(ppc)(0x13,opc2)\n");
         return False;
      }
      if (b11to25 != 0 || b0 != 0) {
         vex_printf("dis_memsync(ppc)(0x13,b11to25|b0)\n");
         return False;
      }
      DIP("isync\n");
      stmt( IRStmt_MBE(Imbe_Fence) );
      break;

   
   case 0x1F:
      switch (opc2) {
      case 0x356: 
         if (b11to25 != 0 || b0 != 0) {
            vex_printf("dis_memsync(ppc)(eiei0,b11to25|b0)\n");
            return False;
         }
         DIP("eieio\n");
         
         stmt( IRStmt_MBE(Imbe_Fence) );
         break;

      case 0x014: { 
         IRTemp res;
         DIP("lwarx r%u,r%u,r%u,EH=%u\n", rD_addr, rA_addr, rB_addr, (UInt)b0);

         
         gen_SIGBUS_if_misaligned( EA, 4 );

         
         res = newTemp(Ity_I32);
         stmt( stmt_load(res, mkexpr(EA), NULL) );

         putIReg( rD_addr, mkWidenFrom32(ty, mkexpr(res), False) );
         break;
      }

      case 0x034: { 
         IRTemp res;
         DIP("lbarx r%u,r%u,r%u,EH=%u\n", rD_addr, rA_addr, rB_addr, (UInt)b0);

         
         res = newTemp(Ity_I8);
         stmt( stmt_load(res, mkexpr(EA), NULL) );

         putIReg( rD_addr, mkWidenFrom8(ty, mkexpr(res), False) );
         break;
     }

      case 0x074: { 
         IRTemp res;
         DIP("lharx r%u,r%u,r%u,EH=%u\n", rD_addr, rA_addr, rB_addr, (UInt)b0);

         
         gen_SIGBUS_if_misaligned( EA, 2 );

         
         res = newTemp(Ity_I16);
         stmt( stmt_load(res, mkexpr(EA), NULL) );

         putIReg( rD_addr, mkWidenFrom16(ty, mkexpr(res), False) );
         break;
      }

      case 0x096: { 
         
         
         
         IRTemp rS = newTemp(Ity_I32);
         IRTemp resSC;
         if (b0 != 1) {
            vex_printf("dis_memsync(ppc)(stwcx.,b0)\n");
            return False;
         }
         DIP("stwcx. r%u,r%u,r%u\n", rS_addr, rA_addr, rB_addr);

         
         gen_SIGBUS_if_misaligned( EA, 4 );

         
         assign( rS, mkNarrowTo32(ty, getIReg(rS_addr)) );

         
         resSC = newTemp(Ity_I1);
         stmt( stmt_load( resSC, mkexpr(EA), mkexpr(rS)) );

         
         
         putCR321(0, binop(Iop_Shl8, unop(Iop_1Uto8, mkexpr(resSC)), mkU8(1)));
         putCR0(0, getXER_SO());

         
         break;
      }

      case 0x2B6: {
         
         
         
         IRTemp rS = newTemp(Ity_I8);
         IRTemp resSC;
         if (b0 != 1) {
            vex_printf("dis_memsync(ppc)(stbcx.,b0)\n");
            return False;
         }
         DIP("stbcx. r%u,r%u,r%u\n", rS_addr, rA_addr, rB_addr);

         
         assign( rS, mkNarrowTo8(ty, getIReg(rS_addr)) );

         
         resSC = newTemp(Ity_I1);
         stmt( stmt_load( resSC, mkexpr(EA), mkexpr(rS)) );

         
         
         putCR321(0, binop(Iop_Shl8, unop(Iop_1Uto8, mkexpr(resSC)), mkU8(1)));
         putCR0(0, getXER_SO());

         
         break;
      }

      case 0x2D6: {
         
         
         
         IRTemp rS = newTemp(Ity_I16);
         IRTemp resSC;
         if (b0 != 1) {
            vex_printf("dis_memsync(ppc)(stwcx.,b0)\n");
            return False;
         }
         DIP("sthcx. r%u,r%u,r%u\n", rS_addr, rA_addr, rB_addr);

         
         gen_SIGBUS_if_misaligned( EA, 2 );

         
         assign( rS, mkNarrowTo16(ty, getIReg(rS_addr)) );

         
         resSC = newTemp(Ity_I1);
         stmt( stmt_load( resSC, mkexpr(EA), mkexpr(rS)) );

         
         
         putCR321(0, binop(Iop_Shl8, unop(Iop_1Uto8, mkexpr(resSC)), mkU8(1)));
         putCR0(0, getXER_SO());

         
         break;
      }

      case 0x256: 
                  
         if (b11to20 != 0 || b0 != 0) {
            vex_printf("dis_memsync(ppc)(sync/lwsync,b11to20|b0)\n");
            return False;
         }
         if (flag_L != 0 && flag_L != 1) {
            vex_printf("dis_memsync(ppc)(sync/lwsync,flag_L)\n");
            return False;
         }
         DIP("%ssync\n", flag_L == 1 ? "lw" : "");
         stmt( IRStmt_MBE(Imbe_Fence) );
         break;

      
      case 0x054: { 
         IRTemp res;
         if (!mode64)
            return False;
         DIP("ldarx r%u,r%u,r%u,EH=%u\n", rD_addr, rA_addr, rB_addr, (UInt)b0);

         
         gen_SIGBUS_if_misaligned( EA, 8 );

         
         res = newTemp(Ity_I64);
         stmt( stmt_load( res, mkexpr(EA), NULL) );

         putIReg( rD_addr, mkexpr(res) );
         break;
      }
      
      case 0x0D6: { 
         
         IRTemp rS = newTemp(Ity_I64);
         IRTemp resSC;
         if (b0 != 1) {
            vex_printf("dis_memsync(ppc)(stdcx.,b0)\n");
            return False;
         }
         if (!mode64)
            return False;
         DIP("stdcx. r%u,r%u,r%u\n", rS_addr, rA_addr, rB_addr);

         
         gen_SIGBUS_if_misaligned( EA, 8 );

         
         assign( rS, getIReg(rS_addr) );

         
         resSC = newTemp(Ity_I1);
         stmt( stmt_load( resSC, mkexpr(EA), mkexpr(rS)) );

         
         
         putCR321(0, binop(Iop_Shl8, unop(Iop_1Uto8, mkexpr(resSC)), mkU8(1)));
         putCR0(0, getXER_SO());

         
         break;
      }

      
      case 0x114: { 
         IRTemp res_hi = newTemp(ty);
         IRTemp res_lo = newTemp(ty);

         DIP("lqarx r%u,r%u,r%u,EH=%u\n", rD_addr, rA_addr, rB_addr, (UInt)b0);

         
         gen_SIGBUS_if_misaligned( EA, 16 );

         
         if (mode64) {
            if (host_endness == VexEndnessBE) {
               stmt( stmt_load( res_hi,
                                mkexpr(EA), NULL) );
               stmt( stmt_load( res_lo,
                                binop(Iop_Add64, mkexpr(EA), mkU64(8) ),
                                NULL) );
	    } else {
               stmt( stmt_load( res_lo,
                                mkexpr(EA), NULL) );
               stmt( stmt_load( res_hi,
                                binop(Iop_Add64, mkexpr(EA), mkU64(8) ),
                                NULL) );
            }
         } else {
            stmt( stmt_load( res_hi,
                             binop( Iop_Add32, mkexpr(EA), mkU32(4) ),
                             NULL) );
            stmt( stmt_load( res_lo,
                             binop( Iop_Add32, mkexpr(EA), mkU32(12) ),
                             NULL) );
         }
         putIReg( rD_addr,   mkexpr(res_hi) );
         putIReg( rD_addr+1, mkexpr(res_lo) );
         break;
      }

      case 0x0B6: { 
         
         IRTemp rS_hi = newTemp(ty);
         IRTemp rS_lo = newTemp(ty);
         IRTemp resSC;
         if (b0 != 1) {
            vex_printf("dis_memsync(ppc)(stqcx.,b0)\n");
            return False;
         }

         DIP("stqcx. r%u,r%u,r%u\n", rS_addr, rA_addr, rB_addr);

         
         gen_SIGBUS_if_misaligned( EA, 16 );
         
         assign( rS_hi, getIReg(rS_addr) );
         assign( rS_lo, getIReg(rS_addr+1) );

         
         resSC = newTemp(Ity_I1);

         if (mode64) {
            if (host_endness == VexEndnessBE) {
               stmt( stmt_load( resSC, mkexpr(EA), mkexpr(rS_hi) ) );
               store( binop( Iop_Add64, mkexpr(EA), mkU64(8) ),
                      mkexpr(rS_lo) );
	    } else {
               stmt( stmt_load( resSC, mkexpr(EA), mkexpr(rS_lo) ) );
               store( binop( Iop_Add64, mkexpr(EA), mkU64(8) ),
                      mkexpr(rS_hi) );
	    }
         } else {
            stmt( stmt_load( resSC, binop( Iop_Add32,
                                           mkexpr(EA),
                                           mkU32(4) ),
                                           mkexpr(rS_hi) ) );
            store( binop(Iop_Add32, mkexpr(EA), mkU32(12) ), mkexpr(rS_lo) );
         }

         
         
         putCR321(0, binop( Iop_Shl8,
                            unop(Iop_1Uto8, mkexpr(resSC) ),
                            mkU8(1)));
         putCR0(0, getXER_SO());
         break;
      }

      default:
         vex_printf("dis_memsync(ppc)(opc2)\n");
         return False;
      }
      break;

   default:
      vex_printf("dis_memsync(ppc)(opc1)\n");
      return False;
   }
   return True;
}



static Bool dis_int_shift ( UInt theInstr )
{
   
   UChar opc1    = ifieldOPC(theInstr);
   UChar rS_addr = ifieldRegDS(theInstr);
   UChar rA_addr = ifieldRegA(theInstr);
   UChar rB_addr = ifieldRegB(theInstr);
   UChar sh_imm  = rB_addr;
   UInt  opc2    = ifieldOPClo10(theInstr);
   UChar b1      = ifieldBIT1(theInstr);
   UChar flag_rC = ifieldBIT0(theInstr);

   IRType  ty         = mode64 ? Ity_I64 : Ity_I32;
   IRTemp  rA         = newTemp(ty);
   IRTemp  rS         = newTemp(ty);
   IRTemp  rB         = newTemp(ty);
   IRTemp  outofrange = newTemp(Ity_I1);
   IRTemp  rS_lo32    = newTemp(Ity_I32);
   IRTemp  rB_lo32    = newTemp(Ity_I32);
   IRExpr* e_tmp;

   assign( rS, getIReg(rS_addr) );
   assign( rB, getIReg(rB_addr) );
   assign( rS_lo32, mkNarrowTo32(ty, mkexpr(rS)) );
   assign( rB_lo32, mkNarrowTo32(ty, mkexpr(rB)) );
   
   if (opc1 == 0x1F) {
      switch (opc2) {
      case 0x018: { 
         DIP("slw%s r%u,r%u,r%u\n", flag_rC ? ".":"",
             rA_addr, rS_addr, rB_addr);
         
         e_tmp =
            binop( Iop_And32,
               binop( Iop_Shl32,
                      mkexpr(rS_lo32), 
                      unop( Iop_32to8,
                            binop(Iop_And32,
                                  mkexpr(rB_lo32), mkU32(31)))),
               unop( Iop_Not32,
                     binop( Iop_Sar32,
                            binop(Iop_Shl32, mkexpr(rB_lo32), mkU8(26)),
                            mkU8(31))) );
         assign( rA, mkWidenFrom32(ty, e_tmp, False) );
         break;
      }

      case 0x318: { 
         IRTemp sh_amt = newTemp(Ity_I32);
         DIP("sraw%s r%u,r%u,r%u\n", flag_rC ? ".":"",
             rA_addr, rS_addr, rB_addr);
         assign( sh_amt, binop(Iop_And32, mkU32(0x3F),
                                          mkexpr(rB_lo32)) );
         assign( outofrange,
                 binop(Iop_CmpLT32U, mkU32(31), mkexpr(sh_amt)) );
         e_tmp = binop( Iop_Sar32, 
                        mkexpr(rS_lo32), 
                        unop( Iop_32to8, 
                              IRExpr_ITE( mkexpr(outofrange), 
                                          mkU32(31),
                                          mkexpr(sh_amt)) ) );
         assign( rA, mkWidenFrom32(ty, e_tmp, True) );

         set_XER_CA( ty, PPCG_FLAG_OP_SRAW,
                     mkexpr(rA),
                     mkWidenFrom32(ty, mkexpr(rS_lo32), True),
                     mkWidenFrom32(ty, mkexpr(sh_amt), True ),
                     mkWidenFrom32(ty, getXER_CA32(), True) );
         break;
      }
         
      case 0x338: 
         DIP("srawi%s r%u,r%u,%d\n", flag_rC ? ".":"",
             rA_addr, rS_addr, sh_imm);
         vassert(sh_imm < 32);
         if (mode64) {
            assign( rA, binop(Iop_Sar64,
                              binop(Iop_Shl64, getIReg(rS_addr),
                                               mkU8(32)),
                              mkU8(32 + sh_imm)) );
         } else {
            assign( rA, binop(Iop_Sar32, mkexpr(rS_lo32),
                                         mkU8(sh_imm)) );
         }

         set_XER_CA( ty, PPCG_FLAG_OP_SRAWI, 
                     mkexpr(rA),
                     mkWidenFrom32(ty, mkexpr(rS_lo32), True),
                     mkSzImm(ty, sh_imm),
                     mkWidenFrom32(ty, getXER_CA32(), False) );
         break;
      
      case 0x218: 
         DIP("srw%s r%u,r%u,r%u\n", flag_rC ? ".":"",
             rA_addr, rS_addr, rB_addr);
         
         e_tmp = 
            binop(
               Iop_And32,
               binop( Iop_Shr32, 
                      mkexpr(rS_lo32), 
                      unop( Iop_32to8, 
                            binop(Iop_And32, mkexpr(rB_lo32),
                                             mkU32(31)))),
               unop( Iop_Not32, 
                     binop( Iop_Sar32, 
                            binop(Iop_Shl32, mkexpr(rB_lo32),
                                             mkU8(26)), 
                            mkU8(31))));
         assign( rA, mkWidenFrom32(ty, e_tmp, False) );
         break;


      
      case 0x01B: 
         DIP("sld%s r%u,r%u,r%u\n",
             flag_rC ? ".":"", rA_addr, rS_addr, rB_addr);
         
         assign( rA,
            binop(
               Iop_And64,
               binop( Iop_Shl64,
                      mkexpr(rS), 
                      unop( Iop_64to8, 
                            binop(Iop_And64, mkexpr(rB), mkU64(63)))),
               unop( Iop_Not64,
                     binop( Iop_Sar64,
                            binop(Iop_Shl64, mkexpr(rB), mkU8(57)), 
                            mkU8(63)))) );
         break;
      
      case 0x31A: { 
         IRTemp sh_amt = newTemp(Ity_I64);
         DIP("srad%s r%u,r%u,r%u\n",
             flag_rC ? ".":"", rA_addr, rS_addr, rB_addr);
         assign( sh_amt, binop(Iop_And64, mkU64(0x7F), mkexpr(rB)) );
         assign( outofrange,
                 binop(Iop_CmpLT64U, mkU64(63), mkexpr(sh_amt)) );
         assign( rA,
                 binop( Iop_Sar64, 
                        mkexpr(rS), 
                        unop( Iop_64to8, 
                              IRExpr_ITE( mkexpr(outofrange), 
                                          mkU64(63),
                                          mkexpr(sh_amt)) ))
               );
         set_XER_CA( ty, PPCG_FLAG_OP_SRAD,
                     mkexpr(rA), mkexpr(rS), mkexpr(sh_amt),
                     mkWidenFrom32(ty, getXER_CA32(), False) );
         break;
      }

      case 0x33A: case 0x33B: 
         sh_imm |= b1<<5;
         vassert(sh_imm < 64);
         DIP("sradi%s r%u,r%u,%u\n",
             flag_rC ? ".":"", rA_addr, rS_addr, sh_imm);
         assign( rA, binop(Iop_Sar64, getIReg(rS_addr), mkU8(sh_imm)) );

         set_XER_CA( ty, PPCG_FLAG_OP_SRADI, 
                     mkexpr(rA),
                     getIReg(rS_addr),
                     mkU64(sh_imm), 
                     mkWidenFrom32(ty, getXER_CA32(), False) );
         break;

      case 0x21B: 
         DIP("srd%s r%u,r%u,r%u\n",
             flag_rC ? ".":"", rA_addr, rS_addr, rB_addr);
         
         assign( rA,
            binop(
               Iop_And64,
               binop( Iop_Shr64, 
                      mkexpr(rS), 
                      unop( Iop_64to8, 
                            binop(Iop_And64, mkexpr(rB), mkU64(63)))),
               unop( Iop_Not64, 
                     binop( Iop_Sar64, 
                            binop(Iop_Shl64, mkexpr(rB), mkU8(57)), 
                            mkU8(63)))) );
         break;
     
      default:
         vex_printf("dis_int_shift(ppc)(opc2)\n");
         return False;
      }
   } else {
      vex_printf("dis_int_shift(ppc)(opc1)\n");
      return False;
   }

   putIReg( rA_addr, mkexpr(rA) );
   
   if (flag_rC) {
      set_CR0( mkexpr(rA) );
   }
   return True;
}



static IRExpr*  gen_byterev32 ( IRTemp t )
{
   vassert(typeOfIRTemp(irsb->tyenv, t) == Ity_I32);
   return
      binop(Iop_Or32,
         binop(Iop_Shl32, mkexpr(t), mkU8(24)),
      binop(Iop_Or32,
         binop(Iop_And32, binop(Iop_Shl32, mkexpr(t), mkU8(8)), 
                          mkU32(0x00FF0000)),
      binop(Iop_Or32,
         binop(Iop_And32, binop(Iop_Shr32, mkexpr(t), mkU8(8)),
                          mkU32(0x0000FF00)),
         binop(Iop_And32, binop(Iop_Shr32, mkexpr(t), mkU8(24)),
                          mkU32(0x000000FF) )
      )));
}

static IRExpr*  gen_byterev16 ( IRTemp t )
{
   vassert(typeOfIRTemp(irsb->tyenv, t) == Ity_I32);
   return
      binop(Iop_Or32,
         binop(Iop_And32, binop(Iop_Shl32, mkexpr(t), mkU8(8)),
                          mkU32(0x0000FF00)),
         binop(Iop_And32, binop(Iop_Shr32, mkexpr(t), mkU8(8)),
                          mkU32(0x000000FF))
      );
}

static Bool dis_int_ldst_rev ( UInt theInstr )
{
   
   UChar opc1    = ifieldOPC(theInstr);
   UChar rD_addr = ifieldRegDS(theInstr);
   UChar rS_addr = rD_addr;
   UChar rA_addr = ifieldRegA(theInstr);
   UChar rB_addr = ifieldRegB(theInstr);
   UInt  opc2    = ifieldOPClo10(theInstr);
   UChar b0      = ifieldBIT0(theInstr);

   IRType ty = mode64 ? Ity_I64 : Ity_I32;
   IRTemp EA = newTemp(ty);
   IRTemp w1 = newTemp(Ity_I32);
   IRTemp w2 = newTemp(Ity_I32);

   if (opc1 != 0x1F || b0 != 0) {
      vex_printf("dis_int_ldst_rev(ppc)(opc1|b0)\n");
      return False;
   }

   assign( EA, ea_rAor0_idxd( rA_addr, rB_addr ) );
   
   switch (opc2) {

      case 0x316: 
         DIP("lhbrx r%u,r%u,r%u\n", rD_addr, rA_addr, rB_addr);
         assign( w1, unop(Iop_16Uto32, load(Ity_I16, mkexpr(EA))) );
         assign( w2, gen_byterev16(w1) );
         putIReg( rD_addr, mkWidenFrom32(ty, mkexpr(w2),
                                         False) );
         break;

      case 0x216: 
         DIP("lwbrx r%u,r%u,r%u\n", rD_addr, rA_addr, rB_addr);
         assign( w1, load(Ity_I32, mkexpr(EA)) );
         assign( w2, gen_byterev32(w1) );
         putIReg( rD_addr, mkWidenFrom32(ty, mkexpr(w2),
                                         False) );
         break;

      case 0x214: 
      {
         IRExpr * nextAddr;
         IRTemp w3 = newTemp( Ity_I32 );
         IRTemp w4 = newTemp( Ity_I32 );
         DIP("ldbrx r%u,r%u,r%u\n", rD_addr, rA_addr, rB_addr);
         assign( w1, load( Ity_I32, mkexpr( EA ) ) );
         assign( w2, gen_byterev32( w1 ) );
         nextAddr = binop( mkSzOp( ty, Iop_Add8 ), mkexpr( EA ),
                           ty == Ity_I64 ? mkU64( 4 ) : mkU32( 4 ) );
         assign( w3, load( Ity_I32, nextAddr ) );
         assign( w4, gen_byterev32( w3 ) );
         if (host_endness == VexEndnessLE)
            putIReg( rD_addr, binop( Iop_32HLto64, mkexpr( w2 ), mkexpr( w4 ) ) );
         else
            putIReg( rD_addr, binop( Iop_32HLto64, mkexpr( w4 ), mkexpr( w2 ) ) );
         break;
      }

      case 0x396: 
         DIP("sthbrx r%u,r%u,r%u\n", rS_addr, rA_addr, rB_addr);
         assign( w1, mkNarrowTo32(ty, getIReg(rS_addr)) );
         store( mkexpr(EA), unop(Iop_32to16, gen_byterev16(w1)) );
         break;
      
      case 0x296: 
         DIP("stwbrx r%u,r%u,r%u\n", rS_addr, rA_addr, rB_addr);
         assign( w1, mkNarrowTo32(ty, getIReg(rS_addr)) );
         store( mkexpr(EA), gen_byterev32(w1) );
         break;

      case 0x294: 
      {
         IRTemp lo = newTemp(Ity_I32);
         IRTemp hi = newTemp(Ity_I32);
         IRTemp rS = newTemp(Ity_I64);
         assign( rS, getIReg( rS_addr ) );
         DIP("stdbrx r%u,r%u,r%u\n", rS_addr, rA_addr, rB_addr);
         assign(lo, unop(Iop_64HIto32, mkexpr(rS)));
         assign(hi, unop(Iop_64to32, mkexpr(rS)));
         store( mkexpr( EA ),
                binop( Iop_32HLto64, gen_byterev32( hi ),
                       gen_byterev32( lo ) ) );
         break;
      }

      default:
         vex_printf("dis_int_ldst_rev(ppc)(opc2)\n");
         return False;
   }
   return True;
}



static Bool dis_proc_ctl ( const VexAbiInfo* vbi, UInt theInstr )
{
   UChar opc1     = ifieldOPC(theInstr);
   
   
   UChar crfD     = toUChar( IFIELD( theInstr, 23, 3 ) );
   UChar b21to22  = toUChar( IFIELD( theInstr, 21, 2 ) );
   UChar rD_addr  = ifieldRegDS(theInstr);
   UInt  b11to20  = IFIELD( theInstr, 11, 10 );

   
   UChar rS_addr  = rD_addr;
   UInt  SPR      = b11to20;
   UInt  TBR      = b11to20;
   UChar b20      = toUChar( IFIELD( theInstr, 20, 1 ) );
   UInt  CRM      = IFIELD( theInstr, 12, 8 );
   UChar b11      = toUChar( IFIELD( theInstr, 11, 1 ) );

   UInt  opc2     = ifieldOPClo10(theInstr);
   UChar b0       = ifieldBIT0(theInstr);

   IRType ty = mode64 ? Ity_I64 : Ity_I32;
   IRTemp rS = newTemp(ty);
   assign( rS, getIReg(rS_addr) );

   
   SPR = ((SPR & 0x1F) << 5) | ((SPR >> 5) & 0x1F);
   
   TBR = ((TBR & 31) << 5) | ((TBR >> 5) & 31);
   
   if (opc1 != 0x1F) {
      vex_printf("dis_proc_ctl(ppc)(opc1|b%d)\n", b0);
      return False;
   }
   
   switch (opc2) {
   
   case 0x200: { 
      if (b21to22 != 0 || b11to20 != 0) {
         vex_printf("dis_proc_ctl(ppc)(mcrxr,b21to22|b11to20)\n");
         return False;
      }
      DIP("mcrxr crf%d\n", crfD);
      
      putGST_field( PPC_GST_CR,
                    getGST_field( PPC_GST_XER, 7 ),
                    crfD );

      
      putXER_SO( mkU8(0) );
      putXER_OV( mkU8(0) );
      putXER_CA( mkU8(0) );
      break;
   }
      
   case 0x013: 
      
      
      
      
      if (b11to20 == 0) {
         DIP("mfcr r%u\n", rD_addr);
         putIReg( rD_addr, mkWidenFrom32(ty, getGST( PPC_GST_CR ),
                                         False) );
         break;
      }
      if (b20 == 1 && b11 == 0) {
         DIP("mfocrf r%u,%u\n", rD_addr, CRM);
         putIReg( rD_addr, mkWidenFrom32(ty, getGST( PPC_GST_CR ),
                                         False) );
         break;
      }
      
      return False;

   
   case 0x153: 
      
      switch (SPR) {  
      case 0x1:
         DIP("mfxer r%u\n", rD_addr);
         putIReg( rD_addr, mkWidenFrom32(ty, getGST( PPC_GST_XER ),
                                         False) );
         break;
      case 0x8:
         DIP("mflr r%u\n", rD_addr);
         putIReg( rD_addr, getGST( PPC_GST_LR ) ); 
         break;
      case 0x9:
         DIP("mfctr r%u\n", rD_addr);
         putIReg( rD_addr, getGST( PPC_GST_CTR ) ); 
         break;
      case 0x80:  
         DIP("mfspr r%u (TFHAR)\n", rD_addr);
         putIReg( rD_addr, getGST( PPC_GST_TFHAR) );
         break;
      case 0x81:  
         DIP("mfspr r%u (TFIAR)\n", rD_addr);
         putIReg( rD_addr, getGST( PPC_GST_TFIAR) );
         break;
      case 0x82:  
         DIP("mfspr r%u (TEXASR)\n", rD_addr);
         putIReg( rD_addr, getGST( PPC_GST_TEXASR) );
         break;
      case 0x83:  
         DIP("mfspr r%u (TEXASRU)\n", rD_addr);
         putIReg( rD_addr, getGST( PPC_GST_TEXASRU) );
         break;
      case 0x100: 
         DIP("mfvrsave r%u\n", rD_addr);
         putIReg( rD_addr, mkWidenFrom32(ty, getGST( PPC_GST_VRSAVE ),
                                         False) );
         break;

      case 0x103:
         DIP("mfspr r%u, SPRG3(readonly)\n", rD_addr);
         putIReg( rD_addr, getGST( PPC_GST_SPRG3_RO ) );
         break;

      case 268 :
      case 269 : {
         UInt     arg  = SPR==268 ? 0 : 1;
         IRTemp   val  = newTemp(Ity_I32);
         IRExpr** args = mkIRExprVec_1( mkU32(arg) );
         IRDirty* d    = unsafeIRDirty_1_N(
                            val,
                            0,
                            "ppc32g_dirtyhelper_MFSPR_268_269",
                            fnptr_to_fnentry
                               (vbi, &ppc32g_dirtyhelper_MFSPR_268_269),
                            args
                         );
         
         stmt( IRStmt_Dirty(d) );
         putIReg( rD_addr,
                  mkWidenFrom32(ty, mkexpr(val), False) );
         DIP("mfspr r%u,%u", rD_addr, (UInt)SPR);
         break;
      }

      case 287:  {
         IRTemp   val  = newTemp(Ity_I32);
         IRExpr** args = mkIRExprVec_0();
         IRDirty* d    = unsafeIRDirty_1_N(
                            val,
                            0,
                            "ppc32g_dirtyhelper_MFSPR_287",
                            fnptr_to_fnentry
                               (vbi, &ppc32g_dirtyhelper_MFSPR_287),
                            args
                         );
         
         stmt( IRStmt_Dirty(d) );
         putIReg( rD_addr,
                  mkWidenFrom32(ty, mkexpr(val), False) );
         DIP("mfspr r%u,%u", rD_addr, (UInt)SPR);
         break;
      }

      default:
         vex_printf("dis_proc_ctl(ppc)(mfspr,SPR)(0x%x)\n", SPR);
         return False;
      }
      break;
      
   case 0x173: { 
      IRTemp   val  = newTemp(Ity_I64);
      IRExpr** args = mkIRExprVec_0();
      IRDirty* d    = unsafeIRDirty_1_N(
                              val, 
                              0, 
                              "ppcg_dirtyhelper_MFTB", 
                              fnptr_to_fnentry(vbi, &ppcg_dirtyhelper_MFTB), 
                              args );
      
      stmt( IRStmt_Dirty(d) );

      switch (TBR) {
      case 269: 
         DIP("mftbu r%u", rD_addr);
         putIReg( rD_addr,
                  mkWidenFrom32(ty, unop(Iop_64HIto32, mkexpr(val)),
                                False) );
         break;
      case 268: 
         DIP("mftb r%u", rD_addr);
         putIReg( rD_addr, (mode64) ? mkexpr(val) :
                                      unop(Iop_64to32, mkexpr(val)) );
         break;
      default:
         return False; 
      }
      break;
   }

   case 0x090: { 
      
      
      Int   cr;
      UChar shft;
      if (b11 != 0)
         return False;
      if (b20 == 1) {
         /* ppc64 v2.02 spec says mtocrf gives undefined outcome if >
            1 field is written.  It seems more robust to decline to
            decode the insn if so. */
         switch (CRM) {
            case 0x01: case 0x02: case 0x04: case 0x08:
            case 0x10: case 0x20: case 0x40: case 0x80:
               break;
            default: 
               return False; 
         }
      }
      DIP("%s 0x%x,r%u\n", b20==1 ? "mtocrf" : "mtcrf", 
                           CRM, rS_addr);
      
      for (cr = 0; cr < 8; cr++) {
         if ((CRM & (1 << (7-cr))) == 0)
            continue;
         shft = 4*(7-cr);
         putGST_field( PPC_GST_CR,
                       binop(Iop_Shr32,
                             mkNarrowTo32(ty, mkexpr(rS)),
                             mkU8(shft)), cr );
      }
      break;
   }

   case 0x1D3: 
      
      switch (SPR) {  
      case 0x1:
         DIP("mtxer r%u\n", rS_addr);
         putGST( PPC_GST_XER, mkNarrowTo32(ty, mkexpr(rS)) );
         break;
      case 0x8:
         DIP("mtlr r%u\n", rS_addr);
         putGST( PPC_GST_LR, mkexpr(rS) ); 
         break;
      case 0x9:
         DIP("mtctr r%u\n", rS_addr);
         putGST( PPC_GST_CTR, mkexpr(rS) ); 
         break;
      case 0x100:
         DIP("mtvrsave r%u\n", rS_addr);
         putGST( PPC_GST_VRSAVE, mkNarrowTo32(ty, mkexpr(rS)) );
         break;
      case 0x80:  
         DIP("mtspr r%u (TFHAR)\n", rS_addr);
         putGST( PPC_GST_TFHAR, mkexpr(rS) );
         break;
      case 0x81:  
         DIP("mtspr r%u (TFIAR)\n", rS_addr);
         putGST( PPC_GST_TFIAR, mkexpr(rS) );
         break;
      case 0x82:  
         DIP("mtspr r%u (TEXASR)\n", rS_addr);
         putGST( PPC_GST_TEXASR, mkexpr(rS) );
         break;
      default:
         vex_printf("dis_proc_ctl(ppc)(mtspr,SPR)(%u)\n", SPR);
         return False;
      }
      break;

   case 0x33:                
   {
      UChar XS = ifieldRegXS( theInstr );
      UChar rA_addr = ifieldRegA(theInstr);
      IRExpr * high64;
      IRTemp vS = newTemp( Ity_V128 );
      DIP("mfvsrd r%u,vsr%d\n", rA_addr, (UInt)XS);

      assign( vS, getVSReg( XS ) );
      high64 = unop( Iop_V128HIto64, mkexpr( vS ) );
      putIReg( rA_addr, (mode64) ? high64 :
      unop( Iop_64to32, high64 ) );
      break;
   }

   case 0x73:                
   {
      UChar XS = ifieldRegXS( theInstr );
      UChar rA_addr = ifieldRegA(theInstr);
      IRExpr * high64;
      IRTemp vS = newTemp( Ity_V128 );
      DIP("mfvsrwz r%u,vsr%d\n", rA_addr, (UInt)XS);

      assign( vS, getVSReg( XS ) );
      high64 = unop( Iop_V128HIto64, mkexpr( vS ) );
      
      putIReg( rA_addr, (mode64) ?
                                  binop( Iop_And64, high64, mkU64( 0xFFFFFFFF ) ) :
                                  unop(  Iop_64to32,
                                         binop( Iop_And64, high64, mkU64( 0xFFFFFFFF ) ) ) );
      break;
   }

   case 0xB3:                
   {
      UChar XT = ifieldRegXT( theInstr );
      UChar rA_addr = ifieldRegA(theInstr);
      IRTemp rA = newTemp(ty);
      DIP("mtvsrd vsr%d,r%u\n", (UInt)XT, rA_addr);
      assign( rA, getIReg(rA_addr) );

      if (mode64)
         putVSReg( XT, binop( Iop_64HLtoV128, mkexpr( rA ), mkU64( 0 ) ) );
      else
         putVSReg( XT, binop( Iop_64HLtoV128,
                              binop( Iop_32HLto64,
                                     mkU32( 0 ),
                                     mkexpr( rA ) ),
                                     mkU64( 0 ) ) );
      break;
   }

   case 0xD3:                
   {
      UChar XT = ifieldRegXT( theInstr );
      UChar rA_addr = ifieldRegA(theInstr);
      IRTemp rA = newTemp( Ity_I32 );
      DIP("mtvsrwa vsr%d,r%u\n", (UInt)XT, rA_addr);
      if (mode64)
         assign( rA, unop( Iop_64to32, getIReg( rA_addr ) ) );
      else
         assign( rA, getIReg(rA_addr) );

      putVSReg( XT, binop( Iop_64HLtoV128,
                           unop( Iop_32Sto64, mkexpr( rA ) ),
                           mkU64( 0 ) ) );
      break;
   }

   case 0xF3:                
      {
         UChar XT = ifieldRegXT( theInstr );
         UChar rA_addr = ifieldRegA(theInstr);
         IRTemp rA = newTemp( Ity_I32 );
         DIP("mtvsrwz vsr%d,r%u\n", rA_addr, (UInt)XT);
         if (mode64)
             assign( rA, unop( Iop_64to32, getIReg( rA_addr ) ) );
         else
             assign( rA, getIReg(rA_addr) );

         putVSReg( XT, binop( Iop_64HLtoV128,
                              binop( Iop_32HLto64, mkU32( 0 ), mkexpr ( rA ) ),
                              mkU64( 0 ) ) );
         break;
      }

   default:
      vex_printf("dis_proc_ctl(ppc)(opc2)\n");
      return False;
   }
   return True;
}


static Bool dis_cache_manage ( UInt         theInstr, 
                               DisResult*   dres,
                               const VexArchInfo* guest_archinfo )
{
   
   UChar opc1    = ifieldOPC(theInstr);
   UChar b21to25 = ifieldRegDS(theInstr);
   UChar rA_addr = ifieldRegA(theInstr);
   UChar rB_addr = ifieldRegB(theInstr);
   UInt  opc2    = ifieldOPClo10(theInstr);
   UChar b0      = ifieldBIT0(theInstr);
   UInt  lineszB = guest_archinfo->ppc_icache_line_szB;
   Bool  is_dcbzl = False;

   IRType ty     = mode64 ? Ity_I64 : Ity_I32;

   
   
   
   if (opc1 == 0x1F && ((opc2 == 0x116) || (opc2 == 0xF6))) {
      if (b21to25 == 0x10 || b21to25 < 0x10)
         b21to25 = 0;
   }
   if (opc1 == 0x1F && opc2 == 0x116 && b21to25 == 0x11)
      b21to25 = 0;

   if (opc1 == 0x1F && opc2 == 0x3F6) { 
      if (b21to25 == 1) {
         is_dcbzl = True;
         b21to25 = 0;
         if (!(guest_archinfo->ppc_dcbzl_szB)) {
            vex_printf("dis_cache_manage(ppc)(dcbzl not supported by host)\n");
            return False;
         }
      }
   }

   if (opc1 != 0x1F || b21to25 != 0 || b0 != 0) {
      if (0) vex_printf("dis_cache_manage %d %d %d\n", 
                        (Int)opc1, (Int)b21to25, (Int)b0);
      vex_printf("dis_cache_manage(ppc)(opc1|b21to25|b0)\n");
      return False;
   }

   
   vassert(lineszB == 16 || lineszB == 32 || lineszB == 64 || lineszB == 128);
   
   switch (opc2) {
      
   case 0x056: 
      DIP("dcbf r%u,r%u\n", rA_addr, rB_addr);
      
      break;
      
   case 0x036: 
      DIP("dcbst r%u,r%u\n", rA_addr, rB_addr);
      
      break;

   case 0x116: 
      DIP("dcbt r%u,r%u\n", rA_addr, rB_addr);
      
      break;
      
   case 0x0F6: 
      DIP("dcbtst r%u,r%u\n", rA_addr, rB_addr);
      
      break;
      
   case 0x3F6: { 
                 
      
      IRTemp  EA   = newTemp(ty);
      IRTemp  addr = newTemp(ty);
      IRExpr* irx_addr;
      UInt    i;
      UInt clearszB;
      if (is_dcbzl) {
          clearszB = guest_archinfo->ppc_dcbzl_szB;
          DIP("dcbzl r%u,r%u\n", rA_addr, rB_addr);
      }
      else {
          clearszB = guest_archinfo->ppc_dcbz_szB;
          DIP("dcbz r%u,r%u\n", rA_addr, rB_addr);
      }

      assign( EA, ea_rAor0_idxd(rA_addr, rB_addr) );

      if (mode64) {
         
         assign( addr, binop( Iop_And64,
                              mkexpr(EA),
                              mkU64( ~((ULong)clearszB-1) )) );
         
         for (i = 0; i < clearszB / 8; i++) {
            irx_addr = binop( Iop_Add64, mkexpr(addr), mkU64(i*8) );
            store( irx_addr, mkU64(0) );
         }
      } else {
         
         assign( addr, binop( Iop_And32,
                              mkexpr(EA),
                              mkU32( ~(clearszB-1) )) );
         
         for (i = 0; i < clearszB / 4; i++) {
            irx_addr = binop( Iop_Add32, mkexpr(addr), mkU32(i*4) );
            store( irx_addr, mkU32(0) );
         }
      }
      break;
   }

   case 0x3D6: { 
      
      IRTemp EA   = newTemp(ty);
      IRTemp addr = newTemp(ty);
      DIP("icbi r%u,r%u\n", rA_addr, rB_addr);
      assign( EA, ea_rAor0_idxd(rA_addr, rB_addr) );

      
      assign( addr, binop( mkSzOp(ty, Iop_And8),
                           mkexpr(EA),
                           mkSzImm(ty, ~(((ULong)lineszB)-1) )) );
      putGST( PPC_GST_CMSTART, mkexpr(addr) );
      putGST( PPC_GST_CMLEN, mkSzImm(ty, lineszB) );

      
      stmt( IRStmt_MBE(Imbe_Fence) );

      putGST( PPC_GST_CIA, mkSzImm(ty, nextInsnAddr()));
      dres->jk_StopHere = Ijk_InvalICache;
      dres->whatNext    = Dis_StopHere;
      break;
   }

   default:
      vex_printf("dis_cache_manage(ppc)(opc2)\n");
      return False;
   }
   return True;
}



static IRExpr*  get_IR_roundingmode ( void )
{
   IRTemp rm_PPC32 = newTemp(Ity_I32);
   assign( rm_PPC32, getGST_masked( PPC_GST_FPSCR, MASK_FPSCR_RN ) );

   
   return binop( Iop_Xor32, 
                 mkexpr(rm_PPC32),
                 binop( Iop_And32, 
                        binop(Iop_Shl32, mkexpr(rm_PPC32), mkU8(1)),
                        mkU32(2) ));
}

static IRExpr*  get_IR_roundingmode_DFP( void )
{
   IRTemp rm_PPC32 = newTemp( Ity_I32 );
   assign( rm_PPC32, getGST_masked_upper( PPC_GST_FPSCR, MASK_FPSCR_DRN ) );

   
   return binop( Iop_Xor32,
                 mkexpr( rm_PPC32 ),
                 binop( Iop_And32,
                        binop( Iop_Shl32, mkexpr( rm_PPC32 ), mkU8( 1 ) ),
                        mkU32( 2 ) ) );
}

#define NANmaskSingle   0x7F800000
#define NANmaskDouble   0x7FF00000

static IRExpr * Check_NaN( IRExpr * value, IRExpr * Hi32Mask )
{
   IRTemp exp_zero  = newTemp(Ity_I8);
   IRTemp frac_mask = newTemp(Ity_I32);
   IRTemp frac_not_zero = newTemp(Ity_I8);

   assign( frac_mask, unop( Iop_Not32,
                            binop( Iop_Or32,
                                   mkU32( 0x80000000ULL ), Hi32Mask) ) );

   assign( exp_zero,
           unop( Iop_1Sto8,
                 binop( Iop_CmpEQ32,
                        binop( Iop_And32,
                               unop( Iop_64HIto32,
                                     unop( Iop_ReinterpF64asI64,
                                           value ) ),
                               Hi32Mask ),
                        Hi32Mask ) ) );
   assign( frac_not_zero,
           binop( Iop_Or8,
                  unop( Iop_1Sto8,
                        binop( Iop_CmpNE32,
                               binop( Iop_And32,
                                      unop( Iop_64HIto32,
                                            unop( Iop_ReinterpF64asI64,
                                                  value ) ),
                                      mkexpr( frac_mask ) ),
                               mkU32( 0x0 ) ) ),
                  unop( Iop_1Sto8,
                        binop( Iop_CmpNE32,
                               binop( Iop_And32,
                                      unop( Iop_64to32,
                                            unop( Iop_ReinterpF64asI64,
                                                  value ) ),
                                      mkU32( 0xFFFFFFFF ) ),
                               mkU32( 0x0 ) ) ) ) );
   return unop( Iop_8Sto32,
                binop( Iop_And8,
                       mkexpr( exp_zero ),
                       mkexpr( frac_not_zero ) ) );
}

static IRExpr * Complement_non_NaN( IRExpr * value, IRExpr * nan_mask )
{
   return  binop( Iop_32HLto64,
                  binop( Iop_Or32,
                         binop( Iop_And32,
                                nan_mask,
                                unop( Iop_64HIto32,
                                      unop( Iop_ReinterpF64asI64,
                                            value ) ) ),
                         binop( Iop_And32,
                                unop( Iop_Not32,
                                      nan_mask ),
                                unop( Iop_64HIto32,
                                      unop( Iop_ReinterpF64asI64,
                                            unop( Iop_NegF64,
                                                  value ) ) ) ) ),
                  unop( Iop_64to32,
                        unop( Iop_ReinterpF64asI64, value ) ) );
}


static Bool dis_fp_load ( UInt theInstr )
{
   
   UChar opc1      = ifieldOPC(theInstr);
   UChar frD_addr  = ifieldRegDS(theInstr);
   UChar rA_addr   = ifieldRegA(theInstr);
   UChar rB_addr   = ifieldRegB(theInstr);
   UInt  opc2      = ifieldOPClo10(theInstr);
   UChar b0        = ifieldBIT0(theInstr);
   UInt  uimm16    = ifieldUIMM16(theInstr);

   Int    simm16 = extend_s_16to32(uimm16);
   IRType ty     = mode64 ? Ity_I64 : Ity_I32;
   IRTemp EA     = newTemp(ty);
   IRTemp rA     = newTemp(ty);
   IRTemp rB     = newTemp(ty);
   IRTemp iHi    = newTemp(Ity_I32);
   IRTemp iLo    = newTemp(Ity_I32);

   assign( rA, getIReg(rA_addr) );
   assign( rB, getIReg(rB_addr) );


   switch (opc1) {
   case 0x30: 
      DIP("lfs fr%u,%d(r%u)\n", frD_addr, simm16, rA_addr);
      assign( EA, ea_rAor0_simm(rA_addr, simm16) );
      putFReg( frD_addr,
               unop(Iop_F32toF64, load(Ity_F32, mkexpr(EA))) );
      break;

   case 0x31: 
      if (rA_addr == 0)
         return False;
      DIP("lfsu fr%u,%d(r%u)\n", frD_addr, simm16, rA_addr);
      assign( EA, ea_rA_simm(rA_addr, simm16) );
      putFReg( frD_addr,
               unop(Iop_F32toF64, load(Ity_F32, mkexpr(EA))) );
      putIReg( rA_addr, mkexpr(EA) );
      break;
      
   case 0x32: 
      DIP("lfd fr%u,%d(r%u)\n", frD_addr, simm16, rA_addr);
      assign( EA, ea_rAor0_simm(rA_addr, simm16) );
      putFReg( frD_addr, load(Ity_F64, mkexpr(EA)) );
      break;

   case 0x33: 
      if (rA_addr == 0)
         return False;
      DIP("lfdu fr%u,%d(r%u)\n", frD_addr, simm16, rA_addr);
      assign( EA, ea_rA_simm(rA_addr, simm16) );
      putFReg( frD_addr, load(Ity_F64, mkexpr(EA)) );
      putIReg( rA_addr, mkexpr(EA) );
      break;

   case 0x1F:
      if (b0 != 0) {
         vex_printf("dis_fp_load(ppc)(instr,b0)\n");
         return False;
      }

      switch(opc2) {
      case 0x217: 
         DIP("lfsx fr%u,r%u,r%u\n", frD_addr, rA_addr, rB_addr);
         assign( EA, ea_rAor0_idxd(rA_addr, rB_addr) );
         putFReg( frD_addr, unop( Iop_F32toF64, 
                                  load(Ity_F32, mkexpr(EA))) );
         break;
         
      case 0x237: 
         if (rA_addr == 0)
            return False;
         DIP("lfsux fr%u,r%u,r%u\n", frD_addr, rA_addr, rB_addr);
         assign( EA, ea_rA_idxd(rA_addr, rB_addr) );
         putFReg( frD_addr,
                  unop(Iop_F32toF64, load(Ity_F32, mkexpr(EA))) );
         putIReg( rA_addr, mkexpr(EA) );
         break;
         
      case 0x257: 
         DIP("lfdx fr%u,r%u,r%u\n", frD_addr, rA_addr, rB_addr);
         assign( EA, ea_rAor0_idxd(rA_addr, rB_addr) );
         putFReg( frD_addr, load(Ity_F64, mkexpr(EA)) );
         break;
         
      case 0x277: 
         if (rA_addr == 0)
            return False;
         DIP("lfdux fr%u,r%u,r%u\n", frD_addr, rA_addr, rB_addr);
         assign( EA, ea_rA_idxd(rA_addr, rB_addr) );
         putFReg( frD_addr, load(Ity_F64, mkexpr(EA)) );
         putIReg( rA_addr, mkexpr(EA) );
         break;
         
      case 0x357: 
         DIP("lfiwax fr%u,r%u,r%u\n", frD_addr, rA_addr, rB_addr);
         assign( EA, ea_rAor0_idxd( rA_addr, rB_addr ) );
         assign( iLo, load(Ity_I32, mkexpr(EA)) );
         assign( iHi, binop(Iop_Sub32,
                            mkU32(0),
                            binop(Iop_Shr32, mkexpr(iLo), mkU8(31)))  );
         putFReg( frD_addr, unop(Iop_ReinterpI64asF64,
                                 binop(Iop_32HLto64, mkexpr(iHi), mkexpr(iLo))) );
         break;

      case 0x377: 
      {
         IRTemp dw = newTemp( Ity_I64 );
         DIP("lfiwzx fr%u,r%u,r%u\n", frD_addr, rA_addr, rB_addr);
         assign( EA, ea_rAor0_idxd( rA_addr, rB_addr ) );
         assign( iLo, load(Ity_I32, mkexpr(EA)) );
         assign( dw, binop( Iop_32HLto64, mkU32( 0 ), mkexpr( iLo ) ) );
         putFReg( frD_addr, unop( Iop_ReinterpI64asF64, mkexpr( dw ) ) );
         break;
      }

      default:
         vex_printf("dis_fp_load(ppc)(opc2)\n");
         return False;
      }
      break;

   default:
      vex_printf("dis_fp_load(ppc)(opc1)\n");
      return False;
   }
   return True;
}



static Bool dis_fp_store ( UInt theInstr )
{
   
   UChar opc1      = ifieldOPC(theInstr);
   UChar frS_addr  = ifieldRegDS(theInstr);
   UChar rA_addr   = ifieldRegA(theInstr);
   UChar rB_addr   = ifieldRegB(theInstr);
   UInt  opc2      = ifieldOPClo10(theInstr);
   UChar b0        = ifieldBIT0(theInstr);
   Int   uimm16    = ifieldUIMM16(theInstr);

   Int    simm16 = extend_s_16to32(uimm16);
   IRTemp frS    = newTemp(Ity_F64);
   IRType ty     = mode64 ? Ity_I64 : Ity_I32;
   IRTemp EA     = newTemp(ty);
   IRTemp rA     = newTemp(ty);
   IRTemp rB     = newTemp(ty);

   assign( frS, getFReg(frS_addr) );
   assign( rA,  getIReg(rA_addr) );
   assign( rB,  getIReg(rB_addr) );


   switch (opc1) {

   case 0x34: 
      DIP("stfs fr%u,%d(r%u)\n", frS_addr, simm16, rA_addr);
      assign( EA, ea_rAor0_simm(rA_addr, simm16) );
      store( mkexpr(EA), unop(Iop_TruncF64asF32, mkexpr(frS)) );
      break;

   case 0x35: 
      if (rA_addr == 0)
         return False;
      DIP("stfsu fr%u,%d(r%u)\n", frS_addr, simm16, rA_addr);
      assign( EA, ea_rA_simm(rA_addr, simm16) );
      
      store( mkexpr(EA), unop(Iop_TruncF64asF32, mkexpr(frS)) );
      putIReg( rA_addr, mkexpr(EA) );
      break;

   case 0x36: 
      DIP("stfd fr%u,%d(r%u)\n", frS_addr, simm16, rA_addr);
      assign( EA, ea_rAor0_simm(rA_addr, simm16) );
      store( mkexpr(EA), mkexpr(frS) );
      break;

   case 0x37: 
      if (rA_addr == 0)
         return False;
      DIP("stfdu fr%u,%d(r%u)\n", frS_addr, simm16, rA_addr);
      assign( EA, ea_rA_simm(rA_addr, simm16) );
      store( mkexpr(EA), mkexpr(frS) );
      putIReg( rA_addr, mkexpr(EA) );
      break;

   case 0x1F:
      if (b0 != 0) {
         vex_printf("dis_fp_store(ppc)(instr,b0)\n");
         return False;
      }
      switch(opc2) {
      case 0x297: 
         DIP("stfsx fr%u,r%u,r%u\n", frS_addr, rA_addr, rB_addr);
         assign( EA, ea_rAor0_idxd(rA_addr, rB_addr) );
         
         store( mkexpr(EA),
                unop(Iop_TruncF64asF32, mkexpr(frS)) );
         break;
         
      case 0x2B7: 
         if (rA_addr == 0)
            return False;
         DIP("stfsux fr%u,r%u,r%u\n", frS_addr, rA_addr, rB_addr);
         assign( EA, ea_rA_idxd(rA_addr, rB_addr) );
         
         store( mkexpr(EA), unop(Iop_TruncF64asF32, mkexpr(frS)) );
         putIReg( rA_addr, mkexpr(EA) );
         break;

      case 0x2D7: 
         DIP("stfdx fr%u,r%u,r%u\n", frS_addr, rA_addr, rB_addr);
         assign( EA, ea_rAor0_idxd(rA_addr, rB_addr) );
         store( mkexpr(EA), mkexpr(frS) );
         break;
         
      case 0x2F7: 
         if (rA_addr == 0)
            return False;
         DIP("stfdux fr%u,r%u,r%u\n", frS_addr, rA_addr, rB_addr);
         assign( EA, ea_rA_idxd(rA_addr, rB_addr) );
         store( mkexpr(EA), mkexpr(frS) );
         putIReg( rA_addr, mkexpr(EA) );
         break;

      case 0x3D7: 
         
         DIP("stfiwx fr%u,r%u,r%u\n", frS_addr, rA_addr, rB_addr);
         assign( EA, ea_rAor0_idxd(rA_addr, rB_addr) );
         store( mkexpr(EA),
                unop(Iop_64to32, unop(Iop_ReinterpF64asI64, mkexpr(frS))) );
         break;

      default:
         vex_printf("dis_fp_store(ppc)(opc2)\n");
         return False;
      }
      break;

   default:
      vex_printf("dis_fp_store(ppc)(opc1)\n");
      return False;
   }
   return True;
}



static Bool dis_fp_arith ( UInt theInstr )
{
   
   UChar opc1     = ifieldOPC(theInstr);
   UChar frD_addr = ifieldRegDS(theInstr);
   UChar frA_addr = ifieldRegA(theInstr);
   UChar frB_addr = ifieldRegB(theInstr);
   UChar frC_addr = ifieldRegC(theInstr);
   UChar opc2     = ifieldOPClo5(theInstr);
   UChar flag_rC  = ifieldBIT0(theInstr);

   IRTemp  frD = newTemp(Ity_F64);
   IRTemp  frA = newTemp(Ity_F64);
   IRTemp  frB = newTemp(Ity_F64);
   IRTemp  frC = newTemp(Ity_F64);
   IRExpr* rm  = get_IR_roundingmode();

   Bool set_FPRF = True;

   Bool clear_CR1 = True;

   assign( frA, getFReg(frA_addr));
   assign( frB, getFReg(frB_addr));
   assign( frC, getFReg(frC_addr));

   switch (opc1) {
   case 0x3B:
      switch (opc2) {
      case 0x12: 
         if (frC_addr != 0)
            return False;
         DIP("fdivs%s fr%u,fr%u,fr%u\n", flag_rC ? ".":"",
             frD_addr, frA_addr, frB_addr);
         assign( frD, triop( Iop_DivF64r32, 
                             rm, mkexpr(frA), mkexpr(frB) ));
         break;

      case 0x14: 
         if (frC_addr != 0)
            return False;
         DIP("fsubs%s fr%u,fr%u,fr%u\n", flag_rC ? ".":"",
             frD_addr, frA_addr, frB_addr);
         assign( frD, triop( Iop_SubF64r32, 
                             rm, mkexpr(frA), mkexpr(frB) ));
         break;

      case 0x15: 
         if (frC_addr != 0)
            return False;
         DIP("fadds%s fr%u,fr%u,fr%u\n", flag_rC ? ".":"",
             frD_addr, frA_addr, frB_addr);
         assign( frD, triop( Iop_AddF64r32, 
                             rm, mkexpr(frA), mkexpr(frB) ));
         break;

      case 0x16: 
         
         if (frA_addr != 0 || frC_addr != 0)
            return False;
         DIP("fsqrts%s fr%u,fr%u\n", flag_rC ? ".":"",
             frD_addr, frB_addr);
         
         
         assign( frD, binop( Iop_SqrtF64, rm, mkexpr(frB) ));
         break;

      case 0x18: 
         
         if (frA_addr != 0 || frC_addr != 0)
            return False;
         DIP("fres%s fr%u,fr%u\n", flag_rC ? ".":"",
             frD_addr, frB_addr);
         { IRExpr* ieee_one
              = IRExpr_Const(IRConst_F64i(0x3ff0000000000000ULL));
           assign( frD, triop( Iop_DivF64r32, 
                               rm,
                               ieee_one, mkexpr(frB) ));
         }
         break;

      case 0x19: 
         if (frB_addr != 0)
            return False;
         DIP("fmuls%s fr%u,fr%u,fr%u\n", flag_rC ? ".":"",
             frD_addr, frA_addr, frC_addr);
         assign( frD, triop( Iop_MulF64r32,
                             rm, mkexpr(frA), mkexpr(frC) ));
         break;

      case 0x1A: 
         
         
         if (frA_addr != 0 || frC_addr != 0)
            return False;
         DIP("frsqrtes%s fr%u,fr%u\n", flag_rC ? ".":"",
             frD_addr, frB_addr);
         assign( frD, unop(Iop_RSqrtEst5GoodF64, mkexpr(frB)) );
         break;

      default:
         vex_printf("dis_fp_arith(ppc)(3B: opc2)\n");
         return False;
      }
      break;

   case 0x3F:
      switch (opc2) {           
      case 0x12: 
         if (frC_addr != 0)
            return False;
         DIP("fdiv%s fr%u,fr%u,fr%u\n", flag_rC ? ".":"",
             frD_addr, frA_addr, frB_addr);
         assign( frD, triop(Iop_DivF64, rm, mkexpr(frA), mkexpr(frB)) );
         break;

      case 0x14: 
         if (frC_addr != 0)
            return False;
         DIP("fsub%s fr%u,fr%u,fr%u\n", flag_rC ? ".":"",
             frD_addr, frA_addr, frB_addr);
         assign( frD, triop(Iop_SubF64, rm, mkexpr(frA), mkexpr(frB)) );
         break;

      case 0x15: 
         if (frC_addr != 0)
            return False;
         DIP("fadd%s fr%u,fr%u,fr%u\n", flag_rC ? ".":"",
             frD_addr, frA_addr, frB_addr);
         assign( frD, triop(Iop_AddF64, rm, mkexpr(frA), mkexpr(frB)) );
         break;

      case 0x16: 
         
         if (frA_addr != 0 || frC_addr != 0)
            return False;
         DIP("fsqrt%s fr%u,fr%u\n", flag_rC ? ".":"",
             frD_addr, frB_addr);
         assign( frD, binop(Iop_SqrtF64, rm, mkexpr(frB)) );
         break;

      case 0x17: { 
         
         IRTemp cc    = newTemp(Ity_I32);
         IRTemp cc_b0 = newTemp(Ity_I32);

         DIP("fsel%s fr%u,fr%u,fr%u,fr%u\n", flag_rC ? ".":"",
             frD_addr, frA_addr, frC_addr, frB_addr);

         
         
         assign( cc, binop(Iop_CmpF64, mkexpr(frA),
                                       IRExpr_Const(IRConst_F64(0))) );
         assign( cc_b0, binop(Iop_And32, mkexpr(cc), mkU32(1)) );

         
         
         assign( frD,
                 IRExpr_ITE(
                    binop(Iop_CmpEQ32, mkexpr(cc_b0), mkU32(0)),
                    mkexpr(frC),
                    mkexpr(frB) ));

         
         set_FPRF = False;
         break;
      }

      case 0x18: 
         
         
         
         if (frA_addr != 0 || frC_addr != 0)
            return False;
         DIP("fre%s fr%u,fr%u\n", flag_rC ? ".":"",
             frD_addr, frB_addr);
         { IRExpr* ieee_one
              = IRExpr_Const(IRConst_F64i(0x3ff0000000000000ULL));
           assign( frD, triop( Iop_DivF64, 
                               rm,
                               ieee_one, mkexpr(frB) ));
         }
         break;

      case 0x19: 
         if (frB_addr != 0)
            vex_printf("dis_fp_arith(ppc)(instr,fmul)\n");
         DIP("fmul%s fr%u,fr%u,fr%u\n", flag_rC ? ".":"",
             frD_addr, frA_addr, frC_addr);
         assign( frD, triop(Iop_MulF64, rm, mkexpr(frA), mkexpr(frC)) );
         break;

      case 0x1A: 
         
         if (frA_addr != 0 || frC_addr != 0)
            return False;
         DIP("frsqrte%s fr%u,fr%u\n", flag_rC ? ".":"",
             frD_addr, frB_addr);
         assign( frD, unop(Iop_RSqrtEst5GoodF64, mkexpr(frB)) );
         break;

      default:
         vex_printf("dis_fp_arith(ppc)(3F: opc2)\n");
         return False;
      }
      break;

   default:
      vex_printf("dis_fp_arith(ppc)(opc1)\n");
      return False;
   }

   putFReg( frD_addr, mkexpr(frD) );

   if (set_FPRF) {
      
      
   }

   if (flag_rC && clear_CR1) {
      putCR321( 1, mkU8(0) );
      putCR0( 1, mkU8(0) );
   }

   return True;
}



static Bool dis_fp_multadd ( UInt theInstr )
{
   
   UChar opc1     = ifieldOPC(theInstr);
   UChar frD_addr = ifieldRegDS(theInstr);
   UChar frA_addr = ifieldRegA(theInstr);
   UChar frB_addr = ifieldRegB(theInstr);
   UChar frC_addr = ifieldRegC(theInstr);
   UChar opc2     = ifieldOPClo5(theInstr);
   UChar flag_rC  = ifieldBIT0(theInstr);

   IRTemp  frD = newTemp(Ity_F64);
   IRTemp  frA = newTemp(Ity_F64);
   IRTemp  frB = newTemp(Ity_F64);
   IRTemp  frC = newTemp(Ity_F64);
   IRTemp  rmt = newTemp(Ity_I32);
   IRTemp  tmp = newTemp(Ity_F64);
   IRTemp  sign_tmp = newTemp(Ity_I64);
   IRTemp  nan_mask = newTemp(Ity_I32);
   IRExpr* rm;

   Bool set_FPRF = True;

   Bool clear_CR1 = True;

   assign( rmt, get_IR_roundingmode() );
   rm = mkexpr(rmt);

   assign( frA, getFReg(frA_addr));
   assign( frB, getFReg(frB_addr));
   assign( frC, getFReg(frC_addr));


   switch (opc1) {
   case 0x3B:
      switch (opc2) {
      case 0x1C: 
         DIP("fmsubs%s fr%u,fr%u,fr%u,fr%u\n", flag_rC ? ".":"",
             frD_addr, frA_addr, frC_addr, frB_addr);
         assign( frD, qop( Iop_MSubF64r32, rm,
                           mkexpr(frA), mkexpr(frC), mkexpr(frB) ));
         break;

      case 0x1D: 
         DIP("fmadds%s fr%u,fr%u,fr%u,fr%u\n", flag_rC ? ".":"",
             frD_addr, frA_addr, frC_addr, frB_addr);
         assign( frD, qop( Iop_MAddF64r32, rm,
                           mkexpr(frA), mkexpr(frC), mkexpr(frB) ));
         break;

      case 0x1E: 
      case 0x1F: 

         if (opc2 == 0x1E) {
            DIP("fnmsubs%s fr%u,fr%u,fr%u,fr%u\n", flag_rC ? ".":"",
                     frD_addr, frA_addr, frC_addr, frB_addr);
            assign( tmp, qop( Iop_MSubF64r32, rm,
                              mkexpr(frA), mkexpr(frC), mkexpr(frB) ) );
         } else {
            DIP("fnmadds%s fr%u,fr%u,fr%u,fr%u\n", flag_rC ? ".":"",
                     frD_addr, frA_addr, frC_addr, frB_addr);
            assign( tmp, qop( Iop_MAddF64r32, rm,
                              mkexpr(frA), mkexpr(frC), mkexpr(frB) ) );
         }

         assign( nan_mask, Check_NaN( mkexpr( tmp ),
                                      mkU32( NANmaskSingle ) ) );
         assign( sign_tmp, Complement_non_NaN( mkexpr( tmp ),
                                               mkexpr( nan_mask ) ) );
         assign( frD, unop( Iop_ReinterpI64asF64, mkexpr( sign_tmp ) ) ); 
         break;

      default:
         vex_printf("dis_fp_multadd(ppc)(3B: opc2)\n");
         return False;
      }
      break;

   case 0x3F:
      switch (opc2) {           
      case 0x1C: 
         DIP("fmsub%s fr%u,fr%u,fr%u,fr%u\n", flag_rC ? ".":"",
             frD_addr, frA_addr, frC_addr, frB_addr);
         assign( frD, qop( Iop_MSubF64, rm,
                           mkexpr(frA), mkexpr(frC), mkexpr(frB) ));
         break;

      case 0x1D: 
         DIP("fmadd%s fr%u,fr%u,fr%u,fr%u\n", flag_rC ? ".":"",
             frD_addr, frA_addr, frC_addr, frB_addr);
         assign( frD, qop( Iop_MAddF64, rm,
                           mkexpr(frA), mkexpr(frC), mkexpr(frB) ));
         break;

      case 0x1E: 
      case 0x1F: 

         if (opc2 == 0x1E) {
            DIP("fnmsub%s fr%u,fr%u,fr%u,fr%u\n", flag_rC ? ".":"",
                     frD_addr, frA_addr, frC_addr, frB_addr);
            assign( tmp, qop( Iop_MSubF64, rm,
                              mkexpr(frA), mkexpr(frC), mkexpr(frB) ) );
         } else {
            DIP("fnmadd%s fr%u,fr%u,fr%u,fr%u\n", flag_rC ? ".":"",
                     frD_addr, frA_addr, frC_addr, frB_addr);
            assign( tmp, qop( Iop_MAddF64, rm,
                              mkexpr(frA), mkexpr(frC), mkexpr(frB) ));
         }

         assign( nan_mask, Check_NaN( mkexpr( tmp ),
                                      mkU32( NANmaskDouble ) ) );
         assign( sign_tmp, Complement_non_NaN( mkexpr( tmp ),
                                               mkexpr( nan_mask ) ) );
         assign( frD, unop( Iop_ReinterpI64asF64, mkexpr( sign_tmp ) ) ); 
         break;

      default:
         vex_printf("dis_fp_multadd(ppc)(3F: opc2)\n");
         return False;
      }
      break;

   default:
      vex_printf("dis_fp_multadd(ppc)(opc1)\n");
      return False;
   }

   putFReg( frD_addr, mkexpr(frD) );

   if (set_FPRF) {
      
      
   }

   if (flag_rC && clear_CR1) {
      putCR321( 1, mkU8(0) );
      putCR0( 1, mkU8(0) );
   }

   return True;
}

static void do_fp_tsqrt(IRTemp frB_Int, Bool sp, IRTemp * fe_flag_tmp, IRTemp * fg_flag_tmp)
{
   
   IRTemp e_b = newTemp(Ity_I32);
   IRExpr * fe_flag,  * fg_flag;
   IRTemp frB_exp_shR = newTemp(Ity_I32);
   UInt bias = sp? 127 : 1023;
   IRExpr * frbNaN, * frbDenorm, * frBNeg;
   IRExpr * eb_LTE;
   IRTemp  frbZero_tmp = newTemp(Ity_I1);
   IRTemp  frbInf_tmp = newTemp(Ity_I1);
   *fe_flag_tmp = newTemp(Ity_I32);
   *fg_flag_tmp = newTemp(Ity_I32);
   assign( frB_exp_shR, fp_exp_part( frB_Int, sp ) );
   assign(e_b, binop( Iop_Sub32, mkexpr(frB_exp_shR), mkU32( bias ) ));

   
   frbNaN = sp ? is_NaN_32(frB_Int) : is_NaN(frB_Int);
   assign( frbInf_tmp, is_Inf(frB_Int, sp) );
   assign( frbZero_tmp, is_Zero(frB_Int, sp ) );
   {
      
      UInt test_value = sp ? 0xffffff99 : 0xfffffc36;
      eb_LTE = binop( Iop_CmpLE32S, mkexpr( e_b ), mkU32( test_value ) );
   }
   frBNeg = binop( Iop_CmpEQ32,
                   binop( Iop_Shr32,
                          sp ? mkexpr( frB_Int ) : unop( Iop_64HIto32, mkexpr( frB_Int ) ),
                          mkU8( 31 ) ),
                   mkU32( 1 ) );
   

   

   if (sp) {
      IRTemp frac_part = newTemp(Ity_I32);
      assign( frac_part, binop( Iop_And32, mkexpr(frB_Int), mkU32(0x007fffff)) );
      frbDenorm
               = mkAND1( binop( Iop_CmpEQ32, mkexpr( frB_exp_shR ), mkU32( 0 ) ),
                         binop( Iop_CmpNE32, mkexpr( frac_part ), mkU32( 0 ) ) );
   } else {
      IRExpr * hi32, * low32, * fraction_is_nonzero;
      IRTemp frac_part = newTemp(Ity_I64);

      assign( frac_part, FP_FRAC_PART(frB_Int) );
      hi32 = unop( Iop_64HIto32, mkexpr( frac_part ) );
      low32 = unop( Iop_64to32, mkexpr( frac_part ) );
      fraction_is_nonzero = binop( Iop_CmpNE32, binop( Iop_Or32, low32, hi32 ),
                                                mkU32( 0 ) );
      frbDenorm
               = mkAND1( binop( Iop_CmpEQ32, mkexpr( frB_exp_shR ), mkU32( 0 ) ),
                         fraction_is_nonzero );
   }
   

   
   fe_flag = mkOR1( mkexpr( frbZero_tmp ),
                    mkOR1( frbNaN,
                           mkOR1( mkexpr( frbInf_tmp ),
                                  mkOR1( frBNeg, eb_LTE ) ) ) );

   fe_flag = unop(Iop_1Uto32, fe_flag);

   fg_flag = mkOR1( mkexpr( frbZero_tmp ),
                    mkOR1( mkexpr( frbInf_tmp ), frbDenorm ) );
   fg_flag = unop(Iop_1Uto32, fg_flag);
   assign (*fg_flag_tmp, fg_flag);
   assign (*fe_flag_tmp, fe_flag);
}
static void _do_fp_tdiv(IRTemp frA_int, IRTemp frB_int, Bool sp, IRTemp * fe_flag_tmp, IRTemp * fg_flag_tmp)
{
   
   IRTemp e_a = newTemp(Ity_I32);
   IRTemp e_b = newTemp(Ity_I32);
   IRTemp frA_exp_shR = newTemp(Ity_I32);
   IRTemp frB_exp_shR = newTemp(Ity_I32);

   UInt bias = sp? 127 : 1023;
   *fe_flag_tmp = newTemp(Ity_I32);
   *fg_flag_tmp = newTemp(Ity_I32);

   IRExpr * fraNaN, * frbNaN, * frbDenorm;
   IRExpr * eb_LTE, * eb_GTE, * ea_eb_GTE, * ea_eb_LTE, * ea_LTE;
   IRTemp  fraInf_tmp = newTemp(Ity_I1);
   IRTemp  frbZero_tmp = newTemp(Ity_I1);
   IRTemp  frbInf_tmp = newTemp(Ity_I1);
   IRTemp  fraNotZero_tmp = newTemp(Ity_I1);

   IRExpr * fe_flag, * fg_flag;

   
   assign( frA_exp_shR, fp_exp_part( frA_int, sp ) );
   assign( frB_exp_shR, fp_exp_part( frB_int, sp ) );
   
   assign(e_a, binop( Iop_Sub32, mkexpr(frA_exp_shR), mkU32( bias ) ));
   assign(e_b, binop( Iop_Sub32, mkexpr(frB_exp_shR), mkU32( bias ) ));


   
   

   fraNaN = sp ? is_NaN_32(frA_int) : is_NaN(frA_int);
   assign(fraInf_tmp, is_Inf(frA_int, sp));

   frbNaN = sp ? is_NaN_32(frB_int) : is_NaN(frB_int);
   assign( frbInf_tmp, is_Inf(frB_int, sp) );
   assign( frbZero_tmp, is_Zero(frB_int, sp) );

   {
      UInt test_value = sp ? 0xffffff82 : 0xfffffc02;
      eb_LTE = binop(Iop_CmpLE32S, mkexpr(e_b), mkU32(test_value));
   }

   {
      Int test_value = sp ? 125 : 1021;
      eb_GTE = binop(Iop_CmpLT32S, mkU32(test_value), mkexpr(e_b));
   }

   assign( fraNotZero_tmp, unop( Iop_Not1, is_Zero( frA_int, sp ) ) );
   ea_eb_GTE = mkAND1( mkexpr( fraNotZero_tmp ),
                       binop( Iop_CmpLT32S, mkU32( bias ),
                              binop( Iop_Sub32, mkexpr( e_a ),
                                     mkexpr( e_b ) ) ) );

   {
      UInt test_value = sp ? 0xffffff83 : 0xfffffc03;

      ea_eb_LTE = mkAND1( mkexpr( fraNotZero_tmp ),
                          binop( Iop_CmpLE32S,
                                 binop( Iop_Sub32,
                                        mkexpr( e_a ),
                                        mkexpr( e_b ) ),
                                        mkU32( test_value ) ) );
   }

   {
      UInt test_value = 0xfffffc36;  

      ea_LTE = mkAND1( mkexpr( fraNotZero_tmp ), binop( Iop_CmpLE32S,
                                                        mkexpr( e_a ),
                                                        mkU32( test_value ) ) );
   }
   

   

   {
      IRExpr * fraction_is_nonzero;

      if (sp) {
         fraction_is_nonzero = binop( Iop_CmpNE32, FP_FRAC_PART32(frB_int),
                                      mkU32( 0 ) );
      } else {
         IRExpr * hi32, * low32;
         IRTemp frac_part = newTemp(Ity_I64);
         assign( frac_part, FP_FRAC_PART(frB_int) );

         hi32 = unop( Iop_64HIto32, mkexpr( frac_part ) );
         low32 = unop( Iop_64to32, mkexpr( frac_part ) );
         fraction_is_nonzero = binop( Iop_CmpNE32, binop( Iop_Or32, low32, hi32 ),
                                      mkU32( 0 ) );
      }
      frbDenorm = mkAND1( binop( Iop_CmpEQ32, mkexpr( frB_exp_shR ),
                                 mkU32( 0x0 ) ), fraction_is_nonzero );

   }
   

   fe_flag
   = mkOR1(
            fraNaN,
            mkOR1(
                   mkexpr( fraInf_tmp ),
                   mkOR1(
                          mkexpr( frbZero_tmp ),
                          mkOR1(
                                 frbNaN,
                                 mkOR1(
                                        mkexpr( frbInf_tmp ),
                                        mkOR1( eb_LTE,
                                               mkOR1( eb_GTE,
                                                      mkOR1( ea_eb_GTE,
                                                             mkOR1( ea_eb_LTE,
                                                                    ea_LTE ) ) ) ) ) ) ) ) );

   fe_flag = unop(Iop_1Uto32, fe_flag);

   fg_flag = mkOR1( mkexpr( fraInf_tmp ), mkOR1( mkexpr( frbZero_tmp ),
                                                 mkOR1( mkexpr( frbInf_tmp ),
                                                        frbDenorm ) ) );
   fg_flag = unop(Iop_1Uto32, fg_flag);
   assign(*fe_flag_tmp, fe_flag);
   assign(*fg_flag_tmp, fg_flag);
}

static IRExpr * do_fp_tdiv(IRTemp frA_int, IRTemp frB_int)
{
   IRTemp  fe_flag, fg_flag;
   
   IRExpr * fl_flag = unop(Iop_Not32, mkU32(0xFFFFFE));
   fe_flag = fg_flag = IRTemp_INVALID;
   _do_fp_tdiv(frA_int, frB_int, False, &fe_flag, &fg_flag);
   return binop( Iop_Or32,
                 binop( Iop_Or32,
                        binop( Iop_Shl32, fl_flag, mkU8( 3 ) ),
                        binop( Iop_Shl32, mkexpr(fg_flag), mkU8( 2 ) ) ),
                 binop( Iop_Shl32, mkexpr(fe_flag), mkU8( 1 ) ) );
}

static Bool dis_fp_tests ( UInt theInstr )
{
   UChar opc1     = ifieldOPC(theInstr);
   UChar crfD     = toUChar( IFIELD( theInstr, 23, 3 ) );
   UChar frB_addr = ifieldRegB(theInstr);
   UChar b0       = ifieldBIT0(theInstr);
   UInt  opc2     = ifieldOPClo10(theInstr);
   IRTemp frB_I64     = newTemp(Ity_I64);

   if (opc1 != 0x3F || b0 != 0 ){
      vex_printf("dis_fp_tests(ppc)(ftdiv)\n");
      return False;
   }
   assign( frB_I64, unop( Iop_ReinterpF64asI64, getFReg( frB_addr ) ) );

   switch (opc2) {
      case 0x080: 
      {
         UChar frA_addr = ifieldRegA(theInstr);
         IRTemp frA_I64     = newTemp(Ity_I64);
         UChar b21to22  = toUChar( IFIELD( theInstr, 21, 2 ) );
         if (b21to22 != 0 ) {
            vex_printf("dis_fp_tests(ppc)(ftdiv)\n");
            return False;
         }

         assign( frA_I64, unop( Iop_ReinterpF64asI64, getFReg( frA_addr ) ) );
         putGST_field( PPC_GST_CR, do_fp_tdiv(frA_I64, frB_I64), crfD );

         DIP("ftdiv crf%d,fr%u,fr%u\n", crfD, frA_addr, frB_addr);
         break;
      }
      case 0x0A0: 
      {
         IRTemp flags = newTemp(Ity_I32);
         IRTemp  fe_flag, fg_flag;
         fe_flag = fg_flag = IRTemp_INVALID;
         UChar b18to22  = toUChar( IFIELD( theInstr, 18, 5 ) );
         if ( b18to22 != 0) {
            vex_printf("dis_fp_tests(ppc)(ftsqrt)\n");
            return False;
         }
         DIP("ftsqrt crf%d,fr%u\n", crfD, frB_addr);
         do_fp_tsqrt(frB_I64, False , &fe_flag, &fg_flag);
         assign( flags,
                 binop( Iop_Or32,
                        binop( Iop_Or32, mkU32( 8 ), 
                               binop( Iop_Shl32, mkexpr(fg_flag), mkU8( 2 ) ) ),
                        binop( Iop_Shl32, mkexpr(fe_flag), mkU8( 1 ) ) ) );
         putGST_field( PPC_GST_CR, mkexpr(flags), crfD );
         break;
      }

      default:
         vex_printf("dis_fp_tests(ppc)(opc2)\n");
         return False;

   }
   return True;
}

static Bool dis_fp_cmp ( UInt theInstr )
{   
   
   UChar opc1     = ifieldOPC(theInstr);
   UChar crfD     = toUChar( IFIELD( theInstr, 23, 3 ) );
   UChar b21to22  = toUChar( IFIELD( theInstr, 21, 2 ) );
   UChar frA_addr = ifieldRegA(theInstr);
   UChar frB_addr = ifieldRegB(theInstr);
   UInt  opc2     = ifieldOPClo10(theInstr);
   UChar b0       = ifieldBIT0(theInstr);

   IRTemp ccIR    = newTemp(Ity_I32);
   IRTemp ccPPC32 = newTemp(Ity_I32);

   IRTemp frA     = newTemp(Ity_F64);
   IRTemp frB     = newTemp(Ity_F64);

   if (opc1 != 0x3F || b21to22 != 0 || b0 != 0) {
      vex_printf("dis_fp_cmp(ppc)(instr)\n");
      return False;
   }

   assign( frA, getFReg(frA_addr));
   assign( frB, getFReg(frB_addr));

   assign( ccIR, binop(Iop_CmpF64, mkexpr(frA), mkexpr(frB)) );
   
   

   
   
   assign(
      ccPPC32,
      binop(
         Iop_Shl32, 
         mkU32(1),
         unop(
            Iop_32to8, 
            binop(
               Iop_Or32,
               binop(
                  Iop_And32, 
                  unop(
                     Iop_Not32,
                     binop(Iop_Shr32, mkexpr(ccIR), mkU8(5))
                  ),
                  mkU32(2)
               ),
               binop(
                  Iop_And32, 
                  binop(
                     Iop_Xor32, 
                     mkexpr(ccIR),
                     binop(Iop_Shr32, mkexpr(ccIR), mkU8(6))
                  ),
                  mkU32(1)
               )
            )
         )
      )
   );

   putGST_field( PPC_GST_CR, mkexpr(ccPPC32), crfD );

   
   

   switch (opc2) {
   case 0x000: 
      DIP("fcmpu crf%d,fr%u,fr%u\n", crfD, frA_addr, frB_addr);
      break;
   case 0x020: 
      DIP("fcmpo crf%d,fr%u,fr%u\n", crfD, frA_addr, frB_addr);
      break;
   default:
      vex_printf("dis_fp_cmp(ppc)(opc2)\n");
      return False;
   }
   return True;
}



static Bool dis_fp_round ( UInt theInstr )
{
   
   UChar opc1     = ifieldOPC(theInstr);
   UChar b16to20  = ifieldRegA(theInstr);
   UChar frD_addr = ifieldRegDS(theInstr);
   UChar frB_addr = ifieldRegB(theInstr);
   UInt  opc2     = ifieldOPClo10(theInstr);
   UChar flag_rC  = ifieldBIT0(theInstr);

   IRTemp  frD     = newTemp(Ity_F64);
   IRTemp  frB     = newTemp(Ity_F64);
   IRTemp  r_tmp32 = newTemp(Ity_I32);
   IRTemp  r_tmp64 = newTemp(Ity_I64);
   IRExpr* rm      = get_IR_roundingmode();

   Bool set_FPRF = True;

   Bool clear_CR1 = True;
   if ((!(opc1 == 0x3F || opc1 == 0x3B)) || b16to20 != 0) {
      vex_printf("dis_fp_round(ppc)(instr)\n");
      return False;
   }

   assign( frB, getFReg(frB_addr));
   if (opc1 == 0x3B) {
      switch (opc2) {
         case 0x34E: 
            DIP("fcfids%s fr%u,fr%u\n", flag_rC ? ".":"", frD_addr, frB_addr);
            assign( r_tmp64, unop( Iop_ReinterpF64asI64, mkexpr(frB)) );
            assign( frD, binop( Iop_RoundF64toF32, rm, binop( Iop_I64StoF64, rm,
                                                              mkexpr( r_tmp64 ) ) ) );
            goto putFR;

         case 0x3Ce: 
            DIP("fcfidus%s fr%u,fr%u\n", flag_rC ? ".":"", frD_addr, frB_addr);
            assign( r_tmp64, unop( Iop_ReinterpF64asI64, mkexpr(frB)) );
            assign( frD, unop( Iop_F32toF64, binop( Iop_I64UtoF32, rm, mkexpr( r_tmp64 ) ) ) );
            goto putFR;
      }
   }


   switch (opc2) {
   case 0x00C: 
      DIP("frsp%s fr%u,fr%u\n", flag_rC ? ".":"", frD_addr, frB_addr);
      assign( frD, binop( Iop_RoundF64toF32, rm, mkexpr(frB) ));
      break;
      
   case 0x00E: 
      DIP("fctiw%s fr%u,fr%u\n", flag_rC ? ".":"", frD_addr, frB_addr);
      assign( r_tmp32,
              binop(Iop_F64toI32S, rm, mkexpr(frB)) );
      assign( frD, unop( Iop_ReinterpI64asF64,
                         unop( Iop_32Uto64, mkexpr(r_tmp32))));
      
      set_FPRF = False;
      break;
      
   case 0x00F: 
      DIP("fctiwz%s fr%u,fr%u\n", flag_rC ? ".":"", frD_addr, frB_addr);
      assign( r_tmp32, 
              binop(Iop_F64toI32S, mkU32(Irrm_ZERO), mkexpr(frB) ));
      assign( frD, unop( Iop_ReinterpI64asF64,
                         unop( Iop_32Uto64, mkexpr(r_tmp32))));
      
      set_FPRF = False;
      break;

   case 0x08F: case 0x08E: 
      DIP("fctiwu%s%s fr%u,fr%u\n", opc2 == 0x08F ? "z" : "",
               flag_rC ? ".":"", frD_addr, frB_addr);
      assign( r_tmp32,
              binop( Iop_F64toI32U,
                     opc2 == 0x08F ? mkU32( Irrm_ZERO ) : rm,
                     mkexpr( frB ) ) );
      assign( frD, unop( Iop_ReinterpI64asF64,
                         unop( Iop_32Uto64, mkexpr(r_tmp32))));
      
      set_FPRF = False;
      break;


   case 0x32E: 
      DIP("fctid%s fr%u,fr%u\n", flag_rC ? ".":"", frD_addr, frB_addr);
      assign( r_tmp64,
              binop(Iop_F64toI64S, rm, mkexpr(frB)) );
      assign( frD, unop( Iop_ReinterpI64asF64, mkexpr(r_tmp64)) );
      
      set_FPRF = False;
      break;

   case 0x32F: 
      DIP("fctidz%s fr%u,fr%u\n", flag_rC ? ".":"", frD_addr, frB_addr);
      assign( r_tmp64, 
              binop(Iop_F64toI64S, mkU32(Irrm_ZERO), mkexpr(frB)) );
      assign( frD, unop( Iop_ReinterpI64asF64, mkexpr(r_tmp64)) );
      
      set_FPRF = False;
      break;

   case 0x3AE: case 0x3AF: 
   {
      DIP("fctidu%s%s fr%u,fr%u\n", opc2 == 0x3AE ? "" : "z",
               flag_rC ? ".":"", frD_addr, frB_addr);
      assign( r_tmp64,
              binop(Iop_F64toI64U, opc2 == 0x3AE ? rm : mkU32(Irrm_ZERO), mkexpr(frB)) );
      assign( frD, unop( Iop_ReinterpI64asF64, mkexpr(r_tmp64)) );
      
      set_FPRF = False;
      break;
   }
   case 0x34E: 
      DIP("fcfid%s fr%u,fr%u\n", flag_rC ? ".":"", frD_addr, frB_addr);
      assign( r_tmp64, unop( Iop_ReinterpF64asI64, mkexpr(frB)) );
      assign( frD, 
              binop(Iop_I64StoF64, rm, mkexpr(r_tmp64)) );
      break;

   case 0x3CE: 
      DIP("fcfidu%s fr%u,fr%u\n", flag_rC ? ".":"", frD_addr, frB_addr);
      assign( r_tmp64, unop( Iop_ReinterpF64asI64, mkexpr(frB)) );
      assign( frD, binop( Iop_I64UtoF64, rm, mkexpr( r_tmp64 ) ) );
      break;

   case 0x188: case 0x1A8: case 0x1C8: case 0x1E8: 
      switch(opc2) {
      case 0x188: 
         DIP("frin%s fr%u,fr%u\n", flag_rC ? ".":"", frD_addr, frB_addr);
         assign( r_tmp64,
                 binop(Iop_F64toI64S, mkU32(Irrm_NEAREST), mkexpr(frB)) );
         break;
      case 0x1A8: 
         DIP("friz%s fr%u,fr%u\n", flag_rC ? ".":"", frD_addr, frB_addr);
         assign( r_tmp64,
                 binop(Iop_F64toI64S, mkU32(Irrm_ZERO), mkexpr(frB)) );
         break;
      case 0x1C8: 
         DIP("frip%s fr%u,fr%u\n", flag_rC ? ".":"", frD_addr, frB_addr);
         assign( r_tmp64,
                 binop(Iop_F64toI64S, mkU32(Irrm_PosINF), mkexpr(frB)) );
         break;
      case 0x1E8: 
         DIP("frim%s fr%u,fr%u\n", flag_rC ? ".":"", frD_addr, frB_addr);
         assign( r_tmp64,
                 binop(Iop_F64toI64S, mkU32(Irrm_NegINF), mkexpr(frB)) );
         break;
      }

      
      
      
      assign(frD, IRExpr_ITE(
                     binop(Iop_CmpNE8,
                           unop(Iop_32to8,
                                binop(Iop_CmpF64,
                                      IRExpr_Const(IRConst_F64(9e18)),
                                      unop(Iop_AbsF64, mkexpr(frB)))),
                           mkU8(0)),
                     mkexpr(frB),
                     IRExpr_ITE(
                        binop(Iop_CmpNE32,
                              binop(Iop_Shr32,
                                    unop(Iop_64HIto32,
                                         unop(Iop_ReinterpF64asI64,
                                              mkexpr(frB))),
                                    mkU8(31)),
                              mkU32(0)),
                        unop(Iop_NegF64,
                             unop( Iop_AbsF64,
                                   binop(Iop_I64StoF64, mkU32(0),
                                         mkexpr(r_tmp64)) )),
                        binop(Iop_I64StoF64, mkU32(0), mkexpr(r_tmp64) )
                     )
      ));
      break;

   default:
      vex_printf("dis_fp_round(ppc)(opc2)\n");
      return False;
   }
putFR:
   putFReg( frD_addr, mkexpr(frD) );

   if (set_FPRF) {
      
      
   }

   if (flag_rC && clear_CR1) {
      putCR321( 1, mkU8(0) );
      putCR0( 1, mkU8(0) );
   }

   return True;
}

static Bool dis_fp_pair ( UInt theInstr )
{
   
   UChar  opc1         = ifieldOPC(theInstr);
   UChar  frT_hi_addr  = ifieldRegDS(theInstr);
   UChar  frT_lo_addr  = frT_hi_addr + 1;
   UChar  rA_addr      = ifieldRegA(theInstr);
   UChar  rB_addr      = ifieldRegB(theInstr);
   UInt  uimm16        = ifieldUIMM16(theInstr);
   Int    simm16       = extend_s_16to32(uimm16);
   UInt   opc2         = ifieldOPClo10(theInstr);
   IRType ty           = mode64 ? Ity_I64 : Ity_I32;
   IRTemp EA_hi        = newTemp(ty);
   IRTemp EA_lo        = newTemp(ty);
   IRTemp frT_hi       = newTemp(Ity_F64);
   IRTemp frT_lo       = newTemp(Ity_F64);
   UChar b0            = ifieldBIT0(theInstr);
   Bool is_load        = 0;

   if ((frT_hi_addr %2) != 0) {
      vex_printf("dis_fp_pair(ppc) : odd frT register\n");
      return False;
   }

   switch (opc1) {
   case 0x1F: 
      switch(opc2) {
      case 0x317:     
         DIP("ldpx fr%u,r%u,r%u\n", frT_hi_addr, rA_addr, rB_addr);
         is_load = 1;
         break;
      case 0x397:     
         DIP("stdpx fr%u,r%u,r%u\n", frT_hi_addr, rA_addr, rB_addr);
         break;
      default:
         vex_printf("dis_fp_pair(ppc) : X-form wrong opc2\n");
         return False;
      }

      if (b0 != 0) {
         vex_printf("dis_fp_pair(ppc)(0x1F,b0)\n");
         return False;
      }
      assign( EA_hi, ea_rAor0_idxd( rA_addr, rB_addr ) );
      break;
   case 0x39: 
      DIP("lfdp fr%u,%d(r%u)\n", frT_hi_addr, simm16, rA_addr);
      assign( EA_hi, ea_rAor0_simm( rA_addr, simm16  ) );
      is_load = 1;
      break;
   case 0x3d: 
      DIP("stfdp fr%u,%d(r%u)\n", frT_hi_addr, simm16, rA_addr);
      assign( EA_hi, ea_rAor0_simm( rA_addr, simm16  ) );
      break;
   default:   
      vex_printf("dis_fp_pair(ppc)(instr)\n");
      return False;
   }

   if (mode64)
      assign( EA_lo, binop(Iop_Add64, mkexpr(EA_hi), mkU64(8)) );
   else
      assign( EA_lo, binop(Iop_Add32, mkexpr(EA_hi), mkU32(8)) );

   assign( frT_hi, getFReg(frT_hi_addr) );
   assign( frT_lo, getFReg(frT_lo_addr) );

   if (is_load) {
      putFReg( frT_hi_addr, load(Ity_F64, mkexpr(EA_hi)) );
      putFReg( frT_lo_addr, load(Ity_F64, mkexpr(EA_lo)) );
   } else {
      store( mkexpr(EA_hi), mkexpr(frT_hi) );
      store( mkexpr(EA_lo), mkexpr(frT_lo) );
   }

   return True;
}


static Bool dis_fp_merge ( UInt theInstr )
{
   
   UInt  opc2     = ifieldOPClo10(theInstr);
   UChar frD_addr = ifieldRegDS(theInstr);
   UChar frA_addr = ifieldRegA(theInstr);
   UChar frB_addr = ifieldRegB(theInstr);

   IRTemp frD = newTemp(Ity_F64);
   IRTemp frA = newTemp(Ity_F64);
   IRTemp frB = newTemp(Ity_F64);

   assign( frA, getFReg(frA_addr));
   assign( frB, getFReg(frB_addr));

   switch (opc2) {
   case 0x3c6: 
      DIP("fmrgew fr%u,fr%u,fr%u\n", frD_addr, frA_addr, frB_addr);

      assign( frD, unop( Iop_ReinterpI64asF64,
                         binop( Iop_32HLto64,
                                unop( Iop_64HIto32,
                                      unop( Iop_ReinterpF64asI64,
                                            mkexpr(frA) ) ),
                                unop( Iop_64HIto32,
                                      unop( Iop_ReinterpF64asI64,
                                            mkexpr(frB) ) ) ) ) );
   break;

   case 0x346: 
      DIP("fmrgow fr%u,fr%u,fr%u\n", frD_addr, frA_addr, frB_addr);

      assign( frD, unop( Iop_ReinterpI64asF64,
                         binop( Iop_32HLto64,
                                unop( Iop_64to32,
                                      unop( Iop_ReinterpF64asI64,
                                            mkexpr(frA) ) ),
                                unop( Iop_64to32,
                                      unop( Iop_ReinterpF64asI64,
                                            mkexpr(frB) ) ) ) ) );
   break;

   default:
      vex_printf("dis_fp_merge(ppc)(opc2)\n");
      return False;
   }

   putFReg( frD_addr, mkexpr(frD) );
   return True;
}

static Bool dis_fp_move ( UInt theInstr )
{
   
   UChar opc1     = ifieldOPC(theInstr);
   UChar frD_addr = ifieldRegDS(theInstr);
   UChar frA_addr = ifieldRegA(theInstr);
   UChar frB_addr = ifieldRegB(theInstr);
   UInt  opc2     = ifieldOPClo10(theInstr);
   UChar flag_rC  = ifieldBIT0(theInstr);

   IRTemp frD = newTemp(Ity_F64);
   IRTemp frB = newTemp(Ity_F64);
   IRTemp itmpB = newTemp(Ity_F64);
   IRTemp frA;
   IRTemp signA;
   IRTemp hiD;

   if (opc1 != 0x3F || (frA_addr != 0 && opc2 != 0x008)) {
      vex_printf("dis_fp_move(ppc)(instr)\n");
      return False;
   }

   assign( frB, getFReg(frB_addr));

   switch (opc2) {
   case 0x008: 
      DIP("fcpsgn%s fr%u,fr%u,fr%u\n", flag_rC ? ".":"", frD_addr, frA_addr,
          frB_addr);
      signA = newTemp(Ity_I32);
      hiD = newTemp(Ity_I32);
      itmpB = newTemp(Ity_I64);
      frA = newTemp(Ity_F64);
      assign( frA, getFReg(frA_addr) );

      
      assign(signA, binop(Iop_And32,
                          unop(Iop_64HIto32, unop(Iop_ReinterpF64asI64,
                                                  mkexpr(frA))),
                          mkU32(0x80000000)) );

      assign( itmpB, unop(Iop_ReinterpF64asI64, mkexpr(frB)) );

      
      assign(hiD, binop(Iop_Or32,
                        binop(Iop_And32,
                              unop(Iop_64HIto32,
                                   mkexpr(itmpB)),  
                              mkU32(0x7fffffff)),
                        mkexpr(signA)) );

      
      assign( frD, unop(Iop_ReinterpI64asF64,
                        binop(Iop_32HLto64,
                              mkexpr(hiD),
                              unop(Iop_64to32,
                                   mkexpr(itmpB)))) );   
      break;

   case 0x028: 
      DIP("fneg%s fr%u,fr%u\n", flag_rC ? ".":"", frD_addr, frB_addr);
      assign( frD, unop( Iop_NegF64, mkexpr(frB) ));
      break;
      
   case 0x048: 
      DIP("fmr%s fr%u,fr%u\n", flag_rC ? ".":"", frD_addr, frB_addr);
      assign( frD, mkexpr(frB) );
      break;
      
   case 0x088: 
      DIP("fnabs%s fr%u,fr%u\n", flag_rC ? ".":"", frD_addr, frB_addr);
      assign( frD, unop( Iop_NegF64, unop( Iop_AbsF64, mkexpr(frB) )));
      break;
      
   case 0x108: 
      DIP("fabs%s fr%u,fr%u\n", flag_rC ? ".":"", frD_addr, frB_addr);
      assign( frD, unop( Iop_AbsF64, mkexpr(frB) ));
      break;
      
   default:
      vex_printf("dis_fp_move(ppc)(opc2)\n");
      return False;
   }

   putFReg( frD_addr, mkexpr(frD) );


   if (flag_rC) {
      putCR321( 1, mkU8(0) );
      putCR0( 1, mkU8(0) );
   }

   return True;
}



static Bool dis_fp_scr ( UInt theInstr, Bool GX_level )
{
   
   UChar opc1    = ifieldOPC(theInstr);
   UInt  opc2    = ifieldOPClo10(theInstr);
   UChar flag_rC = ifieldBIT0(theInstr);

   if (opc1 != 0x3F) {
      vex_printf("dis_fp_scr(ppc)(instr)\n");
      return False;
   }

   switch (opc2) {
   case 0x026: { 
      
      UChar crbD    = ifieldRegDS(theInstr);
      UInt  b11to20 = IFIELD(theInstr, 11, 10);

      if (b11to20 != 0) {
         vex_printf("dis_fp_scr(ppc)(instr,mtfsb1)\n");
         return False;
      }
      DIP("mtfsb1%s crb%d \n", flag_rC ? ".":"", crbD);
      putGST_masked( PPC_GST_FPSCR, mkU64( 1 <<( 31 - crbD ) ),
		     1ULL << ( 31 - crbD ) );
      break;
   }

   case 0x040: { 
      UChar   crfD    = toUChar( IFIELD( theInstr, 23, 3 ) );
      UChar   b21to22 = toUChar( IFIELD( theInstr, 21, 2 ) );
      UChar   crfS    = toUChar( IFIELD( theInstr, 18, 3 ) );
      UChar   b11to17 = toUChar( IFIELD( theInstr, 11, 7 ) );
      IRTemp  tmp     = newTemp(Ity_I32);
      IRExpr* fpscr_all;
      if (b21to22 != 0 || b11to17 != 0 || flag_rC != 0) {
         vex_printf("dis_fp_scr(ppc)(instr,mcrfs)\n");
         return False;
      }
      DIP("mcrfs crf%d,crf%d\n", crfD, crfS);
      vassert(crfD < 8);
      vassert(crfS < 8);
      fpscr_all = getGST_masked( PPC_GST_FPSCR, MASK_FPSCR_RN );
      assign( tmp, binop(Iop_And32,
                         binop(Iop_Shr32,fpscr_all,mkU8(4 * (7-crfS))),
                        mkU32(0xF)) );
      putGST_field( PPC_GST_CR, mkexpr(tmp), crfD );
      break;
   }

   case 0x046: { 
      
      UChar crbD    = ifieldRegDS(theInstr);
      UInt  b11to20 = IFIELD(theInstr, 11, 10);

      if (b11to20 != 0) {
         vex_printf("dis_fp_scr(ppc)(instr,mtfsb0)\n");
         return False;
      }      
      DIP("mtfsb0%s crb%d\n", flag_rC ? ".":"", crbD);
      putGST_masked( PPC_GST_FPSCR, mkU64( 0 ), 1ULL << ( 31 - crbD ) );
      break;
   }

   case 0x086: { 
      UInt crfD     = IFIELD( theInstr, 23, 3 );
      UChar b16to22 = toUChar( IFIELD( theInstr, 16, 7 ) );
      UChar IMM     = toUChar( IFIELD( theInstr, 12, 4 ) );
      UChar b11     = toUChar( IFIELD( theInstr, 11, 1 ) );
      UChar Wbit;

      if (b16to22 != 0 || b11 != 0) {
         vex_printf("dis_fp_scr(ppc)(instr,mtfsfi)\n");
         return False;
      }      
      DIP("mtfsfi%s crf%d,%d\n", flag_rC ? ".":"", crfD, IMM);
      if (GX_level) {
         Wbit = toUChar( IFIELD(theInstr, 16, 1) );
      } else {
         Wbit = 0;
      }
      crfD = crfD + (8 * (1 - Wbit) );
      putGST_field( PPC_GST_FPSCR, mkU32( IMM ), crfD );
      break;
   }

   case 0x247: { 
      UChar   frD_addr  = ifieldRegDS(theInstr);
      UInt    b11to20   = IFIELD(theInstr, 11, 10);
      IRExpr* fpscr_lower = getGST_masked( PPC_GST_FPSCR, MASK_FPSCR_RN );
      IRExpr* fpscr_upper = getGST_masked_upper( PPC_GST_FPSCR,
                                                 MASK_FPSCR_DRN );

      if (b11to20 != 0) {
         vex_printf("dis_fp_scr(ppc)(instr,mffs)\n");
         return False;
      }
      DIP("mffs%s fr%u\n", flag_rC ? ".":"", frD_addr);
      putFReg( frD_addr,
          unop( Iop_ReinterpI64asF64,
                binop( Iop_32HLto64, fpscr_upper, fpscr_lower ) ) );
      break;
   }

   case 0x2C7: { 
      UChar b25      = toUChar( IFIELD(theInstr, 25, 1) );
      UChar FM       = toUChar( IFIELD(theInstr, 17, 8) );
      UChar frB_addr = ifieldRegB(theInstr);
      IRTemp frB   = newTemp(Ity_F64);
      IRTemp rB_64 = newTemp( Ity_I64 );
      Int i;
      ULong mask;
      UChar Wbit;
#define BFP_MASK_SEED 0x3000000000000000ULL
#define DFP_MASK_SEED 0x7000000000000000ULL

      if (GX_level) {
         Wbit = toUChar( IFIELD(theInstr, 16, 1) );
      } else {
         Wbit = 0;
      }

      if (b25 == 1) {
         DIP("mtfsf%s %d,fr%u (L=1)\n", flag_rC ? ".":"", FM, frB_addr);
         mask = 0xFF;

      } else {
         DIP("mtfsf%s %d,fr%u\n", flag_rC ? ".":"", FM, frB_addr);
         
         mask = 0;
         for (i=0; i<8; i++) {
            if ((FM & (1<<(7-i))) == 1) {
               if (Wbit)
                  mask |= DFP_MASK_SEED >> ( 4 * ( i + 8 * ( 1 - Wbit ) ) );
               else
                  mask |= BFP_MASK_SEED >> ( 4 * ( i + 8 * ( 1 - Wbit ) ) );
            }
         }
      }
      assign( frB, getFReg(frB_addr));
      assign( rB_64, unop( Iop_ReinterpF64asI64, mkexpr( frB ) ) );
      putGST_masked( PPC_GST_FPSCR, mkexpr( rB_64 ), mask );
      break;
   }

   default:
      vex_printf("dis_fp_scr(ppc)(opc2)\n");
      return False;
   }
   return True;
}

#define DFP_LONG  1
#define DFP_EXTND 2
#define DFP_LONG_BIAS   398
#define DFP_LONG_ENCODED_FIELD_MASK  0x1F00
#define DFP_EXTND_BIAS  6176
#define DFP_EXTND_ENCODED_FIELD_MASK 0x1F000
#define DFP_LONG_EXP_MSK   0XFF
#define DFP_EXTND_EXP_MSK  0XFFF

#define DFP_G_FIELD_LONG_MASK     0x7FFC0000  
#define DFP_LONG_GFIELD_RT_SHIFT  (63 - 13 - 32) 
#define DFP_G_FIELD_EXTND_MASK    0x7FFFC000  
#define DFP_EXTND_GFIELD_RT_SHIFT (63 - 17 - 32) 
#define DFP_T_FIELD_LONG_MASK     0x3FFFF  
#define DFP_T_FIELD_EXTND_MASK    0x03FFFF 
#define DFP_LONG_EXP_MAX          369      
#define DFP_LONG_EXP_MIN          0        
#define DFP_EXTND_EXP_MAX         6111     
#define DFP_EXTND_EXP_MIN         0        
#define DFP_LONG_MAX_SIG_DIGITS   16
#define DFP_EXTND_MAX_SIG_DIGITS  34
#define MAX_DIGITS_IN_STRING      8


#define  AND(x, y) binop( Iop_And32, x, y )
#define AND4(w, x, y, z) AND( AND( w, x ), AND( y, z ) )
#define   OR(x, y) binop( Iop_Or32,  x, y )
#define  OR3(x, y, z)    OR( x, OR( y, z ) )
#define  OR4(w, x, y, z) OR( OR( w, x ), OR( y, z ) )
#define  NOT(x) unop( Iop_1Uto32, unop( Iop_Not1, unop( Iop_32to1,  mkexpr( x ) ) ) )

#define  SHL(value, by) binop( Iop_Shl32, value, mkU8( by ) )
#define  SHR(value, by) binop( Iop_Shr32, value, mkU8( by ) )

#define BITS5(_b4,_b3,_b2,_b1,_b0) \
   (((_b4) << 4) | ((_b3) << 3) | ((_b2) << 2) | \
    ((_b1) << 1) | ((_b0) << 0))

static IRExpr * Gfield_encoding( IRExpr * lmexp, IRExpr * lmd32 )
{
   IRTemp lmd_07_mask   = newTemp( Ity_I32 );
   IRTemp lmd_8_mask    = newTemp( Ity_I32 );
   IRTemp lmd_9_mask    = newTemp( Ity_I32 );
   IRTemp lmexp_00_mask = newTemp( Ity_I32 );
   IRTemp lmexp_01_mask = newTemp( Ity_I32 );
   IRTemp lmexp_10_mask = newTemp( Ity_I32 );
   IRTemp lmd_07_val    = newTemp( Ity_I32 );
   IRTemp lmd_8_val     = newTemp( Ity_I32 );
   IRTemp lmd_9_val     = newTemp( Ity_I32 );


   
   assign( lmd_07_mask,
           unop( Iop_1Sto32, binop( Iop_CmpLE32U, lmd32, mkU32( 7 ) ) ) );
   assign( lmd_8_mask,
           unop( Iop_1Sto32, binop( Iop_CmpEQ32, lmd32, mkU32( 8 ) ) ) );
   assign( lmd_9_mask,
           unop( Iop_1Sto32, binop( Iop_CmpEQ32, lmd32, mkU32( 9 ) ) ) );
   assign( lmexp_00_mask,
           unop( Iop_1Sto32, binop( Iop_CmpEQ32, lmexp, mkU32( 0 ) ) ) );
   assign( lmexp_01_mask,
           unop( Iop_1Sto32, binop( Iop_CmpEQ32, lmexp, mkU32( 1 ) ) ) );
   assign( lmexp_10_mask,
           unop( Iop_1Sto32, binop( Iop_CmpEQ32, lmexp, mkU32( 2 ) ) ) );

   assign( lmd_07_val,
           binop( Iop_Or32, binop( Iop_Shl32, lmexp, mkU8( 3 ) ), lmd32 ) );
   assign( lmd_8_val,
           binop( Iop_Or32,
                  binop( Iop_Or32,
                         binop( Iop_And32,
                                mkexpr( lmexp_00_mask ),
                                mkU32( 24 ) ),
                         binop( Iop_And32,
                                mkexpr( lmexp_01_mask ),
                                mkU32( 26 ) ) ),
                  binop( Iop_And32, mkexpr( lmexp_10_mask ), mkU32( 28 ) ) ) );
   assign( lmd_9_val,
           binop( Iop_Or32,
                  binop( Iop_Or32,
                         binop( Iop_And32,
                                mkexpr( lmexp_00_mask ),
                                mkU32( 25 ) ),
                         binop( Iop_And32,
                                mkexpr( lmexp_01_mask ),
                                mkU32( 27 ) ) ),
                  binop( Iop_And32, mkexpr( lmexp_10_mask ), mkU32( 29 ) ) ) );

   
   return binop( Iop_Or32,
                 binop( Iop_Or32,
                        binop( Iop_And32,
                               mkexpr( lmd_07_mask ),
                               mkexpr( lmd_07_val ) ),
                        binop( Iop_And32,
                               mkexpr( lmd_8_mask ),
                               mkexpr( lmd_8_val ) ) ),
                 binop( Iop_And32, mkexpr( lmd_9_mask ), mkexpr( lmd_9_val ) ) );
}

static void Get_lmd( IRTemp * lmd, IRExpr * gfield_0_4 )
{
   IRTemp lmd_07_mask   = newTemp( Ity_I32 );
   IRTemp lmd_8_00_mask = newTemp( Ity_I32 );
   IRTemp lmd_8_01_mask = newTemp( Ity_I32 );
   IRTemp lmd_8_10_mask = newTemp( Ity_I32 );
   IRTemp lmd_9_00_mask = newTemp( Ity_I32 );
   IRTemp lmd_9_01_mask = newTemp( Ity_I32 );
   IRTemp lmd_9_10_mask = newTemp( Ity_I32 );

   IRTemp lmd_07_val = newTemp( Ity_I32 );
   IRTemp lmd_8_val  = newTemp( Ity_I32 );
   IRTemp lmd_9_val  = newTemp( Ity_I32 );


   
   assign( lmd_07_mask,
           unop( Iop_1Sto32, binop( Iop_CmpLE32U,
                                    gfield_0_4,
                                    mkU32( BITS5(1,0,1,1,1) ) ) ) );
   assign( lmd_8_00_mask,
           unop( Iop_1Sto32, binop( Iop_CmpEQ32,
                                    gfield_0_4,
                                    mkU32( BITS5(1,1,0,0,0) ) ) ) );
   assign( lmd_8_01_mask,
           unop( Iop_1Sto32, binop( Iop_CmpEQ32,
                                    gfield_0_4,
                                    mkU32( BITS5(1,1,0,1,0) ) ) ) );
   assign( lmd_8_10_mask,
           unop( Iop_1Sto32, binop( Iop_CmpEQ32,
                                    gfield_0_4,
                                    mkU32( BITS5(1,1,1,0,0) ) ) ) );
   assign( lmd_9_00_mask,
           unop( Iop_1Sto32, binop( Iop_CmpEQ32,
                                    gfield_0_4,
                                    mkU32( BITS5(1,1,0,0,1) ) ) ) );
   assign( lmd_9_01_mask,
           unop( Iop_1Sto32, binop( Iop_CmpEQ32,
                                    gfield_0_4,
                                    mkU32( BITS5(1,1,0,1,1) ) ) ) );
   assign( lmd_9_10_mask,
           unop( Iop_1Sto32, binop( Iop_CmpEQ32,
                                    gfield_0_4,
                                    mkU32( BITS5(1,1,1,0,1) ) ) ) );

   assign( lmd_07_val, binop( Iop_And32, gfield_0_4, mkU32( 0x7 ) ) );
   assign( lmd_8_val, mkU32( 0x8 ) );
   assign( lmd_9_val, mkU32( 0x9 ) );

   assign( *lmd,
           OR( OR3 ( AND( mkexpr( lmd_07_mask ), mkexpr( lmd_07_val ) ),
                     AND( mkexpr( lmd_8_00_mask ), mkexpr( lmd_8_val ) ),
                     AND( mkexpr( lmd_8_01_mask ), mkexpr( lmd_8_val ) )),
                     OR4( AND( mkexpr( lmd_8_10_mask ), mkexpr( lmd_8_val ) ),
                          AND( mkexpr( lmd_9_00_mask ), mkexpr( lmd_9_val ) ),
                          AND( mkexpr( lmd_9_01_mask ), mkexpr( lmd_9_val ) ),
                          AND( mkexpr( lmd_9_10_mask ), mkexpr( lmd_9_val ) )
                     ) ) );
}

#define DIGIT1_SHR 4    
#define DIGIT2_SHR 8    
#define DIGIT3_SHR 12
#define DIGIT4_SHR 16
#define DIGIT5_SHR 20
#define DIGIT6_SHR 24
#define DIGIT7_SHR 28

static IRExpr * bcd_digit_inval( IRExpr * bcd_u, IRExpr * bcd_l )
{
   IRTemp valid = newTemp( Ity_I32 );

   assign( valid,
           AND4( AND4 ( unop( Iop_1Sto32,
                              binop( Iop_CmpLE32U,
                                     binop( Iop_And32,
                                            bcd_l,
                                            mkU32 ( 0xF ) ),
                                      mkU32( 0x9 ) ) ),
                        unop( Iop_1Sto32,
                              binop( Iop_CmpLE32U,
                                     binop( Iop_And32,
                                            binop( Iop_Shr32,
                                                   bcd_l,
                                                   mkU8 ( DIGIT1_SHR ) ),
                                             mkU32 ( 0xF ) ),
                                      mkU32( 0x9 ) ) ),
                        unop( Iop_1Sto32,
                              binop( Iop_CmpLE32U,
                                     binop( Iop_And32,
                                            binop( Iop_Shr32,
                                                   bcd_l,
                                                   mkU8 ( DIGIT2_SHR ) ),
                                            mkU32 ( 0xF ) ),
                                      mkU32( 0x9 ) ) ),
                        unop( Iop_1Sto32,
                              binop( Iop_CmpLE32U,
                                     binop( Iop_And32,
                                            binop( Iop_Shr32,
                                                   bcd_l,
                                                   mkU8 ( DIGIT3_SHR ) ),
                                             mkU32 ( 0xF ) ),
                                      mkU32( 0x9 ) ) ) ),
                 AND4 ( unop( Iop_1Sto32,
                              binop( Iop_CmpLE32U,
                                     binop( Iop_And32,
                                            binop( Iop_Shr32,
                                                   bcd_l,
                                                   mkU8 ( DIGIT4_SHR ) ),
                                            mkU32 ( 0xF ) ),
                                     mkU32( 0x9 ) ) ),
                        unop( Iop_1Sto32,
                              binop( Iop_CmpLE32U,
                                     binop( Iop_And32,
                                            binop( Iop_Shr32,
                                                   bcd_l,
                                                   mkU8 ( DIGIT5_SHR ) ),
                                            mkU32 ( 0xF ) ),
                                     mkU32( 0x9 ) ) ),
                        unop( Iop_1Sto32,
                              binop( Iop_CmpLE32U,
                                     binop( Iop_And32,
                                            binop( Iop_Shr32,
                                                   bcd_l,
                                                   mkU8 ( DIGIT6_SHR ) ),
                                            mkU32 ( 0xF ) ),
                                     mkU32( 0x9 ) ) ),
                        unop( Iop_1Sto32,
                              binop( Iop_CmpLE32U,
                                     binop( Iop_And32,
                                            binop( Iop_Shr32,
                                                   bcd_l,
                                                   mkU8 ( DIGIT7_SHR ) ),
                                            mkU32 ( 0xF ) ),
                                     mkU32( 0x9 ) ) ) ),
                 AND4( unop( Iop_1Sto32,
                             binop( Iop_CmpLE32U,
                                    binop( Iop_And32,
                                           bcd_u,
                                           mkU32 ( 0xF ) ),
                                    mkU32( 0x9 ) ) ),
                       unop( Iop_1Sto32,
                             binop( Iop_CmpLE32U,
                                    binop( Iop_And32,
                                           binop( Iop_Shr32,
                                                  bcd_u,
                                                  mkU8 ( DIGIT1_SHR ) ),
                                           mkU32 ( 0xF ) ),
                                    mkU32( 0x9 ) ) ),
                       unop( Iop_1Sto32,
                             binop( Iop_CmpLE32U,
                                    binop( Iop_And32,
                                           binop( Iop_Shr32,
                                                  bcd_u,
                                                  mkU8 ( DIGIT2_SHR ) ),
                                           mkU32 ( 0xF ) ),
                                    mkU32( 0x9 ) ) ),
                       unop( Iop_1Sto32,
                             binop( Iop_CmpLE32U,
                                    binop( Iop_And32,
                                           binop( Iop_Shr32,
                                                  bcd_u,
                                                  mkU8 ( DIGIT3_SHR ) ),
                                           mkU32 ( 0xF ) ),
                                    mkU32( 0x9 ) ) ) ),
                 AND4( unop( Iop_1Sto32,
                             binop( Iop_CmpLE32U,
                                    binop( Iop_And32,
                                           binop( Iop_Shr32,
                                                  bcd_u,
                                                  mkU8 ( DIGIT4_SHR ) ),
                                           mkU32 ( 0xF ) ),
                                    mkU32( 0x9 ) ) ),
                       unop( Iop_1Sto32,
                             binop( Iop_CmpLE32U,
                                    binop( Iop_And32,
                                           binop( Iop_Shr32,
                                                  bcd_u,
                                                  mkU8 ( DIGIT5_SHR ) ),
                                           mkU32 ( 0xF ) ),
                                    mkU32( 0x9 ) ) ),
                       unop( Iop_1Sto32,
                             binop( Iop_CmpLE32U,
                                    binop( Iop_And32,
                                           binop( Iop_Shr32,
                                                  bcd_u,
                                                  mkU8 ( DIGIT6_SHR ) ),
                                           mkU32 ( 0xF ) ),
                                    mkU32( 0x9 ) ) ),
                       unop( Iop_1Sto32,
                             binop( Iop_CmpLE32U,
                                    binop( Iop_And32,
                                           binop( Iop_Shr32,
                                                  bcd_u,
                                                  mkU8 ( DIGIT7_SHR ) ),
                                           mkU32 ( 0xF ) ),
                                    mkU32( 0x9 ) ) ) ) ) );
           
   return unop( Iop_Not32, mkexpr( valid ) );
}
#undef DIGIT1_SHR
#undef DIGIT2_SHR
#undef DIGIT3_SHR
#undef DIGIT4_SHR
#undef DIGIT5_SHR
#undef DIGIT6_SHR
#undef DIGIT7_SHR

static IRExpr * Generate_neg_sign_mask( IRExpr * sign )
{
   return binop( Iop_Or32,
                 unop( Iop_1Sto32, binop( Iop_CmpEQ32, sign, mkU32( 0xB ) ) ),
                 unop( Iop_1Sto32, binop( Iop_CmpEQ32, sign, mkU32( 0xD ) ) )
               );
}

static IRExpr * Generate_pos_sign_mask( IRExpr * sign )
{
   return binop( Iop_Or32,
                 binop( Iop_Or32,
                        unop( Iop_1Sto32,
                              binop( Iop_CmpEQ32, sign, mkU32( 0xA ) ) ),
                        unop( Iop_1Sto32,
                              binop( Iop_CmpEQ32, sign, mkU32( 0xC ) ) ) ),
                 binop( Iop_Or32,
                        unop( Iop_1Sto32,
                              binop( Iop_CmpEQ32, sign, mkU32( 0xE ) ) ),
                        unop( Iop_1Sto32,
                              binop( Iop_CmpEQ32, sign, mkU32( 0xF ) ) ) ) );
}

static IRExpr * Generate_sign_bit( IRExpr * pos_sign_mask,
                                   IRExpr * neg_sign_mask )
{
   return binop( Iop_Or32,
                 binop( Iop_And32, neg_sign_mask, mkU32( 0x80000000 ) ),
                 binop( Iop_And32, pos_sign_mask, mkU32( 0x00000000 ) ) );
}

static IRExpr * Generate_inv_mask( IRExpr * invalid_bcd_mask,
                                   IRExpr * pos_sign_mask,
                                   IRExpr * neg_sign_mask )
{
   return binop( Iop_Or32,
                 invalid_bcd_mask,
                 unop( Iop_1Sto32,
                       binop( Iop_CmpEQ32,
                              binop( Iop_Or32, pos_sign_mask, neg_sign_mask ),
                              mkU32( 0x0 ) ) ) );
}

static void Generate_132_bit_bcd_string( IRExpr * frBI64_hi, IRExpr * frBI64_lo,
                                         IRTemp * top_12_l, IRTemp * mid_60_u,
                                         IRTemp * mid_60_l, IRTemp * low_60_u,
                                         IRTemp * low_60_l)
{
   IRTemp tmplow60 = newTemp( Ity_I64 );
   IRTemp tmpmid60 = newTemp( Ity_I64 );
   IRTemp tmptop12 = newTemp( Ity_I64 );
   IRTemp low_50   = newTemp( Ity_I64 );
   IRTemp mid_50   = newTemp( Ity_I64 );
   IRTemp top_10   = newTemp( Ity_I64 );
   IRTemp top_12_u = newTemp( Ity_I32 ); 

   

   
   assign( low_50,
           binop( Iop_32HLto64,
                  binop( Iop_And32,
                         unop( Iop_64HIto32, frBI64_lo ),
                         mkU32( 0x3FFFF ) ),
                         unop( Iop_64to32, frBI64_lo ) ) );

   assign( tmplow60, unop( Iop_DPBtoBCD, mkexpr( low_50 ) ) );
   assign( *low_60_u, unop( Iop_64HIto32, mkexpr( tmplow60 ) ) );
   assign( *low_60_l, unop( Iop_64to32, mkexpr( tmplow60 ) ) );

   assign( mid_50,
           binop( Iop_32HLto64,
                  binop( Iop_Or32,
                         binop( Iop_Shl32,
                                binop( Iop_And32,
                                       unop( Iop_64HIto32, frBI64_hi ),
                                       mkU32( 0xF ) ),
                                mkU8( 14 ) ),
                         binop( Iop_Shr32,
                                unop( Iop_64to32, frBI64_hi ),
                                mkU8( 18 ) ) ),
                  binop( Iop_Or32,
                         binop( Iop_Shl32,
                                unop( Iop_64to32, frBI64_hi ),
                                mkU8( 14 ) ),
                         binop( Iop_Shr32,
                                unop( Iop_64HIto32, frBI64_lo ),
                                mkU8( 18 ) ) ) ) );

   assign( tmpmid60, unop( Iop_DPBtoBCD, mkexpr( mid_50 ) ) );
   assign( *mid_60_u, unop( Iop_64HIto32, mkexpr( tmpmid60 ) ) );
   assign( *mid_60_l, unop( Iop_64to32, mkexpr( tmpmid60 ) ) );

   
   assign( top_10,
           binop( Iop_32HLto64,
                  mkU32( 0 ),
                  binop( Iop_And32,
                         binop( Iop_Shr32,
                                unop( Iop_64HIto32, frBI64_hi ),
                                mkU8( 4 ) ),
                         mkU32( 0x3FF ) ) ) );

   assign( tmptop12, unop( Iop_DPBtoBCD, mkexpr( top_10 ) ) );
   assign( top_12_u, unop( Iop_64HIto32, mkexpr( tmptop12 ) ) );
   assign( *top_12_l, unop( Iop_64to32, mkexpr( tmptop12 ) ) );
}

static void Count_zeros( int start, IRExpr * init_cnt, IRExpr * init_flag,
                         IRTemp * final_cnt, IRTemp * final_flag,
                         IRExpr * string )
{
   IRTemp cnt[MAX_DIGITS_IN_STRING + 1];IRTemp flag[MAX_DIGITS_IN_STRING+1];
   int digits = MAX_DIGITS_IN_STRING;
   int i;

   cnt[start-1] = newTemp( Ity_I8 );
   flag[start-1] = newTemp( Ity_I8 );
   assign( cnt[start-1], init_cnt);
   assign( flag[start-1], init_flag);

   for ( i = start; i <= digits; i++) {
      cnt[i] = newTemp( Ity_I8 );
      flag[i] = newTemp( Ity_I8 );
      assign( cnt[i],
              binop( Iop_Add8,
                     mkexpr( cnt[i-1] ),
                     binop(Iop_And8,
                           unop( Iop_1Uto8,
                                 binop(Iop_CmpEQ32,
                                       binop(Iop_And32,
                                             string,
                                             mkU32( 0xF <<
                                                    ( ( digits - i ) * 4) ) ),
                                       mkU32( 0 ) ) ),
                           binop( Iop_Xor8, 
                                  mkexpr( flag[i - 1] ),
                                  mkU8( 0xFF ) ) ) ) );

      
      assign( flag[i],
              binop(Iop_Or8,
                    unop( Iop_1Sto8,
                          binop(Iop_CmpNE32,
                                binop(Iop_And32,
                                      string,
                                      mkU32( 0xF <<
                                             ( (digits - i) * 4) ) ),
                                mkU32( 0 ) ) ),
                    mkexpr( flag[i - 1] ) ) );
   }

   *final_cnt = cnt[digits];
   *final_flag = flag[digits];
}

static IRExpr * Count_leading_zeros_60( IRExpr * lmd, IRExpr * upper_28,
                                        IRExpr * low_32 )
{
   IRTemp num_lmd    = newTemp( Ity_I8 );
   IRTemp num_upper  = newTemp( Ity_I8 );
   IRTemp num_low    = newTemp( Ity_I8 );
   IRTemp lmd_flag   = newTemp( Ity_I8 );
   IRTemp upper_flag = newTemp( Ity_I8 );
   IRTemp low_flag   = newTemp( Ity_I8 );

   assign( num_lmd, unop( Iop_1Uto8, binop( Iop_CmpEQ32, lmd, mkU32( 0 ) ) ) );
   assign( lmd_flag, unop( Iop_Not8, mkexpr( num_lmd ) ) );

   Count_zeros( 2,
                mkexpr( num_lmd ),
                mkexpr( lmd_flag ),
                &num_upper,
                &upper_flag,
                upper_28 );

   Count_zeros( 1,
                mkexpr( num_upper ),
                mkexpr( upper_flag ),
                &num_low,
                &low_flag,
                low_32 );

   return mkexpr( num_low );
}

static IRExpr * Count_leading_zeros_128( IRExpr * lmd, IRExpr * top_12_l,
                                         IRExpr * mid_60_u, IRExpr * mid_60_l,
                                         IRExpr * low_60_u, IRExpr * low_60_l)
{
   IRTemp num_lmd   = newTemp( Ity_I8 );
   IRTemp num_top   = newTemp( Ity_I8 );
   IRTemp num_mid_u = newTemp( Ity_I8 );
   IRTemp num_mid_l = newTemp( Ity_I8 );
   IRTemp num_low_u = newTemp( Ity_I8 );
   IRTemp num_low_l = newTemp( Ity_I8 );

   IRTemp lmd_flag   = newTemp( Ity_I8 );
   IRTemp top_flag   = newTemp( Ity_I8 );
   IRTemp mid_u_flag = newTemp( Ity_I8 );
   IRTemp mid_l_flag = newTemp( Ity_I8 );
   IRTemp low_u_flag = newTemp( Ity_I8 );
   IRTemp low_l_flag = newTemp( Ity_I8 );

   
   assign( num_lmd, unop( Iop_1Uto8, binop( Iop_CmpEQ32, lmd, mkU32( 0 ) ) ) );

   assign( lmd_flag, unop( Iop_Not8, mkexpr( num_lmd ) ) );

   Count_zeros( 6,
                mkexpr( num_lmd ),
                mkexpr( lmd_flag ),
                &num_top,
                &top_flag,
                top_12_l );

   Count_zeros( 1,
                mkexpr( num_top ),
                mkexpr( top_flag ),
                &num_mid_u,
                &mid_u_flag,
                binop( Iop_Or32,
                       binop( Iop_Shl32, mid_60_u, mkU8( 2 ) ),
                       binop( Iop_Shr32, mid_60_l, mkU8( 30 ) ) ) );

   Count_zeros( 2,
                mkexpr( num_mid_u ),
                mkexpr( mid_u_flag ),
                &num_mid_l,
                &mid_l_flag,
                mid_60_l );

   Count_zeros( 1,
                mkexpr( num_mid_l ),
                mkexpr( mid_l_flag ),
                &num_low_u,
                &low_u_flag,
                binop( Iop_Or32,
                       binop( Iop_Shl32, low_60_u, mkU8( 2 ) ),
                       binop( Iop_Shr32, low_60_l, mkU8( 30 ) ) ) );

   Count_zeros( 2,
                mkexpr( num_low_u ),
                mkexpr( low_u_flag ),
                &num_low_l,
                &low_l_flag,
                low_60_l );

   return mkexpr( num_low_l );
}

static IRExpr * Check_unordered(IRExpr * val)
{
   IRTemp gfield0to5 = newTemp( Ity_I32 );

   
   assign( gfield0to5,
           binop( Iop_And32,
                  binop( Iop_Shr32, unop( Iop_64HIto32, val ), mkU8( 26 ) ),
                  mkU32( 0x1F ) ) );

   
   return binop( Iop_Or32, 
                 unop( Iop_1Sto32,
                       binop( Iop_CmpEQ32,
                              mkexpr( gfield0to5 ),
                              mkU32( 0x1E ) ) ),
                              unop( Iop_1Sto32, 
                                    binop( Iop_CmpEQ32,
                                           mkexpr( gfield0to5 ),
                                           mkU32( 0x1F ) ) ) );
}

#undef AND
#undef AND4
#undef OR
#undef OR3
#undef OR4
#undef NOT
#undef SHR
#undef SHL
#undef BITS5


static Bool dis_dfp_arith(UInt theInstr)
{
   UInt opc2 = ifieldOPClo10( theInstr );
   UChar frS_addr = ifieldRegDS( theInstr );
   UChar frA_addr = ifieldRegA( theInstr );
   UChar frB_addr = ifieldRegB( theInstr );
   UChar flag_rC = ifieldBIT0( theInstr );

   IRTemp frA = newTemp( Ity_D64 );
   IRTemp frB = newTemp( Ity_D64 );
   IRTemp frS = newTemp( Ity_D64 );
   IRExpr* round = get_IR_roundingmode_DFP();

   Bool clear_CR1 = True;

   assign( frA, getDReg( frA_addr ) );
   assign( frB, getDReg( frB_addr ) );

   switch (opc2) {
   case 0x2: 
      DIP( "dadd%s fr%u,fr%u,fr%u\n",
           flag_rC ? ".":"", frS_addr, frA_addr, frB_addr );
      assign( frS, triop( Iop_AddD64, round, mkexpr( frA ), mkexpr( frB ) ) );
      break;
   case 0x202: 
      DIP( "dsub%s fr%u,fr%u,fr%u\n",
           flag_rC ? ".":"", frS_addr, frA_addr, frB_addr );
      assign( frS, triop( Iop_SubD64, round, mkexpr( frA ), mkexpr( frB ) ) );
      break;
   case 0x22: 
      DIP( "dmul%s fr%u,fr%u,fr%u\n",
           flag_rC ? ".":"", frS_addr, frA_addr, frB_addr );
      assign( frS, triop( Iop_MulD64, round, mkexpr( frA ), mkexpr( frB ) ) );
      break;
   case 0x222: 
      DIP( "ddiv%s fr%u,fr%u,fr%u\n",
           flag_rC ? ".":"", frS_addr, frA_addr, frB_addr );
      assign( frS, triop( Iop_DivD64, round, mkexpr( frA ), mkexpr( frB ) ) );
      break;
   }

   putDReg( frS_addr, mkexpr( frS ) );

   if (flag_rC && clear_CR1) {
      putCR321( 1, mkU8( 0 ) );
      putCR0( 1, mkU8( 0 ) );
   }

   return True;
}

static Bool dis_dfp_arithq(UInt theInstr)
{
   UInt opc2 = ifieldOPClo10( theInstr );
   UChar frS_addr = ifieldRegDS( theInstr );
   UChar frA_addr = ifieldRegA( theInstr );
   UChar frB_addr = ifieldRegB( theInstr );
   UChar flag_rC = ifieldBIT0( theInstr );

   IRTemp frA = newTemp( Ity_D128 );
   IRTemp frB = newTemp( Ity_D128 );
   IRTemp frS = newTemp( Ity_D128 );
   IRExpr* round = get_IR_roundingmode_DFP();

   Bool clear_CR1 = True;

   assign( frA, getDReg_pair( frA_addr ) );
   assign( frB, getDReg_pair( frB_addr ) );

   switch (opc2) {
   case 0x2: 
      DIP( "daddq%s fr%u,fr%u,fr%u\n",
           flag_rC ? ".":"", frS_addr, frA_addr, frB_addr );
      assign( frS, triop( Iop_AddD128, round, mkexpr( frA ), mkexpr( frB ) ) );
      break;
   case 0x202: 
      DIP( "dsubq%s fr%u,fr%u,fr%u\n",
           flag_rC ? ".":"", frS_addr, frA_addr, frB_addr );
      assign( frS, triop( Iop_SubD128, round, mkexpr( frA ), mkexpr( frB ) ) );
      break;
   case 0x22: 
      DIP( "dmulq%s fr%u,fr%u,fr%u\n",
           flag_rC ? ".":"", frS_addr, frA_addr, frB_addr );
      assign( frS, triop( Iop_MulD128, round, mkexpr( frA ), mkexpr( frB ) ) );
      break;
   case 0x222: 
      DIP( "ddivq%s fr%u,fr%u,fr%u\n",
           flag_rC ? ".":"", frS_addr, frA_addr, frB_addr );
      assign( frS, triop( Iop_DivD128, round, mkexpr( frA ), mkexpr( frB ) ) );
      break;
   }

   putDReg_pair( frS_addr, mkexpr( frS ) );

   if (flag_rC && clear_CR1) {
      putCR321( 1, mkU8( 0 ) );
      putCR0( 1, mkU8( 0 ) );
   }

   return True;
}

static Bool dis_dfp_shift(UInt theInstr) {
   UInt opc2       = ifieldOPClo9( theInstr );
   UChar frS_addr  = ifieldRegDS( theInstr );
   UChar frA_addr  = ifieldRegA( theInstr );
   UChar shift_val = IFIELD(theInstr, 10, 6);
   UChar flag_rC   = ifieldBIT0( theInstr );

   IRTemp frA = newTemp( Ity_D64 );
   IRTemp frS = newTemp( Ity_D64 );
   Bool clear_CR1 = True;

   assign( frA, getDReg( frA_addr ) );

   switch (opc2) {
   case 0x42: 
      DIP( "dscli%s fr%u,fr%u,%u\n",
           flag_rC ? ".":"", frS_addr, frA_addr, shift_val );
      assign( frS, binop( Iop_ShlD64, mkexpr( frA ), mkU8( shift_val ) ) );
      break;
   case 0x62: 
      DIP( "dscri%s fr%u,fr%u,%u\n",
           flag_rC ? ".":"", frS_addr, frA_addr, shift_val );
      assign( frS, binop( Iop_ShrD64, mkexpr( frA ), mkU8( shift_val ) ) );
      break;
   }

   putDReg( frS_addr, mkexpr( frS ) );

   if (flag_rC && clear_CR1) {
      putCR321( 1, mkU8( 0 ) );
      putCR0( 1, mkU8( 0 ) );
   }

   return True;
}

static Bool dis_dfp_shiftq(UInt theInstr) {
   UInt opc2       = ifieldOPClo9( theInstr );
   UChar frS_addr  = ifieldRegDS( theInstr );
   UChar frA_addr  = ifieldRegA( theInstr );
   UChar shift_val = IFIELD(theInstr, 10, 6);
   UChar flag_rC   = ifieldBIT0( theInstr );

   IRTemp frA = newTemp( Ity_D128 );
   IRTemp frS = newTemp( Ity_D128 );
   Bool clear_CR1 = True;

   assign( frA, getDReg_pair( frA_addr ) );

   switch (opc2) {
   case 0x42: 
      DIP( "dscliq%s fr%u,fr%u,%u\n",
           flag_rC ? ".":"", frS_addr, frA_addr, shift_val );
      assign( frS, binop( Iop_ShlD128, mkexpr( frA ), mkU8( shift_val ) ) );
      break;
   case 0x62: 
      DIP( "dscriq%s fr%u,fr%u,%u\n",
           flag_rC ? ".":"", frS_addr, frA_addr, shift_val );
      assign( frS, binop( Iop_ShrD128, mkexpr( frA ), mkU8( shift_val ) ) );
      break;
   }

   putDReg_pair( frS_addr, mkexpr( frS ) );

   if (flag_rC && clear_CR1) {
      putCR321( 1, mkU8( 0 ) );
      putCR0( 1, mkU8( 0 ) );
   }

   return True;
}

static Bool dis_dfp_fmt_conv(UInt theInstr) {
   UInt opc2      = ifieldOPClo10( theInstr );
   UChar frS_addr = ifieldRegDS( theInstr );
   UChar frB_addr = ifieldRegB( theInstr );
   IRExpr* round  = get_IR_roundingmode_DFP();
   UChar flag_rC  = ifieldBIT0( theInstr );
   IRTemp frB;
   IRTemp frS;
   Bool clear_CR1 = True;

   switch (opc2) {
   case 0x102: 
      DIP( "dctdp%s fr%u,fr%u\n",
           flag_rC ? ".":"", frS_addr, frB_addr );

      frB = newTemp( Ity_D32 );
      frS = newTemp( Ity_D64 );
      assign( frB, getDReg32( frB_addr ) );
      assign( frS, unop( Iop_D32toD64, mkexpr( frB ) ) );
      putDReg( frS_addr, mkexpr( frS ) );
      break;
   case 0x302: 
      DIP( "drsp%s fr%u,fr%u\n",
           flag_rC ? ".":"", frS_addr, frB_addr );
      frB = newTemp( Ity_D64 );
      frS = newTemp( Ity_D32 );
      assign( frB, getDReg( frB_addr ) );
      assign( frS, binop( Iop_D64toD32, round, mkexpr( frB ) ) );
      putDReg32( frS_addr, mkexpr( frS ) );
      break;
   case 0x122: 
      {
         IRTemp tmp = newTemp( Ity_I64 );

         DIP( "dctfix%s fr%u,fr%u\n",
              flag_rC ? ".":"", frS_addr, frB_addr );
         frB = newTemp( Ity_D64 );
         frS = newTemp( Ity_D64 );
         assign( frB, getDReg( frB_addr ) );
         assign( tmp, binop( Iop_D64toI64S, round, mkexpr( frB ) ) );
         assign( frS, unop( Iop_ReinterpI64asD64, mkexpr( tmp ) ) );
         putDReg( frS_addr, mkexpr( frS ) );
      }
      break;
   case 0x322: 
      DIP( "dcffix%s fr%u,fr%u\n",
           flag_rC ? ".":"", frS_addr, frB_addr );
      frB = newTemp( Ity_D64 );
      frS = newTemp( Ity_D64 );
      assign( frB, getDReg( frB_addr ) );
      assign( frS, binop( Iop_I64StoD64,
                          round,
                          unop( Iop_ReinterpD64asI64, mkexpr( frB ) ) ) );
      putDReg( frS_addr, mkexpr( frS ) );
      break;
   }

   if (flag_rC && clear_CR1) {
      putCR321( 1, mkU8( 0 ) );
      putCR0( 1, mkU8( 0 ) );
   }

   return True;
}

static Bool dis_dfp_fmt_convq(UInt theInstr) {
   UInt opc2      = ifieldOPClo10( theInstr );
   UChar frS_addr = ifieldRegDS( theInstr );
   UChar frB_addr = ifieldRegB( theInstr );
   IRExpr* round  = get_IR_roundingmode_DFP();
   IRTemp frB64   = newTemp( Ity_D64 );
   IRTemp frB128  = newTemp( Ity_D128 );
   IRTemp frS64   = newTemp( Ity_D64 );
   IRTemp frS128  = newTemp( Ity_D128 );
   UChar flag_rC  = ifieldBIT0( theInstr );
   Bool clear_CR1 = True;

   switch (opc2) {
   case 0x102: 
      DIP( "dctqpq%s fr%u,fr%u\n",
           flag_rC ? ".":"", frS_addr, frB_addr );
      assign( frB64, getDReg( frB_addr ) );
      assign( frS128, unop( Iop_D64toD128, mkexpr( frB64 ) ) );
      putDReg_pair( frS_addr, mkexpr( frS128 ) );
      break;
   case 0x122: 
      {
         IRTemp tmp = newTemp( Ity_I64 );

         DIP( "dctfixq%s fr%u,fr%u\n",
              flag_rC ? ".":"", frS_addr, frB_addr );
         assign( frB128, getDReg_pair( frB_addr ) );
         assign( tmp, binop( Iop_D128toI64S, round, mkexpr( frB128 ) ) );
         assign( frS64, unop( Iop_ReinterpI64asD64, mkexpr( tmp ) ) );
         putDReg( frS_addr, mkexpr( frS64 ) );
      }
      break;
   case 0x302: 
      DIP( "drdpq%s fr%u,fr%u\n",
           flag_rC ? ".":"", frS_addr, frB_addr );
      assign( frB128, getDReg_pair( frB_addr ) );
      assign( frS64, binop( Iop_D128toD64, round, mkexpr( frB128 ) ) );
      putDReg( frS_addr, mkexpr( frS64 ) );
      break;
   case 0x322: 
     {
      DIP( "dcffixq%s fr%u,fr%u\n",
           flag_rC ? ".":"", frS_addr, frB_addr );
      assign( frB64, getDReg( frB_addr ) );
      assign( frS128, unop( Iop_I64StoD128,
                            unop( Iop_ReinterpD64asI64,
                                  mkexpr( frB64 ) ) ) );
      putDReg_pair( frS_addr, mkexpr( frS128 ) );
      break;
     }
   }

   if (flag_rC && clear_CR1) {
      putCR321( 1, mkU8( 0 ) );
      putCR0( 1, mkU8( 0 ) );
   }

   return True;
}

static Bool dis_dfp_round( UInt theInstr ) {
   UChar frS_addr = ifieldRegDS(theInstr);
   UChar R        = IFIELD(theInstr, 16, 1);
   UChar RMC      = IFIELD(theInstr, 9, 2);
   UChar frB_addr = ifieldRegB( theInstr );
   UChar flag_rC  = ifieldBIT0( theInstr );
   IRTemp frB     = newTemp( Ity_D64 );
   IRTemp frS     = newTemp( Ity_D64 );
   UInt opc2      = ifieldOPClo8( theInstr );
   Bool clear_CR1 = True;

   switch (opc2) {
   case 0x63: 
   case 0xE3: 
      DIP( "drintx/drintn%s fr%u,fr%u\n",
           flag_rC ? ".":"", frS_addr, frB_addr );

      
      assign( frB, getDReg( frB_addr ) );
      assign( frS, binop( Iop_RoundD64toInt,
                          mkU32( ( R << 3 ) | RMC ),
                          mkexpr( frB ) ) );
      putDReg( frS_addr, mkexpr( frS ) );
      break;
   default:
      vex_printf("dis_dfp_round(ppc)(opc2)\n");
      return False;
   }

   if (flag_rC && clear_CR1) {
      putCR321( 1, mkU8( 0 ) );
      putCR0( 1, mkU8( 0 ) );
   }

   return True;
}

static Bool dis_dfp_roundq(UInt theInstr) {
   UChar frS_addr = ifieldRegDS( theInstr );
   UChar frB_addr = ifieldRegB( theInstr );
   UChar R = IFIELD(theInstr, 16, 1);
   UChar RMC = IFIELD(theInstr, 9, 2);
   UChar flag_rC = ifieldBIT0( theInstr );
   IRTemp frB = newTemp( Ity_D128 );
   IRTemp frS = newTemp( Ity_D128 );
   Bool clear_CR1 = True;
   UInt opc2 = ifieldOPClo8( theInstr );

   switch (opc2) {
   case 0x63: 
   case 0xE3: 
      DIP( "drintxq/drintnq%s fr%u,fr%u\n",
           flag_rC ? ".":"", frS_addr, frB_addr );

      
      assign( frB, getDReg_pair( frB_addr ) );
      assign( frS, binop( Iop_RoundD128toInt,
                          mkU32( ( R << 3 ) | RMC ),
                          mkexpr( frB ) ) );
      putDReg_pair( frS_addr, mkexpr( frS ) );
      break;
   default:
      vex_printf("dis_dfp_roundq(ppc)(opc2)\n");
      return False;
   }

   if (flag_rC && clear_CR1) {
      putCR321( 1, mkU8( 0 ) );
      putCR0( 1, mkU8( 0 ) );
   }

   return True;
}

static Bool dis_dfp_quantize_sig_rrnd(UInt theInstr) {
   UInt opc2 = ifieldOPClo8( theInstr );
   UChar frS_addr = ifieldRegDS( theInstr );
   UChar frA_addr = ifieldRegA( theInstr );
   UChar frB_addr = ifieldRegB( theInstr );
   UChar flag_rC = ifieldBIT0( theInstr );
   UInt TE_value = IFIELD(theInstr, 16, 4);
   UInt TE_sign  = IFIELD(theInstr, 20, 1);
   UInt RMC = IFIELD(theInstr, 9, 2);
   IRTemp frA = newTemp( Ity_D64 );
   IRTemp frB = newTemp( Ity_D64 );
   IRTemp frS = newTemp( Ity_D64 );
   Bool clear_CR1 = True;

   assign( frB, getDReg( frB_addr ) );

   switch (opc2) {
   case 0x43: 
      DIP( "dquai%s fr%u,fr%u,fr%u\n",
           flag_rC ? ".":"", frS_addr, frA_addr, frB_addr );
      IRTemp TE_I64 = newTemp( Ity_I64 );

      if (TE_sign == 1) {
         assign( TE_I64,
                 unop( Iop_32Uto64,
                       binop( Iop_Sub32, mkU32( 397 ),
                              binop( Iop_And32, mkU32( 0xF ),
                                     unop( Iop_Not32, mkU32( TE_value ) )
                                     ) ) ) );

      } else {
          assign( TE_I64,
                  unop( Iop_32Uto64,
                        binop( Iop_Add32, mkU32( 398 ), mkU32( TE_value ) )
                        ) );
      }

      assign( frA, binop( Iop_InsertExpD64, mkexpr( TE_I64 ),
                          unop( Iop_ReinterpI64asD64, mkU64( 1 ) ) ) );

      assign( frS, triop( Iop_QuantizeD64,
                          mkU32( RMC ),
                          mkexpr( frA ),
                          mkexpr( frB ) ) );
      break;

   case 0x3: 
      DIP( "dqua%s fr%u,fr%u,fr%u\n",
           flag_rC ? ".":"", frS_addr, frA_addr, frB_addr );
      assign( frA, getDReg( frA_addr ) );
      assign( frS, triop( Iop_QuantizeD64,
                          mkU32( RMC ),
                          mkexpr( frA ),
                          mkexpr( frB ) ) );
      break;
   case 0x23: 
      {
         IRTemp tmp = newTemp( Ity_I8 );

         DIP( "drrnd%s fr%u,fr%u,fr%u\n",
              flag_rC ? ".":"", frS_addr, frA_addr, frB_addr );
         assign( frA, getDReg( frA_addr ) );
         
         assign( tmp, unop( Iop_32to8,
                            unop( Iop_64to32,
                                  unop( Iop_ReinterpD64asI64,
                                        mkexpr( frA ) ) ) ) );
         assign( frS, triop( Iop_SignificanceRoundD64,
                             mkU32( RMC ),
                             mkexpr( tmp ),
                             mkexpr( frB ) ) );
      }
      break;
   default:
      vex_printf("dis_dfp_quantize_sig_rrnd(ppc)(opc2)\n");
      return False;
   }
   putDReg( frS_addr, mkexpr( frS ) );

   if (flag_rC && clear_CR1) {
      putCR321( 1, mkU8( 0 ) );
      putCR0( 1, mkU8( 0 ) );
   }

   return True;
}

static Bool dis_dfp_quantize_sig_rrndq(UInt theInstr) {
   UInt opc2 = ifieldOPClo8( theInstr );
   UChar frS_addr = ifieldRegDS( theInstr );
   UChar frA_addr = ifieldRegA( theInstr );
   UChar frB_addr = ifieldRegB( theInstr );
   UChar flag_rC = ifieldBIT0( theInstr );
   UInt TE_value = IFIELD(theInstr, 16, 4);
   UInt TE_sign  = IFIELD(theInstr, 20, 1);
   UInt RMC = IFIELD(theInstr, 9, 2);
   IRTemp frA = newTemp( Ity_D128 );
   IRTemp frB = newTemp( Ity_D128 );
   IRTemp frS = newTemp( Ity_D128 );
   Bool clear_CR1 = True;

   assign( frB, getDReg_pair( frB_addr ) );

   switch (opc2) {
   case 0x43: 
      DIP( "dquaiq%s fr%u,fr%u,fr%u\n",
           flag_rC ? ".":"", frS_addr, frA_addr, frB_addr );
      IRTemp TE_I64 = newTemp( Ity_I64 );

      if (TE_sign == 1) {
         assign( TE_I64,
                 unop( Iop_32Uto64,
                       binop( Iop_Sub32, mkU32( 6175 ),
                              binop( Iop_And32, mkU32( 0xF ),
                                     unop( Iop_Not32, mkU32( TE_value ) )
                                     ) ) ) );

      } else {
         assign( TE_I64,
                 unop( Iop_32Uto64,
                       binop( Iop_Add32,
                             mkU32( 6176 ),
                             mkU32( TE_value ) ) ) );
      }

      assign( frA,
              binop( Iop_InsertExpD128, mkexpr( TE_I64 ),
                     unop( Iop_D64toD128,
                           unop( Iop_ReinterpI64asD64, mkU64( 1 ) ) ) ) );
      assign( frS, triop( Iop_QuantizeD128,
                          mkU32( RMC ),
                          mkexpr( frA ),
                          mkexpr( frB ) ) );
      break;
   case 0x3: 
      DIP( "dquaiq%s fr%u,fr%u,fr%u\n",
           flag_rC ? ".":"", frS_addr, frA_addr, frB_addr );
      assign( frA, getDReg_pair( frA_addr ) );
      assign( frS, triop( Iop_QuantizeD128,
                          mkU32( RMC ),
                          mkexpr( frA ),
                          mkexpr( frB ) ) );
      break;
   case 0x23: 
      {
         IRTemp tmp = newTemp( Ity_I8 );

         DIP( "drrndq%s fr%u,fr%u,fr%u\n",
              flag_rC ? ".":"", frS_addr, frA_addr, frB_addr );
         assign( frA, getDReg_pair( frA_addr ) );
         assign( tmp, unop( Iop_32to8,
                            unop( Iop_64to32,
                                  unop( Iop_ReinterpD64asI64,
                                        unop( Iop_D128HItoD64,
                                              mkexpr( frA ) ) ) ) ) );
         assign( frS, triop( Iop_SignificanceRoundD128,
                             mkU32( RMC ),
                             mkexpr( tmp ),
                             mkexpr( frB ) ) );
      }
      break;
   default:
      vex_printf("dis_dfp_quantize_sig_rrndq(ppc)(opc2)\n");
      return False;
   }
   putDReg_pair( frS_addr, mkexpr( frS ) );

   if (flag_rC && clear_CR1) {
      putCR321( 1, mkU8( 0 ) );
      putCR0( 1, mkU8( 0 ) );
   }

   return True;
}

static Bool dis_dfp_extract_insert(UInt theInstr) {
   UInt opc2 = ifieldOPClo10( theInstr );
   UChar frS_addr = ifieldRegDS( theInstr );
   UChar frA_addr = ifieldRegA( theInstr );
   UChar frB_addr = ifieldRegB( theInstr );
   UChar flag_rC = ifieldBIT0( theInstr );
   Bool clear_CR1 = True;

   IRTemp frA = newTemp( Ity_D64 );
   IRTemp frB = newTemp( Ity_D64 );
   IRTemp frS = newTemp( Ity_D64 );
   IRTemp tmp = newTemp( Ity_I64 );

   assign( frA, getDReg( frA_addr ) );
   assign( frB, getDReg( frB_addr ) );

   switch (opc2) {
   case 0x162: 
      DIP( "dxex%s fr%u,fr%u,fr%u\n",
           flag_rC ? ".":"", frS_addr, frA_addr, frB_addr );
      assign( tmp, unop( Iop_ExtractExpD64, mkexpr( frB ) ) );
      assign( frS, unop( Iop_ReinterpI64asD64, mkexpr( tmp ) ) );
      break;
   case 0x362: 
      DIP( "diex%s fr%u,fr%u,fr%u\n",
           flag_rC ? ".":"", frS_addr, frA_addr, frB_addr );
      assign( frS, binop( Iop_InsertExpD64,
                          unop( Iop_ReinterpD64asI64,
                                mkexpr( frA ) ),
                          mkexpr( frB ) ) );
      break;
   default:
      vex_printf("dis_dfp_extract_insert(ppc)(opc2)\n");
      return False;
   }

   putDReg( frS_addr, mkexpr( frS ) );

   if (flag_rC && clear_CR1) {
      putCR321( 1, mkU8( 0 ) );
      putCR0( 1, mkU8( 0 ) );
   }

   return True;
}

static Bool dis_dfp_extract_insertq(UInt theInstr) {
   UInt opc2 = ifieldOPClo10( theInstr );
   UChar frS_addr = ifieldRegDS( theInstr );
   UChar frA_addr = ifieldRegA( theInstr );
   UChar frB_addr = ifieldRegB( theInstr );
   UChar flag_rC = ifieldBIT0( theInstr );

   IRTemp frA   = newTemp( Ity_D64 );
   IRTemp frB   = newTemp( Ity_D128 );
   IRTemp frS64 = newTemp( Ity_D64 );
   IRTemp frS   = newTemp( Ity_D128 );
   IRTemp tmp   = newTemp( Ity_I64 );
   Bool clear_CR1 = True;

   assign( frB, getDReg_pair( frB_addr ) );

   switch (opc2) {
   case 0x162:  
      DIP( "dxexq%s fr%u,fr%u\n",
           flag_rC ? ".":"", frS_addr,  frB_addr );
      assign( tmp, unop( Iop_ExtractExpD128, mkexpr( frB ) ) );
      assign( frS64, unop( Iop_ReinterpI64asD64, mkexpr( tmp ) ) );
      putDReg( frS_addr, mkexpr( frS64 ) );
      break;
   case 0x362:  
      DIP( "diexq%s fr%u,fr%u,fr%u\n",
           flag_rC ? ".":"", frS_addr, frA_addr, frB_addr );
      assign( frA, getDReg( frA_addr ) );
      assign( frS, binop( Iop_InsertExpD128,
                          unop( Iop_ReinterpD64asI64, mkexpr( frA ) ),
                          mkexpr( frB ) ) );
      putDReg_pair( frS_addr, mkexpr( frS ) );
      break;
   default:
      vex_printf("dis_dfp_extract_insertq(ppc)(opc2)\n");
      return False;
   }

   if (flag_rC && clear_CR1) {
      putCR321( 1, mkU8( 0 ) );
      putCR0( 1, mkU8( 0 ) );
   }

   return True;
}

static Bool dis_dfp_compare(UInt theInstr) {
   
   UChar crfD = toUChar( IFIELD( theInstr, 23, 3 ) ); 
   UChar frA_addr = ifieldRegA( theInstr );
   UChar frB_addr = ifieldRegB( theInstr );
   UInt opc1 = ifieldOPC( theInstr );
   IRTemp frA;
   IRTemp frB;

   IRTemp ccIR = newTemp( Ity_I32 );
   IRTemp ccPPC32 = newTemp( Ity_I32 );


   switch (opc1) {
   case 0x3B: 
      DIP( "dcmpo %u,fr%u,fr%u\n", crfD, frA_addr, frB_addr );
      frA = newTemp( Ity_D64 );
      frB = newTemp( Ity_D64 );

      assign( frA, getDReg( frA_addr ) );
      assign( frB, getDReg( frB_addr ) );

      assign( ccIR, binop( Iop_CmpD64, mkexpr( frA ), mkexpr( frB ) ) );
      break;
   case 0x3F: 
      DIP( "dcmpoq %u,fr%u,fr%u\n", crfD, frA_addr, frB_addr );
      frA = newTemp( Ity_D128 );
      frB = newTemp( Ity_D128 );

      assign( frA, getDReg_pair( frA_addr ) );
      assign( frB, getDReg_pair( frB_addr ) );
      assign( ccIR, binop( Iop_CmpD128, mkexpr( frA ), mkexpr( frB ) ) );
      break;
   default:
      vex_printf("dis_dfp_compare(ppc)(opc2)\n");
      return False;
   }

   

   assign( ccPPC32,
           binop( Iop_Shl32,
                  mkU32( 1 ),
                  unop( Iop_32to8,
                        binop( Iop_Or32,
                               binop( Iop_And32,
                                      unop( Iop_Not32,
                                            binop( Iop_Shr32,
                                                   mkexpr( ccIR ),
                                                   mkU8( 5 ) ) ),
                                      mkU32( 2 ) ),
                               binop( Iop_And32,
                                      binop( Iop_Xor32,
                                             mkexpr( ccIR ),
                                             binop( Iop_Shr32,
                                                    mkexpr( ccIR ),
                                                    mkU8( 6 ) ) ),
                                      mkU32( 1 ) ) ) ) ) );

   putGST_field( PPC_GST_CR, mkexpr( ccPPC32 ), crfD );
   return True;
}

static Bool dis_dfp_exponent_test ( UInt theInstr )
{
   UChar frA_addr   = ifieldRegA( theInstr );
   UChar frB_addr   = ifieldRegB( theInstr );
   UChar crfD       = toUChar( IFIELD( theInstr, 23, 3 ) );
   IRTemp frA       = newTemp( Ity_D64 );
   IRTemp frB       = newTemp( Ity_D64 );
   IRTemp frA128    = newTemp( Ity_D128 );
   IRTemp frB128    = newTemp( Ity_D128 );
   UInt opc1        = ifieldOPC( theInstr );
   IRTemp gfield_A  = newTemp( Ity_I32 );
   IRTemp gfield_B  = newTemp( Ity_I32 );
   IRTemp gfield_mask   = newTemp( Ity_I32 );
   IRTemp exponent_A    = newTemp( Ity_I32 );
   IRTemp exponent_B    = newTemp( Ity_I32 );
   IRTemp A_NaN_true    = newTemp( Ity_I32 );
   IRTemp B_NaN_true    = newTemp( Ity_I32 );
   IRTemp A_inf_true    = newTemp( Ity_I32 );
   IRTemp B_inf_true    = newTemp( Ity_I32 );
   IRTemp A_equals_B    = newTemp( Ity_I32 );
   IRTemp finite_number = newTemp( Ity_I32 );
   IRTemp cc0 = newTemp( Ity_I32 );
   IRTemp cc1 = newTemp( Ity_I32 );
   IRTemp cc2 = newTemp( Ity_I32 );
   IRTemp cc3 = newTemp( Ity_I32 );

   switch (opc1) {
   case 0x3b: 
      DIP("dtstex %u,r%u,r%d\n", crfD, frA_addr, frB_addr);
      assign( frA, getDReg( frA_addr ) );
      assign( frB, getDReg( frB_addr ) );
      assign( gfield_mask, mkU32( DFP_G_FIELD_LONG_MASK ) );
      assign(exponent_A, unop( Iop_64to32,
                               unop( Iop_ExtractExpD64,
                                     mkexpr( frA ) ) ) );
      assign(exponent_B, unop( Iop_64to32,
                               unop( Iop_ExtractExpD64,
                                     mkexpr( frB ) ) ) );
      break;

   case 0x3F: 
      DIP("dtstexq %u,r%u,r%d\n", crfD, frA_addr, frB_addr);
      assign( frA128, getDReg_pair( frA_addr ) );
      assign( frB128, getDReg_pair( frB_addr ) );
      assign( frA, unop( Iop_D128HItoD64, mkexpr( frA128 ) ) );
      assign( frB, unop( Iop_D128HItoD64, mkexpr( frB128 ) ) );
      assign( gfield_mask, mkU32( DFP_G_FIELD_EXTND_MASK ) );
      assign( exponent_A, unop( Iop_64to32,
                                unop( Iop_ExtractExpD128,
                                      mkexpr( frA128 ) ) ) );
      assign( exponent_B, unop( Iop_64to32,
                                unop( Iop_ExtractExpD128,
                                      mkexpr( frB128 ) ) ) );
      break;
   default:
      vex_printf("dis_dfp_exponent_test(ppc)(opc2)\n");
      return False;
   }

   
   assign( gfield_A, binop( Iop_And32,
                            mkexpr( gfield_mask ),
                            unop( Iop_64HIto32,
                                  unop( Iop_ReinterpD64asI64,
                                        mkexpr(frA) ) ) ) );

   assign( gfield_B, binop( Iop_And32,
                            mkexpr( gfield_mask ),
                            unop( Iop_64HIto32,
                                  unop( Iop_ReinterpD64asI64,
                                        mkexpr(frB) ) ) ) );

   
   assign( A_NaN_true, binop(Iop_Or32,
                             unop( Iop_1Sto32,
                                   binop( Iop_CmpEQ32,
                                          mkexpr( gfield_A ),
                                          mkU32( 0x7C000000 ) ) ),
                             unop( Iop_1Sto32,
                                   binop( Iop_CmpEQ32,
                                          mkexpr( gfield_A ),
                                          mkU32( 0x7E000000 ) )
                                   ) ) );
   assign( B_NaN_true, binop(Iop_Or32,
                             unop( Iop_1Sto32,
                                   binop( Iop_CmpEQ32,
                                          mkexpr( gfield_B ),
                                          mkU32( 0x7C000000 ) ) ),
                             unop( Iop_1Sto32,
                                   binop( Iop_CmpEQ32,
                                          mkexpr( gfield_B ),
                                          mkU32( 0x7E000000 ) )
                             ) ) );

    
   assign( A_inf_true,
           unop( Iop_1Sto32,
                 binop( Iop_CmpEQ32,
                        mkexpr( gfield_A ),
                        mkU32( 0x78000000 ) ) ) );

   assign( B_inf_true,
           unop( Iop_1Sto32,
                 binop( Iop_CmpEQ32,
                        mkexpr( gfield_B ),
                        mkU32( 0x78000000 ) ) ) );

   assign( finite_number,
           unop( Iop_Not32,
                 binop( Iop_Or32,
                        binop( Iop_Or32,
                               mkexpr( A_NaN_true ),
                               mkexpr( B_NaN_true ) ),
                        binop( Iop_Or32,
                               mkexpr( A_inf_true ),
                               mkexpr( B_inf_true ) ) ) ) );

   assign( A_equals_B,
           binop( Iop_Or32,
                  unop( Iop_1Uto32,
                  binop( Iop_CmpEQ32,
                         mkexpr( exponent_A ),
                         mkexpr( exponent_B ) ) ),
                  binop( Iop_Or32,
                         binop( Iop_And32,
                                mkexpr( A_inf_true ),
                                mkexpr( B_inf_true ) ),
                         binop( Iop_And32,
                                mkexpr( A_NaN_true ),
                                mkexpr( B_NaN_true ) ) ) ) );

   assign( cc0, binop( Iop_And32,
                       mkexpr( finite_number ),
                       binop( Iop_Shl32,
                              unop( Iop_1Uto32,
                                    binop( Iop_CmpLT32U,
                                           mkexpr( exponent_A ),
                                           mkexpr( exponent_B ) ) ),
                                           mkU8( 3 ) ) ) );

   assign( cc1, binop( Iop_And32,
                       mkexpr( finite_number ),
                       binop( Iop_Shl32,
                              unop( Iop_1Uto32,
                                    binop( Iop_CmpLT32U,
                                           mkexpr( exponent_B ),
                                           mkexpr( exponent_A ) ) ),
                                           mkU8( 2 ) ) ) );

   assign( cc2, binop( Iop_Shl32, 
                       binop( Iop_And32,
                              mkexpr( A_equals_B ),
                              mkU32( 1 ) ),
                              mkU8( 1 ) ) );

   assign( cc3, binop( Iop_And32,
                       unop( Iop_Not32, mkexpr( A_equals_B ) ),
                       binop( Iop_And32,
                              mkU32( 0x1 ),
                              binop( Iop_Or32,
                                     binop( Iop_Or32,
                                            mkexpr ( A_inf_true ),
                                            mkexpr ( B_inf_true ) ),
                                            binop( Iop_Or32,
                                                   mkexpr ( A_NaN_true ),
                                                   mkexpr ( B_NaN_true ) ) )
                              ) ) );

   
   putGST_field( PPC_GST_CR,
                 binop( Iop_Or32,
                        mkexpr( cc0 ),
                        binop( Iop_Or32,
                               mkexpr( cc1 ),
                               binop( Iop_Or32,
                                      mkexpr( cc2 ),
                                      mkexpr( cc3 ) ) ) ),
                 crfD );
   return True;
}

static Bool dis_dfp_class_test ( UInt theInstr )
{
   UChar frA_addr   = ifieldRegA( theInstr );
   IRTemp frA       = newTemp( Ity_D64 );
   IRTemp abs_frA   = newTemp( Ity_D64 );
   IRTemp frAI64_hi = newTemp( Ity_I64 );
   IRTemp frAI64_lo = newTemp( Ity_I64 );
   UInt opc1        = ifieldOPC( theInstr );
   UInt opc2        = ifieldOPClo9( theInstr );
   UChar crfD       = toUChar( IFIELD( theInstr, 23, 3 ) );  
   UInt DCM         = IFIELD( theInstr, 10, 6 );
   IRTemp DCM_calc  = newTemp( Ity_I32 );
   UInt max_exp     = 0;
   UInt min_exp     = 0;
   IRTemp min_subnormalD64  = newTemp( Ity_D64 );
   IRTemp min_subnormalD128 = newTemp( Ity_D128 );
   IRTemp significand64  = newTemp( Ity_D64 );
   IRTemp significand128 = newTemp( Ity_D128 );
   IRTemp exp_min_normal = newTemp( Ity_I64 );
   IRTemp exponent       = newTemp( Ity_I32 );

   IRTemp infinity_true  = newTemp( Ity_I32 );
   IRTemp SNaN_true      = newTemp( Ity_I32 );
   IRTemp QNaN_true      = newTemp( Ity_I32 );
   IRTemp subnormal_true = newTemp( Ity_I32 );
   IRTemp normal_true    = newTemp( Ity_I32 );
   IRTemp extreme_true   = newTemp( Ity_I32 );
   IRTemp lmd            = newTemp( Ity_I32 );
   IRTemp lmd_zero_true  = newTemp( Ity_I32 );
   IRTemp zero_true      = newTemp( Ity_I32 );
   IRTemp sign           = newTemp( Ity_I32 );
   IRTemp field          = newTemp( Ity_I32 );
   IRTemp ccIR_zero      = newTemp( Ity_I32 );
   IRTemp ccIR_subnormal = newTemp( Ity_I32 );

   
   IRTemp gfield = newTemp( Ity_I32 );
   IRTemp gfield_0_4_shift  = newTemp( Ity_I8 );
   IRTemp gfield_mask       = newTemp( Ity_I32 );
   IRTemp dcm0 = newTemp( Ity_I32 );
   IRTemp dcm1 = newTemp( Ity_I32 );
   IRTemp dcm2 = newTemp( Ity_I32 );
   IRTemp dcm3 = newTemp( Ity_I32 );
   IRTemp dcm4 = newTemp( Ity_I32 );
   IRTemp dcm5 = newTemp( Ity_I32 );


   assign( frA, getDReg( frA_addr ) );
   assign( frAI64_hi, unop( Iop_ReinterpD64asI64, mkexpr( frA ) ) );

   assign( abs_frA, unop( Iop_ReinterpI64asD64,
                          binop( Iop_And64,
                                 unop( Iop_ReinterpD64asI64,
                                       mkexpr( frA ) ),
                                 mkU64( 0x7FFFFFFFFFFFFFFFULL ) ) ) );
   assign( gfield_0_4_shift, mkU8( 31 - 5 ) );  
   switch (opc1) {
   case 0x3b: 
      DIP("dtstd%s %u,r%u,%d\n", opc2 == 0xc2 ? "c" : "g",
               crfD, frA_addr, DCM);
      
      assign( frAI64_lo, mkU64( 0 ) );
      assign( gfield_mask, mkU32( DFP_G_FIELD_LONG_MASK ) );
      max_exp = DFP_LONG_EXP_MAX;
      min_exp = DFP_LONG_EXP_MIN;

      assign( exponent, unop( Iop_64to32,
                              unop( Iop_ExtractExpD64,
                                    mkexpr( frA ) ) ) );
      assign( significand64,
              unop( Iop_ReinterpI64asD64,
                    mkU64( 0x2234000000000001ULL ) ) );  
      assign( exp_min_normal,mkU64( 398 - 383 ) );
      assign( min_subnormalD64,
              binop( Iop_InsertExpD64,
                     mkexpr( exp_min_normal ),
                     mkexpr( significand64 ) ) );

      assign( ccIR_subnormal,
              binop( Iop_CmpD64,
                     mkexpr( abs_frA ),
                     mkexpr( min_subnormalD64 ) ) );

      
      assign( ccIR_zero,
              binop( Iop_CmpD64,
                     mkexpr( abs_frA ),
                     unop( Iop_ReinterpI64asD64,
                           mkU64( 0x2238000000000000ULL ) ) ) );

      
      break;

   case 0x3F:   
      DIP("dtstd%sq %u,r%u,%d\n", opc2 == 0xc2 ? "c" : "g",
               crfD, frA_addr, DCM);
      assign( frAI64_lo, unop( Iop_ReinterpD64asI64,
                               getDReg( frA_addr+1 ) ) );

      assign( gfield_mask, mkU32( DFP_G_FIELD_EXTND_MASK ) );
      max_exp = DFP_EXTND_EXP_MAX;
      min_exp = DFP_EXTND_EXP_MIN;
      assign( exponent, unop( Iop_64to32, 
                              unop( Iop_ExtractExpD128,
                                    getDReg_pair( frA_addr) ) ) );

      
      assign( exp_min_normal, mkU64( 6176 - 6143 ) );
      assign( significand128,
              unop( Iop_D64toD128,
                    unop( Iop_ReinterpI64asD64,
                          mkU64( 0x2234000000000001ULL ) ) ) );  

      assign( min_subnormalD128,
              binop( Iop_InsertExpD128,
                     mkexpr( exp_min_normal ),
                     mkexpr( significand128 ) ) );

      assign( ccIR_subnormal, 
              binop( Iop_CmpD128,
                     binop( Iop_D64HLtoD128,
                            unop( Iop_ReinterpI64asD64,
                                  binop( Iop_And64,
                                         unop( Iop_ReinterpD64asI64,
                                               mkexpr( frA ) ),
                                         mkU64( 0x7FFFFFFFFFFFFFFFULL ) ) ),
                            getDReg( frA_addr+1 ) ),
                     mkexpr( min_subnormalD128 ) ) );
      assign( ccIR_zero,
              binop( Iop_CmpD128,
                     binop( Iop_D64HLtoD128,
                            mkexpr( abs_frA ),
                            getDReg( frA_addr+1 ) ),
                     unop( Iop_D64toD128,
                           unop( Iop_ReinterpI64asD64,
                                 mkU64( 0x0ULL ) ) ) ) );

      
      break;
   default:
      vex_printf("dis_dfp_class_test(ppc)(opc2)\n");
      return False;
   }

   assign( gfield, binop( Iop_And32,
                          mkexpr( gfield_mask ),
                          unop( Iop_64HIto32,
                                mkexpr(frAI64_hi) ) ) );


   
   assign( infinity_true,
           unop( Iop_1Sto32,
                 binop( Iop_CmpEQ32,
                        binop( Iop_And32,
                               mkU32( 0x7C000000 ),
                               mkexpr( gfield ) ),
                        mkU32( 0x78000000 ) ) ) );

   assign( SNaN_true,
           unop( Iop_1Sto32,
                 binop( Iop_CmpEQ32,
                        binop( Iop_And32,
                               mkU32( 0x7E000000 ),
                               mkexpr( gfield ) ),
                        mkU32( 0x7E000000 ) ) ) );

   assign( QNaN_true,
           binop( Iop_And32,
                  unop( Iop_1Sto32,
                       binop( Iop_CmpEQ32,
                              binop( Iop_And32,
                                     mkU32( 0x7E000000 ),
                                     mkexpr( gfield ) ),
                              mkU32( 0x7C000000 ) ) ),
                  unop( Iop_Not32,
                        mkexpr( SNaN_true ) ) ) );

   assign( zero_true,
           binop( Iop_And32,
                  unop(Iop_1Sto32,
                       binop( Iop_CmpEQ32,
                              mkexpr( ccIR_zero ),
                              mkU32( 0x40 ) ) ),  
                  unop( Iop_Not32,
                        binop( Iop_Or32,
                               mkexpr( infinity_true ),
                               binop( Iop_Or32,
                                      mkexpr( QNaN_true ),
                                      mkexpr( SNaN_true ) ) ) ) ) );

   assign( subnormal_true, 
           binop( Iop_And32,
                  binop( Iop_Or32,
                         unop( Iop_1Sto32,
                               binop( Iop_CmpEQ32,
                                      mkexpr( ccIR_subnormal ),
                                      mkU32( 0x40 ) ) ), 
                         unop( Iop_1Sto32,
                               binop( Iop_CmpEQ32,
                                      mkexpr( ccIR_subnormal ),
                                      mkU32( 0x1 ) ) ) ), 
           unop( Iop_Not32,
                 binop( Iop_Or32,
                        binop( Iop_Or32,
                               mkexpr( infinity_true ),
                               mkexpr( zero_true) ),
                        binop( Iop_Or32,
                               mkexpr( QNaN_true ),
                               mkexpr( SNaN_true ) ) ) ) ) );

   
   assign( normal_true,
           unop( Iop_Not32,
                 binop( Iop_Or32,
                        binop( Iop_Or32,
                               mkexpr( infinity_true ),
                               mkexpr( zero_true ) ),
                        binop( Iop_Or32,
                               mkexpr( subnormal_true ),
                               binop( Iop_Or32,
                                      mkexpr( QNaN_true ),
                                      mkexpr( SNaN_true ) ) ) ) ) );

   if (opc2 == 0xC2) {    

      assign( dcm0, binop( Iop_Shl32,
                           mkexpr( zero_true ),
                           mkU8( 5 ) ) );
      assign( dcm1, binop( Iop_Shl32,
                           binop( Iop_And32,
                                  mkexpr( subnormal_true ),
                                  mkU32( 1 ) ),
                           mkU8( 4 ) ) );
      assign( dcm2, binop( Iop_Shl32,
                           binop( Iop_And32,
                                  mkexpr( normal_true ),
                                  mkU32( 1 ) ),
                           mkU8( 3 ) ) );
      assign( dcm3, binop( Iop_Shl32,
                           binop( Iop_And32,
                                  mkexpr( infinity_true),
                                  mkU32( 1 ) ),
                           mkU8( 2 ) ) );
      assign( dcm4, binop( Iop_Shl32,
                           binop( Iop_And32,
                                  mkexpr( QNaN_true ),
                                  mkU32( 1 ) ),
                           mkU8( 1 ) ) );
      assign( dcm5, binop( Iop_And32, mkexpr( SNaN_true), mkU32( 1 ) ) );

   } else if (opc2 == 0xE2) {   
      
      assign( extreme_true, binop( Iop_Or32,
                                   unop( Iop_1Sto32,
                                         binop( Iop_CmpEQ32,
                                                mkexpr( exponent ),
                                                mkU32( max_exp ) ) ),
                                   unop( Iop_1Sto32,
                                         binop( Iop_CmpEQ32,
                                                mkexpr( exponent ),
                                                mkU32( min_exp ) ) ) ) );

      
      Get_lmd( &lmd, binop( Iop_Shr32,
                            mkexpr( gfield ), mkU8( 31 - 5 ) ) );

      assign( lmd_zero_true, unop( Iop_1Sto32,
                                   binop( Iop_CmpEQ32,
                                          mkexpr( lmd ),
                                          mkU32( 0 ) ) ) );

      assign( dcm0, binop( Iop_Shl32,
                           binop( Iop_And32,
                                  binop( Iop_And32,
                                         unop( Iop_Not32,
                                               mkexpr( extreme_true ) ),
                                         mkexpr( zero_true ) ),
                                  mkU32( 0x1 ) ),
                           mkU8( 5 ) ) );

      assign( dcm1, binop( Iop_Shl32,
                           binop( Iop_And32,
                                  binop( Iop_And32,
                                         mkexpr( extreme_true ),
                                         mkexpr( zero_true ) ),
                                  mkU32( 0x1 ) ),
                           mkU8( 4 ) ) );

      assign( dcm2, binop( Iop_Shl32,
                           binop( Iop_And32,
                                  binop( Iop_Or32,
                                         binop( Iop_And32,
                                                mkexpr( extreme_true ),
                                                mkexpr( normal_true ) ),
                                         mkexpr( subnormal_true ) ),
                                  mkU32( 0x1 ) ),
                           mkU8( 3 ) ) );

      assign( dcm3, binop( Iop_Shl32,
                           binop( Iop_And32,
                                  binop( Iop_And32,
                                         binop( Iop_And32,
                                                unop( Iop_Not32,
                                                      mkexpr( extreme_true ) ),
                                                      mkexpr( normal_true ) ),
                                         unop( Iop_1Sto32,
                                               binop( Iop_CmpEQ32,
                                                      mkexpr( lmd ),
                                                      mkU32( 0 ) ) ) ),
                                  mkU32( 0x1 ) ),
                           mkU8( 2 ) ) );

      assign( dcm4, binop( Iop_Shl32,
                           binop( Iop_And32,
                                  binop( Iop_And32,
                                         binop( Iop_And32,
                                                unop( Iop_Not32,
                                                      mkexpr( extreme_true ) ),
                                                mkexpr( normal_true ) ),
                                          unop( Iop_1Sto32,
                                                binop( Iop_CmpNE32,
                                                       mkexpr( lmd ),
                                                       mkU32( 0 ) ) ) ),
                                  mkU32( 0x1 ) ),
                           mkU8( 1 ) ) );

      assign( dcm5, binop( Iop_And32,
                           binop( Iop_Or32,
                                  mkexpr( SNaN_true),
                                  binop( Iop_Or32,
                                         mkexpr( QNaN_true),
                                         mkexpr( infinity_true) ) ),
                           mkU32( 0x1 ) ) );
   }

   
   assign( DCM_calc,
           binop( Iop_Or32,
                  mkexpr( dcm0 ),
                  binop( Iop_Or32,
                         mkexpr( dcm1 ),
                         binop( Iop_Or32,
                                mkexpr( dcm2 ),
                                binop( Iop_Or32,
                                       mkexpr( dcm3 ),
                                       binop( Iop_Or32,
                                              mkexpr( dcm4 ),
                                              mkexpr( dcm5 ) ) ) ) ) ) );

   
   assign( sign,
           unop( Iop_1Uto32,
                 binop( Iop_CmpEQ32,
                        binop( Iop_Shr32,
                               unop( Iop_64HIto32, mkexpr( frAI64_hi ) ),
                               mkU8( 63 - 32 ) ),
                        mkU32( 1 ) ) ) );

   /* This instruction generates a four bit field to be stored in the
    * condition code register.  The condition code register consists of 7
    * fields.  The field to be written to is specified by the BF (AKA crfD)
    * field.
    *
    * The field layout is as follows:
    *
    *      Field          Meaning
    *      0000           Operand positive with no match
    *      0100           Operand positive with at least one match
    *      0001           Operand negative with no match
    *      0101           Operand negative with at least one match
    */
   assign( field, binop( Iop_Or32,
                         binop( Iop_Shl32,
                                mkexpr( sign ),
                                mkU8( 3 ) ),
                                binop( Iop_Shl32,
                                       unop( Iop_1Uto32,
                                             binop( Iop_CmpNE32,
                                                    binop( Iop_And32,
                                                           mkU32( DCM ),
                                                           mkexpr( DCM_calc ) ),
                                                     mkU32( 0 ) ) ),
                                       mkU8( 1 ) ) ) );

   putGST_field( PPC_GST_CR, mkexpr( field ), crfD );
   return True;
}

static Bool dis_dfp_bcd(UInt theInstr) {
   UInt opc2        = ifieldOPClo10( theInstr );
   ULong sp         = IFIELD(theInstr, 19, 2);
   ULong s          = IFIELD(theInstr, 20, 1);
   UChar frT_addr   = ifieldRegDS( theInstr );
   UChar frB_addr   = ifieldRegB( theInstr );
   IRTemp frB       = newTemp( Ity_D64 );
   IRTemp frBI64    = newTemp( Ity_I64 );
   IRTemp result    = newTemp( Ity_I64 );
   IRTemp resultD64 = newTemp( Ity_D64 );
   IRTemp bcd64     = newTemp( Ity_I64 );
   IRTemp bcd_u     = newTemp( Ity_I32 );
   IRTemp bcd_l     = newTemp( Ity_I32 );
   IRTemp dbcd_u    = newTemp( Ity_I32 );
   IRTemp dbcd_l    = newTemp( Ity_I32 );
   IRTemp lmd       = newTemp( Ity_I32 );

   assign( frB, getDReg( frB_addr ) );
   assign( frBI64, unop( Iop_ReinterpD64asI64, mkexpr( frB ) ) );

   switch ( opc2 ) {
   case 0x142: 
      DIP( "ddedpd %llu,r%u,r%u\n", sp, frT_addr, frB_addr );

         assign( bcd64, unop( Iop_DPBtoBCD, mkexpr( frBI64 ) ) );
         assign( bcd_u, unop( Iop_64HIto32, mkexpr( bcd64 ) ) );
         assign( bcd_l, unop( Iop_64to32, mkexpr( bcd64 ) ) );

      if ( ( sp == 0 ) || ( sp == 1 ) ) {
         
         Get_lmd( &lmd,
                  binop( Iop_Shr32,
                         unop( Iop_64HIto32, mkexpr( frBI64 ) ),
                         mkU8( 31 - 5 ) ) ); 

         assign( result,
                 binop( Iop_32HLto64,
                        binop( Iop_Or32,
                               binop( Iop_Shl32, mkexpr( lmd ), mkU8( 28 ) ),
                               mkexpr( bcd_u ) ),
                        mkexpr( bcd_l ) ) );

      } else {
         IRTemp sign = newTemp( Ity_I32 );

         if (sp == 2) {
            

            assign( sign,
                    binop( Iop_Or32,
                           binop( Iop_Shr32,
                                  unop( Iop_64HIto32, mkexpr( frBI64 ) ),
                                  mkU8( 31 ) ),
                           mkU32( 0xC ) ) );

         } else if ( sp == 3 ) {
            
            IRTemp tmp32 = newTemp( Ity_I32 );

            
            assign( tmp32,
                    binop( Iop_Xor32,
                           binop( Iop_Shr32,
                                  unop( Iop_64HIto32, mkexpr( frBI64 ) ),
                                  mkU8( 30 ) ),
                           mkU32( 0x2 ) ) );

            assign( sign, binop( Iop_Or32, mkexpr( tmp32 ), mkU32( 0xD ) ) );

         } else {
            vpanic( "The impossible happened: dis_dfp_bcd(ppc), undefined SP field" );
         }

         assign( result,
                 binop( Iop_32HLto64,
                        binop( Iop_Or32,
                               binop( Iop_Shr32,
                                      mkexpr( bcd_l ),
                                      mkU8( 28 ) ),
                               binop( Iop_Shl32,
                                      mkexpr( bcd_u ),
                                      mkU8( 4 ) ) ),
                        binop( Iop_Or32,
                                      mkexpr( sign ),
                               binop( Iop_Shl32,
                                      mkexpr( bcd_l ),
                                      mkU8( 4 ) ) ) ) );
      }

      putDReg( frT_addr, unop( Iop_ReinterpI64asD64, mkexpr( result ) ) );
      break;

   case 0x342: 
   {
      IRTemp valid_mask   = newTemp( Ity_I32 );
      IRTemp invalid_mask = newTemp( Ity_I32 );
      IRTemp without_lmd  = newTemp( Ity_I64 );
      IRTemp tmp64        = newTemp( Ity_I64 );
      IRTemp dbcd64       = newTemp( Ity_I64 );
      IRTemp left_exp     = newTemp( Ity_I32 );
      IRTemp g0_4         = newTemp( Ity_I32 );

      DIP( "denbcd %llu,r%u,r%u\n", s, frT_addr, frB_addr );

      if ( s == 0 ) {
         
         assign( dbcd64, unop( Iop_BCDtoDPB, mkexpr(frBI64 ) ) );
         assign( dbcd_u, unop( Iop_64HIto32, mkexpr( dbcd64 ) ) );
         assign( dbcd_l, unop( Iop_64to32, mkexpr( dbcd64 ) ) );

         assign( lmd,
                 binop( Iop_Shr32,
                        binop( Iop_And32,
                               unop( Iop_64HIto32, mkexpr( frBI64 ) ),
                               mkU32( 0xF0000000 ) ),
                        mkU8( 28 ) ) );

         assign( invalid_mask,
                 bcd_digit_inval( unop( Iop_64HIto32, mkexpr( frBI64 ) ),
                                  unop( Iop_64to32, mkexpr( frBI64 ) ) ) );
         assign( valid_mask, unop( Iop_Not32, mkexpr( invalid_mask ) ) );

         assign( without_lmd,
                 unop( Iop_ReinterpD64asI64,
                       binop( Iop_InsertExpD64,
                              mkU64( DFP_LONG_BIAS ),
                              unop( Iop_ReinterpI64asD64,
                                    binop( Iop_32HLto64,
                                           mkexpr( dbcd_u ),
                                           mkexpr( dbcd_l ) ) ) ) ) );
         assign( left_exp,
                 binop( Iop_Shr32,
                        binop( Iop_And32,
                               unop( Iop_64HIto32, mkexpr( without_lmd ) ),
                               mkU32( 0x60000000 ) ),
                        mkU8( 29 ) ) );

         assign( g0_4,
                 binop( Iop_Shl32,
                        Gfield_encoding( mkexpr( left_exp ), mkexpr( lmd ) ),
                        mkU8( 26 ) ) );

         assign( tmp64,
                 binop( Iop_32HLto64,
                        binop( Iop_Or32,
                               binop( Iop_And32,
                                      unop( Iop_64HIto32,
                                            mkexpr( without_lmd ) ),
                                      mkU32( 0x83FFFFFF ) ),
                               mkexpr( g0_4 ) ),
                        unop( Iop_64to32, mkexpr( without_lmd ) ) ) );

      } else if ( s == 1 ) {
         IRTemp sign = newTemp( Ity_I32 );
         IRTemp sign_bit = newTemp( Ity_I32 );
         IRTemp pos_sign_mask = newTemp( Ity_I32 );
         IRTemp neg_sign_mask = newTemp( Ity_I32 );
         IRTemp tmp = newTemp( Ity_I64 );

         assign( tmp, unop( Iop_BCDtoDPB,
                            binop( Iop_32HLto64,
                                   binop( Iop_Shr32,
                                          unop( Iop_64HIto32,
                                                mkexpr( frBI64 ) ),
                                                mkU8( 4 ) ),
                                   binop( Iop_Or32,
                                          binop( Iop_Shr32,
                                                 unop( Iop_64to32,
                                                       mkexpr( frBI64 ) ),
                                                  mkU8( 4 ) ),
                                          binop( Iop_Shl32,
                                                 unop( Iop_64HIto32,
                                                       mkexpr( frBI64 ) ),
                                                       mkU8( 28 ) ) ) ) ) );

         assign( dbcd_u, unop( Iop_64HIto32, mkexpr( tmp ) ) );
         assign( dbcd_l, unop( Iop_64to32, mkexpr( tmp ) ) );

         
         assign( sign,
                 binop( Iop_And32,
                        unop( Iop_64to32, mkexpr( frBI64 ) ),
                        mkU32( 0xF ) ) );

         assign( neg_sign_mask, Generate_neg_sign_mask( mkexpr( sign ) ) );
         assign( pos_sign_mask, Generate_pos_sign_mask( mkexpr( sign ) ) );
         assign( sign_bit,
                 Generate_sign_bit( mkexpr( pos_sign_mask ),
                                    mkexpr( neg_sign_mask ) ) );

         assign( invalid_mask,
                 Generate_inv_mask(
                                   bcd_digit_inval( unop( Iop_64HIto32,
                                                          mkexpr( frBI64 ) ),
                                                    binop( Iop_Shr32,
                                                           unop( Iop_64to32,
                                                                 mkexpr( frBI64 ) ),
                                                           mkU8( 4 ) ) ),
                                   mkexpr( pos_sign_mask ),
                                   mkexpr( neg_sign_mask ) ) );

         assign( valid_mask, unop( Iop_Not32, mkexpr( invalid_mask ) ) );

         
         assign( tmp64,
                 unop( Iop_ReinterpD64asI64,
                       binop( Iop_InsertExpD64,
                              mkU64( DFP_LONG_BIAS ),
                              unop( Iop_ReinterpI64asD64,
                                    binop( Iop_32HLto64,
                                           binop( Iop_Or32,
                                                  mkexpr( dbcd_u ),
                                                  mkexpr( sign_bit ) ),
                                           mkexpr( dbcd_l ) ) ) ) ) );
      }

      assign( resultD64,
              unop( Iop_ReinterpI64asD64,
                    binop( Iop_32HLto64,
                           binop( Iop_Or32,
                                  binop( Iop_And32,
                                         mkexpr( valid_mask ),
                                         unop( Iop_64HIto32,
                                               mkexpr( tmp64 ) ) ),
                                  binop( Iop_And32,
                                         mkU32( 0x7C000000 ),
                                         mkexpr( invalid_mask ) ) ),
                           binop( Iop_Or32,
                                  binop( Iop_And32,
                                         mkexpr( valid_mask ),
                                         unop( Iop_64to32, mkexpr( tmp64 ) ) ),
                                  binop( Iop_And32,
                                         mkU32( 0x0 ),
                                         mkexpr( invalid_mask ) ) ) ) ) );
      putDReg( frT_addr, mkexpr( resultD64 ) );
   }
   break;
   default:
      vpanic( "ERROR: dis_dfp_bcd(ppc), undefined opc2 case " );
      return False;
   }
   return True;
}

static Bool dis_dfp_bcdq( UInt theInstr )
{
   UInt opc2        = ifieldOPClo10( theInstr );
   ULong sp         = IFIELD(theInstr, 19, 2);
   ULong s          = IFIELD(theInstr, 20, 1);
   IRTemp frB_hi    = newTemp( Ity_D64 );
   IRTemp frB_lo    = newTemp( Ity_D64 );
   IRTemp frBI64_hi = newTemp( Ity_I64 );
   IRTemp frBI64_lo = newTemp( Ity_I64 );
   UChar frT_addr   = ifieldRegDS( theInstr );
   UChar frB_addr   = ifieldRegB( theInstr );

   IRTemp lmd       = newTemp( Ity_I32 );
   IRTemp result_hi = newTemp( Ity_I64 );
   IRTemp result_lo = newTemp( Ity_I64 );

   assign( frB_hi, getDReg( frB_addr ) );
   assign( frB_lo, getDReg( frB_addr + 1 ) );
   assign( frBI64_hi, unop( Iop_ReinterpD64asI64, mkexpr( frB_hi ) ) );
   assign( frBI64_lo, unop( Iop_ReinterpD64asI64, mkexpr( frB_lo ) ) );

   switch ( opc2 ) {
   case 0x142: 
   {
      IRTemp low_60_u = newTemp( Ity_I32 );
      IRTemp low_60_l = newTemp( Ity_I32 );
      IRTemp mid_60_u = newTemp( Ity_I32 );
      IRTemp mid_60_l = newTemp( Ity_I32 );
      IRTemp top_12_l = newTemp( Ity_I32 );

      DIP( "ddedpdq %llu,r%u,r%u\n", sp, frT_addr, frB_addr );

      Generate_132_bit_bcd_string( mkexpr( frBI64_hi ),
                                   mkexpr( frBI64_lo ),
                                   &top_12_l,
                                   &mid_60_u,
                                   &mid_60_l,
                                   &low_60_u,
                                   &low_60_l );

      if ( ( sp == 0 ) || ( sp == 1 ) ) {
         
         assign( result_hi,
                 binop( Iop_32HLto64,
                        binop( Iop_Or32,
                               binop( Iop_Shl32,
                                      mkexpr( top_12_l ),
                                      mkU8( 24 ) ),
                               binop( Iop_Shr32,
                                      mkexpr( mid_60_u ),
                                      mkU8( 4 ) ) ),
                        binop( Iop_Or32,
                               binop( Iop_Shl32,
                                      mkexpr( mid_60_u ),
                                      mkU8( 28 ) ),
                               binop( Iop_Shr32,
                                      mkexpr( mid_60_l ),
                                      mkU8( 4 ) ) ) ) );

         assign( result_lo,
                 binop( Iop_32HLto64,
                        binop( Iop_Or32,
                               binop( Iop_Shl32,
                                      mkexpr( mid_60_l ),
                                      mkU8( 28 ) ),
                               mkexpr( low_60_u ) ),
                        mkexpr( low_60_l ) ) );

      } else {
         IRTemp sign = newTemp( Ity_I32 );

         if ( sp == 2 ) {
            
            assign( sign,
                    binop( Iop_Or32,
                           binop( Iop_Shr32,
                                  unop( Iop_64HIto32, mkexpr( frBI64_hi ) ),
                                  mkU8( 31 ) ),
                           mkU32( 0xC ) ) );

         } else if ( sp == 3 ) {
            IRTemp tmp32 = newTemp( Ity_I32 );

            assign( tmp32,
                    binop( Iop_Xor32,
                           binop( Iop_Shr32,
                                  unop( Iop_64HIto32, mkexpr( frBI64_hi ) ),
                                  mkU8( 30 ) ),
                           mkU32( 0x2 ) ) );

            assign( sign, binop( Iop_Or32, mkexpr( tmp32 ), mkU32( 0xD ) ) );

         } else {
            vpanic( "The impossible happened: dis_dfp_bcd(ppc), undefined SP field" );
         }

         assign( result_hi,
                 binop( Iop_32HLto64,
                        binop( Iop_Or32,
                               binop( Iop_Shl32,
                                      mkexpr( top_12_l ),
                                      mkU8( 28 ) ),
                               mkexpr( mid_60_u ) ),
                        mkexpr( mid_60_l ) ) );

         assign( result_lo,
                 binop( Iop_32HLto64,
                        binop( Iop_Or32,
                               binop( Iop_Shl32,
                                      mkexpr( low_60_u ),
                                      mkU8( 4 ) ),
                               binop( Iop_Shr32,
                                      mkexpr( low_60_l ),
                                      mkU8( 28 ) ) ),
                        binop( Iop_Or32,
                               binop( Iop_Shl32,
                                      mkexpr( low_60_l ),
                                      mkU8( 4 ) ),
                               mkexpr( sign ) ) ) );
      }

      putDReg( frT_addr, unop( Iop_ReinterpI64asD64, mkexpr( result_hi ) ) );
      putDReg( frT_addr + 1,
               unop( Iop_ReinterpI64asD64, mkexpr( result_lo ) ) );
   }
   break;
   case 0x342: 
   {
      IRTemp valid_mask      = newTemp( Ity_I32 );
      IRTemp invalid_mask    = newTemp( Ity_I32 );
      IRTemp result128       = newTemp( Ity_D128 );
      IRTemp dfp_significand = newTemp( Ity_D128 );
      IRTemp tmp_hi          = newTemp( Ity_I64 );
      IRTemp tmp_lo          = newTemp( Ity_I64 );
      IRTemp dbcd_top_l      = newTemp( Ity_I32 );
      IRTemp dbcd_mid_u      = newTemp( Ity_I32 );
      IRTemp dbcd_mid_l      = newTemp( Ity_I32 );
      IRTemp dbcd_low_u      = newTemp( Ity_I32 );
      IRTemp dbcd_low_l      = newTemp( Ity_I32 );
      IRTemp bcd_top_8       = newTemp( Ity_I64 );
      IRTemp bcd_mid_60      = newTemp( Ity_I64 );
      IRTemp bcd_low_60      = newTemp( Ity_I64 );
      IRTemp sign_bit        = newTemp( Ity_I32 );
      IRTemp tmptop10        = newTemp( Ity_I64 );
      IRTemp tmpmid50        = newTemp( Ity_I64 );
      IRTemp tmplow50        = newTemp( Ity_I64 );
      IRTemp inval_bcd_digit_mask = newTemp( Ity_I32 );

      DIP( "denbcd %llu,r%u,r%u\n", s, frT_addr, frB_addr );

      if ( s == 0 ) {
         
         assign( sign_bit, mkU32( 0 ) ); 

         assign( bcd_top_8,
                 binop( Iop_32HLto64,
                        mkU32( 0 ),
                        binop( Iop_And32,
                               binop( Iop_Shr32,
                                      unop( Iop_64HIto32,
                                            mkexpr( frBI64_hi ) ),
                                      mkU8( 24 ) ),
                               mkU32( 0xFF ) ) ) );
         assign( bcd_mid_60,
                 binop( Iop_32HLto64,
                        binop( Iop_Or32,
                               binop( Iop_Shr32,
                                      unop( Iop_64to32,
                                            mkexpr( frBI64_hi ) ),
                                      mkU8( 28 ) ),
                               binop( Iop_Shl32,
                                      unop( Iop_64HIto32,
                                            mkexpr( frBI64_hi ) ),
                                      mkU8( 4 ) ) ),
                        binop( Iop_Or32,
                               binop( Iop_Shl32,
                                      unop( Iop_64to32,
                                            mkexpr( frBI64_hi ) ),
                                      mkU8( 4 ) ),
                               binop( Iop_Shr32,
                                      unop( Iop_64HIto32,
                                            mkexpr( frBI64_lo ) ),
                                      mkU8( 28 ) ) ) ) );

         
         assign( bcd_low_60, mkexpr( frBI64_lo ) );

         assign( tmptop10, unop( Iop_BCDtoDPB, mkexpr( bcd_top_8 ) ) );
         assign( dbcd_top_l, unop( Iop_64to32, mkexpr( tmptop10 ) ) );

         assign( tmpmid50, unop( Iop_BCDtoDPB, mkexpr( bcd_mid_60 ) ) );
         assign( dbcd_mid_u, unop( Iop_64HIto32, mkexpr( tmpmid50 ) ) );
         assign( dbcd_mid_l, unop( Iop_64to32, mkexpr( tmpmid50 ) ) );

         assign( tmplow50, unop( Iop_BCDtoDPB, mkexpr( bcd_low_60 ) ) );
         assign( dbcd_low_u, unop( Iop_64HIto32, mkexpr( tmplow50 ) ) );
         assign( dbcd_low_l, unop( Iop_64to32, mkexpr( tmplow50 ) ) );

         assign( lmd, mkU32( 0 ) );

         assign( invalid_mask,
                 binop( Iop_Or32,
                        bcd_digit_inval( mkU32( 0 ),
                                         unop( Iop_64to32,
                                               mkexpr( bcd_top_8 ) ) ),
                        binop( Iop_Or32,
                               bcd_digit_inval( unop( Iop_64HIto32,
                                                      mkexpr( bcd_mid_60 ) ),
                                                unop( Iop_64to32,
                                                      mkexpr( bcd_mid_60 ) ) ),
                               bcd_digit_inval( unop( Iop_64HIto32,
                                                      mkexpr( bcd_low_60 ) ),
                                                unop( Iop_64to32,
                                                      mkexpr( bcd_low_60 ) )
                                                ) ) ) );

      } else if ( s == 1 ) {
         IRTemp sign          = newTemp( Ity_I32 );
         IRTemp zero          = newTemp( Ity_I32 );
         IRTemp pos_sign_mask = newTemp( Ity_I32 );
         IRTemp neg_sign_mask = newTemp( Ity_I32 );

         
         assign( sign,
                 binop( Iop_And32,
                        unop( Iop_64to32, mkexpr( frBI64_lo ) ),
                        mkU32( 0xF ) ) );
         assign( neg_sign_mask, Generate_neg_sign_mask( mkexpr( sign ) ) );
         assign( pos_sign_mask, Generate_pos_sign_mask( mkexpr( sign ) ) );
         assign( sign_bit,
                 Generate_sign_bit( mkexpr( pos_sign_mask ),
                                    mkexpr( neg_sign_mask ) ) );

         
         assign( bcd_top_8,
                 binop( Iop_32HLto64,
                        mkU32( 0x0 ),
                        binop( Iop_Shr32,
                               unop( Iop_64HIto32, mkexpr( frBI64_hi ) ),
                               mkU8( 28 ) ) ) );

         
         assign( bcd_mid_60, mkexpr( frBI64_hi ) );

         
         assign( bcd_low_60,
                 binop( Iop_32HLto64,
                        binop( Iop_Shr32,
                               unop( Iop_64HIto32,
                                     mkexpr( frBI64_lo ) ),
                               mkU8( 4 ) ),
                               binop( Iop_Or32,
                                      binop( Iop_Shl32,
                                             unop( Iop_64HIto32,
                                                   mkexpr( frBI64_lo ) ),
                                             mkU8( 28 ) ),
                                      binop( Iop_Shr32,
                                             unop( Iop_64to32,
                                                   mkexpr( frBI64_lo ) ),
                                             mkU8( 4 ) ) ) ) );
         assign( tmptop10, unop( Iop_BCDtoDPB, mkexpr(bcd_top_8 ) ) );
         assign( dbcd_top_l, unop( Iop_64to32, mkexpr( tmptop10 ) ) );

         assign( tmpmid50, unop( Iop_BCDtoDPB, mkexpr(bcd_mid_60 ) ) );
         assign( dbcd_mid_u, unop( Iop_64HIto32, mkexpr( tmpmid50 ) ) );
         assign( dbcd_mid_l, unop( Iop_64to32, mkexpr( tmpmid50 ) ) );

         assign( tmplow50, unop( Iop_BCDtoDPB, mkexpr( bcd_low_60 ) ) );
         assign( dbcd_low_u, unop( Iop_64HIto32, mkexpr( tmplow50 ) ) );
         assign( dbcd_low_l, unop( Iop_64to32, mkexpr( tmplow50 ) ) );

         assign( lmd, mkU32( 0 ) );

         assign( zero, mkU32( 0 ) );
         assign( inval_bcd_digit_mask,
                 binop( Iop_Or32,
                        bcd_digit_inval( mkexpr( zero ),
                                         unop( Iop_64to32,
                                               mkexpr( bcd_top_8 ) ) ),
                        binop( Iop_Or32,
                               bcd_digit_inval( unop( Iop_64HIto32,
                                                     mkexpr( bcd_mid_60 ) ),
                                               unop( Iop_64to32,
                                                     mkexpr( bcd_mid_60 ) ) ),
                               bcd_digit_inval( unop( Iop_64HIto32,
                                                     mkexpr( frBI64_lo ) ),
                                               binop( Iop_Shr32,
                                                      unop( Iop_64to32,
                                                            mkexpr( frBI64_lo ) ),
                                                        mkU8( 4 ) ) ) ) ) );
         assign( invalid_mask,
                 Generate_inv_mask( mkexpr( inval_bcd_digit_mask ),
                                    mkexpr( pos_sign_mask ),
                                    mkexpr( neg_sign_mask ) ) );

      }

      assign( valid_mask, unop( Iop_Not32, mkexpr( invalid_mask ) ) );

      assign( dfp_significand,
              binop( Iop_D64HLtoD128,
                     unop( Iop_ReinterpI64asD64,
                           binop( Iop_32HLto64,
                                  binop( Iop_Or32,
                                         mkexpr( sign_bit ),
                                         mkexpr( dbcd_top_l ) ),
                                  binop( Iop_Or32,
                                         binop( Iop_Shl32,
                                                mkexpr( dbcd_mid_u ),
                                                mkU8( 18 ) ),
                                         binop( Iop_Shr32,
                                                mkexpr( dbcd_mid_l ),
                                                mkU8( 14 ) ) ) ) ),
                     unop( Iop_ReinterpI64asD64,
                           binop( Iop_32HLto64,
                                  binop( Iop_Or32,
                                         mkexpr( dbcd_low_u ),
                                         binop( Iop_Shl32,
                                                mkexpr( dbcd_mid_l ),
                                                mkU8( 18 ) ) ),
                                  mkexpr( dbcd_low_l ) ) ) ) );

      assign( result128,
              binop( Iop_InsertExpD128,
                     mkU64( DFP_EXTND_BIAS ),
                     mkexpr( dfp_significand ) ) );

      assign( tmp_hi,
              unop( Iop_ReinterpD64asI64,
                    unop( Iop_D128HItoD64, mkexpr( result128 ) ) ) );

      assign( tmp_lo,
              unop( Iop_ReinterpD64asI64,
                    unop( Iop_D128LOtoD64, mkexpr( result128 ) ) ) );

      assign( result_hi,
              binop( Iop_32HLto64,
                     binop( Iop_Or32,
                            binop( Iop_And32,
                                   mkexpr( valid_mask ),
                                   unop( Iop_64HIto32, mkexpr( tmp_hi ) ) ),
                            binop( Iop_And32,
                                   mkU32( 0x7C000000 ),
                                   mkexpr( invalid_mask ) ) ),
                     binop( Iop_Or32,
                            binop( Iop_And32,
                                   mkexpr( valid_mask ),
                                   unop( Iop_64to32, mkexpr( tmp_hi ) ) ),
                            binop( Iop_And32,
                                   mkU32( 0x0 ),
                                   mkexpr( invalid_mask ) ) ) ) );

      assign( result_lo,
              binop( Iop_32HLto64,
                     binop( Iop_Or32,
                            binop( Iop_And32,
                                   mkexpr( valid_mask ),
                                   unop( Iop_64HIto32, mkexpr( tmp_lo ) ) ),
                            binop( Iop_And32,
                                   mkU32( 0x0 ),
                                   mkexpr( invalid_mask ) ) ),
                     binop( Iop_Or32,
                            binop( Iop_And32,
                                   mkexpr( valid_mask ),
                                   unop( Iop_64to32, mkexpr( tmp_lo ) ) ),
                            binop( Iop_And32,
                                   mkU32( 0x0 ),
                                   mkexpr( invalid_mask ) ) ) ) );

      putDReg( frT_addr, unop( Iop_ReinterpI64asD64, mkexpr( result_hi ) ) );
      putDReg( frT_addr + 1,
               unop( Iop_ReinterpI64asD64, mkexpr( result_lo ) ) );

   }
   break;
   default:
      vpanic( "ERROR: dis_dfp_bcdq(ppc), undefined opc2 case " );
      break;
   }
   return True;
}

static Bool dis_dfp_significant_digits( UInt theInstr )
{
   UChar frA_addr = ifieldRegA( theInstr );
   UChar frB_addr = ifieldRegB( theInstr );
   IRTemp frA     = newTemp( Ity_D64 );
   UInt opc1      = ifieldOPC( theInstr );
   IRTemp B_sig   = newTemp( Ity_I8 );
   IRTemp K       = newTemp( Ity_I8 );
   IRTemp lmd_B   = newTemp( Ity_I32 );
   IRTemp field   = newTemp( Ity_I32 );
   UChar crfD     = toUChar( IFIELD( theInstr, 23, 3 ) ); 
   IRTemp Unordered_true     = newTemp( Ity_I32 );
   IRTemp Eq_true_mask       = newTemp( Ity_I32 );
   IRTemp Lt_true_mask       = newTemp( Ity_I32 );
   IRTemp Gt_true_mask       = newTemp( Ity_I32 );
   IRTemp KisZero_true_mask  = newTemp( Ity_I32 );
   IRTemp KisZero_false_mask = newTemp( Ity_I32 );

   
   assign( frA, getDReg( frA_addr ) );

   assign( K, unop( Iop_32to8,
                    binop( Iop_And32,
                           unop( Iop_64to32,
                                 unop( Iop_ReinterpD64asI64,
                                       mkexpr( frA ) ) ),
                           mkU32( 0x3F ) ) ) );

   switch ( opc1 ) {
   case 0x3b: 
   {
      IRTemp frB     = newTemp( Ity_D64 );
      IRTemp frBI64  = newTemp( Ity_I64 );
      IRTemp B_bcd_u = newTemp( Ity_I32 );
      IRTemp B_bcd_l = newTemp( Ity_I32 );
      IRTemp tmp64   = newTemp( Ity_I64 );

      DIP( "dtstsf %u,r%u,r%u\n", crfD, frA_addr, frB_addr );

      assign( frB, getDReg( frB_addr ) );
      assign( frBI64, unop( Iop_ReinterpD64asI64, mkexpr( frB ) ) );

      Get_lmd( &lmd_B,
               binop( Iop_Shr32,
                      unop( Iop_64HIto32, mkexpr( frBI64 ) ),
                      mkU8( 31 - 5 ) ) ); 

      assign( tmp64, unop( Iop_DPBtoBCD, mkexpr( frBI64 ) ) );
      assign( B_bcd_u, unop( Iop_64HIto32, mkexpr( tmp64 ) ) );
      assign( B_bcd_l, unop( Iop_64to32, mkexpr( tmp64 ) ) );

      assign( B_sig,
              binop( Iop_Sub8,
                     mkU8( DFP_LONG_MAX_SIG_DIGITS ),
                     Count_leading_zeros_60( mkexpr( lmd_B ),
                                             mkexpr( B_bcd_u ),
                                             mkexpr( B_bcd_l ) ) ) );
      assign( Unordered_true, Check_unordered( mkexpr( frBI64 ) ) );
   }
   break;
   case 0x3F: 
   {
      IRTemp frB_hi     = newTemp( Ity_D64 );
      IRTemp frB_lo     = newTemp( Ity_D64 );
      IRTemp frBI64_hi  = newTemp( Ity_I64 );
      IRTemp frBI64_lo  = newTemp( Ity_I64 );
      IRTemp B_low_60_u = newTemp( Ity_I32 );
      IRTemp B_low_60_l = newTemp( Ity_I32 );
      IRTemp B_mid_60_u = newTemp( Ity_I32 );
      IRTemp B_mid_60_l = newTemp( Ity_I32 );
      IRTemp B_top_12_l = newTemp( Ity_I32 );

      DIP( "dtstsfq %u,r%u,r%u\n", crfD, frA_addr, frB_addr );

      assign( frB_hi, getDReg( frB_addr ) );
      assign( frB_lo, getDReg( frB_addr + 1 ) );

      assign( frBI64_hi, unop( Iop_ReinterpD64asI64, mkexpr( frB_hi ) ) );
      assign( frBI64_lo, unop( Iop_ReinterpD64asI64, mkexpr( frB_lo ) ) );

      Get_lmd( &lmd_B,
               binop( Iop_Shr32,
                      unop( Iop_64HIto32, mkexpr( frBI64_hi ) ),
                      mkU8( 31 - 5 ) ) ); 

      Generate_132_bit_bcd_string( mkexpr( frBI64_hi ),
                                   mkexpr( frBI64_lo ),
                                   &B_top_12_l,
                                   &B_mid_60_u,
                                   &B_mid_60_l,
                                   &B_low_60_u,
                                   &B_low_60_l );

      assign( B_sig,
              binop( Iop_Sub8,
                     mkU8( DFP_EXTND_MAX_SIG_DIGITS ),
                     Count_leading_zeros_128( mkexpr( lmd_B ),
                                              mkexpr( B_top_12_l ),
                                              mkexpr( B_mid_60_u ),
                                              mkexpr( B_mid_60_l ),
                                              mkexpr( B_low_60_u ),
                                              mkexpr( B_low_60_l ) ) ) );

      assign( Unordered_true, Check_unordered( mkexpr( frBI64_hi ) ) );
   }
   break;
   }

   assign( Eq_true_mask,
           unop( Iop_1Sto32,
                 binop( Iop_CmpEQ32,
                        unop( Iop_8Uto32, mkexpr( K ) ),
                        unop( Iop_8Uto32, mkexpr( B_sig ) ) ) ) );
   assign( Lt_true_mask,
           unop( Iop_1Sto32,
                 binop( Iop_CmpLT32U,
                        unop( Iop_8Uto32, mkexpr( K ) ),
                        unop( Iop_8Uto32, mkexpr( B_sig ) ) ) ) );
   assign( Gt_true_mask,
           unop( Iop_1Sto32,
                 binop( Iop_CmpLT32U,
                        unop( Iop_8Uto32, mkexpr( B_sig ) ),
                        unop( Iop_8Uto32, mkexpr( K ) ) ) ) );

   assign( KisZero_true_mask,
           unop( Iop_1Sto32,
                 binop( Iop_CmpEQ32,
                        unop( Iop_8Uto32, mkexpr( K ) ),
                        mkU32( 0 ) ) ) );
   assign( KisZero_false_mask,
           unop( Iop_1Sto32,
                 binop( Iop_CmpNE32,
                        unop( Iop_8Uto32, mkexpr( K ) ),
                        mkU32( 0 ) ) ) );

   assign( field,
           binop( Iop_Or32,
                  binop( Iop_And32,
                         mkexpr( KisZero_false_mask ),
                         binop( Iop_Or32,
                                binop( Iop_And32,
                                       mkexpr( Lt_true_mask ),
                                       mkU32( 0x8 ) ),
                                binop( Iop_Or32,
                                       binop( Iop_And32,
                                              mkexpr( Gt_true_mask ),
                                              mkU32( 0x4 ) ),
                                       binop( Iop_And32,
                                              mkexpr( Eq_true_mask ),
                                              mkU32( 0x2 ) ) ) ) ),
                  binop( Iop_And32,
                         mkexpr( KisZero_true_mask ),
                         mkU32( 0x4 ) ) ) );

   putGST_field( PPC_GST_CR,
                 binop( Iop_Or32,
                        binop( Iop_And32,
                               mkexpr( Unordered_true ),
                               mkU32( 0x1 ) ),
                        binop( Iop_And32,
                               unop( Iop_Not32, mkexpr( Unordered_true ) ),
                               mkexpr( field ) ) ),
                 crfD );

   return True;
}


static Bool dis_av_datastream ( UInt theInstr )
{
   
   UChar opc1     = ifieldOPC(theInstr);
   UChar flag_T   = toUChar( IFIELD( theInstr, 25, 1 ) );
   UChar flag_A   = flag_T;
   UChar b23to24  = toUChar( IFIELD( theInstr, 23, 2 ) );
   UChar STRM     = toUChar( IFIELD( theInstr, 21, 2 ) );
   UChar rA_addr  = ifieldRegA(theInstr);
   UChar rB_addr  = ifieldRegB(theInstr);
   UInt  opc2     = ifieldOPClo10(theInstr);
   UChar b0       = ifieldBIT0(theInstr);

   if (opc1 != 0x1F || b23to24 != 0 || b0 != 0) {
      vex_printf("dis_av_datastream(ppc)(instr)\n");
      return False;
   }

   switch (opc2) {
   case 0x156: 
      DIP("dst%s r%u,r%u,%d\n", flag_T ? "t" : "",
                                rA_addr, rB_addr, STRM);
      break;

   case 0x176: 
      DIP("dstst%s r%u,r%u,%d\n", flag_T ? "t" : "",
                                  rA_addr, rB_addr, STRM);
      break;

   case 0x336: 
      if (rA_addr != 0 || rB_addr != 0) {
         vex_printf("dis_av_datastream(ppc)(opc2,dst)\n");
         return False;
      }
      if (flag_A == 0) {
         DIP("dss %d\n", STRM);
      } else {
         DIP("dssall\n");
      }
      break;

   default:
      vex_printf("dis_av_datastream(ppc)(opc2)\n");
      return False;
   }
   return True;
}

static Bool dis_av_procctl ( UInt theInstr )
{
   
   UChar opc1    = ifieldOPC(theInstr);
   UChar vD_addr = ifieldRegDS(theInstr);
   UChar vA_addr = ifieldRegA(theInstr);
   UChar vB_addr = ifieldRegB(theInstr);
   UInt  opc2    = IFIELD( theInstr, 0, 11 );

   if (opc1 != 0x4) {
      vex_printf("dis_av_procctl(ppc)(instr)\n");
      return False;
   }

   switch (opc2) {
   case 0x604: 
      if (vA_addr != 0 || vB_addr != 0) {
         vex_printf("dis_av_procctl(ppc)(opc2,dst)\n");
         return False;
      }
      DIP("mfvscr v%d\n", vD_addr);
      putVReg( vD_addr, unop(Iop_32UtoV128, getGST( PPC_GST_VSCR )) ); 
      break;

   case 0x644: { 
      IRTemp vB = newTemp(Ity_V128);
      if (vD_addr != 0 || vA_addr != 0) {
         vex_printf("dis_av_procctl(ppc)(opc2,dst)\n");
         return False;
      }
      DIP("mtvscr v%d\n", vB_addr);
      assign( vB, getVReg(vB_addr));
      putGST( PPC_GST_VSCR, unop(Iop_V128to32, mkexpr(vB)) ); 
      break;
   }
   default:
      vex_printf("dis_av_procctl(ppc)(opc2)\n");
      return False;
   }
   return True;
}

static Bool
dis_vx_conv ( UInt theInstr, UInt opc2 )
{
   
   UChar opc1 = ifieldOPC( theInstr );
   UChar XT = ifieldRegXT( theInstr );
   UChar XB = ifieldRegXB( theInstr );
   IRTemp xB, xB2;
   IRTemp b3, b2, b1, b0;
   xB = xB2 = IRTemp_INVALID;

   if (opc1 != 0x3C) {
      vex_printf( "dis_vx_conv(ppc)(instr)\n" );
      return False;
   }

   
   switch (opc2) {
      
      case 0x2B0: case 0x0b0: case 0x290: case 0x212: case 0x216: case 0x090:
         xB = newTemp(Ity_F64);
         assign( xB,
                 unop( Iop_ReinterpI64asF64,
                       unop( Iop_V128HIto64, getVSReg( XB ) ) ) );
         break;
      
      case 0x1b0: case 0x312: case 0x390: case 0x190: case 0x3B0:

         xB = newTemp(Ity_F64);
         xB2 = newTemp(Ity_F64);
         assign( xB,
                 unop( Iop_ReinterpI64asF64,
                       unop( Iop_V128HIto64, getVSReg( XB ) ) ) );
         assign( xB2,
                 unop( Iop_ReinterpI64asF64,
                       unop( Iop_V128to64, getVSReg( XB ) ) ) );
         break;
      
      case 0x130: case 0x392: case 0x330: case 0x310: case 0x110:
      case 0x1f0: case 0x1d0:
         b3 = b2 = b1 = b0 = IRTemp_INVALID;
         breakV128to4x32(getVSReg(XB), &b3, &b2, &b1, &b0);
         break;
         
      case 0x3f0: case 0x370: case 0x3d0: case 0x350:
         xB = newTemp(Ity_I64);
         assign( xB, unop( Iop_V128HIto64, getVSReg( XB ) ) );
         xB2 = newTemp(Ity_I64);
         assign( xB2, unop( Iop_V128to64, getVSReg( XB ) ) );
         break;
      
      case 0x250: case 0x270: case 0x2D0: case 0x2F0:
         xB = newTemp(Ity_I64);
         assign( xB, unop( Iop_V128HIto64, getVSReg( XB ) ) );
         break;
      
      case 0x292: 
         xB  = newTemp(Ity_I32);

         assign( xB, handle_SNaN_to_QNaN_32(unop( Iop_64HIto32,
                                                  unop( Iop_V128HIto64,
                                                        getVSReg( XB ) ) ) ) );
         break;
      case 0x296: 
         xB = newTemp(Ity_I32);
         assign( xB,
                 unop( Iop_64HIto32, unop( Iop_V128HIto64, getVSReg( XB ) ) ) );
         break;

      case 0x170: case 0x150:
         break; 

      default:
         vex_printf( "dis_vx_conv(ppc)(opc2)\n" );
         return False;
   }


   switch (opc2) {
      case 0x2B0:
         
         
         DIP("xscvdpsxds v%u,v%u\n",  (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128, binop( Iop_F64toI64S,
                                                 mkU32( Irrm_ZERO ),
                                                 mkexpr( xB ) ), mkU64( 0 ) ) );
         break;
      case 0x0b0: 
                  
         DIP("xscvdpsxws v%u,v%u\n",  (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          unop( Iop_32Sto64,
                                binop( Iop_F64toI32S,
                                       mkU32( Irrm_ZERO ),
                                       mkexpr( xB ) ) ),
                                       mkU64( 0ULL ) ) );
         break;
      case 0x290: 
                  
         DIP("xscvdpuxds v%u,v%u\n",  (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          binop( Iop_F64toI64U,
                                 mkU32( Irrm_ZERO ),
                                 mkexpr( xB ) ),
                                 mkU64( 0ULL ) ) );
         break;
      case 0x270:
         
         
         DIP("xscvsxdsp v%u,v%u\n",  (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          unop( Iop_ReinterpF64asI64,
                                binop( Iop_RoundF64toF32,
                                       get_IR_roundingmode(),
                                       binop( Iop_I64StoF64,
                                              get_IR_roundingmode(),
                                              mkexpr( xB ) ) ) ),
                          mkU64( 0 ) ) );
         break;
      case 0x2F0:
         
         
         DIP("xscvsxddp v%u,v%u\n",  (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128, unop( Iop_ReinterpF64asI64,
                                                binop( Iop_I64StoF64, get_IR_roundingmode(),
                                                       mkexpr( xB ) ) ),
                                                       mkU64( 0 ) ) );
         break;
      case 0x250:
         
         
         DIP("xscvuxdsp v%u,v%u\n",  (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          unop( Iop_ReinterpF64asI64,
                                binop( Iop_RoundF64toF32,
                                       get_IR_roundingmode(),
                                       binop( Iop_I64UtoF64,
                                              get_IR_roundingmode(),
                                              mkexpr( xB ) ) ) ),
                          mkU64( 0 ) ) );
         break;
      case 0x2D0:
         
         
         DIP("xscvuxddp v%u,v%u\n",  (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128, unop( Iop_ReinterpF64asI64,
                                                binop( Iop_I64UtoF64, get_IR_roundingmode(),
                                                       mkexpr( xB ) ) ),
                                                       mkU64( 0 ) ) );
         break;
      case 0x1b0: 
                  
      {
         IRTemp hiResult_32 = newTemp(Ity_I32);
         IRTemp loResult_32 = newTemp(Ity_I32);
         IRExpr* rmZero = mkU32(Irrm_ZERO);

         DIP("xvcvdpsxws v%u,v%u\n",  (UInt)XT, (UInt)XB);
         assign(hiResult_32, binop(Iop_F64toI32S, rmZero, mkexpr(xB)));
         assign(loResult_32, binop(Iop_F64toI32S, rmZero, mkexpr(xB2)));
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          unop( Iop_32Sto64, mkexpr( hiResult_32 ) ),
                          unop( Iop_32Sto64, mkexpr( loResult_32 ) ) ) );
         break;
      }
      case 0x130: case 0x110: 
         
         
      {
         IRExpr * b0_result, * b1_result, * b2_result, * b3_result;
         IRTemp tempResult = newTemp(Ity_V128);
         IRTemp res0 = newTemp(Ity_I32);
         IRTemp res1 = newTemp(Ity_I32);
         IRTemp res2 = newTemp(Ity_I32);
         IRTemp res3 = newTemp(Ity_I32);
         IRTemp hi64 = newTemp(Ity_I64);
         IRTemp lo64 = newTemp(Ity_I64);
         Bool un_signed = (opc2 == 0x110);
         IROp op = un_signed ? Iop_QFtoI32Ux4_RZ : Iop_QFtoI32Sx4_RZ;

         DIP("xvcvsp%sxws v%u,v%u\n", un_signed ? "u" : "s", (UInt)XT, (UInt)XB);
         assign(tempResult, unop(op, getVSReg(XB)));
         assign( hi64, unop(Iop_V128HIto64, mkexpr(tempResult)) );
         assign( lo64, unop(Iop_V128to64,   mkexpr(tempResult)) );
         assign( res3, unop(Iop_64HIto32, mkexpr(hi64)) );
         assign( res2, unop(Iop_64to32,   mkexpr(hi64)) );
         assign( res1, unop(Iop_64HIto32, mkexpr(lo64)) );
         assign( res0, unop(Iop_64to32,   mkexpr(lo64)) );

         b3_result = IRExpr_ITE(is_NaN_32(b3),
                                
                                mkU32(un_signed ? 0x00000000 : 0x80000000),
                                
                                mkexpr(res3));
         b2_result = IRExpr_ITE(is_NaN_32(b2),
                                
                                mkU32(un_signed ? 0x00000000 : 0x80000000),
                                
                                mkexpr(res2));
         b1_result = IRExpr_ITE(is_NaN_32(b1),
                                
                                mkU32(un_signed ? 0x00000000 : 0x80000000),
                                
                                mkexpr(res1));
         b0_result = IRExpr_ITE(is_NaN_32(b0),
                                
                                mkU32(un_signed ? 0x00000000 : 0x80000000),
                                
                                mkexpr(res0));

         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          binop( Iop_32HLto64, b3_result, b2_result ),
                          binop( Iop_32HLto64, b1_result, b0_result ) ) );
         break;
      }
      case 0x212: 
                  
         DIP("xscvdpsp v%u,v%u\n",  (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          binop( Iop_32HLto64,
                                 unop( Iop_ReinterpF32asI32,
                                       unop( Iop_TruncF64asF32,
                                             binop( Iop_RoundF64toF32,
                                                    get_IR_roundingmode(),
                                                    mkexpr( xB ) ) ) ),
                                 mkU32( 0 ) ),
                          mkU64( 0ULL ) ) );
         break;
      case 0x216: 
         DIP("xscvdpspn v%u,v%u\n",  (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          binop( Iop_32HLto64,
                                 unop( Iop_ReinterpF32asI32,
                                       unop( Iop_TruncF64asF32,
                                             mkexpr( xB ) ) ),
                                 mkU32( 0 ) ),
                          mkU64( 0ULL ) ) );
         break;
      case 0x090: 
                  
         DIP("xscvdpuxws v%u,v%u\n",  (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          binop( Iop_32HLto64,
                                 mkU32( 0 ),
                                 binop( Iop_F64toI32U,
                                        mkU32( Irrm_ZERO ),
                                        mkexpr( xB ) ) ),
                          mkU64( 0ULL ) ) );
         break;
      case 0x292: 
         DIP("xscvspdp v%u,v%u\n",  (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          unop( Iop_ReinterpF64asI64,
                                unop( Iop_F32toF64,
                                      unop( Iop_ReinterpI32asF32, mkexpr( xB ) ) ) ),
                          mkU64( 0ULL ) ) );
         break;
      case 0x296: 
         DIP("xscvspdpn v%u,v%u\n",  (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          unop( Iop_ReinterpF64asI64,
                                unop( Iop_F32toF64,
                                      unop( Iop_ReinterpI32asF32, mkexpr( xB ) ) ) ),
                                      mkU64( 0ULL ) ) );
         break;
      case 0x312: 
                  
         DIP("xvcvdpsp v%u,v%u\n",  (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          binop( Iop_32HLto64,
                                 unop( Iop_ReinterpF32asI32,
                                       unop( Iop_TruncF64asF32,
                                             binop( Iop_RoundF64toF32,
                                                    get_IR_roundingmode(),
                                                    mkexpr( xB ) ) ) ),
                                 mkU32( 0 ) ),
                          binop( Iop_32HLto64,
                                 unop( Iop_ReinterpF32asI32,
                                       unop( Iop_TruncF64asF32,
                                             binop( Iop_RoundF64toF32,
                                                    get_IR_roundingmode(),
                                                    mkexpr( xB2 ) ) ) ),
                                 mkU32( 0 ) ) ) );
         break;
      case 0x390: 
                  
                  
         DIP("xvcvdpuxds v%u,v%u\n",  (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          binop( Iop_F64toI64U, mkU32( Irrm_ZERO ), mkexpr( xB ) ),
                          binop( Iop_F64toI64U, mkU32( Irrm_ZERO ), mkexpr( xB2 ) ) ) );
         break;
      case 0x190: 
                  
         DIP("xvcvdpuxws v%u,v%u\n",  (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          binop( Iop_32HLto64,
                                 binop( Iop_F64toI32U,
                                        mkU32( Irrm_ZERO ),
                                        mkexpr( xB ) ),
                                 mkU32( 0 ) ),
                          binop( Iop_32HLto64,
                                 binop( Iop_F64toI32U,
                                        mkU32( Irrm_ZERO ),
                                        mkexpr( xB2 ) ),
                                 mkU32( 0 ) ) ) );
         break;
      case 0x392: 
         DIP("xvcvspdp v%u,v%u\n",  (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          unop( Iop_ReinterpF64asI64,
                                unop( Iop_F32toF64,
                                      unop( Iop_ReinterpI32asF32,
                                            handle_SNaN_to_QNaN_32( mkexpr( b3 ) ) ) ) ),
                          unop( Iop_ReinterpF64asI64,
                                unop( Iop_F32toF64,
                                      unop( Iop_ReinterpI32asF32,
                                            handle_SNaN_to_QNaN_32( mkexpr( b1 ) ) ) ) ) ) );
         break;
      case 0x330: 
                  
         DIP("xvcvspsxds v%u,v%u\n",  (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          binop( Iop_F64toI64S,
                                 mkU32( Irrm_ZERO ),
                                 unop( Iop_F32toF64,
                                       unop( Iop_ReinterpI32asF32, mkexpr( b3 ) ) ) ),
                          binop( Iop_F64toI64S,
                                 mkU32( Irrm_ZERO ),
                                 unop( Iop_F32toF64,
                                       unop( Iop_ReinterpI32asF32, mkexpr( b1 ) ) ) ) ) );
         break;
      case 0x310: 
                  
         DIP("xvcvspuxds v%u,v%u\n",  (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          binop( Iop_F64toI64U,
                                 mkU32( Irrm_ZERO ),
                                 unop( Iop_F32toF64,
                                       unop( Iop_ReinterpI32asF32, mkexpr( b3 ) ) ) ),
                          binop( Iop_F64toI64U,
                                 mkU32( Irrm_ZERO ),
                                 unop( Iop_F32toF64,
                                       unop( Iop_ReinterpI32asF32, mkexpr( b1 ) ) ) ) ) );
         break;
      case 0x3B0: 
                  
         DIP("xvcvdpsxds v%u,v%u\n",  (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          binop( Iop_F64toI64S, mkU32( Irrm_ZERO ), mkexpr( xB ) ),
                          binop( Iop_F64toI64S, mkU32( Irrm_ZERO ), mkexpr( xB2 ) ) ) );
         break;
      case 0x3f0: 
                  
         DIP("xvcvsxddp v%u,v%u\n",  (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          unop( Iop_ReinterpF64asI64,
                                binop( Iop_I64StoF64,
                                       get_IR_roundingmode(),
                                       mkexpr( xB ) ) ),
                          unop( Iop_ReinterpF64asI64,
                                binop( Iop_I64StoF64,
                                       get_IR_roundingmode(),
                                       mkexpr( xB2 ) ) ) ) );
         break;
      case 0x3d0: 
                  
         DIP("xvcvuxddp v%u,v%u\n",  (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          unop( Iop_ReinterpF64asI64,
                                binop( Iop_I64UtoF64,
                                       get_IR_roundingmode(),
                                       mkexpr( xB ) ) ),
                          unop( Iop_ReinterpF64asI64,
                                binop( Iop_I64UtoF64,
                                       get_IR_roundingmode(),
                                       mkexpr( xB2 ) ) ) ) );

         break;
      case 0x370: 
                  
         DIP("xvcvsxddp v%u,v%u\n",  (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          binop( Iop_32HLto64,
                                 unop( Iop_ReinterpF32asI32,
                                       unop( Iop_TruncF64asF32,
                                             binop( Iop_RoundF64toF32,
                                                    get_IR_roundingmode(),
                                                    binop( Iop_I64StoF64,
                                                           get_IR_roundingmode(),
                                                           mkexpr( xB ) ) ) ) ),
                                 mkU32( 0 ) ),
                          binop( Iop_32HLto64,
                                 unop( Iop_ReinterpF32asI32,
                                       unop( Iop_TruncF64asF32,
                                             binop( Iop_RoundF64toF32,
                                                    get_IR_roundingmode(),
                                                    binop( Iop_I64StoF64,
                                                           get_IR_roundingmode(),
                                                           mkexpr( xB2 ) ) ) ) ),
                                 mkU32( 0 ) ) ) );
         break;
      case 0x350: 
                  
         DIP("xvcvuxddp v%u,v%u\n", (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          binop( Iop_32HLto64,
                                 unop( Iop_ReinterpF32asI32,
                                       unop( Iop_TruncF64asF32,
                                             binop( Iop_RoundF64toF32,
                                                    get_IR_roundingmode(),
                                                    binop( Iop_I64UtoF64,
                                                           get_IR_roundingmode(),
                                                           mkexpr( xB ) ) ) ) ),
                                 mkU32( 0 ) ),
                          binop( Iop_32HLto64,
                                 unop( Iop_ReinterpF32asI32,
                                       unop( Iop_TruncF64asF32,
                                             binop( Iop_RoundF64toF32,
                                                    get_IR_roundingmode(),
                                                    binop( Iop_I64UtoF64,
                                                           get_IR_roundingmode(),
                                                           mkexpr( xB2 ) ) ) ) ),
                                 mkU32( 0 ) ) ) );
         break;

      case 0x1f0: 
         DIP("xvcvsxwdp v%u,v%u\n",  (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          unop( Iop_ReinterpF64asI64,
                                binop( Iop_I64StoF64, get_IR_roundingmode(),
                                       unop( Iop_32Sto64, mkexpr( b3 ) ) ) ),
                          unop( Iop_ReinterpF64asI64,
                                binop( Iop_I64StoF64, get_IR_roundingmode(),
                                       unop( Iop_32Sto64, mkexpr( b1 ) ) ) ) ) );
         break;
      case 0x1d0: 
         DIP("xvcvuxwdp v%u,v%u\n",  (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          unop( Iop_ReinterpF64asI64,
                                binop( Iop_I64UtoF64, get_IR_roundingmode(),
                                       unop( Iop_32Uto64, mkexpr( b3 ) ) ) ),
                          unop( Iop_ReinterpF64asI64,
                                binop( Iop_I64UtoF64, get_IR_roundingmode(),
                                       unop( Iop_32Uto64, mkexpr( b1 ) ) ) ) ) );
         break;
      case 0x170: 
         DIP("xvcvsxwsp v%u,v%u\n",  (UInt)XT, (UInt)XB);
         putVSReg( XT, unop( Iop_I32StoFx4, getVSReg( XB ) ) );
         break;
      case 0x150: 
         DIP("xvcvuxwsp v%u,v%u\n",  (UInt)XT, (UInt)XB);
         putVSReg( XT, unop( Iop_I32UtoFx4, getVSReg( XB ) ) );
         break;

      default:
         vex_printf( "dis_vx_conv(ppc)(opc2)\n" );
         return False;
   }
   return True;
}

static Bool
dis_vxv_dp_arith ( UInt theInstr, UInt opc2 )
{
   
   UChar opc1 = ifieldOPC( theInstr );
   UChar XT = ifieldRegXT( theInstr );
   UChar XA = ifieldRegXA( theInstr );
   UChar XB = ifieldRegXB( theInstr );
   IRExpr* rm = get_IR_roundingmode();
   IRTemp frA = newTemp(Ity_F64);
   IRTemp frB = newTemp(Ity_F64);
   IRTemp frA2 = newTemp(Ity_F64);
   IRTemp frB2 = newTemp(Ity_F64);

   if (opc1 != 0x3C) {
      vex_printf( "dis_vxv_dp_arith(ppc)(instr)\n" );
      return False;
   }

   assign(frA,  unop(Iop_ReinterpI64asF64, unop(Iop_V128HIto64, getVSReg( XA ))));
   assign(frB,  unop(Iop_ReinterpI64asF64, unop(Iop_V128HIto64, getVSReg( XB ))));
   assign(frA2, unop(Iop_ReinterpI64asF64, unop(Iop_V128to64, getVSReg( XA ))));
   assign(frB2, unop(Iop_ReinterpI64asF64, unop(Iop_V128to64, getVSReg( XB ))));

   switch (opc2) {
      case 0x1E0: 
      case 0x1C0: 
      case 0x180: 
      case 0x1A0: 
      {
         IROp mOp;
         const HChar * oper_name;
         switch (opc2) {
            case 0x1E0:
               mOp = Iop_DivF64;
               oper_name = "div";
               break;
            case 0x1C0:
               mOp = Iop_MulF64;
               oper_name = "mul";
               break;
            case 0x180:
               mOp = Iop_AddF64;
               oper_name = "add";
               break;
            case 0x1A0:
               mOp = Iop_SubF64;
               oper_name = "sub";
               break;

            default:
               vpanic("The impossible happened: dis_vxv_dp_arith(ppc)");
         }
         IRTemp hiResult = newTemp(Ity_I64);
         IRTemp loResult = newTemp(Ity_I64);
         DIP("xv%sdp v%d,v%d,v%d\n", oper_name, (UInt)XT, (UInt)XA, (UInt)XB);

         assign( hiResult,
                 unop( Iop_ReinterpF64asI64,
                       triop( mOp, rm, mkexpr( frA ), mkexpr( frB ) ) ) );
         assign( loResult,
                 unop( Iop_ReinterpF64asI64,
                       triop( mOp, rm, mkexpr( frA2 ), mkexpr( frB2 ) ) ) );
         putVSReg( XT,
                   binop( Iop_64HLtoV128, mkexpr( hiResult ), mkexpr( loResult ) ) );
         break;
      }
      case 0x196: 
      {
         IRTemp hiResult = newTemp(Ity_I64);
         IRTemp loResult = newTemp(Ity_I64);
         DIP("xvsqrtdp v%d,v%d\n", (UInt)XT, (UInt)XB);

         assign( hiResult,
                 unop( Iop_ReinterpF64asI64,
                       binop( Iop_SqrtF64, rm, mkexpr( frB ) ) ) );
         assign( loResult,
                 unop( Iop_ReinterpF64asI64,
                       binop( Iop_SqrtF64, rm, mkexpr( frB2 ) ) ) );
         putVSReg( XT,
                   binop( Iop_64HLtoV128, mkexpr( hiResult ), mkexpr( loResult ) ) );
         break;
      }
      case 0x184: case 0x1A4: 
      case 0x1C4: case 0x1E4: 
      case 0x384: case 0x3A4: 
      case 0x3C4: case 0x3E4: 
      {
         Bool negate;
         IROp mOp = Iop_INVALID;
         const HChar * oper_name = NULL;
         Bool mdp = False;

         switch (opc2) {
            case 0x184: case 0x1A4:
            case 0x384: case 0x3A4:
               mOp = Iop_MAddF64;
               oper_name = "add";
               mdp = (opc2 & 0x0FF) == 0x0A4;
               break;

            case 0x1C4: case 0x1E4:
            case 0x3C4: case 0x3E4:
               mOp = Iop_MSubF64;
               oper_name = "sub";
               mdp = (opc2 & 0x0FF) == 0x0E4;
               break;

            default:
               vpanic("The impossible happened: dis_vxv_sp_arith(ppc)");
         }

         switch (opc2) {
            case 0x384: case 0x3A4:
            case 0x3C4: case 0x3E4:
               negate = True;
               break;
            default:
               negate = False;
         }
         IRTemp hiResult = newTemp(Ity_I64);
         IRTemp loResult = newTemp(Ity_I64);
         IRTemp frT = newTemp(Ity_F64);
         IRTemp frT2 = newTemp(Ity_F64);
         DIP("xv%sm%s%s v%d,v%d,v%d\n", negate ? "n" : "", oper_name, mdp ? "mdp" : "adp",
             (UInt)XT, (UInt)XA, (UInt)XB);
         assign(frT,  unop(Iop_ReinterpI64asF64, unop(Iop_V128HIto64, getVSReg( XT ) ) ) );
         assign(frT2, unop(Iop_ReinterpI64asF64, unop(Iop_V128to64, getVSReg( XT ) ) ) );

         assign( hiResult,
                 unop( Iop_ReinterpF64asI64,
                       qop( mOp,
                            rm,
                            mkexpr( frA ),
                            mkexpr( mdp ? frT : frB ),
                            mkexpr( mdp ? frB : frT ) ) ) );
         assign( loResult,
                 unop( Iop_ReinterpF64asI64,
                       qop( mOp,
                            rm,
                            mkexpr( frA2 ),
                            mkexpr( mdp ? frT2 : frB2 ),
                            mkexpr( mdp ? frB2 : frT2 ) ) ) );
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          mkexpr( negate ? getNegatedResult( hiResult )
                                         : hiResult ),
                          mkexpr( negate ? getNegatedResult( loResult )
                                         : loResult ) ) );
         break;
      }
      case 0x1D4: 
      {
         IRTemp frBHi_I64 = newTemp(Ity_I64);
         IRTemp frBLo_I64 = newTemp(Ity_I64);
         IRTemp flagsHi = newTemp(Ity_I32);
         IRTemp flagsLo = newTemp(Ity_I32);
         UChar crfD     = toUChar( IFIELD( theInstr, 23, 3 ) );
         IRTemp  fe_flagHi, fg_flagHi, fe_flagLo, fg_flagLo;
         fe_flagHi = fg_flagHi = fe_flagLo = fg_flagLo = IRTemp_INVALID;

         DIP("xvtsqrtdp cr%d,v%d\n", (UInt)crfD, (UInt)XB);
         assign( frBHi_I64, unop(Iop_V128HIto64, getVSReg( XB )) );
         assign( frBLo_I64, unop(Iop_V128to64, getVSReg( XB )) );
         do_fp_tsqrt(frBHi_I64, False , &fe_flagHi, &fg_flagHi);
         do_fp_tsqrt(frBLo_I64, False , &fe_flagLo, &fg_flagLo);
         assign( flagsHi,
                 binop( Iop_Or32,
                        binop( Iop_Or32, mkU32( 8 ), 
                               binop( Iop_Shl32, mkexpr(fg_flagHi), mkU8( 2 ) ) ),
                        binop( Iop_Shl32, mkexpr(fe_flagHi), mkU8( 1 ) ) ) );
         assign( flagsLo,
                 binop( Iop_Or32,
                        binop( Iop_Or32, mkU32( 8 ), 
                               binop( Iop_Shl32, mkexpr(fg_flagLo), mkU8( 2 ) ) ),
                        binop( Iop_Shl32, mkexpr(fe_flagLo), mkU8( 1 ) ) ) );
         putGST_field( PPC_GST_CR,
                       binop( Iop_Or32, mkexpr( flagsHi ), mkexpr( flagsLo ) ),
                       crfD );
         break;
      }
      case 0x1F4: 
      {
         IRTemp frBHi_I64 = newTemp(Ity_I64);
         IRTemp frBLo_I64 = newTemp(Ity_I64);
         IRTemp frAHi_I64 = newTemp(Ity_I64);
         IRTemp frALo_I64 = newTemp(Ity_I64);
         IRTemp flagsHi = newTemp(Ity_I32);
         IRTemp flagsLo = newTemp(Ity_I32);
         UChar crfD     = toUChar( IFIELD( theInstr, 23, 3 ) );
         IRTemp  fe_flagHi, fg_flagHi, fe_flagLo, fg_flagLo;
         fe_flagHi = fg_flagHi = fe_flagLo = fg_flagLo = IRTemp_INVALID;

         DIP("xvtdivdp cr%d,v%d,v%d\n", (UInt)crfD, (UInt)XA, (UInt)XB);
         assign( frAHi_I64, unop(Iop_V128HIto64, getVSReg( XA )) );
         assign( frALo_I64, unop(Iop_V128to64, getVSReg( XA )) );
         assign( frBHi_I64, unop(Iop_V128HIto64, getVSReg( XB )) );
         assign( frBLo_I64, unop(Iop_V128to64, getVSReg( XB )) );

         _do_fp_tdiv(frAHi_I64, frBHi_I64, False, &fe_flagHi, &fg_flagHi);
         _do_fp_tdiv(frALo_I64, frBLo_I64, False, &fe_flagLo, &fg_flagLo);
         assign( flagsHi,
                 binop( Iop_Or32,
                        binop( Iop_Or32, mkU32( 8 ), 
                               binop( Iop_Shl32, mkexpr(fg_flagHi), mkU8( 2 ) ) ),
                        binop( Iop_Shl32, mkexpr(fe_flagHi), mkU8( 1 ) ) ) );
         assign( flagsLo,
                 binop( Iop_Or32,
                        binop( Iop_Or32, mkU32( 8 ), 
                               binop( Iop_Shl32, mkexpr(fg_flagLo), mkU8( 2 ) ) ),
                        binop( Iop_Shl32, mkexpr(fe_flagLo), mkU8( 1 ) ) ) );
         putGST_field( PPC_GST_CR,
                       binop( Iop_Or32, mkexpr( flagsHi ), mkexpr( flagsLo ) ),
                       crfD );
         break;
      }

      default:
         vex_printf( "dis_vxv_dp_arith(ppc)(opc2)\n" );
         return False;
   }
   return True;
}

static Bool
dis_vxv_sp_arith ( UInt theInstr, UInt opc2 )
{
   
   UChar opc1 = ifieldOPC( theInstr );
   UChar XT = ifieldRegXT( theInstr );
   UChar XA = ifieldRegXA( theInstr );
   UChar XB = ifieldRegXB( theInstr );
   IRExpr* rm = get_IR_roundingmode();
   IRTemp a3, a2, a1, a0;
   IRTemp b3, b2, b1, b0;
   IRTemp res0 = newTemp(Ity_I32);
   IRTemp res1 = newTemp(Ity_I32);
   IRTemp res2 = newTemp(Ity_I32);
   IRTemp res3 = newTemp(Ity_I32);

   a3 = a2 = a1 = a0 = IRTemp_INVALID;
   b3 = b2 = b1 = b0 = IRTemp_INVALID;

   if (opc1 != 0x3C) {
      vex_printf( "dis_vxv_sp_arith(ppc)(instr)\n" );
      return False;
   }

   switch (opc2) {
      case 0x100: 
         DIP("xvaddsp v%d,v%d,v%d\n", (UInt)XT, (UInt)XA, (UInt)XB);
         
         putVSReg( XT, triop(Iop_Add32Fx4, rm,
                             getVSReg( XA ), getVSReg( XB )) );
         break;

      case 0x140: 
         DIP("xvmulsp v%d,v%d,v%d\n", (UInt)XT, (UInt)XA, (UInt)XB);
         
         putVSReg( XT, triop(Iop_Mul32Fx4, rm,
                             getVSReg( XA ), getVSReg( XB )) );
         break;

      case 0x120: 
         DIP("xvsubsp v%d,v%d,v%d\n", (UInt)XT, (UInt)XA, (UInt)XB);
         
         putVSReg( XT, triop(Iop_Sub32Fx4, rm,
                             getVSReg( XA ), getVSReg( XB )) );
         break;

      case 0x160: 
      {
         DIP("xvdivsp v%d,v%d,v%d\n", (UInt)XT, (UInt)XA, (UInt)XB);
         breakV128to4xF64( getVSReg( XA ), &a3, &a2, &a1, &a0 );
         breakV128to4xF64( getVSReg( XB ), &b3, &b2, &b1, &b0 );

         assign( res0,
              unop( Iop_ReinterpF32asI32,
                    unop( Iop_TruncF64asF32,
                          triop( Iop_DivF64r32, rm, mkexpr( a0 ), mkexpr( b0 ) ) ) ) );
         assign( res1,
                 unop( Iop_ReinterpF32asI32,
                       unop( Iop_TruncF64asF32,
                             triop( Iop_DivF64r32, rm, mkexpr( a1 ), mkexpr( b1 ) ) ) ) );
         assign( res2,
                 unop( Iop_ReinterpF32asI32,
                       unop( Iop_TruncF64asF32,
                             triop( Iop_DivF64r32, rm, mkexpr( a2 ), mkexpr( b2 ) ) ) ) );
         assign( res3,
                 unop( Iop_ReinterpF32asI32,
                       unop( Iop_TruncF64asF32,
                             triop( Iop_DivF64r32, rm, mkexpr( a3 ), mkexpr( b3 ) ) ) ) );

         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          binop( Iop_32HLto64, mkexpr( res3 ), mkexpr( res2 ) ),
                          binop( Iop_32HLto64, mkexpr( res1 ), mkexpr( res0 ) ) ) );
         break;
      }
      case 0x116: 
      {
         DIP("xvsqrtsp v%d,v%d\n", (UInt)XT, (UInt)XB);
         breakV128to4xF64( getVSReg( XB ), &b3, &b2, &b1, &b0 );

         assign( res0,
                 unop( Iop_ReinterpF32asI32,
                       unop( Iop_TruncF64asF32,
                             binop(Iop_SqrtF64, rm, mkexpr( b0 ) ) ) ) );
         assign( res1,
                 unop( Iop_ReinterpF32asI32,
                       unop( Iop_TruncF64asF32,
                             binop(Iop_SqrtF64, rm, mkexpr( b1 ) ) ) ) );
         assign( res2,
                 unop( Iop_ReinterpF32asI32,
                       unop( Iop_TruncF64asF32,
                             binop(Iop_SqrtF64, rm, mkexpr( b2) ) ) ) );
         assign( res3,
                 unop( Iop_ReinterpF32asI32,
                       unop( Iop_TruncF64asF32,
                             binop(Iop_SqrtF64, rm, mkexpr( b3 ) ) ) ) );

         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          binop( Iop_32HLto64, mkexpr( res3 ), mkexpr( res2 ) ),
                          binop( Iop_32HLto64, mkexpr( res1 ), mkexpr( res0 ) ) ) );
         break;
      }

      case 0x104: case 0x124: 
      case 0x144: case 0x164: 
      case 0x304: case 0x324: 
      case 0x344: case 0x364: 
      {
         IRTemp t3, t2, t1, t0;
         Bool msp = False;
         Bool negate;
         const HChar * oper_name = NULL;
         IROp mOp = Iop_INVALID;
         switch (opc2) {
            case 0x104: case 0x124:
            case 0x304: case 0x324:
               msp = (opc2 & 0x0FF) == 0x024;
               mOp = Iop_MAddF64r32;
               oper_name = "madd";
               break;

            case 0x144: case 0x164:
            case 0x344: case 0x364:
               msp = (opc2 & 0x0FF) == 0x064;
               mOp = Iop_MSubF64r32;
               oper_name = "sub";
               break;

            default:
               vpanic("The impossible happened: dis_vxv_sp_arith(ppc)");
         }

         switch (opc2) {
            case 0x304: case 0x324:
            case 0x344: case 0x364:
               negate = True;
               break;

            default:
               negate = False;
         }

         DIP("xv%sm%s%s v%d,v%d,v%d\n", negate ? "n" : "", oper_name, msp ? "msp" : "asp",
             (UInt)XT, (UInt)XA, (UInt)XB);

         t3 = t2 = t1 = t0 = IRTemp_INVALID;
         breakV128to4xF64( getVSReg( XA ), &a3, &a2, &a1, &a0 );
         breakV128to4xF64( getVSReg( XB ), &b3, &b2, &b1, &b0 );
         breakV128to4xF64( getVSReg( XT ), &t3, &t2, &t1, &t0 );

         assign( res0,
                 unop( Iop_ReinterpF32asI32,
                       unop( Iop_TruncF64asF32,
                             qop( mOp,
                                  rm,
                                  mkexpr( a0 ),
                                  mkexpr( msp ? t0 : b0 ),
                                  mkexpr( msp ? b0 : t0 ) ) ) ) );
         assign( res1,
                 unop( Iop_ReinterpF32asI32,
                       unop( Iop_TruncF64asF32,
                             qop( mOp,
                                  rm,
                                  mkexpr( a1 ),
                                  mkexpr( msp ? t1 : b1 ),
                                  mkexpr( msp ? b1 : t1 ) ) ) ) );
         assign( res2,
                 unop( Iop_ReinterpF32asI32,
                       unop( Iop_TruncF64asF32,
                             qop( mOp,
                                  rm,
                                  mkexpr( a2 ),
                                  mkexpr( msp ? t2 : b2 ),
                                  mkexpr( msp ? b2 : t2 ) ) ) ) );
         assign( res3,
                 unop( Iop_ReinterpF32asI32,
                       unop( Iop_TruncF64asF32,
                             qop( mOp,
                                  rm,
                                  mkexpr( a3 ),
                                  mkexpr( msp ? t3 : b3 ),
                                  mkexpr( msp ? b3 : t3 ) ) ) ) );

         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          binop( Iop_32HLto64, mkexpr( negate ? getNegatedResult_32( res3 ) : res3 ),
                                 mkexpr( negate ? getNegatedResult_32( res2 ) : res2 ) ),
                          binop( Iop_32HLto64, mkexpr( negate ? getNegatedResult_32( res1 ) : res1 ),
                                 mkexpr( negate ? getNegatedResult_32( res0 ) : res0 ) ) ) );

         break;
      }
      case 0x154: 
      {
         IRTemp flags0 = newTemp(Ity_I32);
         IRTemp flags1 = newTemp(Ity_I32);
         IRTemp flags2 = newTemp(Ity_I32);
         IRTemp flags3 = newTemp(Ity_I32);
         UChar crfD     = toUChar( IFIELD( theInstr, 23, 3 ) );
         IRTemp  fe_flag0, fg_flag0, fe_flag1, fg_flag1;
         IRTemp  fe_flag2, fg_flag2, fe_flag3, fg_flag3;
         fe_flag0 = fg_flag0 = fe_flag1 = fg_flag1 = IRTemp_INVALID;
         fe_flag2 = fg_flag2 = fe_flag3 = fg_flag3 = IRTemp_INVALID;
         DIP("xvtsqrtsp cr%d,v%d\n", (UInt)crfD, (UInt)XB);

         breakV128to4x32( getVSReg( XB ), &b3, &b2, &b1, &b0 );
         do_fp_tsqrt(b0, True , &fe_flag0, &fg_flag0);
         do_fp_tsqrt(b1, True , &fe_flag1, &fg_flag1);
         do_fp_tsqrt(b2, True , &fe_flag2, &fg_flag2);
         do_fp_tsqrt(b3, True , &fe_flag3, &fg_flag3);

         assign( flags0,
                 binop( Iop_Or32,
                        binop( Iop_Or32, mkU32( 8 ), 
                               binop( Iop_Shl32, mkexpr(fg_flag0), mkU8( 2 ) ) ),
                        binop( Iop_Shl32, mkexpr(fe_flag0), mkU8( 1 ) ) ) );
         assign( flags1,
                 binop( Iop_Or32,
                        binop( Iop_Or32, mkU32( 8 ), 
                               binop( Iop_Shl32, mkexpr(fg_flag1), mkU8( 2 ) ) ),
                        binop( Iop_Shl32, mkexpr(fe_flag1), mkU8( 1 ) ) ) );
         assign( flags2,
                 binop( Iop_Or32,
                        binop( Iop_Or32, mkU32( 8 ), 
                               binop( Iop_Shl32, mkexpr(fg_flag2), mkU8( 2 ) ) ),
                        binop( Iop_Shl32, mkexpr(fe_flag2), mkU8( 1 ) ) ) );
         assign( flags3,
                 binop( Iop_Or32,
                        binop( Iop_Or32, mkU32( 8 ), 
                               binop( Iop_Shl32, mkexpr(fg_flag3), mkU8( 2 ) ) ),
                        binop( Iop_Shl32, mkexpr(fe_flag3), mkU8( 1 ) ) ) );
         putGST_field( PPC_GST_CR,
                       binop( Iop_Or32,
                              mkexpr( flags0 ),
                              binop( Iop_Or32,
                                     mkexpr( flags1 ),
                                     binop( Iop_Or32,
                                            mkexpr( flags2 ),
                                            mkexpr( flags3 ) ) ) ),
                       crfD );

         break;
      }
      case 0x174: 
      {
         IRTemp flags0 = newTemp(Ity_I32);
         IRTemp flags1 = newTemp(Ity_I32);
         IRTemp flags2 = newTemp(Ity_I32);
         IRTemp flags3 = newTemp(Ity_I32);
         UChar crfD     = toUChar( IFIELD( theInstr, 23, 3 ) );
         IRTemp  fe_flag0, fg_flag0, fe_flag1, fg_flag1;
         IRTemp  fe_flag2, fg_flag2, fe_flag3, fg_flag3;
         fe_flag0 = fg_flag0 = fe_flag1 = fg_flag1 = IRTemp_INVALID;
         fe_flag2 = fg_flag2 = fe_flag3 = fg_flag3 = IRTemp_INVALID;
         DIP("xvtdivsp cr%d,v%d,v%d\n", (UInt)crfD, (UInt)XA, (UInt)XB);

         breakV128to4x32( getVSReg( XA ), &a3, &a2, &a1, &a0 );
         breakV128to4x32( getVSReg( XB ), &b3, &b2, &b1, &b0 );
         _do_fp_tdiv(a0, b0, True , &fe_flag0, &fg_flag0);
         _do_fp_tdiv(a1, b1, True , &fe_flag1, &fg_flag1);
         _do_fp_tdiv(a2, b2, True , &fe_flag2, &fg_flag2);
         _do_fp_tdiv(a3, b3, True , &fe_flag3, &fg_flag3);

         assign( flags0,
                 binop( Iop_Or32,
                        binop( Iop_Or32, mkU32( 8 ), 
                               binop( Iop_Shl32, mkexpr(fg_flag0), mkU8( 2 ) ) ),
                        binop( Iop_Shl32, mkexpr(fe_flag0), mkU8( 1 ) ) ) );
         assign( flags1,
                 binop( Iop_Or32,
                        binop( Iop_Or32, mkU32( 8 ), 
                               binop( Iop_Shl32, mkexpr(fg_flag1), mkU8( 2 ) ) ),
                        binop( Iop_Shl32, mkexpr(fe_flag1), mkU8( 1 ) ) ) );
         assign( flags2,
                 binop( Iop_Or32,
                        binop( Iop_Or32, mkU32( 8 ), 
                               binop( Iop_Shl32, mkexpr(fg_flag2), mkU8( 2 ) ) ),
                        binop( Iop_Shl32, mkexpr(fe_flag2), mkU8( 1 ) ) ) );
         assign( flags3,
                 binop( Iop_Or32,
                        binop( Iop_Or32, mkU32( 8 ), 
                               binop( Iop_Shl32, mkexpr(fg_flag3), mkU8( 2 ) ) ),
                        binop( Iop_Shl32, mkexpr(fe_flag3), mkU8( 1 ) ) ) );
         putGST_field( PPC_GST_CR,
                       binop( Iop_Or32,
                              mkexpr( flags0 ),
                              binop( Iop_Or32,
                                     mkexpr( flags1 ),
                                     binop( Iop_Or32,
                                            mkexpr( flags2 ),
                                            mkexpr( flags3 ) ) ) ),
                       crfD );

         break;
      }

      default:
         vex_printf( "dis_vxv_sp_arith(ppc)(opc2)\n" );
         return False;
   }
   return True;
}

static Bool
dis_av_count_bitTranspose ( UInt theInstr, UInt opc2 )
{
   UChar vRB_addr = ifieldRegB(theInstr);
   UChar vRT_addr = ifieldRegDS(theInstr);
   UChar opc1 = ifieldOPC( theInstr );
   IRTemp vB = newTemp(Ity_V128);
   assign( vB, getVReg(vRB_addr));

   if (opc1 != 0x4) {
      vex_printf( "dis_av_count_bitTranspose(ppc)(instr)\n" );
      return False;
   }

   switch (opc2) {
      case 0x702:    
         DIP("vclzb v%d,v%d\n", vRT_addr, vRB_addr);
         putVReg( vRT_addr, unop(Iop_Clz8x16, mkexpr( vB ) ) );
         break;

      case 0x742:    
         DIP("vclzh v%d,v%d\n", vRT_addr, vRB_addr);
         putVReg( vRT_addr, unop(Iop_Clz16x8, mkexpr( vB ) ) );
         break;

      case 0x782:    
         DIP("vclzw v%d,v%d\n", vRT_addr, vRB_addr);
         putVReg( vRT_addr, unop(Iop_Clz32x4, mkexpr( vB ) ) );
         break;

      case 0x7C2:    
         DIP("vclzd v%d,v%d\n", vRT_addr, vRB_addr);
         putVReg( vRT_addr, unop(Iop_Clz64x2, mkexpr( vB ) ) );
         break;

      case 0x703:    
      {
         IRType ty = Ity_I32;
         IRTemp bits0_31, bits32_63, bits64_95, bits96_127;
         bits0_31 = bits32_63 = bits64_95 = bits96_127 = IRTemp_INVALID;
         IRTemp cnt_bits0_31, cnt_bits32_63, cnt_bits64_95, cnt_bits96_127;
         cnt_bits0_31 = cnt_bits32_63 = cnt_bits64_95 = cnt_bits96_127 = IRTemp_INVALID;

         DIP("vpopcntb v%d,v%d\n", vRT_addr, vRB_addr);
         breakV128to4x32(mkexpr( vB), &bits96_127, &bits64_95, &bits32_63, &bits0_31 );
         cnt_bits0_31   = gen_POPCOUNT(ty, bits0_31,   BYTE);
         cnt_bits32_63  = gen_POPCOUNT(ty, bits32_63,  BYTE);
         cnt_bits64_95  = gen_POPCOUNT(ty, bits64_95,  BYTE);
         cnt_bits96_127 = gen_POPCOUNT(ty, bits96_127, BYTE);

         putVReg( vRT_addr, mkV128from32(cnt_bits96_127, cnt_bits64_95,
                                         cnt_bits32_63, cnt_bits0_31) );
         break;
      }

      case 0x743:    
      {
         IRType ty = Ity_I32;
         IRTemp bits0_31, bits32_63, bits64_95, bits96_127;
         bits0_31 = bits32_63 = bits64_95 = bits96_127 = IRTemp_INVALID;
         IRTemp cnt_bits0_31, cnt_bits32_63, cnt_bits64_95, cnt_bits96_127;
         cnt_bits0_31 = cnt_bits32_63 = cnt_bits64_95 = cnt_bits96_127 = IRTemp_INVALID;

         DIP("vpopcnth v%d,v%d\n", vRT_addr, vRB_addr);
         breakV128to4x32(mkexpr( vB), &bits96_127, &bits64_95, &bits32_63, &bits0_31 );

         cnt_bits0_31   = gen_POPCOUNT(ty, bits0_31,   HWORD);
         cnt_bits32_63  = gen_POPCOUNT(ty, bits32_63,  HWORD);
         cnt_bits64_95  = gen_POPCOUNT(ty, bits64_95,  HWORD);
         cnt_bits96_127 = gen_POPCOUNT(ty, bits96_127, HWORD);

         putVReg( vRT_addr, mkV128from32(cnt_bits96_127, cnt_bits64_95,
                                         cnt_bits32_63, cnt_bits0_31) );
         break;
      }

      case 0x783:    
      {
         IRType ty = Ity_I32;
         IRTemp bits0_31, bits32_63, bits64_95, bits96_127;
         bits0_31 = bits32_63 = bits64_95 = bits96_127 = IRTemp_INVALID;
         IRTemp cnt_bits0_31, cnt_bits32_63, cnt_bits64_95, cnt_bits96_127;
         cnt_bits0_31 = cnt_bits32_63 = cnt_bits64_95 = cnt_bits96_127 = IRTemp_INVALID;

         DIP("vpopcntw v%d,v%d\n", vRT_addr, vRB_addr);
         breakV128to4x32(mkexpr( vB), &bits96_127, &bits64_95, &bits32_63, &bits0_31 );

         cnt_bits0_31   = gen_POPCOUNT(ty, bits0_31,   WORD);
         cnt_bits32_63  = gen_POPCOUNT(ty, bits32_63,  WORD);
         cnt_bits64_95  = gen_POPCOUNT(ty, bits64_95,  WORD);
         cnt_bits96_127 = gen_POPCOUNT(ty, bits96_127, WORD);

         putVReg( vRT_addr, mkV128from32(cnt_bits96_127, cnt_bits64_95,
                                         cnt_bits32_63, cnt_bits0_31) );
         break;
      }

      case 0x7C3:    
      {
         if (mode64) {
            IRType ty = Ity_I64;
            IRTemp bits0_63   = newTemp(Ity_I64);
            IRTemp bits64_127 = newTemp(Ity_I64);
            IRTemp cnt_bits0_63   = newTemp(Ity_I64);
            IRTemp cnt_bits64_127 = newTemp(Ity_I64);

            DIP("vpopcntd v%d,v%d\n", vRT_addr, vRB_addr);

            assign(bits0_63,   unop( Iop_V128to64,   mkexpr( vB ) ) );
            assign(bits64_127, unop( Iop_V128HIto64, mkexpr( vB ) ) );
            cnt_bits0_63   = gen_POPCOUNT(ty, bits0_63,   DWORD);
            cnt_bits64_127 = gen_POPCOUNT(ty, bits64_127, DWORD);

            putVReg( vRT_addr, binop( Iop_64HLtoV128,
                                      mkexpr( cnt_bits64_127 ),
                                      mkexpr( cnt_bits0_63 ) ) );
         } else {
            IRTemp bits0_31, bits32_63, bits64_95, bits96_127;
            bits0_31 = bits32_63 = bits64_95 = bits96_127 = IRTemp_INVALID;
            IRTemp cnt_bits0_63   = newTemp(Ity_I64);
            IRTemp cnt_bits64_127  = newTemp(Ity_I64);

            DIP("vpopcntd v%d,v%d\n", vRT_addr, vRB_addr);
            breakV128to4x32(mkexpr( vB), &bits96_127, &bits64_95, &bits32_63, &bits0_31 );

            cnt_bits0_63   = gen_vpopcntd_mode32(bits0_31, bits32_63);
            cnt_bits64_127 = gen_vpopcntd_mode32(bits64_95, bits96_127);

            putVReg( vRT_addr, binop( Iop_64HLtoV128,
                                      mkexpr( cnt_bits64_127 ),
                                      mkexpr( cnt_bits0_63 ) ) );
         }
         break;
      }

      case 0x50C:  
         DIP("vgbbd v%d,v%d\n", vRT_addr, vRB_addr);
         putVReg( vRT_addr, unop( Iop_PwBitMtxXpose64x2, mkexpr( vB ) ) );
         break;

      default:
         vex_printf("dis_av_count_bitTranspose(ppc)(opc2)\n");
         return False;
      break;
   }
   return True;
}

typedef enum {
   PPC_CMP_EQ = 2,
   PPC_CMP_GT = 4,
   PPC_CMP_GE = 6,
   PPC_CMP_LT = 8
} ppc_cmp_t;


static IRTemp
get_fp_cmp_CR_val (IRExpr * ccIR_expr)
{
   IRTemp condcode = newTemp( Ity_I32 );
   IRTemp ccIR = newTemp( Ity_I32 );

   assign(ccIR, ccIR_expr);
   assign( condcode,
           binop( Iop_Shl32,
                  mkU32( 1 ),
                  unop( Iop_32to8,
                        binop( Iop_Or32,
                               binop( Iop_And32,
                                      unop( Iop_Not32,
                                            binop( Iop_Shr32,
                                                   mkexpr( ccIR ),
                                                   mkU8( 5 ) ) ),
                                      mkU32( 2 ) ),
                               binop( Iop_And32,
                                      binop( Iop_Xor32,
                                             mkexpr( ccIR ),
                                             binop( Iop_Shr32,
                                                    mkexpr( ccIR ),
                                                    mkU8( 6 ) ) ),
                                      mkU32( 1 ) ) ) ) ) );
   return condcode;
}

static IRExpr * _get_maxmin_fp_NaN(IRTemp frA_I64, IRTemp frB_I64)
{
   IRTemp frA_isNaN = newTemp(Ity_I1);
   IRTemp frB_isNaN = newTemp(Ity_I1);
   IRTemp frA_isSNaN = newTemp(Ity_I1);
   IRTemp frB_isSNaN = newTemp(Ity_I1);
   IRTemp frA_isQNaN = newTemp(Ity_I1);
   IRTemp frB_isQNaN = newTemp(Ity_I1);

   assign( frA_isNaN, is_NaN( frA_I64 ) );
   assign( frB_isNaN, is_NaN( frB_I64 ) );
   
   assign( frA_isSNaN,
           mkAND1( mkexpr(frA_isNaN),
                   binop( Iop_CmpEQ32,
                          binop( Iop_And32,
                                 unop( Iop_64HIto32, mkexpr( frA_I64 ) ),
                                 mkU32( 0x00080000 ) ),
                          mkU32( 0 ) ) ) );
   assign( frB_isSNaN,
           mkAND1( mkexpr(frB_isNaN),
                   binop( Iop_CmpEQ32,
                          binop( Iop_And32,
                                 unop( Iop_64HIto32, mkexpr( frB_I64 ) ),
                                 mkU32( 0x00080000 ) ),
                          mkU32( 0 ) ) ) );
   assign( frA_isQNaN,
           mkAND1( mkexpr( frA_isNaN ), unop( Iop_Not1, mkexpr( frA_isSNaN ) ) ) );
   assign( frB_isQNaN,
           mkAND1( mkexpr( frB_isNaN ), unop( Iop_Not1, mkexpr( frB_isSNaN ) ) ) );


#define SNAN_MASK 0x0008000000000000ULL
   return
   IRExpr_ITE(mkexpr(frA_isSNaN),
              
              binop(Iop_Or64, mkexpr(frA_I64), mkU64(SNAN_MASK)),
              
              IRExpr_ITE(mkexpr(frB_isSNaN),
                         
                         binop(Iop_Or64, mkexpr(frB_I64), mkU64(SNAN_MASK)),
                         
                         IRExpr_ITE(mkexpr(frB_isQNaN),
                                    
                                    mkexpr(frA_I64),
                                    
                                    mkexpr(frB_I64))));
}

static IRExpr * _get_maxmin_fp_cmp(IRTemp src1, IRTemp src2, Bool isMin)
{
   IRTemp src1cmpsrc2 = get_fp_cmp_CR_val( binop( Iop_CmpF64,
                                                  unop( Iop_ReinterpI64asF64,
                                                        mkexpr( src1 ) ),
                                                  unop( Iop_ReinterpI64asF64,
                                                        mkexpr( src2 ) ) ) );

   return IRExpr_ITE( binop( Iop_CmpEQ32,
                               mkexpr( src1cmpsrc2 ),
                               mkU32( isMin ? PPC_CMP_LT : PPC_CMP_GT ) ),
                      
                      mkexpr( src1 ),
                      
                      mkexpr( src2 ) );
}

static IRExpr * get_max_min_fp(IRTemp frA_I64, IRTemp frB_I64, Bool isMin)
{
   IRTemp anyNaN = newTemp(Ity_I1);
   IRTemp frA_isZero = newTemp(Ity_I1);
   IRTemp frB_isZero = newTemp(Ity_I1);
   assign(frA_isZero, is_Zero(frA_I64, False  ));
   assign(frB_isZero, is_Zero(frB_I64, False  ));
   assign(anyNaN, mkOR1(is_NaN(frA_I64), is_NaN(frB_I64)));
#define MINUS_ZERO 0x8000000000000000ULL

   return IRExpr_ITE( 
                     mkAND1( mkexpr( frA_isZero ), mkexpr( frB_isZero ) ),
                     IRExpr_ITE( binop( Iop_CmpEQ32,
                                        unop( Iop_64HIto32,
                                              mkexpr( frA_I64 ) ),
                                        mkU32( isMin ? 0x80000000 : 0 ) ),
                                 mkU64( isMin ? MINUS_ZERO : 0ULL ),
                                 mkexpr( frB_I64 ) ),
                     
                     IRExpr_ITE( mkexpr( anyNaN ),
                                 
                                 _get_maxmin_fp_NaN( frA_I64, frB_I64 ),
                                 
                                 _get_maxmin_fp_cmp( frB_I64, frA_I64, isMin ) ));
}

static const HChar * _get_vsx_rdpi_suffix(UInt opc2)
{
   switch (opc2 & 0x7F) {
      case 0x72:
         return "m";
      case 0x52:
         return "p";
      case 0x56:
         return "c";
      case 0x32:
         return "z";
      case 0x12:
         return "";

      default: 
         vex_printf("Unrecognized opcode %x\n", opc2);
         vpanic("_get_vsx_rdpi_suffix(ppc)(opc2)");
   }
}

static IRExpr * _do_vsx_fp_roundToInt(IRTemp frB_I64, UInt opc2)
{

   
   IRTemp frB = newTemp(Ity_F64);
   IRTemp frD = newTemp(Ity_F64);
   IRTemp intermediateResult = newTemp(Ity_I64);
   IRTemp is_SNAN = newTemp(Ity_I1);
   IRExpr * hi32;
   IRExpr * rxpi_rm;
   switch (opc2 & 0x7F) {
      case 0x72:
         rxpi_rm = mkU32(Irrm_NegINF);
         break;
      case 0x52:
         rxpi_rm = mkU32(Irrm_PosINF);
         break;
      case 0x56:
         rxpi_rm = get_IR_roundingmode();
         break;
      case 0x32:
         rxpi_rm = mkU32(Irrm_ZERO);
         break;
      case 0x12:
         rxpi_rm = mkU32(Irrm_NEAREST);
         break;

      default: 
         vex_printf("Unrecognized opcode %x\n", opc2);
         vpanic("_do_vsx_fp_roundToInt(ppc)(opc2)");
   }
   assign(frB, unop(Iop_ReinterpI64asF64, mkexpr(frB_I64)));
   assign( intermediateResult,
           binop( Iop_F64toI64S, rxpi_rm,
                  mkexpr( frB ) ) );

   
   
   
   assign( frD,
           IRExpr_ITE(
              binop( Iop_CmpNE8,
                     unop( Iop_32to8,
                           binop( Iop_CmpF64,
                                  IRExpr_Const( IRConst_F64( 9e18 ) ),
                                  unop( Iop_AbsF64, mkexpr( frB ) ) ) ),
                     mkU8(0) ),
              mkexpr( frB ),
              IRExpr_ITE(
                 binop( Iop_CmpNE32,
                        binop( Iop_Shr32,
                               unop( Iop_64HIto32,
                                     mkexpr( frB_I64 ) ),
                               mkU8( 31 ) ),
                        mkU32(0) ),
                 unop( Iop_NegF64,
                       unop( Iop_AbsF64,
                             binop( Iop_I64StoF64,
                                    mkU32( 0 ),
                                    mkexpr( intermediateResult ) ) ) ),
                 binop( Iop_I64StoF64,
                        mkU32( 0 ),
                        mkexpr( intermediateResult ) )
              )
           )
   );

#define SNAN_MASK 0x0008000000000000ULL
   hi32 = unop( Iop_64HIto32, mkexpr(frB_I64) );
   assign( is_SNAN,
           mkAND1( is_NaN( frB_I64 ),
                   binop( Iop_CmpEQ32,
                          binop( Iop_And32, hi32, mkU32( 0x00080000 ) ),
                          mkU32( 0 ) ) ) );

   return IRExpr_ITE( mkexpr( is_SNAN ),
                        unop( Iop_ReinterpI64asF64,
                              binop( Iop_Xor64,
                                     mkU64( SNAN_MASK ),
                                     mkexpr( frB_I64 ) ) ),
                      mkexpr( frD ));
}

static Bool
dis_vxv_misc ( UInt theInstr, UInt opc2 )
{
   
   UChar opc1 = ifieldOPC( theInstr );
   UChar XT = ifieldRegXT( theInstr );
   UChar XB = ifieldRegXB( theInstr );

   if (opc1 != 0x3C) {
      vex_printf( "dis_vxv_misc(ppc)(instr)\n" );
      return False;
   }

   switch (opc2) {
      case 0x1B4:  
      case 0x194:  
                   
      {
         IRExpr* ieee_one = IRExpr_Const(IRConst_F64i(0x3ff0000000000000ULL));
         IRExpr* rm  = get_IR_roundingmode();
         IRTemp frB = newTemp(Ity_I64);
         IRTemp frB2 = newTemp(Ity_I64);
         Bool redp = opc2 == 0x1B4;
         IRTemp sqrtHi = newTemp(Ity_F64);
         IRTemp sqrtLo = newTemp(Ity_F64);
         assign(frB,  unop(Iop_V128HIto64, getVSReg( XB )));
         assign(frB2, unop(Iop_V128to64, getVSReg( XB )));

         DIP("%s v%d,v%d\n", redp ? "xvredp" : "xvrsqrtedp", (UInt)XT, (UInt)XB);
         if (!redp) {
            assign( sqrtHi,
                    binop( Iop_SqrtF64,
                           rm,
                           unop( Iop_ReinterpI64asF64, mkexpr( frB ) ) ) );
            assign( sqrtLo,
                    binop( Iop_SqrtF64,
                           rm,
                           unop( Iop_ReinterpI64asF64, mkexpr( frB2 ) ) ) );
         }
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          unop( Iop_ReinterpF64asI64,
                                triop( Iop_DivF64,
                                       rm,
                                       ieee_one,
                                       redp ? unop( Iop_ReinterpI64asF64,
                                                    mkexpr( frB ) )
                                            : mkexpr( sqrtHi ) ) ),
                          unop( Iop_ReinterpF64asI64,
                                triop( Iop_DivF64,
                                       rm,
                                       ieee_one,
                                       redp ? unop( Iop_ReinterpI64asF64,
                                                    mkexpr( frB2 ) )
                                            : mkexpr( sqrtLo ) ) ) ) );
         break;

      }
      case 0x134: 
      case 0x114: 
      {
         IRTemp b3, b2, b1, b0;
         IRTemp res0 = newTemp(Ity_I32);
         IRTemp res1 = newTemp(Ity_I32);
         IRTemp res2 = newTemp(Ity_I32);
         IRTemp res3 = newTemp(Ity_I32);
         IRTemp sqrt3 = newTemp(Ity_F64);
         IRTemp sqrt2 = newTemp(Ity_F64);
         IRTemp sqrt1 = newTemp(Ity_F64);
         IRTemp sqrt0 = newTemp(Ity_F64);
         IRExpr* rm  = get_IR_roundingmode();
         Bool resp = opc2 == 0x134;

         IRExpr* ieee_one = IRExpr_Const(IRConst_F64i(0x3ff0000000000000ULL));

         b3 = b2 = b1 = b0 = IRTemp_INVALID;
         DIP("%s v%d,v%d\n", resp ? "xvresp" : "xvrsqrtesp", (UInt)XT, (UInt)XB);
         breakV128to4xF64( getVSReg( XB ), &b3, &b2, &b1, &b0 );

         if (!resp) {
            assign( sqrt3, binop( Iop_SqrtF64, rm, mkexpr( b3 ) ) );
            assign( sqrt2, binop( Iop_SqrtF64, rm, mkexpr( b2 ) ) );
            assign( sqrt1, binop( Iop_SqrtF64, rm, mkexpr( b1 ) ) );
            assign( sqrt0, binop( Iop_SqrtF64, rm, mkexpr( b0 ) ) );
         }

         assign( res0,
                 unop( Iop_ReinterpF32asI32,
                       unop( Iop_TruncF64asF32,
                             triop( Iop_DivF64r32,
                                    rm,
                                    ieee_one,
                                    resp ? mkexpr( b0 ) : mkexpr( sqrt0 ) ) ) ) );
         assign( res1,
                 unop( Iop_ReinterpF32asI32,
                       unop( Iop_TruncF64asF32,
                             triop( Iop_DivF64r32,
                                    rm,
                                    ieee_one,
                                    resp ? mkexpr( b1 ) : mkexpr( sqrt1 ) ) ) ) );
         assign( res2,
                 unop( Iop_ReinterpF32asI32,
                       unop( Iop_TruncF64asF32,
                             triop( Iop_DivF64r32,
                                    rm,
                                    ieee_one,
                                    resp ? mkexpr( b2 ) : mkexpr( sqrt2 ) ) ) ) );
         assign( res3,
                 unop( Iop_ReinterpF32asI32,
                       unop( Iop_TruncF64asF32,
                             triop( Iop_DivF64r32,
                                    rm,
                                    ieee_one,
                                    resp ? mkexpr( b3 ) : mkexpr( sqrt3 ) ) ) ) );
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          binop( Iop_32HLto64, mkexpr( res3 ), mkexpr( res2 ) ),
                          binop( Iop_32HLto64, mkexpr( res1 ), mkexpr( res0 ) ) ) );
         break;
      }
      case 0x300: 
      case 0x320: 
      {
         UChar XA = ifieldRegXA( theInstr );
         IRTemp a3, a2, a1, a0;
         IRTemp b3, b2, b1, b0;
         IRTemp res0 = newTemp( Ity_I32 );
         IRTemp res1 = newTemp( Ity_I32 );
         IRTemp res2 = newTemp( Ity_I32 );
         IRTemp res3 = newTemp( Ity_I32 );
         IRTemp a0_I64 = newTemp( Ity_I64 );
         IRTemp a1_I64 = newTemp( Ity_I64 );
         IRTemp a2_I64 = newTemp( Ity_I64 );
         IRTemp a3_I64 = newTemp( Ity_I64 );
         IRTemp b0_I64 = newTemp( Ity_I64 );
         IRTemp b1_I64 = newTemp( Ity_I64 );
         IRTemp b2_I64 = newTemp( Ity_I64 );
         IRTemp b3_I64 = newTemp( Ity_I64 );

         Bool isMin = opc2 == 0x320 ? True : False;

         a3 = a2 = a1 = a0 = IRTemp_INVALID;
         b3 = b2 = b1 = b0 = IRTemp_INVALID;
         DIP("%s v%d,v%d v%d\n", isMin ? "xvminsp" : "xvmaxsp", (UInt)XT, (UInt)XA, (UInt)XB);
         breakV128to4xF64( getVSReg( XA ), &a3, &a2, &a1, &a0 );
         breakV128to4xF64( getVSReg( XB ), &b3, &b2, &b1, &b0 );
         assign( a0_I64, unop( Iop_ReinterpF64asI64, mkexpr( a0 ) ) );
         assign( b0_I64, unop( Iop_ReinterpF64asI64, mkexpr( b0 ) ) );
         assign( a1_I64, unop( Iop_ReinterpF64asI64, mkexpr( a1 ) ) );
         assign( b1_I64, unop( Iop_ReinterpF64asI64, mkexpr( b1 ) ) );
         assign( a2_I64, unop( Iop_ReinterpF64asI64, mkexpr( a2 ) ) );
         assign( b2_I64, unop( Iop_ReinterpF64asI64, mkexpr( b2 ) ) );
         assign( a3_I64, unop( Iop_ReinterpF64asI64, mkexpr( a3 ) ) );
         assign( b3_I64, unop( Iop_ReinterpF64asI64, mkexpr( b3 ) ) );
         assign( res0,
                 unop( Iop_ReinterpF32asI32,
                       unop( Iop_TruncF64asF32,
                             unop( Iop_ReinterpI64asF64,
                                   get_max_min_fp( a0_I64, b0_I64, isMin ) ) ) ) );
         assign( res1,
                 unop( Iop_ReinterpF32asI32,
                       unop( Iop_TruncF64asF32,
                             unop( Iop_ReinterpI64asF64,
                                   get_max_min_fp( a1_I64, b1_I64, isMin ) ) ) ) );
         assign( res2,
                 unop( Iop_ReinterpF32asI32,
                       unop( Iop_TruncF64asF32,
                             unop( Iop_ReinterpI64asF64,
                                   get_max_min_fp( a2_I64, b2_I64, isMin ) ) ) ) );
         assign( res3,
                 unop( Iop_ReinterpF32asI32,
                       unop( Iop_TruncF64asF32,
                             unop( Iop_ReinterpI64asF64,
                                   get_max_min_fp( a3_I64, b3_I64, isMin ) ) ) ) );
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          binop( Iop_32HLto64, mkexpr( res3 ), mkexpr( res2 ) ),
                          binop( Iop_32HLto64, mkexpr( res1 ), mkexpr( res0 ) ) ) );
         break;
      }
      case 0x380: 
      case 0x3A0: 
      {
         UChar XA = ifieldRegXA( theInstr );
         IRTemp frA = newTemp(Ity_I64);
         IRTemp frB = newTemp(Ity_I64);
         IRTemp frA2 = newTemp(Ity_I64);
         IRTemp frB2 = newTemp(Ity_I64);
         Bool isMin = opc2 == 0x3A0 ? True : False;

         assign(frA,  unop(Iop_V128HIto64, getVSReg( XA )));
         assign(frB,  unop(Iop_V128HIto64, getVSReg( XB )));
         assign(frA2, unop(Iop_V128to64, getVSReg( XA )));
         assign(frB2, unop(Iop_V128to64, getVSReg( XB )));
         DIP("%s v%d,v%d v%d\n", isMin ? "xvmindp" : "xvmaxdp", (UInt)XT, (UInt)XA, (UInt)XB);
         putVSReg( XT, binop( Iop_64HLtoV128, get_max_min_fp(frA, frB, isMin), get_max_min_fp(frA2, frB2, isMin) ) );

         break;
      }
      case 0x3c0: 
      {
         UChar XA = ifieldRegXA( theInstr );
         IRTemp frA = newTemp(Ity_I64);
         IRTemp frB = newTemp(Ity_I64);
         IRTemp frA2 = newTemp(Ity_I64);
         IRTemp frB2 = newTemp(Ity_I64);
         assign(frA,  unop(Iop_V128HIto64, getVSReg( XA )));
         assign(frB,  unop(Iop_V128HIto64, getVSReg( XB )));
         assign(frA2, unop(Iop_V128to64, getVSReg( XA )));
         assign(frB2, unop(Iop_V128to64, getVSReg( XB )));

         DIP("xvcpsgndp v%d,v%d,v%d\n", (UInt)XT, (UInt)XA, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          binop( Iop_Or64,
                                 binop( Iop_And64,
                                        mkexpr( frA ),
                                        mkU64( SIGN_BIT ) ),
                                 binop( Iop_And64,
                                        mkexpr( frB ),
                                        mkU64( SIGN_MASK ) ) ),
                          binop( Iop_Or64,
                                 binop( Iop_And64,
                                        mkexpr( frA2 ),
                                        mkU64( SIGN_BIT ) ),
                                 binop( Iop_And64,
                                        mkexpr( frB2 ),
                                        mkU64( SIGN_MASK ) ) ) ) );
         break;
      }
      case 0x340: 
      {
         UChar XA = ifieldRegXA( theInstr );
         IRTemp a3_I64, a2_I64, a1_I64, a0_I64;
         IRTemp b3_I64, b2_I64, b1_I64, b0_I64;
         IRTemp resHi = newTemp(Ity_I64);
         IRTemp resLo = newTemp(Ity_I64);

         a3_I64 = a2_I64 = a1_I64 = a0_I64 = IRTemp_INVALID;
         b3_I64 = b2_I64 = b1_I64 = b0_I64 = IRTemp_INVALID;
         DIP("xvcpsgnsp v%d,v%d v%d\n",(UInt)XT, (UInt)XA, (UInt)XB);
         breakV128to4x64U( getVSReg( XA ), &a3_I64, &a2_I64, &a1_I64, &a0_I64 );
         breakV128to4x64U( getVSReg( XB ), &b3_I64, &b2_I64, &b1_I64, &b0_I64 );

         assign( resHi,
                 binop( Iop_32HLto64,
                        binop( Iop_Or32,
                               binop( Iop_And32,
                                      unop(Iop_64to32, mkexpr( a3_I64 ) ),
                                      mkU32( SIGN_BIT32 ) ),
                               binop( Iop_And32,
                                      unop(Iop_64to32, mkexpr( b3_I64 ) ),
                                      mkU32( SIGN_MASK32) ) ),

                        binop( Iop_Or32,
                               binop( Iop_And32,
                                      unop(Iop_64to32, mkexpr( a2_I64 ) ),
                                      mkU32( SIGN_BIT32 ) ),
                               binop( Iop_And32,
                                      unop(Iop_64to32, mkexpr( b2_I64 ) ),
                                      mkU32( SIGN_MASK32 ) ) ) ) );
         assign( resLo,
                 binop( Iop_32HLto64,
                        binop( Iop_Or32,
                               binop( Iop_And32,
                                      unop(Iop_64to32, mkexpr( a1_I64 ) ),
                                      mkU32( SIGN_BIT32 ) ),
                               binop( Iop_And32,
                                      unop(Iop_64to32, mkexpr( b1_I64 ) ),
                                      mkU32( SIGN_MASK32 ) ) ),

                        binop( Iop_Or32,
                               binop( Iop_And32,
                                      unop(Iop_64to32, mkexpr( a0_I64 ) ),
                                      mkU32( SIGN_BIT32 ) ),
                               binop( Iop_And32,
                                      unop(Iop_64to32, mkexpr( b0_I64 ) ),
                                      mkU32( SIGN_MASK32 ) ) ) ) );
         putVSReg( XT, binop( Iop_64HLtoV128, mkexpr( resHi ), mkexpr( resLo ) ) );
         break;
      }
      case 0x3B2: 
      case 0x3D2: 
      {
         IRTemp frB = newTemp(Ity_F64);
         IRTemp frB2 = newTemp(Ity_F64);
         IRTemp abs_resultHi = newTemp(Ity_F64);
         IRTemp abs_resultLo = newTemp(Ity_F64);
         Bool make_negative = (opc2 == 0x3D2) ? True : False;
         assign(frB,  unop(Iop_ReinterpI64asF64, unop(Iop_V128HIto64, getVSReg( XB ))));
         assign(frB2, unop(Iop_ReinterpI64asF64, unop(Iop_V128to64, getVSReg(XB))));

         DIP("xv%sabsdp v%d,v%d\n", make_negative ? "n" : "", (UInt)XT, (UInt)XB);
         if (make_negative) {
            assign(abs_resultHi, unop( Iop_NegF64, unop( Iop_AbsF64, mkexpr( frB ) ) ) );
            assign(abs_resultLo, unop( Iop_NegF64, unop( Iop_AbsF64, mkexpr( frB2 ) ) ) );

         } else {
            assign(abs_resultHi, unop( Iop_AbsF64, mkexpr( frB ) ) );
            assign(abs_resultLo, unop( Iop_AbsF64, mkexpr( frB2 ) ) );
         }
         putVSReg( XT, binop( Iop_64HLtoV128,
                              unop( Iop_ReinterpF64asI64, mkexpr( abs_resultHi ) ),
                              unop( Iop_ReinterpF64asI64, mkexpr( abs_resultLo ) ) ) );
         break;
      }
      case 0x332: 
      case 0x352: 
      {
         Bool make_negative = (opc2 == 0x352) ? True : False;
         IRTemp shiftVector = newTemp(Ity_V128);
         IRTemp absVal_vector = newTemp(Ity_V128);
         assign( shiftVector,
                 binop( Iop_64HLtoV128,
                        binop( Iop_32HLto64, mkU32( 1 ), mkU32( 1 ) ),
                        binop( Iop_32HLto64, mkU32( 1 ), mkU32( 1 ) ) ) );
         assign( absVal_vector,
                   binop( Iop_Shr32x4,
                          binop( Iop_Shl32x4,
                                 getVSReg( XB ),
                                 mkexpr( shiftVector ) ),
                          mkexpr( shiftVector ) ) );
         if (make_negative) {
            IRTemp signBit_vector = newTemp(Ity_V128);
            assign( signBit_vector,
                    binop( Iop_64HLtoV128,
                           binop( Iop_32HLto64,
                                  mkU32( 0x80000000 ),
                                  mkU32( 0x80000000 ) ),
                           binop( Iop_32HLto64,
                                  mkU32( 0x80000000 ),
                                  mkU32( 0x80000000 ) ) ) );
            putVSReg( XT,
                      binop( Iop_OrV128,
                             mkexpr( absVal_vector ),
                             mkexpr( signBit_vector ) ) );
         } else {
            putVSReg( XT, mkexpr( absVal_vector ) );
         }
         break;
      }
      case 0x3F2: 
      {
         IRTemp frB = newTemp(Ity_F64);
         IRTemp frB2 = newTemp(Ity_F64);
         assign(frB,  unop(Iop_ReinterpI64asF64, unop(Iop_V128HIto64, getVSReg( XB ))));
         assign(frB2, unop(Iop_ReinterpI64asF64, unop(Iop_V128to64, getVSReg(XB))));
         DIP("xvnegdp v%d,v%d\n",  (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          unop( Iop_ReinterpF64asI64,
                                unop( Iop_NegF64, mkexpr( frB ) ) ),
                          unop( Iop_ReinterpF64asI64,
                                unop( Iop_NegF64, mkexpr( frB2 ) ) ) ) );
         break;
      }
      case 0x192: 
      case 0x1D6: 
      case 0x1F2: 
      case 0x1D2: 
      case 0x1B2: 
      {
         IRTemp frBHi_I64 = newTemp(Ity_I64);
         IRTemp frBLo_I64 = newTemp(Ity_I64);
         IRExpr * frD_fp_roundHi = NULL;
         IRExpr * frD_fp_roundLo = NULL;

         assign( frBHi_I64, unop( Iop_V128HIto64, getVSReg( XB ) ) );
         frD_fp_roundHi = _do_vsx_fp_roundToInt(frBHi_I64, opc2);
         assign( frBLo_I64, unop( Iop_V128to64, getVSReg( XB ) ) );
         frD_fp_roundLo = _do_vsx_fp_roundToInt(frBLo_I64, opc2);

         DIP("xvrdpi%s v%d,v%d\n", _get_vsx_rdpi_suffix(opc2), (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          unop( Iop_ReinterpF64asI64, frD_fp_roundHi ),
                          unop( Iop_ReinterpF64asI64, frD_fp_roundLo ) ) );
         break;
      }
      case 0x112: 
      case 0x156: 
      case 0x172: 
      case 0x152: 
      case 0x132: 
      {
         const HChar * insn_suffix = NULL;
         IROp op;
         if (opc2 != 0x156) {
            
            switch (opc2) {
               case 0x112:
                  insn_suffix = "";
                  op = Iop_RoundF32x4_RN;
                  break;
               case 0x172:
                  insn_suffix = "m";
                  op = Iop_RoundF32x4_RM;
                  break;
               case 0x152:
                  insn_suffix = "p";
                  op = Iop_RoundF32x4_RP;
                  break;
               case 0x132:
                  insn_suffix = "z";
                  op = Iop_RoundF32x4_RZ;
                  break;

               default:
                  vex_printf("Unrecognized opcode %x\n", opc2);
                  vpanic("dis_vxv_misc(ppc)(vrspi<x>)(opc2)\n");
            }
            DIP("xvrspi%s v%d,v%d\n", insn_suffix, (UInt)XT, (UInt)XB);
            putVSReg( XT, unop( op, getVSReg(XB) ) );
         } else {
            
            IRExpr * frD_fp_roundb3, * frD_fp_roundb2, * frD_fp_roundb1, * frD_fp_roundb0;
            IRTemp b3_F64, b2_F64, b1_F64, b0_F64;
            IRTemp b3_I64 = newTemp(Ity_I64);
            IRTemp b2_I64 = newTemp(Ity_I64);
            IRTemp b1_I64 = newTemp(Ity_I64);
            IRTemp b0_I64 = newTemp(Ity_I64);

            b3_F64 = b2_F64 = b1_F64 = b0_F64 = IRTemp_INVALID;
            frD_fp_roundb3 = frD_fp_roundb2 = frD_fp_roundb1 = frD_fp_roundb0 = NULL;
            breakV128to4xF64( getVSReg(XB), &b3_F64, &b2_F64, &b1_F64, &b0_F64);
            assign(b3_I64, unop(Iop_ReinterpF64asI64, mkexpr(b3_F64)));
            assign(b2_I64, unop(Iop_ReinterpF64asI64, mkexpr(b2_F64)));
            assign(b1_I64, unop(Iop_ReinterpF64asI64, mkexpr(b1_F64)));
            assign(b0_I64, unop(Iop_ReinterpF64asI64, mkexpr(b0_F64)));
            frD_fp_roundb3 = unop(Iop_TruncF64asF32,
                                  _do_vsx_fp_roundToInt(b3_I64, opc2));
            frD_fp_roundb2 = unop(Iop_TruncF64asF32,
                                  _do_vsx_fp_roundToInt(b2_I64, opc2));
            frD_fp_roundb1 = unop(Iop_TruncF64asF32,
                                  _do_vsx_fp_roundToInt(b1_I64, opc2));
            frD_fp_roundb0 = unop(Iop_TruncF64asF32,
                                  _do_vsx_fp_roundToInt(b0_I64, opc2));
            DIP("xvrspic v%d,v%d\n", (UInt)XT, (UInt)XB);
            putVSReg( XT,
                      binop( Iop_64HLtoV128,
                             binop( Iop_32HLto64,
                                    unop( Iop_ReinterpF32asI32, frD_fp_roundb3 ),
                                    unop( Iop_ReinterpF32asI32, frD_fp_roundb2 ) ),
                             binop( Iop_32HLto64,
                                    unop( Iop_ReinterpF32asI32, frD_fp_roundb1 ),
                                    unop( Iop_ReinterpF32asI32, frD_fp_roundb0 ) ) ) );
         }
         break;
      }

      default:
         vex_printf( "dis_vxv_misc(ppc)(opc2)\n" );
         return False;
   }
   return True;
}


static Bool
dis_vxs_arith ( UInt theInstr, UInt opc2 )
{
   
   UChar opc1 = ifieldOPC( theInstr );
   UChar XT = ifieldRegXT( theInstr );
   UChar XA = ifieldRegXA( theInstr );
   UChar XB = ifieldRegXB( theInstr );
   IRExpr* rm = get_IR_roundingmode();
   IRTemp frA = newTemp(Ity_F64);
   IRTemp frB = newTemp(Ity_F64);

   if (opc1 != 0x3C) {
      vex_printf( "dis_vxs_arith(ppc)(instr)\n" );
      return False;
   }

   assign(frA, unop(Iop_ReinterpI64asF64, unop(Iop_V128HIto64, getVSReg( XA ))));
   assign(frB, unop(Iop_ReinterpI64asF64, unop(Iop_V128HIto64, getVSReg( XB ))));

   switch (opc2) {
      case 0x000: 
         DIP("xsaddsp v%d,v%d,v%d\n", (UInt)XT, (UInt)XA, (UInt)XB);
         putVSReg( XT, binop( Iop_64HLtoV128,
                              unop( Iop_ReinterpF64asI64,
                                    binop( Iop_RoundF64toF32, rm,
                                           triop( Iop_AddF64, rm,
                                                  mkexpr( frA ),
                                                  mkexpr( frB ) ) ) ),
                              mkU64( 0 ) ) );
         break;
      case 0x020: 
         DIP("xssubsp v%d,v%d,v%d\n", (UInt)XT, (UInt)XA, (UInt)XB);
         putVSReg( XT, binop( Iop_64HLtoV128,
                              unop( Iop_ReinterpF64asI64,
                                    binop( Iop_RoundF64toF32, rm,
                                           triop( Iop_SubF64, rm,
                                                  mkexpr( frA ),
                                                  mkexpr( frB ) ) ) ),
                              mkU64( 0 ) ) );
         break;
      case 0x080: 
         DIP("xsadddp v%d,v%d,v%d\n", (UInt)XT, (UInt)XA, (UInt)XB);
         putVSReg( XT, binop( Iop_64HLtoV128, unop( Iop_ReinterpF64asI64,
                                                    triop( Iop_AddF64, rm,
                                                           mkexpr( frA ),
                                                           mkexpr( frB ) ) ),
                              mkU64( 0 ) ) );
         break;
      case 0x060: 
         DIP("xsdivsp v%d,v%d,v%d\n", (UInt)XT, (UInt)XA, (UInt)XB);
         putVSReg( XT, binop( Iop_64HLtoV128,
                              unop( Iop_ReinterpF64asI64,
                                    binop( Iop_RoundF64toF32, rm,
                                           triop( Iop_DivF64, rm,
                                                  mkexpr( frA ),
                                                  mkexpr( frB ) ) ) ),
                               mkU64( 0 ) ) );
         break;
      case 0x0E0: 
         DIP("xsdivdp v%d,v%d,v%d\n", (UInt)XT, (UInt)XA, (UInt)XB);
         putVSReg( XT, binop( Iop_64HLtoV128, unop( Iop_ReinterpF64asI64,
                                                    triop( Iop_DivF64, rm,
                                                           mkexpr( frA ),
                                                           mkexpr( frB ) ) ),
                              mkU64( 0 ) ) );
         break;
      case 0x004: case 0x024: 
      {
         IRTemp frT = newTemp(Ity_F64);
         Bool mdp = opc2 == 0x024;
         DIP("xsmadd%ssp v%d,v%d,v%d\n", mdp ? "m" : "a", (UInt)XT, (UInt)XA, (UInt)XB);
         assign( frT, unop( Iop_ReinterpI64asF64, unop( Iop_V128HIto64,
                                                        getVSReg( XT ) ) ) );
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          unop( Iop_ReinterpF64asI64,
                                binop( Iop_RoundF64toF32, rm,
                                       qop( Iop_MAddF64, rm,
                                            mkexpr( frA ),
                                            mkexpr( mdp ? frT : frB ),
                                            mkexpr( mdp ? frB : frT ) ) ) ),
                          mkU64( 0 ) ) );
         break;
      }
      case 0x084: case 0x0A4: 
      {
         IRTemp frT = newTemp(Ity_F64);
         Bool mdp = opc2 == 0x0A4;
         DIP("xsmadd%sdp v%d,v%d,v%d\n", mdp ? "m" : "a", (UInt)XT, (UInt)XA, (UInt)XB);
         assign( frT, unop( Iop_ReinterpI64asF64, unop( Iop_V128HIto64,
                                                        getVSReg( XT ) ) ) );
         putVSReg( XT, binop( Iop_64HLtoV128, unop( Iop_ReinterpF64asI64,
                                                    qop( Iop_MAddF64, rm,
                                                         mkexpr( frA ),
                                                         mkexpr( mdp ? frT : frB ),
                                                         mkexpr( mdp ? frB : frT ) ) ),
                              mkU64( 0 ) ) );
         break;
      }
      case 0x044: case 0x064: 
      {
         IRTemp frT = newTemp(Ity_F64);
         Bool mdp = opc2 == 0x064;
         DIP("xsmsub%ssp v%d,v%d,v%d\n", mdp ? "m" : "a", (UInt)XT, (UInt)XA, (UInt)XB);
         assign( frT, unop( Iop_ReinterpI64asF64, unop( Iop_V128HIto64,
                                                        getVSReg( XT ) ) ) );
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          unop( Iop_ReinterpF64asI64,
                                binop( Iop_RoundF64toF32, rm,
                                       qop( Iop_MSubF64, rm,
                                            mkexpr( frA ),
                                            mkexpr( mdp ? frT : frB ),
                                            mkexpr( mdp ? frB : frT ) ) ) ),
                          mkU64( 0 ) ) );
         break;
      }
      case 0x0C4: case 0x0E4: 
      {
         IRTemp frT = newTemp(Ity_F64);
         Bool mdp = opc2 == 0x0E4;
         DIP("xsmsub%sdp v%d,v%d,v%d\n", mdp ? "m" : "a", (UInt)XT, (UInt)XA, (UInt)XB);
         assign( frT, unop( Iop_ReinterpI64asF64, unop( Iop_V128HIto64,
                                                        getVSReg( XT ) ) ) );
         putVSReg( XT, binop( Iop_64HLtoV128, unop( Iop_ReinterpF64asI64,
                                                    qop( Iop_MSubF64, rm,
                                                         mkexpr( frA ),
                                                         mkexpr( mdp ? frT : frB ),
                                                         mkexpr( mdp ? frB : frT ) ) ),
                              mkU64( 0 ) ) );
         break;
      }
      case 0x284: case 0x2A4: 
      {
         Bool mdp = opc2 == 0x2A4;
         IRTemp frT = newTemp(Ity_F64);
         IRTemp maddResult = newTemp(Ity_I64);

         DIP("xsnmadd%sdp v%d,v%d,v%d\n", mdp ? "m" : "a", (UInt)XT, (UInt)XA, (UInt)XB);
         assign( frT, unop( Iop_ReinterpI64asF64, unop( Iop_V128HIto64,
                                                        getVSReg( XT ) ) ) );
         assign( maddResult, unop( Iop_ReinterpF64asI64, qop( Iop_MAddF64, rm,
                                                              mkexpr( frA ),
                                                              mkexpr( mdp ? frT : frB ),
                                                              mkexpr( mdp ? frB : frT ) ) ) );

         putVSReg( XT, binop( Iop_64HLtoV128, mkexpr( getNegatedResult(maddResult) ),
                              mkU64( 0 ) ) );
         break;
      }
      case 0x204: case 0x224: 
      {
         Bool mdp = opc2 == 0x224;
         IRTemp frT = newTemp(Ity_F64);
         IRTemp maddResult = newTemp(Ity_I64);

         DIP("xsnmadd%ssp v%d,v%d,v%d\n", mdp ? "m" : "a", (UInt)XT, (UInt)XA, (UInt)XB);
         assign( frT, unop( Iop_ReinterpI64asF64, unop( Iop_V128HIto64,
                                                        getVSReg( XT ) ) ) );
         assign( maddResult,
                 unop( Iop_ReinterpF64asI64,
                       binop( Iop_RoundF64toF32, rm,
                              qop( Iop_MAddF64, rm,
                                   mkexpr( frA ),
                                   mkexpr( mdp ? frT : frB ),
                                   mkexpr( mdp ? frB : frT ) ) ) ) );

         putVSReg( XT, binop( Iop_64HLtoV128,
                              mkexpr( getNegatedResult(maddResult) ),
                              mkU64( 0 ) ) );
         break;
      }
      case 0x244: case 0x264: 
      {
         IRTemp frT = newTemp(Ity_F64);
         Bool mdp = opc2 == 0x264;
         IRTemp msubResult = newTemp(Ity_I64);

         DIP("xsnmsub%ssp v%d,v%d,v%d\n", mdp ? "m" : "a", (UInt)XT, (UInt)XA, (UInt)XB);
         assign( frT, unop( Iop_ReinterpI64asF64, unop( Iop_V128HIto64,
                                                        getVSReg( XT ) ) ) );
         assign( msubResult,
                 unop( Iop_ReinterpF64asI64,
                       binop( Iop_RoundF64toF32, rm,
                              qop( Iop_MSubF64, rm,
                                   mkexpr( frA ),
                                   mkexpr( mdp ? frT : frB ),
                                   mkexpr( mdp ? frB : frT ) ) ) ) );

         putVSReg( XT, binop( Iop_64HLtoV128,
                              mkexpr( getNegatedResult(msubResult) ),
                              mkU64( 0 ) ) );

         break;
      }

      case 0x2C4: case 0x2E4: 
      {
         IRTemp frT = newTemp(Ity_F64);
         Bool mdp = opc2 == 0x2E4;
         IRTemp msubResult = newTemp(Ity_I64);

         DIP("xsnmsub%sdp v%d,v%d,v%d\n", mdp ? "m" : "a", (UInt)XT, (UInt)XA, (UInt)XB);
         assign( frT, unop( Iop_ReinterpI64asF64, unop( Iop_V128HIto64,
                                                        getVSReg( XT ) ) ) );
         assign(msubResult, unop( Iop_ReinterpF64asI64,
                                      qop( Iop_MSubF64,
                                           rm,
                                           mkexpr( frA ),
                                           mkexpr( mdp ? frT : frB ),
                                           mkexpr( mdp ? frB : frT ) ) ));

         putVSReg( XT, binop( Iop_64HLtoV128, mkexpr( getNegatedResult(msubResult) ), mkU64( 0 ) ) );

         break;
      }

      case 0x040: 
         DIP("xsmulsp v%d,v%d,v%d\n", (UInt)XT, (UInt)XA, (UInt)XB);
         putVSReg( XT, binop( Iop_64HLtoV128,
                              unop( Iop_ReinterpF64asI64,
                                    binop( Iop_RoundF64toF32, rm,
                                           triop( Iop_MulF64, rm,
                                                   mkexpr( frA ),
                                                   mkexpr( frB ) ) ) ),
                              mkU64( 0 ) ) );
         break;

      case 0x0C0: 
         DIP("xsmuldp v%d,v%d,v%d\n", (UInt)XT, (UInt)XA, (UInt)XB);
         putVSReg( XT, binop( Iop_64HLtoV128, unop( Iop_ReinterpF64asI64,
                                                    triop( Iop_MulF64, rm,
                                                           mkexpr( frA ),
                                                           mkexpr( frB ) ) ),
                              mkU64( 0 ) ) );
         break;
      case 0x0A0: 
         DIP("xssubdp v%d,v%d,v%d\n", (UInt)XT, (UInt)XA, (UInt)XB);
         putVSReg( XT, binop( Iop_64HLtoV128, unop( Iop_ReinterpF64asI64,
                                                    triop( Iop_SubF64, rm,
                                                           mkexpr( frA ),
                                                           mkexpr( frB ) ) ),
                              mkU64( 0 ) ) );
         break;

      case 0x016: 
         DIP("xssqrtsp v%d,v%d\n", (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          unop( Iop_ReinterpF64asI64,
                                binop( Iop_RoundF64toF32, rm,
                                       binop( Iop_SqrtF64, rm,
                                              mkexpr( frB ) ) ) ),
                          mkU64( 0 ) ) );
         break;

      case 0x096: 
         DIP("xssqrtdp v%d,v%d\n", (UInt)XT, (UInt)XB);
         putVSReg( XT,  binop( Iop_64HLtoV128, unop( Iop_ReinterpF64asI64,
                                                     binop( Iop_SqrtF64, rm,
                                                            mkexpr( frB ) ) ),
                               mkU64( 0 ) ) );
         break;

      case 0x0F4: 
      {
         UChar crfD     = toUChar( IFIELD( theInstr, 23, 3 ) );
         IRTemp frA_I64 = newTemp(Ity_I64);
         IRTemp frB_I64 = newTemp(Ity_I64);
         DIP("xstdivdp crf%d,v%d,v%d\n", crfD, (UInt)XA, (UInt)XB);
         assign( frA_I64, unop( Iop_ReinterpF64asI64, mkexpr( frA ) ) );
         assign( frB_I64, unop( Iop_ReinterpF64asI64, mkexpr( frB ) ) );
         putGST_field( PPC_GST_CR, do_fp_tdiv(frA_I64, frB_I64), crfD );
         break;
      }
      case 0x0D4: 
      {
         IRTemp frB_I64 = newTemp(Ity_I64);
         UChar crfD     = toUChar( IFIELD( theInstr, 23, 3 ) );
         IRTemp flags = newTemp(Ity_I32);
         IRTemp  fe_flag, fg_flag;
         fe_flag = fg_flag = IRTemp_INVALID;
         DIP("xstsqrtdp v%d,v%d\n", (UInt)XT, (UInt)XB);
         assign( frB_I64, unop(Iop_V128HIto64, getVSReg( XB )) );
         do_fp_tsqrt(frB_I64, False , &fe_flag, &fg_flag);
         assign( flags,
                 binop( Iop_Or32,
                        binop( Iop_Or32, mkU32( 8 ), 
                               binop( Iop_Shl32, mkexpr(fg_flag), mkU8( 2 ) ) ),
                        binop( Iop_Shl32, mkexpr(fe_flag), mkU8( 1 ) ) ) );
         putGST_field( PPC_GST_CR, mkexpr(flags), crfD );
         break;
      }

      default:
         vex_printf( "dis_vxs_arith(ppc)(opc2)\n" );
         return False;
   }

   return True;
}


static Bool
dis_vx_cmp( UInt theInstr, UInt opc2 )
{
   
   UChar opc1 = ifieldOPC( theInstr );
   UChar crfD     = toUChar( IFIELD( theInstr, 23, 3 ) );
   IRTemp ccPPC32;
   UChar XA       = ifieldRegXA ( theInstr );
   UChar XB       = ifieldRegXB ( theInstr );
   IRTemp frA     = newTemp(Ity_F64);
   IRTemp frB     = newTemp(Ity_F64);

   if (opc1 != 0x3C) {
      vex_printf( "dis_vx_cmp(ppc)(instr)\n" );
      return False;
   }

   assign(frA, unop(Iop_ReinterpI64asF64, unop(Iop_V128HIto64, getVSReg( XA ))));
   assign(frB, unop(Iop_ReinterpI64asF64, unop(Iop_V128HIto64, getVSReg( XB ))));
   switch (opc2) {
      case 0x08C: case 0x0AC: 
         DIP("xscmp%sdp crf%d,fr%u,fr%u\n", opc2 == 0x08c ? "u" : "o",
                                           crfD, (UInt)XA, (UInt)XB);
         ccPPC32 = get_fp_cmp_CR_val( binop(Iop_CmpF64, mkexpr(frA), mkexpr(frB)));
         putGST_field( PPC_GST_CR, mkexpr(ccPPC32), crfD );
         break;

      default:
         vex_printf( "dis_vx_cmp(ppc)(opc2)\n" );
         return False;
   }
   return True;
}

static void
do_vvec_fp_cmp ( IRTemp vA, IRTemp vB, UChar XT, UChar flag_rC,
                 ppc_cmp_t cmp_type )
{
   IRTemp frA_hi     = newTemp(Ity_F64);
   IRTemp frB_hi     = newTemp(Ity_F64);
   IRTemp frA_lo     = newTemp(Ity_F64);
   IRTemp frB_lo     = newTemp(Ity_F64);
   IRTemp ccPPC32    = newTemp(Ity_I32);
   IRTemp ccIR_hi;
   IRTemp ccIR_lo;

   IRTemp hiResult = newTemp(Ity_I64);
   IRTemp loResult = newTemp(Ity_I64);
   IRTemp hiEQlo = newTemp(Ity_I1);
   IRTemp all_elem_true = newTemp(Ity_I32);
   IRTemp all_elem_false = newTemp(Ity_I32);

   assign(frA_hi, unop(Iop_ReinterpI64asF64, unop(Iop_V128HIto64, mkexpr( vA ))));
   assign(frB_hi, unop(Iop_ReinterpI64asF64, unop(Iop_V128HIto64, mkexpr( vB ))));
   assign(frA_lo, unop(Iop_ReinterpI64asF64, unop(Iop_V128to64, mkexpr( vA ))));
   assign(frB_lo, unop(Iop_ReinterpI64asF64, unop(Iop_V128to64, mkexpr( vB ))));

   ccIR_hi = get_fp_cmp_CR_val( binop( Iop_CmpF64,
                                       mkexpr( frA_hi ),
                                       mkexpr( frB_hi ) ) );
   ccIR_lo = get_fp_cmp_CR_val( binop( Iop_CmpF64,
                                       mkexpr( frA_lo ),
                                       mkexpr( frB_lo ) ) );

   if (cmp_type != PPC_CMP_GE) {
      assign( hiResult,
              unop( Iop_1Sto64,
                    binop( Iop_CmpEQ32, mkexpr( ccIR_hi ), mkU32( cmp_type ) ) ) );
      assign( loResult,
              unop( Iop_1Sto64,
                    binop( Iop_CmpEQ32, mkexpr( ccIR_lo ), mkU32( cmp_type ) ) ) );
   } else {
      
      
      IRTemp lo_GE = newTemp(Ity_I1);
      IRTemp hi_GE = newTemp(Ity_I1);

      assign(hi_GE, mkOR1( binop( Iop_CmpEQ32, mkexpr( ccIR_hi ), mkU32( 2 ) ),
                           binop( Iop_CmpEQ32, mkexpr( ccIR_hi ), mkU32( 4 ) ) ) );
      assign( hiResult,unop( Iop_1Sto64, mkexpr( hi_GE ) ) );

      assign(lo_GE, mkOR1( binop( Iop_CmpEQ32, mkexpr( ccIR_lo ), mkU32( 2 ) ),
                           binop( Iop_CmpEQ32, mkexpr( ccIR_lo ), mkU32( 4 ) ) ) );
      assign( loResult, unop( Iop_1Sto64, mkexpr( lo_GE ) ) );
   }

   
   assign( hiEQlo,
           binop( Iop_CmpEQ32,
                  unop( Iop_64to32, mkexpr( hiResult ) ),
                  unop( Iop_64to32, mkexpr( loResult ) ) ) );
   putVSReg( XT,
             binop( Iop_64HLtoV128, mkexpr( hiResult ), mkexpr( loResult ) ) );

   assign( all_elem_true,
           unop( Iop_1Uto32,
                 mkAND1( mkexpr( hiEQlo ),
                         binop( Iop_CmpEQ32,
                                mkU32( 0xffffffff ),
                                unop( Iop_64to32,
                                mkexpr( hiResult ) ) ) ) ) );

   assign( all_elem_false,
           unop( Iop_1Uto32,
                 mkAND1( mkexpr( hiEQlo ),
                         binop( Iop_CmpEQ32,
                                mkU32( 0 ),
                                unop( Iop_64to32,
                                mkexpr( hiResult ) ) ) ) ) );
   assign( ccPPC32,
           binop( Iop_Or32,
                  binop( Iop_Shl32, mkexpr( all_elem_false ), mkU8( 1 ) ),
                  binop( Iop_Shl32, mkexpr( all_elem_true ), mkU8( 3 ) ) ) );

   if (flag_rC) {
      putGST_field( PPC_GST_CR, mkexpr(ccPPC32), 6 );
   }
}

static Bool
dis_vvec_cmp( UInt theInstr, UInt opc2 )
{
   
   UChar opc1 = ifieldOPC( theInstr );
   UChar XT = ifieldRegXT ( theInstr );
   UChar XA = ifieldRegXA ( theInstr );
   UChar XB = ifieldRegXB ( theInstr );
   UChar flag_rC  = ifieldBIT10(theInstr);
   IRTemp vA = newTemp( Ity_V128 );
   IRTemp vB = newTemp( Ity_V128 );

   if (opc1 != 0x3C) {
      vex_printf( "dis_vvec_cmp(ppc)(instr)\n" );
      return False;
   }

   assign( vA, getVSReg( XA ) );
   assign( vB, getVSReg( XB ) );

   switch (opc2) {
      case 0x18C: case 0x38C:  
      {
         DIP("xvcmpeqdp%s crf%d,fr%u,fr%u\n", (flag_rC ? ".":""),
             (UInt)XT, (UInt)XA, (UInt)XB);
         do_vvec_fp_cmp(vA, vB, XT, flag_rC, PPC_CMP_EQ);
         break;
      }

      case 0x1CC: case 0x3CC: 
      {
         DIP("xvcmpgedp%s crf%d,fr%u,fr%u\n", (flag_rC ? ".":""),
             (UInt)XT, (UInt)XA, (UInt)XB);
         do_vvec_fp_cmp(vA, vB, XT, flag_rC, PPC_CMP_GE);
         break;
      }

      case 0x1AC: case 0x3AC: 
      {
         DIP("xvcmpgtdp%s crf%d,fr%u,fr%u\n", (flag_rC ? ".":""),
             (UInt)XT, (UInt)XA, (UInt)XB);
         do_vvec_fp_cmp(vA, vB, XT, flag_rC, PPC_CMP_GT);
         break;
      }

      case 0x10C: case 0x30C: 
      {
         IRTemp vD = newTemp(Ity_V128);

         DIP("xvcmpeqsp%s crf%d,fr%u,fr%u\n", (flag_rC ? ".":""),
             (UInt)XT, (UInt)XA, (UInt)XB);
         assign( vD, binop(Iop_CmpEQ32Fx4, mkexpr(vA), mkexpr(vB)) );
         putVSReg( XT, mkexpr(vD) );
         if (flag_rC) {
            set_AV_CR6( mkexpr(vD), True );
         }
         break;
      }

      case 0x14C: case 0x34C: 
      {
         IRTemp vD = newTemp(Ity_V128);

         DIP("xvcmpgesp%s crf%d,fr%u,fr%u\n", (flag_rC ? ".":""),
             (UInt)XT, (UInt)XA, (UInt)XB);
         assign( vD, binop(Iop_CmpGE32Fx4, mkexpr(vA), mkexpr(vB)) );
         putVSReg( XT, mkexpr(vD) );
         if (flag_rC) {
            set_AV_CR6( mkexpr(vD), True );
         }
         break;
      }

      case 0x12C: case 0x32C: 
      {
         IRTemp vD = newTemp(Ity_V128);

         DIP("xvcmpgtsp%s crf%d,fr%u,fr%u\n", (flag_rC ? ".":""),
             (UInt)XT, (UInt)XA, (UInt)XB);
         assign( vD, binop(Iop_CmpGT32Fx4, mkexpr(vA), mkexpr(vB)) );
         putVSReg( XT, mkexpr(vD) );
         if (flag_rC) {
            set_AV_CR6( mkexpr(vD), True );
         }
         break;
      }

      default:
         vex_printf( "dis_vvec_cmp(ppc)(opc2)\n" );
         return False;
   }
   return True;
}
static Bool
dis_vxs_misc( UInt theInstr, UInt opc2 )
{
#define VG_PPC_SIGN_MASK 0x7fffffffffffffffULL
   
   UChar opc1 = ifieldOPC( theInstr );
   UChar XT = ifieldRegXT ( theInstr );
   UChar XA = ifieldRegXA ( theInstr );
   UChar XB = ifieldRegXB ( theInstr );
   IRTemp vA = newTemp( Ity_V128 );
   IRTemp vB = newTemp( Ity_V128 );

   if (opc1 != 0x3C) {
      vex_printf( "dis_vxs_misc(ppc)(instr)\n" );
      return False;
   }

   assign( vA, getVSReg( XA ) );
   assign( vB, getVSReg( XB ) );


   switch (opc2) {
      case 0x2B2: 
      {
         
         IRTemp absVal = newTemp(Ity_V128);
         if (host_endness == VexEndnessLE) {
            IRTemp hi64 = newTemp(Ity_I64);
            IRTemp lo64 = newTemp(Ity_I64);
            assign( hi64, unop( Iop_V128HIto64, mkexpr(vB) ) );
            assign( lo64, unop( Iop_V128to64, mkexpr(vB) ) );
            assign( absVal, binop( Iop_64HLtoV128,
                                   binop( Iop_And64, mkexpr(hi64),
                                          mkU64(VG_PPC_SIGN_MASK) ),
                                   mkexpr(lo64) ) );
         } else {
            assign(absVal, binop(Iop_ShrV128,
                                 binop(Iop_ShlV128, mkexpr(vB),
                                       mkU8(1)), mkU8(1)));
         }
         DIP("xsabsdp v%d,v%d\n", (UInt)XT, (UInt)XB);
         putVSReg(XT, mkexpr(absVal));
         break;
      }
      case 0x2C0: 
      {
         
         IRTemp vecA_signed = newTemp(Ity_I64);
         IRTemp vecB_unsigned = newTemp(Ity_I64);
         IRTemp vec_result = newTemp(Ity_V128);
         DIP("xscpsgndp v%d,v%d v%d\n", (UInt)XT, (UInt)XA, (UInt)XB);
         assign( vecA_signed, binop( Iop_And64,
                                     unop( Iop_V128HIto64,
                                           mkexpr(vA)),
                                           mkU64(~VG_PPC_SIGN_MASK) ) );
         assign( vecB_unsigned, binop( Iop_And64,
                                       unop( Iop_V128HIto64,
                                             mkexpr(vB) ),
                                             mkU64(VG_PPC_SIGN_MASK) ) );
         assign( vec_result, binop( Iop_64HLtoV128,
                                    binop( Iop_Or64,
                                           mkexpr(vecA_signed),
                                           mkexpr(vecB_unsigned) ),
                                    mkU64(0x0ULL)));
         putVSReg(XT, mkexpr(vec_result));
         break;
      }
      case 0x2D2: 
      {
         
         IRTemp BHi_signed = newTemp(Ity_I64);
         DIP("xsnabsdp v%d,v%d\n", (UInt)XT, (UInt)XB);
         assign( BHi_signed, binop( Iop_Or64,
                                    unop( Iop_V128HIto64,
                                          mkexpr(vB) ),
                                          mkU64(~VG_PPC_SIGN_MASK) ) );
         putVSReg(XT, binop( Iop_64HLtoV128,
                             mkexpr(BHi_signed), mkU64(0x0ULL) ) );
         break;
      }
      case 0x2F2: 
      {
         
         IRTemp BHi_signed = newTemp(Ity_I64);
         IRTemp BHi_unsigned = newTemp(Ity_I64);
         IRTemp BHi_negated = newTemp(Ity_I64);
         IRTemp BHi_negated_signbit = newTemp(Ity_I1);
         IRTemp vec_result = newTemp(Ity_V128);
         DIP("xsnabsdp v%d,v%d\n", (UInt)XT, (UInt)XB);
         assign( BHi_signed, unop( Iop_V128HIto64, mkexpr(vB) ) );
         assign( BHi_unsigned, binop( Iop_And64, mkexpr(BHi_signed),
                                      mkU64(VG_PPC_SIGN_MASK) ) );
         assign( BHi_negated_signbit,
                 unop( Iop_Not1,
                       unop( Iop_32to1,
                             binop( Iop_Shr32,
                                    unop( Iop_64HIto32,
                                          binop( Iop_And64,
                                                 mkexpr(BHi_signed),
                                                 mkU64(~VG_PPC_SIGN_MASK) )
                                          ),
                                    mkU8(31) ) ) ) );
         assign( BHi_negated,
                 binop( Iop_Or64,
                        binop( Iop_32HLto64,
                               binop( Iop_Shl32,
                                      unop( Iop_1Uto32,
                                            mkexpr(BHi_negated_signbit) ),
                                      mkU8(31) ),
                               mkU32(0) ),
                        mkexpr(BHi_unsigned) ) );
         assign( vec_result, binop( Iop_64HLtoV128, mkexpr(BHi_negated),
                                    mkU64(0x0ULL)));
         putVSReg( XT, mkexpr(vec_result));
         break;
      }
      case 0x280: 
      case 0x2A0: 
      {
         IRTemp frA     = newTemp(Ity_I64);
         IRTemp frB     = newTemp(Ity_I64);
         Bool isMin = opc2 == 0x2A0 ? True : False;
         DIP("%s v%d,v%d v%d\n", isMin ? "xsmaxdp" : "xsmindp", (UInt)XT, (UInt)XA, (UInt)XB);

         assign(frA, unop(Iop_V128HIto64, mkexpr( vA )));
         assign(frB, unop(Iop_V128HIto64, mkexpr( vB )));
         putVSReg( XT, binop( Iop_64HLtoV128, get_max_min_fp(frA, frB, isMin), mkU64( 0 ) ) );

         break;
      }
      case 0x0F2: 
      case 0x0D2: 
      case 0x0D6: 
      case 0x0B2: 
      case 0x092: 
      {
         IRTemp frB_I64 = newTemp(Ity_I64);
         IRExpr * frD_fp_round = NULL;

         assign(frB_I64, unop(Iop_V128HIto64, mkexpr( vB )));
         frD_fp_round = _do_vsx_fp_roundToInt(frB_I64, opc2);

         DIP("xsrdpi%s v%d,v%d\n", _get_vsx_rdpi_suffix(opc2), (UInt)XT, (UInt)XB);
         putVSReg( XT,
                   binop( Iop_64HLtoV128,
                          unop( Iop_ReinterpF64asI64, frD_fp_round),
                          mkU64( 0 ) ) );
         break;
      }
      case 0x034: 
      case 0x014: 
      {
         IRTemp frB = newTemp(Ity_F64);
         IRTemp sqrt = newTemp(Ity_F64);
         IRExpr* ieee_one = IRExpr_Const(IRConst_F64i(0x3ff0000000000000ULL));
         IRExpr* rm  = get_IR_roundingmode();
         Bool redp = opc2 == 0x034;
         DIP("%s v%d,v%d\n", redp ? "xsresp" : "xsrsqrtesp", (UInt)XT,
             (UInt)XB);

         assign( frB,
                 unop( Iop_ReinterpI64asF64,
                       unop( Iop_V128HIto64, mkexpr( vB ) ) ) );

         if (!redp)
            assign( sqrt,
                    binop( Iop_SqrtF64,
                           rm,
                           mkexpr(frB) ) );
         putVSReg( XT,
                      binop( Iop_64HLtoV128,
                             unop( Iop_ReinterpF64asI64,
                                   binop( Iop_RoundF64toF32, rm,
                                          triop( Iop_DivF64,
                                                 rm,
                                                 ieee_one,
                                                 redp ? mkexpr( frB ) :
                                                        mkexpr( sqrt ) ) ) ),
                             mkU64( 0 ) ) );
         break;
      }

      case 0x0B4: 
      case 0x094: 

      {
         IRTemp frB = newTemp(Ity_F64);
         IRTemp sqrt = newTemp(Ity_F64);
         IRExpr* ieee_one = IRExpr_Const(IRConst_F64i(0x3ff0000000000000ULL));
         IRExpr* rm  = get_IR_roundingmode();
         Bool redp = opc2 == 0x0B4;
         DIP("%s v%d,v%d\n", redp ? "xsredp" : "xsrsqrtedp", (UInt)XT, (UInt)XB);
         assign( frB,
                 unop( Iop_ReinterpI64asF64,
                       unop( Iop_V128HIto64, mkexpr( vB ) ) ) );

         if (!redp)
            assign( sqrt,
                    binop( Iop_SqrtF64,
                           rm,
                           mkexpr(frB) ) );
         putVSReg( XT,
                      binop( Iop_64HLtoV128,
                             unop( Iop_ReinterpF64asI64,
                                   triop( Iop_DivF64,
                                          rm,
                                          ieee_one,
                                          redp ? mkexpr( frB ) : mkexpr( sqrt ) ) ),
                             mkU64( 0 ) ) );
         break;
      }

      case 0x232: 
      {
         IRTemp frB = newTemp(Ity_F64);
         IRExpr* rm  = get_IR_roundingmode();
         DIP("xsrsp v%d, v%d\n", (UInt)XT, (UInt)XB);
         assign( frB,
                 unop( Iop_ReinterpI64asF64,
                       unop( Iop_V128HIto64, mkexpr( vB ) ) ) );

         putVSReg( XT, binop( Iop_64HLtoV128,
                              unop( Iop_ReinterpF64asI64,
                                    binop( Iop_RoundF64toF32,
                                           rm,
                                           mkexpr( frB ) ) ),
                              mkU64( 0 ) ) );
         break;
      }

      default:
         vex_printf( "dis_vxs_misc(ppc)(opc2)\n" );
         return False;
   }
   return True;
}

static Bool
dis_vx_logic ( UInt theInstr, UInt opc2 )
{
   
   UChar opc1 = ifieldOPC( theInstr );
   UChar XT = ifieldRegXT ( theInstr );
   UChar XA = ifieldRegXA ( theInstr );
   UChar XB = ifieldRegXB ( theInstr );
   IRTemp vA = newTemp( Ity_V128 );
   IRTemp vB = newTemp( Ity_V128 );

   if (opc1 != 0x3C) {
      vex_printf( "dis_vx_logic(ppc)(instr)\n" );
      return False;
   }

   assign( vA, getVSReg( XA ) );
   assign( vB, getVSReg( XB ) );

   switch (opc2) {
      case 0x268: 
         DIP("xxlxor v%d,v%d,v%d\n", (UInt)XT, (UInt)XA, (UInt)XB);
         putVSReg( XT, binop( Iop_XorV128, mkexpr( vA ), mkexpr( vB ) ) );
         break;
      case 0x248: 
         DIP("xxlor v%d,v%d,v%d\n", (UInt)XT, (UInt)XA, (UInt)XB);
         putVSReg( XT, binop( Iop_OrV128, mkexpr( vA ), mkexpr( vB ) ) );
         break;
      case 0x288: 
         DIP("xxlnor v%d,v%d,v%d\n", (UInt)XT, (UInt)XA, (UInt)XB);
         putVSReg( XT, unop( Iop_NotV128, binop( Iop_OrV128, mkexpr( vA ),
                                                 mkexpr( vB ) ) ) );
         break;
      case 0x208: 
         DIP("xxland v%d,v%d,v%d\n", (UInt)XT, (UInt)XA, (UInt)XB);
         putVSReg( XT, binop( Iop_AndV128, mkexpr( vA ), mkexpr( vB ) ) );
         break;
      case 0x228: 
         DIP("xxlandc v%d,v%d,v%d\n", (UInt)XT, (UInt)XA, (UInt)XB);
         putVSReg( XT, binop( Iop_AndV128, mkexpr( vA ), unop( Iop_NotV128,
                                                               mkexpr( vB ) ) ) );
         break;
      case 0x2A8: 
         DIP("xxlorc v%d,v%d,v%d\n", (UInt)XT, (UInt)XA, (UInt)XB);
         putVSReg( XT, binop( Iop_OrV128,
                              mkexpr( vA ),
                              unop( Iop_NotV128, mkexpr( vB ) ) ) );
         break;
      case 0x2C8: 
         DIP("xxlnand v%d,v%d,v%d\n", (UInt)XT, (UInt)XA, (UInt)XB);
         putVSReg( XT, unop( Iop_NotV128,
                             binop( Iop_AndV128, mkexpr( vA ),
                                    mkexpr( vB ) ) ) );
         break;
      case 0x2E8: 
         DIP("xxleqv v%d,v%d,v%d\n", (UInt)XT, (UInt)XA, (UInt)XB);
         putVSReg( XT, unop( Iop_NotV128,
                             binop( Iop_XorV128,
                             mkexpr( vA ), mkexpr( vB ) ) ) );
         break;
      default:
         vex_printf( "dis_vx_logic(ppc)(opc2)\n" );
         return False;
   }
   return True;
}

static Bool
dis_vx_load ( UInt theInstr )
{
   
   UChar opc1 = ifieldOPC( theInstr );
   UChar XT = ifieldRegXT ( theInstr );
   UChar rA_addr = ifieldRegA( theInstr );
   UChar rB_addr = ifieldRegB( theInstr );
   UInt opc2 = ifieldOPClo10( theInstr );

   IRType ty = mode64 ? Ity_I64 : Ity_I32;
   IRTemp EA = newTemp( ty );

   if (opc1 != 0x1F) {
      vex_printf( "dis_vx_load(ppc)(instr)\n" );
      return False;
   }

   assign( EA, ea_rAor0_idxd( rA_addr, rB_addr ) );

   switch (opc2) {
   case 0x00C: 
   {
      IRExpr * exp;
      DIP("lxsiwzx %d,r%u,r%u\n", (UInt)XT, rA_addr, rB_addr);

      if (host_endness == VexEndnessLE)
         exp = unop( Iop_64to32, load( Ity_I64, mkexpr( EA ) ) );
      else
         exp = unop( Iop_64HIto32, load( Ity_I64, mkexpr( EA ) ) );

      putVSReg( XT, binop( Iop_64HLtoV128,
                           unop( Iop_32Uto64, exp),
                           mkU64(0) ) );
      break;
   }
   case 0x04C: 
   {
      IRExpr * exp;
      DIP("lxsiwax %d,r%u,r%u\n", (UInt)XT, rA_addr, rB_addr);

      if (host_endness == VexEndnessLE)
         exp = unop( Iop_64to32, load( Ity_I64, mkexpr( EA ) ) );
      else
         exp = unop( Iop_64HIto32, load( Ity_I64, mkexpr( EA ) ) );

      putVSReg( XT, binop( Iop_64HLtoV128,
                           unop( Iop_32Sto64, exp),
                           mkU64(0) ) );
      break;
   }
   case 0x20C: 
   {
      IRExpr * exp;
      DIP("lxsspx %d,r%u,r%u\n", (UInt)XT, rA_addr, rB_addr);
      exp = unop( Iop_ReinterpF64asI64,
                  unop( Iop_F32toF64,
                        unop( Iop_ReinterpI32asF32,
                              load( Ity_I32, mkexpr( EA ) ) ) ) );

      putVSReg( XT, binop( Iop_64HLtoV128, exp, mkU64( 0 ) ) );
      break;
   }
   case 0x24C: 
   {
      IRExpr * exp;
      DIP("lxsdx %d,r%u,r%u\n", (UInt)XT, rA_addr, rB_addr);
      exp = load( Ity_I64, mkexpr( EA ) );
      
      
      
      putVSReg( XT, binop( Iop_64HLtoV128, exp, exp ) );
      break;
   }
   case 0x34C: 
   {
      IROp addOp = ty == Ity_I64 ? Iop_Add64 : Iop_Add32;
      IRExpr * high, *low;
      ULong ea_off = 8;
      IRExpr* high_addr;
      DIP("lxvd2x %d,r%u,r%u\n", (UInt)XT, rA_addr, rB_addr);
      high = load( Ity_I64, mkexpr( EA ) );
      high_addr = binop( addOp, mkexpr( EA ), ty == Ity_I64 ? mkU64( ea_off )
            : mkU32( ea_off ) );
      low = load( Ity_I64, high_addr );
      putVSReg( XT, binop( Iop_64HLtoV128, high, low ) );
      break;
   }
   case 0x14C: 
   {
      IRTemp data = newTemp(Ity_I64);
      DIP("lxvdsx %d,r%u,r%u\n", (UInt)XT, rA_addr, rB_addr);
      assign( data, load( Ity_I64, mkexpr( EA ) ) );
      putVSReg( XT, binop( Iop_64HLtoV128, mkexpr( data ), mkexpr( data ) ) );
      break;
   }
   case 0x30C:
   {
      IRExpr *t0;

      DIP("lxvw4x %d,r%u,r%u\n", (UInt)XT, rA_addr, rB_addr);

      
      if (host_endness == VexEndnessLE) {
         IRExpr *t0_BE;
         IRTemp perm_LE = newTemp(Ity_V128);

         t0_BE = load( Ity_V128, mkexpr( EA ) );

         
         assign( perm_LE, binop( Iop_64HLtoV128, mkU64(0x0c0d0e0f08090a0bULL),
                                 mkU64(0x0405060700010203ULL)));

         t0 = binop( Iop_Perm8x16, t0_BE, mkexpr(perm_LE) );
      } else {
         t0 = load( Ity_V128, mkexpr( EA ) );
      }

      putVSReg( XT, t0 );
      break;
   }
   default:
      vex_printf( "dis_vx_load(ppc)(opc2)\n" );
      return False;
   }
   return True;
}

static Bool
dis_vx_store ( UInt theInstr )
{
   
   UChar opc1 = ifieldOPC( theInstr );
   UChar XS = ifieldRegXS( theInstr );
   UChar rA_addr = ifieldRegA( theInstr );
   UChar rB_addr = ifieldRegB( theInstr );
   IRTemp vS = newTemp( Ity_V128 );
   UInt opc2 = ifieldOPClo10( theInstr );

   IRType ty = mode64 ? Ity_I64 : Ity_I32;
   IRTemp EA = newTemp( ty );

   if (opc1 != 0x1F) {
      vex_printf( "dis_vx_store(ppc)(instr)\n" );
      return False;
   }

   assign( EA, ea_rAor0_idxd( rA_addr, rB_addr ) );
   assign( vS, getVSReg( XS ) );

   switch (opc2) {
   case 0x08C:
   {
      IRExpr * high64, * low32;
      DIP("stxsiwx %d,r%u,r%u\n", (UInt)XS, rA_addr, rB_addr);
      high64 = unop( Iop_V128HIto64, mkexpr( vS ) );
      low32  = unop( Iop_64to32, high64 );
      store( mkexpr( EA ), low32 );
      break;
   }
   case 0x28C:
   {
      IRTemp high64 = newTemp(Ity_F64);
      IRTemp val32  = newTemp(Ity_I32);
      DIP("stxsspx %d,r%u,r%u\n", (UInt)XS, rA_addr, rB_addr);
      assign(high64, unop( Iop_ReinterpI64asF64,
                           unop( Iop_V128HIto64, mkexpr( vS ) ) ) );
      assign(val32, unop( Iop_ReinterpF32asI32,
                          unop( Iop_TruncF64asF32,
                                mkexpr(high64) ) ) );
      store( mkexpr( EA ), mkexpr( val32 ) );
      break;
   }
   case 0x2CC:
   {
      IRExpr * high64;
      DIP("stxsdx %d,r%u,r%u\n", (UInt)XS, rA_addr, rB_addr);
      high64 = unop( Iop_V128HIto64, mkexpr( vS ) );
      store( mkexpr( EA ), high64 );
      break;
   }
   case 0x3CC:
   {
      IRExpr * high64, *low64;
      DIP("stxvd2x %d,r%u,r%u\n", (UInt)XS, rA_addr, rB_addr);
      high64 = unop( Iop_V128HIto64, mkexpr( vS ) );
      low64 = unop( Iop_V128to64, mkexpr( vS ) );
      store( mkexpr( EA ), high64 );
      store( binop( mkSzOp( ty, Iop_Add8 ), mkexpr( EA ),
                    ty == Ity_I64 ? mkU64( 8 ) : mkU32( 8 ) ), low64 );
      break;
   }
   case 0x38C:
   {
      UInt ea_off = 0;
      IRExpr* irx_addr;
      IRTemp hi64 = newTemp( Ity_I64 );
      IRTemp lo64 = newTemp( Ity_I64 );

      DIP("stxvw4x %d,r%u,r%u\n", (UInt)XS, rA_addr, rB_addr);

      
      
      assign( hi64, unop( Iop_V128HIto64, mkexpr( vS ) ) );
      assign( lo64, unop( Iop_V128to64, mkexpr( vS ) ) );
      store( mkexpr( EA ), unop( Iop_64HIto32, mkexpr( hi64 ) ) );
      ea_off += 4;
      irx_addr = binop( mkSzOp( ty, Iop_Add8 ), mkexpr( EA ),
                        ty == Ity_I64 ? mkU64( ea_off ) : mkU32( ea_off ) );
      store( irx_addr, unop( Iop_64to32, mkexpr( hi64 ) ) );
      ea_off += 4;
      irx_addr = binop( mkSzOp( ty, Iop_Add8 ), mkexpr( EA ),
                        ty == Ity_I64 ? mkU64( ea_off ) : mkU32( ea_off ) );
      store( irx_addr, unop( Iop_64HIto32, mkexpr( lo64 ) ) );
      ea_off += 4;
      irx_addr = binop( mkSzOp( ty, Iop_Add8 ), mkexpr( EA ),
                        ty == Ity_I64 ? mkU64( ea_off ) : mkU32( ea_off ) );
      store( irx_addr, unop( Iop_64to32, mkexpr( lo64 ) ) );

      break;
   }
   default:
      vex_printf( "dis_vx_store(ppc)(opc2)\n" );
      return False;
   }
   return True;
}

static Bool
dis_vx_permute_misc( UInt theInstr, UInt opc2 )
{
   
   UChar opc1 = ifieldOPC( theInstr );
   UChar XT = ifieldRegXT ( theInstr );
   UChar XA = ifieldRegXA ( theInstr );
   UChar XB = ifieldRegXB ( theInstr );
   IRTemp vT = newTemp( Ity_V128 );
   IRTemp vA = newTemp( Ity_V128 );
   IRTemp vB = newTemp( Ity_V128 );

   if (opc1 != 0x3C) {
      vex_printf( "dis_vx_permute_misc(ppc)(instr)\n" );
      return False;
   }

   assign( vA, getVSReg( XA ) );
   assign( vB, getVSReg( XB ) );

   switch (opc2) {
      case 0x8: 
      {
         UChar SHW = ifieldSHW ( theInstr );
         IRTemp result = newTemp(Ity_V128);
         if ( SHW != 0 ) {
             IRTemp hi = newTemp(Ity_V128);
             IRTemp lo = newTemp(Ity_V128);
             assign( hi, binop(Iop_ShlV128, mkexpr(vA), mkU8(SHW*32)) );
             assign( lo, binop(Iop_ShrV128, mkexpr(vB), mkU8(128-SHW*32)) );
             assign ( result, binop(Iop_OrV128, mkexpr(hi), mkexpr(lo)) );
         } else
             assign ( result, mkexpr(vA) );
         DIP("xxsldwi v%d,v%d,v%d,%d\n", (UInt)XT, (UInt)XA, (UInt)XB, (UInt)SHW);
         putVSReg( XT, mkexpr(result) );
         break;
      }
      case 0x28: 
      {
         UChar DM = ifieldDM ( theInstr );
         IRTemp hi = newTemp(Ity_I64);
         IRTemp lo = newTemp(Ity_I64);

         if (DM & 0x2)
           assign( hi, unop(Iop_V128to64, mkexpr(vA)) );
         else
           assign( hi, unop(Iop_V128HIto64, mkexpr(vA)) );

         if (DM & 0x1)
           assign( lo, unop(Iop_V128to64, mkexpr(vB)) );
         else
           assign( lo, unop(Iop_V128HIto64, mkexpr(vB)) );

         assign( vT, binop(Iop_64HLtoV128, mkexpr(hi), mkexpr(lo)) );

         DIP("xxpermdi v%d,v%d,v%d,0x%x\n", (UInt)XT, (UInt)XA, (UInt)XB, (UInt)DM);
         putVSReg( XT, mkexpr( vT ) );
         break;
      }
      case 0x48: 
      case 0xc8: 
      {
         const HChar type = (opc2 == 0x48) ? 'h' : 'l';
         IROp word_op = (opc2 == 0x48) ? Iop_V128HIto64 : Iop_V128to64;
         IRTemp a64 = newTemp(Ity_I64);
         IRTemp ahi32 = newTemp(Ity_I32);
         IRTemp alo32 = newTemp(Ity_I32);
         IRTemp b64 = newTemp(Ity_I64);
         IRTemp bhi32 = newTemp(Ity_I32);
         IRTemp blo32 = newTemp(Ity_I32);

         assign( a64, unop(word_op, mkexpr(vA)) );
         assign( ahi32, unop(Iop_64HIto32, mkexpr(a64)) );
         assign( alo32, unop(Iop_64to32, mkexpr(a64)) );

         assign( b64, unop(word_op, mkexpr(vB)) );
         assign( bhi32, unop(Iop_64HIto32, mkexpr(b64)) );
         assign( blo32, unop(Iop_64to32, mkexpr(b64)) );

         assign( vT, binop(Iop_64HLtoV128,
                           binop(Iop_32HLto64, mkexpr(ahi32), mkexpr(bhi32)),
                           binop(Iop_32HLto64, mkexpr(alo32), mkexpr(blo32))) );

         DIP("xxmrg%cw v%d,v%d,v%d\n", type, (UInt)XT, (UInt)XA, (UInt)XB);
         putVSReg( XT, mkexpr( vT ) );
         break;
      }
      case 0x018: 
      {
         UChar XC = ifieldRegXC(theInstr);
         IRTemp vC = newTemp( Ity_V128 );
         assign( vC, getVSReg( XC ) );
         DIP("xxsel v%d,v%d,v%d,v%d\n", (UInt)XT, (UInt)XA, (UInt)XB, (UInt)XC);
         
         putVSReg( XT, binop(Iop_OrV128,
            binop(Iop_AndV128, mkexpr(vA), unop(Iop_NotV128, mkexpr(vC))),
            binop(Iop_AndV128, mkexpr(vB), mkexpr(vC))) );
         break;
      }
      case 0x148: 
      {
         UChar UIM   = ifieldRegA(theInstr) & 3;
         UChar sh_uim = (3 - (UIM)) * 32;
         DIP("xxspltw v%d,v%d,%d\n", (UInt)XT, (UInt)XB, UIM);
         putVSReg( XT,
                   unop( Iop_Dup32x4,
                         unop( Iop_V128to32,
                               binop( Iop_ShrV128, mkexpr( vB ), mkU8( sh_uim ) ) ) ) );
         break;
      }

      default:
         vex_printf( "dis_vx_permute_misc(ppc)(opc2)\n" );
         return False;
   }
   return True;
}

static Bool dis_av_load ( const VexAbiInfo* vbi, UInt theInstr )
{
   
   UChar opc1     = ifieldOPC(theInstr);
   UChar vD_addr  = ifieldRegDS(theInstr);
   UChar rA_addr  = ifieldRegA(theInstr);
   UChar rB_addr  = ifieldRegB(theInstr);
   UInt  opc2     = ifieldOPClo10(theInstr);
   UChar b0       = ifieldBIT0(theInstr);

   IRType ty         = mode64 ? Ity_I64 : Ity_I32;
   IRTemp EA         = newTemp(ty);
   IRTemp EA_align16 = newTemp(ty);

   if (opc1 != 0x1F || b0 != 0) {
      vex_printf("dis_av_load(ppc)(instr)\n");
      return False;
   }

   assign( EA, ea_rAor0_idxd(rA_addr, rB_addr) );
   assign( EA_align16, addr_align( mkexpr(EA), 16 ) );

   switch (opc2) {

   case 0x006: { 
      IRDirty* d;
      UInt vD_off = vectorGuestRegOffset(vD_addr);
      IRExpr** args_be = mkIRExprVec_5(
                         IRExpr_BBPTR(),
                         mkU32(vD_off),
                         binop(Iop_And32, mkNarrowTo32(ty, mkexpr(EA)),
                                          mkU32(0xF)),
                         mkU32(0),
                         mkU32(1));
      IRExpr** args_le = mkIRExprVec_5(
                         IRExpr_BBPTR(),
                         mkU32(vD_off),
                         binop(Iop_And32, mkNarrowTo32(ty, mkexpr(EA)),
                                          mkU32(0xF)),
                         mkU32(0),
                         mkU32(0));
      if (!mode64) {
         d = unsafeIRDirty_0_N (
                        0, 
                        "ppc32g_dirtyhelper_LVS",
                        fnptr_to_fnentry(vbi, &ppc32g_dirtyhelper_LVS),
                        args_be );
      } else {
         if (host_endness == VexEndnessBE)
            d = unsafeIRDirty_0_N (
                           0,
                           "ppc64g_dirtyhelper_LVS",
                           fnptr_to_fnentry(vbi, &ppc64g_dirtyhelper_LVS),
                           args_be );
         else
            d = unsafeIRDirty_0_N (
                           0,
                           "ppc64g_dirtyhelper_LVS",
                           &ppc64g_dirtyhelper_LVS,
                           args_le );
      }
      DIP("lvsl v%d,r%u,r%u\n", vD_addr, rA_addr, rB_addr);
      
      d->nFxState = 1;
      vex_bzero(&d->fxState, sizeof(d->fxState));
      d->fxState[0].fx     = Ifx_Write;
      d->fxState[0].offset = vD_off;
      d->fxState[0].size   = sizeof(U128);

      
      stmt( IRStmt_Dirty(d) );
      break;
   }
   case 0x026: { 
      IRDirty* d;
      UInt vD_off = vectorGuestRegOffset(vD_addr);
      IRExpr** args_be = mkIRExprVec_5(
                             IRExpr_BBPTR(),
                             mkU32(vD_off),
                             binop(Iop_And32, mkNarrowTo32(ty, mkexpr(EA)),
                                              mkU32(0xF)),
                             mkU32(1),
                             mkU32(1));
      IRExpr** args_le = mkIRExprVec_5(
                             IRExpr_BBPTR(),
                             mkU32(vD_off),
                             binop(Iop_And32, mkNarrowTo32(ty, mkexpr(EA)),
                                              mkU32(0xF)),
                             mkU32(1),
                             mkU32(0));

      if (!mode64) {
         d = unsafeIRDirty_0_N (
                        0,
                        "ppc32g_dirtyhelper_LVS",
                        fnptr_to_fnentry(vbi, &ppc32g_dirtyhelper_LVS),
                        args_be );
      } else {
         if (host_endness == VexEndnessBE)
            d = unsafeIRDirty_0_N (
                           0,
                           "ppc64g_dirtyhelper_LVS",
                           fnptr_to_fnentry(vbi, &ppc64g_dirtyhelper_LVS),
                           args_be );
         else
            d = unsafeIRDirty_0_N (
                           0,
                           "ppc64g_dirtyhelper_LVS",
                           &ppc64g_dirtyhelper_LVS,
                           args_le );
      }
      DIP("lvsr v%d,r%u,r%u\n", vD_addr, rA_addr, rB_addr);
      
      d->nFxState = 1;
      vex_bzero(&d->fxState, sizeof(d->fxState));
      d->fxState[0].fx     = Ifx_Write;
      d->fxState[0].offset = vD_off;
      d->fxState[0].size   = sizeof(U128);

      
      stmt( IRStmt_Dirty(d) );
      break;
   }
   case 0x007: 
      DIP("lvebx v%d,r%u,r%u\n", vD_addr, rA_addr, rB_addr);
      putVReg( vD_addr, load(Ity_V128, mkexpr(EA_align16)) );
      break;

   case 0x027: 
      DIP("lvehx v%d,r%u,r%u\n", vD_addr, rA_addr, rB_addr);
      
      putVReg( vD_addr, load(Ity_V128, mkexpr(EA_align16)) );
      break;

   case 0x047: 
      DIP("lvewx v%d,r%u,r%u\n", vD_addr, rA_addr, rB_addr);
      
      putVReg( vD_addr, load(Ity_V128, mkexpr(EA_align16)) );
      break;

   case 0x067: 
      DIP("lvx v%d,r%u,r%u\n", vD_addr, rA_addr, rB_addr);
      putVReg( vD_addr, load(Ity_V128, mkexpr(EA_align16)) );
      break;

   case 0x167: 
      DIP("lvxl v%d,r%u,r%u\n", vD_addr, rA_addr, rB_addr);
      putVReg( vD_addr, load(Ity_V128, mkexpr(EA_align16)) );
      break;

   default:
      vex_printf("dis_av_load(ppc)(opc2)\n");
      return False;
   }
   return True;
}

static Bool dis_av_store ( UInt theInstr )
{
   
   UChar opc1     = ifieldOPC(theInstr);
   UChar vS_addr  = ifieldRegDS(theInstr);
   UChar rA_addr  = ifieldRegA(theInstr);
   UChar rB_addr  = ifieldRegB(theInstr);
   UInt  opc2     = ifieldOPClo10(theInstr);
   UChar b0       = ifieldBIT0(theInstr);

   IRType ty           = mode64 ? Ity_I64 : Ity_I32;
   IRTemp EA           = newTemp(ty);
   IRTemp addr_aligned = newTemp(ty);
   IRTemp vS           = newTemp(Ity_V128);
   IRTemp eb           = newTemp(Ity_I8);
   IRTemp idx          = newTemp(Ity_I8);

   if (opc1 != 0x1F || b0 != 0) {
      vex_printf("dis_av_store(ppc)(instr)\n");
      return False;
   }

   assign( vS, getVReg(vS_addr));
   assign( EA, ea_rAor0_idxd(rA_addr, rB_addr) );

   switch (opc2) {
   case 0x087: { 
      DIP("stvebx v%d,r%u,r%u\n", vS_addr, rA_addr, rB_addr);
      assign( eb, binop(Iop_And8, mkU8(0xF),
                        unop(Iop_32to8,
                             mkNarrowTo32(ty, mkexpr(EA)) )) );
     if (host_endness == VexEndnessLE) {
         assign( idx, binop(Iop_Shl8, mkexpr(eb), mkU8(3)) );
      } else {
         assign( idx, binop(Iop_Shl8,
                            binop(Iop_Sub8, mkU8(15), mkexpr(eb)),
                            mkU8(3)) );
      }
      store( mkexpr(EA),
             unop( Iop_32to8, unop(Iop_V128to32,
                   binop(Iop_ShrV128, mkexpr(vS), mkexpr(idx)))) );
      break;
   }
   case 0x0A7: { 
      DIP("stvehx v%d,r%u,r%u\n", vS_addr, rA_addr, rB_addr);
      assign( addr_aligned, addr_align(mkexpr(EA), 2) );
      assign( eb, binop(Iop_And8, mkU8(0xF),
                        mkNarrowTo8(ty, mkexpr(addr_aligned) )) );
      if (host_endness == VexEndnessLE) {
          assign( idx, binop(Iop_Shl8, mkexpr(eb), mkU8(3)) );
      } else {
         assign( idx, binop(Iop_Shl8,
                            binop(Iop_Sub8, mkU8(14), mkexpr(eb)),
                            mkU8(3)) );
      }
      store( mkexpr(addr_aligned),
             unop( Iop_32to16, unop(Iop_V128to32,
                   binop(Iop_ShrV128, mkexpr(vS), mkexpr(idx)))) );
      break;
   }
   case 0x0C7: { 
      DIP("stvewx v%d,r%u,r%u\n", vS_addr, rA_addr, rB_addr);
      assign( addr_aligned, addr_align(mkexpr(EA), 4) );
      assign( eb, binop(Iop_And8, mkU8(0xF),
                        mkNarrowTo8(ty, mkexpr(addr_aligned) )) );
      if (host_endness == VexEndnessLE) {
         assign( idx, binop(Iop_Shl8, mkexpr(eb), mkU8(3)) );
      } else {
         assign( idx, binop(Iop_Shl8,
                            binop(Iop_Sub8, mkU8(12), mkexpr(eb)),
                            mkU8(3)) );
      }
      store( mkexpr( addr_aligned),
             unop( Iop_V128to32,
                   binop(Iop_ShrV128, mkexpr(vS), mkexpr(idx))) );
      break;
   }

   case 0x0E7: 
      DIP("stvx v%d,r%u,r%u\n", vS_addr, rA_addr, rB_addr);
      store( addr_align( mkexpr(EA), 16 ), mkexpr(vS) );
      break;

   case 0x1E7: 
      DIP("stvxl v%d,r%u,r%u\n", vS_addr, rA_addr, rB_addr);
      store( addr_align( mkexpr(EA), 16 ), mkexpr(vS) );
      break;

   default:
      vex_printf("dis_av_store(ppc)(opc2)\n");
      return False;
   }
   return True;
}

static Bool dis_av_arith ( UInt theInstr )
{
   
   UChar opc1     = ifieldOPC(theInstr);
   UChar vD_addr  = ifieldRegDS(theInstr);
   UChar vA_addr  = ifieldRegA(theInstr);
   UChar vB_addr  = ifieldRegB(theInstr);
   UInt  opc2     = IFIELD( theInstr, 0, 11 );

   IRTemp vA = newTemp(Ity_V128);
   IRTemp vB = newTemp(Ity_V128);
   IRTemp z3 = newTemp(Ity_I64);
   IRTemp z2 = newTemp(Ity_I64);
   IRTemp z1 = newTemp(Ity_I64);
   IRTemp z0 = newTemp(Ity_I64);
   IRTemp aEvn, aOdd;
   IRTemp a15, a14, a13, a12, a11, a10, a9, a8;
   IRTemp a7, a6, a5, a4, a3, a2, a1, a0;
   IRTemp b3, b2, b1, b0;

   aEvn = aOdd = IRTemp_INVALID;
   a15 = a14 = a13 = a12 = a11 = a10 = a9 = a8 = IRTemp_INVALID;
   a7 = a6 = a5 = a4 = a3 = a2 = a1 = a0 = IRTemp_INVALID;
   b3 = b2 = b1 = b0 = IRTemp_INVALID;

   assign( vA, getVReg(vA_addr));
   assign( vB, getVReg(vB_addr));

   if (opc1 != 0x4) {
      vex_printf("dis_av_arith(ppc)(opc1 != 0x4)\n");
      return False;
   }

   switch (opc2) {
   
   case 0x180: { 
      DIP("vaddcuw v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      
      putVReg( vD_addr, binop(Iop_ShrN32x4,
                              binop(Iop_CmpGT32Ux4, mkexpr(vB),
                                    unop(Iop_NotV128, mkexpr(vA))),
                              mkU8(31)) );
      break;
   }
   case 0x000: 
      DIP("vaddubm v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Add8x16, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x040: 
      DIP("vadduhm v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Add16x8, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x080: 
      DIP("vadduwm v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Add32x4, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x0C0: 
      DIP("vaddudm v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Add64x2, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x200: 
      DIP("vaddubs v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_QAdd8Ux16, mkexpr(vA), mkexpr(vB)) );
      
      break;

   case 0x240: 
      DIP("vadduhs v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_QAdd16Ux8, mkexpr(vA), mkexpr(vB)) );
      
      break;

   case 0x280: 
      DIP("vadduws v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_QAdd32Ux4, mkexpr(vA), mkexpr(vB)) );
      
      break;

   case 0x300: 
      DIP("vaddsbs v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_QAdd8Sx16, mkexpr(vA), mkexpr(vB)) );
      
      break;

   case 0x340: 
      DIP("vaddshs v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_QAdd16Sx8, mkexpr(vA), mkexpr(vB)) );
      
      break;

   case 0x380: 
      DIP("vaddsws v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_QAdd32Sx4, mkexpr(vA), mkexpr(vB)) );
      
      break;


   
   case 0x580: { 
      DIP("vsubcuw v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      
      putVReg( vD_addr, binop(Iop_ShrN32x4,
                              unop(Iop_NotV128,
                                   binop(Iop_CmpGT32Ux4, mkexpr(vB),
                                         mkexpr(vA))),
                              mkU8(31)) );
      break;
   }     
   case 0x400: 
      DIP("vsububm v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Sub8x16, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x440: 
      DIP("vsubuhm v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Sub16x8, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x480: 
      DIP("vsubuwm v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Sub32x4, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x4C0: 
      DIP("vsubudm v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Sub64x2, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x600: 
      DIP("vsububs v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_QSub8Ux16, mkexpr(vA), mkexpr(vB)) );
      
      break;

   case 0x640: 
      DIP("vsubuhs v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_QSub16Ux8, mkexpr(vA), mkexpr(vB)) );
      
      break;

   case 0x680: 
      DIP("vsubuws v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_QSub32Ux4, mkexpr(vA), mkexpr(vB)) );
      
      break;

   case 0x700: 
      DIP("vsubsbs v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_QSub8Sx16, mkexpr(vA), mkexpr(vB)) );
      
      break;

   case 0x740: 
      DIP("vsubshs v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_QSub16Sx8, mkexpr(vA), mkexpr(vB)) );
      
      break;

   case 0x780: 
      DIP("vsubsws v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_QSub32Sx4, mkexpr(vA), mkexpr(vB)) );
      
      break;


   
   case 0x002: 
      DIP("vmaxub v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Max8Ux16, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x042: 
      DIP("vmaxuh v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Max16Ux8, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x082: 
      DIP("vmaxuw v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Max32Ux4, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x0C2: 
      DIP("vmaxud v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Max64Ux2, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x102: 
      DIP("vmaxsb v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Max8Sx16, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x142: 
      DIP("vmaxsh v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Max16Sx8, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x182: 
      DIP("vmaxsw v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Max32Sx4, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x1C2: 
      DIP("vmaxsd v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Max64Sx2, mkexpr(vA), mkexpr(vB)) );
      break;

   
   case 0x202: 
      DIP("vminub v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Min8Ux16, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x242: 
      DIP("vminuh v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Min16Ux8, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x282: 
      DIP("vminuw v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Min32Ux4, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x2C2: 
      DIP("vminud v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Min64Ux2, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x302: 
      DIP("vminsb v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Min8Sx16, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x342: 
      DIP("vminsh v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Min16Sx8, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x382: 
      DIP("vminsw v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Min32Sx4, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x3C2: 
      DIP("vminsd v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Min64Sx2, mkexpr(vA), mkexpr(vB)) );
      break;


   
   case 0x402: 
      DIP("vavgub v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Avg8Ux16, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x442: 
      DIP("vavguh v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Avg16Ux8, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x482: 
      DIP("vavguw v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Avg32Ux4, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x502: 
      DIP("vavgsb v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Avg8Sx16, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x542: 
      DIP("vavgsh v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Avg16Sx8, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x582: 
      DIP("vavgsw v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Avg32Sx4, mkexpr(vA), mkexpr(vB)) );
      break;


   
   case 0x008: 
      DIP("vmuloub v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr,
               binop(Iop_MullEven8Ux16, mkexpr(vA), mkexpr(vB)));
      break;

   case 0x048: 
      DIP("vmulouh v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr,
               binop(Iop_MullEven16Ux8, mkexpr(vA), mkexpr(vB)));
      break;

   case 0x088: 
      DIP("vmulouw v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop( Iop_MullEven32Ux4, mkexpr(vA), mkexpr(vB) ) );
      break;

   case 0x089: 
      DIP("vmuluwm v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop( Iop_Mul32x4, mkexpr(vA), mkexpr(vB) ) );
      break;

   case 0x108: 
      DIP("vmulosb v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr,
               binop(Iop_MullEven8Sx16, mkexpr(vA), mkexpr(vB)));
      break;

   case 0x148: 
      DIP("vmulosh v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr,
               binop(Iop_MullEven16Sx8, mkexpr(vA), mkexpr(vB)));
      break;

   case 0x188: 
      DIP("vmulosw v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop( Iop_MullEven32Sx4, mkexpr(vA), mkexpr(vB) ) );
      break;

   case 0x208: 
      DIP("vmuleub v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, MK_Iop_MullOdd8Ux16( mkexpr(vA), mkexpr(vB) ));
      break;

   case 0x248: 
      DIP("vmuleuh v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, MK_Iop_MullOdd16Ux8( mkexpr(vA), mkexpr(vB) ));
      break;

   case 0x288: 
      DIP("vmuleuw v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, MK_Iop_MullOdd32Ux4( mkexpr(vA), mkexpr(vB) ) );
      break;

   case 0x308: 
      DIP("vmulesb v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, MK_Iop_MullOdd8Sx16( mkexpr(vA), mkexpr(vB) ));
      break;

   case 0x348: 
      DIP("vmulesh v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, MK_Iop_MullOdd16Sx8( mkexpr(vA), mkexpr(vB) ));
      break;

   case 0x388: 
      DIP("vmulesw v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, MK_Iop_MullOdd32Sx4( mkexpr(vA), mkexpr(vB) ) );
      break;

   
   case 0x608: { 
      IRTemp aEE, aEO, aOE, aOO;
      aEE = aEO = aOE = aOO = IRTemp_INVALID;
      DIP("vsum4ubs v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);

      
      expand8Ux16( mkexpr(vA), &aEvn, &aOdd ); 
      expand16Ux8( mkexpr(aEvn), &aEE, &aEO ); 
      expand16Ux8( mkexpr(aOdd), &aOE, &aOO ); 

      
      breakV128to4x64U( mkexpr(aEE), &a15, &a11, &a7, &a3 );
      breakV128to4x64U( mkexpr(aOE), &a14, &a10, &a6, &a2 );
      breakV128to4x64U( mkexpr(aEO), &a13, &a9,  &a5, &a1 );
      breakV128to4x64U( mkexpr(aOO), &a12, &a8,  &a4, &a0 );
      breakV128to4x64U( mkexpr(vB),  &b3,  &b2,  &b1, &b0 );

      
      assign( z3, binop(Iop_Add64, mkexpr(b3),
                     binop(Iop_Add64,
                        binop(Iop_Add64, mkexpr(a15), mkexpr(a14)),
                        binop(Iop_Add64, mkexpr(a13), mkexpr(a12)))) );
      assign( z2, binop(Iop_Add64, mkexpr(b2),
                     binop(Iop_Add64,
                         binop(Iop_Add64, mkexpr(a11), mkexpr(a10)),
                         binop(Iop_Add64, mkexpr(a9), mkexpr(a8)))) );
      assign( z1, binop(Iop_Add64, mkexpr(b1),
                     binop(Iop_Add64,
                         binop(Iop_Add64, mkexpr(a7), mkexpr(a6)),
                         binop(Iop_Add64, mkexpr(a5), mkexpr(a4)))) );
      assign( z0, binop(Iop_Add64, mkexpr(b0),
                     binop(Iop_Add64,
                         binop(Iop_Add64, mkexpr(a3), mkexpr(a2)),
                         binop(Iop_Add64, mkexpr(a1), mkexpr(a0)))) );
      
      
      putVReg( vD_addr, mkV128from4x64U( mkexpr(z3), mkexpr(z2),
                                         mkexpr(z1), mkexpr(z0)) );
      break;
   }
   case 0x708: { 
      IRTemp aEE, aEO, aOE, aOO;
      aEE = aEO = aOE = aOO = IRTemp_INVALID;
      DIP("vsum4sbs v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);

      
      expand8Sx16( mkexpr(vA), &aEvn, &aOdd ); 
      expand16Sx8( mkexpr(aEvn), &aEE, &aEO ); 
      expand16Sx8( mkexpr(aOdd), &aOE, &aOO ); 

      
      breakV128to4x64S( mkexpr(aEE), &a15, &a11, &a7, &a3 );
      breakV128to4x64S( mkexpr(aOE), &a14, &a10, &a6, &a2 );
      breakV128to4x64S( mkexpr(aEO), &a13, &a9,  &a5, &a1 );
      breakV128to4x64S( mkexpr(aOO), &a12, &a8,  &a4, &a0 );
      breakV128to4x64S( mkexpr(vB),  &b3,  &b2,  &b1, &b0 );

      
      assign( z3, binop(Iop_Add64, mkexpr(b3),
                     binop(Iop_Add64,
                        binop(Iop_Add64, mkexpr(a15), mkexpr(a14)),
                        binop(Iop_Add64, mkexpr(a13), mkexpr(a12)))) );
      assign( z2, binop(Iop_Add64, mkexpr(b2),
                     binop(Iop_Add64,
                        binop(Iop_Add64, mkexpr(a11), mkexpr(a10)),
                        binop(Iop_Add64, mkexpr(a9), mkexpr(a8)))) );
      assign( z1, binop(Iop_Add64, mkexpr(b1),
                     binop(Iop_Add64,
                        binop(Iop_Add64, mkexpr(a7), mkexpr(a6)),
                        binop(Iop_Add64, mkexpr(a5), mkexpr(a4)))) );
      assign( z0, binop(Iop_Add64, mkexpr(b0),
                     binop(Iop_Add64,
                        binop(Iop_Add64, mkexpr(a3), mkexpr(a2)),
                        binop(Iop_Add64, mkexpr(a1), mkexpr(a0)))) );
      
      
      putVReg( vD_addr, mkV128from4x64S( mkexpr(z3), mkexpr(z2),
                                         mkexpr(z1), mkexpr(z0)) );
      break;
   }
   case 0x648: { 
      DIP("vsum4shs v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);

      
      expand16Sx8( mkexpr(vA), &aEvn, &aOdd ); 

      
      breakV128to4x64S( mkexpr(aEvn), &a7, &a5, &a3, &a1 );
      breakV128to4x64S( mkexpr(aOdd), &a6, &a4, &a2, &a0 );
      breakV128to4x64S( mkexpr(vB),   &b3, &b2, &b1, &b0 );

      
      assign( z3, binop(Iop_Add64, mkexpr(b3),
                        binop(Iop_Add64, mkexpr(a7), mkexpr(a6))));
      assign( z2, binop(Iop_Add64, mkexpr(b2),
                        binop(Iop_Add64, mkexpr(a5), mkexpr(a4))));
      assign( z1, binop(Iop_Add64, mkexpr(b1),
                        binop(Iop_Add64, mkexpr(a3), mkexpr(a2))));
      assign( z0, binop(Iop_Add64, mkexpr(b0),
                        binop(Iop_Add64, mkexpr(a1), mkexpr(a0))));

      
      putVReg( vD_addr, mkV128from4x64S( mkexpr(z3), mkexpr(z2),
                                         mkexpr(z1), mkexpr(z0)) );
      break;
   }
   case 0x688: { 
      DIP("vsum2sws v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);

      
      breakV128to4x64S( mkexpr(vA), &a3, &a2, &a1, &a0 );
      breakV128to4x64S( mkexpr(vB), &b3, &b2, &b1, &b0 );

      
      assign( z2, binop(Iop_Add64, mkexpr(b2),
                        binop(Iop_Add64, mkexpr(a3), mkexpr(a2))) );
      assign( z0, binop(Iop_Add64, mkexpr(b0),
                        binop(Iop_Add64, mkexpr(a1), mkexpr(a0))) );

      
      putVReg( vD_addr, mkV128from4x64S( mkU64(0), mkexpr(z2),
                                         mkU64(0), mkexpr(z0)) );
      break;
   }
   case 0x788: { 
      DIP("vsumsws v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);

      
      breakV128to4x64S( mkexpr(vA), &a3, &a2, &a1, &a0 );
      breakV128to4x64S( mkexpr(vB), &b3, &b2, &b1, &b0 );

      
      assign( z0, binop(Iop_Add64, mkexpr(b0),
                     binop(Iop_Add64,
                        binop(Iop_Add64, mkexpr(a3), mkexpr(a2)),
                        binop(Iop_Add64, mkexpr(a1), mkexpr(a0)))) );

      
      putVReg( vD_addr, mkV128from4x64S( mkU64(0), mkU64(0),
                                         mkU64(0), mkexpr(z0)) );
      break;
   }
   default:
      vex_printf("dis_av_arith(ppc)(opc2=0x%x)\n", opc2);
      return False;
   }
   return True;
}

static Bool dis_av_logic ( UInt theInstr )
{
   
   UChar opc1    = ifieldOPC(theInstr);
   UChar vD_addr = ifieldRegDS(theInstr);
   UChar vA_addr = ifieldRegA(theInstr);
   UChar vB_addr = ifieldRegB(theInstr);
   UInt  opc2    = IFIELD( theInstr, 0, 11 );

   IRTemp vA = newTemp(Ity_V128);
   IRTemp vB = newTemp(Ity_V128);
   assign( vA, getVReg(vA_addr));
   assign( vB, getVReg(vB_addr));

   if (opc1 != 0x4) {
      vex_printf("dis_av_logic(ppc)(opc1 != 0x4)\n");
      return False;
   }

   switch (opc2) {
   case 0x404: 
      DIP("vand v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_AndV128, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x444: 
      DIP("vandc v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_AndV128, mkexpr(vA),
                              unop(Iop_NotV128, mkexpr(vB))) );
      break;

   case 0x484: 
      DIP("vor v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_OrV128, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x4C4: 
      DIP("vxor v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_XorV128, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x504: 
      DIP("vnor v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr,
         unop(Iop_NotV128, binop(Iop_OrV128, mkexpr(vA), mkexpr(vB))) );
      break;

   case 0x544: 
      DIP("vorc v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop( Iop_OrV128,
                               mkexpr( vA ),
                               unop( Iop_NotV128, mkexpr( vB ) ) ) );
      break;

   case 0x584: 
      DIP("vnand v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, unop( Iop_NotV128,
                              binop(Iop_AndV128, mkexpr( vA ),
                              mkexpr( vB ) ) ) );
      break;

   case 0x684: 
      DIP("veqv v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, unop( Iop_NotV128,
                              binop( Iop_XorV128, mkexpr( vA ),
                              mkexpr( vB ) ) ) );
      break;

   default:
      vex_printf("dis_av_logic(ppc)(opc2=0x%x)\n", opc2);
      return False;
   }
   return True;
}

static Bool dis_av_cmp ( UInt theInstr )
{
   
   UChar opc1     = ifieldOPC(theInstr);
   UChar vD_addr  = ifieldRegDS(theInstr);
   UChar vA_addr  = ifieldRegA(theInstr);
   UChar vB_addr  = ifieldRegB(theInstr);
   UChar flag_rC  = ifieldBIT10(theInstr);
   UInt  opc2     = IFIELD( theInstr, 0, 10 );

   IRTemp vA = newTemp(Ity_V128);
   IRTemp vB = newTemp(Ity_V128);
   IRTemp vD = newTemp(Ity_V128);
   assign( vA, getVReg(vA_addr));
   assign( vB, getVReg(vB_addr));

   if (opc1 != 0x4) {
      vex_printf("dis_av_cmp(ppc)(instr)\n");
      return False;
   }

   switch (opc2) {
   case 0x006: 
      DIP("vcmpequb%s v%d,v%d,v%d\n", (flag_rC ? ".":""),
                                      vD_addr, vA_addr, vB_addr);
      assign( vD, binop(Iop_CmpEQ8x16, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x046: 
      DIP("vcmpequh%s v%d,v%d,v%d\n", (flag_rC ? ".":""),
                                      vD_addr, vA_addr, vB_addr);
      assign( vD, binop(Iop_CmpEQ16x8, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x086: 
      DIP("vcmpequw%s v%d,v%d,v%d\n", (flag_rC ? ".":""),
                                      vD_addr, vA_addr, vB_addr);
      assign( vD, binop(Iop_CmpEQ32x4, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x0C7: 
      DIP("vcmpequd%s v%d,v%d,v%d\n", (flag_rC ? ".":""),
                                      vD_addr, vA_addr, vB_addr);
      assign( vD, binop(Iop_CmpEQ64x2, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x206: 
      DIP("vcmpgtub%s v%d,v%d,v%d\n", (flag_rC ? ".":""),
                                      vD_addr, vA_addr, vB_addr);
      assign( vD, binop(Iop_CmpGT8Ux16, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x246: 
      DIP("vcmpgtuh%s v%d,v%d,v%d\n", (flag_rC ? ".":""),
                                      vD_addr, vA_addr, vB_addr);
      assign( vD, binop(Iop_CmpGT16Ux8, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x286: 
      DIP("vcmpgtuw%s v%d,v%d,v%d\n", (flag_rC ? ".":""),
                                       vD_addr, vA_addr, vB_addr);
      assign( vD, binop(Iop_CmpGT32Ux4, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x2C7: 
      DIP("vcmpgtud%s v%d,v%d,v%d\n", (flag_rC ? ".":""),
                                      vD_addr, vA_addr, vB_addr);
      assign( vD, binop(Iop_CmpGT64Ux2, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x306: 
      DIP("vcmpgtsb%s v%d,v%d,v%d\n", (flag_rC ? ".":""),
                                       vD_addr, vA_addr, vB_addr);
      assign( vD, binop(Iop_CmpGT8Sx16, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x346: 
      DIP("vcmpgtsh%s v%d,v%d,v%d\n", (flag_rC ? ".":""),
                                      vD_addr, vA_addr, vB_addr);
      assign( vD, binop(Iop_CmpGT16Sx8, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x386: 
      DIP("vcmpgtsw%s v%d,v%d,v%d\n", (flag_rC ? ".":""),
                                      vD_addr, vA_addr, vB_addr);
      assign( vD, binop(Iop_CmpGT32Sx4, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x3C7: 
      DIP("vcmpgtsd%s v%d,v%d,v%d\n", (flag_rC ? ".":""),
                                      vD_addr, vA_addr, vB_addr);
      assign( vD, binop(Iop_CmpGT64Sx2, mkexpr(vA), mkexpr(vB)) );
      break;

   default:
      vex_printf("dis_av_cmp(ppc)(opc2)\n");
      return False;
   }

   putVReg( vD_addr, mkexpr(vD) );

   if (flag_rC) {
      set_AV_CR6( mkexpr(vD), True );
   }
   return True;
}

static Bool dis_av_multarith ( UInt theInstr )
{
   
   UChar opc1     = ifieldOPC(theInstr);
   UChar vD_addr  = ifieldRegDS(theInstr);
   UChar vA_addr  = ifieldRegA(theInstr);
   UChar vB_addr  = ifieldRegB(theInstr);
   UChar vC_addr  = ifieldRegC(theInstr);
   UChar opc2     = toUChar( IFIELD( theInstr, 0, 6 ) );

   IRTemp vA    = newTemp(Ity_V128);
   IRTemp vB    = newTemp(Ity_V128);
   IRTemp vC    = newTemp(Ity_V128);
   IRTemp zeros = newTemp(Ity_V128);
   IRTemp aLo   = newTemp(Ity_V128);
   IRTemp bLo   = newTemp(Ity_V128);
   IRTemp cLo   = newTemp(Ity_V128);
   IRTemp zLo   = newTemp(Ity_V128);
   IRTemp aHi   = newTemp(Ity_V128);
   IRTemp bHi   = newTemp(Ity_V128);
   IRTemp cHi   = newTemp(Ity_V128);
   IRTemp zHi   = newTemp(Ity_V128);
   IRTemp abEvn = newTemp(Ity_V128);
   IRTemp abOdd = newTemp(Ity_V128);
   IRTemp z3    = newTemp(Ity_I64);
   IRTemp z2    = newTemp(Ity_I64);
   IRTemp z1    = newTemp(Ity_I64);
   IRTemp z0    = newTemp(Ity_I64);
   IRTemp ab7, ab6, ab5, ab4, ab3, ab2, ab1, ab0;
   IRTemp c3, c2, c1, c0;

   ab7 = ab6 = ab5 = ab4 = ab3 = ab2 = ab1 = ab0 = IRTemp_INVALID;
   c3 = c2 = c1 = c0 = IRTemp_INVALID;

   assign( vA, getVReg(vA_addr));
   assign( vB, getVReg(vB_addr));
   assign( vC, getVReg(vC_addr));
   assign( zeros, unop(Iop_Dup32x4, mkU32(0)) );

   if (opc1 != 0x4) {
      vex_printf("dis_av_multarith(ppc)(instr)\n");
      return False;
   }

   switch (opc2) {
   
   case 0x20: { 
      IRTemp cSigns = newTemp(Ity_V128);
      DIP("vmhaddshs v%d,v%d,v%d,v%d\n",
          vD_addr, vA_addr, vB_addr, vC_addr);
      assign(cSigns, binop(Iop_CmpGT16Sx8, mkexpr(zeros), mkexpr(vC)));
      assign(aLo, binop(Iop_InterleaveLO16x8, mkexpr(zeros), mkexpr(vA)));
      assign(bLo, binop(Iop_InterleaveLO16x8, mkexpr(zeros), mkexpr(vB)));
      assign(cLo, binop(Iop_InterleaveLO16x8, mkexpr(cSigns),mkexpr(vC)));
      assign(aHi, binop(Iop_InterleaveHI16x8, mkexpr(zeros), mkexpr(vA)));
      assign(bHi, binop(Iop_InterleaveHI16x8, mkexpr(zeros), mkexpr(vB)));
      assign(cHi, binop(Iop_InterleaveHI16x8, mkexpr(cSigns),mkexpr(vC)));

      assign( zLo, binop(Iop_Add32x4, mkexpr(cLo),
                         binop(Iop_SarN32x4,
                               binop(Iop_MullEven16Sx8,
                                     mkexpr(aLo), mkexpr(bLo)),
                               mkU8(15))) );

      assign( zHi, binop(Iop_Add32x4, mkexpr(cHi),
                         binop(Iop_SarN32x4,
                               binop(Iop_MullEven16Sx8,
                                     mkexpr(aHi), mkexpr(bHi)),
                               mkU8(15))) );

      putVReg( vD_addr,
               binop(Iop_QNarrowBin32Sto16Sx8, mkexpr(zHi), mkexpr(zLo)) );
      break;
   }
   case 0x21: { 
      IRTemp zKonst = newTemp(Ity_V128);
      IRTemp cSigns = newTemp(Ity_V128);
      DIP("vmhraddshs v%d,v%d,v%d,v%d\n",
          vD_addr, vA_addr, vB_addr, vC_addr);
      assign(cSigns, binop(Iop_CmpGT16Sx8, mkexpr(zeros), mkexpr(vC)) );
      assign(aLo, binop(Iop_InterleaveLO16x8, mkexpr(zeros), mkexpr(vA)));
      assign(bLo, binop(Iop_InterleaveLO16x8, mkexpr(zeros), mkexpr(vB)));
      assign(cLo, binop(Iop_InterleaveLO16x8, mkexpr(cSigns),mkexpr(vC)));
      assign(aHi, binop(Iop_InterleaveHI16x8, mkexpr(zeros), mkexpr(vA)));
      assign(bHi, binop(Iop_InterleaveHI16x8, mkexpr(zeros), mkexpr(vB)));
      assign(cHi, binop(Iop_InterleaveHI16x8, mkexpr(cSigns),mkexpr(vC)));

      
      assign( zKonst, binop(Iop_ShlN32x4, unop(Iop_Dup32x4, mkU32(0x1)),
                            mkU8(14)) );

      assign( zLo, binop(Iop_Add32x4, mkexpr(cLo),
                         binop(Iop_SarN32x4,
                               binop(Iop_Add32x4, mkexpr(zKonst),
                                     binop(Iop_MullEven16Sx8,
                                           mkexpr(aLo), mkexpr(bLo))),
                               mkU8(15))) );

      assign( zHi, binop(Iop_Add32x4, mkexpr(cHi),
                         binop(Iop_SarN32x4,
                               binop(Iop_Add32x4, mkexpr(zKonst),
                                     binop(Iop_MullEven16Sx8,
                                           mkexpr(aHi), mkexpr(bHi))),
                               mkU8(15))) );

      putVReg( vD_addr,
               binop(Iop_QNarrowBin32Sto16Sx8, mkexpr(zHi), mkexpr(zLo)) );
      break;
   }
   case 0x22: { 
      DIP("vmladduhm v%d,v%d,v%d,v%d\n",
          vD_addr, vA_addr, vB_addr, vC_addr);
      assign(aLo, binop(Iop_InterleaveLO16x8, mkexpr(zeros), mkexpr(vA)));
      assign(bLo, binop(Iop_InterleaveLO16x8, mkexpr(zeros), mkexpr(vB)));
      assign(cLo, binop(Iop_InterleaveLO16x8, mkexpr(zeros), mkexpr(vC)));
      assign(aHi, binop(Iop_InterleaveHI16x8, mkexpr(zeros), mkexpr(vA)));
      assign(bHi, binop(Iop_InterleaveHI16x8, mkexpr(zeros), mkexpr(vB)));
      assign(cHi, binop(Iop_InterleaveHI16x8, mkexpr(zeros), mkexpr(vC)));
      assign(zLo, binop(Iop_Add32x4,
                     binop(Iop_MullEven16Ux8, mkexpr(aLo), mkexpr(bLo)),
                     mkexpr(cLo)) );
      assign(zHi, binop(Iop_Add32x4,
                     binop(Iop_MullEven16Ux8, mkexpr(aHi), mkexpr(bHi)),
                     mkexpr(cHi)));
      putVReg( vD_addr,
               binop(Iop_NarrowBin32to16x8, mkexpr(zHi), mkexpr(zLo)) );
      break;
   }


   
   case 0x24: { 
      IRTemp abEE, abEO, abOE, abOO;
      abEE = abEO = abOE = abOO = IRTemp_INVALID;
      DIP("vmsumubm v%d,v%d,v%d,v%d\n",
          vD_addr, vA_addr, vB_addr, vC_addr);

      
      assign( abEvn, MK_Iop_MullOdd8Ux16( mkexpr(vA), mkexpr(vB) ));
      assign( abOdd, binop(Iop_MullEven8Ux16, mkexpr(vA), mkexpr(vB)) );
      
      
      expand16Ux8( mkexpr(abEvn), &abEE, &abEO );
      expand16Ux8( mkexpr(abOdd), &abOE, &abOO );
      
      putVReg( vD_addr,
         binop(Iop_Add32x4, mkexpr(vC),
               binop(Iop_Add32x4,
                     binop(Iop_Add32x4, mkexpr(abEE), mkexpr(abEO)),
                     binop(Iop_Add32x4, mkexpr(abOE), mkexpr(abOO)))) );
      break;
   }
   case 0x25: { 
      IRTemp aEvn, aOdd, bEvn, bOdd;
      IRTemp abEE = newTemp(Ity_V128);
      IRTemp abEO = newTemp(Ity_V128);
      IRTemp abOE = newTemp(Ity_V128);
      IRTemp abOO = newTemp(Ity_V128);
      aEvn = aOdd = bEvn = bOdd = IRTemp_INVALID;
      DIP("vmsummbm v%d,v%d,v%d,v%d\n",
          vD_addr, vA_addr, vB_addr, vC_addr);

      expand8Sx16( mkexpr(vA), &aEvn, &aOdd );
      expand8Ux16( mkexpr(vB), &bEvn, &bOdd );

      
      assign( abEE, MK_Iop_MullOdd16Sx8( mkexpr(aEvn), mkexpr(bEvn) ));
      assign( abEO, binop(Iop_MullEven16Sx8, mkexpr(aEvn), mkexpr(bEvn)) );
      assign( abOE, MK_Iop_MullOdd16Sx8( mkexpr(aOdd), mkexpr(bOdd) ));
      assign( abOO, binop(Iop_MullEven16Sx8, mkexpr(aOdd), mkexpr(bOdd)) );

      
      putVReg( vD_addr,
         binop(Iop_QAdd32Sx4, mkexpr(vC),
               binop(Iop_QAdd32Sx4,
                     binop(Iop_QAdd32Sx4, mkexpr(abEE), mkexpr(abEO)),
                     binop(Iop_QAdd32Sx4, mkexpr(abOE), mkexpr(abOO)))) );
      break;
   }
   case 0x26: { 
      DIP("vmsumuhm v%d,v%d,v%d,v%d\n",
          vD_addr, vA_addr, vB_addr, vC_addr);
      assign( abEvn, MK_Iop_MullOdd16Ux8( mkexpr(vA), mkexpr(vB) ));
      assign( abOdd, binop(Iop_MullEven16Ux8, mkexpr(vA), mkexpr(vB)) );
      putVReg( vD_addr,
         binop(Iop_Add32x4, mkexpr(vC),
               binop(Iop_Add32x4, mkexpr(abEvn), mkexpr(abOdd))) );
      break;
   }
   case 0x27: { 
      DIP("vmsumuhs v%d,v%d,v%d,v%d\n",
          vD_addr, vA_addr, vB_addr, vC_addr);
      
      assign( abEvn, MK_Iop_MullOdd16Ux8(mkexpr(vA), mkexpr(vB) ));
      assign( abOdd, binop(Iop_MullEven16Ux8, mkexpr(vA), mkexpr(vB)) );

      
      breakV128to4x64U( mkexpr(abEvn), &ab7, &ab5, &ab3, &ab1 );
      breakV128to4x64U( mkexpr(abOdd), &ab6, &ab4, &ab2, &ab0 );
      breakV128to4x64U( mkexpr(vC),    &c3,  &c2,  &c1,  &c0  );

      
      assign( z3, binop(Iop_Add64, mkexpr(c3),
                        binop(Iop_Add64, mkexpr(ab7), mkexpr(ab6))));
      assign( z2, binop(Iop_Add64, mkexpr(c2),
                        binop(Iop_Add64, mkexpr(ab5), mkexpr(ab4))));
      assign( z1, binop(Iop_Add64, mkexpr(c1),
                        binop(Iop_Add64, mkexpr(ab3), mkexpr(ab2))));
      assign( z0, binop(Iop_Add64, mkexpr(c0),
                        binop(Iop_Add64, mkexpr(ab1), mkexpr(ab0))));

      
      putVReg( vD_addr, mkV128from4x64U( mkexpr(z3), mkexpr(z2),
                                         mkexpr(z1), mkexpr(z0)) );

      break;
   }
   case 0x28: { 
      DIP("vmsumshm v%d,v%d,v%d,v%d\n",
          vD_addr, vA_addr, vB_addr, vC_addr);
      assign( abEvn, MK_Iop_MullOdd16Sx8( mkexpr(vA), mkexpr(vB) ));
      assign( abOdd, binop(Iop_MullEven16Sx8, mkexpr(vA), mkexpr(vB)) );
      putVReg( vD_addr,
         binop(Iop_Add32x4, mkexpr(vC),
               binop(Iop_Add32x4, mkexpr(abOdd), mkexpr(abEvn))) );
      break;
   }
   case 0x29: { 
      DIP("vmsumshs v%d,v%d,v%d,v%d\n",
          vD_addr, vA_addr, vB_addr, vC_addr);
      
      assign( abEvn, MK_Iop_MullOdd16Sx8( mkexpr(vA), mkexpr(vB) ));
      assign( abOdd, binop(Iop_MullEven16Sx8, mkexpr(vA), mkexpr(vB)) );

      
      breakV128to4x64S( mkexpr(abEvn), &ab7, &ab5, &ab3, &ab1 );
      breakV128to4x64S( mkexpr(abOdd), &ab6, &ab4, &ab2, &ab0 );
      breakV128to4x64S( mkexpr(vC),    &c3,  &c2,  &c1,  &c0  );

      
      assign( z3, binop(Iop_Add64, mkexpr(c3),
                        binop(Iop_Add64, mkexpr(ab7), mkexpr(ab6))));
      assign( z2, binop(Iop_Add64, mkexpr(c2),
                        binop(Iop_Add64, mkexpr(ab5), mkexpr(ab4))));
      assign( z1, binop(Iop_Add64, mkexpr(c1),
                        binop(Iop_Add64, mkexpr(ab3), mkexpr(ab2))));
      assign( z0, binop(Iop_Add64, mkexpr(c0),
                        binop(Iop_Add64, mkexpr(ab1), mkexpr(ab0))));

      
      putVReg( vD_addr, mkV128from4x64S( mkexpr(z3), mkexpr(z2),
                                         mkexpr(z1), mkexpr(z0)) );
      break;
   }
   default:
      vex_printf("dis_av_multarith(ppc)(opc2)\n");
      return False;
   }
   return True;
}

static Bool dis_av_polymultarith ( UInt theInstr )
{
   
   UChar opc1     = ifieldOPC(theInstr);
   UChar vD_addr  = ifieldRegDS(theInstr);
   UChar vA_addr  = ifieldRegA(theInstr);
   UChar vB_addr  = ifieldRegB(theInstr);
   UChar vC_addr  = ifieldRegC(theInstr);
   UInt  opc2     = IFIELD(theInstr, 0, 11);
   IRTemp vA    = newTemp(Ity_V128);
   IRTemp vB    = newTemp(Ity_V128);
   IRTemp vC    = newTemp(Ity_V128);

   assign( vA, getVReg(vA_addr));
   assign( vB, getVReg(vB_addr));
   assign( vC, getVReg(vC_addr));

   if (opc1 != 0x4) {
      vex_printf("dis_av_polymultarith(ppc)(instr)\n");
      return False;
   }

   switch (opc2) {
      
      case 0x408:  
         DIP("vpmsumb v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
         putVReg( vD_addr, binop(Iop_PolynomialMulAdd8x16,
                                 mkexpr(vA), mkexpr(vB)) );
         break;
      case 0x448:  
         DIP("vpmsumd v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
         putVReg( vD_addr, binop(Iop_PolynomialMulAdd64x2,
                                 mkexpr(vA), mkexpr(vB)) );
         break;
      case 0x488:  
         DIP("vpmsumw v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
         putVReg( vD_addr, binop(Iop_PolynomialMulAdd32x4,
                                 mkexpr(vA), mkexpr(vB)) );
         break;
      case 0x4C8:  
         DIP("vpmsumh v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
         putVReg( vD_addr, binop(Iop_PolynomialMulAdd16x8,
                                 mkexpr(vA), mkexpr(vB)) );
         break;
      default:
         vex_printf("dis_av_polymultarith(ppc)(opc2=0x%x)\n", opc2);
         return False;
   }
   return True;
}

static Bool dis_av_shift ( UInt theInstr )
{
   
   UChar opc1    = ifieldOPC(theInstr);
   UChar vD_addr = ifieldRegDS(theInstr);
   UChar vA_addr = ifieldRegA(theInstr);
   UChar vB_addr = ifieldRegB(theInstr);
   UInt  opc2    = IFIELD( theInstr, 0, 11 );

   IRTemp vA = newTemp(Ity_V128);
   IRTemp vB = newTemp(Ity_V128);
   assign( vA, getVReg(vA_addr));
   assign( vB, getVReg(vB_addr));

   if (opc1 != 0x4){
      vex_printf("dis_av_shift(ppc)(instr)\n");
      return False;
   }

   switch (opc2) {
   
   case 0x004: 
      DIP("vrlb v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Rol8x16, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x044: 
      DIP("vrlh v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Rol16x8, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x084: 
      DIP("vrlw v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Rol32x4, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x0C4: 
      DIP("vrld v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Rol64x2, mkexpr(vA), mkexpr(vB)) );
      break;


   
   case 0x104: 
      DIP("vslb v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Shl8x16, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x144: 
      DIP("vslh v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Shl16x8, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x184: 
      DIP("vslw v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Shl32x4, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x5C4: 
      DIP("vsld v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Shl64x2, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x1C4: { 
      IRTemp sh = newTemp(Ity_I8);
      DIP("vsl v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      assign( sh, binop(Iop_And8, mkU8(0x7),
                        unop(Iop_32to8,
                             unop(Iop_V128to32, mkexpr(vB)))) );
      putVReg( vD_addr,
               binop(Iop_ShlV128, mkexpr(vA), mkexpr(sh)) );
      break;
   }
   case 0x40C: { 
      IRTemp sh = newTemp(Ity_I8);
      DIP("vslo v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      assign( sh, binop(Iop_And8, mkU8(0x78),
                        unop(Iop_32to8,
                             unop(Iop_V128to32, mkexpr(vB)))) );
      putVReg( vD_addr,
               binop(Iop_ShlV128, mkexpr(vA), mkexpr(sh)) );
      break;
   }


   
   case 0x204: 
      DIP("vsrb v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Shr8x16, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x244: 
      DIP("vsrh v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Shr16x8, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x284: 
      DIP("vsrw v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Shr32x4, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x2C4: { 
      IRTemp sh = newTemp(Ity_I8);
      DIP("vsr v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      assign( sh, binop(Iop_And8, mkU8(0x7),
                        unop(Iop_32to8,
                             unop(Iop_V128to32, mkexpr(vB)))) );
      putVReg( vD_addr,
               binop(Iop_ShrV128, mkexpr(vA), mkexpr(sh)) );
      break;
   }
   case 0x304: 
      DIP("vsrab v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Sar8x16, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x344: 
      DIP("vsrah v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Sar16x8, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x384: 
      DIP("vsraw v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Sar32x4, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x3C4: 
      DIP("vsrad v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Sar64x2, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x44C: { 
      IRTemp sh = newTemp(Ity_I8);
      DIP("vsro v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      assign( sh, binop(Iop_And8, mkU8(0x78),
                        unop(Iop_32to8,
                             unop(Iop_V128to32, mkexpr(vB)))) );
      putVReg( vD_addr,
               binop(Iop_ShrV128, mkexpr(vA), mkexpr(sh)) );
      break;
   }

   case 0x6C4: 
      DIP("vsrd v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Shr64x2, mkexpr(vA), mkexpr(vB)) );
      break;


   default:
      vex_printf("dis_av_shift(ppc)(opc2)\n");
      return False;
   }
   return True;
}

static Bool dis_av_permute ( UInt theInstr )
{
   
   UChar opc1      = ifieldOPC(theInstr);
   UChar vD_addr   = ifieldRegDS(theInstr);
   UChar vA_addr   = ifieldRegA(theInstr);
   UChar UIMM_5    = vA_addr;
   UChar vB_addr   = ifieldRegB(theInstr);
   UChar vC_addr   = ifieldRegC(theInstr);
   UChar b10       = ifieldBIT10(theInstr);
   UChar SHB_uimm4 = toUChar( IFIELD( theInstr, 6, 4 ) );
   UInt  opc2      = toUChar( IFIELD( theInstr, 0, 6 ) );

   UChar SIMM_8 = extend_s_5to8(UIMM_5);

   IRTemp vA = newTemp(Ity_V128);
   IRTemp vB = newTemp(Ity_V128);
   IRTemp vC = newTemp(Ity_V128);
   assign( vA, getVReg(vA_addr));
   assign( vB, getVReg(vB_addr));
   assign( vC, getVReg(vC_addr));

   if (opc1 != 0x4) {
      vex_printf("dis_av_permute(ppc)(instr)\n");
      return False;
   }

   switch (opc2) {
   case 0x2A: 
      DIP("vsel v%d,v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr, vC_addr);
      
      putVReg( vD_addr, binop(Iop_OrV128,
         binop(Iop_AndV128, mkexpr(vA), unop(Iop_NotV128, mkexpr(vC))),
         binop(Iop_AndV128, mkexpr(vB), mkexpr(vC))) );
      return True;
     
   case 0x2B: { 
      
      IRTemp a_perm  = newTemp(Ity_V128);
      IRTemp b_perm  = newTemp(Ity_V128);
      IRTemp mask    = newTemp(Ity_V128);
      IRTemp vC_andF = newTemp(Ity_V128);
      DIP("vperm v%d,v%d,v%d,v%d\n",
          vD_addr, vA_addr, vB_addr, vC_addr);
      assign( vC_andF,
              binop(Iop_AndV128, mkexpr(vC),
                                 unop(Iop_Dup8x16, mkU8(0xF))) );
      assign( a_perm,
              binop(Iop_Perm8x16, mkexpr(vA), mkexpr(vC_andF)) );
      assign( b_perm,
              binop(Iop_Perm8x16, mkexpr(vB), mkexpr(vC_andF)) );
      
      assign( mask, binop(Iop_SarN8x16,
                          binop(Iop_ShlN8x16, mkexpr(vC), mkU8(3)),
                          mkU8(7)) );
      
      putVReg( vD_addr, binop(Iop_OrV128,
                              binop(Iop_AndV128, mkexpr(a_perm),
                                    unop(Iop_NotV128, mkexpr(mask))),
                              binop(Iop_AndV128, mkexpr(b_perm),
                                    mkexpr(mask))) );
      return True;
   }
   case 0x2C: 
      if (b10 != 0) {
         vex_printf("dis_av_permute(ppc)(vsldoi)\n");
         return False;
      }
      DIP("vsldoi v%d,v%d,v%d,%d\n",
          vD_addr, vA_addr, vB_addr, SHB_uimm4);
      if (SHB_uimm4 == 0)
         putVReg( vD_addr, mkexpr(vA) );
      else
         putVReg( vD_addr,
            binop(Iop_OrV128,
                  binop(Iop_ShlV128, mkexpr(vA), mkU8(SHB_uimm4*8)),
                  binop(Iop_ShrV128, mkexpr(vB), mkU8((16-SHB_uimm4)*8))) );
      return True;
   case 0x2D: {  
      IRTemp a_perm  = newTemp(Ity_V128);
      IRTemp b_perm  = newTemp(Ity_V128);
      IRTemp vrc_a   = newTemp(Ity_V128);
      IRTemp vrc_b   = newTemp(Ity_V128);

      
      assign( vrc_b, binop( Iop_AndV128, mkexpr( vC ),
                            unop( Iop_Dup8x16, mkU8( 0xF ) ) ) );
      assign( vrc_a, binop( Iop_ShrV128,
                            binop( Iop_AndV128, mkexpr( vC ),
                                   unop( Iop_Dup8x16, mkU8( 0xF0 ) ) ),
                            mkU8 ( 4 ) ) );
      assign( a_perm, binop( Iop_Perm8x16, mkexpr( vA ), mkexpr( vrc_a ) ) );
      assign( b_perm, binop( Iop_Perm8x16, mkexpr( vB ), mkexpr( vrc_b ) ) );
      putVReg( vD_addr, binop( Iop_XorV128,
                               mkexpr( a_perm ), mkexpr( b_perm) ) );
      return True;
   }
   default:
     break; 
   }

   opc2 = IFIELD( theInstr, 0, 11 );
   switch (opc2) {

   
   case 0x00C: 
      DIP("vmrghb v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr,
               binop(Iop_InterleaveHI8x16, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x04C: 
      DIP("vmrghh v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr,
               binop(Iop_InterleaveHI16x8, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x08C: 
      DIP("vmrghw v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr,
               binop(Iop_InterleaveHI32x4, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x10C: 
      DIP("vmrglb v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr,
               binop(Iop_InterleaveLO8x16, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x14C: 
      DIP("vmrglh v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr,
               binop(Iop_InterleaveLO16x8, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x18C: 
      DIP("vmrglw v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr,
               binop(Iop_InterleaveLO32x4, mkexpr(vA), mkexpr(vB)) );
      break;


   
   case 0x20C: { 
      
      UChar sh_uimm = (15 - (UIMM_5 & 15)) * 8;
      DIP("vspltb v%d,v%d,%d\n", vD_addr, vB_addr, UIMM_5);
      putVReg( vD_addr, unop(Iop_Dup8x16,
           unop(Iop_32to8, unop(Iop_V128to32, 
                binop(Iop_ShrV128, mkexpr(vB), mkU8(sh_uimm))))) );
      break;
   }
   case 0x24C: { 
      UChar sh_uimm = (7 - (UIMM_5 & 7)) * 16;
      DIP("vsplth v%d,v%d,%d\n", vD_addr, vB_addr, UIMM_5);
      putVReg( vD_addr, unop(Iop_Dup16x8,
           unop(Iop_32to16, unop(Iop_V128to32, 
                binop(Iop_ShrV128, mkexpr(vB), mkU8(sh_uimm))))) );
      break;
   }
   case 0x28C: { 
      
      UChar sh_uimm = (3 - (UIMM_5 & 3)) * 32;
      DIP("vspltw v%d,v%d,%d\n", vD_addr, vB_addr, UIMM_5);
      putVReg( vD_addr, unop(Iop_Dup32x4,
         unop(Iop_V128to32,
              binop(Iop_ShrV128, mkexpr(vB), mkU8(sh_uimm)))) );
      break;
   }
   case 0x30C: 
      DIP("vspltisb v%d,%d\n", vD_addr, (Char)SIMM_8);
      putVReg( vD_addr, unop(Iop_Dup8x16, mkU8(SIMM_8)) );
      break;

   case 0x34C: 
      DIP("vspltish v%d,%d\n", vD_addr, (Char)SIMM_8);
      putVReg( vD_addr,
               unop(Iop_Dup16x8, mkU16(extend_s_8to32(SIMM_8))) );
      break;

   case 0x38C: 
      DIP("vspltisw v%d,%d\n", vD_addr, (Char)SIMM_8);
      putVReg( vD_addr,
               unop(Iop_Dup32x4, mkU32(extend_s_8to32(SIMM_8))) );
      break;

   case 0x68C: 
     DIP("vmrgow v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr,
               binop(Iop_CatOddLanes32x4, mkexpr(vA), mkexpr(vB) ) );
      break;

   case 0x78C: 
      DIP("vmrgew v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr,
               binop(Iop_CatEvenLanes32x4, mkexpr(vA), mkexpr(vB) ) );
      break;

   default:
      vex_printf("dis_av_permute(ppc)(opc2)\n");
      return False;
   }
   return True;
}

static Bool dis_av_pack ( UInt theInstr )
{
   
   UChar opc1     = ifieldOPC(theInstr);
   UChar vD_addr  = ifieldRegDS(theInstr);
   UChar vA_addr  = ifieldRegA(theInstr);
   UChar vB_addr  = ifieldRegB(theInstr);
   UInt  opc2     = IFIELD( theInstr, 0, 11 );

   IRTemp signs = IRTemp_INVALID;
   IRTemp zeros = IRTemp_INVALID;
   IRTemp vA    = newTemp(Ity_V128);
   IRTemp vB    = newTemp(Ity_V128);
   assign( vA, getVReg(vA_addr));
   assign( vB, getVReg(vB_addr));

   if (opc1 != 0x4) {
      vex_printf("dis_av_pack(ppc)(instr)\n");
      return False;
   }
   switch (opc2) {
   
   case 0x00E: 
      DIP("vpkuhum v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr,
               binop(Iop_NarrowBin16to8x16, mkexpr(vA), mkexpr(vB)) );
      return True;

   case 0x04E: 
      DIP("vpkuwum v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr,
               binop(Iop_NarrowBin32to16x8, mkexpr(vA), mkexpr(vB)) );
      return True;

   case 0x08E: 
      DIP("vpkuhus v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr,
               binop(Iop_QNarrowBin16Uto8Ux16, mkexpr(vA), mkexpr(vB)) );
      
      return True;

   case 0x0CE: 
      DIP("vpkuwus v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr,
               binop(Iop_QNarrowBin32Uto16Ux8, mkexpr(vA), mkexpr(vB)) );
      
      return True;

   case 0x10E: { 
      
      
      
      IRTemp vA_tmp = newTemp(Ity_V128);
      IRTemp vB_tmp = newTemp(Ity_V128);
      DIP("vpkshus v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      assign( vA_tmp, binop(Iop_AndV128, mkexpr(vA),
                            unop(Iop_NotV128,
                                 binop(Iop_SarN16x8,
                                       mkexpr(vA), mkU8(15)))) );
      assign( vB_tmp, binop(Iop_AndV128, mkexpr(vB),
                            unop(Iop_NotV128,
                                 binop(Iop_SarN16x8,
                                       mkexpr(vB), mkU8(15)))) );
      putVReg( vD_addr, binop(Iop_QNarrowBin16Uto8Ux16,
                              mkexpr(vA_tmp), mkexpr(vB_tmp)) );
      
      return True;
   }
   case 0x14E: { 
      
      
      
      IRTemp vA_tmp = newTemp(Ity_V128);
      IRTemp vB_tmp = newTemp(Ity_V128);
      DIP("vpkswus v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      assign( vA_tmp, binop(Iop_AndV128, mkexpr(vA),
                            unop(Iop_NotV128,
                                 binop(Iop_SarN32x4,
                                       mkexpr(vA), mkU8(31)))) );
      assign( vB_tmp, binop(Iop_AndV128, mkexpr(vB),
                            unop(Iop_NotV128,
                                 binop(Iop_SarN32x4,
                                       mkexpr(vB), mkU8(31)))) );
      putVReg( vD_addr, binop(Iop_QNarrowBin32Uto16Ux8,
                              mkexpr(vA_tmp), mkexpr(vB_tmp)) );
      
      return True;
   }
   case 0x18E: 
      DIP("vpkshss v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr,
               binop(Iop_QNarrowBin16Sto8Sx16, mkexpr(vA), mkexpr(vB)) );
      
      return True;

   case 0x1CE: 
      DIP("vpkswss v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr,
               binop(Iop_QNarrowBin32Sto16Sx8, mkexpr(vA), mkexpr(vB)) );
      
      return True;

   case 0x30E: { 
      
      
      IRTemp a1 = newTemp(Ity_V128);
      IRTemp a2 = newTemp(Ity_V128);
      IRTemp a3 = newTemp(Ity_V128);
      IRTemp a_tmp = newTemp(Ity_V128);
      IRTemp b1 = newTemp(Ity_V128);
      IRTemp b2 = newTemp(Ity_V128);
      IRTemp b3 = newTemp(Ity_V128);
      IRTemp b_tmp = newTemp(Ity_V128);
      DIP("vpkpx v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      assign( a1, binop(Iop_ShlN16x8,
                        binop(Iop_ShrN32x4, mkexpr(vA), mkU8(19)),
                        mkU8(10)) );
      assign( a2, binop(Iop_ShlN16x8, 
                        binop(Iop_ShrN16x8, mkexpr(vA), mkU8(11)),
                        mkU8(5)) );
      assign( a3,  binop(Iop_ShrN16x8, 
                         binop(Iop_ShlN16x8, mkexpr(vA), mkU8(8)),
                         mkU8(11)) );
      assign( a_tmp, binop(Iop_OrV128, mkexpr(a1),
                           binop(Iop_OrV128, mkexpr(a2), mkexpr(a3))) );

      assign( b1, binop(Iop_ShlN16x8,
                        binop(Iop_ShrN32x4, mkexpr(vB), mkU8(19)),
                        mkU8(10)) );
      assign( b2, binop(Iop_ShlN16x8, 
                        binop(Iop_ShrN16x8, mkexpr(vB), mkU8(11)),
                        mkU8(5)) );
      assign( b3,  binop(Iop_ShrN16x8, 
                         binop(Iop_ShlN16x8, mkexpr(vB), mkU8(8)),
                         mkU8(11)) );
      assign( b_tmp, binop(Iop_OrV128, mkexpr(b1),
                           binop(Iop_OrV128, mkexpr(b2), mkexpr(b3))) );

      putVReg( vD_addr, binop(Iop_NarrowBin32to16x8,
                              mkexpr(a_tmp), mkexpr(b_tmp)) );
      return True;
   }

   case 0x44E: 
      DIP("vpkudum v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr,
               binop(Iop_NarrowBin64to32x4, mkexpr(vA), mkexpr(vB)) );
      return True;

   case 0x4CE: 
      DIP("vpkudus v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr,
               binop(Iop_QNarrowBin64Uto32Ux4, mkexpr(vA), mkexpr(vB)) );
      
      return True;

   case 0x54E: { 
      
      
      
      
      
      IRTemp vA_tmp = newTemp(Ity_V128);
      IRTemp vB_tmp = newTemp(Ity_V128);
      DIP("vpksdus v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      assign( vA_tmp, binop(Iop_AndV128, mkexpr(vA),
                            unop(Iop_NotV128,
                                 binop(Iop_SarN64x2,
                                       mkexpr(vA), mkU8(63)))) );
      assign( vB_tmp, binop(Iop_AndV128, mkexpr(vB),
                            unop(Iop_NotV128,
                                 binop(Iop_SarN64x2,
                                       mkexpr(vB), mkU8(63)))) );
      putVReg( vD_addr, binop(Iop_QNarrowBin64Uto32Ux4,
                              mkexpr(vA_tmp), mkexpr(vB_tmp)) );
      
      return True;
   }

   case 0x5CE: 
      DIP("vpksdss v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr,
               binop(Iop_QNarrowBin64Sto32Sx4, mkexpr(vA), mkexpr(vB)) );
      
      return True;
   default:
      break; 
   }


   if (vA_addr != 0) {
      vex_printf("dis_av_pack(ppc)(vA_addr)\n");
      return False;
   }

   signs = newTemp(Ity_V128);
   zeros = newTemp(Ity_V128);
   assign( zeros, unop(Iop_Dup32x4, mkU32(0)) );

   switch (opc2) {
   
   case 0x20E: { 
      DIP("vupkhsb v%d,v%d\n", vD_addr, vB_addr);
      assign( signs, binop(Iop_CmpGT8Sx16, mkexpr(zeros), mkexpr(vB)) );
      putVReg( vD_addr,
               binop(Iop_InterleaveHI8x16, mkexpr(signs), mkexpr(vB)) );
      break;
   }
   case 0x24E: { 
      DIP("vupkhsh v%d,v%d\n", vD_addr, vB_addr);
      assign( signs, binop(Iop_CmpGT16Sx8, mkexpr(zeros), mkexpr(vB)) );
      putVReg( vD_addr,
               binop(Iop_InterleaveHI16x8, mkexpr(signs), mkexpr(vB)) );
      break;
   }
   case 0x28E: { 
      DIP("vupklsb v%d,v%d\n", vD_addr, vB_addr);
      assign( signs, binop(Iop_CmpGT8Sx16, mkexpr(zeros), mkexpr(vB)) );
      putVReg( vD_addr,
               binop(Iop_InterleaveLO8x16, mkexpr(signs), mkexpr(vB)) );
      break;
   }
   case 0x2CE: { 
      DIP("vupklsh v%d,v%d\n", vD_addr, vB_addr);
      assign( signs, binop(Iop_CmpGT16Sx8, mkexpr(zeros), mkexpr(vB)) );
      putVReg( vD_addr,
               binop(Iop_InterleaveLO16x8, mkexpr(signs), mkexpr(vB)) );
      break;
   }
   case 0x34E: { 
      
      
      IRTemp z0  = newTemp(Ity_V128);
      IRTemp z1  = newTemp(Ity_V128);
      IRTemp z01 = newTemp(Ity_V128);
      IRTemp z2  = newTemp(Ity_V128);
      IRTemp z3  = newTemp(Ity_V128);
      IRTemp z23 = newTemp(Ity_V128);
      DIP("vupkhpx v%d,v%d\n", vD_addr, vB_addr);
      assign( z0,  binop(Iop_ShlN16x8,
                         binop(Iop_SarN16x8, mkexpr(vB), mkU8(15)),
                         mkU8(8)) );
      assign( z1,  binop(Iop_ShrN16x8, 
                         binop(Iop_ShlN16x8, mkexpr(vB), mkU8(1)),
                         mkU8(11)) );
      assign( z01, binop(Iop_InterleaveHI16x8, mkexpr(zeros),
                         binop(Iop_OrV128, mkexpr(z0), mkexpr(z1))) );
      assign( z2,  binop(Iop_ShrN16x8,
                         binop(Iop_ShlN16x8, 
                               binop(Iop_ShrN16x8, mkexpr(vB), mkU8(5)),
                               mkU8(11)),
                         mkU8(3)) );
      assign( z3,  binop(Iop_ShrN16x8, 
                         binop(Iop_ShlN16x8, mkexpr(vB), mkU8(11)),
                         mkU8(11)) );
      assign( z23, binop(Iop_InterleaveHI16x8, mkexpr(zeros),
                         binop(Iop_OrV128, mkexpr(z2), mkexpr(z3))) );
      putVReg( vD_addr,
               binop(Iop_OrV128,
                     binop(Iop_ShlN32x4, mkexpr(z01), mkU8(16)),
                     mkexpr(z23)) );
      break;
   }
   case 0x3CE: { 
      
      IRTemp z0  = newTemp(Ity_V128);
      IRTemp z1  = newTemp(Ity_V128);
      IRTemp z01 = newTemp(Ity_V128);
      IRTemp z2  = newTemp(Ity_V128);
      IRTemp z3  = newTemp(Ity_V128);
      IRTemp z23 = newTemp(Ity_V128);
      DIP("vupklpx v%d,v%d\n", vD_addr, vB_addr);
      assign( z0,  binop(Iop_ShlN16x8,
                         binop(Iop_SarN16x8, mkexpr(vB), mkU8(15)),
                         mkU8(8)) );
      assign( z1,  binop(Iop_ShrN16x8, 
                         binop(Iop_ShlN16x8, mkexpr(vB), mkU8(1)),
                         mkU8(11)) );
      assign( z01, binop(Iop_InterleaveLO16x8, mkexpr(zeros),
                         binop(Iop_OrV128, mkexpr(z0), mkexpr(z1))) );
      assign( z2,  binop(Iop_ShrN16x8,
                         binop(Iop_ShlN16x8, 
                               binop(Iop_ShrN16x8, mkexpr(vB), mkU8(5)),
                               mkU8(11)),
                         mkU8(3)) );
      assign( z3,  binop(Iop_ShrN16x8, 
                         binop(Iop_ShlN16x8, mkexpr(vB), mkU8(11)),
                         mkU8(11)) );
      assign( z23, binop(Iop_InterleaveLO16x8, mkexpr(zeros),
                         binop(Iop_OrV128, mkexpr(z2), mkexpr(z3))) );
      putVReg( vD_addr,
               binop(Iop_OrV128,
                     binop(Iop_ShlN32x4, mkexpr(z01), mkU8(16)),
                     mkexpr(z23)) );
      break;
   }
   case 0x64E: { 
      DIP("vupkhsw v%d,v%d\n", vD_addr, vB_addr);
      assign( signs, binop(Iop_CmpGT32Sx4, mkexpr(zeros), mkexpr(vB)) );
      putVReg( vD_addr,
               binop(Iop_InterleaveHI32x4, mkexpr(signs), mkexpr(vB)) );
      break;
   }
   case 0x6CE: { 
      DIP("vupklsw v%d,v%d\n", vD_addr, vB_addr);
      assign( signs, binop(Iop_CmpGT32Sx4, mkexpr(zeros), mkexpr(vB)) );
      putVReg( vD_addr,
               binop(Iop_InterleaveLO32x4, mkexpr(signs), mkexpr(vB)) );
      break;
   }
   default:
      vex_printf("dis_av_pack(ppc)(opc2)\n");
      return False;
   }
   return True;
}

static Bool dis_av_cipher ( UInt theInstr )
{
   
   UChar opc1     = ifieldOPC(theInstr);
   UChar vD_addr  = ifieldRegDS(theInstr);
   UChar vA_addr  = ifieldRegA(theInstr);
   UChar vB_addr  = ifieldRegB(theInstr);
   UInt  opc2     = IFIELD( theInstr, 0, 11 );

   IRTemp vA    = newTemp(Ity_V128);
   IRTemp vB    = newTemp(Ity_V128);
   assign( vA, getVReg(vA_addr));
   assign( vB, getVReg(vB_addr));

   if (opc1 != 0x4) {
      vex_printf("dis_av_cipher(ppc)(instr)\n");
      return False;
   }
   switch (opc2) {
      case 0x508: 
         DIP("vcipher v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
         putVReg( vD_addr,
                  binop(Iop_CipherV128, mkexpr(vA), mkexpr(vB)) );
         return True;

      case 0x509: 
         DIP("vcipherlast v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
         putVReg( vD_addr,
                  binop(Iop_CipherLV128, mkexpr(vA), mkexpr(vB)) );
         return True;

      case 0x548: 
         DIP("vncipher v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
         putVReg( vD_addr,
                  binop(Iop_NCipherV128, mkexpr(vA), mkexpr(vB)) );
         return True;

      case 0x549: 
         DIP("vncipherlast v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
         putVReg( vD_addr,
                  binop(Iop_NCipherLV128, mkexpr(vA), mkexpr(vB)) );
         return True;

      case 0x5C8: 
         DIP("vsbox v%d,v%d\n", vD_addr, vA_addr);
         putVReg( vD_addr,
                  unop(Iop_CipherSV128, mkexpr(vA) ) );
         return True;

      default:
         vex_printf("dis_av_cipher(ppc)(opc2)\n");
         return False;
   }
   return True;
}

static Bool dis_av_hash ( UInt theInstr )
{
   
   UChar opc1     = ifieldOPC(theInstr);
   UChar vRT_addr = ifieldRegDS(theInstr);
   UChar vRA_addr  = ifieldRegA(theInstr);
   UChar s_field  = IFIELD( theInstr, 11, 5 );  
   UChar st       = IFIELD( theInstr, 15, 1 );  
   UChar six      = IFIELD( theInstr, 11, 4 );  
   UInt  opc2     = IFIELD( theInstr, 0, 11 );

   IRTemp vA    = newTemp(Ity_V128);
   IRTemp dst    = newTemp(Ity_V128);
   assign( vA, getVReg(vRA_addr));

   if (opc1 != 0x4) {
      vex_printf("dis_av_hash(ppc)(instr)\n");
      return False;
   }

   switch (opc2) {
      case 0x682:  
         DIP("vshasigmaw v%d,v%d,%u,%u\n", vRT_addr, vRA_addr, st, six);
         assign( dst, binop( Iop_SHA256, mkexpr( vA ), mkU8( s_field) ) );
         putVReg( vRT_addr, mkexpr(dst));
         return True;

      case 0x6C2:  
         DIP("vshasigmad v%d,v%d,%u,%u\n", vRT_addr, vRA_addr, st, six);
         putVReg( vRT_addr, binop( Iop_SHA512, mkexpr( vA ), mkU8( s_field) ) );
         return True;

      default:
         vex_printf("dis_av_hash(ppc)(opc2)\n");
         return False;
   }
   return True;
}

static IRTemp _get_quad_modulo_or_carry(IRExpr * vecA, IRExpr * vecB,
                                        IRExpr * cin, Bool modulo)
{
   IRTemp _vecA_32   = IRTemp_INVALID;
   IRTemp _vecB_32   = IRTemp_INVALID;
   IRTemp res_32     = IRTemp_INVALID;
   IRTemp result     = IRTemp_INVALID;
   IRTemp tmp_result = IRTemp_INVALID;
   IRTemp carry      = IRTemp_INVALID;
   Int i;
   IRExpr * _vecA_low64 =  unop( Iop_V128to64, vecA );
   IRExpr * _vecB_low64 =  unop( Iop_V128to64, vecB );
   IRExpr * _vecA_high64 = unop( Iop_V128HIto64, vecA );
   IRExpr * _vecB_high64 = unop( Iop_V128HIto64, vecB );

   for (i = 0; i < 4; i++) {
      _vecA_32 = newTemp(Ity_I32);
      _vecB_32 = newTemp(Ity_I32);
      res_32   = newTemp(Ity_I32);
      switch (i) {
      case 0:
         assign(_vecA_32, unop( Iop_64to32, _vecA_low64 ) );
         assign(_vecB_32, unop( Iop_64to32, _vecB_low64 ) );
         break;
      case 1:
         assign(_vecA_32, unop( Iop_64HIto32, _vecA_low64 ) );
         assign(_vecB_32, unop( Iop_64HIto32, _vecB_low64 ) );
         break;
      case 2:
         assign(_vecA_32, unop( Iop_64to32, _vecA_high64 ) );
         assign(_vecB_32, unop( Iop_64to32, _vecB_high64 ) );
         break;
      case 3:
         assign(_vecA_32, unop( Iop_64HIto32, _vecA_high64 ) );
         assign(_vecB_32, unop( Iop_64HIto32, _vecB_high64 ) );
         break;
      }

      assign(res_32, binop( Iop_Add32,
                            binop( Iop_Add32,
                                   binop ( Iop_Add32,
                                           mkexpr(_vecA_32),
                                           mkexpr(_vecB_32) ),
                                   (i == 0) ? mkU32(0) : mkexpr(carry) ),
                            (i == 0) ? cin : mkU32(0) ) );
      if (modulo) {
         result = newTemp(Ity_V128);
         assign(result, binop( Iop_OrV128,
                              (i == 0) ? binop( Iop_64HLtoV128,
                                                mkU64(0),
                                                mkU64(0) ) : mkexpr(tmp_result),
                              binop( Iop_ShlV128,
                                     binop( Iop_64HLtoV128,
                                            mkU64(0),
                                            binop( Iop_32HLto64,
                                                   mkU32(0),
                                                   mkexpr(res_32) ) ),
                                     mkU8(i * 32) ) ) );
         tmp_result = newTemp(Ity_V128);
         assign(tmp_result, mkexpr(result));
      }
      carry = newTemp(Ity_I32);
      assign(carry, unop(Iop_1Uto32, binop( Iop_CmpLT32U,
                                            mkexpr(res_32),
                                            mkexpr(_vecA_32 ) ) ) );
   }
   if (modulo)
      return result;
   else
      return carry;
}


static Bool dis_av_quad ( UInt theInstr )
{
   
   UChar opc1     = ifieldOPC(theInstr);
   UChar vRT_addr = ifieldRegDS(theInstr);
   UChar vRA_addr = ifieldRegA(theInstr);
   UChar vRB_addr = ifieldRegB(theInstr);
   UChar vRC_addr;
   UInt  opc2     = IFIELD( theInstr, 0, 11 );

   IRTemp vA    = newTemp(Ity_V128);
   IRTemp vB    = newTemp(Ity_V128);
   IRTemp vC    = IRTemp_INVALID;
   IRTemp cin    = IRTemp_INVALID;
   assign( vA, getVReg(vRA_addr));
   assign( vB, getVReg(vRB_addr));

   if (opc1 != 0x4) {
      vex_printf("dis_av_quad(ppc)(instr)\n");
      return False;
   }

   switch (opc2) {
   case 0x140:  
     DIP("vaddcuq v%d,v%d,v%d\n", vRT_addr, vRA_addr, vRB_addr);
     putVReg( vRT_addr, unop( Iop_32UtoV128,
                              mkexpr(_get_quad_modulo_or_carry(mkexpr(vA),
                                                               mkexpr(vB),
                                                               mkU32(0), False) ) ) );
     return True;
   case 0x100: 
      DIP("vadduqm v%d,v%d,v%d\n", vRT_addr, vRA_addr, vRB_addr);
      putVReg( vRT_addr, mkexpr(_get_quad_modulo_or_carry(mkexpr(vA),
                                                          mkexpr(vB), mkU32(0), True) ) );
      return True;
   case 0x540: 
      DIP("vsubcuq v%d,v%d,v%d\n", vRT_addr, vRA_addr, vRB_addr);
      putVReg( vRT_addr,
               unop( Iop_32UtoV128,
                     mkexpr(_get_quad_modulo_or_carry(mkexpr(vA),
                                                      unop( Iop_NotV128,
                                                            mkexpr(vB) ),
                                                      mkU32(1), False) ) ) );
      return True;
   case 0x500: 
      DIP("vsubuqm v%d,v%d,v%d\n", vRT_addr, vRA_addr, vRB_addr);
      putVReg( vRT_addr,
               mkexpr(_get_quad_modulo_or_carry(mkexpr(vA),
                                                unop( Iop_NotV128, mkexpr(vB) ),
                                                mkU32(1), True) ) );
      return True;
   case 0x054C: 
   {
#define BPERMD_IDX_MASK 0x00000000000000FFULL
#define BPERMD_BIT_MASK 0x8000000000000000ULL
      int i;
      IRExpr * vB_expr = mkexpr(vB);
      IRExpr * res = binop(Iop_AndV128, mkV128(0), mkV128(0));
      DIP("vbpermq v%d,v%d,v%d\n", vRT_addr, vRA_addr, vRB_addr);
      for (i = 0; i < 16; i++) {
         IRTemp idx_tmp = newTemp( Ity_V128 );
         IRTemp perm_bit = newTemp( Ity_V128 );
         IRTemp idx = newTemp( Ity_I8 );
         IRTemp idx_LT127 = newTemp( Ity_I1 );
         IRTemp idx_LT127_ity128 = newTemp( Ity_V128 );

         assign( idx_tmp,
                 binop( Iop_AndV128,
                        binop( Iop_64HLtoV128,
                               mkU64(0),
                               mkU64(BPERMD_IDX_MASK) ),
                        vB_expr ) );
         assign( idx_LT127,
                 binop( Iop_CmpEQ32,
                        unop ( Iop_64to32,
                               unop( Iop_V128to64, binop( Iop_ShrV128,
                                                          mkexpr(idx_tmp),
                                                          mkU8(7) ) ) ),
                        mkU32(0) ) );

         assign( idx,
                 binop( Iop_And8,
                        unop( Iop_1Sto8,
                              mkexpr(idx_LT127) ),
                        unop( Iop_32to8,
                              unop( Iop_V128to32, mkexpr( idx_tmp ) ) ) ) );

         assign( idx_LT127_ity128,
                 binop( Iop_64HLtoV128,
                        mkU64(0),
                        unop( Iop_32Uto64,
                              unop( Iop_1Uto32, mkexpr(idx_LT127 ) ) ) ) );
         assign( perm_bit,
                 binop( Iop_AndV128,
                        mkexpr( idx_LT127_ity128 ),
                        binop( Iop_ShrV128,
                               binop( Iop_AndV128,
                                      binop (Iop_64HLtoV128,
                                             mkU64( BPERMD_BIT_MASK ),
                                             mkU64(0)),
                                      binop( Iop_ShlV128,
                                             mkexpr( vA ),
                                             mkexpr( idx ) ) ),
                               mkU8( 127 ) ) ) );
         res = binop( Iop_OrV128,
                      res,
                      binop( Iop_ShlV128,
                             mkexpr( perm_bit ),
                             mkU8( i + 64 ) ) );
         vB_expr = binop( Iop_ShrV128, vB_expr, mkU8( 8 ) );
      }
      putVReg( vRT_addr, res);
      return True;
#undef BPERMD_IDX_MASK
#undef BPERMD_BIT_MASK
   }

   default:
      break;  
   }

   opc2     = IFIELD( theInstr, 0, 6 );
   vRC_addr = ifieldRegC(theInstr);
   vC = newTemp(Ity_V128);
   cin = newTemp(Ity_I32);
   switch (opc2) {
      case 0x3D: 
         assign( vC, getVReg(vRC_addr));
         DIP("vaddecuq v%d,v%d,v%d,v%d\n", vRT_addr, vRA_addr, vRB_addr,
             vRC_addr);
         assign(cin, binop( Iop_And32,
                            unop( Iop_64to32,
                                  unop( Iop_V128to64, mkexpr(vC) ) ),
                            mkU32(1) ) );
         putVReg( vRT_addr,
                  unop( Iop_32UtoV128,
                        mkexpr(_get_quad_modulo_or_carry(mkexpr(vA), mkexpr(vB),
                                                         mkexpr(cin),
                                                         False) ) ) );
         return True;
      case 0x3C: 
         assign( vC, getVReg(vRC_addr));
         DIP("vaddeuqm v%d,v%d,v%d,v%d\n", vRT_addr, vRA_addr, vRB_addr,
             vRC_addr);
         assign(cin, binop( Iop_And32,
                            unop( Iop_64to32,
                                  unop( Iop_V128to64, mkexpr(vC) ) ),
                            mkU32(1) ) );
         putVReg( vRT_addr,
                  mkexpr(_get_quad_modulo_or_carry(mkexpr(vA), mkexpr(vB),
                                                   mkexpr(cin),
                                                   True) ) );
         return True;
      case 0x3F: 
         assign( vC, getVReg(vRC_addr));
         DIP("vsubecuq v%d,v%d,v%d,v%d\n", vRT_addr, vRA_addr, vRB_addr,
             vRC_addr);
         assign(cin, binop( Iop_And32,
                            unop( Iop_64to32,
                                  unop( Iop_V128to64, mkexpr(vC) ) ),
                            mkU32(1) ) );
         putVReg( vRT_addr,
                  unop( Iop_32UtoV128,
                        mkexpr(_get_quad_modulo_or_carry(mkexpr(vA),
                                                         unop( Iop_NotV128,
                                                               mkexpr(vB) ),
                                                         mkexpr(cin),
                                                         False) ) ) );
         return True;
      case 0x3E: 
         assign( vC, getVReg(vRC_addr));
         DIP("vsubeuqm v%d,v%d,v%d,v%d\n", vRT_addr, vRA_addr, vRB_addr,
             vRC_addr);
         assign(cin, binop( Iop_And32,
                            unop( Iop_64to32,
                                  unop( Iop_V128to64, mkexpr(vC) ) ),
                            mkU32(1) ) );
         putVReg( vRT_addr,
                  mkexpr(_get_quad_modulo_or_carry(mkexpr(vA),
                                                   unop( Iop_NotV128, mkexpr(vB) ),
                                                   mkexpr(cin),
                                                   True) ) );
         return True;
      default:
         vex_printf("dis_av_quad(ppc)(opc2.2)\n");
         return False;
   }

   return True;
}


static Bool dis_av_bcd ( UInt theInstr )
{
   
   UChar opc1     = ifieldOPC(theInstr);
   UChar vRT_addr = ifieldRegDS(theInstr);
   UChar vRA_addr = ifieldRegA(theInstr);
   UChar vRB_addr = ifieldRegB(theInstr);
   UChar ps       = IFIELD( theInstr, 9, 1 );
   UInt  opc2     = IFIELD( theInstr, 0, 9 );

   IRTemp vA    = newTemp(Ity_V128);
   IRTemp vB    = newTemp(Ity_V128);
   IRTemp dst    = newTemp(Ity_V128);
   assign( vA, getVReg(vRA_addr));
   assign( vB, getVReg(vRB_addr));

   if (opc1 != 0x4) {
      vex_printf("dis_av_bcd(ppc)(instr)\n");
      return False;
   }

   switch (opc2) {
   case 0x1:  
     DIP("bcdadd. v%d,v%d,v%d,%u\n", vRT_addr, vRA_addr, vRB_addr, ps);
     assign( dst, triop( Iop_BCDAdd, mkexpr( vA ),
                         mkexpr( vB ), mkU8( ps ) ) );
     putVReg( vRT_addr, mkexpr(dst));
     return True;

   case 0x41:  
     DIP("bcdsub. v%d,v%d,v%d,%u\n", vRT_addr, vRA_addr, vRB_addr, ps);
     assign( dst, triop( Iop_BCDSub, mkexpr( vA ),
                         mkexpr( vB ), mkU8( ps ) ) );
     putVReg( vRT_addr, mkexpr(dst));
     return True;

   default:
      vex_printf("dis_av_bcd(ppc)(opc2)\n");
      return False;
   }
   return True;
}

static Bool dis_av_fp_arith ( UInt theInstr )
{
   
   UChar opc1     = ifieldOPC(theInstr);
   UChar vD_addr  = ifieldRegDS(theInstr);
   UChar vA_addr  = ifieldRegA(theInstr);
   UChar vB_addr  = ifieldRegB(theInstr);
   UChar vC_addr  = ifieldRegC(theInstr);
   UInt  opc2=0;

   IRTemp vA = newTemp(Ity_V128);
   IRTemp vB = newTemp(Ity_V128);
   IRTemp vC = newTemp(Ity_V128);
   assign( vA, getVReg(vA_addr));
   assign( vB, getVReg(vB_addr));
   assign( vC, getVReg(vC_addr));

   if (opc1 != 0x4) {
      vex_printf("dis_av_fp_arith(ppc)(instr)\n");
      return False;
   }

   IRTemp rm = newTemp(Ity_I32);
   assign(rm, get_IR_roundingmode());

   opc2 = IFIELD( theInstr, 0, 6 );
   switch (opc2) {
   case 0x2E: 
      DIP("vmaddfp v%d,v%d,v%d,v%d\n",
          vD_addr, vA_addr, vC_addr, vB_addr);
      putVReg( vD_addr,
               triop(Iop_Add32Fx4, mkU32(Irrm_NEAREST),
                     mkexpr(vB),
                     triop(Iop_Mul32Fx4, mkU32(Irrm_NEAREST),
                           mkexpr(vA), mkexpr(vC))) );
      return True;

   case 0x2F: { 
      DIP("vnmsubfp v%d,v%d,v%d,v%d\n",
          vD_addr, vA_addr, vC_addr, vB_addr);
      putVReg( vD_addr,
               triop(Iop_Sub32Fx4, mkU32(Irrm_NEAREST),
                     mkexpr(vB),
                     triop(Iop_Mul32Fx4, mkU32(Irrm_NEAREST),
                           mkexpr(vA), mkexpr(vC))) );
      return True;
   }

   default:
     break; 
   }

   opc2 = IFIELD( theInstr, 0, 11 );
   switch (opc2) {
   case 0x00A: 
      DIP("vaddfp v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, triop(Iop_Add32Fx4,
                              mkU32(Irrm_NEAREST), mkexpr(vA), mkexpr(vB)) );
      return True;

  case 0x04A: 
      DIP("vsubfp v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, triop(Iop_Sub32Fx4,
                              mkU32(Irrm_NEAREST), mkexpr(vA), mkexpr(vB)) );
      return True;

   case 0x40A: 
      DIP("vmaxfp v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Max32Fx4, mkexpr(vA), mkexpr(vB)) );
      return True;

   case 0x44A: 
      DIP("vminfp v%d,v%d,v%d\n", vD_addr, vA_addr, vB_addr);
      putVReg( vD_addr, binop(Iop_Min32Fx4, mkexpr(vA), mkexpr(vB)) );
      return True;

   default:
      break; 
   }


   if (vA_addr != 0) {
      vex_printf("dis_av_fp_arith(ppc)(vA_addr)\n");
      return False;
   }

   switch (opc2) {
   case 0x10A: 
      DIP("vrefp v%d,v%d\n", vD_addr, vB_addr);
      putVReg( vD_addr, unop(Iop_RecipEst32Fx4, mkexpr(vB)) );
      return True;

   case 0x14A: 
      DIP("vrsqrtefp v%d,v%d\n", vD_addr, vB_addr);
      putVReg( vD_addr, unop(Iop_RSqrtEst32Fx4, mkexpr(vB)) );
      return True;

   case 0x18A: 
      DIP("vexptefp v%d,v%d\n", vD_addr, vB_addr);
      DIP(" => not implemented\n");
      return False;

   case 0x1CA: 
      DIP("vlogefp v%d,v%d\n", vD_addr, vB_addr);
      DIP(" => not implemented\n");
      return False;

   default:
      vex_printf("dis_av_fp_arith(ppc)(opc2=0x%x)\n",opc2);
      return False;
   }
   return True;
}

static Bool dis_av_fp_cmp ( UInt theInstr )
{
   
   UChar opc1     = ifieldOPC(theInstr);
   UChar vD_addr  = ifieldRegDS(theInstr);
   UChar vA_addr  = ifieldRegA(theInstr);
   UChar vB_addr  = ifieldRegB(theInstr);
   UChar flag_rC  = ifieldBIT10(theInstr);
   UInt  opc2     = IFIELD( theInstr, 0, 10 );

   Bool cmp_bounds = False;

   IRTemp vA = newTemp(Ity_V128);
   IRTemp vB = newTemp(Ity_V128);
   IRTemp vD = newTemp(Ity_V128);
   assign( vA, getVReg(vA_addr));
   assign( vB, getVReg(vB_addr));

   if (opc1 != 0x4) {
      vex_printf("dis_av_fp_cmp(ppc)(instr)\n");
      return False;
   }

   switch (opc2) {
   case 0x0C6: 
      DIP("vcmpeqfp%s v%d,v%d,v%d\n", (flag_rC ? ".":""),
                                      vD_addr, vA_addr, vB_addr);
      assign( vD, binop(Iop_CmpEQ32Fx4, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x1C6: 
      DIP("vcmpgefp%s v%d,v%d,v%d\n", (flag_rC ? ".":""),
                                      vD_addr, vA_addr, vB_addr);
      assign( vD, binop(Iop_CmpGE32Fx4, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x2C6: 
      DIP("vcmpgtfp%s v%d,v%d,v%d\n", (flag_rC ? ".":""),
                                      vD_addr, vA_addr, vB_addr);
      assign( vD, binop(Iop_CmpGT32Fx4, mkexpr(vA), mkexpr(vB)) );
      break;

   case 0x3C6: { 
      IRTemp gt      = newTemp(Ity_V128);
      IRTemp lt      = newTemp(Ity_V128);
      IRTemp zeros   = newTemp(Ity_V128);
      DIP("vcmpbfp%s v%d,v%d,v%d\n", (flag_rC ? ".":""),
                                     vD_addr, vA_addr, vB_addr);
      cmp_bounds = True;
      assign( zeros,   unop(Iop_Dup32x4, mkU32(0)) );

      assign( gt, unop(Iop_NotV128,
                       binop(Iop_CmpLE32Fx4, mkexpr(vA), mkexpr(vB))) );
      assign( lt, unop(Iop_NotV128,
                       binop(Iop_CmpGE32Fx4, mkexpr(vA),
                             triop(Iop_Sub32Fx4, mkU32(Irrm_NEAREST),
                                   mkexpr(zeros),
                                   mkexpr(vB)))) );

      
      assign( vD, binop(Iop_ShlN32x4,
                        binop(Iop_OrV128,
                              binop(Iop_AndV128, mkexpr(gt),
                                    unop(Iop_Dup32x4, mkU32(0x2))),
                              binop(Iop_AndV128, mkexpr(lt),
                                    unop(Iop_Dup32x4, mkU32(0x1)))),
                        mkU8(30)) );
      break;
   }

   default:
      vex_printf("dis_av_fp_cmp(ppc)(opc2)\n");
      return False;
   }

   putVReg( vD_addr, mkexpr(vD) );

   if (flag_rC) {
      set_AV_CR6( mkexpr(vD), !cmp_bounds );
   }
   return True;
}

static Bool dis_av_fp_convert ( UInt theInstr )
{
   
   UChar opc1     = ifieldOPC(theInstr);
   UChar vD_addr  = ifieldRegDS(theInstr);
   UChar UIMM_5   = ifieldRegA(theInstr);
   UChar vB_addr  = ifieldRegB(theInstr);
   UInt  opc2     = IFIELD( theInstr, 0, 11 );

   IRTemp vB        = newTemp(Ity_V128);
   IRTemp vScale    = newTemp(Ity_V128);
   IRTemp vInvScale = newTemp(Ity_V128);

   float scale, inv_scale;

   assign( vB, getVReg(vB_addr));

   
   scale = (float)( (unsigned int) 1<<UIMM_5 );
   assign( vScale, unop(Iop_Dup32x4, mkU32( float_to_bits(scale) )) );
   inv_scale = 1/scale;
   assign( vInvScale,
           unop(Iop_Dup32x4, mkU32( float_to_bits(inv_scale) )) );

   if (opc1 != 0x4) {
      vex_printf("dis_av_fp_convert(ppc)(instr)\n");
      return False;
   }

   switch (opc2) {
   case 0x30A: 
      DIP("vcfux v%d,v%d,%d\n", vD_addr, vB_addr, UIMM_5);
      putVReg( vD_addr, triop(Iop_Mul32Fx4, mkU32(Irrm_NEAREST),
                              unop(Iop_I32UtoFx4, mkexpr(vB)),
                              mkexpr(vInvScale)) );
      return True;

   case 0x34A: 
      DIP("vcfsx v%d,v%d,%d\n", vD_addr, vB_addr, UIMM_5);

      putVReg( vD_addr, triop(Iop_Mul32Fx4, mkU32(Irrm_NEAREST),
                              unop(Iop_I32StoFx4, mkexpr(vB)),
                              mkexpr(vInvScale)) );
      return True;

   case 0x38A: 
      DIP("vctuxs v%d,v%d,%d\n", vD_addr, vB_addr, UIMM_5);
      putVReg( vD_addr,
               unop(Iop_QFtoI32Ux4_RZ, 
                    triop(Iop_Mul32Fx4, mkU32(Irrm_NEAREST),
                          mkexpr(vB), mkexpr(vScale))) );
      return True;

   case 0x3CA: 
      DIP("vctsxs v%d,v%d,%d\n", vD_addr, vB_addr, UIMM_5);
      putVReg( vD_addr, 
               unop(Iop_QFtoI32Sx4_RZ, 
                     triop(Iop_Mul32Fx4, mkU32(Irrm_NEAREST),
                           mkexpr(vB), mkexpr(vScale))) );
      return True;

   default:
     break;    
   }

   if (UIMM_5 != 0) {
      vex_printf("dis_av_fp_convert(ppc)(UIMM_5)\n");
      return False;
   }

   switch (opc2) {
   case 0x20A: 
      DIP("vrfin v%d,v%d\n", vD_addr, vB_addr);
      putVReg( vD_addr, unop(Iop_RoundF32x4_RN, mkexpr(vB)) );
      break;

   case 0x24A: 
      DIP("vrfiz v%d,v%d\n", vD_addr, vB_addr);
      putVReg( vD_addr, unop(Iop_RoundF32x4_RZ, mkexpr(vB)) );
      break;

   case 0x28A: 
      DIP("vrfip v%d,v%d\n", vD_addr, vB_addr);
      putVReg( vD_addr, unop(Iop_RoundF32x4_RP, mkexpr(vB)) );
      break;

   case 0x2CA: 
      DIP("vrfim v%d,v%d\n", vD_addr, vB_addr);
      putVReg( vD_addr, unop(Iop_RoundF32x4_RM, mkexpr(vB)) );
      break;

   default:
      vex_printf("dis_av_fp_convert(ppc)(opc2)\n");
      return False;
   }
   return True;
}

static Bool dis_transactional_memory ( UInt theInstr, UInt nextInstr,
                                       const VexAbiInfo* vbi,
                                       DisResult* dres,
                                       Bool (*resteerOkFn)(void*,Addr),
                                       void* callback_opaque )
{
   UInt   opc2      = IFIELD( theInstr, 1, 10 );

   switch (opc2) {
   case 0x28E: {        
      UInt R = IFIELD( theInstr, 21, 1 );

      ULong tm_reason;
      UInt failure_code = 0;  
      UInt persistant = 1;    
      UInt nest_overflow = 1; 
      UInt tm_exact   = 1;    

      DIP("tbegin. %d\n", R);

      putCR321( 0, mkU8( 0x2 ) );

      tm_reason = generate_TMreason( failure_code, persistant,
                                     nest_overflow, tm_exact );

      storeTMfailure( guest_CIA_curr_instr, tm_reason,
                      guest_CIA_curr_instr+4 );

      return True;

      break;
   }

   case 0x2AE: {        
      
      UInt A = IFIELD( theInstr, 25, 1 );

      DIP("tend. %d\n", A);
      break;
   }

   case 0x2EE: {        
      
      UInt L = IFIELD( theInstr, 21, 1 );

      DIP("tsr. %d\n", L);
      break;
   }

   case 0x2CE: {        
      
      UInt BF = IFIELD( theInstr, 25, 1 );

      DIP("tcheck. %d\n", BF);
      break;
   }

   case 0x30E: {        
      
      UInt TO = IFIELD( theInstr, 25, 1 );
      UInt RA = IFIELD( theInstr, 16, 5 );
      UInt RB = IFIELD( theInstr, 11, 5 );

      DIP("tabortwc. %d,%d,%d\n", TO, RA, RB);
      break;
   }

   case 0x32E: {        
      
      UInt TO = IFIELD( theInstr, 25, 1 );
      UInt RA = IFIELD( theInstr, 16, 5 );
      UInt RB = IFIELD( theInstr, 11, 5 );

      DIP("tabortdc. %d,%d,%d\n", TO, RA, RB);
      break;
   }

   case 0x34E: {        
      
      UInt TO = IFIELD( theInstr, 25, 1 );
      UInt RA = IFIELD( theInstr, 16, 5 );
      UInt SI = IFIELD( theInstr, 11, 5 );

      DIP("tabortwci. %d,%d,%d\n", TO, RA, SI);
      break;
   }

   case 0x36E: {        
      
      UInt TO = IFIELD( theInstr, 25, 1 );
      UInt RA = IFIELD( theInstr, 16, 5 );
      UInt SI = IFIELD( theInstr, 11, 5 );

      DIP("tabortdci. %d,%d,%d\n", TO, RA, SI);
      break;
   }

   case 0x38E: {        
      
      UInt RA = IFIELD( theInstr, 16, 5 );

      DIP("tabort. %d\n", RA);
      break;
   }

   case 0x3AE: {        
      
      UInt RA = IFIELD( theInstr, 16, 5 );

      DIP("treclaim. %d\n", RA);
      break;
   }

   case 0x3EE: {        
      
      DIP("trechkpt.\n");
      break;
   }

   default:
      vex_printf("dis_transactional_memory(ppc): unrecognized instruction\n");
      return False;
   }

   return True;
}




struct vsx_insn {
   UInt opcode;
   const HChar * name;
};

static struct vsx_insn vsx_all[] = {
      { 0x0, "xsaddsp" },
      { 0x4, "xsmaddasp" },
      { 0x8, "xxsldwi" },
      { 0x14, "xsrsqrtesp" },
      { 0x16, "xssqrtsp" },
      { 0x18, "xxsel" },
      { 0x20, "xssubsp" },
      { 0x24, "xsmaddmsp" },
      { 0x28, "xxpermdi" },
      { 0x34, "xsresp" },
      { 0x40, "xsmulsp" },
      { 0x44, "xsmsubasp" },
      { 0x48, "xxmrghw" },
      { 0x60, "xsdivsp" },
      { 0x64, "xsmsubmsp" },
      { 0x80, "xsadddp" },
      { 0x84, "xsmaddadp" },
      { 0x8c, "xscmpudp" },
      { 0x90, "xscvdpuxws" },
      { 0x92, "xsrdpi" },
      { 0x94, "xsrsqrtedp" },
      { 0x96, "xssqrtdp" },
      { 0xa0, "xssubdp" },
      { 0xa4, "xsmaddmdp" },
      { 0xac, "xscmpodp" },
      { 0xb0, "xscvdpsxws" },
      { 0xb2, "xsrdpiz" },
      { 0xb4, "xsredp" },
      { 0xc0, "xsmuldp" },
      { 0xc4, "xsmsubadp" },
      { 0xc8, "xxmrglw" },
      { 0xd2, "xsrdpip" },
      { 0xd4, "xstsqrtdp" },
      { 0xd6, "xsrdpic" },
      { 0xe0, "xsdivdp" },
      { 0xe4, "xsmsubmdp" },
      { 0xf2, "xsrdpim" },
      { 0xf4, "xstdivdp" },
      { 0x100, "xvaddsp" },
      { 0x104, "xvmaddasp" },
      { 0x10c, "xvcmpeqsp" },
      { 0x110, "xvcvspuxws" },
      { 0x112, "xvrspi" },
      { 0x114, "xvrsqrtesp" },
      { 0x116, "xvsqrtsp" },
      { 0x120, "xvsubsp" },
      { 0x124, "xvmaddmsp" },
      { 0x12c, "xvcmpgtsp" },
      { 0x130, "xvcvspsxws" },
      { 0x132, "xvrspiz" },
      { 0x134, "xvresp" },
      { 0x140, "xvmulsp" },
      { 0x144, "xvmsubasp" },
      { 0x148, "xxspltw" },
      { 0x14c, "xvcmpgesp" },
      { 0x150, "xvcvuxwsp" },
      { 0x152, "xvrspip" },
      { 0x154, "xvtsqrtsp" },
      { 0x156, "xvrspic" },
      { 0x160, "xvdivsp" },
      { 0x164, "xvmsubmsp" },
      { 0x170, "xvcvsxwsp" },
      { 0x172, "xvrspim" },
      { 0x174, "xvtdivsp" },
      { 0x180, "xvadddp" },
      { 0x184, "xvmaddadp" },
      { 0x18c, "xvcmpeqdp" },
      { 0x190, "xvcvdpuxws" },
      { 0x192, "xvrdpi" },
      { 0x194, "xvrsqrtedp" },
      { 0x196, "xvsqrtdp" },
      { 0x1a0, "xvsubdp" },
      { 0x1a4, "xvmaddmdp" },
      { 0x1ac, "xvcmpgtdp" },
      { 0x1b0, "xvcvdpsxws" },
      { 0x1b2, "xvrdpiz" },
      { 0x1b4, "xvredp" },
      { 0x1c0, "xvmuldp" },
      { 0x1c4, "xvmsubadp" },
      { 0x1cc, "xvcmpgedp" },
      { 0x1d0, "xvcvuxwdp" },
      { 0x1d2, "xvrdpip" },
      { 0x1d4, "xvtsqrtdp" },
      { 0x1d6, "xvrdpic" },
      { 0x1e0, "xvdivdp" },
      { 0x1e4, "xvmsubmdp" },
      { 0x1f0, "xvcvsxwdp" },
      { 0x1f2, "xvrdpim" },
      { 0x1f4, "xvtdivdp" },
      { 0x204, "xsnmaddasp" },
      { 0x208, "xxland" },
      { 0x212, "xscvdpsp" },
      { 0x216, "xscvdpspn" },
      { 0x224, "xsnmaddmsp" },
      { 0x228, "xxlandc" },
      { 0x232, "xxrsp" },
      { 0x244, "xsnmsubasp" },
      { 0x248, "xxlor" },
      { 0x250, "xscvuxdsp" },
      { 0x264, "xsnmsubmsp" },
      { 0x268, "xxlxor" },
      { 0x270, "xscvsxdsp" },
      { 0x280, "xsmaxdp" },
      { 0x284, "xsnmaddadp" },
      { 0x288, "xxlnor" },
      { 0x290, "xscvdpuxds" },
      { 0x292, "xscvspdp" },
      { 0x296, "xscvspdpn" },
      { 0x2a0, "xsmindp" },
      { 0x2a4, "xsnmaddmdp" },
      { 0x2a8, "xxlorc" },
      { 0x2b0, "xscvdpsxds" },
      { 0x2b2, "xsabsdp" },
      { 0x2c0, "xscpsgndp" },
      { 0x2c4, "xsnmsubadp" },
      { 0x2c8, "xxlnand" },
      { 0x2d0, "xscvuxddp" },
      { 0x2d2, "xsnabsdp" },
      { 0x2e4, "xsnmsubmdp" },
      { 0x2e8, "xxleqv" },
      { 0x2f0, "xscvsxddp" },
      { 0x2f2, "xsnegdp" },
      { 0x300, "xvmaxsp" },
      { 0x304, "xvnmaddasp" },
      { 0x30c, "xvcmpeqsp." },
      { 0x310, "xvcvspuxds" },
      { 0x312, "xvcvdpsp" },
      { 0x320, "xvminsp" },
      { 0x324, "xvnmaddmsp" },
      { 0x32c, "xvcmpgtsp." },
      { 0x330, "xvcvspsxds" },
      { 0x332, "xvabssp" },
      { 0x340, "xvcpsgnsp" },
      { 0x344, "xvnmsubasp" },
      { 0x34c, "xvcmpgesp." },
      { 0x350, "xvcvuxdsp" },
      { 0x352, "xvnabssp" },
      { 0x364, "xvnmsubmsp" },
      { 0x370, "xvcvsxdsp" },
      { 0x372, "xvnegsp" },
      { 0x380, "xvmaxdp" },
      { 0x384, "xvnmaddadp" },
      { 0x38c, "xvcmpeqdp." },
      { 0x390, "xvcvdpuxds" },
      { 0x392, "xvcvspdp" },
      { 0x3a0, "xvmindp" },
      { 0x3a4, "xvnmaddmdp" },
      { 0x3ac, "xvcmpgtdp." },
      { 0x3b0, "xvcvdpsxds" },
      { 0x3b2, "xvabsdp" },
      { 0x3c0, "xvcpsgndp" },
      { 0x3c4, "xvnmsubadp" },
      { 0x3cc, "xvcmpgedp." },
      { 0x3d0, "xvcvuxddp" },
      { 0x3d2, "xvnabsdp" },
      { 0x3e4, "xvnmsubmdp" },
      { 0x3f0, "xvcvsxddp" },
      { 0x3f2, "xvnegdp" }
};
#define VSX_ALL_LEN (sizeof vsx_all / sizeof *vsx_all)


static Int findVSXextOpCode(UInt opcode)
{
   Int low, mid, high;
   low = 0;
   high = VSX_ALL_LEN - 1;
   while (low <= high) {
      mid = (low + high)/2;
      if (opcode < vsx_all[mid].opcode)
         high = mid - 1;
      else if (opcode > vsx_all[mid].opcode)
         low = mid + 1;
      else
         return mid;
   }
   return -1;
}


static UInt get_VSX60_opc2(UInt opc2_full)
{
#define XX2_MASK 0x000003FE
#define XX3_1_MASK 0x000003FC
#define XX3_2_MASK 0x000001FC
#define XX3_3_MASK 0x0000007C
#define XX4_MASK 0x00000018
   Int ret;
   UInt vsxExtOpcode = 0;

   if (( ret = findVSXextOpCode(opc2_full & XX2_MASK)) >= 0)
      vsxExtOpcode = vsx_all[ret].opcode;
   else if (( ret = findVSXextOpCode(opc2_full & XX3_1_MASK)) >= 0)
      vsxExtOpcode = vsx_all[ret].opcode;
   else if (( ret = findVSXextOpCode(opc2_full & XX3_2_MASK)) >= 0)
      vsxExtOpcode = vsx_all[ret].opcode;
   else if (( ret = findVSXextOpCode(opc2_full & XX3_3_MASK)) >= 0)
      vsxExtOpcode = vsx_all[ret].opcode;
   else if (( ret = findVSXextOpCode(opc2_full & XX4_MASK)) >= 0)
      vsxExtOpcode = vsx_all[ret].opcode;

   return vsxExtOpcode;
}



static   
DisResult disInstr_PPC_WRK ( 
             Bool         (*resteerOkFn) ( void*, Addr ),
             Bool         resteerCisOk,
             void*        callback_opaque,
             Long         delta64,
             const VexArchInfo* archinfo,
             const VexAbiInfo*  abiinfo,
             Bool         sigill_diag
          )
{
   UChar     opc1;
   UInt      opc2;
   DisResult dres;
   UInt      theInstr;
   IRType    ty = mode64 ? Ity_I64 : Ity_I32;
   Bool      allow_F  = False;
   Bool      allow_V  = False;
   Bool      allow_FX = False;
   Bool      allow_GX = False;
   Bool      allow_VX = False;  
   Bool      allow_DFP = False;
   Bool      allow_isa_2_07 = False;
   UInt      hwcaps = archinfo->hwcaps;
   Long      delta;

   
   if (mode64) {
      allow_F  = True;
      allow_V  = (0 != (hwcaps & VEX_HWCAPS_PPC64_V));
      allow_FX = (0 != (hwcaps & VEX_HWCAPS_PPC64_FX));
      allow_GX = (0 != (hwcaps & VEX_HWCAPS_PPC64_GX));
      allow_VX = (0 != (hwcaps & VEX_HWCAPS_PPC64_VX));
      allow_DFP = (0 != (hwcaps & VEX_HWCAPS_PPC64_DFP));
      allow_isa_2_07 = (0 != (hwcaps & VEX_HWCAPS_PPC64_ISA2_07));
   } else {
      allow_F  = (0 != (hwcaps & VEX_HWCAPS_PPC32_F));
      allow_V  = (0 != (hwcaps & VEX_HWCAPS_PPC32_V));
      allow_FX = (0 != (hwcaps & VEX_HWCAPS_PPC32_FX));
      allow_GX = (0 != (hwcaps & VEX_HWCAPS_PPC32_GX));
      allow_VX = (0 != (hwcaps & VEX_HWCAPS_PPC32_VX));
      allow_DFP = (0 != (hwcaps & VEX_HWCAPS_PPC32_DFP));
      allow_isa_2_07 = (0 != (hwcaps & VEX_HWCAPS_PPC32_ISA2_07));
   }

   
   delta = (Long)mkSzAddr(ty, (ULong)delta64);

   
   dres.whatNext    = Dis_Continue;
   dres.len         = 0;
   dres.continueAt  = 0;
   dres.jk_StopHere = Ijk_INVALID;

   theInstr = getUIntPPCendianly( &guest_code[delta] );

   if (0) vex_printf("insn: 0x%x\n", theInstr);

   DIP("\t0x%llx:  ", (ULong)guest_CIA_curr_instr);

   
   {
      const UChar* code = guest_code + delta;
      UInt word1 = mode64 ? 0x78001800 : 0x5400183E;
      UInt word2 = mode64 ? 0x78006800 : 0x5400683E;
      UInt word3 = mode64 ? 0x7800E802 : 0x5400E83E;
      UInt word4 = mode64 ? 0x78009802 : 0x5400983E;
      Bool is_special_preamble = False;
      if (getUIntPPCendianly(code+ 0) == word1 &&
          getUIntPPCendianly(code+ 4) == word2 &&
          getUIntPPCendianly(code+ 8) == word3 &&
          getUIntPPCendianly(code+12) == word4) {
         is_special_preamble = True;
      } else if (! mode64 &&
                 getUIntPPCendianly(code+ 0) == 0x54001800 &&
                 getUIntPPCendianly(code+ 4) == 0x54006800 &&
                 getUIntPPCendianly(code+ 8) == 0x5400E800 &&
                 getUIntPPCendianly(code+12) == 0x54009800) {
         static Bool reported = False;
         if (!reported) {
            vex_printf("disInstr(ppc): old ppc32 instruction magic detected. Code might clobber r0.\n");
            vex_printf("disInstr(ppc): source needs to be recompiled against latest valgrind.h.\n");
            reported = True;
         }
         is_special_preamble = True;
      }
      if (is_special_preamble) {
         
         if (getUIntPPCendianly(code+16) == 0x7C210B78 ) {
            
            DIP("r3 = client_request ( %%r4 )\n");
            delta += 20;
            putGST( PPC_GST_CIA, mkSzImm( ty, guest_CIA_bbstart + delta ));
            dres.jk_StopHere = Ijk_ClientReq;
            dres.whatNext    = Dis_StopHere;
            goto decode_success;
         }
         else
         if (getUIntPPCendianly(code+16) == 0x7C421378 ) {
            
            DIP("r3 = guest_NRADDR\n");
            delta += 20;
            dres.len = 20;
            putIReg(3, IRExpr_Get( OFFB_NRADDR, ty ));
            goto decode_success;
         }
         else
         if (getUIntPPCendianly(code+16) == 0x7C631B78 ) {
            delta += 20;
            if (host_endness == VexEndnessLE) {
                
                DIP("branch-and-link-to-noredir r12\n");
                putGST( PPC_GST_LR,
                        mkSzImm(ty, guest_CIA_bbstart + (Long)delta) );
                putGST( PPC_GST_CIA, getIReg(12));
            } else {
                
                DIP("branch-and-link-to-noredir r11\n");
                putGST( PPC_GST_LR,
                        mkSzImm(ty, guest_CIA_bbstart + (Long)delta) );
                putGST( PPC_GST_CIA, getIReg(11));
            }
            dres.jk_StopHere = Ijk_NoRedir;
            dres.whatNext    = Dis_StopHere;
            goto decode_success;
         }
         else
         if (getUIntPPCendianly(code+16) == 0x7C842378 ) {
            
            DIP("r3 = guest_NRADDR_GPR2\n");
            delta += 20;
            dres.len = 20;
            putIReg(3, IRExpr_Get( OFFB_NRADDR_GPR2, ty ));
            goto decode_success;
         }
         else
         if (getUIntPPCendianly(code+16) == 0x7CA52B78 ) {
            DIP("IR injection\n");
            if (host_endness == VexEndnessBE)
               vex_inject_ir(irsb, Iend_BE);
            else
               vex_inject_ir(irsb, Iend_LE);

            delta += 20;
            dres.len = 20;

            
            
            
            

            stmt(IRStmt_Put(OFFB_CMSTART, mkSzImm(ty, guest_CIA_curr_instr)));
            stmt(IRStmt_Put(OFFB_CMLEN,   mkSzImm(ty, 20)));
   
            putGST( PPC_GST_CIA, mkSzImm( ty, guest_CIA_bbstart + delta ));
            dres.whatNext    = Dis_StopHere;
            dres.jk_StopHere = Ijk_InvalICache;
            goto decode_success;
         }
         theInstr = getUIntPPCendianly(code+16);
         opc1     = ifieldOPC(theInstr);
         opc2     = ifieldOPClo10(theInstr);
         goto decode_failure;
         
      }
   }

   opc1 = ifieldOPC(theInstr);
   opc2 = ifieldOPClo10(theInstr);

   
   switch (opc1) {

   
   case 0x0C: case 0x0D: case 0x0E:  
   case 0x0F: case 0x07: case 0x08:  
      if (dis_int_arith( theInstr )) goto decode_success;
      goto decode_failure;

   
   case 0x0B: case 0x0A: 
      if (dis_int_cmp( theInstr )) goto decode_success;
      goto decode_failure;

   
   case 0x1C: case 0x1D: case 0x18: 
   case 0x19: case 0x1A: case 0x1B: 
      if (dis_int_logic( theInstr )) goto decode_success;
      goto decode_failure;

   
   case 0x14: case 0x15:  case 0x17: 
      if (dis_int_rot( theInstr )) goto decode_success;
      goto decode_failure;

   
   case 0x1E: 
      if (!mode64) goto decode_failure;
      if (dis_int_rot( theInstr )) goto decode_success;
      goto decode_failure;

   
   case 0x22: case 0x23: case 0x2A: 
   case 0x2B: case 0x28: case 0x29: 
   case 0x20: case 0x21:            
      if (dis_int_load( theInstr )) goto decode_success;
      goto decode_failure;

   
   case 0x26: case 0x27: case 0x2C: 
   case 0x2D: case 0x24: case 0x25: 
      if (dis_int_store( theInstr, abiinfo )) goto decode_success;
      goto decode_failure;

   
   case 0x2E: case 0x2F: 
      if (dis_int_ldst_mult( theInstr )) goto decode_success;
      goto decode_failure;

   
   case 0x12: case 0x10: 
      if (dis_branch(theInstr, abiinfo, &dres, 
                               resteerOkFn, callback_opaque)) 
         goto decode_success;
      goto decode_failure;

   
   case 0x11: 
      if (dis_syslink(theInstr, abiinfo, &dres)) goto decode_success;
      goto decode_failure;

   
   case 0x02:    
      if (!mode64) goto decode_failure;
      if (dis_trapi(theInstr, &dres)) goto decode_success;
      goto decode_failure;

   case 0x03:   
      if (dis_trapi(theInstr, &dres)) goto decode_success;
      goto decode_failure;

   
   case 0x30: case 0x31: case 0x32: 
   case 0x33:                       
      if (!allow_F) goto decode_noF;
      if (dis_fp_load( theInstr )) goto decode_success;
      goto decode_failure;

   
   case 0x34: case 0x35: case 0x36: 
   case 0x37:                       
      if (!allow_F) goto decode_noF;
      if (dis_fp_store( theInstr )) goto decode_success;
      goto decode_failure;

      
   case 0x39: case 0x3D:
      if (!allow_F) goto decode_noF;
      if (dis_fp_pair( theInstr )) goto decode_success;
      goto decode_failure;

   
   case 0x38:  
      if (dis_int_load( theInstr )) goto decode_success;
      goto decode_failure;

   
   case 0x3A:  
      if (!mode64) goto decode_failure;
      if (dis_int_load( theInstr )) goto decode_success;
      goto decode_failure;

   case 0x3B:
      if (!allow_F) goto decode_noF;
      opc2 = ifieldOPClo10(theInstr);

      switch (opc2) {
         case 0x2:    
         case 0x202:  
         case 0x22:   
         case 0x222:  
            if (!allow_DFP) goto decode_noDFP;
            if (dis_dfp_arith( theInstr ))
               goto decode_success;
         case 0x82:   
         case 0x282:  
            if (!allow_DFP) goto decode_noDFP;
            if (dis_dfp_compare( theInstr ) )
               goto decode_success;
            goto decode_failure;
         case 0x102: 
         case 0x302: 
         case 0x122: 
            if (!allow_DFP) goto decode_noDFP;
            if (dis_dfp_fmt_conv( theInstr ))
               goto decode_success;
            goto decode_failure;
         case 0x322: 
            if (!allow_VX)
               goto decode_failure;
            if (dis_dfp_fmt_conv( theInstr ))
               goto decode_success;
            goto decode_failure;
         case 0x2A2: 
            if (!allow_DFP) goto decode_noDFP;
            if (dis_dfp_significant_digits(theInstr))
               goto decode_success;
            goto decode_failure;
         case 0x142: 
         case 0x342: 
            if (!allow_DFP) goto decode_noDFP;
            if (dis_dfp_bcd(theInstr))
               goto decode_success;
            goto decode_failure;
         case 0x162:  
         case 0x362:  
            if (!allow_DFP) goto decode_noDFP;
            if (dis_dfp_extract_insert( theInstr ) )
               goto decode_success;
            goto decode_failure;
         case 0x3CE: 
            if (!allow_VX)
               goto decode_noVX;
            if (dis_fp_round( theInstr ))
               goto decode_success;
            goto decode_failure;
         case 0x34E: 
            if (dis_fp_round( theInstr ))
               goto decode_success;
            goto decode_failure;
      }

      opc2 = ifieldOPClo9( theInstr );
      switch (opc2) {
      case 0x42: 
      case 0x62: 
         if (!allow_DFP) goto decode_noDFP;
         if (dis_dfp_shift( theInstr ))
            goto decode_success;
         goto decode_failure;
      case 0xc2:  
      case 0xe2:  
         if (!allow_DFP) goto decode_noDFP;
         if (dis_dfp_class_test( theInstr ))
            goto decode_success;
         goto decode_failure;
      }

      opc2 = ifieldOPClo8( theInstr );
      switch (opc2) {
      case 0x3:   
      case 0x23:  
      case 0x43:  
         if (!allow_DFP) goto decode_noDFP;
         if (dis_dfp_quantize_sig_rrnd( theInstr ) )
            goto decode_success;
         goto decode_failure;
      case 0xA2: 
         if (!allow_DFP) goto decode_noDFP;
         if (dis_dfp_exponent_test( theInstr ) )
            goto decode_success;
         goto decode_failure;
      case 0x63: 
      case 0xE3: 
         if (!allow_DFP) goto decode_noDFP;
         if (dis_dfp_round( theInstr ) ) {
            goto decode_success;
         }
         goto decode_failure;
      default:
         break;  
      }

      opc2 = IFIELD(theInstr, 1, 5);
      switch (opc2) {
      
      case 0x12: case 0x14: case 0x15: 
      case 0x19:                       
         if (dis_fp_arith(theInstr)) goto decode_success;
         goto decode_failure;
      case 0x16:                       
         if (!allow_FX) goto decode_noFX;
         if (dis_fp_arith(theInstr)) goto decode_success;
         goto decode_failure;
      case 0x18:                       
         if (!allow_GX) goto decode_noGX;
         if (dis_fp_arith(theInstr)) goto decode_success;
         goto decode_failure;

      
      case 0x1C: case 0x1D: case 0x1E: 
      case 0x1F:                       
         if (dis_fp_multadd(theInstr)) goto decode_success;
         goto decode_failure;

      case 0x1A:                       
         if (!allow_GX) goto decode_noGX;
         if (dis_fp_arith(theInstr)) goto decode_success;
         goto decode_failure;

      default:
         goto decode_failure;
      }
      break;

   case 0x3C: 
   {
      
      
      if (!allow_V) goto decode_noVX;

      UInt vsxOpc2 = get_VSX60_opc2(opc2);

      switch (vsxOpc2) {
         case 0x8: case 0x28: case 0x48: case 0xc8: 
         case 0x018: case 0x148: 
            if (dis_vx_permute_misc(theInstr, vsxOpc2)) goto decode_success;
            goto decode_failure;
         case 0x268: case 0x248: case 0x288: 
         case 0x208: case 0x228: case 0x2A8: 
         case 0x2C8: case 0x2E8: 
            if (dis_vx_logic(theInstr, vsxOpc2)) goto decode_success;
            goto decode_failure;
         case 0x2B2: case 0x2C0: 
         case 0x2D2: case 0x2F2: 
         case 0x280: case 0x2A0: 
         case 0x0F2: case 0x0D2: 
         case 0x034: case 0x014: 
         case 0x0B4: case 0x094: 
         case 0x0D6: case 0x0B2: 
         case 0x092: case 0x232: 
            if (dis_vxs_misc(theInstr, vsxOpc2)) goto decode_success;
            goto decode_failure;
         case 0x08C: case 0x0AC: 
            if (dis_vx_cmp(theInstr, vsxOpc2)) goto decode_success;
            goto decode_failure;
         case 0x0:   case 0x020: 
         case 0x080:             
         case 0x060: case 0x0E0: 
         case 0x004: case 0x024: 
         case 0x084: case 0x0A4: 
         case 0x044: case 0x064: 
         case 0x0C4: case 0x0E4: 
         case 0x204: case 0x224: 
         case 0x284: case 0x2A4: 
         case 0x244: case 0x264: 
         case 0x2C4: case 0x2E4: 
         case 0x040: case 0x0C0: 
         case 0x0A0:             
         case 0x016: case 0x096: 
         case 0x0F4: case 0x0D4: 
            if (dis_vxs_arith(theInstr, vsxOpc2)) goto decode_success;
            goto decode_failure;
         case 0x180: 
         case 0x1E0: 
         case 0x1C0: 
         case 0x1A0: 
         case 0x184: case 0x1A4: 
         case 0x1C4: case 0x1E4: 
         case 0x384: case 0x3A4: 
         case 0x3C4: case 0x3E4: 
         case 0x1D4: case 0x1F4: 
         case 0x196: 
            if (dis_vxv_dp_arith(theInstr, vsxOpc2)) goto decode_success;
            goto decode_failure;
         case 0x100: 
         case 0x160: 
         case 0x140: 
         case 0x120: 
         case 0x104: case 0x124: 
         case 0x144: case 0x164: 
         case 0x304: case 0x324: 
         case 0x344: case 0x364: 
         case 0x154: case 0x174: 
         case 0x116: 
            if (dis_vxv_sp_arith(theInstr, vsxOpc2)) goto decode_success;
            goto decode_failure;

         case 0x250:             
         case 0x2D0: case 0x3d0: 
         case 0x350: case 0x1d0: 
         case 0x090: 
            
            
            
            
            if (!allow_VX) goto decode_noVX;
            if (dis_vx_conv(theInstr, vsxOpc2)) goto decode_success;
            goto decode_failure;

         case 0x2B0: 
         case 0x270: case 0x2F0: 
         case 0x1b0: case 0x130: 
         case 0x0b0: case 0x290: 
         case 0x212: case 0x216: 
         case 0x292: case 0x296: 
         case 0x312: 
         case 0x390: case 0x190: 
         case 0x3B0: case 0x310: 
         case 0x392: case 0x330: 
         case 0x110: case 0x3f0: 
         case 0x370: case 0x1f0: 
         case 0x170: case 0x150: 
            if (dis_vx_conv(theInstr, vsxOpc2)) goto decode_success;
            goto decode_failure;

         case 0x18C: case 0x38C: 
         case 0x10C: case 0x30C: 
         case 0x14C: case 0x34C: 
         case 0x12C: case 0x32C: 
         case 0x1CC: case 0x3CC: 
         case 0x1AC: case 0x3AC: 
             if (dis_vvec_cmp(theInstr, vsxOpc2)) goto decode_success;
             goto decode_failure;

         case 0x134:  
         case 0x1B4:  
         case 0x194: case 0x114: 
         case 0x380: case 0x3A0: 
         case 0x300: case 0x320: 
         case 0x3C0: case 0x340: 
         case 0x3B2: case 0x332: 
         case 0x3D2: case 0x352: 
         case 0x192: case 0x1D6: 
         case 0x1F2: case 0x1D2: 
         case 0x1B2: case 0x3F2: 
         case 0x112: case 0x156: 
         case 0x172: case 0x152: 
         case 0x132: 
            if (dis_vxv_misc(theInstr, vsxOpc2)) goto decode_success;
            goto decode_failure;

         default:
            goto decode_failure;
      }
      break;
   }

   
   case 0x3E:  
      if (dis_int_store( theInstr, abiinfo )) goto decode_success;
      goto decode_failure;

   case 0x3F:
      if (!allow_F) goto decode_noF;

      opc2 = IFIELD(theInstr, 1, 5);
      switch (opc2) {
      
      case 0x12: case 0x14: case 0x15: 
      case 0x19:                       
         if (dis_fp_arith(theInstr)) goto decode_success;
         goto decode_failure;
      case 0x16:                       
         if (!allow_FX) goto decode_noFX;
         if (dis_fp_arith(theInstr)) goto decode_success;
         goto decode_failure;
      case 0x17: case 0x1A:            
         if (!allow_GX) goto decode_noGX;
         if (dis_fp_arith(theInstr)) goto decode_success;
         goto decode_failure;
         
               
      case 0x1C: case 0x1D: case 0x1E: 
      case 0x1F:                       
         if (dis_fp_multadd(theInstr)) goto decode_success;
         goto decode_failure;

      case 0x18:                       
         if (!allow_GX) goto decode_noGX;
         if (dis_fp_arith(theInstr)) goto decode_success;
         goto decode_failure;

      default:
         break; 
      }

      opc2 = IFIELD(theInstr, 1, 10);
      switch (opc2) {
      
      case 0x2:    
      case 0x202:  
      case 0x22:   
      case 0x222:  
         if (!allow_DFP) goto decode_noDFP;
         if (dis_dfp_arithq( theInstr ))
            goto decode_success;
         goto decode_failure;
      case 0x162:  
      case 0x362:  
         if (!allow_DFP) goto decode_noDFP;
         if (dis_dfp_extract_insertq( theInstr ))
            goto decode_success;
         goto decode_failure;

      case 0x82:   
      case 0x282:  
         if (!allow_DFP) goto decode_noDFP;
         if (dis_dfp_compare( theInstr ) )
            goto decode_success;
         goto decode_failure;

      case 0x102: 
      case 0x302: 
      case 0x122: 
      case 0x322: 
         if (!allow_DFP) goto decode_noDFP;
         if (dis_dfp_fmt_convq( theInstr ))
            goto decode_success;
         goto decode_failure;

      case 0x2A2: 
         if (!allow_DFP) goto decode_noDFP;
         if (dis_dfp_significant_digits(theInstr))
            goto decode_success;
         goto decode_failure;

      case 0x142: 
      case 0x342: 
         if (!allow_DFP) goto decode_noDFP;
         if (dis_dfp_bcdq(theInstr))
            goto decode_success;
         goto decode_failure;

               
      case 0x000: 
      case 0x020: 
         if (dis_fp_cmp(theInstr)) goto decode_success;
         goto decode_failure;
         
      case 0x080: 
      case 0x0A0: 
         if (dis_fp_tests(theInstr)) goto decode_success;
         goto decode_failure;

               
      case 0x00C: 
      case 0x00E: 
      case 0x00F: 
      case 0x32E: 
      case 0x32F: 
      case 0x34E: 
         if (dis_fp_round(theInstr)) goto decode_success;
         goto decode_failure;
      case 0x3CE: case 0x3AE: case 0x3AF: 
      case 0x08F: case 0x08E: 
         if (!allow_VX) goto decode_noVX;
         if (dis_fp_round(theInstr)) goto decode_success;
         goto decode_failure;

      
      case 0x1E8: 
      case 0x1C8: 
      case 0x188: 
      case 0x1A8: 
         
         if ((allow_F && allow_V && allow_FX && allow_GX) &&
             (dis_fp_round(theInstr)))
            goto decode_success;
         goto decode_failure;
         
               
      case 0x008: 
      case 0x028: 
      case 0x048: 
      case 0x088: 
      case 0x108: 
         if (dis_fp_move( theInstr )) goto decode_success;
         goto decode_failure;

      case 0x3c6: case 0x346:          
         if (dis_fp_merge( theInstr )) goto decode_success;
         goto decode_failure;

               
      case 0x026: 
      case 0x040: 
      case 0x046: 
      case 0x086: 
      case 0x247: 
      case 0x2C7: 
         
         
         if (dis_fp_scr( theInstr, allow_GX )) goto decode_success;
         goto decode_failure;

      default:
         break; 
      }

      opc2 = ifieldOPClo9( theInstr );
      switch (opc2) {
      case 0x42: 
      case 0x62: 
         if (!allow_DFP) goto decode_noDFP;
         if (dis_dfp_shiftq( theInstr ))
            goto decode_success;
         goto decode_failure;
      case 0xc2:  
      case 0xe2:  
         if (!allow_DFP) goto decode_noDFP;
         if (dis_dfp_class_test( theInstr ))
            goto decode_success;
         goto decode_failure;
      default:
         break;
      }

      opc2 = ifieldOPClo8( theInstr );
      switch (opc2) {
      case 0x3:   
      case 0x23:  
      case 0x43:  
         if (!allow_DFP) goto decode_noDFP;
         if (dis_dfp_quantize_sig_rrndq( theInstr ))
            goto decode_success;
         goto decode_failure;
      case 0xA2: 
         if (dis_dfp_exponent_test( theInstr ) )
            goto decode_success;
         goto decode_failure;
      case 0x63:  
      case 0xE3:  
         if (!allow_DFP) goto decode_noDFP;
         if (dis_dfp_roundq( theInstr ))
            goto decode_success;
         goto decode_failure;

      default:
         goto decode_failure;
      }
      break;

   case 0x13:
      switch (opc2) {

      
      case 0x101: case 0x081: case 0x121: 
      case 0x0E1: case 0x021: case 0x1C1: 
      case 0x1A1: case 0x0C1: case 0x000: 
         if (dis_cond_logic( theInstr )) goto decode_success;
         goto decode_failure;
         
      
      case 0x210: case 0x010: 
         if (dis_branch(theInstr, abiinfo, &dres, 
                                  resteerOkFn, callback_opaque)) 
            goto decode_success;
         goto decode_failure;
         
      
      case 0x096: 
         if (dis_memsync( theInstr )) goto decode_success;
         goto decode_failure;

      default:
         goto decode_failure;
      }
      break;


   case 0x1F:

      

      opc2 = IFIELD(theInstr, 1, 9);
      switch (opc2) {
      
      case 0x10A: case 0x00A: case 0x08A: 
      case 0x0EA: case 0x0CA: case 0x1EB: 
      case 0x1CB: case 0x04B: case 0x00B: 
      case 0x0EB: case 0x068: case 0x028: 
      case 0x008: case 0x088: case 0x0E8: 
      case 0x0C8: 
         if (dis_int_arith( theInstr )) goto decode_success;
         goto decode_failure;

      case 0x18B: 
      case 0x1AB: 
         if (!allow_VX) goto decode_noVX;
         if (dis_int_arith( theInstr )) goto decode_success;
         goto decode_failure;

      
      case 0x009: case 0x049: case 0x0E9: 
      case 0x1C9: case 0x1E9: 
         if (!mode64) goto decode_failure;
         if (dis_int_arith( theInstr )) goto decode_success;
         goto decode_failure;

      case 0x1A9: 
      case 0x189: 
         if (!allow_VX) goto decode_noVX;
         if (!mode64) goto decode_failure;
         if (dis_int_arith( theInstr )) goto decode_success;
         goto decode_failure;

      case 0x1FC:                         
         if (dis_int_logic( theInstr )) goto decode_success;
         goto decode_failure;

      default:
         break;  
      }

      

      opc2 = IFIELD(theInstr, 1, 10);
      switch (opc2) {
      
      case 0x000: case 0x020: 
         if (dis_int_cmp( theInstr )) goto decode_success;
         goto decode_failure;

      
      case 0x01C: case 0x03C: case 0x01A: 
      case 0x11C: case 0x3BA: case 0x39A: 
      case 0x1DC: case 0x07C: case 0x1BC: 
      case 0x19C: case 0x13C:             
      case 0x2DF: case 0x25F:            
         if (dis_int_logic( theInstr )) goto decode_success;
         goto decode_failure;

      case 0x28E: case 0x2AE:             
      case 0x2EE: case 0x2CE: case 0x30E: 
      case 0x32E: case 0x34E: case 0x36E: 
      case 0x38E: case 0x3AE: case 0x3EE: 
      if (dis_transactional_memory( theInstr,
                                    getUIntPPCendianly( &guest_code[delta + 4]),
                                    abiinfo, &dres,
                                    resteerOkFn, callback_opaque))
            goto decode_success;
         goto decode_failure;

      
      case 0x3DA: case 0x03A: 
         if (!mode64) goto decode_failure;
         if (dis_int_logic( theInstr )) goto decode_success;
         goto decode_failure;

         
      case 0xba: 
         if (!mode64) goto decode_failure;
         if (dis_int_parity( theInstr )) goto decode_success;
         goto decode_failure;

      case 0x9a: 
         if (dis_int_parity( theInstr )) goto decode_success;
         goto decode_failure;

      
      case 0x018: case 0x318: case 0x338: 
      case 0x218:                         
         if (dis_int_shift( theInstr )) goto decode_success;
         goto decode_failure;

      
      case 0x01B: case 0x31A: 
      case 0x33A: case 0x33B: 
      case 0x21B:             
         if (!mode64) goto decode_failure;
         if (dis_int_shift( theInstr )) goto decode_success;
         goto decode_failure;

      
      case 0x057: case 0x077: case 0x157: 
      case 0x177: case 0x117: case 0x137: 
      case 0x017: case 0x037:             
         if (dis_int_load( theInstr )) goto decode_success;
         goto decode_failure;

      
      case 0x035: case 0x015:             
      case 0x175: case 0x155:             
         if (!mode64) goto decode_failure;
         if (dis_int_load( theInstr )) goto decode_success;
         goto decode_failure;

      
      case 0x0F7: case 0x0D7: case 0x1B7: 
      case 0x197: case 0x0B7: case 0x097: 
         if (dis_int_store( theInstr, abiinfo )) goto decode_success;
         goto decode_failure;

      
      case 0x0B5: case 0x095: 
         if (!mode64) goto decode_failure;
         if (dis_int_store( theInstr, abiinfo )) goto decode_success;
         goto decode_failure;

      
      case 0x214: case 0x294: 
         if (!mode64) goto decode_failure;
         if (dis_int_ldst_rev( theInstr )) goto decode_success;
         goto decode_failure;

      case 0x216: case 0x316: case 0x296:    
      case 0x396:                            
         if (dis_int_ldst_rev( theInstr )) goto decode_success;
         goto decode_failure;
         
      
      case 0x255: case 0x215: case 0x2D5: 
      case 0x295: {                       
         Bool stopHere = False;
         Bool ok = dis_int_ldst_str( theInstr, &stopHere );
         if (!ok) goto decode_failure;
         if (stopHere) {
            putGST( PPC_GST_CIA, mkSzImm(ty, nextInsnAddr()) );
            dres.jk_StopHere = Ijk_Boring;
            dres.whatNext    = Dis_StopHere;
         }
         goto decode_success;
      }

      
      case 0x034: case 0x074:             
      case 0x2B6: case 0x2D6:             
         if (!allow_isa_2_07) goto decode_noP8;
         if (dis_memsync( theInstr )) goto decode_success;
         goto decode_failure;

      case 0x356: case 0x014: case 0x096: 
      case 0x256:                         
         if (dis_memsync( theInstr )) goto decode_success;
         goto decode_failure;
         
      
      case 0x054: case 0x0D6: 
         if (!mode64) goto decode_failure;
         if (dis_memsync( theInstr )) goto decode_success;
         goto decode_failure;

      case 0x114: case 0x0B6: 
         if (dis_memsync( theInstr )) goto decode_success;
         goto decode_failure;

      
      case 0x33:  case 0x73: 
      case 0xB3:  case 0xD3: case 0xF3: 
      case 0x200: case 0x013: case 0x153: 
      case 0x173: case 0x090: case 0x1D3: 
      case 0x220:                         
         if (dis_proc_ctl( abiinfo, theInstr )) goto decode_success;
         goto decode_failure;

      
      case 0x2F6: case 0x056: case 0x036: 
      case 0x116: case 0x0F6: case 0x3F6: 
      case 0x3D6:                         
         if (dis_cache_manage( theInstr, &dres, archinfo ))
            goto decode_success;
         goto decode_failure;


      
      case 0x004:             
         if (dis_trap(theInstr, &dres)) goto decode_success;
         goto decode_failure;

      case 0x044:             
         if (!mode64) goto decode_failure;
         if (dis_trap(theInstr, &dres)) goto decode_success;
         goto decode_failure;

      
      case 0x217: case 0x237: case 0x257: 
      case 0x277:                         
         if (!allow_F) goto decode_noF;
         if (dis_fp_load( theInstr )) goto decode_success;
         goto decode_failure;

      
      case 0x297: case 0x2B7: case 0x2D7: 
      case 0x2F7:                         
         if (!allow_F) goto decode_noF;
         if (dis_fp_store( theInstr )) goto decode_success;
         goto decode_failure;
      case 0x3D7:                         
         if (!allow_F) goto decode_noF;
         if (!allow_GX) goto decode_noGX;
         if (dis_fp_store( theInstr )) goto decode_success;
         goto decode_failure;

         
      case 0x317: 
      case 0x397: 
         if (!allow_F) goto decode_noF;
         if (dis_fp_pair(theInstr)) goto decode_success;
         goto decode_failure;

      case 0x357:                         
         if (!allow_F) goto decode_noF;
         if (dis_fp_load( theInstr )) goto decode_success;
         goto decode_failure;

      case 0x377:                         
         if (!allow_F) goto decode_noF;
         if (dis_fp_load( theInstr )) goto decode_success;
         goto decode_failure;

      

      
      case 0x156: case 0x176: case 0x336: 
         if (!allow_V) goto decode_noV;
         if (dis_av_datastream( theInstr )) goto decode_success;
         goto decode_failure;

      
      case 0x006: case 0x026:             
      case 0x007: case 0x027: case 0x047: 
      case 0x067: case 0x167:             
         if (!allow_V) goto decode_noV;
         if (dis_av_load( abiinfo, theInstr )) goto decode_success;
         goto decode_failure;

      
      case 0x087: case 0x0A7: case 0x0C7: 
      case 0x0E7: case 0x1E7:             
         if (!allow_V) goto decode_noV;
         if (dis_av_store( theInstr )) goto decode_success;
         goto decode_failure;

      
      case 0x00C: 
      case 0x04C: 
      case 0x20C: 
      case 0x24C: 
      case 0x34C: 
      case 0x14C: 
      case 0x30C: 
        
        
        if (!allow_V) goto decode_noV;

	if (dis_vx_load( theInstr )) goto decode_success;
          goto decode_failure;

      
      case 0x08C: 
      case 0x28C: 
      case 0x2CC: 
      case 0x3CC: 
      case 0x38C: 
        
        
        if (!allow_V) goto decode_noV;

	if (dis_vx_store( theInstr )) goto decode_success;
    	  goto decode_failure;

      
      case 0x1FA: 
      case 0x17A: 
      case 0x7A:  
	  if (dis_int_logic( theInstr )) goto decode_success;
    	  goto decode_failure;

      case 0x0FC: 
         if (!mode64) goto decode_failure;
         if (dis_int_logic( theInstr )) goto decode_success;
         goto decode_failure;

      default:
         
         if (IFIELD(theInstr, 0, 6) == (15<<1)) {
            UInt rT = ifieldRegDS( theInstr );
            UInt rA = ifieldRegA( theInstr );
            UInt rB = ifieldRegB( theInstr );
            UInt bi = ifieldRegC( theInstr );
            putIReg(
               rT,
               IRExpr_ITE( binop(Iop_CmpNE32, getCRbit( bi ), mkU32(0)),
                           rA == 0 ? (mode64 ? mkU64(0) : mkU32(0))
                                   : getIReg(rA),
                           getIReg(rB))

            );
            DIP("isel r%u,r%u,r%u,crb%u\n", rT,rA,rB,bi);
            goto decode_success;
         }
         goto decode_failure;
      }
      break;


   case 0x04:
      

      opc2 = IFIELD(theInstr, 0, 6);
      switch (opc2) {
      
      case 0x20: case 0x21: case 0x22: 
      case 0x24: case 0x25: case 0x26: 
      case 0x27: case 0x28: case 0x29: 
         if (!allow_V) goto decode_noV;
         if (dis_av_multarith( theInstr )) goto decode_success;
         goto decode_failure;

      
      case 0x2A:                       
      case 0x2B:                       
      case 0x2C:                       
         if (!allow_V) goto decode_noV;
         if (dis_av_permute( theInstr )) goto decode_success;
         goto decode_failure;

      case 0x2D:                       
         if (!allow_isa_2_07) goto decode_noP8;
         if (dis_av_permute( theInstr )) goto decode_success;
         goto decode_failure;

      
      case 0x2E: case 0x2F:            
         if (!allow_V) goto decode_noV;
         if (dis_av_fp_arith( theInstr )) goto decode_success;
         goto decode_failure;

      case 0x3D: case 0x3C:            
      case 0x3F: case 0x3E:            
         if (!allow_V) goto decode_noV;
         if (dis_av_quad( theInstr)) goto decode_success;
         goto decode_failure;

      default:
         break;  
      }

      opc2 = IFIELD(theInstr, 0, 9);
      switch (opc2) {
      
      case 0x1: case 0x41:             
         if (!allow_isa_2_07) goto decode_noP8;
         if (dis_av_bcd( theInstr )) goto decode_success;
         goto decode_failure;

      default:
         break;  
      }

      opc2 = IFIELD(theInstr, 0, 11);
      switch (opc2) {
      
      case 0x180:                         
      case 0x000: case 0x040: case 0x080: 
      case 0x200: case 0x240: case 0x280: 
      case 0x300: case 0x340: case 0x380: 
      case 0x580:                         
      case 0x400: case 0x440: case 0x480: 
      case 0x600: case 0x640: case 0x680: 
      case 0x700: case 0x740: case 0x780: 
      case 0x402: case 0x442: case 0x482: 
      case 0x502: case 0x542: case 0x582: 
      case 0x002: case 0x042: case 0x082: 
      case 0x102: case 0x142: case 0x182: 
      case 0x202: case 0x242: case 0x282: 
      case 0x302: case 0x342: case 0x382: 
      case 0x008: case 0x048:             
      case 0x108: case 0x148:             
      case 0x208: case 0x248:             
      case 0x308: case 0x348:             
      case 0x608: case 0x708: case 0x648: 
      case 0x688: case 0x788:             
         if (!allow_V) goto decode_noV;
         if (dis_av_arith( theInstr )) goto decode_success;
         goto decode_failure;

      case 0x088: case 0x089:             
      case 0x0C0: case 0x0C2:             
      case 0x1C2: case 0x2C2: case 0x3C2: 
      case 0x188: case 0x288: case 0x388: 
      case 0x4C0:                         
         if (!allow_isa_2_07) goto decode_noP8;
         if (dis_av_arith( theInstr )) goto decode_success;
         goto decode_failure;

      
      case 0x408: case 0x448:            
      case 0x488: case 0x4C8:            
         if (!allow_isa_2_07) goto decode_noP8;
         if (dis_av_polymultarith( theInstr )) goto decode_success;
         goto decode_failure;

      
      case 0x004: case 0x044: case 0x084: 
      case 0x104: case 0x144: case 0x184: 
      case 0x204: case 0x244: case 0x284: 
      case 0x304: case 0x344: case 0x384: 
      case 0x1C4: case 0x2C4:             
      case 0x40C: case 0x44C:             
         if (!allow_V) goto decode_noV;
         if (dis_av_shift( theInstr )) goto decode_success;
         goto decode_failure;

      case 0x0C4:                         
      case 0x3C4: case 0x5C4: case 0x6C4: 
          if (!allow_isa_2_07) goto decode_noP8;
          if (dis_av_shift( theInstr )) goto decode_success;
          goto decode_failure;

      
      case 0x404: case 0x444: case 0x484: 
      case 0x4C4: case 0x504:             
         if (!allow_V) goto decode_noV;
         if (dis_av_logic( theInstr )) goto decode_success;
         goto decode_failure;

      case 0x544:                         
      case 0x584: case 0x684:             
         if (!allow_isa_2_07) goto decode_noP8;
         if (dis_av_logic( theInstr )) goto decode_success;
         goto decode_failure;

      
      case 0x604: case 0x644:             
         if (!allow_V) goto decode_noV;
         if (dis_av_procctl( theInstr )) goto decode_success;
         goto decode_failure;

      
      case 0x00A: case 0x04A:             
      case 0x10A: case 0x14A: case 0x18A: 
      case 0x1CA:                         
      case 0x40A: case 0x44A:             
         if (!allow_V) goto decode_noV;
         if (dis_av_fp_arith( theInstr )) goto decode_success;
         goto decode_failure;

      
      case 0x20A: case 0x24A: case 0x28A: 
      case 0x2CA:                         
      case 0x30A: case 0x34A: case 0x38A: 
      case 0x3CA:                         
         if (!allow_V) goto decode_noV;
         if (dis_av_fp_convert( theInstr )) goto decode_success;
         goto decode_failure;

      
      case 0x00C: case 0x04C: case 0x08C: 
      case 0x10C: case 0x14C: case 0x18C: 
      case 0x20C: case 0x24C: case 0x28C: 
      case 0x30C: case 0x34C: case 0x38C: 
         if (!allow_V) goto decode_noV;
         if (dis_av_permute( theInstr )) goto decode_success;
         goto decode_failure;

      case 0x68C: case 0x78C:             
          if (!allow_isa_2_07) goto decode_noP8;
          if (dis_av_permute( theInstr )) goto decode_success;
          goto decode_failure;

      
      case 0x00E: case 0x04E: case 0x08E: 
      case 0x0CE:                         
      case 0x10E: case 0x14E: case 0x18E: 
      case 0x1CE:                         
      case 0x20E: case 0x24E: case 0x28E: 
      case 0x2CE:                         
      case 0x30E: case 0x34E: case 0x3CE: 
          if (!allow_V) goto decode_noV;
          if (dis_av_pack( theInstr )) goto decode_success;
          goto decode_failure;

      case 0x44E: case 0x4CE: case 0x54E: 
      case 0x5CE: case 0x64E: case 0x6cE: 
         if (!allow_isa_2_07) goto decode_noP8;
         if (dis_av_pack( theInstr )) goto decode_success;
         goto decode_failure;

      case 0x508: case 0x509:             
      case 0x548: case 0x549:             
      case 0x5C8:                         
         if (!allow_isa_2_07) goto decode_noP8;
         if (dis_av_cipher( theInstr )) goto decode_success;
         goto decode_failure;

      case 0x6C2: case 0x682:             
         if (!allow_isa_2_07) goto decode_noP8;
         if (dis_av_hash( theInstr )) goto decode_success;
         goto decode_failure;

      case 0x702: case 0x742:             
      case 0x782: case 0x7c2:             
         if (!allow_isa_2_07) goto decode_noP8;
         if (dis_av_count_bitTranspose( theInstr, opc2 )) goto decode_success;
         goto decode_failure;

      case 0x703: case 0x743:             
      case 0x783: case 0x7c3:             
         if (!allow_isa_2_07) goto decode_noP8;
         if (dis_av_count_bitTranspose( theInstr, opc2 )) goto decode_success;
         goto decode_failure;

      case 0x50c:                         
         if (!allow_isa_2_07) goto decode_noP8;
         if (dis_av_count_bitTranspose( theInstr, opc2 )) goto decode_success;
         goto decode_failure;

      case 0x140: case 0x100:             
      case 0x540: case 0x500:             
      case 0x54C:                         
         if (!allow_V) goto decode_noV;
         if (dis_av_quad( theInstr)) goto decode_success;
         goto decode_failure;

      default:
         break;  
      }

      opc2 = IFIELD(theInstr, 0, 10);
      switch (opc2) {

      
      case 0x006: case 0x046: case 0x086: 
      case 0x206: case 0x246: case 0x286: 
      case 0x306: case 0x346: case 0x386: 
         if (!allow_V) goto decode_noV;
         if (dis_av_cmp( theInstr )) goto decode_success;
         goto decode_failure;

      case 0x0C7:                         
      case 0x2C7:                         
      case 0x3C7:                         
          if (!allow_isa_2_07) goto decode_noP8;
          if (dis_av_cmp( theInstr )) goto decode_success;
          goto decode_failure;

      
      case 0x0C6: case 0x1C6: case 0x2C6: 
      case 0x3C6:                         
         if (!allow_V) goto decode_noV;
         if (dis_av_fp_cmp( theInstr )) goto decode_success;
         goto decode_failure;

      default:
         goto decode_failure;
      }
      break;

   default:
      goto decode_failure;

   decode_noF:
      vassert(!allow_F);
      vex_printf("disInstr(ppc): found the Floating Point instruction 0x%x that\n"
		 "can't be handled by Valgrind on this host.  This instruction\n"
		 "requires a host that supports Floating Point instructions.\n",
		 theInstr);
      goto not_supported;
   decode_noV:
      vassert(!allow_V);
      vex_printf("disInstr(ppc): found an AltiVec or an e500 instruction 0x%x\n"
		 "that can't be handled by Valgrind.  If this instruction is an\n"
		 "Altivec instruction, Valgrind must be run on a host that supports"
		 "AltiVec instructions.  If the application was compiled for e500, then\n"
		 "unfortunately Valgrind does not yet support e500 instructions.\n",
		 theInstr);
      goto not_supported;
   decode_noVX:
      vassert(!allow_VX);
      vex_printf("disInstr(ppc): found the instruction 0x%x that is defined in the\n"
		 "Power ISA 2.06 ABI but can't be handled by Valgrind on this host.\n"
		 "This instruction \nrequires a host that supports the ISA 2.06 ABI.\n",
		 theInstr);
      goto not_supported;
   decode_noFX:
      vassert(!allow_FX);
      vex_printf("disInstr(ppc): found the General Purpose-Optional instruction 0x%x\n"
		 "that can't be handled by Valgrind on this host. This instruction\n"
		 "requires a host that supports the General Purpose-Optional instructions.\n",
		 theInstr);
      goto not_supported;
   decode_noGX:
      vassert(!allow_GX);
      vex_printf("disInstr(ppc): found the Graphics-Optional instruction 0x%x\n"
		 "that can't be handled by Valgrind on this host. This instruction\n"
		 "requires a host that supports the Graphic-Optional instructions.\n",
		 theInstr);
      goto not_supported;
   decode_noDFP:
      vassert(!allow_DFP);
      vex_printf("disInstr(ppc): found the decimal floating point (DFP) instruction 0x%x\n"
		 "that can't be handled by Valgrind on this host.  This instruction\n"
		 "requires a host that supports DFP instructions.\n",
		 theInstr);
      goto not_supported;
   decode_noP8:
      vassert(!allow_isa_2_07);
      vex_printf("disInstr(ppc): found the Power 8 instruction 0x%x that can't be handled\n"
		 "by Valgrind on this host.  This instruction requires a host that\n"
		 "supports Power 8 instructions.\n",
		 theInstr);
      goto not_supported;


   decode_failure:
   
   opc2 = (theInstr) & 0x7FF;
   if (sigill_diag) {
      vex_printf("disInstr(ppc): unhandled instruction: "
                 "0x%x\n", theInstr);
      vex_printf("                 primary %d(0x%x), secondary %u(0x%x)\n", 
                 opc1, opc1, opc2, opc2);
   }

   not_supported:
   putGST( PPC_GST_CIA, mkSzImm(ty, guest_CIA_curr_instr) );
   dres.len         = 0;
   dres.whatNext    = Dis_StopHere;
   dres.jk_StopHere = Ijk_NoDecode;
   dres.continueAt  = 0;
   return dres;
   } 

  decode_success:
   
   switch (dres.whatNext) {
      case Dis_Continue:
         putGST( PPC_GST_CIA, mkSzImm(ty, guest_CIA_curr_instr + 4));
         break;
      case Dis_ResteerU:
      case Dis_ResteerC:
         putGST( PPC_GST_CIA, mkSzImm(ty, dres.continueAt));
         break;
      case Dis_StopHere:
         break;
      default:
         vassert(0);
   }
   DIP("\n");

   if (dres.len == 0) {
      dres.len = 4;
   } else {
      vassert(dres.len == 20);
   }
   return dres;
}

#undef DIP
#undef DIS




DisResult disInstr_PPC ( IRSB*        irsb_IN,
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
   IRType     ty;
   DisResult  dres;
   UInt       mask32, mask64;
   UInt hwcaps_guest = archinfo->hwcaps;

   vassert(guest_arch == VexArchPPC32 || guest_arch == VexArchPPC64);

   
   mode64 = guest_arch == VexArchPPC64;
   ty = mode64 ? Ity_I64 : Ity_I32;
   if (!mode64 && (host_endness_IN == VexEndnessLE)) {
      vex_printf("disInstr(ppc): Little Endian 32-bit mode is not supported\n");
      dres.len         = 0;
      dres.whatNext    = Dis_StopHere;
      dres.jk_StopHere = Ijk_NoDecode;
      dres.continueAt   = 0;
      return dres;
   }

   
   mask32 = VEX_HWCAPS_PPC32_F | VEX_HWCAPS_PPC32_V
            | VEX_HWCAPS_PPC32_FX | VEX_HWCAPS_PPC32_GX | VEX_HWCAPS_PPC32_VX
            | VEX_HWCAPS_PPC32_DFP | VEX_HWCAPS_PPC32_ISA2_07;

   mask64 = VEX_HWCAPS_PPC64_V | VEX_HWCAPS_PPC64_FX
            | VEX_HWCAPS_PPC64_GX | VEX_HWCAPS_PPC64_VX | VEX_HWCAPS_PPC64_DFP
            | VEX_HWCAPS_PPC64_ISA2_07;

   if (mode64) {
      vassert((hwcaps_guest & mask32) == 0);
   } else {
      vassert((hwcaps_guest & mask64) == 0);
   }

   
   guest_code           = guest_code_IN;
   irsb                 = irsb_IN;
   host_endness         = host_endness_IN;

   guest_CIA_curr_instr = mkSzAddr(ty, guest_IP);
   guest_CIA_bbstart    = mkSzAddr(ty, guest_IP - delta);

   dres = disInstr_PPC_WRK ( resteerOkFn, resteerCisOk, callback_opaque,
                             delta, archinfo, abiinfo, sigill_diag_IN);

   return dres;
}




