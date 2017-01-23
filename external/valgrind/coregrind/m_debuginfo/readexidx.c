

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2014-2014 Mozilla Foundation

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

/* libunwind - a platform-independent unwind library
   Copyright 2011 Linaro Limited

This file is part of libunwind.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */


// Copyright (c) 2010 Google Inc.
//     * Redistributions of source code must retain the above copyright
// copyright notice, this list of conditions and the following disclaimer
// this software without specific prior written permission.
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT









#if defined(VGA_arm)

#include "pub_core_basics.h"
#include "pub_core_libcbase.h"
#include "pub_core_libcprint.h"
#include "pub_core_libcassert.h"
#include "pub_core_options.h"

#include "priv_storage.h"
#include "priv_readexidx.h"


static void complain ( const HChar* str )
{
   if (!VG_(clo_xml) && VG_(clo_verbosity) > 1)
      VG_(message)(Vg_UserMsg,
                   "  Warning: whilst reading EXIDX: %s\n", str);
}



typedef  struct { Addr start; SizeT len; }  MemoryRange;

static Bool MemoryRange__init ( MemoryRange* mr,
                                const void* startV, SizeT len )
{
   VG_(memset)(mr, 0, sizeof(*mr));
   
   Addr start = (Addr)startV;
   if (len > 0 && start + len - 1 < start) {
      return False;
   }
   mr->start = start;
   mr->len   = len;
   return True;
}

static Bool MemoryRange__covers ( MemoryRange* mr,
                                  const void* startV, SizeT len )
{
   vg_assert(len > 0);
   if (mr->len == 0) {
      return False;
   }
   Addr start = (Addr)startV;
   return start >= mr->start && start + len - 1 <= mr->start + mr->len - 1;
}



#define ARM_EXIDX_CANT_UNWIND 0x00000001
#define ARM_EXIDX_COMPACT     0x80000000
#define ARM_EXTBL_OP_FINISH   0xb0
#define ARM_EXIDX_TABLE_LIMIT (255*4)

typedef
   struct { UInt addr; UInt data; }
   ExidxEntry;


typedef
   enum {
      ExSuccess=1,      
      ExInBufOverflow,  
      ExOutBufOverflow, 
      ExCantUnwind,     
      ExCantRepresent,  
      ExInvalid         
   }
   ExExtractResult;


static void* Prel31ToAddr(const void* addr)
{
   UInt offset32 = *(const UInt*)addr;
   
   
   ULong offset64 = offset32;
   if (offset64 & (1ULL << 30))
      offset64 |= 0xFFFFFFFF80000000ULL;
   else
      offset64 &= 0x000000007FFFFFFFULL;
   return ((UChar*)addr) + (UWord)offset64;
}


