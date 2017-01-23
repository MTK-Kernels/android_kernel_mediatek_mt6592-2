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


#ifndef __SEGMENT_H
#define __SEGMENT_H




#include "drd_vc.h"
#include "pub_drd_bitmap.h"
#include "pub_tool_execontext.h" 
#include "pub_tool_stacktrace.h" 


typedef struct segment
{
   struct segment*    g_next;
   struct segment*    g_prev;
   
   struct segment*    thr_next;
   struct segment*    thr_prev;
   DrdThreadId        tid;
   
   int                refcnt;
   
   ExeContext*        stacktrace;
   
   VectorClock        vc;
   struct bitmap      bm;
} Segment;

extern Segment* DRD_(g_sg_list);

Segment* DRD_(sg_new)(const DrdThreadId creator, const DrdThreadId created);
static int DRD_(sg_get_refcnt)(const Segment* const sg);
Segment* DRD_(sg_get)(Segment* const sg);
void DRD_(sg_put)(Segment* const sg);
static struct bitmap* DRD_(sg_bm)(Segment* const sg);
void DRD_(sg_merge)(Segment* const sg1, Segment* const sg2);
void DRD_(sg_print)(Segment* const sg);
Bool DRD_(sg_get_trace)(void);
void DRD_(sg_set_trace)(const Bool trace_segment);
ULong DRD_(sg_get_segments_created_count)(void);
ULong DRD_(sg_get_segments_alive_count)(void);
ULong DRD_(sg_get_max_segments_alive_count)(void);
ULong DRD_(sg_get_segment_merge_count)(void);


static __inline__ int DRD_(sg_get_refcnt)(const Segment* const sg)
{
#ifdef ENABLE_DRD_CONSISTENCY_CHECKS
   tl_assert(sg);
#endif

   return sg->refcnt;
}

static __inline__ struct bitmap* DRD_(sg_bm)(Segment* const sg)
{
#ifdef ENABLE_DRD_CONSISTENCY_CHECKS
   tl_assert(sg);
#endif

   return &sg->bm;
}



#endif 
