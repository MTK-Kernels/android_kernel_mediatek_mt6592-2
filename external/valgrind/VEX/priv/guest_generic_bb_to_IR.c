

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
#include "libvex_ir.h"
#include "libvex.h"
#include "main_util.h"
#include "main_globals.h"
#include "guest_generic_bb_to_IR.h"


VEX_REGPARM(2)
static UInt genericg_compute_checksum_4al ( HWord first_w32, HWord n_w32s );
VEX_REGPARM(1)
static UInt genericg_compute_checksum_4al_1 ( HWord first_w32 );
VEX_REGPARM(1)
static UInt genericg_compute_checksum_4al_2 ( HWord first_w32 );
VEX_REGPARM(1)
static UInt genericg_compute_checksum_4al_3 ( HWord first_w32 );
VEX_REGPARM(1)
static UInt genericg_compute_checksum_4al_4 ( HWord first_w32 );
VEX_REGPARM(1)
static UInt genericg_compute_checksum_4al_5 ( HWord first_w32 );
VEX_REGPARM(1)
static UInt genericg_compute_checksum_4al_6 ( HWord first_w32 );
VEX_REGPARM(1)
static UInt genericg_compute_checksum_4al_7 ( HWord first_w32 );
VEX_REGPARM(1)
static UInt genericg_compute_checksum_4al_8 ( HWord first_w32 );
VEX_REGPARM(1)
static UInt genericg_compute_checksum_4al_9 ( HWord first_w32 );
VEX_REGPARM(1)
static UInt genericg_compute_checksum_4al_10 ( HWord first_w32 );
VEX_REGPARM(1)
static UInt genericg_compute_checksum_4al_11 ( HWord first_w32 );
VEX_REGPARM(1)
static UInt genericg_compute_checksum_4al_12 ( HWord first_w32 );

VEX_REGPARM(2)
static ULong genericg_compute_checksum_8al ( HWord first_w64, HWord n_w64s );
VEX_REGPARM(1)
static ULong genericg_compute_checksum_8al_1 ( HWord first_w64 );
VEX_REGPARM(1)
static ULong genericg_compute_checksum_8al_2 ( HWord first_w64 );
VEX_REGPARM(1)
static ULong genericg_compute_checksum_8al_3 ( HWord first_w64 );
VEX_REGPARM(1)
static ULong genericg_compute_checksum_8al_4 ( HWord first_w64 );
VEX_REGPARM(1)
static ULong genericg_compute_checksum_8al_5 ( HWord first_w64 );
VEX_REGPARM(1)
static ULong genericg_compute_checksum_8al_6 ( HWord first_w64 );
VEX_REGPARM(1)
static ULong genericg_compute_checksum_8al_7 ( HWord first_w64 );
VEX_REGPARM(1)
static ULong genericg_compute_checksum_8al_8 ( HWord first_w64 );
VEX_REGPARM(1)
static ULong genericg_compute_checksum_8al_9 ( HWord first_w64 );
VEX_REGPARM(1)
static ULong genericg_compute_checksum_8al_10 ( HWord first_w64 );
VEX_REGPARM(1)
static ULong genericg_compute_checksum_8al_11 ( HWord first_w64 );
VEX_REGPARM(1)
static ULong genericg_compute_checksum_8al_12 ( HWord first_w64 );

static Bool const_False ( void* callback_opaque, Addr a ) { 
   return False; 
}

/* Disassemble a complete basic block, starting at guest_IP_start, 
   returning a new IRSB.  The disassembler may chase across basic
   block boundaries if it wishes and if chase_into_ok allows it.
   The precise guest address ranges from which code has been taken
   are written into vge.  guest_IP_bbstart is taken to be the IP in
   the guest's address space corresponding to the instruction at
   &guest_code[0].  

   dis_instr_fn is the arch-specific fn to disassemble on function; it
   is this that does the real work.

   needs_self_check is a callback used to ask the caller which of the
   extents, if any, a self check is required for.  The returned value
   is a bitmask with a 1 in position i indicating that the i'th extent
   needs a check.  Since there can be at most 3 extents, the returned
   values must be between 0 and 7.

   The number of extents which did get a self check (0 to 3) is put in
   n_sc_extents.  The caller already knows this because it told us
   which extents to add checks for, via the needs_self_check callback,
   but we ship the number back out here for the caller's convenience.

   preamble_function is a callback which allows the caller to add
   its own IR preamble (following the self-check, if any).  May be
   NULL.  If non-NULL, the IRSB under construction is handed to 
   this function, which presumably adds IR statements to it.  The
   callback may optionally complete the block and direct bb_to_IR
   not to disassemble any instructions into it; this is indicated
   by the callback returning True.

   offB_CMADDR and offB_CMLEN are the offsets of guest_CMADDR and
   guest_CMLEN.  Since this routine has to work for any guest state,
   without knowing what it is, those offsets have to passed in.

   callback_opaque is a caller-supplied pointer to data which the
   callbacks may want to see.  Vex has no idea what it is.
   (In fact it's a VgInstrumentClosure.)
*/


