
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
*/

#ifndef __VEX_HOST_ARM_DEFS_H
#define __VEX_HOST_ARM_DEFS_H

#include "libvex_basictypes.h"
#include "libvex.h"                      
#include "host_generic_regs.h"           

extern UInt arm_hwcaps;



#define ST_IN static inline
ST_IN HReg hregARM_R4  ( void ) { return mkHReg(False, HRcInt32,  4,  0);  }
ST_IN HReg hregARM_R5  ( void ) { return mkHReg(False, HRcInt32,  5,  1);  }
ST_IN HReg hregARM_R6  ( void ) { return mkHReg(False, HRcInt32,  6,  2);  }
ST_IN HReg hregARM_R7  ( void ) { return mkHReg(False, HRcInt32,  7,  3);  }
ST_IN HReg hregARM_R10 ( void ) { return mkHReg(False, HRcInt32,  10, 4);  }
ST_IN HReg hregARM_R11 ( void ) { return mkHReg(False, HRcInt32,  11, 5);  }

ST_IN HReg hregARM_R0  ( void ) { return mkHReg(False, HRcInt32,  0,  6);  }
ST_IN HReg hregARM_R1  ( void ) { return mkHReg(False, HRcInt32,  1,  7);  }
ST_IN HReg hregARM_R2  ( void ) { return mkHReg(False, HRcInt32,  2,  8);  }
ST_IN HReg hregARM_R3  ( void ) { return mkHReg(False, HRcInt32,  3,  9);  }
ST_IN HReg hregARM_R9  ( void ) { return mkHReg(False, HRcInt32,  9,  10); }

ST_IN HReg hregARM_D8  ( void ) { return mkHReg(False, HRcFlt64,  8,  11); }
ST_IN HReg hregARM_D9  ( void ) { return mkHReg(False, HRcFlt64,  9,  12); }
ST_IN HReg hregARM_D10 ( void ) { return mkHReg(False, HRcFlt64,  10, 13); }
ST_IN HReg hregARM_D11 ( void ) { return mkHReg(False, HRcFlt64,  11, 14); }
ST_IN HReg hregARM_D12 ( void ) { return mkHReg(False, HRcFlt64,  12, 15); }

ST_IN HReg hregARM_S26 ( void ) { return mkHReg(False, HRcFlt32,  26, 16); }
ST_IN HReg hregARM_S27 ( void ) { return mkHReg(False, HRcFlt32,  27, 17); }
ST_IN HReg hregARM_S28 ( void ) { return mkHReg(False, HRcFlt32,  28, 18); }
ST_IN HReg hregARM_S29 ( void ) { return mkHReg(False, HRcFlt32,  29, 19); }
ST_IN HReg hregARM_S30 ( void ) { return mkHReg(False, HRcFlt32,  30, 20); }

ST_IN HReg hregARM_Q8  ( void ) { return mkHReg(False, HRcVec128, 8,  21); }
ST_IN HReg hregARM_Q9  ( void ) { return mkHReg(False, HRcVec128, 9,  22); }
ST_IN HReg hregARM_Q10 ( void ) { return mkHReg(False, HRcVec128, 10, 23); }
ST_IN HReg hregARM_Q11 ( void ) { return mkHReg(False, HRcVec128, 11, 24); }
ST_IN HReg hregARM_Q12 ( void ) { return mkHReg(False, HRcVec128, 12, 25); }

ST_IN HReg hregARM_R8  ( void ) { return mkHReg(False, HRcInt32,  8,  26); }
ST_IN HReg hregARM_R12 ( void ) { return mkHReg(False, HRcInt32,  12, 27); }
ST_IN HReg hregARM_R13 ( void ) { return mkHReg(False, HRcInt32,  13, 28); }
ST_IN HReg hregARM_R14 ( void ) { return mkHReg(False, HRcInt32,  14, 29); }
ST_IN HReg hregARM_R15 ( void ) { return mkHReg(False, HRcInt32,  15, 30); }
ST_IN HReg hregARM_Q13 ( void ) { return mkHReg(False, HRcVec128, 13, 31); }
ST_IN HReg hregARM_Q14 ( void ) { return mkHReg(False, HRcVec128, 14, 32); }
ST_IN HReg hregARM_Q15 ( void ) { return mkHReg(False, HRcVec128, 15, 33); }
#undef ST_IN

extern void ppHRegARM ( HReg );

