

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

#ifndef __LIBVEX_IR_H
#define __LIBVEX_IR_H

#include "libvex_basictypes.h"

   





typedef 
   enum { 
      Ity_INVALID=0x1100,
      Ity_I1, 
      Ity_I8, 
      Ity_I16, 
      Ity_I32, 
      Ity_I64,
      Ity_I128,  
      Ity_F16,   
      Ity_F32,   
      Ity_F64,   
      Ity_D32,   
      Ity_D64,   
      Ity_D128,  
      Ity_F128,  
      Ity_V128,  
      Ity_V256   
   }
   IRType;

extern void ppIRType ( IRType );

 
extern Int sizeofIRType ( IRType );

extern IRType integerIRTypeOfSize ( Int szB );



typedef
   enum { 
      Iend_LE=0x1200, 
      Iend_BE          
   }
   IREndness;




typedef
   enum { 
      Ico_U1=0x1300,
      Ico_U8, 
      Ico_U16, 
      Ico_U32, 
      Ico_U64,
      Ico_F32,   
      Ico_F32i,  
      Ico_F64,   
      Ico_F64i,  
      Ico_V128,  
      Ico_V256   
   }
   IRConstTag;

typedef
   struct _IRConst {
      IRConstTag tag;
      union {
         Bool   U1;
         UChar  U8;
         UShort U16;
         UInt   U32;
         ULong  U64;
         Float  F32;
         UInt   F32i;
         Double F64;
         ULong  F64i;
         UShort V128;   
         UInt   V256;   
      } Ico;
   }
   IRConst;

extern IRConst* IRConst_U1   ( Bool );
extern IRConst* IRConst_U8   ( UChar );
extern IRConst* IRConst_U16  ( UShort );
extern IRConst* IRConst_U32  ( UInt );
extern IRConst* IRConst_U64  ( ULong );
extern IRConst* IRConst_F32  ( Float );
extern IRConst* IRConst_F32i ( UInt );
extern IRConst* IRConst_F64  ( Double );
extern IRConst* IRConst_F64i ( ULong );
extern IRConst* IRConst_V128 ( UShort );
extern IRConst* IRConst_V256 ( UInt );

extern IRConst* deepCopyIRConst ( const IRConst* );

extern void ppIRConst ( const IRConst* );

extern Bool eqIRConst ( const IRConst*, const IRConst* );




typedef
   struct {
      Int          regparms;
      const HChar* name;
      void*        addr;
      UInt         mcx_mask;
   }
   IRCallee;

extern IRCallee* mkIRCallee ( Int regparms, const HChar* name, void* addr );

extern IRCallee* deepCopyIRCallee ( const IRCallee* );

extern void ppIRCallee ( const IRCallee* );



typedef
   struct {
      Int    base;   
      IRType elemTy; 
      Int    nElems; 
   }
   IRRegArray;

extern IRRegArray* mkIRRegArray ( Int, IRType, Int );

extern IRRegArray* deepCopyIRRegArray ( const IRRegArray* );

extern void ppIRRegArray ( const IRRegArray* );
extern Bool eqIRRegArray ( const IRRegArray*, const IRRegArray* );



typedef UInt IRTemp;

extern void ppIRTemp ( IRTemp );

#define IRTemp_INVALID ((IRTemp)0xFFFFFFFF)



