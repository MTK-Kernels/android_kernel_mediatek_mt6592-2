

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

#include "main_util.h"
#include "host_generic_regs.h"

#define DEBUG_REGALLOC 0




typedef
   struct {
      
      Short live_after;
      
      Short dead_before;
      
      Short spill_offset;
      Short spill_size;
      
      HRegClass reg_class;
   }
   VRegLR;


typedef
   struct {
      HReg rreg;
      
      Short live_after;
      
      Short dead_before;
   }
   RRegLR;


typedef
   struct {
      
      
      Bool has_hlrs;
      
      Bool is_spill_cand;
      Bool eq_spill_slot;
      
      enum { Free,     
             Unavail,  
             Bound     
           }
           disp;
      
      HReg vreg;
   }
   RRegState;



#define INVALID_RREG_NO ((Short)(-1))

#define IS_VALID_VREGNO(_zz) ((_zz) >= 0 && (_zz) < n_vregs)
#define IS_VALID_RREGNO(_zz) ((_zz) >= 0 && (_zz) < n_rregs)


static
Int findMostDistantlyMentionedVReg ( 
   HRegUsage*   reg_usages_in,
   Int          search_from_instr,
   Int          num_instrs,
   RRegState*   state,
   Int          n_state
)
{
   Int k, m;
   Int furthest_k = -1;
   Int furthest   = -1;
   vassert(search_from_instr >= 0);
   for (k = 0; k < n_state; k++) {
      if (!state[k].is_spill_cand)
         continue;
      vassert(state[k].disp == Bound);
      for (m = search_from_instr; m < num_instrs; m++) {
         if (HRegUsage__contains(&reg_usages_in[m], state[k].vreg))
            break;
      }
      if (m > furthest) {
         furthest   = m;
         furthest_k = k;
      }
   }
   return furthest_k;
}


inline
static void sanity_check_spill_offset ( VRegLR* vreg )
{
   switch (vreg->reg_class) {
      case HRcVec128: case HRcFlt64:
         vassert(0 == ((UShort)vreg->spill_offset % 16)); break;
      default:
         vassert(0 == ((UShort)vreg->spill_offset % 8)); break;
   }
}


__attribute__((noinline)) 
static void ensureRRLRspace_SLOW ( RRegLR** info, Int* size, Int used )
{
   Int     k;
   RRegLR* arr2;
   if (0)
      vex_printf("ensureRRISpace: %d -> %d\n", *size, 2 * *size);
   vassert(used == *size);
   arr2 = LibVEX_Alloc_inline(2 * *size * sizeof(RRegLR));
   for (k = 0; k < *size; k++)
      arr2[k] = (*info)[k];
   *size *= 2;
   *info = arr2;
}
inline
static void ensureRRLRspace ( RRegLR** info, Int* size, Int used )
{
   if (LIKELY(used < *size)) return;
   ensureRRLRspace_SLOW(info, size, used);
}


static void sortRRLRarray ( RRegLR* arr, 
                            Int size, Bool by_live_after )
{
   Int    incs[14] = { 1, 4, 13, 40, 121, 364, 1093, 3280,
                       9841, 29524, 88573, 265720,
                       797161, 2391484 };
   Int    lo = 0;
   Int    hi = size-1;
   Int    i, j, h, bigN, hp;
   RRegLR v;

   vassert(size >= 0);
   if (size == 0)
      return;

   bigN = hi - lo + 1; if (bigN < 2) return;
   hp = 0; while (hp < 14 && incs[hp] < bigN) hp++; hp--;

   if (by_live_after) {

      for ( ; hp >= 0; hp--) {
         h = incs[hp];
         for (i = lo + h; i <= hi; i++) {
            v = arr[i];
            j = i;
            while (arr[j-h].live_after > v.live_after) {
               arr[j] = arr[j-h];
               j = j - h;
               if (j <= (lo + h - 1)) break;
            }
            arr[j] = v;
         }
      }

   } else {

      for ( ; hp >= 0; hp--) {
         h = incs[hp];
         for (i = lo + h; i <= hi; i++) {
            v = arr[i];
            j = i;
            while (arr[j-h].dead_before > v.dead_before) {
               arr[j] = arr[j-h];
               j = j - h;
               if (j <= (lo + h - 1)) break;
            }
            arr[j] = v;
         }
      }

   }
}


static inline UInt ULong__maxIndex ( ULong w64 ) {
   return 63 - __builtin_clzll(w64);
}

static inline UInt ULong__minIndex ( ULong w64 ) {
   return __builtin_ctzll(w64);
}


