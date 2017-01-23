

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

#ifndef __VEX_HOST_PPC_DEFS_H
#define __VEX_HOST_PPC_DEFS_H

#include "libvex_basictypes.h"
#include "libvex.h"                      
#include "host_generic_regs.h"           



#define ST_IN static inline

#define GPR(_mode64, _enc, _ix64, _ix32) \
  mkHReg(False,  (_mode64) ? HRcInt64 : HRcInt32, \
         (_enc), (_mode64) ? (_ix64) : (_ix32))

#define FPR(_mode64, _enc, _ix64, _ix32) \
  mkHReg(False,  HRcFlt64, \
         (_enc), (_mode64) ? (_ix64) : (_ix32))

#define VR(_mode64, _enc, _ix64, _ix32) \
  mkHReg(False,  HRcVec128, \
         (_enc), (_mode64) ? (_ix64) : (_ix32))

ST_IN HReg hregPPC_GPR3  ( Bool mode64 ) { return GPR(mode64,  3,   0,  0); }
ST_IN HReg hregPPC_GPR4  ( Bool mode64 ) { return GPR(mode64,  4,   1,  1); }
ST_IN HReg hregPPC_GPR5  ( Bool mode64 ) { return GPR(mode64,  5,   2,  2); }
ST_IN HReg hregPPC_GPR6  ( Bool mode64 ) { return GPR(mode64,  6,   3,  3); }
ST_IN HReg hregPPC_GPR7  ( Bool mode64 ) { return GPR(mode64,  7,   4,  4); }
ST_IN HReg hregPPC_GPR8  ( Bool mode64 ) { return GPR(mode64,  8,   5,  5); }
ST_IN HReg hregPPC_GPR9  ( Bool mode64 ) { return GPR(mode64,  9,   6,  6); }
ST_IN HReg hregPPC_GPR10 ( Bool mode64 ) { return GPR(mode64, 10,   7,  7); }

ST_IN HReg hregPPC_GPR11 ( Bool mode64 ) { return GPR(mode64, 11,   0,  8); }
ST_IN HReg hregPPC_GPR12 ( Bool mode64 ) { return GPR(mode64, 12,   0,  9); }

ST_IN HReg hregPPC_GPR14 ( Bool mode64 ) { return GPR(mode64, 14,   8, 10); }
ST_IN HReg hregPPC_GPR15 ( Bool mode64 ) { return GPR(mode64, 15,   9, 11); }
ST_IN HReg hregPPC_GPR16 ( Bool mode64 ) { return GPR(mode64, 16,  10, 12); }
ST_IN HReg hregPPC_GPR17 ( Bool mode64 ) { return GPR(mode64, 17,  11, 13); }
ST_IN HReg hregPPC_GPR18 ( Bool mode64 ) { return GPR(mode64, 18,  12, 14); }
ST_IN HReg hregPPC_GPR19 ( Bool mode64 ) { return GPR(mode64, 19,  13, 15); }
ST_IN HReg hregPPC_GPR20 ( Bool mode64 ) { return GPR(mode64, 20,  14, 16); }
ST_IN HReg hregPPC_GPR21 ( Bool mode64 ) { return GPR(mode64, 21,  15, 17); }
ST_IN HReg hregPPC_GPR22 ( Bool mode64 ) { return GPR(mode64, 22,  16, 18); }
ST_IN HReg hregPPC_GPR23 ( Bool mode64 ) { return GPR(mode64, 23,  17, 19); }
ST_IN HReg hregPPC_GPR24 ( Bool mode64 ) { return GPR(mode64, 24,  18, 20); }
ST_IN HReg hregPPC_GPR25 ( Bool mode64 ) { return GPR(mode64, 25,  19, 21); }
ST_IN HReg hregPPC_GPR26 ( Bool mode64 ) { return GPR(mode64, 26,  20, 22); }
ST_IN HReg hregPPC_GPR27 ( Bool mode64 ) { return GPR(mode64, 27,  21, 23); }
ST_IN HReg hregPPC_GPR28 ( Bool mode64 ) { return GPR(mode64, 28,  22, 24); }

ST_IN HReg hregPPC_FPR14 ( Bool mode64 ) { return FPR(mode64, 14,  23, 25); }
ST_IN HReg hregPPC_FPR15 ( Bool mode64 ) { return FPR(mode64, 15,  24, 26); }
ST_IN HReg hregPPC_FPR16 ( Bool mode64 ) { return FPR(mode64, 16,  25, 27); }
ST_IN HReg hregPPC_FPR17 ( Bool mode64 ) { return FPR(mode64, 17,  26, 28); }
ST_IN HReg hregPPC_FPR18 ( Bool mode64 ) { return FPR(mode64, 18,  27, 29); }
ST_IN HReg hregPPC_FPR19 ( Bool mode64 ) { return FPR(mode64, 19,  28, 30); }
ST_IN HReg hregPPC_FPR20 ( Bool mode64 ) { return FPR(mode64, 20,  29, 31); }
ST_IN HReg hregPPC_FPR21 ( Bool mode64 ) { return FPR(mode64, 21,  30, 32); }

