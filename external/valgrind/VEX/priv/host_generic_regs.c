

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



void ppHRegClass ( HRegClass hrc )
{
   switch (hrc) {
      case HRcInt32:   vex_printf("HRcInt32"); break;
      case HRcInt64:   vex_printf("HRcInt64"); break;
      case HRcFlt32:   vex_printf("HRcFlt32"); break;
      case HRcFlt64:   vex_printf("HRcFlt64"); break;
      case HRcVec64:   vex_printf("HRcVec64"); break;
      case HRcVec128:  vex_printf("HRcVec128"); break;
      default: vpanic("ppHRegClass");
   }
}

void ppHReg ( HReg r ) 
{
   if (hregIsInvalid(r)) {
      vex_printf("HReg_INVALID");
      return;
   }
   const Bool   isV     = hregIsVirtual(r);
   const HChar* maybe_v = isV ? "v" : "";
   const UInt   regNN   = isV ? hregIndex(r) : hregEncoding(r);
   switch (hregClass(r)) {
      case HRcInt32:   vex_printf("%%%sr%u", maybe_v, regNN); return;
      case HRcInt64:   vex_printf("%%%sR%u", maybe_v, regNN); return;
      case HRcFlt32:   vex_printf("%%%sF%u", maybe_v, regNN); return;
      case HRcFlt64:   vex_printf("%%%sD%u", maybe_v, regNN); return;
      case HRcVec64:   vex_printf("%%%sv%u", maybe_v, regNN); return;
      case HRcVec128:  vex_printf("%%%sV%u", maybe_v, regNN); return;
      default: vpanic("ppHReg");
   }
}



void RRegUniverse__init ( RRegUniverse* univ )
{
   *univ = (RRegUniverse){};
   univ->size      = 0;
   univ->allocable = 0;
   for (UInt i = 0; i < N_RREGUNIVERSE_REGS; i++) {
      univ->regs[i] = INVALID_HREG;
   }
}

void RRegUniverse__check_is_sane ( const RRegUniverse* univ )
{
   vassert(univ->size > 0);
   vassert(univ->size <= N_RREGUNIVERSE_REGS);
   vassert(univ->allocable <= univ->size);
   for (UInt i = 0; i < univ->size; i++) {
      HReg reg = univ->regs[i];
      vassert(!hregIsInvalid(reg));
      vassert(!hregIsVirtual(reg));
      vassert(hregIndex(reg) == i);
   }
   for (UInt i = univ->size; i < N_RREGUNIVERSE_REGS; i++) {
      HReg reg = univ->regs[i];
      vassert(hregIsInvalid(reg));
   }
}



void ppHRegUsage ( const RRegUniverse* univ, HRegUsage* tab )
{
   vassert(N_RREGUNIVERSE_REGS == 64);

   vex_printf("HRegUsage {\n");
   
   for (UInt i = 0; i < N_RREGUNIVERSE_REGS; i++) {
      Bool rRd = (tab->rRead    & (1ULL << i)) != 0;
      Bool rWr = (tab->rWritten & (1ULL << i)) != 0;
      const HChar* str = "Modify ";
       if (!rRd && !rWr) { continue; }
      else if ( rRd && !rWr) { str = "Read   "; }
      else if (!rRd &&  rWr) { str = "Write  "; }
      
      vex_printf("   %s ", str);
      ppHReg(univ->regs[i]);
      vex_printf("\n");
   }
   
   for (UInt i = 0; i < tab->n_vRegs; i++) {
      const HChar* str = NULL;
      switch (tab->vMode[i]) {
         case HRmRead:   str = "Read   "; break;
         case HRmWrite:  str = "Write  "; break;
         case HRmModify: str = "Modify "; break;
         default: vpanic("ppHRegUsage");
      }
      vex_printf("   %s ", str);
      ppHReg(tab->vRegs[i]);
      vex_printf("\n");
   }
   vex_printf("}\n");
}