IRSB* bb_to_IR ( 
         VexGuestExtents* vge,
         UInt*            n_sc_extents,
         UInt*            n_guest_instrs, 
         VexRegisterUpdates* pxControl,
          void*            callback_opaque,
          DisOneInstrFn    dis_instr_fn,
          const UChar*     guest_code,
          Addr             guest_IP_bbstart,
          Bool             (*chase_into_ok)(void*,Addr),
          VexEndness       host_endness,
          Bool             sigill_diag,
          VexArch          arch_guest,
          const VexArchInfo* archinfo_guest,
          const VexAbiInfo*  abiinfo_both,
          IRType           guest_word_type,
          UInt             (*needs_self_check)
                                    (void*, VexRegisterUpdates*,
                                            const VexGuestExtents*),
          Bool             (*preamble_function)(void*,IRSB*),
          Int              offB_GUEST_CMSTART,
          Int              offB_GUEST_CMLEN,
          Int              offB_GUEST_IP,
          Int              szB_GUEST_IP
      )
{
   Long       delta;
   Int        i, n_instrs, first_stmt_idx;
   Bool       resteerOK, debug_print;
   DisResult  dres;
   IRStmt*    imark;
   IRStmt*    nop;
   static Int n_resteers = 0;
   Int        d_resteers = 0;
   Int        selfcheck_idx = 0;
   IRSB*      irsb;
   Addr       guest_IP_curr_instr;
   IRConst*   guest_IP_bbstart_IRConst = NULL;
   Int        n_cond_resteers_allowed = 2;

   Bool (*resteerOKfn)(void*,Addr) = NULL;

   debug_print = toBool(vex_traceflags & VEX_TRACE_FE);

   
   vassert(sizeof(HWord) == sizeof(void*));
   vassert(vex_control.guest_max_insns >= 1);
   vassert(vex_control.guest_max_insns <= 100);
   vassert(vex_control.guest_chase_thresh >= 0);
   vassert(vex_control.guest_chase_thresh < vex_control.guest_max_insns);
   vassert(guest_word_type == Ity_I32 || guest_word_type == Ity_I64);

   if (guest_word_type == Ity_I32) {
      vassert(szB_GUEST_IP == 4);
      vassert((offB_GUEST_IP % 4) == 0);
   } else {
      vassert(szB_GUEST_IP == 8);
      vassert((offB_GUEST_IP % 8) == 0);
   }

   
   vge->n_used  = 1;
   vge->base[0] = guest_IP_bbstart;
   vge->len[0]  = 0;
   *n_sc_extents = 0;

   
   irsb = emptyIRSB();

   delta    = 0;
   n_instrs = 0;
   *n_guest_instrs = 0;

   guest_IP_bbstart_IRConst
      = guest_word_type==Ity_I32 
           ? IRConst_U32(toUInt(guest_IP_bbstart))
           : IRConst_U64(guest_IP_bbstart);

   nop = IRStmt_NoOp();
   selfcheck_idx = irsb->stmts_used;
   for (i = 0; i < 3 * 5; i++)
      addStmtToIRSB( irsb, nop );

   if (preamble_function) {
      Bool stopNow = preamble_function( callback_opaque, irsb );
      if (stopNow) {
         return irsb;
      }
   }

   
   while (True) {
      vassert(n_instrs < vex_control.guest_max_insns);

      resteerOK 
         = toBool(
              n_instrs < vex_control.guest_chase_thresh
              && vge->n_used < 3
           );

      resteerOKfn
         = resteerOK ? chase_into_ok : const_False;

      vassert(n_cond_resteers_allowed >= 0 && n_cond_resteers_allowed <= 2);

      guest_IP_curr_instr = guest_IP_bbstart + delta;

      first_stmt_idx = irsb->stmts_used;

      if (arch_guest == VexArchARM && (guest_IP_curr_instr & 1)) {
         
         addStmtToIRSB( irsb,
                        IRStmt_IMark(guest_IP_curr_instr & ~(Addr)1,
                                     0, 
                                     1  
                        )
         );
      } else {
         
         addStmtToIRSB( irsb,
                        IRStmt_IMark(guest_IP_curr_instr,
                                     0, 
                                     0  
                        )
         );
      }

      if (debug_print && n_instrs > 0)
         vex_printf("\n");

      
      vassert(irsb->next == NULL);
      dres = dis_instr_fn ( irsb,
                            resteerOKfn,
                            toBool(n_cond_resteers_allowed > 0),
                            callback_opaque,
                            guest_code,
                            delta,
                            guest_IP_curr_instr,
                            arch_guest,
                            archinfo_guest,
                            abiinfo_both,
                            host_endness,
                            sigill_diag );

      
      vassert(dres.whatNext == Dis_StopHere
              || dres.whatNext == Dis_Continue
              || dres.whatNext == Dis_ResteerU
              || dres.whatNext == Dis_ResteerC);
      
      vassert(dres.len >= 0 && dres.len <= 24);
      
      if (dres.whatNext != Dis_ResteerU && dres.whatNext != Dis_ResteerC)
         vassert(dres.continueAt == 0);
      if (n_cond_resteers_allowed == 0)
         vassert(dres.whatNext != Dis_ResteerC);

      
      vassert(first_stmt_idx >= 0 && first_stmt_idx < irsb->stmts_used);
      imark = irsb->stmts[first_stmt_idx];
      vassert(imark);
      vassert(imark->tag == Ist_IMark);
      vassert(imark->Ist.IMark.len == 0);
      imark->Ist.IMark.len = dres.len;

      
      if (vex_traceflags & VEX_TRACE_FE) {
         for (i = first_stmt_idx; i < irsb->stmts_used; i++) {
            vex_printf("              ");
            ppIRStmt(irsb->stmts[i]);
            vex_printf("\n");
         }
      }

      vassert(irsb->next == NULL);
      vassert(irsb->jumpkind == Ijk_Boring);
      vassert(irsb->offsIP == 0);

      vassert(first_stmt_idx < irsb->stmts_used);
      
      { IRStmt* st = irsb->stmts[irsb->stmts_used-1];
        vassert(st);
        vassert(st->tag == Ist_Put);
        vassert(st->Ist.Put.offset == offB_GUEST_IP);
      }

      
      vassert(vge->len[vge->n_used-1] < 5000);
      vge->len[vge->n_used-1] 
         = toUShort(toUInt( vge->len[vge->n_used-1] + dres.len ));
      n_instrs++;

      
      delta += (Long)dres.len;

      switch (dres.whatNext) {
         case Dis_Continue:
            vassert(dres.continueAt == 0);
            vassert(dres.jk_StopHere == Ijk_INVALID);
            if (n_instrs < vex_control.guest_max_insns) {
               
            } else {
               irsb->next = IRExpr_Get(offB_GUEST_IP, guest_word_type);
               
               irsb->offsIP = offB_GUEST_IP;
               goto done;
            }
            break;
         case Dis_StopHere:
            vassert(dres.continueAt == 0);
            vassert(dres.jk_StopHere != Ijk_INVALID);
            
            irsb->next = IRExpr_Get(offB_GUEST_IP, guest_word_type);
            irsb->jumpkind = dres.jk_StopHere;
            irsb->offsIP = offB_GUEST_IP;
            goto done;

         case Dis_ResteerU:
         case Dis_ResteerC:
            
            vassert(resteerOK);
            if (dres.whatNext == Dis_ResteerC) {
               vassert(n_cond_resteers_allowed > 0);
               n_cond_resteers_allowed--;
            }
            
            vassert(resteerOKfn(callback_opaque,dres.continueAt));
            delta = dres.continueAt - guest_IP_bbstart;
            
            vge->n_used++;
            vassert(vge->n_used <= 3);
            vge->base[vge->n_used-1] = dres.continueAt;
            vge->len[vge->n_used-1] = 0;
            n_resteers++;
            d_resteers++;
            if (0 && (n_resteers & 0xFF) == 0)
            vex_printf("resteer[%d,%d] to 0x%lx (delta = %lld)\n",
                       n_resteers, d_resteers,
                       dres.continueAt, delta);
            break;
         default:
            vpanic("bb_to_IR");
      }
   }
   
   vassert(0);

  done:
   {
      Addr     base2check;
      UInt     len2check;
      HWord    expectedhW;
      IRTemp   tistart_tmp, tilen_tmp;
      HWord    VEX_REGPARM(2) (*fn_generic)(HWord, HWord);
      HWord    VEX_REGPARM(1) (*fn_spec)(HWord);
      const HChar* nm_generic;
      const HChar* nm_spec;
      HWord    fn_generic_entry = 0;
      HWord    fn_spec_entry = 0;
      UInt     host_word_szB = sizeof(HWord);
      IRType   host_word_type = Ity_INVALID;

      UInt extents_needing_check
         = needs_self_check(callback_opaque, pxControl, vge);

      if (host_word_szB == 4) host_word_type = Ity_I32;
      if (host_word_szB == 8) host_word_type = Ity_I64;
      vassert(host_word_type != Ity_INVALID);

      vassert(vge->n_used >= 1 && vge->n_used <= 3);

      vassert((extents_needing_check >> vge->n_used) == 0);

      for (i = 0; i < vge->n_used; i++) {

         
         if ((extents_needing_check & (1 << i)) == 0)
            continue;

         
         (*n_sc_extents)++;

         
         base2check = vge->base[i];
         len2check  = vge->len[i];

         
         vassert(len2check >= 0 && len2check < 1000);

         
         if (len2check == 0)
            continue;

         HWord first_hW = ((HWord)base2check)
                          & ~(HWord)(host_word_szB-1);
         HWord last_hW  = (((HWord)base2check) + len2check - 1)
                          & ~(HWord)(host_word_szB-1);
         vassert(first_hW <= last_hW);
         HWord hW_diff = last_hW - first_hW;
         vassert(0 == (hW_diff & (host_word_szB-1)));
         HWord hWs_to_check = (hW_diff + host_word_szB) / host_word_szB;
         vassert(hWs_to_check > 0
                 && hWs_to_check < 1004 / host_word_szB);

         

         if (host_word_szB == 8) {
            fn_generic =  (VEX_REGPARM(2) HWord(*)(HWord, HWord))
                          genericg_compute_checksum_8al;
            nm_generic = "genericg_compute_checksum_8al";
         } else {
            fn_generic =  (VEX_REGPARM(2) HWord(*)(HWord, HWord))
                          genericg_compute_checksum_4al;
            nm_generic = "genericg_compute_checksum_4al";
         }

         fn_spec = NULL;
         nm_spec = NULL;

         if (host_word_szB == 8) {
            const HChar* nm = NULL;
            ULong  VEX_REGPARM(1) (*fn)(HWord)  = NULL;
            switch (hWs_to_check) {
               case 1:  fn =  genericg_compute_checksum_8al_1;
                        nm = "genericg_compute_checksum_8al_1"; break;
               case 2:  fn =  genericg_compute_checksum_8al_2;
                        nm = "genericg_compute_checksum_8al_2"; break;
               case 3:  fn =  genericg_compute_checksum_8al_3;
                        nm = "genericg_compute_checksum_8al_3"; break;
               case 4:  fn =  genericg_compute_checksum_8al_4;
                        nm = "genericg_compute_checksum_8al_4"; break;
               case 5:  fn =  genericg_compute_checksum_8al_5;
                        nm = "genericg_compute_checksum_8al_5"; break;
               case 6:  fn =  genericg_compute_checksum_8al_6;
                        nm = "genericg_compute_checksum_8al_6"; break;
               case 7:  fn =  genericg_compute_checksum_8al_7;
                        nm = "genericg_compute_checksum_8al_7"; break;
               case 8:  fn =  genericg_compute_checksum_8al_8;
                        nm = "genericg_compute_checksum_8al_8"; break;
               case 9:  fn =  genericg_compute_checksum_8al_9;
                        nm = "genericg_compute_checksum_8al_9"; break;
               case 10: fn =  genericg_compute_checksum_8al_10;
                        nm = "genericg_compute_checksum_8al_10"; break;
               case 11: fn =  genericg_compute_checksum_8al_11;
                        nm = "genericg_compute_checksum_8al_11"; break;
               case 12: fn =  genericg_compute_checksum_8al_12;
                        nm = "genericg_compute_checksum_8al_12"; break;
               default: break;
            }
            fn_spec = (VEX_REGPARM(1) HWord(*)(HWord)) fn;
            nm_spec = nm;
         } else {
            const HChar* nm = NULL;
            UInt   VEX_REGPARM(1) (*fn)(HWord) = NULL;
            switch (hWs_to_check) {
               case 1:  fn =  genericg_compute_checksum_4al_1;
                        nm = "genericg_compute_checksum_4al_1"; break;
               case 2:  fn =  genericg_compute_checksum_4al_2;
                        nm = "genericg_compute_checksum_4al_2"; break;
               case 3:  fn =  genericg_compute_checksum_4al_3;
                        nm = "genericg_compute_checksum_4al_3"; break;
               case 4:  fn =  genericg_compute_checksum_4al_4;
                        nm = "genericg_compute_checksum_4al_4"; break;
               case 5:  fn =  genericg_compute_checksum_4al_5;
                        nm = "genericg_compute_checksum_4al_5"; break;
               case 6:  fn =  genericg_compute_checksum_4al_6;
                        nm = "genericg_compute_checksum_4al_6"; break;
               case 7:  fn =  genericg_compute_checksum_4al_7;
                        nm = "genericg_compute_checksum_4al_7"; break;
               case 8:  fn =  genericg_compute_checksum_4al_8;
                        nm = "genericg_compute_checksum_4al_8"; break;
               case 9:  fn =  genericg_compute_checksum_4al_9;
                        nm = "genericg_compute_checksum_4al_9"; break;
               case 10: fn =  genericg_compute_checksum_4al_10;
                        nm = "genericg_compute_checksum_4al_10"; break;
               case 11: fn =  genericg_compute_checksum_4al_11;
                        nm = "genericg_compute_checksum_4al_11"; break;
               case 12: fn =  genericg_compute_checksum_4al_12;
                        nm = "genericg_compute_checksum_4al_12"; break;
               default: break;
            }
            fn_spec = (VEX_REGPARM(1) HWord(*)(HWord))fn;
            nm_spec = nm;
         }

         expectedhW = fn_generic( first_hW, hWs_to_check );
         if (fn_spec) {
            vassert(nm_spec);
            vassert(expectedhW == fn_spec( first_hW ));
         } else {
            vassert(!nm_spec);
         }


         tistart_tmp = newIRTemp(irsb->tyenv, guest_word_type);
         tilen_tmp   = newIRTemp(irsb->tyenv, guest_word_type);

         IRConst* base2check_IRConst
            = guest_word_type==Ity_I32 ? IRConst_U32(toUInt(base2check))
                                       : IRConst_U64(base2check);
         IRConst* len2check_IRConst
            = guest_word_type==Ity_I32 ? IRConst_U32(len2check)
                                       : IRConst_U64(len2check);

         irsb->stmts[selfcheck_idx + i * 5 + 0]
            = IRStmt_WrTmp(tistart_tmp, IRExpr_Const(base2check_IRConst) );

         irsb->stmts[selfcheck_idx + i * 5 + 1]
            = IRStmt_WrTmp(tilen_tmp, IRExpr_Const(len2check_IRConst) );

         irsb->stmts[selfcheck_idx + i * 5 + 2]
            = IRStmt_Put( offB_GUEST_CMSTART, IRExpr_RdTmp(tistart_tmp) );

         irsb->stmts[selfcheck_idx + i * 5 + 3]
            = IRStmt_Put( offB_GUEST_CMLEN, IRExpr_RdTmp(tilen_tmp) );

         
         if (abiinfo_both->host_ppc_calls_use_fndescrs) {
            HWord* descr = (HWord*)fn_generic;
            fn_generic_entry = descr[0];
            if (fn_spec) {
               descr = (HWord*)fn_spec;
               fn_spec_entry = descr[0];
            } else {
               fn_spec_entry = (HWord)NULL;
            }
         } else {
            fn_generic_entry = (HWord)fn_generic;
            if (fn_spec) {
               fn_spec_entry = (HWord)fn_spec;
            } else {
               fn_spec_entry = (HWord)NULL;
            }
         }

         IRExpr* callexpr = NULL;
         if (fn_spec) {
            callexpr = mkIRExprCCall( 
                          host_word_type, 1, 
                          nm_spec, (void*)fn_spec_entry,
                          mkIRExprVec_1(
                             mkIRExpr_HWord( (HWord)first_hW )
                          )
                       );
         } else {
            callexpr = mkIRExprCCall( 
                          host_word_type, 2, 
                          nm_generic, (void*)fn_generic_entry,
                          mkIRExprVec_2(
                             mkIRExpr_HWord( (HWord)first_hW ),
                             mkIRExpr_HWord( (HWord)hWs_to_check )
                          )
                       );
         }

         irsb->stmts[selfcheck_idx + i * 5 + 4]
            = IRStmt_Exit( 
                 IRExpr_Binop( 
                    host_word_type==Ity_I64 ? Iop_CmpNE64 : Iop_CmpNE32,
                    callexpr,
                       host_word_type==Ity_I64
                          ? IRExpr_Const(IRConst_U64(expectedhW))
                          : IRExpr_Const(IRConst_U32(expectedhW))
                 ),
                 Ijk_InvalICache,
                 guest_IP_bbstart_IRConst,
                 offB_GUEST_IP
              );
      } 
   }

   vassert(irsb->next != NULL);
   if (debug_print) {
      vex_printf("              ");
      vex_printf( "PUT(%d) = ", irsb->offsIP);
      ppIRExpr( irsb->next );
      vex_printf( "; exit-");
      ppIRJumpKind(irsb->jumpkind);
      vex_printf( "\n");
      vex_printf( "\n");
   }

   *n_guest_instrs = n_instrs;
   return irsb;
}