#define ARM_N_ARGREGS 4   



typedef
   enum {
      ARMcc_EQ  = 0,  
      ARMcc_NE  = 1,  

      ARMcc_HS  = 2,  
      ARMcc_LO  = 3,  

      ARMcc_MI  = 4,  
      ARMcc_PL  = 5,  

      ARMcc_VS  = 6,  
      ARMcc_VC  = 7,  

      ARMcc_HI  = 8,  
      ARMcc_LS  = 9,  

      ARMcc_GE  = 10, 
      ARMcc_LT  = 11, 

      ARMcc_GT  = 12, 
      ARMcc_LE  = 13, 

      ARMcc_AL  = 14, 
      ARMcc_NV  = 15  
   }
   ARMCondCode;

extern const HChar* showARMCondCode ( ARMCondCode );




typedef
   enum {
      ARMam1_RI=1,   
      ARMam1_RRS     
   }
   ARMAMode1Tag;

typedef
   struct {
      ARMAMode1Tag tag;
      union {
         struct {
            HReg reg;
            Int  simm13; 
         } RI;
         struct {
            HReg base;
            HReg index;
            UInt shift; 
         } RRS;
      } ARMam1;
   }
   ARMAMode1;

extern ARMAMode1* ARMAMode1_RI  ( HReg reg, Int simm13 );
extern ARMAMode1* ARMAMode1_RRS ( HReg base, HReg index, UInt shift );

extern void ppARMAMode1 ( ARMAMode1* );


typedef
   enum {
      ARMam2_RI=3,   
      ARMam2_RR      
   }
   ARMAMode2Tag;

typedef
   struct {
      ARMAMode2Tag tag;
      union {
         struct {
            HReg reg;
            Int  simm9; 
         } RI;
         struct {
            HReg base;
            HReg index;
         } RR;
      } ARMam2;
   }
   ARMAMode2;

extern ARMAMode2* ARMAMode2_RI ( HReg reg, Int simm9 );
extern ARMAMode2* ARMAMode2_RR ( HReg base, HReg index );

extern void ppARMAMode2 ( ARMAMode2* );


typedef
   struct {
      HReg reg;
      Int  simm11; 
   }
   ARMAModeV;

extern ARMAModeV* mkARMAModeV ( HReg reg, Int simm11 );

extern void ppARMAModeV ( ARMAModeV* );

typedef
   enum {
      ARMamN_R=5,
      ARMamN_RR
      
   }
   ARMAModeNTag;

typedef
   struct {
      ARMAModeNTag tag;
      union {
         struct {
            HReg rN;
            HReg rM;
         } RR;
         struct {
            HReg rN;
         } R;
         
      } ARMamN;
   }
   ARMAModeN;

extern ARMAModeN* mkARMAModeN_RR ( HReg, HReg );
extern ARMAModeN* mkARMAModeN_R ( HReg );
extern void ppARMAModeN ( ARMAModeN* );


typedef
   enum {
      ARMri84_I84=7,   
      ARMri84_R        
   }
   ARMRI84Tag;

typedef
   struct {
      ARMRI84Tag tag;
      union {
         struct {
            UShort imm8;
            UShort imm4;
         } I84;
         struct {
            HReg reg;
         } R;
      } ARMri84;
   }
   ARMRI84;

extern ARMRI84* ARMRI84_I84 ( UShort imm8, UShort imm4 );
extern ARMRI84* ARMRI84_R   ( HReg );

extern void ppARMRI84 ( ARMRI84* );


typedef
   enum {
      ARMri5_I5=9,   
      ARMri5_R       
   }
   ARMRI5Tag;

typedef
   struct {
      ARMRI5Tag tag;
      union {
         struct {
            UInt imm5;
         } I5;
         struct {
            HReg reg;
         } R;
      } ARMri5;
   }
   ARMRI5;

extern ARMRI5* ARMRI5_I5 ( UInt imm5 );
extern ARMRI5* ARMRI5_R  ( HReg );

extern void ppARMRI5 ( ARMRI5* );



typedef
   struct {
      UInt type;
      UInt imm8;
   }
   ARMNImm;

extern ARMNImm* ARMNImm_TI ( UInt type, UInt imm8 );
extern ULong ARMNImm_to_Imm64 ( ARMNImm* );
extern ARMNImm* Imm64_to_ARMNImm ( ULong );

extern void ppARMNImm ( ARMNImm* );