// and return the number of bytes of |buf| written, along with a code
static
ExExtractResult ExtabEntryExtract ( MemoryRange* mr_exidx,
                                    MemoryRange* mr_extab,
                                    const ExidxEntry* entry,
                                    UChar* buf, SizeT buf_size,
                                    SizeT* buf_used)
{
   Bool ok;
   MemoryRange mr_out;
   ok = MemoryRange__init(&mr_out, buf, buf_size);
   if (!ok) return ExOutBufOverflow;

   *buf_used = 0;

#  define PUT_BUF_U8(_byte) \
      do { if (!MemoryRange__covers(&mr_out, &buf[*buf_used], 1)) \
              return ExOutBufOverflow; \
           buf[(*buf_used)++] = (_byte); } while (0)

#  define GET_EX_U32(_lval, _addr, _mr) \
      do { if (!MemoryRange__covers((_mr), (void*)(_addr), 4)) \
              return ExInBufOverflow; \
           (_lval) = *(UInt*)(_addr); } while (0)

#  define GET_EXIDX_U32(_lval, _addr) \
      GET_EX_U32(_lval, _addr, mr_exidx)

#  define GET_EXTAB_U32(_lval, _addr) \
      GET_EX_U32(_lval, _addr, mr_extab)

   UInt data;
   GET_EXIDX_U32(data, &entry->data);

   
   
   if (data == ARM_EXIDX_CANT_UNWIND)
      return ExCantUnwind;

   UInt  pers;          
   UInt  extra;         
   UInt  extra_allowed; 
   UInt* extbl_data;    

   if (data & ARM_EXIDX_COMPACT) {
      
      
      
      
      
      
      
      
      extbl_data = NULL;
      pers  = (data >> 24) & 0x0F;
      extra = (data >> 16) & 0xFF;
      extra_allowed = 0;
   }
   else {
      
      
      
      extbl_data = (UInt*)(Prel31ToAddr(&entry->data));
      GET_EXTAB_U32(data, extbl_data);
      if (!(data & ARM_EXIDX_COMPACT)) {
         
         
         
         return ExCantRepresent;
      }
      
      
      pers  = (data >> 24) & 0x0F;
      extra = (data >> 16) & 0xFF;
      extra_allowed = 255;
      extbl_data++;
   }

   
   
   
   
   
   if (pers == 0) {
      
      
      PUT_BUF_U8(data >> 16);
      PUT_BUF_U8(data >> 8);
      PUT_BUF_U8(data);
   }
   else if ((pers == 1 || pers == 2) && extra <= extra_allowed) {
      
      
      PUT_BUF_U8(data >> 8);
      PUT_BUF_U8(data);
      UInt j;
      for (j = 0; j < extra; j++) {
         GET_EXTAB_U32(data, extbl_data);
         extbl_data++;
         PUT_BUF_U8(data >> 24);
         PUT_BUF_U8(data >> 16);
         PUT_BUF_U8(data >> 8);
         PUT_BUF_U8(data >> 0);
      }
   }
   else {
      
      return ExInvalid;
   }

   
   if (*buf_used > 0 && buf[(*buf_used) - 1] != ARM_EXTBL_OP_FINISH)
      PUT_BUF_U8(ARM_EXTBL_OP_FINISH);

   return ExSuccess;

#  undef GET_EXTAB_U32
#  undef GET_EXIDX_U32
#  undef GET_U32
#  undef PUT_BUF_U8
}



typedef 
   enum {
      ARM_EXIDX_CMD_FINISH=0x100,
      ARM_EXIDX_CMD_SUB_FROM_VSP,
      ARM_EXIDX_CMD_ADD_TO_VSP,
      ARM_EXIDX_CMD_REG_POP,
      ARM_EXIDX_CMD_REG_TO_SP,
      ARM_EXIDX_CMD_VFP_POP,
      ARM_EXIDX_CMD_WREG_POP,
      ARM_EXIDX_CMD_WCGR_POP,
      ARM_EXIDX_CMD_RESERVED,
      ARM_EXIDX_CMD_REFUSED
   }
   ExtabCmd;

static const HChar* showExtabCmd ( ExtabCmd cmd ) {
   switch (cmd) {
      case ARM_EXIDX_CMD_FINISH:       return "FINISH";
      case ARM_EXIDX_CMD_SUB_FROM_VSP: return "SUB_FROM_VSP";
      case ARM_EXIDX_CMD_ADD_TO_VSP:   return "ADD_TO_VSP";
      case ARM_EXIDX_CMD_REG_POP:      return "REG_POP";
      case ARM_EXIDX_CMD_REG_TO_SP:    return "REG_TO_SP";
      case ARM_EXIDX_CMD_VFP_POP:      return "VFP_POP";
      case ARM_EXIDX_CMD_WREG_POP:     return "WREG_POP";
      case ARM_EXIDX_CMD_WCGR_POP:     return "WCGR_POP";
      case ARM_EXIDX_CMD_RESERVED:     return "RESERVED";
      case ARM_EXIDX_CMD_REFUSED:      return "REFUSED";
      default:                         return "???";
   }
}


typedef
   struct { ExtabCmd cmd; UInt data; }
   ExtabData;

static void ppExtabData ( const ExtabData* etd ) {
   VG_(printf)("ExtabData{%-12s 0x%08x}", showExtabCmd(etd->cmd), etd->data);
}


enum extab_cmd_flags {
   ARM_EXIDX_VFP_SHIFT_16 = 1 << 16,
   ARM_EXIDX_VFP_FSTMD = 1 << 17, 
};


typedef  struct _SummState  SummState;
static Int TranslateCmd(SummState* state, const ExtabData* edata);