static inline UInt ROL32 ( UInt w, Int n ) {
   w = (w << n) | (w >> (32-n));
   return w;
}

VEX_REGPARM(2)
static UInt genericg_compute_checksum_4al ( HWord first_w32, HWord n_w32s )
{
   UInt  sum1 = 0, sum2 = 0;
   UInt* p = (UInt*)first_w32;
   
   while (n_w32s >= 4) {
      UInt  w;
      w = p[0];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
      w = p[1];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
      w = p[2];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
      w = p[3];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
      p += 4;
      n_w32s -= 4;
      sum1 ^= sum2;
   }
   while (n_w32s >= 1) {
      UInt  w;
      w = p[0];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
      p += 1;
      n_w32s -= 1;
      sum1 ^= sum2;
   }
   return sum1 + sum2;
}


VEX_REGPARM(1)
static UInt genericg_compute_checksum_4al_1 ( HWord first_w32 )
{
   UInt  sum1 = 0, sum2 = 0;
   UInt* p = (UInt*)first_w32;
   UInt  w;
   w = p[0];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   return sum1 + sum2;
}

VEX_REGPARM(1)
static UInt genericg_compute_checksum_4al_2 ( HWord first_w32 )
{
   UInt  sum1 = 0, sum2 = 0;
   UInt* p = (UInt*)first_w32;
   UInt  w;
   w = p[0];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   w = p[1];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   return sum1 + sum2;
}

