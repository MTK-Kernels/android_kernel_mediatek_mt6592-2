

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
#include "pub_tool_vki.h"
#include "pub_tool_aspacehl.h"
#include "pub_tool_aspacemgr.h"
#include "pub_tool_execontext.h"
#include "pub_tool_hashtable.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_libcsignal.h"
#include "pub_tool_machine.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_options.h"
#include "pub_tool_oset.h"
#include "pub_tool_poolalloc.h"     
#include "pub_tool_signals.h"       
#include "pub_tool_libcsetjmp.h"    
#include "pub_tool_tooliface.h"     

#include "mc_include.h"








#define VG_DEBUG_LEAKCHECK 0
#define VG_DEBUG_CLIQUE    0
 


static Int compare_MC_Chunks(const void* n1, const void* n2)
{
   const MC_Chunk* mc1 = *(const MC_Chunk *const *)n1;
   const MC_Chunk* mc2 = *(const MC_Chunk *const *)n2;
   if (mc1->data < mc2->data) return -1;
   if (mc1->data > mc2->data) return  1;
   return 0;
}

#if VG_DEBUG_LEAKCHECK
static 
Int find_chunk_for_OLD ( Addr       ptr, 
                         MC_Chunk** chunks,
                         Int        n_chunks )

{
   Int  i;
   Addr a_lo, a_hi;
   PROF_EVENT(70, "find_chunk_for_OLD");
   for (i = 0; i < n_chunks; i++) {
      PROF_EVENT(71, "find_chunk_for_OLD(loop)");
      a_lo = chunks[i]->data;
      a_hi = ((Addr)chunks[i]->data) + chunks[i]->szB;
      if (a_lo <= ptr && ptr < a_hi)
         return i;
   }
   return -1;
}
#endif

static 
Int find_chunk_for ( Addr       ptr, 
                     MC_Chunk** chunks,
                     Int        n_chunks )
{
   Addr a_mid_lo, a_mid_hi;
   Int lo, mid, hi, retVal;
   
   retVal = -1;
   lo = 0;
   hi = n_chunks-1;
   while (True) {
      
      if (lo > hi) break; 

      mid      = (lo + hi) / 2;
      a_mid_lo = chunks[mid]->data;
      a_mid_hi = chunks[mid]->data + chunks[mid]->szB;
      
      
      
      
      
      
      if (chunks[mid]->szB == 0)
         a_mid_hi++;

      if (ptr < a_mid_lo) {
         hi = mid-1;
         continue;
      } 
      if (ptr >= a_mid_hi) {
         lo = mid+1;
         continue;
      }
      tl_assert(ptr >= a_mid_lo && ptr < a_mid_hi);
      retVal = mid;
      break;
   }

#  if VG_DEBUG_LEAKCHECK
   tl_assert(retVal == find_chunk_for_OLD ( ptr, chunks, n_chunks ));
#  endif
   
   return retVal;
}


static MC_Chunk**
find_active_chunks(Int* pn_chunks)
{
   
   
   
   MC_Mempool *mp;
   MC_Chunk **mallocs, **chunks, *mc;
   UInt n_mallocs, n_chunks, m, s;
   Bool *malloc_chunk_holds_a_pool_chunk;

   
   
   
   mallocs = (MC_Chunk**) VG_(HT_to_array)( MC_(malloc_list), &n_mallocs );
   if (n_mallocs == 0) {
      tl_assert(mallocs == NULL);
      *pn_chunks = 0;
      return NULL;
   }
   VG_(ssort)(mallocs, n_mallocs, sizeof(VgHashNode*), compare_MC_Chunks);

   
   
   malloc_chunk_holds_a_pool_chunk = VG_(calloc)( "mc.fas.1",
                                                  n_mallocs, sizeof(Bool) );
   n_chunks = n_mallocs;

   
   
   
   VG_(HT_ResetIter)(MC_(mempool_list));
   while ( (mp = VG_(HT_Next)(MC_(mempool_list))) ) {
      VG_(HT_ResetIter)(mp->chunks);
      while ( (mc = VG_(HT_Next)(mp->chunks)) ) {

         
         n_chunks++;

         
         m = find_chunk_for(mc->data, mallocs, n_mallocs);
         if (m != -1 && malloc_chunk_holds_a_pool_chunk[m] == False) {
            tl_assert(n_chunks > 0);
            n_chunks--;
            malloc_chunk_holds_a_pool_chunk[m] = True;
         }

         
         if (mc->szB > 1) {
            m = find_chunk_for(mc->data + (mc->szB - 1), mallocs, n_mallocs);
            if (m != -1 && malloc_chunk_holds_a_pool_chunk[m] == False) {
               tl_assert(n_chunks > 0);
               n_chunks--;
               malloc_chunk_holds_a_pool_chunk[m] = True;
            }
         }
      }
   }
   tl_assert(n_chunks > 0);

   
   chunks = VG_(malloc)("mc.fas.2", sizeof(VgHashNode*) * (n_chunks));
   s = 0;

   
   
   VG_(HT_ResetIter)(MC_(mempool_list));
   while ( (mp = VG_(HT_Next)(MC_(mempool_list))) ) {
      VG_(HT_ResetIter)(mp->chunks);
      while ( (mc = VG_(HT_Next)(mp->chunks)) ) {
         tl_assert(s < n_chunks);
         chunks[s++] = mc;
      }
   }
   for (m = 0; m < n_mallocs; ++m) {
      if (!malloc_chunk_holds_a_pool_chunk[m]) {
         tl_assert(s < n_chunks);
         chunks[s++] = mallocs[m];
      }
   }
   tl_assert(s == n_chunks);

   
   VG_(free)(mallocs);
   VG_(free)(malloc_chunk_holds_a_pool_chunk);

   *pn_chunks = n_chunks;

   return chunks;
}


typedef 
   struct {
      UInt  state:2;    
      UInt  pending:1;  
      UInt  heuristic: (sizeof(UInt)*8)-3;
      
      
      
   
      union {
         SizeT indirect_szB;
         
         SizeT  clique;
         
      } IorC;
   } 
   LC_Extra;

static MC_Chunk** lc_chunks;
static Int        lc_n_chunks;
static SizeT lc_chunks_n_frees_marker;
static LC_Extra* lc_extras;

