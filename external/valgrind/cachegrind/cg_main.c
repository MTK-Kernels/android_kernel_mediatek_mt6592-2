

/*
   This file is part of Cachegrind, a Valgrind tool for cache
   profiling programs.

   Copyright (C) 2002-2013 Nicholas Nethercote
      njn@valgrind.org

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

#include "pub_tool_basics.h"
#include "pub_tool_debuginfo.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_libcfile.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_libcproc.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_options.h"
#include "pub_tool_oset.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_xarray.h"
#include "pub_tool_clientstate.h"
#include "pub_tool_machine.h"      

#include "cg_arch.h"
#include "cg_sim.c"
#include "cg_branchpred.c"


#define DEBUG_CG 0


static Bool  clo_cache_sim  = True;  
static Bool  clo_branch_sim = False; 
static const HChar* clo_cachegrind_out_file = "cachegrind.out.%p";


static Int min_line_size = 0; 


typedef
   struct {
      ULong a;  
      ULong m1; 
      ULong mL; 
   }
   CacheCC;

typedef
   struct {
      ULong b;  
      ULong mp; 
   }
   BranchCC;


typedef struct {
   HChar* file;
   const HChar* fn;
   Int    line;
}
CodeLoc;

typedef struct {
   CodeLoc  loc; 
   CacheCC  Ir;  
   CacheCC  Dr;  
   CacheCC  Dw;  
   BranchCC Bc;  
   BranchCC Bi;  
} LineCC;

static Word cmp_CodeLoc_LineCC(const void *vloc, const void *vcc)
{
   Word res;
   const CodeLoc* a = (const CodeLoc*)vloc;
   const CodeLoc* b = &(((const LineCC*)vcc)->loc);

   res = VG_(strcmp)(a->file, b->file);
   if (0 != res)
      return res;

   res = VG_(strcmp)(a->fn, b->fn);
   if (0 != res)
      return res;

   return a->line - b->line;
}

static OSet* CC_table;


typedef struct _InstrInfo InstrInfo;
struct _InstrInfo {
   Addr    instr_addr;
   UChar   instr_len;
   LineCC* parent;         
};

typedef struct _SB_info SB_info;
struct _SB_info {
   Addr      SB_addr;      
   Int       n_instrs;
   InstrInfo instrs[0];
};

static OSet* instrInfoTable;


static OSet* stringTable;

static Int  distinct_files      = 0;
static Int  distinct_fns        = 0;
static Int  distinct_lines      = 0;
static Int  distinct_instrsGen  = 0;
static Int  distinct_instrsNoX  = 0;

static Int  full_debugs         = 0;
static Int  file_line_debugs    = 0;
static Int  fn_debugs           = 0;
static Int  no_debugs           = 0;


static Word stringCmp( const void* key, const void* elem )
{
   return VG_(strcmp)(*(const HChar *const *)key, *(const HChar *const *)elem);
}

static HChar* get_perm_string(const HChar* s)
{
   HChar** s_ptr = VG_(OSetGen_Lookup)(stringTable, &s);
   if (s_ptr) {
      return *s_ptr;
   } else {
      HChar** s_node = VG_(OSetGen_AllocNode)(stringTable, sizeof(HChar*));
      *s_node = VG_(strdup)("cg.main.gps.1", s);
      VG_(OSetGen_Insert)(stringTable, s_node);
      return *s_node;
   }
}


static void get_debug_info(Addr instr_addr, const HChar **dir,
                           const HChar **file, const HChar **fn, UInt* line)
{
   Bool found_file_line = VG_(get_filename_linenum)(
                             instr_addr, 
                             file, dir,
                             line
                          );
   Bool found_fn        = VG_(get_fnname)(instr_addr, fn);

   if (!found_file_line) {
      *file = "???";
      *line = 0;
   }
   if (!found_fn) {
      *fn = "???";
   }

   if (found_file_line) {
      if (found_fn) full_debugs++;
      else          file_line_debugs++;
   } else {
      if (found_fn) fn_debugs++;
      else          no_debugs++;
   }
}

static LineCC* get_lineCC(Addr origAddr)
{
   const HChar *fn, *file, *dir;
   UInt    line;
   CodeLoc loc;
   LineCC* lineCC;

   get_debug_info(origAddr, &dir, &file, &fn, &line);

   
   HChar absfile[VG_(strlen)(dir) + 1 + VG_(strlen)(file) + 1];

   if (dir[0]) {
      VG_(sprintf)(absfile, "%s/%s", dir, file);
   } else {
      VG_(sprintf)(absfile, "%s", file);
   }

   loc.file = absfile;
   loc.fn   = fn;
   loc.line = line;

   lineCC = VG_(OSetGen_Lookup)(CC_table, &loc);
   if (!lineCC) {
      
      lineCC           = VG_(OSetGen_AllocNode)(CC_table, sizeof(LineCC));
      lineCC->loc.file = get_perm_string(loc.file);
      lineCC->loc.fn   = get_perm_string(loc.fn);
      lineCC->loc.line = loc.line;
      lineCC->Ir.a     = 0;
      lineCC->Ir.m1    = 0;
      lineCC->Ir.mL    = 0;
      lineCC->Dr.a     = 0;
      lineCC->Dr.m1    = 0;
      lineCC->Dr.mL    = 0;
      lineCC->Dw.a     = 0;
      lineCC->Dw.m1    = 0;
      lineCC->Dw.mL    = 0;
      lineCC->Bc.b     = 0;
      lineCC->Bc.mp    = 0;
      lineCC->Bi.b     = 0;
      lineCC->Bi.mp    = 0;
      VG_(OSetGen_Insert)(CC_table, lineCC);
   }

   return lineCC;
}



static VG_REGPARM(1)
void log_1Ir(InstrInfo* n)
{
   n->parent->Ir.a++;
}

static VG_REGPARM(2)
void log_2Ir(InstrInfo* n, InstrInfo* n2)
{
   n->parent->Ir.a++;
   n2->parent->Ir.a++;
}

static VG_REGPARM(3)
void log_3Ir(InstrInfo* n, InstrInfo* n2, InstrInfo* n3)
{
   n->parent->Ir.a++;
   n2->parent->Ir.a++;
   n3->parent->Ir.a++;
}

static VG_REGPARM(1)
void log_1IrGen_0D_cache_access(InstrInfo* n)
{
   
   
   cachesim_I1_doref_Gen(n->instr_addr, n->instr_len,
			 &n->parent->Ir.m1, &n->parent->Ir.mL);
   n->parent->Ir.a++;
}

static VG_REGPARM(1)
void log_1IrNoX_0D_cache_access(InstrInfo* n)
{
   
   
   cachesim_I1_doref_NoX(n->instr_addr, n->instr_len,
			 &n->parent->Ir.m1, &n->parent->Ir.mL);
   n->parent->Ir.a++;
}

static VG_REGPARM(2)
void log_2IrNoX_0D_cache_access(InstrInfo* n, InstrInfo* n2)
{
   
   
   
   
   cachesim_I1_doref_NoX(n->instr_addr, n->instr_len,
			 &n->parent->Ir.m1, &n->parent->Ir.mL);
   n->parent->Ir.a++;
   cachesim_I1_doref_NoX(n2->instr_addr, n2->instr_len,
			 &n2->parent->Ir.m1, &n2->parent->Ir.mL);
   n2->parent->Ir.a++;
}

static VG_REGPARM(3)
void log_3IrNoX_0D_cache_access(InstrInfo* n, InstrInfo* n2, InstrInfo* n3)
{
   
   
   
   
   
   
   cachesim_I1_doref_NoX(n->instr_addr, n->instr_len,
			 &n->parent->Ir.m1, &n->parent->Ir.mL);
   n->parent->Ir.a++;
   cachesim_I1_doref_NoX(n2->instr_addr, n2->instr_len,
			 &n2->parent->Ir.m1, &n2->parent->Ir.mL);
   n2->parent->Ir.a++;
   cachesim_I1_doref_NoX(n3->instr_addr, n3->instr_len,
			 &n3->parent->Ir.m1, &n3->parent->Ir.mL);
   n3->parent->Ir.a++;
}

static VG_REGPARM(3)
void log_1IrNoX_1Dr_cache_access(InstrInfo* n, Addr data_addr, Word data_size)
{
   
   
   
   cachesim_I1_doref_NoX(n->instr_addr, n->instr_len,
			 &n->parent->Ir.m1, &n->parent->Ir.mL);
   n->parent->Ir.a++;

   cachesim_D1_doref(data_addr, data_size, 
                     &n->parent->Dr.m1, &n->parent->Dr.mL);
   n->parent->Dr.a++;
}

static VG_REGPARM(3)
void log_1IrNoX_1Dw_cache_access(InstrInfo* n, Addr data_addr, Word data_size)
{
   
   
   
   cachesim_I1_doref_NoX(n->instr_addr, n->instr_len,
			 &n->parent->Ir.m1, &n->parent->Ir.mL);
   n->parent->Ir.a++;

   cachesim_D1_doref(data_addr, data_size, 
                     &n->parent->Dw.m1, &n->parent->Dw.mL);
   n->parent->Dw.a++;
}

static VG_REGPARM(3)
void log_0Ir_1Dr_cache_access(InstrInfo* n, Addr data_addr, Word data_size)
{
   
   
   cachesim_D1_doref(data_addr, data_size, 
                     &n->parent->Dr.m1, &n->parent->Dr.mL);
   n->parent->Dr.a++;
}

static VG_REGPARM(3)
void log_0Ir_1Dw_cache_access(InstrInfo* n, Addr data_addr, Word data_size)
{
   
   
   cachesim_D1_doref(data_addr, data_size, 
                     &n->parent->Dw.m1, &n->parent->Dw.mL);
   n->parent->Dw.a++;
}


static VG_REGPARM(2)
void log_cond_branch(InstrInfo* n, Word taken)
{
   
   
   n->parent->Bc.b++;
   n->parent->Bc.mp 
      += (1 & do_cond_branch_predict(n->instr_addr, taken));
}

static VG_REGPARM(2)
void log_ind_branch(InstrInfo* n, UWord actual_dst)
{
   
   
   n->parent->Bi.b++;
   n->parent->Bi.mp
      += (1 & do_ind_branch_predict(n->instr_addr, actual_dst));
}




typedef
   IRExpr 
   IRAtom;

typedef 
   enum { 
      Ev_IrNoX,  
      Ev_IrGen,  
      Ev_Dr,     
      Ev_Dw,     
      Ev_Dm,     
      Ev_Bc,     
      Ev_Bi      
   }
   EventTag;

typedef
   struct {
      EventTag   tag;
      InstrInfo* inode;
      union {
         struct {
         } IrGen;
         struct {
         } IrNoX;
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


typedef
   struct {
      
      Event events[N_EVENTS];
      Int   events_used;

      
      SB_info* sbInfo;

      
      Int sbInfo_i;

      
      IRSB* sbOut;
   }
   CgState;



static
SB_info* get_SB_info(IRSB* sbIn, Addr origAddr)
{
   Int      i, n_instrs;
   IRStmt*  st;
   SB_info* sbInfo;

   
   n_instrs = 0;
   for (i = 0; i < sbIn->stmts_used; i++) {
      st = sbIn->stmts[i];
      if (Ist_IMark == st->tag) n_instrs++;
   }

   
   
   
   
   sbInfo = VG_(OSetGen_Lookup)(instrInfoTable, &origAddr);
   tl_assert(NULL == sbInfo);

   
   
   sbInfo = VG_(OSetGen_AllocNode)(instrInfoTable,
                                sizeof(SB_info) + n_instrs*sizeof(InstrInfo)); 
   sbInfo->SB_addr  = origAddr;
   sbInfo->n_instrs = n_instrs;
   VG_(OSetGen_Insert)( instrInfoTable, sbInfo );

   return sbInfo;
}


static void showEvent ( Event* ev )
{
   switch (ev->tag) {
      case Ev_IrGen:
         VG_(printf)("IrGen %p\n", ev->inode);
         break;
      case Ev_IrNoX:
         VG_(printf)("IrNoX %p\n", ev->inode);
         break;
      case Ev_Dr:
         VG_(printf)("Dr %p %d EA=", ev->inode, ev->Ev.Dr.szB);
         ppIRExpr(ev->Ev.Dr.ea); 
         VG_(printf)("\n");
         break;
      case Ev_Dw:
         VG_(printf)("Dw %p %d EA=", ev->inode, ev->Ev.Dw.szB);
         ppIRExpr(ev->Ev.Dw.ea); 
         VG_(printf)("\n");
         break;
      case Ev_Dm:
         VG_(printf)("Dm %p %d EA=", ev->inode, ev->Ev.Dm.szB);
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
      default: 
         tl_assert(0);
         break;
   }
}

static
InstrInfo* setup_InstrInfo ( CgState* cgs, Addr instr_addr, UInt instr_len )
{
   InstrInfo* i_node;
   tl_assert(cgs->sbInfo_i >= 0);
   tl_assert(cgs->sbInfo_i < cgs->sbInfo->n_instrs);
   i_node = &cgs->sbInfo->instrs[ cgs->sbInfo_i ];
   i_node->instr_addr = instr_addr;
   i_node->instr_len  = instr_len;
   i_node->parent     = get_lineCC(instr_addr);
   cgs->sbInfo_i++;
   return i_node;
}



static void flushEvents ( CgState* cgs )
{
   Int        i, regparms;
   const HChar* helperName;
   void*      helperAddr;
   IRExpr**   argv;
   IRExpr*    i_node_expr;
   IRDirty*   di;
   Event*     ev;
   Event*     ev2;
   Event*     ev3;

   i = 0;
   while (i < cgs->events_used) {

      helperName = NULL;
      helperAddr = NULL;
      argv       = NULL;
      regparms   = 0;

      tl_assert(i >= 0 && i < cgs->events_used);

      ev  = &cgs->events[i];
      ev2 = ( i < cgs->events_used-1 ? &cgs->events[i+1] : NULL );
      ev3 = ( i < cgs->events_used-2 ? &cgs->events[i+2] : NULL );
      
      if (DEBUG_CG) {
         VG_(printf)("   flush "); 
         showEvent( ev );
      }

      i_node_expr = mkIRExpr_HWord( (HWord)ev->inode );

      switch (ev->tag) {
         case Ev_IrNoX:
            
            if (ev2 && (ev2->tag == Ev_Dr || ev2->tag == Ev_Dm)) {
               tl_assert(ev2->inode == ev->inode);
               helperName = "log_1IrNoX_1Dr_cache_access";
               helperAddr = &log_1IrNoX_1Dr_cache_access;
               argv = mkIRExprVec_3( i_node_expr,
                                     get_Event_dea(ev2),
                                     mkIRExpr_HWord( get_Event_dszB(ev2) ) );
               regparms = 3;
               i += 2;
            }
            
            else
            if (ev2 && ev2->tag == Ev_Dw) {
               tl_assert(ev2->inode == ev->inode);
               helperName = "log_1IrNoX_1Dw_cache_access";
               helperAddr = &log_1IrNoX_1Dw_cache_access;
               argv = mkIRExprVec_3( i_node_expr,
                                     get_Event_dea(ev2),
                                     mkIRExpr_HWord( get_Event_dszB(ev2) ) );
               regparms = 3;
               i += 2;
            }
            
            else
            if (ev2 && ev3 && ev2->tag == Ev_IrNoX && ev3->tag == Ev_IrNoX)
            {
               if (clo_cache_sim) {
                  helperName = "log_3IrNoX_0D_cache_access";
                  helperAddr = &log_3IrNoX_0D_cache_access;
               } else {
                  helperName = "log_3Ir";
                  helperAddr = &log_3Ir;
               }
               argv = mkIRExprVec_3( i_node_expr, 
                                     mkIRExpr_HWord( (HWord)ev2->inode ), 
                                     mkIRExpr_HWord( (HWord)ev3->inode ) );
               regparms = 3;
               i += 3;
            }
            
            else
            if (ev2 && ev2->tag == Ev_IrNoX) {
               if (clo_cache_sim) {
                  helperName = "log_2IrNoX_0D_cache_access";
                  helperAddr = &log_2IrNoX_0D_cache_access;
               } else {
                  helperName = "log_2Ir";
                  helperAddr = &log_2Ir;
               }
               argv = mkIRExprVec_2( i_node_expr,
                                     mkIRExpr_HWord( (HWord)ev2->inode ) );
               regparms = 2;
               i += 2;
            }
            
            else {
               if (clo_cache_sim) {
                  helperName = "log_1IrNoX_0D_cache_access";
                  helperAddr = &log_1IrNoX_0D_cache_access;
               } else {
                  helperName = "log_1Ir";
                  helperAddr = &log_1Ir;
               }
               argv = mkIRExprVec_1( i_node_expr );
               regparms = 1;
               i++;
            }
            break;
         case Ev_IrGen:
            if (clo_cache_sim) {
	       helperName = "log_1IrGen_0D_cache_access";
	       helperAddr = &log_1IrGen_0D_cache_access;
	    } else {
	       helperName = "log_1Ir";
	       helperAddr = &log_1Ir;
	    }
	    argv = mkIRExprVec_1( i_node_expr );
	    regparms = 1;
	    i++;
            break;
         case Ev_Dr:
         case Ev_Dm:
            
            helperName = "log_0Ir_1Dr_cache_access";
            helperAddr = &log_0Ir_1Dr_cache_access;
            argv = mkIRExprVec_3( i_node_expr, 
                                  get_Event_dea(ev), 
                                  mkIRExpr_HWord( get_Event_dszB(ev) ) );
            regparms = 3;
            i++;
            break;
         case Ev_Dw:
            
            helperName = "log_0Ir_1Dw_cache_access";
            helperAddr = &log_0Ir_1Dw_cache_access;
            argv = mkIRExprVec_3( i_node_expr,
                                  get_Event_dea(ev), 
                                  mkIRExpr_HWord( get_Event_dszB(ev) ) );
            regparms = 3;
            i++;
            break;
         case Ev_Bc:
            
            helperName = "log_cond_branch";
            helperAddr = &log_cond_branch;
            argv = mkIRExprVec_2( i_node_expr, ev->Ev.Bc.taken );
            regparms = 2;
            i++;
            break;
         case Ev_Bi:
            
            helperName = "log_ind_branch";
            helperAddr = &log_ind_branch;
            argv = mkIRExprVec_2( i_node_expr, ev->Ev.Bi.dst );
            regparms = 2;
            i++;
            break;
         default:
            tl_assert(0);
      }

      
      tl_assert(helperName);
      tl_assert(helperAddr);
      tl_assert(argv);
      di = unsafeIRDirty_0_N( regparms, 
                              helperName, VG_(fnptr_to_fnentry)( helperAddr ), 
                              argv );
      addStmtToIRSB( cgs->sbOut, IRStmt_Dirty(di) );
   }

   cgs->events_used = 0;
}

static void addEvent_Ir ( CgState* cgs, InstrInfo* inode )
{
   Event* evt;
   if (cgs->events_used == N_EVENTS)
      flushEvents(cgs);
   tl_assert(cgs->events_used >= 0 && cgs->events_used < N_EVENTS);
   evt = &cgs->events[cgs->events_used];
   init_Event(evt);
   evt->inode    = inode;
   if (cachesim_is_IrNoX(inode->instr_addr, inode->instr_len)) {
      evt->tag = Ev_IrNoX;
      distinct_instrsNoX++;
   } else {
      evt->tag = Ev_IrGen;
      distinct_instrsGen++;
   }
   cgs->events_used++;
}

static
void addEvent_Dr ( CgState* cgs, InstrInfo* inode, Int datasize, IRAtom* ea )
{
   Event* evt;
   tl_assert(isIRAtom(ea));
   tl_assert(datasize >= 1 && datasize <= min_line_size);
   if (!clo_cache_sim)
      return;
   if (cgs->events_used == N_EVENTS)
      flushEvents(cgs);
   tl_assert(cgs->events_used >= 0 && cgs->events_used < N_EVENTS);
   evt = &cgs->events[cgs->events_used];
   init_Event(evt);
   evt->tag       = Ev_Dr;
   evt->inode     = inode;
   evt->Ev.Dr.szB = datasize;
   evt->Ev.Dr.ea  = ea;
   cgs->events_used++;
}

static
void addEvent_Dw ( CgState* cgs, InstrInfo* inode, Int datasize, IRAtom* ea )
{
   Event* lastEvt;
   Event* evt;

   tl_assert(isIRAtom(ea));
   tl_assert(datasize >= 1 && datasize <= min_line_size);

   if (!clo_cache_sim)
      return;

   
   lastEvt = &cgs->events[cgs->events_used-1];
   if (cgs->events_used > 0
       && lastEvt->tag       == Ev_Dr
       && lastEvt->Ev.Dr.szB == datasize
       && lastEvt->inode     == inode
       && eqIRAtom(lastEvt->Ev.Dr.ea, ea))
   {
      lastEvt->tag   = Ev_Dm;
      return;
   }

   
   if (cgs->events_used == N_EVENTS)
      flushEvents(cgs);
   tl_assert(cgs->events_used >= 0 && cgs->events_used < N_EVENTS);
   evt = &cgs->events[cgs->events_used];
   init_Event(evt);
   evt->tag       = Ev_Dw;
   evt->inode     = inode;
   evt->Ev.Dw.szB = datasize;
   evt->Ev.Dw.ea  = ea;
   cgs->events_used++;
}

static
void addEvent_D_guarded ( CgState* cgs, InstrInfo* inode,
                          Int datasize, IRAtom* ea, IRAtom* guard,
                          Bool isWrite )
{
   tl_assert(isIRAtom(ea));
   tl_assert(guard);
   tl_assert(isIRAtom(guard));
   tl_assert(datasize >= 1 && datasize <= min_line_size);

   if (!clo_cache_sim)
      return;

   tl_assert(cgs->events_used >= 0);
   flushEvents(cgs);
   tl_assert(cgs->events_used == 0);
   
   IRExpr*      i_node_expr;
   const HChar* helperName;
   void*        helperAddr;
   IRExpr**     argv;
   Int          regparms;
   IRDirty*     di;
   i_node_expr = mkIRExpr_HWord( (HWord)inode );
   helperName  = isWrite ? "log_0Ir_1Dw_cache_access"
                         : "log_0Ir_1Dr_cache_access";
   helperAddr  = isWrite ? &log_0Ir_1Dw_cache_access
                         : &log_0Ir_1Dr_cache_access;
   argv        = mkIRExprVec_3( i_node_expr,
                                ea, mkIRExpr_HWord( datasize ) );
   regparms    = 3;
   di          = unsafeIRDirty_0_N(
                    regparms, 
                    helperName, VG_(fnptr_to_fnentry)( helperAddr ), 
                    argv );
   di->guard = guard;
   addStmtToIRSB( cgs->sbOut, IRStmt_Dirty(di) );
}


static
void addEvent_Bc ( CgState* cgs, InstrInfo* inode, IRAtom* guard )
{
   Event* evt;
   tl_assert(isIRAtom(guard));
   tl_assert(typeOfIRExpr(cgs->sbOut->tyenv, guard) 
             == (sizeof(HWord)==4 ? Ity_I32 : Ity_I64));
   if (!clo_branch_sim)
      return;
   if (cgs->events_used == N_EVENTS)
      flushEvents(cgs);
   tl_assert(cgs->events_used >= 0 && cgs->events_used < N_EVENTS);
   evt = &cgs->events[cgs->events_used];
   init_Event(evt);
   evt->tag         = Ev_Bc;
   evt->inode       = inode;
   evt->Ev.Bc.taken = guard;
   cgs->events_used++;
}

static
void addEvent_Bi ( CgState* cgs, InstrInfo* inode, IRAtom* whereTo )
{
   Event* evt;
   tl_assert(isIRAtom(whereTo));
   tl_assert(typeOfIRExpr(cgs->sbOut->tyenv, whereTo) 
             == (sizeof(HWord)==4 ? Ity_I32 : Ity_I64));
   if (!clo_branch_sim)
      return;
   if (cgs->events_used == N_EVENTS)
      flushEvents(cgs);
   tl_assert(cgs->events_used >= 0 && cgs->events_used < N_EVENTS);
   evt = &cgs->events[cgs->events_used];
   init_Event(evt);
   evt->tag       = Ev_Bi;
   evt->inode     = inode;
   evt->Ev.Bi.dst = whereTo;
   cgs->events_used++;
}



static
IRSB* cg_instrument ( VgCallbackClosure* closure,
                      IRSB* sbIn, 
                      const VexGuestLayout* layout, 
                      const VexGuestExtents* vge,
                      const VexArchInfo* archinfo_host,
                      IRType gWordTy, IRType hWordTy )
{
   Int        i;
   UInt       isize;
   IRStmt*    st;
   Addr       cia; 
   CgState    cgs;
   IRTypeEnv* tyenv = sbIn->tyenv;
   InstrInfo* curr_inode = NULL;

   if (gWordTy != hWordTy) {
      
      VG_(tool_panic)("host/guest word size mismatch");
   }

   
   cgs.sbOut = deepCopyIRSBExceptStmts(sbIn);

   
   i = 0;
   while (i < sbIn->stmts_used && sbIn->stmts[i]->tag != Ist_IMark) {
      addStmtToIRSB( cgs.sbOut, sbIn->stmts[i] );
      i++;
   }

   
   tl_assert(sbIn->stmts_used > 0);
   tl_assert(i < sbIn->stmts_used);
   st = sbIn->stmts[i];
   tl_assert(Ist_IMark == st->tag);

   cia   = st->Ist.IMark.addr;
   isize = st->Ist.IMark.len;
   
   
   if (isize == 0) isize = VG_MIN_INSTR_SZB;

   
   tl_assert(closure->readdr == vge->base[0]);
   cgs.events_used = 0;
   cgs.sbInfo      = get_SB_info(sbIn, (Addr)closure->readdr);
   cgs.sbInfo_i    = 0;

   if (DEBUG_CG)
      VG_(printf)("\n\n---------- cg_instrument ----------\n");

   
   
   for (; i < sbIn->stmts_used; i++) {

      st = sbIn->stmts[i];
      tl_assert(isFlatIRStmt(st));

      switch (st->tag) {
         case Ist_NoOp:
         case Ist_AbiHint:
         case Ist_Put:
         case Ist_PutI:
         case Ist_MBE:
            break;

         case Ist_IMark:
            cia   = st->Ist.IMark.addr;
            isize = st->Ist.IMark.len;

            
            
            if (isize == 0) isize = VG_MIN_INSTR_SZB;

            
            tl_assert( (VG_MIN_INSTR_SZB <= isize && isize <= VG_MAX_INSTR_SZB)
                     || VG_CLREQ_SZB == isize );

            
            
            
            curr_inode = setup_InstrInfo(&cgs, cia, isize);

            addEvent_Ir( &cgs, curr_inode );
            break;

         case Ist_WrTmp: {
            IRExpr* data = st->Ist.WrTmp.data;
            if (data->tag == Iex_Load) {
               IRExpr* aexpr = data->Iex.Load.addr;
               
               
               addEvent_Dr( &cgs, curr_inode, sizeofIRType(data->Iex.Load.ty), 
                                  aexpr );
            }
            break;
         }

         case Ist_Store: {
            IRExpr* data  = st->Ist.Store.data;
            IRExpr* aexpr = st->Ist.Store.addr;
            addEvent_Dw( &cgs, curr_inode, 
                         sizeofIRType(typeOfIRExpr(tyenv, data)), aexpr );
            break;
         }

         case Ist_StoreG: {
            IRStoreG* sg   = st->Ist.StoreG.details;
            IRExpr*   data = sg->data;
            IRExpr*   addr = sg->addr;
            IRType    type = typeOfIRExpr(tyenv, data);
            tl_assert(type != Ity_INVALID);
            addEvent_D_guarded( &cgs, curr_inode,
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
            addEvent_D_guarded( &cgs, curr_inode,
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
               
               
               
               
               if (dataSize > min_line_size)
                  dataSize = min_line_size;
               if (d->mFx == Ifx_Read || d->mFx == Ifx_Modify)
                  addEvent_Dr( &cgs, curr_inode, dataSize, d->mAddr );
               if (d->mFx == Ifx_Write || d->mFx == Ifx_Modify)
                  addEvent_Dw( &cgs, curr_inode, dataSize, d->mAddr );
            } else {
               tl_assert(d->mAddr == NULL);
               tl_assert(d->mSize == 0);
            }
            break;
         }

         case Ist_CAS: {
            Int    dataSize;
            IRCAS* cas = st->Ist.CAS.details;
            tl_assert(cas->addr != NULL);
            tl_assert(cas->dataLo != NULL);
            dataSize = sizeofIRType(typeOfIRExpr(tyenv, cas->dataLo));
            if (cas->dataHi != NULL)
               dataSize *= 2; 
            
            if (dataSize > min_line_size)
               dataSize = min_line_size;
            addEvent_Dr( &cgs, curr_inode, dataSize, cas->addr );
            addEvent_Dw( &cgs, curr_inode, dataSize, cas->addr );
            break;
         }

         case Ist_LLSC: {
            IRType dataTy;
            if (st->Ist.LLSC.storedata == NULL) {
               
               dataTy = typeOfIRTemp(tyenv, st->Ist.LLSC.result);
               addEvent_Dr( &cgs, curr_inode,
                            sizeofIRType(dataTy), st->Ist.LLSC.addr );
               
               flushEvents( &cgs );
            } else {
               
               dataTy = typeOfIRExpr(tyenv, st->Ist.LLSC.storedata);
               addEvent_Dw( &cgs, curr_inode,
                            sizeofIRType(dataTy), st->Ist.LLSC.addr );
            }
            break;
         }

         case Ist_Exit: {
            
            if ( (st->Ist.Exit.jk == Ijk_Boring) ||
                 (st->Ist.Exit.jk == Ijk_Call) ||
                 (st->Ist.Exit.jk == Ijk_Ret) )
            {
               Bool     inverted;
               Addr     nia, sea;
               IRConst* dst;
               IRType   tyW    = hWordTy;
               IROp     widen  = tyW==Ity_I32  ? Iop_1Uto32  : Iop_1Uto64;
               IROp     opXOR  = tyW==Ity_I32  ? Iop_Xor32   : Iop_Xor64;
               IRTemp   guard1 = newIRTemp(cgs.sbOut->tyenv, Ity_I1);
               IRTemp   guardW = newIRTemp(cgs.sbOut->tyenv, tyW);
               IRTemp   guard  = newIRTemp(cgs.sbOut->tyenv, tyW);
               IRExpr*  one    = tyW==Ity_I32 ? IRExpr_Const(IRConst_U32(1))
                                              : IRExpr_Const(IRConst_U64(1));

               nia = cia + isize;

               
               dst = st->Ist.Exit.dst;
               if (tyW == Ity_I32) {
                  tl_assert(dst->tag == Ico_U32);
                  sea = dst->Ico.U32;
               } else {
                  tl_assert(tyW == Ity_I64);
                  tl_assert(dst->tag == Ico_U64);
                  sea = dst->Ico.U64;
               }

               inverted = nia == sea;

               
               addStmtToIRSB( cgs.sbOut,
                              IRStmt_WrTmp( guard1, st->Ist.Exit.guard ));
               addStmtToIRSB( cgs.sbOut,
                              IRStmt_WrTmp( guardW,
                                            IRExpr_Unop(widen,
                                                        IRExpr_RdTmp(guard1))) );
               
               addStmtToIRSB(
                     cgs.sbOut,
                     IRStmt_WrTmp(
                           guard,
                           inverted ? IRExpr_Binop(opXOR, IRExpr_RdTmp(guardW), one)
                                    : IRExpr_RdTmp(guardW)
                              ));
               
               addEvent_Bc( &cgs, curr_inode, IRExpr_RdTmp(guard) );
            }

            flushEvents( &cgs );
            break;
         }

         default:
            ppIRStmt(st);
            tl_assert(0);
            break;
      }

      
      addStmtToIRSB( cgs.sbOut, st );

      if (DEBUG_CG) {
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
            
            addEvent_Bi( &cgs, curr_inode, sbIn->next );
            break;
         default:
            tl_assert(0); 
      }
   }

   
   flushEvents( &cgs );

   
   tl_assert(cgs.sbInfo_i == cgs.sbInfo->n_instrs);

   if (DEBUG_CG) {
      VG_(printf)( "goto {");
      ppIRJumpKind(sbIn->jumpkind);
      VG_(printf)( "} ");
      ppIRExpr( sbIn->next );
      VG_(printf)( "}\n");
   }

   return cgs.sbOut;
}


static cache_t clo_I1_cache = UNDEFINED_CACHE;
static cache_t clo_D1_cache = UNDEFINED_CACHE;
static cache_t clo_LL_cache = UNDEFINED_CACHE;


static CacheCC  Ir_total;
static CacheCC  Dr_total;
static CacheCC  Dw_total;
static BranchCC Bc_total;
static BranchCC Bi_total;

static void fprint_CC_table_and_calc_totals(void)
{
   Int     i;
   VgFile  *fp;
   HChar   *currFile = NULL;
   const HChar *currFn = NULL;
   LineCC* lineCC;

   
   
   
   
   
   HChar* cachegrind_out_file =
      VG_(expand_file_name)("--cachegrind-out-file", clo_cachegrind_out_file);

   fp = VG_(fopen)(cachegrind_out_file, VKI_O_CREAT|VKI_O_TRUNC|VKI_O_WRONLY,
                                        VKI_S_IRUSR|VKI_S_IWUSR);
   if (fp == NULL) {
      
      
      VG_(umsg)("error: can't open cache simulation output file '%s'\n",
                cachegrind_out_file );
      VG_(umsg)("       ... so simulation results will be missing.\n");
      VG_(free)(cachegrind_out_file);
      return;
   } else {
      VG_(free)(cachegrind_out_file);
   }

   
   
   VG_(fprintf)(fp,  "desc: I1 cache:         %s\n"
                     "desc: D1 cache:         %s\n"
                     "desc: LL cache:         %s\n",
                     I1.desc_line, D1.desc_line, LL.desc_line);

   
   VG_(fprintf)(fp, "cmd: %s", VG_(args_the_exename));
   for (i = 0; i < VG_(sizeXA)( VG_(args_for_client) ); i++) {
      HChar* arg = * (HChar**) VG_(indexXA)( VG_(args_for_client), i );
      VG_(fprintf)(fp, " %s", arg);
   }
   
   if (clo_cache_sim && clo_branch_sim) {
      VG_(fprintf)(fp, "\nevents: Ir I1mr ILmr Dr D1mr DLmr Dw D1mw DLmw "
                                  "Bc Bcm Bi Bim\n");
   }
   else if (clo_cache_sim && !clo_branch_sim) {
      VG_(fprintf)(fp, "\nevents: Ir I1mr ILmr Dr D1mr DLmr Dw D1mw DLmw "
                                  "\n");
   }
   else if (!clo_cache_sim && clo_branch_sim) {
      VG_(fprintf)(fp, "\nevents: Ir Bc Bcm Bi Bim\n");
   }
   else {
      VG_(fprintf)(fp, "\nevents: Ir\n");
   }

   
   VG_(OSetGen_ResetIter)(CC_table);
   while ( (lineCC = VG_(OSetGen_Next)(CC_table)) ) {
      Bool just_hit_a_new_file = False;
      
      
      
      
      
      if ( lineCC->loc.file != currFile ) {
         currFile = lineCC->loc.file;
         VG_(fprintf)(fp, "fl=%s\n", currFile);
         distinct_files++;
         just_hit_a_new_file = True;
      }
      
      
      
      
      if ( just_hit_a_new_file || lineCC->loc.fn != currFn ) {
         currFn = lineCC->loc.fn;
         VG_(fprintf)(fp, "fn=%s\n", currFn);
         distinct_fns++;
      }

      
      if (clo_cache_sim && clo_branch_sim) {
         VG_(fprintf)(fp,  "%u %llu %llu %llu"
                             " %llu %llu %llu"
                             " %llu %llu %llu"
                             " %llu %llu %llu %llu\n",
                            lineCC->loc.line,
                            lineCC->Ir.a, lineCC->Ir.m1, lineCC->Ir.mL, 
                            lineCC->Dr.a, lineCC->Dr.m1, lineCC->Dr.mL,
                            lineCC->Dw.a, lineCC->Dw.m1, lineCC->Dw.mL,
                            lineCC->Bc.b, lineCC->Bc.mp, 
                            lineCC->Bi.b, lineCC->Bi.mp);
      }
      else if (clo_cache_sim && !clo_branch_sim) {
         VG_(fprintf)(fp,  "%u %llu %llu %llu"
                             " %llu %llu %llu"
                             " %llu %llu %llu\n",
                            lineCC->loc.line,
                            lineCC->Ir.a, lineCC->Ir.m1, lineCC->Ir.mL, 
                            lineCC->Dr.a, lineCC->Dr.m1, lineCC->Dr.mL,
                            lineCC->Dw.a, lineCC->Dw.m1, lineCC->Dw.mL);
      }
      else if (!clo_cache_sim && clo_branch_sim) {
         VG_(fprintf)(fp,  "%u %llu"
                             " %llu %llu %llu %llu\n",
                            lineCC->loc.line,
                            lineCC->Ir.a, 
                            lineCC->Bc.b, lineCC->Bc.mp, 
                            lineCC->Bi.b, lineCC->Bi.mp);
      }
      else {
         VG_(fprintf)(fp,  "%u %llu\n",
                            lineCC->loc.line,
                            lineCC->Ir.a);
      }

      
      Ir_total.a  += lineCC->Ir.a;
      Ir_total.m1 += lineCC->Ir.m1;
      Ir_total.mL += lineCC->Ir.mL;
      Dr_total.a  += lineCC->Dr.a;
      Dr_total.m1 += lineCC->Dr.m1;
      Dr_total.mL += lineCC->Dr.mL;
      Dw_total.a  += lineCC->Dw.a;
      Dw_total.m1 += lineCC->Dw.m1;
      Dw_total.mL += lineCC->Dw.mL;
      Bc_total.b  += lineCC->Bc.b;
      Bc_total.mp += lineCC->Bc.mp;
      Bi_total.b  += lineCC->Bi.b;
      Bi_total.mp += lineCC->Bi.mp;

      distinct_lines++;
   }

   
   
   if (clo_cache_sim && clo_branch_sim) {
      VG_(fprintf)(fp,  "summary:"
                        " %llu %llu %llu"
                        " %llu %llu %llu"
                        " %llu %llu %llu"
                        " %llu %llu %llu %llu\n", 
                        Ir_total.a, Ir_total.m1, Ir_total.mL,
                        Dr_total.a, Dr_total.m1, Dr_total.mL,
                        Dw_total.a, Dw_total.m1, Dw_total.mL,
                        Bc_total.b, Bc_total.mp, 
                        Bi_total.b, Bi_total.mp);
   }
   else if (clo_cache_sim && !clo_branch_sim) {
      VG_(fprintf)(fp,  "summary:"
                        " %llu %llu %llu"
                        " %llu %llu %llu"
                        " %llu %llu %llu\n",
                        Ir_total.a, Ir_total.m1, Ir_total.mL,
                        Dr_total.a, Dr_total.m1, Dr_total.mL,
                        Dw_total.a, Dw_total.m1, Dw_total.mL);
   }
   else if (!clo_cache_sim && clo_branch_sim) {
      VG_(fprintf)(fp,  "summary:"
                        " %llu"
                        " %llu %llu %llu %llu\n", 
                        Ir_total.a,
                        Bc_total.b, Bc_total.mp, 
                        Bi_total.b, Bi_total.mp);
   }
   else {
      VG_(fprintf)(fp, "summary:"
                        " %llu\n", 
                        Ir_total.a);
   }

   VG_(fclose)(fp);
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

static void cg_fini(Int exitcode)
{
   static HChar fmt[128];   

   CacheCC  D_total;
   BranchCC B_total;
   ULong LL_total_m, LL_total_mr, LL_total_mw,
         LL_total, LL_total_r, LL_total_w;
   Int l1, l2, l3;

   fprint_CC_table_and_calc_totals();

   if (VG_(clo_verbosity) == 0) 
      return;

   
   #define CG_MAX(a, b)  ((a) >= (b) ? (a) : (b))

   l1 = ULong_width(Ir_total.a);
   l2 = ULong_width(CG_MAX(Dr_total.a, Bc_total.b));
   l3 = ULong_width(CG_MAX(Dw_total.a, Bi_total.b));

   
   VG_(sprintf)(fmt, "%%s %%,%dllu\n", l1);

   
   VG_(umsg)(fmt, "I   refs:     ", Ir_total.a);

   if (clo_cache_sim) {
      VG_(umsg)(fmt, "I1  misses:   ", Ir_total.m1);
      VG_(umsg)(fmt, "LLi misses:   ", Ir_total.mL);

      if (0 == Ir_total.a) Ir_total.a = 1;
      VG_(umsg)("I1  miss rate: %*.2f%%\n", l1,
                Ir_total.m1 * 100.0 / Ir_total.a);
      VG_(umsg)("LLi miss rate: %*.2f%%\n", l1,
                Ir_total.mL * 100.0 / Ir_total.a);
      VG_(umsg)("\n");

      D_total.a  = Dr_total.a  + Dw_total.a;
      D_total.m1 = Dr_total.m1 + Dw_total.m1;
      D_total.mL = Dr_total.mL + Dw_total.mL;

      
      VG_(sprintf)(fmt, "%%s %%,%dllu  (%%,%dllu rd   + %%,%dllu wr)\n",
                        l1, l2, l3);

      VG_(umsg)(fmt, "D   refs:     ", 
                     D_total.a, Dr_total.a, Dw_total.a);
      VG_(umsg)(fmt, "D1  misses:   ",
                     D_total.m1, Dr_total.m1, Dw_total.m1);
      VG_(umsg)(fmt, "LLd misses:   ",
                     D_total.mL, Dr_total.mL, Dw_total.mL);

      if (0 == D_total.a)  D_total.a = 1;
      if (0 == Dr_total.a) Dr_total.a = 1;
      if (0 == Dw_total.a) Dw_total.a = 1;
      VG_(umsg)("D1  miss rate: %*.1f%% (%*.1f%%     + %*.1f%%  )\n",
                l1, D_total.m1  * 100.0 / D_total.a,
                l2, Dr_total.m1 * 100.0 / Dr_total.a,
                l3, Dw_total.m1 * 100.0 / Dw_total.a);
      VG_(umsg)("LLd miss rate: %*.1f%% (%*.1f%%     + %*.1f%%  )\n",
                l1, D_total.mL  * 100.0 / D_total.a,
                l2, Dr_total.mL * 100.0 / Dr_total.a,
                l3, Dw_total.mL * 100.0 / Dw_total.a);
      VG_(umsg)("\n");

      

      LL_total   = Dr_total.m1 + Dw_total.m1 + Ir_total.m1;
      LL_total_r = Dr_total.m1 + Ir_total.m1;
      LL_total_w = Dw_total.m1;
      VG_(umsg)(fmt, "LL refs:      ",
                     LL_total, LL_total_r, LL_total_w);

      LL_total_m  = Dr_total.mL + Dw_total.mL + Ir_total.mL;
      LL_total_mr = Dr_total.mL + Ir_total.mL;
      LL_total_mw = Dw_total.mL;
      VG_(umsg)(fmt, "LL misses:    ",
                     LL_total_m, LL_total_mr, LL_total_mw);

      VG_(umsg)("LL miss rate:  %*.1f%% (%*.1f%%     + %*.1f%%  )\n",
                l1, LL_total_m  * 100.0 / (Ir_total.a + D_total.a),
                l2, LL_total_mr * 100.0 / (Ir_total.a + Dr_total.a),
                l3, LL_total_mw * 100.0 / Dw_total.a);
   }

   
   if (clo_branch_sim) {
      
      VG_(sprintf)(fmt, "%%s %%,%dllu  (%%,%dllu cond + %%,%dllu ind)\n",
                        l1, l2, l3);

      if (0 == Bc_total.b)  Bc_total.b = 1;
      if (0 == Bi_total.b)  Bi_total.b = 1;
      B_total.b  = Bc_total.b  + Bi_total.b;
      B_total.mp = Bc_total.mp + Bi_total.mp;

      VG_(umsg)("\n");
      VG_(umsg)(fmt, "Branches:     ",
                     B_total.b, Bc_total.b, Bi_total.b);

      VG_(umsg)(fmt, "Mispredicts:  ",
                     B_total.mp, Bc_total.mp, Bi_total.mp);

      VG_(umsg)("Mispred rate:  %*.1f%% (%*.1f%%     + %*.1f%%   )\n",
                l1, B_total.mp  * 100.0 / B_total.b,
                l2, Bc_total.mp * 100.0 / Bc_total.b,
                l3, Bi_total.mp * 100.0 / Bi_total.b);
   }

   
   if (VG_(clo_stats)) {
      Int debug_lookups = full_debugs      + fn_debugs +
                          file_line_debugs + no_debugs;

      VG_(dmsg)("\n");
      VG_(dmsg)("cachegrind: distinct files     : %d\n", distinct_files);
      VG_(dmsg)("cachegrind: distinct functions : %d\n", distinct_fns);
      VG_(dmsg)("cachegrind: distinct lines     : %d\n", distinct_lines);
      VG_(dmsg)("cachegrind: distinct instrs NoX: %d\n", distinct_instrsNoX);
      VG_(dmsg)("cachegrind: distinct instrs Gen: %d\n", distinct_instrsGen);
      VG_(dmsg)("cachegrind: debug lookups      : %d\n", debug_lookups);
      
      VG_(dmsg)("cachegrind: with full      info:%6.1f%% (%d)\n", 
                full_debugs * 100.0 / debug_lookups, full_debugs);
      VG_(dmsg)("cachegrind: with file/line info:%6.1f%% (%d)\n", 
                file_line_debugs * 100.0 / debug_lookups, file_line_debugs);
      VG_(dmsg)("cachegrind: with fn name   info:%6.1f%% (%d)\n", 
                fn_debugs * 100.0 / debug_lookups, fn_debugs);
      VG_(dmsg)("cachegrind: with zero      info:%6.1f%% (%d)\n", 
                no_debugs * 100.0 / debug_lookups, no_debugs);

      VG_(dmsg)("cachegrind: string table size: %lu\n",
                VG_(OSetGen_Size)(stringTable));
      VG_(dmsg)("cachegrind: CC table size: %lu\n",
                VG_(OSetGen_Size)(CC_table));
      VG_(dmsg)("cachegrind: InstrInfo table size: %lu\n",
                VG_(OSetGen_Size)(instrInfoTable));
   }
}


static
void cg_discard_superblock_info ( Addr orig_addr64, VexGuestExtents vge )
{
   SB_info* sbInfo;
   Addr     orig_addr = vge.base[0];

   tl_assert(vge.n_used > 0);

   if (DEBUG_CG)
      VG_(printf)( "discard_basic_block_info: %p, %p, %llu\n", 
                   (void*)orig_addr,
                   (void*)vge.base[0], (ULong)vge.len[0]);

   
   
   sbInfo = VG_(OSetGen_Remove)(instrInfoTable, &orig_addr);
   tl_assert(NULL != sbInfo);
   VG_(OSetGen_FreeNode)(instrInfoTable, sbInfo);
}


static Bool cg_process_cmd_line_option(const HChar* arg)
{
   if (VG_(str_clo_cache_opt)(arg,
                              &clo_I1_cache,
                              &clo_D1_cache,
                              &clo_LL_cache)) {}

   else if VG_STR_CLO( arg, "--cachegrind-out-file", clo_cachegrind_out_file) {}
   else if VG_BOOL_CLO(arg, "--cache-sim",  clo_cache_sim)  {}
   else if VG_BOOL_CLO(arg, "--branch-sim", clo_branch_sim) {}
   else
      return False;

   return True;
}

static void cg_print_usage(void)
{
   VG_(print_cache_clo_opts)();
   VG_(printf)(
"    --cache-sim=yes|no  [yes]        collect cache stats?\n"
"    --branch-sim=yes|no [no]         collect branch prediction stats?\n"
"    --cachegrind-out-file=<file>     output file name [cachegrind.out.%%p]\n"
   );
}

static void cg_print_debug_usage(void)
{
   VG_(printf)(
"    (none)\n"
   );
}


static void cg_post_clo_init(void); 

static void cg_pre_clo_init(void)
{
   VG_(details_name)            ("Cachegrind");
   VG_(details_version)         (NULL);
   VG_(details_description)     ("a cache and branch-prediction profiler");
   VG_(details_copyright_author)(
      "Copyright (C) 2002-2013, and GNU GPL'd, by Nicholas Nethercote et al.");
   VG_(details_bug_reports_to)  (VG_BUGS_TO);
   VG_(details_avg_translation_sizeB) ( 500 );

   VG_(clo_vex_control).iropt_register_updates_default
      = VG_(clo_px_file_backed)
      = VexRegUpdSpAtMemAccess; 

   VG_(basic_tool_funcs)          (cg_post_clo_init,
                                   cg_instrument,
                                   cg_fini);

   VG_(needs_superblock_discards)(cg_discard_superblock_info);
   VG_(needs_command_line_options)(cg_process_cmd_line_option,
                                   cg_print_usage,
                                   cg_print_debug_usage);
}

static void cg_post_clo_init(void)
{
   cache_t I1c, D1c, LLc; 

   CC_table =
      VG_(OSetGen_Create)(offsetof(LineCC, loc),
                          cmp_CodeLoc_LineCC,
                          VG_(malloc), "cg.main.cpci.1",
                          VG_(free));
   instrInfoTable =
      VG_(OSetGen_Create)(0,
                          NULL,
                          VG_(malloc), "cg.main.cpci.2",
                          VG_(free));
   stringTable =
      VG_(OSetGen_Create)(0,
                          stringCmp,
                          VG_(malloc), "cg.main.cpci.3",
                          VG_(free));

   VG_(post_clo_init_configure_caches)(&I1c, &D1c, &LLc,
                                       &clo_I1_cache,
                                       &clo_D1_cache,
                                       &clo_LL_cache);

   
   
   
   min_line_size = (I1c.line_size < D1c.line_size) ? I1c.line_size : D1c.line_size;
   min_line_size = (LLc.line_size < min_line_size) ? LLc.line_size : min_line_size;

   Int largest_load_or_store_size
      = VG_(machine_get_size_of_largest_guest_register)();
   if (min_line_size < largest_load_or_store_size) {
      VG_(umsg)("Cachegrind: cannot continue: the minimum line size (%d)\n",
                (Int)min_line_size);
      VG_(umsg)("  must be equal to or larger than the maximum register size (%d)\n",
                largest_load_or_store_size );
      VG_(umsg)("  but it is not.  Exiting now.\n");
      VG_(exit)(1);
   }

   cachesim_initcaches(I1c, D1c, LLc);
}

VG_DETERMINE_INTERFACE_VERSION(cg_pre_clo_init)


