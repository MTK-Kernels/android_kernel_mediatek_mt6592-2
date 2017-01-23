

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2010-2013 RT-RK
      mips-valgrind@rt-rk.com

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

#ifndef __VEX_HOST_MIPS_DEFS_H
#define __VEX_HOST_MIPS_DEFS_H

#include "libvex_basictypes.h"
#include "libvex.h"             
#include "host_generic_regs.h"  



#define ST_IN static inline

#define GPR(_mode64, _enc, _ix64, _ix32) \
  mkHReg(False,  (_mode64) ? HRcInt64 : HRcInt32, \
         (_enc), (_mode64) ? (_ix64) : (_ix32))

#define FR(_mode64, _enc, _ix64, _ix32) \
  mkHReg(False,  (_mode64) ? HRcFlt64 : HRcFlt32, \
         (_enc), (_mode64) ? (_ix64) : (_ix32))

#define DR(_mode64, _enc, _ix64, _ix32) \
  mkHReg(False,  HRcFlt64, \
         (_enc), (_mode64) ? (_ix64) : (_ix32))

ST_IN HReg hregMIPS_GPR16 ( Bool mode64 ) { return GPR(mode64, 16,  0,  0); }
ST_IN HReg hregMIPS_GPR17 ( Bool mode64 ) { return GPR(mode64, 17,  1,  1); }
ST_IN HReg hregMIPS_GPR18 ( Bool mode64 ) { return GPR(mode64, 18,  2,  2); }
ST_IN HReg hregMIPS_GPR19 ( Bool mode64 ) { return GPR(mode64, 19,  3,  3); }
ST_IN HReg hregMIPS_GPR20 ( Bool mode64 ) { return GPR(mode64, 20,  4,  4); }
ST_IN HReg hregMIPS_GPR21 ( Bool mode64 ) { return GPR(mode64, 21,  5,  5); }
ST_IN HReg hregMIPS_GPR22 ( Bool mode64 ) { return GPR(mode64, 22,  6,  6); }

ST_IN HReg hregMIPS_GPR12 ( Bool mode64 ) { return GPR(mode64, 12,  7,  7); }
ST_IN HReg hregMIPS_GPR13 ( Bool mode64 ) { return GPR(mode64, 13,  8,  8); }
ST_IN HReg hregMIPS_GPR14 ( Bool mode64 ) { return GPR(mode64, 14,  9,  9); }
ST_IN HReg hregMIPS_GPR15 ( Bool mode64 ) { return GPR(mode64, 15, 10, 10); }
ST_IN HReg hregMIPS_GPR24 ( Bool mode64 ) { return GPR(mode64, 24, 11, 11); }

ST_IN HReg hregMIPS_F16   ( Bool mode64 ) { return FR (mode64, 16, 12, 12); }
ST_IN HReg hregMIPS_F18   ( Bool mode64 ) { return FR (mode64, 18, 13, 13); }
ST_IN HReg hregMIPS_F20   ( Bool mode64 ) { return FR (mode64, 20, 14, 14); }
ST_IN HReg hregMIPS_F22   ( Bool mode64 ) { return FR (mode64, 22, 15, 15); }
ST_IN HReg hregMIPS_F24   ( Bool mode64 ) { return FR (mode64, 24, 16, 16); }
ST_IN HReg hregMIPS_F26   ( Bool mode64 ) { return FR (mode64, 26, 17, 17); }
ST_IN HReg hregMIPS_F28   ( Bool mode64 ) { return FR (mode64, 28, 18, 18); }
ST_IN HReg hregMIPS_F30   ( Bool mode64 ) { return FR (mode64, 30, 19, 19); }

ST_IN HReg hregMIPS_D0    ( Bool mode64 ) { vassert(!mode64);
                                            return DR (mode64,  0,  0, 20); }
ST_IN HReg hregMIPS_D1    ( Bool mode64 ) { vassert(!mode64);
                                            return DR (mode64,  2,  0, 21); }
ST_IN HReg hregMIPS_D2    ( Bool mode64 ) { vassert(!mode64);
                                            return DR (mode64,  4,  0, 22); }
ST_IN HReg hregMIPS_D3    ( Bool mode64 ) { vassert(!mode64);
                                            return DR (mode64,  6,  0, 23); }