ST_IN HReg hregPPC_VR20  ( Bool mode64 ) { return VR (mode64, 20,  31, 33); }
ST_IN HReg hregPPC_VR21  ( Bool mode64 ) { return VR (mode64, 21,  32, 34); }
ST_IN HReg hregPPC_VR22  ( Bool mode64 ) { return VR (mode64, 22,  33, 35); }
ST_IN HReg hregPPC_VR23  ( Bool mode64 ) { return VR (mode64, 23,  34, 36); }
ST_IN HReg hregPPC_VR24  ( Bool mode64 ) { return VR (mode64, 24,  35, 37); }
ST_IN HReg hregPPC_VR25  ( Bool mode64 ) { return VR (mode64, 25,  36, 38); }
ST_IN HReg hregPPC_VR26  ( Bool mode64 ) { return VR (mode64, 26,  37, 39); }
ST_IN HReg hregPPC_VR27  ( Bool mode64 ) { return VR (mode64, 27,  38, 40); }

ST_IN HReg hregPPC_GPR1  ( Bool mode64 ) { return GPR(mode64,  1,  39, 41); }
ST_IN HReg hregPPC_GPR29 ( Bool mode64 ) { return GPR(mode64, 29,  40, 42); }
ST_IN HReg hregPPC_GPR30 ( Bool mode64 ) { return GPR(mode64, 30,  41, 43); }
ST_IN HReg hregPPC_GPR31 ( Bool mode64 ) { return GPR(mode64, 31,  42, 44); }
ST_IN HReg hregPPC_VR29  ( Bool mode64 ) { return VR (mode64, 29,  43, 45); }

#undef ST_IN
#undef GPR
#undef FPR
#undef VR

#define StackFramePtr(_mode64) hregPPC_GPR1(_mode64)
#define GuestStatePtr(_mode64) hregPPC_GPR31(_mode64)

#define PPC_N_REGPARMS 8

extern void ppHRegPPC ( HReg );



typedef
   enum {
      
      Pcf_7LT  = 28,  
      Pcf_7GT  = 29,  
      Pcf_7EQ  = 30,  
      Pcf_7SO  = 31,  
      Pcf_NONE = 32   
   }
   PPCCondFlag;

typedef
   enum {   
      Pct_FALSE  = 0x4, 
      Pct_TRUE   = 0xC, 
      Pct_ALWAYS = 0x14 
   }
   PPCCondTest;

typedef
   struct {
      PPCCondFlag flag;
      PPCCondTest test;
   }
   PPCCondCode;

extern const HChar* showPPCCondCode ( PPCCondCode );

extern PPCCondCode mk_PPCCondCode ( PPCCondTest, PPCCondFlag );

extern PPCCondTest invertCondTest ( PPCCondTest );





typedef
   enum {
     Pam_IR=1,      
     Pam_RR=2       
   }
   PPCAModeTag;

typedef
   struct {
      PPCAModeTag tag;
      union {
         struct {
            HReg base;
            Int  index;
         } IR;
         struct {
            HReg base;
            HReg index;
         } RR;
      } Pam;
   }
   PPCAMode;

extern PPCAMode* PPCAMode_IR ( Int,  HReg );
extern PPCAMode* PPCAMode_RR ( HReg, HReg );

extern PPCAMode* dopyPPCAMode ( PPCAMode* );

extern void ppPPCAMode ( PPCAMode* );


typedef 
   enum {
      Prh_Imm=3,
      Prh_Reg=4
   }
   PPCRHTag;

typedef
   struct {
      PPCRHTag tag;
      union {
         struct {
            Bool   syned;
            UShort imm16;
         } Imm;
         struct {
            HReg reg;
         } Reg;
      }
      Prh;
   }
   PPCRH;

extern PPCRH* PPCRH_Imm ( Bool, UShort );
extern PPCRH* PPCRH_Reg ( HReg );

extern void ppPPCRH ( PPCRH* );



typedef
   enum {
      Pri_Imm=5,
      Pri_Reg=6
   } 
   PPCRITag;

typedef
   struct {
      PPCRITag tag;
      union {
         ULong Imm;
         HReg  Reg;
      }
      Pri;
   }
   PPCRI;

extern PPCRI* PPCRI_Imm ( ULong );
extern PPCRI* PPCRI_Reg( HReg );

extern void ppPPCRI ( PPCRI* );


typedef
   enum {
      Pvi_Imm=7,
      Pvi_Reg=8
   } 
   PPCVI5sTag;