VEX_REGPARM(1)
static UInt genericg_compute_checksum_4al_3 ( HWord first_w32 )
{
   UInt  sum1 = 0, sum2 = 0;
   UInt* p = (UInt*)first_w32;
   UInt  w;
   w = p[0];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   w = p[1];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   w = p[2];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   return sum1 + sum2;
}

VEX_REGPARM(1)
static UInt genericg_compute_checksum_4al_4 ( HWord first_w32 )
{
   UInt  sum1 = 0, sum2 = 0;
   UInt* p = (UInt*)first_w32;
   UInt  w;
   w = p[0];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[1];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[2];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[3];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   return sum1 + sum2;
}

VEX_REGPARM(1)
static UInt genericg_compute_checksum_4al_5 ( HWord first_w32 )
{
   UInt  sum1 = 0, sum2 = 0;
   UInt* p = (UInt*)first_w32;
   UInt  w;
   w = p[0];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[1];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[2];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[3];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   w = p[4];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   return sum1 + sum2;
}

VEX_REGPARM(1)
static UInt genericg_compute_checksum_4al_6 ( HWord first_w32 )
{
   UInt  sum1 = 0, sum2 = 0;
   UInt* p = (UInt*)first_w32;
   UInt  w;
   w = p[0];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[1];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[2];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[3];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   w = p[4];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   w = p[5];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   return sum1 + sum2;
}

