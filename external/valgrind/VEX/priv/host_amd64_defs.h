

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

#ifndef __VEX_HOST_AMD64_DEFS_H
#define __VEX_HOST_AMD64_DEFS_H

#include "libvex_basictypes.h"
#include "libvex.h"                      
#include "host_generic_regs.h"           



#define ST_IN static inline
ST_IN HReg hregAMD64_RSI   ( void ) { return mkHReg(False, HRcInt64,   6,  0); }
ST_IN HReg hregAMD64_RDI   ( void ) { return mkHReg(False, HRcInt64,   7,  1); }
ST_IN HReg hregAMD64_R8    ( void ) { return mkHReg(False, HRcInt64,   8,  2); }
ST_IN HReg hregAMD64_R9    ( void ) { return mkHReg(False, HRcInt64,   9,  3); }
ST_IN HReg hregAMD64_R12   ( void ) { return mkHReg(False, HRcInt64,  12,  4); }
ST_IN HReg hregAMD64_R13   ( void ) { return mkHReg(False, HRcInt64,  13,  5); }
ST_IN HReg hregAMD64_R14   ( void ) { return mkHReg(False, HRcInt64,  14,  6); }
ST_IN HReg hregAMD64_R15   ( void ) { return mkHReg(False, HRcInt64,  15,  7); }
ST_IN HReg hregAMD64_RBX   ( void ) { return mkHReg(False, HRcInt64,   3,  8); }

ST_IN HReg hregAMD64_XMM3  ( void ) { return mkHReg(False, HRcVec128,  3,  9); }
ST_IN HReg hregAMD64_XMM4  ( void ) { return mkHReg(False, HRcVec128,  4, 10); }
ST_IN HReg hregAMD64_XMM5  ( void ) { return mkHReg(False, HRcVec128,  5, 11); }
ST_IN HReg hregAMD64_XMM6  ( void ) { return mkHReg(False, HRcVec128,  6, 12); }
ST_IN HReg hregAMD64_XMM7  ( void ) { return mkHReg(False, HRcVec128,  7, 13); }
ST_IN HReg hregAMD64_XMM8  ( void ) { return mkHReg(False, HRcVec128,  8, 14); }
ST_IN HReg hregAMD64_XMM9  ( void ) { return mkHReg(False, HRcVec128,  9, 15); }
ST_IN HReg hregAMD64_XMM10 ( void ) { return mkHReg(False, HRcVec128, 10, 16); }
ST_IN HReg hregAMD64_XMM11 ( void ) { return mkHReg(False, HRcVec128, 11, 17); }
ST_IN HReg hregAMD64_XMM12 ( void ) { return mkHReg(False, HRcVec128, 12, 18); }

ST_IN HReg hregAMD64_R10   ( void ) { return mkHReg(False, HRcInt64,  10, 19); }

ST_IN HReg hregAMD64_RAX   ( void ) { return mkHReg(False, HRcInt64,   0, 20); }
ST_IN HReg hregAMD64_RCX   ( void ) { return mkHReg(False, HRcInt64,   1, 21); }
ST_IN HReg hregAMD64_RDX   ( void ) { return mkHReg(False, HRcInt64,   2, 22); }
ST_IN HReg hregAMD64_RSP   ( void ) { return mkHReg(False, HRcInt64,   4, 23); }
ST_IN HReg hregAMD64_RBP   ( void ) { return mkHReg(False, HRcInt64,   5, 24); }
ST_IN HReg hregAMD64_R11   ( void ) { return mkHReg(False, HRcInt64,  11, 25); }

ST_IN HReg hregAMD64_XMM0  ( void ) { return mkHReg(False, HRcVec128,  0, 26); }
ST_IN HReg hregAMD64_XMM1  ( void ) { return mkHReg(False, HRcVec128,  1, 27); }
#undef ST_IN

extern void ppHRegAMD64 ( HReg );



