

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

#ifndef __VEX_HOST_ARM64_DEFS_H
#define __VEX_HOST_ARM64_DEFS_H

#include "libvex_basictypes.h"
#include "libvex.h"                      
#include "host_generic_regs.h"           



#define ST_IN static inline
ST_IN HReg hregARM64_X22 ( void ) { return mkHReg(False, HRcInt64,  22,  0); }
ST_IN HReg hregARM64_X23 ( void ) { return mkHReg(False, HRcInt64,  23,  1); }
ST_IN HReg hregARM64_X24 ( void ) { return mkHReg(False, HRcInt64,  24,  2); }
ST_IN HReg hregARM64_X25 ( void ) { return mkHReg(False, HRcInt64,  25,  3); }
ST_IN HReg hregARM64_X26 ( void ) { return mkHReg(False, HRcInt64,  26,  4); }
ST_IN HReg hregARM64_X27 ( void ) { return mkHReg(False, HRcInt64,  27,  5); }
ST_IN HReg hregARM64_X28 ( void ) { return mkHReg(False, HRcInt64,  28,  6); }

ST_IN HReg hregARM64_X0  ( void ) { return mkHReg(False, HRcInt64,  0,   7); }
ST_IN HReg hregARM64_X1  ( void ) { return mkHReg(False, HRcInt64,  1,   8); }
ST_IN HReg hregARM64_X2  ( void ) { return mkHReg(False, HRcInt64,  2,   9); }
ST_IN HReg hregARM64_X3  ( void ) { return mkHReg(False, HRcInt64,  3,  10); }
ST_IN HReg hregARM64_X4  ( void ) { return mkHReg(False, HRcInt64,  4,  11); }
ST_IN HReg hregARM64_X5  ( void ) { return mkHReg(False, HRcInt64,  5,  12); }
ST_IN HReg hregARM64_X6  ( void ) { return mkHReg(False, HRcInt64,  6,  13); }
ST_IN HReg hregARM64_X7  ( void ) { return mkHReg(False, HRcInt64,  7,  14); }

ST_IN HReg hregARM64_Q16 ( void ) { return mkHReg(False, HRcVec128, 16, 15); }
ST_IN HReg hregARM64_Q17 ( void ) { return mkHReg(False, HRcVec128, 17, 16); }
ST_IN HReg hregARM64_Q18 ( void ) { return mkHReg(False, HRcVec128, 18, 17); }
ST_IN HReg hregARM64_Q19 ( void ) { return mkHReg(False, HRcVec128, 19, 18); }
ST_IN HReg hregARM64_Q20 ( void ) { return mkHReg(False, HRcVec128, 20, 19); }

ST_IN HReg hregARM64_D8  ( void ) { return mkHReg(False, HRcFlt64,  8,  20); }
ST_IN HReg hregARM64_D9  ( void ) { return mkHReg(False, HRcFlt64,  9,  21); }
ST_IN HReg hregARM64_D10 ( void ) { return mkHReg(False, HRcFlt64,  10, 22); }
ST_IN HReg hregARM64_D11 ( void ) { return mkHReg(False, HRcFlt64,  11, 23); }
ST_IN HReg hregARM64_D12 ( void ) { return mkHReg(False, HRcFlt64,  12, 24); }
ST_IN HReg hregARM64_D13 ( void ) { return mkHReg(False, HRcFlt64,  13, 25); }

ST_IN HReg hregARM64_X8  ( void ) { return mkHReg(False, HRcInt64,  8,  26); }
ST_IN HReg hregARM64_X9  ( void ) { return mkHReg(False, HRcInt64,  9,  27); }
ST_IN HReg hregARM64_X21 ( void ) { return mkHReg(False, HRcInt64, 21,  28); }
#undef ST_IN

extern void ppHRegARM64 ( HReg );

#define ARM64_N_ARGREGS 8   



typedef
   enum {
      ARM64cc_EQ  = 0,  
      ARM64cc_NE  = 1,  

      ARM64cc_CS  = 2,  
      ARM64cc_CC  = 3,  

      ARM64cc_MI  = 4,  
      ARM64cc_PL  = 5,  

      ARM64cc_VS  = 6,  
      ARM64cc_VC  = 7,  

      ARM64cc_HI  = 8,  
      ARM64cc_LS  = 9,  

      ARM64cc_GE  = 10, 
      ARM64cc_LT  = 11, 

      ARM64cc_GT  = 12, 
      ARM64cc_LE  = 13, 

      ARM64cc_AL  = 14, 
      ARM64cc_NV  = 15  
   }
   ARM64CondCode;



