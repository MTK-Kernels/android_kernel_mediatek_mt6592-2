

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright IBM Corp. 2010-2013

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


#ifndef __VEX_HOST_S390_DEFS_H
#define __VEX_HOST_S390_DEFS_H

#include "libvex_basictypes.h"            
#include "libvex.h"                       
#include "host_generic_regs.h"            
#include "s390_defs.h"                    

const HChar *s390_hreg_as_string(HReg);
HReg s390_hreg_gpr(UInt regno);
HReg s390_hreg_fpr(UInt regno);

HReg s390_hreg_guest_state_pointer(void);


static __inline__ UInt
s390_gprno_from_arg_index(UInt ix)
{
   return ix + 2;
}



typedef enum {
   S390_AMODE_B12,
   S390_AMODE_B20,
   S390_AMODE_BX12,
   S390_AMODE_BX20
} s390_amode_t;

typedef struct {
   s390_amode_t tag;
   HReg b;
   HReg x;       
   Int  d;       
} s390_amode;


s390_amode *s390_amode_b12(Int d, HReg b);
s390_amode *s390_amode_b20(Int d, HReg b);
s390_amode *s390_amode_bx12(Int d, HReg b, HReg x);
s390_amode *s390_amode_bx20(Int d, HReg b, HReg x);
s390_amode *s390_amode_for_guest_state(Int d);
Bool        s390_amode_is_sane(const s390_amode *);

const HChar *s390_amode_as_string(const s390_amode *);


typedef enum {
   S390_OPND_REG,
   S390_OPND_IMMEDIATE,
   S390_OPND_AMODE
} s390_opnd_t;



typedef struct {
   s390_opnd_t tag;
   union {
      HReg        reg;
      s390_amode *am;
      ULong       imm;
   } variant;
} s390_opnd_RMI;


typedef enum {
   S390_INSN_LOAD,   
   S390_INSN_STORE,  
   S390_INSN_MOVE,   
   S390_INSN_MEMCPY, 
   S390_INSN_COND_MOVE, 
   S390_INSN_LOAD_IMMEDIATE,
   S390_INSN_ALU,
   S390_INSN_SMUL,   
   S390_INSN_UMUL,   
   S390_INSN_SDIV,   
   S390_INSN_UDIV,   
   S390_INSN_DIVS,   
   S390_INSN_CLZ,    
   S390_INSN_UNOP,
   S390_INSN_TEST,   
   S390_INSN_CC2BOOL,
   S390_INSN_COMPARE,
   S390_INSN_HELPER_CALL,
   S390_INSN_CAS,    
   S390_INSN_CDAS,   
   S390_INSN_BFP_BINOP, 
   S390_INSN_BFP_UNOP,
   S390_INSN_BFP_TRIOP,
   S390_INSN_BFP_COMPARE,
   S390_INSN_BFP_CONVERT,
   S390_INSN_DFP_BINOP, 
   S390_INSN_DFP_UNOP,
   S390_INSN_DFP_INTOP,
   S390_INSN_DFP_COMPARE,
   S390_INSN_DFP_CONVERT,
   S390_INSN_DFP_REROUND,
   S390_INSN_FP_CONVERT,
   S390_INSN_MFENCE,
   S390_INSN_MIMM,    
   S390_INSN_MADD,    
   S390_INSN_SET_FPC_BFPRM, 
   S390_INSN_SET_FPC_DFPRM, 
   
   S390_INSN_XDIRECT,     
   S390_INSN_XINDIR,      
   S390_INSN_XASSISTED,   
   S390_INSN_EVCHECK,     
   S390_INSN_PROFINC      
} s390_insn_tag;


typedef enum {
   S390_ALU_ADD,
   S390_ALU_SUB,
   S390_ALU_MUL,   
   S390_ALU_AND,
   S390_ALU_OR,
   S390_ALU_XOR,
   S390_ALU_LSH,
   S390_ALU_RSH,
   S390_ALU_RSHA   
} s390_alu_t;


typedef enum {
   S390_ZERO_EXTEND_8,
   S390_ZERO_EXTEND_16,
   S390_ZERO_EXTEND_32,
   S390_SIGN_EXTEND_8,
   S390_SIGN_EXTEND_16,
   S390_SIGN_EXTEND_32,
   S390_NEGATE
} s390_unop_t;

