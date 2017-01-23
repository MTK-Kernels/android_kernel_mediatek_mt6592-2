
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
#include "costs.h"

#include "pub_tool_threadstate.h"


#define N_BBCC_INITIAL_ENTRIES  10437

bbcc_hash current_bbccs;

void CLG_(init_bbcc_hash)(bbcc_hash* bbccs)
{
   Int i;

   CLG_ASSERT(bbccs != 0);

   bbccs->size    = N_BBCC_INITIAL_ENTRIES;
   bbccs->entries = 0;
   bbccs->table = (BBCC**) CLG_MALLOC("cl.bbcc.ibh.1",
                                      bbccs->size * sizeof(BBCC*));

   for (i = 0; i < bbccs->size; i++) bbccs->table[i] = NULL;
}

void CLG_(copy_current_bbcc_hash)(bbcc_hash* dst)
{
  CLG_ASSERT(dst != 0);

  dst->size    = current_bbccs.size;
  dst->entries = current_bbccs.entries;
  dst->table   = current_bbccs.table;
}

bbcc_hash* CLG_(get_current_bbcc_hash)()
{
  return &current_bbccs;
}

void CLG_(set_current_bbcc_hash)(bbcc_hash* h)
{
  CLG_ASSERT(h != 0);

  current_bbccs.size    = h->size;
  current_bbccs.entries = h->entries;
  current_bbccs.table   = h->table;
}

void CLG_(zero_bbcc)(BBCC* bbcc)
{
  Int i;
  jCC* jcc;

  CLG_ASSERT(bbcc->cxt != 0);
  CLG_DEBUG(1, "  zero_bbcc: BB %#lx, Cxt %d "
	   "(fn '%s', rec %d)\n", 
	   bb_addr(bbcc->bb),
	   bbcc->cxt->base_number + bbcc->rec_index,
	   bbcc->cxt->fn[0]->name,
	   bbcc->rec_index);

  if ((bbcc->ecounter_sum ==0) &&
      (bbcc->ret_counter ==0)) return;

  for(i=0;i<bbcc->bb->cost_count;i++)
    bbcc->cost[i] = 0;
  for(i=0;i <= bbcc->bb->cjmp_count;i++) {
    bbcc->jmp[i].ecounter = 0;
    for(jcc=bbcc->jmp[i].jcc_list; jcc; jcc=jcc->next_from)
	CLG_(init_cost)( CLG_(sets).full, jcc->cost );
  }
  bbcc->ecounter_sum = 0;
  bbcc->ret_counter = 0;
}



void CLG_(forall_bbccs)(void (*func)(BBCC*))
{
  BBCC *bbcc, *bbcc2;
  int i, j;
	
  for (i = 0; i < current_bbccs.size; i++) {
    if ((bbcc=current_bbccs.table[i]) == NULL) continue;
    while (bbcc) {
      
      CLG_ASSERT(bbcc->rec_array != 0);

      for(j=0;j<bbcc->cxt->fn[0]->separate_recursions;j++) {
	if ((bbcc2 = bbcc->rec_array[j]) == 0) continue;

	(*func)(bbcc2);
      }
      bbcc = bbcc->next;
    }
  }
}



static __inline__
UInt bbcc_hash_idx(BB* bb, Context* cxt, UInt size)
{
   CLG_ASSERT(bb != 0);
   CLG_ASSERT(cxt != 0);

   return ((Addr)bb + (Addr)cxt) % size;
}
 

 
static
BBCC* lookup_bbcc(BB* bb, Context* cxt)
{
   BBCC* bbcc = bb->last_bbcc;
   UInt  idx;

   
   if (bbcc->cxt == cxt) {
       if (!CLG_(clo).separate_threads) {
	   
	   return bbcc;
       }
       if (bbcc->tid == CLG_(current_tid)) return bbcc;
   }

   CLG_(stat).bbcc_lru_misses++;

   idx = bbcc_hash_idx(bb, cxt, current_bbccs.size);
   bbcc = current_bbccs.table[idx];
   while (bbcc &&
	  (bb      != bbcc->bb ||
	   cxt     != bbcc->cxt)) {
       bbcc = bbcc->next;
   }
   
   CLG_DEBUG(2,"  lookup_bbcc(BB %#lx, Cxt %d, fn '%s'): %p (tid %d)\n",
	    bb_addr(bb), cxt->base_number, cxt->fn[0]->name, 
	    bbcc, bbcc ? bbcc->tid : 0);

   CLG_DEBUGIF(2)
     if (bbcc) CLG_(print_bbcc)(-2,bbcc);

   return bbcc;
}


