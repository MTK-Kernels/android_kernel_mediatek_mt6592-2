

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

#ifndef __VEX_HOST_X86_DEFS_H
#define __VEX_HOST_X86_DEFS_H

#include "libvex_basictypes.h"
#include "libvex.h"                      
#include "host_generic_regs.h"           



#define ST_IN static inline
ST_IN HReg hregX86_EAX   ( void ) { return mkHReg(False, HRcInt32,  0,  0); }
ST_IN HReg hregX86_EBX   ( void ) { return mkHReg(False, HRcInt32,  3,  1); }
ST_IN HReg hregX86_ECX   ( void ) { return mkHReg(False, HRcInt32,  1,  2); }
ST_IN HReg hregX86_EDX   ( void ) { return mkHReg(False, HRcInt32,  2,  3); }
ST_IN HReg hregX86_ESI   ( void ) { return mkHReg(False, HRcInt32,  6,  4); }
ST_IN HReg hregX86_EDI   ( void ) { return mkHReg(False, HRcInt32,  7,  5); }

ST_IN HReg hregX86_FAKE0 ( void ) { return mkHReg(False, HRcFlt64,  0,  6); }
ST_IN HReg hregX86_FAKE1 ( void ) { return mkHReg(False, HRcFlt64,  1,  7); }
ST_IN HReg hregX86_FAKE2 ( void ) { return mkHReg(False, HRcFlt64,  2,  8); }
ST_IN HReg hregX86_FAKE3 ( void ) { return mkHReg(False, HRcFlt64,  3,  9); }
ST_IN HReg hregX86_FAKE4 ( void ) { return mkHReg(False, HRcFlt64,  4, 10); }
ST_IN HReg hregX86_FAKE5 ( void ) { return mkHReg(False, HRcFlt64,  5, 11); }

ST_IN HReg hregX86_XMM0  ( void ) { return mkHReg(False, HRcVec128, 0, 12); }
ST_IN HReg hregX86_XMM1  ( void ) { return mkHReg(False, HRcVec128, 1, 13); }
ST_IN HReg hregX86_XMM2  ( void ) { return mkHReg(False, HRcVec128, 2, 14); }
ST_IN HReg hregX86_XMM3  ( void ) { return mkHReg(False, HRcVec128, 3, 15); }
ST_IN HReg hregX86_XMM4  ( void ) { return mkHReg(False, HRcVec128, 4, 16); }
ST_IN HReg hregX86_XMM5  ( void ) { return mkHReg(False, HRcVec128, 5, 17); }
ST_IN HReg hregX86_XMM6  ( void ) { return mkHReg(False, HRcVec128, 6, 18); }
ST_IN HReg hregX86_XMM7  ( void ) { return mkHReg(False, HRcVec128, 7, 19); }

ST_IN HReg hregX86_ESP   ( void ) { return mkHReg(False, HRcInt32,  4, 20); }
ST_IN HReg hregX86_EBP   ( void ) { return mkHReg(False, HRcInt32,  5, 21); }
#undef ST_IN

extern void ppHRegX86 ( HReg );



typedef
   enum {
      Xcc_O      = 0,  
      Xcc_NO     = 1,  

      Xcc_B      = 2,  
      Xcc_NB     = 3,  

      Xcc_Z      = 4,  
      Xcc_NZ     = 5,  

      Xcc_BE     = 6,  
      Xcc_NBE    = 7,  

      Xcc_S      = 8,  
      Xcc_NS     = 9,  

      Xcc_P      = 10, 
      Xcc_NP     = 11, 

      Xcc_L      = 12, 
      Xcc_NL     = 13, 

      Xcc_LE     = 14, 
      Xcc_NLE    = 15, 

      Xcc_ALWAYS = 16  
   }
   X86CondCode;

extern const HChar* showX86CondCode ( X86CondCode );



typedef
   enum {
     Xam_IR,        
     Xam_IRRS       
   }
   X86AModeTag;

typedef
   struct {
      X86AModeTag tag;
      union {
         struct {
            UInt imm;
            HReg reg;
         } IR;
         struct {
            UInt imm;
            HReg base;
            HReg index;
            Int  shift; 
         } IRRS;
      } Xam;
   }
   X86AMode;

extern X86AMode* X86AMode_IR   ( UInt, HReg );
extern X86AMode* X86AMode_IRRS ( UInt, HReg, HReg, Int );

extern X86AMode* dopyX86AMode ( X86AMode* );

extern void ppX86AMode ( X86AMode* );