VEX_REGPARM(1)
static UInt genericg_compute_checksum_4al_7 ( HWord first_w32 )
{
   UInt  sum1 = 0, sum2 = 0;
   UInt* p = (UInt*)first_w32;
   UInt  w;
   w = p[0];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[1];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[2];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[3];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   w = p[4];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   w = p[5];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   w = p[6];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   return sum1 + sum2;
}

VEX_REGPARM(1)
static UInt genericg_compute_checksum_4al_8 ( HWord first_w32 )
{
   UInt  sum1 = 0, sum2 = 0;
   UInt* p = (UInt*)first_w32;
   UInt  w;
   w = p[0];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[1];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[2];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[3];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   w = p[4];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[5];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[6];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[7];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   return sum1 + sum2;
}

VEX_REGPARM(1)
static UInt genericg_compute_checksum_4al_9 ( HWord first_w32 )
{
   UInt  sum1 = 0, sum2 = 0;
   UInt* p = (UInt*)first_w32;
   UInt  w;
   w = p[0];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[1];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[2];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[3];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   w = p[4];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[5];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[6];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[7];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   w = p[8];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   return sum1 + sum2;
}

VEX_REGPARM(1)
static UInt genericg_compute_checksum_4al_10 ( HWord first_w32 )
{
   UInt  sum1 = 0, sum2 = 0;
   UInt* p = (UInt*)first_w32;
   UInt  w;
   w = p[0];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[1];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[2];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[3];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   w = p[4];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[5];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[6];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[7];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   w = p[8];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   w = p[9];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   return sum1 + sum2;
}

