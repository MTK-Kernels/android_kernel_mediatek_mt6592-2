

/*
   This file is part of MemCheck, a heavyweight Valgrind tool for
   detecting memory errors.

   Copyright (C) 2000-2013 Julian Seward 
      jseward@acm.org

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

#include "pub_tool_basics.h"
#include "pub_tool_poolalloc.h"     
#include "pub_tool_hashtable.h"     
#include "pub_tool_libcassert.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_machine.h"     
#include "pub_tool_xarray.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_libcbase.h"

#include "mc_include.h"






struct _MCEnv;

static IRType  shadowTypeV ( IRType ty );
static IRExpr* expr2vbits ( struct _MCEnv* mce, IRExpr* e );
static IRTemp  findShadowTmpB ( struct _MCEnv* mce, IRTemp orig );

static IRExpr *i128_const_zero(void);


typedef
   enum { Orig=1, VSh=2, BSh=3 }
   TempKind;

typedef
   struct {
      TempKind kind;
      IRTemp   shadowV;
      IRTemp   shadowB;
   }
   TempMapEnt;


typedef
   struct _MCEnv {
      IRSB* sb;
      Bool  trace;

      XArray*  tmpMap;

      Bool bogusLiterals;

      Bool useLLVMworkarounds;

      const VexGuestLayout* layout;

      IRType hWordTy;
   }
   MCEnv;


static IRTemp newTemp ( MCEnv* mce, IRType ty, TempKind kind )
{
   Word       newIx;
   TempMapEnt ent;
   IRTemp     tmp = newIRTemp(mce->sb->tyenv, ty);
   ent.kind    = kind;
   ent.shadowV = IRTemp_INVALID;
   ent.shadowB = IRTemp_INVALID;
   newIx = VG_(addToXA)( mce->tmpMap, &ent );
   tl_assert(newIx == (Word)tmp);
   return tmp;
}


static IRTemp findShadowTmpV ( MCEnv* mce, IRTemp orig )
{
   TempMapEnt* ent;
   ent = (TempMapEnt*)VG_(indexXA)( mce->tmpMap, (Word)orig );
   tl_assert(ent->kind == Orig);
   if (ent->shadowV == IRTemp_INVALID) {
      IRTemp tmpV
        = newTemp( mce, shadowTypeV(mce->sb->tyenv->types[orig]), VSh );
      ent = (TempMapEnt*)VG_(indexXA)( mce->tmpMap, (Word)orig );
      tl_assert(ent->kind == Orig);
      tl_assert(ent->shadowV == IRTemp_INVALID);
      ent->shadowV = tmpV;
   }
   return ent->shadowV;
}

static void newShadowTmpV ( MCEnv* mce, IRTemp orig )
{
   TempMapEnt* ent;
   ent = (TempMapEnt*)VG_(indexXA)( mce->tmpMap, (Word)orig );
   tl_assert(ent->kind == Orig);
   if (1) {
      IRTemp tmpV
        = newTemp( mce, shadowTypeV(mce->sb->tyenv->types[orig]), VSh );
      ent = (TempMapEnt*)VG_(indexXA)( mce->tmpMap, (Word)orig );
      tl_assert(ent->kind == Orig);
      ent->shadowV = tmpV;
   }
}




typedef  IRExpr  IRAtom;

static Bool isOriginalAtom ( MCEnv* mce, IRAtom* a1 )
{
   if (a1->tag == Iex_Const)
      return True;
   if (a1->tag == Iex_RdTmp) {
      TempMapEnt* ent = VG_(indexXA)( mce->tmpMap, a1->Iex.RdTmp.tmp );
      return ent->kind == Orig;
   }
   return False;
}

static Bool isShadowAtom ( MCEnv* mce, IRAtom* a1 )
{
   if (a1->tag == Iex_Const)
      return True;
   if (a1->tag == Iex_RdTmp) {
      TempMapEnt* ent = VG_(indexXA)( mce->tmpMap, a1->Iex.RdTmp.tmp );
      return ent->kind == VSh || ent->kind == BSh;
   }
   return False;
}

static Bool sameKindedAtoms ( IRAtom* a1, IRAtom* a2 )
{
   if (a1->tag == Iex_RdTmp && a2->tag == Iex_RdTmp)
      return True;
   if (a1->tag == Iex_Const && a2->tag == Iex_Const)
      return True;
   return False;
}




static IRType shadowTypeV ( IRType ty )
{
   switch (ty) {
      case Ity_I1:
      case Ity_I8:
      case Ity_I16:
      case Ity_I32: 
      case Ity_I64: 
      case Ity_I128: return ty;
      case Ity_F16:  return Ity_I16;
      case Ity_F32:  return Ity_I32;
      case Ity_D32:  return Ity_I32;
      case Ity_F64:  return Ity_I64;
      case Ity_D64:  return Ity_I64;
      case Ity_F128: return Ity_I128;
      case Ity_D128: return Ity_I128;
      case Ity_V128: return Ity_V128;
      case Ity_V256: return Ity_V256;
      default: ppIRType(ty); 
               VG_(tool_panic)("memcheck:shadowTypeV");
   }
}

static IRExpr* definedOfType ( IRType ty ) {
   switch (ty) {
      case Ity_I1:   return IRExpr_Const(IRConst_U1(False));
      case Ity_I8:   return IRExpr_Const(IRConst_U8(0));
      case Ity_I16:  return IRExpr_Const(IRConst_U16(0));
      case Ity_I32:  return IRExpr_Const(IRConst_U32(0));
      case Ity_I64:  return IRExpr_Const(IRConst_U64(0));
      case Ity_I128: return i128_const_zero();
      case Ity_V128: return IRExpr_Const(IRConst_V128(0x0000));
      case Ity_V256: return IRExpr_Const(IRConst_V256(0x00000000));
      default:       VG_(tool_panic)("memcheck:definedOfType");
   }
}



static inline void stmt ( HChar cat, MCEnv* mce, IRStmt* st ) {
   if (mce->trace) {
      VG_(printf)("  %c: ", cat);
      ppIRStmt(st);
      VG_(printf)("\n");
   }
   addStmtToIRSB(mce->sb, st);
}

static inline 
void assign ( HChar cat, MCEnv* mce, IRTemp tmp, IRExpr* expr ) {
   stmt(cat, mce, IRStmt_WrTmp(tmp,expr));
}

#define triop(_op, _arg1, _arg2, _arg3) \
                                 IRExpr_Triop((_op),(_arg1),(_arg2),(_arg3))
#define binop(_op, _arg1, _arg2) IRExpr_Binop((_op),(_arg1),(_arg2))
#define unop(_op, _arg)          IRExpr_Unop((_op),(_arg))
#define mkU1(_n)                 IRExpr_Const(IRConst_U1(_n))
#define mkU8(_n)                 IRExpr_Const(IRConst_U8(_n))
#define mkU16(_n)                IRExpr_Const(IRConst_U16(_n))
#define mkU32(_n)                IRExpr_Const(IRConst_U32(_n))
#define mkU64(_n)                IRExpr_Const(IRConst_U64(_n))
#define mkV128(_n)               IRExpr_Const(IRConst_V128(_n))
#define mkexpr(_tmp)             IRExpr_RdTmp((_tmp))

static IRAtom* assignNew ( HChar cat, MCEnv* mce, IRType ty, IRExpr* e )
{
   TempKind k;
   IRTemp   t;
   IRType   tyE = typeOfIRExpr(mce->sb->tyenv, e);

   tl_assert(tyE == ty); 
   switch (cat) {
      case 'V': k = VSh;  break;
      case 'B': k = BSh;  break;
      case 'C': k = Orig; break; 
      default: tl_assert(0);
   }
   t = newTemp(mce, ty, k);
   assign(cat, mce, t, e);
   return mkexpr(t);
}



static IRExpr *i128_const_zero(void)
{
   IRAtom* z64 = IRExpr_Const(IRConst_U64(0));
   return binop(Iop_64HLto128, z64, z64);
}





static IRAtom* mkDifD8 ( MCEnv* mce, IRAtom* a1, IRAtom* a2 ) {
   tl_assert(isShadowAtom(mce,a1));
   tl_assert(isShadowAtom(mce,a2));
   return assignNew('V', mce, Ity_I8, binop(Iop_And8, a1, a2));
}

static IRAtom* mkDifD16 ( MCEnv* mce, IRAtom* a1, IRAtom* a2 ) {
   tl_assert(isShadowAtom(mce,a1));
   tl_assert(isShadowAtom(mce,a2));
   return assignNew('V', mce, Ity_I16, binop(Iop_And16, a1, a2));
}

static IRAtom* mkDifD32 ( MCEnv* mce, IRAtom* a1, IRAtom* a2 ) {
   tl_assert(isShadowAtom(mce,a1));
   tl_assert(isShadowAtom(mce,a2));
   return assignNew('V', mce, Ity_I32, binop(Iop_And32, a1, a2));
}

static IRAtom* mkDifD64 ( MCEnv* mce, IRAtom* a1, IRAtom* a2 ) {
   tl_assert(isShadowAtom(mce,a1));
   tl_assert(isShadowAtom(mce,a2));
   return assignNew('V', mce, Ity_I64, binop(Iop_And64, a1, a2));
}

static IRAtom* mkDifDV128 ( MCEnv* mce, IRAtom* a1, IRAtom* a2 ) {
   tl_assert(isShadowAtom(mce,a1));
   tl_assert(isShadowAtom(mce,a2));
   return assignNew('V', mce, Ity_V128, binop(Iop_AndV128, a1, a2));
}

static IRAtom* mkDifDV256 ( MCEnv* mce, IRAtom* a1, IRAtom* a2 ) {
   tl_assert(isShadowAtom(mce,a1));
   tl_assert(isShadowAtom(mce,a2));
   return assignNew('V', mce, Ity_V256, binop(Iop_AndV256, a1, a2));
}


static IRAtom* mkUifU8 ( MCEnv* mce, IRAtom* a1, IRAtom* a2 ) {
   tl_assert(isShadowAtom(mce,a1));
   tl_assert(isShadowAtom(mce,a2));
   return assignNew('V', mce, Ity_I8, binop(Iop_Or8, a1, a2));
}

static IRAtom* mkUifU16 ( MCEnv* mce, IRAtom* a1, IRAtom* a2 ) {
   tl_assert(isShadowAtom(mce,a1));
   tl_assert(isShadowAtom(mce,a2));
   return assignNew('V', mce, Ity_I16, binop(Iop_Or16, a1, a2));
}

static IRAtom* mkUifU32 ( MCEnv* mce, IRAtom* a1, IRAtom* a2 ) {
   tl_assert(isShadowAtom(mce,a1));
   tl_assert(isShadowAtom(mce,a2));
   return assignNew('V', mce, Ity_I32, binop(Iop_Or32, a1, a2));
}

static IRAtom* mkUifU64 ( MCEnv* mce, IRAtom* a1, IRAtom* a2 ) {
   tl_assert(isShadowAtom(mce,a1));
   tl_assert(isShadowAtom(mce,a2));
   return assignNew('V', mce, Ity_I64, binop(Iop_Or64, a1, a2));
}

static IRAtom* mkUifU128 ( MCEnv* mce, IRAtom* a1, IRAtom* a2 ) {
   IRAtom *tmp1, *tmp2, *tmp3, *tmp4, *tmp5, *tmp6;
   tl_assert(isShadowAtom(mce,a1));
   tl_assert(isShadowAtom(mce,a2));
   tmp1 = assignNew('V', mce, Ity_I64, unop(Iop_128to64, a1));
   tmp2 = assignNew('V', mce, Ity_I64, unop(Iop_128HIto64, a1));
   tmp3 = assignNew('V', mce, Ity_I64, unop(Iop_128to64, a2));
   tmp4 = assignNew('V', mce, Ity_I64, unop(Iop_128HIto64, a2));
   tmp5 = assignNew('V', mce, Ity_I64, binop(Iop_Or64, tmp1, tmp3));
   tmp6 = assignNew('V', mce, Ity_I64, binop(Iop_Or64, tmp2, tmp4));

   return assignNew('V', mce, Ity_I128, binop(Iop_64HLto128, tmp6, tmp5));
}

static IRAtom* mkUifUV128 ( MCEnv* mce, IRAtom* a1, IRAtom* a2 ) {
   tl_assert(isShadowAtom(mce,a1));
   tl_assert(isShadowAtom(mce,a2));
   return assignNew('V', mce, Ity_V128, binop(Iop_OrV128, a1, a2));
}

static IRAtom* mkUifUV256 ( MCEnv* mce, IRAtom* a1, IRAtom* a2 ) {
   tl_assert(isShadowAtom(mce,a1));
   tl_assert(isShadowAtom(mce,a2));
   return assignNew('V', mce, Ity_V256, binop(Iop_OrV256, a1, a2));
}

static IRAtom* mkUifU ( MCEnv* mce, IRType vty, IRAtom* a1, IRAtom* a2 ) {
   switch (vty) {
      case Ity_I8:   return mkUifU8(mce, a1, a2);
      case Ity_I16:  return mkUifU16(mce, a1, a2);
      case Ity_I32:  return mkUifU32(mce, a1, a2);
      case Ity_I64:  return mkUifU64(mce, a1, a2);
      case Ity_I128: return mkUifU128(mce, a1, a2);
      case Ity_V128: return mkUifUV128(mce, a1, a2);
      case Ity_V256: return mkUifUV256(mce, a1, a2);
      default:
         VG_(printf)("\n"); ppIRType(vty); VG_(printf)("\n");
         VG_(tool_panic)("memcheck:mkUifU");
   }
}


static IRAtom* mkLeft8 ( MCEnv* mce, IRAtom* a1 ) {
   tl_assert(isShadowAtom(mce,a1));
   return assignNew('V', mce, Ity_I8, unop(Iop_Left8, a1));
}

static IRAtom* mkLeft16 ( MCEnv* mce, IRAtom* a1 ) {
   tl_assert(isShadowAtom(mce,a1));
   return assignNew('V', mce, Ity_I16, unop(Iop_Left16, a1));
}

static IRAtom* mkLeft32 ( MCEnv* mce, IRAtom* a1 ) {
   tl_assert(isShadowAtom(mce,a1));
   return assignNew('V', mce, Ity_I32, unop(Iop_Left32, a1));
}

static IRAtom* mkLeft64 ( MCEnv* mce, IRAtom* a1 ) {
   tl_assert(isShadowAtom(mce,a1));
   return assignNew('V', mce, Ity_I64, unop(Iop_Left64, a1));
}


static IRAtom* mkImproveAND8 ( MCEnv* mce, IRAtom* data, IRAtom* vbits )
{
   tl_assert(isOriginalAtom(mce, data));
   tl_assert(isShadowAtom(mce, vbits));
   tl_assert(sameKindedAtoms(data, vbits));
   return assignNew('V', mce, Ity_I8, binop(Iop_Or8, data, vbits));
}

static IRAtom* mkImproveAND16 ( MCEnv* mce, IRAtom* data, IRAtom* vbits )
{
   tl_assert(isOriginalAtom(mce, data));
   tl_assert(isShadowAtom(mce, vbits));
   tl_assert(sameKindedAtoms(data, vbits));
   return assignNew('V', mce, Ity_I16, binop(Iop_Or16, data, vbits));
}

static IRAtom* mkImproveAND32 ( MCEnv* mce, IRAtom* data, IRAtom* vbits )
{
   tl_assert(isOriginalAtom(mce, data));
   tl_assert(isShadowAtom(mce, vbits));
   tl_assert(sameKindedAtoms(data, vbits));
   return assignNew('V', mce, Ity_I32, binop(Iop_Or32, data, vbits));
}

static IRAtom* mkImproveAND64 ( MCEnv* mce, IRAtom* data, IRAtom* vbits )
{
   tl_assert(isOriginalAtom(mce, data));
   tl_assert(isShadowAtom(mce, vbits));
   tl_assert(sameKindedAtoms(data, vbits));
   return assignNew('V', mce, Ity_I64, binop(Iop_Or64, data, vbits));
}

static IRAtom* mkImproveANDV128 ( MCEnv* mce, IRAtom* data, IRAtom* vbits )
{
   tl_assert(isOriginalAtom(mce, data));
   tl_assert(isShadowAtom(mce, vbits));
   tl_assert(sameKindedAtoms(data, vbits));
   return assignNew('V', mce, Ity_V128, binop(Iop_OrV128, data, vbits));
}

static IRAtom* mkImproveANDV256 ( MCEnv* mce, IRAtom* data, IRAtom* vbits )
{
   tl_assert(isOriginalAtom(mce, data));
   tl_assert(isShadowAtom(mce, vbits));
   tl_assert(sameKindedAtoms(data, vbits));
   return assignNew('V', mce, Ity_V256, binop(Iop_OrV256, data, vbits));
}

static IRAtom* mkImproveOR8 ( MCEnv* mce, IRAtom* data, IRAtom* vbits )
{
   tl_assert(isOriginalAtom(mce, data));
   tl_assert(isShadowAtom(mce, vbits));
   tl_assert(sameKindedAtoms(data, vbits));
   return assignNew(
             'V', mce, Ity_I8, 
             binop(Iop_Or8, 
                   assignNew('V', mce, Ity_I8, unop(Iop_Not8, data)), 
                   vbits) );
}

static IRAtom* mkImproveOR16 ( MCEnv* mce, IRAtom* data, IRAtom* vbits )
{
   tl_assert(isOriginalAtom(mce, data));
   tl_assert(isShadowAtom(mce, vbits));
   tl_assert(sameKindedAtoms(data, vbits));
   return assignNew(
             'V', mce, Ity_I16, 
             binop(Iop_Or16, 
                   assignNew('V', mce, Ity_I16, unop(Iop_Not16, data)), 
                   vbits) );
}

static IRAtom* mkImproveOR32 ( MCEnv* mce, IRAtom* data, IRAtom* vbits )
{
   tl_assert(isOriginalAtom(mce, data));
   tl_assert(isShadowAtom(mce, vbits));
   tl_assert(sameKindedAtoms(data, vbits));
   return assignNew(
             'V', mce, Ity_I32, 
             binop(Iop_Or32, 
                   assignNew('V', mce, Ity_I32, unop(Iop_Not32, data)), 
                   vbits) );
}

static IRAtom* mkImproveOR64 ( MCEnv* mce, IRAtom* data, IRAtom* vbits )
{
   tl_assert(isOriginalAtom(mce, data));
   tl_assert(isShadowAtom(mce, vbits));
   tl_assert(sameKindedAtoms(data, vbits));
   return assignNew(
             'V', mce, Ity_I64, 
             binop(Iop_Or64, 
                   assignNew('V', mce, Ity_I64, unop(Iop_Not64, data)), 
                   vbits) );
}

static IRAtom* mkImproveORV128 ( MCEnv* mce, IRAtom* data, IRAtom* vbits )
{
   tl_assert(isOriginalAtom(mce, data));
   tl_assert(isShadowAtom(mce, vbits));
   tl_assert(sameKindedAtoms(data, vbits));
   return assignNew(
             'V', mce, Ity_V128, 
             binop(Iop_OrV128, 
                   assignNew('V', mce, Ity_V128, unop(Iop_NotV128, data)), 
                   vbits) );
}

static IRAtom* mkImproveORV256 ( MCEnv* mce, IRAtom* data, IRAtom* vbits )
{
   tl_assert(isOriginalAtom(mce, data));
   tl_assert(isShadowAtom(mce, vbits));
   tl_assert(sameKindedAtoms(data, vbits));
   return assignNew(
             'V', mce, Ity_V256, 
             binop(Iop_OrV256, 
                   assignNew('V', mce, Ity_V256, unop(Iop_NotV256, data)), 
                   vbits) );
}



static IRAtom* mkPCastTo( MCEnv* mce, IRType dst_ty, IRAtom* vbits ) 
{
   IRType  src_ty;
   IRAtom* tmp1;

   
   tl_assert(isShadowAtom(mce,vbits));
   src_ty = typeOfIRExpr(mce->sb->tyenv, vbits);

   
   if (src_ty == Ity_I32 && dst_ty == Ity_I32)
      return assignNew('V', mce, Ity_I32, unop(Iop_CmpwNEZ32, vbits));

   if (src_ty == Ity_I64 && dst_ty == Ity_I64)
      return assignNew('V', mce, Ity_I64, unop(Iop_CmpwNEZ64, vbits));

   if (src_ty == Ity_I32 && dst_ty == Ity_I64) {
      
      IRAtom* tmp = assignNew('V', mce, Ity_I32, unop(Iop_CmpwNEZ32, vbits));
      return assignNew('V', mce, Ity_I64, binop(Iop_32HLto64, tmp, tmp));
   }

   if (src_ty == Ity_I32 && dst_ty == Ity_V128) {
      
      IRAtom* tmp = assignNew('V', mce, Ity_I32, unop(Iop_CmpwNEZ32, vbits));
      tmp = assignNew('V', mce, Ity_I64, binop(Iop_32HLto64, tmp, tmp));
      return assignNew('V', mce, Ity_V128, binop(Iop_64HLtoV128, tmp, tmp));
   }

   if (src_ty == Ity_I32 && dst_ty == Ity_V256) {
      
      IRAtom* tmp = assignNew('V', mce, Ity_I32, unop(Iop_CmpwNEZ32, vbits));
      tmp = assignNew('V', mce, Ity_I64, binop(Iop_32HLto64, tmp, tmp));
      tmp = assignNew('V', mce, Ity_V128, binop(Iop_64HLtoV128, tmp, tmp));
      return assignNew('V', mce, Ity_V256, binop(Iop_V128HLtoV256, tmp, tmp));
   }

   if (src_ty == Ity_I64 && dst_ty == Ity_I32) {
      IRAtom* tmp = assignNew('V', mce, Ity_I64, unop(Iop_CmpwNEZ64, vbits));
      return assignNew('V', mce, Ity_I32, unop(Iop_64to32, tmp));
   }

   if (src_ty == Ity_V128 && dst_ty == Ity_I64) {
      
      IRAtom* hi64hi64
         = assignNew('V', mce, Ity_V128,
                     binop(Iop_InterleaveHI64x2, vbits, vbits));
      
      
      
      IRAtom* lohi64 
         = mkUifUV128(mce, hi64hi64, vbits);
      
      IRAtom* lo64
         = assignNew('V', mce, Ity_I64, unop(Iop_V128to64, lohi64));
      
      
      
      IRAtom* res
         = assignNew('V', mce, Ity_I64, unop(Iop_CmpwNEZ64, lo64));
      return res;
   }

   
   
   tmp1   = NULL;
   switch (src_ty) {
      case Ity_I1:
         tmp1 = vbits;
         break;
      case Ity_I8: 
         tmp1 = assignNew('V', mce, Ity_I1, unop(Iop_CmpNEZ8, vbits));
         break;
      case Ity_I16: 
         tmp1 = assignNew('V', mce, Ity_I1, unop(Iop_CmpNEZ16, vbits));
         break;
      case Ity_I32: 
         tmp1 = assignNew('V', mce, Ity_I1, unop(Iop_CmpNEZ32, vbits));
         break;
      case Ity_I64: 
         tmp1 = assignNew('V', mce, Ity_I1, unop(Iop_CmpNEZ64, vbits));
         break;
      case Ity_I128: {
         IRAtom* tmp2 = assignNew('V', mce, Ity_I64, unop(Iop_128HIto64, vbits));
         IRAtom* tmp3 = assignNew('V', mce, Ity_I64, unop(Iop_128to64, vbits));
         IRAtom* tmp4 = assignNew('V', mce, Ity_I64, binop(Iop_Or64, tmp2, tmp3));
         tmp1         = assignNew('V', mce, Ity_I1, 
                                       unop(Iop_CmpNEZ64, tmp4));
         break;
      }
      default:
         ppIRType(src_ty);
         VG_(tool_panic)("mkPCastTo(1)");
   }
   tl_assert(tmp1);
   
   switch (dst_ty) {
      case Ity_I1:
         return tmp1;
      case Ity_I8: 
         return assignNew('V', mce, Ity_I8, unop(Iop_1Sto8, tmp1));
      case Ity_I16: 
         return assignNew('V', mce, Ity_I16, unop(Iop_1Sto16, tmp1));
      case Ity_I32: 
         return assignNew('V', mce, Ity_I32, unop(Iop_1Sto32, tmp1));
      case Ity_I64: 
         return assignNew('V', mce, Ity_I64, unop(Iop_1Sto64, tmp1));
      case Ity_V128:
         tmp1 = assignNew('V', mce, Ity_I64,  unop(Iop_1Sto64, tmp1));
         tmp1 = assignNew('V', mce, Ity_V128, binop(Iop_64HLtoV128, tmp1, tmp1));
         return tmp1;
      case Ity_I128:
         tmp1 = assignNew('V', mce, Ity_I64,  unop(Iop_1Sto64, tmp1));
         tmp1 = assignNew('V', mce, Ity_I128, binop(Iop_64HLto128, tmp1, tmp1));
         return tmp1;
      case Ity_V256:
         tmp1 = assignNew('V', mce, Ity_I64,  unop(Iop_1Sto64, tmp1));
         tmp1 = assignNew('V', mce, Ity_V128, binop(Iop_64HLtoV128,
                                                    tmp1, tmp1));
         tmp1 = assignNew('V', mce, Ity_V256, binop(Iop_V128HLtoV256,
                                                    tmp1, tmp1));
         return tmp1;
      default: 
         ppIRType(dst_ty);
         VG_(tool_panic)("mkPCastTo(2)");
   }
}

static IRAtom* mkPCastXXtoXXlsb ( MCEnv* mce, IRAtom* varg, IRType ty )
{
   if (ty == Ity_V128) {
      
      IRAtom* varg128 = varg;
      
      IRAtom* pcdTo64 = mkPCastTo(mce, Ity_I64, varg128);
      
      
      IRAtom* d63pc 
         = assignNew('V', mce, Ity_I64, binop(Iop_And64, pcdTo64, mkU64(1)));
      
      IRAtom* d64
         = definedOfType(Ity_I64);
      
      IRAtom* res
         = assignNew('V', mce, Ity_V128, binop(Iop_64HLtoV128, d64, d63pc));
      return res;
   }
   if (ty == Ity_I64) {
      
      
      IRAtom* pcd = mkPCastTo(mce, Ity_I64, varg);
      
      IRAtom* res 
         = assignNew('V', mce, Ity_I64, binop(Iop_And64, pcd, mkU64(1)));   
      return res;
   }
   
   tl_assert(0);
}

static IRAtom* expensiveCmpEQorNE ( MCEnv*  mce,
                                    IRType  ty,
                                    IRAtom* vxx, IRAtom* vyy, 
                                    IRAtom* xx,  IRAtom* yy )
{
   IRAtom *naive, *vec, *improvement_term;
   IRAtom *improved, *final_cast, *top;
   IROp   opDIFD, opUIFU, opXOR, opNOT, opCMP, opOR;

   tl_assert(isShadowAtom(mce,vxx));
   tl_assert(isShadowAtom(mce,vyy));
   tl_assert(isOriginalAtom(mce,xx));
   tl_assert(isOriginalAtom(mce,yy));
   tl_assert(sameKindedAtoms(vxx,xx));
   tl_assert(sameKindedAtoms(vyy,yy));
 
   switch (ty) {
      case Ity_I16:
         opOR   = Iop_Or16;
         opDIFD = Iop_And16;
         opUIFU = Iop_Or16;
         opNOT  = Iop_Not16;
         opXOR  = Iop_Xor16;
         opCMP  = Iop_CmpEQ16;
         top    = mkU16(0xFFFF);
         break;
      case Ity_I32:
         opOR   = Iop_Or32;
         opDIFD = Iop_And32;
         opUIFU = Iop_Or32;
         opNOT  = Iop_Not32;
         opXOR  = Iop_Xor32;
         opCMP  = Iop_CmpEQ32;
         top    = mkU32(0xFFFFFFFF);
         break;
      case Ity_I64:
         opOR   = Iop_Or64;
         opDIFD = Iop_And64;
         opUIFU = Iop_Or64;
         opNOT  = Iop_Not64;
         opXOR  = Iop_Xor64;
         opCMP  = Iop_CmpEQ64;
         top    = mkU64(0xFFFFFFFFFFFFFFFFULL);
         break;
      default:
         VG_(tool_panic)("expensiveCmpEQorNE");
   }

   naive 
      = mkPCastTo(mce,ty,
                  assignNew('V', mce, ty, binop(opUIFU, vxx, vyy)));

   vec 
      = assignNew(
           'V', mce,ty, 
           binop( opOR,
                  assignNew('V', mce,ty, binop(opOR, vxx, vyy)),
                  assignNew(
                     'V', mce,ty, 
                     unop( opNOT,
                           assignNew('V', mce,ty, binop(opXOR, xx, yy))))));

   improvement_term
      = mkPCastTo( mce,ty,
                   assignNew('V', mce,Ity_I1, binop(opCMP, vec, top)));

   improved
      = assignNew( 'V', mce,ty, binop(opDIFD, naive, improvement_term) );

   final_cast
      = mkPCastTo( mce, Ity_I1, improved );

   return final_cast;
}



static Bool isZeroU32 ( IRAtom* e )
{
   return
      toBool( e->tag == Iex_Const
              && e->Iex.Const.con->tag == Ico_U32
              && e->Iex.Const.con->Ico.U32 == 0 );
}

static Bool isZeroU64 ( IRAtom* e )
{
   return
      toBool( e->tag == Iex_Const
              && e->Iex.Const.con->tag == Ico_U64
              && e->Iex.Const.con->Ico.U64 == 0 );
}

static IRAtom* doCmpORD ( MCEnv*  mce,
                          IROp    cmp_op,
                          IRAtom* xxhash, IRAtom* yyhash, 
                          IRAtom* xx,     IRAtom* yy )
{
   Bool   m64    = cmp_op == Iop_CmpORD64S || cmp_op == Iop_CmpORD64U;
   Bool   syned  = cmp_op == Iop_CmpORD64S || cmp_op == Iop_CmpORD32S;
   IROp   opOR   = m64 ? Iop_Or64  : Iop_Or32;
   IROp   opAND  = m64 ? Iop_And64 : Iop_And32;
   IROp   opSHL  = m64 ? Iop_Shl64 : Iop_Shl32;
   IROp   opSHR  = m64 ? Iop_Shr64 : Iop_Shr32;
   IRType ty     = m64 ? Ity_I64   : Ity_I32;
   Int    width  = m64 ? 64        : 32;

   Bool (*isZero)(IRAtom*) = m64 ? isZeroU64 : isZeroU32;

   IRAtom* threeLeft1 = NULL;
   IRAtom* sevenLeft1 = NULL;

   tl_assert(isShadowAtom(mce,xxhash));
   tl_assert(isShadowAtom(mce,yyhash));
   tl_assert(isOriginalAtom(mce,xx));
   tl_assert(isOriginalAtom(mce,yy));
   tl_assert(sameKindedAtoms(xxhash,xx));
   tl_assert(sameKindedAtoms(yyhash,yy));
   tl_assert(cmp_op == Iop_CmpORD32S || cmp_op == Iop_CmpORD32U
             || cmp_op == Iop_CmpORD64S || cmp_op == Iop_CmpORD64U);

   if (0) {
      ppIROp(cmp_op); VG_(printf)(" "); 
      ppIRExpr(xx); VG_(printf)(" "); ppIRExpr( yy ); VG_(printf)("\n");
   }

   if (syned && isZero(yy)) {
      
      
      tl_assert(isZero(yyhash));
      threeLeft1 = m64 ? mkU64(3<<1) : mkU32(3<<1);
      return
         binop(
            opOR,
            assignNew(
               'V', mce,ty,
               binop(
                  opAND,
                  mkPCastTo(mce,ty, xxhash), 
                  threeLeft1
               )),
            assignNew(
               'V', mce,ty,
               binop(
                  opSHL,
                  assignNew(
                     'V', mce,ty,
                     binop(opSHR, xxhash, mkU8(width-1))),
                  mkU8(3)
               ))
	 );
   } else {
      
      sevenLeft1 = m64 ? mkU64(7<<1) : mkU32(7<<1);
      return 
         binop( 
            opAND, 
            mkPCastTo( mce,ty,
                       mkUifU(mce,ty, xxhash,yyhash)),
            sevenLeft1
         );
   }
}



static IRAtom* schemeE ( MCEnv* mce, IRExpr* e ); 



static void setHelperAnns ( MCEnv* mce, IRDirty* di ) {
   di->nFxState = 2;
   di->fxState[0].fx        = Ifx_Read;
   di->fxState[0].offset    = mce->layout->offset_SP;
   di->fxState[0].size      = mce->layout->sizeof_SP;
   di->fxState[0].nRepeats  = 0;
   di->fxState[0].repeatLen = 0;
   di->fxState[1].fx        = Ifx_Read;
   di->fxState[1].offset    = mce->layout->offset_IP;
   di->fxState[1].size      = mce->layout->sizeof_IP;
   di->fxState[1].nRepeats  = 0;
   di->fxState[1].repeatLen = 0;
}


static void complainIfUndefined ( MCEnv* mce, IRAtom* atom, IRExpr *guard )
{
   IRAtom*  vatom;
   IRType   ty;
   Int      sz;
   IRDirty* di;
   IRAtom*  cond;
   IRAtom*  origin;
   void*    fn;
   const HChar* nm;
   IRExpr** args;
   Int      nargs;

   
   if (MC_(clo_mc_level) == 1)
      return;

   if (guard)
      tl_assert(isOriginalAtom(mce, guard));

   tl_assert(isOriginalAtom(mce, atom));
   vatom = expr2vbits( mce, atom );
   tl_assert(isShadowAtom(mce, vatom));
   tl_assert(sameKindedAtoms(atom, vatom));

   ty = typeOfIRExpr(mce->sb->tyenv, vatom);

   
   sz = ty==Ity_I1 ? 0 : sizeofIRType(ty);

   cond = mkPCastTo( mce, Ity_I1, vatom );
   

   if (MC_(clo_mc_level) == 3) {
      origin = schemeE( mce, atom );
      if (mce->hWordTy == Ity_I64) {
         origin = assignNew( 'B', mce, Ity_I64, unop(Iop_32Uto64, origin) );
      }
   } else {
      origin = NULL;
   }

   fn    = NULL;
   nm    = NULL;
   args  = NULL;
   nargs = -1;

   switch (sz) {
      case 0:
         if (origin) {
            fn    = &MC_(helperc_value_check0_fail_w_o);
            nm    = "MC_(helperc_value_check0_fail_w_o)";
            args  = mkIRExprVec_1(origin);
            nargs = 1;
         } else {
            fn    = &MC_(helperc_value_check0_fail_no_o);
            nm    = "MC_(helperc_value_check0_fail_no_o)";
            args  = mkIRExprVec_0();
            nargs = 0;
         }
         break;
      case 1:
         if (origin) {
            fn    = &MC_(helperc_value_check1_fail_w_o);
            nm    = "MC_(helperc_value_check1_fail_w_o)";
            args  = mkIRExprVec_1(origin);
            nargs = 1;
         } else {
            fn    = &MC_(helperc_value_check1_fail_no_o);
            nm    = "MC_(helperc_value_check1_fail_no_o)";
            args  = mkIRExprVec_0();
            nargs = 0;
         }
         break;
      case 4:
         if (origin) {
            fn    = &MC_(helperc_value_check4_fail_w_o);
            nm    = "MC_(helperc_value_check4_fail_w_o)";
            args  = mkIRExprVec_1(origin);
            nargs = 1;
         } else {
            fn    = &MC_(helperc_value_check4_fail_no_o);
            nm    = "MC_(helperc_value_check4_fail_no_o)";
            args  = mkIRExprVec_0();
            nargs = 0;
         }
         break;
      case 8:
         if (origin) {
            fn    = &MC_(helperc_value_check8_fail_w_o);
            nm    = "MC_(helperc_value_check8_fail_w_o)";
            args  = mkIRExprVec_1(origin);
            nargs = 1;
         } else {
            fn    = &MC_(helperc_value_check8_fail_no_o);
            nm    = "MC_(helperc_value_check8_fail_no_o)";
            args  = mkIRExprVec_0();
            nargs = 0;
         }
         break;
      case 2:
      case 16:
         if (origin) {
            fn    = &MC_(helperc_value_checkN_fail_w_o);
            nm    = "MC_(helperc_value_checkN_fail_w_o)";
            args  = mkIRExprVec_2( mkIRExpr_HWord( sz ), origin);
            nargs = 2;
         } else {
            fn    = &MC_(helperc_value_checkN_fail_no_o);
            nm    = "MC_(helperc_value_checkN_fail_no_o)";
            args  = mkIRExprVec_1( mkIRExpr_HWord( sz ) );
            nargs = 1;
         }
         break;
      default:
         VG_(tool_panic)("unexpected szB");
   }

   tl_assert(fn);
   tl_assert(nm);
   tl_assert(args);
   tl_assert(nargs >= 0 && nargs <= 2);
   tl_assert( (MC_(clo_mc_level) == 3 && origin != NULL)
              || (MC_(clo_mc_level) == 2 && origin == NULL) );

   di = unsafeIRDirty_0_N( nargs, nm, 
                           VG_(fnptr_to_fnentry)( fn ), args );
   di->guard = cond; 

   if (guard) {
      IRAtom *g1 = assignNew('V', mce, Ity_I32, unop(Iop_1Uto32, di->guard));
      IRAtom *g2 = assignNew('V', mce, Ity_I32, unop(Iop_1Uto32, guard));
      IRAtom *e  = assignNew('V', mce, Ity_I32, binop(Iop_And32, g1, g2));
      di->guard  = assignNew('V', mce, Ity_I1,  unop(Iop_32to1, e));
   }

   setHelperAnns( mce, di );
   stmt( 'V', mce, IRStmt_Dirty(di));

   tl_assert(isIRAtom(vatom));
   
   if (vatom->tag == Iex_RdTmp) {
      tl_assert(atom->tag == Iex_RdTmp);
      if (guard == NULL) {
         
         newShadowTmpV(mce, atom->Iex.RdTmp.tmp);
         assign('V', mce, findShadowTmpV(mce, atom->Iex.RdTmp.tmp), 
                          definedOfType(ty));
      } else {
         
         
         
         IRTemp old_tmpV = findShadowTmpV(mce, atom->Iex.RdTmp.tmp);
         newShadowTmpV(mce, atom->Iex.RdTmp.tmp);
         IRAtom* new_tmpV
            = assignNew('V', mce, shadowTypeV(ty),
                        IRExpr_ITE(guard, definedOfType(ty),
                                          mkexpr(old_tmpV)));
         assign('V', mce, findShadowTmpV(mce, atom->Iex.RdTmp.tmp), new_tmpV);
      }
   }
}



static Bool isAlwaysDefd ( MCEnv* mce, Int offset, Int size )
{
   Int minoffD, maxoffD, i;
   Int minoff = offset;
   Int maxoff = minoff + size - 1;
   tl_assert((minoff & ~0xFFFF) == 0);
   tl_assert((maxoff & ~0xFFFF) == 0);

   for (i = 0; i < mce->layout->n_alwaysDefd; i++) {
      minoffD = mce->layout->alwaysDefd[i].offset;
      maxoffD = minoffD + mce->layout->alwaysDefd[i].size - 1;
      tl_assert((minoffD & ~0xFFFF) == 0);
      tl_assert((maxoffD & ~0xFFFF) == 0);

      if (maxoff < minoffD || maxoffD < minoff)
         continue; 
      if (minoff >= minoffD && maxoff <= maxoffD)
         return True; 

      VG_(tool_panic)("memcheck:isAlwaysDefd:partial overlap");
   }
   return False; 
}


static
void do_shadow_PUT ( MCEnv* mce,  Int offset, 
                     IRAtom* atom, IRAtom* vatom, IRExpr *guard )
{
   IRType ty;

   
   
   
   if (MC_(clo_mc_level) == 1)
      return;
   
   if (atom) {
      tl_assert(!vatom);
      tl_assert(isOriginalAtom(mce, atom));
      vatom = expr2vbits( mce, atom );
   } else {
      tl_assert(vatom);
      tl_assert(isShadowAtom(mce, vatom));
   }

   ty = typeOfIRExpr(mce->sb->tyenv, vatom);
   tl_assert(ty != Ity_I1);
   tl_assert(ty != Ity_I128);
   if (isAlwaysDefd(mce, offset, sizeofIRType(ty))) {
      
      
      
   } else {
      
      if (guard) {
         IRAtom *cond, *iffalse;

         cond    = assignNew('V', mce, Ity_I1, guard);
         iffalse = assignNew('V', mce, ty,
                             IRExpr_Get(offset + mce->layout->total_sizeB, ty));
         vatom   = assignNew('V', mce, ty, IRExpr_ITE(cond, vatom, iffalse));
      }
      stmt( 'V', mce, IRStmt_Put( offset + mce->layout->total_sizeB, vatom ));
   }
}


static
void do_shadow_PUTI ( MCEnv* mce, IRPutI *puti)
{
   IRAtom* vatom;
   IRType  ty, tyS;
   Int     arrSize;;
   IRRegArray* descr = puti->descr;
   IRAtom*     ix    = puti->ix;
   Int         bias  = puti->bias;
   IRAtom*     atom  = puti->data;

   
   
   
   if (MC_(clo_mc_level) == 1)
      return;
   
   tl_assert(isOriginalAtom(mce,atom));
   vatom = expr2vbits( mce, atom );
   tl_assert(sameKindedAtoms(atom, vatom));
   ty   = descr->elemTy;
   tyS  = shadowTypeV(ty);
   arrSize = descr->nElems * sizeofIRType(ty);
   tl_assert(ty != Ity_I1);
   tl_assert(isOriginalAtom(mce,ix));
   complainIfUndefined(mce, ix, NULL);
   if (isAlwaysDefd(mce, descr->base, arrSize)) {
      
      
      
   } else {
      IRRegArray* new_descr 
         = mkIRRegArray( descr->base + mce->layout->total_sizeB, 
                         tyS, descr->nElems);
      stmt( 'V', mce, IRStmt_PutI( mkIRPutI(new_descr, ix, bias, vatom) ));
   }
}


static 
IRExpr* shadow_GET ( MCEnv* mce, Int offset, IRType ty )
{
   IRType tyS = shadowTypeV(ty);
   tl_assert(ty != Ity_I1);
   tl_assert(ty != Ity_I128);
   if (isAlwaysDefd(mce, offset, sizeofIRType(ty))) {
      
      return definedOfType(tyS);
   } else {
      
      return IRExpr_Get( offset + mce->layout->total_sizeB, tyS );
   }
}


static
IRExpr* shadow_GETI ( MCEnv* mce, 
                      IRRegArray* descr, IRAtom* ix, Int bias )
{
   IRType ty   = descr->elemTy;
   IRType tyS  = shadowTypeV(ty);
   Int arrSize = descr->nElems * sizeofIRType(ty);
   tl_assert(ty != Ity_I1);
   tl_assert(isOriginalAtom(mce,ix));
   complainIfUndefined(mce, ix, NULL);
   if (isAlwaysDefd(mce, descr->base, arrSize)) {
      
      return definedOfType(tyS);
   } else {
      IRRegArray* new_descr 
         = mkIRRegArray( descr->base + mce->layout->total_sizeB, 
                         tyS, descr->nElems);
      return IRExpr_GetI( new_descr, ix, bias );
   }
}



static
IRAtom* mkLazy2 ( MCEnv* mce, IRType finalVty, IRAtom* va1, IRAtom* va2 )
{
   IRAtom* at;
   IRType t1 = typeOfIRExpr(mce->sb->tyenv, va1);
   IRType t2 = typeOfIRExpr(mce->sb->tyenv, va2);
   tl_assert(isShadowAtom(mce,va1));
   tl_assert(isShadowAtom(mce,va2));


   
   if (t1 == Ity_I64 && t2 == Ity_I64 && finalVty == Ity_I64) {
      if (0) VG_(printf)("mkLazy2: I64 x I64 -> I64\n");
      at = mkUifU(mce, Ity_I64, va1, va2);
      at = mkPCastTo(mce, Ity_I64, at);
      return at;
   }

   
   if (t1 == Ity_I64 && t2 == Ity_I64 && finalVty == Ity_I32) {
      if (0) VG_(printf)("mkLazy2: I64 x I64 -> I32\n");
      at = mkUifU(mce, Ity_I64, va1, va2);
      at = mkPCastTo(mce, Ity_I32, at);
      return at;
   }

   if (0) {
      VG_(printf)("mkLazy2 ");
      ppIRType(t1);
      VG_(printf)("_");
      ppIRType(t2);
      VG_(printf)("_");
      ppIRType(finalVty);
      VG_(printf)("\n");
   }

   
   at = mkPCastTo(mce, Ity_I32, va1);
   at = mkUifU(mce, Ity_I32, at, mkPCastTo(mce, Ity_I32, va2));
   at = mkPCastTo(mce, finalVty, at);
   return at;
}


static
IRAtom* mkLazy3 ( MCEnv* mce, IRType finalVty, 
                  IRAtom* va1, IRAtom* va2, IRAtom* va3 )
{
   IRAtom* at;
   IRType t1 = typeOfIRExpr(mce->sb->tyenv, va1);
   IRType t2 = typeOfIRExpr(mce->sb->tyenv, va2);
   IRType t3 = typeOfIRExpr(mce->sb->tyenv, va3);
   tl_assert(isShadowAtom(mce,va1));
   tl_assert(isShadowAtom(mce,va2));
   tl_assert(isShadowAtom(mce,va3));


   
   
   if (t1 == Ity_I32 && t2 == Ity_I64 && t3 == Ity_I64 
       && finalVty == Ity_I64) {
      if (0) VG_(printf)("mkLazy3: I32 x I64 x I64 -> I64\n");
      at = mkPCastTo(mce, Ity_I64, va1);
      
      at = mkUifU(mce, Ity_I64, at, va2);
      at = mkUifU(mce, Ity_I64, at, va3);
      
      at = mkPCastTo(mce, Ity_I64, at);
      return at;
   }

   
   if (t1 == Ity_I32 && t2 == Ity_I8 && t3 == Ity_I64
       && finalVty == Ity_I64) {
      if (0) VG_(printf)("mkLazy3: I32 x I8 x I64 -> I64\n");
      IRAtom* at1 = mkPCastTo(mce, Ity_I64, va1);
      IRAtom* at2 = mkPCastTo(mce, Ity_I64, va2);
      at = mkUifU(mce, Ity_I64, at1, at2);  
      at = mkUifU(mce, Ity_I64, at, va3);
      
      at = mkPCastTo(mce, Ity_I64, at);
      return at;
   }

   
   if (t1 == Ity_I32 && t2 == Ity_I64 && t3 == Ity_I64 
       && finalVty == Ity_I32) {
      if (0) VG_(printf)("mkLazy3: I32 x I64 x I64 -> I32\n");
      at = mkPCastTo(mce, Ity_I64, va1);
      at = mkUifU(mce, Ity_I64, at, va2);
      at = mkUifU(mce, Ity_I64, at, va3);
      at = mkPCastTo(mce, Ity_I32, at);
      return at;
   }

   
   
   if (t1 == Ity_I32 && t2 == Ity_I32 && t3 == Ity_I32 
       && finalVty == Ity_I32) {
      if (0) VG_(printf)("mkLazy3: I32 x I32 x I32 -> I32\n");
      at = va1;
      at = mkUifU(mce, Ity_I32, at, va2);
      at = mkUifU(mce, Ity_I32, at, va3);
      at = mkPCastTo(mce, Ity_I32, at);
      return at;
   }

   
   
   if (t1 == Ity_I32 && t2 == Ity_I128 && t3 == Ity_I128
       && finalVty == Ity_I128) {
      if (0) VG_(printf)("mkLazy3: I32 x I128 x I128 -> I128\n");
      at = mkPCastTo(mce, Ity_I128, va1);
      
      at = mkUifU(mce, Ity_I128, at, va2);
      at = mkUifU(mce, Ity_I128, at, va3);
      
      at = mkPCastTo(mce, Ity_I128, at);
      return at;
   }

   
   
   if (t1 == Ity_I32 && t2 == Ity_I8 && t3 == Ity_I128
       && finalVty == Ity_I128) {
      if (0) VG_(printf)("mkLazy3: I32 x I8 x I128 -> I128\n");
      IRAtom* at1 = mkPCastTo(mce, Ity_I64, va1);
      IRAtom* at2 = mkPCastTo(mce, Ity_I64, va2);
      IRAtom* at3 = mkPCastTo(mce, Ity_I64, va3);
      
      at = mkUifU(mce, Ity_I64, at1, at2);  
      at = mkUifU(mce, Ity_I64, at, at3);   
      
      at = mkPCastTo(mce, Ity_I128, at);
      return at;
   }
   if (1) {
      VG_(printf)("mkLazy3: ");
      ppIRType(t1);
      VG_(printf)(" x ");
      ppIRType(t2);
      VG_(printf)(" x ");
      ppIRType(t3);
      VG_(printf)(" -> ");
      ppIRType(finalVty);
      VG_(printf)("\n");
   }

   tl_assert(0);
   
}


static
IRAtom* mkLazy4 ( MCEnv* mce, IRType finalVty, 
                  IRAtom* va1, IRAtom* va2, IRAtom* va3, IRAtom* va4 )
{
   IRAtom* at;
   IRType t1 = typeOfIRExpr(mce->sb->tyenv, va1);
   IRType t2 = typeOfIRExpr(mce->sb->tyenv, va2);
   IRType t3 = typeOfIRExpr(mce->sb->tyenv, va3);
   IRType t4 = typeOfIRExpr(mce->sb->tyenv, va4);
   tl_assert(isShadowAtom(mce,va1));
   tl_assert(isShadowAtom(mce,va2));
   tl_assert(isShadowAtom(mce,va3));
   tl_assert(isShadowAtom(mce,va4));


   
   
   if (t1 == Ity_I32 && t2 == Ity_I64 && t3 == Ity_I64 && t4 == Ity_I64
       && finalVty == Ity_I64) {
      if (0) VG_(printf)("mkLazy4: I32 x I64 x I64 x I64 -> I64\n");
      at = mkPCastTo(mce, Ity_I64, va1);
      
      at = mkUifU(mce, Ity_I64, at, va2);
      at = mkUifU(mce, Ity_I64, at, va3);
      at = mkUifU(mce, Ity_I64, at, va4);
      
      at = mkPCastTo(mce, Ity_I64, at);
      return at;
   }
   
   
   if (t1 == Ity_I32 && t2 == Ity_I32 && t3 == Ity_I32 && t4 == Ity_I32
       && finalVty == Ity_I32) {
      if (0) VG_(printf)("mkLazy4: I32 x I32 x I32 x I32 -> I32\n");
      at = va1;
      
      at = mkUifU(mce, Ity_I32, at, va2);
      at = mkUifU(mce, Ity_I32, at, va3);
      at = mkUifU(mce, Ity_I32, at, va4);
      at = mkPCastTo(mce, Ity_I32, at);
      return at;
   }

   if (1) {
      VG_(printf)("mkLazy4: ");
      ppIRType(t1);
      VG_(printf)(" x ");
      ppIRType(t2);
      VG_(printf)(" x ");
      ppIRType(t3);
      VG_(printf)(" x ");
      ppIRType(t4);
      VG_(printf)(" -> ");
      ppIRType(finalVty);
      VG_(printf)("\n");
   }

   tl_assert(0);
}


static
IRAtom* mkLazyN ( MCEnv* mce, 
                  IRAtom** exprvec, IRType finalVtype, IRCallee* cee )
{
   Int     i;
   IRAtom* here;
   IRAtom* curr;
   IRType  mergeTy;
   Bool    mergeTy64 = True;

   for (i = 0; exprvec[i]; i++) {
      tl_assert(i < 32);
      tl_assert(isOriginalAtom(mce, exprvec[i]));
      if (cee->mcx_mask & (1<<i))
         continue;
      if (typeOfIRExpr(mce->sb->tyenv, exprvec[i]) != Ity_I64)
         mergeTy64 = False;
   }

   mergeTy = mergeTy64  ? Ity_I64  : Ity_I32;
   curr    = definedOfType(mergeTy);

   for (i = 0; exprvec[i]; i++) {
      tl_assert(i < 32);
      tl_assert(isOriginalAtom(mce, exprvec[i]));
      if (cee->mcx_mask & (1<<i)) {
         if (0) VG_(printf)("excluding %s(%d)\n", cee->name, i);
      } else {
         here = mkPCastTo( mce, mergeTy, expr2vbits(mce, exprvec[i]) );
         curr = mergeTy64 
                   ? mkUifU64(mce, here, curr)
                   : mkUifU32(mce, here, curr);
      }
   }
   return mkPCastTo(mce, finalVtype, curr );
}



static
IRAtom* expensiveAddSub ( MCEnv*  mce,
                          Bool    add,
                          IRType  ty,
                          IRAtom* qaa, IRAtom* qbb, 
                          IRAtom* aa,  IRAtom* bb )
{
   IRAtom *a_min, *b_min, *a_max, *b_max;
   IROp   opAND, opOR, opXOR, opNOT, opADD, opSUB;

   tl_assert(isShadowAtom(mce,qaa));
   tl_assert(isShadowAtom(mce,qbb));
   tl_assert(isOriginalAtom(mce,aa));
   tl_assert(isOriginalAtom(mce,bb));
   tl_assert(sameKindedAtoms(qaa,aa));
   tl_assert(sameKindedAtoms(qbb,bb));

   switch (ty) {
      case Ity_I32:
         opAND = Iop_And32;
         opOR  = Iop_Or32;
         opXOR = Iop_Xor32;
         opNOT = Iop_Not32;
         opADD = Iop_Add32;
         opSUB = Iop_Sub32;
         break;
      case Ity_I64:
         opAND = Iop_And64;
         opOR  = Iop_Or64;
         opXOR = Iop_Xor64;
         opNOT = Iop_Not64;
         opADD = Iop_Add64;
         opSUB = Iop_Sub64;
         break;
      default:
         VG_(tool_panic)("expensiveAddSub");
   }

   
   a_min = assignNew('V', mce,ty, 
                     binop(opAND, aa,
                                  assignNew('V', mce,ty, unop(opNOT, qaa))));

   
   b_min = assignNew('V', mce,ty, 
                     binop(opAND, bb,
                                  assignNew('V', mce,ty, unop(opNOT, qbb))));

   
   a_max = assignNew('V', mce,ty, binop(opOR, aa, qaa));

   
   b_max = assignNew('V', mce,ty, binop(opOR, bb, qbb));

   if (add) {
      
      return
      assignNew('V', mce,ty,
         binop( opOR,
                assignNew('V', mce,ty, binop(opOR, qaa, qbb)),
                assignNew('V', mce,ty, 
                   binop( opXOR, 
                          assignNew('V', mce,ty, binop(opADD, a_min, b_min)),
                          assignNew('V', mce,ty, binop(opADD, a_max, b_max))
                   )
                )
         )
      );
   } else {
      
      return
      assignNew('V', mce,ty,
         binop( opOR,
                assignNew('V', mce,ty, binop(opOR, qaa, qbb)),
                assignNew('V', mce,ty, 
                   binop( opXOR, 
                          assignNew('V', mce,ty, binop(opSUB, a_min, b_max)),
                          assignNew('V', mce,ty, binop(opSUB, a_max, b_min))
                   )
                )
         )
      );
   }

}


static
IRAtom* expensiveCountTrailingZeroes ( MCEnv* mce, IROp czop,
                                       IRAtom* atom, IRAtom* vatom )
{
   IRType ty;
   IROp xorOp, subOp, andOp;
   IRExpr *one;
   IRAtom *improver, *improved;
   tl_assert(isShadowAtom(mce,vatom));
   tl_assert(isOriginalAtom(mce,atom));
   tl_assert(sameKindedAtoms(atom,vatom));

   switch (czop) {
      case Iop_Ctz32:
         ty = Ity_I32;
         xorOp = Iop_Xor32;
         subOp = Iop_Sub32;
         andOp = Iop_And32;
         one = mkU32(1);
         break;
      case Iop_Ctz64:
         ty = Ity_I64;
         xorOp = Iop_Xor64;
         subOp = Iop_Sub64;
         andOp = Iop_And64;
         one = mkU64(1);
         break;
      default:
         ppIROp(czop);
         VG_(tool_panic)("memcheck:expensiveCountTrailingZeroes");
   }

   
   
   
   
   improver = assignNew('V', mce,ty,
                        binop(xorOp,
                              atom,
                              assignNew('V', mce, ty,
                                        binop(subOp, atom, one))));

   
   
   
   
   improved = assignNew('V', mce, ty,
                        binop(andOp, vatom, improver));

   
   return mkPCastTo(mce, ty, improved);
}



static IRAtom* scalarShift ( MCEnv*  mce,
                             IRType  ty,
                             IROp    original_op,
                             IRAtom* qaa, IRAtom* qbb, 
                             IRAtom* aa,  IRAtom* bb )
{
   tl_assert(isShadowAtom(mce,qaa));
   tl_assert(isShadowAtom(mce,qbb));
   tl_assert(isOriginalAtom(mce,aa));
   tl_assert(isOriginalAtom(mce,bb));
   tl_assert(sameKindedAtoms(qaa,aa));
   tl_assert(sameKindedAtoms(qbb,bb));
   return 
      assignNew(
         'V', mce, ty,
         mkUifU( mce, ty,
                 assignNew('V', mce, ty, binop(original_op, qaa, bb)),
                 mkPCastTo(mce, ty, qbb)
         )
   );
}




static IRAtom* mkPCast8x16 ( MCEnv* mce, IRAtom* at )
{
   return assignNew('V', mce, Ity_V128, unop(Iop_CmpNEZ8x16, at));
}

static IRAtom* mkPCast16x8 ( MCEnv* mce, IRAtom* at )
{
   return assignNew('V', mce, Ity_V128, unop(Iop_CmpNEZ16x8, at));
}

static IRAtom* mkPCast32x4 ( MCEnv* mce, IRAtom* at )
{
   return assignNew('V', mce, Ity_V128, unop(Iop_CmpNEZ32x4, at));
}

static IRAtom* mkPCast64x2 ( MCEnv* mce, IRAtom* at )
{
   return assignNew('V', mce, Ity_V128, unop(Iop_CmpNEZ64x2, at));
}

static IRAtom* mkPCast64x4 ( MCEnv* mce, IRAtom* at )
{
   return assignNew('V', mce, Ity_V256, unop(Iop_CmpNEZ64x4, at));
}

static IRAtom* mkPCast32x8 ( MCEnv* mce, IRAtom* at )
{
   return assignNew('V', mce, Ity_V256, unop(Iop_CmpNEZ32x8, at));
}

static IRAtom* mkPCast32x2 ( MCEnv* mce, IRAtom* at )
{
   return assignNew('V', mce, Ity_I64, unop(Iop_CmpNEZ32x2, at));
}

static IRAtom* mkPCast16x16 ( MCEnv* mce, IRAtom* at )
{
   return assignNew('V', mce, Ity_V256, unop(Iop_CmpNEZ16x16, at));
}

static IRAtom* mkPCast16x4 ( MCEnv* mce, IRAtom* at )
{
   return assignNew('V', mce, Ity_I64, unop(Iop_CmpNEZ16x4, at));
}

static IRAtom* mkPCast8x32 ( MCEnv* mce, IRAtom* at )
{
   return assignNew('V', mce, Ity_V256, unop(Iop_CmpNEZ8x32, at));
}

static IRAtom* mkPCast8x8 ( MCEnv* mce, IRAtom* at )
{
   return assignNew('V', mce, Ity_I64, unop(Iop_CmpNEZ8x8, at));
}

static IRAtom* mkPCast16x2 ( MCEnv* mce, IRAtom* at )
{
   return assignNew('V', mce, Ity_I32, unop(Iop_CmpNEZ16x2, at));
}

static IRAtom* mkPCast8x4 ( MCEnv* mce, IRAtom* at )
{
   return assignNew('V', mce, Ity_I32, unop(Iop_CmpNEZ8x4, at));
}




static
IRAtom* binary32Fx4 ( MCEnv* mce, IRAtom* vatomX, IRAtom* vatomY )
{
   IRAtom* at;
   tl_assert(isShadowAtom(mce, vatomX));
   tl_assert(isShadowAtom(mce, vatomY));
   at = mkUifUV128(mce, vatomX, vatomY);
   at = assignNew('V', mce, Ity_V128, mkPCast32x4(mce, at));
   return at;
}

static
IRAtom* unary32Fx4 ( MCEnv* mce, IRAtom* vatomX )
{
   IRAtom* at;
   tl_assert(isShadowAtom(mce, vatomX));
   at = assignNew('V', mce, Ity_V128, mkPCast32x4(mce, vatomX));
   return at;
}

static
IRAtom* binary32F0x4 ( MCEnv* mce, IRAtom* vatomX, IRAtom* vatomY )
{
   IRAtom* at;
   tl_assert(isShadowAtom(mce, vatomX));
   tl_assert(isShadowAtom(mce, vatomY));
   at = mkUifUV128(mce, vatomX, vatomY);
   at = assignNew('V', mce, Ity_I32, unop(Iop_V128to32, at));
   at = mkPCastTo(mce, Ity_I32, at);
   at = assignNew('V', mce, Ity_V128, binop(Iop_SetV128lo32, vatomX, at));
   return at;
}

static
IRAtom* unary32F0x4 ( MCEnv* mce, IRAtom* vatomX )
{
   IRAtom* at;
   tl_assert(isShadowAtom(mce, vatomX));
   at = assignNew('V', mce, Ity_I32, unop(Iop_V128to32, vatomX));
   at = mkPCastTo(mce, Ity_I32, at);
   at = assignNew('V', mce, Ity_V128, binop(Iop_SetV128lo32, vatomX, at));
   return at;
}


static
IRAtom* binary64Fx2 ( MCEnv* mce, IRAtom* vatomX, IRAtom* vatomY )
{
   IRAtom* at;
   tl_assert(isShadowAtom(mce, vatomX));
   tl_assert(isShadowAtom(mce, vatomY));
   at = mkUifUV128(mce, vatomX, vatomY);
   at = assignNew('V', mce, Ity_V128, mkPCast64x2(mce, at));
   return at;
}

static
IRAtom* unary64Fx2 ( MCEnv* mce, IRAtom* vatomX )
{
   IRAtom* at;
   tl_assert(isShadowAtom(mce, vatomX));
   at = assignNew('V', mce, Ity_V128, mkPCast64x2(mce, vatomX));
   return at;
}

static
IRAtom* binary64F0x2 ( MCEnv* mce, IRAtom* vatomX, IRAtom* vatomY )
{
   IRAtom* at;
   tl_assert(isShadowAtom(mce, vatomX));
   tl_assert(isShadowAtom(mce, vatomY));
   at = mkUifUV128(mce, vatomX, vatomY);
   at = assignNew('V', mce, Ity_I64, unop(Iop_V128to64, at));
   at = mkPCastTo(mce, Ity_I64, at);
   at = assignNew('V', mce, Ity_V128, binop(Iop_SetV128lo64, vatomX, at));
   return at;
}

static
IRAtom* unary64F0x2 ( MCEnv* mce, IRAtom* vatomX )
{
   IRAtom* at;
   tl_assert(isShadowAtom(mce, vatomX));
   at = assignNew('V', mce, Ity_I64, unop(Iop_V128to64, vatomX));
   at = mkPCastTo(mce, Ity_I64, at);
   at = assignNew('V', mce, Ity_V128, binop(Iop_SetV128lo64, vatomX, at));
   return at;
}


static
IRAtom* binary32Fx2 ( MCEnv* mce, IRAtom* vatomX, IRAtom* vatomY )
{
   IRAtom* at;
   tl_assert(isShadowAtom(mce, vatomX));
   tl_assert(isShadowAtom(mce, vatomY));
   at = mkUifU64(mce, vatomX, vatomY);
   at = assignNew('V', mce, Ity_I64, mkPCast32x2(mce, at));
   return at;
}

static
IRAtom* unary32Fx2 ( MCEnv* mce, IRAtom* vatomX )
{
   IRAtom* at;
   tl_assert(isShadowAtom(mce, vatomX));
   at = assignNew('V', mce, Ity_I64, mkPCast32x2(mce, vatomX));
   return at;
}


static
IRAtom* binary64Fx4 ( MCEnv* mce, IRAtom* vatomX, IRAtom* vatomY )
{
   IRAtom* at;
   tl_assert(isShadowAtom(mce, vatomX));
   tl_assert(isShadowAtom(mce, vatomY));
   at = mkUifUV256(mce, vatomX, vatomY);
   at = assignNew('V', mce, Ity_V256, mkPCast64x4(mce, at));
   return at;
}

static
IRAtom* unary64Fx4 ( MCEnv* mce, IRAtom* vatomX )
{
   IRAtom* at;
   tl_assert(isShadowAtom(mce, vatomX));
   at = assignNew('V', mce, Ity_V256, mkPCast64x4(mce, vatomX));
   return at;
}


static
IRAtom* binary32Fx8 ( MCEnv* mce, IRAtom* vatomX, IRAtom* vatomY )
{
   IRAtom* at;
   tl_assert(isShadowAtom(mce, vatomX));
   tl_assert(isShadowAtom(mce, vatomY));
   at = mkUifUV256(mce, vatomX, vatomY);
   at = assignNew('V', mce, Ity_V256, mkPCast32x8(mce, at));
   return at;
}

static
IRAtom* unary32Fx8 ( MCEnv* mce, IRAtom* vatomX )
{
   IRAtom* at;
   tl_assert(isShadowAtom(mce, vatomX));
   at = assignNew('V', mce, Ity_V256, mkPCast32x8(mce, vatomX));
   return at;
}


static
IRAtom* binary64Fx2_w_rm ( MCEnv* mce, IRAtom* vRM,
                                       IRAtom* vatomX, IRAtom* vatomY )
{
   
   IRAtom* t1 = binary64Fx2(mce, vatomX, vatomY);
   
   IRAtom* t2 = mkPCastTo(mce, Ity_V128, vRM);
   
   t1 = mkUifUV128(mce, t1, t2);
   return t1;
}


static
IRAtom* binary32Fx4_w_rm ( MCEnv* mce, IRAtom* vRM,
                                       IRAtom* vatomX, IRAtom* vatomY )
{
   IRAtom* t1 = binary32Fx4(mce, vatomX, vatomY);
   
   IRAtom* t2 = mkPCastTo(mce, Ity_V128, vRM);
   
   t1 = mkUifUV128(mce, t1, t2);
   return t1;
}


static
IRAtom* binary64Fx4_w_rm ( MCEnv* mce, IRAtom* vRM,
                                       IRAtom* vatomX, IRAtom* vatomY )
{
   IRAtom* t1 = binary64Fx4(mce, vatomX, vatomY);
   
   IRAtom* t2 = mkPCastTo(mce, Ity_V256, vRM);
   
   t1 = mkUifUV256(mce, t1, t2);
   return t1;
}


static
IRAtom* binary32Fx8_w_rm ( MCEnv* mce, IRAtom* vRM,
                                       IRAtom* vatomX, IRAtom* vatomY )
{
   IRAtom* t1 = binary32Fx8(mce, vatomX, vatomY);
   
   IRAtom* t2 = mkPCastTo(mce, Ity_V256, vRM);
   
   t1 = mkUifUV256(mce, t1, t2);
   return t1;
}


static
IRAtom* unary64Fx2_w_rm ( MCEnv* mce, IRAtom* vRM, IRAtom* vatomX )
{
   
   
   IRAtom* t1 = unary64Fx2(mce, vatomX);
   
   IRAtom* t2 = mkPCastTo(mce, Ity_V128, vRM);
   
   t1 = mkUifUV128(mce, t1, t2);
   return t1;
}


static
IRAtom* unary32Fx4_w_rm ( MCEnv* mce, IRAtom* vRM, IRAtom* vatomX )
{
   
   IRAtom* t1 = unary32Fx4(mce, vatomX);
   
   IRAtom* t2 = mkPCastTo(mce, Ity_V128, vRM);
   
   t1 = mkUifUV128(mce, t1, t2);
   return t1;
}



static
IROp vanillaNarrowingOpOfShape ( IROp qnarrowOp )
{
   switch (qnarrowOp) {
      
      case Iop_QNarrowBin16Sto8Ux16:
      case Iop_QNarrowBin16Sto8Sx16:
      case Iop_QNarrowBin16Uto8Ux16:
      case Iop_QNarrowBin64Sto32Sx4:
      case Iop_QNarrowBin64Uto32Ux4:
         return Iop_NarrowBin16to8x16;
      case Iop_QNarrowBin32Sto16Ux8:
      case Iop_QNarrowBin32Sto16Sx8:
      case Iop_QNarrowBin32Uto16Ux8:
         return Iop_NarrowBin32to16x8;
      
      case Iop_QNarrowBin32Sto16Sx4:
         return Iop_NarrowBin32to16x4;
      case Iop_QNarrowBin16Sto8Ux8:
      case Iop_QNarrowBin16Sto8Sx8:
         return Iop_NarrowBin16to8x8;
      
      case Iop_QNarrowUn64Uto32Ux2:
      case Iop_QNarrowUn64Sto32Sx2:
      case Iop_QNarrowUn64Sto32Ux2:
         return Iop_NarrowUn64to32x2;
      case Iop_QNarrowUn32Uto16Ux4:
      case Iop_QNarrowUn32Sto16Sx4:
      case Iop_QNarrowUn32Sto16Ux4:
         return Iop_NarrowUn32to16x4;
      case Iop_QNarrowUn16Uto8Ux8:
      case Iop_QNarrowUn16Sto8Sx8:
      case Iop_QNarrowUn16Sto8Ux8:
         return Iop_NarrowUn16to8x8;
      default: 
         ppIROp(qnarrowOp);
         VG_(tool_panic)("vanillaNarrowOpOfShape");
   }
}

static
IRAtom* vectorNarrowBinV128 ( MCEnv* mce, IROp narrow_op, 
                              IRAtom* vatom1, IRAtom* vatom2)
{
   IRAtom *at1, *at2, *at3;
   IRAtom* (*pcast)( MCEnv*, IRAtom* );
   switch (narrow_op) {
      case Iop_QNarrowBin64Sto32Sx4: pcast = mkPCast32x4; break;
      case Iop_QNarrowBin64Uto32Ux4: pcast = mkPCast32x4; break;
      case Iop_QNarrowBin32Sto16Sx8: pcast = mkPCast32x4; break;
      case Iop_QNarrowBin32Uto16Ux8: pcast = mkPCast32x4; break;
      case Iop_QNarrowBin32Sto16Ux8: pcast = mkPCast32x4; break;
      case Iop_QNarrowBin16Sto8Sx16: pcast = mkPCast16x8; break;
      case Iop_QNarrowBin16Uto8Ux16: pcast = mkPCast16x8; break;
      case Iop_QNarrowBin16Sto8Ux16: pcast = mkPCast16x8; break;
      default: VG_(tool_panic)("vectorNarrowBinV128");
   }
   IROp vanilla_narrow = vanillaNarrowingOpOfShape(narrow_op);
   tl_assert(isShadowAtom(mce,vatom1));
   tl_assert(isShadowAtom(mce,vatom2));
   at1 = assignNew('V', mce, Ity_V128, pcast(mce, vatom1));
   at2 = assignNew('V', mce, Ity_V128, pcast(mce, vatom2));
   at3 = assignNew('V', mce, Ity_V128, binop(vanilla_narrow, at1, at2));
   return at3;
}

static
IRAtom* vectorNarrowBin64 ( MCEnv* mce, IROp narrow_op, 
                            IRAtom* vatom1, IRAtom* vatom2)
{
   IRAtom *at1, *at2, *at3;
   IRAtom* (*pcast)( MCEnv*, IRAtom* );
   switch (narrow_op) {
      case Iop_QNarrowBin32Sto16Sx4: pcast = mkPCast32x2; break;
      case Iop_QNarrowBin16Sto8Sx8:  pcast = mkPCast16x4; break;
      case Iop_QNarrowBin16Sto8Ux8:  pcast = mkPCast16x4; break;
      default: VG_(tool_panic)("vectorNarrowBin64");
   }
   IROp vanilla_narrow = vanillaNarrowingOpOfShape(narrow_op);
   tl_assert(isShadowAtom(mce,vatom1));
   tl_assert(isShadowAtom(mce,vatom2));
   at1 = assignNew('V', mce, Ity_I64, pcast(mce, vatom1));
   at2 = assignNew('V', mce, Ity_I64, pcast(mce, vatom2));
   at3 = assignNew('V', mce, Ity_I64, binop(vanilla_narrow, at1, at2));
   return at3;
}

static
IRAtom* vectorNarrowUnV128 ( MCEnv* mce, IROp narrow_op,
                             IRAtom* vatom1)
{
   IRAtom *at1, *at2;
   IRAtom* (*pcast)( MCEnv*, IRAtom* );
   tl_assert(isShadowAtom(mce,vatom1));
   switch (narrow_op) {
      case Iop_NarrowUn16to8x8:
      case Iop_NarrowUn32to16x4:
      case Iop_NarrowUn64to32x2:
         at1 = assignNew('V', mce, Ity_I64, unop(narrow_op, vatom1));
         return at1;
      default:
         break; 
   }
   switch (narrow_op) {
      case Iop_QNarrowUn16Sto8Sx8:  pcast = mkPCast16x8; break;
      case Iop_QNarrowUn16Sto8Ux8:  pcast = mkPCast16x8; break;
      case Iop_QNarrowUn16Uto8Ux8:  pcast = mkPCast16x8; break;
      case Iop_QNarrowUn32Sto16Sx4: pcast = mkPCast32x4; break;
      case Iop_QNarrowUn32Sto16Ux4: pcast = mkPCast32x4; break;
      case Iop_QNarrowUn32Uto16Ux4: pcast = mkPCast32x4; break;
      case Iop_QNarrowUn64Sto32Sx2: pcast = mkPCast64x2; break;
      case Iop_QNarrowUn64Sto32Ux2: pcast = mkPCast64x2; break;
      case Iop_QNarrowUn64Uto32Ux2: pcast = mkPCast64x2; break;
      default: VG_(tool_panic)("vectorNarrowUnV128");
   }
   IROp vanilla_narrow = vanillaNarrowingOpOfShape(narrow_op);
   at1 = assignNew('V', mce, Ity_V128, pcast(mce, vatom1));
   at2 = assignNew('V', mce, Ity_I64, unop(vanilla_narrow, at1));
   return at2;
}

static
IRAtom* vectorWidenI64 ( MCEnv* mce, IROp longen_op,
                         IRAtom* vatom1)
{
   IRAtom *at1, *at2;
   IRAtom* (*pcast)( MCEnv*, IRAtom* );
   switch (longen_op) {
      case Iop_Widen8Uto16x8:  pcast = mkPCast16x8; break;
      case Iop_Widen8Sto16x8:  pcast = mkPCast16x8; break;
      case Iop_Widen16Uto32x4: pcast = mkPCast32x4; break;
      case Iop_Widen16Sto32x4: pcast = mkPCast32x4; break;
      case Iop_Widen32Uto64x2: pcast = mkPCast64x2; break;
      case Iop_Widen32Sto64x2: pcast = mkPCast64x2; break;
      default: VG_(tool_panic)("vectorWidenI64");
   }
   tl_assert(isShadowAtom(mce,vatom1));
   at1 = assignNew('V', mce, Ity_V128, unop(longen_op, vatom1));
   at2 = assignNew('V', mce, Ity_V128, pcast(mce, at1));
   return at2;
}





static
IRAtom* binary8Ix32 ( MCEnv* mce, IRAtom* vatom1, IRAtom* vatom2 )
{
   IRAtom* at;
   at = mkUifUV256(mce, vatom1, vatom2);
   at = mkPCast8x32(mce, at);
   return at;   
}

static
IRAtom* binary16Ix16 ( MCEnv* mce, IRAtom* vatom1, IRAtom* vatom2 )
{
   IRAtom* at;
   at = mkUifUV256(mce, vatom1, vatom2);
   at = mkPCast16x16(mce, at);
   return at;   
}

static
IRAtom* binary32Ix8 ( MCEnv* mce, IRAtom* vatom1, IRAtom* vatom2 )
{
   IRAtom* at;
   at = mkUifUV256(mce, vatom1, vatom2);
   at = mkPCast32x8(mce, at);
   return at;   
}

static
IRAtom* binary64Ix4 ( MCEnv* mce, IRAtom* vatom1, IRAtom* vatom2 )
{
   IRAtom* at;
   at = mkUifUV256(mce, vatom1, vatom2);
   at = mkPCast64x4(mce, at);
   return at;   
}


static
IRAtom* binary8Ix16 ( MCEnv* mce, IRAtom* vatom1, IRAtom* vatom2 )
{
   IRAtom* at;
   at = mkUifUV128(mce, vatom1, vatom2);
   at = mkPCast8x16(mce, at);
   return at;   
}

static
IRAtom* binary16Ix8 ( MCEnv* mce, IRAtom* vatom1, IRAtom* vatom2 )
{
   IRAtom* at;
   at = mkUifUV128(mce, vatom1, vatom2);
   at = mkPCast16x8(mce, at);
   return at;   
}

static
IRAtom* binary32Ix4 ( MCEnv* mce, IRAtom* vatom1, IRAtom* vatom2 )
{
   IRAtom* at;
   at = mkUifUV128(mce, vatom1, vatom2);
   at = mkPCast32x4(mce, at);
   return at;   
}

static
IRAtom* binary64Ix2 ( MCEnv* mce, IRAtom* vatom1, IRAtom* vatom2 )
{
   IRAtom* at;
   at = mkUifUV128(mce, vatom1, vatom2);
   at = mkPCast64x2(mce, at);
   return at;   
}


static
IRAtom* binary8Ix8 ( MCEnv* mce, IRAtom* vatom1, IRAtom* vatom2 )
{
   IRAtom* at;
   at = mkUifU64(mce, vatom1, vatom2);
   at = mkPCast8x8(mce, at);
   return at;   
}

static
IRAtom* binary16Ix4 ( MCEnv* mce, IRAtom* vatom1, IRAtom* vatom2 )
{
   IRAtom* at;
   at = mkUifU64(mce, vatom1, vatom2);
   at = mkPCast16x4(mce, at);
   return at;   
}

static
IRAtom* binary32Ix2 ( MCEnv* mce, IRAtom* vatom1, IRAtom* vatom2 )
{
   IRAtom* at;
   at = mkUifU64(mce, vatom1, vatom2);
   at = mkPCast32x2(mce, at);
   return at;   
}

static
IRAtom* binary64Ix1 ( MCEnv* mce, IRAtom* vatom1, IRAtom* vatom2 )
{
   IRAtom* at;
   at = mkUifU64(mce, vatom1, vatom2);
   at = mkPCastTo(mce, Ity_I64, at);
   return at;
}


static
IRAtom* binary8Ix4 ( MCEnv* mce, IRAtom* vatom1, IRAtom* vatom2 )
{
   IRAtom* at;
   at = mkUifU32(mce, vatom1, vatom2);
   at = mkPCast8x4(mce, at);
   return at;   
}

static
IRAtom* binary16Ix2 ( MCEnv* mce, IRAtom* vatom1, IRAtom* vatom2 )
{
   IRAtom* at;
   at = mkUifU32(mce, vatom1, vatom2);
   at = mkPCast16x2(mce, at);
   return at;   
}



static 
IRAtom* expr2vbits_Qop ( MCEnv* mce,
                         IROp op,
                         IRAtom* atom1, IRAtom* atom2, 
                         IRAtom* atom3, IRAtom* atom4 )
{
   IRAtom* vatom1 = expr2vbits( mce, atom1 );
   IRAtom* vatom2 = expr2vbits( mce, atom2 );
   IRAtom* vatom3 = expr2vbits( mce, atom3 );
   IRAtom* vatom4 = expr2vbits( mce, atom4 );

   tl_assert(isOriginalAtom(mce,atom1));
   tl_assert(isOriginalAtom(mce,atom2));
   tl_assert(isOriginalAtom(mce,atom3));
   tl_assert(isOriginalAtom(mce,atom4));
   tl_assert(isShadowAtom(mce,vatom1));
   tl_assert(isShadowAtom(mce,vatom2));
   tl_assert(isShadowAtom(mce,vatom3));
   tl_assert(isShadowAtom(mce,vatom4));
   tl_assert(sameKindedAtoms(atom1,vatom1));
   tl_assert(sameKindedAtoms(atom2,vatom2));
   tl_assert(sameKindedAtoms(atom3,vatom3));
   tl_assert(sameKindedAtoms(atom4,vatom4));
   switch (op) {
      case Iop_MAddF64:
      case Iop_MAddF64r32:
      case Iop_MSubF64:
      case Iop_MSubF64r32:
         
         return mkLazy4(mce, Ity_I64, vatom1, vatom2, vatom3, vatom4);

      case Iop_MAddF32:
      case Iop_MSubF32:
         
         return mkLazy4(mce, Ity_I32, vatom1, vatom2, vatom3, vatom4);

      
      case Iop_64x4toV256:
         return assignNew('V', mce, Ity_V256,
                          IRExpr_Qop(op, vatom1, vatom2, vatom3, vatom4));

      default:
         ppIROp(op);
         VG_(tool_panic)("memcheck:expr2vbits_Qop");
   }
}


static 
IRAtom* expr2vbits_Triop ( MCEnv* mce,
                           IROp op,
                           IRAtom* atom1, IRAtom* atom2, IRAtom* atom3 )
{
   IRAtom* vatom1 = expr2vbits( mce, atom1 );
   IRAtom* vatom2 = expr2vbits( mce, atom2 );
   IRAtom* vatom3 = expr2vbits( mce, atom3 );

   tl_assert(isOriginalAtom(mce,atom1));
   tl_assert(isOriginalAtom(mce,atom2));
   tl_assert(isOriginalAtom(mce,atom3));
   tl_assert(isShadowAtom(mce,vatom1));
   tl_assert(isShadowAtom(mce,vatom2));
   tl_assert(isShadowAtom(mce,vatom3));
   tl_assert(sameKindedAtoms(atom1,vatom1));
   tl_assert(sameKindedAtoms(atom2,vatom2));
   tl_assert(sameKindedAtoms(atom3,vatom3));
   switch (op) {
      case Iop_AddF128:
      case Iop_AddD128:
      case Iop_SubF128:
      case Iop_SubD128:
      case Iop_MulF128:
      case Iop_MulD128:
      case Iop_DivF128:
      case Iop_DivD128:
      case Iop_QuantizeD128:
         
         return mkLazy3(mce, Ity_I128, vatom1, vatom2, vatom3);
      case Iop_AddF64:
      case Iop_AddD64:
      case Iop_AddF64r32:
      case Iop_SubF64:
      case Iop_SubD64:
      case Iop_SubF64r32:
      case Iop_MulF64:
      case Iop_MulD64:
      case Iop_MulF64r32:
      case Iop_DivF64:
      case Iop_DivD64:
      case Iop_DivF64r32:
      case Iop_ScaleF64:
      case Iop_Yl2xF64:
      case Iop_Yl2xp1F64:
      case Iop_AtanF64:
      case Iop_PRemF64:
      case Iop_PRem1F64:
      case Iop_QuantizeD64:
         
         return mkLazy3(mce, Ity_I64, vatom1, vatom2, vatom3);
      case Iop_PRemC3210F64:
      case Iop_PRem1C3210F64:
         
         return mkLazy3(mce, Ity_I32, vatom1, vatom2, vatom3);
      case Iop_AddF32:
      case Iop_SubF32:
      case Iop_MulF32:
      case Iop_DivF32:
         
         return mkLazy3(mce, Ity_I32, vatom1, vatom2, vatom3);
      case Iop_SignificanceRoundD64:
         
         return mkLazy3(mce, Ity_I64, vatom1, vatom2, vatom3);
      case Iop_SignificanceRoundD128:
         
         return mkLazy3(mce, Ity_I128, vatom1, vatom2, vatom3);
      case Iop_SliceV128:
         
         complainIfUndefined(mce, atom3, NULL);
         return assignNew('V', mce, Ity_V128, triop(op, vatom1, vatom2, atom3));
      case Iop_Slice64:
         
         complainIfUndefined(mce, atom3, NULL);
         return assignNew('V', mce, Ity_I64, triop(op, vatom1, vatom2, atom3));
      case Iop_SetElem8x8:
      case Iop_SetElem16x4:
      case Iop_SetElem32x2:
         complainIfUndefined(mce, atom2, NULL);
         return assignNew('V', mce, Ity_I64, triop(op, vatom1, atom2, vatom3));
      
      case Iop_BCDAdd:
      case Iop_BCDSub:
         complainIfUndefined(mce, atom3, NULL);
         return assignNew('V', mce, Ity_V128, triop(op, vatom1, vatom2, atom3));

      
      case Iop_Add64Fx2:
      case Iop_Sub64Fx2:
      case Iop_Mul64Fx2:
      case Iop_Div64Fx2:
         return binary64Fx2_w_rm(mce, vatom1, vatom2, vatom3);

      case Iop_Add32Fx4:
      case Iop_Sub32Fx4:
      case Iop_Mul32Fx4:
      case Iop_Div32Fx4:
        return binary32Fx4_w_rm(mce, vatom1, vatom2, vatom3);

      case Iop_Add64Fx4:
      case Iop_Sub64Fx4:
      case Iop_Mul64Fx4:
      case Iop_Div64Fx4:
         return binary64Fx4_w_rm(mce, vatom1, vatom2, vatom3);

      case Iop_Add32Fx8:
      case Iop_Sub32Fx8:
      case Iop_Mul32Fx8:
      case Iop_Div32Fx8:
         return binary32Fx8_w_rm(mce, vatom1, vatom2, vatom3);

      default:
         ppIROp(op);
         VG_(tool_panic)("memcheck:expr2vbits_Triop");
   }
}


static 
IRAtom* expr2vbits_Binop ( MCEnv* mce,
                           IROp op,
                           IRAtom* atom1, IRAtom* atom2 )
{
   IRType  and_or_ty;
   IRAtom* (*uifu)    (MCEnv*, IRAtom*, IRAtom*);
   IRAtom* (*difd)    (MCEnv*, IRAtom*, IRAtom*);
   IRAtom* (*improve) (MCEnv*, IRAtom*, IRAtom*);

   IRAtom* vatom1 = expr2vbits( mce, atom1 );
   IRAtom* vatom2 = expr2vbits( mce, atom2 );

   tl_assert(isOriginalAtom(mce,atom1));
   tl_assert(isOriginalAtom(mce,atom2));
   tl_assert(isShadowAtom(mce,vatom1));
   tl_assert(isShadowAtom(mce,vatom2));
   tl_assert(sameKindedAtoms(atom1,vatom1));
   tl_assert(sameKindedAtoms(atom2,vatom2));
   switch (op) {

      

      case Iop_Add16x2:
      case Iop_HAdd16Ux2:
      case Iop_HAdd16Sx2:
      case Iop_Sub16x2:
      case Iop_HSub16Ux2:
      case Iop_HSub16Sx2:
      case Iop_QAdd16Sx2:
      case Iop_QSub16Sx2:
      case Iop_QSub16Ux2:
      case Iop_QAdd16Ux2:
         return binary16Ix2(mce, vatom1, vatom2);

      case Iop_Add8x4:
      case Iop_HAdd8Ux4:
      case Iop_HAdd8Sx4:
      case Iop_Sub8x4:
      case Iop_HSub8Ux4:
      case Iop_HSub8Sx4:
      case Iop_QSub8Ux4:
      case Iop_QAdd8Ux4:
      case Iop_QSub8Sx4:
      case Iop_QAdd8Sx4:
         return binary8Ix4(mce, vatom1, vatom2);

      

      case Iop_ShrN8x8:
      case Iop_ShrN16x4:
      case Iop_ShrN32x2:
      case Iop_SarN8x8:
      case Iop_SarN16x4:
      case Iop_SarN32x2:
      case Iop_ShlN16x4:
      case Iop_ShlN32x2:
      case Iop_ShlN8x8:
         
         complainIfUndefined(mce, atom2, NULL);
         return assignNew('V', mce, Ity_I64, binop(op, vatom1, atom2));

      case Iop_QNarrowBin32Sto16Sx4:
      case Iop_QNarrowBin16Sto8Sx8:
      case Iop_QNarrowBin16Sto8Ux8:
         return vectorNarrowBin64(mce, op, vatom1, vatom2);

      case Iop_Min8Ux8:
      case Iop_Min8Sx8:
      case Iop_Max8Ux8:
      case Iop_Max8Sx8:
      case Iop_Avg8Ux8:
      case Iop_QSub8Sx8:
      case Iop_QSub8Ux8:
      case Iop_Sub8x8:
      case Iop_CmpGT8Sx8:
      case Iop_CmpGT8Ux8:
      case Iop_CmpEQ8x8:
      case Iop_QAdd8Sx8:
      case Iop_QAdd8Ux8:
      case Iop_QSal8x8:
      case Iop_QShl8x8:
      case Iop_Add8x8:
      case Iop_Mul8x8:
      case Iop_PolynomialMul8x8:
         return binary8Ix8(mce, vatom1, vatom2);

      case Iop_Min16Sx4:
      case Iop_Min16Ux4:
      case Iop_Max16Sx4:
      case Iop_Max16Ux4:
      case Iop_Avg16Ux4:
      case Iop_QSub16Ux4:
      case Iop_QSub16Sx4:
      case Iop_Sub16x4:
      case Iop_Mul16x4:
      case Iop_MulHi16Sx4:
      case Iop_MulHi16Ux4:
      case Iop_CmpGT16Sx4:
      case Iop_CmpGT16Ux4:
      case Iop_CmpEQ16x4:
      case Iop_QAdd16Sx4:
      case Iop_QAdd16Ux4:
      case Iop_QSal16x4:
      case Iop_QShl16x4:
      case Iop_Add16x4:
      case Iop_QDMulHi16Sx4:
      case Iop_QRDMulHi16Sx4:
         return binary16Ix4(mce, vatom1, vatom2);

      case Iop_Sub32x2:
      case Iop_Mul32x2:
      case Iop_Max32Sx2:
      case Iop_Max32Ux2:
      case Iop_Min32Sx2:
      case Iop_Min32Ux2:
      case Iop_CmpGT32Sx2:
      case Iop_CmpGT32Ux2:
      case Iop_CmpEQ32x2:
      case Iop_Add32x2:
      case Iop_QAdd32Ux2:
      case Iop_QAdd32Sx2:
      case Iop_QSub32Ux2:
      case Iop_QSub32Sx2:
      case Iop_QSal32x2:
      case Iop_QShl32x2:
      case Iop_QDMulHi32Sx2:
      case Iop_QRDMulHi32Sx2:
         return binary32Ix2(mce, vatom1, vatom2);

      case Iop_QSub64Ux1:
      case Iop_QSub64Sx1:
      case Iop_QAdd64Ux1:
      case Iop_QAdd64Sx1:
      case Iop_QSal64x1:
      case Iop_QShl64x1:
      case Iop_Sal64x1:
         return binary64Ix1(mce, vatom1, vatom2);

      case Iop_QShlNsatSU8x8:
      case Iop_QShlNsatUU8x8:
      case Iop_QShlNsatSS8x8:
         complainIfUndefined(mce, atom2, NULL);
         return mkPCast8x8(mce, vatom1);

      case Iop_QShlNsatSU16x4:
      case Iop_QShlNsatUU16x4:
      case Iop_QShlNsatSS16x4:
         complainIfUndefined(mce, atom2, NULL);
         return mkPCast16x4(mce, vatom1);

      case Iop_QShlNsatSU32x2:
      case Iop_QShlNsatUU32x2:
      case Iop_QShlNsatSS32x2:
         complainIfUndefined(mce, atom2, NULL);
         return mkPCast32x2(mce, vatom1);

      case Iop_QShlNsatSU64x1:
      case Iop_QShlNsatUU64x1:
      case Iop_QShlNsatSS64x1:
         complainIfUndefined(mce, atom2, NULL);
         return mkPCast32x2(mce, vatom1);

      case Iop_PwMax32Sx2:
      case Iop_PwMax32Ux2:
      case Iop_PwMin32Sx2:
      case Iop_PwMin32Ux2:
      case Iop_PwMax32Fx2:
      case Iop_PwMin32Fx2:
         return assignNew('V', mce, Ity_I64,
                          binop(Iop_PwMax32Ux2, 
                                mkPCast32x2(mce, vatom1),
                                mkPCast32x2(mce, vatom2)));

      case Iop_PwMax16Sx4:
      case Iop_PwMax16Ux4:
      case Iop_PwMin16Sx4:
      case Iop_PwMin16Ux4:
         return assignNew('V', mce, Ity_I64,
                          binop(Iop_PwMax16Ux4,
                                mkPCast16x4(mce, vatom1),
                                mkPCast16x4(mce, vatom2)));

      case Iop_PwMax8Sx8:
      case Iop_PwMax8Ux8:
      case Iop_PwMin8Sx8:
      case Iop_PwMin8Ux8:
         return assignNew('V', mce, Ity_I64,
                          binop(Iop_PwMax8Ux8,
                                mkPCast8x8(mce, vatom1),
                                mkPCast8x8(mce, vatom2)));

      case Iop_PwAdd32x2:
      case Iop_PwAdd32Fx2:
         return mkPCast32x2(mce,
               assignNew('V', mce, Ity_I64,
                         binop(Iop_PwAdd32x2,
                               mkPCast32x2(mce, vatom1),
                               mkPCast32x2(mce, vatom2))));

      case Iop_PwAdd16x4:
         return mkPCast16x4(mce,
               assignNew('V', mce, Ity_I64,
                         binop(op, mkPCast16x4(mce, vatom1),
                                   mkPCast16x4(mce, vatom2))));

      case Iop_PwAdd8x8:
         return mkPCast8x8(mce,
               assignNew('V', mce, Ity_I64,
                         binop(op, mkPCast8x8(mce, vatom1),
                                   mkPCast8x8(mce, vatom2))));

      case Iop_Shl8x8:
      case Iop_Shr8x8:
      case Iop_Sar8x8:
      case Iop_Sal8x8:
         return mkUifU64(mce,
                   assignNew('V', mce, Ity_I64, binop(op, vatom1, atom2)),
                   mkPCast8x8(mce,vatom2)
                );

      case Iop_Shl16x4:
      case Iop_Shr16x4:
      case Iop_Sar16x4:
      case Iop_Sal16x4:
         return mkUifU64(mce,
                   assignNew('V', mce, Ity_I64, binop(op, vatom1, atom2)),
                   mkPCast16x4(mce,vatom2)
                );

      case Iop_Shl32x2:
      case Iop_Shr32x2:
      case Iop_Sar32x2:
      case Iop_Sal32x2:
         return mkUifU64(mce,
                   assignNew('V', mce, Ity_I64, binop(op, vatom1, atom2)),
                   mkPCast32x2(mce,vatom2)
                );

      
      case Iop_InterleaveLO32x2:
      case Iop_InterleaveLO16x4:
      case Iop_InterleaveLO8x8:
      case Iop_InterleaveHI32x2:
      case Iop_InterleaveHI16x4:
      case Iop_InterleaveHI8x8:
      case Iop_CatOddLanes8x8:
      case Iop_CatEvenLanes8x8:
      case Iop_CatOddLanes16x4:
      case Iop_CatEvenLanes16x4:
      case Iop_InterleaveOddLanes8x8:
      case Iop_InterleaveEvenLanes8x8:
      case Iop_InterleaveOddLanes16x4:
      case Iop_InterleaveEvenLanes16x4:
         return assignNew('V', mce, Ity_I64, binop(op, vatom1, vatom2));

      case Iop_GetElem8x8:
         complainIfUndefined(mce, atom2, NULL);
         return assignNew('V', mce, Ity_I8, binop(op, vatom1, atom2));
      case Iop_GetElem16x4:
         complainIfUndefined(mce, atom2, NULL);
         return assignNew('V', mce, Ity_I16, binop(op, vatom1, atom2));
      case Iop_GetElem32x2:
         complainIfUndefined(mce, atom2, NULL);
         return assignNew('V', mce, Ity_I32, binop(op, vatom1, atom2));

      case Iop_Perm8x8:
         return mkUifU64(
                   mce,
                   assignNew('V', mce, Ity_I64, binop(op, vatom1, atom2)),
                   mkPCast8x8(mce, vatom2)
                );

      

      case Iop_Sqrt32Fx4:
         return unary32Fx4_w_rm(mce, vatom1, vatom2);
      case Iop_Sqrt64Fx2:
         return unary64Fx2_w_rm(mce, vatom1, vatom2);

      case Iop_ShrN8x16:
      case Iop_ShrN16x8:
      case Iop_ShrN32x4:
      case Iop_ShrN64x2:
      case Iop_SarN8x16:
      case Iop_SarN16x8:
      case Iop_SarN32x4:
      case Iop_SarN64x2:
      case Iop_ShlN8x16:
      case Iop_ShlN16x8:
      case Iop_ShlN32x4:
      case Iop_ShlN64x2:
         complainIfUndefined(mce, atom2, NULL);
         return assignNew('V', mce, Ity_V128, binop(op, vatom1, atom2));

      
      case Iop_Shl8x16:
      case Iop_Shr8x16:
      case Iop_Sar8x16:
      case Iop_Sal8x16:
      case Iop_Rol8x16:
      case Iop_Sh8Sx16:
      case Iop_Sh8Ux16:
         return mkUifUV128(mce,
                   assignNew('V', mce, Ity_V128, binop(op, vatom1, atom2)),
                   mkPCast8x16(mce,vatom2)
                );

      case Iop_Shl16x8:
      case Iop_Shr16x8:
      case Iop_Sar16x8:
      case Iop_Sal16x8:
      case Iop_Rol16x8:
      case Iop_Sh16Sx8:
      case Iop_Sh16Ux8:
         return mkUifUV128(mce,
                   assignNew('V', mce, Ity_V128, binop(op, vatom1, atom2)),
                   mkPCast16x8(mce,vatom2)
                );

      case Iop_Shl32x4:
      case Iop_Shr32x4:
      case Iop_Sar32x4:
      case Iop_Sal32x4:
      case Iop_Rol32x4:
      case Iop_Sh32Sx4:
      case Iop_Sh32Ux4:
         return mkUifUV128(mce,
                   assignNew('V', mce, Ity_V128, binop(op, vatom1, atom2)),
                   mkPCast32x4(mce,vatom2)
                );

      case Iop_Shl64x2:
      case Iop_Shr64x2:
      case Iop_Sar64x2:
      case Iop_Sal64x2:
      case Iop_Rol64x2:
      case Iop_Sh64Sx2:
      case Iop_Sh64Ux2:
         return mkUifUV128(mce,
                   assignNew('V', mce, Ity_V128, binop(op, vatom1, atom2)),
                   mkPCast64x2(mce,vatom2)
                );

      case Iop_Rsh8Sx16:
      case Iop_Rsh8Ux16:
         return binary8Ix16(mce, vatom1, vatom2);
      case Iop_Rsh16Sx8:
      case Iop_Rsh16Ux8:
         return binary16Ix8(mce, vatom1, vatom2);
      case Iop_Rsh32Sx4:
      case Iop_Rsh32Ux4:
         return binary32Ix4(mce, vatom1, vatom2);
      case Iop_Rsh64Sx2:
      case Iop_Rsh64Ux2:
         return binary64Ix2(mce, vatom1, vatom2);

      case Iop_F32ToFixed32Ux4_RZ:
      case Iop_F32ToFixed32Sx4_RZ:
      case Iop_Fixed32UToF32x4_RN:
      case Iop_Fixed32SToF32x4_RN:
         complainIfUndefined(mce, atom2, NULL);
         return mkPCast32x4(mce, vatom1);

      case Iop_F32ToFixed32Ux2_RZ:
      case Iop_F32ToFixed32Sx2_RZ:
      case Iop_Fixed32UToF32x2_RN:
      case Iop_Fixed32SToF32x2_RN:
         complainIfUndefined(mce, atom2, NULL);
         return mkPCast32x2(mce, vatom1);

      case Iop_QSub8Ux16:
      case Iop_QSub8Sx16:
      case Iop_Sub8x16:
      case Iop_Min8Ux16:
      case Iop_Min8Sx16:
      case Iop_Max8Ux16:
      case Iop_Max8Sx16:
      case Iop_CmpGT8Sx16:
      case Iop_CmpGT8Ux16:
      case Iop_CmpEQ8x16:
      case Iop_Avg8Ux16:
      case Iop_Avg8Sx16:
      case Iop_QAdd8Ux16:
      case Iop_QAdd8Sx16:
      case Iop_QAddExtUSsatSS8x16:
      case Iop_QAddExtSUsatUU8x16:
      case Iop_QSal8x16:
      case Iop_QShl8x16:
      case Iop_Add8x16:
      case Iop_Mul8x16:
      case Iop_PolynomialMul8x16:
      case Iop_PolynomialMulAdd8x16:
         return binary8Ix16(mce, vatom1, vatom2);

      case Iop_QSub16Ux8:
      case Iop_QSub16Sx8:
      case Iop_Sub16x8:
      case Iop_Mul16x8:
      case Iop_MulHi16Sx8:
      case Iop_MulHi16Ux8:
      case Iop_Min16Sx8:
      case Iop_Min16Ux8:
      case Iop_Max16Sx8:
      case Iop_Max16Ux8:
      case Iop_CmpGT16Sx8:
      case Iop_CmpGT16Ux8:
      case Iop_CmpEQ16x8:
      case Iop_Avg16Ux8:
      case Iop_Avg16Sx8:
      case Iop_QAdd16Ux8:
      case Iop_QAdd16Sx8:
      case Iop_QAddExtUSsatSS16x8:
      case Iop_QAddExtSUsatUU16x8:
      case Iop_QSal16x8:
      case Iop_QShl16x8:
      case Iop_Add16x8:
      case Iop_QDMulHi16Sx8:
      case Iop_QRDMulHi16Sx8:
      case Iop_PolynomialMulAdd16x8:
         return binary16Ix8(mce, vatom1, vatom2);

      case Iop_Sub32x4:
      case Iop_CmpGT32Sx4:
      case Iop_CmpGT32Ux4:
      case Iop_CmpEQ32x4:
      case Iop_QAdd32Sx4:
      case Iop_QAdd32Ux4:
      case Iop_QSub32Sx4:
      case Iop_QSub32Ux4:
      case Iop_QAddExtUSsatSS32x4:
      case Iop_QAddExtSUsatUU32x4:
      case Iop_QSal32x4:
      case Iop_QShl32x4:
      case Iop_Avg32Ux4:
      case Iop_Avg32Sx4:
      case Iop_Add32x4:
      case Iop_Max32Ux4:
      case Iop_Max32Sx4:
      case Iop_Min32Ux4:
      case Iop_Min32Sx4:
      case Iop_Mul32x4:
      case Iop_QDMulHi32Sx4:
      case Iop_QRDMulHi32Sx4:
      case Iop_PolynomialMulAdd32x4:
         return binary32Ix4(mce, vatom1, vatom2);

      case Iop_Sub64x2:
      case Iop_Add64x2:
      case Iop_Max64Sx2:
      case Iop_Max64Ux2:
      case Iop_Min64Sx2:
      case Iop_Min64Ux2:
      case Iop_CmpEQ64x2:
      case Iop_CmpGT64Sx2:
      case Iop_CmpGT64Ux2:
      case Iop_QSal64x2:
      case Iop_QShl64x2:
      case Iop_QAdd64Ux2:
      case Iop_QAdd64Sx2:
      case Iop_QSub64Ux2:
      case Iop_QSub64Sx2:
      case Iop_QAddExtUSsatSS64x2:
      case Iop_QAddExtSUsatUU64x2:
      case Iop_PolynomialMulAdd64x2:
      case Iop_CipherV128:
      case Iop_CipherLV128:
      case Iop_NCipherV128:
      case Iop_NCipherLV128:
        return binary64Ix2(mce, vatom1, vatom2);

      case Iop_QNarrowBin64Sto32Sx4:
      case Iop_QNarrowBin64Uto32Ux4:
      case Iop_QNarrowBin32Sto16Sx8:
      case Iop_QNarrowBin32Uto16Ux8:
      case Iop_QNarrowBin32Sto16Ux8:
      case Iop_QNarrowBin16Sto8Sx16:
      case Iop_QNarrowBin16Uto8Ux16:
      case Iop_QNarrowBin16Sto8Ux16:
         return vectorNarrowBinV128(mce, op, vatom1, vatom2);

      case Iop_Min64Fx2:
      case Iop_Max64Fx2:
      case Iop_CmpLT64Fx2:
      case Iop_CmpLE64Fx2:
      case Iop_CmpEQ64Fx2:
      case Iop_CmpUN64Fx2:
      case Iop_RecipStep64Fx2:
      case Iop_RSqrtStep64Fx2:
         return binary64Fx2(mce, vatom1, vatom2);      

      case Iop_Sub64F0x2:
      case Iop_Mul64F0x2:
      case Iop_Min64F0x2:
      case Iop_Max64F0x2:
      case Iop_Div64F0x2:
      case Iop_CmpLT64F0x2:
      case Iop_CmpLE64F0x2:
      case Iop_CmpEQ64F0x2:
      case Iop_CmpUN64F0x2:
      case Iop_Add64F0x2:
         return binary64F0x2(mce, vatom1, vatom2);      

      case Iop_Min32Fx4:
      case Iop_Max32Fx4:
      case Iop_CmpLT32Fx4:
      case Iop_CmpLE32Fx4:
      case Iop_CmpEQ32Fx4:
      case Iop_CmpUN32Fx4:
      case Iop_CmpGT32Fx4:
      case Iop_CmpGE32Fx4:
      case Iop_RecipStep32Fx4:
      case Iop_RSqrtStep32Fx4:
         return binary32Fx4(mce, vatom1, vatom2);      

      case Iop_Sub32Fx2:
      case Iop_Mul32Fx2:
      case Iop_Min32Fx2:
      case Iop_Max32Fx2:
      case Iop_CmpEQ32Fx2:
      case Iop_CmpGT32Fx2:
      case Iop_CmpGE32Fx2:
      case Iop_Add32Fx2:
      case Iop_RecipStep32Fx2:
      case Iop_RSqrtStep32Fx2:
         return binary32Fx2(mce, vatom1, vatom2);      

      case Iop_Sub32F0x4:
      case Iop_Mul32F0x4:
      case Iop_Min32F0x4:
      case Iop_Max32F0x4:
      case Iop_Div32F0x4:
      case Iop_CmpLT32F0x4:
      case Iop_CmpLE32F0x4:
      case Iop_CmpEQ32F0x4:
      case Iop_CmpUN32F0x4:
      case Iop_Add32F0x4:
         return binary32F0x4(mce, vatom1, vatom2);      

      case Iop_QShlNsatSU8x16:
      case Iop_QShlNsatUU8x16:
      case Iop_QShlNsatSS8x16:
         complainIfUndefined(mce, atom2, NULL);
         return mkPCast8x16(mce, vatom1);

      case Iop_QShlNsatSU16x8:
      case Iop_QShlNsatUU16x8:
      case Iop_QShlNsatSS16x8:
         complainIfUndefined(mce, atom2, NULL);
         return mkPCast16x8(mce, vatom1);

      case Iop_QShlNsatSU32x4:
      case Iop_QShlNsatUU32x4:
      case Iop_QShlNsatSS32x4:
         complainIfUndefined(mce, atom2, NULL);
         return mkPCast32x4(mce, vatom1);

      case Iop_QShlNsatSU64x2:
      case Iop_QShlNsatUU64x2:
      case Iop_QShlNsatSS64x2:
         complainIfUndefined(mce, atom2, NULL);
         return mkPCast32x4(mce, vatom1);

      case Iop_QandQShrNnarrow64Uto32Ux2:
      case Iop_QandQSarNnarrow64Sto32Sx2:
      case Iop_QandQSarNnarrow64Sto32Ux2:
      case Iop_QandQRShrNnarrow64Uto32Ux2:
      case Iop_QandQRSarNnarrow64Sto32Sx2:
      case Iop_QandQRSarNnarrow64Sto32Ux2:
      case Iop_QandQShrNnarrow32Uto16Ux4:
      case Iop_QandQSarNnarrow32Sto16Sx4:
      case Iop_QandQSarNnarrow32Sto16Ux4:
      case Iop_QandQRShrNnarrow32Uto16Ux4:
      case Iop_QandQRSarNnarrow32Sto16Sx4:
      case Iop_QandQRSarNnarrow32Sto16Ux4:
      case Iop_QandQShrNnarrow16Uto8Ux8:
      case Iop_QandQSarNnarrow16Sto8Sx8:
      case Iop_QandQSarNnarrow16Sto8Ux8:
      case Iop_QandQRShrNnarrow16Uto8Ux8:
      case Iop_QandQRSarNnarrow16Sto8Sx8:
      case Iop_QandQRSarNnarrow16Sto8Ux8:
      {
         IRAtom* (*fnPessim) (MCEnv*, IRAtom*) = NULL;
         IROp opNarrow = Iop_INVALID;
         switch (op) {
            case Iop_QandQShrNnarrow64Uto32Ux2:
            case Iop_QandQSarNnarrow64Sto32Sx2:
            case Iop_QandQSarNnarrow64Sto32Ux2:
            case Iop_QandQRShrNnarrow64Uto32Ux2:
            case Iop_QandQRSarNnarrow64Sto32Sx2:
            case Iop_QandQRSarNnarrow64Sto32Ux2:
               fnPessim = mkPCast64x2;
               opNarrow = Iop_NarrowUn64to32x2;
               break;
            case Iop_QandQShrNnarrow32Uto16Ux4:
            case Iop_QandQSarNnarrow32Sto16Sx4:
            case Iop_QandQSarNnarrow32Sto16Ux4:
            case Iop_QandQRShrNnarrow32Uto16Ux4:
            case Iop_QandQRSarNnarrow32Sto16Sx4:
            case Iop_QandQRSarNnarrow32Sto16Ux4:
               fnPessim = mkPCast32x4;
               opNarrow = Iop_NarrowUn32to16x4;
               break;
            case Iop_QandQShrNnarrow16Uto8Ux8:
            case Iop_QandQSarNnarrow16Sto8Sx8:
            case Iop_QandQSarNnarrow16Sto8Ux8:
            case Iop_QandQRShrNnarrow16Uto8Ux8:
            case Iop_QandQRSarNnarrow16Sto8Sx8:
            case Iop_QandQRSarNnarrow16Sto8Ux8:
               fnPessim = mkPCast16x8;
               opNarrow = Iop_NarrowUn16to8x8;
               break;
            default:
               tl_assert(0);
         }
         complainIfUndefined(mce, atom2, NULL);
         
         IRAtom* shV
            = fnPessim(mce, vatom1);
         
         IRAtom* shVnarrowed
            = assignNew('V', mce, Ity_I64, unop(opNarrow, shV));
         
         IRAtom* qV = mkPCastXXtoXXlsb(mce, shVnarrowed, Ity_I64);
         
         return assignNew('V', mce, Ity_V128, 
                          binop(Iop_64HLtoV128, qV, shVnarrowed));
      }

      case Iop_Mull32Sx2:
      case Iop_Mull32Ux2:
      case Iop_QDMull32Sx2:
         return vectorWidenI64(mce, Iop_Widen32Sto64x2,
                                    mkUifU64(mce, vatom1, vatom2));

      case Iop_Mull16Sx4:
      case Iop_Mull16Ux4:
      case Iop_QDMull16Sx4:
         return vectorWidenI64(mce, Iop_Widen16Sto32x4,
                                    mkUifU64(mce, vatom1, vatom2));

      case Iop_Mull8Sx8:
      case Iop_Mull8Ux8:
      case Iop_PolynomialMull8x8:
         return vectorWidenI64(mce, Iop_Widen8Sto16x8,
                                    mkUifU64(mce, vatom1, vatom2));

      case Iop_PwAdd32x4:
         return mkPCast32x4(mce,
               assignNew('V', mce, Ity_V128, binop(op, mkPCast32x4(mce, vatom1),
                     mkPCast32x4(mce, vatom2))));

      case Iop_PwAdd16x8:
         return mkPCast16x8(mce,
               assignNew('V', mce, Ity_V128, binop(op, mkPCast16x8(mce, vatom1),
                     mkPCast16x8(mce, vatom2))));

      case Iop_PwAdd8x16:
         return mkPCast8x16(mce,
               assignNew('V', mce, Ity_V128, binop(op, mkPCast8x16(mce, vatom1),
                     mkPCast8x16(mce, vatom2))));

      
      case Iop_SetV128lo32:
      case Iop_SetV128lo64:
      case Iop_64HLtoV128:
      case Iop_InterleaveLO64x2:
      case Iop_InterleaveLO32x4:
      case Iop_InterleaveLO16x8:
      case Iop_InterleaveLO8x16:
      case Iop_InterleaveHI64x2:
      case Iop_InterleaveHI32x4:
      case Iop_InterleaveHI16x8:
      case Iop_InterleaveHI8x16:
      case Iop_CatOddLanes8x16:
      case Iop_CatOddLanes16x8:
      case Iop_CatOddLanes32x4:
      case Iop_CatEvenLanes8x16:
      case Iop_CatEvenLanes16x8:
      case Iop_CatEvenLanes32x4:
      case Iop_InterleaveOddLanes8x16:
      case Iop_InterleaveOddLanes16x8:
      case Iop_InterleaveOddLanes32x4:
      case Iop_InterleaveEvenLanes8x16:
      case Iop_InterleaveEvenLanes16x8:
      case Iop_InterleaveEvenLanes32x4:
         return assignNew('V', mce, Ity_V128, binop(op, vatom1, vatom2));

      case Iop_GetElem8x16:
         complainIfUndefined(mce, atom2, NULL);
         return assignNew('V', mce, Ity_I8, binop(op, vatom1, atom2));
      case Iop_GetElem16x8:
         complainIfUndefined(mce, atom2, NULL);
         return assignNew('V', mce, Ity_I16, binop(op, vatom1, atom2));
      case Iop_GetElem32x4:
         complainIfUndefined(mce, atom2, NULL);
         return assignNew('V', mce, Ity_I32, binop(op, vatom1, atom2));
      case Iop_GetElem64x2:
         complainIfUndefined(mce, atom2, NULL);
         return assignNew('V', mce, Ity_I64, binop(op, vatom1, atom2));

      case Iop_Perm8x16:
         return mkUifUV128(
                   mce,
                   assignNew('V', mce, Ity_V128, binop(op, vatom1, atom2)),
                   mkPCast8x16(mce, vatom2)
                );
      case Iop_Perm32x4:
         return mkUifUV128(
                   mce,
                   assignNew('V', mce, Ity_V128, binop(op, vatom1, atom2)),
                   mkPCast32x4(mce, vatom2)
                );

      case Iop_MullEven16Ux8:
      case Iop_MullEven16Sx8: {
         IRAtom* at;
         at = binary16Ix8(mce,vatom1,vatom2);
         at = assignNew('V', mce, Ity_V128, binop(Iop_ShlN32x4, at, mkU8(16)));
         at = assignNew('V', mce, Ity_V128, binop(Iop_SarN32x4, at, mkU8(16)));
	 return at;
      }

      
      case Iop_MullEven8Ux16:
      case Iop_MullEven8Sx16: {
         IRAtom* at;
         at = binary8Ix16(mce,vatom1,vatom2);
         at = assignNew('V', mce, Ity_V128, binop(Iop_ShlN16x8, at, mkU8(8)));
         at = assignNew('V', mce, Ity_V128, binop(Iop_SarN16x8, at, mkU8(8)));
	 return at;
      }

      
      case Iop_MullEven32Ux4:
      case Iop_MullEven32Sx4: {
         IRAtom* at;
         at = binary32Ix4(mce,vatom1,vatom2);
         at = assignNew('V', mce, Ity_V128, binop(Iop_ShlN64x2, at, mkU8(32)));
         at = assignNew('V', mce, Ity_V128, binop(Iop_SarN64x2, at, mkU8(32)));
         return at;
      }

      case Iop_NarrowBin32to16x8: 
      case Iop_NarrowBin16to8x16: 
      case Iop_NarrowBin64to32x4:
         return assignNew('V', mce, Ity_V128, 
                                    binop(op, vatom1, vatom2));

      case Iop_ShrV128:
      case Iop_ShlV128:
         complainIfUndefined(mce, atom2, NULL);
         return assignNew('V', mce, Ity_V128, binop(op, vatom1, atom2));

      
      case Iop_SHA256:
      case Iop_SHA512:
         complainIfUndefined(mce, atom2, NULL);
         return assignNew('V', mce, Ity_V128, binop(op, vatom1, atom2));

      
      case Iop_64HLto128:
         return assignNew('V', mce, Ity_I128, binop(op, vatom1, vatom2));

      

      case Iop_Max64Fx4:
      case Iop_Min64Fx4:
         return binary64Fx4(mce, vatom1, vatom2);

      case Iop_Max32Fx8:
      case Iop_Min32Fx8:
         return binary32Fx8(mce, vatom1, vatom2);

      
      case Iop_V128HLtoV256:
         return assignNew('V', mce, Ity_V256, binop(op, vatom1, vatom2));

      

      case Iop_F32toI64S:
      case Iop_F32toI64U:
         
         return mkLazy2(mce, Ity_I64, vatom1, vatom2);

      case Iop_I64StoF32:
         
         return mkLazy2(mce, Ity_I32, vatom1, vatom2);

      case Iop_RoundF64toInt:
      case Iop_RoundF64toF32:
      case Iop_F64toI64S:
      case Iop_F64toI64U:
      case Iop_I64StoF64:
      case Iop_I64UtoF64:
      case Iop_SinF64:
      case Iop_CosF64:
      case Iop_TanF64:
      case Iop_2xm1F64:
      case Iop_SqrtF64:
      case Iop_RecpExpF64:
         
         return mkLazy2(mce, Ity_I64, vatom1, vatom2);

      case Iop_ShlD64:
      case Iop_ShrD64:
      case Iop_RoundD64toInt:
         
         return mkLazy2(mce, Ity_I64, vatom1, vatom2);

      case Iop_ShlD128:
      case Iop_ShrD128:
      case Iop_RoundD128toInt:
         
         return mkLazy2(mce, Ity_I128, vatom1, vatom2);

      case Iop_D64toI64S:
      case Iop_D64toI64U:
      case Iop_I64StoD64:
      case Iop_I64UtoD64:
         
         return mkLazy2(mce, Ity_I64, vatom1, vatom2);

      case Iop_F32toD32:
      case Iop_F64toD32:
      case Iop_F128toD32:
      case Iop_D32toF32:
      case Iop_D64toF32:
      case Iop_D128toF32:
         
         return mkLazy2(mce, Ity_I32, vatom1, vatom2);

      case Iop_F32toD64:
      case Iop_F64toD64:
      case Iop_F128toD64:
      case Iop_D32toF64:
      case Iop_D64toF64:
      case Iop_D128toF64:
         
         return mkLazy2(mce, Ity_I64, vatom1, vatom2);

      case Iop_F32toD128:
      case Iop_F64toD128:
      case Iop_F128toD128:
      case Iop_D32toF128:
      case Iop_D64toF128:
      case Iop_D128toF128:
         
         return mkLazy2(mce, Ity_I128, vatom1, vatom2);

      case Iop_RoundF32toInt:
      case Iop_SqrtF32:
      case Iop_RecpExpF32:
         
         return mkLazy2(mce, Ity_I32, vatom1, vatom2);

      case Iop_SqrtF128:
         
         return mkLazy2(mce, Ity_I128, vatom1, vatom2);

      case Iop_I32StoF32:
      case Iop_I32UtoF32:
      case Iop_F32toI32S:
      case Iop_F32toI32U:
         
         return mkLazy2(mce, Ity_I32, vatom1, vatom2);

      case Iop_F64toF16:
      case Iop_F32toF16:
         
         return mkLazy2(mce, Ity_I16, vatom1, vatom2);

      case Iop_F128toI32S: 
      case Iop_F128toI32U: 
      case Iop_F128toF32:  
      case Iop_D128toI32S: 
      case Iop_D128toI32U: 
         return mkLazy2(mce, Ity_I32, vatom1, vatom2);

      case Iop_F128toI64S: 
      case Iop_F128toI64U: 
      case Iop_F128toF64:  
      case Iop_D128toD64:  
      case Iop_D128toI64S: 
      case Iop_D128toI64U: 
         return mkLazy2(mce, Ity_I64, vatom1, vatom2);

      case Iop_F64HLtoF128:
      case Iop_D64HLtoD128:
         return assignNew('V', mce, Ity_I128,
                          binop(Iop_64HLto128, vatom1, vatom2));

      case Iop_F64toI32U:
      case Iop_F64toI32S:
      case Iop_F64toF32:
      case Iop_I64UtoF32:
      case Iop_D64toI32U:
      case Iop_D64toI32S:
         
         return mkLazy2(mce, Ity_I32, vatom1, vatom2);

      case Iop_D64toD32:
         
         return mkLazy2(mce, Ity_I32, vatom1, vatom2);

      case Iop_F64toI16S:
         
         return mkLazy2(mce, Ity_I16, vatom1, vatom2);

      case Iop_InsertExpD64:
         
         return mkLazy2(mce, Ity_I64, vatom1, vatom2);

      case Iop_InsertExpD128:
         
         return mkLazy2(mce, Ity_I128, vatom1, vatom2);

      case Iop_CmpF32:
      case Iop_CmpF64:
      case Iop_CmpF128:
      case Iop_CmpD64:
      case Iop_CmpD128:
      case Iop_CmpExpD64:
      case Iop_CmpExpD128:
         return mkLazy2(mce, Ity_I32, vatom1, vatom2);

      

      case Iop_DivModU64to32:
      case Iop_DivModS64to32:
         return mkLazy2(mce, Ity_I64, vatom1, vatom2);

      case Iop_DivModU128to64:
      case Iop_DivModS128to64:
         return mkLazy2(mce, Ity_I128, vatom1, vatom2);

      case Iop_8HLto16:
         return assignNew('V', mce, Ity_I16, binop(op, vatom1, vatom2));
      case Iop_16HLto32:
         return assignNew('V', mce, Ity_I32, binop(op, vatom1, vatom2));
      case Iop_32HLto64:
         return assignNew('V', mce, Ity_I64, binop(op, vatom1, vatom2));

      case Iop_DivModS64to64:
      case Iop_MullS64:
      case Iop_MullU64: {
         IRAtom* vLo64 = mkLeft64(mce, mkUifU64(mce, vatom1,vatom2));
         IRAtom* vHi64 = mkPCastTo(mce, Ity_I64, vLo64);
         return assignNew('V', mce, Ity_I128,
                          binop(Iop_64HLto128, vHi64, vLo64));
      }

      case Iop_MullS32:
      case Iop_MullU32: {
         IRAtom* vLo32 = mkLeft32(mce, mkUifU32(mce, vatom1,vatom2));
         IRAtom* vHi32 = mkPCastTo(mce, Ity_I32, vLo32);
         return assignNew('V', mce, Ity_I64,
                          binop(Iop_32HLto64, vHi32, vLo32));
      }

      case Iop_MullS16:
      case Iop_MullU16: {
         IRAtom* vLo16 = mkLeft16(mce, mkUifU16(mce, vatom1,vatom2));
         IRAtom* vHi16 = mkPCastTo(mce, Ity_I16, vLo16);
         return assignNew('V', mce, Ity_I32,
                          binop(Iop_16HLto32, vHi16, vLo16));
      }

      case Iop_MullS8:
      case Iop_MullU8: {
         IRAtom* vLo8 = mkLeft8(mce, mkUifU8(mce, vatom1,vatom2));
         IRAtom* vHi8 = mkPCastTo(mce, Ity_I8, vLo8);
         return assignNew('V', mce, Ity_I16, binop(Iop_8HLto16, vHi8, vLo8));
      }

      case Iop_Sad8Ux4: 
      case Iop_DivS32:
      case Iop_DivU32:
      case Iop_DivU32E:
      case Iop_DivS32E:
      case Iop_QAdd32S: 
      case Iop_QSub32S: 
         return mkLazy2(mce, Ity_I32, vatom1, vatom2);

      case Iop_DivS64:
      case Iop_DivU64:
      case Iop_DivS64E:
      case Iop_DivU64E:
         return mkLazy2(mce, Ity_I64, vatom1, vatom2);

      case Iop_Add32:
         if (mce->bogusLiterals || mce->useLLVMworkarounds)
            return expensiveAddSub(mce,True,Ity_I32, 
                                   vatom1,vatom2, atom1,atom2);
         else
            goto cheap_AddSub32;
      case Iop_Sub32:
         if (mce->bogusLiterals)
            return expensiveAddSub(mce,False,Ity_I32, 
                                   vatom1,vatom2, atom1,atom2);
         else
            goto cheap_AddSub32;

      cheap_AddSub32:
      case Iop_Mul32:
         return mkLeft32(mce, mkUifU32(mce, vatom1,vatom2));

      case Iop_CmpORD32S:
      case Iop_CmpORD32U:
      case Iop_CmpORD64S:
      case Iop_CmpORD64U:
         return doCmpORD(mce, op, vatom1,vatom2, atom1,atom2);

      case Iop_Add64:
         if (mce->bogusLiterals || mce->useLLVMworkarounds)
            return expensiveAddSub(mce,True,Ity_I64, 
                                   vatom1,vatom2, atom1,atom2);
         else
            goto cheap_AddSub64;
      case Iop_Sub64:
         if (mce->bogusLiterals)
            return expensiveAddSub(mce,False,Ity_I64, 
                                   vatom1,vatom2, atom1,atom2);
         else
            goto cheap_AddSub64;

      cheap_AddSub64:
      case Iop_Mul64:
         return mkLeft64(mce, mkUifU64(mce, vatom1,vatom2));

      case Iop_Mul16:
      case Iop_Add16:
      case Iop_Sub16:
         return mkLeft16(mce, mkUifU16(mce, vatom1,vatom2));

      case Iop_Mul8:
      case Iop_Sub8:
      case Iop_Add8:
         return mkLeft8(mce, mkUifU8(mce, vatom1,vatom2));

      case Iop_CmpEQ64: 
      case Iop_CmpNE64:
         if (mce->bogusLiterals)
            goto expensive_cmp64;
         else
            goto cheap_cmp64;

      expensive_cmp64:
      case Iop_ExpCmpNE64:
         return expensiveCmpEQorNE(mce,Ity_I64, vatom1,vatom2, atom1,atom2 );

      cheap_cmp64:
      case Iop_CmpLE64S: case Iop_CmpLE64U: 
      case Iop_CmpLT64U: case Iop_CmpLT64S:
         return mkPCastTo(mce, Ity_I1, mkUifU64(mce, vatom1,vatom2));

      case Iop_CmpEQ32: 
      case Iop_CmpNE32:
         if (mce->bogusLiterals)
            goto expensive_cmp32;
         else
            goto cheap_cmp32;

      expensive_cmp32:
      case Iop_ExpCmpNE32:
         return expensiveCmpEQorNE(mce,Ity_I32, vatom1,vatom2, atom1,atom2 );

      cheap_cmp32:
      case Iop_CmpLE32S: case Iop_CmpLE32U: 
      case Iop_CmpLT32U: case Iop_CmpLT32S:
         return mkPCastTo(mce, Ity_I1, mkUifU32(mce, vatom1,vatom2));

      case Iop_CmpEQ16: case Iop_CmpNE16:
         return mkPCastTo(mce, Ity_I1, mkUifU16(mce, vatom1,vatom2));

      case Iop_ExpCmpNE16:
         return expensiveCmpEQorNE(mce,Ity_I16, vatom1,vatom2, atom1,atom2 );

      case Iop_CmpEQ8: case Iop_CmpNE8:
         return mkPCastTo(mce, Ity_I1, mkUifU8(mce, vatom1,vatom2));

      case Iop_CasCmpEQ8:  case Iop_CasCmpNE8:
      case Iop_CasCmpEQ16: case Iop_CasCmpNE16:
      case Iop_CasCmpEQ32: case Iop_CasCmpNE32:
      case Iop_CasCmpEQ64: case Iop_CasCmpNE64:
         return assignNew('V', mce, Ity_I1, definedOfType(Ity_I1));

      case Iop_Shl64: case Iop_Shr64: case Iop_Sar64:
         return scalarShift( mce, Ity_I64, op, vatom1,vatom2, atom1,atom2 );

      case Iop_Shl32: case Iop_Shr32: case Iop_Sar32:
         return scalarShift( mce, Ity_I32, op, vatom1,vatom2, atom1,atom2 );

      case Iop_Shl16: case Iop_Shr16: case Iop_Sar16:
         return scalarShift( mce, Ity_I16, op, vatom1,vatom2, atom1,atom2 );

      case Iop_Shl8: case Iop_Shr8: case Iop_Sar8:
         return scalarShift( mce, Ity_I8, op, vatom1,vatom2, atom1,atom2 );

      case Iop_AndV256:
         uifu = mkUifUV256; difd = mkDifDV256; 
         and_or_ty = Ity_V256; improve = mkImproveANDV256; goto do_And_Or;
      case Iop_AndV128:
         uifu = mkUifUV128; difd = mkDifDV128; 
         and_or_ty = Ity_V128; improve = mkImproveANDV128; goto do_And_Or;
      case Iop_And64:
         uifu = mkUifU64; difd = mkDifD64; 
         and_or_ty = Ity_I64; improve = mkImproveAND64; goto do_And_Or;
      case Iop_And32:
         uifu = mkUifU32; difd = mkDifD32; 
         and_or_ty = Ity_I32; improve = mkImproveAND32; goto do_And_Or;
      case Iop_And16:
         uifu = mkUifU16; difd = mkDifD16; 
         and_or_ty = Ity_I16; improve = mkImproveAND16; goto do_And_Or;
      case Iop_And8:
         uifu = mkUifU8; difd = mkDifD8; 
         and_or_ty = Ity_I8; improve = mkImproveAND8; goto do_And_Or;

      case Iop_OrV256:
         uifu = mkUifUV256; difd = mkDifDV256; 
         and_or_ty = Ity_V256; improve = mkImproveORV256; goto do_And_Or;
      case Iop_OrV128:
         uifu = mkUifUV128; difd = mkDifDV128; 
         and_or_ty = Ity_V128; improve = mkImproveORV128; goto do_And_Or;
      case Iop_Or64:
         uifu = mkUifU64; difd = mkDifD64; 
         and_or_ty = Ity_I64; improve = mkImproveOR64; goto do_And_Or;
      case Iop_Or32:
         uifu = mkUifU32; difd = mkDifD32; 
         and_or_ty = Ity_I32; improve = mkImproveOR32; goto do_And_Or;
      case Iop_Or16:
         uifu = mkUifU16; difd = mkDifD16; 
         and_or_ty = Ity_I16; improve = mkImproveOR16; goto do_And_Or;
      case Iop_Or8:
         uifu = mkUifU8; difd = mkDifD8; 
         and_or_ty = Ity_I8; improve = mkImproveOR8; goto do_And_Or;

      do_And_Or:
         return
         assignNew(
            'V', mce, 
            and_or_ty,
            difd(mce, uifu(mce, vatom1, vatom2),
                      difd(mce, improve(mce, atom1, vatom1),
                                improve(mce, atom2, vatom2) ) ) );

      case Iop_Xor8:
         return mkUifU8(mce, vatom1, vatom2);
      case Iop_Xor16:
         return mkUifU16(mce, vatom1, vatom2);
      case Iop_Xor32:
         return mkUifU32(mce, vatom1, vatom2);
      case Iop_Xor64:
         return mkUifU64(mce, vatom1, vatom2);
      case Iop_XorV128:
         return mkUifUV128(mce, vatom1, vatom2);
      case Iop_XorV256:
         return mkUifUV256(mce, vatom1, vatom2);

      

      case Iop_ShrN16x16:
      case Iop_ShrN32x8:
      case Iop_ShrN64x4:
      case Iop_SarN16x16:
      case Iop_SarN32x8:
      case Iop_ShlN16x16:
      case Iop_ShlN32x8:
      case Iop_ShlN64x4:
         complainIfUndefined(mce, atom2, NULL);
         return assignNew('V', mce, Ity_V256, binop(op, vatom1, atom2));

      case Iop_QSub8Ux32:
      case Iop_QSub8Sx32:
      case Iop_Sub8x32:
      case Iop_Min8Ux32:
      case Iop_Min8Sx32:
      case Iop_Max8Ux32:
      case Iop_Max8Sx32:
      case Iop_CmpGT8Sx32:
      case Iop_CmpEQ8x32:
      case Iop_Avg8Ux32:
      case Iop_QAdd8Ux32:
      case Iop_QAdd8Sx32:
      case Iop_Add8x32:
         return binary8Ix32(mce, vatom1, vatom2);

      case Iop_QSub16Ux16:
      case Iop_QSub16Sx16:
      case Iop_Sub16x16:
      case Iop_Mul16x16:
      case Iop_MulHi16Sx16:
      case Iop_MulHi16Ux16:
      case Iop_Min16Sx16:
      case Iop_Min16Ux16:
      case Iop_Max16Sx16:
      case Iop_Max16Ux16:
      case Iop_CmpGT16Sx16:
      case Iop_CmpEQ16x16:
      case Iop_Avg16Ux16:
      case Iop_QAdd16Ux16:
      case Iop_QAdd16Sx16:
      case Iop_Add16x16:
         return binary16Ix16(mce, vatom1, vatom2);

      case Iop_Sub32x8:
      case Iop_CmpGT32Sx8:
      case Iop_CmpEQ32x8:
      case Iop_Add32x8:
      case Iop_Max32Ux8:
      case Iop_Max32Sx8:
      case Iop_Min32Ux8:
      case Iop_Min32Sx8:
      case Iop_Mul32x8:
         return binary32Ix8(mce, vatom1, vatom2);

      case Iop_Sub64x4:
      case Iop_Add64x4:
      case Iop_CmpEQ64x4:
      case Iop_CmpGT64Sx4:
         return binary64Ix4(mce, vatom1, vatom2);

      case Iop_Perm32x8:
         return mkUifUV256(
                   mce,
                   assignNew('V', mce, Ity_V256, binop(op, vatom1, atom2)),
                   mkPCast32x8(mce, vatom2)
                );

      case Iop_QandSQsh64x2:  case Iop_QandUQsh64x2:
      case Iop_QandSQRsh64x2: case Iop_QandUQRsh64x2:
      case Iop_QandSQsh32x4:  case Iop_QandUQsh32x4:
      case Iop_QandSQRsh32x4: case Iop_QandUQRsh32x4:
      case Iop_QandSQsh16x8:  case Iop_QandUQsh16x8:
      case Iop_QandSQRsh16x8: case Iop_QandUQRsh16x8:
      case Iop_QandSQsh8x16:  case Iop_QandUQsh8x16:
      case Iop_QandSQRsh8x16: case Iop_QandUQRsh8x16:
      {
         
         IRAtom* (*binaryNIxM)(MCEnv*,IRAtom*,IRAtom*) = NULL;
         switch (op) {
            case Iop_QandSQsh64x2:
            case Iop_QandUQsh64x2:
            case Iop_QandSQRsh64x2:
            case Iop_QandUQRsh64x2:
               binaryNIxM = binary64Ix2;
               break;
            case Iop_QandSQsh32x4:
            case Iop_QandUQsh32x4:
            case Iop_QandSQRsh32x4:
            case Iop_QandUQRsh32x4:
               binaryNIxM = binary32Ix4;
               break;
            case Iop_QandSQsh16x8:
            case Iop_QandUQsh16x8:
            case Iop_QandSQRsh16x8:
            case Iop_QandUQRsh16x8:
               binaryNIxM = binary16Ix8;
               break;
            case Iop_QandSQsh8x16:
            case Iop_QandUQsh8x16:
            case Iop_QandSQRsh8x16:
            case Iop_QandUQRsh8x16:
               binaryNIxM = binary8Ix16;
               break;
            default:
               tl_assert(0);
         }
         tl_assert(binaryNIxM);
         
         IRAtom* shV = binaryNIxM(mce, vatom1, vatom2);
         
         IRAtom* qV = mkPCastXXtoXXlsb(mce, shV, Ity_V128);
         
         return assignNew('V', mce, Ity_V256,
                          binop(Iop_V128HLtoV256, qV, shV));
      }

      default:
         ppIROp(op);
         VG_(tool_panic)("memcheck:expr2vbits_Binop");
   }
}


static 
IRExpr* expr2vbits_Unop ( MCEnv* mce, IROp op, IRAtom* atom )
{
   IRAtom* vatom = expr2vbits( mce, atom );
   tl_assert(isOriginalAtom(mce,atom));
   switch (op) {

      case Iop_Abs64Fx2:
      case Iop_Neg64Fx2:
      case Iop_RSqrtEst64Fx2:
      case Iop_RecipEst64Fx2:
         return unary64Fx2(mce, vatom);

      case Iop_Sqrt64F0x2:
         return unary64F0x2(mce, vatom);

      case Iop_Sqrt32Fx8:
      case Iop_RSqrtEst32Fx8:
      case Iop_RecipEst32Fx8:
         return unary32Fx8(mce, vatom);

      case Iop_Sqrt64Fx4:
         return unary64Fx4(mce, vatom);

      case Iop_RecipEst32Fx4:
      case Iop_I32UtoFx4:
      case Iop_I32StoFx4:
      case Iop_QFtoI32Ux4_RZ:
      case Iop_QFtoI32Sx4_RZ:
      case Iop_RoundF32x4_RM:
      case Iop_RoundF32x4_RP:
      case Iop_RoundF32x4_RN:
      case Iop_RoundF32x4_RZ:
      case Iop_RecipEst32Ux4:
      case Iop_Abs32Fx4:
      case Iop_Neg32Fx4:
      case Iop_RSqrtEst32Fx4:
         return unary32Fx4(mce, vatom);

      case Iop_I32UtoFx2:
      case Iop_I32StoFx2:
      case Iop_RecipEst32Fx2:
      case Iop_RecipEst32Ux2:
      case Iop_Abs32Fx2:
      case Iop_Neg32Fx2:
      case Iop_RSqrtEst32Fx2:
         return unary32Fx2(mce, vatom);

      case Iop_Sqrt32F0x4:
      case Iop_RSqrtEst32F0x4:
      case Iop_RecipEst32F0x4:
         return unary32F0x4(mce, vatom);

      case Iop_32UtoV128:
      case Iop_64UtoV128:
      case Iop_Dup8x16:
      case Iop_Dup16x8:
      case Iop_Dup32x4:
      case Iop_Reverse1sIn8_x16:
      case Iop_Reverse8sIn16_x8:
      case Iop_Reverse8sIn32_x4:
      case Iop_Reverse16sIn32_x4:
      case Iop_Reverse8sIn64_x2:
      case Iop_Reverse16sIn64_x2:
      case Iop_Reverse32sIn64_x2:
      case Iop_V256toV128_1: case Iop_V256toV128_0:
      case Iop_ZeroHI64ofV128:
      case Iop_ZeroHI96ofV128:
      case Iop_ZeroHI112ofV128:
      case Iop_ZeroHI120ofV128:
         return assignNew('V', mce, Ity_V128, unop(op, vatom));

      case Iop_F128HItoF64:  
      case Iop_D128HItoD64:  
         return assignNew('V', mce, Ity_I64, unop(Iop_128HIto64, vatom));
      case Iop_F128LOtoF64:  
      case Iop_D128LOtoD64:  
         return assignNew('V', mce, Ity_I64, unop(Iop_128to64, vatom));

      case Iop_NegF128:
      case Iop_AbsF128:
         return mkPCastTo(mce, Ity_I128, vatom);

      case Iop_I32StoF128: 
      case Iop_I64StoF128: 
      case Iop_I32UtoF128: 
      case Iop_I64UtoF128: 
      case Iop_F32toF128:  
      case Iop_F64toF128:  
      case Iop_I32StoD128: 
      case Iop_I64StoD128: 
      case Iop_I32UtoD128: 
      case Iop_I64UtoD128: 
         return mkPCastTo(mce, Ity_I128, vatom);

      case Iop_F16toF64:
      case Iop_F32toF64: 
      case Iop_I32StoF64:
      case Iop_I32UtoF64:
      case Iop_NegF64:
      case Iop_AbsF64:
      case Iop_RSqrtEst5GoodF64:
      case Iop_RoundF64toF64_NEAREST:
      case Iop_RoundF64toF64_NegINF:
      case Iop_RoundF64toF64_PosINF:
      case Iop_RoundF64toF64_ZERO:
      case Iop_Clz64:
      case Iop_D32toD64:
      case Iop_I32StoD64:
      case Iop_I32UtoD64:
      case Iop_ExtractExpD64:    
      case Iop_ExtractExpD128:   
      case Iop_ExtractSigD64:    
      case Iop_ExtractSigD128:   
      case Iop_DPBtoBCD:
      case Iop_BCDtoDPB:
         return mkPCastTo(mce, Ity_I64, vatom);

      case Iop_D64toD128:
         return mkPCastTo(mce, Ity_I128, vatom);

      case Iop_Clz32:
      case Iop_TruncF64asF32:
      case Iop_NegF32:
      case Iop_AbsF32:
      case Iop_F16toF32: 
         return mkPCastTo(mce, Ity_I32, vatom);

      case Iop_Ctz32:
      case Iop_Ctz64:
         return expensiveCountTrailingZeroes(mce, op, atom, vatom);

      case Iop_1Uto64:
      case Iop_1Sto64:
      case Iop_8Uto64:
      case Iop_8Sto64:
      case Iop_16Uto64:
      case Iop_16Sto64:
      case Iop_32Sto64:
      case Iop_32Uto64:
      case Iop_V128to64:
      case Iop_V128HIto64:
      case Iop_128HIto64:
      case Iop_128to64:
      case Iop_Dup8x8:
      case Iop_Dup16x4:
      case Iop_Dup32x2:
      case Iop_Reverse8sIn16_x4:
      case Iop_Reverse8sIn32_x2:
      case Iop_Reverse16sIn32_x2:
      case Iop_Reverse8sIn64_x1:
      case Iop_Reverse16sIn64_x1:
      case Iop_Reverse32sIn64_x1:
      case Iop_V256to64_0: case Iop_V256to64_1:
      case Iop_V256to64_2: case Iop_V256to64_3:
         return assignNew('V', mce, Ity_I64, unop(op, vatom));

      case Iop_64to32:
      case Iop_64HIto32:
      case Iop_1Uto32:
      case Iop_1Sto32:
      case Iop_8Uto32:
      case Iop_16Uto32:
      case Iop_16Sto32:
      case Iop_8Sto32:
      case Iop_V128to32:
         return assignNew('V', mce, Ity_I32, unop(op, vatom));

      case Iop_8Sto16:
      case Iop_8Uto16:
      case Iop_32to16:
      case Iop_32HIto16:
      case Iop_64to16:
      case Iop_GetMSBs8x16:
         return assignNew('V', mce, Ity_I16, unop(op, vatom));

      case Iop_1Uto8:
      case Iop_1Sto8:
      case Iop_16to8:
      case Iop_16HIto8:
      case Iop_32to8:
      case Iop_64to8:
      case Iop_GetMSBs8x8:
         return assignNew('V', mce, Ity_I8, unop(op, vatom));

      case Iop_32to1:
         return assignNew('V', mce, Ity_I1, unop(Iop_32to1, vatom));

      case Iop_64to1:
         return assignNew('V', mce, Ity_I1, unop(Iop_64to1, vatom));

      case Iop_ReinterpF64asI64:
      case Iop_ReinterpI64asF64:
      case Iop_ReinterpI32asF32:
      case Iop_ReinterpF32asI32:
      case Iop_ReinterpI64asD64:
      case Iop_ReinterpD64asI64:
      case Iop_NotV256:
      case Iop_NotV128:
      case Iop_Not64:
      case Iop_Not32:
      case Iop_Not16:
      case Iop_Not8:
      case Iop_Not1:
         return vatom;

      case Iop_CmpNEZ8x8:
      case Iop_Cnt8x8:
      case Iop_Clz8x8:
      case Iop_Cls8x8:
      case Iop_Abs8x8:
         return mkPCast8x8(mce, vatom);

      case Iop_CmpNEZ8x16:
      case Iop_Cnt8x16:
      case Iop_Clz8x16:
      case Iop_Cls8x16:
      case Iop_Abs8x16:
         return mkPCast8x16(mce, vatom);

      case Iop_CmpNEZ16x4:
      case Iop_Clz16x4:
      case Iop_Cls16x4:
      case Iop_Abs16x4:
         return mkPCast16x4(mce, vatom);

      case Iop_CmpNEZ16x8:
      case Iop_Clz16x8:
      case Iop_Cls16x8:
      case Iop_Abs16x8:
         return mkPCast16x8(mce, vatom);

      case Iop_CmpNEZ32x2:
      case Iop_Clz32x2:
      case Iop_Cls32x2:
      case Iop_FtoI32Ux2_RZ:
      case Iop_FtoI32Sx2_RZ:
      case Iop_Abs32x2:
         return mkPCast32x2(mce, vatom);

      case Iop_CmpNEZ32x4:
      case Iop_Clz32x4:
      case Iop_Cls32x4:
      case Iop_FtoI32Ux4_RZ:
      case Iop_FtoI32Sx4_RZ:
      case Iop_Abs32x4:
      case Iop_RSqrtEst32Ux4:
         return mkPCast32x4(mce, vatom);

      case Iop_CmpwNEZ32:
         return mkPCastTo(mce, Ity_I32, vatom);

      case Iop_CmpwNEZ64:
         return mkPCastTo(mce, Ity_I64, vatom);

      case Iop_CmpNEZ64x2:
      case Iop_CipherSV128:
      case Iop_Clz64x2:
      case Iop_Abs64x2:
         return mkPCast64x2(mce, vatom);

      case Iop_PwBitMtxXpose64x2:
         return assignNew('V', mce, Ity_V128, unop(op, vatom));

      case Iop_NarrowUn16to8x8:
      case Iop_NarrowUn32to16x4:
      case Iop_NarrowUn64to32x2:
      case Iop_QNarrowUn16Sto8Sx8:
      case Iop_QNarrowUn16Sto8Ux8:
      case Iop_QNarrowUn16Uto8Ux8:
      case Iop_QNarrowUn32Sto16Sx4:
      case Iop_QNarrowUn32Sto16Ux4:
      case Iop_QNarrowUn32Uto16Ux4:
      case Iop_QNarrowUn64Sto32Sx2:
      case Iop_QNarrowUn64Sto32Ux2:
      case Iop_QNarrowUn64Uto32Ux2:
         return vectorNarrowUnV128(mce, op, vatom);

      case Iop_Widen8Sto16x8:
      case Iop_Widen8Uto16x8:
      case Iop_Widen16Sto32x4:
      case Iop_Widen16Uto32x4:
      case Iop_Widen32Sto64x2:
      case Iop_Widen32Uto64x2:
         return vectorWidenI64(mce, op, vatom);

      case Iop_PwAddL32Ux2:
      case Iop_PwAddL32Sx2:
         return mkPCastTo(mce, Ity_I64,
               assignNew('V', mce, Ity_I64, unop(op, mkPCast32x2(mce, vatom))));

      case Iop_PwAddL16Ux4:
      case Iop_PwAddL16Sx4:
         return mkPCast32x2(mce,
               assignNew('V', mce, Ity_I64, unop(op, mkPCast16x4(mce, vatom))));

      case Iop_PwAddL8Ux8:
      case Iop_PwAddL8Sx8:
         return mkPCast16x4(mce,
               assignNew('V', mce, Ity_I64, unop(op, mkPCast8x8(mce, vatom))));

      case Iop_PwAddL32Ux4:
      case Iop_PwAddL32Sx4:
         return mkPCast64x2(mce,
               assignNew('V', mce, Ity_V128, unop(op, mkPCast32x4(mce, vatom))));

      case Iop_PwAddL16Ux8:
      case Iop_PwAddL16Sx8:
         return mkPCast32x4(mce,
               assignNew('V', mce, Ity_V128, unop(op, mkPCast16x8(mce, vatom))));

      case Iop_PwAddL8Ux16:
      case Iop_PwAddL8Sx16:
         return mkPCast16x8(mce,
               assignNew('V', mce, Ity_V128, unop(op, mkPCast8x16(mce, vatom))));

      case Iop_I64UtoF32:
      default:
         ppIROp(op);
         VG_(tool_panic)("memcheck:expr2vbits_Unop");
   }
}


static
IRAtom* expr2vbits_Load_WRK ( MCEnv* mce,
                              IREndness end, IRType ty,
                              IRAtom* addr, UInt bias, IRAtom* guard )
{
   tl_assert(isOriginalAtom(mce,addr));
   tl_assert(end == Iend_LE || end == Iend_BE);

   complainIfUndefined( mce, addr, guard );

   ty = shadowTypeV(ty);

   void*        helper           = NULL;
   const HChar* hname            = NULL;
   Bool         ret_via_outparam = False;

   if (end == Iend_LE) {
      switch (ty) {
         case Ity_V256: helper = &MC_(helperc_LOADV256le);
                        hname = "MC_(helperc_LOADV256le)";
                        ret_via_outparam = True;
                        break;
         case Ity_V128: helper = &MC_(helperc_LOADV128le);
                        hname = "MC_(helperc_LOADV128le)";
                        ret_via_outparam = True;
                        break;
         case Ity_I64:  helper = &MC_(helperc_LOADV64le);
                        hname = "MC_(helperc_LOADV64le)";
                        break;
         case Ity_I32:  helper = &MC_(helperc_LOADV32le);
                        hname = "MC_(helperc_LOADV32le)";
                        break;
         case Ity_I16:  helper = &MC_(helperc_LOADV16le);
                        hname = "MC_(helperc_LOADV16le)";
                        break;
         case Ity_I8:   helper = &MC_(helperc_LOADV8);
                        hname = "MC_(helperc_LOADV8)";
                        break;
         default:       ppIRType(ty);
                        VG_(tool_panic)("memcheck:expr2vbits_Load_WRK(LE)");
      }
   } else {
      switch (ty) {
         case Ity_V256: helper = &MC_(helperc_LOADV256be);
                        hname = "MC_(helperc_LOADV256be)";
                        ret_via_outparam = True;
                        break;
         case Ity_V128: helper = &MC_(helperc_LOADV128be);
                        hname = "MC_(helperc_LOADV128be)";
                        ret_via_outparam = True;
                        break;
         case Ity_I64:  helper = &MC_(helperc_LOADV64be);
                        hname = "MC_(helperc_LOADV64be)";
                        break;
         case Ity_I32:  helper = &MC_(helperc_LOADV32be);
                        hname = "MC_(helperc_LOADV32be)";
                        break;
         case Ity_I16:  helper = &MC_(helperc_LOADV16be);
                        hname = "MC_(helperc_LOADV16be)";
                        break;
         case Ity_I8:   helper = &MC_(helperc_LOADV8);
                        hname = "MC_(helperc_LOADV8)";
                        break;
         default:       ppIRType(ty);
                        VG_(tool_panic)("memcheck:expr2vbits_Load_WRK(BE)");
      }
   }

   tl_assert(helper);
   tl_assert(hname);

   
   IRAtom* addrAct;
   if (bias == 0) {
      addrAct = addr;
   } else {
      IROp    mkAdd;
      IRAtom* eBias;
      IRType  tyAddr  = mce->hWordTy;
      tl_assert( tyAddr == Ity_I32 || tyAddr == Ity_I64 );
      mkAdd   = tyAddr==Ity_I32 ? Iop_Add32 : Iop_Add64;
      eBias   = tyAddr==Ity_I32 ? mkU32(bias) : mkU64(bias);
      addrAct = assignNew('V', mce, tyAddr, binop(mkAdd, addr, eBias) );
   }

   IRTemp datavbits = newTemp(mce, ty, VSh);

   
   IRDirty* di;
   if (ret_via_outparam) {
      di = unsafeIRDirty_1_N( datavbits, 
                              2, 
                              hname, VG_(fnptr_to_fnentry)( helper ), 
                              mkIRExprVec_2( IRExpr_VECRET(), addrAct ) );
   } else {
      di = unsafeIRDirty_1_N( datavbits, 
                              1, 
                              hname, VG_(fnptr_to_fnentry)( helper ), 
                              mkIRExprVec_1( addrAct ) );
   }

   setHelperAnns( mce, di );
   if (guard) {
      di->guard = guard;
   }
   stmt( 'V', mce, IRStmt_Dirty(di) );

   return mkexpr(datavbits);
}


static
IRAtom* expr2vbits_Load ( MCEnv* mce,
                          IREndness end, IRType ty,
                          IRAtom* addr, UInt bias,
                          IRAtom* guard )
{
   tl_assert(end == Iend_LE || end == Iend_BE);
   switch (shadowTypeV(ty)) {
      case Ity_I8:
      case Ity_I16:
      case Ity_I32:
      case Ity_I64:
      case Ity_V128:
      case Ity_V256:
         return expr2vbits_Load_WRK(mce, end, ty, addr, bias, guard);
      default:
         VG_(tool_panic)("expr2vbits_Load");
   }
}


static
IRAtom* expr2vbits_Load_guarded_General ( MCEnv* mce,
                                          IREndness end, IRType ty,
                                          IRAtom* addr, UInt bias,
                                          IRAtom* guard,
                                          IROp vwiden, IRAtom* valt )
{
   
   IRType tyWide = Ity_INVALID;
   switch (vwiden) {
      case Iop_INVALID:
         tyWide = ty;
         break;
      case Iop_16Uto32: case Iop_16Sto32: case Iop_8Uto32: case Iop_8Sto32:
         tyWide = Ity_I32; 
         break;
      default:
         VG_(tool_panic)("memcheck:expr2vbits_Load_guarded_General");
   }

   IRAtom* iftrue1
      = assignNew('V', mce, ty,
                  expr2vbits_Load(mce, end, ty, addr, bias, guard));
   IRAtom* iftrue2
      = vwiden == Iop_INVALID
           ? iftrue1
           : assignNew('V', mce, tyWide, unop(vwiden, iftrue1));
   IRAtom* iffalse 
      = valt;
   IRAtom* cond
      = guard == NULL  ? mkU1(1)  : guard;
   
   return assignNew('V', mce, tyWide, IRExpr_ITE(cond, iftrue2, iffalse));
}


static
IRAtom* expr2vbits_Load_guarded_Simple ( MCEnv* mce, 
                                         IREndness end, IRType ty, 
                                         IRAtom* addr, UInt bias,
                                         IRAtom *guard )
{
   return expr2vbits_Load_guarded_General(
             mce, end, ty, addr, bias, guard, Iop_INVALID, definedOfType(ty)
          );
}


static
IRAtom* expr2vbits_ITE ( MCEnv* mce, 
                         IRAtom* cond, IRAtom* iftrue, IRAtom* iffalse )
{
   IRAtom *vbitsC, *vbits0, *vbits1;
   IRType ty;
   tl_assert(isOriginalAtom(mce, cond));
   tl_assert(isOriginalAtom(mce, iftrue));
   tl_assert(isOriginalAtom(mce, iffalse));

   vbitsC = expr2vbits(mce, cond);
   vbits1 = expr2vbits(mce, iftrue);
   vbits0 = expr2vbits(mce, iffalse);
   ty = typeOfIRExpr(mce->sb->tyenv, vbits0);

   return
      mkUifU(mce, ty, assignNew('V', mce, ty, 
                                     IRExpr_ITE(cond, vbits1, vbits0)),
                      mkPCastTo(mce, ty, vbitsC) );
}      


static
IRExpr* expr2vbits ( MCEnv* mce, IRExpr* e )
{
   switch (e->tag) {

      case Iex_Get:
         return shadow_GET( mce, e->Iex.Get.offset, e->Iex.Get.ty );

      case Iex_GetI:
         return shadow_GETI( mce, e->Iex.GetI.descr, 
                                  e->Iex.GetI.ix, e->Iex.GetI.bias );

      case Iex_RdTmp:
         return IRExpr_RdTmp( findShadowTmpV(mce, e->Iex.RdTmp.tmp) );

      case Iex_Const:
         return definedOfType(shadowTypeV(typeOfIRExpr(mce->sb->tyenv, e)));

      case Iex_Qop:
         return expr2vbits_Qop(
                   mce,
                   e->Iex.Qop.details->op,
                   e->Iex.Qop.details->arg1, e->Iex.Qop.details->arg2,
                   e->Iex.Qop.details->arg3, e->Iex.Qop.details->arg4
                );

      case Iex_Triop:
         return expr2vbits_Triop(
                   mce,
                   e->Iex.Triop.details->op,
                   e->Iex.Triop.details->arg1, e->Iex.Triop.details->arg2,
                   e->Iex.Triop.details->arg3
                );

      case Iex_Binop:
         return expr2vbits_Binop(
                   mce,
                   e->Iex.Binop.op,
                   e->Iex.Binop.arg1, e->Iex.Binop.arg2
                );

      case Iex_Unop:
         return expr2vbits_Unop( mce, e->Iex.Unop.op, e->Iex.Unop.arg );

      case Iex_Load:
         return expr2vbits_Load( mce, e->Iex.Load.end,
                                      e->Iex.Load.ty, 
                                      e->Iex.Load.addr, 0, 
                                      NULL );

      case Iex_CCall:
         return mkLazyN( mce, e->Iex.CCall.args, 
                              e->Iex.CCall.retty,
                              e->Iex.CCall.cee );

      case Iex_ITE:
         return expr2vbits_ITE( mce, e->Iex.ITE.cond, e->Iex.ITE.iftrue, 
                                     e->Iex.ITE.iffalse);

      default: 
         VG_(printf)("\n");
         ppIRExpr(e);
         VG_(printf)("\n");
         VG_(tool_panic)("memcheck: expr2vbits");
   }
}



static
IRExpr* zwidenToHostWord ( MCEnv* mce, IRAtom* vatom )
{
   IRType ty, tyH;

   
   tl_assert(isShadowAtom(mce,vatom));

   ty  = typeOfIRExpr(mce->sb->tyenv, vatom);
   tyH = mce->hWordTy;

   if (tyH == Ity_I32) {
      switch (ty) {
         case Ity_I32:
            return vatom;
         case Ity_I16:
            return assignNew('V', mce, tyH, unop(Iop_16Uto32, vatom));
         case Ity_I8:
            return assignNew('V', mce, tyH, unop(Iop_8Uto32, vatom));
         default:
            goto unhandled;
      }
   } else
   if (tyH == Ity_I64) {
      switch (ty) {
         case Ity_I32:
            return assignNew('V', mce, tyH, unop(Iop_32Uto64, vatom));
         case Ity_I16:
            return assignNew('V', mce, tyH, unop(Iop_32Uto64, 
                   assignNew('V', mce, Ity_I32, unop(Iop_16Uto32, vatom))));
         case Ity_I8:
            return assignNew('V', mce, tyH, unop(Iop_32Uto64, 
                   assignNew('V', mce, Ity_I32, unop(Iop_8Uto32, vatom))));
         default:
            goto unhandled;
      }
   } else {
      goto unhandled;
   }
  unhandled:
   VG_(printf)("\nty = "); ppIRType(ty); VG_(printf)("\n");
   VG_(tool_panic)("zwidenToHostWord");
}


static 
void do_shadow_Store ( MCEnv* mce, 
                       IREndness end,
                       IRAtom* addr, UInt bias,
                       IRAtom* data, IRAtom* vdata,
                       IRAtom* guard )
{
   IROp     mkAdd;
   IRType   ty, tyAddr;
   void*    helper = NULL;
   const HChar* hname = NULL;
   IRConst* c;

   tyAddr = mce->hWordTy;
   mkAdd  = tyAddr==Ity_I32 ? Iop_Add32 : Iop_Add64;
   tl_assert( tyAddr == Ity_I32 || tyAddr == Ity_I64 );
   tl_assert( end == Iend_LE || end == Iend_BE );

   if (data) {
      tl_assert(!vdata);
      tl_assert(isOriginalAtom(mce, data));
      tl_assert(bias == 0);
      vdata = expr2vbits( mce, data );
   } else {
      tl_assert(vdata);
   }

   tl_assert(isOriginalAtom(mce,addr));
   tl_assert(isShadowAtom(mce,vdata));

   if (guard) {
      tl_assert(isOriginalAtom(mce, guard));
      tl_assert(typeOfIRExpr(mce->sb->tyenv, guard) == Ity_I1);
   }

   ty = typeOfIRExpr(mce->sb->tyenv, vdata);

   
   
   
   if (MC_(clo_mc_level) == 1) {
      switch (ty) {
         case Ity_V256: 
                        c = IRConst_V256(V_BITS32_DEFINED); break;
         case Ity_V128: 
                        c = IRConst_V128(V_BITS16_DEFINED); break;
         case Ity_I64:  c = IRConst_U64 (V_BITS64_DEFINED); break;
         case Ity_I32:  c = IRConst_U32 (V_BITS32_DEFINED); break;
         case Ity_I16:  c = IRConst_U16 (V_BITS16_DEFINED); break;
         case Ity_I8:   c = IRConst_U8  (V_BITS8_DEFINED);  break;
         default:       VG_(tool_panic)("memcheck:do_shadow_Store(LE)");
      }
      vdata = IRExpr_Const( c );
   }

   complainIfUndefined( mce, addr, guard );

   if (end == Iend_LE) {
      switch (ty) {
         case Ity_V256: 
         case Ity_V128: 
         case Ity_I64: helper = &MC_(helperc_STOREV64le);
                       hname = "MC_(helperc_STOREV64le)";
                       break;
         case Ity_I32: helper = &MC_(helperc_STOREV32le);
                       hname = "MC_(helperc_STOREV32le)";
                       break;
         case Ity_I16: helper = &MC_(helperc_STOREV16le);
                       hname = "MC_(helperc_STOREV16le)";
                       break;
         case Ity_I8:  helper = &MC_(helperc_STOREV8);
                       hname = "MC_(helperc_STOREV8)";
                       break;
         default:      VG_(tool_panic)("memcheck:do_shadow_Store(LE)");
      }
   } else {
      switch (ty) {
         case Ity_V128: 
         case Ity_I64: helper = &MC_(helperc_STOREV64be);
                       hname = "MC_(helperc_STOREV64be)";
                       break;
         case Ity_I32: helper = &MC_(helperc_STOREV32be);
                       hname = "MC_(helperc_STOREV32be)";
                       break;
         case Ity_I16: helper = &MC_(helperc_STOREV16be);
                       hname = "MC_(helperc_STOREV16be)";
                       break;
         case Ity_I8:  helper = &MC_(helperc_STOREV8);
                       hname = "MC_(helperc_STOREV8)";
                       break;
         default:      VG_(tool_panic)("memcheck:do_shadow_Store(BE)");
      }
   }

   if (UNLIKELY(ty == Ity_V256)) {

      
      Int     offQ0, offQ1, offQ2, offQ3;

      
      IRDirty *diQ0,    *diQ1,    *diQ2,    *diQ3;
      IRAtom  *addrQ0,  *addrQ1,  *addrQ2,  *addrQ3;
      IRAtom  *vdataQ0, *vdataQ1, *vdataQ2, *vdataQ3;
      IRAtom  *eBiasQ0, *eBiasQ1, *eBiasQ2, *eBiasQ3;

      if (end == Iend_LE) {
         offQ0 = 0; offQ1 = 8; offQ2 = 16; offQ3 = 24;
      } else {
         offQ3 = 0; offQ2 = 8; offQ1 = 16; offQ0 = 24;
      }

      eBiasQ0 = tyAddr==Ity_I32 ? mkU32(bias+offQ0) : mkU64(bias+offQ0);
      addrQ0  = assignNew('V', mce, tyAddr, binop(mkAdd, addr, eBiasQ0) );
      vdataQ0 = assignNew('V', mce, Ity_I64, unop(Iop_V256to64_0, vdata));
      diQ0    = unsafeIRDirty_0_N( 
                   1, 
                   hname, VG_(fnptr_to_fnentry)( helper ), 
                   mkIRExprVec_2( addrQ0, vdataQ0 )
                );

      eBiasQ1 = tyAddr==Ity_I32 ? mkU32(bias+offQ1) : mkU64(bias+offQ1);
      addrQ1  = assignNew('V', mce, tyAddr, binop(mkAdd, addr, eBiasQ1) );
      vdataQ1 = assignNew('V', mce, Ity_I64, unop(Iop_V256to64_1, vdata));
      diQ1    = unsafeIRDirty_0_N( 
                   1, 
                   hname, VG_(fnptr_to_fnentry)( helper ), 
                   mkIRExprVec_2( addrQ1, vdataQ1 )
                );

      eBiasQ2 = tyAddr==Ity_I32 ? mkU32(bias+offQ2) : mkU64(bias+offQ2);
      addrQ2  = assignNew('V', mce, tyAddr, binop(mkAdd, addr, eBiasQ2) );
      vdataQ2 = assignNew('V', mce, Ity_I64, unop(Iop_V256to64_2, vdata));
      diQ2    = unsafeIRDirty_0_N( 
                   1, 
                   hname, VG_(fnptr_to_fnentry)( helper ), 
                   mkIRExprVec_2( addrQ2, vdataQ2 )
                );

      eBiasQ3 = tyAddr==Ity_I32 ? mkU32(bias+offQ3) : mkU64(bias+offQ3);
      addrQ3  = assignNew('V', mce, tyAddr, binop(mkAdd, addr, eBiasQ3) );
      vdataQ3 = assignNew('V', mce, Ity_I64, unop(Iop_V256to64_3, vdata));
      diQ3    = unsafeIRDirty_0_N( 
                   1, 
                   hname, VG_(fnptr_to_fnentry)( helper ), 
                   mkIRExprVec_2( addrQ3, vdataQ3 )
                );

      if (guard)
         diQ0->guard = diQ1->guard = diQ2->guard = diQ3->guard = guard;

      setHelperAnns( mce, diQ0 );
      setHelperAnns( mce, diQ1 );
      setHelperAnns( mce, diQ2 );
      setHelperAnns( mce, diQ3 );
      stmt( 'V', mce, IRStmt_Dirty(diQ0) );
      stmt( 'V', mce, IRStmt_Dirty(diQ1) );
      stmt( 'V', mce, IRStmt_Dirty(diQ2) );
      stmt( 'V', mce, IRStmt_Dirty(diQ3) );

   } 
   else if (UNLIKELY(ty == Ity_V128)) {

      
      
      

      Int     offLo64, offHi64;
      IRDirty *diLo64, *diHi64;
      IRAtom  *addrLo64, *addrHi64;
      IRAtom  *vdataLo64, *vdataHi64;
      IRAtom  *eBiasLo64, *eBiasHi64;

      if (end == Iend_LE) {
         offLo64 = 0;
         offHi64 = 8;
      } else {
         offLo64 = 8;
         offHi64 = 0;
      }

      eBiasLo64 = tyAddr==Ity_I32 ? mkU32(bias+offLo64) : mkU64(bias+offLo64);
      addrLo64  = assignNew('V', mce, tyAddr, binop(mkAdd, addr, eBiasLo64) );
      vdataLo64 = assignNew('V', mce, Ity_I64, unop(Iop_V128to64, vdata));
      diLo64    = unsafeIRDirty_0_N( 
                     1, 
                     hname, VG_(fnptr_to_fnentry)( helper ), 
                     mkIRExprVec_2( addrLo64, vdataLo64 )
                  );
      eBiasHi64 = tyAddr==Ity_I32 ? mkU32(bias+offHi64) : mkU64(bias+offHi64);
      addrHi64  = assignNew('V', mce, tyAddr, binop(mkAdd, addr, eBiasHi64) );
      vdataHi64 = assignNew('V', mce, Ity_I64, unop(Iop_V128HIto64, vdata));
      diHi64    = unsafeIRDirty_0_N( 
                     1, 
                     hname, VG_(fnptr_to_fnentry)( helper ), 
                     mkIRExprVec_2( addrHi64, vdataHi64 )
                  );
      if (guard) diLo64->guard = guard;
      if (guard) diHi64->guard = guard;
      setHelperAnns( mce, diLo64 );
      setHelperAnns( mce, diHi64 );
      stmt( 'V', mce, IRStmt_Dirty(diLo64) );
      stmt( 'V', mce, IRStmt_Dirty(diHi64) );

   } else {

      IRDirty *di;
      IRAtom  *addrAct;

      
      
      if (bias == 0) {
         addrAct = addr;
      } else {
         IRAtom* eBias   = tyAddr==Ity_I32 ? mkU32(bias) : mkU64(bias);
         addrAct = assignNew('V', mce, tyAddr, binop(mkAdd, addr, eBias));
      }

      if (ty == Ity_I64) {
         di = unsafeIRDirty_0_N( 
                 1, 
                 hname, VG_(fnptr_to_fnentry)( helper ), 
                 mkIRExprVec_2( addrAct, vdata )
              );
      } else {
         di = unsafeIRDirty_0_N( 
                 2, 
                 hname, VG_(fnptr_to_fnentry)( helper ), 
                 mkIRExprVec_2( addrAct,
                                zwidenToHostWord( mce, vdata ))
              );
      }
      if (guard) di->guard = guard;
      setHelperAnns( mce, di );
      stmt( 'V', mce, IRStmt_Dirty(di) );
   }

}



static IRType szToITy ( Int n )
{
   switch (n) {
      case 1: return Ity_I8;
      case 2: return Ity_I16;
      case 4: return Ity_I32;
      case 8: return Ity_I64;
      default: VG_(tool_panic)("szToITy(memcheck)");
   }
}

static
void do_shadow_Dirty ( MCEnv* mce, IRDirty* d )
{
   Int       i, k, n, toDo, gSz, gOff;
   IRAtom    *src, *here, *curr;
   IRType    tySrc, tyDst;
   IRTemp    dst;
   IREndness end;

   
#  if defined(VG_BIGENDIAN)
   end = Iend_BE;
#  elif defined(VG_LITTLEENDIAN)
   end = Iend_LE;
#  else
#    error "Unknown endianness"
#  endif

   
   complainIfUndefined(mce, d->guard, NULL);

   
   curr = definedOfType(Ity_I32);

   for (i = 0; d->args[i]; i++) {
      IRAtom* arg = d->args[i];
      if ( (d->cee->mcx_mask & (1<<i))
           || UNLIKELY(is_IRExpr_VECRET_or_BBPTR(arg)) ) {
         
      } else {
         here = mkPCastTo( mce, Ity_I32, expr2vbits(mce, arg) );
         curr = mkUifU32(mce, here, curr);
      }
   }

   
   for (i = 0; i < d->nFxState; i++) {
      tl_assert(d->fxState[i].fx != Ifx_None);
      if (d->fxState[i].fx == Ifx_Write)
         continue;

      
      for (k = 0; k < 1 + d->fxState[i].nRepeats; k++) {
         gOff = d->fxState[i].offset + k * d->fxState[i].repeatLen;
         gSz  = d->fxState[i].size;

         
         if (isAlwaysDefd(mce, gOff, gSz)) {
            if (0)
            VG_(printf)("memcheck: Dirty gst: ignored off %d, sz %d\n",
                        gOff, gSz);
            continue;
         }

         while (True) {
            tl_assert(gSz >= 0);
            if (gSz == 0) break;
            n = gSz <= 8 ? gSz : 8;
            tySrc = szToITy( n );

            IRAtom *cond, *iffalse, *iftrue;

            cond    = assignNew('V', mce, Ity_I1, d->guard);
            iftrue  = assignNew('V', mce, tySrc, shadow_GET(mce, gOff, tySrc));
            iffalse = assignNew('V', mce, tySrc, definedOfType(tySrc));
            src     = assignNew('V', mce, tySrc,
                                IRExpr_ITE(cond, iftrue, iffalse));

            here = mkPCastTo( mce, Ity_I32, src );
            curr = mkUifU32(mce, here, curr);
            gSz -= n;
            gOff += n;
         }
      }
   }


   if (d->mFx != Ifx_None) {
      IRType tyAddr;
      tl_assert(d->mAddr);
      complainIfUndefined(mce, d->mAddr, d->guard);

      tyAddr = typeOfIRExpr(mce->sb->tyenv, d->mAddr);
      tl_assert(tyAddr == Ity_I32 || tyAddr == Ity_I64);
      tl_assert(tyAddr == mce->hWordTy); 
   }

   
   if (d->mFx == Ifx_Read || d->mFx == Ifx_Modify) {
      toDo   = d->mSize;
      while (toDo >= 4) {
         here = mkPCastTo( 
                   mce, Ity_I32,
                   expr2vbits_Load_guarded_Simple(
                      mce, end, Ity_I32, d->mAddr, d->mSize - toDo, d->guard )
                );
         curr = mkUifU32(mce, here, curr);
         toDo -= 4;
      }
      
      while (toDo >= 2) {
         here = mkPCastTo( 
                   mce, Ity_I32,
                   expr2vbits_Load_guarded_Simple(
                      mce, end, Ity_I16, d->mAddr, d->mSize - toDo, d->guard )
                );
         curr = mkUifU32(mce, here, curr);
         toDo -= 2;
      }
      
      if (toDo == 1) {
         here = mkPCastTo( 
                   mce, Ity_I32,
                   expr2vbits_Load_guarded_Simple(
                      mce, end, Ity_I8, d->mAddr, d->mSize - toDo, d->guard )
                );
         curr = mkUifU32(mce, here, curr);
         toDo -= 1;
      }
      tl_assert(toDo == 0);
   }


   
   if (d->tmp != IRTemp_INVALID) {
      dst   = findShadowTmpV(mce, d->tmp);
      tyDst = typeOfIRTemp(mce->sb->tyenv, d->tmp);
      assign( 'V', mce, dst, mkPCastTo( mce, tyDst, curr) );
   }

   
   for (i = 0; i < d->nFxState; i++) {
      tl_assert(d->fxState[i].fx != Ifx_None);
      if (d->fxState[i].fx == Ifx_Read)
         continue;

      
      for (k = 0; k < 1 + d->fxState[i].nRepeats; k++) {
         gOff = d->fxState[i].offset + k * d->fxState[i].repeatLen;
         gSz  = d->fxState[i].size;

         
         if (isAlwaysDefd(mce, gOff, gSz))
            continue;

         /* This state element is written or modified.  So we need to
            consider it.  If larger than 8 bytes, deal with it in
            8-byte chunks. */
         while (True) {
            tl_assert(gSz >= 0);
            if (gSz == 0) break;
            n = gSz <= 8 ? gSz : 8;
            tyDst = szToITy( n );
            do_shadow_PUT( mce, gOff,
                                NULL, 
                                mkPCastTo( mce, tyDst, curr ), d->guard );
            gSz -= n;
            gOff += n;
         }
      }
   }

   if (d->mFx == Ifx_Write || d->mFx == Ifx_Modify) {
      toDo   = d->mSize;
      
      while (toDo >= 4) {
         do_shadow_Store( mce, end, d->mAddr, d->mSize - toDo,
                          NULL, 
                          mkPCastTo( mce, Ity_I32, curr ),
                          d->guard );
         toDo -= 4;
      }
      
      while (toDo >= 2) {
         do_shadow_Store( mce, end, d->mAddr, d->mSize - toDo,
                          NULL, 
                          mkPCastTo( mce, Ity_I16, curr ),
                          d->guard );
         toDo -= 2;
      }
      
      if (toDo == 1) {
         do_shadow_Store( mce, end, d->mAddr, d->mSize - toDo,
                          NULL, 
                          mkPCastTo( mce, Ity_I8, curr ),
                          d->guard );
         toDo -= 1;
      }
      tl_assert(toDo == 0);
   }

}


