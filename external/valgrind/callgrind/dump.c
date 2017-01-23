
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

#include "config.h"
#include "global.h"

#include "pub_tool_threadstate.h"
#include "pub_tool_libcfile.h"


static Int out_counter = 0;

static HChar* out_file = 0;
static Bool dumps_initialized = False;

static HChar *cmdbuf;

FullCost CLG_(total_cost) = 0;
static FullCost dump_total_cost = 0;

EventMapping* CLG_(dumpmap) = 0;

Int CLG_(get_dump_counter)(void)
{
  return out_counter;
}


static Bool* dump_array = 0;
static Int   dump_array_size = 0;
static Bool* obj_dumped = 0;
static Bool* file_dumped = 0;
static Bool* fn_dumped = 0;
static Bool* cxt_dumped = 0;

static
void reset_dump_array(void)
{
    int i;

    CLG_ASSERT(dump_array != 0);

    for(i=0;i<dump_array_size;i++)
	dump_array[i] = False;
}

static
void init_dump_array(void)
{
    dump_array_size = CLG_(stat).distinct_objs +
      CLG_(stat).distinct_files +
      CLG_(stat).distinct_fns +
      CLG_(stat).context_counter;
    CLG_ASSERT(dump_array == 0);
    dump_array = (Bool*) CLG_MALLOC("cl.dump.ida.1",
                                    dump_array_size * sizeof(Bool));
    obj_dumped  = dump_array;
    file_dumped = obj_dumped + CLG_(stat).distinct_objs;
    fn_dumped   = file_dumped + CLG_(stat).distinct_files;
    cxt_dumped  = fn_dumped + CLG_(stat).distinct_fns;

    reset_dump_array();

    CLG_DEBUG(1, "  init_dump_array: size %d\n", dump_array_size);
}

static __inline__
void free_dump_array(void)
{
    CLG_ASSERT(dump_array != 0);
    VG_(free)(dump_array);

    dump_array = 0;
    obj_dumped = 0;
    file_dumped = 0;
    fn_dumped = 0;
    cxt_dumped = 0;
}


static __inline__
void init_fpos(FnPos* p)
 {
    p->file = 0;
    p->fn = 0;
    p->obj = 0;
    p->cxt = 0;
    p->rec_index = 0;
}


static void print_obj(VgFile *fp, const HChar* prefix, obj_node* obj)
{
    if (CLG_(clo).compress_strings) {
	CLG_ASSERT(obj_dumped != 0);
	if (obj_dumped[obj->number])
            VG_(fprintf)(fp, "%s(%d)\n", prefix, obj->number);
	else {
            VG_(fprintf)(fp, "%s(%d) %s\n", prefix, obj->number, obj->name);
	}
    }
    else
        VG_(fprintf)(fp, "%s%s\n", prefix, obj->name);

#if 0
    if (!obj_dumped[obj->number]) {
	obj_dumped[obj->number];
	VG_(fprintf)(fp, "mp=%p %p %p\n",
		     pos->obj->start, pos->obj->size, pos->obj->offset);
    }
#else
    obj_dumped[obj->number] = True;
#endif
}

static void print_file(VgFile *fp, const char *prefix, const file_node* file)
{
    if (CLG_(clo).compress_strings) {
	CLG_ASSERT(file_dumped != 0);
	if (file_dumped[file->number])
            VG_(fprintf)(fp, "%s(%d)\n", prefix, file->number);
	else {
            VG_(fprintf)(fp, "%s(%d) %s\n", prefix, file->number, file->name);
	    file_dumped[file->number] = True;
	}
    }
    else
        VG_(fprintf)(fp, "%s%s\n", prefix, file->name);
}

static void print_fn(VgFile *fp, const HChar* tag, const fn_node* fn)
{
    VG_(fprintf)(fp, "%s=",tag);
    if (CLG_(clo).compress_strings) {
	CLG_ASSERT(fn_dumped != 0);
	if (fn_dumped[fn->number])
	    VG_(fprintf)(fp, "(%d)\n", fn->number);
	else {
	    VG_(fprintf)(fp, "(%d) %s\n", fn->number, fn->name);
	    fn_dumped[fn->number] = True;
	}
    }
    else
        VG_(fprintf)(fp, "%s\n", fn->name);
}

static void print_mangled_fn(VgFile *fp, const HChar* tag, 
			     Context* cxt, int rec_index)
{
    int i;

    if (CLG_(clo).compress_strings && CLG_(clo).compress_mangled) {

	int n;
	Context* last;

	CLG_ASSERT(cxt_dumped != 0);
	if (cxt_dumped[cxt->base_number+rec_index]) {
            VG_(fprintf)(fp, "%s=(%d)\n",
			     tag, cxt->base_number + rec_index);
	    return;
	}

	last = 0;
	/* make sure that for all context parts compressed data is written */
	for(i=cxt->size;i>0;i--) {
	    CLG_ASSERT(cxt->fn[i-1]->pure_cxt != 0);
	    n = cxt->fn[i-1]->pure_cxt->base_number;
	    if (cxt_dumped[n]) continue;
	    VG_(fprintf)(fp, "%s=(%d) %s\n",
			     tag, n, cxt->fn[i-1]->name);

	    cxt_dumped[n] = True;
	    last = cxt->fn[i-1]->pure_cxt;
	}
	
	if ((last == cxt) && (rec_index == 0)) return;

	VG_(fprintf)(fp, "%s=(%d) (%d)", tag,
			 cxt->base_number + rec_index,
			 cxt->fn[0]->pure_cxt->base_number);
	if (rec_index >0)
	    VG_(fprintf)(fp, "'%d", rec_index +1);
	for(i=1;i<cxt->size;i++)
	    VG_(fprintf)(fp, "'(%d)", 
			      cxt->fn[i]->pure_cxt->base_number);
	VG_(fprintf)(fp, "\n");

	cxt_dumped[cxt->base_number+rec_index] = True;
	return;
    }


    VG_(fprintf)(fp, "%s=", tag);
    if (CLG_(clo).compress_strings) {
	CLG_ASSERT(cxt_dumped != 0);
	if (cxt_dumped[cxt->base_number+rec_index]) {
	    VG_(fprintf)(fp, "(%d)\n", cxt->base_number + rec_index);
	    return;
	}
	else {
	    VG_(fprintf)(fp, "(%d) ", cxt->base_number + rec_index);
	    cxt_dumped[cxt->base_number+rec_index] = True;
	}
    }

    VG_(fprintf)(fp, "%s", cxt->fn[0]->name);
    if (rec_index >0)
	VG_(fprintf)(fp, "'%d", rec_index +1);
    for(i=1;i<cxt->size;i++)
	VG_(fprintf)(fp, "'%s", cxt->fn[i]->name);

    VG_(fprintf)(fp, "\n");
}