VEX_REGPARM(1)
static UInt genericg_compute_checksum_4al_11 ( HWord first_w32 )
{
   UInt  sum1 = 0, sum2 = 0;
   UInt* p = (UInt*)first_w32;
   UInt  w;
   w = p[0];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[1];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[2];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[3];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   w = p[4];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[5];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[6];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[7];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   w = p[8];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   w = p[9];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   w = p[10]; sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   return sum1 + sum2;
}

VEX_REGPARM(1)
static UInt genericg_compute_checksum_4al_12 ( HWord first_w32 )
{
   UInt  sum1 = 0, sum2 = 0;
   UInt* p = (UInt*)first_w32;
   UInt  w;
   w = p[0];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[1];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[2];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[3];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   w = p[4];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[5];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[6];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[7];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   w = p[8];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[9];  sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[10]; sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   w = p[11]; sum1 = ROL32(sum1 ^ w, 31);  sum2 += w;
   sum1 ^= sum2;
   return sum1 + sum2;
}



static inline ULong ROL64 ( ULong w, Int n ) {
   w = (w << n) | (w >> (64-n));
   return w;
}

VEX_REGPARM(2)
static ULong genericg_compute_checksum_8al ( HWord first_w64, HWord n_w64s )
{
   ULong  sum1 = 0, sum2 = 0;
   ULong* p = (ULong*)first_w64;
   
   while (n_w64s >= 4) {
      ULong  w;
      w = p[0];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
      w = p[1];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
      w = p[2];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
      w = p[3];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
      p += 4;
      n_w64s -= 4;
      sum1 ^= sum2;
   }
   while (n_w64s >= 1) {
      ULong  w;
      w = p[0];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
      p += 1;
      n_w64s -= 1;
      sum1 ^= sum2;
   }
   return sum1 + sum2;
}


