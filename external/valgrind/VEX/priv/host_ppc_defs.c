

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
#include "libvex.h"
#include "libvex_trc_values.h"

#include "main_util.h"
#include "host_generic_regs.h"
#include "host_ppc_defs.h"



const RRegUniverse* getRRegUniverse_PPC ( Bool mode64 )
{
   static RRegUniverse rRegUniverse_PPC;
   static UInt         rRegUniverse_PPC_initted = 0;

   
   RRegUniverse* ru = &rRegUniverse_PPC;

   
   UInt howNeeded = mode64 ? 2 : 1;
   if (LIKELY(rRegUniverse_PPC_initted == howNeeded))
      return ru;

   RRegUniverse__init(ru);

   
   
   
   ru->regs[ru->size++] = hregPPC_GPR3(mode64);
   ru->regs[ru->size++] = hregPPC_GPR4(mode64);
   ru->regs[ru->size++] = hregPPC_GPR5(mode64);
   ru->regs[ru->size++] = hregPPC_GPR6(mode64);
   ru->regs[ru->size++] = hregPPC_GPR7(mode64);
   ru->regs[ru->size++] = hregPPC_GPR8(mode64);
   ru->regs[ru->size++] = hregPPC_GPR9(mode64);
   ru->regs[ru->size++] = hregPPC_GPR10(mode64);
   if (!mode64) {
      ru->regs[ru->size++] = hregPPC_GPR11(mode64);
      ru->regs[ru->size++] = hregPPC_GPR12(mode64);
   }
   
   
   ru->regs[ru->size++] = hregPPC_GPR14(mode64);
   ru->regs[ru->size++] = hregPPC_GPR15(mode64);
   ru->regs[ru->size++] = hregPPC_GPR16(mode64);
   ru->regs[ru->size++] = hregPPC_GPR17(mode64);
   ru->regs[ru->size++] = hregPPC_GPR18(mode64);
   ru->regs[ru->size++] = hregPPC_GPR19(mode64);
   ru->regs[ru->size++] = hregPPC_GPR20(mode64);
   ru->regs[ru->size++] = hregPPC_GPR21(mode64);
   ru->regs[ru->size++] = hregPPC_GPR22(mode64);
   ru->regs[ru->size++] = hregPPC_GPR23(mode64);
   ru->regs[ru->size++] = hregPPC_GPR24(mode64);
   ru->regs[ru->size++] = hregPPC_GPR25(mode64);
   ru->regs[ru->size++] = hregPPC_GPR26(mode64);
   ru->regs[ru->size++] = hregPPC_GPR27(mode64);
   ru->regs[ru->size++] = hregPPC_GPR28(mode64);
   
   
   

   ru->regs[ru->size++] = hregPPC_FPR14(mode64);
   ru->regs[ru->size++] = hregPPC_FPR15(mode64);
   ru->regs[ru->size++] = hregPPC_FPR16(mode64);
   ru->regs[ru->size++] = hregPPC_FPR17(mode64);
   ru->regs[ru->size++] = hregPPC_FPR18(mode64);
   ru->regs[ru->size++] = hregPPC_FPR19(mode64);
   ru->regs[ru->size++] = hregPPC_FPR20(mode64);
   ru->regs[ru->size++] = hregPPC_FPR21(mode64);

   
   
   ru->regs[ru->size++] = hregPPC_VR20(mode64);
   ru->regs[ru->size++] = hregPPC_VR21(mode64);
   ru->regs[ru->size++] = hregPPC_VR22(mode64);
   ru->regs[ru->size++] = hregPPC_VR23(mode64);
   ru->regs[ru->size++] = hregPPC_VR24(mode64);
   ru->regs[ru->size++] = hregPPC_VR25(mode64);
   ru->regs[ru->size++] = hregPPC_VR26(mode64);
   ru->regs[ru->size++] = hregPPC_VR27(mode64);
   ru->allocable = ru->size;

   
   ru->regs[ru->size++] = hregPPC_GPR1(mode64);
   ru->regs[ru->size++] = hregPPC_GPR29(mode64);
   ru->regs[ru->size++] = hregPPC_GPR30(mode64);
   ru->regs[ru->size++] = hregPPC_GPR31(mode64);
   ru->regs[ru->size++] = hregPPC_VR29(mode64);

   rRegUniverse_PPC_initted = howNeeded;

   RRegUniverse__check_is_sane(ru);
   return ru;
}


void ppHRegPPC ( HReg reg ) 
{
   Int r;
   static const HChar* ireg32_names[32] 
      = { "%r0",  "%r1",  "%r2",  "%r3",
          "%r4",  "%r5",  "%r6",  "%r7",
          "%r8",  "%r9",  "%r10", "%r11",
          "%r12", "%r13", "%r14", "%r15",
          "%r16", "%r17", "%r18", "%r19",
          "%r20", "%r21", "%r22", "%r23",
          "%r24", "%r25", "%r26", "%r27",
          "%r28", "%r29", "%r30", "%r31" };
   
   if (hregIsVirtual(reg)) {
      ppHReg(reg);
      return;
   }
   
   switch (hregClass(reg)) {
   case HRcInt64:
      r = hregEncoding(reg);
      vassert(r >= 0 && r < 32);
      vex_printf("%s", ireg32_names[r]);
      return;
   case HRcInt32:
      r = hregEncoding(reg);
      vassert(r >= 0 && r < 32);
      vex_printf("%s", ireg32_names[r]);
      return;
   case HRcFlt64:
      r = hregEncoding(reg);
      vassert(r >= 0 && r < 32);
      vex_printf("%%fr%d", r);
      return;
   case HRcVec128:
      r = hregEncoding(reg);
      vassert(r >= 0 && r < 32);
      vex_printf("%%v%d", r);
      return;
   default:
      vpanic("ppHRegPPC");
   }
}



const HChar* showPPCCondCode ( PPCCondCode cond )
{
   if (cond.test == Pct_ALWAYS) return "always";

   switch (cond.flag) {
   case Pcf_7SO:
      return (cond.test == Pct_TRUE) ? "cr7.so=1" : "cr7.so=0";
   case Pcf_7EQ:
      return (cond.test == Pct_TRUE) ? "cr7.eq=1" : "cr7.eq=0";
   case Pcf_7GT:
      return (cond.test == Pct_TRUE) ? "cr7.gt=1" : "cr7.gt=0";
   case Pcf_7LT:
      return (cond.test == Pct_TRUE) ? "cr7.lt=1" : "cr7.lt=0";
   case Pcf_NONE:
      return "no-flag";
   default: vpanic("ppPPCCondCode");
   }
}

PPCCondCode mk_PPCCondCode ( PPCCondTest test, PPCCondFlag flag )
{
   PPCCondCode cc;
   cc.flag = flag;
   cc.test = test;
   if (test == Pct_ALWAYS) { 
      vassert(flag == Pcf_NONE);
   } else {
      vassert(flag != Pcf_NONE);
   }
   return cc;
}

PPCCondTest invertCondTest ( PPCCondTest ct )
{
   vassert(ct != Pct_ALWAYS);
   return (ct == Pct_TRUE) ? Pct_FALSE : Pct_TRUE;
}



PPCAMode* PPCAMode_IR ( Int idx, HReg base ) {
   PPCAMode* am = LibVEX_Alloc_inline(sizeof(PPCAMode));
   vassert(idx >= -0x8000 && idx < 0x8000);
   am->tag = Pam_IR;
   am->Pam.IR.base = base;
   am->Pam.IR.index = idx;
   return am;
}
PPCAMode* PPCAMode_RR ( HReg idx, HReg base ) {
   PPCAMode* am = LibVEX_Alloc_inline(sizeof(PPCAMode));
   am->tag = Pam_RR;
   am->Pam.RR.base = base;
   am->Pam.RR.index = idx;
   return am;
}

PPCAMode* dopyPPCAMode ( PPCAMode* am ) {
   switch (am->tag) {
   case Pam_IR: 
      return PPCAMode_IR( am->Pam.IR.index, am->Pam.IR.base );
   case Pam_RR: 
      return PPCAMode_RR( am->Pam.RR.index, am->Pam.RR.base );
   default:
      vpanic("dopyPPCAMode");
   }
}

void ppPPCAMode ( PPCAMode* am ) {
   switch (am->tag) {
   case Pam_IR: 
      if (am->Pam.IR.index == 0)
         vex_printf("0(");
      else
         vex_printf("%d(", (Int)am->Pam.IR.index);
      ppHRegPPC(am->Pam.IR.base);
      vex_printf(")");
      return;
   case Pam_RR:
      ppHRegPPC(am->Pam.RR.base);
      vex_printf(",");
      ppHRegPPC(am->Pam.RR.index);
      return;
   default:
      vpanic("ppPPCAMode");
   }
}

static void addRegUsage_PPCAMode ( HRegUsage* u, PPCAMode* am ) {
   switch (am->tag) {
   case Pam_IR: 
      addHRegUse(u, HRmRead, am->Pam.IR.base);
      return;
   case Pam_RR:
      addHRegUse(u, HRmRead, am->Pam.RR.base);
      addHRegUse(u, HRmRead, am->Pam.RR.index);
      return;
   default:
      vpanic("addRegUsage_PPCAMode");
   }
}

static void mapRegs_PPCAMode ( HRegRemap* m, PPCAMode* am ) {
   switch (am->tag) {
   case Pam_IR: 
      am->Pam.IR.base = lookupHRegRemap(m, am->Pam.IR.base);
      return;
   case Pam_RR:
      am->Pam.RR.base = lookupHRegRemap(m, am->Pam.RR.base);
      am->Pam.RR.index = lookupHRegRemap(m, am->Pam.RR.index);
      return;
   default:
      vpanic("mapRegs_PPCAMode");
   }
}


PPCRH* PPCRH_Imm ( Bool syned, UShort imm16 ) {
   PPCRH* op         = LibVEX_Alloc_inline(sizeof(PPCRH));
   op->tag           = Prh_Imm;
   op->Prh.Imm.syned = syned;
   op->Prh.Imm.imm16 = imm16;
   if (syned)
      vassert(imm16 != 0x8000);
   vassert(syned == True || syned == False);
   return op;
}
PPCRH* PPCRH_Reg ( HReg reg ) {
   PPCRH* op       = LibVEX_Alloc_inline(sizeof(PPCRH));
   op->tag         = Prh_Reg;
   op->Prh.Reg.reg = reg;
   return op;
}

void ppPPCRH ( PPCRH* op ) {
   switch (op->tag) {
   case Prh_Imm: 
      if (op->Prh.Imm.syned)
         vex_printf("%d", (Int)(Short)op->Prh.Imm.imm16);
      else
         vex_printf("%u", (UInt)(UShort)op->Prh.Imm.imm16);
      return;
   case Prh_Reg: 
      ppHRegPPC(op->Prh.Reg.reg);
      return;
   default: 
      vpanic("ppPPCRH");
   }
}

static void addRegUsage_PPCRH ( HRegUsage* u, PPCRH* op ) {
   switch (op->tag) {
   case Prh_Imm: 
      return;
   case Prh_Reg: 
      addHRegUse(u, HRmRead, op->Prh.Reg.reg);
      return;
   default: 
      vpanic("addRegUsage_PPCRH");
   }
}

static void mapRegs_PPCRH ( HRegRemap* m, PPCRH* op ) {
   switch (op->tag) {
   case Prh_Imm: 
      return;
   case Prh_Reg: 
      op->Prh.Reg.reg = lookupHRegRemap(m, op->Prh.Reg.reg);
      return;
   default: 
      vpanic("mapRegs_PPCRH");
   }
}



PPCRI* PPCRI_Imm ( ULong imm64 ) {
   PPCRI* op   = LibVEX_Alloc_inline(sizeof(PPCRI));
   op->tag     = Pri_Imm;
   op->Pri.Imm = imm64;
   return op;
}
PPCRI* PPCRI_Reg ( HReg reg ) {
   PPCRI* op   = LibVEX_Alloc_inline(sizeof(PPCRI));
   op->tag     = Pri_Reg;
   op->Pri.Reg = reg;
   return op;
}

void ppPPCRI ( PPCRI* dst ) {
   switch (dst->tag) {
      case Pri_Imm: 
         vex_printf("0x%llx", dst->Pri.Imm);
         break;
      case Pri_Reg: 
         ppHRegPPC(dst->Pri.Reg);
         break;
      default: 
         vpanic("ppPPCRI");
   }
}

static void addRegUsage_PPCRI ( HRegUsage* u, PPCRI* dst ) {
   switch (dst->tag) {
      case Pri_Imm: 
         return;
      case Pri_Reg: 
         addHRegUse(u, HRmRead, dst->Pri.Reg);
         return;
      default: 
         vpanic("addRegUsage_PPCRI");
   }
}

static void mapRegs_PPCRI ( HRegRemap* m, PPCRI* dst ) {
   switch (dst->tag) {
      case Pri_Imm: 
         return;
      case Pri_Reg: 
         dst->Pri.Reg = lookupHRegRemap(m, dst->Pri.Reg);
         return;
      default: 
         vpanic("mapRegs_PPCRI");
   }
}



PPCVI5s* PPCVI5s_Imm ( Char simm5 ) {
   PPCVI5s* op   = LibVEX_Alloc_inline(sizeof(PPCVI5s));
   op->tag       = Pvi_Imm;
   op->Pvi.Imm5s = simm5;
   vassert(simm5 >= -16 && simm5 <= 15);
   return op;
}
PPCVI5s* PPCVI5s_Reg ( HReg reg ) {
   PPCVI5s* op = LibVEX_Alloc_inline(sizeof(PPCVI5s));
   op->tag     = Pvi_Reg;
   op->Pvi.Reg = reg;
   vassert(hregClass(reg) == HRcVec128);
   return op;
}

void ppPPCVI5s ( PPCVI5s* src ) {
   switch (src->tag) {
      case Pvi_Imm: 
         vex_printf("%d", (Int)src->Pvi.Imm5s);
         break;
      case Pvi_Reg: 
         ppHRegPPC(src->Pvi.Reg);
         break;
      default: 
         vpanic("ppPPCVI5s");
   }
}

static void addRegUsage_PPCVI5s ( HRegUsage* u, PPCVI5s* dst ) {
   switch (dst->tag) {
      case Pvi_Imm: 
         return;
      case Pvi_Reg: 
         addHRegUse(u, HRmRead, dst->Pvi.Reg);
         return;
      default: 
         vpanic("addRegUsage_PPCVI5s");
   }
}

static void mapRegs_PPCVI5s ( HRegRemap* m, PPCVI5s* dst ) {
   switch (dst->tag) {
      case Pvi_Imm: 
         return;
      case Pvi_Reg: 
         dst->Pvi.Reg = lookupHRegRemap(m, dst->Pvi.Reg);
         return;
      default: 
         vpanic("mapRegs_PPCVI5s");
   }
}



const HChar* showPPCUnaryOp ( PPCUnaryOp op ) {
   switch (op) {
   case Pun_NOT:   return "not";
   case Pun_NEG:   return "neg";
   case Pun_CLZ32: return "cntlzw";
   case Pun_CLZ64: return "cntlzd";
   case Pun_EXTSW: return "extsw";
   default: vpanic("showPPCUnaryOp");
   }
}

const HChar* showPPCAluOp ( PPCAluOp op, Bool immR ) {
   switch (op) {
      case Palu_ADD: return immR ? "addi"  : "add";
      case Palu_SUB: return immR ? "subi"  : "sub";
      case Palu_AND: return immR ? "andi." : "and";
      case Palu_OR:  return immR ? "ori"   : "or";
      case Palu_XOR: return immR ? "xori"  : "xor";
      default: vpanic("showPPCAluOp");
   }
}

const HChar* showPPCShftOp ( PPCShftOp op, Bool immR, Bool sz32 ) {
   switch (op) {
      case Pshft_SHL: return sz32 ? (immR ? "slwi"  : "slw") : 
                                    (immR ? "sldi"  : "sld");
      case Pshft_SHR: return sz32 ? (immR ? "srwi"  : "srw") :
                                    (immR ? "srdi"  : "srd");
      case Pshft_SAR: return sz32 ? (immR ? "srawi" : "sraw") :
                                    (immR ? "sradi" : "srad");
      default: vpanic("showPPCShftOp");
   }
}

const HChar* showPPCFpOp ( PPCFpOp op ) {
   switch (op) {
      case Pfp_ADDD:   return "fadd";
      case Pfp_SUBD:   return "fsub";
      case Pfp_MULD:   return "fmul";
      case Pfp_DIVD:   return "fdiv";
      case Pfp_MADDD:  return "fmadd";
      case Pfp_MSUBD:  return "fmsub";
      case Pfp_MADDS:  return "fmadds";
      case Pfp_MSUBS:  return "fmsubs";
      case Pfp_ADDS:   return "fadds";
      case Pfp_SUBS:   return "fsubs";
      case Pfp_MULS:   return "fmuls";
      case Pfp_DIVS:   return "fdivs";
      case Pfp_SQRT:   return "fsqrt";
      case Pfp_ABS:    return "fabs";
      case Pfp_NEG:    return "fneg";
      case Pfp_MOV:    return "fmr";
      case Pfp_RES:    return "fres";
      case Pfp_RSQRTE: return "frsqrte";
      case Pfp_FRIM:   return "frim";
      case Pfp_FRIN:   return "frin";
      case Pfp_FRIP:   return "frip";
      case Pfp_FRIZ:   return "friz";
      case Pfp_DFPADD:     return "dadd";
      case Pfp_DFPADDQ:    return "daddq";
      case Pfp_DFPSUB:     return "dsub";
      case Pfp_DFPSUBQ:    return "dsubq";
      case Pfp_DFPMUL:     return "dmul";
      case Pfp_DFPMULQ:    return "dmulq";
      case Pfp_DFPDIV:     return "ddivd";
      case Pfp_DFPDIVQ:    return "ddivq";
      case Pfp_DCTDP:      return "dctdp";
      case Pfp_DRSP:       return "drsp";
      case Pfp_DCTFIX:     return "dctfix";
      case Pfp_DCFFIX:     return "dcffix";
      case Pfp_DCTQPQ:     return "dctqpq";
      case Pfp_DCFFIXQ:    return "dcffixq";
      case Pfp_DQUA:       return "dqua";
      case Pfp_DQUAQ:      return "dquaq";
      case Pfp_DXEX:       return "dxex";
      case Pfp_DXEXQ:      return "dxexq";
      case Pfp_DIEX:       return "diex";
      case Pfp_DIEXQ:      return "diexq";
      case Pfp_RRDTR:      return "rrdtr";
      default: vpanic("showPPCFpOp");
   }
}

const HChar* showPPCAvOp ( PPCAvOp op ) {
   switch (op) {

   
   case Pav_MOV:       return "vmr";      
     
   case Pav_AND:       return "vand";     
   case Pav_OR:        return "vor";
   case Pav_XOR:       return "vxor";
   case Pav_NOT:       return "vnot";

   case Pav_UNPCKH8S:  return "vupkhsb";  
   case Pav_UNPCKH16S: return "vupkhsh";
   case Pav_UNPCKL8S:  return "vupklsb";
   case Pav_UNPCKL16S: return "vupklsh";
   case Pav_UNPCKHPIX: return "vupkhpx";
   case Pav_UNPCKLPIX: return "vupklpx";

   
   case Pav_ADDU:      return "vaddu_m";  
   case Pav_QADDU:     return "vaddu_s";  
   case Pav_QADDS:     return "vadds_s";  
     
   case Pav_SUBU:      return "vsubu_m";  
   case Pav_QSUBU:     return "vsubu_s";  
   case Pav_QSUBS:     return "vsubs_s";  
     
   case Pav_MULU:      return "vmulu";    
   case Pav_OMULU:     return "vmulou";   
   case Pav_OMULS:     return "vmulos";   
   case Pav_EMULU:     return "vmuleu";   
   case Pav_EMULS:     return "vmules";   
  
   case Pav_AVGU:      return "vavgu";    
   case Pav_AVGS:      return "vavgs";    
     
   case Pav_MAXU:      return "vmaxu";    
   case Pav_MAXS:      return "vmaxs";    
     
   case Pav_MINU:      return "vminu";    
   case Pav_MINS:      return "vmins";    
     
   
   case Pav_CMPEQU:    return "vcmpequ";  
   case Pav_CMPGTU:    return "vcmpgtu";  
   case Pav_CMPGTS:    return "vcmpgts";  

   
   case Pav_SHL:       return "vsl";      
   case Pav_SHR:       return "vsr";      
   case Pav_SAR:       return "vsra";     
   case Pav_ROTL:      return "vrl";      

   
   case Pav_PACKUU:    return "vpku_um";  
   case Pav_QPACKUU:   return "vpku_us";  
   case Pav_QPACKSU:   return "vpks_us";  
   case Pav_QPACKSS:   return "vpks_ss";  
   case Pav_PACKPXL:   return "vpkpx";

   
   case Pav_MRGHI:     return "vmrgh";    
   case Pav_MRGLO:     return "vmrgl";    

   
   case Pav_CATODD:     return "vmrgow";    
   case Pav_CATEVEN:    return "vmrgew";    

   
   case Pav_SHA256:     return "vshasigmaw"; 
   case Pav_SHA512:     return "vshasigmaw"; 

   
   case Pav_BCDAdd:     return "bcdadd.";  
   case Pav_BCDSub:     return "bcdsub.";  

   
   case Pav_POLYMULADD: return "vpmsum";   

   
   case Pav_CIPHERV128:  case Pav_CIPHERLV128:
   case Pav_NCIPHERV128: case Pav_NCIPHERLV128:
   case Pav_CIPHERSUBV128: return "v_cipher_";  

   
   case Pav_ZEROCNTBYTE: case Pav_ZEROCNTWORD:
   case Pav_ZEROCNTHALF: case Pav_ZEROCNTDBL:
      return "vclz_";                           

   
   case Pav_BITMTXXPOSE:
      return "vgbbd";

   default: vpanic("showPPCAvOp");
   }
}

const HChar* showPPCAvFpOp ( PPCAvFpOp op ) {
   switch (op) {
   
   case Pavfp_ADDF:      return "vaddfp";
   case Pavfp_SUBF:      return "vsubfp";
   case Pavfp_MULF:      return "vmaddfp";
   case Pavfp_MAXF:      return "vmaxfp";
   case Pavfp_MINF:      return "vminfp";
   case Pavfp_CMPEQF:    return "vcmpeqfp";
   case Pavfp_CMPGTF:    return "vcmpgtfp";
   case Pavfp_CMPGEF:    return "vcmpgefp";
     
   
   case Pavfp_RCPF:      return "vrefp";
   case Pavfp_RSQRTF:    return "vrsqrtefp";
   case Pavfp_CVTU2F:    return "vcfux";
   case Pavfp_CVTS2F:    return "vcfsx";
   case Pavfp_QCVTF2U:   return "vctuxs";
   case Pavfp_QCVTF2S:   return "vctsxs";
   case Pavfp_ROUNDM:    return "vrfim";
   case Pavfp_ROUNDP:    return "vrfip";
   case Pavfp_ROUNDN:    return "vrfin";
   case Pavfp_ROUNDZ:    return "vrfiz";

   default: vpanic("showPPCAvFpOp");
   }
}

PPCInstr* PPCInstr_LI ( HReg dst, ULong imm64, Bool mode64 )
{
   PPCInstr* i     = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag          = Pin_LI;
   i->Pin.LI.dst   = dst;
   i->Pin.LI.imm64 = imm64;
   if (!mode64)
      vassert( (Long)imm64 == (Long)(Int)(UInt)imm64 );
   return i;
}
PPCInstr* PPCInstr_Alu ( PPCAluOp op, HReg dst, 
                         HReg srcL, PPCRH* srcR ) {
   PPCInstr* i     = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag          = Pin_Alu;
   i->Pin.Alu.op   = op;
   i->Pin.Alu.dst  = dst;
   i->Pin.Alu.srcL = srcL;
   i->Pin.Alu.srcR = srcR;
   return i;
}
PPCInstr* PPCInstr_Shft ( PPCShftOp op, Bool sz32, 
                          HReg dst, HReg srcL, PPCRH* srcR ) {
   PPCInstr* i      = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag           = Pin_Shft;
   i->Pin.Shft.op   = op;
   i->Pin.Shft.sz32 = sz32;
   i->Pin.Shft.dst  = dst;
   i->Pin.Shft.srcL = srcL;
   i->Pin.Shft.srcR = srcR;
   return i;
}
PPCInstr* PPCInstr_AddSubC ( Bool isAdd, Bool setC,
                             HReg dst, HReg srcL, HReg srcR ) {
   PPCInstr* i          = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag               = Pin_AddSubC;
   i->Pin.AddSubC.isAdd = isAdd;
   i->Pin.AddSubC.setC  = setC;
   i->Pin.AddSubC.dst   = dst;
   i->Pin.AddSubC.srcL  = srcL;
   i->Pin.AddSubC.srcR  = srcR;
   return i;
}
PPCInstr* PPCInstr_Cmp ( Bool syned, Bool sz32, 
                         UInt crfD, HReg srcL, PPCRH* srcR ) {
   PPCInstr* i      = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag           = Pin_Cmp;
   i->Pin.Cmp.syned = syned;
   i->Pin.Cmp.sz32  = sz32;
   i->Pin.Cmp.crfD  = crfD;
   i->Pin.Cmp.srcL  = srcL;
   i->Pin.Cmp.srcR  = srcR;
   return i;
}
PPCInstr* PPCInstr_Unary ( PPCUnaryOp op, HReg dst, HReg src ) {
   PPCInstr* i      = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag           = Pin_Unary;
   i->Pin.Unary.op  = op;
   i->Pin.Unary.dst = dst;
   i->Pin.Unary.src = src;
   return i;
}
PPCInstr* PPCInstr_MulL ( Bool syned, Bool hi, Bool sz32, 
                          HReg dst, HReg srcL, HReg srcR ) {
   PPCInstr* i       = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag            = Pin_MulL;
   i->Pin.MulL.syned = syned;
   i->Pin.MulL.hi    = hi;
   i->Pin.MulL.sz32  = sz32;
   i->Pin.MulL.dst   = dst;
   i->Pin.MulL.srcL  = srcL;
   i->Pin.MulL.srcR  = srcR;
   if (!hi) vassert(!syned);
   return i;
}
PPCInstr* PPCInstr_Div ( Bool extended, Bool syned, Bool sz32,
                         HReg dst, HReg srcL, HReg srcR ) {
   PPCInstr* i      = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag           = Pin_Div;
   i->Pin.Div.extended = extended;
   i->Pin.Div.syned = syned;
   i->Pin.Div.sz32  = sz32;
   i->Pin.Div.dst   = dst;
   i->Pin.Div.srcL  = srcL;
   i->Pin.Div.srcR  = srcR;
   return i;
}
PPCInstr* PPCInstr_Call ( PPCCondCode cond, 
                          Addr64 target, UInt argiregs, RetLoc rloc ) {
   UInt mask;
   PPCInstr* i          = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag               = Pin_Call;
   i->Pin.Call.cond     = cond;
   i->Pin.Call.target   = target;
   i->Pin.Call.argiregs = argiregs;
   i->Pin.Call.rloc     = rloc;
   
   mask = (1<<3)|(1<<4)|(1<<5)|(1<<6)|(1<<7)|(1<<8)|(1<<9)|(1<<10);
   vassert(0 == (argiregs & ~mask));
   vassert(is_sane_RetLoc(rloc));
   return i;
}
PPCInstr* PPCInstr_XDirect ( Addr64 dstGA, PPCAMode* amCIA,
                             PPCCondCode cond, Bool toFastEP ) {
   PPCInstr* i             = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag                  = Pin_XDirect;
   i->Pin.XDirect.dstGA    = dstGA;
   i->Pin.XDirect.amCIA    = amCIA;
   i->Pin.XDirect.cond     = cond;
   i->Pin.XDirect.toFastEP = toFastEP;
   return i;
}
PPCInstr* PPCInstr_XIndir ( HReg dstGA, PPCAMode* amCIA,
                            PPCCondCode cond ) {
   PPCInstr* i         = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag              = Pin_XIndir;
   i->Pin.XIndir.dstGA = dstGA;
   i->Pin.XIndir.amCIA = amCIA;
   i->Pin.XIndir.cond  = cond;
   return i;
}
PPCInstr* PPCInstr_XAssisted ( HReg dstGA, PPCAMode* amCIA,
                               PPCCondCode cond, IRJumpKind jk ) {
   PPCInstr* i            = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag                 = Pin_XAssisted;
   i->Pin.XAssisted.dstGA = dstGA;
   i->Pin.XAssisted.amCIA = amCIA;
   i->Pin.XAssisted.cond  = cond;
   i->Pin.XAssisted.jk    = jk;
   return i;
}
PPCInstr* PPCInstr_CMov  ( PPCCondCode cond, 
                           HReg dst, PPCRI* src ) {
   PPCInstr* i      = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag           = Pin_CMov;
   i->Pin.CMov.cond = cond;
   i->Pin.CMov.src  = src;
   i->Pin.CMov.dst  = dst;
   vassert(cond.test != Pct_ALWAYS);
   return i;
}
PPCInstr* PPCInstr_Load ( UChar sz,
                          HReg dst, PPCAMode* src, Bool mode64 ) {
   PPCInstr* i       = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag            = Pin_Load;
   i->Pin.Load.sz    = sz;
   i->Pin.Load.src   = src;
   i->Pin.Load.dst   = dst;
   vassert(sz == 1 || sz == 2 || sz == 4 || sz == 8);
   if (sz == 8) vassert(mode64);
   return i;
}
PPCInstr* PPCInstr_LoadL ( UChar sz,
                           HReg dst, HReg src, Bool mode64 )
{
   PPCInstr* i       = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag            = Pin_LoadL;
   i->Pin.LoadL.sz   = sz;
   i->Pin.LoadL.src  = src;
   i->Pin.LoadL.dst  = dst;
   vassert(sz == 1 || sz == 2 || sz == 4 || sz == 8);
   if (sz == 8) vassert(mode64);
   return i;
}
PPCInstr* PPCInstr_Store ( UChar sz, PPCAMode* dst, HReg src,
                           Bool mode64 ) {
   PPCInstr* i      = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag           = Pin_Store;
   i->Pin.Store.sz  = sz;
   i->Pin.Store.src = src;
   i->Pin.Store.dst = dst;
   vassert(sz == 1 || sz == 2 || sz == 4 || sz == 8);
   if (sz == 8) vassert(mode64);
   return i;
}
PPCInstr* PPCInstr_StoreC ( UChar sz, HReg dst, HReg src, Bool mode64 ) {
   PPCInstr* i       = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag            = Pin_StoreC;
   i->Pin.StoreC.sz  = sz;
   i->Pin.StoreC.src = src;
   i->Pin.StoreC.dst = dst;
   vassert(sz == 1 || sz == 2 || sz == 4 || sz == 8);
   if (sz == 8) vassert(mode64);
   return i;
}
PPCInstr* PPCInstr_Set ( PPCCondCode cond, HReg dst ) {
   PPCInstr* i     = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag          = Pin_Set;
   i->Pin.Set.cond = cond;
   i->Pin.Set.dst  = dst;
   return i;
}
PPCInstr* PPCInstr_MfCR ( HReg dst )
{
   PPCInstr* i     = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag          = Pin_MfCR;
   i->Pin.MfCR.dst = dst;
   return i;
}
PPCInstr* PPCInstr_MFence ( void )
{
   PPCInstr* i = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag      = Pin_MFence;
   return i;
}