static
void do_AbiHint ( MCEnv* mce, IRExpr* base, Int len, IRExpr* nia )
{
   IRDirty* di;
   if (MC_(clo_mc_level) < 3)
      nia = mkIRExpr_HWord(0);

   di = unsafeIRDirty_0_N(
           0,
           "MC_(helperc_MAKE_STACK_UNINIT)",
           VG_(fnptr_to_fnentry)( &MC_(helperc_MAKE_STACK_UNINIT) ),
           mkIRExprVec_3( base, mkIRExpr_HWord( (UInt)len), nia )
        );
   stmt( 'V', mce, IRStmt_Dirty(di) );
}



static IRAtom* gen_load_b  ( MCEnv* mce, Int szB, 
                             IRAtom* baseaddr, Int offset );
static IRAtom* gen_maxU32  ( MCEnv* mce, IRAtom* b1, IRAtom* b2 );
static void    gen_store_b ( MCEnv* mce, Int szB,
                             IRAtom* baseaddr, Int offset, IRAtom* dataB,
                             IRAtom* guard );

static void do_shadow_CAS_single ( MCEnv* mce, IRCAS* cas );
static void do_shadow_CAS_double ( MCEnv* mce, IRCAS* cas );


static void bind_shadow_tmp_to_orig ( UChar how,
                                      MCEnv* mce,
                                      IRAtom* orig, IRAtom* shadow )
{
   tl_assert(isOriginalAtom(mce, orig));
   tl_assert(isShadowAtom(mce, shadow));
   switch (orig->tag) {
      case Iex_Const:
         tl_assert(shadow->tag == Iex_Const);
         break;
      case Iex_RdTmp:
         tl_assert(shadow->tag == Iex_RdTmp);
         if (how == 'V') {
            assign('V', mce, findShadowTmpV(mce,orig->Iex.RdTmp.tmp),
                   shadow);
         } else {
            tl_assert(how == 'B');
            assign('B', mce, findShadowTmpB(mce,orig->Iex.RdTmp.tmp),
                   shadow);
         }
         break;
      default:
         tl_assert(0);
   }
}