static OSet*        lr_table;
static LossRecord** lr_array;

static UInt detect_memory_leaks_last_heuristics;

LeakCheckDeltaMode MC_(detect_memory_leaks_last_delta_mode);

UInt MC_(leak_search_gen);

static Int* lc_markstack;
static Int  lc_markstack_top;    

static SizeT lc_scanned_szB;
static SizeT lc_sig_skipped_szB;


SizeT MC_(bytes_leaked)     = 0;
SizeT MC_(bytes_indirect)   = 0;
SizeT MC_(bytes_dubious)    = 0;
SizeT MC_(bytes_reachable)  = 0;
SizeT MC_(bytes_suppressed) = 0;

SizeT MC_(blocks_leaked)     = 0;
SizeT MC_(blocks_indirect)   = 0;
SizeT MC_(blocks_dubious)    = 0;
SizeT MC_(blocks_reachable)  = 0;
SizeT MC_(blocks_suppressed) = 0;

static SizeT MC_(bytes_heuristically_reachable)[N_LEAK_CHECK_HEURISTICS]
                                               = {0,0,0,0};
static SizeT MC_(blocks_heuristically_reachable)[N_LEAK_CHECK_HEURISTICS]
                                                = {0,0,0,0};

static Bool
lc_is_a_chunk_ptr(Addr ptr, Int* pch_no, MC_Chunk** pch, LC_Extra** pex)
{
   Int ch_no;
   MC_Chunk* ch;
   LC_Extra* ex;

   
   
   
   
   if (!VG_(am_is_valid_for_client)(ptr, 1, VKI_PROT_READ)) {
      return False;
   } else {
      ch_no = find_chunk_for(ptr, lc_chunks, lc_n_chunks);
      tl_assert(ch_no >= -1 && ch_no < lc_n_chunks);

      if (ch_no == -1) {
         return False;
      } else {
         
         
         ch = lc_chunks[ch_no];
         ex = &(lc_extras[ch_no]);

         tl_assert(ptr >= ch->data);
         tl_assert(ptr < ch->data + ch->szB + (ch->szB==0  ? 1  : 0));

         if (VG_DEBUG_LEAKCHECK)
            VG_(printf)("ptr=%#lx -> block %d\n", ptr, ch_no);

         *pch_no = ch_no;
         *pch    = ch;
         *pex    = ex;

         return True;
      }
   }
}

static void lc_push(Int ch_no, MC_Chunk* ch)
{
   if (!lc_extras[ch_no].pending) {
      if (0) {
         VG_(printf)("pushing %#lx-%#lx\n", ch->data, ch->data + ch->szB);
      }
      lc_markstack_top++;
      tl_assert(lc_markstack_top < lc_n_chunks);
      lc_markstack[lc_markstack_top] = ch_no;
      tl_assert(!lc_extras[ch_no].pending);
      lc_extras[ch_no].pending = True;
   }
}

static Bool lc_pop(Int* ret)
{
   if (-1 == lc_markstack_top) {
      return False;
   } else {
      tl_assert(0 <= lc_markstack_top && lc_markstack_top < lc_n_chunks);
      *ret = lc_markstack[lc_markstack_top];
      lc_markstack_top--;
      tl_assert(lc_extras[*ret].pending);
      lc_extras[*ret].pending = False;
      return True;
   }
}

static const HChar* pp_heuristic(LeakCheckHeuristic h)
{
   switch(h) {
   case LchNone:                return "none";
   case LchStdString:           return "stdstring";
   case LchLength64:            return "length64";
   case LchNewArray:            return "newarray";
   case LchMultipleInheritance: return "multipleinheritance";
   default:                     return "???invalid heuristic???";
   }
}

static Bool aligned_ptr_above_page0_is_vtable_addr(Addr ptr)
{
   
   
   

   
   
   
   
   
   
   
   
#define VTABLE_MAX_CHECK 20 

   NSegment const *seg;
   UInt nr_fn_ptrs = 0;
   Addr scan;
   Addr scan_max;

   
   
   seg = VG_(am_find_nsegment) (ptr);
   if (seg == NULL
       || seg->kind != SkFileC
       || !seg->hasR)
      return False;

   
   scan_max = ptr + VTABLE_MAX_CHECK*sizeof(Addr);
   
   if (scan_max > seg->end - sizeof(Addr))
      scan_max = seg->end - sizeof(Addr);
   for (scan = ptr; scan <= scan_max; scan+=sizeof(Addr)) {
      Addr pot_fn = *((Addr *)scan);
      if (pot_fn == 0)
         continue; 
      seg = VG_(am_find_nsegment) (pot_fn);
#if defined(VGA_ppc64be)
      
      
      if (seg == NULL
          || seg->kind != SkFileC
          || !seg->hasR
          || !seg->hasW)
         return False; 
      pot_fn = *((Addr *)pot_fn);
      if (pot_fn == 0)
         continue; 
      seg = VG_(am_find_nsegment) (pot_fn);
#endif
      if (seg == NULL
          || seg->kind != SkFileC
          || !seg->hasT)
         return False; 
      nr_fn_ptrs++;
      if (nr_fn_ptrs == 2)
         return True;
   }

   return False;
}

static Bool is_valid_aligned_ULong ( Addr a )
{
   if (sizeof(Word) == 8)
      return MC_(is_valid_aligned_word)(a);

   return MC_(is_valid_aligned_word)(a)
      && MC_(is_valid_aligned_word)(a + 4);
}