static
Int ExtabEntryDecode(SummState* state, const UChar* buf, SizeT buf_size)
{
   if (buf == NULL || buf_size == 0)
      return -3;

   MemoryRange mr_in;
   Bool ok = MemoryRange__init(&mr_in, buf, buf_size);
   if (!ok)
      return -2;

#  define GET_BUF_U8(_lval) \
      do { if (!MemoryRange__covers(&mr_in, buf, 1)) \
              return -4; \
           (_lval) = *(buf++); } while (0)

   const UChar* end = buf + buf_size;

   while (buf < end) {
      ExtabData edata;
      VG_(bzero_inline)(&edata, sizeof(edata));

      UChar op;
      GET_BUF_U8(op);
      if ((op & 0xc0) == 0x00) {
         
         edata.cmd  = ARM_EXIDX_CMD_ADD_TO_VSP;
         edata.data = (((Int)op & 0x3f) << 2) + 4;
      }
      else if ((op & 0xc0) == 0x40) {
         
         edata.cmd  = ARM_EXIDX_CMD_SUB_FROM_VSP;
         edata.data = (((Int)op & 0x3f) << 2) + 4;
      }
      else if ((op & 0xf0) == 0x80) {
         UChar op2;
         GET_BUF_U8(op2);
         if (op == 0x80 && op2 == 0x00) {
            
            edata.cmd = ARM_EXIDX_CMD_REFUSED;
         } else {
            
            edata.cmd  = ARM_EXIDX_CMD_REG_POP;
            edata.data = ((op & 0xf) << 8) | op2;
            edata.data = edata.data << 4;
         }
      }
      else if ((op & 0xf0) == 0x90) {
         if (op == 0x9d || op == 0x9f) {
            
            
            edata.cmd = ARM_EXIDX_CMD_RESERVED;
         } else {
            
            edata.cmd  = ARM_EXIDX_CMD_REG_TO_SP;
            edata.data = op & 0x0f;
         }
      }
      else if ((op & 0xf0) == 0xa0) {
         
         
         Int nnn    = (op & 0x07);
         edata.data = (1 << (nnn + 1)) - 1;
         edata.data = edata.data << 4;
         if (op & 0x08) edata.data |= 1 << 14;
         edata.cmd = ARM_EXIDX_CMD_REG_POP;
      }
      else if (op == ARM_EXTBL_OP_FINISH) {
         
         edata.cmd = ARM_EXIDX_CMD_FINISH;
         buf = end;
      }
      else if (op == 0xb1) {
         UChar op2;
         GET_BUF_U8(op2);
         if (op2 == 0 || (op2 & 0xf0)) {
            
            edata.cmd = ARM_EXIDX_CMD_RESERVED;
         } else {
            
            edata.cmd = ARM_EXIDX_CMD_REG_POP;
            edata.data = op2 & 0x0f;
         }
      }
      else if (op == 0xb2) {
         
         ULong offset = 0;
         UChar byte, shift = 0;
         do {
            GET_BUF_U8(byte);
            offset |= (byte & 0x7f) << shift;
            shift += 7;
         } while ((byte & 0x80) && buf < end);
         edata.data = offset * 4 + 0x204;
         edata.cmd  = ARM_EXIDX_CMD_ADD_TO_VSP;
      }
      else if (op == 0xb3 || op == 0xc8 || op == 0xc9) {
         
         
         
         edata.cmd = ARM_EXIDX_CMD_VFP_POP;
         GET_BUF_U8(edata.data);
         if (op == 0xc8) edata.data |= ARM_EXIDX_VFP_SHIFT_16;
         if (op != 0xb3) edata.data |= ARM_EXIDX_VFP_FSTMD;
      }
      else if ((op & 0xf8) == 0xb8 || (op & 0xf8) == 0xd0) {
         
         
         edata.cmd  = ARM_EXIDX_CMD_VFP_POP;
         edata.data = 0x80 | (op & 0x07);
         if ((op & 0xf8) == 0xd0) edata.data |= ARM_EXIDX_VFP_FSTMD;
      }
      else if (op >= 0xc0 && op <= 0xc5) {
         
         edata.cmd  = ARM_EXIDX_CMD_WREG_POP;
         edata.data = 0xa0 | (op & 0x07);
      }
      else if (op == 0xc6) {
         
         edata.cmd = ARM_EXIDX_CMD_WREG_POP;
         GET_BUF_U8(edata.data);
      }
      else if (op == 0xc7) {
         UChar op2;
         GET_BUF_U8(op2);
         if (op2 == 0 || (op2 & 0xf0)) {
            
            edata.cmd = ARM_EXIDX_CMD_RESERVED;
         } else {
            
            edata.cmd = ARM_EXIDX_CMD_WCGR_POP;
            edata.data = op2 & 0x0f;
         }
      }
      else {
         
         edata.cmd = ARM_EXIDX_CMD_RESERVED;
      }

      if (0)
         VG_(printf)("  edata:  cmd %08x  data %08x\n",
                     (UInt)edata.cmd, (UInt)edata.data);

      Int ret = TranslateCmd ( state, &edata );
      if (ret < 0) return ret;
   }
   return 0;

# undef GET_BUF_U8
}