typedef
   struct {
      PPCVI5sTag tag;
      union {
         Char Imm5s;
         HReg Reg;
      }
      Pvi;
   }
   PPCVI5s;

extern PPCVI5s* PPCVI5s_Imm ( Char );
extern PPCVI5s* PPCVI5s_Reg ( HReg );

extern void ppPPCVI5s ( PPCVI5s* );



typedef
   enum {
      Pun_NEG,
      Pun_NOT,
      Pun_CLZ32,
      Pun_CLZ64,
      Pun_EXTSW
   }
   PPCUnaryOp;

extern const HChar* showPPCUnaryOp ( PPCUnaryOp );


typedef 
   enum {
      Palu_INVALID,
      Palu_ADD, Palu_SUB,
      Palu_AND, Palu_OR, Palu_XOR,
   }
   PPCAluOp;

extern 
const HChar* showPPCAluOp ( PPCAluOp, 
                            Bool );


typedef 
   enum {
      Pshft_INVALID,
      Pshft_SHL, Pshft_SHR, Pshft_SAR, 
   }
   PPCShftOp;

extern 
const HChar* showPPCShftOp ( PPCShftOp, 
                             Bool ,
                             Bool  );


typedef
   enum {
      Pfp_INVALID,

      
      Pfp_MADDD,  Pfp_MSUBD,
      Pfp_MADDS,  Pfp_MSUBS,
      Pfp_DFPADD, Pfp_DFPADDQ,
      Pfp_DFPSUB, Pfp_DFPSUBQ,
      Pfp_DFPMUL, Pfp_DFPMULQ,
      Pfp_DFPDIV, Pfp_DFPDIVQ,
      Pfp_DQUAQ,  Pfp_DRRNDQ,

      
      Pfp_ADDD, Pfp_SUBD, Pfp_MULD, Pfp_DIVD,
      Pfp_ADDS, Pfp_SUBS, Pfp_MULS, Pfp_DIVS,
      Pfp_DRSP, Pfp_DRDPQ, Pfp_DCTFIX, Pfp_DCTFIXQ, Pfp_DCFFIX, 
      Pfp_DQUA, Pfp_RRDTR, Pfp_DIEX, Pfp_DIEXQ, Pfp_DRINTN,

      
      Pfp_SQRT, Pfp_ABS, Pfp_NEG, Pfp_MOV, Pfp_RES, Pfp_RSQRTE,
      Pfp_FRIN, Pfp_FRIM, Pfp_FRIP, Pfp_FRIZ, 
      Pfp_DSCLI, Pfp_DSCRI, Pfp_DSCLIQ, Pfp_DSCRIQ, Pfp_DCTDP,
      Pfp_DCTQPQ, Pfp_DCFFIXQ, Pfp_DXEX, Pfp_DXEXQ, 

   }
   PPCFpOp;

extern const HChar* showPPCFpOp ( PPCFpOp );


typedef
   enum {
      Pav_INVALID,

      
      Pav_MOV,                             
      Pav_NOT,                             
      Pav_UNPCKH8S,  Pav_UNPCKH16S,        
      Pav_UNPCKL8S,  Pav_UNPCKL16S,
      Pav_UNPCKHPIX, Pav_UNPCKLPIX,

      
      Pav_AND, Pav_OR, Pav_XOR,            
      Pav_ADDU, Pav_QADDU, Pav_QADDS,
      Pav_SUBU, Pav_QSUBU, Pav_QSUBS,
      Pav_MULU,
      Pav_OMULU, Pav_OMULS, Pav_EMULU, Pav_EMULS,
      Pav_AVGU, Pav_AVGS,
      Pav_MAXU, Pav_MAXS,
      Pav_MINU, Pav_MINS,

      
      Pav_CMPEQU, Pav_CMPGTU, Pav_CMPGTS,

      
      Pav_SHL, Pav_SHR, Pav_SAR, Pav_ROTL,

      
      Pav_PACKUU, Pav_QPACKUU, Pav_QPACKSU, Pav_QPACKSS,
      Pav_PACKPXL,

      
      Pav_MRGHI, Pav_MRGLO,

      
      Pav_CATODD, Pav_CATEVEN,

      
      Pav_POLYMULADD,

      
      Pav_CIPHERV128, Pav_CIPHERLV128, Pav_NCIPHERV128, Pav_NCIPHERLV128,
      Pav_CIPHERSUBV128,

      
      Pav_SHA256, Pav_SHA512,

      
      Pav_BCDAdd, Pav_BCDSub,

      
      Pav_ZEROCNTBYTE, Pav_ZEROCNTWORD, Pav_ZEROCNTHALF, Pav_ZEROCNTDBL,

      
      Pav_BITMTXXPOSE,
   }
   PPCAvOp;

extern const HChar* showPPCAvOp ( PPCAvOp );