PPCInstr* PPCInstr_FpUnary ( PPCFpOp op, HReg dst, HReg src ) {
   PPCInstr* i        = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag             = Pin_FpUnary;
   i->Pin.FpUnary.op  = op;
   i->Pin.FpUnary.dst = dst;
   i->Pin.FpUnary.src = src;
   return i;
}
PPCInstr* PPCInstr_FpBinary ( PPCFpOp op, HReg dst,
                              HReg srcL, HReg srcR ) {
   PPCInstr* i          = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag               = Pin_FpBinary;
   i->Pin.FpBinary.op   = op;
   i->Pin.FpBinary.dst  = dst;
   i->Pin.FpBinary.srcL = srcL;
   i->Pin.FpBinary.srcR = srcR;
   return i;
}
PPCInstr* PPCInstr_FpMulAcc ( PPCFpOp op, HReg dst, HReg srcML, 
                                          HReg srcMR, HReg srcAcc )
{
   PPCInstr* i            = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag                 = Pin_FpMulAcc;
   i->Pin.FpMulAcc.op     = op;
   i->Pin.FpMulAcc.dst    = dst;
   i->Pin.FpMulAcc.srcML  = srcML;
   i->Pin.FpMulAcc.srcMR  = srcMR;
   i->Pin.FpMulAcc.srcAcc = srcAcc;
   return i;
}
PPCInstr* PPCInstr_FpLdSt ( Bool isLoad, UChar sz,
                            HReg reg, PPCAMode* addr ) {
   PPCInstr* i          = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag               = Pin_FpLdSt;
   i->Pin.FpLdSt.isLoad = isLoad;
   i->Pin.FpLdSt.sz     = sz;
   i->Pin.FpLdSt.reg    = reg;
   i->Pin.FpLdSt.addr   = addr;
   vassert(sz == 4 || sz == 8);
   return i;
}
PPCInstr* PPCInstr_FpSTFIW ( HReg addr, HReg data )
{
   PPCInstr* i         = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag              = Pin_FpSTFIW;
   i->Pin.FpSTFIW.addr = addr;
   i->Pin.FpSTFIW.data = data;
   return i;
}
PPCInstr* PPCInstr_FpRSP ( HReg dst, HReg src ) {
   PPCInstr* i      = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag           = Pin_FpRSP;
   i->Pin.FpRSP.dst = dst;
   i->Pin.FpRSP.src = src;
   return i;
}
PPCInstr* PPCInstr_Dfp64Unary(PPCFpOp op, HReg dst, HReg src) {
   PPCInstr* i = LibVEX_Alloc_inline( sizeof(PPCInstr) );
   i->tag = Pin_Dfp64Unary;
   i->Pin.Dfp64Unary.op = op;
   i->Pin.Dfp64Unary.dst = dst;
   i->Pin.Dfp64Unary.src = src;
   return i;
}
PPCInstr* PPCInstr_Dfp64Binary(PPCFpOp op, HReg dst, HReg srcL, HReg srcR) {
   PPCInstr* i = LibVEX_Alloc_inline( sizeof(PPCInstr) );
   i->tag = Pin_Dfp64Binary;
   i->Pin.Dfp64Binary.op = op;
   i->Pin.Dfp64Binary.dst = dst;
   i->Pin.Dfp64Binary.srcL = srcL;
   i->Pin.Dfp64Binary.srcR = srcR;
   return i;
}
PPCInstr* PPCInstr_DfpShift ( PPCFpOp op, HReg dst, HReg src, PPCRI* shift ) {
   PPCInstr* i            = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag                 = Pin_DfpShift;
   i->Pin.DfpShift.op     = op;
   i->Pin.DfpShift.shift  = shift;
   i->Pin.DfpShift.src    = src;
   i->Pin.DfpShift.dst    = dst;
   return i;
}
PPCInstr* PPCInstr_Dfp128Unary(PPCFpOp op, HReg dst_hi, HReg dst_lo,
                                HReg src_hi, HReg src_lo) {
   PPCInstr* i = LibVEX_Alloc_inline( sizeof(PPCInstr) );
   i->tag = Pin_Dfp128Unary;
   i->Pin.Dfp128Unary.op = op;
   i->Pin.Dfp128Unary.dst_hi = dst_hi;
   i->Pin.Dfp128Unary.dst_lo = dst_lo;
   i->Pin.Dfp128Unary.src_hi = src_hi;
   i->Pin.Dfp128Unary.src_lo = src_lo;
   return i;
}
PPCInstr* PPCInstr_Dfp128Binary(PPCFpOp op, HReg dst_hi, HReg dst_lo,
                                HReg srcR_hi, HReg srcR_lo) {
   
   PPCInstr* i = LibVEX_Alloc_inline( sizeof(PPCInstr) );
   i->tag = Pin_Dfp128Binary;
   i->Pin.Dfp128Binary.op = op;
   i->Pin.Dfp128Binary.dst_hi = dst_hi;
   i->Pin.Dfp128Binary.dst_lo = dst_lo;
   i->Pin.Dfp128Binary.srcR_hi = srcR_hi;
   i->Pin.Dfp128Binary.srcR_lo = srcR_lo;
   return i;
}
PPCInstr* PPCInstr_DfpShift128 ( PPCFpOp op, HReg dst_hi, HReg dst_lo, 
                                 HReg src_hi, HReg src_lo,
                                 PPCRI* shift ) {
   PPCInstr* i               = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag                    = Pin_DfpShift128;
   i->Pin.DfpShift128.op     = op;
   i->Pin.DfpShift128.shift  = shift;
   i->Pin.DfpShift128.src_hi = src_hi;
   i->Pin.DfpShift128.src_lo = src_lo;
   i->Pin.DfpShift128.dst_hi = dst_hi;
   i->Pin.DfpShift128.dst_lo = dst_lo;
   return i;
}
PPCInstr* PPCInstr_DfpRound ( HReg dst, HReg src, PPCRI* r_rmc ) {
   PPCInstr* i           = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag                = Pin_DfpRound;
   i->Pin.DfpRound.dst   = dst;
   i->Pin.DfpRound.src   = src;
   i->Pin.DfpRound.r_rmc = r_rmc;
   return i;
}
PPCInstr* PPCInstr_DfpRound128 ( HReg dst_hi, HReg dst_lo, HReg src_hi, 
                                 HReg src_lo, PPCRI* r_rmc ) {
   PPCInstr* i               = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag                    = Pin_DfpRound128;
   i->Pin.DfpRound128.dst_hi = dst_hi;
   i->Pin.DfpRound128.dst_lo = dst_lo;
   i->Pin.DfpRound128.src_hi = src_hi;
   i->Pin.DfpRound128.src_lo = src_lo;
   i->Pin.DfpRound128.r_rmc  = r_rmc;
   return i;
}
PPCInstr* PPCInstr_DfpQuantize ( PPCFpOp op, HReg dst, HReg srcL, HReg srcR,
                                 PPCRI* rmc ) {
   PPCInstr* i             = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag                  = Pin_DfpQuantize;
   i->Pin.DfpQuantize.op   = op;
   i->Pin.DfpQuantize.dst  = dst;
   i->Pin.DfpQuantize.srcL = srcL;
   i->Pin.DfpQuantize.srcR = srcR;
   i->Pin.DfpQuantize.rmc  = rmc;
   return i;
}
PPCInstr* PPCInstr_DfpQuantize128 ( PPCFpOp op, HReg dst_hi, HReg dst_lo,
                                    HReg src_hi, HReg src_lo, PPCRI* rmc ) {
   
   PPCInstr* i                  = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag                       = Pin_DfpQuantize128;
   i->Pin.DfpQuantize128.op     = op;
   i->Pin.DfpQuantize128.dst_hi = dst_hi;
   i->Pin.DfpQuantize128.dst_lo = dst_lo;
   i->Pin.DfpQuantize128.src_hi = src_hi;
   i->Pin.DfpQuantize128.src_lo = src_lo;
   i->Pin.DfpQuantize128.rmc    = rmc;
   return i;
}
PPCInstr* PPCInstr_DfpD128toD64 ( PPCFpOp op, HReg dst,
                                  HReg src_hi, HReg src_lo ) {
   PPCInstr* i                = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag                     = Pin_DfpD128toD64;
   i->Pin.DfpD128toD64.op     = op;
   i->Pin.DfpD128toD64.src_hi = src_hi;
   i->Pin.DfpD128toD64.src_lo = src_lo;
   i->Pin.DfpD128toD64.dst    = dst;
   return i;
}
PPCInstr* PPCInstr_DfpI64StoD128 ( PPCFpOp op, HReg dst_hi,
                                   HReg dst_lo, HReg src ) {
   PPCInstr* i                 = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag                      = Pin_DfpI64StoD128;
   i->Pin.DfpI64StoD128.op     = op;
   i->Pin.DfpI64StoD128.src    = src;
   i->Pin.DfpI64StoD128.dst_hi = dst_hi;
   i->Pin.DfpI64StoD128.dst_lo = dst_lo;
   return i;
}
PPCInstr* PPCInstr_ExtractExpD128 ( PPCFpOp op, HReg dst,
                                    HReg src_hi, HReg src_lo ) {
                                
   PPCInstr* i                  = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag                       = Pin_ExtractExpD128;
   i->Pin.ExtractExpD128.op     = op;
   i->Pin.ExtractExpD128.dst    = dst;
   i->Pin.ExtractExpD128.src_hi = src_hi;
   i->Pin.ExtractExpD128.src_lo = src_lo;
   return i;
}
PPCInstr* PPCInstr_InsertExpD128 ( PPCFpOp op, HReg dst_hi, HReg dst_lo,   
                                   HReg srcL, HReg srcR_hi, HReg srcR_lo ) {
                                
   PPCInstr* i                  = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag                       = Pin_InsertExpD128;
   i->Pin.InsertExpD128.op      = op;
   i->Pin.InsertExpD128.dst_hi  = dst_hi;
   i->Pin.InsertExpD128.dst_lo  = dst_lo;
   i->Pin.InsertExpD128.srcL    = srcL;
   i->Pin.InsertExpD128.srcR_hi = srcR_hi;
   i->Pin.InsertExpD128.srcR_lo = srcR_lo;
   return i;
}
PPCInstr* PPCInstr_Dfp64Cmp ( HReg dst, HReg srcL, HReg srcR ) {
   PPCInstr* i          = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag               = Pin_Dfp64Cmp;
   i->Pin.Dfp64Cmp.dst = dst;
   i->Pin.Dfp64Cmp.srcL = srcL;
   i->Pin.Dfp64Cmp.srcR = srcR;
   return i;                                                   
}
PPCInstr* PPCInstr_Dfp128Cmp ( HReg dst, HReg srcL_hi, HReg srcL_lo,
                               HReg srcR_hi, HReg srcR_lo ) {
   PPCInstr* i               = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag                    = Pin_Dfp128Cmp;
   i->Pin.Dfp128Cmp.dst      = dst;
   i->Pin.Dfp128Cmp.srcL_hi  = srcL_hi;
   i->Pin.Dfp128Cmp.srcL_lo  = srcL_lo;
   i->Pin.Dfp128Cmp.srcR_hi  = srcR_hi;
   i->Pin.Dfp128Cmp.srcR_lo  = srcR_lo;
   return i;                                                   
}
PPCInstr* PPCInstr_EvCheck ( PPCAMode* amCounter,
                             PPCAMode* amFailAddr ) {
   PPCInstr* i               = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag                    = Pin_EvCheck;
   i->Pin.EvCheck.amCounter  = amCounter;
   i->Pin.EvCheck.amFailAddr = amFailAddr;
   return i;
}
PPCInstr* PPCInstr_ProfInc ( void ) {
   PPCInstr* i = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag      = Pin_ProfInc;
   return i;
}

PPCInstr* PPCInstr_FpCftI ( Bool fromI, Bool int32, Bool syned,
                            Bool flt64, HReg dst, HReg src ) {
   Bool tmp = fromI | int32 | syned | flt64;
   vassert(tmp == True || tmp == False); 
   UShort conversion = 0;
   conversion = (fromI << 3) | (int32 << 2) | (syned << 1) | flt64;
   switch (conversion) {
      
      case 1: case 3: case 5: case 7:
      case 8: case 9: case 11:
         break;
      default:
         vpanic("PPCInstr_FpCftI(ppc_host)");
   }
   PPCInstr* i         = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag              = Pin_FpCftI;
   i->Pin.FpCftI.fromI = fromI;
   i->Pin.FpCftI.int32 = int32;
   i->Pin.FpCftI.syned = syned;
   i->Pin.FpCftI.flt64 = flt64;
   i->Pin.FpCftI.dst   = dst;
   i->Pin.FpCftI.src   = src;
   return i;
}
PPCInstr* PPCInstr_FpCMov ( PPCCondCode cond, HReg dst, HReg src ) {
   PPCInstr* i        = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag             = Pin_FpCMov;
   i->Pin.FpCMov.cond = cond;
   i->Pin.FpCMov.dst  = dst;
   i->Pin.FpCMov.src  = src;
   vassert(cond.test != Pct_ALWAYS);
   return i;
}
PPCInstr* PPCInstr_FpLdFPSCR ( HReg src, Bool dfp_rm ) {
   PPCInstr* i          = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag               = Pin_FpLdFPSCR;
   i->Pin.FpLdFPSCR.src = src;
   i->Pin.FpLdFPSCR.dfp_rm = dfp_rm ? 1 : 0;
   return i;
}
PPCInstr* PPCInstr_FpCmp ( HReg dst, HReg srcL, HReg srcR ) {
   PPCInstr* i       = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag            = Pin_FpCmp;
   i->Pin.FpCmp.dst  = dst;
   i->Pin.FpCmp.srcL = srcL;
   i->Pin.FpCmp.srcR = srcR;
   return i;
}

PPCInstr* PPCInstr_RdWrLR ( Bool wrLR, HReg gpr ) {
   PPCInstr* i        = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag             = Pin_RdWrLR;
   i->Pin.RdWrLR.wrLR = wrLR;
   i->Pin.RdWrLR.gpr  = gpr;
   return i;
}

PPCInstr* PPCInstr_AvLdSt ( Bool isLoad, UChar sz,
                            HReg reg, PPCAMode* addr ) {
   PPCInstr* i          = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag               = Pin_AvLdSt;
   i->Pin.AvLdSt.isLoad = isLoad;
   i->Pin.AvLdSt.sz     = sz;
   i->Pin.AvLdSt.reg    = reg;
   i->Pin.AvLdSt.addr   = addr;
   return i;
}
PPCInstr* PPCInstr_AvUnary ( PPCAvOp op, HReg dst, HReg src ) {
   PPCInstr* i        = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag             = Pin_AvUnary;
   i->Pin.AvUnary.op  = op;
   i->Pin.AvUnary.dst = dst;
   i->Pin.AvUnary.src = src;
   return i;
}
PPCInstr* PPCInstr_AvBinary ( PPCAvOp op, HReg dst,
                              HReg srcL, HReg srcR ) {
   PPCInstr* i          = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag               = Pin_AvBinary;
   i->Pin.AvBinary.op   = op;
   i->Pin.AvBinary.dst  = dst;
   i->Pin.AvBinary.srcL = srcL;
   i->Pin.AvBinary.srcR = srcR;
   return i;
}
PPCInstr* PPCInstr_AvBin8x16 ( PPCAvOp op, HReg dst,
                               HReg srcL, HReg srcR ) {
   PPCInstr* i           = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag                = Pin_AvBin8x16;
   i->Pin.AvBin8x16.op   = op;
   i->Pin.AvBin8x16.dst  = dst;
   i->Pin.AvBin8x16.srcL = srcL;
   i->Pin.AvBin8x16.srcR = srcR;
   return i;
}
PPCInstr* PPCInstr_AvBin16x8 ( PPCAvOp op, HReg dst,
                               HReg srcL, HReg srcR ) {
   PPCInstr* i           = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag                = Pin_AvBin16x8;
   i->Pin.AvBin16x8.op   = op;
   i->Pin.AvBin16x8.dst  = dst;
   i->Pin.AvBin16x8.srcL = srcL;
   i->Pin.AvBin16x8.srcR = srcR;
   return i;
}
PPCInstr* PPCInstr_AvBin32x4 ( PPCAvOp op, HReg dst,
                               HReg srcL, HReg srcR ) {
   PPCInstr* i           = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag                = Pin_AvBin32x4;
   i->Pin.AvBin32x4.op   = op;
   i->Pin.AvBin32x4.dst  = dst;
   i->Pin.AvBin32x4.srcL = srcL;
   i->Pin.AvBin32x4.srcR = srcR;
   return i;
}
PPCInstr* PPCInstr_AvBin64x2 ( PPCAvOp op, HReg dst,
                               HReg srcL, HReg srcR ) {
   PPCInstr* i           = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag                = Pin_AvBin64x2;
   i->Pin.AvBin64x2.op   = op;
   i->Pin.AvBin64x2.dst  = dst;
   i->Pin.AvBin64x2.srcL = srcL;
   i->Pin.AvBin64x2.srcR = srcR;
   return i;
}

PPCInstr* PPCInstr_AvBin32Fx4 ( PPCAvFpOp op, HReg dst,
                                HReg srcL, HReg srcR ) {
   PPCInstr* i            = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag                 = Pin_AvBin32Fx4;
   i->Pin.AvBin32Fx4.op   = op;
   i->Pin.AvBin32Fx4.dst  = dst;
   i->Pin.AvBin32Fx4.srcL = srcL;
   i->Pin.AvBin32Fx4.srcR = srcR;
   return i;
}
PPCInstr* PPCInstr_AvUn32Fx4 ( PPCAvFpOp op, HReg dst, HReg src ) {
   PPCInstr* i          = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag               = Pin_AvUn32Fx4;
   i->Pin.AvUn32Fx4.op  = op;
   i->Pin.AvUn32Fx4.dst = dst;
   i->Pin.AvUn32Fx4.src = src;
   return i;
}
PPCInstr* PPCInstr_AvPerm ( HReg dst, HReg srcL, HReg srcR, HReg ctl ) {
   PPCInstr* i        = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag             = Pin_AvPerm;
   i->Pin.AvPerm.dst  = dst;
   i->Pin.AvPerm.srcL = srcL;
   i->Pin.AvPerm.srcR = srcR;
   i->Pin.AvPerm.ctl  = ctl;
   return i;
}

PPCInstr* PPCInstr_AvSel ( HReg ctl, HReg dst, HReg srcL, HReg srcR ) {
   PPCInstr* i       = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag            = Pin_AvSel;
   i->Pin.AvSel.ctl  = ctl;
   i->Pin.AvSel.dst  = dst;
   i->Pin.AvSel.srcL = srcL;
   i->Pin.AvSel.srcR = srcR;
   return i;
}
PPCInstr* PPCInstr_AvSh ( Bool shLeft, HReg dst, PPCAMode* addr ) {
   PPCInstr*  i       = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag             = Pin_AvSh;
   i->Pin.AvSh.shLeft = shLeft;
   i->Pin.AvSh.dst    = dst;
   i->Pin.AvSh.addr   = addr;
   return i;
}
PPCInstr* PPCInstr_AvShlDbl ( UChar shift, HReg dst,
                              HReg srcL, HReg srcR ) {
   PPCInstr* i           = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag                = Pin_AvShlDbl;
   i->Pin.AvShlDbl.shift = shift;
   i->Pin.AvShlDbl.dst   = dst;
   i->Pin.AvShlDbl.srcL  = srcL;
   i->Pin.AvShlDbl.srcR  = srcR;
   return i;
}
PPCInstr* PPCInstr_AvSplat ( UChar sz, HReg dst, PPCVI5s* src ) {
   PPCInstr* i        = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag             = Pin_AvSplat;
   i->Pin.AvSplat.sz  = sz;
   i->Pin.AvSplat.dst = dst;
   i->Pin.AvSplat.src = src;
   return i;
}
PPCInstr* PPCInstr_AvCMov ( PPCCondCode cond, HReg dst, HReg src ) {
   PPCInstr* i        = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag             = Pin_AvCMov;
   i->Pin.AvCMov.cond = cond;
   i->Pin.AvCMov.dst  = dst;
   i->Pin.AvCMov.src  = src;
   vassert(cond.test != Pct_ALWAYS);
   return i;
}
PPCInstr* PPCInstr_AvLdVSCR ( HReg src ) {
   PPCInstr* i         = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag              = Pin_AvLdVSCR;
   i->Pin.AvLdVSCR.src = src;
   return i;
}
PPCInstr* PPCInstr_AvCipherV128Unary ( PPCAvOp op, HReg dst, HReg src ) {
   PPCInstr* i              = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag                   = Pin_AvCipherV128Unary;
   i->Pin.AvCipherV128Unary.op   = op;
   i->Pin.AvCipherV128Unary.dst  = dst;
   i->Pin.AvCipherV128Unary.src  = src;
   return i;
}
PPCInstr* PPCInstr_AvCipherV128Binary ( PPCAvOp op, HReg dst,
                                        HReg srcL, HReg srcR ) {
   PPCInstr* i              = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag                   = Pin_AvCipherV128Binary;
   i->Pin.AvCipherV128Binary.op   = op;
   i->Pin.AvCipherV128Binary.dst  = dst;
   i->Pin.AvCipherV128Binary.srcL = srcL;
   i->Pin.AvCipherV128Binary.srcR = srcR;
   return i;
}
PPCInstr* PPCInstr_AvHashV128Binary ( PPCAvOp op, HReg dst,
                                      HReg src, PPCRI* s_field ) {
   PPCInstr* i              = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag                   = Pin_AvHashV128Binary;
   i->Pin.AvHashV128Binary.op  = op;
   i->Pin.AvHashV128Binary.dst = dst;
   i->Pin.AvHashV128Binary.src = src;
   i->Pin.AvHashV128Binary.s_field = s_field;
   return i;
}
PPCInstr* PPCInstr_AvBCDV128Trinary ( PPCAvOp op, HReg dst,
                                      HReg src1, HReg src2, PPCRI* ps ) {
   PPCInstr* i = LibVEX_Alloc_inline(sizeof(PPCInstr));
   i->tag      = Pin_AvBCDV128Trinary;
   i->Pin.AvBCDV128Trinary.op   = op;
   i->Pin.AvBCDV128Trinary.dst  = dst;
   i->Pin.AvBCDV128Trinary.src1 = src1;
   i->Pin.AvBCDV128Trinary.src2 = src2;
   i->Pin.AvBCDV128Trinary.ps   = ps;
   return i;
}


static void ppLoadImm ( HReg dst, ULong imm, Bool mode64 ) {
   vex_printf("li_word ");
   ppHRegPPC(dst);
   if (!mode64) {
      vex_printf(",0x%08x", (UInt)imm);
   } else {
      vex_printf(",0x%016llx", imm);
   }
}

static void ppMovReg ( HReg dst, HReg src ) {
   if (!sameHReg(dst, src)) {
      vex_printf("mr ");
      ppHRegPPC(dst);
      vex_printf(",");
      ppHRegPPC(src);
   }
}