struct _SummState {
   
   DiCfSI_m   cfi;
   Int        vsp_off;
   
   DebugInfo* di;
};


static
Int gen_CfiExpr_CfiReg_ARM_GPR ( DebugInfo* di, UInt gprNo )
{
   CfiReg creg = Creg_INVALID;
   switch (gprNo) {
      case 13: creg = Creg_ARM_R13; break;
      case 12: creg = Creg_ARM_R12; break;
      case 15: creg = Creg_ARM_R15; break;
      case 14: creg = Creg_ARM_R14; break;
      case 7:  creg = Creg_ARM_R7;  break;
      default: break;
   }
   if (creg == Creg_INVALID) {
      return -1;
   }
   if (!di->cfsi_exprs) {
      di->cfsi_exprs = VG_(newXA)( ML_(dinfo_zalloc), "di.gCCAG",
                                   ML_(dinfo_free), sizeof(CfiExpr) );
   }
   Int res = ML_(CfiExpr_CfiReg)( di->cfsi_exprs, creg );
   vg_assert(res >= 0);
   return res;
}


static
void maybeFindExprForRegno( UChar** howPP, Int** offPP,
                            DiCfSI_m* cfsi_m, Int regNo )
{
   switch (regNo) {
      case 15: *howPP = &cfsi_m->ra_how;  *offPP = &cfsi_m->ra_off;  return;
      case 14: *howPP = &cfsi_m->r14_how; *offPP = &cfsi_m->r14_off; return;
      case 13: *howPP = &cfsi_m->r13_how; *offPP = &cfsi_m->r13_off; return;
      case 12: *howPP = &cfsi_m->r12_how; *offPP = &cfsi_m->r12_off; return;
      case 11: *howPP = &cfsi_m->r11_how; *offPP = &cfsi_m->r11_off; return;
      case 7:  *howPP = &cfsi_m->r7_how;  *offPP = &cfsi_m->r7_off;  return;
      default: break;
   }
   *howPP = NULL; *offPP = NULL;
}


static
Bool setCFAfromCFIR( DiCfSI_m* cfi, XArray* cfsi_exprs,
                     UChar how, Int off )
{
   switch (how) {
      case CFIR_EXPR:
         if (!cfsi_exprs) return False;
         CfiExpr* e = (CfiExpr*)VG_(indexXA)(cfsi_exprs, off);
         if (e->tag != Cex_CfiReg) return False;
         if (e->Cex.CfiReg.reg == Creg_ARM_R7) {
            cfi->cfa_how = CFIC_ARM_R7REL;
            cfi->cfa_off = 0;
            return True;
         }
         ML_(ppCfiExpr)(cfsi_exprs, off);
         vg_assert(0);
      default:
         break;
   }
   VG_(printf)("setCFAfromCFIR: FAIL: how %d off %d\n", (Int)how, (Int)off);
   vg_assert(0);
   return False;
}


#define ARM_EXBUF_START(x) (((x) >> 4) & 0x0f)
#define ARM_EXBUF_COUNT(x) ((x) & 0x0f)
#define ARM_EXBUF_END(x)   (ARM_EXBUF_START(x) + ARM_EXBUF_COUNT(x))