typedef
   enum {
      Pavfp_INVALID,

      
      Pavfp_ADDF, Pavfp_SUBF, Pavfp_MULF,
      Pavfp_MAXF, Pavfp_MINF,
      Pavfp_CMPEQF, Pavfp_CMPGTF, Pavfp_CMPGEF,

      
      Pavfp_RCPF, Pavfp_RSQRTF,
      Pavfp_CVTU2F, Pavfp_CVTS2F, Pavfp_QCVTF2U, Pavfp_QCVTF2S,
      Pavfp_ROUNDM, Pavfp_ROUNDP, Pavfp_ROUNDN, Pavfp_ROUNDZ,
   }
   PPCAvFpOp;

extern const HChar* showPPCAvFpOp ( PPCAvFpOp );


typedef
   enum {
      Pin_LI,         
      Pin_Alu,        
      Pin_Shft,       
      Pin_AddSubC,    
      Pin_Cmp,        
      Pin_Unary,      
      Pin_MulL,       
      Pin_Div,        
      Pin_Call,       
      Pin_XDirect,    
      Pin_XIndir,     
      Pin_XAssisted,  
      Pin_CMov,       
      Pin_Load,       
      Pin_LoadL,      
      Pin_Store,      
      Pin_StoreC,     
      Pin_Set,        
      Pin_MfCR,       
      Pin_MFence,     

      Pin_FpUnary,    
      Pin_FpBinary,   
      Pin_FpMulAcc,   
      Pin_FpLdSt,     
      Pin_FpSTFIW,    
      Pin_FpRSP,      
      Pin_FpCftI,     
      Pin_FpCMov,     
      Pin_FpLdFPSCR,  
      Pin_FpCmp,      

      Pin_RdWrLR,     

      Pin_AvLdSt,     
      Pin_AvUnary,    

      Pin_AvBinary,   
      Pin_AvBin8x16,  
      Pin_AvBin16x8,  
      Pin_AvBin32x4,  
      Pin_AvBin64x2,  

      Pin_AvBin32Fx4, 
      Pin_AvUn32Fx4,  

      Pin_AvPerm,     
      Pin_AvSel,      
      Pin_AvSh,       
      Pin_AvShlDbl,   
      Pin_AvSplat,    
      Pin_AvLdVSCR,   
      Pin_AvCMov,     
      Pin_AvCipherV128Unary,  
      Pin_AvCipherV128Binary, 
      Pin_AvHashV128Binary, 
      Pin_AvBCDV128Trinary, 
      Pin_Dfp64Unary,   
      Pin_Dfp128Unary,  
      Pin_DfpShift,     
      Pin_Dfp64Binary,  
      Pin_Dfp128Binary, 
      Pin_DfpShift128,  
      Pin_DfpD128toD64, 
      Pin_DfpI64StoD128, 
      Pin_DfpRound,       
      Pin_DfpRound128,    
      Pin_ExtractExpD128, 
      Pin_InsertExpD128,  
      Pin_Dfp64Cmp,       
      Pin_Dfp128Cmp,      
      Pin_DfpQuantize,    
      Pin_DfpQuantize128, 
      Pin_EvCheck,    
      Pin_ProfInc     
   }
   PPCInstrTag;