typedef 
   enum {
      Xrmi_Imm,
      Xrmi_Reg,
      Xrmi_Mem
   }
   X86RMITag;

typedef
   struct {
      X86RMITag tag;
      union {
         struct {
            UInt imm32;
         } Imm;
         struct {
            HReg reg;
         } Reg;
         struct {
            X86AMode* am;
         } Mem;
      }
      Xrmi;
   }
   X86RMI;

extern X86RMI* X86RMI_Imm ( UInt );
extern X86RMI* X86RMI_Reg ( HReg );
extern X86RMI* X86RMI_Mem ( X86AMode* );

extern void ppX86RMI ( X86RMI* );



typedef 
   enum {
      Xri_Imm,
      Xri_Reg
   }
   X86RITag;

typedef
   struct {
      X86RITag tag;
      union {
         struct {
            UInt imm32;
         } Imm;
         struct {
            HReg reg;
         } Reg;
      }
      Xri;
   }
   X86RI;

extern X86RI* X86RI_Imm ( UInt );
extern X86RI* X86RI_Reg ( HReg );

extern void ppX86RI ( X86RI* );



typedef 
   enum {
      Xrm_Reg,
      Xrm_Mem
   }
   X86RMTag;

typedef
   struct {
      X86RMTag tag;
      union {
         struct {
            HReg reg;
         } Reg;
         struct {
            X86AMode* am;
         } Mem;
      }
      Xrm;
   }
   X86RM;

extern X86RM* X86RM_Reg ( HReg );
extern X86RM* X86RM_Mem ( X86AMode* );

extern void ppX86RM ( X86RM* );



typedef
   enum {
      Xun_NEG,
      Xun_NOT
   }
   X86UnaryOp;

extern const HChar* showX86UnaryOp ( X86UnaryOp );


typedef 
   enum {
      Xalu_INVALID,
      Xalu_MOV,
      Xalu_CMP,
      Xalu_ADD, Xalu_SUB, Xalu_ADC, Xalu_SBB, 
      Xalu_AND, Xalu_OR, Xalu_XOR,
      Xalu_MUL
   }
   X86AluOp;

extern const HChar* showX86AluOp ( X86AluOp );


typedef
   enum {
      Xsh_INVALID,
      Xsh_SHL, Xsh_SHR, Xsh_SAR
   }
   X86ShiftOp;

extern const HChar* showX86ShiftOp ( X86ShiftOp );


typedef
   enum {
      Xfp_INVALID,
      
      Xfp_ADD, Xfp_SUB, Xfp_MUL, Xfp_DIV, 
      Xfp_SCALE, Xfp_ATAN, Xfp_YL2X, Xfp_YL2XP1, Xfp_PREM, Xfp_PREM1,
      
      Xfp_SQRT, Xfp_ABS, Xfp_NEG, Xfp_MOV, Xfp_SIN, Xfp_COS, Xfp_TAN,
      Xfp_ROUND, Xfp_2XM1
   }
   X86FpOp;

extern const HChar* showX86FpOp ( X86FpOp );


typedef
   enum {
      Xsse_INVALID,
      
      Xsse_MOV,
      
      Xsse_ADDF, Xsse_SUBF, Xsse_MULF, Xsse_DIVF,
      Xsse_MAXF, Xsse_MINF,
      Xsse_CMPEQF, Xsse_CMPLTF, Xsse_CMPLEF, Xsse_CMPUNF,
      
      Xsse_RCPF, Xsse_RSQRTF, Xsse_SQRTF, 
      
      Xsse_AND, Xsse_OR, Xsse_XOR, Xsse_ANDN,
      
      Xsse_ADD8,   Xsse_ADD16,   Xsse_ADD32,   Xsse_ADD64,
      Xsse_QADD8U, Xsse_QADD16U,
      Xsse_QADD8S, Xsse_QADD16S,
      Xsse_SUB8,   Xsse_SUB16,   Xsse_SUB32,   Xsse_SUB64,
      Xsse_QSUB8U, Xsse_QSUB16U,
      Xsse_QSUB8S, Xsse_QSUB16S,
      Xsse_MUL16,
      Xsse_MULHI16U,
      Xsse_MULHI16S,
      Xsse_AVG8U, Xsse_AVG16U,
      Xsse_MAX16S,
      Xsse_MAX8U,
      Xsse_MIN16S,
      Xsse_MIN8U,
      Xsse_CMPEQ8,  Xsse_CMPEQ16,  Xsse_CMPEQ32,
      Xsse_CMPGT8S, Xsse_CMPGT16S, Xsse_CMPGT32S,
      Xsse_SHL16, Xsse_SHL32, Xsse_SHL64,
      Xsse_SHR16, Xsse_SHR32, Xsse_SHR64,
      Xsse_SAR16, Xsse_SAR32, 
      Xsse_PACKSSD, Xsse_PACKSSW, Xsse_PACKUSW,
      Xsse_UNPCKHB, Xsse_UNPCKHW, Xsse_UNPCKHD, Xsse_UNPCKHQ,
      Xsse_UNPCKLB, Xsse_UNPCKLW, Xsse_UNPCKLD, Xsse_UNPCKLQ
   }
   X86SseOp;

