

/*
  This file is part of Valgrind, a dynamic binary instrumentation
  framework.

  Copyright (C) 2010-2013  Tilera Corp.

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



#include "libvex_basictypes.h"
#include "libvex_ir.h"
#include "libvex.h"
#include "libvex_guest_tilegx.h"

#include "main_util.h"
#include "main_globals.h"
#include "guest_generic_bb_to_IR.h"
#include "guest_tilegx_defs.h"
#include "tilegx_disasm.h"



static VexEndness host_endness;

static UChar *guest_code;

static Addr64 guest_PC_bbstart;

static Addr64 guest_PC_curr_instr;

static IRSB *irsb;


#define DIP(format, args...)                    \
  if (vex_traceflags & VEX_TRACE_FE)            \
    vex_printf(format, ## args)


static Int integerGuestRegOffset ( UInt iregNo )
{
  return 8 * (iregNo);
}



static IRExpr *mkU8 ( UInt i )
{
  return IRExpr_Const(IRConst_U8((UChar) i));
}

static IRExpr *mkU32 ( UInt i )
{
  return IRExpr_Const(IRConst_U32(i));
}

static IRExpr *mkU64 ( ULong i )
{
  return IRExpr_Const(IRConst_U64(i));
}

static IRExpr *mkexpr ( IRTemp tmp )
{
  return IRExpr_RdTmp(tmp);
}

static IRExpr *unop ( IROp op, IRExpr * a )
{
  return IRExpr_Unop(op, a);
}

static IRExpr *binop ( IROp op, IRExpr * a1, IRExpr * a2 )
{
  return IRExpr_Binop(op, a1, a2);
}

static IRExpr *load ( IRType ty, IRExpr * addr )
{
  IRExpr *load1 = NULL;

  load1 = IRExpr_Load(Iend_LE, ty, addr);
  return load1;
}

static void stmt ( IRStmt * st )
{
  addStmtToIRSB(irsb, st);
}

#define OFFB_PC     offsetof(VexGuestTILEGXState, guest_pc)

static void putPC ( IRExpr * e )
{
  stmt(IRStmt_Put(OFFB_PC, e));
}

static void assign ( IRTemp dst, IRExpr * e )
{
  stmt(IRStmt_WrTmp(dst, e));
}

static void store ( IRExpr * addr, IRExpr * data )
{
  stmt(IRStmt_Store(Iend_LE, addr, data));
}

static IRTemp newTemp ( IRType ty )
{
  vassert(isPlausibleIRType(ty));
  return newIRTemp(irsb->tyenv, ty);
}

static ULong extend_s_16to64 ( UInt x )
{
  return (ULong) ((((Long) x) << 48) >> 48);
}

static ULong extend_s_8to64 ( UInt x )
{
  return (ULong) ((((Long) x) << 56) >> 56);
}

static IRExpr *getIReg ( UInt iregNo )
{
  IRType ty = Ity_I64;
  if(!(iregNo < 56 || iregNo == 63 ||
       (iregNo >= 70 && iregNo <= 73))) {
    vex_printf("iregNo=%d\n", iregNo);
    vassert(0);
  }
  return IRExpr_Get(integerGuestRegOffset(iregNo), ty);
}

static void putIReg ( UInt archreg, IRExpr * e )
{
  IRType ty = Ity_I64;
  if(!(archreg < 56 || archreg == 63 || archreg == 70 ||
       archreg == 72 || archreg == 73)) {
    vex_printf("archreg=%d\n", archreg);
    vassert(0);
  }
  vassert(typeOfIRExpr(irsb->tyenv, e) == ty);
  if (archreg != 63)
    stmt(IRStmt_Put(integerGuestRegOffset(archreg), e));
}

static IRExpr *narrowTo ( IRType dst_ty, IRExpr * e )
{
  IRType src_ty = typeOfIRExpr(irsb->tyenv, e);
  if (src_ty == dst_ty)
    return e;
  if (src_ty == Ity_I32 && dst_ty == Ity_I16)
    return unop(Iop_32to16, e);
  if (src_ty == Ity_I32 && dst_ty == Ity_I8)
    return unop(Iop_32to8, e);

  if (src_ty == Ity_I64 && dst_ty == Ity_I8) {
    return unop(Iop_64to8, e);
  }
  if (src_ty == Ity_I64 && dst_ty == Ity_I16) {
    return unop(Iop_64to16, e);
  }
  if (src_ty == Ity_I64 && dst_ty == Ity_I32) {
    return unop(Iop_64to32, e);
  }

  if (vex_traceflags & VEX_TRACE_FE) {
    vex_printf("\nsrc, dst tys are: ");
    ppIRType(src_ty);
    vex_printf(", ");
    ppIRType(dst_ty);
    vex_printf("\n");
  }
  vpanic("narrowTo(tilegx)");
  return e;
}

#define signExtend(_e, _n)                                              \
  ((_n == 32) ?                                                         \
   unop(Iop_32Sto64, _e) :                                              \
   ((_n == 16) ?                                                        \
    unop(Iop_16Sto64, _e) :						\
    (binop(Iop_Sar64, binop(Iop_Shl64, _e, mkU8(63 - (_n))), mkU8(63 - (_n))))))

static IRStmt* dis_branch ( IRExpr* guard, ULong imm )
{
  IRTemp t0;

  t0 = newTemp(Ity_I1);
  assign(t0, guard);
  return IRStmt_Exit(mkexpr(t0), Ijk_Boring,
                     IRConst_U64(imm), OFFB_PC);
}

#define  MARK_REG_WB(_rd, _td)                  \
  do {                                          \
    vassert(rd_wb_index < 6);                   \
    rd_wb_temp[rd_wb_index] = _td;              \
    rd_wb_reg[rd_wb_index] = _rd;               \
    rd_wb_index++;                              \
  } while(0)


static DisResult disInstr_TILEGX_WRK ( Bool(*resteerOkFn) (void *, Addr),
                                       Bool resteerCisOk,
                                       void *callback_opaque,
                                       Long delta64,
                                       const VexArchInfo * archinfo,
                                       const VexAbiInfo * abiinfo,
                                       Bool sigill_diag )
{
  struct tilegx_decoded_instruction
    decoded[TILEGX_MAX_INSTRUCTIONS_PER_BUNDLE];
  ULong  cins, opcode = -1, rd, ra, rb, imm = 0;
  ULong  opd[4];
  ULong  opd_src_map, opd_dst_map, opd_imm_map;
  Int    use_dirty_helper;
  IRTemp t0, t1, t2, t3, t4;
  IRTemp tb[4];
  IRTemp rd_wb_temp[6];
  ULong  rd_wb_reg[6];
  
  Int    rd_wb_index;
  Int    n = 0, nr_insn;
  DisResult dres;

  
  Long delta = delta64;

  

  UChar *code = (UChar *) (guest_code + delta);

  IRStmt *bstmt = NULL;  
  IRExpr *next = NULL; 
  ULong  jumpkind =  Ijk_Boring;
  ULong  steering_pc;

  
  dres.whatNext = Dis_Continue;
  dres.len = 0;
  dres.continueAt = 0;
  dres.jk_StopHere = Ijk_INVALID;

  
  vassert((((Addr)code) & 7) == 0);

  
  cins = *((ULong *)(Addr) code);

  
#define CL_W0 0x02b3c7ff91234fffULL
#define CL_W1 0x0091a7ff95678fffULL

  if (*((ULong*)(Addr)(code)) == CL_W0 &&
      *((ULong*)(Addr)(code + 8)) == CL_W1) {
    
    if (*((ULong*)(Addr)(code + 16)) ==
        0x283a69a6d1483000ULL  ) {
      
      DIP("r0 = client_request ( r12 )\n");

      putPC(mkU64(guest_PC_curr_instr + 24));

      dres.jk_StopHere = Ijk_ClientReq;
      dres.whatNext = Dis_StopHere;
      dres.len = 24;
      goto decode_success;

    } else if (*((ULong*)(Addr)(code + 16)) ==
               0x283a71c751483000ULL  ) {
      
      DIP("r11 = guest_NRADDR\n");
      dres.len = 24;
      putIReg(11, IRExpr_Get(offsetof(VexGuestTILEGXState, guest_NRADDR),
                             Ity_I64));
      putPC(mkU64(guest_PC_curr_instr + 8));
      goto decode_success;

    } else if (*((ULong*)(Addr)(code + 16)) ==
               0x283a79e7d1483000ULL   ) {
      
      DIP("branch-and-link-to-noredir r12\n");
      dres.len = 24;
      putIReg(55, mkU64(guest_PC_curr_instr + 24));

      putPC(getIReg(12));

      dres.jk_StopHere = Ijk_NoRedir;
      dres.whatNext = Dis_StopHere;
      goto decode_success;

    }  else if (*((ULong*)(Addr)(code + 16)) ==
                0x283a5965d1483000ULL   ) {
      
      DIP("vex-inject-ir\n");
      dres.len = 24;

      vex_inject_ir(irsb, Iend_LE);

      stmt(IRStmt_Put(offsetof(VexGuestTILEGXState, guest_CMSTART),
                      mkU64(guest_PC_curr_instr)));
      stmt(IRStmt_Put(offsetof(VexGuestTILEGXState, guest_CMLEN),
                      mkU64(24)));

      
      putPC(mkU64(guest_PC_curr_instr + 24));

      dres.jk_StopHere = Ijk_InvalICache;
      dres.whatNext = Dis_StopHere;
      goto decode_success;
    }

    
    vex_printf("%s: unexpect special bundles at %lx\n",
               __func__, (Addr)guest_PC_curr_instr);
    delta += 16;
    goto decode_failure;
    
  }

  
  nr_insn = parse_insn_tilegx((tilegx_bundle_bits)cins,
                              (ULong)(Addr)code,
                              decoded);

  if (vex_traceflags & VEX_TRACE_FE)
    decode_and_display(&cins, 1, (ULong)(Addr)code);

  
  rd_wb_index = 0;

  steering_pc = -1ULL;

  for (n = 0; n < nr_insn; n++) {
    opcode = decoded[n].opcode->mnemonic;
    Int opi;

    rd = ra = rb = -1;
    opd[0] = opd[1] = opd[2] = opd[3] = -1;
    opd_dst_map = 0;
    opd_src_map = 0;
    opd_imm_map = 0;

    for (opi = 0; opi < decoded[n].opcode->num_operands; opi++) {
      const struct tilegx_operand *op = decoded[n].operands[opi];
      opd[opi] = decoded[n].operand_values[opi];

      
      if (opi < 3) {
        if (op->is_dest_reg) {
          if (rd == -1)
            rd =  decoded[n].operand_values[opi];
          else if (ra == -1)
            ra =  decoded[n].operand_values[opi];
        } else if (op->is_src_reg) {
          if (ra == -1) {
            ra = decoded[n].operand_values[opi];
          } else if(rb == -1) {
            rb = decoded[n].operand_values[opi];
          } else {
            vassert(0);
          }
        } else {
          imm = decoded[n].operand_values[opi];
        }
      }

      if (op->is_dest_reg) {
        opd_dst_map |= 1ULL << opi;
        if(op->is_src_reg)
          opd_src_map |= 1ULL << opi;
      } else if(op->is_src_reg) {
        opd_src_map |= 1ULL << opi;
      } else {
        opd_imm_map |= 1ULL << opi;
      }
    }

    use_dirty_helper = 0;

    switch (opcode) {
    case 0:    
      
      opd_imm_map |= (1 << 0);
      opd[0] = cins;
      use_dirty_helper = 1;
      break;
    case 1:     
      break;
    case 2:     
      break;
    case 3:     
      break;
    case 4:      
      break;
    case 5:  
      t2 = newTemp(Ity_I64);
      assign(t2, getIReg(ra));
      MARK_REG_WB(rd, t2);
      break;
    case 6:  
      t2 = newTemp(Ity_I64);
      assign(t2, mkU64(extend_s_8to64(imm)));
      MARK_REG_WB(rd, t2);
      break;
    case 7:  
      t2 = newTemp(Ity_I64);
      assign(t2, mkU64(extend_s_16to64(imm)));
      MARK_REG_WB(rd, t2);
      break;
    case 8:     
      break;
    case 9:     
      break;
    case 10:    
      break;
    case 11:    
      break;
    case 12:    
      break;
    case 13:    
      break;
    case 14:    
      break;
    case 15:   
      break;
    case 16:    
      break;
    case 17:    
      break;
    case 18:    
      break;
    case 19:    
      break;
    case 20:    
      break;
    case 21: 
      opd_imm_map |= (1 << 0);
      opd[0] = cins;
      use_dirty_helper = 1;
      break;
    case 22: 
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_Add64, getIReg(ra), getIReg(rb)));
      MARK_REG_WB(rd, t2);
      break;
    case 23: 
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_Add64, getIReg(ra),
                       mkU64(extend_s_8to64(imm))));
      MARK_REG_WB(rd, t2);
      break;
    case 24: 
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_Add64, getIReg(ra),
                       mkU64(extend_s_16to64(imm))));
      MARK_REG_WB(rd, t2);
      break;
    case 25: 
      t2 = newTemp(Ity_I64);
      assign(t2, signExtend(binop(Iop_Add32,
                                  narrowTo(Ity_I32, getIReg(ra)),
                                  narrowTo(Ity_I32, getIReg(rb))),
                            32));
      MARK_REG_WB(rd, t2);
      break;
    case 26: 
      t2 = newTemp(Ity_I64);
      assign(t2, signExtend(binop(Iop_Add32,
                                  narrowTo(Ity_I32, getIReg(ra)),
                                  mkU32(imm)), 32));
      MARK_REG_WB(rd, t2);
      break;
    case 27: 
      t2 = newTemp(Ity_I64);
      assign(t2, signExtend(binop(Iop_Add32,
                                  narrowTo(Ity_I32, getIReg(ra)),
                                  mkU32(imm)), 32));

      MARK_REG_WB(rd, t2);
      break;
    case 28: 
      use_dirty_helper = 1;
      break;
    case 29: 
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_And64, getIReg(ra), getIReg(rb)));
      MARK_REG_WB(rd, t2);
      break;
    case 30: 
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_And64, getIReg(ra),
                       mkU64(extend_s_8to64(imm))));
      MARK_REG_WB(rd, t2);
      break;
    case 31: 
      
    case 32:
      
      bstmt = dis_branch(binop(Iop_CmpEQ64, getIReg(ra), mkU64(0)),
                         imm);
      break;
    case 33: 
      {
        ULong imm0 = decoded[n].operand_values[3];
        ULong mask = ((-1ULL) ^ ((-1ULL << ((imm0 - imm) & 63)) << 1));
        t0 = newTemp(Ity_I64);
        t2 = newTemp(Ity_I64);
        assign(t0, binop(Iop_Xor64,
                         binop(Iop_Sub64,
                               binop(Iop_And64,
                                     binop(Iop_Shr64,
                                           getIReg(ra),
                                           mkU8(imm0)),
                                     mkU64(1)),
                               mkU64(1)),
                         mkU64(-1ULL)));
        assign(t2,
               binop(Iop_Or64,
                     binop(Iop_And64,
                           binop(Iop_Or64,
                                 binop(Iop_Shr64,
                                       getIReg(ra),
                                       mkU8(imm)),
                                 binop(Iop_Shl64,
                                       getIReg(ra),
                                       mkU8(64 - imm))),
                           mkU64(mask)),
                     binop(Iop_And64,
                           mkexpr(t0),
                           mkU64(~mask))));

        MARK_REG_WB(rd, t2);
      }
      break;
    case 34:  
      {
        ULong imm0 = decoded[n].operand_values[3];
        ULong mask = 0;
        t2 = newTemp(Ity_I64);
        mask = ((-1ULL) ^ ((-1ULL << ((imm0 - imm) & 63)) << 1));

        assign(t2,
               binop(Iop_And64,
                     binop(Iop_Or64,
                           binop(Iop_Shr64,
                                 getIReg(ra),
                                 mkU8(imm)),
                           binop(Iop_Shl64,
                                 getIReg(ra),
                                 mkU8(64 - imm))),
                     mkU64(mask)));
        MARK_REG_WB(rd, t2);
      }
      break;
    case 35:  
      {
        ULong mask;
        ULong imm0 = decoded[n].operand_values[3];
        t0 = newTemp(Ity_I64);
        t2 = newTemp(Ity_I64);
        if (imm <= imm0)
        {
          mask = ((-1ULL << imm) ^ ((-1ULL << imm0) << 1));
        }
        else
        {
          mask = ((-1ULL << imm) | (-1ULL >> (63 - imm0)));
        }

        assign(t0, binop(Iop_Or64,
                         binop(Iop_Shl64,
                               getIReg(ra),
                               mkU8(imm)),
                         binop(Iop_Shr64,
                               getIReg(ra),
                               mkU8(64 - imm))));

        assign(t2, binop(Iop_Or64,
                         binop(Iop_And64,
                               mkexpr(t0),
                               mkU64(mask)),
                         binop(Iop_And64,
                               getIReg(rd),
                               mkU64(~mask))));

        MARK_REG_WB(rd, t2);
      }
      break;
    case 36:  
      
    case 37:  
      bstmt = dis_branch(binop(Iop_CmpEQ64,
                               binop(Iop_And64,
                                     getIReg(ra),
                                     mkU64(0x8000000000000000ULL)),
                               mkU64(0x0)),
                         imm);
      break;
    case 38:  
      
    case 39:
      
      bstmt = dis_branch(unop(Iop_Not1,
                              binop(Iop_CmpLE64S,
                                    getIReg(ra),
                                    mkU64(0))),
                         imm);
      break;
    case 40:  
      
    case 41:  
      bstmt = dis_branch(unop(Iop_64to1,
                              unop(Iop_Not64, getIReg(ra))),
                         imm);

      break;
    case 42:  
      
    case 43:
      
      bstmt = dis_branch(unop(Iop_64to1,
                              getIReg(ra)),
                         imm);
      break;
    case 44:  
      bstmt = dis_branch(binop(Iop_CmpLE64S, getIReg(ra),
                               mkU64(0)),
                         imm);
      break;
    case 45:  
      bstmt = dis_branch(binop(Iop_CmpLE64S, getIReg(ra),
                               mkU64(0)),
                         imm);
      break;
    case 46:  
      bstmt = dis_branch(binop(Iop_CmpLT64S, getIReg(ra),
                               mkU64(0)),
                         imm);
      break;
    case 47:  
      bstmt = dis_branch(binop(Iop_CmpLT64S, getIReg(ra),
                               mkU64(0)),
                         imm);
      break;
    case 48:  
      
    case 49:
      
      bstmt = dis_branch(binop(Iop_CmpNE64, getIReg(ra),
                               mkU64(0)),
                         imm);
      break;
    case 50:  
      t2 = newTemp(Ity_I64);
      assign(t2, unop(Iop_Clz64, getIReg(ra)));

      MARK_REG_WB(rd, t2);
      break;
    case 51:  
      t2 = newTemp(Ity_I64);
      assign(t2, IRExpr_ITE(binop(Iop_CmpEQ64, getIReg(ra), mkU64(0)),
                            getIReg(rb), getIReg(rd)));
      MARK_REG_WB(rd, t2);
      break;
    case 52:  
      t2 = newTemp(Ity_I64);
      assign(t2, IRExpr_ITE(binop(Iop_CmpEQ64, getIReg(ra), mkU64(0)),
                            getIReg(rd), getIReg(rb)));
      MARK_REG_WB(rd, t2);
      break;
    case 53:  
      t2 = newTemp(Ity_I64);
      assign(t2,  unop(Iop_1Uto64, binop(Iop_CmpEQ64,
                                         getIReg(ra), getIReg(rb))));
      MARK_REG_WB(rd, t2);
      break;

    case 54:  
      t2 = newTemp(Ity_I64);
      assign(t2, unop(Iop_1Uto64, binop(Iop_CmpEQ64,
                                        getIReg(ra),
                                        mkU64(extend_s_8to64(imm)))));
      MARK_REG_WB(rd, t2);
      break;
    case 55:  
      t1 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);

      assign(t1, getIReg(rb));
      stmt( IRStmt_CAS(mkIRCAS(IRTemp_INVALID, t2, Iend_LE,
                               getIReg(ra),
                               NULL, binop(Iop_Add64,
                                           getIReg(70),
                                           getIReg(71)),
                               NULL, mkexpr(t1))));
      MARK_REG_WB(rd, t2);
      break;
    case 56:  
      t1 = newTemp(Ity_I32);
      t2 = newTemp(Ity_I64);
      t3 = newTemp(Ity_I32);

      assign(t1, narrowTo(Ity_I32, getIReg(rb)));
      stmt( IRStmt_CAS(mkIRCAS(IRTemp_INVALID, t3, Iend_LE,
                               getIReg(ra),
                               NULL,
                               narrowTo(Ity_I32, binop(Iop_Add64,
                                                       getIReg(70),
                                                       getIReg(71))),
                               NULL,
                               mkexpr(t1))));
      assign(t2, unop(Iop_32Uto64, mkexpr(t3)));
      MARK_REG_WB(rd, t2);
      break;
    case 57:  
      t2 = newTemp(Ity_I64);
      assign(t2, unop(Iop_1Uto64,
                      binop(Iop_CmpLE64S, getIReg(ra), getIReg(rb))));
      MARK_REG_WB(rd, t2);
      break;
    case 58:  
      t2 = newTemp(Ity_I64);
      assign(t2,  unop(Iop_1Uto64,
                       binop(Iop_CmpLE64U, getIReg(ra), getIReg(rb))));
      MARK_REG_WB(rd, t2);
      break;
    case 59:  
      t2 = newTemp(Ity_I64);
      assign(t2, unop(Iop_1Uto64,
                      binop(Iop_CmpLT64S, getIReg(ra), getIReg(rb))));
      MARK_REG_WB(rd, t2);
      break;
    case 60:  
      t2 = newTemp(Ity_I64);
      assign(t2, unop(Iop_1Uto64,
                      binop(Iop_CmpLT64S,
                            getIReg(ra),
                            mkU64(extend_s_8to64(imm)))));
      MARK_REG_WB(rd, t2);
      break;
    case 61:

      
      t2 = newTemp(Ity_I64);
      assign(t2, unop(Iop_1Uto64,
                      binop(Iop_CmpLT64U, getIReg(ra), getIReg(rb))));
      MARK_REG_WB(rd, t2);


      break;
    case 62:  
      t2 = newTemp(Ity_I64);
      assign(t2,  unop(Iop_1Uto64,
                       binop(Iop_CmpLT64U,
                             getIReg(ra),
                             mkU64(imm))));
      MARK_REG_WB(rd, t2);


      break;
    case 63:  
      t2 = newTemp(Ity_I64);
      assign(t2, unop(Iop_1Uto64,
                      binop(Iop_CmpNE64, getIReg(ra), getIReg(rb))));
      MARK_REG_WB(rd, t2);


      break;
    case 64:
      
    case 65:
      
    case 66:
      
    case 67:
      
    case 68:
      
    case 69:
      
    case 70:
      
    case 71:
      
    case 72:
      use_dirty_helper = 1;
      break;
    case 73:  
      t2 = newTemp(Ity_I64);
      assign(t2, unop(Iop_Ctz64, getIReg(ra)));

      MARK_REG_WB(rd, t2);


      break;
    case 74:  
      t0 = newTemp(Ity_I64);
      t1 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);

      
      assign(t0, binop(Iop_Shl64,
                       binop(Iop_And64,
                             getIReg(rb),
                             mkU64(7)),
                       mkU8(3)));
      assign(t1, binop(Iop_Sub64,
                       mkU64(64),
                       mkexpr(t0)));

      assign(t2, binop(Iop_Or64,
                       binop(Iop_Shl64,
                             getIReg(ra),
                             unop(Iop_64to8, mkexpr(t1))),
                       binop(Iop_Shr64,
                             getIReg(rd),
                             unop(Iop_64to8, mkexpr(t0)))));

      MARK_REG_WB(rd, t2);
      break;
    case 75:
      
    case 76:
      
    case 77:
      
    case 78:
      
    case 79:
      use_dirty_helper = 1;
      break;
    case 80:  
      t2 = newTemp(Ity_I64);
      stmt( IRStmt_CAS(
              mkIRCAS(IRTemp_INVALID,
                      t2,
                      Iend_LE,
                      getIReg(ra),
                      NULL,
                      mkU64(0x0),
                      NULL,
                      getIReg(rb))));
      MARK_REG_WB(rd, t2);
      break;
    case 81:  
      t0 = newTemp(Ity_I32);
      t2 = newTemp(Ity_I64);
      stmt( IRStmt_CAS(
              mkIRCAS(IRTemp_INVALID,
                      t0,
                      Iend_LE,
                      getIReg(ra),
                      NULL,
                      mkU32(0x0),
                      NULL,
                      narrowTo(Ity_I32,
                               getIReg(rb)))));
      assign(t2, unop(Iop_32Sto64, mkexpr(t0)));
      MARK_REG_WB(rd, t2);
      break;
    case 82:
      
    case 83:
      
    case 84:
      
    case 85:
      
    case 86:
      
    case 87:
      
    case 88:
      
    case 89:
      use_dirty_helper = 1;
      break;
    case 90:  
      t2 = newTemp(Ity_I64);
      stmt( IRStmt_CAS(
              mkIRCAS(IRTemp_INVALID,
                      t2,
                      Iend_LE,
                      getIReg(ra),
                      NULL,
                      
                      mkU64(0x3),
                      NULL,
                      getIReg(rb))));
      MARK_REG_WB(rd, t2);
      break;
    case 91:  
      t0 = newTemp(Ity_I32);
      t2 = newTemp(Ity_I64);
      stmt( IRStmt_CAS(
              mkIRCAS(IRTemp_INVALID,
                      t0,
                      Iend_LE,
                      getIReg(ra),
                      NULL,
                      
                      mkU32(0x3),
                      NULL,
                      narrowTo(Ity_I32,
                               getIReg(rb)))));
      assign(t2, unop(Iop_32Sto64, mkexpr(t0)));
      MARK_REG_WB(rd, t2);

      break;
    case 92:  
      t2 = newTemp(Ity_I64);
      stmt( IRStmt_CAS(
              mkIRCAS(IRTemp_INVALID,
                      t2,
                      Iend_LE,
                      getIReg(ra),
                      NULL,
                      
                      mkU64(0x5),
                      NULL,
                      getIReg(rb))));
      MARK_REG_WB(rd, t2);
      break;
    case 93:  
      t0 = newTemp(Ity_I32);
      t2 = newTemp(Ity_I64);
      stmt( IRStmt_CAS(
              mkIRCAS(IRTemp_INVALID,
                      t0,
                      Iend_LE,
                      getIReg(ra),
                      NULL,
                      
                      mkU32(0x5),
                      NULL,
                      narrowTo(Ity_I32,
                               getIReg(rb)))));
      assign(t2, unop(Iop_32Sto64, mkexpr(t0)));
      MARK_REG_WB(rd, t2);
      break;
    case 94:  
      t2 = newTemp(Ity_I64);
      stmt( IRStmt_CAS(
              mkIRCAS(IRTemp_INVALID,
                      t2,
                      Iend_LE,
                      getIReg(ra),
                      NULL,
                      mkU64(0x2),
                      NULL,
                      getIReg(rb))));
      MARK_REG_WB(rd, t2);
      break;
    case 95:
      
      t0 = newTemp(Ity_I32);
      t2 = newTemp(Ity_I64);
      stmt( IRStmt_CAS(
              mkIRCAS(IRTemp_INVALID,
                      t0,
                      Iend_LE,
                      getIReg(ra),
                      NULL,
                      mkU32(0x2),
                      NULL,
                      narrowTo(Ity_I32,
                               getIReg(rb)))));
      assign(t2, unop(Iop_32Sto64, mkexpr(t0)));
      MARK_REG_WB(rd, t2);
      break;
    case 96:  
      t2 = newTemp(Ity_I64);
      stmt( IRStmt_CAS(
              mkIRCAS(IRTemp_INVALID,
                      t2,
                      Iend_LE,
                      getIReg(ra),
                      NULL,
                      mkU64(0x4),
                      NULL,
                      getIReg(rb))));
      MARK_REG_WB(rd, t2);
      break;
    case 97:  
      t0 = newTemp(Ity_I32);
      t2 = newTemp(Ity_I64);
      stmt( IRStmt_CAS(
              mkIRCAS(IRTemp_INVALID,
                      t0,
                      Iend_LE,
                      getIReg(ra),
                      NULL,
                      mkU32(0x4),
                      NULL,
                      narrowTo(Ity_I32,
                               getIReg(rb)))));
      assign(t2, unop(Iop_32Sto64, mkexpr(t0)));
      MARK_REG_WB(rd, t2);
      break;
    case 98:
      
    case 99:
      
    case 100:
      use_dirty_helper = 1;
      break;
    case 101: 
      break;
    case 102:
      
    case 103:
      
    case 104:
      
    case 105:
      
    case 106:
      
    case 107:
      
    case 108:
      use_dirty_helper = 1;
      break;
    case 109:
      
    case 110:
      
    case 111:
      use_dirty_helper = 1;
      break;
    case 112:  
      next = mkU64(guest_PC_curr_instr + 8);
      jumpkind = Ijk_Ret;
      break;
    case 113:  
      next = mkU64(imm);
      
      steering_pc = imm;
      jumpkind = Ijk_Boring;
      break;
    case 114:
      t2 = newTemp(Ity_I64);
      assign(t2, mkU64(guest_PC_curr_instr + 8));
      
      steering_pc = imm;
      next = mkU64(imm);
      jumpkind = Ijk_Call;
      MARK_REG_WB(55, t2);
      break;
    case 115:  
      
    case 116:  
      t1 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);
      assign(t1, getIReg(ra));
      assign(t2, mkU64(guest_PC_curr_instr + 8));
      next = mkexpr(t1);
      jumpkind = Ijk_Call;
      MARK_REG_WB(55, t2);
      break;
    case 117:  
      
    case 118:  
      next = getIReg(ra);
      jumpkind = Ijk_Boring;
      break;
    case 119:  
      t2 = newTemp(Ity_I64);
      assign(t2, load(Ity_I64, (getIReg(ra))));
      MARK_REG_WB(rd, t2);
      break;
    case 120:  
      t2 = newTemp(Ity_I64);
      assign(t2,  unop(Iop_8Sto64,
                       load(Ity_I8, (getIReg(ra)))));
      MARK_REG_WB(rd, t2);
      break;
    case 121:  
      t1 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);
      assign(t1, binop(Iop_Add64, getIReg(ra), mkU64(imm)));
      assign(t2,  unop(Iop_8Sto64,
                       load(Ity_I8, (getIReg(ra)))));
      MARK_REG_WB(ra, t1);
      MARK_REG_WB(rd, t2);
      break;
    case 122:  
      t2 = newTemp(Ity_I64);
      assign(t2,  unop(Iop_8Uto64,
                       load(Ity_I8, (getIReg(ra)))));
      MARK_REG_WB(rd, t2);

      break;
    case 123:  
      t1 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);
      assign(t1,  binop(Iop_Add64, getIReg(ra), mkU64(imm)));
      assign(t2,  unop(Iop_8Uto64,
                       load(Ity_I8, (getIReg(ra)))));
      MARK_REG_WB(ra, t1);
      MARK_REG_WB(rd, t2);
      break;
    case 124:  
      t2 = newTemp(Ity_I64);
      assign(t2,  unop(Iop_16Sto64,
                       load(Ity_I16, getIReg(ra))));
      MARK_REG_WB(rd, t2);
      break;
    case 125:  
      t1 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);
      assign(t1,  binop(Iop_Add64, getIReg(ra), mkU64(imm)));
      assign(t2,  unop(Iop_16Sto64,
                       load(Ity_I16, getIReg(ra))));
      MARK_REG_WB(rd, t2);
      MARK_REG_WB(ra, t1);
      break;
    case 126: 
      t2 = newTemp(Ity_I64);
      assign(t2,  unop(Iop_16Uto64,
                       load(Ity_I16, getIReg(ra))));
      MARK_REG_WB(rd, t2);
      break;
    case 127: 
      t1 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);
      assign(t1,  binop(Iop_Add64, getIReg(ra), mkU64(imm)));
      assign(t2,  unop(Iop_16Uto64,
                       load(Ity_I16, getIReg(ra))));
      MARK_REG_WB(rd, t2);
      MARK_REG_WB(ra, t1);
      break;
    case 128: 
      t2 = newTemp(Ity_I64);
      assign(t2,  unop(Iop_32Sto64,
                       load(Ity_I32, (getIReg(ra)))));
      MARK_REG_WB(rd, t2);
      break;
    case 129: 
      t2 = newTemp(Ity_I64);
      t1 = newTemp(Ity_I64);
      assign(t1,  binop(Iop_Add64, getIReg(ra), mkU64(imm)));
      assign(t2,  unop(Iop_32Sto64,
                       load(Ity_I32, (getIReg(ra)))));
      MARK_REG_WB(rd, t2);
      MARK_REG_WB(ra, t1);
      break;
    case 130:  
      t2 = newTemp(Ity_I64);
      assign(t2,  unop(Iop_32Uto64,
                       load(Ity_I32, getIReg(ra))));
      MARK_REG_WB(rd, t2);
      break;
    case 131:  
      t1 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);
      assign(t1, binop(Iop_Add64, getIReg(ra), mkU64(imm)));
      assign(t2,  unop(Iop_32Uto64,
                       load(Ity_I32, getIReg(ra))));
      MARK_REG_WB(ra, t1);
      MARK_REG_WB(rd, t2);
      break;
    case 132:  
      t1 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);
      assign(t1, load(Ity_I64, getIReg(ra)));
      assign(t2, binop(Iop_Add64, getIReg(ra), mkU64(imm)));
      MARK_REG_WB(ra, t2);
      MARK_REG_WB(rd, t1);
      break;
    case 133:  
      t2 = newTemp(Ity_I64);
      assign(t2, load(Ity_I64,
                      binop(Iop_And64,
                            getIReg(ra),
                            unop(Iop_Not64,
                                 mkU64(7)))));
      MARK_REG_WB(rd, t2);
      break;
    case 134:  
      t1 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);

      assign(t1, binop(Iop_Add64, getIReg(ra), mkU64(imm)));
      assign(t2, load(Ity_I64,
                      binop(Iop_And64,
                            getIReg(ra),
                            unop(Iop_Not64,
                                 mkU64(7)))));
      MARK_REG_WB(ra, t1);
      MARK_REG_WB(rd, t2);
      break;
    case 135:  
      
      t2 = newTemp(Ity_I64);
      assign(t2, load(Ity_I64, (getIReg(ra))));
      MARK_REG_WB(rd, t2);
      break;
    case 136:  
      t2 = newTemp(Ity_I64);
      assign(t2,  unop(Iop_8Sto64,
                       load(Ity_I8, (getIReg(ra)))));
      MARK_REG_WB(rd, t2);
      break;
    case 137:  
      t1 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);
      assign(t2,  unop(Iop_8Sto64,
                       load(Ity_I8, (getIReg(ra)))));
      assign(t1, binop(Iop_Add64, getIReg(ra), mkU64(imm)));
      MARK_REG_WB(ra, t1);
      MARK_REG_WB(rd, t2);
      break;
    case 138:  
      t2 = newTemp(Ity_I64);
      assign(t2,  unop(Iop_8Uto64,
                       load(Ity_I8, (getIReg(ra)))));
      MARK_REG_WB(rd, t2);
      break;
    case 139:  
      t1 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);

      assign(t1, binop(Iop_Add64, getIReg(ra), mkU64(imm)));
      assign(t2,  unop(Iop_8Uto64,
                       load(Ity_I8, (getIReg(ra)))));

      MARK_REG_WB(ra, t1);
      MARK_REG_WB(rd, t2);
      break;
    case 140:  
      t2 = newTemp(Ity_I64);
      assign(t2,  unop(Iop_16Sto64,
                       load(Ity_I16, getIReg(ra))));
      MARK_REG_WB(rd, t2);
      break;
    case 141:  
      t1 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);
      assign(t2,  unop(Iop_16Sto64,
                       load(Ity_I16, getIReg(ra))));
      assign(t1, binop(Iop_Add64, getIReg(ra), mkU64(imm)));
      MARK_REG_WB(ra, t1);
      MARK_REG_WB(rd, t2);
      break;
    case 142:  
      t2 = newTemp(Ity_I64);
      assign(t2,  unop(Iop_16Uto64,
                       load(Ity_I16, getIReg(ra))));
      MARK_REG_WB(rd, t2);
      break;
    case 143:  
      t1 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);
      assign(t2,  unop(Iop_16Uto64,
                       load(Ity_I16, getIReg(ra))));
      assign(t1, binop(Iop_Add64, getIReg(ra), mkU64(imm)));
      MARK_REG_WB(ra, t1);
      MARK_REG_WB(rd, t2);
      break;
    case 144:  
      t2 = newTemp(Ity_I64);
      assign(t2,  unop(Iop_32Sto64,
                       load(Ity_I32, (getIReg(ra)))));
      MARK_REG_WB(rd, t2);
      break;
    case 145:  
      t1 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);
      assign(t2,  unop(Iop_32Sto64,
                       load(Ity_I32, (getIReg(ra)))));
      assign(t1, binop(Iop_Add64, getIReg(ra), mkU64(imm)));
      MARK_REG_WB(rd, t2);
      MARK_REG_WB(ra, t1);
      break;
    case 146:  
      t2 = newTemp(Ity_I64);
      assign(t2,  unop(Iop_32Uto64,
                       load(Ity_I32, getIReg(ra))));
      MARK_REG_WB(rd, t2);
      break;
    case 147:  
      t1 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);
      assign(t2,  unop(Iop_32Uto64,
                       load(Ity_I32, getIReg(ra))));
      assign(t1, binop(Iop_Add64, getIReg(ra), mkU64(imm)));
      MARK_REG_WB(rd, t2);
      MARK_REG_WB(ra, t1);
      break;
    case 148:  
      t1 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);
      assign(t1, load(Ity_I64, getIReg(ra)));
      assign(t2, binop(Iop_Add64, getIReg(ra), mkU64(imm)));
      MARK_REG_WB(rd, t1);
      MARK_REG_WB(ra, t2);
      break;
    case 149:  
      t2 = newTemp(Ity_I64);
      assign(t2,  mkU64(guest_PC_curr_instr + 8));
      MARK_REG_WB(rd, t2);
      break;
    case 150:  
      use_dirty_helper = 1;
      break;
    case 151:  
      t2 = newTemp(Ity_I64);
      if (imm == 0x2780) { 
	 assign(t2, getIReg(70));
	 MARK_REG_WB(rd, t2);
      } else if (imm == 0x2580) { 
         assign(t2, getIReg(576 / 8));
         MARK_REG_WB(rd, t2);
      } else if (imm == 0x2581) { 
         assign(t2, getIReg(584 / 8));
         MARK_REG_WB(rd, t2);
      } else
        use_dirty_helper = 1;
      break;
    case 152:  
      use_dirty_helper = 1;
      break;
    case 153:  
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_And64,
                       unop(Iop_1Sto64, binop(Iop_CmpNE64,
                                              getIReg(ra),
                                              mkU64(0))),
                       getIReg(rb)));
      MARK_REG_WB(rd, t2);
      break;
    case 154:  
      if (imm == 0x2780) 
        putIReg(70, getIReg(ra));
      else if (imm == 0x2580) 
        putIReg(576/8, getIReg(ra));
      else if (imm == 0x2581) 
        putIReg(584/8, getIReg(ra));
      else
        use_dirty_helper = 1;
      break;
    case 155:  
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_MullS32,
                       unop(Iop_64to32,
                            binop(Iop_Shr64,
                                  getIReg(ra),
                                  mkU8(32))),
                       unop(Iop_64to32,
                            binop(Iop_Shr64,
                                  getIReg(rb),
                                  mkU8(32)))));
      MARK_REG_WB(rd, t2);
      break;
    case 156:  
      t0 = newTemp(Ity_I64);
      t1 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);
      t3 = newTemp(Ity_I64);

      assign(t0, unop(Iop_32Sto64,
                      unop(Iop_64to32,
                           binop(Iop_Shr64, getIReg(ra), mkU8(32)))));
      assign(t1, binop(Iop_MullU32,
                       unop(Iop_64to32, mkexpr(t0)),
                       unop(Iop_64to32, binop(Iop_Shr64, getIReg(rb), mkU8(32)))));
      assign(t3, binop(Iop_MullU32,
                       unop(Iop_64to32, binop(Iop_Shr64,
                                              mkexpr(t0),
                                              mkU8(32))),
                       unop(Iop_64to32, binop(Iop_Shr64, getIReg(rb), mkU8(32)))));
      assign(t2, binop(Iop_Add64,
                       mkexpr(t1),
                       binop(Iop_Shl64,
                             mkexpr(t3),
                             mkU8(32))));
      MARK_REG_WB(rd, t2);
      break;
    case 157:  
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_MullS32,
                       unop(Iop_64to32,
                            binop(Iop_Shr64,
                                  getIReg(ra),
                                  mkU8(32))),
                       unop(Iop_64to32,
                            getIReg(rb))));
      MARK_REG_WB(rd, t2);
      break;
    case 158:  
      t0 = newTemp(Ity_I64);
      t1 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);
      t3 = newTemp(Ity_I64);

      assign(t0, unop(Iop_32Sto64,
                      unop(Iop_64to32,
                           binop(Iop_Shr64, getIReg(ra), mkU8(32)))));
      assign(t1, binop(Iop_MullU32,
                       unop(Iop_64to32, mkexpr(t0)),
                       unop(Iop_64to32, getIReg(rb))));
      assign(t3, binop(Iop_MullU32,
                       unop(Iop_64to32, binop(Iop_Shr64,
                                              mkexpr(t0),
                                              mkU8(32))),
                       unop(Iop_64to32, getIReg(rb))));
      assign(t2, binop(Iop_Add64,
                       mkexpr(t1),
                       binop(Iop_Shl64,
                             mkexpr(t3),
                             mkU8(32))));
      MARK_REG_WB(rd, t2);
      break;
    case 159:  
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_MullU32,
                       unop(Iop_64to32,
                            binop(Iop_Shr64,
                                  getIReg(ra),
                                  mkU8(32))),
                       unop(Iop_64to32,
                            binop(Iop_Shr64,
                                  getIReg(rb),
                                  mkU8(32)))));
      MARK_REG_WB(rd, t2);
      break;
    case 160:  
      t0 = newTemp(Ity_I64);
      t1 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);
      t3 = newTemp(Ity_I64);

      assign(t0, unop(Iop_32Sto64,
                      unop(Iop_64to32,
                           getIReg(ra))));

      assign(t1, binop(Iop_MullU32,
                       unop(Iop_64to32, mkexpr(t0)),
                       unop(Iop_64to32, binop(Iop_Shr64, getIReg(rb), mkU8(32)))));
      assign(t3, binop(Iop_MullU32,
                       unop(Iop_64to32, binop(Iop_Shr64,
                                              mkexpr(t0),
                                              mkU8(32))),
                       unop(Iop_64to32, binop(Iop_Shr64, getIReg(rb), mkU8(32)))));
      assign(t2, binop(Iop_Add64,
                       mkexpr(t1),
                       binop(Iop_Shl64,
                             mkexpr(t3),
                             mkU8(32))));
      MARK_REG_WB(rd, t2);
      break;
    case 161:  
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_MullU32,
                       unop(Iop_64to32,
                            binop(Iop_Shr64,
                                  getIReg(ra),
                                  mkU8(32))),
                       unop(Iop_64to32,
                            getIReg(rb))));
      MARK_REG_WB(rd, t2);
      break;
    case 162:  
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_MullS32,
                       unop(Iop_64to32, getIReg(ra)),
                       unop(Iop_64to32, getIReg(rb))));
      MARK_REG_WB(rd, t2);
      break;
    case 163:  
      t0 = newTemp(Ity_I64);
      t1 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);
      t3 = newTemp(Ity_I64);

      assign(t0, unop(Iop_32Sto64,
                      unop(Iop_64to32, getIReg(ra))));
      assign(t1, binop(Iop_MullU32,
                       unop(Iop_64to32, mkexpr(t0)),
                       unop(Iop_64to32, getIReg(rb))));
      assign(t3, binop(Iop_MullU32,
                       unop(Iop_64to32, binop(Iop_Shr64,
                                              mkexpr(t0),
                                              mkU8(32))),
                       unop(Iop_64to32, getIReg(rb))));
      assign(t2, binop(Iop_Add64,
                       mkexpr(t1),
                       binop(Iop_Shl64,
                             mkexpr(t3),
                             mkU8(32))));
      MARK_REG_WB(rd, t2);
      break;
    case 164:   
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_MullU32,
                       unop(Iop_64to32, getIReg(ra)),
                       unop(Iop_64to32, getIReg(rb))));
      MARK_REG_WB(rd, t2);
      break;
    case 165:   
      t0 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);

      assign(t0, binop(Iop_MullS32,
                       unop(Iop_64to32, binop(Iop_Shr64,
                                              getIReg(ra), mkU8(32))),
                       unop(Iop_64to32, binop(Iop_Shr64,
                                              getIReg(rb), mkU8(32)))));
      assign(t2, binop(Iop_Add64, getIReg(rd), mkexpr(t0)));
      MARK_REG_WB(rd, t2);
      break;
    case 166:   
      t0 = newTemp(Ity_I64);
      t1 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);
      t3 = newTemp(Ity_I64);
      t4 = newTemp(Ity_I64);
      assign(t0, unop(Iop_32Sto64,
                      unop(Iop_64to32,
                           binop(Iop_Shr64, getIReg(ra), mkU8(32)))));
      assign(t1, binop(Iop_MullU32,
                       unop(Iop_64to32, mkexpr(t0)),
                       unop(Iop_64to32, binop(Iop_Shr64,
                                              getIReg(rb), mkU8(32)))));
      assign(t3, binop(Iop_MullU32,
                       unop(Iop_64to32, binop(Iop_Shr64,
                                              mkexpr(t0),
                                              mkU8(32))),
                       unop(Iop_64to32, binop(Iop_Shr64,
                                              getIReg(rb), mkU8(32)))));
      assign(t2, binop(Iop_Add64,
                       mkexpr(t1),
                       binop(Iop_Shl64,
                             mkexpr(t3),
                             mkU8(32))));
      assign(t4, binop(Iop_Add64, getIReg(rd), mkexpr(t2)));
      MARK_REG_WB(rd, t4);
      break;
    case 167:   
      t2 = newTemp(Ity_I64);
      t4 = newTemp(Ity_I64);
      assign(t2, binop(Iop_MullS32,
                       unop(Iop_64to32,
                            binop(Iop_Shr64,
                                  getIReg(ra),
                                  mkU8(32))),
                       unop(Iop_64to32,
                            getIReg(rb))));
      assign(t4, binop(Iop_Add64, getIReg(rd), mkexpr(t2)));
      MARK_REG_WB(rd, t4);
      break;
    case 168:   
      t0 = newTemp(Ity_I64);
      t1 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);
      t3 = newTemp(Ity_I64);
      t4 = newTemp(Ity_I64);
      assign(t0, unop(Iop_32Sto64,
                      unop(Iop_64to32,
                           binop(Iop_Shr64, getIReg(ra), mkU8(32)))));
      assign(t1, binop(Iop_MullU32,
                       unop(Iop_64to32, mkexpr(t0)),
                       unop(Iop_64to32, getIReg(rb))));
      assign(t3, binop(Iop_MullU32,
                       unop(Iop_64to32, binop(Iop_Shr64,
                                              mkexpr(t0),
                                              mkU8(32))),
                       unop(Iop_64to32, getIReg(rb))));
      assign(t2, binop(Iop_Add64,
                       mkexpr(t1),
                       binop(Iop_Shl64,
                             mkexpr(t3),
                             mkU8(32))));
      assign(t4, binop(Iop_Add64, getIReg(rd), mkexpr(t2)));
      MARK_REG_WB(rd, t4);
      break;
    case 169:   
      use_dirty_helper = 1;
      break;
    case 170:   
      use_dirty_helper = 1;
      break;
    case 171:   
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_Add64,
                       binop(Iop_MullU32,
                             unop(Iop_64to32,
                                  binop(Iop_Shr64,
                                        getIReg(ra),
                                        mkU8(32))),
                             unop(Iop_64to32,
                                  getIReg(rb))),
                       getIReg(rd)));
      MARK_REG_WB(rd, t2);
      break;
    case 172:  
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_Add64,
                       getIReg(rd),
                       binop(Iop_MullS32,
                             unop(Iop_64to32, getIReg(ra)),
                             unop(Iop_64to32, getIReg(rb)))));
      MARK_REG_WB(rd, t2);
      break;
    case 173:  
      t0 = newTemp(Ity_I64);
      t1 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);
      t3 = newTemp(Ity_I64);

      assign(t0, unop(Iop_32Sto64,
                      unop(Iop_64to32, getIReg(ra))));
      assign(t1, binop(Iop_MullU32,
                       unop(Iop_64to32, mkexpr(t0)),
                       unop(Iop_64to32, getIReg(rb))));
      assign(t3, binop(Iop_MullU32,
                       unop(Iop_64to32, binop(Iop_Shr64,
                                              mkexpr(t0),
                                              mkU8(32))),
                       unop(Iop_64to32, getIReg(rb))));
      assign(t2, binop(Iop_Add64,
                       getIReg(rd),
                       binop(Iop_Add64,
                             mkexpr(t1),
                             binop(Iop_Shl64,
                                   mkexpr(t3),
                                   mkU8(32)))));
      MARK_REG_WB(rd, t2);
      break;
    case 174:  
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_Add64,
                       binop(Iop_MullU32,
                             unop(Iop_64to32,
                                  getIReg(ra)),
                             unop(Iop_64to32,
                                  getIReg(rb))),
                       getIReg(rd)));
      MARK_REG_WB(rd, t2);
      break;
    case 175:   
      t2 = newTemp(Ity_I64);
      assign(t2, unop(Iop_32Sto64,
                      unop(Iop_64to32,
                           binop(Iop_Add64,
                                 getIReg(rd),
                                 binop(Iop_MullU32,
                                       narrowTo(Ity_I32, getIReg(ra)),
                                       narrowTo(Ity_I32, getIReg(rb)))))));
      MARK_REG_WB(rd, t2);
      break;
    case 176:   
      t2 = newTemp(Ity_I64);
      assign(t2, unop(Iop_32Sto64,
                      unop(Iop_64to32,
                           binop(Iop_MullU32,
                                 narrowTo(Ity_I32, getIReg(ra)),
                                 narrowTo(Ity_I32, getIReg(rb))))));
      MARK_REG_WB(rd, t2);
      break;
    case 177:   
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_And64,
                       unop(Iop_1Sto64, binop(Iop_CmpEQ64,
                                              getIReg(ra),
                                              mkU64(0))),
                       getIReg(rb)));
      MARK_REG_WB(rd, t2);
      break;
    case 178:  
      break;
    case 179:  
      break;
    case 180:  
      t2 = newTemp(Ity_I64);
      assign(t2, unop(Iop_Not64,
                      binop(Iop_Or64,
                            getIReg(ra),
                            getIReg(rb))));
      MARK_REG_WB(rd, t2);
      break;
    case 181:  
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_Or64,
                       getIReg(ra),
                       getIReg(rb)));
      MARK_REG_WB(rd, t2);
      break;
    case 182:  
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_Or64,
                       getIReg(ra),
                       mkU64(imm)));
      MARK_REG_WB(rd, t2);
      break;
    case 183:
      
    case 184:
      
    case 185:
      use_dirty_helper = 1;
      break;
    case 186:  
      t0 = newTemp(Ity_I64);
      t1 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);
      assign(t0, binop(Iop_Shl64,
                       getIReg(ra),
                       unop(Iop_64to8, getIReg(rb))));
      assign(t1, binop(Iop_Shr64,
                       getIReg(ra),
                       unop(Iop_64to8, binop(Iop_Sub64,
                                             mkU64(0),
                                             getIReg(rb)))));
      assign(t2, binop(Iop_Or64, mkexpr(t0), mkexpr(t1)));
      MARK_REG_WB(rd, t2);
      break;
    case 187:  
      t0 = newTemp(Ity_I64);
      t1 = newTemp(Ity_I64);
      t2 = newTemp(Ity_I64);
      assign(t0, binop(Iop_Shl64,
                       getIReg(ra),
                       mkU8(imm)));
      assign(t1, binop(Iop_Shr64,
                       getIReg(ra),
                       mkU8(0 - imm)));
      assign(t2, binop(Iop_Or64, mkexpr(t0), mkexpr(t1)));
      MARK_REG_WB(rd, t2);
      break;
    case 188:   
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_Shl64,
                       getIReg(ra),
                       unop(Iop_64to8, getIReg(rb))));
      MARK_REG_WB(rd, t2);

      break;
    case 189:   
      t2 = newTemp(Ity_I64);
      t3 = newTemp(Ity_I64);
      assign(t3, binop(Iop_Shl64, getIReg(ra), mkU8(16)));
      imm &= 0xFFFFULL;
      if (imm & 0x8000)
      {
        t4 = newTemp(Ity_I64);
        assign(t4, mkU64(imm));
        assign(t2, binop(Iop_Add64, mkexpr(t3), mkexpr(t4)));
      }
      else
      {
        assign(t2, binop(Iop_Add64, mkexpr(t3), mkU64(imm)));
      }
      MARK_REG_WB(rd, t2);

      break;
    case 190:   
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_Add64,
                       binop(Iop_Shl64,
                             getIReg(ra), mkU8(1)),
                       getIReg(rb)));

      MARK_REG_WB(rd, t2);
      break;
    case 191:   
      t2 = newTemp(Ity_I64);
      assign(t2,
             unop(Iop_32Sto64,
                  unop(Iop_64to32,
                       binop(Iop_Add64,
                             binop(Iop_Shl64,
                                   getIReg(ra), mkU8(1)),
                             getIReg(rb)))));
      MARK_REG_WB(rd, t2);
      break;
    case 192:   
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_Add64,
                       binop(Iop_Shl64,
                             getIReg(ra), mkU8(2)),
                       getIReg(rb)));

      MARK_REG_WB(rd, t2);

      break;
    case 193:   
      t2 = newTemp(Ity_I64);
      assign(t2,
             unop(Iop_32Sto64,
                  unop(Iop_64to32,
                       binop(Iop_Add64,
                             binop(Iop_Shl64,
                                   getIReg(ra), mkU8(2)),
                             getIReg(rb)))));
      MARK_REG_WB(rd, t2);

      break;
    case 194:   
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_Add64,
                       binop(Iop_Shl64,
                             getIReg(ra), mkU8(3)),
                       getIReg(rb)));

      MARK_REG_WB(rd, t2);
      break;
    case 195:   
      t2 = newTemp(Ity_I64);
      assign(t2,
             unop(Iop_32Sto64,
                  unop(Iop_64to32,
                       binop(Iop_Add64,
                             binop(Iop_Shl64,
                                   getIReg(ra), mkU8(3)),
                             getIReg(rb)))));
      MARK_REG_WB(rd, t2);
      break;
    case 196:   
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_Shl64, getIReg(ra),
                       mkU8(imm)));
      MARK_REG_WB(rd, t2);
      break;
    case 197:   
      t2 = newTemp(Ity_I64);
      assign(t2, unop(Iop_32Sto64,
                      binop(Iop_Shl32,
                            narrowTo(Ity_I32, getIReg(ra)),
                            narrowTo(Ity_I8, getIReg(rb)))));
      MARK_REG_WB(rd, t2);
      break;
    case 198:   
      t2 = newTemp(Ity_I64);
      assign(t2, signExtend(binop(Iop_Shl32,
                                  narrowTo(Ity_I32, getIReg(ra)),
                                  mkU8(imm)),
                            32));
      MARK_REG_WB(rd, t2);
      break;
    case 199:  
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_Sar64, getIReg(ra),
                       narrowTo(Ity_I8, getIReg(rb))));

      MARK_REG_WB(rd, t2);
      break;
    case 200:  
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_Sar64, getIReg(ra),
                       mkU8(imm)));

      MARK_REG_WB(rd, t2);
      break;
    case 201:  
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_Shr64,
                       getIReg(ra),
                       narrowTo(Ity_I8, (getIReg(rb)))));

      MARK_REG_WB(rd, t2);
      break;
    case 202:  
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_Shr64, getIReg(ra), mkU8(imm)));

      MARK_REG_WB(rd, t2);
      break;
    case 203:  
      t2 = newTemp(Ity_I64);
      assign(t2, unop(Iop_32Sto64,
                      (binop(Iop_Shr32,
                             narrowTo(Ity_I32, getIReg(ra)),
                             narrowTo(Ity_I8, getIReg(rb))))));
      MARK_REG_WB(rd, t2);
      break;
    case 204:  
      t2 = newTemp(Ity_I64);
      assign(t2, unop(Iop_32Sto64,
                      (binop(Iop_Shr32,
                             narrowTo(Ity_I32, getIReg(ra)),
                             mkU8(imm)))));
      MARK_REG_WB(rd, t2);
      break;
    case 205:  
      use_dirty_helper = 1;
      break;
    case 206:  
      store(getIReg(ra),  getIReg(rb));
      break;
    case 207:  
      store(getIReg(ra),  narrowTo(Ity_I8, getIReg(rb)));
      break;
    case 208:  
      t2 = newTemp(Ity_I64);
      store(getIReg(opd[0]),  narrowTo(Ity_I8, getIReg(opd[1])));
      assign(t2, binop(Iop_Add64, getIReg(opd[0]), mkU64(opd[2])));
      MARK_REG_WB(opd[0], t2);
      break;
    case 209:  
      store(getIReg(ra),  narrowTo(Ity_I16, getIReg(rb)));
      break;
    case 210:  
      t2 = newTemp(Ity_I64);
      store(getIReg(opd[0]),  narrowTo(Ity_I16, getIReg(opd[1])));
      assign(t2, binop(Iop_Add64, getIReg(opd[0]), mkU64(opd[2])));
      MARK_REG_WB(opd[0], t2);
      break;
    case 211:  
      store(getIReg(ra),  narrowTo(Ity_I32, getIReg(rb)));
      break;
    case 212:  
      t2 = newTemp(Ity_I64);
      store(getIReg(opd[0]),  narrowTo(Ity_I32, getIReg(opd[1])));
      assign(t2, binop(Iop_Add64, getIReg(opd[0]), mkU64(opd[2])));
      MARK_REG_WB(opd[0], t2);
      break;
    case 213:  
      t2 = newTemp(Ity_I64);
      store(getIReg(opd[0]),  getIReg(opd[1]));
      assign(t2, binop(Iop_Add64, getIReg(opd[0]), mkU64(opd[2])));
      MARK_REG_WB(opd[0], t2);
      break;
    case 214:  
      store(getIReg(ra),  getIReg(rb));
      break;
    case 215:  
      store(getIReg(ra),  narrowTo(Ity_I8, getIReg(rb)));
      break;
    case 216:  
      t2 = newTemp(Ity_I64);
      store(getIReg(opd[0]),  narrowTo(Ity_I8, getIReg(opd[1])));
      assign(t2, binop(Iop_Add64, getIReg(opd[0]), mkU64(opd[2])));
      MARK_REG_WB(opd[0], t2);
      break;
    case 217:  
      store(getIReg(ra),  narrowTo(Ity_I16, getIReg(rb)));
      break;
    case 218:  
      t2 = newTemp(Ity_I64);
      store(getIReg(opd[0]),  narrowTo(Ity_I16, getIReg(opd[1])));
      assign(t2, binop(Iop_Add64, getIReg(opd[0]), mkU64(opd[2])));
      MARK_REG_WB(opd[0], t2);
      break;
    case 219:  
      store(getIReg(ra),  narrowTo(Ity_I32, getIReg(rb)));
      break;
    case 220:  
      t2 = newTemp(Ity_I64);
      store(getIReg(opd[0]),  narrowTo(Ity_I32, getIReg(opd[1])));
      assign(t2, binop(Iop_Add64, getIReg(opd[0]), mkU64(opd[2])));
      MARK_REG_WB(opd[0], t2);
      break;
    case 221:  
      t2 = newTemp(Ity_I64);
      store(getIReg(opd[0]),  getIReg(opd[1]));
      assign(t2, binop(Iop_Add64, getIReg(opd[0]), mkU64(opd[2])));
      MARK_REG_WB(opd[0], t2);
      break;
    case 222:  
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_Sub64, getIReg(ra),
                       getIReg(rb)));
      MARK_REG_WB(rd, t2);
      break;
    case 223:  
      t2 = newTemp(Ity_I64);
      assign(t2,  unop(Iop_32Sto64,
                       binop(Iop_Sub32,
                             narrowTo(Ity_I32, getIReg(ra)),
                             narrowTo(Ity_I32, getIReg(rb)))));
      MARK_REG_WB(rd, t2);
      break;
    case 224:  
      use_dirty_helper = 1;
      break;
    case 225:  
      vex_printf( "\n *** swint0 ***\n");
      vassert(0);
      break;
    case 226:  
      next = mkU64(guest_PC_curr_instr + 8);
      jumpkind = Ijk_Sys_syscall;
      break;
    case 227:  
      vex_printf( "\n *** swint2 ***\n");
      vassert(0);
      break;
    case 228:  
      vex_printf( "\n *** swint3 ***\n");
      vassert(0);
      break;
    case 229:
      
    case 230:
      
    case 231:
      
    case 232:
      
    case 233:
      
    case 234:
      
    case 235:
      
    case 236:
      
    case 237:
      use_dirty_helper = 1;
      break;
    case 238:  
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_CmpEQ8x8, getIReg(ra),
                       getIReg(rb)));
      MARK_REG_WB(rd, t2);
      break;
    case 239:  
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_CmpEQ8x8, getIReg(ra),
                       mkU64(imm)));

      MARK_REG_WB(rd, t2);
      break;
    case 240:
      
    case 241:
      
    case 242:
      
    case 243:
      
    case 244:
      
    case 245:
      use_dirty_helper = 1;
      break;
    case 246:  
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_CmpEQ8x8,
                       binop(Iop_CmpEQ8x8, getIReg(ra),
                             getIReg(rb)),
                       getIReg(63)));
      MARK_REG_WB(rd, t2);
      break;
    case 247:
      
    case 248:
      
    case 249:
      
    case 250:
      
    case 251:
      
    case 252:
      
    case 253:
      
    case 254:
      
    case 255:
      
    case 256:
      
    case 257:
      
    case 258:
      
    case 259:
      
    case 260:
      
    case 261:
      
    case 262:
      
    case 263:
      
    case 264:
      
    case 265:
      
    case 266:
      
    case 267:
      
    case 268:
      
    case 269:
      
    case 270:
      
    case 271:
      
    case 272:
      
    case 273:
      
    case 274:
      use_dirty_helper = 1;
      break;
    case 275:  
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_Shr8x8,
                       getIReg(ra),
                       mkU64(imm)));
      MARK_REG_WB(rd, t2);
      break;
    case 276:
      
    case 277:
      
    case 278:
      
    case 279:
      
    case 280:
      
    case 281:
      
    case 282:
      
    case 283:
      
    case 284:
      
    case 285:
      
    case 286:
      
    case 287:
      
    case 288:
      
    case 289:
      
    case 290:
      
    case 291:
      
    case 292:
      
    case 293:
      
    case 294:
      
    case 295:
      
    case 296:
      
    case 297:
      
    case 298:
      
    case 299:
      
    case 300:
      
    case 301:
      
    case 302:
      
    case 303:
      
    case 304:
      
    case 305:
      
    case 306:
      
    case 307:
      
    case 308:
      
    case 309:
      
    case 310:
      
    case 311:
      
    case 312:
      
    case 313:
      
    case 314:
      
    case 315:
      
    case 316:
      
    case 317:
      
    case 318:
      
    case 319:
      
    case 320:
      
    case 321:
      
    case 322:
      
    case 323:
      use_dirty_helper = 1;
      break;
    case 324:   
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_Or64,
                       binop(Iop_Shl64,
                             getIReg(ra),
                             mkU8(32)),
                       binop(Iop_And64,
                             getIReg(rb),
                             mkU64(0xFFFFFFFF))));
      MARK_REG_WB(rd, t2);
      break;
    case 325:
      
    case 326:
      
    case 327:
      
    case 328:
      
    case 329:
      
    case 330:
      
    case 331:
      use_dirty_helper = 1;
      break;
    case 332:        
      break;
    case 333:   
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_Xor64,
                       getIReg(ra),
                       getIReg(rb)));
      MARK_REG_WB(rd, t2);
      break;
    case 334:   
      t2 = newTemp(Ity_I64);
      assign(t2, binop(Iop_Xor64,
                       getIReg(ra),
                       mkU64(imm)));
      MARK_REG_WB(rd, t2);
      break;
    case 335:     
      break;
    default:

    decode_failure:
      vex_printf("error: %d\n",  (Int)opcode);

      
      vex_printf("vex tilegx->IR: unhandled instruction: "
                 "%s 0x%llx 0x%llx 0x%llx 0x%llx\n",
                 decoded[n].opcode->name,
                 opd[0], opd[1], opd[2], opd[3]);

      stmt(IRStmt_Put(offsetof(VexGuestTILEGXState, guest_pc),
                      mkU64(guest_PC_curr_instr)));
      dres.whatNext = Dis_StopHere;
      dres.len = 0;
      return dres;
    }

    
    if (use_dirty_helper)
    {
      Int i = 0;
      Int wbc = 0;
      IRExpr *opc_oprand[5];

      opc_oprand[0] = mkU64(opcode);

      
      for (i = 0 ; i < 4; i++)
      {
        opc_oprand[i + 1] = NULL;

        if (opd_dst_map & (1ULL << i))
        {
          tb[wbc] = newTemp(Ity_I64);
          wbc++;
          opc_oprand[i + 1] = getIReg(opd[i]);
        }
        else if (opd_imm_map & (1ULL << i))
          opc_oprand[i + 1] = mkU64(opd[i]);
        else if (opd_src_map & (1ULL << i))
          opc_oprand[i + 1] = getIReg(opd[i]);
        else
          opc_oprand[i + 1] = mkU64(0xfeee);
      }

      IRExpr **args = mkIRExprVec_5(opc_oprand[0], opc_oprand[1],
                                    opc_oprand[2], opc_oprand[3],
                                    opc_oprand[4]);
      IRDirty *genIR = NULL;

      switch (wbc) {
      case 0:
        {
          genIR = unsafeIRDirty_0_N (0,
                                     "tilegx_dirtyhelper_gen",
                                     &tilegx_dirtyhelper_gen,
                                     args);
        }
        break;
      case 1:
        {
          genIR = unsafeIRDirty_1_N (tb[0],
                                     0,
                                     "tilegx_dirtyhelper_gen",
                                     &tilegx_dirtyhelper_gen,
                                     args);
        }
        break;
      default:
        vex_printf("opc = %d\n", (Int)opcode);
        vassert(0);
      }

      stmt(IRStmt_Dirty(genIR));

      wbc = 0;
      for (i = 0 ; i < 4; i++)
      {
        if(opd_dst_map & (1 << i))
        {
          
          MARK_REG_WB(opd[i], tb[wbc]);
          wbc++;
        }
      }
    }
  }

  for (n = 0; n < rd_wb_index; n++)
    putIReg(rd_wb_reg[n], mkexpr(rd_wb_temp[n]));

  
  if (bstmt) {
    stmt(bstmt);
    dres.whatNext = Dis_StopHere;

    dres.jk_StopHere = jumpkind;
    stmt(IRStmt_Put(offsetof(VexGuestTILEGXState, guest_pc),
                    mkU64(guest_PC_curr_instr + 8)));
  } else if (next) {
    if (steering_pc != -1ULL) {
      if (resteerOkFn(callback_opaque, steering_pc)) {
        dres.whatNext   = Dis_ResteerU;
        dres.continueAt = steering_pc;
        stmt(IRStmt_Put(offsetof(VexGuestTILEGXState, guest_pc),
                        mkU64(steering_pc)));
      } else {
        dres.whatNext = Dis_StopHere;
        dres.jk_StopHere = jumpkind;
        stmt(IRStmt_Put(offsetof(VexGuestTILEGXState, guest_pc),
                        mkU64(steering_pc)));
      }
    } else {
      dres.whatNext = Dis_StopHere;
      dres.jk_StopHere = jumpkind;
      stmt(IRStmt_Put(offsetof(VexGuestTILEGXState, guest_pc), next));
    }
  } else {
    
    stmt(IRStmt_Put(offsetof(VexGuestTILEGXState, guest_pc),
                    mkU64(guest_PC_curr_instr + 8)));
  }

  irsb->jumpkind = Ijk_Boring;
  irsb->next = NULL;
  dres.len = 8;

 decode_success:

  return dres;
}



DisResult
disInstr_TILEGX ( IRSB* irsb_IN,
                  Bool (*resteerOkFn) (void *, Addr),
                  Bool resteerCisOk,
                  void* callback_opaque,
                  const UChar* guest_code_IN,
                  Long delta,
                  Addr guest_IP,
                  VexArch guest_arch,
                  const VexArchInfo* archinfo,
                  const VexAbiInfo* abiinfo,
                  VexEndness host_endness_IN,
                  Bool sigill_diag_IN )
{
  DisResult dres;

  
  vassert(guest_arch == VexArchTILEGX);

  guest_code = (UChar*)(Addr)guest_code_IN;
  irsb = irsb_IN;
  host_endness = host_endness_IN;
  guest_PC_curr_instr = (Addr64) guest_IP;
  guest_PC_bbstart = (Addr64) toUInt(guest_IP - delta);

  dres = disInstr_TILEGX_WRK(resteerOkFn, resteerCisOk,
                             callback_opaque,
                             delta, archinfo, abiinfo, sigill_diag_IN);

  return dres;
}