static LeakCheckHeuristic heuristic_reachedness (Addr ptr,
                                                 MC_Chunk *ch, LC_Extra *ex,
                                                 UInt heur_set)
{
   if (HiS(LchStdString, heur_set)) {
      
      
      
      
      
      
      if ( ptr == ch->data + 3 * sizeof(SizeT)
           && MC_(is_valid_aligned_word)(ch->data + sizeof(SizeT))) {
         const SizeT capacity = *((SizeT*)(ch->data + sizeof(SizeT)));
         if (3 * sizeof(SizeT) + capacity + 1 == ch->szB
            && MC_(is_valid_aligned_word)(ch->data)) {
            const SizeT length = *((SizeT*)ch->data);
            if (length <= capacity) {
               
               
               
               
               
               
               
               return LchStdString;
            }
         }
      }
   }

   if (HiS(LchLength64, heur_set)) {
      
      
      
      
      
      
      
      if ( ptr == ch->data + sizeof(ULong)
          && is_valid_aligned_ULong(ch->data)) {
         const ULong size = *((ULong*)ch->data);
         if (size > 0 && (ch->szB - sizeof(ULong)) == size) {
            return LchLength64;
         }
      }
   }

   if (HiS(LchNewArray, heur_set)) {
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      if ( ptr == ch->data + sizeof(SizeT)
           && MC_(is_valid_aligned_word)(ch->data)) {
         const SizeT nr_elts = *((SizeT*)ch->data);
         if (nr_elts > 0 && (ch->szB - sizeof(SizeT)) % nr_elts == 0) {
            
            return LchNewArray;
         }
      }
   }

   if (HiS(LchMultipleInheritance, heur_set)) {
      
      
      if (VG_IS_WORD_ALIGNED(ptr)
          && MC_(is_valid_aligned_word)(ptr)) {
         Addr first_addr;
         Addr inner_addr;

         
         
         
         
         
         
         
         
         inner_addr = *((Addr*)ptr);
         if (VG_IS_WORD_ALIGNED(inner_addr) 
             && inner_addr >= (Addr)VKI_PAGE_SIZE
             && MC_(is_valid_aligned_word)(ch->data)) {
            first_addr = *((Addr*)ch->data);
            if (VG_IS_WORD_ALIGNED(first_addr)
                && first_addr >= (Addr)VKI_PAGE_SIZE
                && aligned_ptr_above_page0_is_vtable_addr(inner_addr)
                && aligned_ptr_above_page0_is_vtable_addr(first_addr)) {
               
               return LchMultipleInheritance;
            }
         }
      }
   }

   return LchNone;
}


static void
lc_push_without_clique_if_a_chunk_ptr(Addr ptr, Bool is_prior_definite)
{
   Int ch_no;
   MC_Chunk* ch;
   LC_Extra* ex;
   Reachedness ch_via_ptr; 

   if ( ! lc_is_a_chunk_ptr(ptr, &ch_no, &ch, &ex) )
      return;

   if (ex->state == Reachable) {
      if (ex->heuristic && ptr == ch->data)
         
         
         ex->heuristic = LchNone;
      return;
   }
   
   
   
   
   

   if (ptr == ch->data)
      ch_via_ptr = Reachable;
   else if (detect_memory_leaks_last_heuristics) {
      ex->heuristic 
         = heuristic_reachedness (ptr, ch, ex,
                                  detect_memory_leaks_last_heuristics);
      if (ex->heuristic)
         ch_via_ptr = Reachable;
      else
         ch_via_ptr = Possible;
   } else
      ch_via_ptr = Possible;
         
   if (ch_via_ptr == Reachable && is_prior_definite) {
      
      
      
      ex->state = Reachable;

      
      
      lc_push(ch_no, ch);

   } else if (ex->state == Unreached) {
      
      
      ex->state = Possible;

      
      
      lc_push(ch_no, ch);
   }
}

static void
lc_push_if_a_chunk_ptr_register(ThreadId tid, const HChar* regname, Addr ptr)
{
   lc_push_without_clique_if_a_chunk_ptr(ptr, True);
}

static void
lc_push_with_clique_if_a_chunk_ptr(Addr ptr, Int clique, Int cur_clique)
{
   Int ch_no;
   MC_Chunk* ch;
   LC_Extra* ex;

   tl_assert(0 <= clique && clique < lc_n_chunks);

   if ( ! lc_is_a_chunk_ptr(ptr, &ch_no, &ch, &ex) )
      return;

   
   
   
   if (ex->state == Unreached && ch_no != clique) {
      
      
      
      lc_push(ch_no, ch);

      
      
      
      
      if (VG_DEBUG_CLIQUE) {
         if (ex->IorC.indirect_szB > 0)
            VG_(printf)("  clique %d joining clique %d adding %lu+%lu\n", 
                        ch_no, clique, (unsigned long)ch->szB,
			(unsigned long)ex->IorC.indirect_szB);
         else
            VG_(printf)("  block %d joining clique %d adding %lu\n", 
                        ch_no, clique, (unsigned long)ch->szB);
      }

      lc_extras[clique].IorC.indirect_szB += ch->szB;
      lc_extras[clique].IorC.indirect_szB += ex->IorC.indirect_szB;
      ex->state = IndirectLeak;
      ex->IorC.clique = (SizeT) cur_clique;
   }
}

static void
lc_push_if_a_chunk_ptr(Addr ptr,
                       Int clique, Int cur_clique, Bool is_prior_definite)
{
   if (-1 == clique) 
      lc_push_without_clique_if_a_chunk_ptr(ptr, is_prior_definite);
   else
      lc_push_with_clique_if_a_chunk_ptr(ptr, clique, cur_clique);
}


static VG_MINIMAL_JMP_BUF(memscan_jmpbuf);
static volatile Addr bad_scanned_addr;

static
void scan_all_valid_memory_catcher ( Int sigNo, Addr addr )
{
   if (0)
      VG_(printf)("OUCH! sig=%d addr=%#lx\n", sigNo, addr);
   if (sigNo == VKI_SIGSEGV || sigNo == VKI_SIGBUS) {
      bad_scanned_addr = addr;
      VG_MINIMAL_LONGJMP(memscan_jmpbuf);
   }
}