typedef
   enum {
      ARMNRS_Reg=11,
      ARMNRS_Scalar
   }
   ARMNRS_tag;

typedef
   struct {
      ARMNRS_tag tag;
      HReg reg;
      UInt index;
   }
   ARMNRS;

extern ARMNRS* mkARMNRS(ARMNRS_tag, HReg reg, UInt index);
extern void ppARMNRS ( ARMNRS* );


typedef
   enum {
      ARMalu_ADD=20,   
      ARMalu_ADDS,     
      ARMalu_ADC,      
      ARMalu_SUB,      
      ARMalu_SUBS,     
      ARMalu_SBC,      
      ARMalu_AND,
      ARMalu_BIC,
      ARMalu_OR,
      ARMalu_XOR
   }
   ARMAluOp;

extern const HChar* showARMAluOp ( ARMAluOp op );


typedef
   enum {
      ARMsh_SHL=40,
      ARMsh_SHR,
      ARMsh_SAR
   }
   ARMShiftOp;

extern const HChar* showARMShiftOp ( ARMShiftOp op );


typedef
   enum {
      ARMun_NEG=50,
      ARMun_NOT,
      ARMun_CLZ
   }
   ARMUnaryOp;

extern const HChar* showARMUnaryOp ( ARMUnaryOp op );


typedef
   enum {
      ARMmul_PLAIN=60,
      ARMmul_ZX,
      ARMmul_SX
   }
   ARMMulOp;

extern const HChar* showARMMulOp ( ARMMulOp op );


typedef
   enum {
      ARMvfp_ADD=70,
      ARMvfp_SUB,
      ARMvfp_MUL,
      ARMvfp_DIV
   }
   ARMVfpOp;

extern const HChar* showARMVfpOp ( ARMVfpOp op );


typedef
   enum {
      ARMvfpu_COPY=80,
      ARMvfpu_NEG,
      ARMvfpu_ABS,
      ARMvfpu_SQRT
   }
   ARMVfpUnaryOp;

extern const HChar* showARMVfpUnaryOp ( ARMVfpUnaryOp op );

typedef
   enum {
      ARMneon_VAND=90,
      ARMneon_VORR,
      ARMneon_VXOR,
      ARMneon_VADD,
      ARMneon_VADDFP,
      ARMneon_VRHADDS,
      ARMneon_VRHADDU,
      ARMneon_VPADDFP,
      ARMneon_VABDFP,
      ARMneon_VSUB,
      ARMneon_VSUBFP,
      ARMneon_VMAXU,
      ARMneon_VMAXS,
      ARMneon_VMAXF,
      ARMneon_VMINU,
      ARMneon_VMINS,
      ARMneon_VMINF,
      ARMneon_VQADDU,
      ARMneon_VQADDS,
      ARMneon_VQSUBU,
      ARMneon_VQSUBS,
      ARMneon_VCGTU,
      ARMneon_VCGTS,
      ARMneon_VCGEU,
      ARMneon_VCGES,
      ARMneon_VCGTF,
      ARMneon_VCGEF,
      ARMneon_VCEQ,
      ARMneon_VCEQF,
      ARMneon_VEXT,
      ARMneon_VMUL,
      ARMneon_VMULFP,
      ARMneon_VMULLU,
      ARMneon_VMULLS,
      ARMneon_VMULP,
      ARMneon_VMULLP,
      ARMneon_VQDMULH,
      ARMneon_VQRDMULH,
      ARMneon_VPADD,
      ARMneon_VPMINU,
      ARMneon_VPMINS,
      ARMneon_VPMINF,
      ARMneon_VPMAXU,
      ARMneon_VPMAXS,
      ARMneon_VPMAXF,
      ARMneon_VTBL,
      ARMneon_VQDMULL,
      ARMneon_VRECPS,
      ARMneon_VRSQRTS,
      ARMneon_INVALID
      
   }
   ARMNeonBinOp;

typedef
   enum {
      ARMneon_VSHL=150,
      ARMneon_VSAL, 
      ARMneon_VQSHL,
      ARMneon_VQSAL
   }
   ARMNeonShiftOp;