typedef
   enum {
      Acc_O      = 0,  
      Acc_NO     = 1,  

      Acc_B      = 2,  
      Acc_NB     = 3,  

      Acc_Z      = 4,  
      Acc_NZ     = 5,  

      Acc_BE     = 6,  
      Acc_NBE    = 7,  

      Acc_S      = 8,  
      Acc_NS     = 9,  

      Acc_P      = 10, 
      Acc_NP     = 11, 

      Acc_L      = 12, 
      Acc_NL     = 13, 

      Acc_LE     = 14, 
      Acc_NLE    = 15, 

      Acc_ALWAYS = 16  
   }
   AMD64CondCode;

extern const HChar* showAMD64CondCode ( AMD64CondCode );



typedef
   enum {
     Aam_IR,        
     Aam_IRRS       
   }
   AMD64AModeTag;

typedef
   struct {
      AMD64AModeTag tag;
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
      } Aam;
   }
   AMD64AMode;

extern AMD64AMode* AMD64AMode_IR   ( UInt, HReg );
extern AMD64AMode* AMD64AMode_IRRS ( UInt, HReg, HReg, Int );

extern AMD64AMode* dopyAMD64AMode ( AMD64AMode* );

extern void ppAMD64AMode ( AMD64AMode* );



typedef 
   enum {
      Armi_Imm,
      Armi_Reg,
      Armi_Mem
   }
   AMD64RMITag;

typedef
   struct {
      AMD64RMITag tag;
      union {
         struct {
            UInt imm32;
         } Imm;
         struct {
            HReg reg;
         } Reg;
         struct {
            AMD64AMode* am;
         } Mem;
      }
      Armi;
   }
   AMD64RMI;

extern AMD64RMI* AMD64RMI_Imm ( UInt );
extern AMD64RMI* AMD64RMI_Reg ( HReg );
extern AMD64RMI* AMD64RMI_Mem ( AMD64AMode* );

extern void ppAMD64RMI      ( AMD64RMI* );
extern void ppAMD64RMI_lo32 ( AMD64RMI* );



typedef 
   enum {
      Ari_Imm,
      Ari_Reg
   }
   AMD64RITag;

typedef
   struct {
      AMD64RITag tag;
      union {
         struct {
            UInt imm32;
         } Imm;
         struct {
            HReg reg;
         } Reg;
      }
      Ari;
   }
   AMD64RI;

extern AMD64RI* AMD64RI_Imm ( UInt );
extern AMD64RI* AMD64RI_Reg ( HReg );

extern void ppAMD64RI ( AMD64RI* );



typedef 
   enum {
      Arm_Reg,
      Arm_Mem
   }
   AMD64RMTag;

typedef
   struct {
      AMD64RMTag tag;
      union {
         struct {
            HReg reg;
         } Reg;
         struct {
            AMD64AMode* am;
         } Mem;
      }
      Arm;
   }
   AMD64RM;

extern AMD64RM* AMD64RM_Reg ( HReg );
extern AMD64RM* AMD64RM_Mem ( AMD64AMode* );

extern void ppAMD64RM ( AMD64RM* );



typedef
   enum {
      Aun_NEG,
      Aun_NOT
   }
   AMD64UnaryOp;

extern const HChar* showAMD64UnaryOp ( AMD64UnaryOp );


typedef 
   enum {
      Aalu_INVALID,
      Aalu_MOV,
      Aalu_CMP,
      Aalu_ADD, Aalu_SUB, Aalu_ADC, Aalu_SBB, 
      Aalu_AND, Aalu_OR, Aalu_XOR,
      Aalu_MUL
   }
   AMD64AluOp;

extern const HChar* showAMD64AluOp ( AMD64AluOp );


typedef
   enum {
      Ash_INVALID,
      Ash_SHL, Ash_SHR, Ash_SAR
   }
   AMD64ShiftOp;

extern const HChar* showAMD64ShiftOp ( AMD64ShiftOp );


typedef
   enum {
      Afp_INVALID,
      
      Afp_SCALE, Afp_ATAN, Afp_YL2X, Afp_YL2XP1, Afp_PREM, Afp_PREM1,
      
      Afp_SQRT,
      Afp_SIN, Afp_COS, Afp_TAN,
      Afp_ROUND, Afp_2XM1
   }
   A87FpOp;

extern const HChar* showA87FpOp ( A87FpOp );