typedef
   enum { 

      Iop_INVALID=0x1400,
      Iop_Add8,  Iop_Add16,  Iop_Add32,  Iop_Add64,
      Iop_Sub8,  Iop_Sub16,  Iop_Sub32,  Iop_Sub64,
      
      Iop_Mul8,  Iop_Mul16,  Iop_Mul32,  Iop_Mul64,
      Iop_Or8,   Iop_Or16,   Iop_Or32,   Iop_Or64,
      Iop_And8,  Iop_And16,  Iop_And32,  Iop_And64,
      Iop_Xor8,  Iop_Xor16,  Iop_Xor32,  Iop_Xor64,
      Iop_Shl8,  Iop_Shl16,  Iop_Shl32,  Iop_Shl64,
      Iop_Shr8,  Iop_Shr16,  Iop_Shr32,  Iop_Shr64,
      Iop_Sar8,  Iop_Sar16,  Iop_Sar32,  Iop_Sar64,
      
      Iop_CmpEQ8,  Iop_CmpEQ16,  Iop_CmpEQ32,  Iop_CmpEQ64,
      Iop_CmpNE8,  Iop_CmpNE16,  Iop_CmpNE32,  Iop_CmpNE64,
      
      Iop_Not8,  Iop_Not16,  Iop_Not32,  Iop_Not64,

      Iop_CasCmpEQ8, Iop_CasCmpEQ16, Iop_CasCmpEQ32, Iop_CasCmpEQ64,
      Iop_CasCmpNE8, Iop_CasCmpNE16, Iop_CasCmpNE32, Iop_CasCmpNE64,

      Iop_ExpCmpNE8, Iop_ExpCmpNE16, Iop_ExpCmpNE32, Iop_ExpCmpNE64,

      

      
      Iop_MullS8, Iop_MullS16, Iop_MullS32, Iop_MullS64,
      Iop_MullU8, Iop_MullU16, Iop_MullU32, Iop_MullU64,

      
      Iop_Clz64, Iop_Clz32,   
      Iop_Ctz64, Iop_Ctz32,   

      
      Iop_CmpLT32S, Iop_CmpLT64S,
      Iop_CmpLE32S, Iop_CmpLE64S,
      Iop_CmpLT32U, Iop_CmpLT64U,
      Iop_CmpLE32U, Iop_CmpLE64U,

      
      Iop_CmpNEZ8, Iop_CmpNEZ16,  Iop_CmpNEZ32,  Iop_CmpNEZ64,
      Iop_CmpwNEZ32, Iop_CmpwNEZ64, 
      Iop_Left8, Iop_Left16, Iop_Left32, Iop_Left64, 
      Iop_Max32U, 

      Iop_CmpORD32U, Iop_CmpORD64U,
      Iop_CmpORD32S, Iop_CmpORD64S,

      
      
      Iop_DivU32,   
      Iop_DivS32,   
      Iop_DivU64,   
      Iop_DivS64,   
      Iop_DivU64E,  
                    
      Iop_DivS64E,  
      Iop_DivU32E,  
                    
      Iop_DivS32E,  

      Iop_DivModU64to32, 
                         
      Iop_DivModS64to32, 

      Iop_DivModU128to64, 
                          
      Iop_DivModS128to64, 

      Iop_DivModS64to64, 
                         


      
      Iop_8Uto16, Iop_8Uto32,  Iop_8Uto64,
                  Iop_16Uto32, Iop_16Uto64,
                               Iop_32Uto64,
      Iop_8Sto16, Iop_8Sto32,  Iop_8Sto64,
                  Iop_16Sto32, Iop_16Sto64,
                               Iop_32Sto64,

      
      Iop_64to8, Iop_32to8, Iop_64to16,
      
      Iop_16to8,      
      Iop_16HIto8,    
      Iop_8HLto16,    
      
      Iop_32to16,     
      Iop_32HIto16,   
      Iop_16HLto32,   
      
      Iop_64to32,     
      Iop_64HIto32,   
      Iop_32HLto64,   
      
      Iop_128to64,    
      Iop_128HIto64,  
      Iop_64HLto128,  
      
      Iop_Not1,   
      Iop_32to1,  
      Iop_64to1,  
      Iop_1Uto8,  
      Iop_1Uto32, 
      Iop_1Uto64, 
      Iop_1Sto8,  
      Iop_1Sto16, 
      Iop_1Sto32, 
      Iop_1Sto64, 

      

      

      
       
      Iop_AddF64, Iop_SubF64, Iop_MulF64, Iop_DivF64,

       
      Iop_AddF32, Iop_SubF32, Iop_MulF32, Iop_DivF32,

       
      Iop_AddF64r32, Iop_SubF64r32, Iop_MulF64r32, Iop_DivF64r32, 

      
      
      Iop_NegF64, Iop_AbsF64,

      
      Iop_NegF32, Iop_AbsF32,

      
      
      Iop_SqrtF64,

      
      Iop_SqrtF32,

      
      Iop_CmpF64,
      Iop_CmpF32,
      Iop_CmpF128,

      

      Iop_F64toI16S, 
      Iop_F64toI32S, 
      Iop_F64toI64S, 
      Iop_F64toI64U, 

      Iop_F64toI32U, 

      Iop_I32StoF64, 
      Iop_I64StoF64, 
      Iop_I64UtoF64, 
      Iop_I64UtoF32, 

      Iop_I32UtoF32, 
      Iop_I32UtoF64, 

      Iop_F32toI32S, 
      Iop_F32toI64S, 
      Iop_F32toI32U, 
      Iop_F32toI64U, 

      Iop_I32StoF32, 
      Iop_I64StoF32, 

      
      Iop_F32toF64,  
      Iop_F64toF32,  

      Iop_ReinterpF64asI64, Iop_ReinterpI64asF64,
      Iop_ReinterpF32asI32, Iop_ReinterpI32asF32,

      
      Iop_F64HLtoF128,
      Iop_F128HItoF64,
      Iop_F128LOtoF64,

      
      Iop_AddF128, Iop_SubF128, Iop_MulF128, Iop_DivF128,

      
      Iop_NegF128, Iop_AbsF128,

      
      Iop_SqrtF128,

      Iop_I32StoF128, 
      Iop_I64StoF128, 
      Iop_I32UtoF128, 
      Iop_I64UtoF128, 
      Iop_F32toF128,  
      Iop_F64toF128,  

      Iop_F128toI32S, 
      Iop_F128toI64S, 
      Iop_F128toI32U, 
      Iop_F128toI64U, 
      Iop_F128toF64,  
      Iop_F128toF32,  

      

      
       
      Iop_AtanF64,       
      Iop_Yl2xF64,       
      Iop_Yl2xp1F64,     
      Iop_PRemF64,       
      Iop_PRemC3210F64,  
      Iop_PRem1F64,      
      Iop_PRem1C3210F64, 
      Iop_ScaleF64,      

      
      
      Iop_SinF64,    
      Iop_CosF64,    
      Iop_TanF64,    
      Iop_2xm1F64,   
      Iop_RoundF64toInt, 
      Iop_RoundF32toInt, 

      

      
 
      Iop_MAddF32, Iop_MSubF32,

      

      
 
      Iop_MAddF64, Iop_MSubF64,

       
      Iop_MAddF64r32, Iop_MSubF64r32,

      
      Iop_RSqrtEst5GoodF64, 
      Iop_RoundF64toF64_NEAREST, 
      Iop_RoundF64toF64_NegINF,   
      Iop_RoundF64toF64_PosINF,  
      Iop_RoundF64toF64_ZERO,    

      
      Iop_TruncF64asF32, 

      
      Iop_RoundF64toF32, 

      

      Iop_RecpExpF64,  
      Iop_RecpExpF32,  

      

      Iop_F16toF64,  
      Iop_F64toF16,  

      Iop_F16toF32,  
      Iop_F32toF16,  

      

      
      Iop_QAdd32S,
      Iop_QSub32S,

      
      Iop_Add16x2, Iop_Sub16x2,
      Iop_QAdd16Sx2, Iop_QAdd16Ux2,
      Iop_QSub16Sx2, Iop_QSub16Ux2,

      Iop_HAdd16Ux2, Iop_HAdd16Sx2,
      Iop_HSub16Ux2, Iop_HSub16Sx2,

      
      Iop_Add8x4, Iop_Sub8x4,
      Iop_QAdd8Sx4, Iop_QAdd8Ux4,
      Iop_QSub8Sx4, Iop_QSub8Ux4,

      Iop_HAdd8Ux4, Iop_HAdd8Sx4,
      Iop_HSub8Ux4, Iop_HSub8Sx4,

      
      Iop_Sad8Ux4,

      
      Iop_CmpNEZ16x2, Iop_CmpNEZ8x4,

      

      
      Iop_I32UtoFx2,  Iop_I32StoFx2,    
      Iop_FtoI32Ux2_RZ,  Iop_FtoI32Sx2_RZ,    
      Iop_F32ToFixed32Ux2_RZ, Iop_F32ToFixed32Sx2_RZ, 
      Iop_Fixed32UToF32x2_RN, Iop_Fixed32SToF32x2_RN, 

      
      Iop_Max32Fx2,      Iop_Min32Fx2,
      Iop_PwMax32Fx2,    Iop_PwMin32Fx2,
      Iop_CmpEQ32Fx2, Iop_CmpGT32Fx2, Iop_CmpGE32Fx2,

      Iop_RecipEst32Fx2,

      Iop_RecipStep32Fx2,

      Iop_RSqrtEst32Fx2,

      Iop_RSqrtStep32Fx2,

      
      Iop_Neg32Fx2, Iop_Abs32Fx2,

      

      
      Iop_CmpNEZ8x8, Iop_CmpNEZ16x4, Iop_CmpNEZ32x2,

      
      Iop_Add8x8,   Iop_Add16x4,   Iop_Add32x2,
      Iop_QAdd8Ux8, Iop_QAdd16Ux4, Iop_QAdd32Ux2, Iop_QAdd64Ux1,
      Iop_QAdd8Sx8, Iop_QAdd16Sx4, Iop_QAdd32Sx2, Iop_QAdd64Sx1,

      
      Iop_PwAdd8x8,  Iop_PwAdd16x4,  Iop_PwAdd32x2,
      Iop_PwMax8Sx8, Iop_PwMax16Sx4, Iop_PwMax32Sx2,
      Iop_PwMax8Ux8, Iop_PwMax16Ux4, Iop_PwMax32Ux2,
      Iop_PwMin8Sx8, Iop_PwMin16Sx4, Iop_PwMin32Sx2,
      Iop_PwMin8Ux8, Iop_PwMin16Ux4, Iop_PwMin32Ux2,
      Iop_PwAddL8Ux8, Iop_PwAddL16Ux4, Iop_PwAddL32Ux2,
      Iop_PwAddL8Sx8, Iop_PwAddL16Sx4, Iop_PwAddL32Sx2,

      
      Iop_Sub8x8,   Iop_Sub16x4,   Iop_Sub32x2,
      Iop_QSub8Ux8, Iop_QSub16Ux4, Iop_QSub32Ux2, Iop_QSub64Ux1,
      Iop_QSub8Sx8, Iop_QSub16Sx4, Iop_QSub32Sx2, Iop_QSub64Sx1,

      
      Iop_Abs8x8, Iop_Abs16x4, Iop_Abs32x2,

      
      Iop_Mul8x8, Iop_Mul16x4, Iop_Mul32x2,
      Iop_Mul32Fx2,
      Iop_MulHi16Ux4,
      Iop_MulHi16Sx4,
      Iop_PolynomialMul8x8,

      Iop_QDMulHi16Sx4, Iop_QDMulHi32Sx2,
      Iop_QRDMulHi16Sx4, Iop_QRDMulHi32Sx2,

      
      Iop_Avg8Ux8,
      Iop_Avg16Ux4,

      
      Iop_Max8Sx8, Iop_Max16Sx4, Iop_Max32Sx2,
      Iop_Max8Ux8, Iop_Max16Ux4, Iop_Max32Ux2,
      Iop_Min8Sx8, Iop_Min16Sx4, Iop_Min32Sx2,
      Iop_Min8Ux8, Iop_Min16Ux4, Iop_Min32Ux2,

      
      Iop_CmpEQ8x8,  Iop_CmpEQ16x4,  Iop_CmpEQ32x2,
      Iop_CmpGT8Ux8, Iop_CmpGT16Ux4, Iop_CmpGT32Ux2,
      Iop_CmpGT8Sx8, Iop_CmpGT16Sx4, Iop_CmpGT32Sx2,

      Iop_Cnt8x8,
      Iop_Clz8x8, Iop_Clz16x4, Iop_Clz32x2,
      Iop_Cls8x8, Iop_Cls16x4, Iop_Cls32x2,
      Iop_Clz64x2,

      
      Iop_Shl8x8, Iop_Shl16x4, Iop_Shl32x2,
      Iop_Shr8x8, Iop_Shr16x4, Iop_Shr32x2,
      Iop_Sar8x8, Iop_Sar16x4, Iop_Sar32x2,
      Iop_Sal8x8, Iop_Sal16x4, Iop_Sal32x2, Iop_Sal64x1,

      
      Iop_ShlN8x8, Iop_ShlN16x4, Iop_ShlN32x2,
      Iop_ShrN8x8, Iop_ShrN16x4, Iop_ShrN32x2,
      Iop_SarN8x8, Iop_SarN16x4, Iop_SarN32x2,

      
      Iop_QShl8x8, Iop_QShl16x4, Iop_QShl32x2, Iop_QShl64x1,
      Iop_QSal8x8, Iop_QSal16x4, Iop_QSal32x2, Iop_QSal64x1,
      
      Iop_QShlNsatSU8x8,  Iop_QShlNsatSU16x4,
      Iop_QShlNsatSU32x2, Iop_QShlNsatSU64x1,
      Iop_QShlNsatUU8x8,  Iop_QShlNsatUU16x4,
      Iop_QShlNsatUU32x2, Iop_QShlNsatUU64x1,
      Iop_QShlNsatSS8x8,  Iop_QShlNsatSS16x4,
      Iop_QShlNsatSS32x2, Iop_QShlNsatSS64x1,

      Iop_QNarrowBin16Sto8Ux8,
      Iop_QNarrowBin16Sto8Sx8, Iop_QNarrowBin32Sto16Sx4,
      Iop_NarrowBin16to8x8,    Iop_NarrowBin32to16x4,

      
      Iop_InterleaveHI8x8, Iop_InterleaveHI16x4, Iop_InterleaveHI32x2,
      Iop_InterleaveLO8x8, Iop_InterleaveLO16x4, Iop_InterleaveLO32x2,
      Iop_InterleaveOddLanes8x8, Iop_InterleaveEvenLanes8x8,
      Iop_InterleaveOddLanes16x4, Iop_InterleaveEvenLanes16x4,

      Iop_CatOddLanes8x8, Iop_CatOddLanes16x4,
      Iop_CatEvenLanes8x8, Iop_CatEvenLanes16x4,

      
      Iop_GetElem8x8, Iop_GetElem16x4, Iop_GetElem32x2,
      Iop_SetElem8x8, Iop_SetElem16x4, Iop_SetElem32x2,

      
      Iop_Dup8x8,   Iop_Dup16x4,   Iop_Dup32x2,

      Iop_Slice64,  

      Iop_Reverse8sIn16_x4,
      Iop_Reverse8sIn32_x2, Iop_Reverse16sIn32_x2,
      Iop_Reverse8sIn64_x1, Iop_Reverse16sIn64_x1, Iop_Reverse32sIn64_x1,

      Iop_Perm8x8,

      Iop_GetMSBs8x8, 

      Iop_RecipEst32Ux2, Iop_RSqrtEst32Ux2,

      

      Iop_AddD64, Iop_SubD64, Iop_MulD64, Iop_DivD64,

      Iop_AddD128, Iop_SubD128, Iop_MulD128, Iop_DivD128,

      Iop_ShlD64, Iop_ShrD64,

      
      Iop_ShlD128, Iop_ShrD128,


      Iop_D32toD64,

      
      Iop_D64toD128, 

      
      Iop_I32StoD128,

      
      Iop_I32UtoD128,

      
      Iop_I64StoD128, 

      
      Iop_I64UtoD128,

      
      Iop_D64toD32,

      
      Iop_D128toD64,

      
      Iop_I32StoD64,

      
      Iop_I32UtoD64,

      
      Iop_I64StoD64,

      
      Iop_I64UtoD64,

      
      Iop_D64toI32S,

      
      Iop_D64toI32U,

      
      Iop_D64toI64S,

      
      Iop_D64toI64U,

      
      Iop_D128toI32S,

      
      Iop_D128toI32U,

      
      Iop_D128toI64S,

      
      Iop_D128toI64U,

      
      Iop_F32toD32,

      
      Iop_F32toD64,

      
      Iop_F32toD128,

      
      Iop_F64toD32,

      
      Iop_F64toD64,

      
      Iop_F64toD128,

      
      Iop_F128toD32,

      
      Iop_F128toD64,

      
      Iop_F128toD128,

      
      Iop_D32toF32,

      
      Iop_D32toF64,

      
      Iop_D32toF128,

      
      Iop_D64toF32,

      
      Iop_D64toF64,

      
      Iop_D64toF128,

      
      Iop_D128toF32,

      
      Iop_D128toF64,

      
      Iop_D128toF128,

      Iop_RoundD64toInt,

      
      Iop_RoundD128toInt,

      Iop_CmpD64,

      
      Iop_CmpD128,

      Iop_CmpExpD64,

      
      Iop_CmpExpD128,

      Iop_QuantizeD64,

      
      Iop_QuantizeD128,

      Iop_SignificanceRoundD64,

      
      Iop_SignificanceRoundD128,

      Iop_ExtractExpD64,

      
      Iop_ExtractExpD128,

      Iop_ExtractSigD64,

      
      Iop_ExtractSigD128,

      Iop_InsertExpD64,

      
      Iop_InsertExpD128,

      
      Iop_D64HLtoD128, Iop_D128HItoD64, Iop_D128LOtoD64,

      Iop_DPBtoBCD,

      Iop_BCDtoDPB,

      Iop_BCDAdd, Iop_BCDSub,

      
      Iop_ReinterpI64asD64,

      
      Iop_ReinterpD64asI64,

      

      

      
      Iop_Add32Fx4, Iop_Sub32Fx4, Iop_Mul32Fx4, Iop_Div32Fx4, 

      
      Iop_Max32Fx4, Iop_Min32Fx4,
      Iop_Add32Fx2, Iop_Sub32Fx2,
      Iop_CmpEQ32Fx4, Iop_CmpLT32Fx4, Iop_CmpLE32Fx4, Iop_CmpUN32Fx4,
      Iop_CmpGT32Fx4, Iop_CmpGE32Fx4,

      
      Iop_PwMax32Fx4, Iop_PwMin32Fx4,

      
      Iop_Abs32Fx4,
      Iop_Neg32Fx4,

      
      Iop_Sqrt32Fx4,

      Iop_RecipEst32Fx4,

      Iop_RecipStep32Fx4,

      Iop_RSqrtEst32Fx4,

      Iop_RSqrtStep32Fx4,

      
      Iop_I32UtoFx4,     Iop_I32StoFx4,       
      Iop_FtoI32Ux4_RZ,  Iop_FtoI32Sx4_RZ,    
      Iop_QFtoI32Ux4_RZ, Iop_QFtoI32Sx4_RZ,   
      Iop_RoundF32x4_RM, Iop_RoundF32x4_RP,   
      Iop_RoundF32x4_RN, Iop_RoundF32x4_RZ,   
      Iop_F32ToFixed32Ux4_RZ, Iop_F32ToFixed32Sx4_RZ, 
      Iop_Fixed32UToF32x4_RN, Iop_Fixed32SToF32x4_RN, 

      
      
      Iop_F32toF16x4, Iop_F16toF32x4,         

      


      
      Iop_Add32F0x4, Iop_Sub32F0x4, Iop_Mul32F0x4, Iop_Div32F0x4, 
      Iop_Max32F0x4, Iop_Min32F0x4,
      Iop_CmpEQ32F0x4, Iop_CmpLT32F0x4, Iop_CmpLE32F0x4, Iop_CmpUN32F0x4, 

      
      Iop_RecipEst32F0x4, Iop_Sqrt32F0x4, Iop_RSqrtEst32F0x4,

      

      
      Iop_Add64Fx2, Iop_Sub64Fx2, Iop_Mul64Fx2, Iop_Div64Fx2, 

      
      Iop_Max64Fx2, Iop_Min64Fx2,
      Iop_CmpEQ64Fx2, Iop_CmpLT64Fx2, Iop_CmpLE64Fx2, Iop_CmpUN64Fx2, 

      
      Iop_Abs64Fx2,
      Iop_Neg64Fx2,

      
      Iop_Sqrt64Fx2,

      
      Iop_RecipEst64Fx2,    
      Iop_RecipStep64Fx2,   
      Iop_RSqrtEst64Fx2,    
      Iop_RSqrtStep64Fx2,   

      


      
      Iop_Add64F0x2, Iop_Sub64F0x2, Iop_Mul64F0x2, Iop_Div64F0x2, 
      Iop_Max64F0x2, Iop_Min64F0x2,
      Iop_CmpEQ64F0x2, Iop_CmpLT64F0x2, Iop_CmpLE64F0x2, Iop_CmpUN64F0x2, 

      
      Iop_Sqrt64F0x2,

      

      
      Iop_V128to64,     
      Iop_V128HIto64,   
      Iop_64HLtoV128,   

      Iop_64UtoV128,
      Iop_SetV128lo64,

      
      Iop_ZeroHI64ofV128,    
      Iop_ZeroHI96ofV128,    
      Iop_ZeroHI112ofV128,   
      Iop_ZeroHI120ofV128,   

      
      Iop_32UtoV128,
      Iop_V128to32,     
      Iop_SetV128lo32,  

      

      
      Iop_NotV128,
      Iop_AndV128, Iop_OrV128, Iop_XorV128, 

      
      Iop_ShlV128, Iop_ShrV128,

      
      Iop_CmpNEZ8x16, Iop_CmpNEZ16x8, Iop_CmpNEZ32x4, Iop_CmpNEZ64x2,

      
      Iop_Add8x16,    Iop_Add16x8,    Iop_Add32x4,    Iop_Add64x2,
      Iop_QAdd8Ux16,  Iop_QAdd16Ux8,  Iop_QAdd32Ux4,  Iop_QAdd64Ux2,
      Iop_QAdd8Sx16,  Iop_QAdd16Sx8,  Iop_QAdd32Sx4,  Iop_QAdd64Sx2,

      
      Iop_QAddExtUSsatSS8x16, Iop_QAddExtUSsatSS16x8,
      Iop_QAddExtUSsatSS32x4, Iop_QAddExtUSsatSS64x2,
      Iop_QAddExtSUsatUU8x16, Iop_QAddExtSUsatUU16x8,
      Iop_QAddExtSUsatUU32x4, Iop_QAddExtSUsatUU64x2,

      
      Iop_Sub8x16,   Iop_Sub16x8,   Iop_Sub32x4,   Iop_Sub64x2,
      Iop_QSub8Ux16, Iop_QSub16Ux8, Iop_QSub32Ux4, Iop_QSub64Ux2,
      Iop_QSub8Sx16, Iop_QSub16Sx8, Iop_QSub32Sx4, Iop_QSub64Sx2,

      
      Iop_Mul8x16,  Iop_Mul16x8,    Iop_Mul32x4,
                    Iop_MulHi16Ux8, Iop_MulHi32Ux4,
                    Iop_MulHi16Sx8, Iop_MulHi32Sx4,
      
      Iop_MullEven8Ux16, Iop_MullEven16Ux8, Iop_MullEven32Ux4,
      Iop_MullEven8Sx16, Iop_MullEven16Sx8, Iop_MullEven32Sx4,

      
      Iop_Mull8Ux8, Iop_Mull8Sx8,
      Iop_Mull16Ux4, Iop_Mull16Sx4,
      Iop_Mull32Ux2, Iop_Mull32Sx2,

      
      Iop_QDMull16Sx4, Iop_QDMull32Sx2,

      Iop_QDMulHi16Sx8,  Iop_QDMulHi32Sx4,  
      Iop_QRDMulHi16Sx8, Iop_QRDMulHi32Sx4, 

      Iop_PolynomialMul8x16, 
      Iop_PolynomialMull8x8, 

      Iop_PolynomialMulAdd8x16, Iop_PolynomialMulAdd16x8,
      Iop_PolynomialMulAdd32x4, Iop_PolynomialMulAdd64x2,

      
      Iop_PwAdd8x16, Iop_PwAdd16x8, Iop_PwAdd32x4,
      Iop_PwAdd32Fx2,
      Iop_PwAddL8Ux16, Iop_PwAddL16Ux8, Iop_PwAddL32Ux4,
      Iop_PwAddL8Sx16, Iop_PwAddL16Sx8, Iop_PwAddL32Sx4,

      

      
      Iop_PwBitMtxXpose64x2,

      
      Iop_Abs8x16, Iop_Abs16x8, Iop_Abs32x4, Iop_Abs64x2,

      
      Iop_Avg8Ux16, Iop_Avg16Ux8, Iop_Avg32Ux4,
      Iop_Avg8Sx16, Iop_Avg16Sx8, Iop_Avg32Sx4,

      
      Iop_Max8Sx16, Iop_Max16Sx8, Iop_Max32Sx4, Iop_Max64Sx2,
      Iop_Max8Ux16, Iop_Max16Ux8, Iop_Max32Ux4, Iop_Max64Ux2,
      Iop_Min8Sx16, Iop_Min16Sx8, Iop_Min32Sx4, Iop_Min64Sx2,
      Iop_Min8Ux16, Iop_Min16Ux8, Iop_Min32Ux4, Iop_Min64Ux2,

      
      Iop_CmpEQ8x16,  Iop_CmpEQ16x8,  Iop_CmpEQ32x4,  Iop_CmpEQ64x2,
      Iop_CmpGT8Sx16, Iop_CmpGT16Sx8, Iop_CmpGT32Sx4, Iop_CmpGT64Sx2,
      Iop_CmpGT8Ux16, Iop_CmpGT16Ux8, Iop_CmpGT32Ux4, Iop_CmpGT64Ux2,

      Iop_Cnt8x16,
      Iop_Clz8x16, Iop_Clz16x8, Iop_Clz32x4,
      Iop_Cls8x16, Iop_Cls16x8, Iop_Cls32x4,

      
      Iop_ShlN8x16, Iop_ShlN16x8, Iop_ShlN32x4, Iop_ShlN64x2,
      Iop_ShrN8x16, Iop_ShrN16x8, Iop_ShrN32x4, Iop_ShrN64x2,
      Iop_SarN8x16, Iop_SarN16x8, Iop_SarN32x4, Iop_SarN64x2,

      
      Iop_Shl8x16, Iop_Shl16x8, Iop_Shl32x4, Iop_Shl64x2,
      Iop_Shr8x16, Iop_Shr16x8, Iop_Shr32x4, Iop_Shr64x2,
      Iop_Sar8x16, Iop_Sar16x8, Iop_Sar32x4, Iop_Sar64x2,
      Iop_Sal8x16, Iop_Sal16x8, Iop_Sal32x4, Iop_Sal64x2,
      Iop_Rol8x16, Iop_Rol16x8, Iop_Rol32x4, Iop_Rol64x2,

      
      Iop_QShl8x16, Iop_QShl16x8, Iop_QShl32x4, Iop_QShl64x2,
      Iop_QSal8x16, Iop_QSal16x8, Iop_QSal32x4, Iop_QSal64x2,
      
      Iop_QShlNsatSU8x16, Iop_QShlNsatSU16x8,
      Iop_QShlNsatSU32x4, Iop_QShlNsatSU64x2,
      Iop_QShlNsatUU8x16, Iop_QShlNsatUU16x8,
      Iop_QShlNsatUU32x4, Iop_QShlNsatUU64x2,
      Iop_QShlNsatSS8x16, Iop_QShlNsatSS16x8,
      Iop_QShlNsatSS32x4, Iop_QShlNsatSS64x2,

      
      
      
      Iop_QandUQsh8x16, Iop_QandUQsh16x8,
      Iop_QandUQsh32x4, Iop_QandUQsh64x2,
      
      Iop_QandSQsh8x16, Iop_QandSQsh16x8,
      Iop_QandSQsh32x4, Iop_QandSQsh64x2,

      
      Iop_QandUQRsh8x16, Iop_QandUQRsh16x8,
      Iop_QandUQRsh32x4, Iop_QandUQRsh64x2,
      
      Iop_QandSQRsh8x16, Iop_QandSQRsh16x8,
      Iop_QandSQRsh32x4, Iop_QandSQRsh64x2,

      
      
      
      Iop_Sh8Sx16, Iop_Sh16Sx8, Iop_Sh32Sx4, Iop_Sh64Sx2,
      Iop_Sh8Ux16, Iop_Sh16Ux8, Iop_Sh32Ux4, Iop_Sh64Ux2,

      
      Iop_Rsh8Sx16, Iop_Rsh16Sx8, Iop_Rsh32Sx4, Iop_Rsh64Sx2,
      Iop_Rsh8Ux16, Iop_Rsh16Ux8, Iop_Rsh32Ux4, Iop_Rsh64Ux2,


      
      
      
      Iop_QandQShrNnarrow16Uto8Ux8,
      Iop_QandQShrNnarrow32Uto16Ux4, Iop_QandQShrNnarrow64Uto32Ux2,
      
      Iop_QandQSarNnarrow16Sto8Sx8,
      Iop_QandQSarNnarrow32Sto16Sx4, Iop_QandQSarNnarrow64Sto32Sx2,
      
      Iop_QandQSarNnarrow16Sto8Ux8,
      Iop_QandQSarNnarrow32Sto16Ux4, Iop_QandQSarNnarrow64Sto32Ux2,

      
      Iop_QandQRShrNnarrow16Uto8Ux8,
      Iop_QandQRShrNnarrow32Uto16Ux4, Iop_QandQRShrNnarrow64Uto32Ux2,
      
      Iop_QandQRSarNnarrow16Sto8Sx8,
      Iop_QandQRSarNnarrow32Sto16Sx4, Iop_QandQRSarNnarrow64Sto32Sx2,
      
      Iop_QandQRSarNnarrow16Sto8Ux8,
      Iop_QandQRSarNnarrow32Sto16Ux4, Iop_QandQRSarNnarrow64Sto32Ux2,

      
      Iop_QNarrowBin16Sto8Ux16, Iop_QNarrowBin32Sto16Ux8,
      Iop_QNarrowBin16Sto8Sx16, Iop_QNarrowBin32Sto16Sx8,
      Iop_QNarrowBin16Uto8Ux16, Iop_QNarrowBin32Uto16Ux8,
      Iop_NarrowBin16to8x16, Iop_NarrowBin32to16x8,
      Iop_QNarrowBin64Sto32Sx4, Iop_QNarrowBin64Uto32Ux4,
      Iop_NarrowBin64to32x4,

      
      Iop_NarrowUn16to8x8, Iop_NarrowUn32to16x4, Iop_NarrowUn64to32x2,
      Iop_QNarrowUn16Sto8Sx8, Iop_QNarrowUn32Sto16Sx4, Iop_QNarrowUn64Sto32Sx2,
      Iop_QNarrowUn16Sto8Ux8, Iop_QNarrowUn32Sto16Ux4, Iop_QNarrowUn64Sto32Ux2,
      
      Iop_QNarrowUn16Uto8Ux8, Iop_QNarrowUn32Uto16Ux4, Iop_QNarrowUn64Uto32Ux2,

      Iop_Widen8Uto16x8, Iop_Widen16Uto32x4, Iop_Widen32Uto64x2,
      Iop_Widen8Sto16x8, Iop_Widen16Sto32x4, Iop_Widen32Sto64x2,

      
      Iop_InterleaveHI8x16, Iop_InterleaveHI16x8,
      Iop_InterleaveHI32x4, Iop_InterleaveHI64x2,
      Iop_InterleaveLO8x16, Iop_InterleaveLO16x8,
      Iop_InterleaveLO32x4, Iop_InterleaveLO64x2,
      Iop_InterleaveOddLanes8x16, Iop_InterleaveEvenLanes8x16,
      Iop_InterleaveOddLanes16x8, Iop_InterleaveEvenLanes16x8,
      Iop_InterleaveOddLanes32x4, Iop_InterleaveEvenLanes32x4,

      Iop_CatOddLanes8x16, Iop_CatOddLanes16x8, Iop_CatOddLanes32x4,
      Iop_CatEvenLanes8x16, Iop_CatEvenLanes16x8, Iop_CatEvenLanes32x4,

      
      Iop_GetElem8x16, Iop_GetElem16x8, Iop_GetElem32x4, Iop_GetElem64x2,

      
      Iop_Dup8x16,   Iop_Dup16x8,   Iop_Dup32x4,

      Iop_SliceV128,  

      Iop_Reverse8sIn16_x8,
      Iop_Reverse8sIn32_x4, Iop_Reverse16sIn32_x4,
      Iop_Reverse8sIn64_x2, Iop_Reverse16sIn64_x2, Iop_Reverse32sIn64_x2,
      Iop_Reverse1sIn8_x16, 

      Iop_Perm8x16,
      Iop_Perm32x4, 

      Iop_GetMSBs8x16, 

      Iop_RecipEst32Ux4, Iop_RSqrtEst32Ux4,

      

      
      Iop_V256to64_0,  
      Iop_V256to64_1,
      Iop_V256to64_2,
      Iop_V256to64_3,  

      Iop_64x4toV256,  
                       

      Iop_V256toV128_0, 
      Iop_V256toV128_1, 
      Iop_V128HLtoV256, 

      Iop_AndV256,
      Iop_OrV256,
      Iop_XorV256,
      Iop_NotV256,

      
      Iop_CmpNEZ8x32, Iop_CmpNEZ16x16, Iop_CmpNEZ32x8, Iop_CmpNEZ64x4,

      Iop_Add8x32,    Iop_Add16x16,    Iop_Add32x8,    Iop_Add64x4,
      Iop_Sub8x32,    Iop_Sub16x16,    Iop_Sub32x8,    Iop_Sub64x4,

      Iop_CmpEQ8x32,  Iop_CmpEQ16x16,  Iop_CmpEQ32x8,  Iop_CmpEQ64x4,
      Iop_CmpGT8Sx32, Iop_CmpGT16Sx16, Iop_CmpGT32Sx8, Iop_CmpGT64Sx4,

      Iop_ShlN16x16, Iop_ShlN32x8, Iop_ShlN64x4,
      Iop_ShrN16x16, Iop_ShrN32x8, Iop_ShrN64x4,
      Iop_SarN16x16, Iop_SarN32x8,

      Iop_Max8Sx32, Iop_Max16Sx16, Iop_Max32Sx8,
      Iop_Max8Ux32, Iop_Max16Ux16, Iop_Max32Ux8,
      Iop_Min8Sx32, Iop_Min16Sx16, Iop_Min32Sx8,
      Iop_Min8Ux32, Iop_Min16Ux16, Iop_Min32Ux8,

      Iop_Mul16x16, Iop_Mul32x8,
      Iop_MulHi16Ux16, Iop_MulHi16Sx16,

      Iop_QAdd8Ux32, Iop_QAdd16Ux16,
      Iop_QAdd8Sx32, Iop_QAdd16Sx16,
      Iop_QSub8Ux32, Iop_QSub16Ux16,
      Iop_QSub8Sx32, Iop_QSub16Sx16,

      Iop_Avg8Ux32, Iop_Avg16Ux16,

      Iop_Perm32x8,

      
      Iop_CipherV128, Iop_CipherLV128, Iop_CipherSV128,
      Iop_NCipherV128, Iop_NCipherLV128,

      Iop_SHA512, Iop_SHA256,

      

      
      Iop_Add64Fx4, Iop_Sub64Fx4, Iop_Mul64Fx4, Iop_Div64Fx4,
      Iop_Add32Fx8, Iop_Sub32Fx8, Iop_Mul32Fx8, Iop_Div32Fx8,

      Iop_Sqrt32Fx8,
      Iop_Sqrt64Fx4,
      Iop_RSqrtEst32Fx8,
      Iop_RecipEst32Fx8,

      Iop_Max32Fx8, Iop_Min32Fx8,
      Iop_Max64Fx4, Iop_Min64Fx4,
      Iop_LAST      
   }
   IROp;