static void
lc_scan_memory(Addr start, SizeT len, Bool is_prior_definite,
               Int clique, Int cur_clique,
               Addr searched, SizeT szB)
{
#if defined(VGA_s390x)
   
   
   volatile
#endif
   Addr ptr = VG_ROUNDUP(start, sizeof(Addr));
   const Addr end = VG_ROUNDDN(start+len, sizeof(Addr));
   vki_sigset_t sigmask;

   if (VG_DEBUG_LEAKCHECK)
      VG_(printf)("scan %#lx-%#lx (%lu)\n", start, end, len);

   VG_(sigprocmask)(VKI_SIG_SETMASK, NULL, &sigmask);
   VG_(set_fault_catcher)(scan_all_valid_memory_catcher);

   if ( ! MC_(is_within_valid_secondary)(ptr) ) {
      
      ptr = VG_ROUNDUP(ptr+1, SM_SIZE);
   } else if (!VG_(am_is_valid_for_client)(ptr, sizeof(Addr), VKI_PROT_READ)) {
      
      
      
      
      ptr = VG_PGROUNDUP(ptr+1);        
   }

   
   
   
   
   
   
   
   
   if (VG_MINIMAL_SETJMP(memscan_jmpbuf) != 0) {
      
      
      
      VG_(sigprocmask)(VKI_SIG_SETMASK, &sigmask, NULL);
#     if defined(VGA_s390x)
      
      
      
      
      
      
      lc_sig_skipped_szB += VKI_PAGE_SIZE;
      ptr = ptr + VKI_PAGE_SIZE; 
#     else
      
      lc_sig_skipped_szB += sizeof(Addr);
      tl_assert(bad_scanned_addr >= VG_ROUNDUP(start, sizeof(Addr)));
      tl_assert(bad_scanned_addr < VG_ROUNDDN(start+len, sizeof(Addr)));
      ptr = bad_scanned_addr + sizeof(Addr); 
#endif
   }
   while (ptr < end) {
      Addr addr;

      
      if (UNLIKELY((ptr % SM_SIZE) == 0)) {
         if (! MC_(is_within_valid_secondary)(ptr) ) {
            ptr = VG_ROUNDUP(ptr+1, SM_SIZE);
            continue;
         }
      }

      
      if (UNLIKELY((ptr % VKI_PAGE_SIZE) == 0)) {
         if (!VG_(am_is_valid_for_client)(ptr, sizeof(Addr), VKI_PROT_READ)) {
            ptr += VKI_PAGE_SIZE;      
            continue;
         }
      }

      if ( MC_(is_valid_aligned_word)(ptr) ) {
         lc_scanned_szB += sizeof(Addr);
         
         addr = *(Addr *)ptr;
         
         
         if (UNLIKELY(searched)) {
            if (addr >= searched && addr < searched + szB) {
               if (addr == searched) {
                  VG_(umsg)("*%#lx points at %#lx\n", ptr, searched);
                  MC_(pp_describe_addr) (ptr);
               } else {
                  Int ch_no;
                  MC_Chunk *ch;
                  LC_Extra *ex;
                  VG_(umsg)("*%#lx interior points at %lu bytes inside %#lx\n",
                            ptr, (long unsigned) addr - searched, searched);
                  MC_(pp_describe_addr) (ptr);
                  if (lc_is_a_chunk_ptr(addr, &ch_no, &ch, &ex) ) {
                     Int h;
                     for (h = LchStdString; h < N_LEAK_CHECK_HEURISTICS; h++) {
                        if (heuristic_reachedness(addr, ch, ex, H2S(h)) == h) {
                           VG_(umsg)("block at %#lx considered reachable "
                                     "by ptr %#lx using %s heuristic\n",
                                     ch->data, addr, pp_heuristic(h));
                        }
                     }
                     
                     
                     
                     
                     tl_assert (h == N_LEAK_CHECK_HEURISTICS);
                  }
               }
            }
         } else {
            lc_push_if_a_chunk_ptr(addr, clique, cur_clique, is_prior_definite);
         }
      } else if (0 && VG_DEBUG_LEAKCHECK) {
         VG_(printf)("%#lx not valid\n", ptr);
      }
      ptr += sizeof(Addr);
   }

   VG_(sigprocmask)(VKI_SIG_SETMASK, &sigmask, NULL);
   VG_(set_fault_catcher)(NULL);
}


static void lc_process_markstack(Int clique)
{
   Int  top = -1;    
   Bool is_prior_definite;

   while (lc_pop(&top)) {
      tl_assert(top >= 0 && top < lc_n_chunks);

      
      is_prior_definite = ( Possible != lc_extras[top].state );

      lc_scan_memory(lc_chunks[top]->data, lc_chunks[top]->szB,
                     is_prior_definite, clique, (clique == -1 ? -1 : top),
                      0, 0);
   }
}

static Word cmp_LossRecordKey_LossRecord(const void* key, const void* elem)
{
   const LossRecordKey* a = key;
   const LossRecordKey* b = &(((const LossRecord*)elem)->key);

   
   if (a->state < b->state) return -1;
   if (a->state > b->state) return  1;
   
   if (VG_(eq_ExeContext)(
            MC_(clo_leak_resolution), a->allocated_at, b->allocated_at))
      return 0;
   
   if (a->allocated_at < b->allocated_at) return -1;
   if (a->allocated_at > b->allocated_at) return  1;
   VG_(tool_panic)("bad LossRecord comparison");
}

static Int cmp_LossRecords(const void* va, const void* vb)
{
   const LossRecord* lr_a = *(const LossRecord *const *)va;
   const LossRecord* lr_b = *(const LossRecord *const *)vb;
   SizeT total_szB_a = lr_a->szB + lr_a->indirect_szB;
   SizeT total_szB_b = lr_b->szB + lr_b->indirect_szB;

   
   if (total_szB_a < total_szB_b) return -1;
   if (total_szB_a > total_szB_b) return  1;
   
   if (lr_a->key.state < lr_b->key.state) return -1;
   if (lr_a->key.state > lr_b->key.state) return  1;
   
   
   
   if (lr_a->num_blocks < lr_b->num_blocks) return -1;
   if (lr_a->num_blocks > lr_b->num_blocks) return  1;
   
   
   if (lr_a->key.allocated_at < lr_b->key.allocated_at) return -1;
   if (lr_a->key.allocated_at > lr_b->key.allocated_at) return  1;
   return 0;
}

static Int get_lr_array_from_lr_table(void) {
   Int          i, n_lossrecords;
   LossRecord*  lr;

   n_lossrecords = VG_(OSetGen_Size)(lr_table);

   
   
   if (lr_array != NULL)
      VG_(free)(lr_array);
   lr_array = VG_(malloc)("mc.pr.2", n_lossrecords * sizeof(LossRecord*));
   i = 0;
   VG_(OSetGen_ResetIter)(lr_table);
   while ( (lr = VG_(OSetGen_Next)(lr_table)) ) {
      lr_array[i++] = lr;
   }
   tl_assert(i == n_lossrecords);
   return n_lossrecords;
}