typedef
   enum {
      ARMneon_COPY=160,
      ARMneon_COPYLU,
      ARMneon_COPYLS,
      ARMneon_COPYN,
      ARMneon_COPYQNSS,
      ARMneon_COPYQNUS,
      ARMneon_COPYQNUU,
      ARMneon_NOT,
      ARMneon_EQZ,
      ARMneon_DUP,
      ARMneon_PADDLS,
      ARMneon_PADDLU,
      ARMneon_CNT,
      ARMneon_CLZ,
      ARMneon_CLS,
      ARMneon_VCVTxFPxINT,
      ARMneon_VQSHLNSS,
      ARMneon_VQSHLNUU,
      ARMneon_VQSHLNUS,
      ARMneon_VCVTFtoU,
      ARMneon_VCVTFtoS,
      ARMneon_VCVTUtoF,
      ARMneon_VCVTStoF,
      ARMneon_VCVTFtoFixedU,
      ARMneon_VCVTFtoFixedS,
      ARMneon_VCVTFixedUtoF,
      ARMneon_VCVTFixedStoF,
      ARMneon_VCVTF16toF32,
      ARMneon_VCVTF32toF16,
      ARMneon_REV16,
      ARMneon_REV32,
      ARMneon_REV64,
      ARMneon_ABS,
      ARMneon_VNEGF,
      ARMneon_VRECIP,
      ARMneon_VRECIPF,
      ARMneon_VABSFP,
      ARMneon_VRSQRTEFP,
      ARMneon_VRSQRTE
      
   }
   ARMNeonUnOp;

typedef
   enum {
      ARMneon_SETELEM=200,
      ARMneon_GETELEMU,
      ARMneon_GETELEMS,
      ARMneon_VDUP,
   }
   ARMNeonUnOpS;

typedef
   enum {
      ARMneon_TRN=210,
      ARMneon_ZIP,
      ARMneon_UZP
      
   }
   ARMNeonDualOp;

extern const HChar* showARMNeonBinOp ( ARMNeonBinOp op );
extern const HChar* showARMNeonUnOp ( ARMNeonUnOp op );
extern const HChar* showARMNeonUnOpS ( ARMNeonUnOpS op );
extern const HChar* showARMNeonShiftOp ( ARMNeonShiftOp op );
extern const HChar* showARMNeonDualOp ( ARMNeonDualOp op );
extern const HChar* showARMNeonBinOpDataType ( ARMNeonBinOp op );
extern const HChar* showARMNeonUnOpDataType ( ARMNeonUnOp op );
extern const HChar* showARMNeonUnOpSDataType ( ARMNeonUnOpS op );
extern const HChar* showARMNeonShiftOpDataType ( ARMNeonShiftOp op );
extern const HChar* showARMNeonDualOpDataType ( ARMNeonDualOp op );

typedef
   enum {
      
      ARMin_Alu=220,
      ARMin_Shift,
      ARMin_Unary,
      ARMin_CmpOrTst,
      ARMin_Mov,
      ARMin_Imm32,
      ARMin_LdSt32,
      ARMin_LdSt16,
      ARMin_LdSt8U,
      ARMin_Ld8S,
      ARMin_XDirect,     
      ARMin_XIndir,      
      ARMin_XAssisted,   
      ARMin_CMov,
      ARMin_Call,
      ARMin_Mul,
      ARMin_LdrEX,
      ARMin_StrEX,
      
      ARMin_VLdStD,
      ARMin_VLdStS,
      ARMin_VAluD,
      ARMin_VAluS,
      ARMin_VUnaryD,
      ARMin_VUnaryS,
      ARMin_VCmpD,
      ARMin_VCMovD,
      ARMin_VCMovS,
      ARMin_VCvtSD,
      ARMin_VXferD,
      ARMin_VXferS,
      ARMin_VCvtID,
      ARMin_FPSCR,
      ARMin_MFence,
      ARMin_CLREX,
      
      ARMin_NLdStQ,
      ARMin_NLdStD,
      ARMin_NUnary,
      ARMin_NUnaryS,
      ARMin_NDual,
      ARMin_NBinary,
      ARMin_NBinaryS,
      ARMin_NShift,
      ARMin_NShl64, 
      ARMin_NeonImm,
      ARMin_NCMovQ,
      ARMin_Add32,
      ARMin_EvCheck,     
      ARMin_ProfInc      
   }
   ARMInstrTag;