static Bool print_fn_pos(VgFile *fp, FnPos* last, BBCC* bbcc)
{
    Bool res = False;

    CLG_ASSERT(bbcc && bbcc->cxt);

    CLG_DEBUGIF(3) {
	CLG_DEBUG(2, "+ print_fn_pos: ");
	CLG_(print_cxt)(16, bbcc->cxt, bbcc->rec_index);
    }

    if (!CLG_(clo).mangle_names) {
	if (last->rec_index != bbcc->rec_index) {
	    VG_(fprintf)(fp, "rec=%d\n\n", bbcc->rec_index);
	    last->rec_index = bbcc->rec_index;
	    last->cxt = 0; 
	    res = True;
	}
	
	if (last->cxt != bbcc->cxt) {
	    fn_node* last_from = (last->cxt && last->cxt->size >1) ?
				 last->cxt->fn[1] : 0;
	    fn_node* curr_from = (bbcc->cxt->size >1) ?
				 bbcc->cxt->fn[1] : 0;
	    if (curr_from == 0) {
		if (last_from != 0) {
		    
		    VG_(fprintf)(fp, "frfn=(spontaneous)\n");
		    res = True;
		}
	    }
	    else if (last_from != curr_from) {
		print_fn(fp, "frfn", curr_from);
		res = True;
	    }
	    last->cxt = bbcc->cxt;
	}
    }

    if (last->obj != bbcc->cxt->fn[0]->file->obj) {
	print_obj(fp, "ob=", bbcc->cxt->fn[0]->file->obj);
	last->obj = bbcc->cxt->fn[0]->file->obj;
	res = True;
    }

    if (last->file != bbcc->cxt->fn[0]->file) {
        print_file(fp, "fl=", bbcc->cxt->fn[0]->file);
	last->file = bbcc->cxt->fn[0]->file;
	res = True;
    }

    if (!CLG_(clo).mangle_names) {
	if (last->fn != bbcc->cxt->fn[0]) {
	    print_fn(fp, "fn", bbcc->cxt->fn[0]);
	    last->fn = bbcc->cxt->fn[0];
	    res = True;
	}
    }
    else {
	
	if ((last->rec_index != bbcc->rec_index) ||
	    (last->cxt != bbcc->cxt)) {

	    print_mangled_fn(fp, "fn", bbcc->cxt, bbcc->rec_index);
	    last->fn = bbcc->cxt->fn[0];
	    last->rec_index = bbcc->rec_index;
	    res = True;
	}
    }

    last->cxt = bbcc->cxt;

    CLG_DEBUG(2, "- print_fn_pos: %s\n", res ? "changed" : "");
    
    return res;
}

#define DEBUG_CACHE_SIZE 1777

static Addr       debug_cache_addr[DEBUG_CACHE_SIZE];
static file_node* debug_cache_file[DEBUG_CACHE_SIZE];
static int        debug_cache_line[DEBUG_CACHE_SIZE];
static Bool       debug_cache_info[DEBUG_CACHE_SIZE];

static __inline__
void init_debug_cache(void)
{
    int i;
    for(i=0;i<DEBUG_CACHE_SIZE;i++) {
	debug_cache_addr[i] = 0;
	debug_cache_file[i] = 0;
	debug_cache_line[i] = 0;
	debug_cache_info[i] = 0;
    }
}

static 
Bool get_debug_pos(BBCC* bbcc, Addr addr, AddrPos* p)
{
    const HChar *file, *dir;
    Bool found_file_line;

    int cachepos = addr % DEBUG_CACHE_SIZE;
    
    if (debug_cache_addr[cachepos] == addr) {
	p->line = debug_cache_line[cachepos];
	p->file = debug_cache_file[cachepos];
	found_file_line = debug_cache_info[cachepos];
    }
    else {
	found_file_line = VG_(get_filename_linenum)(addr,
						    &file,
						    &dir,
						    &(p->line));
	if (!found_file_line) {
            file = "???";
	    p->line = 0;
	}
	p->file    = CLG_(get_file_node)(bbcc->bb->obj, dir, file);

	debug_cache_info[cachepos] = found_file_line;
	debug_cache_addr[cachepos] = addr;
	debug_cache_line[cachepos] = p->line;
	debug_cache_file[cachepos] = p->file;
    }

    
    p->addr = addr - bbcc->bb->obj->offset;
    p->bb_addr = bbcc->bb->offset;

    CLG_DEBUG(3, "  get_debug_pos(%#lx): BB %#lx, fn '%s', file '%s', line %u\n",
	     addr, bb_addr(bbcc->bb), bbcc->cxt->fn[0]->name,
	     p->file->name, p->line);

    return found_file_line;
}


static void init_apos(AddrPos* p, Addr addr, Addr bbaddr, file_node* file)
{
    p->addr    = addr;
    p->bb_addr = bbaddr;
    p->file    = file;
    p->line    = 0;
}

static void copy_apos(AddrPos* dst, AddrPos* src)
{
    dst->addr    = src->addr;
    dst->bb_addr = src->bb_addr;
    dst->file    = src->file;
    dst->line    = src->line;
}   

static void init_fcost(AddrCost* c, Addr addr, Addr bbaddr, file_node* file)
{
    init_apos( &(c->p), addr, bbaddr, file);
    
    c->cost = CLG_(get_eventset_cost)( CLG_(sets).full );
    CLG_(init_cost)( CLG_(sets).full, c->cost );
}