static void get_printing_rules(LeakCheckParams* lcp,
                               LossRecord*  lr,
                               Bool* count_as_error,
                               Bool* print_record)
{
   
   
   
   
   

   Bool delta_considered;

   switch (lcp->deltamode) {
   case LCD_Any: 
      delta_considered = lr->num_blocks > 0;
      break;
   case LCD_Increased:
      delta_considered 
         = lr->szB > lr->old_szB
         || lr->indirect_szB > lr->old_indirect_szB
         || lr->num_blocks > lr->old_num_blocks;
      break;
   case LCD_Changed: 
      delta_considered = lr->szB != lr->old_szB
         || lr->indirect_szB != lr->old_indirect_szB
         || lr->num_blocks != lr->old_num_blocks;
      break;
   default:
      tl_assert(0);
   }

   *print_record = lcp->mode == LC_Full && delta_considered 
      && RiS(lr->key.state,lcp->show_leak_kinds);
   
   
   
   
   *count_as_error = lcp->mode == LC_Full && delta_considered 
      && RiS(lr->key.state,lcp->errors_for_leak_kinds);
}

static void print_results(ThreadId tid, LeakCheckParams* lcp)
{
   Int          i, n_lossrecords, start_lr_output_scan;
   LossRecord*  lr;
   Bool         is_suppressed;
   
   SizeT        old_bytes_leaked      = MC_(bytes_leaked);
   SizeT        old_bytes_indirect    = MC_(bytes_indirect); 
   SizeT        old_bytes_dubious     = MC_(bytes_dubious); 
   SizeT        old_bytes_reachable   = MC_(bytes_reachable); 
   SizeT        old_bytes_suppressed  = MC_(bytes_suppressed); 
   SizeT        old_blocks_leaked     = MC_(blocks_leaked);
   SizeT        old_blocks_indirect   = MC_(blocks_indirect);
   SizeT        old_blocks_dubious    = MC_(blocks_dubious);
   SizeT        old_blocks_reachable  = MC_(blocks_reachable);
   SizeT        old_blocks_suppressed = MC_(blocks_suppressed);

   SizeT old_bytes_heuristically_reachable[N_LEAK_CHECK_HEURISTICS];
   SizeT old_blocks_heuristically_reachable[N_LEAK_CHECK_HEURISTICS];

   for (i = 0; i < N_LEAK_CHECK_HEURISTICS; i++) {
      old_bytes_heuristically_reachable[i]   
         =  MC_(bytes_heuristically_reachable)[i];
      MC_(bytes_heuristically_reachable)[i] = 0;
      old_blocks_heuristically_reachable[i]  
         =  MC_(blocks_heuristically_reachable)[i];
      MC_(blocks_heuristically_reachable)[i] = 0;
   }

   if (lr_table == NULL)
      
      
      
      
      
      lr_table =
         VG_(OSetGen_Create)(offsetof(LossRecord, key),
                             cmp_LossRecordKey_LossRecord,
                             VG_(malloc), "mc.pr.1",
                             VG_(free));

   
   
   n_lossrecords = get_lr_array_from_lr_table();
   for (i = 0; i < n_lossrecords; i++) {
      if (lr_array[i]->num_blocks == 0) {
         
         VG_(OSetGen_Remove) (lr_table, &lr_array[i]->key);
         VG_(OSetGen_FreeNode)(lr_table, lr_array[i]);
      } else {
         
         
         lr_array[i]->old_szB          = lr_array[i]->szB;
         lr_array[i]->old_indirect_szB = lr_array[i]->indirect_szB;
         lr_array[i]->old_num_blocks   = lr_array[i]->num_blocks;
         lr_array[i]->szB              = 0;
         lr_array[i]->indirect_szB     = 0;
         lr_array[i]->num_blocks       = 0;
      }
   }
   
   
   VG_(free) (lr_array);
   lr_array = NULL;

   
   for (i = 0; i < lc_n_chunks; i++) {
      MC_Chunk*     ch = lc_chunks[i];
      LC_Extra*     ex = &(lc_extras)[i];
      LossRecord*   old_lr;
      LossRecordKey lrkey;
      lrkey.state        = ex->state;
      lrkey.allocated_at = MC_(allocated_at)(ch);

     if (ex->heuristic) {
        MC_(bytes_heuristically_reachable)[ex->heuristic] += ch->szB;
        MC_(blocks_heuristically_reachable)[ex->heuristic]++;
        if (VG_DEBUG_LEAKCHECK)
           VG_(printf)("heuristic %s %#lx len %lu\n",
                       pp_heuristic(ex->heuristic),
                       ch->data, (unsigned long)ch->szB);
     }

      old_lr = VG_(OSetGen_Lookup)(lr_table, &lrkey);
      if (old_lr) {
         
         
         
         old_lr->szB          += ch->szB;
         if (ex->state == Unreached)
            old_lr->indirect_szB += ex->IorC.indirect_szB;
         old_lr->num_blocks++;
      } else {
         
         
         lr = VG_(OSetGen_AllocNode)(lr_table, sizeof(LossRecord));
         lr->key              = lrkey;
         lr->szB              = ch->szB;
         if (ex->state == Unreached)
            lr->indirect_szB     = ex->IorC.indirect_szB;
         else
            lr->indirect_szB     = 0;
         lr->num_blocks       = 1;
         lr->old_szB          = 0;
         lr->old_indirect_szB = 0;
         lr->old_num_blocks   = 0;
         VG_(OSetGen_Insert)(lr_table, lr);
      }
   }

   
   n_lossrecords = get_lr_array_from_lr_table ();
   tl_assert(VG_(OSetGen_Size)(lr_table) == n_lossrecords);

   
   VG_(ssort)(lr_array, n_lossrecords, sizeof(LossRecord*),
              cmp_LossRecords);

   
   MC_(blocks_leaked)     = MC_(bytes_leaked)     = 0;
   MC_(blocks_indirect)   = MC_(bytes_indirect)   = 0;
   MC_(blocks_dubious)    = MC_(bytes_dubious)    = 0;
   MC_(blocks_reachable)  = MC_(bytes_reachable)  = 0;
   MC_(blocks_suppressed) = MC_(bytes_suppressed) = 0;

   
   
   
   
   
   start_lr_output_scan = 0;
   if (lcp->mode == LC_Full && lcp->max_loss_records_output < n_lossrecords) {
      Int nr_printable_records = 0;
      for (i = n_lossrecords - 1; i >= 0 && start_lr_output_scan == 0; i--) {
         Bool count_as_error, print_record;
         lr = lr_array[i];
         get_printing_rules (lcp, lr, &count_as_error, &print_record);
         
         
         is_suppressed = 
            MC_(record_leak_error) ( tid, i+1, n_lossrecords, lr, 
                                     False ,
                                     False );
         if (print_record && !is_suppressed) {
            nr_printable_records++;
            if (nr_printable_records == lcp->max_loss_records_output)
               start_lr_output_scan = i;
         }
      }
   }

   
   for (i = start_lr_output_scan; i < n_lossrecords; i++) {
      Bool count_as_error, print_record;
      lr = lr_array[i];
      get_printing_rules(lcp, lr, &count_as_error, &print_record);
      is_suppressed = 
         MC_(record_leak_error) ( tid, i+1, n_lossrecords, lr, print_record,
                                  count_as_error );

      if (is_suppressed) {
         MC_(blocks_suppressed) += lr->num_blocks;
         MC_(bytes_suppressed)  += lr->szB;

      } else if (Unreached == lr->key.state) {
         MC_(blocks_leaked)     += lr->num_blocks;
         MC_(bytes_leaked)      += lr->szB;

      } else if (IndirectLeak == lr->key.state) {
         MC_(blocks_indirect)   += lr->num_blocks;
         MC_(bytes_indirect)    += lr->szB;

      } else if (Possible == lr->key.state) {
         MC_(blocks_dubious)    += lr->num_blocks;
         MC_(bytes_dubious)     += lr->szB;

      } else if (Reachable == lr->key.state) {
         MC_(blocks_reachable)  += lr->num_blocks;
         MC_(bytes_reachable)   += lr->szB;

      } else {
         VG_(tool_panic)("unknown loss mode");
      }
   }

   if (VG_(clo_verbosity) > 0 && !VG_(clo_xml)) {
      HChar d_bytes[31];
      HChar d_blocks[31];
#     define DBY(new,old) \
      MC_(snprintf_delta) (d_bytes, sizeof(d_bytes), (new), (old), \
                           lcp->deltamode)
#     define DBL(new,old) \
      MC_(snprintf_delta) (d_blocks, sizeof(d_blocks), (new), (old), \
                           lcp->deltamode)

      VG_(umsg)("LEAK SUMMARY:\n");
      VG_(umsg)("   definitely lost: %'lu%s bytes in %'lu%s blocks\n",
                MC_(bytes_leaked), 
                DBY (MC_(bytes_leaked), old_bytes_leaked),
                MC_(blocks_leaked),
                DBL (MC_(blocks_leaked), old_blocks_leaked));
      VG_(umsg)("   indirectly lost: %'lu%s bytes in %'lu%s blocks\n",
                MC_(bytes_indirect), 
                DBY (MC_(bytes_indirect), old_bytes_indirect),
                MC_(blocks_indirect),
                DBL (MC_(blocks_indirect), old_blocks_indirect));
      VG_(umsg)("     possibly lost: %'lu%s bytes in %'lu%s blocks\n",
                MC_(bytes_dubious), 
                DBY (MC_(bytes_dubious), old_bytes_dubious), 
                MC_(blocks_dubious),
                DBL (MC_(blocks_dubious), old_blocks_dubious));
      VG_(umsg)("   still reachable: %'lu%s bytes in %'lu%s blocks\n",
                MC_(bytes_reachable), 
                DBY (MC_(bytes_reachable), old_bytes_reachable), 
                MC_(blocks_reachable),
                DBL (MC_(blocks_reachable), old_blocks_reachable));
      for (i = 0; i < N_LEAK_CHECK_HEURISTICS; i++)
         if (old_blocks_heuristically_reachable[i] > 0 
             || MC_(blocks_heuristically_reachable)[i] > 0) {
            VG_(umsg)("                      of which "
                      "reachable via heuristic:\n");
            break;
         }
      for (i = 0; i < N_LEAK_CHECK_HEURISTICS; i++)
         if (old_blocks_heuristically_reachable[i] > 0 
             || MC_(blocks_heuristically_reachable)[i] > 0)
            VG_(umsg)("                        %-19s: "
                      "%'lu%s bytes in %'lu%s blocks\n",
                      pp_heuristic(i),
                      MC_(bytes_heuristically_reachable)[i], 
                      DBY (MC_(bytes_heuristically_reachable)[i],
                           old_bytes_heuristically_reachable[i]), 
                      MC_(blocks_heuristically_reachable)[i],
                      DBL (MC_(blocks_heuristically_reachable)[i],
                           old_blocks_heuristically_reachable[i]));
      VG_(umsg)("        suppressed: %'lu%s bytes in %'lu%s blocks\n",
                MC_(bytes_suppressed), 
                DBY (MC_(bytes_suppressed), old_bytes_suppressed), 
                MC_(blocks_suppressed),
                DBL (MC_(blocks_suppressed), old_blocks_suppressed));
      if (lcp->mode != LC_Full &&
          (MC_(blocks_leaked) + MC_(blocks_indirect) +
           MC_(blocks_dubious) + MC_(blocks_reachable)) > 0) {
         if (lcp->requested_by_monitor_command)
            VG_(umsg)("To see details of leaked memory, "
                      "give 'full' arg to leak_check\n");
         else
            VG_(umsg)("Rerun with --leak-check=full to see details "
                      "of leaked memory\n");
      }
      if (lcp->mode == LC_Full &&
          MC_(blocks_reachable) > 0 && !RiS(Reachable,lcp->show_leak_kinds)) {
         VG_(umsg)("Reachable blocks (those to which a pointer "
                   "was found) are not shown.\n");
         if (lcp->requested_by_monitor_command)
            VG_(umsg)("To see them, add 'reachable any' args to leak_check\n");
         else
            VG_(umsg)("To see them, rerun with: --leak-check=full "
                      "--show-leak-kinds=all\n");
      }
      VG_(umsg)("\n");
      #undef DBL
      #undef DBY
   }
}