static
void do_shadow_CAS ( MCEnv* mce, IRCAS* cas )
{
   /* Scheme is (both single- and double- cases):

      1. fetch data#,dataB (the proposed new value)

      2. fetch expd#,expdB (what we expect to see at the address)

      3. check definedness of address

      4. load old#,oldB from shadow memory; this also checks
         addressibility of the address

      5. the CAS itself

      6. compute "expected == old".  See COMMENT_ON_CasCmpEQ below.

      7. if "expected == old" (as computed by (6))
            store data#,dataB to shadow memory

      Note that 5 reads 'old' but 4 reads 'old#'.  Similarly, 5 stores
      'data' but 7 stores 'data#'.  Hence it is possible for the
      shadow data to be incorrectly checked and/or updated:

      * 7 is at least gated correctly, since the 'expected == old'
        condition is derived from outputs of 5.  However, the shadow
        write could happen too late: imagine after 5 we are
        descheduled, a different thread runs, writes a different
        (shadow) value at the address, and then we resume, hence
        overwriting the shadow value written by the other thread.

      Because the original memory access is atomic, there's no way to
      make both the original and shadow accesses into a single atomic
      thing, hence this is unavoidable.

      At least as Valgrind stands, I don't think it's a problem, since
      we're single threaded *and* we guarantee that there are no
      context switches during the execution of any specific superblock
      -- context switches can only happen at superblock boundaries.

      If Valgrind ever becomes MT in the future, then it might be more
      of a problem.  A possible kludge would be to artificially
      associate with the location, a lock, which we must acquire and
      release around the transaction as a whole.  Hmm, that probably
      would't work properly since it only guards us against other
      threads doing CASs on the same location, not against other
      threads doing normal reads and writes.

      ------------------------------------------------------------

      COMMENT_ON_CasCmpEQ:

      Note two things.  Firstly, in the sequence above, we compute
      "expected == old", but we don't check definedness of it.  Why
      not?  Also, the x86 and amd64 front ends use
      Iop_CasCmp{EQ,NE}{8,16,32,64} comparisons to make the equivalent
      determination (expected == old ?) for themselves, and we also
      don't check definedness for those primops; we just say that the
      result is defined.  Why?  Details follow.

      x86/amd64 contains various forms of locked insns:
      * lock prefix before all basic arithmetic insn; 
        eg lock xorl %reg1,(%reg2)
      * atomic exchange reg-mem
      * compare-and-swaps

      Rather than attempt to represent them all, which would be a
      royal PITA, I used a result from Maurice Herlihy
      (http://en.wikipedia.org/wiki/Maurice_Herlihy), in which he
      demonstrates that compare-and-swap is a primitive more general
      than the other two, and so can be used to represent all of them.
      So the translation scheme for (eg) lock incl (%reg) is as
      follows:

        again:
         old = * %reg
         new = old + 1
         atomically { if (* %reg == old) { * %reg = new } else { goto again } }

      The "atomically" is the CAS bit.  The scheme is always the same:
      get old value from memory, compute new value, atomically stuff
      new value back in memory iff the old value has not changed (iow,
      no other thread modified it in the meantime).  If it has changed
      then we've been out-raced and we have to start over.

      Now that's all very neat, but it has the bad side effect of
      introducing an explicit equality test into the translation.
      Consider the behaviour of said code on a memory location which
      is uninitialised.  We will wind up doing a comparison on
      uninitialised data, and mc duly complains.

      What's difficult about this is, the common case is that the
      location is uncontended, and so we're usually comparing the same
      value (* %reg) with itself.  So we shouldn't complain even if it
      is undefined.  But mc doesn't know that.

      My solution is to mark the == in the IR specially, so as to tell
      mc that it almost certainly compares a value with itself, and we
      should just regard the result as always defined.  Rather than
      add a bit to all IROps, I just cloned Iop_CmpEQ{8,16,32,64} into
      Iop_CasCmpEQ{8,16,32,64} so as not to disturb anything else.

      So there's always the question of, can this give a false
      negative?  eg, imagine that initially, * %reg is defined; and we
      read that; but then in the gap between the read and the CAS, a
      different thread writes an undefined (and different) value at
      the location.  Then the CAS in this thread will fail and we will
      go back to "again:", but without knowing that the trip back
      there was based on an undefined comparison.  No matter; at least
      the other thread won the race and the location is correctly
      marked as undefined.  What if it wrote an uninitialised version
      of the same value that was there originally, though?

      etc etc.  Seems like there's a small corner case in which we
      might lose the fact that something's defined -- we're out-raced
      in between the "old = * reg" and the "atomically {", _and_ the
      other thread is writing in an undefined version of what's
      already there.  Well, that seems pretty unlikely.

      ---

      If we ever need to reinstate it .. code which generates a
      definedness test for "expected == old" was removed at r10432 of
      this file.
   */
   if (cas->oldHi == IRTemp_INVALID) {
      do_shadow_CAS_single( mce, cas );
   } else {
      do_shadow_CAS_double( mce, cas );
   }
}