void ppPPCInstr ( const PPCInstr* i, Bool mode64 )
{
   switch (i->tag) {
   case Pin_LI:
      ppLoadImm(i->Pin.LI.dst, i->Pin.LI.imm64, mode64);
      break;
   case Pin_Alu: {
      HReg   r_srcL  = i->Pin.Alu.srcL;
      PPCRH* rh_srcR = i->Pin.Alu.srcR;
      
      if (i->Pin.Alu.op == Palu_OR &&   
          rh_srcR->tag == Prh_Reg &&
          sameHReg(rh_srcR->Prh.Reg.reg, r_srcL)) {
         vex_printf("mr ");
         ppHRegPPC(i->Pin.Alu.dst);
         vex_printf(",");
         ppHRegPPC(r_srcL);
         return;
      }
      
      if (i->Pin.Alu.op == Palu_ADD &&   
          rh_srcR->tag == Prh_Imm &&
          hregEncoding(r_srcL) == 0) {
         vex_printf("li ");
         ppHRegPPC(i->Pin.Alu.dst);
         vex_printf(",");
         ppPPCRH(rh_srcR);
         return;
      }
      
      vex_printf("%s ", showPPCAluOp(i->Pin.Alu.op,
                                     toBool(rh_srcR->tag == Prh_Imm)));
      ppHRegPPC(i->Pin.Alu.dst);
      vex_printf(",");
      ppHRegPPC(r_srcL);
      vex_printf(",");
      ppPPCRH(rh_srcR);
      return;
   }
   case Pin_Shft: {
      HReg   r_srcL  = i->Pin.Shft.srcL;
      PPCRH* rh_srcR = i->Pin.Shft.srcR;
      vex_printf("%s ", showPPCShftOp(i->Pin.Shft.op,
                                      toBool(rh_srcR->tag == Prh_Imm),
                                      i->Pin.Shft.sz32));
      ppHRegPPC(i->Pin.Shft.dst);
      vex_printf(",");
      ppHRegPPC(r_srcL);
      vex_printf(",");
      ppPPCRH(rh_srcR);
      return;
   }
   case Pin_AddSubC:
      vex_printf("%s%s ",
                 i->Pin.AddSubC.isAdd ? "add" : "sub",
                 i->Pin.AddSubC.setC ? "c" : "e");
      ppHRegPPC(i->Pin.AddSubC.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.AddSubC.srcL);
      vex_printf(",");
      ppHRegPPC(i->Pin.AddSubC.srcR);
      return;
   case Pin_Cmp:
      vex_printf("%s%c%s %%cr%u,",
                 i->Pin.Cmp.syned ? "cmp" : "cmpl",
                 i->Pin.Cmp.sz32 ? 'w' : 'd',
                 i->Pin.Cmp.srcR->tag == Prh_Imm ? "i" : "",
                 i->Pin.Cmp.crfD);
      ppHRegPPC(i->Pin.Cmp.srcL);
      vex_printf(",");
      ppPPCRH(i->Pin.Cmp.srcR);
      return;
   case Pin_Unary:
      vex_printf("%s ", showPPCUnaryOp(i->Pin.Unary.op));
      ppHRegPPC(i->Pin.Unary.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.Unary.src);
      return;
   case Pin_MulL:
      vex_printf("mul%c%c%s ",
                 i->Pin.MulL.hi ? 'h' : 'l',
                 i->Pin.MulL.sz32 ? 'w' : 'd',
                 i->Pin.MulL.hi ? (i->Pin.MulL.syned ? "s" : "u") : "");
      ppHRegPPC(i->Pin.MulL.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.MulL.srcL);
      vex_printf(",");
      ppHRegPPC(i->Pin.MulL.srcR);
      return;
   case Pin_Div:
      vex_printf("div%c%s%s ",
                 i->Pin.Div.sz32 ? 'w' : 'd',
                 i->Pin.Div.extended ? "e" : "",
                 i->Pin.Div.syned ? "" : "u");
      ppHRegPPC(i->Pin.Div.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.Div.srcL);
      vex_printf(",");
      ppHRegPPC(i->Pin.Div.srcR);
      return;
   case Pin_Call: {
      Int n;
      vex_printf("call: ");
      if (i->Pin.Call.cond.test != Pct_ALWAYS) {
         vex_printf("if (%s) ", showPPCCondCode(i->Pin.Call.cond));
      }
      vex_printf("{ ");
      ppLoadImm(hregPPC_GPR10(mode64), i->Pin.Call.target, mode64);
      vex_printf(" ; mtctr r10 ; bctrl [");
      for (n = 0; n < 32; n++) {
         if (i->Pin.Call.argiregs & (1<<n)) {
            vex_printf("r%d", n);
            if ((i->Pin.Call.argiregs >> n) > 1)
               vex_printf(",");
         }
      }
      vex_printf(",");
      ppRetLoc(i->Pin.Call.rloc);
      vex_printf("] }");
      break;
   }
   case Pin_XDirect:
      vex_printf("(xDirect) ");
      vex_printf("if (%s) { ",
                 showPPCCondCode(i->Pin.XDirect.cond));
      if (mode64) {
         vex_printf("imm64 r30,0x%llx; ", i->Pin.XDirect.dstGA);
         vex_printf("std r30,");
      } else {
         vex_printf("imm32 r30,0x%llx; ", i->Pin.XDirect.dstGA);
         vex_printf("stw r30,");
      }
      ppPPCAMode(i->Pin.XDirect.amCIA);
      vex_printf("; ");
      if (mode64) {
         vex_printf("imm64-fixed5 r30,$disp_cp_chain_me_to_%sEP; ",
                    i->Pin.XDirect.toFastEP ? "fast" : "slow");
      } else {
         vex_printf("imm32-fixed2 r30,$disp_cp_chain_me_to_%sEP; ",
                    i->Pin.XDirect.toFastEP ? "fast" : "slow");
      }
      vex_printf("mtctr r30; bctrl }");
      return;
   case Pin_XIndir:
      vex_printf("(xIndir) ");
      vex_printf("if (%s) { ",
                 showPPCCondCode(i->Pin.XIndir.cond));
      vex_printf("%s ", mode64 ? "std" : "stw");
      ppHRegPPC(i->Pin.XIndir.dstGA);
      vex_printf(",");
      ppPPCAMode(i->Pin.XIndir.amCIA);
      vex_printf("; ");
      vex_printf("imm%s r30,$disp_cp_xindir; ", mode64 ? "64" : "32");
      vex_printf("mtctr r30; bctr }");
      return;
   case Pin_XAssisted:
      vex_printf("(xAssisted) ");
      vex_printf("if (%s) { ",
                 showPPCCondCode(i->Pin.XAssisted.cond));
      vex_printf("%s ", mode64 ? "std" : "stw");
      ppHRegPPC(i->Pin.XAssisted.dstGA);
      vex_printf(",");
      ppPPCAMode(i->Pin.XAssisted.amCIA);
      vex_printf("; ");
      vex_printf("li r31,$IRJumpKind_to_TRCVAL(%d); ",                            
                 (Int)i->Pin.XAssisted.jk);
      vex_printf("imm%s r30,$disp_cp_xindir; ", mode64 ? "64" : "32");
      vex_printf("mtctr r30; bctr }");
      return;
   case Pin_CMov:
      vex_printf("cmov (%s) ", showPPCCondCode(i->Pin.CMov.cond));
      ppHRegPPC(i->Pin.CMov.dst);
      vex_printf(",");
      ppPPCRI(i->Pin.CMov.src);
      vex_printf(": ");
      if (i->Pin.CMov.cond.test != Pct_ALWAYS) {
         vex_printf("if (%s) ", showPPCCondCode(i->Pin.CMov.cond));
      }
      vex_printf("{ ");
      if (i->Pin.CMov.src->tag == Pri_Imm) {
         ppLoadImm(i->Pin.CMov.dst, i->Pin.CMov.src->Pri.Imm, mode64);
      } else {
         ppMovReg(i->Pin.CMov.dst, i->Pin.CMov.src->Pri.Reg);
      }
      vex_printf(" }");
      return;
   case Pin_Load: {
      Bool idxd = toBool(i->Pin.Load.src->tag == Pam_RR);
      UChar sz = i->Pin.Load.sz;
      HChar c_sz = sz==1 ? 'b' : sz==2 ? 'h' : sz==4 ? 'w' : 'd';
      vex_printf("l%c%s%s ", c_sz, sz==8 ? "" : "z", idxd ? "x" : "" );
      ppHRegPPC(i->Pin.Load.dst);
      vex_printf(",");
      ppPPCAMode(i->Pin.Load.src);
      return;
   }
   case Pin_LoadL: {
      UChar sz = i->Pin.LoadL.sz;
      HChar c_sz = sz==1 ? 'b' : sz==2 ? 'h' : sz==4 ? 'w' : 'd';
      vex_printf("l%carx ", c_sz);
      ppHRegPPC(i->Pin.LoadL.dst);
      vex_printf(",%%r0,");
      ppHRegPPC(i->Pin.LoadL.src);
      return;
   }
   case Pin_Store: {
      UChar sz = i->Pin.Store.sz;
      Bool idxd = toBool(i->Pin.Store.dst->tag == Pam_RR);
      HChar c_sz = sz==1 ? 'b' : sz==2 ? 'h' : sz==4 ? 'w' :  'd';
      vex_printf("st%c%s ", c_sz, idxd ? "x" : "" );
      ppHRegPPC(i->Pin.Store.src);
      vex_printf(",");
      ppPPCAMode(i->Pin.Store.dst);
      return;
   }
   case Pin_StoreC: {
      UChar sz = i->Pin.StoreC.sz;
      HChar c_sz = sz==1 ? 'b' : sz==2 ? 'h' : sz==4 ? 'w' : 'd';
      vex_printf("st%ccx. ", c_sz);
      ppHRegPPC(i->Pin.StoreC.src);
      vex_printf(",%%r0,");
      ppHRegPPC(i->Pin.StoreC.dst);
      return;
   }
   case Pin_Set: {
      PPCCondCode cc = i->Pin.Set.cond;
      vex_printf("set (%s),", showPPCCondCode(cc));
      ppHRegPPC(i->Pin.Set.dst);
      if (cc.test == Pct_ALWAYS) {
         vex_printf(": { li ");
         ppHRegPPC(i->Pin.Set.dst);
         vex_printf(",1 }");
      } else {
         vex_printf(": { mfcr r0 ; rlwinm ");
         ppHRegPPC(i->Pin.Set.dst);
         vex_printf(",r0,%u,31,31", cc.flag+1);
         if (cc.test == Pct_FALSE) {
            vex_printf("; xori ");
            ppHRegPPC(i->Pin.Set.dst);
            vex_printf(",");
            ppHRegPPC(i->Pin.Set.dst);
            vex_printf(",1");
         }
         vex_printf(" }");
      }
      return;
   }
   case Pin_MfCR:
      vex_printf("mfcr ");
      ppHRegPPC(i->Pin.MfCR.dst);
      break;
   case Pin_MFence:
      vex_printf("mfence (=sync)");
      return;

   case Pin_FpUnary:
      vex_printf("%s ", showPPCFpOp(i->Pin.FpUnary.op));
      ppHRegPPC(i->Pin.FpUnary.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.FpUnary.src);
      return;
   case Pin_FpBinary:
      vex_printf("%s ", showPPCFpOp(i->Pin.FpBinary.op));
      ppHRegPPC(i->Pin.FpBinary.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.FpBinary.srcL);
      vex_printf(",");
      ppHRegPPC(i->Pin.FpBinary.srcR);
      return;
   case Pin_FpMulAcc:
      vex_printf("%s ", showPPCFpOp(i->Pin.FpMulAcc.op));
      ppHRegPPC(i->Pin.FpMulAcc.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.FpMulAcc.srcML);
      vex_printf(",");
      ppHRegPPC(i->Pin.FpMulAcc.srcMR);
      vex_printf(",");
      ppHRegPPC(i->Pin.FpMulAcc.srcAcc);
      return;
   case Pin_FpLdSt: {
      UChar sz = i->Pin.FpLdSt.sz;
      Bool idxd = toBool(i->Pin.FpLdSt.addr->tag == Pam_RR);
      if (i->Pin.FpLdSt.isLoad) {
         vex_printf("lf%c%s ",
                    (sz==4 ? 's' : 'd'),
                    idxd ? "x" : "" );
         ppHRegPPC(i->Pin.FpLdSt.reg);
         vex_printf(",");
         ppPPCAMode(i->Pin.FpLdSt.addr);
      } else {
         vex_printf("stf%c%s ",
                    (sz==4 ? 's' : 'd'),
                    idxd ? "x" : "" );
         ppHRegPPC(i->Pin.FpLdSt.reg);
         vex_printf(",");
         ppPPCAMode(i->Pin.FpLdSt.addr);
      }
      return;
   }
   case Pin_FpSTFIW:
      vex_printf("stfiwz ");
      ppHRegPPC(i->Pin.FpSTFIW.data);
      vex_printf(",0(");
      ppHRegPPC(i->Pin.FpSTFIW.addr);
      vex_printf(")");
      return;
   case Pin_FpRSP:
      vex_printf("frsp ");
      ppHRegPPC(i->Pin.FpRSP.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.FpRSP.src);
      return;
   case Pin_FpCftI: {
      const HChar* str = "fc?????";
      if (i->Pin.FpCftI.fromI == False && i->Pin.FpCftI.int32 == False)
         if (i->Pin.FpCftI.syned == True)
            str = "fctid";
         else
            str = "fctidu";
      else if (i->Pin.FpCftI.fromI == False && i->Pin.FpCftI.int32 == True)
         if (i->Pin.FpCftI.syned == True)
            str = "fctiw";
         else
            str = "fctiwu";
      else if (i->Pin.FpCftI.fromI == True && i->Pin.FpCftI.int32 == False) {
         if (i->Pin.FpCftI.syned == True) {
            str = "fcfid";
         } else {
            if (i->Pin.FpCftI.flt64 == True)
               str = "fcfidu";
            else
               str = "fcfidus";
         }
      }
      vex_printf("%s ", str);
      ppHRegPPC(i->Pin.FpCftI.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.FpCftI.src);
      return;
   }
   case Pin_FpCMov:
      vex_printf("fpcmov (%s) ", showPPCCondCode(i->Pin.FpCMov.cond));
      ppHRegPPC(i->Pin.FpCMov.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.FpCMov.src);
      vex_printf(": ");
      vex_printf("if (fr_dst != fr_src) { ");
      if (i->Pin.FpCMov.cond.test != Pct_ALWAYS) {
         vex_printf("if (%s) { ", showPPCCondCode(i->Pin.FpCMov.cond));
      }
      vex_printf("fmr ");
      ppHRegPPC(i->Pin.FpCMov.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.FpCMov.src);
      if (i->Pin.FpCMov.cond.test != Pct_ALWAYS)
         vex_printf(" }");
      vex_printf(" }");
      return;
   case Pin_FpLdFPSCR:
      vex_printf("mtfsf 0xFF,");
      ppHRegPPC(i->Pin.FpLdFPSCR.src);
      vex_printf(",0, %s", i->Pin.FpLdFPSCR.dfp_rm ? "1" : "0");
      return;
   case Pin_FpCmp:
      vex_printf("fcmpo %%cr1,");
      ppHRegPPC(i->Pin.FpCmp.srcL);
      vex_printf(",");
      ppHRegPPC(i->Pin.FpCmp.srcR);
      vex_printf("; mfcr ");
      ppHRegPPC(i->Pin.FpCmp.dst);
      vex_printf("; rlwinm ");
      ppHRegPPC(i->Pin.FpCmp.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.FpCmp.dst);
      vex_printf(",8,28,31");
      return;

   case Pin_RdWrLR:
      vex_printf("%s ", i->Pin.RdWrLR.wrLR ? "mtlr" : "mflr");
      ppHRegPPC(i->Pin.RdWrLR.gpr);
      return;

   case Pin_AvLdSt: {
      UChar  sz = i->Pin.AvLdSt.sz;
      const HChar* str_size;
      if (i->Pin.AvLdSt.addr->tag == Pam_IR) {
         ppLoadImm(hregPPC_GPR30(mode64),
                   i->Pin.AvLdSt.addr->Pam.IR.index, mode64);
         vex_printf(" ; ");
      }
      str_size = sz==1 ? "eb" : sz==2 ? "eh" : sz==4 ? "ew" : "";
      if (i->Pin.AvLdSt.isLoad)
         vex_printf("lv%sx ", str_size);
      else
         vex_printf("stv%sx ", str_size);
      ppHRegPPC(i->Pin.AvLdSt.reg);
      vex_printf(",");
      if (i->Pin.AvLdSt.addr->tag == Pam_IR)
         vex_printf("%%r30");
      else 
         ppHRegPPC(i->Pin.AvLdSt.addr->Pam.RR.index);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvLdSt.addr->Pam.RR.base);
      return;
   }
   case Pin_AvUnary:
      vex_printf("%s ", showPPCAvOp(i->Pin.AvUnary.op));
      ppHRegPPC(i->Pin.AvUnary.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvUnary.src);
      return;
   case Pin_AvBinary:
      vex_printf("%s ", showPPCAvOp(i->Pin.AvBinary.op));
      ppHRegPPC(i->Pin.AvBinary.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvBinary.srcL);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvBinary.srcR);
      return;
   case Pin_AvBin8x16:
      vex_printf("%s(b) ", showPPCAvOp(i->Pin.AvBin8x16.op));
      ppHRegPPC(i->Pin.AvBin8x16.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvBin8x16.srcL);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvBin8x16.srcR);
      return;
   case Pin_AvBin16x8:
      vex_printf("%s(h) ", showPPCAvOp(i->Pin.AvBin16x8.op));
      ppHRegPPC(i->Pin.AvBin16x8.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvBin16x8.srcL);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvBin16x8.srcR);
      return;
   case Pin_AvBin32x4:
      vex_printf("%s(w) ", showPPCAvOp(i->Pin.AvBin32x4.op));
      ppHRegPPC(i->Pin.AvBin32x4.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvBin32x4.srcL);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvBin32x4.srcR);
      return;
   case Pin_AvBin64x2:
      vex_printf("%s(w) ", showPPCAvOp(i->Pin.AvBin64x2.op));
      ppHRegPPC(i->Pin.AvBin64x2.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvBin64x2.srcL);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvBin64x2.srcR);
      return;
   case Pin_AvBin32Fx4:
      vex_printf("%s ", showPPCAvFpOp(i->Pin.AvBin32Fx4.op));
      ppHRegPPC(i->Pin.AvBin32Fx4.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvBin32Fx4.srcL);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvBin32Fx4.srcR);
      return;
   case Pin_AvUn32Fx4:
      vex_printf("%s ", showPPCAvFpOp(i->Pin.AvUn32Fx4.op));
      ppHRegPPC(i->Pin.AvUn32Fx4.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvUn32Fx4.src);
      return;
   case Pin_AvPerm:
      vex_printf("vperm ");
      ppHRegPPC(i->Pin.AvPerm.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvPerm.srcL);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvPerm.srcR);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvPerm.ctl);
      return;

   case Pin_AvSel:
      vex_printf("vsel ");
      ppHRegPPC(i->Pin.AvSel.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvSel.srcL);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvSel.srcR);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvSel.ctl);
      return;

   case Pin_AvSh:
      if (i->Pin.AvSh.addr->tag == Pam_IR) {
         ppLoadImm(hregPPC_GPR30(mode64),
                   i->Pin.AvSh.addr->Pam.IR.index, mode64);
         vex_printf(" ; ");
      }

      if (i->Pin.AvSh.shLeft)
         vex_printf("lvsl ");
      else
         vex_printf("lvsr ");

      ppHRegPPC(i->Pin.AvSh.dst);
      if (i->Pin.AvSh.addr->tag == Pam_IR)
         vex_printf("%%r30");
      else
         ppHRegPPC(i->Pin.AvSh.addr->Pam.RR.index);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvSh.addr->Pam.RR.base);
      return;

   case Pin_AvShlDbl:
      vex_printf("vsldoi ");
      ppHRegPPC(i->Pin.AvShlDbl.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvShlDbl.srcL);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvShlDbl.srcR);
      vex_printf(",%d", i->Pin.AvShlDbl.shift);
      return;

   case Pin_AvSplat: {
      UChar sz = i->Pin.AvSplat.sz;
      HChar ch_sz = toUChar( (sz == 8) ? 'b' : (sz == 16) ? 'h' : 'w' );
      vex_printf("vsplt%s%c ",
                 i->Pin.AvSplat.src->tag == Pvi_Imm ? "is" : "", ch_sz);
      ppHRegPPC(i->Pin.AvSplat.dst);
      vex_printf(",");
      ppPPCVI5s(i->Pin.AvSplat.src);
      if (i->Pin.AvSplat.src->tag == Pvi_Reg)
         vex_printf(", %d", (128/sz)-1);   
      return;
   }

   case Pin_AvCMov:
      vex_printf("avcmov (%s) ", showPPCCondCode(i->Pin.AvCMov.cond));
      ppHRegPPC(i->Pin.AvCMov.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvCMov.src);
      vex_printf(": ");
      vex_printf("if (v_dst != v_src) { ");
      if (i->Pin.AvCMov.cond.test != Pct_ALWAYS) {
         vex_printf("if (%s) { ", showPPCCondCode(i->Pin.AvCMov.cond));
      }
      vex_printf("vmr ");
      ppHRegPPC(i->Pin.AvCMov.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvCMov.src);
      if (i->Pin.FpCMov.cond.test != Pct_ALWAYS)
         vex_printf(" }");
      vex_printf(" }");
      return;

   case Pin_AvLdVSCR:
      vex_printf("mtvscr ");
      ppHRegPPC(i->Pin.AvLdVSCR.src);
      return;

   case Pin_AvCipherV128Unary:
      vex_printf("%s(w) ", showPPCAvOp(i->Pin.AvCipherV128Unary.op));
      ppHRegPPC(i->Pin.AvCipherV128Unary.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvCipherV128Unary.src);
      return;

   case Pin_AvCipherV128Binary:
      vex_printf("%s(w) ", showPPCAvOp(i->Pin.AvCipherV128Binary.op));
      ppHRegPPC(i->Pin.AvCipherV128Binary.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvCipherV128Binary.srcL);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvCipherV128Binary.srcR);
      return;

   case Pin_AvHashV128Binary:
      vex_printf("%s(w) ", showPPCAvOp(i->Pin.AvHashV128Binary.op));
      ppHRegPPC(i->Pin.AvHashV128Binary.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvHashV128Binary.src);
      vex_printf(",");
      ppPPCRI(i->Pin.AvHashV128Binary.s_field);
      return;

   case Pin_AvBCDV128Trinary:
      vex_printf("%s(w) ", showPPCAvOp(i->Pin.AvBCDV128Trinary.op));
      ppHRegPPC(i->Pin.AvBCDV128Trinary.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvBCDV128Trinary.src1);
      vex_printf(",");
      ppHRegPPC(i->Pin.AvBCDV128Trinary.src2);
      vex_printf(",");
      ppPPCRI(i->Pin.AvBCDV128Trinary.ps);
      return;

   case Pin_Dfp64Unary:
      vex_printf("%s ", showPPCFpOp(i->Pin.Dfp64Unary.op));
      ppHRegPPC(i->Pin.Dfp64Unary.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.Dfp64Unary.src);
      return;

   case Pin_Dfp64Binary:
      vex_printf("%s ", showPPCFpOp(i->Pin.Dfp64Binary.op));
      ppHRegPPC(i->Pin.Dfp64Binary.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.Dfp64Binary.srcL);
      vex_printf(",");
      ppHRegPPC(i->Pin.Dfp64Binary.srcR);
      return;

   case Pin_DfpShift:
      vex_printf("%s ", showPPCFpOp(i->Pin.DfpShift.op));
      ppHRegPPC(i->Pin.DfpShift.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.DfpShift.src);
      vex_printf(",");
      ppPPCRI(i->Pin.DfpShift.shift);
      return;

   case Pin_Dfp128Unary:
      vex_printf("%s ", showPPCFpOp(i->Pin.Dfp128Unary.op));
      ppHRegPPC(i->Pin.Dfp128Unary.dst_hi);
      vex_printf(",");
      ppHRegPPC(i->Pin.Dfp128Unary.src_hi);
      return;

   case Pin_Dfp128Binary:
      vex_printf("%s ", showPPCFpOp(i->Pin.Dfp128Binary.op));
      ppHRegPPC(i->Pin.Dfp128Binary.dst_hi);
      vex_printf(",");
      ppHRegPPC(i->Pin.Dfp128Binary.srcR_hi);
      return;

   case Pin_DfpShift128:
      vex_printf("%s ", showPPCFpOp(i->Pin.DfpShift128.op));
      ppHRegPPC(i->Pin.DfpShift128.dst_hi);
      vex_printf(",");
      ppHRegPPC(i->Pin.DfpShift128.src_hi);
      vex_printf(",");
      ppPPCRI(i->Pin.DfpShift128.shift);
      return;

   case Pin_DfpRound:
      vex_printf("drintx ");
      ppHRegPPC(i->Pin.DfpRound.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.DfpRound.src);
      vex_printf(",");
      ppPPCRI(i->Pin.DfpRound.r_rmc); 
      return;

   case Pin_DfpRound128:
      vex_printf("drintxq ");
      ppHRegPPC(i->Pin.DfpRound128.dst_hi);
      vex_printf(",");
      ppHRegPPC(i->Pin.DfpRound128.src_hi);
      vex_printf(",");
      ppPPCRI(i->Pin.DfpRound128.r_rmc); 
      return;

   case Pin_DfpQuantize:
      vex_printf("%s ", showPPCFpOp(i->Pin.DfpQuantize.op));
      ppHRegPPC(i->Pin.DfpQuantize.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.DfpQuantize.srcL);
      vex_printf(",");
      ppHRegPPC(i->Pin.DfpQuantize.srcR);
      vex_printf(",");
      ppPPCRI(i->Pin.DfpQuantize.rmc);
      return;

   case Pin_DfpQuantize128:
      
      vex_printf("dquaq ");
      ppHRegPPC(i->Pin.DfpQuantize128.dst_hi);
      vex_printf(",");
      ppHRegPPC(i->Pin.DfpQuantize128.dst_hi);
      vex_printf(",");
      ppHRegPPC(i->Pin.DfpQuantize128.src_hi);
      vex_printf(",");
      ppPPCRI(i->Pin.DfpQuantize128.rmc);
      return;

   case Pin_DfpD128toD64:
      vex_printf("%s ", showPPCFpOp(i->Pin.DfpD128toD64.op));
      ppHRegPPC(i->Pin.DfpD128toD64.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.DfpD128toD64.src_hi);
      vex_printf(",");
      return;

   case Pin_DfpI64StoD128:
      vex_printf("%s ", showPPCFpOp(i->Pin.DfpI64StoD128.op));
      ppHRegPPC(i->Pin.DfpI64StoD128.dst_hi);
      vex_printf(",");
      ppHRegPPC(i->Pin.DfpI64StoD128.src);
      vex_printf(",");
      return;
   case Pin_ExtractExpD128:
      vex_printf("dxexq ");
      ppHRegPPC(i->Pin.ExtractExpD128.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.ExtractExpD128.src_hi);
      return;
   case Pin_InsertExpD128:
      vex_printf("diexq ");
      ppHRegPPC(i->Pin.InsertExpD128.dst_hi);
      vex_printf(",");
      ppHRegPPC(i->Pin.InsertExpD128.srcL);
      vex_printf(",");
      ppHRegPPC(i->Pin.InsertExpD128.srcR_hi);
      return;
   case Pin_Dfp64Cmp:
      vex_printf("dcmpo %%cr1,");
      ppHRegPPC(i->Pin.Dfp64Cmp.srcL);
      vex_printf(",");
      ppHRegPPC(i->Pin.Dfp64Cmp.srcR);
      vex_printf("; mfcr ");
      ppHRegPPC(i->Pin.Dfp64Cmp.dst);
      vex_printf("; rlwinm ");
      ppHRegPPC(i->Pin.Dfp64Cmp.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.Dfp64Cmp.dst);
      vex_printf(",8,28,31");
      return;
   case Pin_Dfp128Cmp:
      vex_printf("dcmpoq %%cr1,");
      ppHRegPPC(i->Pin.Dfp128Cmp.srcL_hi);
      vex_printf(",");
      ppHRegPPC(i->Pin.Dfp128Cmp.srcR_hi);
      vex_printf("; mfcr ");
      ppHRegPPC(i->Pin.Dfp128Cmp.dst);
      vex_printf("; rlwinm ");
      ppHRegPPC(i->Pin.Dfp128Cmp.dst);
      vex_printf(",");
      ppHRegPPC(i->Pin.Dfp128Cmp.dst);
      vex_printf(",8,28,31");
      return;
   case Pin_EvCheck:
      
      vex_printf("(evCheck) ");
      vex_printf("lwz r30,");
      ppPPCAMode(i->Pin.EvCheck.amCounter);
      vex_printf("; addic. r30,r30,-1; ");
      vex_printf("stw r30,");
      ppPPCAMode(i->Pin.EvCheck.amCounter);
      vex_printf("; bge nofail; lwz r30,");
      ppPPCAMode(i->Pin.EvCheck.amFailAddr);
      vex_printf("; mtctr r30; bctr; nofail:");
      return;
   case Pin_ProfInc:
      if (mode64) {
         vex_printf("(profInc) imm64-fixed5 r30,$NotKnownYet; ");
         vex_printf("ld r29,(r30); addi r29,r29,1; std r29,(r30)");
      } else {
         vex_printf("(profInc) imm32-fixed2 r30,$NotKnownYet; ");
         vex_printf("lwz r29,4(r30); addic. r29,r29,1; stw r29,4(r30)");
         vex_printf("lwz r29,0(r30); addze r29,r29; stw r29,0(r30)");
      }
      break;
   default:
      vex_printf("\nppPPCInstr: No such tag(%d)\n", (Int)i->tag);
      vpanic("ppPPCInstr");
   }
}