ST_IN HReg hregMIPS_D4    ( Bool mode64 ) { vassert(!mode64);
                                            return DR (mode64,  8,  0, 24); }
ST_IN HReg hregMIPS_D5    ( Bool mode64 ) { vassert(!mode64);
                                            return DR (mode64, 10,  0, 25); }
ST_IN HReg hregMIPS_D6    ( Bool mode64 ) { vassert(!mode64);
                                            return DR (mode64, 12,  0, 26); }
ST_IN HReg hregMIPS_D7    ( Bool mode64 ) { vassert(!mode64);
                                            return DR (mode64, 14,  0, 27); }

ST_IN HReg hregMIPS_HI    ( Bool mode64 ) { return FR (mode64, 33, 20, 28); }
ST_IN HReg hregMIPS_LO    ( Bool mode64 ) { return FR (mode64, 34, 21, 29); }

ST_IN HReg hregMIPS_GPR0  ( Bool mode64 ) { return GPR(mode64,  0, 22, 30); }
ST_IN HReg hregMIPS_GPR1  ( Bool mode64 ) { return GPR(mode64,  1, 23, 31); }
ST_IN HReg hregMIPS_GPR2  ( Bool mode64 ) { return GPR(mode64,  2, 24, 32); }
ST_IN HReg hregMIPS_GPR3  ( Bool mode64 ) { return GPR(mode64,  3, 25, 33); }
ST_IN HReg hregMIPS_GPR4  ( Bool mode64 ) { return GPR(mode64,  4, 26, 34); }
ST_IN HReg hregMIPS_GPR5  ( Bool mode64 ) { return GPR(mode64,  5, 27, 35); }
ST_IN HReg hregMIPS_GPR6  ( Bool mode64 ) { return GPR(mode64,  6, 28, 36); }
ST_IN HReg hregMIPS_GPR7  ( Bool mode64 ) { return GPR(mode64,  7, 29, 37); }
ST_IN HReg hregMIPS_GPR8  ( Bool mode64 ) { return GPR(mode64,  8, 30, 38); }
ST_IN HReg hregMIPS_GPR9  ( Bool mode64 ) { return GPR(mode64,  9, 31, 39); }
ST_IN HReg hregMIPS_GPR10 ( Bool mode64 ) { return GPR(mode64, 10, 32, 40); }
ST_IN HReg hregMIPS_GPR11 ( Bool mode64 ) { return GPR(mode64, 11, 33, 41); }
ST_IN HReg hregMIPS_GPR23 ( Bool mode64 ) { return GPR(mode64, 23, 34, 42); }
ST_IN HReg hregMIPS_GPR25 ( Bool mode64 ) { return GPR(mode64, 25, 35, 43); }
ST_IN HReg hregMIPS_GPR29 ( Bool mode64 ) { return GPR(mode64, 29, 36, 44); }
ST_IN HReg hregMIPS_GPR31 ( Bool mode64 ) { return GPR(mode64, 31, 37, 45); }

#undef ST_IN
#undef GPR
#undef FR
#undef DR

#define GuestStatePointer(_mode64)     hregMIPS_GPR23(_mode64)
#define StackFramePointer(_mode64)     hregMIPS_GPR30(_mode64)
#define StackPointer(_mode64)          hregMIPS_GPR29(_mode64)

#if defined(VGP_mips32_linux)
  
# define MIPS_N_REGPARMS 4
#else
  
# define MIPS_N_REGPARMS 8
#endif

extern void ppHRegMIPS ( HReg, Bool );


typedef enum {
   MIPScc_EQ = 0,   
   MIPScc_NE = 1,   

   MIPScc_HS = 2,   
   MIPScc_LO = 3,   

   MIPScc_MI = 4,   
   MIPScc_PL = 5,   

   MIPScc_VS = 6,   
   MIPScc_VC = 7,   

   MIPScc_HI = 8,   
   MIPScc_LS = 9,   

   MIPScc_GE = 10,  
   MIPScc_LT = 11,  

   MIPScc_GT = 12,  
   MIPScc_LE = 13,  

   MIPScc_AL = 14,  
   MIPScc_NV = 15   
} MIPSCondCode;

extern const HChar *showMIPSCondCode(MIPSCondCode);