typedef
   enum {
      ARM64am_RI9=10, 
      ARM64am_RI12,   
      ARM64am_RR      
   }
   ARM64AModeTag;

typedef
   struct {
      ARM64AModeTag tag;
      union {
         struct {
            HReg reg;
            Int  simm9; 
         } RI9;
         struct {
            HReg  reg;
            UInt  uimm12; 
            UChar szB;    
         } RI12;
         struct {
            HReg base;
            HReg index;
         } RR;
      } ARM64am;
   }
   ARM64AMode;

extern ARM64AMode* ARM64AMode_RI9  ( HReg reg, Int simm9 );
extern ARM64AMode* ARM64AMode_RI12 ( HReg reg, Int uimm12, UChar szB );
extern ARM64AMode* ARM64AMode_RR   ( HReg base, HReg index );



typedef
   enum {
      ARM64riA_I12=20, 
      ARM64riA_R       
   }
   ARM64RIATag;

typedef
   struct {
      ARM64RIATag tag;
      union {
         struct {
            UShort imm12;  
            UChar  shift;  
         } I12;
         struct {
            HReg reg;
         } R;
      } ARM64riA;
   }
   ARM64RIA;

extern ARM64RIA* ARM64RIA_I12 ( UShort imm12, UChar shift );
extern ARM64RIA* ARM64RIA_R   ( HReg );



typedef
   enum {
      ARM64riL_I13=6, 
      ARM64riL_R      
   }
   ARM64RILTag;

typedef
   struct {
      ARM64RILTag tag;
      union {
         struct {
            UChar bitN; 
            UChar immR; 
            UChar immS; 
         } I13;
         struct {
            HReg reg;
         } R;
      } ARM64riL;
   }
   ARM64RIL;

extern ARM64RIL* ARM64RIL_I13 ( UChar bitN, UChar immR, UChar immS );
extern ARM64RIL* ARM64RIL_R   ( HReg );



typedef
   enum {
      ARM64ri6_I6=30, 
      ARM64ri6_R      
   }
   ARM64RI6Tag;

typedef
   struct {
      ARM64RI6Tag tag;
      union {
         struct {
            UInt imm6;   
         } I6;
         struct {
            HReg reg;
         } R;
      } ARM64ri6;
   }
   ARM64RI6;

extern ARM64RI6* ARM64RI6_I6 ( UInt imm6 );
extern ARM64RI6* ARM64RI6_R  ( HReg );



typedef
   enum {
      ARM64lo_AND=40,
      ARM64lo_OR,
      ARM64lo_XOR
   }
   ARM64LogicOp;

typedef
   enum {
      ARM64sh_SHL=50,
      ARM64sh_SHR,
      ARM64sh_SAR
   }
   ARM64ShiftOp;

typedef
   enum {
      ARM64un_NEG=60,
      ARM64un_NOT,
      ARM64un_CLZ,
   }
   ARM64UnaryOp;

typedef
   enum {
      ARM64mul_PLAIN=70, 
      ARM64mul_ZX,       
      ARM64mul_SX        
   }
   ARM64MulOp;

typedef
   enum {
      ARM64cvt_F32_I32S=80,
      ARM64cvt_F64_I32S,
      ARM64cvt_F32_I64S,
      ARM64cvt_F64_I64S,
      ARM64cvt_F32_I32U,
      ARM64cvt_F64_I32U,
      ARM64cvt_F32_I64U,
      ARM64cvt_F64_I64U,
      ARM64cvt_INVALID
   }
   ARM64CvtOp;

typedef
   enum {
      ARM64fpb_ADD=100,
      ARM64fpb_SUB,
      ARM64fpb_MUL,
      ARM64fpb_DIV,
      ARM64fpb_INVALID
   }
   ARM64FpBinOp;

typedef
   enum {
      ARM64fpu_NEG=110,
      ARM64fpu_ABS,
      ARM64fpu_SQRT,
      ARM64fpu_RINT,
      ARM64fpu_RECPX,
      ARM64fpu_INVALID
   }
   ARM64FpUnaryOp;