static void do_shadow_CAS_single ( MCEnv* mce, IRCAS* cas )
{
   IRAtom *vdataLo = NULL, *bdataLo = NULL;
   IRAtom *vexpdLo = NULL, *bexpdLo = NULL;
   IRAtom *voldLo  = NULL, *boldLo  = NULL;
   IRAtom *expd_eq_old = NULL;
   IROp   opCasCmpEQ;
   Int    elemSzB;
   IRType elemTy;
   Bool   otrak = MC_(clo_mc_level) >= 3; 

   
   tl_assert(cas->oldHi == IRTemp_INVALID);
   tl_assert(cas->expdHi == NULL);
   tl_assert(cas->dataHi == NULL);

   elemTy = typeOfIRExpr(mce->sb->tyenv, cas->expdLo);
   switch (elemTy) {
      case Ity_I8:  elemSzB = 1; opCasCmpEQ = Iop_CasCmpEQ8;  break;
      case Ity_I16: elemSzB = 2; opCasCmpEQ = Iop_CasCmpEQ16; break;
      case Ity_I32: elemSzB = 4; opCasCmpEQ = Iop_CasCmpEQ32; break;
      case Ity_I64: elemSzB = 8; opCasCmpEQ = Iop_CasCmpEQ64; break;
      default: tl_assert(0); 
   }

   
   tl_assert(isOriginalAtom(mce, cas->dataLo));
   vdataLo
      = assignNew('V', mce, elemTy, expr2vbits(mce, cas->dataLo));
   tl_assert(isShadowAtom(mce, vdataLo));
   if (otrak) {
      bdataLo
         = assignNew('B', mce, Ity_I32, schemeE(mce, cas->dataLo));
      tl_assert(isShadowAtom(mce, bdataLo));
   }

   
   tl_assert(isOriginalAtom(mce, cas->expdLo));
   vexpdLo
      = assignNew('V', mce, elemTy, expr2vbits(mce, cas->expdLo));
   tl_assert(isShadowAtom(mce, vexpdLo));
   if (otrak) {
      bexpdLo
         = assignNew('B', mce, Ity_I32, schemeE(mce, cas->expdLo));
      tl_assert(isShadowAtom(mce, bexpdLo));
   }

   
   voldLo
      = assignNew(
           'V', mce, elemTy,
           expr2vbits_Load( 
              mce,
              cas->end, elemTy, cas->addr, 0,
              NULL
        ));
   bind_shadow_tmp_to_orig('V', mce, mkexpr(cas->oldLo), voldLo);
   if (otrak) {
      boldLo
         = assignNew('B', mce, Ity_I32,
                     gen_load_b(mce, elemSzB, cas->addr, 0));
      bind_shadow_tmp_to_orig('B', mce, mkexpr(cas->oldLo), boldLo);
   }

   
   stmt( 'C', mce, IRStmt_CAS(cas) );

   
   
   expd_eq_old
      = assignNew('C', mce, Ity_I1,
                  binop(opCasCmpEQ, cas->expdLo, mkexpr(cas->oldLo)));

   do_shadow_Store( mce, cas->end, cas->addr, 0,
                    NULL, vdataLo,
                    expd_eq_old );
   if (otrak) {
      gen_store_b( mce, elemSzB, cas->addr, 0,
                   bdataLo,
                   expd_eq_old );
   }
}


