

/*
   This file is part of Ptrcheck, a Valgrind tool for checking pointer
   use in programs.

   Initial version (Annelid):

   Copyright (C) 2003-2013 Nicholas Nethercote
      njn@valgrind.org

   Valgrind-3.X port:

   Copyright (C) 2008-2013 OpenWorks Ltd
      info@open-works.co.uk

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
#include "pub_tool_libcbase.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_execontext.h"
#include "pub_tool_hashtable.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_replacemalloc.h"
#include "pub_tool_options.h"
#include "pub_tool_execontext.h"
#include "pub_tool_aspacemgr.h"    
#include "pub_tool_vki.h"          
#include "pub_tool_machine.h"      
#include "pub_tool_debuginfo.h"    
#include "pub_tool_threadstate.h"  
#include "pub_tool_oset.h"
#include "pub_tool_vkiscnums.h"
#include "pub_tool_machine.h"
#include "pub_tool_wordfm.h"
#include "pub_tool_xarray.h"

#include "pc_common.h"

#include "h_main.h"

#include "sg_main.h"   




static ULong stats__client_mallocs = 0;
static ULong stats__client_frees   = 0;
static ULong stats__segs_allocd    = 0;
static ULong stats__segs_recycled  = 0;




#define N_FREED_SEGS (1 * 1000 * 1000)

struct _Seg {
   Addr  addr;
   SizeT szB; 
   ExeContext* ec;  
   struct _Seg* nextfree;
};

void Seg__cmp(Seg* seg, Addr a, Int* cmp, UWord* n)
{
   if (a < seg->addr) {
      *cmp = -1;
      *n   = seg->addr - a;
   } else if (a < seg->addr + seg->szB && seg->szB > 0) {
      *cmp = 0;
      *n = a - seg->addr;
   } else {
      *cmp = 1;
      *n = a - (seg->addr + seg->szB);
   }
}

 Bool Seg__is_freed(Seg* seg)
{
   if (!is_known_segment(seg))
      return False;
   else
      return seg->nextfree != (Seg*)1;
}

ExeContext* Seg__where(Seg* seg)
{
   tl_assert(is_known_segment(seg));
   return seg->ec;
}

SizeT Seg__size(Seg* seg)
{
   tl_assert(is_known_segment(seg));
   return seg->szB;
}

Addr Seg__addr(Seg* seg)
{
   tl_assert(is_known_segment(seg));
   return seg->addr;
}


#define N_SEGS_PER_GROUP 10000

typedef
   struct _SegGroup {
      struct _SegGroup* admin;
      UWord nextfree; 
      Seg segs[N_SEGS_PER_GROUP];
   }
   SegGroup;

static SegGroup* group_list = NULL;
static UWord     nFreeSegs = 0;
static Seg*      freesegs_youngest = NULL;
static Seg*      freesegs_oldest = NULL;


static SegGroup* new_SegGroup ( void ) {
   SegGroup* g = VG_(malloc)("pc.h_main.nTG.1", sizeof(SegGroup));
   VG_(memset)(g, 0, sizeof(*g));
   return g;
}

static Seg* new_Seg ( void )
{
   Seg*      teg;
   SegGroup* g;
   if (group_list == NULL) {
      g = new_SegGroup();
      g->admin = NULL;
      group_list = g;
   }
   tl_assert(group_list->nextfree <= N_SEGS_PER_GROUP);
   if (group_list->nextfree == N_SEGS_PER_GROUP) {
      g = new_SegGroup();
      g->admin = group_list;
      group_list = g;
   }
   tl_assert(group_list->nextfree < N_SEGS_PER_GROUP);
   teg = &group_list->segs[ group_list->nextfree ];
   group_list->nextfree++;
   stats__segs_allocd++;
   return teg;
}

static Seg* get_Seg_for_malloc ( void )
{
   Seg* seg;
   if (nFreeSegs < N_FREED_SEGS) {
      seg = new_Seg();
      seg->nextfree = (Seg*)1;
      return seg;
   }
   
   tl_assert(freesegs_youngest);
   tl_assert(freesegs_oldest);
   tl_assert(freesegs_youngest != freesegs_oldest);
   seg = freesegs_oldest;
   freesegs_oldest = seg->nextfree;
   nFreeSegs--;
   seg->nextfree = (Seg*)1;
   stats__segs_recycled++;
   return seg;
}

static void set_Seg_freed ( Seg* seg )
{
   tl_assert(seg);
   tl_assert(!Seg__is_freed(seg));
   if (nFreeSegs == 0) {
      tl_assert(freesegs_oldest == NULL);
      tl_assert(freesegs_youngest == NULL);
      seg->nextfree = NULL;
      freesegs_youngest = seg;
      freesegs_oldest = seg;
      nFreeSegs++;
   } else {
      tl_assert(freesegs_youngest);
      tl_assert(freesegs_oldest);
      if (nFreeSegs == 1) {
         tl_assert(freesegs_youngest == freesegs_oldest);
      } else {
         tl_assert(freesegs_youngest != freesegs_oldest);
      }
      tl_assert(freesegs_youngest->nextfree == NULL);
      tl_assert(seg != freesegs_youngest && seg != freesegs_oldest);
      seg->nextfree = NULL;
      freesegs_youngest->nextfree = seg;
      freesegs_youngest = seg;
      nFreeSegs++;
   }
}

static WordFM* addr_to_seg_map = NULL; 

static void addr_to_seg_map_ENSURE_INIT ( void )
{
   if (UNLIKELY(addr_to_seg_map == NULL)) {
      addr_to_seg_map = VG_(newFM)( VG_(malloc), "pc.h_main.attmEI.1",
                                    VG_(free), NULL );
   }
}

static Seg* find_Seg_by_addr ( Addr ga )
{
   UWord keyW, valW;
   addr_to_seg_map_ENSURE_INIT();
   if (VG_(lookupFM)( addr_to_seg_map, &keyW, &valW, (UWord)ga )) {
      tl_assert(keyW == ga);
      return (Seg*)valW;
   } else {
      return NULL;
   }
}

static void bind_addr_to_Seg ( Addr ga, Seg* seg )
{
   Bool b;
   addr_to_seg_map_ENSURE_INIT();
   b = VG_(addToFM)( addr_to_seg_map, (UWord)ga, (UWord)seg );
   tl_assert(!b); 
}

static void unbind_addr_from_Seg ( Addr ga )
{
   Bool b;
   UWord keyW, valW;
   addr_to_seg_map_ENSURE_INIT();
   b = VG_(delFromFM)( addr_to_seg_map, &keyW, &valW, (UWord)ga );
   tl_assert(b); 
   tl_assert(keyW == ga);
   tl_assert(valW != 0);
}



static Seg* add_new_segment ( ThreadId tid, Addr p, SizeT size )
{
   Seg* seg = get_Seg_for_malloc();
   tl_assert(seg != (Seg*)1); 
   seg->addr = p;
   seg->szB  = size;
   seg->ec   = VG_(record_ExeContext)( tid, 0 );
   tl_assert(!Seg__is_freed(seg));

   bind_addr_to_Seg(p, seg);

   return seg;
}



static
void* alloc_and_new_mem_heap ( ThreadId tid,
                               SizeT size, SizeT alignment, Bool is_zeroed )
{
   Addr p;

   if ( ((SSizeT)size) < 0) return NULL;

   p = (Addr)VG_(cli_malloc)(alignment, size);
   if (is_zeroed) VG_(memset)((void*)p, 0, size);

   add_new_segment( tid, p, size );

   stats__client_mallocs++;
   return (void*)p;
}

static void die_and_free_mem_heap ( ThreadId tid, Seg* seg )
{
   
   tl_assert(!Seg__is_freed(seg));

   VG_(cli_free)( (void*)seg->addr );

   
   seg->ec = VG_(record_ExeContext)( tid, 0 );

   set_Seg_freed(seg);
   unbind_addr_from_Seg( seg->addr );

   stats__client_frees++;
}

static void handle_free_heap( ThreadId tid, void* p )
{
   Seg* seg = find_Seg_by_addr( (Addr)p );
   if (!seg) {
      
      return;
   }
   die_and_free_mem_heap( tid, seg );
}



void* h_replace_malloc ( ThreadId tid, SizeT n )
{
   return alloc_and_new_mem_heap ( tid, n, VG_(clo_alignment),
                                        False );
}

void* h_replace___builtin_new ( ThreadId tid, SizeT n )
{
   return alloc_and_new_mem_heap ( tid, n, VG_(clo_alignment),
                                           False );
}

void* h_replace___builtin_vec_new ( ThreadId tid, SizeT n )
{
   return alloc_and_new_mem_heap ( tid, n, VG_(clo_alignment),
                                           False );
}

void* h_replace_memalign ( ThreadId tid, SizeT align, SizeT n )
{
   return alloc_and_new_mem_heap ( tid, n, align,
                                        False );
}

void* h_replace_calloc ( ThreadId tid, SizeT nmemb, SizeT size1 )
{
   return alloc_and_new_mem_heap ( tid, nmemb*size1, VG_(clo_alignment),
                                        True );
}

void h_replace_free ( ThreadId tid, void* p )
{
   
   
   
   
   
   
   
   
   
   
   
   

   handle_free_heap(tid, p);
}

void h_replace___builtin_delete ( ThreadId tid, void* p )
{
   handle_free_heap(tid, p);
}

void h_replace___builtin_vec_delete ( ThreadId tid, void* p )
{
   handle_free_heap(tid, p);
}

void* h_replace_realloc ( ThreadId tid, void* p_old, SizeT new_size )
{
   Seg* seg;

   
   seg = find_Seg_by_addr( (Addr)p_old );
   if (!seg)
      return NULL;

   tl_assert(seg->addr == (Addr)p_old);

   if (new_size <= seg->szB) {
      
      Addr p_new = (Addr)VG_(cli_malloc)(VG_(clo_alignment), new_size);
      VG_(memcpy)((void*)p_new, p_old, new_size);

      
      die_and_free_mem_heap( tid, seg );

      add_new_segment ( tid, p_new, new_size );

      return (void*)p_new;
   } else {
      
      Addr p_new = (Addr)VG_(cli_malloc)(VG_(clo_alignment), new_size);
      VG_(memcpy)((void*)p_new, p_old, seg->szB);

      
      die_and_free_mem_heap( tid, seg );

      add_new_segment ( tid, p_new, new_size );

      return (void*)p_new;
   }
}

SizeT h_replace_malloc_usable_size ( ThreadId tid, void* p )
{
   Seg* seg = find_Seg_by_addr( (Addr)p );

   
   
   return ( seg ? seg->szB : 0 );
}





typedef
   enum { NonShad=1, Shad=2 }
   TempKind;

typedef
   struct {
      TempKind kind;
      IRTemp   shadow;
   }
   TempMapEnt;



typedef
   struct {
      IRSB* sb;
      Bool  trace;

      XArray*  qmpMap;

      IRType hWordTy;

      
      IRType gWordTy;

      Int guest_state_sizeB;
   }
   PCEnv;


static IRTemp newTemp ( PCEnv* pce, IRType ty, TempKind kind )
{
   Word       newIx;
   TempMapEnt ent;
   IRTemp     tmp = newIRTemp(pce->sb->tyenv, ty);
   ent.kind   = kind;
   ent.shadow = IRTemp_INVALID;
   newIx = VG_(addToXA)( pce->qmpMap, &ent );
   tl_assert(newIx == (Word)tmp);
   return tmp;
}


static  void stmt ( HChar cat, PCEnv* pce, IRStmt* st ) {
   if (pce->trace) {
      VG_(printf)("  %c: ", cat);
      ppIRStmt(st);
      VG_(printf)("\n");
   }
   addStmtToIRSB(pce->sb, st);
}

static IRTemp for_sg__newIRTemp_cb ( IRType ty, void* opaque )
{
   PCEnv* pce = (PCEnv*)opaque;
   return newTemp( pce, ty, NonShad );
}


IRSB* h_instrument ( VgCallbackClosure* closure,
                     IRSB* sbIn,
                     const VexGuestLayout* layout,
                     const VexGuestExtents* vge,
                     const VexArchInfo* archinfo_host,
                     IRType gWordTy, IRType hWordTy )
{
   Bool  verboze = 0||False;
   Int   i ;
   PCEnv pce;
   struct _SGEnv* sgenv;

   if (gWordTy != hWordTy) {
      
      VG_(tool_panic)("host/guest word size mismatch");
   }

   
   tl_assert(sizeof(UWord)  == sizeof(void*));
   tl_assert(sizeof(Word)   == sizeof(void*));
   tl_assert(sizeof(Addr)   == sizeof(void*));
   tl_assert(sizeof(ULong)  == 8);
   tl_assert(sizeof(Long)   == 8);
   tl_assert(sizeof(Addr)   == sizeof(void*));
   tl_assert(sizeof(UInt)   == 4);
   tl_assert(sizeof(Int)    == 4);

   VG_(memset)(&pce, 0, sizeof(pce));
   pce.sb                = deepCopyIRSBExceptStmts(sbIn);
   pce.trace             = verboze;
   pce.hWordTy           = hWordTy;
   pce.gWordTy           = gWordTy;
   pce.guest_state_sizeB = layout->total_sizeB;

   pce.qmpMap = VG_(newXA)( VG_(malloc), "pc.h_instrument.1", VG_(free),
                            sizeof(TempMapEnt));
   for (i = 0; i < sbIn->tyenv->types_used; i++) {
      TempMapEnt ent;
      ent.kind   = NonShad;
      ent.shadow = IRTemp_INVALID;
      VG_(addToXA)( pce.qmpMap, &ent );
   }
   tl_assert( VG_(sizeXA)( pce.qmpMap ) == sbIn->tyenv->types_used );

   sgenv = sg_instrument_init( for_sg__newIRTemp_cb,
                               (void*)&pce );

   

   i = 0;
   while (i < sbIn->stmts_used && sbIn->stmts[i]->tag != Ist_IMark) {
      IRStmt* st = sbIn->stmts[i];
      tl_assert(st);
      tl_assert(isFlatIRStmt(st));
      stmt( 'C', &pce, sbIn->stmts[i] );
      i++;
   }

   

   tl_assert(sbIn->stmts_used > 0);
   tl_assert(i >= 0);
   tl_assert(i < sbIn->stmts_used);
   tl_assert(sbIn->stmts[i]->tag == Ist_IMark);

   for (; i < sbIn->stmts_used; i++) {
      
      sg_instrument_IRStmt( sgenv, pce.sb, sbIn->stmts[i],
                            layout, gWordTy, hWordTy );

      stmt( 'C', &pce, sbIn->stmts[i] );
   }

   
   sg_instrument_final_jump( sgenv, pce.sb, sbIn->next, sbIn->jumpkind,
                             layout, gWordTy, hWordTy );

   
   sg_instrument_fini( sgenv );

   tl_assert( VG_(sizeXA)( pce.qmpMap ) == pce.sb->tyenv->types_used );
   VG_(deleteXA)( pce.qmpMap );

   return pce.sb;
}



void h_fini ( Int exitcode )
{
   if (VG_(clo_verbosity) == 1 && !VG_(clo_xml)) {
      VG_(message)(Vg_UserMsg, 
                   "For counts of detected and suppressed errors, "
                   "rerun with: -v\n");
   }

   if (VG_(clo_stats)) {
      VG_(message)(Vg_DebugMsg,
                   "  h_:  %'10llu client allocs, %'10llu client frees\n", 
                   stats__client_mallocs, stats__client_frees);
      VG_(message)(Vg_DebugMsg,
                   "  h_:  %'10llu Segs allocd,   %'10llu Segs recycled\n", 
                   stats__segs_allocd, stats__segs_recycled);
   }
}