typedef enum {
   Mam_IR,        
   Mam_RR         
} MIPSAModeTag;

typedef struct {
   MIPSAModeTag tag;
   union {
      struct {
         HReg base;
         Int index;
      } IR;
      struct {
         HReg base;
         HReg index;
      } RR;
   } Mam;
} MIPSAMode;

extern MIPSAMode *MIPSAMode_IR(Int, HReg);
extern MIPSAMode *MIPSAMode_RR(HReg, HReg);

extern MIPSAMode *dopyMIPSAMode(MIPSAMode *);
extern MIPSAMode *nextMIPSAModeFloat(MIPSAMode *);
extern MIPSAMode *nextMIPSAModeInt(MIPSAMode *);

extern void ppMIPSAMode(MIPSAMode *, Bool);

typedef enum {
   Mrh_Imm,
   Mrh_Reg
} MIPSRHTag;

typedef struct {
   MIPSRHTag tag;
   union {
      struct {
         Bool syned;
         UShort imm16;
      } Imm;
      struct {
         HReg reg;
      } Reg;
   } Mrh;
} MIPSRH;

extern void ppMIPSRH(MIPSRH *, Bool);

extern MIPSRH *MIPSRH_Imm(Bool, UShort);
extern MIPSRH *MIPSRH_Reg(HReg);



typedef enum {
   Mun_CLO,
   Mun_CLZ,
   Mun_DCLO,
   Mun_DCLZ,
   Mun_NOP,
} MIPSUnaryOp;

extern const HChar *showMIPSUnaryOp(MIPSUnaryOp);


typedef enum {
   Malu_INVALID,
   Malu_ADD, Malu_SUB,
   Malu_AND, Malu_OR, Malu_NOR, Malu_XOR,
   Malu_DADD, Malu_DSUB,
   Malu_SLT
} MIPSAluOp;

extern const HChar *showMIPSAluOp(MIPSAluOp,
                            Bool  );

typedef enum {
   Mshft_INVALID,
   Mshft_SLL, Mshft_SRL,
   Mshft_SRA
} MIPSShftOp;

extern const HChar *showMIPSShftOp(MIPSShftOp,
                             Bool  ,
                             Bool  );

typedef enum {
   Macc_ADD,
   Macc_SUB
} MIPSMaccOp;

extern const HChar *showMIPSMaccOp(MIPSMaccOp, Bool);

typedef enum {
   Min_LI,         
   Min_Alu,        
   Min_Shft,       
   Min_Unary,      

   Min_Cmp,        

   Min_Mul,        
   Min_Div,        

   Min_Call,       

   
   Min_XDirect,    
   Min_XIndir,     
   Min_XAssisted,  
   Min_EvCheck,    
   Min_ProfInc,    

   Min_RdWrLR,     
   Min_Mthi,       
   Min_Mtlo,       
   Min_Mfhi,       
   Min_Mflo,       
   Min_Macc,       

   Min_Load,       
   Min_Store,      
   Min_Cas,        
   Min_LoadL,      
   Min_StoreC,     

   Min_FpUnary,    
   Min_FpBinary,   
   Min_FpTernary,  
   Min_FpConvert,  
   Min_FpMulAcc,   
   Min_FpLdSt,     
   Min_FpSTFIW,    
   Min_FpRSP,      
   Min_FpCftI,     
   Min_FpCMov,     
   Min_MtFCSR,     
   Min_MfFCSR,     
   Min_FpCompare,  

   Min_FpGpMove,   
   Min_MoveCond    
} MIPSInstrTag;

typedef enum {
   Mfp_INVALID,

   
   Mfp_MADDD, Mfp_MSUBD,
   Mfp_MADDS, Mfp_MSUBS,

   
   Mfp_ADDD, Mfp_SUBD, Mfp_MULD, Mfp_DIVD,
   Mfp_ADDS, Mfp_SUBS, Mfp_MULS, Mfp_DIVS,

   
   Mfp_SQRTS, Mfp_SQRTD,
   Mfp_ABSS, Mfp_ABSD, Mfp_NEGS, Mfp_NEGD, Mfp_MOVS, Mfp_MOVD,

   
   Mfp_CVTSD, Mfp_CVTSW, Mfp_CVTWD,
   Mfp_CVTWS, Mfp_CVTDL, Mfp_CVTSL, Mfp_CVTLS, Mfp_CVTLD, Mfp_TRULS, Mfp_TRULD,
   Mfp_TRUWS, Mfp_TRUWD, Mfp_FLOORWS, Mfp_FLOORWD, Mfp_ROUNDWS, Mfp_ROUNDWD,
   Mfp_CVTDW, Mfp_CEILWS, Mfp_CEILWD, Mfp_CEILLS, Mfp_CEILLD, Mfp_CVTDS,
   Mfp_ROUNDLD, Mfp_FLOORLD,

   
   Mfp_CMP_UN, Mfp_CMP_EQ, Mfp_CMP_LT, Mfp_CMP_NGT

} MIPSFpOp;