static void fprint_apos(VgFile *fp, AddrPos* curr, AddrPos* last,
                        file_node* func_file)
{
    CLG_ASSERT(curr->file != 0);
    CLG_DEBUG(2, "    print_apos(file '%s', line %d, bb %#lx, addr %#lx) fnFile '%s'\n",
	     curr->file->name, curr->line, curr->bb_addr, curr->addr,
	     func_file->name);

    if (curr->file != last->file) {

	
	if (curr->file == func_file)
            print_file(fp, "fe=", curr->file);
	else
            print_file(fp, "fi=", curr->file);
    }

    if (CLG_(clo).dump_bbs) {
	if (curr->line != last->line) {
	    VG_(fprintf)(fp, "ln=%d\n", curr->line);
	}
    }
}



static
void fprint_pos(VgFile *fp, const AddrPos* curr, const AddrPos* last)
{
    if (0) 
	VG_(fprintf)(fp, "%lu ", curr->addr - curr->bb_addr);
    else {
	if (CLG_(clo).dump_instr) {
	    int diff = curr->addr - last->addr;
	    if ( CLG_(clo).compress_pos && (last->addr >0) && 
		 (diff > -100) && (diff < 100)) {
		if (diff >0)
		    VG_(fprintf)(fp, "+%d ", diff);
		else if (diff==0)
		    VG_(fprintf)(fp, "* ");
	        else
		    VG_(fprintf)(fp, "%d ", diff);
	    }
	    else
		VG_(fprintf)(fp, "%#lx ", curr->addr);
	}

	if (CLG_(clo).dump_bb) {
	    int diff = curr->bb_addr - last->bb_addr;
	    if ( CLG_(clo).compress_pos && (last->bb_addr >0) && 
		 (diff > -100) && (diff < 100)) {
		if (diff >0)
		    VG_(fprintf)(fp, "+%d ", diff);
		else if (diff==0)
		    VG_(fprintf)(fp, "* ");
	        else
		    VG_(fprintf)(fp, "%d ", diff);
	    }
	    else
		VG_(fprintf)(fp, "%#lx ", curr->bb_addr);
	}

	if (CLG_(clo).dump_line) {
	    int diff = curr->line - last->line;
	    if ( CLG_(clo).compress_pos && (last->line >0) && 
		 (diff > -100) && (diff < 100)) {

		if (diff >0)
		    VG_(fprintf)(fp, "+%d ", diff);
		else if (diff==0)
		    VG_(fprintf)(fp, "* ");
	        else
		    VG_(fprintf)(fp, "%d ", diff);
	    }
	    else
		VG_(fprintf)(fp, "%u ", curr->line);
	}
    }
}



static
void fprint_cost(VgFile *fp, const EventMapping* es, const ULong* cost)
{
  HChar *mcost = CLG_(mappingcost_as_string)(es, cost);
  VG_(fprintf)(fp, "%s\n", mcost);
  CLG_FREE(mcost);
}



/* Write the cost of a source line; only that parts of the source
 * position are written that changed relative to last written position.
 * funcPos is the source position of the first line of actual function.
 * Something is written only if cost != 0; returns True in this case.
 */
static void fprint_fcost(VgFile *fp, AddrCost* c, AddrPos* last)
{
  CLG_DEBUGIF(3) {
    CLG_DEBUG(2, "   print_fcost(file '%s', line %d, bb %#lx, addr %#lx):\n",
	     c->p.file->name, c->p.line, c->p.bb_addr, c->p.addr);
    CLG_(print_cost)(-5, CLG_(sets).full, c->cost);
  }
    
  fprint_pos(fp, &(c->p), last);
  copy_apos( last, &(c->p) ); 

  fprint_cost(fp, CLG_(dumpmap), c->cost);

  
  CLG_(add_and_zero_cost)( CLG_(sets).full, dump_total_cost, c->cost );
}


static void fprint_jcc(VgFile *fp, jCC* jcc, AddrPos* curr, AddrPos* last,
                       ULong ecounter)
{
    static AddrPos target;
    file_node* file;
    obj_node*  obj;

    CLG_DEBUGIF(2) {
      CLG_DEBUG(2, "   fprint_jcc (jkind %d)\n", jcc->jmpkind);
      CLG_(print_jcc)(-10, jcc);
    }

    CLG_ASSERT(jcc->to !=0);
    CLG_ASSERT(jcc->from !=0);
    
    if (!get_debug_pos(jcc->to, bb_addr(jcc->to->bb), &target)) {
	
	target.file = last->file;
    }

    if ((jcc->jmpkind == jk_CondJump) || (jcc->jmpkind == jk_Jump)) {
	    
      
      CLG_ASSERT(CLG_(is_zero_cost)( CLG_(sets).full, jcc->cost));
	
      CLG_ASSERT(jcc->from->bb->obj == jcc->to->bb->obj);

	
	if (!CLG_(clo).dump_instr && !CLG_(clo).dump_bb && target.line==0) {
	  jcc->call_counter = 0;
	  return;
	}

	if (last->file != target.file) {
            print_file(fp, "jfi=", target.file);
	}
	
	if (jcc->from->cxt != jcc->to->cxt) {
	    if (CLG_(clo).mangle_names)
		print_mangled_fn(fp, "jfn",
				 jcc->to->cxt, jcc->to->rec_index);
	    else
		print_fn(fp, "jfn", jcc->to->cxt->fn[0]);
	}
	    
	if (jcc->jmpkind == jk_CondJump) {
	    
	    VG_(fprintf)(fp, "jcnd=%llu/%llu ",
			 jcc->call_counter, ecounter);
	}
	else {
	    
	    VG_(fprintf)(fp, "jump=%llu ",
			 jcc->call_counter);
	}
		
	fprint_pos(fp, &target, last);
	VG_(fprintf)(fp, "\n");
	fprint_pos(fp, curr, last);
	VG_(fprintf)(fp, "\n");

	jcc->call_counter = 0;
	return;
    }

    file = jcc->to->cxt->fn[0]->file;
    obj  = jcc->to->bb->obj;
    
    
    if (jcc->from->cxt->fn[0]->file->obj != obj) {
	print_obj(fp, "cob=", obj);
    }

    
    if (last->file != file) {
        print_file(fp, "cfi=", file);
    }

    if (CLG_(clo).mangle_names)
	print_mangled_fn(fp, "cfn", jcc->to->cxt, jcc->to->rec_index);
    else
	print_fn(fp, "cfn", jcc->to->cxt->fn[0]);

    if (!CLG_(is_zero_cost)( CLG_(sets).full, jcc->cost)) {
        VG_(fprintf)(fp, "calls=%llu ", 
		   jcc->call_counter);

	fprint_pos(fp, &target, last);
        VG_(fprintf)(fp, "\n");
	fprint_pos(fp, curr, last);
	fprint_cost(fp, CLG_(dumpmap), jcc->cost);

	CLG_(init_cost)( CLG_(sets).full, jcc->cost );

	jcc->call_counter = 0;
    }
}



