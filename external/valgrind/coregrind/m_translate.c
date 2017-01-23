

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2000-2013 Julian Seward 
      jseward@acm.org

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

#include "pub_core_basics.h"
#include "pub_core_vki.h"
#include "pub_core_aspacemgr.h"

#include "pub_core_machine.h"    
                                 
                                 
#include "pub_core_libcbase.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcprint.h"
#include "pub_core_options.h"

#include "pub_core_debuginfo.h"  
#include "pub_core_redir.h"      

#include "pub_core_signals.h"    
#include "pub_core_stacks.h"     
#include "pub_core_tooliface.h"  

#include "pub_core_translate.h"
#include "pub_core_transtab.h"
#include "pub_core_dispatch.h" 
                               

#include "pub_core_threadstate.h"  
#include "pub_core_trampoline.h"   

#include "pub_core_execontext.h"  

#include "pub_core_gdbserver.h"   

#include "libvex_emnote.h"        


static ULong n_SP_updates_fast            = 0;
static ULong n_SP_updates_generic_known   = 0;
static ULong n_SP_updates_generic_unknown = 0;

static ULong n_PX_VexRegUpdSpAtMemAccess         = 0;
static ULong n_PX_VexRegUpdUnwindregsAtMemAccess = 0;
static ULong n_PX_VexRegUpdAllregsAtMemAccess    = 0;
static ULong n_PX_VexRegUpdAllregsAtEachInsn     = 0;

void VG_(print_translation_stats) ( void )
{
   UInt n_SP_updates = n_SP_updates_fast + n_SP_updates_generic_known
                                         + n_SP_updates_generic_unknown;
   if (n_SP_updates == 0) {
      VG_(message)(Vg_DebugMsg, "translate: no SP updates identified\n");
   } else {
      VG_(message)(Vg_DebugMsg,
         "translate:            fast SP updates identified: %'llu (%3.1f%%)\n",
         n_SP_updates_fast, n_SP_updates_fast * 100.0 / n_SP_updates );

      VG_(message)(Vg_DebugMsg,
         "translate:   generic_known SP updates identified: %'llu (%3.1f%%)\n",
         n_SP_updates_generic_known,
         n_SP_updates_generic_known * 100.0 / n_SP_updates );

      VG_(message)(Vg_DebugMsg,
         "translate: generic_unknown SP updates identified: %'llu (%3.1f%%)\n",
         n_SP_updates_generic_unknown,
         n_SP_updates_generic_unknown * 100.0 / n_SP_updates );
   }

   VG_(message)(Vg_DebugMsg,
                "translate: PX: SPonly %'llu,  UnwRegs %'llu,  AllRegs %'llu,  AllRegsAllInsns %'llu\n", n_PX_VexRegUpdSpAtMemAccess, n_PX_VexRegUpdUnwindregsAtMemAccess, n_PX_VexRegUpdAllregsAtMemAccess, n_PX_VexRegUpdAllregsAtEachInsn);
}


static Bool need_to_handle_SP_assignment(void)
{
   return ( VG_(tdict).track_new_mem_stack_4   ||
            VG_(tdict).track_die_mem_stack_4   ||
            VG_(tdict).track_new_mem_stack_8   ||
            VG_(tdict).track_die_mem_stack_8   ||
            VG_(tdict).track_new_mem_stack_12  ||
            VG_(tdict).track_die_mem_stack_12  ||
            VG_(tdict).track_new_mem_stack_16  ||
            VG_(tdict).track_die_mem_stack_16  ||
            VG_(tdict).track_new_mem_stack_32  ||
            VG_(tdict).track_die_mem_stack_32  ||
            VG_(tdict).track_new_mem_stack_112 ||
            VG_(tdict).track_die_mem_stack_112 ||
            VG_(tdict).track_new_mem_stack_128 ||
            VG_(tdict).track_die_mem_stack_128 ||
            VG_(tdict).track_new_mem_stack_144 ||
            VG_(tdict).track_die_mem_stack_144 ||
            VG_(tdict).track_new_mem_stack_160 ||
            VG_(tdict).track_die_mem_stack_160 ||
            VG_(tdict).track_new_mem_stack     ||
            VG_(tdict).track_die_mem_stack     );
}


typedef 
   struct {
      IRTemp temp;
      Long   delta;
   }
   SP_Alias;

#define N_ALIASES    32
static SP_Alias SP_aliases[N_ALIASES];
static Int      next_SP_alias_slot = 0;

static void clear_SP_aliases(void)
{
   Int i;
   for (i = 0; i < N_ALIASES; i++) {
      SP_aliases[i].temp  = IRTemp_INVALID;
      SP_aliases[i].delta = 0;
   }
   next_SP_alias_slot = 0;
}

static void add_SP_alias(IRTemp temp, Long delta)
{
   vg_assert(temp != IRTemp_INVALID);
   SP_aliases[ next_SP_alias_slot ].temp  = temp;
   SP_aliases[ next_SP_alias_slot ].delta = delta;
   next_SP_alias_slot++;
   if (N_ALIASES == next_SP_alias_slot) next_SP_alias_slot = 0;
}

static Bool get_SP_delta(IRTemp temp, Long* delta)
{
   Int i;      
   vg_assert(IRTemp_INVALID != temp);
   
   for (i = next_SP_alias_slot-1; i >= 0; i--) {
      if (temp == SP_aliases[i].temp) {
         *delta = SP_aliases[i].delta;
         return True;
      }
   }
   
   for (i = N_ALIASES-1; i >= next_SP_alias_slot; i--) {
      if (temp == SP_aliases[i].temp) {
         *delta = SP_aliases[i].delta;
         return True;
      }
   }
   return False;
}

static void update_SP_aliases(Long delta)
{
   Int i;
   for (i = 0; i < N_ALIASES; i++) {
      if (SP_aliases[i].temp == IRTemp_INVALID) {
         return;
      }
      SP_aliases[i].delta += delta;
   }
}

static IRExpr* mk_ecu_Expr ( Addr guest_IP )
{
   UInt ecu;
   ExeContext* ec
      = VG_(make_depth_1_ExeContext_from_Addr)( guest_IP );
   vg_assert(ec);
   ecu = VG_(get_ECU_from_ExeContext)( ec );
   vg_assert(VG_(is_plausible_ECU)(ecu));
   return mkIRExpr_HWord( (HWord)ecu );
}

static
IRSB* tool_instrument_then_gdbserver_if_needed ( VgCallbackClosure* closureV,
                                                 IRSB*              sb_in,
                                                 const VexGuestLayout*  layout,
                                                 const VexGuestExtents* vge,
                                                 const VexArchInfo*     vai,
                                                 IRType             gWordTy, 
                                                 IRType             hWordTy )
{
   return VG_(instrument_for_gdbserver_if_needed)
      (VG_(tdict).tool_instrument (closureV,
                                   sb_in,
                                   layout,
                                   vge,
                                   vai,
                                   gWordTy,
                                   hWordTy),
       layout,
       vge,
       gWordTy,
       hWordTy);                                   
}