typedef
   enum {
      ARM64vecb_ADD64x2=120, ARM64vecb_ADD32x4,
      ARM64vecb_ADD16x8,     ARM64vecb_ADD8x16,
      ARM64vecb_SUB64x2,     ARM64vecb_SUB32x4,
      ARM64vecb_SUB16x8,     ARM64vecb_SUB8x16,
                             ARM64vecb_MUL32x4,
      ARM64vecb_MUL16x8,     ARM64vecb_MUL8x16,
      ARM64vecb_FADD64x2,    ARM64vecb_FADD32x4,
      ARM64vecb_FSUB64x2,    ARM64vecb_FSUB32x4,
      ARM64vecb_FMUL64x2,    ARM64vecb_FMUL32x4,
      ARM64vecb_FDIV64x2,    ARM64vecb_FDIV32x4,
      ARM64vecb_FMAX64x2,    ARM64vecb_FMAX32x4,
      ARM64vecb_FMIN64x2,    ARM64vecb_FMIN32x4,
                             ARM64vecb_UMAX32x4,
      ARM64vecb_UMAX16x8,    ARM64vecb_UMAX8x16,
                             ARM64vecb_UMIN32x4,
      ARM64vecb_UMIN16x8,    ARM64vecb_UMIN8x16,
                             ARM64vecb_SMAX32x4,
      ARM64vecb_SMAX16x8,    ARM64vecb_SMAX8x16,
                             ARM64vecb_SMIN32x4,
      ARM64vecb_SMIN16x8,    ARM64vecb_SMIN8x16,
      ARM64vecb_AND,
      ARM64vecb_ORR,
      ARM64vecb_XOR,
      ARM64vecb_CMEQ64x2,    ARM64vecb_CMEQ32x4,
      ARM64vecb_CMEQ16x8,    ARM64vecb_CMEQ8x16,
      ARM64vecb_CMHI64x2,    ARM64vecb_CMHI32x4, 
      ARM64vecb_CMHI16x8,    ARM64vecb_CMHI8x16,
      ARM64vecb_CMGT64x2,    ARM64vecb_CMGT32x4, 
      ARM64vecb_CMGT16x8,    ARM64vecb_CMGT8x16,
      ARM64vecb_FCMEQ64x2,   ARM64vecb_FCMEQ32x4,
      ARM64vecb_FCMGE64x2,   ARM64vecb_FCMGE32x4,
      ARM64vecb_FCMGT64x2,   ARM64vecb_FCMGT32x4,
      ARM64vecb_TBL1,
      ARM64vecb_UZP164x2,    ARM64vecb_UZP132x4,
      ARM64vecb_UZP116x8,    ARM64vecb_UZP18x16,
      ARM64vecb_UZP264x2,    ARM64vecb_UZP232x4,
      ARM64vecb_UZP216x8,    ARM64vecb_UZP28x16,
      ARM64vecb_ZIP132x4,    ARM64vecb_ZIP116x8,
      ARM64vecb_ZIP18x16,    ARM64vecb_ZIP232x4,
      ARM64vecb_ZIP216x8,    ARM64vecb_ZIP28x16,
                             ARM64vecb_PMUL8x16,
                             ARM64vecb_PMULL8x8,
                             ARM64vecb_UMULL2DSS,
      ARM64vecb_UMULL4SHH,   ARM64vecb_UMULL8HBB,
                             ARM64vecb_SMULL2DSS,
      ARM64vecb_SMULL4SHH,   ARM64vecb_SMULL8HBB,
      ARM64vecb_SQADD64x2,   ARM64vecb_SQADD32x4,
      ARM64vecb_SQADD16x8,   ARM64vecb_SQADD8x16,
      ARM64vecb_UQADD64x2,   ARM64vecb_UQADD32x4,
      ARM64vecb_UQADD16x8,   ARM64vecb_UQADD8x16,
      ARM64vecb_SQSUB64x2,   ARM64vecb_SQSUB32x4,
      ARM64vecb_SQSUB16x8,   ARM64vecb_SQSUB8x16,
      ARM64vecb_UQSUB64x2,   ARM64vecb_UQSUB32x4,
      ARM64vecb_UQSUB16x8,   ARM64vecb_UQSUB8x16,
                             ARM64vecb_SQDMULL2DSS,
      ARM64vecb_SQDMULL4SHH,
                             ARM64vecb_SQDMULH32x4,
      ARM64vecb_SQDMULH16x8,
                             ARM64vecb_SQRDMULH32x4,
      ARM64vecb_SQRDMULH16x8,
      ARM64vecb_SQSHL64x2,   ARM64vecb_SQSHL32x4,
      ARM64vecb_SQSHL16x8,   ARM64vecb_SQSHL8x16,
      ARM64vecb_UQSHL64x2,   ARM64vecb_UQSHL32x4,
      ARM64vecb_UQSHL16x8,   ARM64vecb_UQSHL8x16,
      ARM64vecb_SQRSHL64x2,  ARM64vecb_SQRSHL32x4,
      ARM64vecb_SQRSHL16x8,  ARM64vecb_SQRSHL8x16,
      ARM64vecb_UQRSHL64x2,  ARM64vecb_UQRSHL32x4,
      ARM64vecb_UQRSHL16x8,  ARM64vecb_UQRSHL8x16,
      ARM64vecb_SSHL64x2,    ARM64vecb_SSHL32x4,
      ARM64vecb_SSHL16x8,    ARM64vecb_SSHL8x16, 
      ARM64vecb_USHL64x2,    ARM64vecb_USHL32x4,
      ARM64vecb_USHL16x8,    ARM64vecb_USHL8x16, 
      ARM64vecb_SRSHL64x2,   ARM64vecb_SRSHL32x4,
      ARM64vecb_SRSHL16x8,   ARM64vecb_SRSHL8x16, 
      ARM64vecb_URSHL64x2,   ARM64vecb_URSHL32x4,
      ARM64vecb_URSHL16x8,   ARM64vecb_URSHL8x16, 
      ARM64vecb_FRECPS64x2,  ARM64vecb_FRECPS32x4,
      ARM64vecb_FRSQRTS64x2, ARM64vecb_FRSQRTS32x4,
      ARM64vecb_INVALID
   }
   ARM64VecBinOp;