typedef
   enum {
      Asse_INVALID,
      
      Asse_MOV,
      
      Asse_ADDF, Asse_SUBF, Asse_MULF, Asse_DIVF,
      Asse_MAXF, Asse_MINF,
      Asse_CMPEQF, Asse_CMPLTF, Asse_CMPLEF, Asse_CMPUNF,
      
      Asse_RCPF, Asse_RSQRTF, Asse_SQRTF, 
      
      Asse_AND, Asse_OR, Asse_XOR, Asse_ANDN,
      Asse_ADD8, Asse_ADD16, Asse_ADD32, Asse_ADD64,
      Asse_QADD8U, Asse_QADD16U,
      Asse_QADD8S, Asse_QADD16S,
      Asse_SUB8, Asse_SUB16, Asse_SUB32, Asse_SUB64,
      Asse_QSUB8U, Asse_QSUB16U,
      Asse_QSUB8S, Asse_QSUB16S,
      Asse_MUL16,
      Asse_MULHI16U,
      Asse_MULHI16S,
      Asse_AVG8U, Asse_AVG16U,
      Asse_MAX16S,
      Asse_MAX8U,
      Asse_MIN16S,
      Asse_MIN8U,
      Asse_CMPEQ8, Asse_CMPEQ16, Asse_CMPEQ32,
      Asse_CMPGT8S, Asse_CMPGT16S, Asse_CMPGT32S,
      Asse_SHL16, Asse_SHL32, Asse_SHL64,
      Asse_SHR16, Asse_SHR32, Asse_SHR64,
      Asse_SAR16, Asse_SAR32, 
      Asse_PACKSSD, Asse_PACKSSW, Asse_PACKUSW,
      Asse_UNPCKHB, Asse_UNPCKHW, Asse_UNPCKHD, Asse_UNPCKHQ,
      Asse_UNPCKLB, Asse_UNPCKLW, Asse_UNPCKLD, Asse_UNPCKLQ
   }
   AMD64SseOp;

extern const HChar* showAMD64SseOp ( AMD64SseOp );


typedef
   enum {
      Ain_Imm64,       
      Ain_Alu64R,      
      Ain_Alu64M,      
      Ain_Sh64,        
      Ain_Test64,      
      Ain_Unary64,     
      Ain_Lea64,       
      Ain_Alu32R,      
      Ain_MulL,        
      Ain_Div,         
      Ain_Push,        
      Ain_Call,        
      Ain_XDirect,     
      Ain_XIndir,      
      Ain_XAssisted,   
      Ain_CMov64,      
      Ain_CLoad,       
      Ain_CStore,      
      Ain_MovxLQ,      
      Ain_LoadEX,      
      Ain_Store,       
      Ain_Set64,       
      Ain_Bsfr64,      
      Ain_MFence,      
      Ain_ACAS,        
      Ain_DACAS,       
      Ain_A87Free,     
      Ain_A87PushPop,  
      Ain_A87FpOp,     
      Ain_A87LdCW,     
      Ain_A87StSW,     
      Ain_LdMXCSR,     
      Ain_SseUComIS,   
      Ain_SseSI2SF,    
      Ain_SseSF2SI,    
      Ain_SseSDSS,     
      Ain_SseLdSt,     
      Ain_SseLdzLO,    
      Ain_Sse32Fx4,    
      Ain_Sse32FLo,    
      Ain_Sse64Fx2,    
      Ain_Sse64FLo,    
      Ain_SseReRg,     
      Ain_SseCMov,     
      Ain_SseShuf,     
      
      
      
      Ain_EvCheck,     
      Ain_ProfInc      
   }
   AMD64InstrTag;