static void resize_bbcc_hash(void)
{
    Int i, new_size, conflicts1 = 0, conflicts2 = 0;
    BBCC** new_table;
    UInt new_idx;
    BBCC *curr_BBCC, *next_BBCC;

    new_size = 2*current_bbccs.size+3;
    new_table = (BBCC**) CLG_MALLOC("cl.bbcc.rbh.1",
                                    new_size * sizeof(BBCC*));
 
    for (i = 0; i < new_size; i++)
      new_table[i] = NULL;
 
    for (i = 0; i < current_bbccs.size; i++) {
	if (current_bbccs.table[i] == NULL) continue;
 
	curr_BBCC = current_bbccs.table[i];
	while (NULL != curr_BBCC) {
	    next_BBCC = curr_BBCC->next;

	    new_idx = bbcc_hash_idx(curr_BBCC->bb,
				    curr_BBCC->cxt,
				    new_size);

	    curr_BBCC->next = new_table[new_idx];
	    new_table[new_idx] = curr_BBCC;
	    if (curr_BBCC->next) {
		conflicts1++;
		if (curr_BBCC->next->next)
		    conflicts2++;
	    }

	    curr_BBCC = next_BBCC;
	}
    }

    VG_(free)(current_bbccs.table);


    CLG_DEBUG(0,"Resize BBCC Hash: %d => %d (entries %d, conflicts %d/%d)\n",
	     current_bbccs.size, new_size,
	     current_bbccs.entries, conflicts1, conflicts2);

    current_bbccs.size = new_size;
    current_bbccs.table = new_table;
    CLG_(stat).bbcc_hash_resizes++;
}


static __inline
BBCC** new_recursion(int size)
{
    BBCC** bbccs;
    int i;

    bbccs = (BBCC**) CLG_MALLOC("cl.bbcc.nr.1", sizeof(BBCC*) * size);
    for(i=0;i<size;i++)
	bbccs[i] = 0;

    CLG_DEBUG(3,"  new_recursion(size %d): %p\n", size, bbccs);

    return bbccs;
}
  

static __inline__ 
BBCC* new_bbcc(BB* bb)
{
   BBCC* bbcc;
   Int i;

   bbcc = (BBCC*)CLG_MALLOC("cl.bbcc.nb.1",
			    sizeof(BBCC) +
			    (bb->cjmp_count+1) * sizeof(JmpData));
   bbcc->bb  = bb;
   bbcc->tid = CLG_(current_tid);

   bbcc->ret_counter = 0;
   bbcc->skipped = 0;
   bbcc->cost = CLG_(get_costarray)(bb->cost_count);
   for(i=0;i<bb->cost_count;i++)
     bbcc->cost[i] = 0;
   for(i=0; i<=bb->cjmp_count; i++) {
       bbcc->jmp[i].ecounter = 0;
       bbcc->jmp[i].jcc_list = 0;
   }
   bbcc->ecounter_sum = 0;

   
   bbcc->lru_next_bbcc = 0;
   bbcc->lru_from_jcc  = 0;
   bbcc->lru_to_jcc  = 0;
   
   CLG_(stat).distinct_bbccs++;

   CLG_DEBUG(3, "  new_bbcc(BB %#lx): %p (now %d)\n",
	    bb_addr(bb), bbcc, CLG_(stat).distinct_bbccs);

   return bbcc;
}


static
void insert_bbcc_into_hash(BBCC* bbcc)
{
    UInt idx;
    
    CLG_ASSERT(bbcc->cxt != 0);

    CLG_DEBUG(3,"+ insert_bbcc_into_hash(BB %#lx, fn '%s')\n",
	     bb_addr(bbcc->bb), bbcc->cxt->fn[0]->name);

    
    current_bbccs.entries++;
    if (100 * current_bbccs.entries / current_bbccs.size > 90)
	resize_bbcc_hash();

    idx = bbcc_hash_idx(bbcc->bb, bbcc->cxt, current_bbccs.size);
    bbcc->next = current_bbccs.table[idx];
    current_bbccs.table[idx] = bbcc;

    CLG_DEBUG(3,"- insert_bbcc_into_hash: %d entries\n",
	     current_bbccs.entries);
}