static AddrCost ccSum[2];
static int currSum;

/*
 * Print all costs of a BBCC:
 * - FCCs of instructions
 * - JCCs of the unique jump of this BB
 * returns True if something was written 
 */
static Bool fprint_bbcc(VgFile *fp, BBCC* bbcc, AddrPos* last)
{
  InstrInfo* instr_info;
  ULong ecounter;
  Bool something_written = False;
  jCC* jcc;
  AddrCost *currCost, *newCost;
  Int jcc_count = 0, instr, i, jmp;
  BB* bb = bbcc->bb;

  CLG_ASSERT(bbcc->cxt != 0);
  CLG_DEBUGIF(1) {
    VG_(printf)("+ fprint_bbcc (Instr %d): ", bb->instr_count);
    CLG_(print_bbcc)(15, bbcc);
  }

  CLG_ASSERT(currSum == 0 || currSum == 1);
  currCost = &(ccSum[currSum]);
  newCost  = &(ccSum[1-currSum]);

  ecounter = bbcc->ecounter_sum;
  jmp = 0;
  instr_info = &(bb->instr[0]);
  for(instr=0; instr<bb->instr_count; instr++, instr_info++) {

    if (!get_debug_pos(bbcc, bb_addr(bb) + instr_info->instr_offset, 
		       &(newCost->p))) {
      
      newCost->p.file = bbcc->cxt->fn[0]->file;
    }

    if (CLG_(clo).dump_bbs || CLG_(clo).dump_instr ||
	(newCost->p.line != currCost->p.line) ||
	(newCost->p.file != currCost->p.file)) {
      
      if (!CLG_(is_zero_cost)( CLG_(sets).full, currCost->cost )) {
	something_written = True;
	
	fprint_apos(fp, &(currCost->p), last, bbcc->cxt->fn[0]->file);
	fprint_fcost(fp, currCost, last);
      }
	   
      
      currSum = 1 - currSum;
      currCost = &(ccSum[currSum]);
      newCost  = &(ccSum[1-currSum]);
    }
       
    
    (*CLG_(cachesim).add_icost)(currCost->cost, bbcc, instr_info, ecounter);

    
    if (bb->jmp[jmp].instr == instr) {
	jcc_count=0;
	for(jcc=bbcc->jmp[jmp].jcc_list; jcc; jcc=jcc->next_from)
	    if (((jcc->jmpkind != jk_Call) && (jcc->call_counter >0)) ||
		(!CLG_(is_zero_cost)( CLG_(sets).full, jcc->cost )))
	      jcc_count++;

	if (jcc_count>0) {    
	    if (!CLG_(is_zero_cost)( CLG_(sets).full, currCost->cost )) {
		
		fprint_apos(fp, &(currCost->p), last, bbcc->cxt->fn[0]->file);
		fprint_fcost(fp, currCost, last);
	    }
	    get_debug_pos(bbcc, bb_addr(bb)+instr_info->instr_offset, &(currCost->p));
	    fprint_apos(fp, &(currCost->p), last, bbcc->cxt->fn[0]->file);
	    something_written = True;
	    for(jcc=bbcc->jmp[jmp].jcc_list; jcc; jcc=jcc->next_from) {
		if (((jcc->jmpkind != jk_Call) && (jcc->call_counter >0)) ||
		    (!CLG_(is_zero_cost)( CLG_(sets).full, jcc->cost )))
		    fprint_jcc(fp, jcc, &(currCost->p), last, ecounter);
	    }
	}
    }

    
    if (jmp < bb->cjmp_count)
	if (bb->jmp[jmp].instr == instr) {
	    ecounter -= bbcc->jmp[jmp].ecounter;
	    jmp++;
	}
  }
  
  
  jcc_count = 0;
  for(jcc=bbcc->jmp[jmp].jcc_list; jcc; jcc=jcc->next_from) {
      
      if ( ((jcc->jmpkind != jk_Call) && (jcc->call_counter >0)) ||
	   (!CLG_(is_zero_cost)( CLG_(sets).full, jcc->cost )))
	  jcc_count++;
  }
  
  if ( (bbcc->skipped &&
	!CLG_(is_zero_cost)(CLG_(sets).full, bbcc->skipped)) || 
       (jcc_count>0) ) {
    
    if (!CLG_(is_zero_cost)( CLG_(sets).full, currCost->cost )) {
      
      fprint_apos(fp, &(currCost->p), last, bbcc->cxt->fn[0]->file);
      fprint_fcost(fp, currCost, last);
    }
    
    get_debug_pos(bbcc, bb_jmpaddr(bb), &(currCost->p));
    fprint_apos(fp, &(currCost->p), last, bbcc->cxt->fn[0]->file);
    something_written = True;
    
    
    if (bbcc->skipped && !CLG_(is_zero_cost)( CLG_(sets).full,
					     bbcc->skipped )) {
      CLG_(add_and_zero_cost)( CLG_(sets).full,
			      currCost->cost, bbcc->skipped );
#if 0
      VG_(fprintf)(fp, "# Skipped\n");
#endif
      fprint_fcost(fp, currCost, last);
    }

    if (jcc_count > 0)
	for(jcc=bbcc->jmp[jmp].jcc_list; jcc; jcc=jcc->next_from) {
	    CLG_ASSERT(jcc->jmp == jmp);
	    if ( ((jcc->jmpkind != jk_Call) && (jcc->call_counter >0)) ||
		 (!CLG_(is_zero_cost)( CLG_(sets).full, jcc->cost )))
	  
		fprint_jcc(fp, jcc, &(currCost->p), last, ecounter);
	}
  }

  if (CLG_(clo).dump_bbs || CLG_(clo).dump_bb) {
    if (!CLG_(is_zero_cost)( CLG_(sets).full, currCost->cost )) {
      something_written = True;
      
      fprint_apos(fp, &(currCost->p), last, bbcc->cxt->fn[0]->file);
      fprint_fcost(fp, currCost, last);
    }
    if (CLG_(clo).dump_bbs) VG_(fprintf)(fp, "\n");
    
    /* when every cost was immediatly written, we must have done so,
     * as this function is only called when there's cost in a BBCC
     */
    CLG_ASSERT(something_written);
  }
  
  bbcc->ecounter_sum = 0;
  for(i=0; i<=bbcc->bb->cjmp_count; i++)
    bbcc->jmp[i].ecounter = 0;
  bbcc->ret_counter = 0;
  
  CLG_DEBUG(1, "- fprint_bbcc: JCCs %d\n", jcc_count);
  
  return something_written;
}