typedef
   struct {
      AMD64InstrTag tag;
      union {
         struct {
            ULong imm64;
            HReg  dst;
         } Imm64;
         struct {
            AMD64AluOp op;
            AMD64RMI*  src;
            HReg       dst;
         } Alu64R;
         struct {
            AMD64AluOp  op;
            AMD64RI*    src;
            AMD64AMode* dst;
         } Alu64M;
         struct {
            AMD64ShiftOp op;
            UInt         src;  
            HReg         dst;
         } Sh64;
         struct {
            UInt   imm32;
            HReg   dst;
         } Test64;
         
         struct {
            AMD64UnaryOp op;
            HReg         dst;
         } Unary64;
         
         struct {
            AMD64AMode* am;
            HReg        dst;
         } Lea64;
         
         struct {
            AMD64AluOp op;
            AMD64RMI*  src;
            HReg       dst;
         } Alu32R;
         struct {
            Bool     syned;
            AMD64RM* src;
         } MulL;
         struct {
            Bool     syned;
            Int      sz; 
            AMD64RM* src;
         } Div;
         struct {
            AMD64RMI* src;
         } Push;
         struct {
            AMD64CondCode cond;
            Addr64        target;
            Int           regparms; 
            RetLoc        rloc;     
         } Call;
         struct {
            Addr64        dstGA;    
            AMD64AMode*   amRIP;    
            AMD64CondCode cond;     
            Bool          toFastEP; 
         } XDirect;
         struct {
            HReg          dstGA;
            AMD64AMode*   amRIP;
            AMD64CondCode cond; 
         } XIndir;
         struct {
            HReg          dstGA;
            AMD64AMode*   amRIP;
            AMD64CondCode cond; 
            IRJumpKind    jk;
         } XAssisted;
         struct {
            AMD64CondCode cond;
            HReg          src;
            HReg          dst;
         } CMov64;
         struct {
            AMD64CondCode cond;
            UChar         szB; 
            AMD64AMode*   addr;
            HReg          dst;
         } CLoad;
         struct {
            AMD64CondCode cond;
            UChar         szB; 
            HReg          src;
            AMD64AMode*   addr;
         } CStore;
         
         struct {
            Bool syned;
            HReg src;
            HReg dst;
         } MovxLQ;
         
         struct {
            UChar       szSmall; 
            Bool        syned;
            AMD64AMode* src;
            HReg        dst;
         } LoadEX;
         
         struct {
            UChar       sz; 
            HReg        src;
            AMD64AMode* dst;
         } Store;
         
         struct {
            AMD64CondCode cond;
            HReg          dst;
         } Set64;
         
         struct {
            Bool isFwds;
            HReg src;
            HReg dst;
         } Bsfr64;
         struct {
         } MFence;
         struct {
            AMD64AMode* addr;
            UChar       sz; 
         } ACAS;
         struct {
            AMD64AMode* addr;
            UChar       sz; 
         } DACAS;

         


         
         struct {
            Int nregs; 
         } A87Free;

         struct {
            AMD64AMode* addr;
            Bool        isPush;
            UChar       szB; 
         } A87PushPop;

         struct {
            A87FpOp op;
         } A87FpOp;

         
         struct {
            AMD64AMode* addr;
         } A87LdCW;

         
         struct {
            AMD64AMode* addr;
         } A87StSW;

         

         
         struct {
            AMD64AMode* addr;
         }
         LdMXCSR;
         
         struct {
            UChar   sz;   
            HReg    srcL; 
            HReg    srcR; 
            HReg    dst;  
         } SseUComIS;
         
         struct {
            UChar szS; 
            UChar szD; 
            HReg  src; 
            HReg  dst; 
         } SseSI2SF;
         
         struct {
            UChar szS; 
            UChar szD; 
            HReg  src; 
            HReg  dst; 
         } SseSF2SI;
         
         struct {
            Bool from64; 
            HReg src;
            HReg dst;
         } SseSDSS;
         struct {
            Bool        isLoad;
            UChar       sz; 
            HReg        reg;
            AMD64AMode* addr;
         } SseLdSt;
         struct {
            Int         sz; 
            HReg        reg;
            AMD64AMode* addr;
         } SseLdzLO;
         struct {
            AMD64SseOp op;
            HReg       src;
            HReg       dst;
         } Sse32Fx4;
         struct {
            AMD64SseOp op;
            HReg       src;
            HReg       dst;
         } Sse32FLo;
         struct {
            AMD64SseOp op;
            HReg       src;
            HReg       dst;
         } Sse64Fx2;
         struct {
            AMD64SseOp op;
            HReg       src;
            HReg       dst;
         } Sse64FLo;
         struct {
            AMD64SseOp op;
            HReg       src;
            HReg       dst;
         } SseReRg;
         struct {
            AMD64CondCode cond;
            HReg          src;
            HReg          dst;
         } SseCMov;
         struct {
            Int    order; 
            HReg   src;
            HReg   dst;
         } SseShuf;
         
         
         
         
         
         
         
         
         
         
         struct {
            AMD64AMode* amCounter;
            AMD64AMode* amFailAddr;
         } EvCheck;
         struct {
         } ProfInc;

      } Ain;
   }
   AMD64Instr;