typedef
   enum {
      ARM64vecmo_SUQADD64x2=300, ARM64vecmo_SUQADD32x4,
      ARM64vecmo_SUQADD16x8,     ARM64vecmo_SUQADD8x16,
      ARM64vecmo_USQADD64x2,     ARM64vecmo_USQADD32x4,
      ARM64vecmo_USQADD16x8,     ARM64vecmo_USQADD8x16,
      ARM64vecmo_INVALID
   }
   ARM64VecModifyOp;

typedef
   enum {
      ARM64vecu_FNEG64x2=350, ARM64vecu_FNEG32x4,
      ARM64vecu_FABS64x2,     ARM64vecu_FABS32x4,
      ARM64vecu_NOT,
      ARM64vecu_ABS64x2,      ARM64vecu_ABS32x4,
      ARM64vecu_ABS16x8,      ARM64vecu_ABS8x16,
      ARM64vecu_CLS32x4,      ARM64vecu_CLS16x8,      ARM64vecu_CLS8x16, 
      ARM64vecu_CLZ32x4,      ARM64vecu_CLZ16x8,      ARM64vecu_CLZ8x16, 
      ARM64vecu_CNT8x16,
      ARM64vecu_RBIT,
      ARM64vecu_REV1616B,
      ARM64vecu_REV3216B,     ARM64vecu_REV328H,
      ARM64vecu_REV6416B,     ARM64vecu_REV648H,      ARM64vecu_REV644S,
      ARM64vecu_URECPE32x4,
      ARM64vecu_URSQRTE32x4,
      ARM64vecu_FRECPE64x2,   ARM64vecu_FRECPE32x4,
      ARM64vecu_FRSQRTE64x2,  ARM64vecu_FRSQRTE32x4,
      ARM64vecu_FSQRT64x2,    ARM64vecu_FSQRT32x4,
      ARM64vecu_INVALID
   }
   ARM64VecUnaryOp;

typedef
   enum {
      ARM64vecshi_USHR64x2=400, ARM64vecshi_USHR32x4,
      ARM64vecshi_USHR16x8,     ARM64vecshi_USHR8x16,
      ARM64vecshi_SSHR64x2,     ARM64vecshi_SSHR32x4,
      ARM64vecshi_SSHR16x8,     ARM64vecshi_SSHR8x16,
      ARM64vecshi_SHL64x2,      ARM64vecshi_SHL32x4,
      ARM64vecshi_SHL16x8,      ARM64vecshi_SHL8x16,
      ARM64vecshi_SQSHRN2SD,    ARM64vecshi_SQSHRN4HS,   ARM64vecshi_SQSHRN8BH,
      ARM64vecshi_UQSHRN2SD,    ARM64vecshi_UQSHRN4HS,   ARM64vecshi_UQSHRN8BH,
      ARM64vecshi_SQSHRUN2SD,   ARM64vecshi_SQSHRUN4HS,  ARM64vecshi_SQSHRUN8BH,
      ARM64vecshi_SQRSHRN2SD,   ARM64vecshi_SQRSHRN4HS,  ARM64vecshi_SQRSHRN8BH,
      ARM64vecshi_UQRSHRN2SD,   ARM64vecshi_UQRSHRN4HS,  ARM64vecshi_UQRSHRN8BH,
      ARM64vecshi_SQRSHRUN2SD,  ARM64vecshi_SQRSHRUN4HS, ARM64vecshi_SQRSHRUN8BH,
      
      ARM64vecshi_UQSHL64x2,    ARM64vecshi_UQSHL32x4,
      ARM64vecshi_UQSHL16x8,    ARM64vecshi_UQSHL8x16, 
      ARM64vecshi_SQSHL64x2,    ARM64vecshi_SQSHL32x4,
      ARM64vecshi_SQSHL16x8,    ARM64vecshi_SQSHL8x16, 
      ARM64vecshi_SQSHLU64x2,   ARM64vecshi_SQSHLU32x4,
      ARM64vecshi_SQSHLU16x8,   ARM64vecshi_SQSHLU8x16, 
      ARM64vecshi_INVALID
   }
   ARM64VecShiftImmOp;