static void print_clique (Int clique, UInt level)
{
   Int ind;
   Int i,  n_lossrecords;;

   n_lossrecords = VG_(OSetGen_Size)(lr_table);

   for (ind = 0; ind < lc_n_chunks; ind++) {
      LC_Extra*     ind_ex = &(lc_extras)[ind];
      if (ind_ex->state == IndirectLeak 
          && ind_ex->IorC.clique == (SizeT) clique) {
         MC_Chunk*    ind_ch = lc_chunks[ind];
         LossRecord*  ind_lr;
         LossRecordKey ind_lrkey;
         Int lr_i;
         ind_lrkey.state = ind_ex->state;
         ind_lrkey.allocated_at = MC_(allocated_at)(ind_ch);
         ind_lr = VG_(OSetGen_Lookup)(lr_table, &ind_lrkey);
         for (lr_i = 0; lr_i < n_lossrecords; lr_i++)
            if (ind_lr == lr_array[lr_i])
               break;
         for (i = 0; i < level; i++)
            VG_(umsg)("  ");
         VG_(umsg)("%p[%lu] indirect loss record %d\n",
                   (void *)ind_ch->data, (unsigned long)ind_ch->szB,
                   lr_i+1); 
         if (lr_i >= n_lossrecords)
            VG_(umsg)
               ("error: no indirect loss record found for %p[%lu]?????\n",
                (void *)ind_ch->data, (unsigned long)ind_ch->szB);
         print_clique(ind, level+1);
      }
   }
 }