void addHRegUse ( HRegUsage* tab, HRegMode mode, HReg reg )
{
   if (LIKELY(hregIsVirtual(reg))) {
      
      UInt i;
      
      for (i = 0; i < tab->n_vRegs; i++)
         if (sameHReg(tab->vRegs[i], reg))
            break;
      if (i == tab->n_vRegs) {
         
         vassert(tab->n_vRegs < N_HREGUSAGE_VREGS);
         tab->vRegs[tab->n_vRegs] = reg;
         tab->vMode[tab->n_vRegs] = mode;
         tab->n_vRegs++;
      } else {
         
         if (tab->vMode[i] == mode) {
            
         } else {
            tab->vMode[i] = HRmModify;
         }
      }
   } else {
      
      UInt ix = hregIndex(reg);
      vassert(ix < N_RREGUNIVERSE_REGS);
      ULong mask = 1ULL << ix;
      switch (mode) {
         case HRmRead:   tab->rRead |= mask; break;
         case HRmWrite:  tab->rWritten |= mask; break;
         case HRmModify: tab->rRead |= mask; tab->rWritten |= mask; break;
         default: vassert(0);
      }
   }
}

Bool HRegUsage__contains ( const HRegUsage* tab, HReg reg )
{
   vassert(!hregIsInvalid(reg));
   if (hregIsVirtual(reg)) {
      for (UInt i = 0; i < tab->n_vRegs; i++) {
         if (sameHReg(reg, tab->vRegs[i]))
            return True;
      }
      return False;
   } else {
      UInt ix = hregIndex(reg);
      vassert(ix < N_RREGUNIVERSE_REGS);
      ULong mentioned = tab->rRead | tab->rWritten;
      return (mentioned & (1ULL << ix)) != 0;
   }
   
}



void ppHRegRemap ( HRegRemap* map )
{
   Int   i;
   vex_printf("HRegRemap {\n");
   for (i = 0; i < map->n_used; i++) {
      vex_printf("   ");
      ppHReg(map->orig[i]);
      vex_printf("  -->  ");
      ppHReg(map->replacement[i]);
      vex_printf("\n");
   }
   vex_printf("}\n");
}


void addToHRegRemap ( HRegRemap* map, HReg orig, HReg replacement )
{
   Int i;
   for (i = 0; i < map->n_used; i++)
      if (sameHReg(map->orig[i], orig))
         vpanic("addToHRegMap: duplicate entry");
   if (!hregIsVirtual(orig))
      vpanic("addToHRegMap: orig is not a vreg");
   if (hregIsVirtual(replacement))
      vpanic("addToHRegMap: replacement is a vreg");

   vassert(map->n_used+1 < N_HREG_REMAP);
   map->orig[map->n_used]        = orig;
   map->replacement[map->n_used] = replacement;
   map->n_used++;
}


HReg lookupHRegRemap ( HRegRemap* map, HReg orig )
{
   Int i;
   if (!hregIsVirtual(orig))
      return orig;
   for (i = 0; i < map->n_used; i++)
      if (sameHReg(map->orig[i], orig))
         return map->replacement[i];
   vpanic("lookupHRegRemap: not found");
}



HInstrArray* newHInstrArray ( void )
{
   HInstrArray* ha = LibVEX_Alloc_inline(sizeof(HInstrArray));
   ha->arr_size = 4;
   ha->arr_used = 0;
   ha->arr      = LibVEX_Alloc_inline(ha->arr_size * sizeof(HInstr*));
   ha->n_vregs  = 0;
   return ha;
}

__attribute__((noinline))
void addHInstr_SLOW ( HInstrArray* ha, HInstr* instr )
{
   vassert(ha->arr_used == ha->arr_size);
   Int      i;
   HInstr** arr2 = LibVEX_Alloc_inline(ha->arr_size * 2 * sizeof(HInstr*));
   for (i = 0; i < ha->arr_size; i++) {
      arr2[i] = ha->arr[i];
   }
   ha->arr_size *= 2;
   ha->arr = arr2;
   addHInstr(ha, instr);
}



void ppRetLoc ( RetLoc ska )
{
   switch (ska.pri) {
      case RLPri_INVALID:
         vex_printf("RLPri_INVALID"); return;
      case RLPri_None:
         vex_printf("RLPri_None");    return;
      case RLPri_Int:
         vex_printf("RLPri_Int");     return;
      case RLPri_2Int:
         vex_printf("RLPri_2Int");    return;
      case RLPri_V128SpRel:
         vex_printf("RLPri_V128SpRel(%d)", ska.spOff); return;
      case RLPri_V256SpRel:
         vex_printf("RLPri_V256SpRel(%d)", ska.spOff); return;
      default:
         vpanic("ppRetLoc");
   }
}