typedef
   enum {
      ARM64vecna_XTN=450,
      ARM64vecna_SQXTN,
      ARM64vecna_UQXTN,
      ARM64vecna_SQXTUN,
      ARM64vecna_INVALID
   }
   ARM64VecNarrowOp;

typedef
   enum {
      
      ARM64in_Arith=1220,
      ARM64in_Cmp,
      ARM64in_Logic,
      ARM64in_Test,
      ARM64in_Shift,
      ARM64in_Unary,
      ARM64in_MovI,        
      ARM64in_Imm64,
      ARM64in_LdSt64,
      ARM64in_LdSt32,      
      ARM64in_LdSt16,      
      ARM64in_LdSt8,       
      ARM64in_XDirect,     
      ARM64in_XIndir,      
      ARM64in_XAssisted,   
      ARM64in_CSel,
      ARM64in_Call,
      ARM64in_AddToSP,     
      ARM64in_FromSP,      
      ARM64in_Mul,
      ARM64in_LdrEX,
      ARM64in_StrEX,
      ARM64in_MFence,
      
      ARM64in_VLdStH,   
      ARM64in_VLdStS,   
      ARM64in_VLdStD,   
      ARM64in_VLdStQ,   
      ARM64in_VCvtI2F,
      ARM64in_VCvtF2I,
      ARM64in_VCvtSD,   
      ARM64in_VCvtHS,   
      ARM64in_VCvtHD,   
      ARM64in_VUnaryD,
      ARM64in_VUnaryS,
      ARM64in_VBinD,
      ARM64in_VBinS,
      ARM64in_VCmpD,
      ARM64in_VCmpS,
      ARM64in_VFCSel,
      ARM64in_FPCR,
      ARM64in_FPSR,
      
      ARM64in_VBinV,
      ARM64in_VModifyV,
      ARM64in_VUnaryV,
      ARM64in_VNarrowV,
      ARM64in_VShiftImmV,
      ARM64in_VExtV,
      ARM64in_VImmQ,
      ARM64in_VDfromX,    
      ARM64in_VQfromX,    
      ARM64in_VQfromXX,   
      ARM64in_VXfromQ,    
      ARM64in_VXfromDorS, 
      ARM64in_VMov,       
      
      ARM64in_EvCheck,    
      ARM64in_ProfInc     
   }
   ARM64InstrTag;