extern void ppIROp ( IROp );


typedef
   enum { 
      Irrm_NEAREST              = 0,  
      Irrm_NegINF               = 1,  
      Irrm_PosINF               = 2,  
      Irrm_ZERO                 = 3,  
      Irrm_NEAREST_TIE_AWAY_0   = 4,  
      Irrm_PREPARE_SHORTER      = 5,  
                                      
      Irrm_AWAY_FROM_ZERO       = 6,  
      Irrm_NEAREST_TIE_TOWARD_0 = 7   
   }
   IRRoundingMode;

typedef
   enum {
      Ircr_UN = 0x45,
      Ircr_LT = 0x01,
      Ircr_GT = 0x00,
      Ircr_EQ = 0x40
   }
   IRCmpFResult;

typedef IRCmpFResult IRCmpF32Result;
typedef IRCmpFResult IRCmpF64Result;
typedef IRCmpFResult IRCmpF128Result;

typedef IRCmpFResult IRCmpDResult;
typedef IRCmpDResult IRCmpD64Result;
typedef IRCmpDResult IRCmpD128Result;


typedef struct _IRQop   IRQop;   
typedef struct _IRTriop IRTriop; 


typedef
   enum { 
      Iex_Binder=0x1900,
      Iex_Get,
      Iex_GetI,
      Iex_RdTmp,
      Iex_Qop,
      Iex_Triop,
      Iex_Binop,
      Iex_Unop,
      Iex_Load,
      Iex_Const,
      Iex_ITE,
      Iex_CCall,
      Iex_VECRET,
      Iex_BBPTR
   }
   IRExprTag;