typedef enum {
   S390_BFP_MADD,
   S390_BFP_MSUB,
} s390_bfp_triop_t;

typedef enum {
   S390_BFP_ADD,
   S390_BFP_SUB,
   S390_BFP_MUL,
   S390_BFP_DIV
} s390_bfp_binop_t;

typedef enum {
   S390_BFP_ABS,
   S390_BFP_NABS,
   S390_BFP_NEG,
   S390_BFP_SQRT
} s390_bfp_unop_t;

typedef enum {
   S390_BFP_I32_TO_F32,
   S390_BFP_I32_TO_F64,
   S390_BFP_I32_TO_F128,
   S390_BFP_I64_TO_F32,
   S390_BFP_I64_TO_F64,
   S390_BFP_I64_TO_F128,
   S390_BFP_U32_TO_F32,
   S390_BFP_U32_TO_F64,
   S390_BFP_U32_TO_F128,
   S390_BFP_U64_TO_F32,
   S390_BFP_U64_TO_F64,
   S390_BFP_U64_TO_F128,
   S390_BFP_F32_TO_I32,
   S390_BFP_F32_TO_I64,
   S390_BFP_F32_TO_U32,
   S390_BFP_F32_TO_U64,
   S390_BFP_F32_TO_F64,
   S390_BFP_F32_TO_F128,
   S390_BFP_F64_TO_I32,
   S390_BFP_F64_TO_I64,
   S390_BFP_F64_TO_U32,
   S390_BFP_F64_TO_U64,
   S390_BFP_F64_TO_F32,
   S390_BFP_F64_TO_F128,
   S390_BFP_F128_TO_I32,
   S390_BFP_F128_TO_I64,
   S390_BFP_F128_TO_U32,
   S390_BFP_F128_TO_U64,
   S390_BFP_F128_TO_F32,
   S390_BFP_F128_TO_F64
} s390_bfp_conv_t;

typedef enum {
   S390_DFP_D32_TO_D64,
   S390_DFP_D64_TO_D32,
   S390_DFP_D64_TO_D128,
   S390_DFP_D128_TO_D64,
   S390_DFP_I32_TO_D64,
   S390_DFP_I32_TO_D128,
   S390_DFP_I64_TO_D64,
   S390_DFP_I64_TO_D128,
   S390_DFP_U32_TO_D64,
   S390_DFP_U32_TO_D128,
   S390_DFP_U64_TO_D64,
   S390_DFP_U64_TO_D128,
   S390_DFP_D64_TO_I32,
   S390_DFP_D64_TO_I64,
   S390_DFP_D64_TO_U32,
   S390_DFP_D64_TO_U64,
   S390_DFP_D128_TO_I32,
   S390_DFP_D128_TO_I64,
   S390_DFP_D128_TO_U32,
   S390_DFP_D128_TO_U64
} s390_dfp_conv_t;

typedef enum {
   S390_FP_F32_TO_D32,
   S390_FP_F32_TO_D64,
   S390_FP_F32_TO_D128,
   S390_FP_F64_TO_D32,
   S390_FP_F64_TO_D64,
   S390_FP_F64_TO_D128,
   S390_FP_F128_TO_D32,
   S390_FP_F128_TO_D64,
   S390_FP_F128_TO_D128,
   S390_FP_D32_TO_F32,
   S390_FP_D32_TO_F64,
   S390_FP_D32_TO_F128,
   S390_FP_D64_TO_F32,
   S390_FP_D64_TO_F64,
   S390_FP_D64_TO_F128,
   S390_FP_D128_TO_F32,
   S390_FP_D128_TO_F64,
   S390_FP_D128_TO_F128
} s390_fp_conv_t;

typedef enum {
   S390_DFP_ADD,
   S390_DFP_SUB,
   S390_DFP_MUL,
   S390_DFP_DIV,
   S390_DFP_QUANTIZE
} s390_dfp_binop_t;

typedef enum {
   S390_DFP_EXTRACT_EXP_D64,
   S390_DFP_EXTRACT_EXP_D128,
   S390_DFP_EXTRACT_SIG_D64,
   S390_DFP_EXTRACT_SIG_D128,
} s390_dfp_unop_t;

typedef enum {
   S390_DFP_SHIFT_LEFT,
   S390_DFP_SHIFT_RIGHT,
   S390_DFP_INSERT_EXP
} s390_dfp_intop_t;