static int my_cmp(BBCC** pbbcc1, BBCC** pbbcc2)
{
#if 0
    return (*pbbcc1)->bb->offset - (*pbbcc2)->bb->offset;
#else
    BBCC *bbcc1 = *pbbcc1;
    BBCC *bbcc2 = *pbbcc2;
    Context* cxt1 = bbcc1->cxt;
    Context* cxt2 = bbcc2->cxt;
    int off = 1;

    if (cxt1->fn[0]->file->obj != cxt2->fn[0]->file->obj)
	return cxt1->fn[0]->file->obj - cxt2->fn[0]->file->obj;

    if (cxt1->fn[0]->file != cxt2->fn[0]->file)
	return cxt1->fn[0]->file - cxt2->fn[0]->file;

    if (cxt1->fn[0] != cxt2->fn[0])
	return cxt1->fn[0] - cxt2->fn[0];

    if (bbcc1->rec_index != bbcc2->rec_index)
	return bbcc1->rec_index - bbcc2->rec_index;

    while((off < cxt1->size) && (off < cxt2->size)) {
	fn_node* ffn1 = cxt1->fn[off];
	fn_node* ffn2 = cxt2->fn[off];
	if (ffn1->file->obj != ffn2->file->obj)
	    return ffn1->file->obj - ffn2->file->obj;
	if (ffn1 != ffn2)
	    return ffn1 - ffn2;
	off++;
    }
    if      (cxt1->size > cxt2->size) return 1;
    else if (cxt1->size < cxt2->size) return -1;

    return bbcc1->bb->offset - bbcc2->bb->offset;
#endif
}





/* modified version of:
 *
 * qsort -- qsort interface implemented by faster quicksort.
 * J. L. Bentley and M. D. McIlroy, SPE 23 (1993) 1249-1265.
 * Copyright 1993, John Wiley.
*/

static __inline__
void swap(BBCC** a, BBCC** b)
{
    BBCC* t;
    t = *a; *a = *b; *b = t;
}

#define min(x, y) ((x)<=(y) ? (x) : (y))

static
BBCC** med3(BBCC **a, BBCC **b, BBCC **c, int (*cmp)(BBCC**,BBCC**))
{	return cmp(a, b) < 0 ?
		  (cmp(b, c) < 0 ? b : cmp(a, c) < 0 ? c : a)
		: (cmp(b, c) > 0 ? b : cmp(a, c) > 0 ? c : a);
}

static BBCC** qsort_start = 0;

static void qsort(BBCC **a, int n, int (*cmp)(BBCC**,BBCC**))
{
	BBCC **pa, **pb, **pc, **pd, **pl, **pm, **pn, **pv;
	int s, r;
	BBCC* v;

	CLG_DEBUG(8, "  qsort(%ld,%ld)\n", a-qsort_start + 0L, n + 0L);

	if (n < 7) {	 
		for (pm = a+1; pm < a+n; pm++)
			for (pl = pm; pl > a && cmp(pl-1, pl) > 0; pl --)
				swap(pl, pl-1);

		CLG_DEBUGIF(8) {
		    for (pm = a; pm < a+n; pm++) {
			VG_(printf)("   %3ld BB %#lx, ",
                                    pm - qsort_start + 0L,
				    bb_addr((*pm)->bb));      
			CLG_(print_cxt)(9, (*pm)->cxt, (*pm)->rec_index);
		    }
		}
		return;
	}
	pm = a + n/2;    
	if (n > 7) {
		pl = a;
		pn = a + (n-1);
		if (n > 40) {    
			s = n/8;
			pl = med3(pl, pl+s, pl+2*s, cmp);
			pm = med3(pm-s, pm, pm+s, cmp);
			pn = med3(pn-2*s, pn-s, pn, cmp);
		}
		pm = med3(pl, pm, pn, cmp); 
	}


	v = *pm;
	pv = &v;
	pa = pb = a;
	pc = pd = a + (n-1);
	for (;;) {
		while ((pb <= pc) && ((r=cmp(pb, pv)) <= 0)) {
		    if (r==0) {
			
			swap(pa,pb); pa++; 
		    }
		    pb ++;
		}
		while ((pb <= pc) && ((r=cmp(pc, pv)) >= 0)) {
		    if (r==0) {
			
			swap(pc,pd); pd--; 
		    }
		    pc --;
		}
		if (pb > pc) { break; }
		swap(pb, pc);
		pb ++;
		pc --;
	}
	pb--;
	pc++;

	
	if ((s = pa-a)>0) { for(r=0;r<s;r++) swap(a+r, pb+1-s+r); }
	
	if ((s = a+n-1-pd)>0) { for(r=0;r<s;r++) swap(pc+r, a+n-s+r); }	    

	CLG_DEBUGIF(8) {
	  VG_(printf)("   PV BB %#lx, ", bb_addr((*pv)->bb));
	    CLG_(print_cxt)(9, (*pv)->cxt, (*pv)->rec_index);

	    s = pb-pa+1;
	    VG_(printf)("    Lower %ld - %ld:\n",
                        a-qsort_start + 0L,
                        a+s-1-qsort_start + 0L);
	    for (r=0;r<s;r++) {
		pm = a+r;
		VG_(printf)("     %3ld BB %#lx, ",
			    pm-qsort_start + 0L,
                            bb_addr((*pm)->bb));
		CLG_(print_cxt)(9, (*pm)->cxt, (*pm)->rec_index);
	    }

	    s = pd-pc+1;
	    VG_(printf)("    Upper %ld - %ld:\n",
			a+n-s-qsort_start + 0L,
                        a+n-1-qsort_start + 0L);
	    for (r=0;r<s;r++) {
		pm = a+n-s+r;
		VG_(printf)("     %3ld BB %#lx, ",
			    pm-qsort_start + 0L,
                            bb_addr((*pm)->bb));
		CLG_(print_cxt)(9, (*pm)->cxt, (*pm)->rec_index);
	    }
	}

	if ((s = pb+1-pa) > 1) qsort(a,     s, cmp);
	if ((s = pd+1-pc) > 1) qsort(a+n-s, s, cmp);
}