typedef
   struct _IRExpr
   IRExpr;

struct _IRExpr {
   IRExprTag tag;
   union {
      struct {
         Int binder;
      } Binder;

      struct {
         Int    offset;    
         IRType ty;        
      } Get;

      struct {
         IRRegArray* descr; 
         IRExpr*     ix;    
         Int         bias;  
      } GetI;

      struct {
         IRTemp tmp;       
      } RdTmp;

      struct {
        IRQop* details;
      } Qop;

      struct {
        IRTriop* details;
      } Triop;

      struct {
         IROp op;          
         IRExpr* arg1;     
         IRExpr* arg2;     
      } Binop;

      struct {
         IROp    op;       
         IRExpr* arg;      
      } Unop;

      struct {
         IREndness end;    
         IRType    ty;     
         IRExpr*   addr;   
      } Load;

      struct {
         IRConst* con;     
      } Const;

      struct {
         IRCallee* cee;    
         IRType    retty;  
         IRExpr**  args;   
      }  CCall;

      struct {
         IRExpr* cond;     
         IRExpr* iftrue;   
         IRExpr* iffalse;  
      } ITE;
   } Iex;
};

struct _IRTriop {
   IROp op;          
   IRExpr* arg1;     
   IRExpr* arg2;     
   IRExpr* arg3;     
};