typedef enum {
   S390_DFP_COMPARE,
   S390_DFP_COMPARE_EXP,
} s390_dfp_cmp_t;

typedef struct {
   HReg        op1_high;
   HReg        op1_low;
   s390_amode *op2;
   HReg        op3_high;
   HReg        op3_low;
   HReg        old_mem_high;
   HReg        old_mem_low;
   HReg        scratch;
} s390_cdas;

typedef struct {
   s390_dfp_binop_t tag;
   s390_dfp_round_t rounding_mode;
   HReg         dst_hi; 
   HReg         dst_lo; 
   HReg         op2_hi; 
   HReg         op2_lo; 
   HReg         op3_hi; 
   HReg         op3_lo; 
} s390_dfp_binop;

typedef struct {
   s390_fp_conv_t  tag;
   s390_dfp_round_t rounding_mode;
   HReg         dst_hi; 
   HReg         dst_lo; 
   HReg         op_hi;  
   HReg         op_lo;  
   HReg         r1;     
} s390_fp_convert;

typedef struct {
   s390_cc_t    cond     : 16;
   UInt         num_args : 16;
   RetLoc       rloc;     
   Addr64       target;
   const HChar *name;      
} s390_helper_call;

typedef struct {
   s390_insn_tag tag;
   UChar size;
   union {
      struct {
         HReg        dst;
         s390_amode *src;
      } load;
      struct {
         s390_amode *dst;
         HReg        src;
      } store;
      struct {
         HReg        dst;
         HReg        src;
      } move;
      struct {
         s390_amode *dst;
         s390_amode *src;
      } memcpy;
      struct {
         s390_cc_t     cond;
         HReg          dst;
         s390_opnd_RMI src;
      } cond_move;
      struct {
         HReg        dst;
         ULong       value;  
      } load_immediate;
      
      struct {
         s390_alu_t    tag;
         HReg          dst; 
         s390_opnd_RMI op2;
      } alu;
      struct {
         HReg          dst_hi;  
         HReg          dst_lo;  
         s390_opnd_RMI op2;
      } mul;
      struct {
         HReg          op1_hi;  
         HReg          op1_lo;  
         s390_opnd_RMI op2;
      } div;
      struct {
         HReg          rem; 
         HReg          op1; 
         s390_opnd_RMI op2;
      } divs;
      struct {
         HReg          num_bits; 
         HReg          clobber;  
         s390_opnd_RMI src;
      } clz;
      struct {
         s390_unop_t   tag;
         HReg          dst;
         s390_opnd_RMI src;
      } unop;
      struct {
         Bool          signed_comparison;
         HReg          src1;
         s390_opnd_RMI src2;
      } compare;
      struct {
         s390_opnd_RMI src;
      } test;
      
      struct {
         s390_cc_t cond;
         HReg      dst;
      } cc2bool;
      struct {
         HReg        op1;
         s390_amode *op2;
         HReg        op3;
         HReg        old_mem;
      } cas;
      struct {
         s390_cdas *details;
      } cdas;
      struct {
         s390_helper_call *details;
      } helper_call;


      
      struct {
         s390_bfp_triop_t tag;
         HReg         dst;
         HReg         op2;
         HReg         op3;
      } bfp_triop;
      struct {
         s390_bfp_binop_t tag;
         HReg         dst_hi; 
         HReg         dst_lo; 
         HReg         op2_hi; 
         HReg         op2_lo; 
      } bfp_binop;
      struct {
         s390_bfp_unop_t  tag;
         HReg         dst_hi; 
         HReg         dst_lo; 
         HReg         op_hi;  
         HReg         op_lo;  
      } bfp_unop;
      struct {
         s390_bfp_conv_t  tag;
         s390_bfp_round_t rounding_mode;
         HReg         dst_hi; 
         HReg         dst_lo; 
         HReg         op_hi;  
         HReg         op_lo;  
      } bfp_convert;
      struct {
         HReg         dst;     
         HReg         op1_hi;  
         HReg         op1_lo;  
         HReg         op2_hi;  
         HReg         op2_lo;  
      } bfp_compare;
      struct {
         s390_dfp_binop *details;
      } dfp_binop;
      struct {
         s390_dfp_unop_t tag;
         HReg         dst_hi; 
         HReg         dst_lo; 
         HReg         op_hi;  
         HReg         op_lo;  
      } dfp_unop;
      struct {
         s390_dfp_intop_t tag;
         HReg         dst_hi; 
         HReg         dst_lo; 
         HReg         op2;    
         HReg         op3_hi; 
         HReg         op3_lo; 
      } dfp_intop;
      struct {
         s390_dfp_conv_t  tag;
         s390_dfp_round_t rounding_mode;
         HReg         dst_hi; 
         HReg         dst_lo; 
         HReg         op_hi;  
         HReg         op_lo;  
      } dfp_convert;
      struct {
         s390_fp_convert *details;
      } fp_convert;
      struct {
         s390_dfp_cmp_t tag;
         HReg         dst;     
         HReg         op1_hi;  
         HReg         op1_lo;  
         HReg         op2_hi;  
         HReg         op2_lo;  
      } dfp_compare;
      struct {
         s390_dfp_round_t rounding_mode;
         HReg         dst_hi; 
         HReg         dst_lo; 
         HReg         op2;    
         HReg         op3_hi; 
         HReg         op3_lo; 
      } dfp_reround;

      
      struct {
         s390_amode      *dst;
         ULong            value;  
      } mimm;
      struct {
         s390_amode      *dst;
         UChar            delta;
         ULong            value;  
      } madd;
      struct {
         HReg             mode;
      } set_fpc_bfprm;
      struct {
         HReg             mode;
      } set_fpc_dfprm;

      

      struct {
         s390_cc_t     cond;
         Bool          to_fast_entry;  
         Addr64        dst;            
         s390_amode   *guest_IA;
      } xdirect;
      struct {
         s390_cc_t     cond;
         HReg          dst;
         s390_amode   *guest_IA;
      } xindir;
      struct {
         s390_cc_t     cond;
         IRJumpKind    kind;
         HReg          dst;
         s390_amode   *guest_IA;
      } xassisted;
      struct {
         s390_amode   *counter;    
         s390_amode   *fail_addr;
      } evcheck;
      struct {
      } profinc;

   } variant;
} s390_insn;