void getRegUsage_PPCInstr ( HRegUsage* u, const PPCInstr* i, Bool mode64 )
{
   initHRegUsage(u);
   switch (i->tag) {
   case Pin_LI:
      addHRegUse(u, HRmWrite, i->Pin.LI.dst);
      break;
   case Pin_Alu:
      addHRegUse(u, HRmRead,  i->Pin.Alu.srcL);
      addRegUsage_PPCRH(u,    i->Pin.Alu.srcR);
      addHRegUse(u, HRmWrite, i->Pin.Alu.dst);
      return;
   case Pin_Shft:
      addHRegUse(u, HRmRead,  i->Pin.Shft.srcL);
      addRegUsage_PPCRH(u,    i->Pin.Shft.srcR);
      addHRegUse(u, HRmWrite, i->Pin.Shft.dst);
      return;
   case Pin_AddSubC:
      addHRegUse(u, HRmWrite, i->Pin.AddSubC.dst);
      addHRegUse(u, HRmRead,  i->Pin.AddSubC.srcL);
      addHRegUse(u, HRmRead,  i->Pin.AddSubC.srcR);
      return;
   case Pin_Cmp:
      addHRegUse(u, HRmRead, i->Pin.Cmp.srcL);
      addRegUsage_PPCRH(u,   i->Pin.Cmp.srcR);
      return;
   case Pin_Unary:
      addHRegUse(u, HRmWrite, i->Pin.Unary.dst);
      addHRegUse(u, HRmRead,  i->Pin.Unary.src);
      return;
   case Pin_MulL:
      addHRegUse(u, HRmWrite, i->Pin.MulL.dst);
      addHRegUse(u, HRmRead,  i->Pin.MulL.srcL);
      addHRegUse(u, HRmRead,  i->Pin.MulL.srcR);
      return;
   case Pin_Div:
      addHRegUse(u, HRmWrite, i->Pin.Div.dst);
      addHRegUse(u, HRmRead,  i->Pin.Div.srcL);
      addHRegUse(u, HRmRead,  i->Pin.Div.srcR);
      return;
   case Pin_Call: {
      UInt argir;
      
      addHRegUse(u, HRmWrite, hregPPC_GPR3(mode64));
      addHRegUse(u, HRmWrite, hregPPC_GPR4(mode64));
      addHRegUse(u, HRmWrite, hregPPC_GPR5(mode64));
      addHRegUse(u, HRmWrite, hregPPC_GPR6(mode64));
      addHRegUse(u, HRmWrite, hregPPC_GPR7(mode64));
      addHRegUse(u, HRmWrite, hregPPC_GPR8(mode64));
      addHRegUse(u, HRmWrite, hregPPC_GPR9(mode64));
      addHRegUse(u, HRmWrite, hregPPC_GPR10(mode64));
      if (!mode64) {
         addHRegUse(u, HRmWrite, hregPPC_GPR11(mode64));
         addHRegUse(u, HRmWrite, hregPPC_GPR12(mode64));
      }

      argir = i->Pin.Call.argiregs;
      if (argir &(1<<10)) addHRegUse(u, HRmRead, hregPPC_GPR10(mode64));
      if (argir & (1<<9)) addHRegUse(u, HRmRead, hregPPC_GPR9(mode64));
      if (argir & (1<<8)) addHRegUse(u, HRmRead, hregPPC_GPR8(mode64));
      if (argir & (1<<7)) addHRegUse(u, HRmRead, hregPPC_GPR7(mode64));
      if (argir & (1<<6)) addHRegUse(u, HRmRead, hregPPC_GPR6(mode64));
      if (argir & (1<<5)) addHRegUse(u, HRmRead, hregPPC_GPR5(mode64));
      if (argir & (1<<4)) addHRegUse(u, HRmRead, hregPPC_GPR4(mode64));
      if (argir & (1<<3)) addHRegUse(u, HRmRead, hregPPC_GPR3(mode64));

      vassert(0 == (argir & ~((1<<3)|(1<<4)|(1<<5)|(1<<6)
                              |(1<<7)|(1<<8)|(1<<9)|(1<<10))));

      addHRegUse(u, HRmWrite, hregPPC_GPR10(mode64));
      return;
   }
   case Pin_XDirect:
      addRegUsage_PPCAMode(u, i->Pin.XDirect.amCIA);
      return;
   case Pin_XIndir:
      addHRegUse(u, HRmRead, i->Pin.XIndir.dstGA);
      addRegUsage_PPCAMode(u, i->Pin.XIndir.amCIA);
      return;
   case Pin_XAssisted:
      addHRegUse(u, HRmRead, i->Pin.XAssisted.dstGA);
      addRegUsage_PPCAMode(u, i->Pin.XAssisted.amCIA);
      return;
   case Pin_CMov:
      addRegUsage_PPCRI(u,  i->Pin.CMov.src);
      addHRegUse(u, HRmWrite, i->Pin.CMov.dst);
      return;
   case Pin_Load:
      addRegUsage_PPCAMode(u, i->Pin.Load.src);
      addHRegUse(u, HRmWrite, i->Pin.Load.dst);
      return;
   case Pin_LoadL:
      addHRegUse(u, HRmRead,  i->Pin.LoadL.src);
      addHRegUse(u, HRmWrite, i->Pin.LoadL.dst);
      return;
   case Pin_Store:
      addHRegUse(u, HRmRead,  i->Pin.Store.src);
      addRegUsage_PPCAMode(u, i->Pin.Store.dst);
      return;
   case Pin_StoreC:
      addHRegUse(u, HRmRead, i->Pin.StoreC.src);
      addHRegUse(u, HRmRead, i->Pin.StoreC.dst);
      return;
   case Pin_Set:
      addHRegUse(u, HRmWrite, i->Pin.Set.dst);
      return;
   case Pin_MfCR:
      addHRegUse(u, HRmWrite, i->Pin.MfCR.dst);
      return;
   case Pin_MFence:
      return;

   case Pin_FpUnary:
      addHRegUse(u, HRmWrite, i->Pin.FpUnary.dst);
      addHRegUse(u, HRmRead,  i->Pin.FpUnary.src);
      return;
   case Pin_FpBinary:
      addHRegUse(u, HRmWrite, i->Pin.FpBinary.dst);
      addHRegUse(u, HRmRead,  i->Pin.FpBinary.srcL);
      addHRegUse(u, HRmRead,  i->Pin.FpBinary.srcR);
      return;
   case Pin_FpMulAcc:
      addHRegUse(u, HRmWrite, i->Pin.FpMulAcc.dst);
      addHRegUse(u, HRmRead,  i->Pin.FpMulAcc.srcML);
      addHRegUse(u, HRmRead,  i->Pin.FpMulAcc.srcMR);
      addHRegUse(u, HRmRead,  i->Pin.FpMulAcc.srcAcc);
      return;
   case Pin_FpLdSt:
      addHRegUse(u, (i->Pin.FpLdSt.isLoad ? HRmWrite : HRmRead),
                 i->Pin.FpLdSt.reg);
      addRegUsage_PPCAMode(u, i->Pin.FpLdSt.addr);
      return;
   case Pin_FpSTFIW:
      addHRegUse(u, HRmRead, i->Pin.FpSTFIW.addr);
      addHRegUse(u, HRmRead, i->Pin.FpSTFIW.data);
      return;
   case Pin_FpRSP:
      addHRegUse(u, HRmWrite, i->Pin.FpRSP.dst);
      addHRegUse(u, HRmRead,  i->Pin.FpRSP.src);
      return;
   case Pin_FpCftI:
      addHRegUse(u, HRmWrite, i->Pin.FpCftI.dst);
      addHRegUse(u, HRmRead,  i->Pin.FpCftI.src);
      return;
   case Pin_FpCMov:
      addHRegUse(u, HRmModify, i->Pin.FpCMov.dst);
      addHRegUse(u, HRmRead,   i->Pin.FpCMov.src);
      return;
   case Pin_FpLdFPSCR:
      addHRegUse(u, HRmRead, i->Pin.FpLdFPSCR.src);
      return;
   case Pin_FpCmp:
      addHRegUse(u, HRmWrite, i->Pin.FpCmp.dst);
      addHRegUse(u, HRmRead,  i->Pin.FpCmp.srcL);
      addHRegUse(u, HRmRead,  i->Pin.FpCmp.srcR);
      return;

   case Pin_RdWrLR:
      addHRegUse(u, (i->Pin.RdWrLR.wrLR ? HRmRead : HRmWrite),
                 i->Pin.RdWrLR.gpr);
      return;

   case Pin_AvLdSt:
      addHRegUse(u, (i->Pin.AvLdSt.isLoad ? HRmWrite : HRmRead),
                 i->Pin.AvLdSt.reg);
      if (i->Pin.AvLdSt.addr->tag == Pam_IR)
         addHRegUse(u, HRmWrite, hregPPC_GPR30(mode64));
      addRegUsage_PPCAMode(u, i->Pin.AvLdSt.addr);
      return;
   case Pin_AvUnary:
      addHRegUse(u, HRmWrite, i->Pin.AvUnary.dst);
      addHRegUse(u, HRmRead,  i->Pin.AvUnary.src);
      return;
   case Pin_AvBinary:
      if (i->Pin.AvBinary.op == Pav_XOR
          && sameHReg(i->Pin.AvBinary.dst, i->Pin.AvBinary.srcL)
          && sameHReg(i->Pin.AvBinary.dst, i->Pin.AvBinary.srcR)) {
         
         
         addHRegUse(u, HRmWrite, i->Pin.AvBinary.dst);
      } else {
         addHRegUse(u, HRmWrite, i->Pin.AvBinary.dst);
         addHRegUse(u, HRmRead,  i->Pin.AvBinary.srcL);
         addHRegUse(u, HRmRead,  i->Pin.AvBinary.srcR);
      }
      return;
   case Pin_AvBin8x16:
      addHRegUse(u, HRmWrite, i->Pin.AvBin8x16.dst);
      addHRegUse(u, HRmRead,  i->Pin.AvBin8x16.srcL);
      addHRegUse(u, HRmRead,  i->Pin.AvBin8x16.srcR);
      return;
   case Pin_AvBin16x8:
      addHRegUse(u, HRmWrite, i->Pin.AvBin16x8.dst);
      addHRegUse(u, HRmRead,  i->Pin.AvBin16x8.srcL);
      addHRegUse(u, HRmRead,  i->Pin.AvBin16x8.srcR);
      return;
   case Pin_AvBin32x4:
      addHRegUse(u, HRmWrite, i->Pin.AvBin32x4.dst);
      addHRegUse(u, HRmRead,  i->Pin.AvBin32x4.srcL);
      addHRegUse(u, HRmRead,  i->Pin.AvBin32x4.srcR);
      return;
   case Pin_AvBin64x2:
      addHRegUse(u, HRmWrite, i->Pin.AvBin64x2.dst);
      addHRegUse(u, HRmRead,  i->Pin.AvBin64x2.srcL);
      addHRegUse(u, HRmRead,  i->Pin.AvBin64x2.srcR);
      return;
   case Pin_AvBin32Fx4:
      addHRegUse(u, HRmWrite, i->Pin.AvBin32Fx4.dst);
      addHRegUse(u, HRmRead,  i->Pin.AvBin32Fx4.srcL);
      addHRegUse(u, HRmRead,  i->Pin.AvBin32Fx4.srcR);
      if (i->Pin.AvBin32Fx4.op == Pavfp_MULF)
         addHRegUse(u, HRmWrite, hregPPC_VR29(mode64));
      return;
   case Pin_AvUn32Fx4:
      addHRegUse(u, HRmWrite, i->Pin.AvUn32Fx4.dst);
      addHRegUse(u, HRmRead,  i->Pin.AvUn32Fx4.src);
      return;
   case Pin_AvPerm:
      addHRegUse(u, HRmWrite, i->Pin.AvPerm.dst);
      addHRegUse(u, HRmRead,  i->Pin.AvPerm.srcL);
      addHRegUse(u, HRmRead,  i->Pin.AvPerm.srcR);
      addHRegUse(u, HRmRead,  i->Pin.AvPerm.ctl);
      return;
   case Pin_AvSel:
      addHRegUse(u, HRmWrite, i->Pin.AvSel.dst);
      addHRegUse(u, HRmRead,  i->Pin.AvSel.ctl);
      addHRegUse(u, HRmRead,  i->Pin.AvSel.srcL);
      addHRegUse(u, HRmRead,  i->Pin.AvSel.srcR);
      return;
   case Pin_AvSh:
      addHRegUse(u, HRmWrite, i->Pin.AvSh.dst);
      if (i->Pin.AvSh.addr->tag == Pam_IR)
         addHRegUse(u, HRmWrite, hregPPC_GPR30(mode64));
      addRegUsage_PPCAMode(u, i->Pin.AvSh.addr);
      return;
   case Pin_AvShlDbl:
      addHRegUse(u, HRmWrite, i->Pin.AvShlDbl.dst);
      addHRegUse(u, HRmRead,  i->Pin.AvShlDbl.srcL);
      addHRegUse(u, HRmRead,  i->Pin.AvShlDbl.srcR);
      return;
   case Pin_AvSplat:
      addHRegUse(u, HRmWrite, i->Pin.AvSplat.dst);
      addRegUsage_PPCVI5s(u,  i->Pin.AvSplat.src);
      return;
   case Pin_AvCMov:
      addHRegUse(u, HRmModify, i->Pin.AvCMov.dst);
      addHRegUse(u, HRmRead,   i->Pin.AvCMov.src);
      return;
   case Pin_AvLdVSCR:
      addHRegUse(u, HRmRead, i->Pin.AvLdVSCR.src);
      return;
   case Pin_AvCipherV128Unary:
      addHRegUse(u, HRmWrite, i->Pin.AvCipherV128Unary.dst);
      addHRegUse(u, HRmRead,  i->Pin.AvCipherV128Unary.src);
      return;
   case Pin_AvCipherV128Binary:
      addHRegUse(u, HRmWrite, i->Pin.AvCipherV128Binary.dst);
      addHRegUse(u, HRmRead,  i->Pin.AvCipherV128Binary.srcL);
      addHRegUse(u, HRmRead,  i->Pin.AvCipherV128Binary.srcR);
      return;
   case Pin_AvHashV128Binary:
      addHRegUse(u, HRmWrite, i->Pin.AvHashV128Binary.dst);
      addHRegUse(u, HRmRead,  i->Pin.AvHashV128Binary.src);
      addRegUsage_PPCRI(u,    i->Pin.AvHashV128Binary.s_field);
      return;
   case Pin_AvBCDV128Trinary:
      addHRegUse(u, HRmWrite, i->Pin.AvBCDV128Trinary.dst);
      addHRegUse(u, HRmRead,  i->Pin.AvBCDV128Trinary.src1);
      addHRegUse(u, HRmRead,  i->Pin.AvBCDV128Trinary.src2);
      addRegUsage_PPCRI(u,    i->Pin.AvBCDV128Trinary.ps);
      return;
   case Pin_Dfp64Unary:
      addHRegUse(u, HRmWrite, i->Pin.Dfp64Unary.dst);
      addHRegUse(u, HRmRead, i->Pin.Dfp64Unary.src);
      return;
   case Pin_Dfp64Binary:
      addHRegUse(u, HRmWrite, i->Pin.Dfp64Binary.dst);
      addHRegUse(u, HRmRead, i->Pin.Dfp64Binary.srcL);
      addHRegUse(u, HRmRead, i->Pin.Dfp64Binary.srcR);
      return;
   case Pin_DfpShift:
      addRegUsage_PPCRI(u,    i->Pin.DfpShift.shift);
      addHRegUse(u, HRmWrite, i->Pin.DfpShift.src);
      addHRegUse(u, HRmWrite, i->Pin.DfpShift.dst);
      return;
   case Pin_Dfp128Unary:
      addHRegUse(u, HRmWrite, i->Pin.Dfp128Unary.dst_hi);
      addHRegUse(u, HRmWrite, i->Pin.Dfp128Unary.dst_lo);
      addHRegUse(u, HRmRead,  i->Pin.Dfp128Unary.src_hi);
      addHRegUse(u, HRmRead,  i->Pin.Dfp128Unary.src_lo);
      return;
   case Pin_Dfp128Binary:
      addHRegUse(u, HRmWrite, i->Pin.Dfp128Binary.dst_hi);
      addHRegUse(u, HRmWrite, i->Pin.Dfp128Binary.dst_lo);
      addHRegUse(u, HRmRead, i->Pin.Dfp128Binary.srcR_hi);
      addHRegUse(u, HRmRead, i->Pin.Dfp128Binary.srcR_lo);
      return;
   case Pin_DfpRound:
      addHRegUse(u, HRmWrite, i->Pin.DfpRound.dst);
      addHRegUse(u, HRmRead,  i->Pin.DfpRound.src);
      return;
   case Pin_DfpRound128:
      addHRegUse(u, HRmWrite, i->Pin.DfpRound128.dst_hi);
      addHRegUse(u, HRmWrite, i->Pin.DfpRound128.dst_lo);
      addHRegUse(u, HRmRead,  i->Pin.DfpRound128.src_hi);
      addHRegUse(u, HRmRead,  i->Pin.DfpRound128.src_lo);
      return;
   case Pin_DfpQuantize:
      addRegUsage_PPCRI(u,  i->Pin.DfpQuantize.rmc);
      addHRegUse(u, HRmWrite, i->Pin.DfpQuantize.dst);
      addHRegUse(u, HRmRead,  i->Pin.DfpQuantize.srcL);
      addHRegUse(u, HRmRead,  i->Pin.DfpQuantize.srcR);
      return;
   case Pin_DfpQuantize128:
      addHRegUse(u, HRmWrite, i->Pin.DfpQuantize128.dst_hi);
      addHRegUse(u, HRmWrite, i->Pin.DfpQuantize128.dst_lo);
      addHRegUse(u, HRmRead,  i->Pin.DfpQuantize128.src_hi);
      addHRegUse(u, HRmRead,  i->Pin.DfpQuantize128.src_lo);
      return;
   case Pin_DfpShift128:
      addRegUsage_PPCRI(u,    i->Pin.DfpShift128.shift);
      addHRegUse(u, HRmWrite, i->Pin.DfpShift128.src_hi);
      addHRegUse(u, HRmWrite, i->Pin.DfpShift128.src_lo);
      addHRegUse(u, HRmWrite, i->Pin.DfpShift128.dst_hi);
      addHRegUse(u, HRmWrite, i->Pin.DfpShift128.dst_lo);
      return;
   case Pin_DfpD128toD64:
      addHRegUse(u, HRmWrite, i->Pin.DfpD128toD64.src_hi);
      addHRegUse(u, HRmWrite, i->Pin.DfpD128toD64.src_lo);
      addHRegUse(u, HRmWrite, i->Pin.DfpD128toD64.dst);
      return;
   case Pin_DfpI64StoD128:
      addHRegUse(u, HRmWrite, i->Pin.DfpI64StoD128.src);
      addHRegUse(u, HRmWrite, i->Pin.DfpI64StoD128.dst_hi);
      addHRegUse(u, HRmWrite, i->Pin.DfpI64StoD128.dst_lo);
      return;
   case Pin_ExtractExpD128:
      addHRegUse(u, HRmWrite, i->Pin.ExtractExpD128.dst);
      addHRegUse(u, HRmRead,  i->Pin.ExtractExpD128.src_hi);
      addHRegUse(u, HRmRead,  i->Pin.ExtractExpD128.src_lo);
      return;
   case Pin_InsertExpD128:
      addHRegUse(u, HRmWrite, i->Pin.InsertExpD128.dst_hi);
      addHRegUse(u, HRmWrite, i->Pin.InsertExpD128.dst_lo);
      addHRegUse(u, HRmRead,  i->Pin.InsertExpD128.srcL);
      addHRegUse(u, HRmRead,  i->Pin.InsertExpD128.srcR_hi);
      addHRegUse(u, HRmRead,  i->Pin.InsertExpD128.srcR_lo);
      return;
   case Pin_Dfp64Cmp:
      addHRegUse(u, HRmWrite, i->Pin.Dfp64Cmp.dst);
      addHRegUse(u, HRmRead,  i->Pin.Dfp64Cmp.srcL);
      addHRegUse(u, HRmRead,  i->Pin.Dfp64Cmp.srcR);
      return;
   case Pin_Dfp128Cmp:
      addHRegUse(u, HRmWrite, i->Pin.Dfp128Cmp.dst);
      addHRegUse(u, HRmRead,  i->Pin.Dfp128Cmp.srcL_hi);
      addHRegUse(u, HRmRead,  i->Pin.Dfp128Cmp.srcL_lo);
      addHRegUse(u, HRmRead,  i->Pin.Dfp128Cmp.srcR_hi);
      addHRegUse(u, HRmRead,  i->Pin.Dfp128Cmp.srcR_lo);
      return;                                           
   case Pin_EvCheck:
      addRegUsage_PPCAMode(u, i->Pin.EvCheck.amCounter);
      addRegUsage_PPCAMode(u, i->Pin.EvCheck.amFailAddr);
      addHRegUse(u, HRmWrite, hregPPC_GPR30(mode64)); 
      return;
   case Pin_ProfInc:
      addHRegUse(u, HRmWrite, hregPPC_GPR29(mode64));
      addHRegUse(u, HRmWrite, hregPPC_GPR30(mode64));
      return;
   default:
      ppPPCInstr(i, mode64);
      vpanic("getRegUsage_PPCInstr");
   }
}

static void mapReg( HRegRemap* m, HReg* r )
{
   *r = lookupHRegRemap(m, *r);
}

void mapRegs_PPCInstr ( HRegRemap* m, PPCInstr* i, Bool mode64 )
{
   switch (i->tag) {
   case Pin_LI:
      mapReg(m, &i->Pin.LI.dst);
      return;
   case Pin_Alu:
      mapReg(m, &i->Pin.Alu.dst);
      mapReg(m, &i->Pin.Alu.srcL);
      mapRegs_PPCRH(m, i->Pin.Alu.srcR);
      return;
   case Pin_Shft:
      mapReg(m, &i->Pin.Shft.dst);
      mapReg(m, &i->Pin.Shft.srcL);
      mapRegs_PPCRH(m, i->Pin.Shft.srcR);
      return;
   case Pin_AddSubC:
      mapReg(m, &i->Pin.AddSubC.dst);
      mapReg(m, &i->Pin.AddSubC.srcL);
      mapReg(m, &i->Pin.AddSubC.srcR);
      return;
   case Pin_Cmp:
      mapReg(m, &i->Pin.Cmp.srcL);
      mapRegs_PPCRH(m, i->Pin.Cmp.srcR);
      return;
   case Pin_Unary:
      mapReg(m, &i->Pin.Unary.dst);
      mapReg(m, &i->Pin.Unary.src);
      return;
   case Pin_MulL:
      mapReg(m, &i->Pin.MulL.dst);
      mapReg(m, &i->Pin.MulL.srcL);
      mapReg(m, &i->Pin.MulL.srcR);
      return;
   case Pin_Div:
      mapReg(m, &i->Pin.Div.dst);
      mapReg(m, &i->Pin.Div.srcL);
      mapReg(m, &i->Pin.Div.srcR);
      return;
   case Pin_Call:
      return;
   case Pin_XDirect:
      mapRegs_PPCAMode(m, i->Pin.XDirect.amCIA);
      return;
   case Pin_XIndir:
      mapReg(m, &i->Pin.XIndir.dstGA);
      mapRegs_PPCAMode(m, i->Pin.XIndir.amCIA);
      return;
   case Pin_XAssisted:
      mapReg(m, &i->Pin.XAssisted.dstGA);
      mapRegs_PPCAMode(m, i->Pin.XAssisted.amCIA);
      return;
   case Pin_CMov:
      mapRegs_PPCRI(m, i->Pin.CMov.src);
      mapReg(m, &i->Pin.CMov.dst);
      return;
   case Pin_Load:
      mapRegs_PPCAMode(m, i->Pin.Load.src);
      mapReg(m, &i->Pin.Load.dst);
      return;
   case Pin_LoadL:
      mapReg(m, &i->Pin.LoadL.src);
      mapReg(m, &i->Pin.LoadL.dst);
      return;
   case Pin_Store:
      mapReg(m, &i->Pin.Store.src);
      mapRegs_PPCAMode(m, i->Pin.Store.dst);
      return;
   case Pin_StoreC:
      mapReg(m, &i->Pin.StoreC.src);
      mapReg(m, &i->Pin.StoreC.dst);
      return;
   case Pin_Set:
      mapReg(m, &i->Pin.Set.dst);
      return;
   case Pin_MfCR:
      mapReg(m, &i->Pin.MfCR.dst);
      return;
   case Pin_MFence:
      return;
   case Pin_FpUnary:
      mapReg(m, &i->Pin.FpUnary.dst);
      mapReg(m, &i->Pin.FpUnary.src);
      return;
   case Pin_FpBinary:
      mapReg(m, &i->Pin.FpBinary.dst);
      mapReg(m, &i->Pin.FpBinary.srcL);
      mapReg(m, &i->Pin.FpBinary.srcR);
      return;
   case Pin_FpMulAcc:
      mapReg(m, &i->Pin.FpMulAcc.dst);
      mapReg(m, &i->Pin.FpMulAcc.srcML);
      mapReg(m, &i->Pin.FpMulAcc.srcMR);
      mapReg(m, &i->Pin.FpMulAcc.srcAcc);
      return;
   case Pin_FpLdSt:
      mapReg(m, &i->Pin.FpLdSt.reg);
      mapRegs_PPCAMode(m, i->Pin.FpLdSt.addr);
      return;
   case Pin_FpSTFIW:
      mapReg(m, &i->Pin.FpSTFIW.addr);
      mapReg(m, &i->Pin.FpSTFIW.data);
      return;
   case Pin_FpRSP:
      mapReg(m, &i->Pin.FpRSP.dst);
      mapReg(m, &i->Pin.FpRSP.src);
      return;
   case Pin_FpCftI:
      mapReg(m, &i->Pin.FpCftI.dst);
      mapReg(m, &i->Pin.FpCftI.src);
      return;
   case Pin_FpCMov:
      mapReg(m, &i->Pin.FpCMov.dst);
      mapReg(m, &i->Pin.FpCMov.src);
      return;
   case Pin_FpLdFPSCR:
      mapReg(m, &i->Pin.FpLdFPSCR.src);
      return;
   case Pin_FpCmp:
      mapReg(m, &i->Pin.FpCmp.dst);
      mapReg(m, &i->Pin.FpCmp.srcL);
      mapReg(m, &i->Pin.FpCmp.srcR);
      return;
   case Pin_RdWrLR:
      mapReg(m, &i->Pin.RdWrLR.gpr);
      return;
   case Pin_AvLdSt:
      mapReg(m, &i->Pin.AvLdSt.reg);
      mapRegs_PPCAMode(m, i->Pin.AvLdSt.addr);
      return;
   case Pin_AvUnary:
      mapReg(m, &i->Pin.AvUnary.dst);
      mapReg(m, &i->Pin.AvUnary.src);
      return;
   case Pin_AvBinary:
      mapReg(m, &i->Pin.AvBinary.dst);
      mapReg(m, &i->Pin.AvBinary.srcL);
      mapReg(m, &i->Pin.AvBinary.srcR);
      return;
   case Pin_AvBin8x16:
      mapReg(m, &i->Pin.AvBin8x16.dst);
      mapReg(m, &i->Pin.AvBin8x16.srcL);
      mapReg(m, &i->Pin.AvBin8x16.srcR);
      return;
   case Pin_AvBin16x8:
      mapReg(m, &i->Pin.AvBin16x8.dst);
      mapReg(m, &i->Pin.AvBin16x8.srcL);
      mapReg(m, &i->Pin.AvBin16x8.srcR);
      return;
   case Pin_AvBin32x4:
      mapReg(m, &i->Pin.AvBin32x4.dst);
      mapReg(m, &i->Pin.AvBin32x4.srcL);
      mapReg(m, &i->Pin.AvBin32x4.srcR);
      return;
   case Pin_AvBin64x2:
      mapReg(m, &i->Pin.AvBin64x2.dst);
      mapReg(m, &i->Pin.AvBin64x2.srcL);
      mapReg(m, &i->Pin.AvBin64x2.srcR);
      return;
   case Pin_AvBin32Fx4:
      mapReg(m, &i->Pin.AvBin32Fx4.dst);
      mapReg(m, &i->Pin.AvBin32Fx4.srcL);
      mapReg(m, &i->Pin.AvBin32Fx4.srcR);
      return;
   case Pin_AvUn32Fx4:
      mapReg(m, &i->Pin.AvUn32Fx4.dst);
      mapReg(m, &i->Pin.AvUn32Fx4.src);
      return;
   case Pin_AvPerm:
      mapReg(m, &i->Pin.AvPerm.dst);
      mapReg(m, &i->Pin.AvPerm.srcL);
      mapReg(m, &i->Pin.AvPerm.srcR);
      mapReg(m, &i->Pin.AvPerm.ctl);
      return;
   case Pin_AvSel:
      mapReg(m, &i->Pin.AvSel.dst);
      mapReg(m, &i->Pin.AvSel.srcL);
      mapReg(m, &i->Pin.AvSel.srcR);
      mapReg(m, &i->Pin.AvSel.ctl);
      return;
   case Pin_AvSh:
      mapReg(m, &i->Pin.AvSh.dst);
      mapRegs_PPCAMode(m, i->Pin.AvSh.addr);
      return;
   case Pin_AvShlDbl:
      mapReg(m, &i->Pin.AvShlDbl.dst);
      mapReg(m, &i->Pin.AvShlDbl.srcL);
      mapReg(m, &i->Pin.AvShlDbl.srcR);
      return;
   case Pin_AvSplat:
      mapReg(m, &i->Pin.AvSplat.dst);
      mapRegs_PPCVI5s(m, i->Pin.AvSplat.src);
      return;
   case Pin_AvCMov:
     mapReg(m, &i->Pin.AvCMov.dst);
     mapReg(m, &i->Pin.AvCMov.src);
     return;
   case Pin_AvLdVSCR:
      mapReg(m, &i->Pin.AvLdVSCR.src);
      return;
   case Pin_AvCipherV128Unary:
      mapReg(m, &i->Pin.AvCipherV128Unary.dst);
      mapReg(m, &i->Pin.AvCipherV128Unary.src);
      return;
   case Pin_AvCipherV128Binary:
      mapReg(m, &i->Pin.AvCipherV128Binary.dst);
      mapReg(m, &i->Pin.AvCipherV128Binary.srcL);
      mapReg(m, &i->Pin.AvCipherV128Binary.srcR);
      return;
   case Pin_AvHashV128Binary:
      mapRegs_PPCRI(m, i->Pin.AvHashV128Binary.s_field);
      mapReg(m, &i->Pin.AvHashV128Binary.dst);
      mapReg(m, &i->Pin.AvHashV128Binary.src);
      return;
   case Pin_AvBCDV128Trinary:
      mapReg(m, &i->Pin.AvBCDV128Trinary.dst);
      mapReg(m, &i->Pin.AvBCDV128Trinary.src1);
      mapReg(m, &i->Pin.AvBCDV128Trinary.src2);
      mapRegs_PPCRI(m, i->Pin.AvBCDV128Trinary.ps);
      return;
   case Pin_Dfp64Unary:
      mapReg(m, &i->Pin.Dfp64Unary.dst);
      mapReg(m, &i->Pin.Dfp64Unary.src);
      return;
   case Pin_Dfp64Binary:
      mapReg(m, &i->Pin.Dfp64Binary.dst);
      mapReg(m, &i->Pin.Dfp64Binary.srcL);
      mapReg(m, &i->Pin.Dfp64Binary.srcR);
      return;
   case Pin_DfpShift:
      mapRegs_PPCRI(m, i->Pin.DfpShift.shift);
      mapReg(m, &i->Pin.DfpShift.src);
      mapReg(m, &i->Pin.DfpShift.dst);
      return;
   case Pin_Dfp128Unary:
      mapReg(m, &i->Pin.Dfp128Unary.dst_hi);
      mapReg(m, &i->Pin.Dfp128Unary.dst_lo);
      mapReg(m, &i->Pin.Dfp128Unary.src_hi);
      mapReg(m, &i->Pin.Dfp128Unary.src_lo);
     return;
   case Pin_Dfp128Binary:
      mapReg(m, &i->Pin.Dfp128Binary.dst_hi);
      mapReg(m, &i->Pin.Dfp128Binary.dst_lo);
      mapReg(m, &i->Pin.Dfp128Binary.srcR_hi);
      mapReg(m, &i->Pin.Dfp128Binary.srcR_lo);
      return;
   case Pin_DfpShift128:
      mapRegs_PPCRI(m, i->Pin.DfpShift128.shift);
      mapReg(m, &i->Pin.DfpShift128.src_hi);
      mapReg(m, &i->Pin.DfpShift128.src_lo);
      mapReg(m, &i->Pin.DfpShift128.dst_hi);
      mapReg(m, &i->Pin.DfpShift128.dst_lo);
      return;
   case Pin_DfpRound:
      mapReg(m, &i->Pin.DfpRound.dst);
      mapReg(m, &i->Pin.DfpRound.src);
      return;
   case Pin_DfpRound128:
      mapReg(m, &i->Pin.DfpRound128.dst_hi);
      mapReg(m, &i->Pin.DfpRound128.dst_lo);
      mapReg(m, &i->Pin.DfpRound128.src_hi);
      mapReg(m, &i->Pin.DfpRound128.src_lo);
      return;
   case Pin_DfpQuantize:
      mapRegs_PPCRI(m, i->Pin.DfpQuantize.rmc);
      mapReg(m, &i->Pin.DfpQuantize.dst);
      mapReg(m, &i->Pin.DfpQuantize.srcL);
      mapReg(m, &i->Pin.DfpQuantize.srcR);
      return;
   case Pin_DfpQuantize128:
      mapRegs_PPCRI(m, i->Pin.DfpQuantize128.rmc);
      mapReg(m, &i->Pin.DfpQuantize128.dst_hi);
      mapReg(m, &i->Pin.DfpQuantize128.dst_lo);
      mapReg(m, &i->Pin.DfpQuantize128.src_hi);
      mapReg(m, &i->Pin.DfpQuantize128.src_lo);
      return;
   case Pin_DfpD128toD64:
      mapReg(m, &i->Pin.DfpD128toD64.src_hi);
      mapReg(m, &i->Pin.DfpD128toD64.src_lo);
      mapReg(m, &i->Pin.DfpD128toD64.dst);
      return;
   case Pin_DfpI64StoD128:
      mapReg(m, &i->Pin.DfpI64StoD128.src);
      mapReg(m, &i->Pin.DfpI64StoD128.dst_hi);
      mapReg(m, &i->Pin.DfpI64StoD128.dst_lo);
      return;
   case Pin_ExtractExpD128:
      mapReg(m, &i->Pin.ExtractExpD128.dst);
      mapReg(m, &i->Pin.ExtractExpD128.src_hi);
      mapReg(m, &i->Pin.ExtractExpD128.src_lo);
      return;
   case Pin_InsertExpD128:
      mapReg(m, &i->Pin.InsertExpD128.dst_hi);
      mapReg(m, &i->Pin.InsertExpD128.dst_lo);
      mapReg(m, &i->Pin.InsertExpD128.srcL);
      mapReg(m, &i->Pin.InsertExpD128.srcR_hi);
      mapReg(m, &i->Pin.InsertExpD128.srcR_lo);
      return;
   case Pin_Dfp64Cmp:
      mapReg(m, &i->Pin.Dfp64Cmp.dst);
      mapReg(m, &i->Pin.Dfp64Cmp.srcL);
      mapReg(m, &i->Pin.Dfp64Cmp.srcR);
      return;
   case Pin_Dfp128Cmp:
      mapReg(m, &i->Pin.Dfp128Cmp.dst);
      mapReg(m, &i->Pin.Dfp128Cmp.srcL_hi);
      mapReg(m, &i->Pin.Dfp128Cmp.srcL_lo);
      mapReg(m, &i->Pin.Dfp128Cmp.srcR_hi);
      mapReg(m, &i->Pin.Dfp128Cmp.srcR_lo);
      return;
   case Pin_EvCheck:
      mapRegs_PPCAMode(m, i->Pin.EvCheck.amCounter);
      mapRegs_PPCAMode(m, i->Pin.EvCheck.amFailAddr);
      return;
   case Pin_ProfInc:
      
      return;
   default:
      ppPPCInstr(i, mode64);
      vpanic("mapRegs_PPCInstr");
   }
}