struct _IRQop {
   IROp op;          
   IRExpr* arg1;     
   IRExpr* arg2;     
   IRExpr* arg3;     
   IRExpr* arg4;     
};





static inline Bool is_IRExpr_VECRET_or_BBPTR ( const IRExpr* e ) {
   return e->tag == Iex_VECRET || e->tag == Iex_BBPTR;
}


extern IRExpr* IRExpr_Binder ( Int binder );
extern IRExpr* IRExpr_Get    ( Int off, IRType ty );
extern IRExpr* IRExpr_GetI   ( IRRegArray* descr, IRExpr* ix, Int bias );
extern IRExpr* IRExpr_RdTmp  ( IRTemp tmp );
extern IRExpr* IRExpr_Qop    ( IROp op, IRExpr* arg1, IRExpr* arg2, 
                                        IRExpr* arg3, IRExpr* arg4 );
extern IRExpr* IRExpr_Triop  ( IROp op, IRExpr* arg1, 
                                        IRExpr* arg2, IRExpr* arg3 );
extern IRExpr* IRExpr_Binop  ( IROp op, IRExpr* arg1, IRExpr* arg2 );
extern IRExpr* IRExpr_Unop   ( IROp op, IRExpr* arg );
extern IRExpr* IRExpr_Load   ( IREndness end, IRType ty, IRExpr* addr );
extern IRExpr* IRExpr_Const  ( IRConst* con );
extern IRExpr* IRExpr_CCall  ( IRCallee* cee, IRType retty, IRExpr** args );
extern IRExpr* IRExpr_ITE    ( IRExpr* cond, IRExpr* iftrue, IRExpr* iffalse );
extern IRExpr* IRExpr_VECRET ( void );
extern IRExpr* IRExpr_BBPTR  ( void );