s390_insn *s390_insn_load(UChar size, HReg dst, s390_amode *src);
s390_insn *s390_insn_store(UChar size, s390_amode *dst, HReg src);
s390_insn *s390_insn_move(UChar size, HReg dst, HReg src);
s390_insn *s390_insn_memcpy(UChar size, s390_amode *dst, s390_amode *src);
s390_insn *s390_insn_cond_move(UChar size, s390_cc_t cond, HReg dst,
                               s390_opnd_RMI src);
s390_insn *s390_insn_load_immediate(UChar size, HReg dst, ULong val);
s390_insn *s390_insn_alu(UChar size, s390_alu_t, HReg dst,
                         s390_opnd_RMI op2);
s390_insn *s390_insn_mul(UChar size, HReg dst_hi, HReg dst_lo,
                         s390_opnd_RMI op2, Bool signed_multiply);
s390_insn *s390_insn_div(UChar size, HReg op1_hi, HReg op1_lo,
                         s390_opnd_RMI op2, Bool signed_divide);
s390_insn *s390_insn_divs(UChar size, HReg rem, HReg op1, s390_opnd_RMI op2);
s390_insn *s390_insn_clz(UChar size, HReg num_bits, HReg clobber,
                         s390_opnd_RMI op);
s390_insn *s390_insn_cas(UChar size, HReg op1, s390_amode *op2, HReg op3,
                         HReg old);
s390_insn *s390_insn_cdas(UChar size, HReg op1_high, HReg op1_low,
                          s390_amode *op2, HReg op3_high, HReg op3_low,
                          HReg old_high, HReg old_low, HReg scratch);
s390_insn *s390_insn_unop(UChar size, s390_unop_t tag, HReg dst,
                          s390_opnd_RMI opnd);
s390_insn *s390_insn_cc2bool(HReg dst, s390_cc_t src);
s390_insn *s390_insn_test(UChar size, s390_opnd_RMI src);
s390_insn *s390_insn_compare(UChar size, HReg dst, s390_opnd_RMI opnd,
                             Bool signed_comparison);
s390_insn *s390_insn_helper_call(s390_cc_t cond, Addr64 target, UInt num_args,
                                 const HChar *name, RetLoc rloc);