typedef
   struct {
      ARM64InstrTag tag;
      union {
         
         
         struct {
            HReg      dst;
            HReg      argL;
            ARM64RIA* argR;
            Bool      isAdd;
         } Arith;
         
         struct {
            HReg      argL;
            ARM64RIA* argR;
            Bool      is64;
         } Cmp;
         
         struct {
            HReg         dst;
            HReg         argL;
            ARM64RIL*    argR;
            ARM64LogicOp op;
         } Logic;
         
         struct {
            HReg      argL;
            ARM64RIL* argR;
         } Test;
         
         struct {
            HReg         dst;
            HReg         argL;
            ARM64RI6*    argR;
            ARM64ShiftOp op;
         } Shift;
         
         struct {
            HReg         dst;
            HReg         src;
            ARM64UnaryOp op;
         } Unary;
         
         struct {
            HReg dst;
            HReg src;
         } MovI;
         
         struct {
            HReg  dst;
            ULong imm64;
         } Imm64;
         
         struct {
            Bool        isLoad;
            HReg        rD;
            ARM64AMode* amode;
         } LdSt64;
         
         struct {
            Bool        isLoad;
            HReg        rD;
            ARM64AMode* amode;
         } LdSt32;
         
         struct {
            Bool        isLoad;
            HReg        rD;
            ARM64AMode* amode;
         } LdSt16;
         
         struct {
            Bool        isLoad;
            HReg        rD;
            ARM64AMode* amode;
         } LdSt8;
         struct {
            Addr64        dstGA;    
            ARM64AMode*   amPC;     
            ARM64CondCode cond;     
            Bool          toFastEP; 
         } XDirect;
         struct {
            HReg          dstGA;
            ARM64AMode*   amPC;
            ARM64CondCode cond; 
         } XIndir;
         struct {
            HReg          dstGA;
            ARM64AMode*   amPC;
            ARM64CondCode cond; 
            IRJumpKind    jk;
         } XAssisted;
         
          struct {
            HReg          dst;
            HReg          argL;
            HReg          argR;
            ARM64CondCode cond;
         } CSel;
         struct {
            RetLoc        rloc;     
            Addr64        target;
            ARM64CondCode cond;
            Int           nArgRegs; 
         } Call;
         
         struct {
            Int simm; 
         } AddToSP;
         
         struct {
            HReg dst;
         } FromSP;
         struct {
            HReg       dst;
            HReg       argL;
            HReg       argR;
            ARM64MulOp op;
         } Mul;
         
         struct {
            Int  szB; 
         } LdrEX;
         
         struct {
            Int  szB; 
         } StrEX;
         struct {
         } MFence;
         
         
         struct {
            Bool isLoad;
            HReg hD;
            HReg rN;
            UInt uimm12;  
         } VLdStH;
         
         struct {
            Bool isLoad;
            HReg sD;
            HReg rN;
            UInt uimm12;  
         } VLdStS;
         
         struct {
            Bool isLoad;
            HReg dD;
            HReg rN;
            UInt uimm12;  
         } VLdStD;
         
         struct {
            Bool isLoad;
            HReg rQ; 
            HReg rN; 
         } VLdStQ;
         
         struct {
            ARM64CvtOp how;
            HReg       rD; 
            HReg       rS; 
         } VCvtI2F;
         
         struct {
            ARM64CvtOp how;
            HReg       rD; 
            HReg       rS; 
            UChar      armRM; 
                              
         } VCvtF2I;
         
         struct {
            Bool sToD; 
            HReg dst;
            HReg src;
         } VCvtSD;
         
         struct {
            Bool hToS; 
            HReg dst;
            HReg src;
         } VCvtHS;
         
         struct {
            Bool hToD; 
            HReg dst;
            HReg src;
         } VCvtHD;
         
         struct {
            ARM64FpUnaryOp op;
            HReg           dst;
            HReg           src;
         } VUnaryD;
         
         struct {
            ARM64FpUnaryOp op;
            HReg           dst;
            HReg           src;
         } VUnaryS;
         
         struct {
            ARM64FpBinOp op;
            HReg         dst;
            HReg         argL;
            HReg         argR;
         } VBinD;
         
         struct {
            ARM64FpBinOp op;
            HReg         dst;
            HReg         argL;
            HReg         argR;
         } VBinS;
         
         struct {
            HReg argL;
            HReg argR;
         } VCmpD;
         
         struct {
            HReg argL;
            HReg argR;
         } VCmpS;
         
         struct {
            HReg          dst;
            HReg          argL;
            HReg          argR;
            ARM64CondCode cond;
            Bool          isD;
         }
         VFCSel;
         
         struct {
            Bool toFPCR;
            HReg iReg;
         } FPCR;
         
         struct {
            Bool toFPSR;
            HReg iReg;
         } FPSR;
         
         struct {
            ARM64VecBinOp op;
            HReg          dst;
            HReg          argL;
            HReg          argR;
         } VBinV;
         struct {
            ARM64VecModifyOp op;
            HReg             mod;
            HReg             arg;
         } VModifyV;
         
         struct {
            ARM64VecUnaryOp op;
            HReg            dst;
            HReg            arg;
         } VUnaryV;
        struct {
           ARM64VecNarrowOp op;
           UInt             dszBlg2; 
           HReg             dst;     
           HReg             src;     
        } VNarrowV;
        struct {
           ARM64VecShiftImmOp op;
           HReg               dst;
           HReg               src;
           UInt               amt;
        } VShiftImmV;
        struct {
           HReg dst;
           HReg srcLo;
           HReg srcHi;
           UInt amtB;
        } VExtV;
         struct {
            HReg   rQ;
            UShort imm; 
         } VImmQ;
         struct {
            HReg rD;
            HReg rX;
         } VDfromX;
         struct {
            HReg rQ;
            HReg rXlo;
         } VQfromX;
         struct {
            HReg rQ;
            HReg rXhi;
            HReg rXlo;
         } VQfromXX;
         struct {
            HReg rX;
            HReg rQ;
            UInt laneNo; 
         } VXfromQ;
         struct {
            HReg rX;
            HReg rDorS;
            Bool fromD;
         } VXfromDorS;
         
         struct {
            UInt szB; 
            HReg dst;
            HReg src;
         } VMov;
         struct {
            ARM64AMode* amCounter;
            ARM64AMode* amFailAddr;
         } EvCheck;
         struct {
         } ProfInc;
      } ARM64in;
   }
   ARM64Instr;