typedef
   struct {
      ARMInstrTag tag;
      union {
         
         struct {
            ARMAluOp op;
            HReg     dst;
            HReg     argL;
            ARMRI84* argR;
         } Alu;
         
         struct {
            ARMShiftOp op;
            HReg       dst;
            HReg       argL;
            ARMRI5*    argR;
         } Shift;
         
         struct {
            ARMUnaryOp op;
            HReg       dst;
            HReg       src;
         } Unary;
         
         struct {
            Bool     isCmp;
            HReg     argL;
            ARMRI84* argR;
         } CmpOrTst;
         
         struct {
            HReg     dst;
            ARMRI84* src;
         } Mov;
         
         struct {
            HReg dst;
            UInt imm32;
         } Imm32;
         
         struct {
            ARMCondCode cc; 
            Bool        isLoad;
            HReg        rD;
            ARMAMode1*  amode;
         } LdSt32;
         
         struct {
            ARMCondCode cc; 
            Bool        isLoad;
            Bool        signedLoad;
            HReg        rD;
            ARMAMode2*  amode;
         } LdSt16;
         
         struct {
            ARMCondCode cc; 
            Bool        isLoad;
            HReg        rD;
            ARMAMode1*  amode;
         } LdSt8U;
         
         struct {
            ARMCondCode cc; 
            HReg        rD;
            ARMAMode2*  amode;
         } Ld8S;
         struct {
            Addr32      dstGA;    
            ARMAMode1*  amR15T;   
            ARMCondCode cond;     
            Bool        toFastEP; 
         } XDirect;
         struct {
            HReg        dstGA;
            ARMAMode1*  amR15T;
            ARMCondCode cond; 
         } XIndir;
         struct {
            HReg        dstGA;
            ARMAMode1*  amR15T;
            ARMCondCode cond; 
            IRJumpKind  jk;
         } XAssisted;
         struct {
            ARMCondCode cond;
            HReg        dst;
            ARMRI84*    src;
         } CMov;
         struct {
            ARMCondCode cond;
            Addr32      target;
            Int         nArgRegs; 
            RetLoc      rloc;     
         } Call;
         struct {
            ARMMulOp op;
         } Mul;
         struct {
            Int  szB; 
         } LdrEX;
         struct {
            Int  szB; 
         } StrEX;
         
         
         struct {
            Bool       isLoad;
            HReg       dD;
            ARMAModeV* amode;
         } VLdStD;
         
         struct {
            Bool       isLoad;
            HReg       fD;
            ARMAModeV* amode;
         } VLdStS;
         
         struct {
            ARMVfpOp op;
            HReg     dst;
            HReg     argL;
            HReg     argR;
         } VAluD;
         
         struct {
            ARMVfpOp op;
            HReg     dst;
            HReg     argL;
            HReg     argR;
         } VAluS;
         
         struct {
            ARMVfpUnaryOp op;
            HReg          dst;
            HReg          src;
         } VUnaryD;
         
         struct {
            ARMVfpUnaryOp op;
            HReg          dst;
            HReg          src;
         } VUnaryS;
         
         struct {
            HReg argL;
            HReg argR;
         } VCmpD;
         struct {
            ARMCondCode cond;
            HReg        dst;
            HReg        src;
         } VCMovD;
         struct {
            ARMCondCode cond;
            HReg        dst;
            HReg        src;
         } VCMovS;
         struct {
            Bool sToD; 
            HReg dst;
            HReg src;
         } VCvtSD;
         
         struct {
            Bool toD;
            HReg dD;
            HReg rHi;
            HReg rLo;
         } VXferD;
         
         struct {
            Bool toS;
            HReg fD;
            HReg rLo;
         } VXferS;
         struct {
            Bool iToD; 
            Bool syned; 
            HReg dst;
            HReg src;
         } VCvtID;
         
         struct {
            Bool toFPSCR;
            HReg iReg;
         } FPSCR;
         struct {
         } MFence;
         
         struct {
         } CLREX;
         struct {
            ARMNeonBinOp op;
            HReg dst;
            HReg argL;
            HReg argR;
            UInt size;
            Bool Q;
         } NBinary;
         struct {
            ARMNeonBinOp op;
            ARMNRS* dst;
            ARMNRS* argL;
            ARMNRS* argR;
            UInt size;
            Bool Q;
         } NBinaryS;
         struct {
            ARMNeonShiftOp op;
            HReg dst;
            HReg argL;
            HReg argR;
            UInt size;
            Bool Q;
         } NShift;
         struct {
            HReg dst;
            HReg src;
            UInt amt; 
         } NShl64;
         struct {
            Bool isLoad;
            HReg dQ;
            ARMAModeN *amode;
         } NLdStQ;
         struct {
            Bool isLoad;
            HReg dD;
            ARMAModeN *amode;
         } NLdStD;
         struct {
            ARMNeonUnOpS op;
            ARMNRS*  dst;
            ARMNRS*  src;
            UInt size;
            Bool Q;
         } NUnaryS;
         struct {
            ARMNeonUnOp op;
            HReg  dst;
            HReg  src;
            UInt size;
            Bool Q;
         } NUnary;
         
         struct {
            ARMNeonDualOp op;
            HReg  arg1;
            HReg  arg2;
            UInt size;
            Bool Q;
         } NDual;
         struct {
            HReg dst;
            ARMNImm* imm;
         } NeonImm;
         struct {
            ARMCondCode cond;
            HReg        dst;
            HReg        src;
         } NCMovQ;
         struct {
            
            HReg rD;
            HReg rN;
            UInt imm32;
         } Add32;
         struct {
            ARMAMode1* amCounter;
            ARMAMode1* amFailAddr;
         } EvCheck;
         struct {
         } ProfInc;
      } ARMin;
   }
   ARMInstr;