static Int    prepare_count;
static BBCC** prepare_ptr;


static void hash_addCount(BBCC* bbcc)
{
  if ((bbcc->ecounter_sum > 0) || (bbcc->ret_counter>0))
    prepare_count++;
}

static void hash_addPtr(BBCC* bbcc)
{
  if ((bbcc->ecounter_sum == 0) &&
      (bbcc->ret_counter == 0)) return;

  *prepare_ptr = bbcc;
  prepare_ptr++;
}


static void cs_addCount(thread_info* ti)
{
  Int i;
  BBCC* bbcc;

      
  for(i = 0; i < CLG_(current_call_stack).sp; i++) {
    call_entry* e = &(CLG_(current_call_stack).entry[i]);
    if (e->jcc == 0) continue;
    
    CLG_(add_diff_cost_lz)( CLG_(sets).full, &(e->jcc->cost),
			   e->enter_cost, CLG_(current_state).cost);
    bbcc = e->jcc->from;

    CLG_DEBUG(1, " [%2d] (tid %d), added active: %s\n",
	     i,CLG_(current_tid),bbcc->cxt->fn[0]->name);
    
    if (bbcc->ecounter_sum>0 || bbcc->ret_counter>0) {
      
      continue;
    }
    prepare_count++;
  }
}

static void cs_addPtr(thread_info* ti)
{
  Int i;
  BBCC* bbcc;

      
  for(i = 0; i < CLG_(current_call_stack).sp; i++) {
    call_entry* e = &(CLG_(current_call_stack).entry[i]);
    if (e->jcc == 0) continue;

    bbcc = e->jcc->from;
    
    if (bbcc->ecounter_sum>0 || bbcc->ret_counter>0) {
      
      continue;
    }

    *prepare_ptr = bbcc;
    prepare_ptr++;
  }
}


static
BBCC** prepare_dump(void)
{
    BBCC **array;

    prepare_count = 0;
    
    
    
    CLG_(forall_bbccs)(hash_addCount);

    if (CLG_(clo).separate_threads)
      cs_addCount(0);
    else
      CLG_(forall_threads)(cs_addCount);

    CLG_DEBUG(0, "prepare_dump: %d BBCCs\n", prepare_count);

    
    prepare_ptr = array =
      (BBCC**) CLG_MALLOC("cl.dump.pd.1",
                          (prepare_count+1) * sizeof(BBCC*));    

    CLG_(forall_bbccs)(hash_addPtr);

    if (CLG_(clo).separate_threads)
      cs_addPtr(0);
    else
      CLG_(forall_threads)(cs_addPtr);

    CLG_ASSERT(array + prepare_count == prepare_ptr);

    
    *prepare_ptr = 0;

    CLG_DEBUG(0,"             BBCCs inserted\n");

    qsort_start = array;
    qsort(array, prepare_count, my_cmp);

    CLG_DEBUG(0,"             BBCCs sorted\n");

    return array;
}




static void fprint_cost_ln(VgFile *fp, const HChar* prefix,
			   const EventMapping* em, const ULong* cost)
{
    HChar *mcost = CLG_(mappingcost_as_string)(em, cost);
    VG_(fprintf)(fp, "%s%s\n", prefix, mcost);
    CLG_FREE(mcost);
}

static ULong bbs_done = 0;
static HChar* filename = 0;

static
void file_err(void)
{
   VG_(message)(Vg_UserMsg,
                "Error: can not open cache simulation output file `%s'\n",
                filename );
   VG_(exit)(1);
}