Bool isMove_PPCInstr ( const PPCInstr* i, HReg* src, HReg* dst )
{
   
   if (i->tag == Pin_Alu) {
      
      if (i->Pin.Alu.op != Palu_OR)
         return False;
      if (i->Pin.Alu.srcR->tag != Prh_Reg)
         return False;
      if (! sameHReg(i->Pin.Alu.srcR->Prh.Reg.reg, i->Pin.Alu.srcL))
         return False;
      *src = i->Pin.Alu.srcL;
      *dst = i->Pin.Alu.dst;
      return True;
   }
   
   if (i->tag == Pin_FpUnary) {
      if (i->Pin.FpUnary.op != Pfp_MOV)
         return False;
      *src = i->Pin.FpUnary.src;
      *dst = i->Pin.FpUnary.dst;
      return True;
   }
   return False;
}



void genSpill_PPC ( HInstr** i1, HInstr** i2,
                    HReg rreg, Int offsetB, Bool mode64 )
{
   PPCAMode* am;
   vassert(!hregIsVirtual(rreg));
   *i1 = *i2 = NULL;
   am = PPCAMode_IR( offsetB, GuestStatePtr(mode64) );
   switch (hregClass(rreg)) {
      case HRcInt64:
         vassert(mode64);
         *i1 = PPCInstr_Store( 8, am, rreg, mode64 );
         return;
      case HRcInt32:
         vassert(!mode64);
         *i1 = PPCInstr_Store( 4, am, rreg, mode64 );
         return;
      case HRcFlt64:
         *i1 = PPCInstr_FpLdSt ( False, 8, rreg, am );
         return;
      case HRcVec128:
         
         
         *i1 = PPCInstr_AvLdSt ( False, 16, rreg, am );
         return;
      default: 
         ppHRegClass(hregClass(rreg));
         vpanic("genSpill_PPC: unimplemented regclass");
   }
}

void genReload_PPC ( HInstr** i1, HInstr** i2,
                     HReg rreg, Int offsetB, Bool mode64 )
{
   PPCAMode* am;
   vassert(!hregIsVirtual(rreg));
   *i1 = *i2 = NULL;
   am = PPCAMode_IR( offsetB, GuestStatePtr(mode64) );
   switch (hregClass(rreg)) {
      case HRcInt64:
         vassert(mode64);
         *i1 = PPCInstr_Load( 8, rreg, am, mode64 );
         return;
      case HRcInt32:
         vassert(!mode64);
         *i1 = PPCInstr_Load( 4, rreg, am, mode64 );
         return;
      case HRcFlt64:
         *i1 = PPCInstr_FpLdSt ( True, 8, rreg, am );
         return;
      case HRcVec128:
         
         *i1 = PPCInstr_AvLdSt ( True, 16, rreg, am );
         return;
      default: 
         ppHRegClass(hregClass(rreg));
         vpanic("genReload_PPC: unimplemented regclass");
   }
}



inline static UInt iregEnc ( HReg r, Bool mode64 )
{
   UInt n;
   vassert(hregClass(r) == (mode64 ? HRcInt64 : HRcInt32));
   vassert(!hregIsVirtual(r));
   n = hregEncoding(r);
   vassert(n <= 32);
   return n;
}

inline static UInt fregEnc ( HReg fr )
{
   UInt n;
   vassert(hregClass(fr) == HRcFlt64);
   vassert(!hregIsVirtual(fr));
   n = hregEncoding(fr);
   vassert(n <= 32);
   return n;
}

inline static UInt vregEnc ( HReg v )
{
   UInt n;
   vassert(hregClass(v) == HRcVec128);
   vassert(!hregIsVirtual(v));
   n = hregEncoding(v);
   vassert(n <= 32);
   return n;
}

static UChar* emit32 ( UChar* p, UInt w32, VexEndness endness_host )
{
  if (endness_host == VexEndnessBE) {
    *p++ = toUChar((w32 >> 24) & 0x000000FF);
    *p++ = toUChar((w32 >> 16) & 0x000000FF);
    *p++ = toUChar((w32 >>  8) & 0x000000FF);
    *p++ = toUChar((w32)       & 0x000000FF);
  } else {
    *p++ = toUChar((w32)       & 0x000000FF);
    *p++ = toUChar((w32 >>  8) & 0x000000FF);
    *p++ = toUChar((w32 >> 16) & 0x000000FF);
    *p++ = toUChar((w32 >> 24) & 0x000000FF);
  }
   return p;
}

static UInt fetch32 ( UChar* p, VexEndness endness_host )
{
   UInt w32 = 0;
   if (endness_host == VexEndnessBE) {
      w32 |= ((0xFF & (UInt)p[0]) << 24);
      w32 |= ((0xFF & (UInt)p[1]) << 16);
      w32 |= ((0xFF & (UInt)p[2]) <<  8);
      w32 |= ((0xFF & (UInt)p[3]) <<  0);
  } else {
      w32 |= ((0xFF & (UInt)p[3]) << 24);
      w32 |= ((0xFF & (UInt)p[2]) << 16);
      w32 |= ((0xFF & (UInt)p[1]) <<  8);
      w32 |= ((0xFF & (UInt)p[0]) <<  0);
  }
   return w32;
}


static UChar* mkFormD ( UChar* p, UInt opc1,
                        UInt r1, UInt r2, UInt imm, VexEndness endness_host )
{
   UInt theInstr;
   vassert(opc1 < 0x40);
   vassert(r1   < 0x20);
   vassert(r2   < 0x20);
   imm = imm & 0xFFFF;
   theInstr = ((opc1<<26) | (r1<<21) | (r2<<16) | (imm));
   return emit32(p, theInstr, endness_host);
}

static UChar* mkFormMD ( UChar* p, UInt opc1, UInt r1, UInt r2,
                         UInt imm1, UInt imm2, UInt opc2,
                         VexEndness endness_host )
{
   UInt theInstr;
   vassert(opc1 < 0x40);
   vassert(r1   < 0x20);
   vassert(r2   < 0x20);
   vassert(imm1 < 0x40);
   vassert(imm2 < 0x40);
   vassert(opc2 < 0x08);
   imm2 = ((imm2 & 0x1F) << 1) | (imm2 >> 5);
   theInstr = ((opc1<<26) | (r1<<21) | (r2<<16) |
               ((imm1 & 0x1F)<<11) | (imm2<<5) |
               (opc2<<2) | ((imm1 >> 5)<<1));
   return emit32(p, theInstr, endness_host);
}

static UChar* mkFormX ( UChar* p, UInt opc1, UInt r1, UInt r2,
                        UInt r3, UInt opc2, UInt b0, VexEndness endness_host )
{
   UInt theInstr;
   vassert(opc1 < 0x40);
   vassert(r1   < 0x20);
   vassert(r2   < 0x20);
   vassert(r3   < 0x20);
   vassert(opc2 < 0x400);
   vassert(b0   < 0x2);
   theInstr = ((opc1<<26) | (r1<<21) | (r2<<16) |
               (r3<<11) | (opc2<<1) | (b0));
   return emit32(p, theInstr, endness_host);
}

static UChar* mkFormXO ( UChar* p, UInt opc1, UInt r1, UInt r2,
                         UInt r3, UInt b10, UInt opc2, UInt b0,
                         VexEndness endness_host )
{
   UInt theInstr;
   vassert(opc1 < 0x40);
   vassert(r1   < 0x20);
   vassert(r2   < 0x20);
   vassert(r3   < 0x20);
   vassert(b10  < 0x2);
   vassert(opc2 < 0x200);
   vassert(b0   < 0x2);
   theInstr = ((opc1<<26) | (r1<<21) | (r2<<16) |
               (r3<<11) | (b10 << 10) | (opc2<<1) | (b0));
   return emit32(p, theInstr, endness_host);
}

static UChar* mkFormXL ( UChar* p, UInt opc1, UInt f1, UInt f2,
                         UInt f3, UInt opc2, UInt b0, VexEndness endness_host )
{
   UInt theInstr;
   vassert(opc1 < 0x40);
   vassert(f1   < 0x20);
   vassert(f2   < 0x20);
   vassert(f3   < 0x20);
   vassert(opc2 < 0x400);
   vassert(b0   < 0x2);
   theInstr = ((opc1<<26) | (f1<<21) | (f2<<16) |
               (f3<<11) | (opc2<<1) | (b0));
   return emit32(p, theInstr, endness_host);
}

static UChar* mkFormXFX ( UChar* p, UInt r1, UInt f2, UInt opc2,
                          VexEndness endness_host )
{
   UInt theInstr;
   vassert(r1   < 0x20);
   vassert(f2   < 0x20);
   vassert(opc2 < 0x400);
   switch (opc2) {
   case 144:  
      vassert(f2 < 0x100);
      f2 = f2 << 1;
      break;
   case 339:  
   case 371:  
   case 467:  
      vassert(f2 < 0x400);
      
      f2 = ((f2>>5) & 0x1F) | ((f2 & 0x1F)<<5);
      break;
   default: vpanic("mkFormXFX(ppch)");
   }
   theInstr = ((31<<26) | (r1<<21) | (f2<<11) | (opc2<<1));
   return emit32(p, theInstr, endness_host);
}

static UChar* mkFormXFL ( UChar* p, UInt FM, UInt freg, UInt dfp_rm,
                          VexEndness endness_host )
{
   UInt theInstr;
   vassert(FM   < 0x100);
   vassert(freg < 0x20);
   theInstr = ((63<<26) | (FM<<17) | (dfp_rm<<16) | (freg<<11) | (711<<1));
   return emit32(p, theInstr, endness_host);
}

static UChar* mkFormXS ( UChar* p, UInt opc1, UInt r1, UInt r2,
                         UInt imm, UInt opc2, UInt b0,
                         VexEndness endness_host )
{
   UInt theInstr;
   vassert(opc1 < 0x40);
   vassert(r1   < 0x20);
   vassert(r2   < 0x20);
   vassert(imm  < 0x40);
   vassert(opc2 < 0x400);
   vassert(b0   < 0x2);
   theInstr = ((opc1<<26) | (r1<<21) | (r2<<16) |
               ((imm & 0x1F)<<11) | (opc2<<2) | ((imm>>5)<<1) | (b0));
   return emit32(p, theInstr, endness_host);
}


#if 0
static UChar* mkFormI ( UChar* p, UInt LI, UInt AA, UInt LK,
                        VexEndness endness_host )
{
   UInt theInstr;
   vassert(LI  < 0x1000000);
   vassert(AA  < 0x2);
   vassert(LK  < 0x2);
   theInstr = ((18<<26) | (LI<<2) | (AA<<1) | (LK));
   return emit32(p, theInstr, endness_host);
}
#endif

static UChar* mkFormB ( UChar* p, UInt BO, UInt BI,
                        UInt BD, UInt AA, UInt LK, VexEndness endness_host )
{
   UInt theInstr;
   vassert(BO  < 0x20);
   vassert(BI  < 0x20);
   vassert(BD  < 0x4000);
   vassert(AA  < 0x2);
   vassert(LK  < 0x2);
   theInstr = ((16<<26) | (BO<<21) | (BI<<16) |
               (BD<<2) | (AA<<1) | (LK));
   return emit32(p, theInstr, endness_host);
}

static UChar* mkFormM ( UChar* p, UInt opc1, UInt r1, UInt r2,
                        UInt f3, UInt MB, UInt ME, UInt Rc,
                        VexEndness endness_host )
{
   UInt theInstr;
   vassert(opc1 < 0x40);
   vassert(r1   < 0x20);
   vassert(r2   < 0x20);
   vassert(f3   < 0x20);
   vassert(MB   < 0x20);
   vassert(ME   < 0x20);
   vassert(Rc   < 0x2);
   theInstr = ((opc1<<26) | (r1<<21) | (r2<<16) |
               (f3<<11) | (MB<<6) | (ME<<1) | (Rc));
   return emit32(p, theInstr, endness_host);
}

static UChar* mkFormA ( UChar* p, UInt opc1, UInt r1, UInt r2,
                        UInt r3, UInt r4, UInt opc2, UInt b0,
                        VexEndness endness_host )
{
   UInt theInstr;
   vassert(opc1 < 0x40);
   vassert(r1   < 0x20);
   vassert(r2   < 0x20);
   vassert(r3   < 0x20);
   vassert(r4   < 0x20);
   vassert(opc2 < 0x20);
   vassert(b0   < 0x2 );
   theInstr = ((opc1<<26) | (r1<<21) | (r2<<16) | (r3<<11) |
               (r4<<6) | (opc2<<1) | (b0));
   return emit32(p, theInstr, endness_host);
}

static UChar* mkFormZ22 ( UChar* p, UInt opc1, UInt r1, UInt r2,
                          UInt constant, UInt opc2, UInt b0,
                          VexEndness endness_host)
{
   UInt theInstr;
   vassert(opc1     < 0x40);
   vassert(r1       < 0x20);
   vassert(r2       < 0x20);
   vassert(constant < 0x40);   
   vassert(opc2     < 0x200);  
   vassert(b0       < 0x2);
   theInstr = ((opc1<<26) | (r1<<21) | (r2<<16) |
               (constant<<10) | (opc2<<1) | (b0));
   return emit32(p, theInstr, endness_host);
}

static UChar* mkFormZ23 ( UChar* p, UInt opc1, UInt r1, UInt r2,
                          UInt r3, UInt rmc, UInt opc2, UInt b0,
                          VexEndness endness_host)
{
   UInt theInstr;
   vassert(opc1 < 0x40);
   vassert(r1   < 0x20);
   vassert(r2   < 0x20);
   vassert(r3   < 0x20);
   vassert(rmc  < 0x4);
   vassert(opc2 < 0x100);
   vassert(b0   < 0x2);
   theInstr = ((opc1<<26) | (r1<<21) | (r2<<16) |
               (r3<<11) | (rmc<<9) | (opc2<<1) | (b0));
   return emit32(p, theInstr, endness_host);
}

static UChar* doAMode_IR ( UChar* p, UInt opc1, UInt rSD,
                           PPCAMode* am, Bool mode64, VexEndness endness_host )
{
   UInt rA, idx;
   vassert(am->tag == Pam_IR);
   vassert(am->Pam.IR.index < 0x10000);

   rA  = iregEnc(am->Pam.IR.base, mode64);
   idx = am->Pam.IR.index;

   if (opc1 == 58 || opc1 == 62) { 
      vassert(mode64);
      vassert(0 == (idx & 3));
   }
   p = mkFormD(p, opc1, rSD, rA, idx, endness_host);
   return p;
}

static UChar* doAMode_RR ( UChar* p, UInt opc1, UInt opc2,
                           UInt rSD, PPCAMode* am, Bool mode64,
                           VexEndness endness_host )
{
   UInt rA, rB;
   vassert(am->tag == Pam_RR);

   rA  = iregEnc(am->Pam.RR.base, mode64);
   rB  = iregEnc(am->Pam.RR.index, mode64);
   
   p = mkFormX(p, opc1, rSD, rA, rB, opc2, 0, endness_host);
   return p;
}


static UChar* mkLoadImm ( UChar* p, UInt r_dst, ULong imm, Bool mode64,
                          VexEndness endness_host )
{
   vassert(r_dst < 0x20);

   if (!mode64) {
      UInt u32 = (UInt)imm;
      Int  s32 = (Int)u32;
      Long s64 = (Long)s32;
      imm = (ULong)s64;
   }

   if (imm >= 0xFFFFFFFFFFFF8000ULL || imm < 0x8000) {
      

      
      p = mkFormD(p, 14, r_dst, 0, imm & 0xFFFF, endness_host);
   } else {
      if (imm >= 0xFFFFFFFF80000000ULL || imm < 0x80000000ULL) {
         

         
         p = mkFormD(p, 15, r_dst, 0, (imm>>16) & 0xFFFF, endness_host);
         
         p = mkFormD(p, 24, r_dst, r_dst, imm & 0xFFFF, endness_host);
      } else {
         
         vassert(mode64);

         

         
         p = mkFormD(p, 15, r_dst, 0, (imm>>48) & 0xFFFF, endness_host);

         
         if ((imm>>32) & 0xFFFF)
	   p = mkFormD(p, 24, r_dst, r_dst, (imm>>32) & 0xFFFF, endness_host);
         
         
         p = mkFormMD(p, 30, r_dst, r_dst, 32, 31, 1, endness_host);

         

         
         if ((imm>>16) & 0xFFFF)
            p = mkFormD(p, 25, r_dst, r_dst, (imm>>16) & 0xFFFF, endness_host);

         
         if (imm & 0xFFFF)
            p = mkFormD(p, 24, r_dst, r_dst, imm & 0xFFFF, endness_host);
      }
   }
   return p;
}

static UChar* mkLoadImm_EXACTLY2or5 ( UChar* p,
                                      UInt r_dst, ULong imm, Bool mode64,
                                      VexEndness endness_host )
{
   vassert(r_dst < 0x20);

   if (!mode64) {
      UInt u32 = (UInt)imm;
      Int  s32 = (Int)u32;
      Long s64 = (Long)s32;
      imm = (ULong)s64;
   }

   if (!mode64) {
      
      p = mkFormD(p, 15, r_dst, 0, (imm>>16) & 0xFFFF, endness_host);
      
      p = mkFormD(p, 24, r_dst, r_dst, imm & 0xFFFF, endness_host);

   } else {
      

      
      
      p = mkFormD(p, 15, r_dst, 0, (imm>>48) & 0xFFFF, endness_host);

      
      p = mkFormD(p, 24, r_dst, r_dst, (imm>>32) & 0xFFFF, endness_host);
         
      
      p = mkFormMD(p, 30, r_dst, r_dst, 32, 31, 1, endness_host);

      
      
      p = mkFormD(p, 25, r_dst, r_dst, (imm>>16) & 0xFFFF, endness_host);

      
      p = mkFormD(p, 24, r_dst, r_dst, imm & 0xFFFF, endness_host);
   }
   return p;
}

static Bool isLoadImm_EXACTLY2or5 ( UChar* p_to_check,
                                    UInt r_dst, ULong imm, Bool mode64,
                                    VexEndness endness_host )
{
   vassert(r_dst < 0x20);

   if (!mode64) {
      UInt u32 = (UInt)imm;
      Int  s32 = (Int)u32;
      Long s64 = (Long)s32;
      imm = (ULong)s64;
   }

   if (!mode64) {
      UInt   expect[2] = { 0, 0 };
      UChar* p         = (UChar*)&expect[0];
      
      p = mkFormD(p, 15, r_dst, 0, (imm>>16) & 0xFFFF, endness_host);
      
      p = mkFormD(p, 24, r_dst, r_dst, imm & 0xFFFF, endness_host);
      vassert(p == (UChar*)&expect[2]);

      return fetch32(p_to_check + 0, endness_host) == expect[0]
             && fetch32(p_to_check + 4, endness_host) == expect[1];

   } else {
      UInt   expect[5] = { 0, 0, 0, 0, 0 };
      UChar* p         = (UChar*)&expect[0];
      

      
      
      p = mkFormD(p, 15, r_dst, 0, (imm>>48) & 0xFFFF, endness_host);

      
      p = mkFormD(p, 24, r_dst, r_dst, (imm>>32) & 0xFFFF, endness_host);
         
      
      p = mkFormMD(p, 30, r_dst, r_dst, 32, 31, 1, endness_host);

      
      
      p = mkFormD(p, 25, r_dst, r_dst, (imm>>16) & 0xFFFF, endness_host);

      
      p = mkFormD(p, 24, r_dst, r_dst, imm & 0xFFFF, endness_host);

      vassert(p == (UChar*)&expect[5]);

      return fetch32(p_to_check + 0, endness_host) == expect[0]
             && fetch32(p_to_check + 4,  endness_host) == expect[1]
             && fetch32(p_to_check + 8,  endness_host) == expect[2]
             && fetch32(p_to_check + 12, endness_host) == expect[3]
             && fetch32(p_to_check + 16, endness_host) == expect[4];
   }
}


static UChar* do_load_or_store_machine_word ( 
                 UChar* p, Bool isLoad,
                 UInt reg, PPCAMode* am, Bool mode64, VexEndness endness_host )
{
   if (isLoad) {
      UInt opc1, sz = mode64 ? 8 : 4;
      switch (am->tag) {
         case Pam_IR:
            if (mode64) {
               vassert(0 == (am->Pam.IR.index & 3));
            }
            switch (sz) {
               case 4:  opc1 = 32; vassert(!mode64); break;
               case 8:  opc1 = 58; vassert(mode64);  break;
               default: vassert(0);
            }
            p = doAMode_IR(p, opc1, reg, am, mode64, endness_host);
            break;
         case Pam_RR:
            vassert(0);
         default:
            vassert(0);
      }
   } else  {
      UInt opc1, sz = mode64 ? 8 : 4;
      switch (am->tag) {
         case Pam_IR:
            if (mode64) {
               vassert(0 == (am->Pam.IR.index & 3));
            }
            switch (sz) {
               case 4:  opc1 = 36; vassert(!mode64); break;
               case 8:  opc1 = 62; vassert(mode64);  break;
               default: vassert(0);
            }
            p = doAMode_IR(p, opc1, reg, am, mode64, endness_host);
            break;
         case Pam_RR:
            vassert(0);
         default:
            vassert(0);
      }
   }
   return p;
}

static UChar* do_load_or_store_word32 ( 
                 UChar* p, Bool isLoad,
                 UInt reg, PPCAMode* am, Bool mode64, VexEndness endness_host )
{
   if (isLoad) {
      UInt opc1;
      switch (am->tag) {
         case Pam_IR:
            if (mode64) {
               vassert(0 == (am->Pam.IR.index & 3));
            }
            opc1 = 32;
            p = doAMode_IR(p, opc1, reg, am, mode64, endness_host);
            break;
         case Pam_RR:
            vassert(0);
         default:
            vassert(0);
      }
   } else  {
      UInt opc1;
      switch (am->tag) {
         case Pam_IR:
            if (mode64) {
               vassert(0 == (am->Pam.IR.index & 3));
            }
            opc1 = 36;
            p = doAMode_IR(p, opc1, reg, am, mode64, endness_host);
            break;
         case Pam_RR:
            vassert(0);
         default:
            vassert(0);
      }
   }
   return p;
}

static UChar* mkMoveReg ( UChar* p, UInt r_dst, UInt r_src,
                          VexEndness endness_host )
{
   vassert(r_dst < 0x20);
   vassert(r_src < 0x20);

   if (r_dst != r_src) {
      
      p = mkFormX(p, 31, r_src, r_dst, r_src, 444, 0, endness_host );
   }
   return p;
}

static UChar* mkFormVX ( UChar* p, UInt opc1, UInt r1, UInt r2,
                         UInt r3, UInt opc2, VexEndness endness_host )
{
   UInt theInstr;
   vassert(opc1 < 0x40);
   vassert(r1   < 0x20);
   vassert(r2   < 0x20);
   vassert(r3   < 0x20);
   vassert(opc2 < 0x800);
   theInstr = ((opc1<<26) | (r1<<21) | (r2<<16) | (r3<<11) | opc2);
   return emit32(p, theInstr, endness_host);
}