extern ARMInstr* ARMInstr_Alu      ( ARMAluOp, HReg, HReg, ARMRI84* );
extern ARMInstr* ARMInstr_Shift    ( ARMShiftOp, HReg, HReg, ARMRI5* );
extern ARMInstr* ARMInstr_Unary    ( ARMUnaryOp, HReg, HReg );
extern ARMInstr* ARMInstr_CmpOrTst ( Bool isCmp, HReg, ARMRI84* );
extern ARMInstr* ARMInstr_Mov      ( HReg, ARMRI84* );
extern ARMInstr* ARMInstr_Imm32    ( HReg, UInt );
extern ARMInstr* ARMInstr_LdSt32   ( ARMCondCode,
                                     Bool isLoad, HReg, ARMAMode1* );
extern ARMInstr* ARMInstr_LdSt16   ( ARMCondCode,
                                     Bool isLoad, Bool signedLoad,
                                     HReg, ARMAMode2* );
extern ARMInstr* ARMInstr_LdSt8U   ( ARMCondCode,
                                     Bool isLoad, HReg, ARMAMode1* );
extern ARMInstr* ARMInstr_Ld8S     ( ARMCondCode, HReg, ARMAMode2* );
extern ARMInstr* ARMInstr_XDirect  ( Addr32 dstGA, ARMAMode1* amR15T,
                                     ARMCondCode cond, Bool toFastEP );
extern ARMInstr* ARMInstr_XIndir   ( HReg dstGA, ARMAMode1* amR15T,
                                     ARMCondCode cond );
extern ARMInstr* ARMInstr_XAssisted ( HReg dstGA, ARMAMode1* amR15T,
                                      ARMCondCode cond, IRJumpKind jk );
extern ARMInstr* ARMInstr_CMov     ( ARMCondCode, HReg dst, ARMRI84* src );
extern ARMInstr* ARMInstr_Call     ( ARMCondCode, Addr32, Int nArgRegs,
                                     RetLoc rloc );
extern ARMInstr* ARMInstr_Mul      ( ARMMulOp op );
extern ARMInstr* ARMInstr_LdrEX    ( Int szB );
extern ARMInstr* ARMInstr_StrEX    ( Int szB );
extern ARMInstr* ARMInstr_VLdStD   ( Bool isLoad, HReg, ARMAModeV* );
extern ARMInstr* ARMInstr_VLdStS   ( Bool isLoad, HReg, ARMAModeV* );
extern ARMInstr* ARMInstr_VAluD    ( ARMVfpOp op, HReg, HReg, HReg );
extern ARMInstr* ARMInstr_VAluS    ( ARMVfpOp op, HReg, HReg, HReg );
extern ARMInstr* ARMInstr_VUnaryD  ( ARMVfpUnaryOp, HReg dst, HReg src );
extern ARMInstr* ARMInstr_VUnaryS  ( ARMVfpUnaryOp, HReg dst, HReg src );
extern ARMInstr* ARMInstr_VCmpD    ( HReg argL, HReg argR );
extern ARMInstr* ARMInstr_VCMovD   ( ARMCondCode, HReg dst, HReg src );
extern ARMInstr* ARMInstr_VCMovS   ( ARMCondCode, HReg dst, HReg src );
extern ARMInstr* ARMInstr_VCvtSD   ( Bool sToD, HReg dst, HReg src );
extern ARMInstr* ARMInstr_VXferD   ( Bool toD, HReg dD, HReg rHi, HReg rLo );
extern ARMInstr* ARMInstr_VXferS   ( Bool toS, HReg fD, HReg rLo );
extern ARMInstr* ARMInstr_VCvtID   ( Bool iToD, Bool syned,
                                     HReg dst, HReg src );