extern const HChar *showMIPSFpOp(MIPSFpOp);

typedef enum {
   MFpGpMove_mfc1,   
   MFpGpMove_dmfc1,  
   MFpGpMove_mtc1,   
   MFpGpMove_dmtc1   
} MIPSFpGpMoveOp;

extern const HChar *showMIPSFpGpMoveOp ( MIPSFpGpMoveOp );

typedef enum {
   MFpMoveCond_movns,  
   MFpMoveCond_movnd,
   MMoveCond_movn      
} MIPSMoveCondOp;

extern const HChar *showMIPSMoveCondOp ( MIPSMoveCondOp );


typedef struct {
   MIPSInstrTag tag;
   union {
      struct {
         HReg dst;
         ULong imm;
      } LI;
      struct {
         MIPSAluOp op;
         HReg dst;
         HReg srcL;
         MIPSRH *srcR;
      } Alu;
      struct {
         MIPSShftOp op;
         Bool sz32;  
         HReg dst;
         HReg srcL;
         MIPSRH *srcR;
      } Shft;
      
      struct {
         MIPSUnaryOp op;
         HReg dst;
         HReg src;
      } Unary;
      
      struct {
         Bool syned;
         Bool sz32;
         HReg dst;
         HReg srcL;
         HReg srcR;

         MIPSCondCode cond;
      } Cmp;
      struct {
         Bool widening;  
         Bool syned;     
         Bool sz32;
         HReg dst;
         HReg srcL;
         HReg srcR;
      } Mul;
      struct {
         Bool syned;  
         Bool sz32;
         HReg srcL;
         HReg srcR;
      } Div;
      struct {
         MIPSCondCode cond;
         Addr64 target;
         UInt argiregs;
         HReg src;
         RetLoc rloc;     
      } Call;
      struct {
         Addr64       dstGA;     
         MIPSAMode*   amPC;      
         MIPSCondCode cond;      
         Bool         toFastEP;  
      } XDirect;
      struct {
         HReg        dstGA;
         MIPSAMode*   amPC;
         MIPSCondCode cond; 
      } XIndir;
      struct {
         HReg        dstGA;
         MIPSAMode*   amPC;
         MIPSCondCode cond; 
         IRJumpKind  jk;
      } XAssisted;
      
      struct {
         UChar sz;   
         HReg dst;
         MIPSAMode *src;
      } Load;
      
      struct {
         UChar sz;   
         MIPSAMode *dst;
         HReg src;
      } Store;
      struct {
         UChar sz;   
         HReg dst;
         MIPSAMode *src;
      } LoadL;
      struct {
         UChar sz;   
         HReg  old;
         HReg  addr;
         HReg  expd;
         HReg  data;
      } Cas;
      struct {
         UChar sz;   
         MIPSAMode *dst;
         HReg src;
      } StoreC;
      
      struct {
         HReg dst;
      } MfHL;

      
      struct {
         HReg src;
      } MtHL;

      
      struct {
         Bool wrLR;
         HReg gpr;
      } RdWrLR;

      
      struct {
         MIPSMaccOp op;
         Bool syned;

         HReg srcL;
         HReg srcR;
      } Macc;

      
      struct {
         MIPSFpOp op;
         HReg dst;
         HReg src;
      } FpUnary;
      struct {
         MIPSFpOp op;
         HReg dst;
         HReg srcL;
         HReg srcR;
      } FpBinary;
      struct {
         MIPSFpOp op;
         HReg dst;
         HReg src1;
         HReg src2;
         HReg src3;
      } FpTernary;
      struct {
         MIPSFpOp op;
         HReg dst;
         HReg srcML;
         HReg srcMR;
         HReg srcAcc;
      } FpMulAcc;
      struct {
         Bool isLoad;
         UChar sz;   
         HReg reg;
         MIPSAMode *addr;
      } FpLdSt;

      struct {
         MIPSFpOp op;
         HReg dst;
         HReg src;
      } FpConvert;
      struct {
         MIPSFpOp op;
         HReg dst;
         HReg srcL;
         HReg srcR;
         UChar cond1;
      } FpCompare;
      
      struct {
         HReg src;
      } MtFCSR;
      
      struct {
         HReg dst;
      } MfFCSR;
      struct {
         MIPSAMode* amCounter;
         MIPSAMode* amFailAddr;
      } EvCheck;
      struct {
      } ProfInc;

      
      struct {
         MIPSFpGpMoveOp op;
         HReg dst;
         HReg src;
      } FpGpMove;
      struct {
         MIPSMoveCondOp op;
         HReg dst;
         HReg src;
         HReg cond;
      } MoveCond;

   } Min;
} MIPSInstr;