extern const HChar* showX86SseOp ( X86SseOp );


typedef
   enum {
      Xin_Alu32R,    
      Xin_Alu32M,    
      Xin_Sh32,      
      Xin_Test32,    
      Xin_Unary32,   
      Xin_Lea32,     
      Xin_MulL,      
      Xin_Div,       
      Xin_Sh3232,    
      Xin_Push,      
      Xin_Call,      
      Xin_XDirect,   
      Xin_XIndir,    
      Xin_XAssisted, 
      Xin_CMov32,    
      Xin_LoadEX,    
      Xin_Store,     
      Xin_Set32,     
      Xin_Bsfr32,    
      Xin_MFence,    
      Xin_ACAS,      
      Xin_DACAS,     

      Xin_FpUnary,   
      Xin_FpBinary,  
      Xin_FpLdSt,    
      Xin_FpLdStI,   
      Xin_Fp64to32,  
      Xin_FpCMov,    
      Xin_FpLdCW,    
      Xin_FpStSW_AX, 
      Xin_FpCmp,     

      Xin_SseConst,  
      Xin_SseLdSt,   
      Xin_SseLdzLO,  
      Xin_Sse32Fx4,  
      Xin_Sse32FLo,  
      Xin_Sse64Fx2,  
      Xin_Sse64FLo,  
      Xin_SseReRg,   
      Xin_SseCMov,   
      Xin_SseShuf,   
      Xin_EvCheck,   
      Xin_ProfInc    
   }
   X86InstrTag;