static void do_shadow_CAS_double ( MCEnv* mce, IRCAS* cas )
{
   IRAtom *vdataHi = NULL, *bdataHi = NULL;
   IRAtom *vdataLo = NULL, *bdataLo = NULL;
   IRAtom *vexpdHi = NULL, *bexpdHi = NULL;
   IRAtom *vexpdLo = NULL, *bexpdLo = NULL;
   IRAtom *voldHi  = NULL, *boldHi  = NULL;
   IRAtom *voldLo  = NULL, *boldLo  = NULL;
   IRAtom *xHi = NULL, *xLo = NULL, *xHL = NULL;
   IRAtom *expd_eq_old = NULL, *zero = NULL;
   IROp   opCasCmpEQ, opOr, opXor;
   Int    elemSzB, memOffsLo, memOffsHi;
   IRType elemTy;
   Bool   otrak = MC_(clo_mc_level) >= 3; 

   
   tl_assert(cas->oldHi != IRTemp_INVALID);
   tl_assert(cas->expdHi != NULL);
   tl_assert(cas->dataHi != NULL);

   elemTy = typeOfIRExpr(mce->sb->tyenv, cas->expdLo);
   switch (elemTy) {
      case Ity_I8:
         opCasCmpEQ = Iop_CasCmpEQ8; opOr = Iop_Or8; opXor = Iop_Xor8; 
         elemSzB = 1; zero = mkU8(0);
         break;
      case Ity_I16:
         opCasCmpEQ = Iop_CasCmpEQ16; opOr = Iop_Or16; opXor = Iop_Xor16;
         elemSzB = 2; zero = mkU16(0);
         break;
      case Ity_I32:
         opCasCmpEQ = Iop_CasCmpEQ32; opOr = Iop_Or32; opXor = Iop_Xor32;
         elemSzB = 4; zero = mkU32(0);
         break;
      case Ity_I64:
         opCasCmpEQ = Iop_CasCmpEQ64; opOr = Iop_Or64; opXor = Iop_Xor64;
         elemSzB = 8; zero = mkU64(0);
         break;
      default:
         tl_assert(0); 
   }

   
   tl_assert(isOriginalAtom(mce, cas->dataHi));
   tl_assert(isOriginalAtom(mce, cas->dataLo));
   vdataHi
      = assignNew('V', mce, elemTy, expr2vbits(mce, cas->dataHi));
   vdataLo
      = assignNew('V', mce, elemTy, expr2vbits(mce, cas->dataLo));
   tl_assert(isShadowAtom(mce, vdataHi));
   tl_assert(isShadowAtom(mce, vdataLo));
   if (otrak) {
      bdataHi
         = assignNew('B', mce, Ity_I32, schemeE(mce, cas->dataHi));
      bdataLo
         = assignNew('B', mce, Ity_I32, schemeE(mce, cas->dataLo));
      tl_assert(isShadowAtom(mce, bdataHi));
      tl_assert(isShadowAtom(mce, bdataLo));
   }

   
   tl_assert(isOriginalAtom(mce, cas->expdHi));
   tl_assert(isOriginalAtom(mce, cas->expdLo));
   vexpdHi
      = assignNew('V', mce, elemTy, expr2vbits(mce, cas->expdHi));
   vexpdLo
      = assignNew('V', mce, elemTy, expr2vbits(mce, cas->expdLo));
   tl_assert(isShadowAtom(mce, vexpdHi));
   tl_assert(isShadowAtom(mce, vexpdLo));
   if (otrak) {
      bexpdHi
         = assignNew('B', mce, Ity_I32, schemeE(mce, cas->expdHi));
      bexpdLo
         = assignNew('B', mce, Ity_I32, schemeE(mce, cas->expdLo));
      tl_assert(isShadowAtom(mce, bexpdHi));
      tl_assert(isShadowAtom(mce, bexpdLo));
   }

   
   if (cas->end == Iend_LE) {
      memOffsLo = 0;
      memOffsHi = elemSzB;
   } else {
      tl_assert(cas->end == Iend_BE);
      memOffsLo = elemSzB;
      memOffsHi = 0;
   }
   voldHi
      = assignNew(
           'V', mce, elemTy,
           expr2vbits_Load( 
              mce,
              cas->end, elemTy, cas->addr, memOffsHi,
              NULL
        ));
   voldLo
      = assignNew(
           'V', mce, elemTy,
           expr2vbits_Load( 
              mce,
              cas->end, elemTy, cas->addr, memOffsLo,
              NULL
        ));
   bind_shadow_tmp_to_orig('V', mce, mkexpr(cas->oldHi), voldHi);
   bind_shadow_tmp_to_orig('V', mce, mkexpr(cas->oldLo), voldLo);
   if (otrak) {
      boldHi
         = assignNew('B', mce, Ity_I32,
                     gen_load_b(mce, elemSzB, cas->addr,
                                memOffsHi));
      boldLo
         = assignNew('B', mce, Ity_I32,
                     gen_load_b(mce, elemSzB, cas->addr,
                                memOffsLo));
      bind_shadow_tmp_to_orig('B', mce, mkexpr(cas->oldHi), boldHi);
      bind_shadow_tmp_to_orig('B', mce, mkexpr(cas->oldLo), boldLo);
   }

   
   stmt( 'C', mce, IRStmt_CAS(cas) );

   
   
   xHi = assignNew('C', mce, elemTy,
                   binop(opXor, cas->expdHi, mkexpr(cas->oldHi))); 
   xLo = assignNew('C', mce, elemTy,
                   binop(opXor, cas->expdLo, mkexpr(cas->oldLo)));
   xHL = assignNew('C', mce, elemTy,
                   binop(opOr, xHi, xLo));
   expd_eq_old
      = assignNew('C', mce, Ity_I1,
                  binop(opCasCmpEQ, xHL, zero));

   do_shadow_Store( mce, cas->end, cas->addr, memOffsHi,
                    NULL, vdataHi,
                    expd_eq_old );
   do_shadow_Store( mce, cas->end, cas->addr, memOffsLo,
                    NULL, vdataLo,
                    expd_eq_old );
   if (otrak) {
      gen_store_b( mce, elemSzB, cas->addr, memOffsHi,
                   bdataHi,
                   expd_eq_old );
      gen_store_b( mce, elemSzB, cas->addr, memOffsLo,
                   bdataLo,
                   expd_eq_old );
   }
}