extern AMD64Instr* AMD64Instr_Imm64      ( ULong imm64, HReg dst );
extern AMD64Instr* AMD64Instr_Alu64R     ( AMD64AluOp, AMD64RMI*, HReg );
extern AMD64Instr* AMD64Instr_Alu64M     ( AMD64AluOp, AMD64RI*,  AMD64AMode* );
extern AMD64Instr* AMD64Instr_Unary64    ( AMD64UnaryOp op, HReg dst );
extern AMD64Instr* AMD64Instr_Lea64      ( AMD64AMode* am, HReg dst );
extern AMD64Instr* AMD64Instr_Alu32R     ( AMD64AluOp, AMD64RMI*, HReg );
extern AMD64Instr* AMD64Instr_Sh64       ( AMD64ShiftOp, UInt, HReg );
extern AMD64Instr* AMD64Instr_Test64     ( UInt imm32, HReg dst );
extern AMD64Instr* AMD64Instr_MulL       ( Bool syned, AMD64RM* );
extern AMD64Instr* AMD64Instr_Div        ( Bool syned, Int sz, AMD64RM* );
extern AMD64Instr* AMD64Instr_Push       ( AMD64RMI* );
extern AMD64Instr* AMD64Instr_Call       ( AMD64CondCode, Addr64, Int, RetLoc );
extern AMD64Instr* AMD64Instr_XDirect    ( Addr64 dstGA, AMD64AMode* amRIP,
                                           AMD64CondCode cond, Bool toFastEP );
extern AMD64Instr* AMD64Instr_XIndir     ( HReg dstGA, AMD64AMode* amRIP,
                                           AMD64CondCode cond );
extern AMD64Instr* AMD64Instr_XAssisted  ( HReg dstGA, AMD64AMode* amRIP,
                                           AMD64CondCode cond, IRJumpKind jk );
extern AMD64Instr* AMD64Instr_CMov64     ( AMD64CondCode, HReg src, HReg dst );
extern AMD64Instr* AMD64Instr_CLoad      ( AMD64CondCode cond, UChar szB,
                                           AMD64AMode* addr, HReg dst );
extern AMD64Instr* AMD64Instr_CStore     ( AMD64CondCode cond, UChar szB,
                                           HReg src, AMD64AMode* addr );
extern AMD64Instr* AMD64Instr_MovxLQ     ( Bool syned, HReg src, HReg dst );
extern AMD64Instr* AMD64Instr_LoadEX     ( UChar szSmall, Bool syned,
                                           AMD64AMode* src, HReg dst );
extern AMD64Instr* AMD64Instr_Store      ( UChar sz, HReg src, AMD64AMode* dst );
extern AMD64Instr* AMD64Instr_Set64      ( AMD64CondCode cond, HReg dst );
extern AMD64Instr* AMD64Instr_Bsfr64     ( Bool isFwds, HReg src, HReg dst );
extern AMD64Instr* AMD64Instr_MFence     ( void );
extern AMD64Instr* AMD64Instr_ACAS       ( AMD64AMode* addr, UChar sz );
extern AMD64Instr* AMD64Instr_DACAS      ( AMD64AMode* addr, UChar sz );

