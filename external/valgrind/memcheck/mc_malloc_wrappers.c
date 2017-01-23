

/*
   This file is part of MemCheck, a heavyweight Valgrind tool for
   detecting memory errors.

   Copyright (C) 2000-2013 Julian Seward 
      jseward@acm.org

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
#include "pub_tool_execontext.h"
#include "pub_tool_poolalloc.h"
#include "pub_tool_hashtable.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_options.h"
#include "pub_tool_replacemalloc.h"
#include "pub_tool_threadstate.h"
#include "pub_tool_tooliface.h"     
#include "pub_tool_stacktrace.h"    

#include "mc_include.h"


static SizeT cmalloc_n_mallocs  = 0;
static SizeT cmalloc_n_frees    = 0;
static ULong cmalloc_bs_mallocd = 0;

#define MEMPOOL_DEBUG_STACKTRACE_DEPTH 16



SizeT MC_(Malloc_Redzone_SzB) = -10000000; 

VgHashTable *MC_(malloc_list) = NULL;

VgHashTable *MC_(mempool_list) = NULL;

   
PoolAlloc *MC_(chunk_poolalloc) = NULL;
static
MC_Chunk* create_MC_Chunk ( ThreadId tid, Addr p, SizeT szB,
                            MC_AllocKind kind);
static inline
void delete_MC_Chunk (MC_Chunk* mc);

static MC_Chunk* freed_list_start[2]  = {NULL, NULL};
static MC_Chunk* freed_list_end[2]    = {NULL, NULL};

static void add_to_freed_queue ( MC_Chunk* mc )
{
   const Bool show = False;
   const int l = (mc->szB >= MC_(clo_freelist_big_blocks) ? 0 : 1);

   if (freed_list_end[l] == NULL) {
      tl_assert(freed_list_start[l] == NULL);
      mc->next = NULL;
      freed_list_end[l]    = freed_list_start[l] = mc;
   } else {
      tl_assert(freed_list_end[l]->next == NULL);
      if (mc->szB >= MC_(clo_freelist_vol)) {
         mc->next = freed_list_start[l];
         freed_list_start[l] = mc;
      } else {
         mc->next = NULL;
         freed_list_end[l]->next = mc;
         freed_list_end[l]       = mc;
      }
   }
   VG_(free_queue_volume) += (Long)mc->szB;
   if (show)
      VG_(printf)("mc_freelist: acquire: volume now %lld\n", 
                  VG_(free_queue_volume));
   VG_(free_queue_length)++;
}

static void release_oldest_block(void)
{
   const Bool show = False;
   int i;
   tl_assert (VG_(free_queue_volume) > MC_(clo_freelist_vol));
   tl_assert (freed_list_start[0] != NULL || freed_list_start[1] != NULL);

   for (i = 0; i < 2; i++) {
      while (VG_(free_queue_volume) > MC_(clo_freelist_vol)
             && freed_list_start[i] != NULL) {
         MC_Chunk* mc1;

         tl_assert(freed_list_end[i] != NULL);
         
         mc1 = freed_list_start[i];
         VG_(free_queue_volume) -= (Long)mc1->szB;
         VG_(free_queue_length)--;
         if (show)
            VG_(printf)("mc_freelist: discard: volume now %lld\n", 
                        VG_(free_queue_volume));
         tl_assert(VG_(free_queue_volume) >= 0);
         
         if (freed_list_start[i] == freed_list_end[i]) {
            freed_list_start[i] = freed_list_end[i] = NULL;
         } else {
            freed_list_start[i] = mc1->next;
         }
         mc1->next = NULL; 

         
         if (MC_AllocCustom != mc1->allockind)
            VG_(cli_free) ( (void*)(mc1->data) );
         delete_MC_Chunk ( mc1 );
      }
   }
}

MC_Chunk* MC_(get_freed_block_bracketting) (Addr a)
{
   int i;
   for (i = 0; i < 2; i++) {
      MC_Chunk*  mc;
      mc = freed_list_start[i];
      while (mc) {
         if (VG_(addr_is_in_block)( a, mc->data, mc->szB,
                                    MC_(Malloc_Redzone_SzB) ))
            return mc;
         mc = mc->next;
      }
   }
   return NULL;
}

static
MC_Chunk* create_MC_Chunk ( ThreadId tid, Addr p, SizeT szB,
                            MC_AllocKind kind)
{
   MC_Chunk* mc  = VG_(allocEltPA)(MC_(chunk_poolalloc));
   mc->data      = p;
   mc->szB       = szB;
   mc->allockind = kind;
   switch ( MC_(n_where_pointers)() ) {
      case 2: mc->where[1] = 0; 
      case 1: mc->where[0] = 0; 
      case 0: break;
      default: tl_assert(0);
   }
   MC_(set_allocated_at) (tid, mc);

   if (VG_(free_queue_volume) > MC_(clo_freelist_vol))
      release_oldest_block();

   if (!MC_(check_mem_is_noaccess)( (Addr)mc, sizeof(MC_Chunk), NULL )) {
      VG_(tool_panic)("create_MC_Chunk: shadow area is accessible");
   } 
   return mc;
}

static inline
void delete_MC_Chunk (MC_Chunk* mc)
{
   VG_(freeEltPA) (MC_(chunk_poolalloc), mc);
}

static Bool in_block_list (const VgHashTable *block_list, MC_Chunk* mc)
{
   MC_Chunk* found_mc = VG_(HT_lookup) ( block_list, (UWord)mc->data );
   if (found_mc) {
      tl_assert (found_mc->data == mc->data);
      if (found_mc->szB != mc->szB
          || found_mc->allockind != mc->allockind)
         return False;
      tl_assert (found_mc == mc);
      return True;
   } else
      return False;
}

static Bool live_block (MC_Chunk* mc)
{
   if (mc->allockind == MC_AllocCustom) {
      MC_Mempool* mp;
      VG_(HT_ResetIter)(MC_(mempool_list));
      while ( (mp = VG_(HT_Next)(MC_(mempool_list))) ) {
         if ( in_block_list (mp->chunks, mc) )
            return True;
      }
   }
   return in_block_list ( MC_(malloc_list), mc );
}

ExeContext* MC_(allocated_at) (MC_Chunk* mc)
{
   switch (MC_(clo_keep_stacktraces)) {
      case KS_none:            return VG_(null_ExeContext) ();
      case KS_alloc:           return mc->where[0];
      case KS_free:            return VG_(null_ExeContext) ();
      case KS_alloc_then_free: return (live_block(mc) ?
                                       mc->where[0] : VG_(null_ExeContext) ());
      case KS_alloc_and_free:  return mc->where[0];
      default: tl_assert (0);
   }
}

ExeContext* MC_(freed_at) (MC_Chunk* mc)
{
   switch (MC_(clo_keep_stacktraces)) {
      case KS_none:            return VG_(null_ExeContext) ();
      case KS_alloc:           return VG_(null_ExeContext) ();
      case KS_free:            return (mc->where[0] ?
                                       mc->where[0] : VG_(null_ExeContext) ());
      case KS_alloc_then_free: return (live_block(mc) ?
                                       VG_(null_ExeContext) () : mc->where[0]);
      case KS_alloc_and_free:  return (mc->where[1] ?
                                       mc->where[1] : VG_(null_ExeContext) ());
      default: tl_assert (0);
   }
}

void  MC_(set_allocated_at) (ThreadId tid, MC_Chunk* mc)
{
   switch (MC_(clo_keep_stacktraces)) {
      case KS_none:            return;
      case KS_alloc:           break;
      case KS_free:            return;
      case KS_alloc_then_free: break;
      case KS_alloc_and_free:  break;
      default: tl_assert (0);
   }
   mc->where[0] = VG_(record_ExeContext) ( tid, 0 );
}

void  MC_(set_freed_at) (ThreadId tid, MC_Chunk* mc)
{
   UInt pos;
   switch (MC_(clo_keep_stacktraces)) {
      case KS_none:            return;
      case KS_alloc:           return;
      case KS_free:            pos = 0; break;
      case KS_alloc_then_free: pos = 0; break;
      case KS_alloc_and_free:  pos = 1; break;
      default: tl_assert (0);
   }
   mc->where[pos] = VG_(record_ExeContext) ( tid, 0 );
}

UInt MC_(n_where_pointers) (void)
{
   switch (MC_(clo_keep_stacktraces)) {
      case KS_none:            return 0;
      case KS_alloc:
      case KS_free:
      case KS_alloc_then_free: return 1;
      case KS_alloc_and_free:  return 2;
      default: tl_assert (0);
   }
}


void* MC_(new_block) ( ThreadId tid,
                       Addr p, SizeT szB, SizeT alignB,
                       Bool is_zeroed, MC_AllocKind kind, VgHashTable *table)
{
   MC_Chunk* mc;

   
   if (p) {
      tl_assert(MC_AllocCustom == kind);
   } else {
      tl_assert(MC_AllocCustom != kind);
      p = (Addr)VG_(cli_malloc)( alignB, szB );
      if (!p) {
         return NULL;
      }
      if (is_zeroed) {
         VG_(memset)((void*)p, 0, szB);
      } else 
      if (MC_(clo_malloc_fill) != -1) {
         tl_assert(MC_(clo_malloc_fill) >= 0x00 && MC_(clo_malloc_fill) <= 0xFF);
         VG_(memset)((void*)p, MC_(clo_malloc_fill), szB);
      }
   }

   
   cmalloc_n_mallocs ++;
   cmalloc_bs_mallocd += (ULong)szB;
   mc = create_MC_Chunk (tid, p, szB, kind);
   VG_(HT_add_node)( table, mc );

   if (is_zeroed)
      MC_(make_mem_defined)( p, szB );
   else {
      UInt ecu = VG_(get_ECU_from_ExeContext)(MC_(allocated_at)(mc));
      tl_assert(VG_(is_plausible_ECU)(ecu));
      MC_(make_mem_undefined_w_otag)( p, szB, ecu | MC_OKIND_HEAP );
   }

   return (void*)p;
}

void* MC_(malloc) ( ThreadId tid, SizeT n )
{
   if (MC_(record_fishy_value_error)(tid, "malloc", "size", n)) {
      return NULL;
   } else {
      return MC_(new_block) ( tid, 0, n, VG_(clo_alignment), 
         False, MC_AllocMalloc, MC_(malloc_list));
   }
}

void* MC_(__builtin_new) ( ThreadId tid, SizeT n )
{
   if (MC_(record_fishy_value_error)(tid, "__builtin_new", "size", n)) {
      return NULL;
   } else {
      return MC_(new_block) ( tid, 0, n, VG_(clo_alignment), 
         False, MC_AllocNew, MC_(malloc_list));
   }
}

void* MC_(__builtin_vec_new) ( ThreadId tid, SizeT n )
{
   if (MC_(record_fishy_value_error)(tid, "__builtin_vec_new", "size", n)) {
      return NULL;
   } else {
      return MC_(new_block) ( tid, 0, n, VG_(clo_alignment), 
         False, MC_AllocNewVec, MC_(malloc_list));
   }
}

void* MC_(memalign) ( ThreadId tid, SizeT alignB, SizeT n )
{
   if (MC_(record_fishy_value_error)(tid, "memalign", "size", n)) {
      return NULL;
   } else {
      return MC_(new_block) ( tid, 0, n, alignB, 
         False, MC_AllocMalloc, MC_(malloc_list));
   }
}

void* MC_(calloc) ( ThreadId tid, SizeT nmemb, SizeT size1 )
{
   if (MC_(record_fishy_value_error)(tid, "calloc", "nmemb", nmemb) ||
       MC_(record_fishy_value_error)(tid, "calloc", "size", size1)) {
      return NULL;
   } else {
      return MC_(new_block) ( tid, 0, nmemb*size1, VG_(clo_alignment),
         True, MC_AllocMalloc, MC_(malloc_list));
   }
}

static
void die_and_free_mem ( ThreadId tid, MC_Chunk* mc, SizeT rzB )
{
   if (MC_(clo_free_fill) != -1 && MC_AllocCustom != mc->allockind ) {
      tl_assert(MC_(clo_free_fill) >= 0x00 && MC_(clo_free_fill) <= 0xFF);
      VG_(memset)((void*)mc->data, MC_(clo_free_fill), mc->szB);
   }

   MC_(make_mem_noaccess)( mc->data-rzB, mc->szB + 2*rzB );

   
   MC_(set_freed_at) (tid, mc);
   
   add_to_freed_queue ( mc );
}


static
void record_freemismatch_error (ThreadId tid, MC_Chunk* mc)
{
   
   if (!MC_(clo_show_mismatched_frees))
      return;

   VG_(HT_add_node)( MC_(malloc_list), mc );
   MC_(record_freemismatch_error) ( tid, mc );
   if ((mc != VG_(HT_remove) ( MC_(malloc_list), (UWord)mc->data )))
      tl_assert(0);
}

void MC_(handle_free) ( ThreadId tid, Addr p, UInt rzB, MC_AllocKind kind )
{
   MC_Chunk* mc;

   cmalloc_n_frees++;

   mc = VG_(HT_remove) ( MC_(malloc_list), (UWord)p );
   if (mc == NULL) {
      MC_(record_free_error) ( tid, p );
   } else {
      
      if (kind != mc->allockind) {
         tl_assert(p == mc->data);
         record_freemismatch_error ( tid, mc );
      }
      die_and_free_mem ( tid, mc, rzB );
   }
}

void MC_(free) ( ThreadId tid, void* p )
{
   MC_(handle_free)( 
      tid, (Addr)p, MC_(Malloc_Redzone_SzB), MC_AllocMalloc );
}

void MC_(__builtin_delete) ( ThreadId tid, void* p )
{
   MC_(handle_free)(
      tid, (Addr)p, MC_(Malloc_Redzone_SzB), MC_AllocNew);
}

void MC_(__builtin_vec_delete) ( ThreadId tid, void* p )
{
   MC_(handle_free)(
      tid, (Addr)p, MC_(Malloc_Redzone_SzB), MC_AllocNewVec);
}

void* MC_(realloc) ( ThreadId tid, void* p_old, SizeT new_szB )
{
   MC_Chunk* old_mc;
   MC_Chunk* new_mc;
   Addr      a_new; 
   SizeT     old_szB;

   if (MC_(record_fishy_value_error)(tid, "realloc", "size", new_szB))
      return NULL;

   cmalloc_n_frees ++;
   cmalloc_n_mallocs ++;
   cmalloc_bs_mallocd += (ULong)new_szB;

   
   old_mc = VG_(HT_remove) ( MC_(malloc_list), (UWord)p_old );
   if (old_mc == NULL) {
      MC_(record_free_error) ( tid, (Addr)p_old );
      
      return NULL;
   }

   
   if (MC_AllocMalloc != old_mc->allockind) {
      
      tl_assert((Addr)p_old == old_mc->data);
      record_freemismatch_error ( tid, old_mc );
      
   }

   old_szB = old_mc->szB;

   
   a_new = (Addr)VG_(cli_malloc)(VG_(clo_alignment), new_szB);

   if (a_new) {

      
      new_mc = create_MC_Chunk( tid, a_new, new_szB, MC_AllocMalloc );

      
      VG_(HT_add_node)( MC_(malloc_list), new_mc );

      

      
      MC_(make_mem_noaccess)( a_new-MC_(Malloc_Redzone_SzB), 
                              MC_(Malloc_Redzone_SzB) );

      
      if (old_szB >= new_szB) {
         

         
         MC_(copy_address_range_state) ( (Addr)p_old, a_new, new_szB );
         VG_(memcpy)((void*)a_new, p_old, new_szB);
      } else {
         
         UInt        ecu;

         
         MC_(copy_address_range_state) ( (Addr)p_old, a_new, old_szB );
         VG_(memcpy)((void*)a_new, p_old, old_szB);

         
         
         
         ecu = VG_(get_ECU_from_ExeContext)(MC_(allocated_at)(new_mc));
         tl_assert(VG_(is_plausible_ECU)(ecu));
         MC_(make_mem_undefined_w_otag)( a_new+old_szB,
                                         new_szB-old_szB,
                                         ecu | MC_OKIND_HEAP );

         
         if (MC_(clo_malloc_fill) != -1) {
            tl_assert(MC_(clo_malloc_fill) >= 0x00
                      && MC_(clo_malloc_fill) <= 0xFF);
            VG_(memset)((void*)(a_new+old_szB), MC_(clo_malloc_fill), 
                                                new_szB-old_szB);
         }
      }

      
      MC_(make_mem_noaccess)        ( a_new+new_szB, MC_(Malloc_Redzone_SzB));

      
      if (MC_(clo_free_fill) != -1) {
         tl_assert(MC_(clo_free_fill) >= 0x00 && MC_(clo_free_fill) <= 0xFF);
         VG_(memset)((void*)p_old, MC_(clo_free_fill), old_szB);
      }

      
      die_and_free_mem ( tid, old_mc, MC_(Malloc_Redzone_SzB) );

   } else {
      VG_(HT_add_node)( MC_(malloc_list), old_mc );
   }

   return (void*)a_new;
}

SizeT MC_(malloc_usable_size) ( ThreadId tid, void* p )
{
   MC_Chunk* mc = VG_(HT_lookup) ( MC_(malloc_list), (UWord)p );

   
   
   return ( mc ? mc->szB : 0 );
}

void MC_(handle_resizeInPlace)(ThreadId tid, Addr p,
                               SizeT oldSizeB, SizeT newSizeB, SizeT rzB)
{
   MC_Chunk* mc = VG_(HT_lookup) ( MC_(malloc_list), (UWord)p );
   if (!mc || mc->szB != oldSizeB || newSizeB == 0) {
      MC_(record_free_error) ( tid, p );
      return;
   }

   if (oldSizeB == newSizeB)
      return;

   mc->szB = newSizeB;
   if (newSizeB < oldSizeB) {
      MC_(make_mem_noaccess)( p + newSizeB, oldSizeB - newSizeB + rzB );
   } else {
      ExeContext* ec  = VG_(record_ExeContext)(tid, 0);
      UInt        ecu = VG_(get_ECU_from_ExeContext)(ec);
      MC_(make_mem_undefined_w_otag)( p + oldSizeB, newSizeB - oldSizeB,
                                      ecu | MC_OKIND_HEAP );
      if (rzB > 0)
         MC_(make_mem_noaccess)( p + newSizeB, rzB );
   }
}



#define MP_DETAILED_SANITY_CHECKS 0

static void check_mempool_sane(MC_Mempool* mp); 


void MC_(create_mempool)(Addr pool, UInt rzB, Bool is_zeroed)
{
   MC_Mempool* mp;

   if (VG_(clo_verbosity) > 2) {
      VG_(message)(Vg_UserMsg, "create_mempool(0x%lx, %d, %d)\n",
                               pool, rzB, is_zeroed);
      VG_(get_and_pp_StackTrace)
         (VG_(get_running_tid)(), MEMPOOL_DEBUG_STACKTRACE_DEPTH);
   }

   mp = VG_(HT_lookup)(MC_(mempool_list), (UWord)pool);
   if (mp != NULL) {
     VG_(tool_panic)("MC_(create_mempool): duplicate pool creation");
   }
   
   mp = VG_(malloc)("mc.cm.1", sizeof(MC_Mempool));
   mp->pool       = pool;
   mp->rzB        = rzB;
   mp->is_zeroed  = is_zeroed;
   mp->chunks     = VG_(HT_construct)( "MC_(create_mempool)" );
   check_mempool_sane(mp);

   if (!MC_(check_mem_is_noaccess)( (Addr)mp, sizeof(MC_Mempool), NULL )) {
      VG_(tool_panic)("MC_(create_mempool): shadow area is accessible");
   } 

   VG_(HT_add_node)( MC_(mempool_list), mp );
}

void MC_(destroy_mempool)(Addr pool)
{
   MC_Chunk*   mc;
   MC_Mempool* mp;

   if (VG_(clo_verbosity) > 2) {
      VG_(message)(Vg_UserMsg, "destroy_mempool(0x%lx)\n", pool);
      VG_(get_and_pp_StackTrace)
         (VG_(get_running_tid)(), MEMPOOL_DEBUG_STACKTRACE_DEPTH);
   }

   mp = VG_(HT_remove) ( MC_(mempool_list), (UWord)pool );

   if (mp == NULL) {
      ThreadId tid = VG_(get_running_tid)();
      MC_(record_illegal_mempool_error) ( tid, pool );
      return;
   }
   check_mempool_sane(mp);

   
   VG_(HT_ResetIter)(mp->chunks);
   while ( (mc = VG_(HT_Next)(mp->chunks)) ) {
      MC_(make_mem_noaccess)(mc->data-mp->rzB, mc->szB + 2*mp->rzB );
   }
   
   VG_(HT_destruct)(mp->chunks, (void (*)(void *))delete_MC_Chunk);

   VG_(free)(mp);
}

static Int 
mp_compar(const void* n1, const void* n2)
{
   const MC_Chunk* mc1 = *(const MC_Chunk *const *)n1;
   const MC_Chunk* mc2 = *(const MC_Chunk *const *)n2;
   if (mc1->data < mc2->data) return -1;
   if (mc1->data > mc2->data) return  1;
   return 0;
}

static void 
check_mempool_sane(MC_Mempool* mp)
{
   UInt n_chunks, i, bad = 0;   
   static UInt tick = 0;

   MC_Chunk **chunks = (MC_Chunk**) VG_(HT_to_array)( mp->chunks, &n_chunks );
   if (!chunks)
      return;

   if (VG_(clo_verbosity) > 1) {
     if (tick++ >= 10000)
       {
	 UInt total_pools = 0, total_chunks = 0;
	 MC_Mempool* mp2;
	 
	 VG_(HT_ResetIter)(MC_(mempool_list));
	 while ( (mp2 = VG_(HT_Next)(MC_(mempool_list))) ) {
	   total_pools++;
	   VG_(HT_ResetIter)(mp2->chunks);
	   while (VG_(HT_Next)(mp2->chunks)) {
	     total_chunks++;
	   }
	 }
	 
         VG_(message)(Vg_UserMsg, 
                      "Total mempools active: %d pools, %d chunks\n", 
		      total_pools, total_chunks);
	 tick = 0;
       }
   }


   VG_(ssort)((void*)chunks, n_chunks, sizeof(VgHashNode*), mp_compar);
         
   
   for (i = 0; i < n_chunks-1; i++) {
      if (chunks[i]->data > chunks[i+1]->data) {
         VG_(message)(Vg_UserMsg, 
                      "Mempool chunk %d / %d is out of order "
                      "wrt. its successor\n", 
                      i+1, n_chunks);
         bad = 1;
      }
   }
   
   
   for (i = 0; i < n_chunks-1; i++) {
      if (chunks[i]->data + chunks[i]->szB > chunks[i+1]->data ) {
         VG_(message)(Vg_UserMsg, 
                      "Mempool chunk %d / %d overlaps with its successor\n", 
                      i+1, n_chunks);
         bad = 1;
      }
   }

   if (bad) {
         VG_(message)(Vg_UserMsg, 
                "Bad mempool (%d chunks), dumping chunks for inspection:\n",
                n_chunks);
         for (i = 0; i < n_chunks; ++i) {
            VG_(message)(Vg_UserMsg, 
                         "Mempool chunk %d / %d: %ld bytes "
                         "[%lx,%lx), allocated:\n",
                         i+1, 
                         n_chunks, 
                         chunks[i]->szB + 0UL,
                         chunks[i]->data, 
                         chunks[i]->data + chunks[i]->szB);

            VG_(pp_ExeContext)(MC_(allocated_at)(chunks[i]));
         }
   }
   VG_(free)(chunks);
}

void MC_(mempool_alloc)(ThreadId tid, Addr pool, Addr addr, SizeT szB)
{
   MC_Mempool* mp;

   if (VG_(clo_verbosity) > 2) {     
      VG_(message)(Vg_UserMsg, "mempool_alloc(0x%lx, 0x%lx, %ld)\n",
                               pool, addr, szB);
      VG_(get_and_pp_StackTrace) (tid, MEMPOOL_DEBUG_STACKTRACE_DEPTH);
   }

   mp = VG_(HT_lookup) ( MC_(mempool_list), (UWord)pool );
   if (mp == NULL) {
      MC_(record_illegal_mempool_error) ( tid, pool );
   } else {
      if (MP_DETAILED_SANITY_CHECKS) check_mempool_sane(mp);
      MC_(new_block)(tid, addr, szB, 0, mp->is_zeroed,
                     MC_AllocCustom, mp->chunks);
      if (mp->rzB > 0) {
         
         
         
         
         MC_(make_mem_noaccess) ( addr - mp->rzB, mp->rzB);
         MC_(make_mem_noaccess) ( addr + szB, mp->rzB);
      }
      if (MP_DETAILED_SANITY_CHECKS) check_mempool_sane(mp);
   }
}

void MC_(mempool_free)(Addr pool, Addr addr)
{
   MC_Mempool*  mp;
   MC_Chunk*    mc;
   ThreadId     tid = VG_(get_running_tid)();

   mp = VG_(HT_lookup)(MC_(mempool_list), (UWord)pool);
   if (mp == NULL) {
      MC_(record_illegal_mempool_error)(tid, pool);
      return;
   }

   if (VG_(clo_verbosity) > 2) {
      VG_(message)(Vg_UserMsg, "mempool_free(0x%lx, 0x%lx)\n", pool, addr);
      VG_(get_and_pp_StackTrace) (tid, MEMPOOL_DEBUG_STACKTRACE_DEPTH);
   }

   if (MP_DETAILED_SANITY_CHECKS) check_mempool_sane(mp);
   mc = VG_(HT_remove)(mp->chunks, (UWord)addr);
   if (mc == NULL) {
      MC_(record_free_error)(tid, (Addr)addr);
      return;
   }

   if (VG_(clo_verbosity) > 2) {
      VG_(message)(Vg_UserMsg, 
		   "mempool_free(0x%lx, 0x%lx) freed chunk of %ld bytes\n",
		   pool, addr, mc->szB + 0UL);
   }

   die_and_free_mem ( tid, mc, mp->rzB );
   if (MP_DETAILED_SANITY_CHECKS) check_mempool_sane(mp);
}


void MC_(mempool_trim)(Addr pool, Addr addr, SizeT szB)
{
   MC_Mempool*  mp;
   MC_Chunk*    mc;
   ThreadId     tid = VG_(get_running_tid)();
   UInt         n_shadows, i;
   VgHashNode** chunks;

   if (VG_(clo_verbosity) > 2) {
      VG_(message)(Vg_UserMsg, "mempool_trim(0x%lx, 0x%lx, %ld)\n",
                               pool, addr, szB);
      VG_(get_and_pp_StackTrace) (tid, MEMPOOL_DEBUG_STACKTRACE_DEPTH);
   }

   mp = VG_(HT_lookup)(MC_(mempool_list), (UWord)pool);
   if (mp == NULL) {
      MC_(record_illegal_mempool_error)(tid, pool);
      return;
   }

   check_mempool_sane(mp);
   chunks = VG_(HT_to_array) ( mp->chunks, &n_shadows );
   if (n_shadows == 0) {
     tl_assert(chunks == NULL);
     return;
   }

   tl_assert(chunks != NULL);
   for (i = 0; i < n_shadows; ++i) {

      Addr lo, hi, min, max;

      mc = (MC_Chunk*) chunks[i];

      lo = mc->data;
      hi = mc->szB == 0 ? mc->data : mc->data + mc->szB - 1;

#define EXTENT_CONTAINS(x) ((addr <= (x)) && ((x) < addr + szB))

      if (EXTENT_CONTAINS(lo) && EXTENT_CONTAINS(hi)) {


         continue;

      } else if ( (! EXTENT_CONTAINS(lo)) &&
                  (! EXTENT_CONTAINS(hi)) ) {


         if (VG_(HT_remove)(mp->chunks, (UWord)mc->data) == NULL) {
            MC_(record_free_error)(tid, (Addr)mc->data);
            VG_(free)(chunks);
            if (MP_DETAILED_SANITY_CHECKS) check_mempool_sane(mp);
            return;
         }
         die_and_free_mem ( tid, mc, mp->rzB );  

      } else {


         tl_assert(EXTENT_CONTAINS(lo) ||
                   EXTENT_CONTAINS(hi));
         if (VG_(HT_remove)(mp->chunks, (UWord)mc->data) == NULL) {
            MC_(record_free_error)(tid, (Addr)mc->data);
            VG_(free)(chunks);
            if (MP_DETAILED_SANITY_CHECKS) check_mempool_sane(mp);
            return;
         }

         if (mc->data < addr) {
           min = mc->data;
           lo = addr;
         } else {
           min = addr;
           lo = mc->data;
         }

         if (mc->data + szB > addr + szB) {
           max = mc->data + szB;
           hi = addr + szB;
         } else {
           max = addr + szB;
           hi = mc->data + szB;
         }

         tl_assert(min <= lo);
         tl_assert(lo < hi);
         tl_assert(hi <= max);

         if (min < lo && !EXTENT_CONTAINS(min)) {
           MC_(make_mem_noaccess)( min, lo - min);
         }

         if (hi < max && !EXTENT_CONTAINS(max)) {
           MC_(make_mem_noaccess)( hi, max - hi );
         }

         mc->data = lo;
         mc->szB = (UInt) (hi - lo);
         VG_(HT_add_node)( mp->chunks, mc );        
      }

#undef EXTENT_CONTAINS
      
   }
   check_mempool_sane(mp);
   VG_(free)(chunks);
}

void MC_(move_mempool)(Addr poolA, Addr poolB)
{
   MC_Mempool* mp;

   if (VG_(clo_verbosity) > 2) {
      VG_(message)(Vg_UserMsg, "move_mempool(0x%lx, 0x%lx)\n", poolA, poolB);
      VG_(get_and_pp_StackTrace)
         (VG_(get_running_tid)(), MEMPOOL_DEBUG_STACKTRACE_DEPTH);
   }

   mp = VG_(HT_remove) ( MC_(mempool_list), (UWord)poolA );

   if (mp == NULL) {
      ThreadId tid = VG_(get_running_tid)();
      MC_(record_illegal_mempool_error) ( tid, poolA );
      return;
   }

   mp->pool = poolB;
   VG_(HT_add_node)( MC_(mempool_list), mp );
}

void MC_(mempool_change)(Addr pool, Addr addrA, Addr addrB, SizeT szB)
{
   MC_Mempool*  mp;
   MC_Chunk*    mc;
   ThreadId     tid = VG_(get_running_tid)();

   if (VG_(clo_verbosity) > 2) {
      VG_(message)(Vg_UserMsg, "mempool_change(0x%lx, 0x%lx, 0x%lx, %ld)\n",
                   pool, addrA, addrB, szB);
      VG_(get_and_pp_StackTrace) (tid, MEMPOOL_DEBUG_STACKTRACE_DEPTH);
   }

   mp = VG_(HT_lookup)(MC_(mempool_list), (UWord)pool);
   if (mp == NULL) {
      MC_(record_illegal_mempool_error)(tid, pool);
      return;
   }

   check_mempool_sane(mp);

   mc = VG_(HT_remove)(mp->chunks, (UWord)addrA);
   if (mc == NULL) {
      MC_(record_free_error)(tid, (Addr)addrA);
      return;
   }

   mc->data = addrB;
   mc->szB  = szB;
   VG_(HT_add_node)( mp->chunks, mc );

   check_mempool_sane(mp);
}

Bool MC_(mempool_exists)(Addr pool)
{
   MC_Mempool*  mp;

   mp = VG_(HT_lookup)(MC_(mempool_list), (UWord)pool);
   if (mp == NULL) {
       return False;
   }
   return True;
}



void MC_(print_malloc_stats) ( void )
{
   MC_Chunk* mc;
   SizeT     nblocks = 0;
   ULong     nbytes  = 0;
   
   if (VG_(clo_verbosity) == 0)
      return;
   if (VG_(clo_xml))
      return;

   
   VG_(HT_ResetIter)(MC_(malloc_list));
   while ( (mc = VG_(HT_Next)(MC_(malloc_list))) ) {
      nblocks++;
      nbytes += (ULong)mc->szB;
   }

   VG_(umsg)(
      "HEAP SUMMARY:\n"
      "    in use at exit: %'llu bytes in %'lu blocks\n"
      "  total heap usage: %'lu allocs, %'lu frees, %'llu bytes allocated\n"
      "\n",
      nbytes, nblocks,
      cmalloc_n_mallocs,
      cmalloc_n_frees, cmalloc_bs_mallocd
   );
}

SizeT MC_(get_cmalloc_n_frees) ( void )
{
   return cmalloc_n_frees;
}