static HChar* mangled_cxt(const Context* cxt, Int rec_index)
{
    Int i, p;

    if (!cxt) return VG_(strdup)("cl.bbcc.mcxt", "(no context)");

    
    SizeT need = 20;   
    for (i = 0; i < cxt->size; ++i)
       need += VG_(strlen)(cxt->fn[i]->name) + 1;   

    HChar *mangled = CLG_MALLOC("cl.bbcc.mcxt", need);
    p = VG_(sprintf)(mangled, "%s", cxt->fn[0]->name);
    if (rec_index >0)
	p += VG_(sprintf)(mangled+p, "'%d", rec_index +1);
    for(i=1;i<cxt->size;i++)
	p += VG_(sprintf)(mangled+p, "'%s", cxt->fn[i]->name);

    return mangled;
}


static BBCC* clone_bbcc(BBCC* orig, Context* cxt, Int rec_index)
{
    BBCC* bbcc;

    CLG_DEBUG(3,"+ clone_bbcc(BB %#lx, rec %d, fn %s)\n",
	     bb_addr(orig->bb), rec_index, cxt->fn[0]->name);

    bbcc = new_bbcc(orig->bb);

    if (rec_index == 0) {

      
      CLG_ASSERT((orig->tid != CLG_(current_tid)) ||
		(orig->cxt != cxt));

      bbcc->rec_index = 0;
      bbcc->cxt = cxt;
      bbcc->rec_array = new_recursion(cxt->fn[0]->separate_recursions);
      bbcc->rec_array[0] = bbcc;

      insert_bbcc_into_hash(bbcc);
    }
    else {
      if (CLG_(clo).separate_threads)
	CLG_ASSERT(orig->tid == CLG_(current_tid));

      CLG_ASSERT(orig->cxt == cxt);
      CLG_ASSERT(orig->rec_array);
      CLG_ASSERT(cxt->fn[0]->separate_recursions > rec_index);
      CLG_ASSERT(orig->rec_array[rec_index] ==0);

      
      bbcc->rec_index = rec_index;
      bbcc->cxt = cxt;
      bbcc->rec_array = orig->rec_array;
      bbcc->rec_array[rec_index] = bbcc;
    }

    
    bbcc->next_bbcc = orig->bb->bbcc_list;
    orig->bb->bbcc_list = bbcc;


    CLG_DEBUGIF(3)
      CLG_(print_bbcc)(-2, bbcc);

    HChar *mangled_orig = mangled_cxt(orig->cxt, orig->rec_index);
    HChar *mangled_bbcc = mangled_cxt(bbcc->cxt, bbcc->rec_index);
    CLG_DEBUG(2,"- clone_BBCC(%p, %d) for BB %#lx\n"
		"   orig %s\n"
		"   new  %s\n",
	     orig, rec_index, bb_addr(orig->bb),
             mangled_orig,
             mangled_bbcc);
    CLG_FREE(mangled_orig);
    CLG_FREE(mangled_bbcc);

    CLG_(stat).bbcc_clones++;
 
    return bbcc;
};



 
BBCC* CLG_(get_bbcc)(BB* bb)
{
   BBCC* bbcc;

   CLG_DEBUG(3, "+ get_bbcc(BB %#lx)\n", bb_addr(bb));

   bbcc = bb->bbcc_list;

   if (!bbcc) {
     bbcc = new_bbcc(bb);

     
     bbcc->cxt       = 0;
     bbcc->rec_array = 0;
     bbcc->rec_index = 0;

     bbcc->next_bbcc = bb->bbcc_list;
     bb->bbcc_list = bbcc;
     bb->last_bbcc = bbcc;

     CLG_DEBUGIF(3)
       CLG_(print_bbcc)(-2, bbcc);
   }

   CLG_DEBUG(3, "- get_bbcc(BB %#lx): BBCC %p\n",
		bb_addr(bb), bbcc);

   return bbcc;
}


