

/*
   This file is part of Callgrind, a Valgrind tool for call graph
   profiling programs.

   Copyright (C) 2002-2013, Josef Weidendorfer (Josef.Weidendorfer@gmx.de)

   This tool is derived from and contains code from Cachegrind
   Copyright (C) 2002-2013 Nicholas Nethercote (njn@valgrind.org)

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
#include "callgrind.h"
#include "global.h"

#include "pub_tool_threadstate.h"
#include "pub_tool_gdbserver.h"
#include "pub_tool_transtab.h"       

#include "cg_branchpred.c"


CommandLineOptions CLG_(clo);
Statistics CLG_(stat);
Bool CLG_(instrument_state) = True; 

exec_state CLG_(current_state);

Int CLG_(min_line_size) = 0;



static void CLG_(init_statistics)(Statistics* s)
{
  s->call_counter        = 0;
  s->jcnd_counter        = 0;
  s->jump_counter        = 0;
  s->rec_call_counter    = 0;
  s->ret_counter         = 0;
  s->bb_executions       = 0;

  s->context_counter     = 0;
  s->bb_retranslations   = 0;

  s->distinct_objs       = 0;
  s->distinct_files      = 0;
  s->distinct_fns        = 0;
  s->distinct_contexts   = 0;
  s->distinct_bbs        = 0;
  s->distinct_bbccs      = 0;
  s->distinct_instrs     = 0;
  s->distinct_skips      = 0;

  s->bb_hash_resizes     = 0;
  s->bbcc_hash_resizes   = 0;
  s->jcc_hash_resizes    = 0;
  s->cxt_hash_resizes    = 0;
  s->fn_array_resizes    = 0;
  s->call_stack_resizes  = 0;
  s->fn_stack_resizes    = 0;

  s->full_debug_BBs      = 0;
  s->file_line_debug_BBs = 0;
  s->fn_name_debug_BBs   = 0;
  s->no_debug_BBs        = 0;
  s->bbcc_lru_misses     = 0;
  s->jcc_lru_misses      = 0;
  s->cxt_lru_misses      = 0;
  s->bbcc_clones         = 0;
}



VG_REGPARM(1)
static void log_global_event(InstrInfo* ii)
{
    ULong* cost_Bus;

    CLG_DEBUG(6, "log_global_event:  Ir  %#lx/%u\n",
              CLG_(bb_base) + ii->instr_offset, ii->instr_size);

    if (!CLG_(current_state).collect) return;

    CLG_ASSERT( (ii->eventset->mask & (1u<<EG_BUS))>0 );

    CLG_(current_state).cost[ fullOffset(EG_BUS) ]++;

    if (CLG_(current_state).nonskipped)
        cost_Bus = CLG_(current_state).nonskipped->skipped + fullOffset(EG_BUS);
    else
        cost_Bus = CLG_(cost_base) + ii->cost_offset + ii->eventset->offset[EG_BUS];
    cost_Bus[0]++;
}



static VG_REGPARM(2)
void log_cond_branch(InstrInfo* ii, Word taken)
{
    Bool miss;
    Int fullOffset_Bc;
    ULong* cost_Bc;

    CLG_DEBUG(6, "log_cond_branch:  Ir %#lx, taken %lu\n",
              CLG_(bb_base) + ii->instr_offset, taken);

    miss = 1 & do_cond_branch_predict(CLG_(bb_base) + ii->instr_offset, taken);

    if (!CLG_(current_state).collect) return;

    CLG_ASSERT( (ii->eventset->mask & (1u<<EG_BC))>0 );

    if (CLG_(current_state).nonskipped)
        cost_Bc = CLG_(current_state).nonskipped->skipped + fullOffset(EG_BC);
    else
        cost_Bc = CLG_(cost_base) + ii->cost_offset + ii->eventset->offset[EG_BC];

    fullOffset_Bc = fullOffset(EG_BC);
    CLG_(current_state).cost[ fullOffset_Bc ]++;
    cost_Bc[0]++;
    if (miss) {
        CLG_(current_state).cost[ fullOffset_Bc+1 ]++;
        cost_Bc[1]++;
    }
}

static VG_REGPARM(2)
void log_ind_branch(InstrInfo* ii, UWord actual_dst)
{
    Bool miss;
    Int fullOffset_Bi;
    ULong* cost_Bi;

    CLG_DEBUG(6, "log_ind_branch:  Ir  %#lx, dst %#lx\n",
              CLG_(bb_base) + ii->instr_offset, actual_dst);

    miss = 1 & do_ind_branch_predict(CLG_(bb_base) + ii->instr_offset, actual_dst);

    if (!CLG_(current_state).collect) return;

    CLG_ASSERT( (ii->eventset->mask & (1u<<EG_BI))>0 );

    if (CLG_(current_state).nonskipped)
        cost_Bi = CLG_(current_state).nonskipped->skipped + fullOffset(EG_BI);
    else
        cost_Bi = CLG_(cost_base) + ii->cost_offset + ii->eventset->offset[EG_BI];

    fullOffset_Bi = fullOffset(EG_BI);
    CLG_(current_state).cost[ fullOffset_Bi ]++;
    cost_Bi[0]++;
    if (miss) {
        CLG_(current_state).cost[ fullOffset_Bi+1 ]++;
        cost_Bi[1]++;
    }
}



typedef
   IRExpr
   IRAtom;

typedef
   enum {
      Ev_Ir,  
      Ev_Dr,  
      Ev_Dw,  
      Ev_Dm,  
      Ev_Bc,  
      Ev_Bi,  
      Ev_G    
   }
   EventTag;

typedef
   struct {
      EventTag   tag;
      InstrInfo* inode;
      union {
	 struct {
	 } Ir;
	 struct {
	    IRAtom* ea;
	    Int     szB;
	 } Dr;
	 struct {
	    IRAtom* ea;
	    Int     szB;
	 } Dw;
	 struct {
	    IRAtom* ea;
	    Int     szB;
	 } Dm;
         struct {
            IRAtom* taken; 
         } Bc;
         struct {
            IRAtom* dst;
         } Bi;
	 struct {
	 } G;
      } Ev;
   }
   Event;

static void init_Event ( Event* ev ) {
   VG_(memset)(ev, 0, sizeof(Event));
}

static IRAtom* get_Event_dea ( Event* ev ) {
   switch (ev->tag) {
      case Ev_Dr: return ev->Ev.Dr.ea;
      case Ev_Dw: return ev->Ev.Dw.ea;
      case Ev_Dm: return ev->Ev.Dm.ea;
      default:    tl_assert(0);
   }
}

static Int get_Event_dszB ( Event* ev ) {
   switch (ev->tag) {
      case Ev_Dr: return ev->Ev.Dr.szB;
      case Ev_Dw: return ev->Ev.Dw.szB;
      case Ev_Dm: return ev->Ev.Dm.szB;
      default:    tl_assert(0);
   }
}


#define N_EVENTS 16


typedef struct {
    
    Event events[N_EVENTS];
    Int   events_used;

    
    BB* bb;

    
    Bool seen_before;

    
    UInt ii_index;

    
    UInt instr_offset;

    
    IRSB* sbOut;
} ClgState;


static void showEvent ( Event* ev )
{
   switch (ev->tag) {
      case Ev_Ir:
	 VG_(printf)("Ir (InstrInfo %p) at +%d\n",
		     ev->inode, ev->inode->instr_offset);
	 break;
      case Ev_Dr:
	 VG_(printf)("Dr (InstrInfo %p) at +%d %d EA=",
		     ev->inode, ev->inode->instr_offset, ev->Ev.Dr.szB);
	 ppIRExpr(ev->Ev.Dr.ea);
	 VG_(printf)("\n");
	 break;
      case Ev_Dw:
	 VG_(printf)("Dw (InstrInfo %p) at +%d %d EA=",
		     ev->inode, ev->inode->instr_offset, ev->Ev.Dw.szB);
	 ppIRExpr(ev->Ev.Dw.ea);
	 VG_(printf)("\n");
	 break;
      case Ev_Dm:
	 VG_(printf)("Dm (InstrInfo %p) at +%d %d EA=",
		     ev->inode, ev->inode->instr_offset, ev->Ev.Dm.szB);
	 ppIRExpr(ev->Ev.Dm.ea);
	 VG_(printf)("\n");
	 break;
      case Ev_Bc:
         VG_(printf)("Bc %p   GA=", ev->inode);
         ppIRExpr(ev->Ev.Bc.taken);
         VG_(printf)("\n");
         break;
      case Ev_Bi:
         VG_(printf)("Bi %p  DST=", ev->inode);
         ppIRExpr(ev->Ev.Bi.dst);
         VG_(printf)("\n");
         break;
      case Ev_G:
         VG_(printf)("G  %p\n", ev->inode);
         break;
      default:
	 tl_assert(0);
	 break;
   }
}


static void flushEvents ( ClgState* clgs )
{
   Int        i, regparms, inew;
   const HChar* helperName;
   void*      helperAddr;
   IRExpr**   argv;
   IRExpr*    i_node_expr;
   IRDirty*   di;
   Event*     ev;
   Event*     ev2;
   Event*     ev3;

   if (!clgs->seen_before) {
       
       
       for(i=0; i<clgs->events_used; i++) {
	   ev  = &clgs->events[i];
	   switch(ev->tag) {
	   case Ev_Ir:
	       
	       CLG_ASSERT(ev->inode->eventset == 0);
	       ev->inode->eventset = CLG_(sets).base;
	       break;
	   case Ev_Dr:
               
	       ev->inode->eventset = CLG_(add_event_group)(ev->inode->eventset,
							   EG_DR);
	       break;
	   case Ev_Dw:
	   case Ev_Dm:
               
	       ev->inode->eventset = CLG_(add_event_group)(ev->inode->eventset,
							   EG_DW);
	       break;
           case Ev_Bc:
               
               ev->inode->eventset = CLG_(add_event_group)(ev->inode->eventset,
                                                           EG_BC);
               break;
           case Ev_Bi:
               
               ev->inode->eventset = CLG_(add_event_group)(ev->inode->eventset,
                                                           EG_BI);
               break;
	   case Ev_G:
               
	       ev->inode->eventset = CLG_(add_event_group)(ev->inode->eventset,
							   EG_BUS);
	       break;
	   default:
	       tl_assert(0);
	   }
       }
   }

   for(i = 0; i < clgs->events_used; i = inew) {

      helperName = NULL;
      helperAddr = NULL;
      argv       = NULL;
      regparms   = 0;

      tl_assert(i >= 0 && i < clgs->events_used);

      ev  = &clgs->events[i];
      ev2 = ( i < clgs->events_used-1 ? &clgs->events[i+1] : NULL );
      ev3 = ( i < clgs->events_used-2 ? &clgs->events[i+2] : NULL );

      CLG_DEBUGIF(5) {
	 VG_(printf)("   flush ");
	 showEvent( ev );
      }

      i_node_expr = mkIRExpr_HWord( (HWord)ev->inode );

      switch (ev->tag) {
	 case Ev_Ir:
	    
	    if (ev2 && ev2->tag == Ev_Dr) {
	       tl_assert(ev2->inode == ev->inode);
	       helperName = CLG_(cachesim).log_1I1Dr_name;
	       helperAddr = CLG_(cachesim).log_1I1Dr;
	       argv = mkIRExprVec_3( i_node_expr,
				     get_Event_dea(ev2),
				     mkIRExpr_HWord( get_Event_dszB(ev2) ) );
	       regparms = 3;
	       inew = i+2;
	    }
	    
	    else
	    if (ev2 && (ev2->tag == Ev_Dw || ev2->tag == Ev_Dm)) {
	       tl_assert(ev2->inode == ev->inode);
	       helperName = CLG_(cachesim).log_1I1Dw_name;
	       helperAddr = CLG_(cachesim).log_1I1Dw;
	       argv = mkIRExprVec_3( i_node_expr,
				     get_Event_dea(ev2),
				     mkIRExpr_HWord( get_Event_dszB(ev2) ) );
	       regparms = 3;
	       inew = i+2;
	    }
	    
	    else
	    if (ev2 && ev3 && ev2->tag == Ev_Ir && ev3->tag == Ev_Ir) {
	       helperName = CLG_(cachesim).log_3I0D_name;
	       helperAddr = CLG_(cachesim).log_3I0D;
	       argv = mkIRExprVec_3( i_node_expr,
				     mkIRExpr_HWord( (HWord)ev2->inode ),
				     mkIRExpr_HWord( (HWord)ev3->inode ) );
	       regparms = 3;
	       inew = i+3;
	    }
	    
	    else
	    if (ev2 && ev2->tag == Ev_Ir) {
	       helperName = CLG_(cachesim).log_2I0D_name;
	       helperAddr = CLG_(cachesim).log_2I0D;
	       argv = mkIRExprVec_2( i_node_expr,
				     mkIRExpr_HWord( (HWord)ev2->inode ) );
	       regparms = 2;
	       inew = i+2;
	    }
	    
	    else {
	       helperName = CLG_(cachesim).log_1I0D_name;
	       helperAddr = CLG_(cachesim).log_1I0D;
	       argv = mkIRExprVec_1( i_node_expr );
	       regparms = 1;
	       inew = i+1;
	    }
	    break;
	 case Ev_Dr:
	    
	    helperName = CLG_(cachesim).log_0I1Dr_name;
	    helperAddr = CLG_(cachesim).log_0I1Dr;
	    argv = mkIRExprVec_3( i_node_expr,
				  get_Event_dea(ev),
				  mkIRExpr_HWord( get_Event_dszB(ev) ) );
	    regparms = 3;
	    inew = i+1;
	    break;
	 case Ev_Dw:
	 case Ev_Dm:
	    
	    helperName = CLG_(cachesim).log_0I1Dw_name;
	    helperAddr = CLG_(cachesim).log_0I1Dw;
	    argv = mkIRExprVec_3( i_node_expr,
				  get_Event_dea(ev),
				  mkIRExpr_HWord( get_Event_dszB(ev) ) );
	    regparms = 3;
	    inew = i+1;
	    break;
         case Ev_Bc:
            
            helperName = "log_cond_branch";
            helperAddr = &log_cond_branch;
            argv = mkIRExprVec_2( i_node_expr, ev->Ev.Bc.taken );
            regparms = 2;
            inew = i+1;
            break;
         case Ev_Bi:
            
            helperName = "log_ind_branch";
            helperAddr = &log_ind_branch;
            argv = mkIRExprVec_2( i_node_expr, ev->Ev.Bi.dst );
            regparms = 2;
            inew = i+1;
            break;
         case Ev_G:
            
            helperName = "log_global_event";
            helperAddr = &log_global_event;
            argv = mkIRExprVec_1( i_node_expr );
            regparms = 1;
            inew = i+1;
            break;
	 default:
	    tl_assert(0);
      }

      CLG_DEBUGIF(5) {
	  if (inew > i+1) {
	      VG_(printf)("   merge ");
	      showEvent( ev2 );
	  }
	  if (inew > i+2) {
	      VG_(printf)("   merge ");
	      showEvent( ev3 );
	  }
	  if (helperAddr)
	      VG_(printf)("   call  %s (%p)\n",
			  helperName, helperAddr);
      }

      
      if (helperAddr == 0) continue;

      
      tl_assert(helperName);
      tl_assert(helperAddr);
      tl_assert(argv);
      di = unsafeIRDirty_0_N( regparms,
			      helperName, VG_(fnptr_to_fnentry)( helperAddr ),
			      argv );
      addStmtToIRSB( clgs->sbOut, IRStmt_Dirty(di) );
   }

   clgs->events_used = 0;
}

static void addEvent_Ir ( ClgState* clgs, InstrInfo* inode )
{
   Event* evt;
   tl_assert(clgs->seen_before || (inode->eventset == 0));
   if (!CLG_(clo).simulate_cache) return;

   if (clgs->events_used == N_EVENTS)
      flushEvents(clgs);
   tl_assert(clgs->events_used >= 0 && clgs->events_used < N_EVENTS);
   evt = &clgs->events[clgs->events_used];
   init_Event(evt);
   evt->tag      = Ev_Ir;
   evt->inode    = inode;
   clgs->events_used++;
}

static
void addEvent_Dr ( ClgState* clgs, InstrInfo* inode, Int datasize, IRAtom* ea )
{
   Event* evt;
   tl_assert(isIRAtom(ea));
   tl_assert(datasize >= 1);
   if (!CLG_(clo).simulate_cache) return;
   tl_assert(datasize <= CLG_(min_line_size));

   if (clgs->events_used == N_EVENTS)
      flushEvents(clgs);
   tl_assert(clgs->events_used >= 0 && clgs->events_used < N_EVENTS);
   evt = &clgs->events[clgs->events_used];
   init_Event(evt);
   evt->tag       = Ev_Dr;
   evt->inode     = inode;
   evt->Ev.Dr.szB = datasize;
   evt->Ev.Dr.ea  = ea;
   clgs->events_used++;
}

static
void addEvent_Dw ( ClgState* clgs, InstrInfo* inode, Int datasize, IRAtom* ea )
{
   Event* lastEvt;
   Event* evt;
   tl_assert(isIRAtom(ea));
   tl_assert(datasize >= 1);
   if (!CLG_(clo).simulate_cache) return;
   tl_assert(datasize <= CLG_(min_line_size));

   
   lastEvt = &clgs->events[clgs->events_used-1];
   if (clgs->events_used > 0
       && lastEvt->tag       == Ev_Dr
       && lastEvt->Ev.Dr.szB == datasize
       && lastEvt->inode     == inode
       && eqIRAtom(lastEvt->Ev.Dr.ea, ea))
   {
      lastEvt->tag   = Ev_Dm;
      return;
   }

   
   if (clgs->events_used == N_EVENTS)
      flushEvents(clgs);
   tl_assert(clgs->events_used >= 0 && clgs->events_used < N_EVENTS);
   evt = &clgs->events[clgs->events_used];
   init_Event(evt);
   evt->tag       = Ev_Dw;
   evt->inode     = inode;
   evt->Ev.Dw.szB = datasize;
   evt->Ev.Dw.ea  = ea;
   clgs->events_used++;
}

static
void addEvent_D_guarded ( ClgState* clgs, InstrInfo* inode,
                          Int datasize, IRAtom* ea, IRAtom* guard,
                          Bool isWrite )
{
   tl_assert(isIRAtom(ea));
   tl_assert(guard);
   tl_assert(isIRAtom(guard));
   tl_assert(datasize >= 1);
   if (!CLG_(clo).simulate_cache) return;
   tl_assert(datasize <= CLG_(min_line_size));

   tl_assert(clgs->events_used >= 0);
   flushEvents(clgs);
   tl_assert(clgs->events_used == 0);
   
   IRExpr*      i_node_expr;
   const HChar* helperName;
   void*        helperAddr;
   IRExpr**     argv;
   Int          regparms;
   IRDirty*     di;
   i_node_expr = mkIRExpr_HWord( (HWord)inode );
   helperName  = isWrite ? CLG_(cachesim).log_0I1Dw_name
                         : CLG_(cachesim).log_0I1Dr_name;
   helperAddr  = isWrite ? CLG_(cachesim).log_0I1Dw
                         : CLG_(cachesim).log_0I1Dr;
   argv        = mkIRExprVec_3( i_node_expr,
                                ea, mkIRExpr_HWord( datasize ) );
   regparms    = 3;
   di          = unsafeIRDirty_0_N(
                    regparms, 
                    helperName, VG_(fnptr_to_fnentry)( helperAddr ), 
                    argv );
   di->guard = guard;
   addStmtToIRSB( clgs->sbOut, IRStmt_Dirty(di) );
}

static
void addEvent_Bc ( ClgState* clgs, InstrInfo* inode, IRAtom* guard )
{
   Event* evt;
   tl_assert(isIRAtom(guard));
   tl_assert(typeOfIRExpr(clgs->sbOut->tyenv, guard)
             == (sizeof(HWord)==4 ? Ity_I32 : Ity_I64));
   if (!CLG_(clo).simulate_branch) return;

   if (clgs->events_used == N_EVENTS)
      flushEvents(clgs);
   tl_assert(clgs->events_used >= 0 && clgs->events_used < N_EVENTS);
   evt = &clgs->events[clgs->events_used];
   init_Event(evt);
   evt->tag         = Ev_Bc;
   evt->inode       = inode;
   evt->Ev.Bc.taken = guard;
   clgs->events_used++;
}

static
void addEvent_Bi ( ClgState* clgs, InstrInfo* inode, IRAtom* whereTo )
{
   Event* evt;
   tl_assert(isIRAtom(whereTo));
   tl_assert(typeOfIRExpr(clgs->sbOut->tyenv, whereTo)
             == (sizeof(HWord)==4 ? Ity_I32 : Ity_I64));
   if (!CLG_(clo).simulate_branch) return;

   if (clgs->events_used == N_EVENTS)
      flushEvents(clgs);
   tl_assert(clgs->events_used >= 0 && clgs->events_used < N_EVENTS);
   evt = &clgs->events[clgs->events_used];
   init_Event(evt);
   evt->tag       = Ev_Bi;
   evt->inode     = inode;
   evt->Ev.Bi.dst = whereTo;
   clgs->events_used++;
}

static
void addEvent_G ( ClgState* clgs, InstrInfo* inode )
{
   Event* evt;
   if (!CLG_(clo).collect_bus) return;

   if (clgs->events_used == N_EVENTS)
      flushEvents(clgs);
   tl_assert(clgs->events_used >= 0 && clgs->events_used < N_EVENTS);
   evt = &clgs->events[clgs->events_used];
   init_Event(evt);
   evt->tag       = Ev_G;
   evt->inode     = inode;
   clgs->events_used++;
}

static
InstrInfo* next_InstrInfo ( ClgState* clgs, UInt instr_size )
{
   InstrInfo* ii;
   tl_assert(clgs->ii_index >= 0);
   tl_assert(clgs->ii_index < clgs->bb->instr_count);
   ii = &clgs->bb->instr[ clgs->ii_index ];

   if (clgs->seen_before) {
       CLG_ASSERT(ii->instr_offset == clgs->instr_offset);
       CLG_ASSERT(ii->instr_size == instr_size);
   }
   else {
       ii->instr_offset = clgs->instr_offset;
       ii->instr_size = instr_size;
       ii->cost_offset = 0;
       ii->eventset = 0;
   }

   clgs->ii_index++;
   clgs->instr_offset += instr_size;
   CLG_(stat).distinct_instrs++;

   return ii;
}

static
UInt update_cost_offsets( ClgState* clgs )
{
    Int i;
    InstrInfo* ii;
    UInt cost_offset = 0;

    CLG_ASSERT(clgs->bb->instr_count == clgs->ii_index);
    for(i=0; i<clgs->ii_index; i++) {
	ii = &clgs->bb->instr[i];
	if (clgs->seen_before) {
	    CLG_ASSERT(ii->cost_offset == cost_offset);
	} else
	    ii->cost_offset = cost_offset;
	cost_offset += ii->eventset ? ii->eventset->size : 0;
    }

    return cost_offset;
}


#if defined(VG_BIGENDIAN)
# define CLGEndness Iend_BE
#elif defined(VG_LITTLEENDIAN)
# define CLGEndness Iend_LE
#else
# error "Unknown endianness"
#endif

static
Addr IRConst2Addr(IRConst* con)
{
    Addr addr;

    if (sizeof(Addr) == 4) {
	CLG_ASSERT( con->tag == Ico_U32 );
	addr = con->Ico.U32;
    }
    else if (sizeof(Addr) == 8) {
	CLG_ASSERT( con->tag == Ico_U64 );
	addr = con->Ico.U64;
    }
    else
	VG_(tool_panic)("Callgrind: invalid Addr type");

    return addr;
}

void CLG_(collectBlockInfo)(IRSB* sbIn,
			     UInt* instrs,
			     UInt* cjmps,
			     Bool* cjmp_inverted)
{
    Int i;
    IRStmt* st;
    Addr instrAddr =0, jumpDst;
    UInt instrLen = 0;
    Bool toNextInstr = False;

    
    
    
    Bool inPreamble = True;

    if (!sbIn) return;

    for (i = 0; i < sbIn->stmts_used; i++) {
	  st = sbIn->stmts[i];
	  if (Ist_IMark == st->tag) {
	      inPreamble = False;

	      instrAddr = st->Ist.IMark.addr;
	      instrLen  = st->Ist.IMark.len;

	      (*instrs)++;
	      toNextInstr = False;
	  }
	  if (inPreamble) continue;
	  if (Ist_Exit == st->tag) {
	      jumpDst = IRConst2Addr(st->Ist.Exit.dst);
	      toNextInstr =  (jumpDst == instrAddr + instrLen);

	      (*cjmps)++;
	  }
    }

    *cjmp_inverted = toNextInstr;
}

static
void addConstMemStoreStmt( IRSB* bbOut, UWord addr, UInt val, IRType hWordTy)
{
    addStmtToIRSB( bbOut,
		   IRStmt_Store(CLGEndness,
				IRExpr_Const(hWordTy == Ity_I32 ?
					     IRConst_U32( addr ) :
					     IRConst_U64( addr )),
				IRExpr_Const(IRConst_U32(val)) ));
}   


static
void addBBSetupCall(ClgState* clgs)
{
   IRDirty* di;
   IRExpr  *arg1, **argv;

   arg1 = mkIRExpr_HWord( (HWord)clgs->bb );
   argv = mkIRExprVec_1(arg1);
   di = unsafeIRDirty_0_N( 1, "setup_bbcc",
			      VG_(fnptr_to_fnentry)( & CLG_(setup_bbcc) ),
			      argv);
   addStmtToIRSB( clgs->sbOut, IRStmt_Dirty(di) );
}


static
IRSB* CLG_(instrument)( VgCallbackClosure* closure,
                        IRSB* sbIn,
			const VexGuestLayout* layout,
			const VexGuestExtents* vge,
                        const VexArchInfo* archinfo_host,
			IRType gWordTy, IRType hWordTy )
{
   Int        i;
   IRStmt*    st;
   Addr       origAddr;
   InstrInfo* curr_inode = NULL;
   ClgState   clgs;
   UInt       cJumps = 0;
   IRTypeEnv* tyenv = sbIn->tyenv;

   if (gWordTy != hWordTy) {
      
      VG_(tool_panic)("host/guest word size mismatch");
   }

   
   if (! CLG_(instrument_state)) {
       CLG_DEBUG(5, "instrument(BB %#lx) [Instrumentation OFF]\n",
		 (Addr)closure->readdr);
       return sbIn;
   }

   CLG_DEBUG(3, "+ instrument(BB %#lx)\n", (Addr)closure->readdr);

   
   clgs.sbOut = deepCopyIRSBExceptStmts(sbIn);

   
   i = 0;
   while (i < sbIn->stmts_used && sbIn->stmts[i]->tag != Ist_IMark) {
      addStmtToIRSB( clgs.sbOut, sbIn->stmts[i] );
      i++;
   }

   
   CLG_ASSERT(sbIn->stmts_used >0);
   CLG_ASSERT(i < sbIn->stmts_used);
   st = sbIn->stmts[i];
   CLG_ASSERT(Ist_IMark == st->tag);

   origAddr = st->Ist.IMark.addr + st->Ist.IMark.delta;
   CLG_ASSERT(origAddr == st->Ist.IMark.addr 
                          + st->Ist.IMark.delta);  

   clgs.bb = CLG_(get_bb)(origAddr, sbIn, &(clgs.seen_before));

   addBBSetupCall(&clgs);

   
   clgs.events_used = 0;
   clgs.ii_index = 0;
   clgs.instr_offset = 0;

   for (; i < sbIn->stmts_used; i++) {

      st = sbIn->stmts[i];
      CLG_ASSERT(isFlatIRStmt(st));

      switch (st->tag) {
	 case Ist_NoOp:
	 case Ist_AbiHint:
	 case Ist_Put:
	 case Ist_PutI:
	 case Ist_MBE:
	    break;

	 case Ist_IMark: {
            Addr   cia   = st->Ist.IMark.addr + st->Ist.IMark.delta;
            UInt   isize = st->Ist.IMark.len;
            CLG_ASSERT(clgs.instr_offset == cia - origAddr);
	    
	    
	    if (isize == 0) isize = VG_MIN_INSTR_SZB;

	    
	    tl_assert( (VG_MIN_INSTR_SZB <= isize && isize <= VG_MAX_INSTR_SZB)
		     || VG_CLREQ_SZB == isize );

	    
	    
	    
	    curr_inode = next_InstrInfo (&clgs, isize);

	    addEvent_Ir( &clgs, curr_inode );
	    break;
	 }

	 case Ist_WrTmp: {
	    IRExpr* data = st->Ist.WrTmp.data;
	    if (data->tag == Iex_Load) {
	       IRExpr* aexpr = data->Iex.Load.addr;
	       
	       
	       addEvent_Dr( &clgs, curr_inode,
			    sizeofIRType(data->Iex.Load.ty), aexpr );
	    }
	    break;
	 }

	 case Ist_Store: {
	    IRExpr* data  = st->Ist.Store.data;
	    IRExpr* aexpr = st->Ist.Store.addr;
	    addEvent_Dw( &clgs, curr_inode,
			 sizeofIRType(typeOfIRExpr(sbIn->tyenv, data)), aexpr );
	    break;
	 }

         case Ist_StoreG: {
            IRStoreG* sg   = st->Ist.StoreG.details;
            IRExpr*   data = sg->data;
            IRExpr*   addr = sg->addr;
            IRType    type = typeOfIRExpr(tyenv, data);
            tl_assert(type != Ity_INVALID);
            addEvent_D_guarded( &clgs, curr_inode,
                                sizeofIRType(type), addr, sg->guard,
                                True );
            break;
         }

         case Ist_LoadG: {
            IRLoadG* lg       = st->Ist.LoadG.details;
            IRType   type     = Ity_INVALID; 
            IRType   typeWide = Ity_INVALID; 
            IRExpr*  addr     = lg->addr;
            typeOfIRLoadGOp(lg->cvt, &typeWide, &type);
            tl_assert(type != Ity_INVALID);
            addEvent_D_guarded( &clgs, curr_inode,
                                sizeofIRType(type), addr, lg->guard,
                                False );
            break;
         }

	 case Ist_Dirty: {
	    Int      dataSize;
	    IRDirty* d = st->Ist.Dirty.details;
	    if (d->mFx != Ifx_None) {
	       
	       tl_assert(d->mAddr != NULL);
	       tl_assert(d->mSize != 0);
	       dataSize = d->mSize;
	       
	       
	       
	       
	       if (CLG_(clo).simulate_cache && dataSize > CLG_(min_line_size))
		  dataSize = CLG_(min_line_size);
	       if (d->mFx == Ifx_Read || d->mFx == Ifx_Modify)
		  addEvent_Dr( &clgs, curr_inode, dataSize, d->mAddr );
	       if (d->mFx == Ifx_Write || d->mFx == Ifx_Modify)
		  addEvent_Dw( &clgs, curr_inode, dataSize, d->mAddr );
	    } else {
	       tl_assert(d->mAddr == NULL);
	       tl_assert(d->mSize == 0);
	    }
	    break;
	 }

         case Ist_CAS: {
            Int    dataSize;
            IRCAS* cas = st->Ist.CAS.details;
            CLG_ASSERT(cas->addr && isIRAtom(cas->addr));
            CLG_ASSERT(cas->dataLo);
            dataSize = sizeofIRType(typeOfIRExpr(sbIn->tyenv, cas->dataLo));
            if (cas->dataHi != NULL)
               dataSize *= 2; 
            addEvent_Dr( &clgs, curr_inode, dataSize, cas->addr );
            addEvent_Dw( &clgs, curr_inode, dataSize, cas->addr );
            addEvent_G(  &clgs, curr_inode );
            break;
         }

         case Ist_LLSC: {
            IRType dataTy;
            if (st->Ist.LLSC.storedata == NULL) {
               
               dataTy = typeOfIRTemp(sbIn->tyenv, st->Ist.LLSC.result);
               addEvent_Dr( &clgs, curr_inode,
                            sizeofIRType(dataTy), st->Ist.LLSC.addr );
               
               flushEvents( &clgs );
            } else {
               
               dataTy = typeOfIRExpr(sbIn->tyenv, st->Ist.LLSC.storedata);
               addEvent_Dw( &clgs, curr_inode,
                            sizeofIRType(dataTy), st->Ist.LLSC.addr );
               addEvent_G(  &clgs, curr_inode );
            }
            break;
         }

 	 case Ist_Exit: {
            Bool guest_exit, inverted;

            if (cJumps+1 == clgs.bb->cjmp_count)
                inverted = clgs.bb->cjmp_inverted;
            else
                inverted = False;

            
            guest_exit = (st->Ist.Exit.jk == Ijk_Boring) ||
                         (st->Ist.Exit.jk == Ijk_Call) ||
                         (st->Ist.Exit.jk == Ijk_Ret);

            if (guest_exit) {
                IRType   tyW    = hWordTy;
                IROp     widen  = tyW==Ity_I32  ? Iop_1Uto32  : Iop_1Uto64;
                IROp     opXOR  = tyW==Ity_I32  ? Iop_Xor32   : Iop_Xor64;
                IRTemp   guard1 = newIRTemp(clgs.sbOut->tyenv, Ity_I1);
                IRTemp   guardW = newIRTemp(clgs.sbOut->tyenv, tyW);
                IRTemp   guard  = newIRTemp(clgs.sbOut->tyenv, tyW);
                IRExpr*  one    = tyW==Ity_I32 ? IRExpr_Const(IRConst_U32(1))
                                               : IRExpr_Const(IRConst_U64(1));

                
                addStmtToIRSB( clgs.sbOut,
                               IRStmt_WrTmp( guard1, st->Ist.Exit.guard ));
                addStmtToIRSB( clgs.sbOut,
                               IRStmt_WrTmp( guardW,
                                             IRExpr_Unop(widen,
                                                         IRExpr_RdTmp(guard1))) );
                
                addStmtToIRSB(
                        clgs.sbOut,
                        IRStmt_WrTmp(
                                guard,
                                inverted ? IRExpr_Binop(opXOR, IRExpr_RdTmp(guardW), one)
                                    : IRExpr_RdTmp(guardW)
                                    ));
                
                addEvent_Bc( &clgs, curr_inode, IRExpr_RdTmp(guard) );
            }

	    flushEvents( &clgs );

	    CLG_ASSERT(clgs.ii_index>0);
	    if (!clgs.seen_before) {
	      ClgJumpKind jk;

	      if      (st->Ist.Exit.jk == Ijk_Call) jk = jk_Call;
	      else if (st->Ist.Exit.jk == Ijk_Ret)  jk = jk_Return;
	      else {
		if (IRConst2Addr(st->Ist.Exit.dst) ==
		    origAddr + curr_inode->instr_offset + curr_inode->instr_size)
		  jk = jk_None;
		else
		  jk = jk_Jump;
	      }

	      clgs.bb->jmp[cJumps].instr = clgs.ii_index-1;
	      clgs.bb->jmp[cJumps].jmpkind = jk;
	    }

	    UInt val = inverted ? cJumps+1 : cJumps;
	    addConstMemStoreStmt( clgs.sbOut,
				  (UWord) &CLG_(current_state).jmps_passed,
				  val, hWordTy);
	    cJumps++;

	    break;
	 }

	 default:
	    tl_assert(0);
	    break;
      }

      
      addStmtToIRSB( clgs.sbOut, st );

      CLG_DEBUGIF(5) {
	 VG_(printf)("   pass  ");
	 ppIRStmt(st);
	 VG_(printf)("\n");
      }
   }

   if ((sbIn->jumpkind == Ijk_Boring) || (sbIn->jumpkind == Ijk_Call)) {
      if (0) { ppIRExpr( sbIn->next ); VG_(printf)("\n"); }
      switch (sbIn->next->tag) {
         case Iex_Const:
            break; 
         case Iex_RdTmp:
            
            addEvent_Bi( &clgs, curr_inode, sbIn->next );
            break;
         default:
            tl_assert(0);
      }
   }

   
   flushEvents( &clgs );

   if (cJumps>0) {
      UInt jmps_passed = cJumps;
      if (clgs.bb->cjmp_inverted) jmps_passed--;
      addConstMemStoreStmt( clgs.sbOut,
			    (UWord) &CLG_(current_state).jmps_passed,
			    jmps_passed, hWordTy);
   }
   CLG_ASSERT(clgs.bb->cjmp_count == cJumps);
   CLG_ASSERT(clgs.bb->instr_count == clgs.ii_index);

   
   {
     ClgJumpKind jk;

     if      (sbIn->jumpkind == Ijk_Call) jk = jk_Call;
     else if (sbIn->jumpkind == Ijk_Ret)  jk = jk_Return;
     else {
       jk = jk_Jump;
       if ((sbIn->next->tag == Iex_Const) &&
	   (IRConst2Addr(sbIn->next->Iex.Const.con) ==
	    origAddr + clgs.instr_offset))
	 jk = jk_None;
     }
     clgs.bb->jmp[cJumps].jmpkind = jk;
     clgs.bb->jmp[cJumps].instr = clgs.ii_index-1;
   }

   
   if (clgs.bb->cjmp_inverted) {
     ClgJumpKind jk;
     UInt instr;

     jk = clgs.bb->jmp[cJumps].jmpkind;
     clgs.bb->jmp[cJumps].jmpkind = clgs.bb->jmp[cJumps-1].jmpkind;
     clgs.bb->jmp[cJumps-1].jmpkind = jk;
     instr = clgs.bb->jmp[cJumps].instr;
     clgs.bb->jmp[cJumps].instr = clgs.bb->jmp[cJumps-1].instr;
     clgs.bb->jmp[cJumps-1].instr = instr;
   }

   if (clgs.seen_before) {
       CLG_ASSERT(clgs.bb->cost_count == update_cost_offsets(&clgs));
       CLG_ASSERT(clgs.bb->instr_len == clgs.instr_offset);
   }
   else {
       clgs.bb->cost_count = update_cost_offsets(&clgs);
       clgs.bb->instr_len = clgs.instr_offset;
   }

   CLG_DEBUG(3, "- instrument(BB %#lx): byteLen %u, CJumps %u, CostLen %u\n",
	     origAddr, clgs.bb->instr_len,
	     clgs.bb->cjmp_count, clgs.bb->cost_count);
   if (cJumps>0) {
       CLG_DEBUG(3, "                     [ ");
       for (i=0;i<cJumps;i++)
	   CLG_DEBUG(3, "%d ", clgs.bb->jmp[i].instr);
       CLG_DEBUG(3, "], last inverted: %s \n",
		 clgs.bb->cjmp_inverted ? "yes":"no");
   }

  return clgs.sbOut;
}


static
void clg_discard_superblock_info ( Addr orig_addr, VexGuestExtents vge )
{
    tl_assert(vge.n_used > 0);

   if (0)
      VG_(printf)( "discard_superblock_info: %p, %p, %llu\n",
                   (void*)orig_addr,
                   (void*)vge.base[0], (ULong)vge.len[0]);

   
   
   
   
   CLG_(delete_bb)(vge.base[0]);
}





static void zero_thread_cost(thread_info* t)
{
  Int i;

  for(i = 0; i < CLG_(current_call_stack).sp; i++) {
    if (!CLG_(current_call_stack).entry[i].jcc) continue;

    
    CLG_(copy_cost)( CLG_(sets).full, 
		    CLG_(current_call_stack).entry[i].enter_cost,
		    CLG_(current_state).cost );
    CLG_(current_call_stack).entry[i].jcc->call_counter = 0;
  }

  CLG_(forall_bbccs)(CLG_(zero_bbcc));

  
  CLG_(copy_cost)( CLG_(sets).full, 
		  t->lastdump_cost, CLG_(current_state).cost );
}

void CLG_(zero_all_cost)(Bool only_current_thread)
{
  if (VG_(clo_verbosity) > 1)
    VG_(message)(Vg_DebugMsg, "  Zeroing costs...\n");

  if (only_current_thread)
    zero_thread_cost(CLG_(get_current_thread)());
  else
    CLG_(forall_threads)(zero_thread_cost);

  if (VG_(clo_verbosity) > 1)
    VG_(message)(Vg_DebugMsg, "  ...done\n");
}

static
void unwind_thread(thread_info* t)
{
  
  while(CLG_(current_state).sig !=0)
    CLG_(post_signal)(CLG_(current_tid),CLG_(current_state).sig);

  
  while(CLG_(current_call_stack).sp>0)
    CLG_(pop_call_stack)();

  
  CLG_(init_exec_state)( &CLG_(current_state) );
  CLG_(current_fn_stack).top = CLG_(current_fn_stack).bottom;
}

static
void zero_state_cost(thread_info* t)
{
    CLG_(zero_cost)( CLG_(sets).full, CLG_(current_state).cost );
}

void CLG_(set_instrument_state)(const HChar* reason, Bool state)
{
  if (CLG_(instrument_state) == state) {
    CLG_DEBUG(2, "%s: instrumentation already %s\n",
	     reason, state ? "ON" : "OFF");
    return;
  }
  CLG_(instrument_state) = state;
  CLG_DEBUG(2, "%s: Switching instrumentation %s ...\n",
	   reason, state ? "ON" : "OFF");

  VG_(discard_translations_safely)( (Addr)0x1000, ~(SizeT)0xfff, "callgrind");

  
  CLG_(forall_threads)(unwind_thread);
  CLG_(forall_threads)(zero_state_cost);
  (*CLG_(cachesim).clear)();

  if (VG_(clo_verbosity) > 1)
    VG_(message)(Vg_DebugMsg, "%s: instrumentation switched %s\n",
		 reason, state ? "ON" : "OFF");
}

static void dump_state_of_thread_togdb(thread_info* ti)
{
    static FullCost sum = 0, tmp = 0;
    Int t, i;
    BBCC *from, *to;
    call_entry* ce;
    HChar *mcost;

    t = CLG_(current_tid);
    CLG_(init_cost_lz)( CLG_(sets).full, &sum );
    CLG_(copy_cost_lz)( CLG_(sets).full, &tmp, ti->lastdump_cost );
    CLG_(add_diff_cost)( CLG_(sets).full, sum, ti->lastdump_cost,
			 ti->states.entry[0]->cost);
    CLG_(copy_cost)( CLG_(sets).full, ti->lastdump_cost, tmp );
    mcost = CLG_(mappingcost_as_string)(CLG_(dumpmap), sum);
    VG_(gdb_printf)("events-%d: %s\n", t, mcost);
    VG_(free)(mcost);
    VG_(gdb_printf)("frames-%d: %d\n", t, CLG_(current_call_stack).sp);

    ce = 0;
    for(i = 0; i < CLG_(current_call_stack).sp; i++) {
      ce = CLG_(get_call_entry)(i);
      
      if (!ce->jcc) continue;
      
      from = ce->jcc->from;
      VG_(gdb_printf)("function-%d-%d: %s\n",t, i, from->cxt->fn[0]->name);
      VG_(gdb_printf)("calls-%d-%d: %llu\n",t, i, ce->jcc->call_counter);
      
      
      CLG_(copy_cost)( CLG_(sets).full, sum, ce->jcc->cost );
      CLG_(copy_cost)( CLG_(sets).full, tmp, ce->enter_cost );
      CLG_(add_diff_cost)( CLG_(sets).full, sum,
			  ce->enter_cost, CLG_(current_state).cost );
      CLG_(copy_cost)( CLG_(sets).full, ce->enter_cost, tmp );
      
      mcost = CLG_(mappingcost_as_string)(CLG_(dumpmap), sum);
      VG_(gdb_printf)("events-%d-%d: %s\n",t, i, mcost);
      VG_(free)(mcost);
    }
    if (ce && ce->jcc) {
      to = ce->jcc->to;
      VG_(gdb_printf)("function-%d-%d: %s\n",t, i, to->cxt->fn[0]->name );
    }
}

static void dump_state_togdb(void)
{
    thread_info** th;
    int t;
    Int orig_tid = CLG_(current_tid);

    VG_(gdb_printf)("instrumentation: %s\n",
		    CLG_(instrument_state) ? "on":"off");
    if (!CLG_(instrument_state)) return;

    VG_(gdb_printf)("executed-bbs: %llu\n", CLG_(stat).bb_executions);
    VG_(gdb_printf)("executed-calls: %llu\n", CLG_(stat).call_counter);
    VG_(gdb_printf)("distinct-bbs: %d\n", CLG_(stat).distinct_bbs);
    VG_(gdb_printf)("distinct-calls: %d\n", CLG_(stat).distinct_jccs);
    VG_(gdb_printf)("distinct-functions: %d\n", CLG_(stat).distinct_fns);
    VG_(gdb_printf)("distinct-contexts: %d\n", CLG_(stat).distinct_contexts);

    
    HChar *evmap = CLG_(eventmapping_as_string)(CLG_(dumpmap));
    VG_(gdb_printf)("events: %s\n", evmap);
    VG_(free)(evmap);
    
    VG_(gdb_printf)("part: %d\n", CLG_(get_dump_counter)());
		
    
    th = CLG_(get_threads)();
    VG_(gdb_printf)("threads:");
    for(t=1;t<VG_N_THREADS;t++) {
	if (!th[t]) continue;
	VG_(gdb_printf)(" %d", t);
    }
    VG_(gdb_printf)("\n");
    VG_(gdb_printf)("current-tid: %d\n", orig_tid);
    CLG_(forall_threads)(dump_state_of_thread_togdb);
}

  
static void print_monitor_help ( void )
{
   VG_(gdb_printf) ("\n");
   VG_(gdb_printf) ("callgrind monitor commands:\n");
   VG_(gdb_printf) ("  dump [<dump_hint>]\n");
   VG_(gdb_printf) ("        dump counters\n");
   VG_(gdb_printf) ("  zero\n");
   VG_(gdb_printf) ("        zero counters\n");
   VG_(gdb_printf) ("  status\n");
   VG_(gdb_printf) ("        print status\n");
   VG_(gdb_printf) ("  instrumentation [on|off]\n");
   VG_(gdb_printf) ("        get/set (if on/off given) instrumentation state\n");
   VG_(gdb_printf) ("\n");
}

static Bool handle_gdb_monitor_command (ThreadId tid, const HChar *req)
{
   HChar* wcmd;
   HChar s[VG_(strlen(req)) + 1]; 
   HChar *ssaveptr;

   VG_(strcpy) (s, req);

   wcmd = VG_(strtok_r) (s, " ", &ssaveptr);
   switch (VG_(keyword_id) ("help dump zero status instrumentation", 
                            wcmd, kwd_report_duplicated_matches)) {
   case -2: 
      return True;
   case -1: 
      return False;
   case  0: 
      print_monitor_help();
      return True;
   case  1: { 
      CLG_(dump_profile)(req, False);
      return True;
   }
   case  2: { 
      CLG_(zero_all_cost)(False);
      return True;
   }

   case 3: { 
     HChar* arg = VG_(strtok_r) (0, " ", &ssaveptr);
     if (arg && (VG_(strcmp)(arg, "internal") == 0)) {
       
       dump_state_togdb();
       return True;
     }

     if (!CLG_(instrument_state)) {
       VG_(gdb_printf)("No status available as instrumentation is switched off\n");
     } else {
       
       thread_info** th = CLG_(get_threads)();
       Int t, tcount = 0;
       for(t=1;t<VG_N_THREADS;t++)
	 if (th[t]) tcount++;
       VG_(gdb_printf)("%d thread(s) running.\n", tcount);
     }
     return True;
   }

   case 4: { 
     HChar* arg = VG_(strtok_r) (0, " ", &ssaveptr);
     if (!arg) {
       VG_(gdb_printf)("instrumentation: %s\n",
		       CLG_(instrument_state) ? "on":"off");
     }
     else
       CLG_(set_instrument_state)("Command", VG_(strcmp)(arg,"off")!=0);
     return True;
   }

   default: 
      tl_assert(0);
      return False;
   }
}

static
Bool CLG_(handle_client_request)(ThreadId tid, UWord *args, UWord *ret)
{
   if (!VG_IS_TOOL_USERREQ('C','T',args[0])
       && VG_USERREQ__GDB_MONITOR_COMMAND   != args[0])
      return False;

   switch(args[0]) {
   case VG_USERREQ__DUMP_STATS:     
      CLG_(dump_profile)("Client Request", True);
      *ret = 0;                 
      break;

   case VG_USERREQ__DUMP_STATS_AT:
     {
       const HChar *arg = (HChar*)args[1];
       HChar buf[30 + VG_(strlen)(arg)];    
       VG_(sprintf)(buf,"Client Request: %s", arg);
       CLG_(dump_profile)(buf, True);
       *ret = 0;                 
     }
     break;

   case VG_USERREQ__ZERO_STATS:
     CLG_(zero_all_cost)(True);
      *ret = 0;                 
      break;

   case VG_USERREQ__TOGGLE_COLLECT:
     CLG_(current_state).collect = !CLG_(current_state).collect;
     CLG_DEBUG(2, "Client Request: toggled collection state to %s\n",
	      CLG_(current_state).collect ? "ON" : "OFF");
     *ret = 0;                 
     break;

   case VG_USERREQ__START_INSTRUMENTATION:
     CLG_(set_instrument_state)("Client Request", True);
     *ret = 0;                 
     break;

   case VG_USERREQ__STOP_INSTRUMENTATION:
     CLG_(set_instrument_state)("Client Request", False);
     *ret = 0;                 
     break;

   case VG_USERREQ__GDB_MONITOR_COMMAND: {
      Bool handled = handle_gdb_monitor_command (tid, (HChar*)args[1]);
      if (handled)
         *ret = 1;
      else
         *ret = 0;
      return handled;
   }
   default:
      return False;
   }

   return True;
}



#if CLG_MICROSYSTIME
ULong *syscalltime;
#else
UInt *syscalltime;
#endif

static
void CLG_(pre_syscalltime)(ThreadId tid, UInt syscallno,
                           UWord* args, UInt nArgs)
{
  if (CLG_(clo).collect_systime) {
#if CLG_MICROSYSTIME
    struct vki_timeval tv_now;
    VG_(gettimeofday)(&tv_now, NULL);
    syscalltime[tid] = tv_now.tv_sec * 1000000ULL + tv_now.tv_usec;
#else
    syscalltime[tid] = VG_(read_millisecond_timer)();
#endif
  }
}

static
void CLG_(post_syscalltime)(ThreadId tid, UInt syscallno,
                            UWord* args, UInt nArgs, SysRes res)
{
  if (CLG_(clo).collect_systime &&
      CLG_(current_state).bbcc) {
      Int o;
#if CLG_MICROSYSTIME
    struct vki_timeval tv_now;
    ULong diff;
    
    VG_(gettimeofday)(&tv_now, NULL);
    diff = (tv_now.tv_sec * 1000000ULL + tv_now.tv_usec) - syscalltime[tid];
#else
    UInt diff = VG_(read_millisecond_timer)() - syscalltime[tid];
#endif  

    
    o = fullOffset(EG_SYS);
    CLG_ASSERT(o>=0);
    CLG_DEBUG(0,"   Time (Off %d) for Syscall %u: %llu\n", o, syscallno,
              (ULong)diff);
    
    CLG_(current_state).cost[o] ++;
    CLG_(current_state).cost[o+1] += diff;
    if (!CLG_(current_state).bbcc->skipped)
      CLG_(init_cost_lz)(CLG_(sets).full,
			&(CLG_(current_state).bbcc->skipped));
    CLG_(current_state).bbcc->skipped[o] ++;
    CLG_(current_state).bbcc->skipped[o+1] += diff;
  }
}

static UInt ULong_width(ULong n)
{
   UInt w = 0;
   while (n > 0) {
      n = n / 10;
      w++;
   }
   if (w == 0) w = 1;
   return w + (w-1)/3;   
}

static
void branchsim_printstat(int l1, int l2, int l3)
{
    static HChar fmt[128];    
    FullCost total;
    ULong Bc_total_b, Bc_total_mp, Bi_total_b, Bi_total_mp;
    ULong B_total_b, B_total_mp;

    total = CLG_(total_cost);
    Bc_total_b  = total[ fullOffset(EG_BC)   ];
    Bc_total_mp = total[ fullOffset(EG_BC)+1 ];
    Bi_total_b  = total[ fullOffset(EG_BI)   ];
    Bi_total_mp = total[ fullOffset(EG_BI)+1 ];

    
    VG_(sprintf)(fmt, "%%s %%,%dllu  (%%,%dllu cond + %%,%dllu ind)\n",
                 l1, l2, l3);

    if (0 == Bc_total_b)  Bc_total_b = 1;
    if (0 == Bi_total_b)  Bi_total_b = 1;
    B_total_b  = Bc_total_b  + Bi_total_b;
    B_total_mp = Bc_total_mp + Bi_total_mp;

    VG_(umsg)("\n");
    VG_(umsg)(fmt, "Branches:     ",
              B_total_b, Bc_total_b, Bi_total_b);

    VG_(umsg)(fmt, "Mispredicts:  ",
              B_total_mp, Bc_total_mp, Bi_total_mp);

    VG_(umsg)("Mispred rate:  %*.1f%% (%*.1f%%     + %*.1f%%   )\n",
              l1, B_total_mp  * 100.0 / B_total_b,
              l2, Bc_total_mp * 100.0 / Bc_total_b,
              l3, Bi_total_mp * 100.0 / Bi_total_b);
}

static
void clg_print_stats(void)
{
   int BB_lookups =
     CLG_(stat).full_debug_BBs +
     CLG_(stat).fn_name_debug_BBs +
     CLG_(stat).file_line_debug_BBs +
     CLG_(stat).no_debug_BBs;

   
   VG_(message)(Vg_DebugMsg, "Distinct objects: %d\n",
		CLG_(stat).distinct_objs);
   VG_(message)(Vg_DebugMsg, "Distinct files:   %d\n",
		CLG_(stat).distinct_files);
   VG_(message)(Vg_DebugMsg, "Distinct fns:     %d\n",
		CLG_(stat).distinct_fns);
   VG_(message)(Vg_DebugMsg, "Distinct contexts:%d\n",
		CLG_(stat).distinct_contexts);
   VG_(message)(Vg_DebugMsg, "Distinct BBs:     %d\n",
		CLG_(stat).distinct_bbs);
   VG_(message)(Vg_DebugMsg, "Cost entries:     %d (Chunks %d)\n",
		CLG_(costarray_entries), CLG_(costarray_chunks));
   VG_(message)(Vg_DebugMsg, "Distinct BBCCs:   %d\n",
		CLG_(stat).distinct_bbccs);
   VG_(message)(Vg_DebugMsg, "Distinct JCCs:    %d\n",
		CLG_(stat).distinct_jccs);
   VG_(message)(Vg_DebugMsg, "Distinct skips:   %d\n",
		CLG_(stat).distinct_skips);
   VG_(message)(Vg_DebugMsg, "BB lookups:       %d\n",
		BB_lookups);
   if (BB_lookups>0) {
      VG_(message)(Vg_DebugMsg, "With full      debug info:%3d%% (%d)\n",
		   CLG_(stat).full_debug_BBs    * 100 / BB_lookups,
		   CLG_(stat).full_debug_BBs);
      VG_(message)(Vg_DebugMsg, "With file/line debug info:%3d%% (%d)\n",
		   CLG_(stat).file_line_debug_BBs * 100 / BB_lookups,
		   CLG_(stat).file_line_debug_BBs);
      VG_(message)(Vg_DebugMsg, "With fn name   debug info:%3d%% (%d)\n",
		   CLG_(stat).fn_name_debug_BBs * 100 / BB_lookups,
		   CLG_(stat).fn_name_debug_BBs);
      VG_(message)(Vg_DebugMsg, "With no        debug info:%3d%% (%d)\n",
		   CLG_(stat).no_debug_BBs      * 100 / BB_lookups,
		   CLG_(stat).no_debug_BBs);
   }
   VG_(message)(Vg_DebugMsg, "BBCC Clones:       %d\n",
		CLG_(stat).bbcc_clones);
   VG_(message)(Vg_DebugMsg, "BBs Retranslated:  %d\n",
		CLG_(stat).bb_retranslations);
   VG_(message)(Vg_DebugMsg, "Distinct instrs:   %d\n",
		CLG_(stat).distinct_instrs);

   VG_(message)(Vg_DebugMsg, "LRU Contxt Misses: %d\n",
		CLG_(stat).cxt_lru_misses);
   VG_(message)(Vg_DebugMsg, "LRU BBCC Misses:   %d\n",
		CLG_(stat).bbcc_lru_misses);
   VG_(message)(Vg_DebugMsg, "LRU JCC Misses:    %d\n",
		CLG_(stat).jcc_lru_misses);
   VG_(message)(Vg_DebugMsg, "BBs Executed:      %llu\n",
		CLG_(stat).bb_executions);
   VG_(message)(Vg_DebugMsg, "Calls:             %llu\n",
		CLG_(stat).call_counter);
   VG_(message)(Vg_DebugMsg, "CondJMP followed:  %llu\n",
		CLG_(stat).jcnd_counter);
   VG_(message)(Vg_DebugMsg, "Boring JMPs:       %llu\n",
		CLG_(stat).jump_counter);
   VG_(message)(Vg_DebugMsg, "Recursive calls:   %llu\n",
		CLG_(stat).rec_call_counter);
   VG_(message)(Vg_DebugMsg, "Returns:           %llu\n",
		CLG_(stat).ret_counter);
}


static
void finish(void)
{
  HChar fmt[128];    
  Int l1, l2, l3;
  FullCost total;

  CLG_DEBUG(0, "finish()\n");

  (*CLG_(cachesim).finish)();

  CLG_(forall_threads)(unwind_thread);

  CLG_(dump_profile)(0, False);

  if (VG_(clo_verbosity) == 0) return;
  
  if (VG_(clo_stats)) {
    VG_(message)(Vg_DebugMsg, "\n");
    clg_print_stats();
    VG_(message)(Vg_DebugMsg, "\n");
  }

  HChar *evmap = CLG_(eventmapping_as_string)(CLG_(dumpmap));
  VG_(message)(Vg_UserMsg, "Events    : %s\n", evmap);
  VG_(free)(evmap);
  HChar *mcost = CLG_(mappingcost_as_string)(CLG_(dumpmap), CLG_(total_cost));
  VG_(message)(Vg_UserMsg, "Collected : %s\n", mcost);
  VG_(free)(mcost);
  VG_(message)(Vg_UserMsg, "\n");

  
  total = CLG_(total_cost);
  l1 = ULong_width( total[fullOffset(EG_IR)] );
  l2 = l3 = 0;
  if (CLG_(clo).simulate_cache) {
      l2 = ULong_width( total[fullOffset(EG_DR)] );
      l3 = ULong_width( total[fullOffset(EG_DW)] );
  }
  if (CLG_(clo).simulate_branch) {
      int l2b = ULong_width( total[fullOffset(EG_BC)] );
      int l3b = ULong_width( total[fullOffset(EG_BI)] );
      if (l2b > l2) l2 = l2b;
      if (l3b > l3) l3 = l3b;
  }

  
  VG_(sprintf)(fmt, "%%s %%,%dllu\n", l1);

  
  VG_(umsg)(fmt, "I   refs:     ", total[fullOffset(EG_IR)] );

  if (CLG_(clo).simulate_cache)
      (*CLG_(cachesim).printstat)(l1, l2, l3);

  if (CLG_(clo).simulate_branch)
      branchsim_printstat(l1, l2, l3);

}


void CLG_(fini)(Int exitcode)
{
  finish();
}



static void clg_start_client_code_callback ( ThreadId tid, ULong blocks_done )
{
   static ULong last_blocks_done = 0;

   if (0)
      VG_(printf)("%d R %llu\n", (Int)tid, blocks_done);

   
   if (blocks_done - last_blocks_done < 5000) return;
   last_blocks_done = blocks_done;

   CLG_(run_thread)( tid );
}

static
void CLG_(post_clo_init)(void)
{
   if (VG_(clo_vex_control).iropt_register_updates_default
       != VexRegUpdSpAtMemAccess) {
      CLG_DEBUG(1, " Using user specified value for "
                "--vex-iropt-register-updates\n");
   } else {
      CLG_DEBUG(1, 
                " Using default --vex-iropt-register-updates="
                "sp-at-mem-access\n");
   }

   if (VG_(clo_px_file_backed) != VexRegUpdSpAtMemAccess) {
      CLG_DEBUG(1, " Using user specified value for "
                "--px-file-backed\n");
   } else {
      CLG_DEBUG(1, 
                " Using default --px-file-backed="
                "sp-at-mem-access\n");
   }

   if (VG_(clo_vex_control).iropt_unroll_thresh != 0) {
      VG_(message)(Vg_UserMsg, 
                   "callgrind only works with --vex-iropt-unroll-thresh=0\n"
                   "=> resetting it back to 0\n");
      VG_(clo_vex_control).iropt_unroll_thresh = 0;   
   }
   if (VG_(clo_vex_control).guest_chase_thresh != 0) {
      VG_(message)(Vg_UserMsg,
                   "callgrind only works with --vex-guest-chase-thresh=0\n"
                   "=> resetting it back to 0\n");
      VG_(clo_vex_control).guest_chase_thresh = 0; 
   }
   
   CLG_DEBUG(1, "  dump threads: %s\n", CLG_(clo).separate_threads ? "Yes":"No");
   CLG_DEBUG(1, "  call sep. : %d\n", CLG_(clo).separate_callers);
   CLG_DEBUG(1, "  rec. sep. : %d\n", CLG_(clo).separate_recursions);

   if (!CLG_(clo).dump_line && !CLG_(clo).dump_instr && !CLG_(clo).dump_bb) {
       VG_(message)(Vg_UserMsg, "Using source line as position.\n");
       CLG_(clo).dump_line = True;
   }

   CLG_(init_dumps)();

   (*CLG_(cachesim).post_clo_init)();

   CLG_(init_eventsets)();
   CLG_(init_statistics)(& CLG_(stat));
   CLG_(init_cost_lz)( CLG_(sets).full, &CLG_(total_cost) );

   
   CLG_(init_obj_table)();
   CLG_(init_cxt_table)();
   CLG_(init_bb_hash)();

   CLG_(init_threads)();
   CLG_(run_thread)(1);

   CLG_(instrument_state) = CLG_(clo).instrument_atstart;

   if (VG_(clo_verbosity > 0)) {
      VG_(message)(Vg_UserMsg,
                   "For interactive control, run 'callgrind_control%s%s -h'.\n",
                   (VG_(arg_vgdb_prefix) ? " " : ""),
                   (VG_(arg_vgdb_prefix) ? VG_(arg_vgdb_prefix) : ""));
   }
}

static
void CLG_(pre_clo_init)(void)
{
    VG_(details_name)            ("Callgrind");
    VG_(details_version)         (NULL);
    VG_(details_description)     ("a call-graph generating cache profiler");
    VG_(details_copyright_author)("Copyright (C) 2002-2013, and GNU GPL'd, "
				  "by Josef Weidendorfer et al.");
    VG_(details_bug_reports_to)  (VG_BUGS_TO);
    VG_(details_avg_translation_sizeB) ( 500 );

    VG_(clo_vex_control).iropt_register_updates_default
       = VG_(clo_px_file_backed)
       = VexRegUpdSpAtMemAccess; 

    VG_(clo_vex_control).iropt_unroll_thresh = 0;   
    VG_(clo_vex_control).guest_chase_thresh = 0;    

    VG_(basic_tool_funcs)        (CLG_(post_clo_init),
                                  CLG_(instrument),
                                  CLG_(fini));

    VG_(needs_superblock_discards)(clg_discard_superblock_info);


    VG_(needs_command_line_options)(CLG_(process_cmd_line_option),
				    CLG_(print_usage),
				    CLG_(print_debug_usage));

    VG_(needs_client_requests)(CLG_(handle_client_request));
    VG_(needs_print_stats)    (clg_print_stats);
    VG_(needs_syscall_wrapper)(CLG_(pre_syscalltime),
			       CLG_(post_syscalltime));

    VG_(track_start_client_code)  ( & clg_start_client_code_callback );
    VG_(track_pre_deliver_signal) ( & CLG_(pre_signal) );
    VG_(track_post_deliver_signal)( & CLG_(post_signal) );

    CLG_(set_clo_defaults)();

    syscalltime = CLG_MALLOC("cl.main.pci.1",
                             VG_N_THREADS * sizeof syscalltime[0]);
    for (UInt i = 0; i < VG_N_THREADS; ++i) {
       syscalltime[i] = 0;
    }
}

VG_DETERMINE_INTERFACE_VERSION(CLG_(pre_clo_init))