extern AMD64Instr* AMD64Instr_A87Free    ( Int nregs );
extern AMD64Instr* AMD64Instr_A87PushPop ( AMD64AMode* addr, Bool isPush, UChar szB );
extern AMD64Instr* AMD64Instr_A87FpOp    ( A87FpOp op );
extern AMD64Instr* AMD64Instr_A87LdCW    ( AMD64AMode* addr );
extern AMD64Instr* AMD64Instr_A87StSW    ( AMD64AMode* addr );
extern AMD64Instr* AMD64Instr_LdMXCSR    ( AMD64AMode* );
extern AMD64Instr* AMD64Instr_SseUComIS  ( Int sz, HReg srcL, HReg srcR, HReg dst );
extern AMD64Instr* AMD64Instr_SseSI2SF   ( Int szS, Int szD, HReg src, HReg dst );
extern AMD64Instr* AMD64Instr_SseSF2SI   ( Int szS, Int szD, HReg src, HReg dst );
extern AMD64Instr* AMD64Instr_SseSDSS    ( Bool from64, HReg src, HReg dst );
extern AMD64Instr* AMD64Instr_SseLdSt    ( Bool isLoad, Int sz, HReg, AMD64AMode* );
extern AMD64Instr* AMD64Instr_SseLdzLO   ( Int sz, HReg, AMD64AMode* );
extern AMD64Instr* AMD64Instr_Sse32Fx4   ( AMD64SseOp, HReg, HReg );
extern AMD64Instr* AMD64Instr_Sse32FLo   ( AMD64SseOp, HReg, HReg );
extern AMD64Instr* AMD64Instr_Sse64Fx2   ( AMD64SseOp, HReg, HReg );
extern AMD64Instr* AMD64Instr_Sse64FLo   ( AMD64SseOp, HReg, HReg );
extern AMD64Instr* AMD64Instr_SseReRg    ( AMD64SseOp, HReg, HReg );
extern AMD64Instr* AMD64Instr_SseCMov    ( AMD64CondCode, HReg src, HReg dst );
extern AMD64Instr* AMD64Instr_SseShuf    ( Int order, HReg src, HReg dst );
extern AMD64Instr* AMD64Instr_EvCheck    ( AMD64AMode* amCounter,
                                           AMD64AMode* amFailAddr );
extern AMD64Instr* AMD64Instr_ProfInc    ( void );


extern void ppAMD64Instr ( const AMD64Instr*, Bool );

extern void getRegUsage_AMD64Instr ( HRegUsage*, const AMD64Instr*, Bool );
extern void mapRegs_AMD64Instr     ( HRegRemap*, AMD64Instr*, Bool );
extern Bool isMove_AMD64Instr      ( const AMD64Instr*, HReg*, HReg* );
extern Int          emit_AMD64Instr   ( Bool* is_profInc,
                                        UChar* buf, Int nbuf,
                                        const AMD64Instr* i, 
                                        Bool mode64,
                                        VexEndness endness_host,
                                        const void* disp_cp_chain_me_to_slowEP,
                                        const void* disp_cp_chain_me_to_fastEP,
                                        const void* disp_cp_xindir,
                                        const void* disp_cp_xassisted );

extern void genSpill_AMD64  ( HInstr** i1, HInstr** i2,
                              HReg rreg, Int offset, Bool );
extern void genReload_AMD64 ( HInstr** i1, HInstr** i2,
                              HReg rreg, Int offset, Bool );

extern const RRegUniverse* getRRegUniverse_AMD64 ( void );

extern HInstrArray* iselSB_AMD64           ( const IRSB*, 
                                             VexArch,
                                             const VexArchInfo*,
                                             const VexAbiInfo*,
                                             Int offs_Host_EvC_Counter,
                                             Int offs_Host_EvC_FailAddr,
                                             Bool chainingAllowed,
                                             Bool addProfInc,
                                             Addr max_ga );

extern Int evCheckSzB_AMD64 (void);

extern VexInvalRange chainXDirect_AMD64 ( VexEndness endness_host,
                                          void* place_to_chain,
                                          const void* disp_cp_chain_me_EXPECTED,
                                          const void* place_to_jump_to );

extern VexInvalRange unchainXDirect_AMD64 ( VexEndness endness_host,
                                            void* place_to_unchain,
                                            const void* place_to_jump_to_EXPECTED,
                                            const void* disp_cp_chain_me );

extern VexInvalRange patchProfInc_AMD64 ( VexEndness endness_host,
                                          void*  place_to_patch,
                                          const ULong* location_of_counter );


#endif 