typedef
   struct {
      X86InstrTag tag;
      union {
         struct {
            X86AluOp op;
            X86RMI*  src;
            HReg     dst;
         } Alu32R;
         struct {
            X86AluOp  op;
            X86RI*    src;
            X86AMode* dst;
         } Alu32M;
         struct {
            X86ShiftOp op;
            UInt  src;  
            HReg  dst;
         } Sh32;
         struct {
            UInt   imm32;
            X86RM* dst; /* not written, only read */
         } Test32;
         
         struct {
            X86UnaryOp op;
            HReg       dst;
         } Unary32;
         
         struct {
            X86AMode* am;
            HReg      dst;
         } Lea32;
         
         struct {
            Bool   syned;
            X86RM* src;
         } MulL;
         
         struct {
            Bool   syned;
            X86RM* src;
         } Div;
         
         struct {
            X86ShiftOp op;
            UInt       amt;   
            HReg       src;
            HReg       dst;
         } Sh3232;
         struct {
            X86RMI* src;
         } Push;
         struct {
            X86CondCode cond;
            Addr32      target;
            Int         regparms; 
            RetLoc      rloc;     
         } Call;
         struct {
            Addr32      dstGA;    
            X86AMode*   amEIP;    
            X86CondCode cond;     
            Bool        toFastEP; 
         } XDirect;
         struct {
            HReg        dstGA;
            X86AMode*   amEIP;
            X86CondCode cond; 
         } XIndir;
         struct {
            HReg        dstGA;
            X86AMode*   amEIP;
            X86CondCode cond; 
            IRJumpKind  jk;
         } XAssisted;
         struct {
            X86CondCode cond;
            X86RM*      src;
            HReg        dst;
         } CMov32;
         
         struct {
            UChar     szSmall;
            Bool      syned;
            X86AMode* src;
            HReg      dst;
         } LoadEX;
         struct {
            UChar     sz; 
            HReg      src;
            X86AMode* dst;
         } Store;
         
         struct {
            X86CondCode cond;
            HReg        dst;
         } Set32;
         
         struct {
            Bool isFwds;
            HReg src;
            HReg dst;
         } Bsfr32;
         struct {
            UInt hwcaps;
         } MFence;
         struct {
            X86AMode* addr;
            UChar     sz; 
         } ACAS;
         struct {
            X86AMode* addr;
         } DACAS;

         
         struct {
            X86FpOp op;
            HReg    src;
            HReg    dst;
         } FpUnary;
         struct {
            X86FpOp op;
            HReg    srcL;
            HReg    srcR;
            HReg    dst;
         } FpBinary;
         struct {
            Bool      isLoad;
            UChar     sz; 
            HReg      reg;
            X86AMode* addr;
         } FpLdSt;
         struct {
            Bool      isLoad;
            UChar     sz; 
            HReg      reg;
            X86AMode* addr;
         } FpLdStI;
         struct {
            HReg src;
            HReg dst;
         } Fp64to32;
         struct {
            X86CondCode cond;
            HReg        src;
            HReg        dst;
         } FpCMov;
         
         struct {
            X86AMode* addr;
         }
         FpLdCW;
         
         struct {
            
         }
         FpStSW_AX;
         
         struct {
            HReg    srcL;
            HReg    srcR;
            HReg    dst;
         } FpCmp;

         
         struct {
            UShort  con;
            HReg    dst;
         } SseConst;
         struct {
            Bool      isLoad;
            HReg      reg;
            X86AMode* addr;
         } SseLdSt;
         struct {
            UChar     sz; 
            HReg      reg;
            X86AMode* addr;
         } SseLdzLO;
         struct {
            X86SseOp op;
            HReg     src;
            HReg     dst;
         } Sse32Fx4;
         struct {
            X86SseOp op;
            HReg     src;
            HReg     dst;
         } Sse32FLo;
         struct {
            X86SseOp op;
            HReg     src;
            HReg     dst;
         } Sse64Fx2;
         struct {
            X86SseOp op;
            HReg     src;
            HReg     dst;
         } Sse64FLo;
         struct {
            X86SseOp op;
            HReg     src;
            HReg     dst;
         } SseReRg;
         struct {
            X86CondCode cond;
            HReg        src;
            HReg        dst;
         } SseCMov;
         struct {
            Int    order; 
            HReg   src;
            HReg   dst;
         } SseShuf;
         struct {
            X86AMode* amCounter;
            X86AMode* amFailAddr;
         } EvCheck;
         struct {
         } ProfInc;

      } Xin;
   }
   X86Instr;

extern X86Instr* X86Instr_Alu32R    ( X86AluOp, X86RMI*, HReg );
extern X86Instr* X86Instr_Alu32M    ( X86AluOp, X86RI*,  X86AMode* );
extern X86Instr* X86Instr_Unary32   ( X86UnaryOp op, HReg dst );
extern X86Instr* X86Instr_Lea32     ( X86AMode* am, HReg dst );

extern X86Instr* X86Instr_Sh32      ( X86ShiftOp, UInt, HReg );
extern X86Instr* X86Instr_Test32    ( UInt imm32, X86RM* dst );
extern X86Instr* X86Instr_MulL      ( Bool syned, X86RM* );
extern X86Instr* X86Instr_Div       ( Bool syned, X86RM* );
extern X86Instr* X86Instr_Sh3232    ( X86ShiftOp, UInt amt, HReg src, HReg dst );
extern X86Instr* X86Instr_Push      ( X86RMI* );
extern X86Instr* X86Instr_Call      ( X86CondCode, Addr32, Int, RetLoc );
extern X86Instr* X86Instr_XDirect   ( Addr32 dstGA, X86AMode* amEIP,
                                      X86CondCode cond, Bool toFastEP );
extern X86Instr* X86Instr_XIndir    ( HReg dstGA, X86AMode* amEIP,
                                      X86CondCode cond );
extern X86Instr* X86Instr_XAssisted ( HReg dstGA, X86AMode* amEIP,
                                      X86CondCode cond, IRJumpKind jk );
extern X86Instr* X86Instr_CMov32    ( X86CondCode, X86RM* src, HReg dst );
extern X86Instr* X86Instr_LoadEX    ( UChar szSmall, Bool syned,
                                      X86AMode* src, HReg dst );
extern X86Instr* X86Instr_Store     ( UChar sz, HReg src, X86AMode* dst );
extern X86Instr* X86Instr_Set32     ( X86CondCode cond, HReg dst );
extern X86Instr* X86Instr_Bsfr32    ( Bool isFwds, HReg src, HReg dst );
extern X86Instr* X86Instr_MFence    ( UInt hwcaps );
extern X86Instr* X86Instr_ACAS      ( X86AMode* addr, UChar sz );
extern X86Instr* X86Instr_DACAS     ( X86AMode* addr );