extern ARM64Instr* ARM64Instr_Arith   ( HReg, HReg, ARM64RIA*, Bool isAdd );
extern ARM64Instr* ARM64Instr_Cmp     ( HReg, ARM64RIA*, Bool is64 );
extern ARM64Instr* ARM64Instr_Logic   ( HReg, HReg, ARM64RIL*, ARM64LogicOp );
extern ARM64Instr* ARM64Instr_Test    ( HReg, ARM64RIL* );
extern ARM64Instr* ARM64Instr_Shift   ( HReg, HReg, ARM64RI6*, ARM64ShiftOp );
extern ARM64Instr* ARM64Instr_Unary   ( HReg, HReg, ARM64UnaryOp );
extern ARM64Instr* ARM64Instr_MovI    ( HReg, HReg );
extern ARM64Instr* ARM64Instr_Imm64   ( HReg, ULong );
extern ARM64Instr* ARM64Instr_LdSt64  ( Bool isLoad, HReg, ARM64AMode* );
extern ARM64Instr* ARM64Instr_LdSt32  ( Bool isLoad, HReg, ARM64AMode* );
extern ARM64Instr* ARM64Instr_LdSt16  ( Bool isLoad, HReg, ARM64AMode* );
extern ARM64Instr* ARM64Instr_LdSt8   ( Bool isLoad, HReg, ARM64AMode* );
extern ARM64Instr* ARM64Instr_XDirect ( Addr64 dstGA, ARM64AMode* amPC,
                                        ARM64CondCode cond, Bool toFastEP );
extern ARM64Instr* ARM64Instr_XIndir  ( HReg dstGA, ARM64AMode* amPC,
                                        ARM64CondCode cond );
extern ARM64Instr* ARM64Instr_XAssisted ( HReg dstGA, ARM64AMode* amPC,
                                          ARM64CondCode cond, IRJumpKind jk );
extern ARM64Instr* ARM64Instr_CSel    ( HReg dst, HReg argL, HReg argR,
                                        ARM64CondCode cond );
extern ARM64Instr* ARM64Instr_Call    ( ARM64CondCode, Addr64, Int nArgRegs,
                                        RetLoc rloc );
extern ARM64Instr* ARM64Instr_AddToSP ( Int simm );
extern ARM64Instr* ARM64Instr_FromSP  ( HReg dst );
extern ARM64Instr* ARM64Instr_Mul     ( HReg dst, HReg argL, HReg argR,
                                        ARM64MulOp op );
extern ARM64Instr* ARM64Instr_LdrEX   ( Int szB );
extern ARM64Instr* ARM64Instr_StrEX   ( Int szB );
extern ARM64Instr* ARM64Instr_MFence  ( void );
extern ARM64Instr* ARM64Instr_VLdStH  ( Bool isLoad, HReg sD, HReg rN,
                                        UInt uimm12  );
extern ARM64Instr* ARM64Instr_VLdStS  ( Bool isLoad, HReg sD, HReg rN,
                                        UInt uimm12  );
extern ARM64Instr* ARM64Instr_VLdStD  ( Bool isLoad, HReg dD, HReg rN,
                                        UInt uimm12  );
extern ARM64Instr* ARM64Instr_VLdStQ  ( Bool isLoad, HReg rQ, HReg rN );
extern ARM64Instr* ARM64Instr_VCvtI2F ( ARM64CvtOp how, HReg rD, HReg rS );
extern ARM64Instr* ARM64Instr_VCvtF2I ( ARM64CvtOp how, HReg rD, HReg rS,
                                        UChar armRM );
extern ARM64Instr* ARM64Instr_VCvtSD  ( Bool sToD, HReg dst, HReg src );
extern ARM64Instr* ARM64Instr_VCvtHS  ( Bool hToS, HReg dst, HReg src );
extern ARM64Instr* ARM64Instr_VCvtHD  ( Bool hToD, HReg dst, HReg src );
extern ARM64Instr* ARM64Instr_VUnaryD ( ARM64FpUnaryOp op, HReg dst, HReg src );
extern ARM64Instr* ARM64Instr_VUnaryS ( ARM64FpUnaryOp op, HReg dst, HReg src );
extern ARM64Instr* ARM64Instr_VBinD   ( ARM64FpBinOp op, HReg, HReg, HReg );
extern ARM64Instr* ARM64Instr_VBinS   ( ARM64FpBinOp op, HReg, HReg, HReg );
extern ARM64Instr* ARM64Instr_VCmpD   ( HReg argL, HReg argR );
extern ARM64Instr* ARM64Instr_VCmpS   ( HReg argL, HReg argR );
extern ARM64Instr* ARM64Instr_VFCSel  ( HReg dst, HReg argL, HReg argR,
                                        ARM64CondCode cond, Bool isD );