extern MIPSInstr *MIPSInstr_LI(HReg, ULong);
extern MIPSInstr *MIPSInstr_Alu(MIPSAluOp, HReg, HReg, MIPSRH *);
extern MIPSInstr *MIPSInstr_Shft(MIPSShftOp, Bool sz32, HReg, HReg, MIPSRH *);
extern MIPSInstr *MIPSInstr_Unary(MIPSUnaryOp op, HReg dst, HReg src);
extern MIPSInstr *MIPSInstr_Cmp(Bool, Bool, HReg, HReg, HReg, MIPSCondCode);

extern MIPSInstr *MIPSInstr_Mul(Bool syned, Bool hi32, Bool sz32, HReg,
                                HReg, HReg);
extern MIPSInstr *MIPSInstr_Div(Bool syned, Bool sz32, HReg, HReg);
extern MIPSInstr *MIPSInstr_Madd(Bool, HReg, HReg);
extern MIPSInstr *MIPSInstr_Msub(Bool, HReg, HReg);

extern MIPSInstr *MIPSInstr_Load(UChar sz, HReg dst, MIPSAMode * src,
                                 Bool mode64);
extern MIPSInstr *MIPSInstr_Store(UChar sz, MIPSAMode * dst, HReg src,
                                  Bool mode64);

extern MIPSInstr *MIPSInstr_LoadL(UChar sz, HReg dst, MIPSAMode * src,
                                  Bool mode64);
extern MIPSInstr *MIPSInstr_StoreC(UChar sz, MIPSAMode * dst, HReg src,
                                   Bool mode64);
extern MIPSInstr *MIPSInstr_Cas(UChar sz, HReg old, HReg addr,
                                HReg expd, HReg data, Bool mode64);

extern MIPSInstr *MIPSInstr_Call ( MIPSCondCode, Addr64, UInt, HReg, RetLoc );
extern MIPSInstr *MIPSInstr_CallAlways ( MIPSCondCode, Addr64, UInt, RetLoc );

extern MIPSInstr *MIPSInstr_XDirect ( Addr64 dstGA, MIPSAMode* amPC,
                                      MIPSCondCode cond, Bool toFastEP );
extern MIPSInstr *MIPSInstr_XIndir(HReg dstGA, MIPSAMode* amPC,
                                     MIPSCondCode cond);
extern MIPSInstr *MIPSInstr_XAssisted(HReg dstGA, MIPSAMode* amPC,
                                      MIPSCondCode cond, IRJumpKind jk);

extern MIPSInstr *MIPSInstr_FpUnary(MIPSFpOp op, HReg dst, HReg src);
extern MIPSInstr *MIPSInstr_FpBinary(MIPSFpOp op, HReg dst, HReg srcL,
                                     HReg srcR);
extern MIPSInstr *MIPSInstr_FpTernary ( MIPSFpOp op, HReg dst, HReg src1,
                                        HReg src2, HReg src3 );
extern MIPSInstr *MIPSInstr_FpConvert(MIPSFpOp op, HReg dst, HReg src);
extern MIPSInstr *MIPSInstr_FpCompare(MIPSFpOp op, HReg dst, HReg srcL,
                                      HReg srcR);