extern X86Instr* X86Instr_FpUnary   ( X86FpOp op, HReg src, HReg dst );
extern X86Instr* X86Instr_FpBinary  ( X86FpOp op, HReg srcL, HReg srcR, HReg dst );
extern X86Instr* X86Instr_FpLdSt    ( Bool isLoad, UChar sz, HReg reg, X86AMode* );
extern X86Instr* X86Instr_FpLdStI   ( Bool isLoad, UChar sz, HReg reg, X86AMode* );
extern X86Instr* X86Instr_Fp64to32  ( HReg src, HReg dst );
extern X86Instr* X86Instr_FpCMov    ( X86CondCode, HReg src, HReg dst );
extern X86Instr* X86Instr_FpLdCW    ( X86AMode* );
extern X86Instr* X86Instr_FpStSW_AX ( void );
extern X86Instr* X86Instr_FpCmp     ( HReg srcL, HReg srcR, HReg dst );

extern X86Instr* X86Instr_SseConst  ( UShort con, HReg dst );
extern X86Instr* X86Instr_SseLdSt   ( Bool isLoad, HReg, X86AMode* );
extern X86Instr* X86Instr_SseLdzLO  ( Int sz, HReg, X86AMode* );
extern X86Instr* X86Instr_Sse32Fx4  ( X86SseOp, HReg, HReg );
extern X86Instr* X86Instr_Sse32FLo  ( X86SseOp, HReg, HReg );
extern X86Instr* X86Instr_Sse64Fx2  ( X86SseOp, HReg, HReg );
extern X86Instr* X86Instr_Sse64FLo  ( X86SseOp, HReg, HReg );
extern X86Instr* X86Instr_SseReRg   ( X86SseOp, HReg, HReg );
extern X86Instr* X86Instr_SseCMov   ( X86CondCode, HReg src, HReg dst );
extern X86Instr* X86Instr_SseShuf   ( Int order, HReg src, HReg dst );
extern X86Instr* X86Instr_EvCheck   ( X86AMode* amCounter,
                                      X86AMode* amFailAddr );
extern X86Instr* X86Instr_ProfInc   ( void );


extern void ppX86Instr ( const X86Instr*, Bool );

extern void         getRegUsage_X86Instr ( HRegUsage*, const X86Instr*, Bool );
extern void         mapRegs_X86Instr     ( HRegRemap*, X86Instr*, Bool );
extern Bool         isMove_X86Instr      ( const X86Instr*, HReg*, HReg* );
extern Int          emit_X86Instr   ( Bool* is_profInc,
                                      UChar* buf, Int nbuf, const X86Instr* i, 
                                      Bool mode64,
                                      VexEndness endness_host,
                                      const void* disp_cp_chain_me_to_slowEP,
                                      const void* disp_cp_chain_me_to_fastEP,
                                      const void* disp_cp_xindir,
                                      const void* disp_cp_xassisted );

extern void genSpill_X86  ( HInstr** i1, HInstr** i2,
                            HReg rreg, Int offset, Bool );
extern void genReload_X86 ( HInstr** i1, HInstr** i2,
                            HReg rreg, Int offset, Bool );

extern X86Instr* directReload_X86 ( X86Instr* i, HReg vreg, Short spill_off );

extern const RRegUniverse* getRRegUniverse_X86 ( void );

extern HInstrArray* iselSB_X86           ( const IRSB*,
                                           VexArch,
                                           const VexArchInfo*,
                                           const VexAbiInfo*,
                                           Int offs_Host_EvC_Counter,
                                           Int offs_Host_EvC_FailAddr,
                                           Bool chainingAllowed,
                                           Bool addProfInc,
                                           Addr max_ga );

extern Int evCheckSzB_X86 (void);

extern VexInvalRange chainXDirect_X86 ( VexEndness endness_host,
                                        void* place_to_chain,
                                        const void* disp_cp_chain_me_EXPECTED,
                                        const void* place_to_jump_to );

extern VexInvalRange unchainXDirect_X86 ( VexEndness endness_host,
                                          void* place_to_unchain,
                                          const void* place_to_jump_to_EXPECTED,
                                          const void* disp_cp_chain_me );

extern VexInvalRange patchProfInc_X86 ( VexEndness endness_host,
                                        void*  place_to_patch,
                                        const ULong* location_of_counter );


#endif 