typedef
   struct {
      PPCInstrTag tag;
      union {
         struct {
            HReg dst;
            ULong imm64;
         } LI;
         struct {
            PPCAluOp op;
            HReg     dst;
            HReg     srcL;
            PPCRH*   srcR;
         } Alu;
         struct {
            PPCShftOp op;
            Bool      sz32;   
            HReg      dst;
            HReg      srcL;
            PPCRH*    srcR;
         } Shft;
         
         struct {
            Bool isAdd;  
            Bool setC;   
            HReg dst;
            HReg srcL;
            HReg srcR;
         } AddSubC;
         struct {
            Bool   syned;
            Bool   sz32;    
            UInt   crfD;
            HReg   srcL;
            PPCRH* srcR;
         } Cmp;
         
         struct {
            PPCUnaryOp op;
            HReg       dst;
            HReg       src;
         } Unary;
         struct {
            Bool syned;  
            Bool hi;     
            Bool sz32;   
            HReg dst;
            HReg srcL;
            HReg srcR;
         } MulL;
         
         struct {
            Bool extended;
            Bool syned;
            Bool sz32;   
            HReg dst;
            HReg srcL;
            HReg srcR;
         } Div;
         struct {
            PPCCondCode cond;
            Addr64      target;
            UInt        argiregs;
            RetLoc      rloc;     
         } Call;
         struct {
            Addr64      dstGA;    
            PPCAMode*   amCIA;    
            PPCCondCode cond;     
            Bool        toFastEP; 
         } XDirect;
         struct {
            HReg        dstGA;
            PPCAMode*   amCIA;
            PPCCondCode cond; 
         } XIndir;
         struct {
            HReg        dstGA;
            PPCAMode*   amCIA;
            PPCCondCode cond; 
            IRJumpKind  jk;
         } XAssisted;
         struct {
            PPCCondCode cond;
            HReg        dst;
            PPCRI*      src;
         } CMov;
         
         struct {
            UChar     sz; 
            HReg      dst;
            PPCAMode* src;
         } Load;
         
         struct {
            UChar sz; 
            HReg  dst;
            HReg  src;
         } LoadL;
         
         struct {
            UChar     sz; 
            PPCAMode* dst;
            HReg      src;
         } Store;
         
         struct {
            UChar sz; 
            HReg  dst;
            HReg  src;
         } StoreC;
         
         struct {
            PPCCondCode cond;
            HReg        dst;
         } Set;
         
         struct {
            HReg dst;
         } MfCR;
         struct {
         } MFence;

         
         struct {
            PPCFpOp op;
            HReg    dst;
            HReg    src;
         } FpUnary;
         struct {
            PPCFpOp op;
            HReg    dst;
            HReg    srcL;
            HReg    srcR;
         } FpBinary;
         struct {
            PPCFpOp op;
            HReg    dst;
            HReg    srcML;
            HReg    srcMR;
            HReg    srcAcc;
         } FpMulAcc;
         struct {
            Bool      isLoad;
            UChar     sz; 
            HReg      reg;
            PPCAMode* addr;
         } FpLdSt;
         struct {
            HReg addr; 
            HReg data; 
         } FpSTFIW;
         
         struct {
            HReg src;
            HReg dst;
         } FpRSP;
         struct {
            Bool fromI; 
            Bool int32; 
            Bool syned;
            Bool flt64; 
            HReg src;
            HReg dst;
         } FpCftI;
         
         struct {
            PPCCondCode cond;
            HReg        dst;
            HReg        src;
         } FpCMov;
         
         struct {
            HReg src;
            UInt dfp_rm;
         } FpLdFPSCR;
         
         struct {
            UChar crfD;
            HReg  dst;
            HReg  srcL;
            HReg  srcR;
         } FpCmp;

         
         struct {
            Bool wrLR;
            HReg gpr;
         } RdWrLR;

         
         struct {
            Bool      isLoad;
            UChar     sz;      
            HReg      reg;
            PPCAMode* addr;
         } AvLdSt;
         struct {
            PPCAvOp op;
            HReg    dst;
            HReg    src;
         } AvUnary;
         struct {
            PPCAvOp op;
            HReg    dst;
            HReg    srcL;
            HReg    srcR;
         } AvBinary;
         struct {
            PPCAvOp op;
            HReg    dst;
            HReg    srcL;
            HReg    srcR;
         } AvBin8x16;
         struct {
            PPCAvOp op;
            HReg    dst;
            HReg    srcL;
            HReg    srcR;
         } AvBin16x8;
         struct {
            PPCAvOp op;
            HReg    dst;
            HReg    srcL;
            HReg    srcR;
         } AvBin32x4;
         
         struct {
            PPCAvOp op;
            HReg    dst;
            HReg    srcL;
            HReg    srcR;
         } AvBin64x2;
         struct {
            PPCAvFpOp op;
            HReg      dst;
            HReg      srcL;
            HReg      srcR;
         } AvBin32Fx4;
         struct {
            PPCAvFpOp op;
            HReg      dst;
            HReg      src;
         } AvUn32Fx4;
         
         struct {
            HReg dst;
            HReg srcL;
            HReg srcR;
            HReg ctl;
         } AvPerm;
         struct {
            HReg dst;
            HReg srcL;
            HReg srcR;
            HReg ctl;
         } AvSel;
         struct {
            Bool  shLeft;
            HReg  dst;
            PPCAMode* addr;
         } AvSh;
         struct {
            UChar shift;
            HReg  dst;
            HReg  srcL;
            HReg  srcR;
         } AvShlDbl;
         struct {
            UChar    sz;   
            HReg     dst;
            PPCVI5s* src; 
         } AvSplat;
         struct {
            PPCCondCode cond;
            HReg        dst;
            HReg        src;
         } AvCMov;
         
         struct {
            HReg src;
         } AvLdVSCR;
         struct {
            PPCAvOp   op;
            HReg      dst;
            HReg      src;
         } AvCipherV128Unary;
         struct {
            PPCAvOp     op;
            HReg       dst;
            HReg       src;
            PPCRI* s_field;
         } AvHashV128Binary;
         struct {
            PPCAvOp     op;
            HReg       dst;
            HReg      src1;
            HReg      src2;
            PPCRI*      ps;
         } AvBCDV128Trinary;
         struct {
            PPCAvOp   op;
            HReg      dst;
            HReg      srcL;
            HReg      srcR;
         } AvCipherV128Binary;
         struct {
            PPCFpOp op;
            HReg dst;
            HReg src;
         } Dfp64Unary;
         struct {
            PPCFpOp op;
            HReg dst;
            HReg srcL;
            HReg srcR;
         } Dfp64Binary;
         struct {
            PPCFpOp op;
            HReg   dst;
            HReg   src;
            PPCRI* shift;
         } DfpShift;
         struct {
            PPCFpOp op;
            HReg dst_hi;
            HReg dst_lo;
            HReg src_hi;
            HReg src_lo;
         } Dfp128Unary;
         struct {
            PPCFpOp op;
            HReg dst_hi;
            HReg dst_lo;
            HReg srcR_hi;
            HReg srcR_lo;
         } Dfp128Binary;
         struct {
            PPCFpOp op;
            HReg   dst_hi;
            HReg   dst_lo;
            HReg   src_hi;
            HReg   src_lo;
            PPCRI* shift;
         } DfpShift128;
         struct {
            HReg dst;
            HReg src;
            PPCRI* r_rmc;
         } DfpRound;
         struct {
            HReg dst_hi;
            HReg dst_lo;
            HReg src_hi;
            HReg src_lo;
            PPCRI* r_rmc;
         } DfpRound128;
         struct {
	    PPCFpOp op;
            HReg dst;
            HReg srcL;
            HReg srcR;
            PPCRI* rmc;
         } DfpQuantize;
         struct {
	    PPCFpOp op;
            HReg dst_hi;
            HReg dst_lo;
            HReg src_hi;
            HReg src_lo;
  	    PPCRI* rmc;
         } DfpQuantize128;
         struct {
            PPCFpOp op;
            HReg dst;
            HReg src_hi;
            HReg src_lo;
         } ExtractExpD128;
         struct {
	    PPCFpOp op;
            HReg dst_hi;
            HReg dst_lo;
            HReg srcL;
            HReg srcR_hi;
            HReg srcR_lo;
         } InsertExpD128;
         struct {
            PPCFpOp op;
            HReg   dst;
            HReg   src_hi;
            HReg   src_lo;
         } DfpD128toD64;
         struct {
            PPCFpOp op;
            HReg   dst_hi;
            HReg   dst_lo;
            HReg   src;
         } DfpI64StoD128;
         struct {
            UChar crfD;
            HReg  dst;
            HReg  srcL;
            HReg  srcR;
         } Dfp64Cmp;
         struct {         
            UChar crfD;   
            HReg  dst;    
            HReg  srcL_hi;
            HReg  srcL_lo;
            HReg  srcR_hi;
            HReg  srcR_lo;
         } Dfp128Cmp;     
         struct {
            PPCAMode* amCounter;
            PPCAMode* amFailAddr;
         } EvCheck;
         struct {
         } ProfInc;
      } Pin;
   }
   PPCInstr;


