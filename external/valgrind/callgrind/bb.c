
/*
   This file is part of Callgrind, a Valgrind tool for call tracing.

   Copyright (C) 2002-2013, Josef Weidendorfer (Josef.Weidendorfer@gmx.de)

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

#include "global.h"


bb_hash bbs;

void CLG_(init_bb_hash)()
{
   Int i;

   bbs.size    = 8437;
   bbs.entries = 0;
   bbs.table = (BB**) CLG_MALLOC("cl.bb.ibh.1",
                                 bbs.size * sizeof(BB*));

   for (i = 0; i < bbs.size; i++) bbs.table[i] = NULL;
}

bb_hash* CLG_(get_bb_hash)()
{
  return &bbs;
}

static __inline__
UInt bb_hash_idx(obj_node* obj, PtrdiffT offset, UInt size)
{
  return (((Addr)obj) + offset) % size;
}

static
void resize_bb_table(void)
{
    Int i, new_size, conflicts1 = 0, conflicts2 = 0;
    BB **new_table, *curr, *next;
    UInt new_idx;

    new_size  = 2* bbs.size +3;
    new_table = (BB**) CLG_MALLOC("cl.bb.rbt.1",
                                  new_size * sizeof(BB*));
 
    for (i = 0; i < new_size; i++)
      new_table[i] = NULL;
 
    for (i = 0; i < bbs.size; i++) {
	if (bbs.table[i] == NULL) continue;
 
	curr = bbs.table[i];
	while (NULL != curr) {
	    next = curr->next;

	    new_idx = bb_hash_idx(curr->obj, curr->offset, new_size);

	    curr->next = new_table[new_idx];
	    new_table[new_idx] = curr;
	    if (curr->next) {
		conflicts1++;
		if (curr->next->next)
		    conflicts2++;
	    }

	    curr = next;
	}
    }

    VG_(free)(bbs.table);


    CLG_DEBUG(0, "Resize BB Hash: %d => %d (entries %d, conflicts %d/%d)\n",
	     bbs.size, new_size,
	     bbs.entries, conflicts1, conflicts2);

    bbs.size  = new_size;
    bbs.table = new_table;
    CLG_(stat).bb_hash_resizes++;
}


static BB* new_bb(obj_node* obj, PtrdiffT offset,
		  UInt instr_count, UInt cjmp_count, Bool cjmp_inverted)
{
   BB* bb;
   UInt idx, size;

   
   bbs.entries++;
   if (10 * bbs.entries / bbs.size > 8)
       resize_bb_table();

   size = sizeof(BB) + instr_count * sizeof(InstrInfo)
                     + (cjmp_count+1) * sizeof(CJmpInfo);
   bb = (BB*) CLG_MALLOC("cl.bb.nb.1", size);
   VG_(memset)(bb, 0, size);

   bb->obj        = obj;
   bb->offset     = offset;
   
   bb->instr_count = instr_count;
   bb->cjmp_count  = cjmp_count;
   bb->cjmp_inverted = cjmp_inverted;
   bb->jmp         = (CJmpInfo*) &(bb->instr[instr_count]);
   bb->instr_len   = 0;
   bb->cost_count  = 0;
   bb->sect_kind   = VG_(DebugInfo_sect_kind)(NULL, offset + obj->offset);
   bb->fn          = 0;
   bb->line        = 0;
   bb->is_entry    = 0;
   bb->bbcc_list   = 0;
   bb->last_bbcc   = 0;

   
   idx = bb_hash_idx(obj, offset, bbs.size);
   bb->next = bbs.table[idx];
   bbs.table[idx] = bb;

   CLG_(stat).distinct_bbs++;

#if CLG_ENABLE_DEBUG
   CLG_DEBUGIF(3) {
     VG_(printf)("  new_bb (instr %d, jmps %d, inv %s) [now %d]: ",
		 instr_count, cjmp_count,
		 cjmp_inverted ? "yes":"no",
		 CLG_(stat).distinct_bbs);
      CLG_(print_bb)(0, bb);
      VG_(printf)("\n");
   }
#endif

   CLG_(get_fn_node)(bb);

   return bb;
}


static __inline__
BB* lookup_bb(obj_node* obj, PtrdiffT offset)
{
    BB* bb;
    Int idx;

    idx = bb_hash_idx(obj, offset, bbs.size);
    bb = bbs.table[idx];

    while(bb) {
      if ((bb->obj == obj) && (bb->offset == offset)) break;
      bb = bb->next;
    }

    CLG_DEBUG(5, "  lookup_bb (Obj %s, off %#lx): %p\n",
	     obj->name, offset, bb);
    return bb;
}

static __inline__
obj_node* obj_of_address(Addr addr)
{
  obj_node* obj;
  DebugInfo* di;
  PtrdiffT offset;

  di = VG_(find_DebugInfo)(addr);
  obj = CLG_(get_obj_node)( di );

  
  offset = di ? VG_(DebugInfo_get_text_bias)(di):0;
  if (obj->offset != offset) {
      Addr start = di ? VG_(DebugInfo_get_text_avma)(di) : 0;

      CLG_DEBUG(0, "Mapping changed for '%s': %#lx -> %#lx\n",
		obj->name, obj->start, start);

      
      CLG_ASSERT( obj->size == (di ? VG_(DebugInfo_get_text_size)(di) : 0) );
      CLG_ASSERT( obj->start - start == obj->offset - offset );
      obj->offset = offset;
      obj->start = start;
  }

  return obj;
}

BB* CLG_(get_bb)(Addr addr, IRSB* bbIn,  Bool *seen_before)
{
  BB*   bb;
  obj_node* obj;
  UInt n_instrs, n_jmps;
  Bool cjmp_inverted = False;

  CLG_DEBUG(5, "+ get_bb(BB %#lx)\n", addr);

  obj = obj_of_address(addr);
  bb = lookup_bb(obj, addr - obj->offset);

  n_instrs = 0;
  n_jmps = 0;
  CLG_(collectBlockInfo)(bbIn, &n_instrs, &n_jmps, &cjmp_inverted);

  *seen_before = bb ? True : False;
  if (*seen_before) {
    if (bb->instr_count != n_instrs) {
      VG_(message)(Vg_DebugMsg, 
		   "ERROR: BB Retranslation Mismatch at BB %#lx\n", addr);
      VG_(message)(Vg_DebugMsg,
		   "  new: Obj %s, Off %#lx, BBOff %#lx, Instrs %u\n",
		   obj->name, obj->offset,
		   addr - obj->offset, n_instrs);
      VG_(message)(Vg_DebugMsg,
		   "  old: Obj %s, Off %#lx, BBOff %#lx, Instrs %u\n",
		   bb->obj->name, bb->obj->offset,
		   bb->offset, bb->instr_count);
      CLG_ASSERT(bb->instr_count == n_instrs );
    }
    CLG_ASSERT(bb->cjmp_count == n_jmps );
    CLG_(stat).bb_retranslations++;

    CLG_DEBUG(5, "- get_bb(BB %#lx): seen before.\n", addr);
    return bb;
  }

  bb = new_bb(obj, addr - obj->offset, n_instrs, n_jmps, cjmp_inverted);

  CLG_DEBUG(5, "- get_bb(BB %#lx)\n", addr);

  return bb;
}

void CLG_(delete_bb)(Addr addr)
{
    BB  *bb, *bp;
    Int idx, size;

    obj_node* obj = obj_of_address(addr);
    PtrdiffT offset = addr - obj->offset;

    idx = bb_hash_idx(obj, offset, bbs.size);
    bb = bbs.table[idx];

    bp = NULL;
    while(bb) {
      if ((bb->obj == obj) && (bb->offset == offset)) break;
      bp = bb;
      bb = bb->next;
    }

    if (bb == NULL) {
	CLG_DEBUG(3, "  delete_bb (Obj %s, off %#lx): NOT FOUND\n",
		  obj->name, offset);

	return;
    }

    

    if (bp == NULL) {
       
       tl_assert(bb == bbs.table[idx]);
       bbs.table[idx] = bb->next;
    } else {
       tl_assert(bb != bbs.table[idx]);
       bp->next = bb->next;
    }

    CLG_DEBUG(3, "  delete_bb (Obj %s, off %#lx): %p, BBCC head: %p\n",
	      obj->name, offset, bb, bb->bbcc_list);

    if (bb->bbcc_list == 0) {
	

	size = sizeof(BB)
	    + bb->instr_count * sizeof(InstrInfo)
	    + (bb->cjmp_count+1) * sizeof(CJmpInfo);
	VG_(memset)( bb, 0xAA, size );
	CLG_FREE(bb);
	return;
    }
    CLG_DEBUG(3, "  delete_bb: BB in use, can not free!\n");
}