static
IRSB* vg_SP_update_pass ( void*             closureV,
                          IRSB*             sb_in, 
                          const VexGuestLayout*   layout, 
                          const VexGuestExtents*  vge,
                          const VexArchInfo*      vai,
                          IRType            gWordTy, 
                          IRType            hWordTy )
{
   Int         i, j, k, minoff_ST, maxoff_ST, sizeof_SP, offset_SP;
   Int         first_SP, last_SP, first_Put, last_Put;
   IRDirty     *dcall, *d;
   IRStmt*     st;
   IRExpr*     e;
   IRRegArray* descr;
   IRType      typeof_SP;
   Long        delta, con;

   
   Bool   curr_IP_known = False;
   Addr   curr_IP       = 0;

   
   IRSB* bb     = emptyIRSB();
   bb->tyenv    = deepCopyIRTypeEnv(sb_in->tyenv);
   bb->next     = deepCopyIRExpr(sb_in->next);
   bb->jumpkind = sb_in->jumpkind;
   bb->offsIP   = sb_in->offsIP;

   delta = 0;

   sizeof_SP = layout->sizeof_SP;
   offset_SP = layout->offset_SP;
   typeof_SP = sizeof_SP==4 ? Ity_I32 : Ity_I64;
   vg_assert(sizeof_SP == 4 || sizeof_SP == 8);

   

#  define IS_ADD(op) (sizeof_SP==4 ? ((op)==Iop_Add32) : ((op)==Iop_Add64))
#  define IS_SUB(op) (sizeof_SP==4 ? ((op)==Iop_Sub32) : ((op)==Iop_Sub64))

#  define IS_ADD_OR_SUB(op) (IS_ADD(op) || IS_SUB(op))

#  define GET_CONST(con)                                                \
       (sizeof_SP==4 ? (Long)(Int)(con->Ico.U32)                        \
                     : (Long)(con->Ico.U64))

#  define DO_NEW(syze, tmpp)                                            \
      do {                                                              \
         Bool vanilla, w_ecu;                                           \
         vg_assert(curr_IP_known);                                      \
         vanilla = NULL != VG_(tdict).track_new_mem_stack_##syze;       \
         w_ecu   = NULL != VG_(tdict).track_new_mem_stack_##syze##_w_ECU; \
         vg_assert(!(vanilla && w_ecu));           \
         if (!(vanilla || w_ecu))                                       \
            goto generic;                                               \
                                                                        \
             \
                 \
         if (w_ecu) {                                                   \
            dcall = unsafeIRDirty_0_N(                                  \
                       2,                                   \
                       "track_new_mem_stack_" #syze "_w_ECU",           \
                       VG_(fnptr_to_fnentry)(                           \
                          VG_(tdict).track_new_mem_stack_##syze##_w_ECU ), \
                       mkIRExprVec_2(IRExpr_RdTmp(tmpp),                \
                                     mk_ecu_Expr(curr_IP))              \
                    );                                                  \
         } else {                                                       \
            dcall = unsafeIRDirty_0_N(                                  \
                       1,                                   \
                       "track_new_mem_stack_" #syze ,                   \
                       VG_(fnptr_to_fnentry)(                           \
                          VG_(tdict).track_new_mem_stack_##syze ),      \
                       mkIRExprVec_1(IRExpr_RdTmp(tmpp))                \
                    );                                                  \
         }                                                              \
         dcall->nFxState = 1;                                           \
         dcall->fxState[0].fx     = Ifx_Read;                           \
         dcall->fxState[0].offset = layout->offset_SP;                  \
         dcall->fxState[0].size   = layout->sizeof_SP;                  \
         dcall->fxState[0].nRepeats  = 0;                               \
         dcall->fxState[0].repeatLen = 0;                               \
                                                                        \
         addStmtToIRSB( bb, IRStmt_Dirty(dcall) );                      \
                                                                        \
         vg_assert(syze > 0);                                           \
         update_SP_aliases(syze);                                       \
                                                                        \
         n_SP_updates_fast++;                                           \
                                                                        \
      } while (0)

#  define DO_DIE(syze, tmpp)                                            \
      do {                                                              \
         if (!VG_(tdict).track_die_mem_stack_##syze)                    \
            goto generic;                                               \
                                                                        \
             \
                 \
         dcall = unsafeIRDirty_0_N(                                     \
                    1,                                      \
                    "track_die_mem_stack_" #syze,                       \
                    VG_(fnptr_to_fnentry)(                              \
                       VG_(tdict).track_die_mem_stack_##syze ),         \
                    mkIRExprVec_1(IRExpr_RdTmp(tmpp))                   \
                 );                                                     \
         dcall->nFxState = 1;                                           \
         dcall->fxState[0].fx     = Ifx_Read;                           \
         dcall->fxState[0].offset = layout->offset_SP;                  \
         dcall->fxState[0].size   = layout->sizeof_SP;                  \
         dcall->fxState[0].nRepeats  = 0;                               \
         dcall->fxState[0].repeatLen = 0;                               \
                                                                        \
         addStmtToIRSB( bb, IRStmt_Dirty(dcall) );                      \
                                                                        \
         vg_assert(syze > 0);                                           \
         update_SP_aliases(-(syze));                                    \
                                                                        \
         n_SP_updates_fast++;                                           \
                                                                        \
      } while (0)

   

   clear_SP_aliases();

   for (i = 0; i <  sb_in->stmts_used; i++) {

      st = sb_in->stmts[i];

      if (st->tag == Ist_IMark) {
         curr_IP_known = True;
         curr_IP       = st->Ist.IMark.addr;
      }

      
      if (st->tag != Ist_WrTmp) goto case2;
      e = st->Ist.WrTmp.data;
      if (e->tag != Iex_Get)              goto case2;
      if (e->Iex.Get.offset != offset_SP) goto case2;
      if (e->Iex.Get.ty != typeof_SP)     goto case2;
      vg_assert( typeOfIRTemp(bb->tyenv, st->Ist.WrTmp.tmp) == typeof_SP );
      add_SP_alias(st->Ist.WrTmp.tmp, 0);
      addStmtToIRSB( bb, st );
      continue;

     case2:
      
      if (st->tag != Ist_WrTmp) goto case3;
      e = st->Ist.WrTmp.data;
      if (e->tag != Iex_Binop) goto case3;
      if (e->Iex.Binop.arg1->tag != Iex_RdTmp) goto case3;
      if (!get_SP_delta(e->Iex.Binop.arg1->Iex.RdTmp.tmp, &delta)) goto case3;
      if (e->Iex.Binop.arg2->tag != Iex_Const) goto case3;
      if (!IS_ADD_OR_SUB(e->Iex.Binop.op)) goto case3;
      con = GET_CONST(e->Iex.Binop.arg2->Iex.Const.con);
      vg_assert( typeOfIRTemp(bb->tyenv, st->Ist.WrTmp.tmp) == typeof_SP );
      if (IS_ADD(e->Iex.Binop.op)) {
         add_SP_alias(st->Ist.WrTmp.tmp, delta + con);
      } else {
         add_SP_alias(st->Ist.WrTmp.tmp, delta - con);
      }
      addStmtToIRSB( bb, st );
      continue;

     case3:
      
      if (st->tag != Ist_WrTmp) goto case4;
      e = st->Ist.WrTmp.data;
      if (e->tag != Iex_RdTmp) goto case4;
      if (!get_SP_delta(e->Iex.RdTmp.tmp, &delta)) goto case4;
      vg_assert( typeOfIRTemp(bb->tyenv, st->Ist.WrTmp.tmp) == typeof_SP );
      add_SP_alias(st->Ist.WrTmp.tmp, delta);
      addStmtToIRSB( bb, st );
      continue;

     case4:
      
      /* More generally, we must correctly handle a Put which writes
         any part of SP, not just the case where all of SP is
         written. */
      if (st->tag != Ist_Put) goto case5;
      first_SP  = offset_SP;
      last_SP   = first_SP + sizeof_SP - 1;
      first_Put = st->Ist.Put.offset;
      last_Put  = first_Put
                  + sizeofIRType( typeOfIRExpr( bb->tyenv, st->Ist.Put.data ))
                  - 1;
      vg_assert(first_SP <= last_SP);
      vg_assert(first_Put <= last_Put);

      if (last_Put < first_SP || last_SP < first_Put)
         goto case5; 

      if (st->Ist.Put.data->tag == Iex_RdTmp
          && get_SP_delta(st->Ist.Put.data->Iex.RdTmp.tmp, &delta)) {
         IRTemp tttmp = st->Ist.Put.data->Iex.RdTmp.tmp;
         vg_assert( typeOfIRTemp(bb->tyenv, tttmp) == typeof_SP );
         vg_assert(first_SP == first_Put);
         vg_assert(last_SP == last_Put);
         switch (delta) {
            case    0:                      addStmtToIRSB(bb,st); continue;
            case    4: DO_DIE(  4,  tttmp); addStmtToIRSB(bb,st); continue;
            case   -4: DO_NEW(  4,  tttmp); addStmtToIRSB(bb,st); continue;
            case    8: DO_DIE(  8,  tttmp); addStmtToIRSB(bb,st); continue;
            case   -8: DO_NEW(  8,  tttmp); addStmtToIRSB(bb,st); continue;
            case   12: DO_DIE(  12, tttmp); addStmtToIRSB(bb,st); continue;
            case  -12: DO_NEW(  12, tttmp); addStmtToIRSB(bb,st); continue;
            case   16: DO_DIE(  16, tttmp); addStmtToIRSB(bb,st); continue;
            case  -16: DO_NEW(  16, tttmp); addStmtToIRSB(bb,st); continue;
            case   32: DO_DIE(  32, tttmp); addStmtToIRSB(bb,st); continue;
            case  -32: DO_NEW(  32, tttmp); addStmtToIRSB(bb,st); continue;
            case  112: DO_DIE( 112, tttmp); addStmtToIRSB(bb,st); continue;
            case -112: DO_NEW( 112, tttmp); addStmtToIRSB(bb,st); continue;
            case  128: DO_DIE( 128, tttmp); addStmtToIRSB(bb,st); continue;
            case -128: DO_NEW( 128, tttmp); addStmtToIRSB(bb,st); continue;
            case  144: DO_DIE( 144, tttmp); addStmtToIRSB(bb,st); continue;
            case -144: DO_NEW( 144, tttmp); addStmtToIRSB(bb,st); continue;
            case  160: DO_DIE( 160, tttmp); addStmtToIRSB(bb,st); continue;
            case -160: DO_NEW( 160, tttmp); addStmtToIRSB(bb,st); continue;
            default:  
               
               n_SP_updates_generic_known++;
               goto generic;
         }
      } else {
         IRTemp  old_SP;
         n_SP_updates_generic_unknown++;

         
         
         
        generic:
         old_SP = newIRTemp(bb->tyenv, typeof_SP);
         addStmtToIRSB( 
            bb,
            IRStmt_WrTmp( old_SP, IRExpr_Get(offset_SP, typeof_SP) ) 
         );

         if (first_Put == first_SP && last_Put == last_SP) {
            vg_assert(curr_IP_known);
            if (NULL != VG_(tdict).track_new_mem_stack_w_ECU)
               dcall = unsafeIRDirty_0_N( 
                          3, 
                          "VG_(unknown_SP_update_w_ECU)", 
                          VG_(fnptr_to_fnentry)( &VG_(unknown_SP_update_w_ECU) ),
                          mkIRExprVec_3( IRExpr_RdTmp(old_SP), st->Ist.Put.data,
                                         mk_ecu_Expr(curr_IP) ) 
                       );
            else
               dcall = unsafeIRDirty_0_N( 
                          2, 
                          "VG_(unknown_SP_update)", 
                          VG_(fnptr_to_fnentry)( &VG_(unknown_SP_update) ),
                          mkIRExprVec_2( IRExpr_RdTmp(old_SP), st->Ist.Put.data )
                       );

            addStmtToIRSB( bb, IRStmt_Dirty(dcall) );
            
            addStmtToIRSB( bb, st );
         } else {
            IRTemp new_SP;
            
            addStmtToIRSB( bb, st );
            
            new_SP = newIRTemp(bb->tyenv, typeof_SP);
            addStmtToIRSB( 
               bb,
               IRStmt_WrTmp( new_SP, IRExpr_Get(offset_SP, typeof_SP) ) 
            );
            
            addStmtToIRSB( bb, IRStmt_Put(offset_SP, IRExpr_RdTmp(old_SP) ));
            
            vg_assert(curr_IP_known);
            if (NULL != VG_(tdict).track_new_mem_stack_w_ECU)
               dcall = unsafeIRDirty_0_N( 
                          3, 
                          "VG_(unknown_SP_update_w_ECU)", 
                          VG_(fnptr_to_fnentry)( &VG_(unknown_SP_update_w_ECU) ),
                          mkIRExprVec_3( IRExpr_RdTmp(old_SP),
                                         IRExpr_RdTmp(new_SP), 
                                         mk_ecu_Expr(curr_IP) )
                       );
            else
               dcall = unsafeIRDirty_0_N( 
                          2, 
                          "VG_(unknown_SP_update)", 
                          VG_(fnptr_to_fnentry)( &VG_(unknown_SP_update) ),
                          mkIRExprVec_2( IRExpr_RdTmp(old_SP),
                                         IRExpr_RdTmp(new_SP) )
                       );
            addStmtToIRSB( bb, IRStmt_Dirty(dcall) );
            
            addStmtToIRSB( bb, IRStmt_Put(offset_SP, IRExpr_RdTmp(new_SP) ));
         }

         
         clear_SP_aliases();


         if (first_Put == first_SP && last_Put == last_SP
             && st->Ist.Put.data->tag == Iex_RdTmp) {
            vg_assert( typeOfIRTemp(bb->tyenv, st->Ist.Put.data->Iex.RdTmp.tmp)
                       == typeof_SP );
            add_SP_alias(st->Ist.Put.data->Iex.RdTmp.tmp, 0);
         }
         continue;
      }

     case5:
      if (st->tag == Ist_PutI) {
         descr = st->Ist.PutI.details->descr;
         minoff_ST = descr->base;
         maxoff_ST = descr->base 
                     + descr->nElems * sizeofIRType(descr->elemTy) - 1;
         if (!(offset_SP > maxoff_ST
               || (offset_SP + sizeof_SP - 1) < minoff_ST))
            goto complain;
      }
      if (st->tag == Ist_Dirty) {
         d = st->Ist.Dirty.details;
         for (j = 0; j < d->nFxState; j++) {
            if (d->fxState[j].fx == Ifx_Read || d->fxState[j].fx == Ifx_None)
               continue;
            
            for (k = 0; k < 1 + d->fxState[j].nRepeats; k++) {
               minoff_ST = d->fxState[j].offset + k * d->fxState[j].repeatLen;
               maxoff_ST = minoff_ST + d->fxState[j].size - 1;
               if (!(offset_SP > maxoff_ST
                     || (offset_SP + sizeof_SP - 1) < minoff_ST))
                  goto complain;
            }
         }
      }

      
      addStmtToIRSB( bb, st );

   } 

   return bb;

  complain:
   VG_(core_panic)("vg_SP_update_pass: PutI or Dirty which overlaps SP");

#undef IS_ADD
#undef IS_SUB
#undef IS_ADD_OR_SUB
#undef GET_CONST
#undef DO_NEW
#undef DO_DIE
}




#define N_TMPBUF 60000
static UChar tmpbuf[N_TMPBUF];


__attribute__ ((noreturn))
static
void failure_exit ( void )
{
   LibVEX_ShowAllocStats();
   VG_(core_panic)("LibVEX called failure_exit().");
}

static
void log_bytes ( const HChar* bytes, SizeT nbytes )
{
  SizeT i = 0;
  if (nbytes >= 4)
     for (; i < nbytes-3; i += 4)
        VG_(printf)("%c%c%c%c", bytes[i], bytes[i+1], bytes[i+2], bytes[i+3]);
  for (; i < nbytes; i++) 
     VG_(printf)("%c", bytes[i]);
}




static Bool translations_allowable_from_seg ( NSegment const* seg, Addr addr )
{
#  if defined(VGA_x86) || defined(VGA_s390x) || defined(VGA_mips32)     \
     || defined(VGA_mips64) || defined(VGA_tilegx)
   Bool allowR = True;
#  else
   Bool allowR = False;
#  endif
   return seg != NULL
          && (seg->kind == SkAnonC || seg->kind == SkFileC || seg->kind == SkShmC)
          && (seg->hasX 
              || (seg->hasR && (allowR
                                || VG_(has_gdbserver_breakpoint) (addr))));
}



static UInt needs_self_check ( void* closureV,
                               VexRegisterUpdates* pxControl,
                               const VexGuestExtents* vge )
{
   VgCallbackClosure* closure = (VgCallbackClosure*)closureV;
   UInt i, bitset;

   vg_assert(vge->n_used >= 1 && vge->n_used <= 3);
   bitset = 0;

   Bool pxStatusMightChange 
      = 
        VG_(clo_px_file_backed) != VexRegUpd_INVALID
        
        && *pxControl != VG_(clo_px_file_backed);

   const NSegment *segs[3] = { NULL, NULL, NULL };

   for (i = 0; i < vge->n_used; i++) {
      Bool  check = False;
      Addr  addr  = vge->base[i];
      SizeT len   = vge->len[i];
      NSegment const* segA = NULL;

#     if defined(VGO_darwin)
      // GrP fixme hack - dyld i386 IMPORT gets rewritten.
      
      
      segA = VG_(am_find_nsegment)(addr);
      if (segA && segA->hasX && segA->hasW)
         check = True;
#     endif

      if (!check) {
         switch (VG_(clo_smc_check)) {
            case Vg_SmcNone:
               
               break;
            case Vg_SmcAll: 
               
               check = True;
               break;
            case Vg_SmcStack: {
               Addr sp = VG_(get_SP)(closure->tid);
               if (!segA) {
                  segA = VG_(am_find_nsegment)(addr);
               }
               NSegment const* segSP = VG_(am_find_nsegment)(sp);
               if (segA && segSP && segA == segSP)
                  check = True;
               break;
            }
            case Vg_SmcAllNonFile: {
               if (!segA) {
                  segA = VG_(am_find_nsegment)(addr);
               }
               if (segA && segA->kind == SkFileC && segA->start <= addr
                   && (len == 0 || addr + len <= segA->end + 1)) {
                  
               } else {
                  check = True;
               }
               break;
            }
            default:
               vg_assert(0);
         }
      }

      if (check)
         bitset |= (1 << i);

      if (pxStatusMightChange && segA) {
         vg_assert(i < sizeof(segs)/sizeof(segs[0]));
         segs[i] = segA;
      }
   }

   if (pxStatusMightChange) {

      Bool allFileBacked = True;
      for (i = 0; i < vge->n_used; i++) {
         Addr  addr  = vge->base[i];
         SizeT len   = vge->len[i];
         NSegment const* segA = segs[i];
         if (!segA) {
            
            segA = VG_(am_find_nsegment)(addr);
         }
         vg_assert(segA); 
         if (segA && segA->kind == SkFileC && segA->start <= addr
             && (len == 0 || addr + len <= segA->end + 1)) {
            
         } else {
            allFileBacked = False;
            break;
         }
      }

      if (allFileBacked) {
         *pxControl = VG_(clo_px_file_backed);
      }

   }

   switch (*pxControl) {
      case VexRegUpdSpAtMemAccess:
         n_PX_VexRegUpdSpAtMemAccess++; break;
      case VexRegUpdUnwindregsAtMemAccess:
         n_PX_VexRegUpdUnwindregsAtMemAccess++; break;
      case VexRegUpdAllregsAtMemAccess:
         n_PX_VexRegUpdAllregsAtMemAccess++; break;
      case VexRegUpdAllregsAtEachInsn:
         n_PX_VexRegUpdAllregsAtEachInsn++; break;
      default:
         vg_assert(0);
   }

   return bitset;
}


static Bool chase_into_ok ( void* closureV, Addr addr )
{
   NSegment const*    seg     = VG_(am_find_nsegment)(addr);


   
   if (!translations_allowable_from_seg(seg, addr))
      goto dontchase;

   
   if (addr != VG_(redir_do_lookup)(addr, NULL))
      goto dontchase;

#  if defined(VG_PLAT_USES_PPCTOC) || defined(VGP_ppc64le_linux)
   
   if (addr == (Addr)&VG_(ppctoc_magic_redirect_return_stub))
      goto dontchase;
#  endif

   if (addr == TRANSTAB_BOGUS_GUEST_ADDR)
      goto dontchase;

#  if defined(VGA_s390x)
   if (((UChar *)addr)[0] == 0x44 ||   
       ((UChar *)addr)[0] == 0xC6)     
      goto dontchase;
#  endif

   
   return True;

   vg_assert(0);
   

  dontchase:
   if (0) VG_(printf)("not chasing into 0x%lx\n", addr);
   return False;
}




static IRExpr* mkU64 ( ULong n ) {
   return IRExpr_Const(IRConst_U64(n));
}
static IRExpr* mkU32 ( UInt n ) {
   return IRExpr_Const(IRConst_U32(n));
}

#if defined(VG_PLAT_USES_PPCTOC) || defined(VGP_ppc64le_linux)
static IRExpr* mkU8 ( UChar n ) {
   return IRExpr_Const(IRConst_U8(n));
}
static IRExpr* narrowTo32 ( IRTypeEnv* tyenv, IRExpr* e ) {
   if (typeOfIRExpr(tyenv, e) == Ity_I32) {
      return e;
   } else {
      vg_assert(typeOfIRExpr(tyenv, e) == Ity_I64);
      return IRExpr_Unop(Iop_64to32, e);
   }
}


static void gen_PUSH ( IRSB* bb, IRExpr* e )
{
   IRRegArray* descr;
   IRTemp      t1;
   IRExpr*     one;

#  if defined(VGP_ppc64be_linux) || defined(VGP_ppc64le_linux)
   Int    stack_size       = VEX_GUEST_PPC64_REDIR_STACK_SIZE;
   Int    offB_REDIR_SP    = offsetof(VexGuestPPC64State,guest_REDIR_SP);
   Int    offB_REDIR_STACK = offsetof(VexGuestPPC64State,guest_REDIR_STACK);
   Int    offB_EMNOTE      = offsetof(VexGuestPPC64State,guest_EMNOTE);
   Int    offB_CIA         = offsetof(VexGuestPPC64State,guest_CIA);
   Bool   is64             = True;
   IRType ty_Word          = Ity_I64;
   IROp   op_CmpNE         = Iop_CmpNE64;
   IROp   op_Sar           = Iop_Sar64;
   IROp   op_Sub           = Iop_Sub64;
   IROp   op_Add           = Iop_Add64;
   IRExpr*(*mkU)(ULong)    = mkU64;
   vg_assert(VG_WORDSIZE == 8);
#  else
   Int    stack_size       = VEX_GUEST_PPC32_REDIR_STACK_SIZE;
   Int    offB_REDIR_SP    = offsetof(VexGuestPPC32State,guest_REDIR_SP);
   Int    offB_REDIR_STACK = offsetof(VexGuestPPC32State,guest_REDIR_STACK);
   Int    offB_EMNOTE      = offsetof(VexGuestPPC32State,guest_EMNOTE);
   Int    offB_CIA         = offsetof(VexGuestPPC32State,guest_CIA);
   Bool   is64             = False;
   IRType ty_Word          = Ity_I32;
   IROp   op_CmpNE         = Iop_CmpNE32;
   IROp   op_Sar           = Iop_Sar32;
   IROp   op_Sub           = Iop_Sub32;
   IROp   op_Add           = Iop_Add32;
   IRExpr*(*mkU)(UInt)     = mkU32;
   vg_assert(VG_WORDSIZE == 4);
#  endif

   vg_assert(sizeof(void*) == VG_WORDSIZE);
   vg_assert(sizeof(Word)  == VG_WORDSIZE);
   vg_assert(sizeof(Addr)  == VG_WORDSIZE);

   descr = mkIRRegArray( offB_REDIR_STACK, ty_Word, stack_size );
   t1    = newIRTemp( bb->tyenv, ty_Word );
   one   = mkU(1);

   vg_assert(typeOfIRExpr(bb->tyenv, e) == ty_Word);

   
   addStmtToIRSB(
      bb, 
      IRStmt_WrTmp(
         t1, 
         IRExpr_Binop(op_Add, IRExpr_Get( offB_REDIR_SP, ty_Word ), one)
      )
   );

   addStmtToIRSB(
      bb,
      IRStmt_Put(offB_EMNOTE, mkU32(EmWarn_PPC64_redir_overflow))
   );
   addStmtToIRSB(
      bb,
      IRStmt_Exit(
         IRExpr_Binop(
            op_CmpNE,
            IRExpr_Binop(
               op_Sar,
               IRExpr_Binop(op_Sub,mkU(stack_size-1),IRExpr_RdTmp(t1)),
               mkU8(8 * VG_WORDSIZE - 1)
            ),
            mkU(0)
         ),
         Ijk_EmFail,
         is64 ? IRConst_U64(0) : IRConst_U32(0),
         offB_CIA
      )
   );

   
   addStmtToIRSB(bb, IRStmt_Put(offB_REDIR_SP, IRExpr_RdTmp(t1)));

   
   
   addStmtToIRSB(
      bb, 
      IRStmt_PutI(mkIRPutI(descr, 
                           narrowTo32(bb->tyenv,IRExpr_RdTmp(t1)), 0, e)));
}



static IRTemp gen_POP ( IRSB* bb )
{
#  if defined(VGP_ppc64be_linux) || defined(VGP_ppc64le_linux)
   Int    stack_size       = VEX_GUEST_PPC64_REDIR_STACK_SIZE;
   Int    offB_REDIR_SP    = offsetof(VexGuestPPC64State,guest_REDIR_SP);
   Int    offB_REDIR_STACK = offsetof(VexGuestPPC64State,guest_REDIR_STACK);
   Int    offB_EMNOTE      = offsetof(VexGuestPPC64State,guest_EMNOTE);
   Int    offB_CIA         = offsetof(VexGuestPPC64State,guest_CIA);
   Bool   is64             = True;
   IRType ty_Word          = Ity_I64;
   IROp   op_CmpNE         = Iop_CmpNE64;
   IROp   op_Sar           = Iop_Sar64;
   IROp   op_Sub           = Iop_Sub64;
   IRExpr*(*mkU)(ULong)    = mkU64;
#  else
   Int    stack_size       = VEX_GUEST_PPC32_REDIR_STACK_SIZE;
   Int    offB_REDIR_SP    = offsetof(VexGuestPPC32State,guest_REDIR_SP);
   Int    offB_REDIR_STACK = offsetof(VexGuestPPC32State,guest_REDIR_STACK);
   Int    offB_EMNOTE      = offsetof(VexGuestPPC32State,guest_EMNOTE);
   Int    offB_CIA         = offsetof(VexGuestPPC32State,guest_CIA);
   Bool   is64             = False;
   IRType ty_Word          = Ity_I32;
   IROp   op_CmpNE         = Iop_CmpNE32;
   IROp   op_Sar           = Iop_Sar32;
   IROp   op_Sub           = Iop_Sub32;
   IRExpr*(*mkU)(UInt)     = mkU32;
#  endif

   IRRegArray* descr = mkIRRegArray( offB_REDIR_STACK, ty_Word, stack_size );
   IRTemp      t1    = newIRTemp( bb->tyenv, ty_Word );
   IRTemp      res   = newIRTemp( bb->tyenv, ty_Word );
   IRExpr*     one   = mkU(1);

   vg_assert(sizeof(void*) == VG_WORDSIZE);
   vg_assert(sizeof(Word)  == VG_WORDSIZE);
   vg_assert(sizeof(Addr)  == VG_WORDSIZE);

   
   addStmtToIRSB(
      bb, 
      IRStmt_WrTmp( t1, IRExpr_Get( offB_REDIR_SP, ty_Word ) )
   );

   
   addStmtToIRSB(
      bb,
      IRStmt_Put(offB_EMNOTE, mkU32(EmWarn_PPC64_redir_underflow))
   );
   addStmtToIRSB(
      bb,
      IRStmt_Exit(
         IRExpr_Binop(
            op_CmpNE,
            IRExpr_Binop(
               op_Sar,
               IRExpr_RdTmp(t1),
               mkU8(8 * VG_WORDSIZE - 1)
            ),
            mkU(0)
         ),
         Ijk_EmFail,
         is64 ? IRConst_U64(0) : IRConst_U32(0),
         offB_CIA
      )
   );

   
   
   addStmtToIRSB(
      bb,
      IRStmt_WrTmp(
         res, 
         IRExpr_GetI(descr, narrowTo32(bb->tyenv,IRExpr_RdTmp(t1)), 0)
      )
   );

   
   addStmtToIRSB(
      bb, 
      IRStmt_Put(offB_REDIR_SP, IRExpr_Binop(op_Sub, IRExpr_RdTmp(t1), one))
   );

   return res;
}

#endif

#if defined(VG_PLAT_USES_PPCTOC)


static void gen_push_and_set_LR_R2 ( IRSB* bb, Addr new_R2_value )
{
#  if defined(VGP_ppc64be_linux)
   Addr   bogus_RA  = (Addr)&VG_(ppctoc_magic_redirect_return_stub);
   Int    offB_GPR2 = offsetof(VexGuestPPC64State,guest_GPR2);
   Int    offB_LR   = offsetof(VexGuestPPC64State,guest_LR);
   gen_PUSH( bb, IRExpr_Get(offB_LR,   Ity_I64) );
   gen_PUSH( bb, IRExpr_Get(offB_GPR2, Ity_I64) );
   addStmtToIRSB( bb, IRStmt_Put( offB_LR,   mkU64( bogus_RA )) );
   addStmtToIRSB( bb, IRStmt_Put( offB_GPR2, mkU64( new_R2_value )) );

#  else
#    error Platform is not TOC-afflicted, fortunately
#  endif
}
#endif

#if defined(VG_PLAT_USES_PPCTOC) || defined(VGP_ppc64le_linux)

static void gen_pop_R2_LR_then_bLR ( IRSB* bb )
{
#  if defined(VGP_ppc64be_linux)  || defined(VGP_ppc64le_linux)
   Int    offB_GPR2 = offsetof(VexGuestPPC64State,guest_GPR2);
   Int    offB_LR   = offsetof(VexGuestPPC64State,guest_LR);
   Int    offB_CIA  = offsetof(VexGuestPPC64State,guest_CIA);
   IRTemp old_R2    = newIRTemp( bb->tyenv, Ity_I64 );
   IRTemp old_LR    = newIRTemp( bb->tyenv, Ity_I64 );
   
   old_R2 = gen_POP( bb );
   addStmtToIRSB( bb, IRStmt_Put( offB_GPR2, IRExpr_RdTmp(old_R2)) );
   
   old_LR = gen_POP( bb );
   addStmtToIRSB( bb, IRStmt_Put( offB_LR, IRExpr_RdTmp(old_LR)) );
   
   bb->jumpkind = Ijk_Boring;
   bb->next     = IRExpr_Binop(Iop_And64, IRExpr_RdTmp(old_LR), mkU64(~(3ULL)));
   bb->offsIP   = offB_CIA;
#  else
#    error Platform is not TOC-afflicted, fortunately
#  endif
}
#endif

#if defined(VG_PLAT_USES_PPCTOC) || defined(VGP_ppc64le_linux)

static
Bool mk_preamble__ppctoc_magic_return_stub ( void* closureV, IRSB* bb )
{
   VgCallbackClosure* closure = (VgCallbackClosure*)closureV;
   addStmtToIRSB( bb, IRStmt_IMark( closure->readdr, 4, 0 ) );
   gen_pop_R2_LR_then_bLR(bb);
   return True; 
}
#endif

#if defined(VGP_ppc64le_linux)

static void gen_push_R2_and_set_LR ( IRSB* bb )
{
   Addr   bogus_RA  = (Addr)&VG_(ppctoc_magic_redirect_return_stub);
   Int    offB_GPR2 = offsetof(VexGuestPPC64State,guest_GPR2);
   Int    offB_LR   = offsetof(VexGuestPPC64State,guest_LR);
   gen_PUSH( bb, IRExpr_Get(offB_LR,   Ity_I64) );
   gen_PUSH( bb, IRExpr_Get(offB_GPR2, Ity_I64) );
   addStmtToIRSB( bb, IRStmt_Put( offB_LR,   mkU64( bogus_RA )) );
}
#  endif



/* This is the IR preamble generator used for replacement
   functions.  It adds code to set the guest_NRADDR{_GPR2} to zero
   (technically not necessary, but facilitates detecting mixups in
   which a replacement function has been erroneously declared using
   VG_REPLACE_FUNCTION_Z{U,Z} when instead it should have been written
   using VG_WRAP_FUNCTION_Z{U,Z}).

   On with-TOC platforms the follow hacks are also done: LR and R2 are
   pushed onto a hidden stack, R2 is set to the correct value for the
   replacement function, and LR is set to point at the magic
   return-stub address.  Setting LR causes the return of the
   wrapped/redirected function to lead to our magic return stub, which
   restores LR and R2 from said stack and returns for real.

   VG_(get_StackTrace_wrk) understands that the LR value may point to
   the return stub address, and that in that case it can get the real
   LR value from the hidden stack instead. */
static 
Bool mk_preamble__set_NRADDR_to_zero ( void* closureV, IRSB* bb )
{
   Int nraddr_szB
      = sizeof(((VexGuestArchState*)0)->guest_NRADDR);
   vg_assert(nraddr_szB == 4 || nraddr_szB == 8);
   vg_assert(nraddr_szB == VG_WORDSIZE);
   addStmtToIRSB( 
      bb,
      IRStmt_Put( 
         offsetof(VexGuestArchState,guest_NRADDR),
         nraddr_szB == 8 ? mkU64(0) : mkU32(0)
      )
   );
   
#  if defined(VGP_mips32_linux)
   VgCallbackClosure* closure = (VgCallbackClosure*)closureV;
   Int offB_GPR25 = offsetof(VexGuestMIPS32State, guest_r25);
   addStmtToIRSB(bb, IRStmt_Put(offB_GPR25, mkU32(closure->readdr)));
#  endif
#  if defined(VGP_mips64_linux)
   VgCallbackClosure* closure = (VgCallbackClosure*)closureV;
   Int offB_GPR25 = offsetof(VexGuestMIPS64State, guest_r25);
   addStmtToIRSB(bb, IRStmt_Put(offB_GPR25, mkU64(closure->readdr)));
#  endif
#  if defined(VG_PLAT_USES_PPCTOC)
   { VgCallbackClosure* closure = (VgCallbackClosure*)closureV;
     addStmtToIRSB(
        bb,
        IRStmt_Put(
           offsetof(VexGuestArchState,guest_NRADDR_GPR2),
           VG_WORDSIZE==8 ? mkU64(0) : mkU32(0)
        )
     );
     gen_push_and_set_LR_R2 ( bb, VG_(get_tocptr)( closure->readdr ) );
   }
#  endif

#if defined(VGP_ppc64le_linux)
   VgCallbackClosure* closure = (VgCallbackClosure*)closureV;
   Int offB_GPR12 = offsetof(VexGuestArchState, guest_GPR12);
   addStmtToIRSB(bb, IRStmt_Put(offB_GPR12, mkU64(closure->readdr)));
   addStmtToIRSB(bb,
      IRStmt_Put(
         offsetof(VexGuestArchState,guest_NRADDR_GPR2),
         VG_WORDSIZE==8 ? mkU64(0) : mkU32(0)
      )
   );
   gen_push_R2_and_set_LR ( bb );
#endif
   return False;
}

static 
Bool mk_preamble__set_NRADDR_to_nraddr ( void* closureV, IRSB* bb )
{
   VgCallbackClosure* closure = (VgCallbackClosure*)closureV;
   Int nraddr_szB
      = sizeof(((VexGuestArchState*)0)->guest_NRADDR);
   vg_assert(nraddr_szB == 4 || nraddr_szB == 8);
   vg_assert(nraddr_szB == VG_WORDSIZE);
   addStmtToIRSB( 
      bb,
      IRStmt_Put( 
         offsetof(VexGuestArchState,guest_NRADDR),
         nraddr_szB == 8
            ? IRExpr_Const(IRConst_U64( closure->nraddr ))
            : IRExpr_Const(IRConst_U32( (UInt)closure->nraddr ))
      )
   );
   
#  if defined(VGP_mips32_linux)
   Int offB_GPR25 = offsetof(VexGuestMIPS32State, guest_r25);
   addStmtToIRSB(bb, IRStmt_Put(offB_GPR25, mkU32(closure->readdr)));
#  endif
#  if defined(VGP_mips64_linux)
   Int offB_GPR25 = offsetof(VexGuestMIPS64State, guest_r25);
   addStmtToIRSB(bb, IRStmt_Put(offB_GPR25, mkU64(closure->readdr)));
#  endif
#  if defined(VG_PLAT_USES_PPCTOC)
   addStmtToIRSB( 
      bb,
      IRStmt_Put( 
         offsetof(VexGuestArchState,guest_NRADDR_GPR2),
         IRExpr_Get(offsetof(VexGuestArchState,guest_GPR2), 
                    VG_WORDSIZE==8 ? Ity_I64 : Ity_I32)
      )
   );
   gen_push_and_set_LR_R2 ( bb, VG_(get_tocptr)( closure->readdr ) );
#  endif
#if defined(VGP_ppc64le_linux)
   Int offB_GPR12 = offsetof(VexGuestArchState, guest_GPR12);
   addStmtToIRSB(
      bb,
      IRStmt_Put(
         offsetof(VexGuestArchState,guest_NRADDR_GPR2),
         IRExpr_Get(offsetof(VexGuestArchState,guest_GPR2),
                    VG_WORDSIZE==8 ? Ity_I64 : Ity_I32)
      )
   );
   addStmtToIRSB(bb, IRStmt_Put(offB_GPR12, mkU64(closure->readdr)));
   gen_push_R2_and_set_LR ( bb );
#endif
   return False;
}


__attribute__((unused))
static Bool const_True ( Addr guest_addr )
{
   return True;
}



typedef 
   enum {
      
      T_Normal, 
      
      T_Redir_Wrap,
      
      T_Redir_Replace,
      
      T_NoRedir
   }
   T_Kind;


Bool VG_(translate) ( ThreadId tid, 
                      Addr     nraddr,
                      Bool     debugging_translation,
                      Int      debugging_verbosity,
                      ULong    bbs_done,
                      Bool     allow_redirection )
{
   Addr               addr;
   T_Kind             kind;
   Int                tmpbuf_used, verbosity, i;
   Bool (*preamble_fn)(void*,IRSB*);
   VexArch            vex_arch;
   VexArchInfo        vex_archinfo;
   VexAbiInfo         vex_abiinfo;
   VexGuestExtents    vge;
   VexTranslateArgs   vta;
   VexTranslateResult tres;
   VgCallbackClosure  closure;

   

   static Bool vex_init_done = False;

   if (!vex_init_done) {
      LibVEX_Init ( &failure_exit, &log_bytes, 
                    1,      
                    &VG_(clo_vex_control) );
      vex_init_done = True;
   }

   if (allow_redirection) {
      Bool isWrap;
      Addr tmp = VG_(redir_do_lookup)( nraddr, &isWrap );
      if (tmp == nraddr) {
         
         addr = nraddr;
         kind = T_Normal;
      } else {
         
         addr = tmp;
         kind = isWrap ? T_Redir_Wrap : T_Redir_Replace;
      }
   } else {
      addr = nraddr;
      kind = T_NoRedir;
   }

   

   

   if ((kind == T_Redir_Wrap || kind == T_Redir_Replace)
       && (VG_(clo_verbosity) >= 2 || VG_(clo_trace_redir))) {
      Bool ok;
      const HChar *buf;
      const HChar *name2;

      const HChar* nraddr_soname = "???";
      DebugInfo*   nraddr_di     = VG_(find_DebugInfo)(nraddr);
      if (nraddr_di) {
         const HChar* t = VG_(DebugInfo_get_soname)(nraddr_di);
         if (t)
            nraddr_soname = t;
      }

      ok = VG_(get_fnname_w_offset)(nraddr, &buf);
      if (!ok) buf = "???";
      
      HChar name1[VG_(strlen)(buf) + 1];
      VG_(strcpy)(name1, buf);
      ok = VG_(get_fnname_w_offset)(addr, &name2);
      if (!ok) name2 = "???";

      VG_(message)(Vg_DebugMsg, 
                   "REDIR: 0x%lx (%s:%s) redirected to 0x%lx (%s)\n",
                   nraddr, nraddr_soname, name1,
                   addr, name2 );
   }

   if (!debugging_translation)
      VG_TRACK( pre_mem_read, Vg_CoreTranslate, 
                              tid, "(translator)", addr, 1 );

   
   if (VG_(clo_trace_flags) || debugging_translation) {
      const HChar* objname = "UNKNOWN_OBJECT";
      OffT         objoff  = 0;
      DebugInfo*   di      = VG_(find_DebugInfo)( addr );
      if (di) {
         objname = VG_(DebugInfo_get_filename)(di);
         objoff  = addr - VG_(DebugInfo_get_text_bias)(di);
      }
      vg_assert(objname);
 
      const HChar *fnname;
      Bool ok = VG_(get_fnname_w_offset)(addr, &fnname);
      if (!ok) fnname = "UNKNOWN_FUNCTION";
      VG_(printf)(
         "==== SB %d (evchecks %lld) [tid %d] 0x%lx %s %s+0x%llx\n",
         VG_(get_bbs_translated)(), bbs_done, (Int)tid, addr,
         fnname, objname, (ULong)objoff
      );
   }

   

   { 
   NSegment const* seg = VG_(am_find_nsegment)(addr);

   if ( (!translations_allowable_from_seg(seg, addr))
        || addr == TRANSTAB_BOGUS_GUEST_ADDR ) {
      if (VG_(clo_trace_signals))
         VG_(message)(Vg_DebugMsg, "translations not allowed here (0x%lx)"
                                   " - throwing SEGV\n", addr);
      
      if (seg != NULL) {
         if (debugging_translation)
            VG_(printf)("translations not allowed here (segment not executable)"
                        "(0x%lx)\n", addr);
         else
            VG_(synth_fault_perms)(tid, addr);
      } else {
         if (debugging_translation)
            VG_(printf)("translations not allowed here (no segment)"
                        "(0x%lx)\n", addr);
         else
            VG_(synth_fault_mapping)(tid, addr);
      }
      return False;
   }

   
   verbosity = 0;
   if (debugging_translation) {
      verbosity = debugging_verbosity;
   }
   else
   if ( (VG_(clo_trace_flags) > 0
        && VG_(get_bbs_translated)() <= VG_(clo_trace_notabove)
        && VG_(get_bbs_translated)() >= VG_(clo_trace_notbelow) )) {
      verbosity = VG_(clo_trace_flags);
   }

   
   preamble_fn = NULL;
   if (kind == T_Redir_Replace)
      preamble_fn = mk_preamble__set_NRADDR_to_zero;
   else 
   if (kind == T_Redir_Wrap)
      preamble_fn = mk_preamble__set_NRADDR_to_nraddr;

   
#  if defined(VG_PLAT_USES_PPCTOC) || defined(VGP_ppc64le_linux)
   if (nraddr == (Addr)&VG_(ppctoc_magic_redirect_return_stub)) {
      vg_assert(kind == T_Normal);
      vg_assert(nraddr == addr);
      preamble_fn = mk_preamble__ppctoc_magic_return_stub;
   }
#  endif

   
   vg_assert2(VG_(tdict).tool_instrument,
              "you forgot to set VgToolInterface function 'tool_instrument'");

   
   VG_(machine_get_VexArchInfo)( &vex_arch, &vex_archinfo );


   LibVEX_default_VexAbiInfo( &vex_abiinfo );
   vex_abiinfo.guest_stack_redzone_size = VG_STACK_REDZONE_SZB;

#  if defined(VGP_amd64_linux)
   vex_abiinfo.guest_amd64_assume_fs_is_const = True;
   vex_abiinfo.guest_amd64_assume_gs_is_const = True;
#  endif
#  if defined(VGP_amd64_darwin)
   vex_abiinfo.guest_amd64_assume_gs_is_const = True;
#  endif
#  if defined(VGP_ppc32_linux)
   vex_abiinfo.guest_ppc_zap_RZ_at_blr        = False;
   vex_abiinfo.guest_ppc_zap_RZ_at_bl         = NULL;
#  endif
#  if defined(VGP_ppc64be_linux)
   vex_abiinfo.guest_ppc_zap_RZ_at_blr        = True;
   vex_abiinfo.guest_ppc_zap_RZ_at_bl         = const_True;
   vex_abiinfo.host_ppc_calls_use_fndescrs    = True;
#  endif
#  if defined(VGP_ppc64le_linux)
   vex_abiinfo.guest_ppc_zap_RZ_at_blr        = True;
   vex_abiinfo.guest_ppc_zap_RZ_at_bl         = const_True;
   vex_abiinfo.host_ppc_calls_use_fndescrs    = False;
#  endif

   
   closure.tid    = tid;
   closure.nraddr = nraddr;
   closure.readdr = addr;

   
   vta.arch_guest       = vex_arch;
   vta.archinfo_guest   = vex_archinfo;
   vta.arch_host        = vex_arch;
   vta.archinfo_host    = vex_archinfo;
   vta.abiinfo_both     = vex_abiinfo;
   vta.callback_opaque  = (void*)&closure;
   vta.guest_bytes      = (UChar*)addr;
   vta.guest_bytes_addr = addr;
   vta.chase_into_ok    = chase_into_ok;
   vta.guest_extents    = &vge;
   vta.host_bytes       = tmpbuf;
   vta.host_bytes_size  = N_TMPBUF;
   vta.host_bytes_used  = &tmpbuf_used;
   { 
     IRSB*(*f)(VgCallbackClosure*,
               IRSB*,const VexGuestLayout*,const VexGuestExtents*,
               const VexArchInfo*,IRType,IRType)
        = VG_(clo_vgdb) != Vg_VgdbNo
             ? tool_instrument_then_gdbserver_if_needed
             : VG_(tdict).tool_instrument;
     IRSB*(*g)(void*,
               IRSB*,const VexGuestLayout*,const VexGuestExtents*,
               const VexArchInfo*,IRType,IRType) = (__typeof__(g)) f;
     vta.instrument1     = g;
   }
   
   vta.instrument2       = need_to_handle_SP_assignment()
                              ? vg_SP_update_pass
                              : NULL;
   vta.finaltidy         = VG_(needs).final_IR_tidy_pass
                              ? VG_(tdict).tool_final_IR_tidy_pass
                              : NULL;
   vta.needs_self_check  = needs_self_check;
   vta.preamble_function = preamble_fn;
   vta.traceflags        = verbosity;
   vta.sigill_diag       = VG_(clo_sigill_diag);
   vta.addProfInc        = VG_(clo_profyle_sbs) && kind != T_NoRedir;

   if (allow_redirection) {
      vta.disp_cp_chain_me_to_slowEP
         = VG_(fnptr_to_fnentry)( &VG_(disp_cp_chain_me_to_slowEP) );
      vta.disp_cp_chain_me_to_fastEP
         = VG_(fnptr_to_fnentry)( &VG_(disp_cp_chain_me_to_fastEP) );
      vta.disp_cp_xindir
         = VG_(fnptr_to_fnentry)( &VG_(disp_cp_xindir) );
   } else {
      vta.disp_cp_chain_me_to_slowEP = NULL;
      vta.disp_cp_chain_me_to_fastEP = NULL;
      vta.disp_cp_xindir             = NULL;
   }
   
   vta.disp_cp_xassisted
      = VG_(fnptr_to_fnentry)( &VG_(disp_cp_xassisted) );

   
   tres = LibVEX_Translate ( &vta );

   vg_assert(tres.status == VexTransOK);
   vg_assert(tres.n_sc_extents >= 0 && tres.n_sc_extents <= 3);
   vg_assert(tmpbuf_used <= N_TMPBUF);
   vg_assert(tmpbuf_used > 0);
   } 

   for (i = 0; i < vge.n_used; i++) {
      VG_(am_set_segment_hasT)( vge.base[i] );
   }

   
   vg_assert(tmpbuf_used > 0 && tmpbuf_used < 65536);

   
   
   if (!debugging_translation) {

      if (kind != T_NoRedir) {
          
          

          
          
          VG_(add_to_transtab)( &vge,
                                nraddr,
                                (Addr)(&tmpbuf[0]), 
                                tmpbuf_used,
                                tres.n_sc_extents > 0,
                                tres.offs_profInc,
                                tres.n_guest_instrs );
      } else {
          vg_assert(tres.offs_profInc == -1); 
          VG_(add_to_unredir_transtab)( &vge,
                                        nraddr,
                                        (Addr)(&tmpbuf[0]), 
                                        tmpbuf_used );
      }
   }

   return True;
}