s390_insn *s390_insn_bfp_triop(UChar size, s390_bfp_triop_t, HReg dst,
                               HReg op2, HReg op3);
s390_insn *s390_insn_bfp_binop(UChar size, s390_bfp_binop_t, HReg dst,
                               HReg op2);
s390_insn *s390_insn_bfp_unop(UChar size, s390_bfp_unop_t tag, HReg dst,
                              HReg op);
s390_insn *s390_insn_bfp_compare(UChar size, HReg dst, HReg op1, HReg op2);
s390_insn *s390_insn_bfp_convert(UChar size, s390_bfp_conv_t tag, HReg dst,
                                 HReg op, s390_bfp_round_t);
s390_insn *s390_insn_bfp128_binop(UChar size, s390_bfp_binop_t, HReg dst_hi,
                                  HReg dst_lo, HReg op2_hi, HReg op2_lo);
s390_insn *s390_insn_bfp128_unop(UChar size, s390_bfp_unop_t, HReg dst_hi,
                                 HReg dst_lo, HReg op_hi, HReg op_lo);
s390_insn *s390_insn_bfp128_compare(UChar size, HReg dst, HReg op1_hi,
                                    HReg op1_lo, HReg op2_hi, HReg op2_lo);
s390_insn *s390_insn_bfp128_convert_to(UChar size, s390_bfp_conv_t,
                                       HReg dst_hi, HReg dst_lo, HReg op);
s390_insn *s390_insn_bfp128_convert_from(UChar size, s390_bfp_conv_t,
                                         HReg dst_hi, HReg dst_lo, HReg op_hi,
                                         HReg op_lo, s390_bfp_round_t);
s390_insn *s390_insn_dfp_binop(UChar size, s390_dfp_binop_t, HReg dst,
                               HReg op2, HReg op3,
                               s390_dfp_round_t rounding_mode);
s390_insn *s390_insn_dfp_unop(UChar size, s390_dfp_unop_t, HReg dst, HReg op);
s390_insn *s390_insn_dfp_intop(UChar size, s390_dfp_intop_t, HReg dst,
                               HReg op2, HReg op3);
s390_insn *s390_insn_dfp_compare(UChar size, s390_dfp_cmp_t, HReg dst,
                                 HReg op1, HReg op2);
s390_insn *s390_insn_dfp_convert(UChar size, s390_dfp_conv_t tag, HReg dst,
                                 HReg op, s390_dfp_round_t);
s390_insn *s390_insn_dfp_reround(UChar size, HReg dst, HReg op2, HReg op3,
                                 s390_dfp_round_t);
s390_insn *s390_insn_fp_convert(UChar size, s390_fp_conv_t tag,
                                HReg dst, HReg op, HReg r1, s390_dfp_round_t);
s390_insn *s390_insn_fp128_convert(UChar size, s390_fp_conv_t tag,
                                   HReg dst_hi, HReg dst_lo, HReg op_hi,
                                   HReg op_lo, HReg r1, s390_dfp_round_t);
s390_insn *s390_insn_dfp128_binop(UChar size, s390_dfp_binop_t, HReg dst_hi,
                                  HReg dst_lo, HReg op2_hi, HReg op2_lo,
                                  HReg op3_hi, HReg op3_lo,
                                  s390_dfp_round_t rounding_mode);
s390_insn *s390_insn_dfp128_unop(UChar size, s390_dfp_unop_t, HReg dst,
                                 HReg op_hi, HReg op_lo);
s390_insn *s390_insn_dfp128_intop(UChar size, s390_dfp_intop_t, HReg dst_hi,
                                  HReg dst_lo, HReg op2,
                                  HReg op3_hi, HReg op3_lo);
s390_insn *s390_insn_dfp128_compare(UChar size, s390_dfp_cmp_t, HReg dst,
                                    HReg op1_hi, HReg op1_lo, HReg op2_hi,
                                    HReg op2_lo);
s390_insn *s390_insn_dfp128_convert_to(UChar size, s390_dfp_conv_t,
                                       HReg dst_hi, HReg dst_lo, HReg op);
s390_insn *s390_insn_dfp128_convert_from(UChar size, s390_dfp_conv_t,
                                         HReg dst_hi, HReg dst_lo, HReg op_hi,
                                         HReg op_lo, s390_dfp_round_t);