static void do_shadow_LLSC ( MCEnv*    mce,
                             IREndness stEnd,
                             IRTemp    stResult,
                             IRExpr*   stAddr,
                             IRExpr*   stStoredata )
{
   IRType resTy  = typeOfIRTemp(mce->sb->tyenv, stResult);
   IRTemp resTmp = findShadowTmpV(mce, stResult);

   tl_assert(isIRAtom(stAddr));
   if (stStoredata)
      tl_assert(isIRAtom(stStoredata));

   if (stStoredata == NULL) {
      
      
      tl_assert(resTy == Ity_I64 || resTy == Ity_I32
                || resTy == Ity_I16 || resTy == Ity_I8);
      assign( 'V', mce, resTmp,
                   expr2vbits_Load(
                      mce, stEnd, resTy, stAddr, 0,
                      NULL) );
   } else {
      
      
      IRType dataTy = typeOfIRExpr(mce->sb->tyenv,
                                   stStoredata);
      tl_assert(dataTy == Ity_I64 || dataTy == Ity_I32
                || dataTy == Ity_I16 || dataTy == Ity_I8);
      do_shadow_Store( mce, stEnd,
                            stAddr, 0,
                            stStoredata,
                            NULL ,
                            NULL );
      tl_assert(resTy == Ity_I1);
      assign( 'V', mce, resTmp, definedOfType(resTy) );
   }
}