static UChar* mkFormVXI ( UChar* p, UInt opc1, UInt r1, UInt r2,
                          UInt r3, UInt opc2, VexEndness endness_host )
{
   UInt theInstr;
   vassert(opc1 < 0x40);
   vassert(r1   < 0x20);
   vassert(r2   < 0x20);
   vassert(r3   < 0x20);
   vassert(opc2 < 0x27);
   theInstr = ((opc1<<26) | (r1<<21) | (r2<<16) | (r3<<11) | opc2<<1);
   return emit32(p, theInstr, endness_host);
}

static UChar* mkFormVXR ( UChar* p, UInt opc1, UInt r1, UInt r2,
                          UInt r3, UInt Rc, UInt opc2,
                          VexEndness endness_host )
{
   UInt theInstr;
   vassert(opc1 < 0x40);
   vassert(r1   < 0x20);
   vassert(r2   < 0x20);
   vassert(r3   < 0x20);
   vassert(Rc   < 0x2);
   vassert(opc2 < 0x400);
   theInstr = ((opc1<<26) | (r1<<21) | (r2<<16) |
               (r3<<11) | (Rc<<10) | opc2);
   return emit32(p, theInstr, endness_host);
}

static UChar* mkFormVA ( UChar* p, UInt opc1, UInt r1, UInt r2,
                         UInt r3, UInt r4, UInt opc2, VexEndness endness_host )
{
   UInt theInstr;
   vassert(opc1 < 0x40);
   vassert(r1   < 0x20);
   vassert(r2   < 0x20);
   vassert(r3   < 0x20);
   vassert(r4   < 0x20);
   vassert(opc2 < 0x40);
   theInstr = ((opc1<<26) | (r1<<21) | (r2<<16) |
               (r3<<11) | (r4<<6) | opc2);
   return emit32(p, theInstr, endness_host);
}