extern PPCInstr* PPCInstr_LI         ( HReg, ULong, Bool );
extern PPCInstr* PPCInstr_Alu        ( PPCAluOp, HReg, HReg, PPCRH* );
extern PPCInstr* PPCInstr_Shft       ( PPCShftOp, Bool sz32, HReg, HReg, PPCRH* );
extern PPCInstr* PPCInstr_AddSubC    ( Bool, Bool, HReg, HReg, HReg );
extern PPCInstr* PPCInstr_Cmp        ( Bool, Bool, UInt, HReg, PPCRH* );
extern PPCInstr* PPCInstr_Unary      ( PPCUnaryOp op, HReg dst, HReg src );
extern PPCInstr* PPCInstr_MulL       ( Bool syned, Bool hi32, Bool sz32, HReg, HReg, HReg );
extern PPCInstr* PPCInstr_Div        ( Bool extended, Bool syned, Bool sz32, HReg dst, HReg srcL, HReg srcR );
extern PPCInstr* PPCInstr_Call       ( PPCCondCode, Addr64, UInt, RetLoc );
extern PPCInstr* PPCInstr_XDirect    ( Addr64 dstGA, PPCAMode* amCIA,
                                       PPCCondCode cond, Bool toFastEP );
extern PPCInstr* PPCInstr_XIndir     ( HReg dstGA, PPCAMode* amCIA,
                                       PPCCondCode cond );
extern PPCInstr* PPCInstr_XAssisted  ( HReg dstGA, PPCAMode* amCIA,
                                       PPCCondCode cond, IRJumpKind jk );
extern PPCInstr* PPCInstr_CMov       ( PPCCondCode, HReg dst, PPCRI* src );
extern PPCInstr* PPCInstr_Load       ( UChar sz,
                                       HReg dst, PPCAMode* src, Bool mode64 );
extern PPCInstr* PPCInstr_LoadL      ( UChar sz,
                                       HReg dst, HReg src, Bool mode64 );
extern PPCInstr* PPCInstr_Store      ( UChar sz, PPCAMode* dst,
                                       HReg src, Bool mode64 );