extern IRExpr* deepCopyIRExpr ( const IRExpr* );

extern void ppIRExpr ( const IRExpr* );

extern IRExpr** mkIRExprVec_0 ( void );
extern IRExpr** mkIRExprVec_1 ( IRExpr* );
extern IRExpr** mkIRExprVec_2 ( IRExpr*, IRExpr* );
extern IRExpr** mkIRExprVec_3 ( IRExpr*, IRExpr*, IRExpr* );
extern IRExpr** mkIRExprVec_4 ( IRExpr*, IRExpr*, IRExpr*, IRExpr* );
extern IRExpr** mkIRExprVec_5 ( IRExpr*, IRExpr*, IRExpr*, IRExpr*,
                                IRExpr* );
extern IRExpr** mkIRExprVec_6 ( IRExpr*, IRExpr*, IRExpr*, IRExpr*,
                                IRExpr*, IRExpr* );
extern IRExpr** mkIRExprVec_7 ( IRExpr*, IRExpr*, IRExpr*, IRExpr*,
                                IRExpr*, IRExpr*, IRExpr* );
extern IRExpr** mkIRExprVec_8 ( IRExpr*, IRExpr*, IRExpr*, IRExpr*,
                                IRExpr*, IRExpr*, IRExpr*, IRExpr*);

extern IRExpr** shallowCopyIRExprVec ( IRExpr** );
extern IRExpr** deepCopyIRExprVec ( IRExpr *const * );

extern IRExpr* mkIRExpr_HWord ( HWord );

extern 
IRExpr* mkIRExprCCall ( IRType retty,
                        Int regparms, const HChar* name, void* addr, 
                        IRExpr** args );


static inline Bool isIRAtom ( const IRExpr* e ) {
   return toBool(e->tag == Iex_RdTmp || e->tag == Iex_Const);
}

extern Bool eqIRAtom ( const IRExpr*, const IRExpr* );