Int emit_PPCInstr ( Bool* is_profInc,
                    UChar* buf, Int nbuf, const PPCInstr* i, 
                    Bool mode64, VexEndness endness_host,
                    const void* disp_cp_chain_me_to_slowEP,
                    const void* disp_cp_chain_me_to_fastEP,
                    const void* disp_cp_xindir,
                    const void* disp_cp_xassisted)
{
   UChar* p = &buf[0];
   vassert(nbuf >= 32);

   if (0) {
      vex_printf("asm  ");ppPPCInstr(i, mode64); vex_printf("\n");
   }

   switch (i->tag) {

   case Pin_LI:
      p = mkLoadImm(p, iregEnc(i->Pin.LI.dst, mode64),
                    i->Pin.LI.imm64, mode64, endness_host);
      goto done;

   case Pin_Alu: {
      PPCRH* srcR   = i->Pin.Alu.srcR;
      Bool   immR   = toBool(srcR->tag == Prh_Imm);
      UInt   r_dst  = iregEnc(i->Pin.Alu.dst, mode64);
      UInt   r_srcL = iregEnc(i->Pin.Alu.srcL, mode64);
      UInt   r_srcR = immR ? (-1) :
                             iregEnc(srcR->Prh.Reg.reg, mode64);

      switch (i->Pin.Alu.op) {
      case Palu_ADD:
         if (immR) {
            
            vassert(srcR->Prh.Imm.syned);
            vassert(srcR->Prh.Imm.imm16 != 0x8000);
            p = mkFormD(p, 14, r_dst, r_srcL, srcR->Prh.Imm.imm16, endness_host);
         } else {
            
            p = mkFormXO(p, 31, r_dst, r_srcL, r_srcR, 0, 266, 0, endness_host);
         }
         break;

      case Palu_SUB:
         if (immR) {
            
            vassert(srcR->Prh.Imm.syned);
            vassert(srcR->Prh.Imm.imm16 != 0x8000);
            p = mkFormD(p, 14, r_dst, r_srcL, (- srcR->Prh.Imm.imm16),
                        endness_host);
         } else {
            
            p = mkFormXO(p, 31, r_dst, r_srcR, r_srcL, 0, 40, 0, endness_host);
         }
         break;

      case Palu_AND:
         if (immR) {
            
            vassert(!srcR->Prh.Imm.syned);
            p = mkFormD(p, 28, r_srcL, r_dst, srcR->Prh.Imm.imm16, endness_host);
         } else {
            
            p = mkFormX(p, 31, r_srcL, r_dst, r_srcR, 28, 0, endness_host);
         }
         break;

      case Palu_OR:
         if (immR) {
            
            vassert(!srcR->Prh.Imm.syned);
            p = mkFormD(p, 24, r_srcL, r_dst, srcR->Prh.Imm.imm16, endness_host);
         } else {
            
            p = mkFormX(p, 31, r_srcL, r_dst, r_srcR, 444, 0, endness_host);
         }
         break;

      case Palu_XOR:
         if (immR) {
            
            vassert(!srcR->Prh.Imm.syned);
            p = mkFormD(p, 26, r_srcL, r_dst, srcR->Prh.Imm.imm16, endness_host);
         } else {
            
            p = mkFormX(p, 31, r_srcL, r_dst, r_srcR, 316, 0, endness_host);
         }
         break;

      default:
         goto bad;
      }
      goto done;
   }

   case Pin_Shft: {
      PPCRH* srcR   = i->Pin.Shft.srcR;
      Bool   sz32   = i->Pin.Shft.sz32;
      Bool   immR   = toBool(srcR->tag == Prh_Imm);
      UInt   r_dst  = iregEnc(i->Pin.Shft.dst, mode64);
      UInt   r_srcL = iregEnc(i->Pin.Shft.srcL, mode64);
      UInt   r_srcR = immR ? (-1) :
                             iregEnc(srcR->Prh.Reg.reg, mode64);
      if (!mode64)
         vassert(sz32);

      switch (i->Pin.Shft.op) {
      case Pshft_SHL:
         if (sz32) {
            if (immR) {
               UInt n = srcR->Prh.Imm.imm16;
               vassert(!srcR->Prh.Imm.syned);
               vassert(n > 0 && n < 32);
               p = mkFormM(p, 21, r_srcL, r_dst, n, 0, 31-n, 0, endness_host);
            } else {
               
               p = mkFormX(p, 31, r_srcL, r_dst, r_srcR, 24, 0, endness_host);
            }
         } else {
            if (immR) {
               UInt n = srcR->Prh.Imm.imm16;
               vassert(!srcR->Prh.Imm.syned);
               vassert(n > 0 && n < 64);
               p = mkFormMD(p, 30, r_srcL, r_dst, n, 63-n, 1, endness_host);
            } else {
               
               p = mkFormX(p, 31, r_srcL, r_dst, r_srcR, 27, 0, endness_host);
            }
         }
         break;

      case Pshft_SHR:
         if (sz32) {
             if (immR) {
               UInt n = srcR->Prh.Imm.imm16;
               vassert(!srcR->Prh.Imm.syned);
               vassert(n > 0 && n < 32);
               p = mkFormM(p, 21, r_srcL, r_dst, 32-n, n, 31, 0, endness_host);
            } else {
               
               p = mkFormX(p, 31, r_srcL, r_dst, r_srcR, 536, 0, endness_host);
            }
         } else {
            if (immR) {
               UInt n = srcR->Prh.Imm.imm16;
               vassert(!srcR->Prh.Imm.syned);
               vassert(n > 0 && n < 64);
               p = mkFormMD(p, 30, r_srcL, r_dst, 64-n, n, 0, endness_host);
            } else {
               
               p = mkFormX(p, 31, r_srcL, r_dst, r_srcR, 539, 0, endness_host);
            }
         }
         break;

      case Pshft_SAR:
         if (sz32) {
            if (immR) {
               
               UInt n = srcR->Prh.Imm.imm16;
               vassert(!srcR->Prh.Imm.syned);
               if (mode64)
                  vassert(n >= 0 && n < 32);
               else 
                  vassert(n > 0 && n < 32);
               p = mkFormX(p, 31, r_srcL, r_dst, n, 824, 0, endness_host);
            } else {
               
               p = mkFormX(p, 31, r_srcL, r_dst, r_srcR, 792, 0, endness_host);
            }
         } else {
            if (immR) {
               
               UInt n = srcR->Prh.Imm.imm16;
               vassert(!srcR->Prh.Imm.syned);
               vassert(n > 0 && n < 64);
               p = mkFormXS(p, 31, r_srcL, r_dst, n, 413, 0, endness_host);
            } else {
               
               p = mkFormX(p, 31, r_srcL, r_dst, r_srcR, 794, 0, endness_host);
            }
         }
         break;

      default:
         goto bad;
      }
      goto done;
   }

   case Pin_AddSubC: {
      Bool isAdd  = i->Pin.AddSubC.isAdd;
      Bool setC   = i->Pin.AddSubC.setC;
      UInt r_srcL = iregEnc(i->Pin.AddSubC.srcL, mode64);
      UInt r_srcR = iregEnc(i->Pin.AddSubC.srcR, mode64);
      UInt r_dst  = iregEnc(i->Pin.AddSubC.dst, mode64);
      
      if (isAdd) {
         if (setC) 
            p = mkFormXO(p, 31, r_dst, r_srcL, r_srcR, 0, 10, 0, endness_host);
         else          
            p = mkFormXO(p, 31, r_dst, r_srcL, r_srcR, 0, 138, 0, endness_host);
      } else {
         
         if (setC) 
            p = mkFormXO(p, 31, r_dst, r_srcR, r_srcL, 0, 8, 0, endness_host);
         else          
            p = mkFormXO(p, 31, r_dst, r_srcR, r_srcL, 0, 136, 0, endness_host);
      }
      goto done;
   }

   case Pin_Cmp: {
      Bool syned  = i->Pin.Cmp.syned;
      Bool sz32   = i->Pin.Cmp.sz32;
      UInt fld1   = i->Pin.Cmp.crfD << 2;
      UInt r_srcL = iregEnc(i->Pin.Cmp.srcL, mode64);
      UInt r_srcR, imm_srcR;
      PPCRH* srcR = i->Pin.Cmp.srcR;

      if (!mode64)        
         vassert(sz32);      
      else if (!sz32)     
         fld1 |= 1;
 
      switch (srcR->tag) {
      case Prh_Imm:
         vassert(syned == srcR->Prh.Imm.syned);
         imm_srcR = srcR->Prh.Imm.imm16;
         if (syned) {  
            vassert(imm_srcR != 0x8000);
            p = mkFormD(p, 11, fld1, r_srcL, imm_srcR, endness_host);
         } else {      
            p = mkFormD(p, 10, fld1, r_srcL, imm_srcR, endness_host);
         }
         break;
      case Prh_Reg:
         r_srcR = iregEnc(srcR->Prh.Reg.reg, mode64);
         if (syned)  
            p = mkFormX(p, 31, fld1, r_srcL, r_srcR, 0, 0, endness_host);
         else        
            p = mkFormX(p, 31, fld1, r_srcL, r_srcR, 32, 0, endness_host);
         break;
      default: 
         goto bad;
      }        
      goto done;
   }

   case Pin_Unary: {
      UInt r_dst = iregEnc(i->Pin.Unary.dst, mode64);
      UInt r_src = iregEnc(i->Pin.Unary.src, mode64);

      switch (i->Pin.Unary.op) {
      case Pun_NOT:  
         p = mkFormX(p, 31, r_src, r_dst, r_src, 124, 0, endness_host);
         break;
      case Pun_NEG:  
         p = mkFormXO(p, 31, r_dst, r_src, 0, 0, 104, 0, endness_host);
         break;
      case Pun_CLZ32:  
         p = mkFormX(p, 31, r_src, r_dst, 0, 26, 0, endness_host);
         break;
      case Pun_CLZ64:  
         vassert(mode64);
         p = mkFormX(p, 31, r_src, r_dst, 0, 58, 0, endness_host);
         break;
      case Pun_EXTSW:  
         vassert(mode64);
         p = mkFormX(p, 31, r_src, r_dst, 0, 986, 0, endness_host);
         break;
      default: goto bad;
      }
      goto done;
   }

   case Pin_MulL: {
      Bool syned  = i->Pin.MulL.syned;
      Bool sz32   = i->Pin.MulL.sz32;
      UInt r_dst  = iregEnc(i->Pin.MulL.dst, mode64);
      UInt r_srcL = iregEnc(i->Pin.MulL.srcL, mode64);
      UInt r_srcR = iregEnc(i->Pin.MulL.srcR, mode64);

      if (!mode64)
         vassert(sz32);

      if (i->Pin.MulL.hi) {
         
         if (sz32) {
            if (syned)  
               p = mkFormXO(p, 31, r_dst, r_srcL, r_srcR, 0, 75, 0,
                            endness_host);
            else        
               p = mkFormXO(p, 31, r_dst, r_srcL, r_srcR, 0, 11, 0,
                            endness_host);
         } else {
            if (syned)  
               p = mkFormXO(p, 31, r_dst, r_srcL, r_srcR, 0, 73, 0,
                            endness_host);
            else        
               p = mkFormXO(p, 31, r_dst, r_srcL, r_srcR, 0, 9, 0, endness_host);
         }
      } else {
         
         vassert(!i->Pin.MulL.syned);
         if (sz32)      
            p = mkFormXO(p, 31, r_dst, r_srcL, r_srcR, 0, 235, 0, endness_host);
         else           
            p = mkFormXO(p, 31, r_dst, r_srcL, r_srcR, 0, 233, 0, endness_host);
      }
      goto done;
   }

   case Pin_Div: {
      Bool syned  = i->Pin.Div.syned;
      Bool sz32   = i->Pin.Div.sz32;
      UInt r_dst  = iregEnc(i->Pin.Div.dst, mode64);
      UInt r_srcL = iregEnc(i->Pin.Div.srcL, mode64);
      UInt r_srcR = iregEnc(i->Pin.Div.srcR, mode64);

      if (!mode64)
         vassert(sz32);

      if (i->Pin.Div.extended) {
         if (sz32) {
            if (syned)
               
               p = mkFormXO(p, 31, r_dst, r_srcL, r_srcR, 0, 427, 0,
                            endness_host);
            else
               
               p = mkFormXO(p, 31, r_dst, r_srcL, r_srcR, 0, 395, 0,
                            endness_host);
         } else {
            if (syned)
               
               p = mkFormXO(p, 31, r_dst, r_srcL, r_srcR, 0, 425, 0,
                            endness_host);
            else
               
               p = mkFormXO(p, 31, r_dst, r_srcL, r_srcR, 0, 393, 0,
                            endness_host);
         }
      } else if (sz32) {
         if (syned)  
            p = mkFormXO(p, 31, r_dst, r_srcL, r_srcR, 0, 491, 0, endness_host);
         else        
            p = mkFormXO(p, 31, r_dst, r_srcL, r_srcR, 0, 459, 0, endness_host);
      } else {
         if (syned)  
            p = mkFormXO(p, 31, r_dst, r_srcL, r_srcR, 0, 489, 0, endness_host);
         else        
            p = mkFormXO(p, 31, r_dst, r_srcL, r_srcR, 0, 457, 0, endness_host);
      }
      goto done;
   }

   case Pin_Call: {
      if (i->Pin.Call.cond.test != Pct_ALWAYS
          && i->Pin.Call.rloc.pri != RLPri_None) {
         goto bad;
      }
      PPCCondCode cond  = i->Pin.Call.cond;
      UInt        r_dst = 10;

      
      UChar* ptmp = NULL;
      if (cond.test != Pct_ALWAYS) {
         
         ptmp = p; 
         p += 4;                                          
      }

                                
      p = mkLoadImm(p, r_dst, i->Pin.Call.target, mode64, endness_host);

      
      p = mkFormXFX(p, r_dst, 9, 467, endness_host);               
      
      
      p = mkFormXL(p, 19, Pct_ALWAYS, 0, 0, 528, 1, endness_host); 

      
      if (cond.test != Pct_ALWAYS) {
         Int delta = p - ptmp;
         vassert(delta >= 16 && delta <= 32);
         
         mkFormB(ptmp, invertCondTest(cond.test),
                 cond.flag, (delta>>2), 0, 0, endness_host);
      }
      goto done;
   }

   case Pin_XDirect: {
      vassert(disp_cp_chain_me_to_slowEP != NULL);
      vassert(disp_cp_chain_me_to_fastEP != NULL);

      UChar* ptmp = NULL;
      if (i->Pin.XDirect.cond.test != Pct_ALWAYS) {
         vassert(i->Pin.XDirect.cond.flag != Pcf_NONE);
         ptmp = p;
         p += 4;
      } else {
         vassert(i->Pin.XDirect.cond.flag == Pcf_NONE);
      }

      
      
      if (!mode64) vassert(0 == (((ULong)i->Pin.XDirect.dstGA) >> 32));
      p = mkLoadImm(p, 30, (ULong)i->Pin.XDirect.dstGA, mode64,
                    endness_host);
      
      p = do_load_or_store_machine_word(
             p, False,
             30, i->Pin.XDirect.amCIA, mode64, endness_host
          );

      
      
      const void* disp_cp_chain_me
               = i->Pin.XDirect.toFastEP ? disp_cp_chain_me_to_fastEP 
                                         : disp_cp_chain_me_to_slowEP;
      p = mkLoadImm_EXACTLY2or5(
             p, 30, (Addr)disp_cp_chain_me, mode64, endness_host);
      
      p = mkFormXFX(p, 30, 9, 467, endness_host);
      
      p = mkFormXL(p, 19, Pct_ALWAYS, 0, 0, 528, 1, endness_host);
      

      
      if (i->Pin.XDirect.cond.test != Pct_ALWAYS) {
         Int delta = p - ptmp;
         vassert(delta >= 16 && delta <= 64 && 0 == (delta & 3));
         
         mkFormB(ptmp, invertCondTest(i->Pin.XDirect.cond.test),
                 i->Pin.XDirect.cond.flag, (delta>>2), 0, 0, endness_host);
      }
      goto done;
   }

   case Pin_XIndir: {
      vassert(disp_cp_xindir != NULL);

      UChar* ptmp = NULL;
      if (i->Pin.XIndir.cond.test != Pct_ALWAYS) {
         vassert(i->Pin.XIndir.cond.flag != Pcf_NONE);
         ptmp = p;
         p += 4;
      } else {
         vassert(i->Pin.XIndir.cond.flag == Pcf_NONE);
      }

      
      
      p = do_load_or_store_machine_word(
             p, False,
             iregEnc(i->Pin.XIndir.dstGA, mode64),
             i->Pin.XIndir.amCIA, mode64, endness_host
          );

      
      p = mkLoadImm(p, 30, (ULong)(Addr)disp_cp_xindir, mode64,
                    endness_host);
      
      p = mkFormXFX(p, 30, 9, 467, endness_host);
      
      p = mkFormXL(p, 19, Pct_ALWAYS, 0, 0, 528, 0, endness_host);

      
      if (i->Pin.XIndir.cond.test != Pct_ALWAYS) {
         Int delta = p - ptmp;
         vassert(delta >= 16 && delta <= 32 && 0 == (delta & 3));
         
         mkFormB(ptmp, invertCondTest(i->Pin.XIndir.cond.test),
                 i->Pin.XIndir.cond.flag, (delta>>2), 0, 0, endness_host);
      }
      goto done;
   }

   case Pin_XAssisted: {
      UChar* ptmp = NULL;
      if (i->Pin.XAssisted.cond.test != Pct_ALWAYS) {
         vassert(i->Pin.XAssisted.cond.flag != Pcf_NONE);
         ptmp = p;
         p += 4;
      } else {
         vassert(i->Pin.XAssisted.cond.flag == Pcf_NONE);
      }

      
      
      p = do_load_or_store_machine_word(
             p, False,
             iregEnc(i->Pin.XIndir.dstGA, mode64),
             i->Pin.XIndir.amCIA, mode64, endness_host
          );

      
      UInt trcval = 0;
      switch (i->Pin.XAssisted.jk) {
         case Ijk_ClientReq:   trcval = VEX_TRC_JMP_CLIENTREQ;   break;
         case Ijk_Sys_syscall: trcval = VEX_TRC_JMP_SYS_SYSCALL; break;
         
         
         case Ijk_EmWarn:      trcval = VEX_TRC_JMP_EMWARN;      break;
         case Ijk_EmFail:      trcval = VEX_TRC_JMP_EMFAIL;      break;
         
         case Ijk_NoDecode:    trcval = VEX_TRC_JMP_NODECODE;    break;
         case Ijk_InvalICache: trcval = VEX_TRC_JMP_INVALICACHE; break;
         case Ijk_NoRedir:     trcval = VEX_TRC_JMP_NOREDIR;     break;
         case Ijk_SigTRAP:     trcval = VEX_TRC_JMP_SIGTRAP;     break;
         
         case Ijk_SigBUS:        trcval = VEX_TRC_JMP_SIGBUS;    break;
         case Ijk_Boring:      trcval = VEX_TRC_JMP_BORING;      break;
         
         
         
         
         default: 
            ppIRJumpKind(i->Pin.XAssisted.jk);
            vpanic("emit_ARMInstr.Pin_XAssisted: unexpected jump kind");
      }
      vassert(trcval != 0);
      p = mkLoadImm(p, 31, trcval, mode64, endness_host);

      
      p = mkLoadImm(p, 30,
                    (ULong)(Addr)disp_cp_xassisted, mode64,
                     endness_host);
      
      p = mkFormXFX(p, 30, 9, 467, endness_host);
      
      p = mkFormXL(p, 19, Pct_ALWAYS, 0, 0, 528, 0, endness_host);

      
      if (i->Pin.XAssisted.cond.test != Pct_ALWAYS) {
         Int delta = p - ptmp;
         vassert(delta >= 16 && delta <= 32 && 0 == (delta & 3));
         
         mkFormB(ptmp, invertCondTest(i->Pin.XAssisted.cond.test),
                 i->Pin.XAssisted.cond.flag, (delta>>2), 0, 0, endness_host);
      }
      goto done;
   }

   case Pin_CMov: {
      UInt  r_dst, r_src;
      ULong imm_src;
      PPCCondCode cond;
      vassert(i->Pin.CMov.cond.test != Pct_ALWAYS);

      r_dst = iregEnc(i->Pin.CMov.dst, mode64);
      cond = i->Pin.CMov.cond;

      
      UChar* ptmp = NULL;
      if (cond.test != Pct_ALWAYS) {
         ptmp = p; 
         p += 4;
      }

      
      switch (i->Pin.CMov.src->tag) {
      case Pri_Imm:
         imm_src = i->Pin.CMov.src->Pri.Imm;
         p = mkLoadImm(p, r_dst, imm_src, mode64, endness_host);  
         break;
      case Pri_Reg:
         r_src = iregEnc(i->Pin.CMov.src->Pri.Reg, mode64);
         p = mkMoveReg(p, r_dst, r_src, endness_host);            
         break;
      default: goto bad;
      }

      
      if (cond.test != Pct_ALWAYS) {
         Int delta = p - ptmp;
         vassert(delta >= 8 && delta <= 24);
         
         mkFormB(ptmp, invertCondTest(cond.test),
                 cond.flag, (delta>>2), 0, 0, endness_host);
      }
      goto done;
   }

   case Pin_Load: {
      PPCAMode* am_addr = i->Pin.Load.src;
      UInt r_dst = iregEnc(i->Pin.Load.dst, mode64);
      UInt opc1, opc2, sz = i->Pin.Load.sz;
      switch (am_addr->tag) {
      case Pam_IR:
         if (mode64 && (sz == 4 || sz == 8)) {
            
            vassert(0 == (am_addr->Pam.IR.index & 3));
         }
         switch(sz) {
            case 1:  opc1 = 34; break;
            case 2:  opc1 = 40; break;
            case 4:  opc1 = 32; break;
            case 8:  opc1 = 58; vassert(mode64); break;
            default: goto bad;
         }
         p = doAMode_IR(p, opc1, r_dst, am_addr, mode64, endness_host);
         goto done;
      case Pam_RR:
         switch(sz) {
            case 1:  opc2 = 87;  break;
            case 2:  opc2 = 279; break;
            case 4:  opc2 = 23;  break;
            case 8:  opc2 = 21; vassert(mode64); break;
            default: goto bad;
         }
         p = doAMode_RR(p, 31, opc2, r_dst, am_addr, mode64, endness_host);
         goto done;
      default:
         goto bad;
      }
   }

   case Pin_LoadL: {
      if (i->Pin.LoadL.sz == 1) {
         p = mkFormX(p, 31, iregEnc(i->Pin.LoadL.dst, mode64),
                     0, iregEnc(i->Pin.LoadL.src, mode64), 52, 0, endness_host);
         goto done;
      }
      if (i->Pin.LoadL.sz == 2) {
         p = mkFormX(p, 31, iregEnc(i->Pin.LoadL.dst, mode64),
                     0, iregEnc(i->Pin.LoadL.src, mode64), 116, 0, endness_host);
         goto done;
      }
      if (i->Pin.LoadL.sz == 4) {
         p = mkFormX(p, 31, iregEnc(i->Pin.LoadL.dst, mode64),
                     0, iregEnc(i->Pin.LoadL.src, mode64), 20, 0, endness_host);
         goto done;
      }
      if (i->Pin.LoadL.sz == 8 && mode64) {
         p = mkFormX(p, 31, iregEnc(i->Pin.LoadL.dst, mode64),
                     0, iregEnc(i->Pin.LoadL.src, mode64), 84, 0, endness_host);
         goto done;
      }
      goto bad;
   }

   case Pin_Set: {
      UInt        r_dst = iregEnc(i->Pin.Set.dst, mode64);
      PPCCondCode cond  = i->Pin.Set.cond;
      UInt rot_imm, r_tmp;

      if (cond.test == Pct_ALWAYS) {
         
         p = mkFormD(p, 14, r_dst, 0, 1, endness_host);
      } else {
         vassert(cond.flag != Pcf_NONE);
         rot_imm = 1 + cond.flag;
         r_tmp = 0;  

         
         p = mkFormX(p, 31, r_tmp, 0, 0, 19, 0, endness_host);

         
         
         p = mkFormM(p, 21, r_tmp, r_dst, rot_imm, 31, 31, 0, endness_host);

         if (cond.test == Pct_FALSE) {
            
            p = mkFormD(p, 26, r_dst, r_dst, 1, endness_host);
         }
      }
      goto done;
   }

   case Pin_MfCR:
      
      p = mkFormX(p, 31, iregEnc(i->Pin.MfCR.dst, mode64), 0, 0, 19, 0,
                  endness_host);
      goto done;

   case Pin_MFence: {
      p = mkFormX(p, 31, 0, 0, 0, 598, 0, endness_host);   
      
      
      goto done;
   }

   case Pin_Store: {
      PPCAMode* am_addr = i->Pin.Store.dst;
      UInt r_src = iregEnc(i->Pin.Store.src, mode64);
      UInt opc1, opc2, sz = i->Pin.Store.sz;
      switch (i->Pin.Store.dst->tag) {
      case Pam_IR:
         if (mode64 && (sz == 4 || sz == 8)) {
            
            vassert(0 == (am_addr->Pam.IR.index & 3));
         }
         switch(sz) {
         case 1: opc1 = 38; break;
         case 2: opc1 = 44; break;
         case 4: opc1 = 36; break;
         case 8: vassert(mode64);
                 opc1 = 62; break;
         default:
            goto bad;
         }
         p = doAMode_IR(p, opc1, r_src, am_addr, mode64, endness_host);
         goto done;
      case Pam_RR:
         switch(sz) {
         case 1: opc2 = 215; break;
         case 2: opc2 = 407; break;
         case 4: opc2 = 151; break;
         case 8: vassert(mode64);
                 opc2 = 149; break;
         default:
            goto bad;
         }
         p = doAMode_RR(p, 31, opc2, r_src, am_addr, mode64, endness_host);
         goto done;
      default:
         goto bad;
      }
      goto done;
   }

   case Pin_StoreC: {
      if (i->Pin.StoreC.sz == 1) {
         p = mkFormX(p, 31, iregEnc(i->Pin.StoreC.src, mode64),
                     0, iregEnc(i->Pin.StoreC.dst, mode64), 694, 1, endness_host);
         goto done;
      }
      if (i->Pin.StoreC.sz == 2) {
         p = mkFormX(p, 31, iregEnc(i->Pin.StoreC.src, mode64),
                     0, iregEnc(i->Pin.StoreC.dst, mode64), 726, 1, endness_host);
         goto done;
      }

      if (i->Pin.StoreC.sz == 4) {
         p = mkFormX(p, 31, iregEnc(i->Pin.StoreC.src, mode64),
                     0, iregEnc(i->Pin.StoreC.dst, mode64), 150, 1, endness_host);
         goto done;
      }
      if (i->Pin.StoreC.sz == 8 && mode64) {
         p = mkFormX(p, 31, iregEnc(i->Pin.StoreC.src, mode64),
                     0, iregEnc(i->Pin.StoreC.dst, mode64), 214, 1, endness_host);
         goto done;
      }
      goto bad;
   }

   case Pin_FpUnary: {
      UInt fr_dst = fregEnc(i->Pin.FpUnary.dst);
      UInt fr_src = fregEnc(i->Pin.FpUnary.src);
      switch (i->Pin.FpUnary.op) {
      case Pfp_RSQRTE: 
         p = mkFormA( p, 63, fr_dst, 0, fr_src, 0, 26, 0, endness_host );
         break;
      case Pfp_RES:   
         p = mkFormA( p, 59, fr_dst, 0, fr_src, 0, 24, 0, endness_host );
         break;
      case Pfp_SQRT:  
         p = mkFormA( p, 63, fr_dst, 0, fr_src, 0, 22, 0, endness_host );
         break;
      case Pfp_ABS:   
         p = mkFormX(p, 63, fr_dst, 0, fr_src, 264, 0, endness_host);
         break;
      case Pfp_NEG:   
         p = mkFormX(p, 63, fr_dst, 0, fr_src, 40, 0, endness_host);
         break;
      case Pfp_MOV:   
         p = mkFormX(p, 63, fr_dst, 0, fr_src, 72, 0, endness_host);
         break;
      case Pfp_FRIM:  
         p = mkFormX(p, 63, fr_dst, 0, fr_src, 488, 0, endness_host);
         break;
      case Pfp_FRIP:  
         p = mkFormX(p, 63, fr_dst, 0, fr_src, 456, 0, endness_host);
         break;
      case Pfp_FRIN:  
         p = mkFormX(p, 63, fr_dst, 0, fr_src, 392, 0, endness_host);
         break;
      case Pfp_FRIZ:  
         p = mkFormX(p, 63, fr_dst, 0, fr_src, 424, 0, endness_host);
         break;
      default:
         goto bad;
      }
      goto done;
   }

   case Pin_FpBinary: {
      UInt fr_dst  = fregEnc(i->Pin.FpBinary.dst);
      UInt fr_srcL = fregEnc(i->Pin.FpBinary.srcL);
      UInt fr_srcR = fregEnc(i->Pin.FpBinary.srcR);
      switch (i->Pin.FpBinary.op) {
      case Pfp_ADDD:   
         p = mkFormA( p, 63, fr_dst, fr_srcL, fr_srcR, 0, 21, 0, endness_host );
         break;
      case Pfp_ADDS:   
         p = mkFormA( p, 59, fr_dst, fr_srcL, fr_srcR, 0, 21, 0, endness_host );
         break;
      case Pfp_SUBD:   
         p = mkFormA( p, 63, fr_dst, fr_srcL, fr_srcR, 0, 20, 0, endness_host );
         break;
      case Pfp_SUBS:   
         p = mkFormA( p, 59, fr_dst, fr_srcL, fr_srcR, 0, 20, 0, endness_host );
         break;
      case Pfp_MULD:   
         p = mkFormA( p, 63, fr_dst, fr_srcL, 0, fr_srcR, 25, 0, endness_host );
         break;
      case Pfp_MULS:   
         p = mkFormA( p, 59, fr_dst, fr_srcL, 0, fr_srcR, 25, 0, endness_host );
         break;
      case Pfp_DIVD:   
         p = mkFormA( p, 63, fr_dst, fr_srcL, fr_srcR, 0, 18, 0, endness_host );
         break;
      case Pfp_DIVS:   
         p = mkFormA( p, 59, fr_dst, fr_srcL, fr_srcR, 0, 18, 0, endness_host );
         break;
      default:
         goto bad;
      }
      goto done;
   }

   case Pin_FpMulAcc: {
      UInt fr_dst    = fregEnc(i->Pin.FpMulAcc.dst);
      UInt fr_srcML  = fregEnc(i->Pin.FpMulAcc.srcML);
      UInt fr_srcMR  = fregEnc(i->Pin.FpMulAcc.srcMR);
      UInt fr_srcAcc = fregEnc(i->Pin.FpMulAcc.srcAcc);
      switch (i->Pin.FpMulAcc.op) {
      case Pfp_MADDD:   
         p = mkFormA( p, 63, fr_dst, fr_srcML, fr_srcAcc, fr_srcMR, 29, 0,
                      endness_host );
         break;
      case Pfp_MADDS:   
         p = mkFormA( p, 59, fr_dst, fr_srcML, fr_srcAcc, fr_srcMR, 29, 0,
                      endness_host );
         break;
      case Pfp_MSUBD:   
         p = mkFormA( p, 63, fr_dst, fr_srcML, fr_srcAcc, fr_srcMR, 28, 0,
                      endness_host );
         break;
      case Pfp_MSUBS:   
         p = mkFormA( p, 59, fr_dst, fr_srcML, fr_srcAcc, fr_srcMR, 28, 0,
                      endness_host );
         break;
      default:
         goto bad;
      }
      goto done;
   }

   case Pin_FpLdSt: {
      PPCAMode* am_addr = i->Pin.FpLdSt.addr;
      UInt f_reg = fregEnc(i->Pin.FpLdSt.reg);
      Bool idxd = toBool(i->Pin.FpLdSt.addr->tag == Pam_RR);
      UChar sz = i->Pin.FpLdSt.sz;
      UInt opc;
      vassert(sz == 4 || sz == 8);

      if (i->Pin.FpLdSt.isLoad) {   
         if (idxd) {  
            opc = (sz == 4) ? 535 : 599;
            p = doAMode_RR(p, 31, opc, f_reg, am_addr, mode64, endness_host);
         } else {     
            opc = (sz == 4) ? 48 : 50;
            p = doAMode_IR(p, opc, f_reg, am_addr, mode64, endness_host);
         }
      } else {                      
         if (idxd) { 
            opc = (sz == 4) ? 663 : 727;
            p = doAMode_RR(p, 31, opc, f_reg, am_addr, mode64, endness_host);
         } else {    
            opc = (sz == 4) ? 52 : 54;
            p = doAMode_IR(p, opc, f_reg, am_addr, mode64, endness_host);
         }
      }
      goto done;
   }

   case Pin_FpSTFIW: {
      UInt ir_addr = iregEnc(i->Pin.FpSTFIW.addr, mode64);
      UInt fr_data = fregEnc(i->Pin.FpSTFIW.data);
      
      
      p = mkFormX(p, 31, fr_data, 0, ir_addr, 983, 0, endness_host);
      goto done;
   }

   case Pin_FpRSP: {
      UInt fr_dst = fregEnc(i->Pin.FpRSP.dst);
      UInt fr_src = fregEnc(i->Pin.FpRSP.src);
      
      p = mkFormX(p, 63, fr_dst, 0, fr_src, 12, 0, endness_host);
      goto done;
   }

   case Pin_FpCftI: {
      UInt fr_dst = fregEnc(i->Pin.FpCftI.dst);
      UInt fr_src = fregEnc(i->Pin.FpCftI.src);
      if (i->Pin.FpCftI.fromI == False && i->Pin.FpCftI.int32 == True) {
         if (i->Pin.FpCftI.syned == True) {
            
            p = mkFormX(p, 63, fr_dst, 0, fr_src, 14, 0, endness_host);
            goto done;
         } else {
            
            p = mkFormX(p, 63, fr_dst, 0, fr_src, 142, 0, endness_host);
            goto done;
         }
      }
      if (i->Pin.FpCftI.fromI == False && i->Pin.FpCftI.int32 == False) {
         if (i->Pin.FpCftI.syned == True) {
            
            p = mkFormX(p, 63, fr_dst, 0, fr_src, 814, 0, endness_host);
            goto done;
         } else {
            
            p = mkFormX(p, 63, fr_dst, 0, fr_src, 942, 0, endness_host);
            goto done;
         }
      }
      if (i->Pin.FpCftI.fromI == True && i->Pin.FpCftI.int32 == False) {
         if (i->Pin.FpCftI.syned == True) {
            
            p = mkFormX(p, 63, fr_dst, 0, fr_src, 846, 0, endness_host);
            goto done;
         } else if (i->Pin.FpCftI.flt64 == True) {
            
            p = mkFormX(p, 63, fr_dst, 0, fr_src, 974, 0, endness_host);
            goto done;
         } else {
            
            p = mkFormX(p, 59, fr_dst, 0, fr_src, 974, 0, endness_host);
            goto done;
         }
      }
      goto bad;
   }

   case Pin_FpCMov: {
      UInt        fr_dst = fregEnc(i->Pin.FpCMov.dst);
      UInt        fr_src = fregEnc(i->Pin.FpCMov.src);
      PPCCondCode cc     = i->Pin.FpCMov.cond;

      if (fr_dst == fr_src) goto done;
      
      vassert(cc.test != Pct_ALWAYS);

      
      if (cc.test != Pct_ALWAYS) {
         
         p = mkFormB(p, invertCondTest(cc.test), cc.flag, 8>>2, 0, 0,
                     endness_host);
      }

      
      p = mkFormX(p, 63, fr_dst, 0, fr_src, 72, 0, endness_host);
      goto done;
   }

   case Pin_FpLdFPSCR: {
      UInt fr_src = fregEnc(i->Pin.FpLdFPSCR.src);
      p = mkFormXFL(p, 0xFF, fr_src, i->Pin.FpLdFPSCR.dfp_rm, endness_host); 
      goto done;
   }

   case Pin_FpCmp: {
      UChar crfD    = 1;
      UInt  r_dst   = iregEnc(i->Pin.FpCmp.dst, mode64);
      UInt  fr_srcL = fregEnc(i->Pin.FpCmp.srcL);
      UInt  fr_srcR = fregEnc(i->Pin.FpCmp.srcR);
      vassert(crfD < 8);
      
      p = mkFormX(p, 63, crfD<<2, fr_srcL, fr_srcR, 32, 0, endness_host);

      
      p = mkFormX(p, 31, r_dst, 0, 0, 19, 0, endness_host);
      
      
      
      p = mkFormM(p, 21, r_dst, r_dst, 8, 28, 31, 0, endness_host);
      goto done;
   }

   case Pin_RdWrLR: {
      UInt reg = iregEnc(i->Pin.RdWrLR.gpr, mode64);
      
      p = mkFormXFX(p, reg, 8, (i->Pin.RdWrLR.wrLR==True) ? 467 : 339,
                    endness_host);
      goto done;
   }


   
   case Pin_AvLdSt: {
      UInt opc2, v_reg, r_idx, r_base;
      UChar sz   = i->Pin.AvLdSt.sz;
      Bool  idxd = toBool(i->Pin.AvLdSt.addr->tag == Pam_RR);
      vassert(sz == 1 || sz == 2 || sz == 4 || sz == 16);

      v_reg  = vregEnc(i->Pin.AvLdSt.reg);
      r_base = iregEnc(i->Pin.AvLdSt.addr->Pam.RR.base, mode64);

      
      if (!idxd) {
         r_idx = 30;                       
         p = mkLoadImm(p, r_idx,
                       i->Pin.AvLdSt.addr->Pam.IR.index, mode64, endness_host);
      } else {
         r_idx  = iregEnc(i->Pin.AvLdSt.addr->Pam.RR.index, mode64);
      }

      if (i->Pin.FpLdSt.isLoad) {  
         opc2 = (sz==1) ?   7 : (sz==2) ?  39 : (sz==4) ?  71 : 103;
         p = mkFormX(p, 31, v_reg, r_idx, r_base, opc2, 0, endness_host);
      } else {                      
         opc2 = (sz==1) ? 135 : (sz==2) ? 167 : (sz==4) ? 199 : 231;
         p = mkFormX(p, 31, v_reg, r_idx, r_base, opc2, 0, endness_host);
      }
      goto done;
   }

   case Pin_AvUnary: {
      UInt v_dst = vregEnc(i->Pin.AvUnary.dst);
      UInt v_src = vregEnc(i->Pin.AvUnary.src);
      UInt opc2;
      switch (i->Pin.AvUnary.op) {
      case Pav_MOV:       opc2 = 1156; break; 
      case Pav_NOT:       opc2 = 1284; break; 
      case Pav_UNPCKH8S:  opc2 =  526; break; 
      case Pav_UNPCKH16S: opc2 =  590; break; 
      case Pav_UNPCKL8S:  opc2 =  654; break; 
      case Pav_UNPCKL16S: opc2 =  718; break; 
      case Pav_UNPCKHPIX: opc2 =  846; break; 
      case Pav_UNPCKLPIX: opc2 =  974; break; 

      case Pav_ZEROCNTBYTE: opc2 = 1794; break; 
      case Pav_ZEROCNTHALF: opc2 = 1858; break; 
      case Pav_ZEROCNTWORD: opc2 = 1922; break; 
      case Pav_ZEROCNTDBL:  opc2 = 1986; break; 
      case Pav_BITMTXXPOSE: opc2 = 1292; break; 
      default:
         goto bad;
      }
      switch (i->Pin.AvUnary.op) {
      case Pav_MOV:
      case Pav_NOT:
         p = mkFormVX( p, 4, v_dst, v_src, v_src, opc2, endness_host );
         break;
      default:
         p = mkFormVX( p, 4, v_dst, 0, v_src, opc2, endness_host );
         break;
      }
      goto done;
   }

   case Pin_AvBinary: {
      UInt v_dst  = vregEnc(i->Pin.AvBinary.dst);
      UInt v_srcL = vregEnc(i->Pin.AvBinary.srcL);
      UInt v_srcR = vregEnc(i->Pin.AvBinary.srcR);
      UInt opc2;
      if (i->Pin.AvBinary.op == Pav_SHL) {
         p = mkFormVX( p, 4, v_dst, v_srcL, v_srcR, 1036, endness_host ); 
         p = mkFormVX( p, 4, v_dst, v_dst,  v_srcR, 452, endness_host );  
         goto done;
      }
      if (i->Pin.AvBinary.op == Pav_SHR) {
         p = mkFormVX( p, 4, v_dst, v_srcL, v_srcR, 1100, endness_host ); 
         p = mkFormVX( p, 4, v_dst, v_dst,  v_srcR, 708, endness_host );  
         goto done;
      }
      switch (i->Pin.AvBinary.op) {
      
      case Pav_AND:       opc2 = 1028; break; 
      case Pav_OR:        opc2 = 1156; break; 
      case Pav_XOR:       opc2 = 1220; break; 
      default:
         goto bad;
      }
      p = mkFormVX( p, 4, v_dst, v_srcL, v_srcR, opc2, endness_host );
      goto done;
   }

   case Pin_AvBin8x16: {
      UInt v_dst  = vregEnc(i->Pin.AvBin8x16.dst);
      UInt v_srcL = vregEnc(i->Pin.AvBin8x16.srcL);
      UInt v_srcR = vregEnc(i->Pin.AvBin8x16.srcR);
      UInt opc2;
      switch (i->Pin.AvBin8x16.op) {

      case Pav_ADDU:     opc2 =    0; break; 
      case Pav_QADDU:    opc2 =  512; break; 
      case Pav_QADDS:    opc2 =  768; break; 

      case Pav_SUBU:     opc2 = 1024; break; 
      case Pav_QSUBU:    opc2 = 1536; break; 
      case Pav_QSUBS:    opc2 = 1792; break; 

      case Pav_OMULU:   opc2 =    8; break; 
      case Pav_OMULS:   opc2 =  264; break; 
      case Pav_EMULU:   opc2 =  520; break; 
      case Pav_EMULS:   opc2 =  776; break; 

      case Pav_AVGU:     opc2 = 1026; break; 
      case Pav_AVGS:     opc2 = 1282; break; 
      case Pav_MAXU:     opc2 =    2; break; 
      case Pav_MAXS:     opc2 =  258; break; 
      case Pav_MINU:     opc2 =  514; break; 
      case Pav_MINS:     opc2 =  770; break; 

      case Pav_CMPEQU:   opc2 =    6; break; 
      case Pav_CMPGTU:   opc2 =  518; break; 
      case Pav_CMPGTS:   opc2 =  774; break; 

      case Pav_SHL:      opc2 =  260; break; 
      case Pav_SHR:      opc2 =  516; break; 
      case Pav_SAR:      opc2 =  772; break; 
      case Pav_ROTL:     opc2 =    4; break; 

      case Pav_MRGHI:    opc2 =   12; break; 
      case Pav_MRGLO:    opc2 =  268; break; 

      case Pav_POLYMULADD: opc2 = 1032; break; 

      default:
         goto bad;
      }
      p = mkFormVX( p, 4, v_dst, v_srcL, v_srcR, opc2, endness_host );
      goto done;
   }

   case Pin_AvBin16x8: {
      UInt v_dst  = vregEnc(i->Pin.AvBin16x8.dst);
      UInt v_srcL = vregEnc(i->Pin.AvBin16x8.srcL);
      UInt v_srcR = vregEnc(i->Pin.AvBin16x8.srcR);
      UInt opc2;
      switch (i->Pin.AvBin16x8.op) {

      case Pav_ADDU:    opc2 =   64; break; 
      case Pav_QADDU:   opc2 =  576; break; 
      case Pav_QADDS:   opc2 =  832; break; 

      case Pav_SUBU:    opc2 = 1088; break; 
      case Pav_QSUBU:   opc2 = 1600; break; 
      case Pav_QSUBS:   opc2 = 1856; break; 

      case Pav_OMULU:   opc2 =   72; break; 
      case Pav_OMULS:   opc2 =  328; break; 
      case Pav_EMULU:   opc2 =  584; break; 
      case Pav_EMULS:   opc2 =  840; break; 

      case Pav_AVGU:    opc2 = 1090; break; 
      case Pav_AVGS:    opc2 = 1346; break; 
      case Pav_MAXU:    opc2 =   66; break; 
      case Pav_MAXS:    opc2 =  322; break; 
      case Pav_MINS:    opc2 =  834; break; 
      case Pav_MINU:    opc2 =  578; break; 

      case Pav_CMPEQU:  opc2 =   70; break; 
      case Pav_CMPGTU:  opc2 =  582; break; 
      case Pav_CMPGTS:  opc2 =  838; break; 

      case Pav_SHL:     opc2 =  324; break; 
      case Pav_SHR:     opc2 =  580; break; 
      case Pav_SAR:     opc2 =  836; break; 
      case Pav_ROTL:    opc2 =   68; break; 

      case Pav_PACKUU:  opc2 =   14; break; 
      case Pav_QPACKUU: opc2 =  142; break; 
      case Pav_QPACKSU: opc2 =  270; break; 
      case Pav_QPACKSS: opc2 =  398; break; 
      case Pav_PACKPXL: opc2 =  782; break; 

      case Pav_MRGHI:   opc2 =   76; break; 
      case Pav_MRGLO:   opc2 =  332; break; 

      case Pav_POLYMULADD: opc2 = 1224; break; 

      default:
         goto bad;
      }
      p = mkFormVX( p, 4, v_dst, v_srcL, v_srcR, opc2, endness_host );
      goto done;
   }

   case Pin_AvBin32x4: {
      UInt v_dst  = vregEnc(i->Pin.AvBin32x4.dst);
      UInt v_srcL = vregEnc(i->Pin.AvBin32x4.srcL);
      UInt v_srcR = vregEnc(i->Pin.AvBin32x4.srcR);
      UInt opc2;
      switch (i->Pin.AvBin32x4.op) {

      case Pav_ADDU:    opc2 =  128; break; 
      case Pav_QADDU:   opc2 =  640; break; 
      case Pav_QADDS:   opc2 =  896; break; 

      case Pav_SUBU:    opc2 = 1152; break; 
      case Pav_QSUBU:   opc2 = 1664; break; 
      case Pav_QSUBS:   opc2 = 1920; break; 

      case Pav_MULU:    opc2 =  137; break; 
      case Pav_OMULU:   opc2 =  136; break; 
      case Pav_OMULS:   opc2 =  392; break; 
      case Pav_EMULU:   opc2 =  648; break; 
      case Pav_EMULS:   opc2 =  904; break; 

      case Pav_AVGU:    opc2 = 1154; break; 
      case Pav_AVGS:    opc2 = 1410; break; 

      case Pav_MAXU:    opc2 =  130; break; 
      case Pav_MAXS:    opc2 =  386; break; 

      case Pav_MINS:    opc2 =  898; break; 
      case Pav_MINU:    opc2 =  642; break; 

      case Pav_CMPEQU:  opc2 =  134; break; 
      case Pav_CMPGTS:  opc2 =  902; break; 
      case Pav_CMPGTU:  opc2 =  646; break; 

      case Pav_SHL:     opc2 =  388; break; 
      case Pav_SHR:     opc2 =  644; break; 
      case Pav_SAR:     opc2 =  900; break; 
      case Pav_ROTL:    opc2 =  132; break; 

      case Pav_PACKUU:  opc2 =   78; break; 
      case Pav_QPACKUU: opc2 =  206; break; 
      case Pav_QPACKSU: opc2 =  334; break; 
      case Pav_QPACKSS: opc2 =  462; break; 

      case Pav_MRGHI:   opc2 =  140; break; 
      case Pav_MRGLO:   opc2 =  396; break; 

      case Pav_CATODD:  opc2 = 1676; break; 
      case Pav_CATEVEN: opc2 = 1932; break; 

      case Pav_POLYMULADD: opc2 = 1160; break; 

      default:
         goto bad;
      }
      p = mkFormVX( p, 4, v_dst, v_srcL, v_srcR, opc2, endness_host );
      goto done;
   }

   case Pin_AvBin64x2: {
      UInt v_dst  = vregEnc(i->Pin.AvBin64x2.dst);
      UInt v_srcL = vregEnc(i->Pin.AvBin64x2.srcL);
      UInt v_srcR = vregEnc(i->Pin.AvBin64x2.srcR);
      UInt opc2;
      switch (i->Pin.AvBin64x2.op) {
      case Pav_ADDU:    opc2 =  192; break; 
      case Pav_SUBU:    opc2 = 1216; break; 
      case Pav_MAXU:    opc2 =  194; break; 
      case Pav_MAXS:    opc2 =  450; break; 
      case Pav_MINU:    opc2 =  706; break; 
      case Pav_MINS:    opc2 =  962; break; 
      case Pav_CMPEQU:  opc2 =  199; break; 
      case Pav_CMPGTU:  opc2 =  711; break; 
      case Pav_CMPGTS:  opc2 =  967; break; 
      case Pav_SHL:     opc2 = 1476; break; 
      case Pav_SHR:     opc2 = 1732; break; 
      case Pav_SAR:     opc2 =  964; break; 
      case Pav_ROTL:    opc2 =  196; break; 
      case Pav_PACKUU:  opc2 = 1102; break; 
      case Pav_QPACKUU: opc2 = 1230; break; 
      case Pav_QPACKSS: opc2 = 1486; break; 
      case Pav_MRGHI:   opc2 = 1614; break; 
      case Pav_MRGLO:   opc2 = 1742; break; 
      case Pav_POLYMULADD: opc2 = 1096; break; 
      default:
         goto bad;
      }
      p = mkFormVX( p, 4, v_dst, v_srcL, v_srcR, opc2, endness_host );
      goto done;
   }
   case Pin_AvCipherV128Unary: {
      UInt v_dst = vregEnc(i->Pin.AvCipherV128Unary.dst);
      UInt v_src = vregEnc(i->Pin.AvCipherV128Unary.src);
      UInt opc2;
      switch (i->Pin.AvCipherV128Unary.op) {
      case Pav_CIPHERSUBV128:   opc2 =  1480; break; 
      default:
         goto bad;
      }
      p = mkFormVX( p, 4, v_dst, v_src, 0, opc2, endness_host );
      goto done;
   }
   case Pin_AvCipherV128Binary: {
      UInt v_dst  = vregEnc(i->Pin.AvCipherV128Binary.dst);
      UInt v_srcL = vregEnc(i->Pin.AvCipherV128Binary.srcL);
      UInt v_srcR = vregEnc(i->Pin.AvCipherV128Binary.srcR);
      UInt opc2;
      switch (i->Pin.AvCipherV128Binary.op) {
      case Pav_CIPHERV128:     opc2 =  1288; break; 
      case Pav_CIPHERLV128:    opc2 =  1289; break; 
      case Pav_NCIPHERV128:    opc2 =  1352; break; 
      case Pav_NCIPHERLV128:   opc2 =  1353; break; 
      default:
         goto bad;
      }
      p = mkFormVX( p, 4, v_dst, v_srcL, v_srcR, opc2, endness_host );
      goto done;
   }
   case Pin_AvHashV128Binary: {
      UInt v_dst = vregEnc(i->Pin.AvHashV128Binary.dst);
      UInt v_src = vregEnc(i->Pin.AvHashV128Binary.src);
      PPCRI* s_field = i->Pin.AvHashV128Binary.s_field;
      UInt opc2;
      switch (i->Pin.AvHashV128Binary.op) {
      case Pav_SHA256:   opc2 =  1666; break; 
      case Pav_SHA512:   opc2 =  1730; break; 
      default:
         goto bad;
      }
      p = mkFormVX( p, 4, v_dst, v_src, s_field->Pri.Imm, opc2, endness_host );
      goto done;
   }
   case Pin_AvBCDV128Trinary: {
      UInt v_dst  = vregEnc(i->Pin.AvBCDV128Trinary.dst);
      UInt v_src1 = vregEnc(i->Pin.AvBCDV128Trinary.src1);
      UInt v_src2 = vregEnc(i->Pin.AvBCDV128Trinary.src2);
      PPCRI* ps   = i->Pin.AvBCDV128Trinary.ps;
      UInt opc2;
      switch (i->Pin.AvBCDV128Trinary.op) {
      case Pav_BCDAdd:   opc2 =  1; break; 
      case Pav_BCDSub:   opc2 = 65; break; 
      default:
         goto bad;
      }
      p = mkFormVXR( p, 4, v_dst, v_src1, v_src2,
                     0x1, (ps->Pri.Imm << 9) | opc2, endness_host );
      goto done;
   }
   case Pin_AvBin32Fx4: {
      UInt v_dst  = vregEnc(i->Pin.AvBin32Fx4.dst);
      UInt v_srcL = vregEnc(i->Pin.AvBin32Fx4.srcL);
      UInt v_srcR = vregEnc(i->Pin.AvBin32Fx4.srcR);
      switch (i->Pin.AvBin32Fx4.op) {

      case Pavfp_ADDF:
         p = mkFormVX( p, 4, v_dst, v_srcL, v_srcR, 10, endness_host );   
         break;
      case Pavfp_SUBF:
         p = mkFormVX( p, 4, v_dst, v_srcL, v_srcR, 74, endness_host );   
         break;
      case Pavfp_MAXF:
         p = mkFormVX( p, 4, v_dst, v_srcL, v_srcR, 1034, endness_host ); 
         break;
      case Pavfp_MINF:
         p = mkFormVX( p, 4, v_dst, v_srcL, v_srcR, 1098, endness_host ); 
         break;

      case Pavfp_MULF: {
         UInt vB = 29;  
                        
                        
         UInt konst = 0x1F;

         
         
         p = mkFormVX( p, 4, vB, konst, 0, 908, endness_host );

         
         p = mkFormVX( p, 4, vB, vB, vB, 388, endness_host );

         
         p = mkFormVA( p, 4, v_dst, v_srcL, vB, v_srcR, 46, endness_host );
         break;
      }
      case Pavfp_CMPEQF:  
         p = mkFormVXR( p, 4, v_dst, v_srcL, v_srcR, 0, 198, endness_host);
         break;
      case Pavfp_CMPGTF:  
         p = mkFormVXR( p, 4, v_dst, v_srcL, v_srcR, 0, 710, endness_host );
         break;
      case Pavfp_CMPGEF:  
         p = mkFormVXR( p, 4, v_dst, v_srcL, v_srcR, 0, 454, endness_host );
         break;

      default:
         goto bad;
      }
      goto done;
   }

   case Pin_AvUn32Fx4: {
      UInt v_dst = vregEnc(i->Pin.AvUn32Fx4.dst);
      UInt v_src = vregEnc(i->Pin.AvUn32Fx4.src);
      UInt opc2;
      switch (i->Pin.AvUn32Fx4.op) {
      case Pavfp_RCPF:    opc2 =  266; break; 
      case Pavfp_RSQRTF:  opc2 =  330; break; 
      case Pavfp_CVTU2F:  opc2 =  778; break; 
      case Pavfp_CVTS2F:  opc2 =  842; break; 
      case Pavfp_QCVTF2U: opc2 =  906; break; 
      case Pavfp_QCVTF2S: opc2 =  970; break; 
      case Pavfp_ROUNDM:  opc2 =  714; break; 
      case Pavfp_ROUNDP:  opc2 =  650; break; 
      case Pavfp_ROUNDN:  opc2 =  522; break; 
      case Pavfp_ROUNDZ:  opc2 =  586; break; 
      default:
         goto bad;
      }
      p = mkFormVX( p, 4, v_dst, 0, v_src, opc2, endness_host );
      goto done;
   }

   case Pin_AvPerm: {  
      UInt v_dst  = vregEnc(i->Pin.AvPerm.dst);
      UInt v_srcL = vregEnc(i->Pin.AvPerm.srcL);
      UInt v_srcR = vregEnc(i->Pin.AvPerm.srcR);
      UInt v_ctl  = vregEnc(i->Pin.AvPerm.ctl);
      p = mkFormVA( p, 4, v_dst, v_srcL, v_srcR, v_ctl, 43, endness_host );
      goto done;
   }

   case Pin_AvSel: {  
      UInt v_ctl  = vregEnc(i->Pin.AvSel.ctl);
      UInt v_dst  = vregEnc(i->Pin.AvSel.dst);
      UInt v_srcL = vregEnc(i->Pin.AvSel.srcL);
      UInt v_srcR = vregEnc(i->Pin.AvSel.srcR);
      p = mkFormVA( p, 4, v_dst, v_srcL, v_srcR, v_ctl, 42, endness_host );
      goto done;
   }

   case Pin_AvSh: {  
      UInt v_dst  = vregEnc(i->Pin.AvSh.dst);
      Bool  idxd = toBool(i->Pin.AvSh.addr->tag == Pam_RR);
      UInt r_idx, r_base;

      r_base = iregEnc(i->Pin.AvSh.addr->Pam.RR.base, mode64);

      if (!idxd) {
         r_idx = 30; 
         p = mkLoadImm(p, r_idx,
                       i->Pin.AvSh.addr->Pam.IR.index, mode64, endness_host);
      } else {
         r_idx  = iregEnc(i->Pin.AvSh.addr->Pam.RR.index, mode64);
      }

      if (i->Pin.AvSh.shLeft)
         
         p = mkFormVXI( p, 31, v_dst, r_idx, r_base, 6, endness_host );
      else
         
         p = mkFormVXI( p, 31, v_dst, r_idx, r_base, 38, endness_host );
      goto done;
   }

   case Pin_AvShlDbl: {  
      UInt shift  = i->Pin.AvShlDbl.shift;
      UInt v_dst  = vregEnc(i->Pin.AvShlDbl.dst);
      UInt v_srcL = vregEnc(i->Pin.AvShlDbl.srcL);
      UInt v_srcR = vregEnc(i->Pin.AvShlDbl.srcR);
      vassert(shift <= 0xF);
      p = mkFormVA( p, 4, v_dst, v_srcL, v_srcR, shift, 44, endness_host );
      goto done;
   }

   case Pin_AvSplat: { 
      UInt v_dst = vregEnc(i->Pin.AvShlDbl.dst);
      UChar sz   = i->Pin.AvSplat.sz;
      UInt v_src, opc2;
      vassert(sz == 8 || sz == 16 || sz == 32);

      if (i->Pin.AvSplat.src->tag == Pvi_Imm) {
         Char simm5;
         opc2 = (sz == 8) ? 780 : (sz == 16) ? 844 : 908;   
         
         simm5 = i->Pin.AvSplat.src->Pvi.Imm5s;
         vassert(simm5 >= -16 && simm5 <= 15);
         simm5 = simm5 & 0x1F;
         p = mkFormVX( p, 4, v_dst, (UInt)simm5, 0, opc2, endness_host );
      }
      else {  
         UInt lowest_lane;
         opc2 = (sz == 8) ? 524 : (sz == 16) ? 588 : 652;  
         vassert(hregClass(i->Pin.AvSplat.src->Pvi.Reg) == HRcVec128);
         v_src = vregEnc(i->Pin.AvSplat.src->Pvi.Reg);
         lowest_lane = (128/sz)-1;
         p = mkFormVX( p, 4, v_dst, lowest_lane, v_src, opc2, endness_host );
      }
      goto done;
   }

   case Pin_AvCMov: {
      UInt v_dst     = vregEnc(i->Pin.AvCMov.dst);
      UInt v_src     = vregEnc(i->Pin.AvCMov.src);
      PPCCondCode cc = i->Pin.AvCMov.cond;

      if (v_dst == v_src) goto done;
      
      vassert(cc.test != Pct_ALWAYS);

      
      if (cc.test != Pct_ALWAYS) {
         
         p = mkFormB(p, invertCondTest(cc.test), cc.flag, 8>>2, 0, 0,
                     endness_host);
      }
      
      p = mkFormVX( p, 4, v_dst, v_src, v_src, 1156, endness_host );
      goto done;
   }

   case Pin_AvLdVSCR: {  
      UInt v_src = vregEnc(i->Pin.AvLdVSCR.src);
      p = mkFormVX( p, 4, 0, 0, v_src, 1604, endness_host );
      goto done;
   }

   case Pin_Dfp64Unary: {
      UInt fr_dst = fregEnc( i->Pin.FpUnary.dst );
      UInt fr_src = fregEnc( i->Pin.FpUnary.src );

      switch (i->Pin.Dfp64Unary.op) {
      case Pfp_MOV: 
         p = mkFormX( p, 63, fr_dst, 0, fr_src, 72, 0, endness_host );
         break;
      case Pfp_DCTDP:   
         p = mkFormX( p, 59, fr_dst, 0, fr_src, 258, 0, endness_host );
         break;
      case Pfp_DRSP:    
         p = mkFormX( p, 59, fr_dst, 0, fr_src, 770, 0, endness_host );
         break;
      case Pfp_DCFFIX:   
         
         p = mkFormX( p, 59, fr_dst, 0, fr_src, 802, 0, endness_host );
         break;
      case Pfp_DCTFIX:   
         p = mkFormX( p, 59, fr_dst, 0, fr_src, 290, 0, endness_host );
         break;
      case Pfp_DXEX:     
         p = mkFormX( p, 59, fr_dst, 0, fr_src, 354, 0, endness_host );
         break;                                
      default:
         goto bad;
      }
      goto done;
   }

   case Pin_Dfp64Binary: {
      UInt fr_dst = fregEnc( i->Pin.Dfp64Binary.dst );
      UInt fr_srcL = fregEnc( i->Pin.Dfp64Binary.srcL );
      UInt fr_srcR = fregEnc( i->Pin.Dfp64Binary.srcR );
      switch (i->Pin.Dfp64Binary.op) {
      case Pfp_DFPADD: 
         p = mkFormX( p, 59, fr_dst, fr_srcL, fr_srcR, 2, 0, endness_host );
         break;
      case Pfp_DFPSUB: 
         p = mkFormX( p, 59, fr_dst, fr_srcL, fr_srcR, 514, 0, endness_host );
         break;
      case Pfp_DFPMUL: 
         p = mkFormX( p, 59, fr_dst, fr_srcL, fr_srcR, 34, 0, endness_host );
         break;
      case Pfp_DFPDIV: 
         p = mkFormX( p, 59, fr_dst, fr_srcL, fr_srcR, 546, 0, endness_host );
         break;
      case Pfp_DIEX:  
         p = mkFormX( p, 59, fr_dst, fr_srcL, fr_srcR, 866, 0, endness_host );
         break;
      default:
         goto bad;
      }
      goto done;
   }

   case Pin_DfpShift: {
      UInt fr_src = fregEnc(i->Pin.DfpShift.src);
      UInt fr_dst = fregEnc(i->Pin.DfpShift.dst);
      UInt shift;

      shift =  i->Pin.DfpShift.shift->Pri.Imm;

      switch (i->Pin.DfpShift.op) {
      case Pfp_DSCLI:    
         p = mkFormZ22( p, 59, fr_dst, fr_src, shift,  66, 0, endness_host );
         break;
      case Pfp_DSCRI:    
         p = mkFormZ22( p, 59, fr_dst, fr_src, shift,  98, 0, endness_host );
         break;
      default:
         vex_printf("ERROR: emit_PPCInstr default case\n");
         goto bad;
      }
      goto done;
   }

   case Pin_ExtractExpD128: {
      UInt fr_dst   = fregEnc(i->Pin.ExtractExpD128.dst);
      UInt fr_srcHi = fregEnc(i->Pin.ExtractExpD128.src_hi);
      UInt fr_srcLo = fregEnc(i->Pin.ExtractExpD128.src_lo);

      switch (i->Pin.ExtractExpD128.op) {
      case Pfp_DXEXQ:                                                          
         p = mkFormX( p, 63, 12, 0, fr_srcHi, 72, 0, endness_host );
         p = mkFormX( p, 63, 13, 0, fr_srcLo, 72, 0, endness_host );
         p = mkFormX( p, 63, 10, 0, 12, 354, 0, endness_host );

         p = mkFormX(p, 63, fr_dst, 0, 10,  72, 0, endness_host);
         break;
      default:
         vex_printf("Error: emit_PPCInstr case Pin_DfpExtractExp, case inst Default\n");
         goto bad;
      }
      goto done;
   }
   case Pin_Dfp128Unary: {
     UInt fr_dstHi = fregEnc(i->Pin.Dfp128Unary.dst_hi);
     UInt fr_dstLo = fregEnc(i->Pin.Dfp128Unary.dst_lo);
     UInt fr_srcLo = fregEnc(i->Pin.Dfp128Unary.src_lo);

     switch (i->Pin.Dfp128Unary.op) {
     case Pfp_DCTQPQ: 
        p = mkFormX( p, 63, 12, 0, fr_srcLo, 72, 0, endness_host );

        p = mkFormX( p, 63, 10, 0, 12, 258, 0, endness_host );

        p = mkFormX(p, 63, fr_dstHi, 0, 10,  72, 0, endness_host);
        p = mkFormX(p, 63, fr_dstLo, 0, 11,  72, 0, endness_host);
        break;
     default:
        vex_printf("Error: emit_PPCInstr case Pin_Dfp128Unary, case inst Default\
\n");
        goto bad;
     }
     goto done;
   }

   case Pin_Dfp128Binary: {
      UInt fr_dstHi = fregEnc( i->Pin.Dfp128Binary.dst_hi );
      UInt fr_dstLo = fregEnc( i->Pin.Dfp128Binary.dst_lo );
      UInt fr_srcRHi = fregEnc( i->Pin.Dfp128Binary.srcR_hi );
      UInt fr_srcRLo = fregEnc( i->Pin.Dfp128Binary.srcR_lo );

      p = mkFormX( p, 63, 10, 0, fr_dstHi, 72, 0, endness_host );
      p = mkFormX( p, 63, 11, 0, fr_dstLo, 72, 0, endness_host );
      p = mkFormX( p, 63, 12, 0, fr_srcRHi, 72, 0, endness_host );
      p = mkFormX( p, 63, 13, 0, fr_srcRLo, 72, 0, endness_host );

      switch (i->Pin.Dfp128Binary.op) {
      case Pfp_DFPADDQ:
         p = mkFormX( p, 63, 10, 10, 12, 2, 0, endness_host );
         break;
      case Pfp_DFPSUBQ:
         p = mkFormX( p, 63, 10, 10, 12, 514, 0, endness_host );
         break;
      case Pfp_DFPMULQ:
         p = mkFormX( p, 63, 10, 10, 12, 34, 0, endness_host );
         break;
      case Pfp_DFPDIVQ:
         p = mkFormX( p, 63, 10, 10, 12, 546, 0, endness_host );
         break;
      default:
         goto bad;
      }

      p = mkFormX(p, 63, fr_dstHi, 0, 10,  72, 0, endness_host);
      p = mkFormX(p, 63, fr_dstLo, 0, 11,  72, 0, endness_host);
      goto done;
   }

   case Pin_DfpShift128: {
      UInt fr_src_hi = fregEnc(i->Pin.DfpShift128.src_hi);
      UInt fr_src_lo = fregEnc(i->Pin.DfpShift128.src_lo);
      UInt fr_dst_hi = fregEnc(i->Pin.DfpShift128.dst_hi);
      UInt fr_dst_lo = fregEnc(i->Pin.DfpShift128.dst_lo);
      UInt shift;

      shift =  i->Pin.DfpShift128.shift->Pri.Imm;

      
      p = mkFormX(p, 63, 12, 0, fr_src_hi, 72, 0, endness_host);
      p = mkFormX(p, 63, 13, 0, fr_src_lo, 72, 0, endness_host);

      
      switch (i->Pin.DfpShift128.op) {
      case Pfp_DSCLIQ:    
         p = mkFormZ22( p, 63, 10, 12, shift,  66, 0, endness_host );
         break;
      case Pfp_DSCRIQ:    
         p = mkFormZ22( p, 63, 10, 12, shift,  98, 0, endness_host );
         break;
      default:
         vex_printf("ERROR: emit_PPCInstr quad default case %d \n",
                    i->Pin.DfpShift128.op);
         goto bad;
      }

      p = mkFormX(p, 63, fr_dst_hi, 0, 10,  72, 0, endness_host);
      p = mkFormX(p, 63, fr_dst_lo, 0, 11,  72, 0, endness_host);
      goto done;
   }

   case Pin_DfpRound: {
      UInt fr_dst = fregEnc(i->Pin.DfpRound.dst);
      UInt fr_src = fregEnc(i->Pin.DfpRound.src);
      UInt r_rmc, r, rmc;

      r_rmc =  i->Pin.DfpRound.r_rmc->Pri.Imm;
      r = (r_rmc & 0x8) >> 3;
      rmc = r_rmc & 0x3;

      
      p = mkFormZ23(p, 59, fr_dst, r, fr_src, rmc, 99, 0, endness_host);
      goto done;
   }

   case Pin_DfpRound128: {
      UInt fr_dstHi = fregEnc(i->Pin.DfpRound128.dst_hi);
      UInt fr_dstLo = fregEnc(i->Pin.DfpRound128.dst_lo);
      UInt fr_srcHi = fregEnc(i->Pin.DfpRound128.src_hi);
      UInt fr_srcLo = fregEnc(i->Pin.DfpRound128.src_lo);
      UInt r_rmc, r, rmc;

      r_rmc =  i->Pin.DfpRound128.r_rmc->Pri.Imm;
      r = (r_rmc & 0x8) >> 3;
      rmc = r_rmc & 0x3;

      p = mkFormX(p, 63, 12, 0, fr_srcHi, 72, 0, endness_host);
      p = mkFormX(p, 63, 13, 0, fr_srcLo, 72, 0, endness_host);

      p = mkFormZ23(p, 63, 10, r, 12, rmc, 99, 0, endness_host);

      p = mkFormX(p, 63, fr_dstHi, 0, 10,  72, 0, endness_host);
      p = mkFormX(p, 63, fr_dstLo, 0, 11,  72, 0, endness_host);
      goto done;
   }

   case Pin_DfpQuantize: {
      UInt fr_dst  = fregEnc(i->Pin.DfpQuantize.dst);
      UInt fr_srcL = fregEnc(i->Pin.DfpQuantize.srcL);
      UInt fr_srcR = fregEnc(i->Pin.DfpQuantize.srcR);
      UInt rmc;

      rmc =  i->Pin.DfpQuantize.rmc->Pri.Imm;

      switch (i->Pin.DfpQuantize.op) {
      case Pfp_DQUA:
         p = mkFormZ23(p, 59, fr_dst, fr_srcL, fr_srcR, rmc, 3, 0, endness_host);
         break;
      case Pfp_RRDTR:
         p = mkFormZ23(p, 59, fr_dst, fr_srcL, fr_srcR, rmc, 35, 0, endness_host);
         break;
      default:
         break;
      }
      goto done;
   }

   case Pin_DfpQuantize128: {
      UInt fr_dst_hi = fregEnc(i->Pin.DfpQuantize128.dst_hi);
      UInt fr_dst_lo = fregEnc(i->Pin.DfpQuantize128.dst_lo);
      UInt fr_src_hi = fregEnc(i->Pin.DfpQuantize128.src_hi);
      UInt fr_src_lo = fregEnc(i->Pin.DfpQuantize128.src_lo);
      UInt rmc;

      rmc =  i->Pin.DfpQuantize128.rmc->Pri.Imm;
      p = mkFormX(p, 63, 10, 0, fr_dst_hi, 72, 0, endness_host);
      p = mkFormX(p, 63, 11, 0, fr_dst_lo, 72, 0, endness_host);
      p = mkFormX(p, 63, 12, 0, fr_src_hi, 72, 0, endness_host);
      p = mkFormX(p, 63, 13, 0, fr_src_lo, 72, 0, endness_host);

      switch (i->Pin.DfpQuantize128.op) {
      case Pfp_DQUAQ:
         p = mkFormZ23(p, 63, 10, 10, 12, rmc, 3, 0, endness_host);
         break;
      case Pfp_DRRNDQ:
         p = mkFormZ23(p, 63, 10, 10, 12, rmc, 35, 0, endness_host);
         break;
      default:
         vpanic("Pin_DfpQuantize128: default case, couldn't find inst to issue \n");
         break;
      }

      p = mkFormX(p, 63, fr_dst_hi, 0, 10,  72, 0, endness_host);
      p = mkFormX(p, 63, fr_dst_lo, 0, 11,  72, 0, endness_host);
      goto done;
   }

   case Pin_DfpD128toD64: {
      UInt fr_dst   = fregEnc( i->Pin.DfpD128toD64.dst );
      UInt fr_srcHi = fregEnc( i->Pin.DfpD128toD64.src_hi );
      UInt fr_srcLo = fregEnc( i->Pin.DfpD128toD64.src_lo );

      p = mkFormX( p, 63, 10, 0, fr_dst, 72, 0, endness_host );
      p = mkFormX( p, 63, 12, 0, fr_srcHi, 72, 0, endness_host );
      p = mkFormX( p, 63, 13, 0, fr_srcLo, 72, 0, endness_host );

      
      switch (i->Pin.Dfp128Binary.op) {
      case Pfp_DRDPQ:
         p = mkFormX( p, 63, 10, 0, 12, 770, 0, endness_host );
         break;
      case Pfp_DCTFIXQ:
         p = mkFormX( p, 63, 10, 0, 12, 290, 0, endness_host );
         break;
      default:
         goto bad;
      }

      
      p = mkFormX(p, 63, fr_dst, 0, 10,  72, 0, endness_host);
      goto done;
   }

   case Pin_DfpI64StoD128: {
      UInt fr_dstHi = fregEnc( i->Pin.DfpI64StoD128.dst_hi );
      UInt fr_dstLo = fregEnc( i->Pin.DfpI64StoD128.dst_lo );
      UInt fr_src   = fregEnc( i->Pin.DfpI64StoD128.src );

      switch (i->Pin.Dfp128Binary.op) {
      case Pfp_DCFFIXQ:
         p = mkFormX( p, 63, 10, 11, fr_src, 802, 0, endness_host );
         break;
      default:
         goto bad;
      }

      
      p = mkFormX(p, 63, fr_dstHi, 0, 10,  72, 0, endness_host);
      p = mkFormX(p, 63, fr_dstLo, 0, 11,  72, 0, endness_host);
      goto done;
   }

   case Pin_InsertExpD128: {
      UInt fr_dstHi  = fregEnc(i->Pin.InsertExpD128.dst_hi);
      UInt fr_dstLo  = fregEnc(i->Pin.InsertExpD128.dst_lo);
      UInt fr_srcL   = fregEnc(i->Pin.InsertExpD128.srcL);
      UInt fr_srcRHi = fregEnc(i->Pin.InsertExpD128.srcR_hi);
      UInt fr_srcRLo = fregEnc(i->Pin.InsertExpD128.srcR_lo);

      p = mkFormX(p, 63, 10, 0, fr_srcL, 72, 0, endness_host);
      p = mkFormX(p, 63, 12, 0, fr_srcRHi, 72, 0, endness_host);
      p = mkFormX(p, 63, 13, 0, fr_srcRLo, 72, 0, endness_host);
      p = mkFormX(p, 63, 10, 10, 12, 866, 0, endness_host );

      p = mkFormX(p, 63, fr_dstHi, 0, 10,  72, 0, endness_host);
      p = mkFormX(p, 63, fr_dstLo, 0, 11,  72, 0, endness_host);
      goto done;
   }                                                                           

   case Pin_Dfp64Cmp:{
      UChar crfD    = 1;
      UInt  r_dst   = iregEnc(i->Pin.Dfp64Cmp.dst, mode64);
      UInt  fr_srcL = fregEnc(i->Pin.Dfp64Cmp.srcL);
      UInt  fr_srcR = fregEnc(i->Pin.Dfp64Cmp.srcR);
      vassert(crfD < 8);
      
      p = mkFormX(p, 59, crfD<<2, fr_srcL, fr_srcR, 130, 0, endness_host);

      
      p = mkFormX(p, 31, r_dst, 0, 0, 19, 0, endness_host);

      
      
      p = mkFormM(p, 21, r_dst, r_dst, 8, 28, 31, 0, endness_host);
      goto done;
   }

   case Pin_Dfp128Cmp: {
      UChar crfD       = 1;
      UInt  r_dst      = iregEnc(i->Pin.Dfp128Cmp.dst, mode64);
      UInt  fr_srcL_hi = fregEnc(i->Pin.Dfp128Cmp.srcL_hi);
      UInt  fr_srcL_lo = fregEnc(i->Pin.Dfp128Cmp.srcL_lo);
      UInt  fr_srcR_hi = fregEnc(i->Pin.Dfp128Cmp.srcR_hi);
      UInt  fr_srcR_lo = fregEnc(i->Pin.Dfp128Cmp.srcR_lo);
      vassert(crfD < 8);
      
      p = mkFormX(p, 63, 10, 0, fr_srcL_hi, 72, 0, endness_host);
      p = mkFormX(p, 63, 11, 0, fr_srcL_lo, 72, 0, endness_host);
      p = mkFormX(p, 63, 12, 0, fr_srcR_hi, 72, 0, endness_host);
      p = mkFormX(p, 63, 13, 0, fr_srcR_lo, 72, 0, endness_host);

      p = mkFormX(p, 63, crfD<<2, 10, 12, 130, 0, endness_host);

      
      p = mkFormX(p, 31, r_dst, 0, 0, 19, 0, endness_host);

      
      
      p = mkFormM(p, 21, r_dst, r_dst, 8, 28, 31, 0, endness_host);
      goto done;
   }

   case Pin_EvCheck: {
      UChar* p0 = p;
      
      p = do_load_or_store_word32(p, True, 30,
                                  i->Pin.EvCheck.amCounter, mode64,
                                  endness_host);
      
      p = emit32(p, 0x37DEFFFF, endness_host);
      
      p = do_load_or_store_word32(p, False, 30,
                                  i->Pin.EvCheck.amCounter, mode64,
                                  endness_host);
      
      p = emit32(p, 0x40800010, endness_host);
      
      p = do_load_or_store_machine_word(p, True, 30,
                                        i->Pin.EvCheck.amFailAddr, mode64,
                                        endness_host);
      
      p = mkFormXFX(p, 30, 9, 467, endness_host);
      
      p = mkFormXL(p, 19, Pct_ALWAYS, 0, 0, 528, 0, endness_host);
      

      
      vassert(evCheckSzB_PPC() == (UChar*)p - (UChar*)p0);
      goto done;
   }

   case Pin_ProfInc: {
      if (mode64) {
         p = mkLoadImm_EXACTLY2or5(
                p, 30, 0x6555655565556555ULL, True, endness_host);
         p = emit32(p, 0xEBBE0000, endness_host);
         p = emit32(p, 0x3BBD0001, endness_host);
         p = emit32(p, 0xFBBE0000, endness_host);
      } else {
         p = mkLoadImm_EXACTLY2or5(
                p, 30, 0x65556555ULL, False, endness_host);
         p = emit32(p, 0x83BE0004, endness_host);
         p = emit32(p, 0x37BD0001, endness_host);
         p = emit32(p, 0x93BE0004, endness_host);
         p = emit32(p, 0x83BE0000, endness_host);
         p = emit32(p, 0x7FBD0194, endness_host);
         p = emit32(p, 0x93BE0000, endness_host);
      }
      
      vassert(!(*is_profInc));
      *is_profInc = True;
      goto done;
   }

   default: 
      goto bad;
   }

  bad:
   vex_printf("\n=> ");
   ppPPCInstr(i, mode64);
   vpanic("emit_PPCInstr");
   
   
  done:
   vassert(p - &buf[0] <= 64);
   return p - &buf[0];
}


