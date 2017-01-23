

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

#ifndef __VEX_HOST_GENERIC_REGS_H
#define __VEX_HOST_GENERIC_REGS_H

#include "libvex_basictypes.h"




typedef  struct { UInt u32; }  HReg;

typedef
   enum { 
      HRcINVALID=1,   
      HRcInt32=3,     
      HRcInt64=4,     
      HRcFlt32=5,     
      HRcFlt64=6,     
      HRcVec64=7,     
      HRcVec128=8     
   }
   HRegClass;

extern void ppHRegClass ( HRegClass );


extern void ppHReg ( HReg );

static inline HReg mkHReg ( Bool virtual, HRegClass rc, UInt enc, UInt ix )
{
   vassert(ix <= 0xFFFFF);
   vassert(enc <= 0x7F);
   vassert(((UInt)rc) <= 0xF);
   vassert(((UInt)virtual) <= 1);
   if (virtual) vassert(enc == 0);
   HReg r;
   r.u32 = ((((UInt)virtual) & 1)       << 31)  |
           ((((UInt)rc)      & 0xF)     << 27)  |
           ((((UInt)enc)     & 0x7F)    << 20)  |
           ((((UInt)ix)      & 0xFFFFF) << 0);
   return r;
}

static inline HRegClass hregClass ( HReg r )
{
   HRegClass rc = (HRegClass)((r.u32 >> 27) & 0xF);
   vassert(rc >= HRcInt32 && rc <= HRcVec128);
   return rc;
}

static inline UInt hregIndex ( HReg r )
{
   return r.u32 & 0xFFFFF;
}

static inline UInt hregEncoding ( HReg r )
{
   return (r.u32 >> 20) & 0x7F;
}

static inline Bool hregIsVirtual ( HReg r )
{
   return toBool((r.u32 >> 31) & 1);
}

static inline Bool sameHReg ( HReg r1, HReg r2 )
{
   return toBool(r1.u32 == r2.u32);
}

static const HReg INVALID_HREG = { .u32 = 0xFFFFFFFF };

static inline Bool hregIsInvalid ( HReg r )
{
   return sameHReg(r, INVALID_HREG);
}




#define N_RREGUNIVERSE_REGS 64

typedef
   struct {
      
      UInt size;
      
      UInt allocable;
      HReg regs[N_RREGUNIVERSE_REGS];
   }
   RRegUniverse;

void RRegUniverse__init ( RRegUniverse* );

void RRegUniverse__check_is_sane ( const RRegUniverse* );

void RRegUniverse__show ( const RRegUniverse* );



typedef
   struct {
      ULong         bitset;
      RRegUniverse* univ;
   }
   RRegSet;



typedef
   enum { HRmRead, HRmWrite, HRmModify }
   HRegMode;


#define N_HREGUSAGE_VREGS 5

typedef
   struct {
      ULong    rRead;     
      ULong    rWritten;  /* real regs that are written */
      
      HReg     vRegs[N_HREGUSAGE_VREGS];
      HRegMode vMode[N_HREGUSAGE_VREGS];
      UInt     n_vRegs;
   }
   HRegUsage;

extern void ppHRegUsage ( const RRegUniverse*, HRegUsage* );

static inline void initHRegUsage ( HRegUsage* tab )
{
   tab->rRead    = 0;
   tab->rWritten = 0;
   tab->n_vRegs  = 0;
}

extern void addHRegUse ( HRegUsage*, HRegMode, HReg );

extern Bool HRegUsage__contains ( const HRegUsage*, HReg );




#define N_HREG_REMAP 6

typedef
   struct {
      HReg orig       [N_HREG_REMAP];
      HReg replacement[N_HREG_REMAP];
      Int  n_used;
   }
   HRegRemap;

extern void ppHRegRemap     ( HRegRemap* );
extern void addToHRegRemap  ( HRegRemap*, HReg, HReg );
extern HReg lookupHRegRemap ( HRegRemap*, HReg );

static inline void initHRegRemap ( HRegRemap* map )
{
   map->n_used = 0;
}




typedef  void  HInstr;


typedef
   struct {
      HInstr** arr;
      Int      arr_size;
      Int      arr_used;
      Int      n_vregs;
   }
   HInstrArray;

extern HInstrArray* newHInstrArray ( void );

__attribute__((noinline))
extern void addHInstr_SLOW ( HInstrArray*, HInstr* );

static inline void addHInstr ( HInstrArray* ha, HInstr* instr )
{
   if (LIKELY(ha->arr_used < ha->arr_size)) {
      ha->arr[ha->arr_used] = instr;
      ha->arr_used++;
   } else {
      addHInstr_SLOW(ha, instr);
   }
}




typedef
   enum {
      RLPri_INVALID,   
      RLPri_None,      
      RLPri_Int,       
      RLPri_2Int,      
      RLPri_V128SpRel, 
      RLPri_V256SpRel  
   }
   RetLocPrimary;

typedef
   struct {
      
      RetLocPrimary pri;
      Int spOff;
   }
   RetLoc;

extern void ppRetLoc ( RetLoc rloc );

static inline RetLoc mk_RetLoc_simple ( RetLocPrimary pri ) {
   vassert(pri >= RLPri_INVALID && pri <= RLPri_2Int);
   return (RetLoc){pri, 0};
}

static inline RetLoc mk_RetLoc_spRel ( RetLocPrimary pri, Int off ) {
   vassert(pri >= RLPri_V128SpRel && pri <= RLPri_V256SpRel);
   return (RetLoc){pri, off};
}

static inline Bool is_sane_RetLoc ( RetLoc rloc ) {
   switch (rloc.pri) {
      case RLPri_None: case RLPri_Int: case RLPri_2Int:
         return rloc.spOff == 0;
      case RLPri_V128SpRel: case RLPri_V256SpRel:
         return True;
      default:
         return False;
   }
}

static inline RetLoc mk_RetLoc_INVALID ( void ) {
   return (RetLoc){RLPri_INVALID, 0};
}

static inline Bool is_RetLoc_INVALID ( RetLoc rl ) {
   return rl.pri == RLPri_INVALID && rl.spOff == 0;
}



extern
HInstrArray* doRegisterAllocation (

    
   HInstrArray* instrs_in,

   const RRegUniverse* univ,

   Bool (*isMove) (const HInstr*, HReg*, HReg*),

   
   void (*getRegUsage) (HRegUsage*, const HInstr*, Bool),

   
   void (*mapRegs) (HRegRemap*, HInstr*, Bool),

   void    (*genSpill) (  HInstr**, HInstr**, HReg, Int, Bool ),
   void    (*genReload) ( HInstr**, HInstr**, HReg, Int, Bool ),
   HInstr* (*directReload) ( HInstr*, HReg, Short ),
   Int     guest_sizeB,

   
   void (*ppInstr) ( const HInstr*, Bool ),
   void (*ppReg) ( HReg ),

   
   Bool mode64
);


#endif 