typedef
   enum {
      Ijk_INVALID=0x1A00, 
      Ijk_Boring,         
      Ijk_Call,           
      Ijk_Ret,            
      Ijk_ClientReq,      
      Ijk_Yield,          
      Ijk_EmWarn,         
      Ijk_EmFail,         
      Ijk_NoDecode,       
      Ijk_MapFail,        
      Ijk_InvalICache,    
      Ijk_FlushDCache,    
      Ijk_NoRedir,        
      Ijk_SigILL,         
      Ijk_SigTRAP,        
      Ijk_SigSEGV,        
      Ijk_SigBUS,         
      Ijk_SigFPE_IntDiv,  
      Ijk_SigFPE_IntOvf,  
      Ijk_Sys_syscall,    
      Ijk_Sys_int32,      
      Ijk_Sys_int128,     
      Ijk_Sys_int129,     
      Ijk_Sys_int130,     
      Ijk_Sys_sysenter    
   }
   IRJumpKind;

extern void ppIRJumpKind ( IRJumpKind );



/* A dirty call is a flexible mechanism for calling (possibly
   conditionally) a helper function or procedure.  The helper function
   may read, write or modify client memory, and may read, write or
   modify client state.  It can take arguments and optionally return a
   value.  It may return different results and/or do different things
   when called repeatedly with the same arguments, by means of storing
   private state.

   If a value is returned, it is assigned to the nominated return
   temporary.

   Dirty calls are statements rather than expressions for obvious
   reasons.  If a dirty call is marked as writing guest state, any
   pre-existing values derived from the written parts of the guest
   state are invalid.  Similarly, if the dirty call is stated as
   writing memory, any pre-existing loaded values are invalidated by
   it.

   In order that instrumentation is possible, the call must state, and
   state correctly:

   * Whether it reads, writes or modifies memory, and if so where.

   * Whether it reads, writes or modifies guest state, and if so which
     pieces.  Several pieces may be stated, and their extents must be
     known at translation-time.  Each piece is allowed to repeat some
     number of times at a fixed interval, if required.

   Normally, code is generated to pass just the args to the helper.
   However, if IRExpr_BBPTR() is present in the argument list (at most
   one instance is allowed), then the baseblock pointer is passed for
   that arg, so that the callee can access the guest state.  It is
   invalid for .nFxState to be zero but IRExpr_BBPTR() to be present,
   since .nFxState==0 is a claim that the call does not access guest
   state.

   IMPORTANT NOTE re GUARDS: Dirty calls are strict, very strict.  The
   arguments and 'mFx' are evaluated REGARDLESS of the guard value.
   The order of argument evaluation is unspecified.  The guard
   expression is evaluated AFTER the arguments and 'mFx' have been
   evaluated.  'mFx' is expected (by Memcheck) to be a defined value
   even if the guard evaluates to false.
*/

#define VEX_N_FXSTATE  7   

typedef
   enum {
      Ifx_None=0x1B00,      
      Ifx_Read,             
      Ifx_Write,            
      Ifx_Modify,           
   }
   IREffect;

extern void ppIREffect ( IREffect );

typedef
   struct _IRDirty {
      /* What to call, and details of args/results.  .guard must be
         non-NULL.  If .tmp is not IRTemp_INVALID, then the call
         returns a result which is placed in .tmp.  If at runtime the
         guard evaluates to false, .tmp has an 0x555..555 bit pattern
         written to it.  Hence conditional calls that assign .tmp are
         allowed. */
      IRCallee* cee;    
      IRExpr*   guard;  
      IRExpr**  args;   
      IRTemp    tmp;    

      
      IREffect  mFx;    
      IRExpr*   mAddr;  
      Int       mSize;  

      
      Int  nFxState; 
      struct {
         IREffect fx:16;   
         UShort   offset;
         UShort   size;
         UChar    nRepeats;
         UChar    repeatLen;
      } fxState[VEX_N_FXSTATE];
   }
   IRDirty;

extern void     ppIRDirty ( const IRDirty* );

extern IRDirty* emptyIRDirty ( void );

extern IRDirty* deepCopyIRDirty ( const IRDirty* );

extern 
IRDirty* unsafeIRDirty_0_N ( Int regparms, const HChar* name, void* addr, 
                             IRExpr** args );

extern 
IRDirty* unsafeIRDirty_1_N ( IRTemp dst, 
                             Int regparms, const HChar* name, void* addr, 
                             IRExpr** args );



typedef
   enum { 
      Imbe_Fence=0x1C00, 
      Imbe_CancelReservation
   }
   IRMBusEvent;

extern void ppIRMBusEvent ( IRMBusEvent );



/* This denotes an atomic compare and swap operation, either
   a single-element one or a double-element one.

   In the single-element case:

     .addr is the memory address.
     .end  is the endianness with which memory is accessed

     If .addr contains the same value as .expdLo, then .dataLo is
     written there, else there is no write.  In both cases, the
     original value at .addr is copied into .oldLo.

     Types: .expdLo, .dataLo and .oldLo must all have the same type.
     It may be any integral type, viz: I8, I16, I32 or, for 64-bit
     guests, I64.

     .oldHi must be IRTemp_INVALID, and .expdHi and .dataHi must
     be NULL.

   In the double-element case:

     .addr is the memory address.
     .end  is the endianness with which memory is accessed

     The operation is the same:

     If .addr contains the same value as .expdHi:.expdLo, then
     .dataHi:.dataLo is written there, else there is no write.  In
     both cases the original value at .addr is copied into
     .oldHi:.oldLo.

     Types: .expdHi, .expdLo, .dataHi, .dataLo, .oldHi, .oldLo must
     all have the same type, which may be any integral type, viz: I8,
     I16, I32 or, for 64-bit guests, I64.

     The double-element case is complicated by the issue of
     endianness.  In all cases, the two elements are understood to be
     located adjacently in memory, starting at the address .addr.

       If .end is Iend_LE, then the .xxxLo component is at the lower
       address and the .xxxHi component is at the higher address, and
       each component is itself stored little-endianly.

       If .end is Iend_BE, then the .xxxHi component is at the lower
       address and the .xxxLo component is at the higher address, and
       each component is itself stored big-endianly.

   This allows representing more cases than most architectures can
   handle.  For example, x86 cannot do DCAS on 8- or 16-bit elements.

   How to know if the CAS succeeded?

   * if .oldLo == .expdLo (resp. .oldHi:.oldLo == .expdHi:.expdLo),
     then the CAS succeeded, .dataLo (resp. .dataHi:.dataLo) is now
     stored at .addr, and the original value there was .oldLo (resp
     .oldHi:.oldLo).

   * if .oldLo != .expdLo (resp. .oldHi:.oldLo != .expdHi:.expdLo),
     then the CAS failed, and the original value at .addr was .oldLo
     (resp. .oldHi:.oldLo).

   Hence it is easy to know whether or not the CAS succeeded.
*/
typedef
   struct {
      IRTemp    oldHi;  /* old value of *addr is written here */
      IRTemp    oldLo;
      IREndness end;    
      IRExpr*   addr;   
      IRExpr*   expdHi; 
      IRExpr*   expdLo;
      IRExpr*   dataHi; 
      IRExpr*   dataLo;
   }
   IRCAS;

extern void ppIRCAS ( const IRCAS* cas );

extern IRCAS* mkIRCAS ( IRTemp oldHi, IRTemp oldLo,
                        IREndness end, IRExpr* addr, 
                        IRExpr* expdHi, IRExpr* expdLo,
                        IRExpr* dataHi, IRExpr* dataLo );

extern IRCAS* deepCopyIRCAS ( const IRCAS* );



typedef
   struct {
      IRRegArray* descr; 
      IRExpr*     ix;    
      Int         bias;  
      IRExpr*     data;  
   } IRPutI;

extern void ppIRPutI ( const IRPutI* puti );

extern IRPutI* mkIRPutI ( IRRegArray* descr, IRExpr* ix,
                          Int bias, IRExpr* data );

extern IRPutI* deepCopyIRPutI ( const IRPutI* );