Int evCheckSzB_PPC (void)
{
  return 28;
}


VexInvalRange chainXDirect_PPC ( VexEndness endness_host,
                                 void* place_to_chain,
                                 const void* disp_cp_chain_me_EXPECTED,
                                 const void* place_to_jump_to,
                                 Bool  mode64 )
{
   if (mode64) {
      vassert((endness_host == VexEndnessBE) ||
              (endness_host == VexEndnessLE));
   } else {
      vassert(endness_host == VexEndnessBE);
   }

   UChar* p = (UChar*)place_to_chain;
   vassert(0 == (3 & (HWord)p));
   vassert(isLoadImm_EXACTLY2or5(p, 30,
                                 (Addr)disp_cp_chain_me_EXPECTED,
                                 mode64, endness_host));
   vassert(fetch32(p + (mode64 ? 20 : 8) + 0, endness_host) == 0x7FC903A6);
   vassert(fetch32(p + (mode64 ? 20 : 8) + 4, endness_host) == 0x4E800421);
   p = mkLoadImm_EXACTLY2or5(p, 30,
                             (Addr)place_to_jump_to, mode64, 
                             endness_host);
   p = emit32(p, 0x7FC903A6, endness_host);
   p = emit32(p, 0x4E800420, endness_host);

   Int len = p - (UChar*)place_to_chain;
   vassert(len == (mode64 ? 28 : 16)); 
   VexInvalRange vir = {(HWord)place_to_chain, len};
   return vir;
}


VexInvalRange unchainXDirect_PPC ( VexEndness endness_host,
                                   void* place_to_unchain,
                                   const void* place_to_jump_to_EXPECTED,
                                   const void* disp_cp_chain_me,
                                   Bool  mode64 )
{
   if (mode64) {
      vassert((endness_host == VexEndnessBE) ||
              (endness_host == VexEndnessLE));
   } else {
      vassert(endness_host == VexEndnessBE);
   }

   UChar* p = (UChar*)place_to_unchain;
   vassert(0 == (3 & (HWord)p));
   vassert(isLoadImm_EXACTLY2or5(p, 30,
                                 (Addr)place_to_jump_to_EXPECTED,
                                 mode64, endness_host));
   vassert(fetch32(p + (mode64 ? 20 : 8) + 0, endness_host) == 0x7FC903A6);
   vassert(fetch32(p + (mode64 ? 20 : 8) + 4, endness_host) == 0x4E800420);
   p = mkLoadImm_EXACTLY2or5(p, 30,
                             (Addr)disp_cp_chain_me, mode64, 
                             endness_host);
   p = emit32(p, 0x7FC903A6, endness_host);
   p = emit32(p, 0x4E800421, endness_host);

   Int len = p - (UChar*)place_to_unchain;
   vassert(len == (mode64 ? 28 : 16)); 
   VexInvalRange vir = {(HWord)place_to_unchain, len};
   return vir;
}


VexInvalRange patchProfInc_PPC ( VexEndness endness_host,
                                 void*  place_to_patch,
                                 const ULong* location_of_counter,
                                 Bool   mode64 )
{
   if (mode64) {
      vassert((endness_host == VexEndnessBE) ||
              (endness_host == VexEndnessLE));
   } else {
      vassert(endness_host == VexEndnessBE);
   }

   UChar* p = (UChar*)place_to_patch;
   vassert(0 == (3 & (HWord)p));

   Int len = 0;
   if (mode64) {
      vassert(isLoadImm_EXACTLY2or5(p, 30,
                                    0x6555655565556555ULL, True,
                                    endness_host));
      vassert(fetch32(p + 20, endness_host) == 0xEBBE0000);
      vassert(fetch32(p + 24, endness_host) == 0x3BBD0001);
      vassert(fetch32(p + 28, endness_host) == 0xFBBE0000);
      p = mkLoadImm_EXACTLY2or5(p, 30,
                                (Addr)location_of_counter,
                                True, endness_host);
      len = p - (UChar*)place_to_patch;
      vassert(len == 20);
   } else {
      vassert(isLoadImm_EXACTLY2or5(p, 30,
                                    0x65556555ULL, False, 
                                    endness_host));
      vassert(fetch32(p +  8, endness_host) == 0x83BE0004);
      vassert(fetch32(p + 12, endness_host) == 0x37BD0001);
      vassert(fetch32(p + 16, endness_host) == 0x93BE0004);
      vassert(fetch32(p + 20, endness_host) == 0x83BE0000);
      vassert(fetch32(p + 24, endness_host) == 0x7FBD0194);
      vassert(fetch32(p + 28, endness_host) == 0x93BE0000);
      p = mkLoadImm_EXACTLY2or5(p, 30,
                                (Addr)location_of_counter,
                                False, endness_host);
      len = p - (UChar*)place_to_patch;
      vassert(len == 8);
   }
   VexInvalRange vir = {(HWord)place_to_patch, len};
   return vir;
}