extern ARM64Instr* ARM64Instr_FPCR    ( Bool toFPCR, HReg iReg );
extern ARM64Instr* ARM64Instr_FPSR    ( Bool toFPSR, HReg iReg );
extern ARM64Instr* ARM64Instr_VBinV   ( ARM64VecBinOp op, HReg, HReg, HReg );
extern ARM64Instr* ARM64Instr_VModifyV ( ARM64VecModifyOp, HReg, HReg );
extern ARM64Instr* ARM64Instr_VUnaryV ( ARM64VecUnaryOp op, HReg, HReg );
extern ARM64Instr* ARM64Instr_VNarrowV ( ARM64VecNarrowOp op, UInt dszBlg2,
                                         HReg dst, HReg src );
extern ARM64Instr* ARM64Instr_VShiftImmV ( ARM64VecShiftImmOp op,
                                           HReg dst, HReg src, UInt amt );
extern ARM64Instr* ARM64Instr_VExtV   ( HReg dst,
                                        HReg srcLo, HReg srcHi, UInt amtB );
extern ARM64Instr* ARM64Instr_VImmQ   ( HReg, UShort );
extern ARM64Instr* ARM64Instr_VDfromX ( HReg rD, HReg rX );
extern ARM64Instr* ARM64Instr_VQfromX ( HReg rQ, HReg rXlo );
extern ARM64Instr* ARM64Instr_VQfromXX( HReg rQ, HReg rXhi, HReg rXlo );
extern ARM64Instr* ARM64Instr_VXfromQ ( HReg rX, HReg rQ, UInt laneNo );
extern ARM64Instr* ARM64Instr_VXfromDorS ( HReg rX, HReg rDorS, Bool fromD );
extern ARM64Instr* ARM64Instr_VMov    ( UInt szB, HReg dst, HReg src );

extern ARM64Instr* ARM64Instr_EvCheck ( ARM64AMode* amCounter,
                                        ARM64AMode* amFailAddr );
extern ARM64Instr* ARM64Instr_ProfInc ( void );

extern void ppARM64Instr ( const ARM64Instr* );


extern void getRegUsage_ARM64Instr ( HRegUsage*, const ARM64Instr*, Bool );
extern void mapRegs_ARM64Instr     ( HRegRemap*, ARM64Instr*, Bool );
extern Bool isMove_ARM64Instr      ( const ARM64Instr*, HReg*, HReg* );
extern Int  emit_ARM64Instr        ( Bool* is_profInc,
                                     UChar* buf, Int nbuf, const ARM64Instr* i,
                                     Bool mode64,
                                     VexEndness endness_host,
                                     const void* disp_cp_chain_me_to_slowEP,
                                     const void* disp_cp_chain_me_to_fastEP,
                                     const void* disp_cp_xindir,
                                     const void* disp_cp_xassisted );

extern void genSpill_ARM64  ( HInstr** i1, HInstr** i2,
                              HReg rreg, Int offset, Bool );
extern void genReload_ARM64 ( HInstr** i1, HInstr** i2,
                              HReg rreg, Int offset, Bool );

extern const RRegUniverse* getRRegUniverse_ARM64 ( void );

extern HInstrArray* iselSB_ARM64 ( const IRSB*, 
                                   VexArch,
                                   const VexArchInfo*,
                                   const VexAbiInfo*,
                                   Int offs_Host_EvC_Counter,
                                   Int offs_Host_EvC_FailAddr,
                                   Bool chainingAllowed,
                                   Bool addProfInc,
                                   Addr max_ga );

extern Int evCheckSzB_ARM64 (void);

extern VexInvalRange chainXDirect_ARM64 ( VexEndness endness_host,
                                          void* place_to_chain,
                                          const void* disp_cp_chain_me_EXPECTED,
                                          const void* place_to_jump_to );

extern VexInvalRange unchainXDirect_ARM64 ( VexEndness endness_host,
                                            void* place_to_unchain,
                                            const void* place_to_jump_to_EXPECTED,
                                            const void* disp_cp_chain_me );

extern VexInvalRange patchProfInc_ARM64 ( VexEndness endness_host,
                                          void*  place_to_patch,
                                          const ULong* location_of_counter );


#endif 