extern PPCInstr* PPCInstr_StoreC     ( UChar sz, HReg dst, HReg src,
                                       Bool mode64 );
extern PPCInstr* PPCInstr_Set        ( PPCCondCode cond, HReg dst );
extern PPCInstr* PPCInstr_MfCR       ( HReg dst );
extern PPCInstr* PPCInstr_MFence     ( void );

extern PPCInstr* PPCInstr_FpUnary    ( PPCFpOp op, HReg dst, HReg src );
extern PPCInstr* PPCInstr_FpBinary   ( PPCFpOp op, HReg dst, HReg srcL, HReg srcR );
extern PPCInstr* PPCInstr_FpMulAcc   ( PPCFpOp op, HReg dst, HReg srcML, 
                                                   HReg srcMR, HReg srcAcc );
extern PPCInstr* PPCInstr_FpLdSt     ( Bool isLoad, UChar sz, HReg, PPCAMode* );
extern PPCInstr* PPCInstr_FpSTFIW    ( HReg addr, HReg data );
extern PPCInstr* PPCInstr_FpRSP      ( HReg dst, HReg src );
extern PPCInstr* PPCInstr_FpCftI     ( Bool fromI, Bool int32, Bool syned,
                                       Bool dst64, HReg dst, HReg src );
extern PPCInstr* PPCInstr_FpCMov     ( PPCCondCode, HReg dst, HReg src );
extern PPCInstr* PPCInstr_FpLdFPSCR  ( HReg src, Bool dfp_rm );
extern PPCInstr* PPCInstr_FpCmp      ( HReg dst, HReg srcL, HReg srcR );

extern PPCInstr* PPCInstr_RdWrLR     ( Bool wrLR, HReg gpr );

extern PPCInstr* PPCInstr_AvLdSt     ( Bool isLoad, UChar sz, HReg, PPCAMode* );
extern PPCInstr* PPCInstr_AvUnary    ( PPCAvOp op, HReg dst, HReg src );
extern PPCInstr* PPCInstr_AvBinary   ( PPCAvOp op, HReg dst, HReg srcL, HReg srcR );
extern PPCInstr* PPCInstr_AvBin8x16  ( PPCAvOp op, HReg dst, HReg srcL, HReg srcR );
extern PPCInstr* PPCInstr_AvBin16x8  ( PPCAvOp op, HReg dst, HReg srcL, HReg srcR );
extern PPCInstr* PPCInstr_AvBin32x4  ( PPCAvOp op, HReg dst, HReg srcL, HReg srcR );
extern PPCInstr* PPCInstr_AvBin64x2  ( PPCAvOp op, HReg dst, HReg srcL, HReg srcR );
extern PPCInstr* PPCInstr_AvBin32Fx4 ( PPCAvFpOp op, HReg dst, HReg srcL, HReg srcR );
extern PPCInstr* PPCInstr_AvUn32Fx4  ( PPCAvFpOp op, HReg dst, HReg src );
extern PPCInstr* PPCInstr_AvPerm     ( HReg dst, HReg srcL, HReg srcR, HReg ctl );
extern PPCInstr* PPCInstr_AvSel      ( HReg ctl, HReg dst, HReg srcL, HReg srcR );
extern PPCInstr* PPCInstr_AvSh       ( Bool shLeft, HReg dst, PPCAMode* am_addr );
extern PPCInstr* PPCInstr_AvShlDbl   ( UChar shift, HReg dst, HReg srcL, HReg srcR );
extern PPCInstr* PPCInstr_AvSplat    ( UChar sz, HReg dst, PPCVI5s* src );
extern PPCInstr* PPCInstr_AvCMov     ( PPCCondCode, HReg dst, HReg src );
extern PPCInstr* PPCInstr_AvLdVSCR   ( HReg src );
extern PPCInstr* PPCInstr_AvCipherV128Unary  ( PPCAvOp op, HReg dst,
                                               HReg srcR );
extern PPCInstr* PPCInstr_AvCipherV128Binary ( PPCAvOp op, HReg dst,
                                               HReg srcL, HReg srcR );
extern PPCInstr* PPCInstr_AvHashV128Binary ( PPCAvOp op, HReg dst,
                                             HReg src, PPCRI* s_field );
extern PPCInstr* PPCInstr_AvBCDV128Trinary ( PPCAvOp op, HReg dst,
                                             HReg src1, HReg src2,
                                             PPCRI* ps );
extern PPCInstr* PPCInstr_Dfp64Unary  ( PPCFpOp op, HReg dst, HReg src );
extern PPCInstr* PPCInstr_Dfp64Binary ( PPCFpOp op, HReg dst, HReg srcL,
                                        HReg srcR );
extern PPCInstr* PPCInstr_DfpShift    ( PPCFpOp op, HReg dst, HReg src,
                                        PPCRI* shift );