extern ARMInstr* ARMInstr_FPSCR    ( Bool toFPSCR, HReg iReg );
extern ARMInstr* ARMInstr_MFence   ( void );
extern ARMInstr* ARMInstr_CLREX    ( void );
extern ARMInstr* ARMInstr_NLdStQ   ( Bool isLoad, HReg, ARMAModeN* );
extern ARMInstr* ARMInstr_NLdStD   ( Bool isLoad, HReg, ARMAModeN* );
extern ARMInstr* ARMInstr_NUnary   ( ARMNeonUnOp, HReg, HReg, UInt, Bool );
extern ARMInstr* ARMInstr_NUnaryS  ( ARMNeonUnOpS, ARMNRS*, ARMNRS*,
                                     UInt, Bool );
extern ARMInstr* ARMInstr_NDual    ( ARMNeonDualOp, HReg, HReg, UInt, Bool );
extern ARMInstr* ARMInstr_NBinary  ( ARMNeonBinOp, HReg, HReg, HReg,
                                     UInt, Bool );
extern ARMInstr* ARMInstr_NShift   ( ARMNeonShiftOp, HReg, HReg, HReg,
                                     UInt, Bool );
extern ARMInstr* ARMInstr_NShl64   ( HReg, HReg, UInt );
extern ARMInstr* ARMInstr_NeonImm  ( HReg, ARMNImm* );
extern ARMInstr* ARMInstr_NCMovQ   ( ARMCondCode, HReg, HReg );
extern ARMInstr* ARMInstr_Add32    ( HReg rD, HReg rN, UInt imm32 );
extern ARMInstr* ARMInstr_EvCheck  ( ARMAMode1* amCounter,
                                     ARMAMode1* amFailAddr );
extern ARMInstr* ARMInstr_ProfInc  ( void );

extern void ppARMInstr ( const ARMInstr* );


extern void getRegUsage_ARMInstr ( HRegUsage*, const ARMInstr*, Bool );
extern void mapRegs_ARMInstr     ( HRegRemap*, ARMInstr*, Bool );
extern Bool isMove_ARMInstr      ( const ARMInstr*, HReg*, HReg* );
extern Int  emit_ARMInstr        ( Bool* is_profInc,
                                   UChar* buf, Int nbuf, const ARMInstr* i, 
                                   Bool mode64,
                                   VexEndness endness_host,
                                   const void* disp_cp_chain_me_to_slowEP,
                                   const void* disp_cp_chain_me_to_fastEP,
                                   const void* disp_cp_xindir,
                                   const void* disp_cp_xassisted );

extern void genSpill_ARM  ( HInstr** i1, HInstr** i2,
                            HReg rreg, Int offset, Bool );
extern void genReload_ARM ( HInstr** i1, HInstr** i2,
                            HReg rreg, Int offset, Bool );

extern const RRegUniverse* getRRegUniverse_ARM ( void );

extern HInstrArray* iselSB_ARM   ( const IRSB*, 
                                   VexArch,
                                   const VexArchInfo*,
                                   const VexAbiInfo*,
                                   Int offs_Host_EvC_Counter,
                                   Int offs_Host_EvC_FailAddr,
                                   Bool chainingAllowed,
                                   Bool addProfInc,
                                   Addr max_ga );

extern Int evCheckSzB_ARM (void);

extern VexInvalRange chainXDirect_ARM ( VexEndness endness_host,
                                        void* place_to_chain,
                                        const void* disp_cp_chain_me_EXPECTED,
                                        const void* place_to_jump_to );

extern VexInvalRange unchainXDirect_ARM ( VexEndness endness_host,
                                          void* place_to_unchain,
                                          const void* place_to_jump_to_EXPECTED,
                                          const void* disp_cp_chain_me );

extern VexInvalRange patchProfInc_ARM ( VexEndness endness_host,
                                        void*  place_to_patch,
                                        const ULong* location_of_counter );


#endif 