extern MIPSInstr *MIPSInstr_FpMulAcc(MIPSFpOp op, HReg dst, HReg srcML,
                                     HReg srcMR, HReg srcAcc);
extern MIPSInstr *MIPSInstr_FpLdSt(Bool isLoad, UChar sz, HReg, MIPSAMode *);
extern MIPSInstr *MIPSInstr_FpSTFIW(HReg addr, HReg data);
extern MIPSInstr *MIPSInstr_FpRSP(HReg dst, HReg src);
extern MIPSInstr *MIPSInstr_FpCftI(Bool fromI, Bool int32, HReg dst, HReg src);
extern MIPSInstr *MIPSInstr_FpCMov(MIPSCondCode, HReg dst, HReg src);
extern MIPSInstr *MIPSInstr_MtFCSR(HReg src);
extern MIPSInstr *MIPSInstr_MfFCSR(HReg dst);
extern MIPSInstr *MIPSInstr_FpCmp(HReg dst, HReg srcL, HReg srcR);

extern MIPSInstr *MIPSInstr_Mfhi(HReg dst);
extern MIPSInstr *MIPSInstr_Mflo(HReg dst);
extern MIPSInstr *MIPSInstr_Mthi(HReg src);
extern MIPSInstr *MIPSInstr_Mtlo(HReg src);

extern MIPSInstr *MIPSInstr_RdWrLR(Bool wrLR, HReg gpr);

extern MIPSInstr *MIPSInstr_MoveCond ( MIPSMoveCondOp op, HReg dst,
                                       HReg src, HReg cond );

extern MIPSInstr *MIPSInstr_FpGpMove ( MIPSFpGpMoveOp op, HReg dst, HReg src );

extern MIPSInstr *MIPSInstr_EvCheck(MIPSAMode* amCounter,
                                    MIPSAMode* amFailAddr );
extern MIPSInstr *MIPSInstr_ProfInc( void );

extern void ppMIPSInstr(const MIPSInstr *, Bool mode64);

extern void getRegUsage_MIPSInstr (HRegUsage *, const MIPSInstr *, Bool);
extern void mapRegs_MIPSInstr     (HRegRemap *, MIPSInstr *, Bool mode64);
extern Bool isMove_MIPSInstr      (const MIPSInstr *, HReg *, HReg *);
extern Int        emit_MIPSInstr (Bool* is_profInc,
                                  UChar* buf, Int nbuf, const MIPSInstr* i,
                                  Bool mode64,
                                  VexEndness endness_host,
                                  const void* disp_cp_chain_me_to_slowEP,
                                  const void* disp_cp_chain_me_to_fastEP,
                                  const void* disp_cp_xindir,
                                  const void* disp_cp_xassisted );

extern void genSpill_MIPS (  HInstr ** i1,  HInstr ** i2,
                            HReg rreg, Int offset, Bool);
extern void genReload_MIPS(  HInstr ** i1,  HInstr ** i2,
                            HReg rreg, Int offset, Bool);

extern const RRegUniverse* getRRegUniverse_MIPS ( Bool mode64 );

extern HInstrArray *iselSB_MIPS          ( const IRSB*,
                                           VexArch,
                                           const VexArchInfo*,
                                           const VexAbiInfo*,
                                           Int offs_Host_EvC_Counter,
                                           Int offs_Host_EvC_FailAddr,
                                           Bool chainingAllowed,
                                           Bool addProfInc,
                                           Addr max_ga );

extern Int evCheckSzB_MIPS (void);

extern VexInvalRange chainXDirect_MIPS ( VexEndness endness_host,
                                         void* place_to_chain,
                                         const void* disp_cp_chain_me_EXPECTED,
                                         const void* place_to_jump_to,
                                         Bool  mode64 );

extern VexInvalRange unchainXDirect_MIPS ( VexEndness endness_host,
                                           void* place_to_unchain,
                                           const void* place_to_jump_to_EXPECTED,
                                           const void* disp_cp_chain_me,
                                           Bool  mode64 );

extern VexInvalRange patchProfInc_MIPS ( VexEndness endness_host,
                                         void*  place_to_patch,
                                         const ULong* location_of_counter,
                                         Bool  mode64 );

#endif 