static void handleUnderflow(BB* bb)
{
  
  BBCC* source_bbcc;
  BB* source_bb;
  Bool seen_before;
  fn_node* caller;
  int fn_number;
  unsigned *pactive;
  call_entry* call_entry_up;

  CLG_DEBUG(1,"  Callstack underflow !\n");

  source_bb = CLG_(get_bb)(bb_addr(bb)-1, 0, &seen_before);
  source_bbcc = CLG_(get_bbcc)(source_bb);

  
  if (!seen_before) {
    source_bbcc->ecounter_sum = CLG_(current_state).collect ? 1 : 0;
  }
  else if (CLG_(current_state).collect)
    source_bbcc->ecounter_sum++;
  
  
  CLG_(current_fn_stack).top--;
  CLG_(current_state).cxt = 0;
  caller = CLG_(get_fn_node)(bb);
  CLG_(push_cxt)( caller );

  if (!seen_before) {
    
    source_bbcc->rec_array = new_recursion(caller->separate_recursions);
    source_bbcc->rec_array[0] = source_bbcc;

    CLG_ASSERT(source_bbcc->cxt == 0);
    source_bbcc->cxt = CLG_(current_state).cxt;
    insert_bbcc_into_hash(source_bbcc);
  }
  CLG_ASSERT(CLG_(current_state).bbcc);

  
  fn_number = CLG_(current_state).bbcc->cxt->fn[0]->number;
  pactive = CLG_(get_fn_entry)(fn_number);
  (*pactive)--;

  

  CLG_(current_state).nonskipped = 0; 
  
  CLG_(push_cxt)( CLG_(current_state).bbcc->cxt->fn[0] );
  CLG_(push_call_stack)(source_bbcc, 0, CLG_(current_state).bbcc,
		       (Addr)-1, False);
  call_entry_up = 
    &(CLG_(current_call_stack).entry[CLG_(current_call_stack).sp -1]);
  if (CLG_(current_state).sig == 0)
    CLG_(copy_cost)( CLG_(sets).full, call_entry_up->enter_cost,
		    CLG_(get_current_thread)()->lastdump_cost );
  else
    CLG_(zero_cost)( CLG_(sets).full, call_entry_up->enter_cost );
}