extern PPCInstr* PPCInstr_Dfp128Unary  ( PPCFpOp op, HReg dst_hi, HReg dst_lo,
                                         HReg srcR_hi, HReg srcR_lo );
extern PPCInstr* PPCInstr_Dfp128Binary ( PPCFpOp op, HReg dst_hi, HReg dst_lo,
                                         HReg srcR_hi, HReg srcR_lo );
extern PPCInstr* PPCInstr_DfpShift128  ( PPCFpOp op, HReg dst_hi, HReg src_hi,
                                         HReg dst_lo, HReg src_lo,
                                         PPCRI* shift );
extern PPCInstr* PPCInstr_DfpD128toD64 ( PPCFpOp op, HReg dst,
                                         HReg dst_lo, HReg src_lo);
extern PPCInstr* PPCInstr_DfpI64StoD128  ( PPCFpOp op, HReg dst_hi,
                                           HReg dst_lo, HReg src);
extern PPCInstr* PPCInstr_DfpRound       ( HReg dst, HReg src, PPCRI* r_rmc );
extern PPCInstr* PPCInstr_DfpRound128    ( HReg dst_hi, HReg dst_lo, HReg src_hi,
                                           HReg src_lo, PPCRI* r_rmc );
extern PPCInstr* PPCInstr_DfpQuantize    ( PPCFpOp op, HReg dst, HReg srcL,
                                           HReg srcR, PPCRI* rmc );
extern PPCInstr* PPCInstr_DfpQuantize128 ( PPCFpOp op, HReg dst_hi,
                                           HReg dst_lo,
                                           HReg src_hi,
                                           HReg src_lo, PPCRI* rmc );
extern PPCInstr* PPCInstr_ExtractExpD128 ( PPCFpOp op,   HReg dst, 
                                           HReg src_hi, HReg src_lo );
extern PPCInstr* PPCInstr_InsertExpD128  ( PPCFpOp op,   HReg dst_hi, 
                                           HReg dst_lo,  HReg srcL,
                                           HReg srcR_hi, HReg srcR_lo );
extern PPCInstr* PPCInstr_Dfp64Cmp       ( HReg dst, HReg srcL, HReg srcR );
extern PPCInstr* PPCInstr_Dfp128Cmp      ( HReg dst, HReg srcL_hi, HReg srcL_lo,
                                           HReg srcR_hi, HReg srcR_lo );
extern PPCInstr* PPCInstr_EvCheck     ( PPCAMode* amCounter,
                                        PPCAMode* amFailAddr );
extern PPCInstr* PPCInstr_ProfInc     ( void );

extern void ppPPCInstr(const PPCInstr*, Bool mode64);


extern void getRegUsage_PPCInstr ( HRegUsage*, const PPCInstr*, Bool mode64 );
extern void mapRegs_PPCInstr     ( HRegRemap*, PPCInstr* , Bool mode64);
extern Bool isMove_PPCInstr      ( const PPCInstr*, HReg*, HReg* );
extern Int          emit_PPCInstr   ( Bool* is_profInc,
                                      UChar* buf, Int nbuf, const PPCInstr* i, 
                                      Bool mode64,
                                      VexEndness endness_host,
                                      const void* disp_cp_chain_me_to_slowEP,
                                      const void* disp_cp_chain_me_to_fastEP,
                                      const void* disp_cp_xindir,
                                      const void* disp_cp_xassisted );

extern void genSpill_PPC  ( HInstr** i1, HInstr** i2,
                            HReg rreg, Int offsetB, Bool mode64 );
extern void genReload_PPC ( HInstr** i1, HInstr** i2,
                            HReg rreg, Int offsetB, Bool mode64 );

extern const RRegUniverse* getRRegUniverse_PPC ( Bool mode64 );

extern HInstrArray* iselSB_PPC           ( const IRSB*,
                                           VexArch,
                                           const VexArchInfo*,
                                           const VexAbiInfo*,
                                           Int offs_Host_EvC_Counter,
                                           Int offs_Host_EvC_FailAddr,
                                           Bool chainingAllowed,
                                           Bool addProfInc,
                                           Addr max_ga );

extern Int evCheckSzB_PPC (void);

extern VexInvalRange chainXDirect_PPC ( VexEndness endness_host,
                                        void* place_to_chain,
                                        const void* disp_cp_chain_me_EXPECTED,
                                        const void* place_to_jump_to,
                                        Bool  mode64 );

extern VexInvalRange unchainXDirect_PPC ( VexEndness endness_host,
                                          void* place_to_unchain,
                                          const void* place_to_jump_to_EXPECTED,
                                          const void* disp_cp_chain_me,
                                          Bool  mode64 );

extern VexInvalRange patchProfInc_PPC ( VexEndness endness_host,
                                        void*  place_to_patch,
                                        const ULong* location_of_counter,
                                        Bool   mode64 );


#endif 