static void do_shadow_StoreG ( MCEnv* mce, IRStoreG* sg )
{
   complainIfUndefined(mce, sg->guard, NULL);
   do_shadow_Store( mce, sg->end,
                    sg->addr, 0,
                    sg->data,
                    NULL ,
                    sg->guard );
}

static void do_shadow_LoadG ( MCEnv* mce, IRLoadG* lg )
{
   complainIfUndefined(mce, lg->guard, NULL);

   IROp   vwiden   = Iop_INVALID;
   IRType loadedTy = Ity_INVALID;
   switch (lg->cvt) {
      case ILGop_Ident64: loadedTy = Ity_I64; vwiden = Iop_INVALID; break;
      case ILGop_Ident32: loadedTy = Ity_I32; vwiden = Iop_INVALID; break;
      case ILGop_16Uto32: loadedTy = Ity_I16; vwiden = Iop_16Uto32; break;
      case ILGop_16Sto32: loadedTy = Ity_I16; vwiden = Iop_16Sto32; break;
      case ILGop_8Uto32:  loadedTy = Ity_I8;  vwiden = Iop_8Uto32;  break;
      case ILGop_8Sto32:  loadedTy = Ity_I8;  vwiden = Iop_8Sto32;  break;
      default: VG_(tool_panic)("do_shadow_LoadG");
   }

   IRAtom* vbits_alt
      = expr2vbits( mce, lg->alt );
   IRAtom* vbits_final
      = expr2vbits_Load_guarded_General(mce, lg->end, loadedTy,
                                        lg->addr, 0,
                                        lg->guard, vwiden, vbits_alt );
   
   assign( 'V', mce, findShadowTmpV(mce, lg->dst), vbits_final );
}



static void schemeS ( MCEnv* mce, IRStmt* st );

static Bool isBogusAtom ( IRAtom* at )
{
   ULong n = 0;
   IRConst* con;
   tl_assert(isIRAtom(at));
   if (at->tag == Iex_RdTmp)
      return False;
   tl_assert(at->tag == Iex_Const);
   con = at->Iex.Const.con;
   switch (con->tag) {
      case Ico_U1:   return False;
      case Ico_U8:   n = (ULong)con->Ico.U8; break;
      case Ico_U16:  n = (ULong)con->Ico.U16; break;
      case Ico_U32:  n = (ULong)con->Ico.U32; break;
      case Ico_U64:  n = (ULong)con->Ico.U64; break;
      case Ico_F32:  return False;
      case Ico_F64:  return False;
      case Ico_F32i: return False;
      case Ico_F64i: return False;
      case Ico_V128: return False;
      case Ico_V256: return False;
      default: ppIRExpr(at); tl_assert(0);
   }
   
   return (    n == 0xFEFEFEFFULL
            || n == 0x80808080ULL
            || n == 0x7F7F7F7FULL
            || n == 0x7EFEFEFFULL
            || n == 0x81010100ULL
            || n == 0xFFFFFFFFFEFEFEFFULL
            || n == 0xFEFEFEFEFEFEFEFFULL
            || n == 0x0000000000008080ULL
            || n == 0x8080808080808080ULL
            || n == 0x0101010101010101ULL
          );
}

static Bool checkForBogusLiterals (  IRStmt* st )
{
   Int      i;
   IRExpr*  e;
   IRDirty* d;
   IRCAS*   cas;
   switch (st->tag) {
      case Ist_WrTmp:
         e = st->Ist.WrTmp.data;
         switch (e->tag) {
            case Iex_Get:
            case Iex_RdTmp:
               return False;
            case Iex_Const:
               return isBogusAtom(e);
            case Iex_Unop: 
               return isBogusAtom(e->Iex.Unop.arg)
                      || e->Iex.Unop.op == Iop_GetMSBs8x16;
            case Iex_GetI:
               return isBogusAtom(e->Iex.GetI.ix);
            case Iex_Binop: 
               return isBogusAtom(e->Iex.Binop.arg1)
                      || isBogusAtom(e->Iex.Binop.arg2);
            case Iex_Triop: 
               return isBogusAtom(e->Iex.Triop.details->arg1)
                      || isBogusAtom(e->Iex.Triop.details->arg2)
                      || isBogusAtom(e->Iex.Triop.details->arg3);
            case Iex_Qop: 
               return isBogusAtom(e->Iex.Qop.details->arg1)
                      || isBogusAtom(e->Iex.Qop.details->arg2)
                      || isBogusAtom(e->Iex.Qop.details->arg3)
                      || isBogusAtom(e->Iex.Qop.details->arg4);
            case Iex_ITE:
               return isBogusAtom(e->Iex.ITE.cond)
                      || isBogusAtom(e->Iex.ITE.iftrue)
                      || isBogusAtom(e->Iex.ITE.iffalse);
            case Iex_Load: 
               return isBogusAtom(e->Iex.Load.addr);
            case Iex_CCall:
               for (i = 0; e->Iex.CCall.args[i]; i++)
                  if (isBogusAtom(e->Iex.CCall.args[i]))
                     return True;
               return False;
            default: 
               goto unhandled;
         }
      case Ist_Dirty:
         d = st->Ist.Dirty.details;
         for (i = 0; d->args[i]; i++) {
            IRAtom* atom = d->args[i];
            if (LIKELY(!is_IRExpr_VECRET_or_BBPTR(atom))) {
               if (isBogusAtom(atom))
                  return True;
            }
         }
         if (isBogusAtom(d->guard))
            return True;
         if (d->mAddr && isBogusAtom(d->mAddr))
            return True;
         return False;
      case Ist_Put:
         return isBogusAtom(st->Ist.Put.data);
      case Ist_PutI:
         return isBogusAtom(st->Ist.PutI.details->ix) 
                || isBogusAtom(st->Ist.PutI.details->data);
      case Ist_Store:
         return isBogusAtom(st->Ist.Store.addr) 
                || isBogusAtom(st->Ist.Store.data);
      case Ist_StoreG: {
         IRStoreG* sg = st->Ist.StoreG.details;
         return isBogusAtom(sg->addr) || isBogusAtom(sg->data)
                || isBogusAtom(sg->guard);
      }
      case Ist_LoadG: {
         IRLoadG* lg = st->Ist.LoadG.details;
         return isBogusAtom(lg->addr) || isBogusAtom(lg->alt)
                || isBogusAtom(lg->guard);
      }
      case Ist_Exit:
         return isBogusAtom(st->Ist.Exit.guard);
      case Ist_AbiHint:
         return isBogusAtom(st->Ist.AbiHint.base)
                || isBogusAtom(st->Ist.AbiHint.nia);
      case Ist_NoOp:
      case Ist_IMark:
      case Ist_MBE:
         return False;
      case Ist_CAS:
         cas = st->Ist.CAS.details;
         return isBogusAtom(cas->addr)
                || (cas->expdHi ? isBogusAtom(cas->expdHi) : False)
                || isBogusAtom(cas->expdLo)
                || (cas->dataHi ? isBogusAtom(cas->dataHi) : False)
                || isBogusAtom(cas->dataLo);
      case Ist_LLSC:
         return isBogusAtom(st->Ist.LLSC.addr)
                || (st->Ist.LLSC.storedata
                       ? isBogusAtom(st->Ist.LLSC.storedata)
                       : False);
      default: 
      unhandled:
         ppIRStmt(st);
         VG_(tool_panic)("hasBogusLiterals");
   }
}


IRSB* MC_(instrument) ( VgCallbackClosure* closure,
                        IRSB* sb_in, 
                        const VexGuestLayout* layout, 
                        const VexGuestExtents* vge,
                        const VexArchInfo* archinfo_host,
                        IRType gWordTy, IRType hWordTy )
{
   Bool    verboze = 0||False;
   Bool    bogus;
   Int     i, j, first_stmt;
   IRStmt* st;
   MCEnv   mce;
   IRSB*   sb_out;

   if (gWordTy != hWordTy) {
      
      VG_(tool_panic)("host/guest word size mismatch");
   }

   
   tl_assert(sizeof(UWord)  == sizeof(void*));
   tl_assert(sizeof(Word)   == sizeof(void*));
   tl_assert(sizeof(Addr)   == sizeof(void*));
   tl_assert(sizeof(ULong)  == 8);
   tl_assert(sizeof(Long)   == 8);
   tl_assert(sizeof(UInt)   == 4);
   tl_assert(sizeof(Int)    == 4);

   tl_assert(MC_(clo_mc_level) >= 1 && MC_(clo_mc_level) <= 3);

   
   sb_out = deepCopyIRSBExceptStmts(sb_in);

   VG_(memset)(&mce, 0, sizeof(mce));
   mce.sb             = sb_out;
   mce.trace          = verboze;
   mce.layout         = layout;
   mce.hWordTy        = hWordTy;
   mce.bogusLiterals  = False;

   mce.useLLVMworkarounds = False;
#  if defined(VGO_darwin)
   mce.useLLVMworkarounds = True;
#  endif

   mce.tmpMap = VG_(newXA)( VG_(malloc), "mc.MC_(instrument).1", VG_(free),
                            sizeof(TempMapEnt));
   VG_(hintSizeXA) (mce.tmpMap, sb_in->tyenv->types_used);
   for (i = 0; i < sb_in->tyenv->types_used; i++) {
      TempMapEnt ent;
      ent.kind    = Orig;
      ent.shadowV = IRTemp_INVALID;
      ent.shadowB = IRTemp_INVALID;
      VG_(addToXA)( mce.tmpMap, &ent );
   }
   tl_assert( VG_(sizeXA)( mce.tmpMap ) == sb_in->tyenv->types_used );


   bogus = False;

   for (i = 0; i < sb_in->stmts_used; i++) {

      st = sb_in->stmts[i];
      tl_assert(st);
      tl_assert(isFlatIRStmt(st));

      if (!bogus) {
         bogus = checkForBogusLiterals(st);
         if (0 && bogus) {
            VG_(printf)("bogus: ");
            ppIRStmt(st);
            VG_(printf)("\n");
         }
      }

   }

   mce.bogusLiterals = bogus;

   

   tl_assert(mce.sb == sb_out);
   tl_assert(mce.sb != sb_in);

   i = 0;
   while (i < sb_in->stmts_used && sb_in->stmts[i]->tag != Ist_IMark) {

      st = sb_in->stmts[i];
      tl_assert(st);
      tl_assert(isFlatIRStmt(st));

      stmt( 'C', &mce, sb_in->stmts[i] );
      i++;
   }

   for (j = 0; j < i; j++) {
      if (sb_in->stmts[j]->tag == Ist_WrTmp) {
         IRTemp tmp_o = sb_in->stmts[j]->Ist.WrTmp.tmp;
         IRTemp tmp_v = findShadowTmpV(&mce, tmp_o);
         IRType ty_v  = typeOfIRTemp(sb_out->tyenv, tmp_v);
         assign( 'V', &mce, tmp_v, definedOfType( ty_v ) );
         if (MC_(clo_mc_level) == 3) {
            IRTemp tmp_b = findShadowTmpB(&mce, tmp_o);
            tl_assert(typeOfIRTemp(sb_out->tyenv, tmp_b) == Ity_I32);
            assign( 'B', &mce, tmp_b, mkU32(0));
         }
         if (0) {
            VG_(printf)("create shadow tmp(s) for preamble tmp [%d] ty ", j);
            ppIRType( ty_v );
            VG_(printf)("\n");
         }
      }
   }

   

   tl_assert(sb_in->stmts_used > 0);
   tl_assert(i >= 0);
   tl_assert(i < sb_in->stmts_used);
   tl_assert(sb_in->stmts[i]->tag == Ist_IMark);

   for (; i < sb_in->stmts_used; i++) {

      st = sb_in->stmts[i];
      first_stmt = sb_out->stmts_used;

      if (verboze) {
         VG_(printf)("\n");
         ppIRStmt(st);
         VG_(printf)("\n");
      }

      if (MC_(clo_mc_level) == 3) {
         
         if (st->tag != Ist_CAS) 
            schemeS( &mce, st );
      }

      

      switch (st->tag) {

         case Ist_WrTmp:
            assign( 'V', &mce, findShadowTmpV(&mce, st->Ist.WrTmp.tmp), 
                               expr2vbits( &mce, st->Ist.WrTmp.data) );
            break;

         case Ist_Put:
            do_shadow_PUT( &mce, 
                           st->Ist.Put.offset,
                           st->Ist.Put.data,
                           NULL , NULL  );
            break;

         case Ist_PutI:
            do_shadow_PUTI( &mce, st->Ist.PutI.details);
            break;

         case Ist_Store:
            do_shadow_Store( &mce, st->Ist.Store.end,
                                   st->Ist.Store.addr, 0,
                                   st->Ist.Store.data,
                                   NULL ,
                                   NULL );
            break;

         case Ist_StoreG:
            do_shadow_StoreG( &mce, st->Ist.StoreG.details );
            break;

         case Ist_LoadG:
            do_shadow_LoadG( &mce, st->Ist.LoadG.details );
            break;

         case Ist_Exit:
            complainIfUndefined( &mce, st->Ist.Exit.guard, NULL );
            break;

         case Ist_IMark:
            break;

         case Ist_NoOp:
         case Ist_MBE:
            break;

         case Ist_Dirty:
            do_shadow_Dirty( &mce, st->Ist.Dirty.details );
            break;

         case Ist_AbiHint:
            do_AbiHint( &mce, st->Ist.AbiHint.base,
                              st->Ist.AbiHint.len,
                              st->Ist.AbiHint.nia );
            break;

         case Ist_CAS:
            do_shadow_CAS( &mce, st->Ist.CAS.details );
            break;

         case Ist_LLSC:
            do_shadow_LLSC( &mce,
                            st->Ist.LLSC.end,
                            st->Ist.LLSC.result,
                            st->Ist.LLSC.addr,
                            st->Ist.LLSC.storedata );
            break;

         default:
            VG_(printf)("\n");
            ppIRStmt(st);
            VG_(printf)("\n");
            VG_(tool_panic)("memcheck: unhandled IRStmt");

      } 

      if (0 && verboze) {
         for (j = first_stmt; j < sb_out->stmts_used; j++) {
            VG_(printf)("   ");
            ppIRStmt(sb_out->stmts[j]);
            VG_(printf)("\n");
         }
         VG_(printf)("\n");
      }

      if (st->tag != Ist_CAS)
         stmt('C', &mce, st);
   }

   
   first_stmt = sb_out->stmts_used;

   if (verboze) {
      VG_(printf)("sb_in->next = ");
      ppIRExpr(sb_in->next);
      VG_(printf)("\n\n");
   }

   complainIfUndefined( &mce, sb_in->next, NULL );

   if (0 && verboze) {
      for (j = first_stmt; j < sb_out->stmts_used; j++) {
         VG_(printf)("   ");
         ppIRStmt(sb_out->stmts[j]);
         VG_(printf)("\n");
      }
      VG_(printf)("\n");
   }

   tl_assert( VG_(sizeXA)( mce.tmpMap ) == mce.sb->tyenv->types_used );
   VG_(deleteXA)( mce.tmpMap );

   tl_assert(mce.sb == sb_out);
   return sb_out;
}