static VgFile *new_dumpfile(int tid, const HChar* trigger)
{
    Bool appending = False;
    int i;
    FullCost sum = 0;
    VgFile *fp;

    CLG_ASSERT(dumps_initialized);
    CLG_ASSERT(filename != 0);

    if (!CLG_(clo).combine_dumps) {
	i = VG_(sprintf)(filename, "%s", out_file);
    
	if (trigger)
	    i += VG_(sprintf)(filename+i, ".%d", out_counter);

	if (CLG_(clo).separate_threads)
	    VG_(sprintf)(filename+i, "-%02d", tid);

	fp = VG_(fopen)(filename, VKI_O_WRONLY|VKI_O_TRUNC, 0);
    }
    else {
	VG_(sprintf)(filename, "%s", out_file);
        fp = VG_(fopen)(filename, VKI_O_WRONLY|VKI_O_APPEND, 0);
	if (fp && out_counter>1)
	    appending = True;
    }

    if (fp == NULL) {
	fp = VG_(fopen)(filename, VKI_O_CREAT|VKI_O_WRONLY,
                        VKI_S_IRUSR|VKI_S_IWUSR);
	if (fp == NULL) {
	    file_err();
	}
    }

    CLG_DEBUG(2, "  new_dumpfile '%s'\n", filename);

    if (!appending)
	reset_dump_array();


    if (!appending) {
	
	VG_(fprintf)(fp, "version: 1\n");

	
	VG_(fprintf)(fp, "creator: callgrind-" VERSION "\n");

	
	VG_(fprintf)(fp, "pid: %d\n", VG_(getpid)());

	
	VG_(fprintf)(fp, "cmd: %s", cmdbuf);
    }

    VG_(fprintf)(fp, "\npart: %d\n", out_counter);
    if (CLG_(clo).separate_threads) {
	VG_(fprintf)(fp, "thread: %d\n", tid);
    }

    
    if (!appending) {
        VG_(fprintf)(fp, "\n");

#if 0
	
	VG_(fprintf)(fp, "\ndesc: Option: --skip-plt=%s\n",
		     CLG_(clo).skip_plt ? "yes" : "no");
	VG_(fprintf)(fp, "desc: Option: --collect-jumps=%s\n",
		     CLG_(clo).collect_jumps ? "yes" : "no");
	VG_(fprintf)(fp, "desc: Option: --separate-recs=%d\n",
		     CLG_(clo).separate_recursions);
	VG_(fprintf)(fp, "desc: Option: --separate-callers=%d\n",
		     CLG_(clo).separate_callers);

	VG_(fprintf)(fp, "desc: Option: --dump-bbs=%s\n",
		     CLG_(clo).dump_bbs ? "yes" : "no");
	VG_(fprintf)(fp, "desc: Option: --separate-threads=%s\n",
		     CLG_(clo).separate_threads ? "yes" : "no");
#endif

	(*CLG_(cachesim).dump_desc)(fp);
    }

    VG_(fprintf)(fp, "\ndesc: Timerange: Basic block %llu - %llu\n",
		 bbs_done, CLG_(stat).bb_executions);

    VG_(fprintf)(fp, "desc: Trigger: %s\n",
		 trigger ? trigger : "Program termination");

#if 0
   for (i = 0; i < N_FNCONFIG_ENTRIES; i++) {
       fnc = fnc_table[i];
       while (fnc) {
	   if (fnc->skip) {
	       VG_(fprintf)(fp, "desc: Option: --fn-skip=%s\n", fnc->name);
	   }
	   if (fnc->dump_at_enter) {
	       VG_(fprintf)(fp, "desc: Option: --fn-dump-at-enter=%s\n",
			    fnc->name);
	   }   
	   if (fnc->dump_at_leave) {
	       VG_(fprintf)(fp, "desc: Option: --fn-dump-at-leave=%s\n",
			    fnc->name);
	   }
	   if (fnc->separate_callers != CLG_(clo).separate_callers) {
	       VG_(fprintf)(fp, "desc: Option: --separate-callers%d=%s\n",
			    fnc->separate_callers, fnc->name);
	   }   
	   if (fnc->separate_recursions != CLG_(clo).separate_recursions) {
	       VG_(fprintf)(fp, "desc: Option: --separate-recs%d=%s\n",
			    fnc->separate_recursions, fnc->name);
	   }   
	   fnc = fnc->next;
       }
   }
#endif

   
   VG_(fprintf)(fp, "\npositions:%s%s%s\n",
		CLG_(clo).dump_instr ? " instr" : "",
		CLG_(clo).dump_bb    ? " bb" : "",
		CLG_(clo).dump_line  ? " line" : "");

   
   HChar *evmap = CLG_(eventmapping_as_string)(CLG_(dumpmap));
   VG_(fprintf)(fp, "events: %s\n", evmap);
   VG_(free)(evmap);

   
   sum = CLG_(get_eventset_cost)( CLG_(sets).full );
   CLG_(zero_cost)(CLG_(sets).full, sum);
   if (CLG_(clo).separate_threads) {
     thread_info* ti = CLG_(get_current_thread)();
     CLG_(add_diff_cost)(CLG_(sets).full, sum, ti->lastdump_cost,
			   ti->states.entry[0]->cost);
   }
   else {
     int t;
     thread_info** thr = CLG_(get_threads)();
     for(t=1;t<VG_N_THREADS;t++) {
       if (!thr[t]) continue;
       CLG_(add_diff_cost)(CLG_(sets).full, sum,
			  thr[t]->lastdump_cost,
			  thr[t]->states.entry[0]->cost);
     }
   }
   fprint_cost_ln(fp, "summary: ", CLG_(dumpmap), sum);

   
   CLG_(init_cost_lz)( CLG_(sets).full, &dump_total_cost );

   VG_(fprintf)(fp, "\n\n");

   if (VG_(clo_verbosity) > 1)
       VG_(message)(Vg_DebugMsg, "Dump to %s\n", filename);

   return fp;
}


static void close_dumpfile(VgFile *fp)
{
    if (fp == NULL) return;

    fprint_cost_ln(fp, "totals: ", CLG_(dumpmap),
		   dump_total_cost);
    
    CLG_(add_cost_lz)(CLG_(sets).full, 
		     &CLG_(total_cost), dump_total_cost);

    VG_(fclose)(fp);

    if (filename[0] == '.') {
	if (-1 == VG_(rename) (filename, filename+1)) {
	    
	    VG_(message)(Vg_DebugMsg, "Warning: Can not rename .%s to %s\n",
			 filename, filename);
       }
   }
}



static const HChar* print_trigger;