static void* local_memset ( void *destV, Int c, SizeT sz )
{
#  define IS_4_ALIGNED(aaa_p) (0 == (((HWord)(aaa_p)) & ((HWord)0x3)))

   UInt   c4;
   UChar* d = destV;
   UChar  uc = c;

   while ((!IS_4_ALIGNED(d)) && sz >= 1) {
      d[0] = uc;
      d++;
      sz--;
   }
   if (sz == 0)
      return destV;
   c4 = uc;
   c4 |= (c4 << 8);
   c4 |= (c4 << 16);
   while (sz >= 16) {
      ((UInt*)d)[0] = c4;
      ((UInt*)d)[1] = c4;
      ((UInt*)d)[2] = c4;
      ((UInt*)d)[3] = c4;
      d += 16;
      sz -= 16;
   }
   while (sz >= 4) {
      ((UInt*)d)[0] = c4;
      d += 4;
      sz -= 4;
   }
   while (sz >= 1) {
      d[0] = c;
      d++;
      sz--;
   }
   return destV;

#  undef IS_4_ALIGNED
}


HInstrArray* doRegisterAllocation (

    
   HInstrArray* instrs_in,

   const RRegUniverse* univ,

   Bool (*isMove) ( const HInstr*, HReg*, HReg* ),

   
   void (*getRegUsage) ( HRegUsage*, const HInstr*, Bool ),

   
   void (*mapRegs) ( HRegRemap*, HInstr*, Bool ),

   void    (*genSpill)  ( HInstr**, HInstr**, HReg, Int, Bool ),
   void    (*genReload) ( HInstr**, HInstr**, HReg, Int, Bool ),
   HInstr* (*directReload) ( HInstr*, HReg, Short ),
   Int     guest_sizeB,

   
   void (*ppInstr) ( const HInstr*, Bool ),
   void (*ppReg) ( HReg ),

   
   Bool mode64
)
{
#  define N_SPILL64S  (LibVEX_N_SPILL_BYTES / 8)

   const Bool eq_spill_opt = True;

   Int     n_vregs;
   VRegLR* vreg_lrs; 

   RRegLR* rreg_lrs_la;
   RRegLR* rreg_lrs_db;
   Int     rreg_lrs_size;
   Int     rreg_lrs_used;
   Int     rreg_lrs_la_next;
   Int     rreg_lrs_db_next;

   HRegUsage* reg_usage_arr; 

   Short ss_busy_until_before[N_SPILL64S];

   
   Int* rreg_live_after;
   Int* rreg_dead_before;

   
   RRegState* rreg_state;  
   Int        n_rregs;

   
   Short*     vreg_state;  

   HRegRemap remap;

   
   HInstrArray* instrs_out;

   Bool do_sanity_check;

   vassert(0 == (guest_sizeB % LibVEX_GUEST_STATE_ALIGN));
   vassert(0 == (LibVEX_N_SPILL_BYTES % LibVEX_GUEST_STATE_ALIGN));
   vassert(0 == (N_SPILL64S % 2));

   vassert(instrs_in->arr_used <= 15000);

#  define INVALID_INSTRNO (-2)

#  define EMIT_INSTR(_instr)                  \
      do {                                    \
        HInstr* _tmp = (_instr);              \
        if (DEBUG_REGALLOC) {                 \
           vex_printf("**  ");                \
           (*ppInstr)(_tmp, mode64);          \
           vex_printf("\n\n");                \
        }                                     \
        addHInstr ( instrs_out, _tmp );       \
      } while (0)

#   define PRINT_STATE						   \
      do {							   \
         Int z, q;						   \
         for (z = 0; z < n_rregs; z++) {			   \
            vex_printf("  rreg_state[%2d] = ", z);		   \
            (*ppReg)(univ->regs[z]);	       			   \
            vex_printf("  \t");					   \
            switch (rreg_state[z].disp) {			   \
               case Free:    vex_printf("Free\n"); break;	   \
               case Unavail: vex_printf("Unavail\n"); break;	   \
               case Bound:   vex_printf("BoundTo "); 		   \
                             (*ppReg)(rreg_state[z].vreg);	   \
                             vex_printf("\n"); break;		   \
            }							   \
         }							   \
         vex_printf("\n  vreg_state[0 .. %d]:\n    ", n_vregs-1);  \
         q = 0;                                                    \
         for (z = 0; z < n_vregs; z++) {                           \
            if (vreg_state[z] == INVALID_RREG_NO)                  \
               continue;                                           \
            vex_printf("[%d] -> %d   ", z, vreg_state[z]);         \
            q++;                                                   \
            if (q > 0 && (q % 6) == 0)                             \
               vex_printf("\n    ");                               \
         }                                                         \
         vex_printf("\n");                                         \
      } while (0)


   
   

   instrs_out = newHInstrArray();

   
   
   n_rregs = univ->allocable;
   n_vregs = instrs_in->n_vregs;

   
   vassert(n_vregs < 32767);

   
   vassert(n_rregs > 0);

   rreg_state = LibVEX_Alloc_inline(n_rregs * sizeof(RRegState));
   vreg_state = LibVEX_Alloc_inline(n_vregs * sizeof(Short));

   for (Int j = 0; j < n_rregs; j++) {
      rreg_state[j].has_hlrs      = False;
      rreg_state[j].disp          = Free;
      rreg_state[j].vreg          = INVALID_HREG;
      rreg_state[j].is_spill_cand = False;
      rreg_state[j].eq_spill_slot = False;
   }

   for (Int j = 0; j < n_vregs; j++)
      vreg_state[j] = INVALID_RREG_NO;


   
   

   


   vreg_lrs = NULL;
   if (n_vregs > 0)
      vreg_lrs = LibVEX_Alloc_inline(sizeof(VRegLR) * n_vregs);

   for (Int j = 0; j < n_vregs; j++) {
      vreg_lrs[j].live_after     = INVALID_INSTRNO;
      vreg_lrs[j].dead_before    = INVALID_INSTRNO;
      vreg_lrs[j].spill_offset   = 0;
      vreg_lrs[j].spill_size     = 0;
      vreg_lrs[j].reg_class      = HRcINVALID;
   }

   reg_usage_arr
      = LibVEX_Alloc_inline(sizeof(HRegUsage) * instrs_in->arr_used-1);

   

   


   rreg_lrs_used = 0;
   rreg_lrs_size = 4;
   rreg_lrs_la = LibVEX_Alloc_inline(rreg_lrs_size * sizeof(RRegLR));
   rreg_lrs_db = NULL; 

   vassert(n_rregs > 0);
   rreg_live_after  = LibVEX_Alloc_inline(n_rregs * sizeof(Int));
   rreg_dead_before = LibVEX_Alloc_inline(n_rregs * sizeof(Int));

   for (Int j = 0; j < n_rregs; j++) {
      rreg_live_after[j] = 
      rreg_dead_before[j] = INVALID_INSTRNO;
   }

   

   

   for (Int ii = 0; ii < instrs_in->arr_used; ii++) {

      (*getRegUsage)( &reg_usage_arr[ii], instrs_in->arr[ii], mode64 );

      if (0) {
         vex_printf("\n%d  stage1: ", ii);
         (*ppInstr)(instrs_in->arr[ii], mode64);
         vex_printf("\n");
         ppHRegUsage(univ, &reg_usage_arr[ii]);
      }

      

      
      for (Int j = 0; j < reg_usage_arr[ii].n_vRegs; j++) {

         HReg vreg = reg_usage_arr[ii].vRegs[j];
         vassert(hregIsVirtual(vreg));

         Int k = hregIndex(vreg);
         if (k < 0 || k >= n_vregs) {
            vex_printf("\n");
            (*ppInstr)(instrs_in->arr[ii], mode64);
            vex_printf("\n");
            vex_printf("vreg %d, n_vregs %d\n", k, n_vregs);
            vpanic("doRegisterAllocation: out-of-range vreg");
         }

         if (vreg_lrs[k].reg_class == HRcINVALID) {
            
            vreg_lrs[k].reg_class = hregClass(vreg);
         } else {
            
            vassert(vreg_lrs[k].reg_class == hregClass(vreg));
         }

         
         switch (reg_usage_arr[ii].vMode[j]) {
            case HRmRead: 
               if (vreg_lrs[k].live_after == INVALID_INSTRNO) {
                  vex_printf("\n\nOFFENDING VREG = %d\n", k);
                  vpanic("doRegisterAllocation: "
                         "first event for vreg is Read");
               }
               vreg_lrs[k].dead_before = toShort(ii + 1);
               break;
            case HRmWrite:
               if (vreg_lrs[k].live_after == INVALID_INSTRNO)
                  vreg_lrs[k].live_after = toShort(ii);
               vreg_lrs[k].dead_before = toShort(ii + 1);
               break;
            case HRmModify:
               if (vreg_lrs[k].live_after == INVALID_INSTRNO) {
                  vex_printf("\n\nOFFENDING VREG = %d\n", k);
                  vpanic("doRegisterAllocation: "
                         "first event for vreg is Modify");
               }
               vreg_lrs[k].dead_before = toShort(ii + 1);
               break;
            default:
               vpanic("doRegisterAllocation(1)");
         } 

      } 

      

      

      vassert(N_RREGUNIVERSE_REGS == 64);

      const ULong rRead      = reg_usage_arr[ii].rRead;
      const ULong rWritten   = reg_usage_arr[ii].rWritten;
      const ULong rMentioned = rRead | rWritten;

      UInt rReg_minIndex;
      UInt rReg_maxIndex;
      if (rMentioned == 0) {
         rReg_minIndex = 1;
         rReg_maxIndex = 0;
      } else {
         rReg_minIndex = ULong__minIndex(rMentioned);
         rReg_maxIndex = ULong__maxIndex(rMentioned);
         if (rReg_maxIndex >= n_rregs)
            rReg_maxIndex = n_rregs-1;
      }

      
      for (Int j = rReg_minIndex; j <= rReg_maxIndex; j++) {

         const ULong jMask = 1ULL << j;
         if (LIKELY((rMentioned & jMask) == 0))
            continue;

         const Bool isR = (rRead    & jMask) != 0;
         const Bool isW = (rWritten & jMask) != 0;

         Int  flush_la = INVALID_INSTRNO, flush_db = INVALID_INSTRNO;
         Bool flush = False;

         if (isW && !isR) {
            flush_la = rreg_live_after[j];
            flush_db = rreg_dead_before[j];
            if (flush_la != INVALID_INSTRNO && flush_db != INVALID_INSTRNO)
               flush = True;
            rreg_live_after[j]  = ii;
            rreg_dead_before[j] = ii+1;
         } else if (!isW && isR) {
            if (rreg_live_after[j] == INVALID_INSTRNO) {
               vex_printf("\nOFFENDING RREG = ");
               (*ppReg)(univ->regs[j]);
               vex_printf("\n");
               vex_printf("\nOFFENDING instr = ");
               (*ppInstr)(instrs_in->arr[ii], mode64);
               vex_printf("\n");
               vpanic("doRegisterAllocation: "
                      "first event for rreg is Read");
            }
            rreg_dead_before[j] = ii+1;
         } else {
            vassert(isR && isW);
            if (rreg_live_after[j] == INVALID_INSTRNO) {
               vex_printf("\nOFFENDING RREG = ");
               (*ppReg)(univ->regs[j]);
               vex_printf("\n");
               vex_printf("\nOFFENDING instr = ");
               (*ppInstr)(instrs_in->arr[ii], mode64);
               vex_printf("\n");
               vpanic("doRegisterAllocation: "
                      "first event for rreg is Modify");
            }
            rreg_dead_before[j] = ii+1;
         }

         if (flush) {
            vassert(flush_la != INVALID_INSTRNO);
            vassert(flush_db != INVALID_INSTRNO);
            ensureRRLRspace(&rreg_lrs_la, &rreg_lrs_size, rreg_lrs_used);
            if (0) 
               vex_printf("FLUSH 1 (%d,%d)\n", flush_la, flush_db);
            rreg_lrs_la[rreg_lrs_used].rreg        = univ->regs[j];
            rreg_lrs_la[rreg_lrs_used].live_after  = toShort(flush_la);
            rreg_lrs_la[rreg_lrs_used].dead_before = toShort(flush_db);
            rreg_lrs_used++;
         }

      } 

      

   } 

   

   

   
   for (Int j = 0; j < n_rregs; j++) {

      if (0) {
         vex_printf("residual %d:  %d %d\n", j, rreg_live_after[j],
                                                rreg_dead_before[j]);
      }
      vassert( (rreg_live_after[j] == INVALID_INSTRNO 
                && rreg_dead_before[j] == INVALID_INSTRNO)
              ||
               (rreg_live_after[j] != INVALID_INSTRNO 
                && rreg_dead_before[j] != INVALID_INSTRNO)
            );

      if (rreg_live_after[j] == INVALID_INSTRNO)
         continue;

      ensureRRLRspace(&rreg_lrs_la, &rreg_lrs_size, rreg_lrs_used);
      if (0)
         vex_printf("FLUSH 2 (%d,%d)\n", 
                    rreg_live_after[j], rreg_dead_before[j]);
      rreg_lrs_la[rreg_lrs_used].rreg        = univ->regs[j];
      rreg_lrs_la[rreg_lrs_used].live_after  = toShort(rreg_live_after[j]);
      rreg_lrs_la[rreg_lrs_used].dead_before = toShort(rreg_dead_before[j]);
      rreg_lrs_used++;
   }


   for (Int j = 0; j < rreg_lrs_used; j++) {
      HReg rreg = rreg_lrs_la[j].rreg;
      vassert(!hregIsVirtual(rreg));
      UInt ix = hregIndex(rreg);
      vassert(ix < n_rregs);
      rreg_state[ix].has_hlrs = True;
   }
   if (0) {
      for (Int j = 0; j < n_rregs; j++) {
         if (!rreg_state[j].has_hlrs)
            continue;
         ppReg(univ->regs[j]);
         vex_printf(" hinted\n");
      }
   }

   rreg_lrs_db = LibVEX_Alloc_inline(rreg_lrs_used * sizeof(RRegLR));
   for (Int j = 0; j < rreg_lrs_used; j++)
      rreg_lrs_db[j] = rreg_lrs_la[j];

   sortRRLRarray( rreg_lrs_la, rreg_lrs_used, True   );
   sortRRLRarray( rreg_lrs_db, rreg_lrs_used, False );

   
   rreg_lrs_la_next = 0;
   rreg_lrs_db_next = 0;

   for (Int j = 1; j < rreg_lrs_used; j++) {
      vassert(rreg_lrs_la[j-1].live_after  <= rreg_lrs_la[j].live_after);
      vassert(rreg_lrs_db[j-1].dead_before <= rreg_lrs_db[j].dead_before);
   }

   

   if (DEBUG_REGALLOC) {
      for (Int j = 0; j < n_vregs; j++) {
         vex_printf("vreg %d:  la = %d,  db = %d\n", 
                    j, vreg_lrs[j].live_after, vreg_lrs[j].dead_before );
      }
   }

   if (DEBUG_REGALLOC) {
      vex_printf("RRegLRs by LA:\n");
      for (Int j = 0; j < rreg_lrs_used; j++) {
         vex_printf("  ");
         (*ppReg)(rreg_lrs_la[j].rreg);
         vex_printf("      la = %d,  db = %d\n",
                    rreg_lrs_la[j].live_after, rreg_lrs_la[j].dead_before );
      }
      vex_printf("RRegLRs by DB:\n");
      for (Int j = 0; j < rreg_lrs_used; j++) {
         vex_printf("  ");
         (*ppReg)(rreg_lrs_db[j].rreg);
         vex_printf("      la = %d,  db = %d\n",
                    rreg_lrs_db[j].live_after, rreg_lrs_db[j].dead_before );
      }
   }

   

   

   local_memset(ss_busy_until_before, 0, sizeof(ss_busy_until_before));

   for (Int j = 0; j < n_vregs; j++) {

      if (vreg_lrs[j].live_after == INVALID_INSTRNO) {
         vassert(vreg_lrs[j].reg_class == HRcINVALID);
         continue;
      }

      Int ss_no = -1;
      switch (vreg_lrs[j].reg_class) {

         case HRcVec128: case HRcFlt64:
            for (ss_no = 0; ss_no < N_SPILL64S-1; ss_no += 2)
               if (ss_busy_until_before[ss_no+0] <= vreg_lrs[j].live_after
                   && ss_busy_until_before[ss_no+1] <= vreg_lrs[j].live_after)
                  break;
            if (ss_no >= N_SPILL64S-1) {
               vpanic("LibVEX_N_SPILL_BYTES is too low.  " 
                      "Increase and recompile.");
            }
            ss_busy_until_before[ss_no+0] = vreg_lrs[j].dead_before;
            ss_busy_until_before[ss_no+1] = vreg_lrs[j].dead_before;
            break;

         default:
            
            for (ss_no = 0; ss_no < N_SPILL64S; ss_no++)
               if (ss_busy_until_before[ss_no] <= vreg_lrs[j].live_after)
                  break;
            if (ss_no == N_SPILL64S) {
               vpanic("LibVEX_N_SPILL_BYTES is too low.  " 
                      "Increase and recompile.");
            }
            ss_busy_until_before[ss_no] = vreg_lrs[j].dead_before;
            break;

      } 

      vreg_lrs[j].spill_offset = toShort(guest_sizeB * 3 + ss_no * 8);

      
      sanity_check_spill_offset( &vreg_lrs[j] );
      
      
   }

   if (0) {
      vex_printf("\n\n");
      for (Int j = 0; j < n_vregs; j++)
         vex_printf("vreg %d    --> spill offset %d\n",
                    j, vreg_lrs[j].spill_offset);
   }

   



   


   

   for (Int ii = 0; ii < instrs_in->arr_used; ii++) {

      if (DEBUG_REGALLOC) {
         vex_printf("\n====----====---- Insn %d ----====----====\n", ii);
         vex_printf("---- ");
         (*ppInstr)(instrs_in->arr[ii], mode64);
         vex_printf("\n\nInitial state:\n");
         PRINT_STATE;
         vex_printf("\n");
      }

      

      do_sanity_check
         = toBool(
              False 
              || ii == instrs_in->arr_used-1
              || (ii > 0 && (ii % 13) == 0)
           );

      if (do_sanity_check) {

         for (Int j = 0; j < rreg_lrs_used; j++) {
            if (rreg_lrs_la[j].live_after < ii
                && ii < rreg_lrs_la[j].dead_before) {
               HReg reg = rreg_lrs_la[j].rreg;

               if (0) {
                  vex_printf("considering la %d .. db %d   reg = ", 
                             rreg_lrs_la[j].live_after, 
                             rreg_lrs_la[j].dead_before);
                  (*ppReg)(reg);
                  vex_printf("\n");
               }

               
               vassert(!hregIsVirtual(reg));
               vassert(rreg_state[hregIndex(reg)].disp == Unavail);
            }
         }

         for (Int j = 0; j < n_rregs; j++) {
            vassert(rreg_state[j].disp == Bound
                    || rreg_state[j].disp == Free
                    || rreg_state[j].disp == Unavail);
            if (rreg_state[j].disp != Unavail)
               continue;
            Int k;
            for (k = 0; k < rreg_lrs_used; k++) {
               HReg reg = rreg_lrs_la[k].rreg;
               vassert(!hregIsVirtual(reg));
               if (hregIndex(reg) == j
                   && rreg_lrs_la[k].live_after < ii 
                   && ii < rreg_lrs_la[k].dead_before) 
                  break;
            }
            vassert(k < rreg_lrs_used);
         }

         for (Int j = 0; j < n_rregs; j++) {
            if (rreg_state[j].disp != Bound) {
               vassert(rreg_state[j].eq_spill_slot == False);
               continue;
            }
            vassert(hregClass(univ->regs[j]) 
                    == hregClass(rreg_state[j].vreg));
            vassert( hregIsVirtual(rreg_state[j].vreg));
         }

         for (Int j = 0; j < n_rregs; j++) {
            if (rreg_state[j].disp != Bound)
               continue;
            Int k = hregIndex(rreg_state[j].vreg);
            vassert(IS_VALID_VREGNO(k));
            vassert(vreg_state[k] == j);
         }
         for (Int j = 0; j < n_vregs; j++) {
            Int k = vreg_state[j];
            if (k == INVALID_RREG_NO)
               continue;
            vassert(IS_VALID_RREGNO(k));
            vassert(rreg_state[k].disp == Bound);
            vassert(hregIndex(rreg_state[k].vreg) == j);
         }

      } 

      

      HReg vregS = INVALID_HREG;
      HReg vregD = INVALID_HREG;
      if ( (*isMove)( instrs_in->arr[ii], &vregS, &vregD ) ) {
         if (!hregIsVirtual(vregS)) goto cannot_coalesce;
         if (!hregIsVirtual(vregD)) goto cannot_coalesce;
         
         vassert(hregClass(vregS) == hregClass(vregD));
         Int k = hregIndex(vregS);
         Int m = hregIndex(vregD);
         vassert(IS_VALID_VREGNO(k));
         vassert(IS_VALID_VREGNO(m));
         if (vreg_lrs[k].dead_before != ii + 1) goto cannot_coalesce;
         if (vreg_lrs[m].live_after != ii) goto cannot_coalesce;
         if (DEBUG_REGALLOC) {
         vex_printf("COALESCE ");
            (*ppReg)(vregS);
            vex_printf(" -> ");
            (*ppReg)(vregD);
            vex_printf("\n\n");
         }
         
         Int n = vreg_state[k]; 
         if (n == INVALID_RREG_NO) {
            goto cannot_coalesce;
         }
         vassert(IS_VALID_RREGNO(n));

         rreg_state[n].vreg = vregD;
         vassert(IS_VALID_VREGNO(hregIndex(vregD)));
         vassert(IS_VALID_VREGNO(hregIndex(vregS)));
         vreg_state[hregIndex(vregD)] = toShort(n);
         vreg_state[hregIndex(vregS)] = INVALID_RREG_NO;

         rreg_state[n].eq_spill_slot = False;

         continue;
      }
     cannot_coalesce:

      


      for (Int j = 0; j < n_rregs; j++) {
         if (rreg_state[j].disp != Bound)
            continue;
         UInt vregno = hregIndex(rreg_state[j].vreg);
         vassert(IS_VALID_VREGNO(vregno));
         if (vreg_lrs[vregno].dead_before <= ii) {
            rreg_state[j].disp = Free;
            rreg_state[j].eq_spill_slot = False;
            Int m = hregIndex(rreg_state[j].vreg);
            vassert(IS_VALID_VREGNO(m));
            vreg_state[m] = INVALID_RREG_NO;
            if (DEBUG_REGALLOC) {
               vex_printf("free up "); 
               (*ppReg)(univ->regs[j]); 
               vex_printf("\n");
            }
         }
      }

      

      while (True) {
         vassert(rreg_lrs_la_next >= 0);
         vassert(rreg_lrs_la_next <= rreg_lrs_used);
         if (rreg_lrs_la_next == rreg_lrs_used)
            break; 
         if (ii < rreg_lrs_la[rreg_lrs_la_next].live_after)
            break; 
         vassert(ii == rreg_lrs_la[rreg_lrs_la_next].live_after);
         if (DEBUG_REGALLOC) {
            vex_printf("need to free up rreg: ");
            (*ppReg)(rreg_lrs_la[rreg_lrs_la_next].rreg);
            vex_printf("\n\n");
         }
         Int k = hregIndex(rreg_lrs_la[rreg_lrs_la_next].rreg);

         vassert(IS_VALID_RREGNO(k));
         Int m = hregIndex(rreg_state[k].vreg);
         if (rreg_state[k].disp == Bound) {
            vassert(IS_VALID_VREGNO(m));
            vreg_state[m] = INVALID_RREG_NO;
            if (vreg_lrs[m].dead_before > ii) {
               vassert(vreg_lrs[m].reg_class != HRcINVALID);
               if ((!eq_spill_opt) || !rreg_state[k].eq_spill_slot) {
                  HInstr* spill1 = NULL;
                  HInstr* spill2 = NULL;
                  (*genSpill)( &spill1, &spill2, univ->regs[k],
                               vreg_lrs[m].spill_offset, mode64 );
                  vassert(spill1 || spill2); 
                  if (spill1)
                     EMIT_INSTR(spill1);
                  if (spill2)
                     EMIT_INSTR(spill2);
               }
               rreg_state[k].eq_spill_slot = True;
            }
         }
         rreg_state[k].disp = Unavail;
         rreg_state[k].vreg = INVALID_HREG;
         rreg_state[k].eq_spill_slot = False;

         
         rreg_lrs_la_next++;
      }

      if (DEBUG_REGALLOC) {
         vex_printf("After pre-insn actions for fixed regs:\n");
         PRINT_STATE;
         vex_printf("\n");
      }

      


      initHRegRemap(&remap);

      

      
      if (directReload && reg_usage_arr[ii].n_vRegs <= 2) {
         Bool  debug_direct_reload = False;
         HReg  cand     = INVALID_HREG;
         Bool  nreads   = 0;
         Short spilloff = 0;

         for (Int j = 0; j < reg_usage_arr[ii].n_vRegs; j++) {

            HReg vreg = reg_usage_arr[ii].vRegs[j];
            vassert(hregIsVirtual(vreg));

            if (reg_usage_arr[ii].vMode[j] == HRmRead) {
               nreads++;
               Int m = hregIndex(vreg);
               vassert(IS_VALID_VREGNO(m));
               Int k = vreg_state[m];
               if (!IS_VALID_RREGNO(k)) {
                  
                  vassert(vreg_lrs[m].dead_before >= ii+1);
                  if (vreg_lrs[m].dead_before == ii+1
                      && hregIsInvalid(cand)) {
                     spilloff = vreg_lrs[m].spill_offset;
                     cand = vreg;
                  }
               }
            }
         }

         if (nreads == 1 && ! hregIsInvalid(cand)) {
            HInstr* reloaded;
            if (reg_usage_arr[ii].n_vRegs == 2)
               vassert(! sameHReg(reg_usage_arr[ii].vRegs[0],
                                  reg_usage_arr[ii].vRegs[1]));

            reloaded = directReload ( instrs_in->arr[ii], cand, spilloff );
            if (debug_direct_reload && !reloaded) {
               vex_printf("[%3d] ", spilloff); ppHReg(cand); vex_printf(" "); 
               ppInstr(instrs_in->arr[ii], mode64); 
            }
            if (reloaded) {
               instrs_in->arr[ii] = reloaded;
               (*getRegUsage)( &reg_usage_arr[ii], instrs_in->arr[ii], mode64 );
               if (debug_direct_reload && !reloaded) {
                  vex_printf("  -->  ");
                  ppInstr(reloaded, mode64);
               }
            }

            if (debug_direct_reload && !reloaded)
               vex_printf("\n");
         }

      }

      

      
      for (Int j = 0; j < reg_usage_arr[ii].n_vRegs; j++) {

         HReg vreg = reg_usage_arr[ii].vRegs[j];
         vassert(hregIsVirtual(vreg));

         if (0) {
            vex_printf("considering "); (*ppReg)(vreg); vex_printf("\n");
         }

         Int m = hregIndex(vreg);
         vassert(IS_VALID_VREGNO(m));
         Int n = vreg_state[m];
         if (IS_VALID_RREGNO(n)) {
            vassert(rreg_state[n].disp == Bound);
            addToHRegRemap(&remap, vreg, univ->regs[n]);
            /* If this rreg is written or modified, mark it as different
               from any spill slot value. */
            if (reg_usage_arr[ii].vMode[j] != HRmRead)
               rreg_state[n].eq_spill_slot = False;
            continue;
         } else {
            vassert(n == INVALID_RREG_NO);
         }

         Int k_suboptimal = -1;
         Int k;
         for (k = 0; k < n_rregs; k++) {
            if (rreg_state[k].disp != Free
                || hregClass(univ->regs[k]) != hregClass(vreg))
               continue;
            if (rreg_state[k].has_hlrs) {
               k_suboptimal = k;
            } else {
               
               k_suboptimal = -1;
               break;
            }
         }
         if (k_suboptimal >= 0)
            k = k_suboptimal;

         if (k < n_rregs) {
            rreg_state[k].disp = Bound;
            rreg_state[k].vreg = vreg;
            Int p = hregIndex(vreg);
            vassert(IS_VALID_VREGNO(p));
            vreg_state[p] = toShort(k);
            addToHRegRemap(&remap, vreg, univ->regs[k]);
            if (reg_usage_arr[ii].vMode[j] != HRmWrite) {
               vassert(vreg_lrs[p].reg_class != HRcINVALID);
               HInstr* reload1 = NULL;
               HInstr* reload2 = NULL;
               (*genReload)( &reload1, &reload2, univ->regs[k],
                             vreg_lrs[p].spill_offset, mode64 );
               vassert(reload1 || reload2); 
               if (reload1)
                  EMIT_INSTR(reload1);
               if (reload2)
                  EMIT_INSTR(reload2);
               if (reg_usage_arr[ii].vMode[j] == HRmRead) {
                  rreg_state[k].eq_spill_slot = True;
               } else {
                  vassert(reg_usage_arr[ii].vMode[j] == HRmModify);
                  rreg_state[k].eq_spill_slot = False;
               }
            } else {
               rreg_state[k].eq_spill_slot = False;
            }

            continue;
         }


         for (k = 0; k < n_rregs; k++) {
            rreg_state[k].is_spill_cand = False;
            if (rreg_state[k].disp != Bound)
               continue;
            if (hregClass(univ->regs[k]) != hregClass(vreg))
               continue;
            rreg_state[k].is_spill_cand = True;
            for (m = 0; m < reg_usage_arr[ii].n_vRegs; m++) {
               if (sameHReg(rreg_state[k].vreg, reg_usage_arr[ii].vRegs[m])) {
                  rreg_state[k].is_spill_cand = False;
                  break;
               }
            }
         }

         Int spillee
            = findMostDistantlyMentionedVReg ( 
                 reg_usage_arr, ii+1, instrs_in->arr_used, rreg_state, n_rregs );

         if (spillee == -1) {
            vex_printf("reg_alloc: can't find a register in class: ");
            ppHRegClass(hregClass(vreg));
            vex_printf("\n");
            vpanic("reg_alloc: can't create a free register.");
         }

         
         vassert(IS_VALID_RREGNO(spillee));
         vassert(rreg_state[spillee].disp == Bound);
         
         vassert(hregClass(univ->regs[spillee]) == hregClass(vreg));
         vassert(! sameHReg(rreg_state[spillee].vreg, vreg));

         m = hregIndex(rreg_state[spillee].vreg);
         vassert(IS_VALID_VREGNO(m));

         vassert(vreg_lrs[m].dead_before > ii);
         vassert(vreg_lrs[m].reg_class != HRcINVALID);
         if ((!eq_spill_opt) || !rreg_state[spillee].eq_spill_slot) {
            HInstr* spill1 = NULL;
            HInstr* spill2 = NULL;
            (*genSpill)( &spill1, &spill2, univ->regs[spillee],
                         vreg_lrs[m].spill_offset, mode64 );
            vassert(spill1 || spill2); 
            if (spill1)
               EMIT_INSTR(spill1);
            if (spill2)
               EMIT_INSTR(spill2);
         }

         rreg_state[spillee].vreg = vreg;
         vreg_state[m] = INVALID_RREG_NO;

         rreg_state[spillee].eq_spill_slot = False; 

         m = hregIndex(vreg);
         vassert(IS_VALID_VREGNO(m));
         vreg_state[m] = toShort(spillee);

         /* Now, if this vreg is being read or modified (as opposed to
            written), we have to generate a reload for it. */
         if (reg_usage_arr[ii].vMode[j] != HRmWrite) {
            vassert(vreg_lrs[m].reg_class != HRcINVALID);
            HInstr* reload1 = NULL;
            HInstr* reload2 = NULL;
            (*genReload)( &reload1, &reload2, univ->regs[spillee],
                          vreg_lrs[m].spill_offset, mode64 );
            vassert(reload1 || reload2); 
            if (reload1)
               EMIT_INSTR(reload1);
            if (reload2)
               EMIT_INSTR(reload2);
            if (reg_usage_arr[ii].vMode[j] == HRmRead) {
               rreg_state[spillee].eq_spill_slot = True;
            } else {
               vassert(reg_usage_arr[ii].vMode[j] == HRmModify);
               rreg_state[spillee].eq_spill_slot = False;
            }
         }

         addToHRegRemap(&remap, vreg, univ->regs[spillee]);

      } 


      
      (*mapRegs)( &remap, instrs_in->arr[ii], mode64 );
      EMIT_INSTR( instrs_in->arr[ii] );

      if (DEBUG_REGALLOC) {
         vex_printf("After dealing with current insn:\n");
         PRINT_STATE;
         vex_printf("\n");
      }

      

      while (True) {
         vassert(rreg_lrs_db_next >= 0);
         vassert(rreg_lrs_db_next <= rreg_lrs_used);
         if (rreg_lrs_db_next == rreg_lrs_used)
            break; 
         if (ii+1 < rreg_lrs_db[rreg_lrs_db_next].dead_before)
            break; 
         vassert(ii+1 == rreg_lrs_db[rreg_lrs_db_next].dead_before);
         HReg reg = rreg_lrs_db[rreg_lrs_db_next].rreg;
         vassert(!hregIsVirtual(reg));
         Int k = hregIndex(reg);
         vassert(IS_VALID_RREGNO(k));
         vassert(rreg_state[k].disp == Unavail);
         rreg_state[k].disp = Free;
         rreg_state[k].vreg = INVALID_HREG;
         rreg_state[k].eq_spill_slot = False;

         
         rreg_lrs_db_next++;
      }

      if (DEBUG_REGALLOC) {
         vex_printf("After post-insn actions for fixed regs:\n");
         PRINT_STATE;
         vex_printf("\n");
      }

   } 

   

   
   
   

   
   vassert(rreg_lrs_la_next == rreg_lrs_used);
   vassert(rreg_lrs_db_next == rreg_lrs_used);

   return instrs_out;

#  undef INVALID_INSTRNO
#  undef EMIT_INSTR
#  undef PRINT_STATE
}