s390_insn *s390_insn_dfp128_reround(UChar size, HReg dst_hi, HReg dst_lo,
                                    HReg op2, HReg op3_hi, HReg op3_lo,
                                    s390_dfp_round_t);
s390_insn *s390_insn_mfence(void);
s390_insn *s390_insn_mimm(UChar size, s390_amode *dst, ULong value);
s390_insn *s390_insn_madd(UChar size, s390_amode *dst, UChar delta,
                          ULong value);
s390_insn *s390_insn_set_fpc_bfprm(UChar size, HReg mode);
s390_insn *s390_insn_set_fpc_dfprm(UChar size, HReg mode);

s390_insn *s390_insn_xdirect(s390_cc_t cond, Addr64 dst, s390_amode *guest_IA,
                             Bool to_fast_entry);
s390_insn *s390_insn_xindir(s390_cc_t cond, HReg dst, s390_amode *guest_IA);
s390_insn *s390_insn_xassisted(s390_cc_t cond, HReg dst, s390_amode *guest_IA,
                               IRJumpKind kind);
s390_insn *s390_insn_evcheck(s390_amode *counter, s390_amode *fail_addr);
s390_insn *s390_insn_profinc(void);

const HChar *s390_insn_as_string(const s390_insn *);


void ppS390AMode(const s390_amode *);
void ppS390Instr(const s390_insn *, Bool mode64);
void ppHRegS390(HReg);

void  getRegUsage_S390Instr( HRegUsage *, const s390_insn *, Bool );
void  mapRegs_S390Instr    ( HRegRemap *, s390_insn *, Bool );
Bool  isMove_S390Instr     ( const s390_insn *, HReg *, HReg * );
Int   emit_S390Instr       ( Bool *, UChar *, Int, const s390_insn *, Bool,
                             VexEndness, const void *, const void *,
                             const void *, const void *);
const RRegUniverse *getRRegUniverse_S390( void );
void  genSpill_S390        ( HInstr **, HInstr **, HReg , Int , Bool );
void  genReload_S390       ( HInstr **, HInstr **, HReg , Int , Bool );
HInstrArray *iselSB_S390   ( const IRSB *, VexArch, const VexArchInfo *,
                             const VexAbiInfo *, Int, Int, Bool, Bool, Addr);

Int evCheckSzB_S390(void);

VexInvalRange chainXDirect_S390(VexEndness endness_host,
                                void *place_to_chain,
                                const void *disp_cp_chain_me_EXPECTED,
                                const void *place_to_jump_to);

VexInvalRange unchainXDirect_S390(VexEndness endness_host,
                                  void *place_to_unchain,
                                  const void *place_to_jump_to_EXPECTED,
                                  const void *disp_cp_chain_me);

VexInvalRange patchProfInc_S390(VexEndness endness_host,
                                void  *code_to_patch,
                                const ULong *location_of_counter);

extern UInt s390_host_hwcaps;

#define s390_host_has_ldisp \
                      (s390_host_hwcaps & (VEX_HWCAPS_S390X_LDISP))
#define s390_host_has_eimm \
                      (s390_host_hwcaps & (VEX_HWCAPS_S390X_EIMM))
#define s390_host_has_gie \
                      (s390_host_hwcaps & (VEX_HWCAPS_S390X_GIE))
#define s390_host_has_dfp \
                      (s390_host_hwcaps & (VEX_HWCAPS_S390X_DFP))
#define s390_host_has_fgx \
                      (s390_host_hwcaps & (VEX_HWCAPS_S390X_FGX))
#define s390_host_has_etf2 \
                      (s390_host_hwcaps & (VEX_HWCAPS_S390X_ETF2))
#define s390_host_has_stfle \
                      (s390_host_hwcaps & (VEX_HWCAPS_S390X_STFLE))
#define s390_host_has_etf3 \
                      (s390_host_hwcaps & (VEX_HWCAPS_S390X_ETF3))
#define s390_host_has_stckf \
                      (s390_host_hwcaps & (VEX_HWCAPS_S390X_STCKF))
#define s390_host_has_fpext \
                      (s390_host_hwcaps & (VEX_HWCAPS_S390X_FPEXT))
#define s390_host_has_lsc \
                      (s390_host_hwcaps & (VEX_HWCAPS_S390X_LSC))
#define s390_host_has_pfpo \
                      (s390_host_hwcaps & (VEX_HWCAPS_S390X_PFPO))

#endif 