static void print_bbccs_of_thread(thread_info* ti)
{
  BBCC **p, **array;
  FnPos lastFnPos;
  AddrPos lastAPos;

  CLG_DEBUG(1, "+ print_bbccs(tid %d)\n", CLG_(current_tid));

  VgFile *print_fp = new_dumpfile(CLG_(current_tid), print_trigger);
  if (print_fp == NULL) {
    CLG_DEBUG(1, "- print_bbccs(tid %d): No output...\n", CLG_(current_tid));
    return;
  }

  p = array = prepare_dump();
  init_fpos(&lastFnPos);
  init_apos(&lastAPos, 0, 0, 0);

  while(1) {

    
    if (lastFnPos.cxt && ((*p==0) ||				 
			 (lastFnPos.cxt != (*p)->cxt) ||
			 (lastFnPos.rec_index != (*p)->rec_index))) {
      if (!CLG_(is_zero_cost)( CLG_(sets).full, ccSum[currSum].cost )) {
	
	fprint_apos(print_fp, &(ccSum[currSum].p), &lastAPos,
		    lastFnPos.cxt->fn[0]->file);
	fprint_fcost(print_fp, &ccSum[currSum], &lastAPos);
      }
      
      if (ccSum[currSum].p.file != lastFnPos.cxt->fn[0]->file) {
	
	print_file(print_fp, "fe=", lastFnPos.cxt->fn[0]->file);
      }
      VG_(fprintf)(print_fp, "\n");
    }
    
    if (*p == 0) break;
    
    if (print_fn_pos(print_fp, &lastFnPos, *p)) {
      
      
      init_apos(&lastAPos, 0, 0, (*p)->cxt->fn[0]->file);
      init_fcost(&ccSum[0], 0, 0, 0);
      init_fcost(&ccSum[1], 0, 0, 0);
      currSum = 0;
    }
    
    if (CLG_(clo).dump_bbs) {
	
        int i;
	ULong ecounter = (*p)->ecounter_sum;
        VG_(fprintf)(print_fp, "bb=%#lx ", (*p)->bb->offset);
	for(i = 0; i<(*p)->bb->cjmp_count;i++) {
	    VG_(fprintf)(print_fp, "%d %llu ", 
				(*p)->bb->jmp[i].instr,
				ecounter);
	    ecounter -= (*p)->jmp[i].ecounter;
	}
	VG_(fprintf)(print_fp, "%d %llu\n", 
		     (*p)->bb->instr_count,
		     ecounter);
    }
    
    fprint_bbcc(print_fp, *p, &lastAPos);
    
    p++;
  }

  close_dumpfile(print_fp);
  VG_(free)(array);
  
  
  CLG_(copy_cost)( CLG_(sets).full, ti->lastdump_cost,
		  CLG_(current_state).cost );

  CLG_DEBUG(1, "- print_bbccs(tid %d)\n", CLG_(current_tid));
}


static void print_bbccs(const HChar* trigger, Bool only_current_thread)
{
  init_dump_array();
  init_debug_cache();

  print_trigger = trigger;

  if (!CLG_(clo).separate_threads) {
    
    Int orig_tid = CLG_(current_tid);

    CLG_(switch_thread)(1);
    print_bbccs_of_thread( CLG_(get_current_thread)() );
    CLG_(switch_thread)(orig_tid);
  }
  else if (only_current_thread)
    print_bbccs_of_thread( CLG_(get_current_thread)() );
  else
    CLG_(forall_threads)(print_bbccs_of_thread);

  free_dump_array();
}


void CLG_(dump_profile)(const HChar* trigger, Bool only_current_thread)
{
   CLG_DEBUG(2, "+ dump_profile(Trigger '%s')\n",
	    trigger ? trigger : "Prg.Term.");

   CLG_(init_dumps)();

   if (VG_(clo_verbosity) > 1)
       VG_(message)(Vg_DebugMsg, "Start dumping at BB %llu (%s)...\n",
		    CLG_(stat).bb_executions,
		    trigger ? trigger : "Prg.Term.");

   out_counter++;

   print_bbccs(trigger, only_current_thread);

   bbs_done = CLG_(stat).bb_executions++;

   if (VG_(clo_verbosity) > 1)
     VG_(message)(Vg_DebugMsg, "Dumping done.\n");
}

static
void init_cmdbuf(void)
{
  SizeT size;
  Int i,j;

  
  size  = 1;  
  size += VG_(strlen)( VG_(args_the_exename) );
  for (i = 0; i < VG_(sizeXA)( VG_(args_for_client) ); i++) {
     const HChar *arg = *(HChar**)VG_(indexXA)( VG_(args_for_client), i );
     size += 1;   
     
     for(j=0; arg[j]; j++)
       switch(arg[j]) {
       case '\n':
       case '\\':
	 size++; 
       default:
	 size++;
       }
  }

  cmdbuf = CLG_MALLOC("cl.dump.ic.1", size + 1);  

  
  size = VG_(sprintf)(cmdbuf, " %s", VG_(args_the_exename));

  for(i = 0; i < VG_(sizeXA)( VG_(args_for_client) ); i++) {
     const HChar *arg = * (HChar**) VG_(indexXA)( VG_(args_for_client), i );
     cmdbuf[size++] = ' ';
     for(j=0; arg[j]; j++)
       switch(arg[j]) {
       case '\n':
	 cmdbuf[size++] = '\\';
	 cmdbuf[size++] = 'n';
	 break;
       case '\\':
	 cmdbuf[size++] = '\\';
	 cmdbuf[size++] = '\\';
	 break;
       default:
	 cmdbuf[size++] = arg[j];
	 break;
       }
  }
  cmdbuf[size] = '\0';
}

void CLG_(init_dumps)()
{
   SysRes res;

   static int thisPID = 0;
   int currentPID = VG_(getpid)();
   if (currentPID == thisPID) {
       
       CLG_ASSERT(out_file != 0);
       return;
   }
   thisPID = currentPID;
   
   if (!CLG_(clo).out_format)
     CLG_(clo).out_format = DEFAULT_OUTFORMAT;

   
   if (out_file) {
       VG_(free)(out_file);
       VG_(free)(filename);
       out_counter = 0;
   }

   
   out_file =
       VG_(expand_file_name)("--callgrind-out-file", CLG_(clo).out_format);

   
   filename = (HChar*) CLG_MALLOC("cl.dump.init_dumps.2",
                                 VG_(strlen)(out_file)+32);
       
   /* Make sure the output base file can be written.
    * This is used for the dump at program termination.
    * We stop with an error here if we can not create the
    * file: This is probably because of missing rights,
    * and trace parts wouldn't be allowed to be written, too.
    */ 
    VG_(strcpy)(filename, out_file);
    res = VG_(open)(filename, VKI_O_WRONLY|VKI_O_TRUNC, 0);
    if (sr_isError(res)) { 
	res = VG_(open)(filename, VKI_O_CREAT|VKI_O_WRONLY,
		       VKI_S_IRUSR|VKI_S_IWUSR);
	if (sr_isError(res)) {
	    file_err(); 
	}
    }
    if (!sr_isError(res)) VG_(close)( (Int)sr_Res(res) );

    if (!dumps_initialized)
	init_cmdbuf();

    dumps_initialized = True;
}