Bool MC_(print_block_list) ( UInt loss_record_nr)
{
   Int          i,  n_lossrecords;
   LossRecord*  lr;

   if (lr_table == NULL || lc_chunks == NULL || lc_extras == NULL) {
      VG_(umsg)("Can't print block list : no valid leak search result\n");
      return False;
   }

   if (lc_chunks_n_frees_marker != MC_(get_cmalloc_n_frees)()) {
      VG_(umsg)("Can't print obsolete block list : redo a leak search first\n");
      return False;
   }

   n_lossrecords = VG_(OSetGen_Size)(lr_table);
   if (loss_record_nr >= n_lossrecords)
      return False; 

   tl_assert (lr_array);
   lr = lr_array[loss_record_nr];
   
   
   
   MC_(pp_LossRecord)(loss_record_nr+1, n_lossrecords, lr);

   
   for (i = 0; i < lc_n_chunks; i++) {
      MC_Chunk*     ch = lc_chunks[i];
      LC_Extra*     ex = &(lc_extras)[i];
      LossRecord*   old_lr;
      LossRecordKey lrkey;
      lrkey.state        = ex->state;
      lrkey.allocated_at = MC_(allocated_at)(ch);

      old_lr = VG_(OSetGen_Lookup)(lr_table, &lrkey);
      if (old_lr) {
         
         
         if (old_lr == lr_array[loss_record_nr]) {
            VG_(umsg)("%p[%lu]\n",
                      (void *)ch->data, (unsigned long) ch->szB);
            if (ex->state != Reachable) {
               
               
               
               
               
               
               print_clique(i, 1);
            }
         }
      } else {
         
         VG_(umsg)("error: no loss record found for %p[%lu]?????\n",
                   (void *)ch->data, (unsigned long) ch->szB);
      }
   }
   return True;
}

static void scan_memory_root_set(Addr searched, SizeT szB)
{
   Int   i;
   Int   n_seg_starts;
   Addr* seg_starts = VG_(get_segment_starts)( SkFileC | SkAnonC | SkShmC,
                                               &n_seg_starts );

   tl_assert(seg_starts && n_seg_starts > 0);

   lc_scanned_szB = 0;
   lc_sig_skipped_szB = 0;

   
   for (i = 0; i < n_seg_starts; i++) {
      SizeT seg_size;
      NSegment const* seg = VG_(am_find_nsegment)( seg_starts[i] );
      tl_assert(seg);
      tl_assert(seg->kind == SkFileC || seg->kind == SkAnonC ||
                seg->kind == SkShmC);

      if (!(seg->hasR && seg->hasW))                    continue;
      if (seg->isCH)                                    continue;

      
      
      
      if (seg->kind == SkFileC 
          && (VKI_S_ISCHR(seg->mode) || VKI_S_ISBLK(seg->mode))) {
         const HChar* dev_name = VG_(am_get_filename)( seg );
         if (dev_name && 0 == VG_(strcmp)(dev_name, "/dev/zero")) {
            
         } else {
            
            continue;
         }
      }

      if (0)
         VG_(printf)("ACCEPT %2d  %#lx %#lx\n", i, seg->start, seg->end);

      
      
      seg_size = seg->end - seg->start + 1;
      if (VG_(clo_verbosity) > 2) {
         VG_(message)(Vg_DebugMsg,
                      "  Scanning root segment: %#lx..%#lx (%lu)\n",
                      seg->start, seg->end, seg_size);
      }
      lc_scan_memory(seg->start, seg_size, True,
                     -1, -1,
                     searched, szB);
   }
   VG_(free)(seg_starts);
}