static Bool mentionsCFA ( DiCfSI_m* cfi )
{
#  define MENTIONS_CFA(_how) ((_how) == CFIR_CFAREL || (_how) == CFIR_MEMCFAREL)
   if (MENTIONS_CFA(cfi->ra_how))  return True;
   if (MENTIONS_CFA(cfi->r14_how)) return True;
   if (MENTIONS_CFA(cfi->r13_how)) return True;
   if (MENTIONS_CFA(cfi->r12_how)) return True;
   if (MENTIONS_CFA(cfi->r11_how)) return True;
   if (MENTIONS_CFA(cfi->r7_how))  return True;
   return False;
#  undef MENTIONS_CFA
}


static
Int TranslateCmd(SummState* state, const ExtabData* edata)
{
   
   vg_assert(state);
   switch (state->cfi.cfa_how) {
      case CFIC_ARM_R13REL: case CFIC_ARM_R12REL:
      case CFIC_ARM_R11REL: case CFIC_ARM_R7REL: break;
      default: vg_assert(0);
   }

   if (0) {
      VG_(printf)("  TranslateCmd: ");
      ppExtabData(edata);
      VG_(printf)("\n");
   }

   Int ret = 0;
   switch (edata->cmd) {
      case ARM_EXIDX_CMD_FINISH:
         
         if (state->cfi.ra_how == CFIR_UNKNOWN) {
            if (state->cfi.r14_how == CFIR_UNKNOWN) {
               state->cfi.ra_how = CFIR_EXPR;
               state->cfi.ra_off = gen_CfiExpr_CfiReg_ARM_GPR(state->di, 14);
               vg_assert(state->cfi.ra_off >= 0);
            } else {
               state->cfi.ra_how = state->cfi.r14_how;
               state->cfi.ra_off = state->cfi.r14_off;
            }
         }
         break;
      case ARM_EXIDX_CMD_SUB_FROM_VSP:
         state->vsp_off -= (Int)(edata->data);
         break;
      case ARM_EXIDX_CMD_ADD_TO_VSP:
         state->vsp_off += (Int)(edata->data);
         break;
      case ARM_EXIDX_CMD_REG_POP: {
         UInt i;
         for (i = 0; i < 16; i++) {
            if (edata->data & (1 << i)) {
               
               
               
               
               
               UChar* rX_howP = NULL;
               Int*   rX_offP = NULL;
               maybeFindExprForRegno(&rX_howP, &rX_offP, &state->cfi, i);
               if (rX_howP) {
                  vg_assert(rX_offP);
                  *rX_howP = CFIR_MEMCFAREL;
                  *rX_offP = state->vsp_off;
               } else {
                  
                  vg_assert(!rX_offP);
               }
               state->vsp_off += 4;
            }
         }
         
         if (edata->data & (1 << 13)) {
            
            
            
            
            
            
            goto cant_summarise;
         }
         break;
         }
      case ARM_EXIDX_CMD_REG_TO_SP: {
         if (mentionsCFA(&state->cfi))
            goto cant_summarise;
         vg_assert(edata->data < 16);
         Int reg_no = edata->data;
         
         UChar* rX_howP = NULL;
         Int*   rX_offP = NULL;
         maybeFindExprForRegno(&rX_howP, &rX_offP, &state->cfi, reg_no);
         if (rX_howP) {
            vg_assert(rX_offP);
            if (*rX_howP == CFIR_UNKNOWN) {
               
               Int expr_ix = gen_CfiExpr_CfiReg_ARM_GPR(state->di, reg_no);
               if (expr_ix >= 0) {
                  state->cfi.r13_how = CFIR_EXPR;
                  state->cfi.r13_off = expr_ix;
               } else {
                  goto cant_summarise;
               }
            } else {
               
               state->cfi.r13_how = *rX_howP;
               state->cfi.r13_off = *rX_offP;
            }
            
            Bool ok = setCFAfromCFIR( &state->cfi, state->di->cfsi_exprs,
                                      state->cfi.r13_how, state->cfi.r13_off );
            if (!ok) goto cant_summarise;
            state->vsp_off = 0;
         } else {
            vg_assert(!rX_offP);
         }
         break;
      }
      case ARM_EXIDX_CMD_VFP_POP: {
         UInt i;
         for (i = ARM_EXBUF_START(edata->data);
              i <= ARM_EXBUF_END(edata->data); i++) {
            state->vsp_off += 8;
         }
         if (!(edata->data & ARM_EXIDX_VFP_FSTMD)) {
            state->vsp_off += 4;
         }
         break;
      }
      case ARM_EXIDX_CMD_WREG_POP: {
         UInt i;
         for (i = ARM_EXBUF_START(edata->data);
              i <= ARM_EXBUF_END(edata->data); i++) {
            state->vsp_off += 8;
         }
         break;
      }
      case ARM_EXIDX_CMD_WCGR_POP: {
         UInt i;
         
         for (i = 0; i < 4; i++) {
            if (edata->data & (1 << i)) {
               state->vsp_off += 4;
            }
         }
         break;
      }
      case ARM_EXIDX_CMD_REFUSED:
      case ARM_EXIDX_CMD_RESERVED:
         ret = -1;
         break;
   }
   return ret;

 cant_summarise:
   return -10;
}