VG_REGPARM(1)
void CLG_(setup_bbcc)(BB* bb)
{
  BBCC *bbcc, *last_bbcc;
  Bool  call_emulation = False, delayed_push = False, skip = False;
  Addr sp;
  BB* last_bb;
  ThreadId tid;
  ClgJumpKind jmpkind;
  Bool isConditionalJump;
  Int passed = 0, csp;
  Bool ret_without_call = False;
  Int popcount_on_return = 1;

  CLG_DEBUG(3,"+ setup_bbcc(BB %#lx)\n", bb_addr(bb));

  tid = VG_(get_running_tid)();
#if 1
  if (UNLIKELY(tid != CLG_(current_tid)))
     CLG_(switch_thread)(tid);
#else
  CLG_ASSERT(VG_(get_running_tid)() == CLG_(current_tid));
#endif

  sp = VG_(get_SP)(tid);
  last_bbcc = CLG_(current_state).bbcc;
  last_bb = last_bbcc ? last_bbcc->bb : 0;

  if (last_bb) {
      passed = CLG_(current_state).jmps_passed;
      CLG_ASSERT(passed <= last_bb->cjmp_count);
      jmpkind = last_bb->jmp[passed].jmpkind;
      isConditionalJump = (passed < last_bb->cjmp_count);

      if (CLG_(current_state).collect) {
	if (!CLG_(current_state).nonskipped) {
	  last_bbcc->ecounter_sum++;
	  last_bbcc->jmp[passed].ecounter++;
	  if (!CLG_(clo).simulate_cache) {
	                    
              UInt instr_count = last_bb->jmp[passed].instr+1;
              CLG_(current_state).cost[ fullOffset(EG_IR) ] += instr_count;
	  }
	}
	else {
	  if (!CLG_(clo).simulate_cache) {
	      
              UInt instr_count = last_bb->jmp[passed].instr+1;
              CLG_(current_state).cost[ fullOffset(EG_IR) ] += instr_count;
              CLG_(current_state).nonskipped->skipped[ fullOffset(EG_IR) ]
		+= instr_count;
	  }
	}
      }

      CLG_DEBUGIF(4) {
	  CLG_(print_execstate)(-2, &CLG_(current_state) );
	  CLG_(print_bbcc_cost)(-2, last_bbcc);
      }
  }
  else {
      jmpkind = jk_None;
      isConditionalJump = False;
  }

  

  csp = CLG_(current_call_stack).sp;

  
  if ( (jmpkind == jk_Return) && (csp >0)) {
      Int csp_up = csp-1;      
      call_entry* top_ce = &(CLG_(current_call_stack).entry[csp_up]);

      if (sp < top_ce->sp) popcount_on_return = 0;
      else if (top_ce->sp == sp) {
	  while(1) {
	      if (top_ce->ret_addr == bb_addr(bb)) break;
	      if (csp_up>0) {
		  csp_up--;
		  top_ce = &(CLG_(current_call_stack).entry[csp_up]);
		  if (top_ce->sp == sp) {
		      popcount_on_return++;
		      continue; 
		  }
	      }
	      popcount_on_return = 0;
	      break;
	  }
      }
      if (popcount_on_return == 0) {
	  jmpkind = jk_Jump;
	  ret_without_call = True;
      }
  }

  
  if (( jmpkind != jk_Return) &&
      ( jmpkind != jk_Call) && last_bb) {

    if (ret_without_call ||
#if defined(VGA_ppc32)
	(bb->is_entry && (last_bb->fn != bb->fn)) ||
#else
	bb->is_entry ||
#endif
	(last_bb->sect_kind != bb->sect_kind) ||
	(last_bb->obj->number != bb->obj->number)) {

	CLG_DEBUG(1,"     JMP: %s[%s] to %s[%s]%s!\n",
		  last_bb->fn->name, last_bb->obj->name,
		  bb->fn->name, bb->obj->name,
		  ret_without_call?" (RET w/o CALL)":"");

	if (CLG_(get_fn_node)(last_bb)->pop_on_jump && (csp>0)) {

	    call_entry* top_ce = &(CLG_(current_call_stack).entry[csp-1]);
	    
	    if (top_ce->jcc) {

		CLG_DEBUG(1,"     Pop on Jump!\n");

		
		CLG_(current_state).bbcc = top_ce->jcc->from;
		sp = top_ce->sp;
		passed = top_ce->jcc->jmp;
		CLG_(pop_call_stack)();
	    }
	    else {
		CLG_ASSERT(CLG_(current_state).nonskipped != 0);
	    }
	}

	jmpkind = jk_Call;
	call_emulation = True;
    }
  }

  if (jmpkind == jk_Call)
    skip = CLG_(get_fn_node)(bb)->skip;

  CLG_DEBUGIF(1) {
    if (isConditionalJump)
      VG_(printf)("Cond-");
    switch(jmpkind) {
    case jk_None:   VG_(printf)("Fall-through"); break;
    case jk_Jump:   VG_(printf)("Jump"); break;
    case jk_Call:   VG_(printf)("Call"); break;
    case jk_Return: VG_(printf)("Return"); break;
    default:        tl_assert(0);
    }
    VG_(printf)(" %08lx -> %08lx, SP %08lx\n",
		last_bb ? bb_jmpaddr(last_bb) : 0,
		bb_addr(bb), sp);
  }

  
  
  if (jmpkind == jk_Return) {
    
    if ((csp == 0) || 
	((CLG_(current_fn_stack).top > CLG_(current_fn_stack).bottom) &&
	 ( *(CLG_(current_fn_stack).top-1)==0)) ) {

	
      handleUnderflow(bb);
      CLG_(pop_call_stack)();
    }
    else {
	CLG_ASSERT(popcount_on_return >0);
	CLG_(unwind_call_stack)(sp, popcount_on_return);
    }
  }
  else {
    Int unwind_count = CLG_(unwind_call_stack)(sp, 0);
    if (unwind_count > 0) {
      
      jmpkind = jk_Return;
    }
    
    if (jmpkind == jk_Call) {
      delayed_push = True;

      csp = CLG_(current_call_stack).sp;
      if (call_emulation && csp>0)
	sp = CLG_(current_call_stack).entry[csp-1].sp;	

    }
  }
  
  
  if ((delayed_push && !skip) || (CLG_(current_state).cxt == 0)) {
    CLG_(push_cxt)(CLG_(get_fn_node)(bb));
  }
  CLG_ASSERT(CLG_(current_fn_stack).top > CLG_(current_fn_stack).bottom);
  
  
  bbcc = CLG_(get_bbcc)(bb);
  if (bbcc->cxt == 0) {
    CLG_ASSERT(bbcc->rec_array == 0);
      
    bbcc->cxt = CLG_(current_state).cxt;
    bbcc->rec_array = 
      new_recursion((*CLG_(current_fn_stack).top)->separate_recursions);
    bbcc->rec_array[0] = bbcc;
      
    insert_bbcc_into_hash(bbcc);
  }
  else {
    
    
    
    
    if (last_bbcc) {
      bbcc = last_bbcc->lru_next_bbcc;
      if (bbcc &&
	  ((bbcc->bb != bb) ||
	   (bbcc->cxt != CLG_(current_state).cxt)))
	bbcc = 0;
    }
    else
      bbcc = 0;

    if (!bbcc)
      bbcc = lookup_bbcc(bb, CLG_(current_state).cxt);
    if (!bbcc)
      bbcc = clone_bbcc(bb->bbcc_list, CLG_(current_state).cxt, 0);
    
    bb->last_bbcc = bbcc;
  }

  
  if (last_bbcc)
    last_bbcc->lru_next_bbcc = bbcc;

  if ((*CLG_(current_fn_stack).top)->separate_recursions >1) {
    UInt level, idx;
    fn_node* top = *(CLG_(current_fn_stack).top);

    level = *CLG_(get_fn_entry)(top->number);

    if (delayed_push && !skip) {
      if (CLG_(clo).skip_direct_recursion) {
        
	CLG_ASSERT(CLG_(current_state).bbcc != 0);
	 
	if (CLG_(current_state).bbcc->cxt->fn[0] != bbcc->cxt->fn[0])
	  level++;
      }
      else level++;
    }
    if (level> top->separate_recursions)
      level = top->separate_recursions;

    if (level == 0) {
      
      level = 1;
      *CLG_(get_fn_entry)(top->number) = 1;
    }

    idx = level -1;
    if (bbcc->rec_array[idx])
      bbcc = bbcc->rec_array[idx];
    else
      bbcc = clone_bbcc(bbcc, CLG_(current_state).cxt, idx);

    CLG_ASSERT(bbcc->rec_array[bbcc->rec_index] == bbcc);
  }

  if (delayed_push) {
    if (!skip && CLG_(current_state).nonskipped) {
      
      CLG_(current_state).bbcc = CLG_(current_state).nonskipped;
      
      passed = CLG_(current_state).bbcc->bb->cjmp_count;
    }
    CLG_(push_call_stack)(CLG_(current_state).bbcc, passed,
			 bbcc, sp, skip);
  }

  if (CLG_(clo).collect_jumps && (jmpkind == jk_Jump)) {
    
    
    jCC* jcc = CLG_(get_jcc)(last_bbcc, passed, bbcc);
    CLG_ASSERT(jcc != 0);
    
    if (jcc->jmpkind == jk_Call)
      jcc->jmpkind = isConditionalJump ? jk_CondJump : jk_Jump;
    else {
	
	
    }
    
    jcc->call_counter++;
    if (isConditionalJump)
      CLG_(stat).jcnd_counter++;
    else
      CLG_(stat).jump_counter++;
  }
  
  CLG_(current_state).bbcc = bbcc;
  CLG_(current_state).jmps_passed = 0;
  
  CLG_(bb_base)   = bb->obj->offset + bb->offset;
  CLG_(cost_base) = bbcc->cost;
  
  CLG_DEBUGIF(1) {
    VG_(printf)("     ");
    CLG_(print_bbcc_fn)(bbcc);
    VG_(printf)("\n");
  }
  
  CLG_DEBUG(3,"- setup_bbcc (BB %#lx): Cost %p (Len %d), Instrs %d (Len %d)\n",
	   bb_addr(bb), bbcc->cost, bb->cost_count, 
	   bb->instr_count, bb->instr_len);
  CLG_DEBUGIF(3)
    CLG_(print_cxt)(-8, CLG_(current_state).cxt, bbcc->rec_index);
  CLG_DEBUG(3,"\n");
  
  CLG_(stat).bb_executions++;
}