typedef
   struct {
      IREndness end;    
      IRExpr*   addr;   
      IRExpr*   data;   
      IRExpr*   guard;  
   }
   IRStoreG;

typedef
   enum {
      ILGop_INVALID=0x1D00,
      ILGop_Ident64,   
      ILGop_Ident32,   
      ILGop_16Uto32,   
      ILGop_16Sto32,   
      ILGop_8Uto32,    
      ILGop_8Sto32     
   }
   IRLoadGOp;

typedef
   struct {
      IREndness end;    
      IRLoadGOp cvt;    
      IRTemp    dst;    
      IRExpr*   addr;   
      IRExpr*   alt;    
      IRExpr*   guard;  
   }
   IRLoadG;

extern void ppIRStoreG ( const IRStoreG* sg );

extern void ppIRLoadGOp ( IRLoadGOp cvt );

extern void ppIRLoadG ( const IRLoadG* lg );

extern IRStoreG* mkIRStoreG ( IREndness end,
                              IRExpr* addr, IRExpr* data,
                              IRExpr* guard );

extern IRLoadG* mkIRLoadG ( IREndness end, IRLoadGOp cvt,
                            IRTemp dst, IRExpr* addr, IRExpr* alt, 
                            IRExpr* guard );




typedef 
   enum {
      Ist_NoOp=0x1E00,
      Ist_IMark,     
      Ist_AbiHint,   
      Ist_Put,
      Ist_PutI,
      Ist_WrTmp,
      Ist_Store,
      Ist_LoadG,
      Ist_StoreG,
      Ist_CAS,
      Ist_LLSC,
      Ist_Dirty,
      Ist_MBE,
      Ist_Exit
   } 
   IRStmtTag;

typedef
   struct _IRStmt {
      IRStmtTag tag;
      union {
         struct {
	 } NoOp;

         struct {
            Addr   addr;   
            UInt   len;    
            UChar  delta;  
         } IMark;

         struct {
            IRExpr* base;     
            Int     len;      
            IRExpr* nia;      
         } AbiHint;

         struct {
            Int     offset;   
            IRExpr* data;     
         } Put;

         struct {
            IRPutI* details;
         } PutI;

         struct {
            IRTemp  tmp;   
            IRExpr* data;  
         } WrTmp;

         struct {
            IREndness end;    
            IRExpr*   addr;   
            IRExpr*   data;   
         } Store;

         struct {
            IRStoreG* details;
         } StoreG;

         struct {
            IRLoadG* details;
         } LoadG;

         struct {
            IRCAS* details;
         } CAS;

         /* Either Load-Linked or Store-Conditional, depending on
            STOREDATA.

            If STOREDATA is NULL then this is a Load-Linked, meaning
            that data is loaded from memory as normal, but a
            'reservation' for the address is also lodged in the
            hardware.

               result = Load-Linked(addr, end)

            The data transfer type is the type of RESULT (I32, I64,
            etc).  ppIRStmt output:

               result = LD<end>-Linked(<addr>), eg. LDbe-Linked(t1)

            If STOREDATA is not NULL then this is a Store-Conditional,
            hence:

               result = Store-Conditional(addr, storedata, end)

            The data transfer type is the type of STOREDATA and RESULT
            has type Ity_I1. The store may fail or succeed depending
            on the state of a previously lodged reservation on this
            address.  RESULT is written 1 if the store succeeds and 0
            if it fails.  eg ppIRStmt output:

               result = ( ST<end>-Cond(<addr>) = <storedata> )
               eg t3 = ( STbe-Cond(t1, t2) )

            In all cases, the address must be naturally aligned for
            the transfer type -- any misaligned addresses should be
            caught by a dominating IR check and side exit.  This
            alignment restriction exists because on at least some
            LL/SC platforms (ppc), stwcx. etc will trap w/ SIGBUS on
            misaligned addresses, and we have to actually generate
            stwcx. on the host, and we don't want it trapping on the
            host.

            Summary of rules for transfer type:
              STOREDATA == NULL (LL):
                transfer type = type of RESULT
              STOREDATA != NULL (SC):
                transfer type = type of STOREDATA, and RESULT :: Ity_I1
         */
         struct {
            IREndness end;
            IRTemp    result;
            IRExpr*   addr;
            IRExpr*   storedata; 
         } LLSC;

       
         struct {
            IRDirty* details;
         } Dirty;

         struct {
            IRMBusEvent event;
         } MBE;

         struct {
            IRExpr*    guard;    
            IRConst*   dst;      
            IRJumpKind jk;       
            Int        offsIP;   
         } Exit;
      } Ist;
   }
   IRStmt;

extern IRStmt* IRStmt_NoOp    ( void );
extern IRStmt* IRStmt_IMark   ( Addr addr, UInt len, UChar delta );
extern IRStmt* IRStmt_AbiHint ( IRExpr* base, Int len, IRExpr* nia );
extern IRStmt* IRStmt_Put     ( Int off, IRExpr* data );
extern IRStmt* IRStmt_PutI    ( IRPutI* details );
extern IRStmt* IRStmt_WrTmp   ( IRTemp tmp, IRExpr* data );
extern IRStmt* IRStmt_Store   ( IREndness end, IRExpr* addr, IRExpr* data );
extern IRStmt* IRStmt_StoreG  ( IREndness end, IRExpr* addr, IRExpr* data,
                                IRExpr* guard );
extern IRStmt* IRStmt_LoadG   ( IREndness end, IRLoadGOp cvt, IRTemp dst,
                                IRExpr* addr, IRExpr* alt, IRExpr* guard );
extern IRStmt* IRStmt_CAS     ( IRCAS* details );
extern IRStmt* IRStmt_LLSC    ( IREndness end, IRTemp result,
                                IRExpr* addr, IRExpr* storedata );
extern IRStmt* IRStmt_Dirty   ( IRDirty* details );
extern IRStmt* IRStmt_MBE     ( IRMBusEvent event );
extern IRStmt* IRStmt_Exit    ( IRExpr* guard, IRJumpKind jk, IRConst* dst,
                                Int offsIP );

extern IRStmt* deepCopyIRStmt ( const IRStmt* );

extern void ppIRStmt ( const IRStmt* );



typedef
   struct {
      IRType* types;
      Int     types_size;
      Int     types_used;
   }
   IRTypeEnv;

extern IRTemp newIRTemp ( IRTypeEnv*, IRType );

extern IRTypeEnv* deepCopyIRTypeEnv ( const IRTypeEnv* );

extern void ppIRTypeEnv ( const IRTypeEnv* );


typedef
   struct {
      IRTypeEnv* tyenv;
      IRStmt**   stmts;
      Int        stmts_size;
      Int        stmts_used;
      IRExpr*    next;
      IRJumpKind jumpkind;
      Int        offsIP;
   }
   IRSB;

extern IRSB* emptyIRSB ( void );

extern IRSB* deepCopyIRSB ( const IRSB* );

extern IRSB* deepCopyIRSBExceptStmts ( const IRSB* );

extern void ppIRSB ( const IRSB* );

extern void addStmtToIRSB ( IRSB*, IRStmt* );



extern IRTypeEnv* emptyIRTypeEnv  ( void );

extern IRType typeOfIRConst ( const IRConst* );
extern IRType typeOfIRTemp  ( const IRTypeEnv*, IRTemp );
extern IRType typeOfIRExpr  ( const IRTypeEnv*, const IRExpr* );

extern void typeOfIRLoadGOp ( IRLoadGOp cvt,
                              IRType* t_res,
                              IRType* t_arg );

extern void sanityCheckIRSB ( const  IRSB*  bb, 
                              const  HChar* caller,
                              Bool   require_flatness, 
                              IRType guest_word_size );
extern Bool isFlatIRStmt ( const IRStmt* );

extern Bool isPlausibleIRType ( IRType ty );



void vex_inject_ir(IRSB *, IREndness);


#endif 