VEX_REGPARM(1)
static ULong genericg_compute_checksum_8al_1 ( HWord first_w64 )
{
   ULong  sum1 = 0, sum2 = 0;
   ULong* p = (ULong*)first_w64;
   ULong  w;
   w = p[0];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   return sum1 + sum2;
}

VEX_REGPARM(1)
static ULong genericg_compute_checksum_8al_2 ( HWord first_w64 )
{
   ULong  sum1 = 0, sum2 = 0;
   ULong* p = (ULong*)first_w64;
   ULong  w;
   w = p[0];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   w = p[1];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   return sum1 + sum2;
}

VEX_REGPARM(1)
static ULong genericg_compute_checksum_8al_3 ( HWord first_w64 )
{
   ULong  sum1 = 0, sum2 = 0;
   ULong* p = (ULong*)first_w64;
   ULong  w;
   w = p[0];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   w = p[1];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   w = p[2];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   return sum1 + sum2;
}

VEX_REGPARM(1)
static ULong genericg_compute_checksum_8al_4 ( HWord first_w64 )
{
   ULong  sum1 = 0, sum2 = 0;
   ULong* p = (ULong*)first_w64;
   ULong  w;
   w = p[0];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[1];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[2];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[3];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   return sum1 + sum2;
}

VEX_REGPARM(1)
static ULong genericg_compute_checksum_8al_5 ( HWord first_w64 )
{
   ULong  sum1 = 0, sum2 = 0;
   ULong* p = (ULong*)first_w64;
   ULong  w;
   w = p[0];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[1];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[2];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[3];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   w = p[4];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   return sum1 + sum2;
}