void MC_(detect_memory_leaks) ( ThreadId tid, LeakCheckParams* lcp)
{
   Int i, j;
   
   tl_assert(lcp->mode != LC_Off);

   
   tl_assert((VKI_PAGE_SIZE % sizeof(Addr)) == 0);
   tl_assert((SM_SIZE % sizeof(Addr)) == 0);
   
   
   
   
   tl_assert((SM_SIZE % VKI_PAGE_SIZE) == 0);

   MC_(leak_search_gen)++;
   MC_(detect_memory_leaks_last_delta_mode) = lcp->deltamode;
   detect_memory_leaks_last_heuristics = lcp->heuristics;

   
   if (lc_chunks) {
      VG_(free)(lc_chunks);
      lc_chunks = NULL;
   }
   lc_chunks = find_active_chunks(&lc_n_chunks);
   lc_chunks_n_frees_marker = MC_(get_cmalloc_n_frees)();
   if (lc_n_chunks == 0) {
      tl_assert(lc_chunks == NULL);
      if (lr_table != NULL) {
         
         
         
         
         
         VG_(OSetGen_Destroy) (lr_table);
         lr_table = NULL;
      }
      if (VG_(clo_verbosity) >= 1 && !VG_(clo_xml)) {
         VG_(umsg)("All heap blocks were freed -- no leaks are possible\n");
         VG_(umsg)("\n");
      }
      return;
   }

   
   VG_(ssort)(lc_chunks, lc_n_chunks, sizeof(VgHashNode*), compare_MC_Chunks);

   
   for (i = 0; i < lc_n_chunks-1; i++) {
      tl_assert( lc_chunks[i]->data <= lc_chunks[i+1]->data);
   }

   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   for (i = 0; i < lc_n_chunks-1; i++) {
      MC_Chunk* ch1 = lc_chunks[i];
      MC_Chunk* ch2 = lc_chunks[i+1];

      Addr start1    = ch1->data;
      Addr start2    = ch2->data;
      Addr end1      = ch1->data + ch1->szB - 1;
      Addr end2      = ch2->data + ch2->szB - 1;
      Bool isCustom1 = ch1->allockind == MC_AllocCustom;
      Bool isCustom2 = ch2->allockind == MC_AllocCustom;

      if (end1 < start2) {
         

      
      
         

      } else if (start1 >= start2 && end1 <= end2 && isCustom1 && !isCustom2) {
         
         
         for (j = i+1; j < lc_n_chunks-1; j++) {
            lc_chunks[j] = lc_chunks[j+1];
         }
         lc_n_chunks--;

      } else if (start2 >= start1 && end2 <= end1 && isCustom2 && !isCustom1) {
         
         
         for (j = i; j < lc_n_chunks-1; j++) {
            lc_chunks[j] = lc_chunks[j+1];
         }
         lc_n_chunks--;

      } else {
         VG_(umsg)("Block 0x%lx..0x%lx overlaps with block 0x%lx..0x%lx\n",
                   start1, end1, start2, end2);
         VG_(umsg)("Blocks allocation contexts:\n"),
         VG_(pp_ExeContext)( MC_(allocated_at)(ch1));
         VG_(umsg)("\n"),
         VG_(pp_ExeContext)(  MC_(allocated_at)(ch2));
         VG_(umsg)("This is usually caused by using VALGRIND_MALLOCLIKE_BLOCK");
         VG_(umsg)("in an inappropriate way.\n");
         tl_assert (0);
      }
   }

   
   if (lc_extras) {
      VG_(free)(lc_extras);
      lc_extras = NULL;
   }
   lc_extras = VG_(malloc)( "mc.dml.2", lc_n_chunks * sizeof(LC_Extra) );
   for (i = 0; i < lc_n_chunks; i++) {
      lc_extras[i].state        = Unreached;
      lc_extras[i].pending      = False;
      lc_extras[i].heuristic = LchNone;
      lc_extras[i].IorC.indirect_szB = 0;
   }

   
   lc_markstack = VG_(malloc)( "mc.dml.2", lc_n_chunks * sizeof(Int) );
   for (i = 0; i < lc_n_chunks; i++) {
      lc_markstack[i] = -1;
   }
   lc_markstack_top = -1;

   
   if (VG_(clo_verbosity) > 1 && !VG_(clo_xml)) {
      VG_(umsg)( "Searching for pointers to %'d not-freed blocks\n",
                 lc_n_chunks );
   }

   
   
   scan_memory_root_set(0, 0);

   
   VG_(apply_to_GP_regs)(lc_push_if_a_chunk_ptr_register);

   
   
   lc_process_markstack(-1);

   if (VG_(clo_verbosity) > 1 && !VG_(clo_xml)) {
      VG_(umsg)("Checked %'lu bytes\n", lc_scanned_szB);
      if (lc_sig_skipped_szB > 0)
         VG_(umsg)("Skipped %'lu bytes due to read errors\n",
                   lc_sig_skipped_szB);
      VG_(umsg)( "\n" );
   }

   
   
   
   
   
   
   
   for (i = 0; i < lc_n_chunks; i++) {
      MC_Chunk* ch = lc_chunks[i];
      LC_Extra* ex = &(lc_extras[i]);

      if (VG_DEBUG_CLIQUE)
         VG_(printf)("cliques: %d at %#lx -> Loss state %d\n",
                     i, ch->data, ex->state);

      tl_assert(lc_markstack_top == -1);

      if (ex->state == Unreached) {
         if (VG_DEBUG_CLIQUE)
            VG_(printf)("%d: gathering clique %#lx\n", i, ch->data);
         
         
         lc_push(i, ch);
         lc_process_markstack(i);

         tl_assert(lc_markstack_top == -1);
         tl_assert(ex->state == Unreached);
      }
   }
      
   print_results( tid, lcp);

   VG_(free) ( lc_markstack );
   lc_markstack = NULL;
   
   
   
}

static Addr searched_wpa;
static SizeT searched_szB;
static void
search_address_in_GP_reg(ThreadId tid, const HChar* regname, Addr addr_in_reg)
{
   if (addr_in_reg >= searched_wpa 
       && addr_in_reg < searched_wpa + searched_szB) {
      if (addr_in_reg == searched_wpa)
         VG_(umsg)
            ("tid %d register %s pointing at %#lx\n",
             tid, regname, searched_wpa);  
      else
         VG_(umsg)
            ("tid %d register %s interior pointing %lu bytes inside %#lx\n",
             tid, regname, (long unsigned) addr_in_reg - searched_wpa,
             searched_wpa);
   }
}

void MC_(who_points_at) ( Addr address, SizeT szB)
{
   MC_Chunk** chunks;
   Int        n_chunks;
   Int        i;

   if (szB == 1)
      VG_(umsg) ("Searching for pointers to %#lx\n", address);
   else
      VG_(umsg) ("Searching for pointers pointing in %lu bytes from %#lx\n",
                 szB, address);

   chunks = find_active_chunks(&n_chunks);

   
   scan_memory_root_set(address, szB);

   
   for (i = 0; i < n_chunks; i++) {
      lc_scan_memory(chunks[i]->data, chunks[i]->szB,
                     True,
                     -1, -1,
                     address, szB);
   }
   VG_(free) ( chunks );

   
   searched_wpa = address;
   searched_szB = szB;
   VG_(apply_to_GP_regs)(search_address_in_GP_reg);

}


