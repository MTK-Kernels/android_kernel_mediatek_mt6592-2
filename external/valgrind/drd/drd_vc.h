/*
  This file is part of drd, a thread error detector.

  Copyright (C) 2006-2013 Bart Van Assche <bvanassche@acm.org>.

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


#ifndef __DRD_VC_H
#define __DRD_VC_H




#include "pub_tool_basics.h"     
#include "drd_basics.h"          
#include "pub_tool_libcassert.h" 


#define VC_PREALLOCATED 8


typedef struct
{
   DrdThreadId threadid;
   UInt        count;
} VCElem;

typedef struct
{
   unsigned capacity; 
   unsigned size;     
   VCElem*  vc;       
   VCElem   preallocated[VC_PREALLOCATED];
} VectorClock;


void DRD_(vc_init)(VectorClock* const vc,
                   const VCElem* const vcelem,
                   const unsigned size);
void DRD_(vc_cleanup)(VectorClock* const vc);
void DRD_(vc_copy)(VectorClock* const new, const VectorClock* const rhs);
void DRD_(vc_assign)(VectorClock* const lhs, const VectorClock* const rhs);
void DRD_(vc_increment)(VectorClock* const vc, DrdThreadId const tid);
static __inline__
Bool DRD_(vc_lte)(const VectorClock* const vc1,
                  const VectorClock* const vc2);
Bool DRD_(vc_ordered)(const VectorClock* const vc1,
                      const VectorClock* const vc2);
void DRD_(vc_min)(VectorClock* const result,
                  const VectorClock* const rhs);
void DRD_(vc_combine)(VectorClock* const result,
                      const VectorClock* const rhs);
void DRD_(vc_print)(const VectorClock* const vc);
HChar* DRD_(vc_aprint)(const VectorClock* const vc);
void DRD_(vc_check)(const VectorClock* const vc);
void DRD_(vc_test)(void);



static __inline__
Bool DRD_(vc_lte)(const VectorClock* const vc1, const VectorClock* const vc2)
{
   unsigned i;
   unsigned j = 0;

   for (i = 0; i < vc1->size; i++)
   {
      while (j < vc2->size && vc2->vc[j].threadid < vc1->vc[i].threadid)
         j++;
      if (j >= vc2->size || vc2->vc[j].threadid > vc1->vc[i].threadid)
         return False;
#ifdef ENABLE_DRD_CONSISTENCY_CHECKS
      tl_assert(j < vc2->size && vc2->vc[j].threadid == vc1->vc[i].threadid);
#endif
      if (vc1->vc[i].count > vc2->vc[j].count)
         return False;
   }
   return True;
}


#endif 