static
void AddStackFrame ( SummState* state,
                     DebugInfo* di )
{
   VG_(bzero_inline)(state, sizeof(*state));
   state->vsp_off = 0;
   state->di      = di;
   
   state->cfi.cfa_how = CFIC_ARM_R13REL;
   state->cfi.cfa_off = 0;
   state->cfi.ra_how  = CFIR_UNKNOWN;
   state->cfi.r14_how = CFIR_UNKNOWN;
   state->cfi.r13_how = CFIR_UNKNOWN;
   state->cfi.r12_how = CFIR_UNKNOWN;
   state->cfi.r11_how = CFIR_UNKNOWN;
   state->cfi.r7_how  = CFIR_UNKNOWN;
}

static
void SubmitStackFrame( DebugInfo* di,
                       SummState* state, Addr avma, SizeT len )
{
   
   
   
   
   

   
   switch (state->cfi.cfa_how) {
      case CFIC_ARM_R13REL: case CFIC_ARM_R12REL:
      case CFIC_ARM_R11REL: case CFIC_ARM_R7REL: break;
      default: vg_assert(0);
   }
   state->cfi.r13_how = CFIR_CFAREL;
   state->cfi.r13_off = state->vsp_off;

   
   if (len > 0) {

      
      
      if (state->cfi.r7_how == CFIR_UNKNOWN) {
         state->cfi.r7_how = CFIR_SAME;
         state->cfi.r7_off = 0;
      }
      if (state->cfi.r11_how == CFIR_UNKNOWN) {
         state->cfi.r11_how = CFIR_SAME;
         state->cfi.r11_off = 0;
      }
      if (state->cfi.r12_how == CFIR_UNKNOWN) {
         state->cfi.r12_how = CFIR_SAME;
         state->cfi.r12_off = 0;
      }
      if (state->cfi.r14_how == CFIR_UNKNOWN) {
         state->cfi.r14_how = CFIR_SAME;
         state->cfi.r14_off = 0;
      }

      
      ML_(addDiCfSI)(di, avma, len, &state->cfi);
      if (di->trace_cfi)
         ML_(ppDiCfSI)(di->cfsi_exprs, avma, len, &state->cfi);
   }
}