VEX_REGPARM(1)
static ULong genericg_compute_checksum_8al_6 ( HWord first_w64 )
{
   ULong  sum1 = 0, sum2 = 0;
   ULong* p = (ULong*)first_w64;
   ULong  w;
   w = p[0];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[1];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[2];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[3];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   w = p[4];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   w = p[5];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   return sum1 + sum2;
}

VEX_REGPARM(1)
static ULong genericg_compute_checksum_8al_7 ( HWord first_w64 )
{
   ULong  sum1 = 0, sum2 = 0;
   ULong* p = (ULong*)first_w64;
   ULong  w;
   w = p[0];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[1];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[2];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[3];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   w = p[4];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   w = p[5];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   w = p[6];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   return sum1 + sum2;
}

VEX_REGPARM(1)
static ULong genericg_compute_checksum_8al_8 ( HWord first_w64 )
{
   ULong  sum1 = 0, sum2 = 0;
   ULong* p = (ULong*)first_w64;
   ULong  w;
   w = p[0];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[1];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[2];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[3];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   w = p[4];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[5];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[6];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[7];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   return sum1 + sum2;
}

VEX_REGPARM(1)
static ULong genericg_compute_checksum_8al_9 ( HWord first_w64 )
{
   ULong  sum1 = 0, sum2 = 0;
   ULong* p = (ULong*)first_w64;
   ULong  w;
   w = p[0];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[1];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[2];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[3];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   w = p[4];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[5];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[6];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[7];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   w = p[8];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   return sum1 + sum2;
}

VEX_REGPARM(1)
static ULong genericg_compute_checksum_8al_10 ( HWord first_w64 )
{
   ULong  sum1 = 0, sum2 = 0;
   ULong* p = (ULong*)first_w64;
   ULong  w;
   w = p[0];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[1];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[2];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[3];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   w = p[4];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[5];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[6];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[7];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   w = p[8];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   w = p[9];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   return sum1 + sum2;
}

VEX_REGPARM(1)
static ULong genericg_compute_checksum_8al_11 ( HWord first_w64 )
{
   ULong  sum1 = 0, sum2 = 0;
   ULong* p = (ULong*)first_w64;
   ULong  w;
   w = p[0];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[1];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[2];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[3];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   w = p[4];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[5];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[6];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[7];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   w = p[8];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   w = p[9];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   w = p[10]; sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   return sum1 + sum2;
}

VEX_REGPARM(1)
static ULong genericg_compute_checksum_8al_12 ( HWord first_w64 )
{
   ULong  sum1 = 0, sum2 = 0;
   ULong* p = (ULong*)first_w64;
   ULong  w;
   w = p[0];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[1];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[2];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[3];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   w = p[4];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[5];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[6];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[7];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   w = p[8];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[9];  sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[10]; sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   w = p[11]; sum1 = ROL64(sum1 ^ w, 63);  sum2 += w;
   sum1 ^= sum2;
   return sum1 + sum2;
}