typedef
   struct { void* entry; IRExpr* guard; }
   Pair;


static Bool sameIRValue ( IRExpr* e1, IRExpr* e2 )
{
   if (e1->tag != e2->tag)
      return False;
   switch (e1->tag) {
      case Iex_Const:
         return eqIRConst( e1->Iex.Const.con, e2->Iex.Const.con );
      case Iex_Binop:
         return e1->Iex.Binop.op == e2->Iex.Binop.op 
                && sameIRValue(e1->Iex.Binop.arg1, e2->Iex.Binop.arg1)
                && sameIRValue(e1->Iex.Binop.arg2, e2->Iex.Binop.arg2);
      case Iex_Unop:
         return e1->Iex.Unop.op == e2->Iex.Unop.op 
                && sameIRValue(e1->Iex.Unop.arg, e2->Iex.Unop.arg);
      case Iex_RdTmp:
         return e1->Iex.RdTmp.tmp == e2->Iex.RdTmp.tmp;
      case Iex_ITE:
         return sameIRValue( e1->Iex.ITE.cond, e2->Iex.ITE.cond )
                && sameIRValue( e1->Iex.ITE.iftrue,  e2->Iex.ITE.iftrue )
                && sameIRValue( e1->Iex.ITE.iffalse, e2->Iex.ITE.iffalse );
      case Iex_Qop:
      case Iex_Triop:
      case Iex_CCall:
         return False;
      case Iex_Get:
      case Iex_GetI:
      case Iex_Load:
         return False;
      case Iex_Binder:
         
         
      default:
         VG_(printf)("mc_translate.c: sameIRValue: unhandled: ");
         ppIRExpr(e1); 
         VG_(tool_panic)("memcheck:sameIRValue");
         return False;
   }
}


static 
Bool check_or_add ( XArray*  pairs, IRExpr* guard, void* entry )
{
   Pair  p;
   Pair* pp;
   Int   i, n = VG_(sizeXA)( pairs );
   for (i = 0; i < n; i++) {
      pp = VG_(indexXA)( pairs, i );
      if (pp->entry == entry && sameIRValue(pp->guard, guard))
         return True;
   }
   p.guard = guard;
   p.entry = entry;
   VG_(addToXA)( pairs, &p );
   return False;
}

static Bool is_helperc_value_checkN_fail ( const HChar* name )
{
   return
      0==VG_(strcmp)(name, "MC_(helperc_value_check0_fail_no_o)")
      || 0==VG_(strcmp)(name, "MC_(helperc_value_check1_fail_no_o)")
      || 0==VG_(strcmp)(name, "MC_(helperc_value_check4_fail_no_o)")
      || 0==VG_(strcmp)(name, "MC_(helperc_value_check8_fail_no_o)")
      || 0==VG_(strcmp)(name, "MC_(helperc_value_check0_fail_w_o)")
      || 0==VG_(strcmp)(name, "MC_(helperc_value_check1_fail_w_o)")
      || 0==VG_(strcmp)(name, "MC_(helperc_value_check4_fail_w_o)")
      || 0==VG_(strcmp)(name, "MC_(helperc_value_check8_fail_w_o)");
}

IRSB* MC_(final_tidy) ( IRSB* sb_in )
{
   Int i;
   IRStmt*   st;
   IRDirty*  di;
   IRExpr*   guard;
   IRCallee* cee;
   Bool      alreadyPresent;
   XArray*   pairs = VG_(newXA)( VG_(malloc), "mc.ft.1",
                                 VG_(free), sizeof(Pair) );
   for (i = 0; i < sb_in->stmts_used; i++) {
      st = sb_in->stmts[i];
      tl_assert(st);
      if (st->tag != Ist_Dirty)
         continue;
      di = st->Ist.Dirty.details;
      guard = di->guard;
      tl_assert(guard);
      if (0) { ppIRExpr(guard); VG_(printf)("\n"); }
      cee = di->cee;
      if (!is_helperc_value_checkN_fail( cee->name )) 
         continue;
      alreadyPresent = check_or_add( pairs, guard, cee->addr );
      if (alreadyPresent) {
         sb_in->stmts[i] = IRStmt_NoOp();
         if (0) VG_(printf)("XX\n");
      }
   }
   VG_(deleteXA)( pairs );
   return sb_in;
}



static IRTemp findShadowTmpB ( MCEnv* mce, IRTemp orig )
{
   TempMapEnt* ent;
   ent = (TempMapEnt*)VG_(indexXA)( mce->tmpMap, (Word)orig );
   tl_assert(ent->kind == Orig);
   if (ent->shadowB == IRTemp_INVALID) {
      IRTemp tmpB
        = newTemp( mce, Ity_I32, BSh );
      ent = (TempMapEnt*)VG_(indexXA)( mce->tmpMap, (Word)orig );
      tl_assert(ent->kind == Orig);
      tl_assert(ent->shadowB == IRTemp_INVALID);
      ent->shadowB = tmpB;
   }
   return ent->shadowB;
}

static IRAtom* gen_maxU32 ( MCEnv* mce, IRAtom* b1, IRAtom* b2 )
{
   return assignNew( 'B', mce, Ity_I32, binop(Iop_Max32U, b1, b2) );
}


static IRAtom* gen_guarded_load_b ( MCEnv* mce, Int szB, 
                                    IRAtom* baseaddr, 
                                    Int offset, IRExpr* guard )
{
   void*    hFun;
   const HChar* hName;
   IRTemp   bTmp;
   IRDirty* di;
   IRType   aTy   = typeOfIRExpr( mce->sb->tyenv, baseaddr );
   IROp     opAdd = aTy == Ity_I32 ? Iop_Add32 : Iop_Add64;
   IRAtom*  ea    = baseaddr;
   if (offset != 0) {
      IRAtom* off = aTy == Ity_I32 ? mkU32( offset )
                                   : mkU64( (Long)(Int)offset );
      ea = assignNew( 'B', mce, aTy, binop(opAdd, ea, off));
   }
   bTmp = newTemp(mce, mce->hWordTy, BSh);

   switch (szB) {
      case 1: hFun  = (void*)&MC_(helperc_b_load1);
              hName = "MC_(helperc_b_load1)";
              break;
      case 2: hFun  = (void*)&MC_(helperc_b_load2);
              hName = "MC_(helperc_b_load2)";
              break;
      case 4: hFun  = (void*)&MC_(helperc_b_load4);
              hName = "MC_(helperc_b_load4)";
              break;
      case 8: hFun  = (void*)&MC_(helperc_b_load8);
              hName = "MC_(helperc_b_load8)";
              break;
      case 16: hFun  = (void*)&MC_(helperc_b_load16);
               hName = "MC_(helperc_b_load16)";
               break;
      case 32: hFun  = (void*)&MC_(helperc_b_load32);
               hName = "MC_(helperc_b_load32)";
               break;
      default:
         VG_(printf)("mc_translate.c: gen_load_b: unhandled szB == %d\n", szB);
         tl_assert(0);
   }
   di = unsafeIRDirty_1_N(
           bTmp, 1, hName, VG_(fnptr_to_fnentry)( hFun ),
           mkIRExprVec_1( ea )
        );
   if (guard) {
      di->guard = guard;
   }
   stmt( 'B', mce, IRStmt_Dirty(di) );
   if (mce->hWordTy == Ity_I64) {
      
      IRTemp bTmp32 = newTemp(mce, Ity_I32, BSh);
      assign( 'B', mce, bTmp32, unop(Iop_64to32, mkexpr(bTmp)) );
      return mkexpr(bTmp32);
   } else {
      
      return mkexpr(bTmp);
   }
}


static IRAtom* gen_load_b ( MCEnv* mce, Int szB, IRAtom* baseaddr,
                            Int offset )
{
   return gen_guarded_load_b(mce, szB, baseaddr, offset, NULL);
}


static
IRAtom* expr2ori_Load_guarded_General ( MCEnv* mce,
                                        IRType ty,
                                        IRAtom* addr, UInt bias,
                                        IRAtom* guard, IRAtom* balt )
{
   IRAtom* iftrue
      = assignNew('B', mce, Ity_I32,
                  gen_guarded_load_b(mce, sizeofIRType(ty),
                                     addr, bias, guard));
   IRAtom* iffalse 
      = balt;
   IRAtom* cond
      = guard == NULL  ? mkU1(1)  : guard;
   
   return assignNew('B', mce, Ity_I32, IRExpr_ITE(cond, iftrue, iffalse));
}


static void gen_store_b ( MCEnv* mce, Int szB,
                          IRAtom* baseaddr, Int offset, IRAtom* dataB,
                          IRAtom* guard )
{
   void*    hFun;
   const HChar* hName;
   IRDirty* di;
   IRType   aTy   = typeOfIRExpr( mce->sb->tyenv, baseaddr );
   IROp     opAdd = aTy == Ity_I32 ? Iop_Add32 : Iop_Add64;
   IRAtom*  ea    = baseaddr;
   if (guard) {
      tl_assert(isOriginalAtom(mce, guard));
      tl_assert(typeOfIRExpr(mce->sb->tyenv, guard) == Ity_I1);
   }
   if (offset != 0) {
      IRAtom* off = aTy == Ity_I32 ? mkU32( offset )
                                   : mkU64( (Long)(Int)offset );
      ea = assignNew(  'B', mce, aTy, binop(opAdd, ea, off));
   }
   if (mce->hWordTy == Ity_I64)
      dataB = assignNew( 'B', mce, Ity_I64, unop(Iop_32Uto64, dataB));

   switch (szB) {
      case 1: hFun  = (void*)&MC_(helperc_b_store1);
              hName = "MC_(helperc_b_store1)";
              break;
      case 2: hFun  = (void*)&MC_(helperc_b_store2);
              hName = "MC_(helperc_b_store2)";
              break;
      case 4: hFun  = (void*)&MC_(helperc_b_store4);
              hName = "MC_(helperc_b_store4)";
              break;
      case 8: hFun  = (void*)&MC_(helperc_b_store8);
              hName = "MC_(helperc_b_store8)";
              break;
      case 16: hFun  = (void*)&MC_(helperc_b_store16);
               hName = "MC_(helperc_b_store16)";
               break;
      case 32: hFun  = (void*)&MC_(helperc_b_store32);
               hName = "MC_(helperc_b_store32)";
               break;
      default:
         tl_assert(0);
   }
   di = unsafeIRDirty_0_N( 2,
           hName, VG_(fnptr_to_fnentry)( hFun ),
           mkIRExprVec_2( ea, dataB )
        );
   if (guard) di->guard = guard;
   stmt( 'B', mce, IRStmt_Dirty(di) );
}

static IRAtom* narrowTo32 ( MCEnv* mce, IRAtom* e ) {
   IRType eTy = typeOfIRExpr(mce->sb->tyenv, e);
   if (eTy == Ity_I64)
      return assignNew( 'B', mce, Ity_I32, unop(Iop_64to32, e) );
   if (eTy == Ity_I32)
      return e;
   tl_assert(0);
}

static IRAtom* zWidenFrom32 ( MCEnv* mce, IRType dstTy, IRAtom* e ) {
   IRType eTy = typeOfIRExpr(mce->sb->tyenv, e);
   tl_assert(eTy == Ity_I32);
   if (dstTy == Ity_I64)
      return assignNew( 'B', mce, Ity_I64, unop(Iop_32Uto64, e) );
   tl_assert(0);
}


static IRAtom* schemeE ( MCEnv* mce, IRExpr* e )
{
   tl_assert(MC_(clo_mc_level) == 3);

   switch (e->tag) {

      case Iex_GetI: {
         IRRegArray* descr_b;
         IRAtom      *t1, *t2, *t3, *t4;
         IRRegArray* descr      = e->Iex.GetI.descr;
         IRType equivIntTy 
            = MC_(get_otrack_reg_array_equiv_int_type)(descr);
         if (equivIntTy == Ity_INVALID)
            return mkU32(0);
         tl_assert(sizeofIRType(equivIntTy) >= 4);
         tl_assert(sizeofIRType(equivIntTy) == sizeofIRType(descr->elemTy));
         descr_b = mkIRRegArray( descr->base + 2*mce->layout->total_sizeB,
                                 equivIntTy, descr->nElems );
         t1 = assignNew( 'B', mce, equivIntTy, 
                          IRExpr_GetI( descr_b, e->Iex.GetI.ix, 
                                                e->Iex.GetI.bias ));
         t2 = narrowTo32( mce, t1 );
         t3 = schemeE( mce, e->Iex.GetI.ix );
         t4 = gen_maxU32( mce, t2, t3 );
         return t4;
      }
      case Iex_CCall: {
         Int i;
         IRAtom*  here;
         IRExpr** args = e->Iex.CCall.args;
         IRAtom*  curr = mkU32(0);
         for (i = 0; args[i]; i++) {
            tl_assert(i < 32);
            tl_assert(isOriginalAtom(mce, args[i]));
            if (e->Iex.CCall.cee->mcx_mask & (1<<i)) {
               if (0) VG_(printf)("excluding %s(%d)\n",
                                  e->Iex.CCall.cee->name, i);
            } else {
               here = schemeE( mce, args[i] );
               curr = gen_maxU32( mce, curr, here );
            }
         }
         return curr;
      }
      case Iex_Load: {
         Int dszB;
         dszB = sizeofIRType(e->Iex.Load.ty);
         tl_assert(isIRAtom(e->Iex.Load.addr));
         tl_assert(mce->hWordTy == Ity_I32 || mce->hWordTy == Ity_I64);
         return gen_load_b( mce, dszB, e->Iex.Load.addr, 0 );
      }
      case Iex_ITE: {
         IRAtom* b1 = schemeE( mce, e->Iex.ITE.cond );
         IRAtom* b3 = schemeE( mce, e->Iex.ITE.iftrue );
         IRAtom* b2 = schemeE( mce, e->Iex.ITE.iffalse );
         return gen_maxU32( mce, b1, gen_maxU32( mce, b2, b3 ));
      }
      case Iex_Qop: {
         IRAtom* b1 = schemeE( mce, e->Iex.Qop.details->arg1 );
         IRAtom* b2 = schemeE( mce, e->Iex.Qop.details->arg2 );
         IRAtom* b3 = schemeE( mce, e->Iex.Qop.details->arg3 );
         IRAtom* b4 = schemeE( mce, e->Iex.Qop.details->arg4 );
         return gen_maxU32( mce, gen_maxU32( mce, b1, b2 ),
                                 gen_maxU32( mce, b3, b4 ) );
      }
      case Iex_Triop: {
         IRAtom* b1 = schemeE( mce, e->Iex.Triop.details->arg1 );
         IRAtom* b2 = schemeE( mce, e->Iex.Triop.details->arg2 );
         IRAtom* b3 = schemeE( mce, e->Iex.Triop.details->arg3 );
         return gen_maxU32( mce, b1, gen_maxU32( mce, b2, b3 ) );
      }
      case Iex_Binop: {
         switch (e->Iex.Binop.op) {
            case Iop_CasCmpEQ8:  case Iop_CasCmpNE8:
            case Iop_CasCmpEQ16: case Iop_CasCmpNE16:
            case Iop_CasCmpEQ32: case Iop_CasCmpNE32:
            case Iop_CasCmpEQ64: case Iop_CasCmpNE64:
               return mkU32(0);
            default: {
               IRAtom* b1 = schemeE( mce, e->Iex.Binop.arg1 );
               IRAtom* b2 = schemeE( mce, e->Iex.Binop.arg2 );
               return gen_maxU32( mce, b1, b2 );
            }
         }
         tl_assert(0);
         
      }
      case Iex_Unop: {
         IRAtom* b1 = schemeE( mce, e->Iex.Unop.arg );
         return b1;
      }
      case Iex_Const:
         return mkU32(0);
      case Iex_RdTmp:
         return mkexpr( findShadowTmpB( mce, e->Iex.RdTmp.tmp ));
      case Iex_Get: {
         Int b_offset = MC_(get_otrack_shadow_offset)( 
                           e->Iex.Get.offset,
                           sizeofIRType(e->Iex.Get.ty) 
                        );
         tl_assert(b_offset >= -1
                   && b_offset <= mce->layout->total_sizeB -4);
         if (b_offset >= 0) {
            
            return IRExpr_Get( b_offset + 2*mce->layout->total_sizeB,
                               Ity_I32 );
         }
         return mkU32(0);
      }
      default:
         VG_(printf)("mc_translate.c: schemeE: unhandled: ");
         ppIRExpr(e); 
         VG_(tool_panic)("memcheck:schemeE");
   }
}


static void do_origins_Dirty ( MCEnv* mce, IRDirty* d )
{
   
   Int       i, k, n, toDo, gSz, gOff;
   IRAtom    *here, *curr;
   IRTemp    dst;

   
   curr = schemeE( mce, d->guard );

   

   for (i = 0; d->args[i]; i++) {
      IRAtom* arg = d->args[i];
      if ( (d->cee->mcx_mask & (1<<i))
           || UNLIKELY(is_IRExpr_VECRET_or_BBPTR(arg)) ) {
         
      } else {
         here = schemeE( mce, arg );
         curr = gen_maxU32( mce, curr, here );
      }
   }

   
   for (i = 0; i < d->nFxState; i++) {
      tl_assert(d->fxState[i].fx != Ifx_None);
      if (d->fxState[i].fx == Ifx_Write)
         continue;

      
      for (k = 0; k < 1 + d->fxState[i].nRepeats; k++) {
         gOff = d->fxState[i].offset + k * d->fxState[i].repeatLen;
         gSz  = d->fxState[i].size;

         
         if (isAlwaysDefd(mce, gOff, gSz)) {
            if (0)
            VG_(printf)("memcheck: Dirty gst: ignored off %d, sz %d\n",
                        gOff, gSz);
            continue;
         }

         while (True) {
            Int b_offset;
            tl_assert(gSz >= 0);
            if (gSz == 0) break;
            n = gSz <= 4 ? gSz : 4;
            b_offset = MC_(get_otrack_shadow_offset)(gOff, 4);
            if (b_offset != -1) {
               IRAtom *cond, *iffalse, *iftrue;

               cond = assignNew( 'B', mce, Ity_I1, d->guard);
               iffalse = mkU32(0);
               iftrue  = assignNew( 'B', mce, Ity_I32,
                                    IRExpr_Get(b_offset
                                                 + 2*mce->layout->total_sizeB,
                                               Ity_I32));
               here = assignNew( 'B', mce, Ity_I32,
                                 IRExpr_ITE(cond, iftrue, iffalse));
               curr = gen_maxU32( mce, curr, here );
            }
            gSz -= n;
            gOff += n;
         }
      }
   }

   

   if (d->mFx != Ifx_None) {
      tl_assert(d->mAddr);
      here = schemeE( mce, d->mAddr );
      curr = gen_maxU32( mce, curr, here );
   }

   
   if (d->mFx == Ifx_Read || d->mFx == Ifx_Modify) {
      toDo   = d->mSize;
      while (toDo >= 4) {
         here = gen_guarded_load_b( mce, 4, d->mAddr, d->mSize - toDo,
                                    d->guard );
         curr = gen_maxU32( mce, curr, here );
         toDo -= 4;
      }
      
      while (toDo >= 2) {
         here = gen_guarded_load_b( mce, 2, d->mAddr, d->mSize - toDo,
                                    d->guard );
         curr = gen_maxU32( mce, curr, here );
         toDo -= 2;
      }
      
      if (toDo == 1) {
         here = gen_guarded_load_b( mce, 1, d->mAddr, d->mSize - toDo,
                                    d->guard );
         curr = gen_maxU32( mce, curr, here );
         toDo -= 1;
      }
      tl_assert(toDo == 0);
   }


   
   if (d->tmp != IRTemp_INVALID) {
      dst   = findShadowTmpB(mce, d->tmp);
      assign( 'V', mce, dst, curr );
   }

   
   for (i = 0; i < d->nFxState; i++) {
      tl_assert(d->fxState[i].fx != Ifx_None);
      if (d->fxState[i].fx == Ifx_Read)
         continue;

      
      for (k = 0; k < 1 + d->fxState[i].nRepeats; k++) {
         gOff = d->fxState[i].offset + k * d->fxState[i].repeatLen;
         gSz  = d->fxState[i].size;

         
         if (isAlwaysDefd(mce, gOff, gSz))
            continue;

         /* This state element is written or modified.  So we need to
            consider it.  If larger than 4 bytes, deal with it in
            4-byte chunks. */
         while (True) {
            Int b_offset;
            tl_assert(gSz >= 0);
            if (gSz == 0) break;
            n = gSz <= 4 ? gSz : 4;
            
            b_offset = MC_(get_otrack_shadow_offset)(gOff, 4);
            if (b_offset != -1) {

               IRAtom *cond, *iffalse;

               cond    = assignNew('B', mce, Ity_I1,
                                   d->guard);
               iffalse = assignNew('B', mce, Ity_I32,
                                   IRExpr_Get(b_offset +
                                              2*mce->layout->total_sizeB,
                                              Ity_I32));
               curr = assignNew('V', mce, Ity_I32,
                                IRExpr_ITE(cond, curr, iffalse));

               stmt( 'B', mce, IRStmt_Put(b_offset
                                          + 2*mce->layout->total_sizeB,
                                          curr ));
            }
            gSz -= n;
            gOff += n;
         }
      }
   }

   if (d->mFx == Ifx_Write || d->mFx == Ifx_Modify) {
      toDo   = d->mSize;
      
      while (toDo >= 4) {
         gen_store_b( mce, 4, d->mAddr, d->mSize - toDo, curr,
                      d->guard );
         toDo -= 4;
      }
      
      while (toDo >= 2) {
         gen_store_b( mce, 2, d->mAddr, d->mSize - toDo, curr,
                      d->guard );
         toDo -= 2;
      }
      
      if (toDo == 1) {
         gen_store_b( mce, 1, d->mAddr, d->mSize - toDo, curr,
                      d->guard );
         toDo -= 1;
      }
      tl_assert(toDo == 0);
   }
}


static void do_origins_Store_guarded ( MCEnv* mce,
                                       IREndness stEnd,
                                       IRExpr* stAddr,
                                       IRExpr* stData,
                                       IRExpr* guard )
{
   Int     dszB;
   IRAtom* dataB;
   tl_assert(isIRAtom(stAddr));
   tl_assert(isIRAtom(stData));
   dszB  = sizeofIRType( typeOfIRExpr(mce->sb->tyenv, stData ) );
   dataB = schemeE( mce, stData );
   gen_store_b( mce, dszB, stAddr, 0, dataB, guard );
}


static void do_origins_Store_plain ( MCEnv* mce,
                                     IREndness stEnd,
                                     IRExpr* stAddr,
                                     IRExpr* stData )
{
   do_origins_Store_guarded ( mce, stEnd, stAddr, stData,
                              NULL );
}



static void do_origins_StoreG ( MCEnv* mce, IRStoreG* sg )
{
   do_origins_Store_guarded( mce, sg->end, sg->addr,
                             sg->data, sg->guard );
}

static void do_origins_LoadG ( MCEnv* mce, IRLoadG* lg )
{
   IRType loadedTy = Ity_INVALID;
   switch (lg->cvt) {
      case ILGop_Ident64: loadedTy = Ity_I64; break;
      case ILGop_Ident32: loadedTy = Ity_I32; break;
      case ILGop_16Uto32: loadedTy = Ity_I16; break;
      case ILGop_16Sto32: loadedTy = Ity_I16; break;
      case ILGop_8Uto32:  loadedTy = Ity_I8;  break;
      case ILGop_8Sto32:  loadedTy = Ity_I8;  break;
      default: VG_(tool_panic)("schemeS.IRLoadG");
   }
   IRAtom* ori_alt
      = schemeE( mce,lg->alt );
   IRAtom* ori_final
      = expr2ori_Load_guarded_General(mce, loadedTy,
                                      lg->addr, 0,
                                      lg->guard, ori_alt );
   
   assign( 'B', mce, findShadowTmpB(mce, lg->dst), ori_final );
}


static void schemeS ( MCEnv* mce, IRStmt* st )
{
   tl_assert(MC_(clo_mc_level) == 3);

   switch (st->tag) {

      case Ist_AbiHint:
         break;

      case Ist_PutI: {
         IRPutI *puti = st->Ist.PutI.details;
         IRRegArray* descr_b;
         IRAtom      *t1, *t2, *t3, *t4;
         IRRegArray* descr = puti->descr;
         IRType equivIntTy
            = MC_(get_otrack_reg_array_equiv_int_type)(descr);
         if (equivIntTy == Ity_INVALID)
            break;
         tl_assert(sizeofIRType(equivIntTy) >= 4);
         tl_assert(sizeofIRType(equivIntTy) == sizeofIRType(descr->elemTy));
         descr_b
            = mkIRRegArray( descr->base + 2*mce->layout->total_sizeB,
                            equivIntTy, descr->nElems );
         t1 = schemeE( mce, puti->data );
         t2 = schemeE( mce, puti->ix );
         t3 = gen_maxU32( mce, t1, t2 );
         t4 = zWidenFrom32( mce, equivIntTy, t3 );
         stmt( 'B', mce, IRStmt_PutI( mkIRPutI(descr_b, puti->ix,
                                               puti->bias, t4) ));
         break;
      }

      case Ist_Dirty:
         do_origins_Dirty( mce, st->Ist.Dirty.details );
         break;

      case Ist_Store:
         do_origins_Store_plain( mce, st->Ist.Store.end,
                                      st->Ist.Store.addr,
                                      st->Ist.Store.data );
         break;

      case Ist_StoreG:
         do_origins_StoreG( mce, st->Ist.StoreG.details );
         break;

      case Ist_LoadG:
         do_origins_LoadG( mce, st->Ist.LoadG.details );
         break;

      case Ist_LLSC: {
         if (st->Ist.LLSC.storedata == NULL) {
            
            IRType resTy 
               = typeOfIRTemp(mce->sb->tyenv, st->Ist.LLSC.result);
            IRExpr* vanillaLoad
               = IRExpr_Load(st->Ist.LLSC.end, resTy, st->Ist.LLSC.addr);
            tl_assert(resTy == Ity_I64 || resTy == Ity_I32
                      || resTy == Ity_I16 || resTy == Ity_I8);
            assign( 'B', mce, findShadowTmpB(mce, st->Ist.LLSC.result),
                              schemeE(mce, vanillaLoad));
         } else {
            
            do_origins_Store_plain( mce, st->Ist.LLSC.end,
                                    st->Ist.LLSC.addr,
                                    st->Ist.LLSC.storedata );
            assign( 'B', mce, findShadowTmpB(mce, st->Ist.LLSC.result),
                              mkU32(0) );
         }
         break;
      }

      case Ist_Put: {
         Int b_offset
            = MC_(get_otrack_shadow_offset)(
                 st->Ist.Put.offset,
                 sizeofIRType(typeOfIRExpr(mce->sb->tyenv, st->Ist.Put.data))
              );
         if (b_offset >= 0) {
            
            stmt( 'B', mce, IRStmt_Put(b_offset + 2*mce->layout->total_sizeB, 
                                       schemeE( mce, st->Ist.Put.data )) );
         }
         break;
      }

      case Ist_WrTmp:
         assign( 'B', mce, findShadowTmpB(mce, st->Ist.WrTmp.tmp),
                           schemeE(mce, st->Ist.WrTmp.data) );
         break;

      case Ist_MBE:
      case Ist_NoOp:
      case Ist_Exit:
      case Ist_IMark:
         break;

      default:
         VG_(printf)("mc_translate.c: schemeS: unhandled: ");
         ppIRStmt(st); 
         VG_(tool_panic)("memcheck:schemeS");
   }
}