void ML_(read_exidx) ( DebugInfo* di,
                       UChar*   exidx_img, SizeT exidx_size,
                       UChar*   extab_img, SizeT extab_size,
                       Addr     text_last_svma,
                       PtrdiffT text_bias )
{
   if (di->trace_cfi)
      VG_(printf)("BEGIN ML_(read_exidx) exidx_img=[%p, +%lu) "
                  "extab_img=[%p, +%lu) text_last_svma=%lx text_bias=%lx\n",
                  exidx_img, exidx_size, extab_img, extab_size,
                  text_last_svma, text_bias);
   Bool ok;
   MemoryRange mr_exidx, mr_extab;
   ok =       MemoryRange__init(&mr_exidx, exidx_img, exidx_size);
   ok = ok && MemoryRange__init(&mr_extab, extab_img, extab_size);
   if (!ok) {
      complain(".exidx or .extab image area wraparound");
      return;
   }

   const ExidxEntry* start_img = (const ExidxEntry*)exidx_img;
   const ExidxEntry* end_img   = (const ExidxEntry*)(exidx_img + exidx_size);

   if (VG_(clo_verbosity) > 1)
      VG_(message)(Vg_DebugMsg, "  Reading EXIDX entries: %lu available\n",
                   exidx_size / sizeof(ExidxEntry) );

   
   
   UWord n_attempted = 0, n_successful = 0;

   const ExidxEntry* entry_img;
   for (entry_img = start_img; entry_img < end_img; ++entry_img) {

      n_attempted++;
      
      
      Addr avma = (Addr)Prel31ToAddr(&entry_img->addr);
      if (di->trace_cfi)
         VG_(printf)("XXX1 entry: entry->addr 0x%lx, avma 0x%lx\n",
                     (UWord)entry_img->addr, avma);

      Addr next_avma;
      if (entry_img < end_img - 1) {
         next_avma = (Addr)Prel31ToAddr(&(entry_img+1)->addr);
      } else {
         
         
         
         
         
         
         
         
         
         
         
         
         
         
         Addr text_last_avma = text_last_svma + text_bias;

         Bool plausible;
         Addr maybe_next_avma = text_last_avma + 1;
         if (maybe_next_avma > avma && maybe_next_avma - avma <= 4096) {
            next_avma = maybe_next_avma;
            plausible = True;
         } else {
            next_avma = avma + 1;
            plausible = False;
         }

         if (!plausible && avma != text_last_avma + 1) {
            HChar buf[100];
            VG_(snprintf)(buf, sizeof(buf),
                          "Implausible EXIDX last entry size %u"
                          "; using 1 instead.", (UInt)(text_last_avma - avma));
            buf[sizeof(buf)-1] = 0;
            complain(buf);
         }
      }

      
      
      
      
      if (di->trace_cfi)
         VG_(printf)("XXX1 entry is for AVMA 0x%lx 0x%lx\n",
                     avma, next_avma-1);
      UChar buf[ARM_EXIDX_TABLE_LIMIT];
      SizeT buf_used = 0;
      ExExtractResult res
         = ExtabEntryExtract(&mr_exidx, &mr_extab,
                             entry_img, buf, sizeof(buf), &buf_used);
      if (res != ExSuccess) {
         
         switch (res) {
            case ExInBufOverflow:
               complain("ExtabEntryExtract: .exidx/.extab section overrun");
               break;
            case ExOutBufOverflow:
               complain("ExtabEntryExtract: bytecode buffer overflow");
               break;
            case ExCantUnwind:
               
               
               
               
               n_attempted--;
               if (0)
                  complain("ExtabEntryExtract: function is marked CANT_UNWIND");
               break;
            case ExCantRepresent:
               complain("ExtabEntryExtract: bytecode can't be represented");
               break;
            case ExInvalid:
               complain("ExtabEntryExtract: index table entry is invalid");
               break;
            default: {
               HChar mbuf[100];
               VG_(snprintf)(mbuf, sizeof(mbuf),
                             "ExtabEntryExtract: unknown error: %d", (Int)res);
               buf[sizeof(mbuf)-1] = 0;
               complain(mbuf);
               break;
            }
         }
         continue;
      }

      
      
      
      

      SummState state;
      AddStackFrame( &state, di );
      Int ret = ExtabEntryDecode( &state, buf, buf_used );
      if (ret < 0) {
         
         HChar mbuf[100];
         VG_(snprintf)(mbuf, sizeof(mbuf),
                       "ExtabEntryDecode: failed with error code: %d", ret);
         mbuf[sizeof(mbuf)-1] = 0;
         complain(mbuf);
      } else {
         
         SubmitStackFrame( di, &state, avma, next_avma - avma );
         n_successful++;
      }

   } 

   if (VG_(clo_verbosity) > 1)
      VG_(message)(Vg_DebugMsg,
                   "  Reading EXIDX entries: %lu attempted, %lu successful\n",
                   n_attempted, n_successful);
}

#endif 

