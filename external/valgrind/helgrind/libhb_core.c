

/*
   This file is part of LibHB, a library for implementing and checking
   the happens-before relationship in concurrent programs.

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
#include "pub_tool_poolalloc.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_wordfm.h"
#include "pub_tool_hashtable.h"
#include "pub_tool_xarray.h"
#include "pub_tool_oset.h"
#include "pub_tool_threadstate.h"
#include "pub_tool_aspacemgr.h"
#include "pub_tool_stacktrace.h"
#include "pub_tool_execontext.h"
#include "pub_tool_errormgr.h"
#include "pub_tool_options.h"        
#include "hg_basics.h"
#include "hg_wordset.h"
#include "hg_lock_n_thread.h"
#include "hg_errors.h"

#include "libhb.h"



#if 0
#  define CHECK_MSM 1
#else
#  define CHECK_MSM 0
#endif


#if 0
#  define CHECK_CEM 1
#else
#  define CHECK_CEM 0
#endif


#if 0
#  define CHECK_ZSM 1  
#  define inline __attribute__((noinline))
   
#else
#  define CHECK_ZSM 0   
#endif



typedef  UInt  VtsID;
#define VtsID_INVALID 0xFFFFFFFF




typedef  ULong  SVal;

#define SVal_INVALID (3ULL << 62)

#define SVal_NOACCESS (2ULL << 62)




#define SCALARTS_N_THRBITS 18  

#define SCALARTS_N_TYMBITS (64 - SCALARTS_N_THRBITS)
typedef
   struct {
      ThrID thrid : SCALARTS_N_THRBITS;
      ULong tym   : SCALARTS_N_TYMBITS;
   }
   ScalarTS;

#define ThrID_MAX_VALID ((1 << SCALARTS_N_THRBITS) - 1)




#define FI_LINE_SZB_LOG2  5
#define FI_NUM_LINES_LOG2 10

#define FI_LINE_SZB       (1 << FI_LINE_SZB_LOG2)
#define FI_NUM_LINES      (1 << FI_NUM_LINES_LOG2)

#define FI_TAG_MASK        (~(Addr)(FI_LINE_SZB - 1))
#define FI_GET_TAG(_a)     ((_a) & FI_TAG_MASK)

#define FI_GET_LINENO(_a)  ( ((_a) >> FI_LINE_SZB_LOG2) \
                             & (Addr)(FI_NUM_LINES-1) )



typedef
   struct {
      UShort u16s[FI_LINE_SZB / 8]; 
   }
   FiLine;

typedef
   struct {
      Addr   tags[FI_NUM_LINES];
      FiLine lines[FI_NUM_LINES];
   }
   Filter;




typedef
   struct { ULong ull; ExeContext* ec; }
   ULong_n_EC;


#define N_KWs_N_STACKs_PER_THREAD 62500


struct _Thr {
   VtsID viR;
   VtsID viW;

   Bool llexit_done;

   Bool joinedwith_done;

   ThrID thrid : SCALARTS_N_THRBITS;

   Filter* filter;

   Thread* hgthread;

   XArray*  local_Kws_n_stacks;
};




#define SO_MAGIC 0x56b3c5b0U

struct _SO {
   struct _SO* admin_prev;
   struct _SO* admin_next;
   VtsID viR; 
   VtsID viW; 
   UInt  magic;
};




static void        (*main_get_stacktrace)( Thr*, Addr*, UWord ) = NULL;
static ExeContext* (*main_get_EC)( Thr* ) = NULL;

static void VtsID__rcinc ( VtsID ii );
static void VtsID__rcdec ( VtsID ii );

static inline Bool SVal__isC ( SVal s );
static inline VtsID SVal__unC_Rmin ( SVal s );
static inline VtsID SVal__unC_Wmin ( SVal s );
static inline SVal SVal__mkC ( VtsID rmini, VtsID wmini );
static inline void SVal__rcinc ( SVal s );
static inline void SVal__rcdec ( SVal s );
static inline void *SVal2Ptr (SVal s);
static inline SVal Ptr2SVal (void* ptr);

SO* admin_SO;




#ifndef __HB_ZSM_H
#define __HB_ZSM_H

static void zsm_init ( void );

static void zsm_sset_range  ( Addr, SizeT, SVal );
static void zsm_sset_range_SMALL ( Addr a, SizeT len, SVal svNew );
static void zsm_scopy_range ( Addr, Addr, SizeT );
static void zsm_flush_cache ( void );

#endif 


#define ROUNDUP(a, N)   ((a + N - 1) & ~(N-1))
#define ROUNDDN(a, N)   ((a) & ~(N-1))

static inline Bool address_in_range (Addr a, Addr start,  SizeT szB)
{
   if (CHECK_ZSM)
      tl_assert ((a - start < szB)
                 == (start <= a
                     &&       a < start + szB));
   return a - start < szB;
}


#define N_LINE_BITS      6 
#define N_LINE_ARANGE    (1 << N_LINE_BITS)
#define N_LINE_TREES     (N_LINE_ARANGE >> 3)

typedef
   struct {
      UShort descrs[N_LINE_TREES];
      SVal   svals[N_LINE_ARANGE]; 
   }
   CacheLine;

#define TREE_DESCR_16_0 (1<<0)
#define TREE_DESCR_32_0 (1<<1)
#define TREE_DESCR_16_1 (1<<2)
#define TREE_DESCR_64   (1<<3)
#define TREE_DESCR_16_2 (1<<4)
#define TREE_DESCR_32_1 (1<<5)
#define TREE_DESCR_16_3 (1<<6)
#define TREE_DESCR_8_0  (1<<7)
#define TREE_DESCR_8_1  (1<<8)
#define TREE_DESCR_8_2  (1<<9)
#define TREE_DESCR_8_3  (1<<10)
#define TREE_DESCR_8_4  (1<<11)
#define TREE_DESCR_8_5  (1<<12)
#define TREE_DESCR_8_6  (1<<13)
#define TREE_DESCR_8_7  (1<<14)
#define TREE_DESCR_DTY  (1<<15)

typedef
   struct {
      SVal  dict[4]; 
      UChar ix2s[N_LINE_ARANGE/4]; 
   }
   LineZ; 



typedef
   struct {
      SVal w64s[N_LINE_ARANGE];
   }
   LineF; 

static PoolAlloc* LineF_pool_allocator;

static inline LineF *LineF_Ptr (LineZ *lineZ)
{
   tl_assert(lineZ->dict[0] == SVal_INVALID);
   return SVal2Ptr (lineZ->dict[1]);
}

#define N_SECMAP_BITS   13
#define N_SECMAP_ARANGE (1 << N_SECMAP_BITS)

#define N_SECMAP_ZLINES (N_SECMAP_ARANGE / N_LINE_ARANGE)


typedef
   struct {
      UInt   magic;
      LineZ  linesZ[N_SECMAP_ZLINES];
   }
   SecMap;

#define SecMap_MAGIC   0x571e58cbU

#define SecMap_free_MAGIC 0x5a977f30U

__attribute__((unused))
static inline Bool is_sane_SecMap ( SecMap* sm ) {
   return sm != NULL && sm->magic == SecMap_MAGIC;
}


#define N_WAY_BITS 16
#define N_WAY_NENT (1 << N_WAY_BITS)

typedef
   struct {
      CacheLine lyns0[N_WAY_NENT];
      Addr      tags0[N_WAY_NENT];
   }
   Cache;

static inline Bool is_valid_scache_tag ( Addr tag ) {
   return 0 == (tag & (N_LINE_ARANGE - 1));
}



static WordFM* map_shmem = NULL; 
static Cache   cache_shmem;


static UWord stats__secmaps_search       = 0; 
static UWord stats__secmaps_search_slow  = 0; 
static UWord stats__secmaps_allocd       = 0; 
static UWord stats__secmaps_in_map_shmem = 0; 
static UWord stats__secmaps_scanGC       = 0; 
static UWord stats__secmaps_scanGCed     = 0; 
static UWord stats__secmaps_ssetGCed     = 0; 
static UWord stats__secmap_ga_space_covered = 0; 
static UWord stats__secmap_linesZ_allocd = 0; 
static UWord stats__secmap_linesZ_bytes  = 0; 
static UWord stats__cache_Z_fetches      = 0; 
static UWord stats__cache_Z_wbacks       = 0; // # Z lines written back
static UWord stats__cache_F_fetches      = 0; 
static UWord stats__cache_F_wbacks       = 0; // # F lines written back
static UWord stats__cache_flushes_invals = 0; 
static UWord stats__cache_totrefs        = 0; 
static UWord stats__cache_totmisses      = 0; 
static ULong stats__cache_make_New_arange = 0; 
static ULong stats__cache_make_New_inZrep = 0; 
static UWord stats__cline_normalises     = 0; 
static UWord stats__cline_cread64s       = 0; 
static UWord stats__cline_cread32s       = 0; 
static UWord stats__cline_cread16s       = 0; 
static UWord stats__cline_cread08s       = 0; 
static UWord stats__cline_cwrite64s      = 0; 
static UWord stats__cline_cwrite32s      = 0; 
static UWord stats__cline_cwrite16s      = 0; 
static UWord stats__cline_cwrite08s      = 0; 
static UWord stats__cline_sread08s       = 0; 
static UWord stats__cline_swrite08s      = 0; 
static UWord stats__cline_swrite16s      = 0; 
static UWord stats__cline_swrite32s      = 0; 
static UWord stats__cline_swrite64s      = 0; 
static UWord stats__cline_scopy08s       = 0; 
static UWord stats__cline_64to32splits   = 0; 
static UWord stats__cline_32to16splits   = 0; 
static UWord stats__cline_16to8splits    = 0; 
static UWord stats__cline_64to32pulldown = 0; 
static UWord stats__cline_32to16pulldown = 0; 
static UWord stats__cline_16to8pulldown  = 0; 
static UWord stats__vts__tick            = 0; 
static UWord stats__vts__join            = 0; 
static UWord stats__vts__cmpLEQ          = 0; 
static UWord stats__vts__cmp_structural  = 0; 
static UWord stats__vts_tab_GC           = 0; 
static UWord stats__vts_pruning          = 0; 

static UWord stats__vts__cmp_structural_slow = 0;

static UWord stats__vts__indexat_slow = 0;

static UWord stats__vts_set__focaa    = 0;

static UWord stats__vts_set__focaa_a  = 0;


static inline Addr shmem__round_to_SecMap_base ( Addr a ) {
   return a & ~(N_SECMAP_ARANGE - 1);
}
static inline UWord shmem__get_SecMap_offset ( Addr a ) {
   return a & (N_SECMAP_ARANGE - 1);
}




static HChar* shmem__bigchunk_next = NULL;
static HChar* shmem__bigchunk_end1 = NULL;

static void* shmem__bigchunk_alloc ( SizeT n )
{
   const SizeT sHMEM__BIGCHUNK_SIZE = 4096 * 256 * 4;
   tl_assert(n > 0);
   n = VG_ROUNDUP(n, 16);
   tl_assert(shmem__bigchunk_next <= shmem__bigchunk_end1);
   tl_assert(shmem__bigchunk_end1 - shmem__bigchunk_next
             <= (SSizeT)sHMEM__BIGCHUNK_SIZE);
   if (shmem__bigchunk_next + n > shmem__bigchunk_end1) {
      if (0)
      VG_(printf)("XXXXX bigchunk: abandoning %d bytes\n",
                  (Int)(shmem__bigchunk_end1 - shmem__bigchunk_next));
      shmem__bigchunk_next = VG_(am_shadow_alloc)( sHMEM__BIGCHUNK_SIZE );
      if (shmem__bigchunk_next == NULL)
         VG_(out_of_memory_NORETURN)(
            "helgrind:shmem__bigchunk_alloc", sHMEM__BIGCHUNK_SIZE );
      shmem__bigchunk_end1 = shmem__bigchunk_next + sHMEM__BIGCHUNK_SIZE;
   }
   tl_assert(shmem__bigchunk_next);
   tl_assert( 0 == (((Addr)shmem__bigchunk_next) & (16-1)) );
   tl_assert(shmem__bigchunk_next + n <= shmem__bigchunk_end1);
   shmem__bigchunk_next += n;
   return shmem__bigchunk_next - n;
}

static inline SecMap *SecMap_freelist_next ( SecMap* sm )
{
   tl_assert (sm);
   tl_assert (sm->magic == SecMap_free_MAGIC);
   return SVal2Ptr (sm->linesZ[0].dict[1]);
}
static inline void set_SecMap_freelist_next ( SecMap* sm, SecMap* next )
{
   tl_assert (sm);
   tl_assert (sm->magic == SecMap_free_MAGIC);
   tl_assert (next == NULL || next->magic == SecMap_free_MAGIC);
   sm->linesZ[0].dict[1] = Ptr2SVal (next);
}

static SecMap *SecMap_freelist = NULL;
static UWord SecMap_freelist_length(void)
{
   SecMap *sm;
   UWord n = 0;

   sm = SecMap_freelist;
   while (sm) {
     n++;
     sm = SecMap_freelist_next (sm);
   }
   return n;
}

static void push_SecMap_on_freelist(SecMap* sm)
{
   if (0) VG_(message)(Vg_DebugMsg, "%p push\n", sm);
   sm->magic = SecMap_free_MAGIC;
   set_SecMap_freelist_next(sm, SecMap_freelist);
   SecMap_freelist = sm;
}
static SecMap *pop_SecMap_from_freelist(void)
{
   SecMap *sm;

   sm = SecMap_freelist;
   if (sm) {
      tl_assert (sm->magic == SecMap_free_MAGIC);
      SecMap_freelist = SecMap_freelist_next (sm);
      if (0) VG_(message)(Vg_DebugMsg, "%p pop\n", sm);
   }
   return sm;
}

static SecMap* shmem__alloc_or_recycle_SecMap ( void )
{
   Word    i, j;
   SecMap* sm = pop_SecMap_from_freelist();

   if (!sm) {
      sm = shmem__bigchunk_alloc( sizeof(SecMap) );
      stats__secmaps_allocd++;
      stats__secmap_ga_space_covered += N_SECMAP_ARANGE;
      stats__secmap_linesZ_allocd += N_SECMAP_ZLINES;
      stats__secmap_linesZ_bytes += N_SECMAP_ZLINES * sizeof(LineZ);
   }
   if (0) VG_(printf)("alloc_SecMap %p\n",sm);
   tl_assert(sm);
   sm->magic = SecMap_MAGIC;
   for (i = 0; i < N_SECMAP_ZLINES; i++) {
      sm->linesZ[i].dict[0] = SVal_NOACCESS;
      sm->linesZ[i].dict[1] = SVal_INVALID;
      sm->linesZ[i].dict[2] = SVal_INVALID;
      sm->linesZ[i].dict[3] = SVal_INVALID;
      for (j = 0; j < N_LINE_ARANGE/4; j++)
         sm->linesZ[i].ix2s[j] = 0; 
   }
   return sm;
}

typedef struct { Addr gaKey; SecMap* sm; } SMCacheEnt;
static SMCacheEnt smCache[3] = { {1,NULL}, {1,NULL}, {1,NULL} };

static SecMap* shmem__find_SecMap ( Addr ga ) 
{
   SecMap* sm    = NULL;
   Addr    gaKey = shmem__round_to_SecMap_base(ga);
   
   stats__secmaps_search++;
   if (LIKELY(gaKey == smCache[0].gaKey))
      return smCache[0].sm;
   if (LIKELY(gaKey == smCache[1].gaKey)) {
      SMCacheEnt tmp = smCache[0];
      smCache[0] = smCache[1];
      smCache[1] = tmp;
      return smCache[0].sm;
   }
   if (gaKey == smCache[2].gaKey) {
      SMCacheEnt tmp = smCache[1];
      smCache[1] = smCache[2];
      smCache[2] = tmp;
      return smCache[1].sm;
   }
   
   stats__secmaps_search_slow++;
   if (VG_(lookupFM)( map_shmem,
                      NULL, (UWord*)&sm, (UWord)gaKey )) {
      tl_assert(sm != NULL);
      smCache[2] = smCache[1];
      smCache[1] = smCache[0];
      smCache[0].gaKey = gaKey;
      smCache[0].sm    = sm;
   } else {
      tl_assert(sm == NULL);
   }
   return sm;
}

static UWord next_SecMap_GC_at = 1000;
__attribute__((noinline))
static UWord shmem__SecMap_do_GC(Bool really)
{
   UWord secmapW = 0;
   Addr  gaKey;
   UWord examined = 0;
   UWord ok_GCed = 0;

   
   smCache[0].gaKey = 1;
   smCache[1].gaKey = 1;
   smCache[2].gaKey = 1;
   STATIC_ASSERT (3 == sizeof(smCache)/sizeof(smCache[0]));

   VG_(initIterFM)( map_shmem );
   while (VG_(nextIterFM)( map_shmem, &gaKey, &secmapW )) {
      UWord   i;
      UWord   j;
      UWord   n_linesF = 0;
      SecMap* sm = (SecMap*)secmapW;
      tl_assert(sm->magic == SecMap_MAGIC);
      Bool ok_to_GC = True;

      examined++;

      
      for (i = 0; i < N_SECMAP_ZLINES && ok_to_GC; i++) {
         LineZ* lineZ = &sm->linesZ[i];
         if (lineZ->dict[0] != SVal_INVALID) {
            ok_to_GC = lineZ->dict[0] == SVal_NOACCESS
               && !SVal__isC (lineZ->dict[1])
               && !SVal__isC (lineZ->dict[2])
               && !SVal__isC (lineZ->dict[3]);
         } else {
            LineF *lineF = LineF_Ptr(lineZ);
            n_linesF++;
            for (j = 0; j < N_LINE_ARANGE && ok_to_GC; j++)
               ok_to_GC = lineF->w64s[j] == SVal_NOACCESS;
         }
      }
      if (ok_to_GC)
         ok_GCed++;
      if (ok_to_GC && really) {
        SecMap *fm_sm;
        Addr fm_gaKey;
        VG_(doneIterFM) ( map_shmem );
        if (n_linesF > 0) {
           for (i = 0; i < N_SECMAP_ZLINES && n_linesF > 0; i++) {
              LineZ* lineZ = &sm->linesZ[i];
              if (lineZ->dict[0] == SVal_INVALID) {
                 VG_(freeEltPA)( LineF_pool_allocator, LineF_Ptr(lineZ) );
                 n_linesF--;
              }
           }
        }
        if (!VG_(delFromFM)(map_shmem, &fm_gaKey, (UWord*)&fm_sm, gaKey))
          tl_assert (0);
        stats__secmaps_in_map_shmem--;
        tl_assert (gaKey == fm_gaKey);
        tl_assert (sm == fm_sm);
        stats__secmaps_scanGCed++;
        push_SecMap_on_freelist (sm);
        VG_(initIterAtFM) (map_shmem, gaKey + N_SECMAP_ARANGE);
      }
   }
   VG_(doneIterFM)( map_shmem );

   if (really) {
      stats__secmaps_scanGC++;
      
      next_SecMap_GC_at = stats__secmaps_allocd - 1000;
      if (ok_GCed < stats__secmaps_allocd/10)
         next_SecMap_GC_at = stats__secmaps_allocd + stats__secmaps_allocd/10;

   }

   if (VG_(clo_stats) && really) {
      VG_(message)(Vg_DebugMsg,
                  "libhb: SecMap GC: #%lu scanned %lu, GCed %lu,"
                   " next GC at %lu\n",
                   stats__secmaps_scanGC, examined, ok_GCed,
                   next_SecMap_GC_at);
   }

   return ok_GCed;
}

static SecMap* shmem__find_or_alloc_SecMap ( Addr ga )
{
   SecMap* sm = shmem__find_SecMap ( ga );
   if (LIKELY(sm)) {
      if (CHECK_ZSM) tl_assert(is_sane_SecMap(sm));
      return sm;
   } else {
      
      Addr gaKey = shmem__round_to_SecMap_base(ga);
      sm = shmem__alloc_or_recycle_SecMap();
      tl_assert(sm);
      VG_(addToFM)( map_shmem, (UWord)gaKey, (UWord)sm );
      stats__secmaps_in_map_shmem++;
      if (CHECK_ZSM) tl_assert(is_sane_SecMap(sm));
      return sm;
   }
}

__attribute__((noinline))
static UWord shmem__SecMap_used_linesF(void)
{
   UWord secmapW = 0;
   Addr  gaKey;
   UWord inUse = 0;

   VG_(initIterFM)( map_shmem );
   while (VG_(nextIterFM)( map_shmem, &gaKey, &secmapW )) {
      UWord   i;
      SecMap* sm = (SecMap*)secmapW;
      tl_assert(sm->magic == SecMap_MAGIC);

      for (i = 0; i < N_SECMAP_ZLINES; i++) {
         LineZ* lineZ = &sm->linesZ[i];
         if (lineZ->dict[0] == SVal_INVALID)
            inUse++;
      }
   }
   VG_(doneIterFM)( map_shmem );

   return inUse;
}


static void rcinc_LineF ( LineF* lineF ) {
   UWord i;
   for (i = 0; i < N_LINE_ARANGE; i++)
      SVal__rcinc(lineF->w64s[i]);
}

static void rcdec_LineF ( LineF* lineF ) {
   UWord i;
   for (i = 0; i < N_LINE_ARANGE; i++)
      SVal__rcdec(lineF->w64s[i]);
}

static void rcinc_LineZ ( LineZ* lineZ ) {
   tl_assert(lineZ->dict[0] != SVal_INVALID);
   SVal__rcinc(lineZ->dict[0]);
   if (lineZ->dict[1] != SVal_INVALID) SVal__rcinc(lineZ->dict[1]);
   if (lineZ->dict[2] != SVal_INVALID) SVal__rcinc(lineZ->dict[2]);
   if (lineZ->dict[3] != SVal_INVALID) SVal__rcinc(lineZ->dict[3]);
}

static void rcdec_LineZ ( LineZ* lineZ ) {
   tl_assert(lineZ->dict[0] != SVal_INVALID);
   SVal__rcdec(lineZ->dict[0]);
   if (lineZ->dict[1] != SVal_INVALID) SVal__rcdec(lineZ->dict[1]);
   if (lineZ->dict[2] != SVal_INVALID) SVal__rcdec(lineZ->dict[2]);
   if (lineZ->dict[3] != SVal_INVALID) SVal__rcdec(lineZ->dict[3]);
}

inline
static void write_twobit_array ( UChar* arr, UWord ix, UWord b2 ) {
   Word bix, shft, mask, prep;
   tl_assert(ix >= 0);
   bix  = ix >> 2;
   shft = 2 * (ix & 3); 
   mask = 3 << shft;
   prep = b2 << shft;
   arr[bix] = (arr[bix] & ~mask) | prep;
}

inline
static UWord read_twobit_array ( UChar* arr, UWord ix ) {
   Word bix, shft;
   tl_assert(ix >= 0);
   bix  = ix >> 2;
   shft = 2 * (ix & 3); 
   return (arr[bix] >> shft) & 3;
}

static LineF *free_lineF = NULL;

static inline LineF *alloc_LineF_for_Z (LineZ *lineZ)
{
   LineF *lineF;

   tl_assert(lineZ->dict[0] == SVal_INVALID);

   if (LIKELY(free_lineF)) {
      lineF = free_lineF;
      free_lineF = NULL;
   } else {
      lineF = VG_(allocEltPA) ( LineF_pool_allocator );
   }
   lineZ->dict[0] = lineZ->dict[2] = lineZ->dict[3] = SVal_INVALID;
   lineZ->dict[1] = Ptr2SVal (lineF);

   return lineF;
}

/* rcdec the LineF of lineZ, frees the lineF, and sets lineZ
   back to its initial state SVal_NOACCESS (i.e. ready to be
   read or written just after SecMap allocation). */
static inline void clear_LineF_of_Z (LineZ *lineZ)
{
   LineF *lineF = LineF_Ptr(lineZ);

   rcdec_LineF(lineF);
   if (UNLIKELY(free_lineF)) {
      VG_(freeEltPA)( LineF_pool_allocator, lineF );
   } else {
      free_lineF = lineF;
   }
   lineZ->dict[0] = SVal_NOACCESS;
   lineZ->dict[1] = SVal_INVALID;
}

static void find_ZF_for_reading ( LineZ** zp,
                                  LineF** fp, Addr tag ) {
   LineZ* lineZ;
   LineF* lineF;
   UWord   zix;
   SecMap* sm    = shmem__find_or_alloc_SecMap(tag);
   UWord   smoff = shmem__get_SecMap_offset(tag);
   tl_assert(0 == (smoff & (N_LINE_ARANGE - 1)));
   zix = smoff >> N_LINE_BITS;
   tl_assert(zix < N_SECMAP_ZLINES);
   lineZ = &sm->linesZ[zix];
   lineF = NULL;
   if (lineZ->dict[0] == SVal_INVALID) {
      lineF = LineF_Ptr (lineZ);
      lineZ = NULL;
   }
   *zp = lineZ;
   *fp = lineF;
}

/* Given address 'tag', return the relevant SecMap and the index of
   the LineZ within it, in the expectation that the line is to be
   overwritten.  Regardless of whether 'tag' is currently associated
   with a Z or F representation, to rcdec on the current
   representation, in recognition of the fact that the contents are
   just about to be overwritten. */
static __attribute__((noinline))
void find_Z_for_writing ( SecMap** smp,
                          Word* zixp,
                          Addr tag ) {
   LineZ* lineZ;
   UWord   zix;
   SecMap* sm    = shmem__find_or_alloc_SecMap(tag);
   UWord   smoff = shmem__get_SecMap_offset(tag);
   tl_assert(0 == (smoff & (N_LINE_ARANGE - 1)));
   zix = smoff >> N_LINE_BITS;
   tl_assert(zix < N_SECMAP_ZLINES);
   lineZ = &sm->linesZ[zix];
   
   if (lineZ->dict[0] == SVal_INVALID)
      clear_LineF_of_Z(lineZ);
   else
      rcdec_LineZ(lineZ);
   *smp  = sm;
   *zixp = zix;
}


__attribute__((unused))
static void pp_CacheLine ( CacheLine* cl ) {
   Word i;
   if (!cl) {
      VG_(printf)("%s","pp_CacheLine(NULL)\n");
      return;
   }
   for (i = 0; i < N_LINE_TREES; i++) 
      VG_(printf)("   descr: %04lx\n", (UWord)cl->descrs[i]);
   for (i = 0; i < N_LINE_ARANGE; i++) 
      VG_(printf)("    sval: %08lx\n", (UWord)cl->svals[i]);
}

static UChar descr_to_validbits ( UShort descr )
{
   
#  define DESCR(b8_7, b8_6, b8_5, b8_4, b8_3, b8_2, b8_1, b8_0, \
                b16_3, b32_1, b16_2, b64, b16_1, b32_0, b16_0)  \
             ( (UShort) ( ( (b8_7)  << 14) | ( (b8_6)  << 13) | \
                          ( (b8_5)  << 12) | ( (b8_4)  << 11) | \
                          ( (b8_3)  << 10) | ( (b8_2)  << 9)  | \
                          ( (b8_1)  << 8)  | ( (b8_0)  << 7)  | \
                          ( (b16_3) << 6)  | ( (b32_1) << 5)  | \
                          ( (b16_2) << 4)  | ( (b64)   << 3)  | \
                          ( (b16_1) << 2)  | ( (b32_0) << 1)  | \
                          ( (b16_0) << 0) ) )

#  define BYTE(bit7, bit6, bit5, bit4, bit3, bit2, bit1, bit0) \
             ( (UChar) ( ( (bit7) << 7) | ( (bit6) << 6) | \
                         ( (bit5) << 5) | ( (bit4) << 4) | \
                         ( (bit3) << 3) | ( (bit2) << 2) | \
                         ( (bit1) << 1) | ( (bit0) << 0) ) )

   
   tl_assert(DESCR(1,0,0,0,0,0,0,0, 0,0,0, 0, 0,0,0) == TREE_DESCR_8_7);
   tl_assert(DESCR(0,0,0,0,0,0,0,1, 0,0,0, 0, 0,0,0) == TREE_DESCR_8_0);
   tl_assert(DESCR(0,0,0,0,0,0,0,0, 1,0,0, 0, 0,0,0) == TREE_DESCR_16_3);
   tl_assert(DESCR(0,0,0,0,0,0,0,0, 0,1,0, 0, 0,0,0) == TREE_DESCR_32_1);
   tl_assert(DESCR(0,0,0,0,0,0,0,0, 0,0,1, 0, 0,0,0) == TREE_DESCR_16_2);
   tl_assert(DESCR(0,0,0,0,0,0,0,0, 0,0,0, 1, 0,0,0) == TREE_DESCR_64);
   tl_assert(DESCR(0,0,0,0,0,0,0,0, 0,0,0, 0, 1,0,0) == TREE_DESCR_16_1);
   tl_assert(DESCR(0,0,0,0,0,0,0,0, 0,0,0, 0, 0,1,0) == TREE_DESCR_32_0);
   tl_assert(DESCR(0,0,0,0,0,0,0,0, 0,0,0, 0, 0,0,1) == TREE_DESCR_16_0);

   switch (descr) {
   case DESCR(1,1,1,1,1,1,1,1, 0,0,0, 0, 0,0,0): 
                                                 return BYTE(1,1,1,1,1,1,1,1);
   case DESCR(1,1,0,0,1,1,1,1, 0,0,1, 0, 0,0,0): 
                                                 return BYTE(1,1,0,1,1,1,1,1);
   case DESCR(0,0,1,1,1,1,1,1, 1,0,0, 0, 0,0,0):  
                                                 return BYTE(0,1,1,1,1,1,1,1);
   case DESCR(0,0,0,0,1,1,1,1, 1,0,1, 0, 0,0,0): 
                                                 return BYTE(0,1,0,1,1,1,1,1);

   case DESCR(1,1,1,1,1,1,0,0, 0,0,0, 0, 0,0,1):  
                                                 return BYTE(1,1,1,1,1,1,0,1);
   case DESCR(1,1,0,0,1,1,0,0, 0,0,1, 0, 0,0,1): 
                                                 return BYTE(1,1,0,1,1,1,0,1);
   case DESCR(0,0,1,1,1,1,0,0, 1,0,0, 0, 0,0,1): 
                                                 return BYTE(0,1,1,1,1,1,0,1);
   case DESCR(0,0,0,0,1,1,0,0, 1,0,1, 0, 0,0,1): 
                                                 return BYTE(0,1,0,1,1,1,0,1);

   case DESCR(1,1,1,1,0,0,1,1, 0,0,0, 0, 1,0,0): 
                                                 return BYTE(1,1,1,1,0,1,1,1);
   case DESCR(1,1,0,0,0,0,1,1, 0,0,1, 0, 1,0,0): 
                                                 return BYTE(1,1,0,1,0,1,1,1);
   case DESCR(0,0,1,1,0,0,1,1, 1,0,0, 0, 1,0,0): 
                                                 return BYTE(0,1,1,1,0,1,1,1);
   case DESCR(0,0,0,0,0,0,1,1, 1,0,1, 0, 1,0,0): 
                                                 return BYTE(0,1,0,1,0,1,1,1);

   case DESCR(1,1,1,1,0,0,0,0, 0,0,0, 0, 1,0,1): 
                                                 return BYTE(1,1,1,1,0,1,0,1);
   case DESCR(1,1,0,0,0,0,0,0, 0,0,1, 0, 1,0,1): 
                                                 return BYTE(1,1,0,1,0,1,0,1);
   case DESCR(0,0,1,1,0,0,0,0, 1,0,0, 0, 1,0,1): 
                                                 return BYTE(0,1,1,1,0,1,0,1);
   case DESCR(0,0,0,0,0,0,0,0, 1,0,1, 0, 1,0,1): 
                                                 return BYTE(0,1,0,1,0,1,0,1);

   case DESCR(0,0,0,0,1,1,1,1, 0,1,0, 0, 0,0,0): 
                                                 return BYTE(0,0,0,1,1,1,1,1);
   case DESCR(0,0,0,0,1,1,0,0, 0,1,0, 0, 0,0,1): 
                                                 return BYTE(0,0,0,1,1,1,0,1);
   case DESCR(0,0,0,0,0,0,1,1, 0,1,0, 0, 1,0,0): 
                                                 return BYTE(0,0,0,1,0,1,1,1);
   case DESCR(0,0,0,0,0,0,0,0, 0,1,0, 0, 1,0,1): 
                                                 return BYTE(0,0,0,1,0,1,0,1);

   case DESCR(1,1,1,1,0,0,0,0, 0,0,0, 0, 0,1,0): 
                                                 return BYTE(1,1,1,1,0,0,0,1);
   case DESCR(1,1,0,0,0,0,0,0, 0,0,1, 0, 0,1,0): 
                                                 return BYTE(1,1,0,1,0,0,0,1);
   case DESCR(0,0,1,1,0,0,0,0, 1,0,0, 0, 0,1,0): 
                                                 return BYTE(0,1,1,1,0,0,0,1);
   case DESCR(0,0,0,0,0,0,0,0, 1,0,1, 0, 0,1,0): 
                                                 return BYTE(0,1,0,1,0,0,0,1);

   case DESCR(0,0,0,0,0,0,0,0, 0,1,0, 0, 0,1,0): 
                                                 return BYTE(0,0,0,1,0,0,0,1);

   case DESCR(0,0,0,0,0,0,0,0, 0,0,0, 1, 0,0,0): 
                                                 return BYTE(0,0,0,0,0,0,0,1);

   default: return BYTE(0,0,0,0,0,0,0,0); 
   }
   
   tl_assert(0);

#  undef DESCR
#  undef BYTE
}

__attribute__((unused))
static Bool is_sane_Descr ( UShort descr ) {
   return descr_to_validbits(descr) != 0;
}

static void sprintf_Descr ( HChar* dst, UShort descr ) {
   VG_(sprintf)(dst, 
                "%d%d%d%d%d%d%d%d %d%d%d %d %d%d%d",
                (Int)((descr & TREE_DESCR_8_7) ? 1 : 0),
                (Int)((descr & TREE_DESCR_8_6) ? 1 : 0),
                (Int)((descr & TREE_DESCR_8_5) ? 1 : 0),
                (Int)((descr & TREE_DESCR_8_4) ? 1 : 0),
                (Int)((descr & TREE_DESCR_8_3) ? 1 : 0),
                (Int)((descr & TREE_DESCR_8_2) ? 1 : 0),
                (Int)((descr & TREE_DESCR_8_1) ? 1 : 0),
                (Int)((descr & TREE_DESCR_8_0) ? 1 : 0),
                (Int)((descr & TREE_DESCR_16_3) ? 1 : 0),
                (Int)((descr & TREE_DESCR_32_1) ? 1 : 0),
                (Int)((descr & TREE_DESCR_16_2) ? 1 : 0),
                (Int)((descr & TREE_DESCR_64)   ? 1 : 0),
                (Int)((descr & TREE_DESCR_16_1) ? 1 : 0),
                (Int)((descr & TREE_DESCR_32_0) ? 1 : 0),
                (Int)((descr & TREE_DESCR_16_0) ? 1 : 0)
   );
}
static void sprintf_Byte ( HChar* dst, UChar byte ) {
   VG_(sprintf)(dst, "%d%d%d%d%d%d%d%d",
                     (Int)((byte & 128) ? 1 : 0),
                     (Int)((byte &  64) ? 1 : 0),
                     (Int)((byte &  32) ? 1 : 0),
                     (Int)((byte &  16) ? 1 : 0),
                     (Int)((byte &   8) ? 1 : 0),
                     (Int)((byte &   4) ? 1 : 0),
                     (Int)((byte &   2) ? 1 : 0),
                     (Int)((byte &   1) ? 1 : 0)
   );
}

static Bool is_sane_Descr_and_Tree ( UShort descr, SVal* tree ) {
   Word  i;
   UChar validbits = descr_to_validbits(descr);
   HChar buf[128], buf2[128];    
   if (validbits == 0)
      goto bad;
   for (i = 0; i < 8; i++) {
      if (validbits & (1<<i)) {
         if (tree[i] == SVal_INVALID)
            goto bad;
      } else {
         if (tree[i] != SVal_INVALID)
            goto bad;
      }
   }
   return True;
  bad:
   sprintf_Descr( buf, descr );
   sprintf_Byte( buf2, validbits );
   VG_(printf)("%s","is_sane_Descr_and_Tree: bad tree {\n");
   VG_(printf)("   validbits 0x%02lx    %s\n", (UWord)validbits, buf2);
   VG_(printf)("       descr 0x%04lx  %s\n", (UWord)descr, buf);
   for (i = 0; i < 8; i++)
      VG_(printf)("   [%ld] 0x%016llx\n", i, tree[i]);
   VG_(printf)("%s","}\n");
   return 0;
}

static Bool is_sane_CacheLine ( CacheLine* cl )
{
   Word tno, cloff;

   if (!cl) goto bad;

   for (tno = 0, cloff = 0;  tno < N_LINE_TREES;  tno++, cloff += 8) {
      UShort descr = cl->descrs[tno];
      SVal*  tree  = &cl->svals[cloff];
      if (!is_sane_Descr_and_Tree(descr, tree))
         goto bad;
   }
   tl_assert(cloff == N_LINE_ARANGE);
   return True;
  bad:
   pp_CacheLine(cl);
   return False;
}

static UShort normalise_tree ( SVal* tree )
{
   UShort descr;
   if (CHECK_ZSM
       && UNLIKELY(tree[7] == SVal_INVALID || tree[6] == SVal_INVALID
                   || tree[5] == SVal_INVALID || tree[4] == SVal_INVALID
                   || tree[3] == SVal_INVALID || tree[2] == SVal_INVALID
                   || tree[1] == SVal_INVALID || tree[0] == SVal_INVALID))
      tl_assert(0);
   
   descr = TREE_DESCR_8_7 | TREE_DESCR_8_6 | TREE_DESCR_8_5
           | TREE_DESCR_8_4 | TREE_DESCR_8_3 | TREE_DESCR_8_2
           | TREE_DESCR_8_1 | TREE_DESCR_8_0;
   
   if (tree[1] == tree[0]) {
      tree[1] = SVal_INVALID;
      descr &= ~(TREE_DESCR_8_1 | TREE_DESCR_8_0);
      descr |= TREE_DESCR_16_0;
   }
   if (tree[3] == tree[2]) {
      tree[3] = SVal_INVALID;
      descr &= ~(TREE_DESCR_8_3 | TREE_DESCR_8_2);
      descr |= TREE_DESCR_16_1;
   }
   if (tree[5] == tree[4]) {
      tree[5] = SVal_INVALID;
      descr &= ~(TREE_DESCR_8_5 | TREE_DESCR_8_4);
      descr |= TREE_DESCR_16_2;
   }
   if (tree[7] == tree[6]) {
      tree[7] = SVal_INVALID;
      descr &= ~(TREE_DESCR_8_7 | TREE_DESCR_8_6);
      descr |= TREE_DESCR_16_3;
   }
   
   if (tree[2] == tree[0]
       && (descr & TREE_DESCR_16_1) && (descr & TREE_DESCR_16_0)) {
      tree[2] = SVal_INVALID; 
      descr &= ~(TREE_DESCR_16_1 | TREE_DESCR_16_0);
      descr |= TREE_DESCR_32_0;
   }
   if (tree[6] == tree[4]
       && (descr & TREE_DESCR_16_3) && (descr & TREE_DESCR_16_2)) {
      tree[6] = SVal_INVALID; 
      descr &= ~(TREE_DESCR_16_3 | TREE_DESCR_16_2);
      descr |= TREE_DESCR_32_1;
   }
   
   if (tree[4] == tree[0]
       && (descr & TREE_DESCR_32_1) && (descr & TREE_DESCR_32_0)) {
      tree[4] = SVal_INVALID; 
      descr &= ~(TREE_DESCR_32_1 | TREE_DESCR_32_0);
      descr |= TREE_DESCR_64;
   }
   return descr;
}

static void normalise_CacheLine ( CacheLine* cl )
{
   Word tno, cloff;
   for (tno = 0, cloff = 0;  tno < N_LINE_TREES;  tno++, cloff += 8) {
      SVal* tree = &cl->svals[cloff];
      cl->descrs[tno] = normalise_tree( tree );
   }
   tl_assert(cloff == N_LINE_ARANGE);
   if (CHECK_ZSM)
      tl_assert(is_sane_CacheLine(cl)); 
   stats__cline_normalises++;
}


typedef struct { UChar count; SVal sval; } CountedSVal;

static
void sequentialise_CacheLine ( CountedSVal* dst,
                               Word* dstUsedP,
                               Word nDst, CacheLine* src )
{
   Word  tno, cloff, dstUsed;

   tl_assert(nDst == N_LINE_ARANGE);
   dstUsed = 0;

   for (tno = 0, cloff = 0;  tno < N_LINE_TREES;  tno++, cloff += 8) {
      UShort descr = src->descrs[tno];
      SVal*  tree  = &src->svals[cloff];

      
#     define PUT(_n,_v)                                \
         do { dst[dstUsed  ].count = (_n);             \
              dst[dstUsed++].sval  = (_v);             \
         } while (0)

      
      if (descr & TREE_DESCR_64)   PUT(8, tree[0]); else
      if (descr & TREE_DESCR_32_0) PUT(4, tree[0]); else
      if (descr & TREE_DESCR_16_0) PUT(2, tree[0]); else
      if (descr & TREE_DESCR_8_0)  PUT(1, tree[0]);
      
      if (descr & TREE_DESCR_8_1)  PUT(1, tree[1]);
      
      if (descr & TREE_DESCR_16_1) PUT(2, tree[2]); else
      if (descr & TREE_DESCR_8_2)  PUT(1, tree[2]);
      
      if (descr & TREE_DESCR_8_3)  PUT(1, tree[3]);
      
      if (descr & TREE_DESCR_32_1) PUT(4, tree[4]); else
      if (descr & TREE_DESCR_16_2) PUT(2, tree[4]); else
      if (descr & TREE_DESCR_8_4)  PUT(1, tree[4]);
      
      if (descr & TREE_DESCR_8_5)  PUT(1, tree[5]);
      
      if (descr & TREE_DESCR_16_3) PUT(2, tree[6]); else
      if (descr & TREE_DESCR_8_6)  PUT(1, tree[6]);
      
      if (descr & TREE_DESCR_8_7)  PUT(1, tree[7]);

#     undef PUT
      

   }
   tl_assert(cloff == N_LINE_ARANGE);
   tl_assert(dstUsed <= nDst);

   *dstUsedP = dstUsed;
}

static __attribute__((noinline)) void cacheline_wback ( UWord wix )
{
   Word        i, j, k, m;
   Addr        tag;
   SecMap*     sm;
   CacheLine*  cl;
   LineZ* lineZ;
   LineF* lineF;
   Word        zix, fix, csvalsUsed;
   CountedSVal csvals[N_LINE_ARANGE];
   SVal        sv;

   if (0)
   VG_(printf)("scache wback line %d\n", (Int)wix);

   tl_assert(wix >= 0 && wix < N_WAY_NENT);

   tag =  cache_shmem.tags0[wix];
   cl  = &cache_shmem.lyns0[wix];

   
   if (!is_valid_scache_tag(tag))
      return;

   
   sm         = NULL;
   lineZ      = NULL;
   lineF      = NULL;
   zix = fix = -1;

   find_Z_for_writing( &sm, &zix, tag );

   tl_assert(sm);
   tl_assert(zix >= 0 && zix < N_SECMAP_ZLINES);
   lineZ = &sm->linesZ[zix];

   
   if (CHECK_ZSM)
      tl_assert(is_sane_CacheLine(cl)); 

   csvalsUsed = -1;
   sequentialise_CacheLine( csvals, &csvalsUsed, 
                            N_LINE_ARANGE, cl );
   tl_assert(csvalsUsed >= 1 && csvalsUsed <= N_LINE_ARANGE);
   if (0) VG_(printf)("%lu ", csvalsUsed);

   lineZ->dict[0] = lineZ->dict[1] 
                  = lineZ->dict[2] = lineZ->dict[3] = SVal_INVALID;

   
   i = 0;
   for (k = 0; k < csvalsUsed; k++) {

      sv = csvals[k].sval;
      if (CHECK_ZSM)
         tl_assert(csvals[k].count >= 1 && csvals[k].count <= 8);
      
      if (sv == lineZ->dict[0]) { j = 0; goto dict_ok; }
      if (sv == lineZ->dict[1]) { j = 1; goto dict_ok; }
      if (sv == lineZ->dict[2]) { j = 2; goto dict_ok; }
      if (sv == lineZ->dict[3]) { j = 3; goto dict_ok; }
      
      if (CHECK_ZSM)
         tl_assert(sv != SVal_INVALID);
      if (lineZ->dict[0] 
          == SVal_INVALID) { lineZ->dict[0] = sv; j = 0; goto dict_ok; }
      if (lineZ->dict[1]
          == SVal_INVALID) { lineZ->dict[1] = sv; j = 1; goto dict_ok; }
      if (lineZ->dict[2]
          == SVal_INVALID) { lineZ->dict[2] = sv; j = 2; goto dict_ok; }
      if (lineZ->dict[3]
          == SVal_INVALID) { lineZ->dict[3] = sv; j = 3; goto dict_ok; }
      break; 
     dict_ok:
      m = csvals[k].count;
      if (m == 8) {
         write_twobit_array( lineZ->ix2s, i+0, j );
         write_twobit_array( lineZ->ix2s, i+1, j );
         write_twobit_array( lineZ->ix2s, i+2, j );
         write_twobit_array( lineZ->ix2s, i+3, j );
         write_twobit_array( lineZ->ix2s, i+4, j );
         write_twobit_array( lineZ->ix2s, i+5, j );
         write_twobit_array( lineZ->ix2s, i+6, j );
         write_twobit_array( lineZ->ix2s, i+7, j );
         i += 8;
      }
      else if (m == 4) {
         write_twobit_array( lineZ->ix2s, i+0, j );
         write_twobit_array( lineZ->ix2s, i+1, j );
         write_twobit_array( lineZ->ix2s, i+2, j );
         write_twobit_array( lineZ->ix2s, i+3, j );
         i += 4;
      }
      else if (m == 1) {
         write_twobit_array( lineZ->ix2s, i+0, j );
         i += 1;
      }
      else if (m == 2) {
         write_twobit_array( lineZ->ix2s, i+0, j );
         write_twobit_array( lineZ->ix2s, i+1, j );
         i += 2;
      }
      else {
         tl_assert(0); 
      }

   }

   if (LIKELY(i == N_LINE_ARANGE)) {
      rcinc_LineZ(lineZ);
      stats__cache_Z_wbacks++;
   } else {
      tl_assert(i >= 0 && i < N_LINE_ARANGE);
      lineZ->dict[0] = lineZ->dict[2] = lineZ->dict[3] = SVal_INVALID;
      lineF = alloc_LineF_for_Z (lineZ);
      i = 0;
      for (k = 0; k < csvalsUsed; k++) {
         if (CHECK_ZSM)
            tl_assert(csvals[k].count >= 1 && csvals[k].count <= 8);
         sv = csvals[k].sval;
         if (CHECK_ZSM)
            tl_assert(sv != SVal_INVALID);
         for (m = csvals[k].count; m > 0; m--) {
            lineF->w64s[i] = sv;
            i++;
         }
      }
      tl_assert(i == N_LINE_ARANGE);
      rcinc_LineF(lineF);
      stats__cache_F_wbacks++;
   }
}

static __attribute__((noinline)) void cacheline_fetch ( UWord wix )
{
   Word       i;
   Addr       tag;
   CacheLine* cl;
   LineZ*     lineZ;
   LineF*     lineF;

   if (0)
   VG_(printf)("scache fetch line %d\n", (Int)wix);

   tl_assert(wix >= 0 && wix < N_WAY_NENT);

   tag =  cache_shmem.tags0[wix];
   cl  = &cache_shmem.lyns0[wix];

   
   tl_assert(is_valid_scache_tag(tag));

   lineZ = NULL;
   lineF = NULL;
   find_ZF_for_reading( &lineZ, &lineF, tag );
   tl_assert( (lineZ && !lineF) || (!lineZ && lineF) );

   if (lineF) {
      for (i = 0; i < N_LINE_ARANGE; i++) {
         cl->svals[i] = lineF->w64s[i];
      }
      stats__cache_F_fetches++;
   } else {
      for (i = 0; i < N_LINE_ARANGE; i++) {
         UWord ix = read_twobit_array( lineZ->ix2s, i );
         if (CHECK_ZSM) tl_assert(ix >= 0 && ix <= 3);
         cl->svals[i] = lineZ->dict[ix];
         if (CHECK_ZSM) tl_assert(cl->svals[i] != SVal_INVALID);
      }
      stats__cache_Z_fetches++;
   }
   normalise_CacheLine( cl );
}

static void shmem__invalidate_scache_range (Addr ga, SizeT szB)
{
   Word wix;

   
   tl_assert (is_valid_scache_tag (ga));
   
   tl_assert (0 == (szB & (N_LINE_ARANGE - 1)));
   

   Word ga_ix = (ga >> N_LINE_BITS) & (N_WAY_NENT - 1);
   Word nwix = szB / N_LINE_ARANGE;

   if (nwix > N_WAY_NENT)
      nwix = N_WAY_NENT; 

   for (wix = 0; wix < nwix; wix++) {
      if (address_in_range(cache_shmem.tags0[ga_ix], ga, szB))
         cache_shmem.tags0[ga_ix] = 1;
      ga_ix++;
      if (UNLIKELY(ga_ix == N_WAY_NENT))
         ga_ix = 0;
   }
}


static void shmem__flush_and_invalidate_scache ( void ) {
   Word wix;
   Addr tag;
   if (0) VG_(printf)("%s","scache flush and invalidate\n");
   tl_assert(!is_valid_scache_tag(1));
   for (wix = 0; wix < N_WAY_NENT; wix++) {
      tag = cache_shmem.tags0[wix];
      if (tag == 1) {
         
      } else {
         tl_assert(is_valid_scache_tag(tag));
         cacheline_wback( wix );
      }
      cache_shmem.tags0[wix] = 1;
   }
   stats__cache_flushes_invals++;
}


static inline Bool aligned16 ( Addr a ) {
   return 0 == (a & 1);
}
static inline Bool aligned32 ( Addr a ) {
   return 0 == (a & 3);
}
static inline Bool aligned64 ( Addr a ) {
   return 0 == (a & 7);
}
static inline UWord get_cacheline_offset ( Addr a ) {
   return (UWord)(a & (N_LINE_ARANGE - 1));
}
static inline Addr cacheline_ROUNDUP ( Addr a ) {
   return ROUNDUP(a, N_LINE_ARANGE);
}
static inline Addr cacheline_ROUNDDN ( Addr a ) {
   return ROUNDDN(a, N_LINE_ARANGE);
}
static inline UWord get_treeno ( Addr a ) {
   return get_cacheline_offset(a) >> 3;
}
static inline UWord get_tree_offset ( Addr a ) {
   return a & 7;
}

static __attribute__((noinline))
       CacheLine* get_cacheline_MISS ( Addr a ); 
static inline CacheLine* get_cacheline ( Addr a )
{
   Addr       tag = a & ~(N_LINE_ARANGE - 1);
   UWord      wix = (a >> N_LINE_BITS) & (N_WAY_NENT - 1);
   stats__cache_totrefs++;
   if (LIKELY(tag == cache_shmem.tags0[wix])) {
      return &cache_shmem.lyns0[wix];
   } else {
      return get_cacheline_MISS( a );
   }
}

static __attribute__((noinline))
       CacheLine* get_cacheline_MISS ( Addr a )
{

   CacheLine* cl;
   Addr*      tag_old_p;
   Addr       tag = a & ~(N_LINE_ARANGE - 1);
   UWord      wix = (a >> N_LINE_BITS) & (N_WAY_NENT - 1);

   tl_assert(tag != cache_shmem.tags0[wix]);

   
   stats__cache_totmisses++;

   cl        = &cache_shmem.lyns0[wix];
   tag_old_p = &cache_shmem.tags0[wix];

   if (is_valid_scache_tag( *tag_old_p )) {
      
      if (CHECK_ZSM)
         tl_assert(is_sane_CacheLine(cl)); 
      cacheline_wback( wix );
   }
   
   *tag_old_p = tag;
   cacheline_fetch( wix );
   if (CHECK_ZSM)
      tl_assert(is_sane_CacheLine(cl)); 
   return cl;
}

static UShort pulldown_to_32 ( SVal* tree, UWord toff, UShort descr ) {
   stats__cline_64to32pulldown++;
   switch (toff) {
      case 0: case 4:
         tl_assert(descr & TREE_DESCR_64);
         tree[4] = tree[0];
         descr &= ~TREE_DESCR_64;
         descr |= (TREE_DESCR_32_1 | TREE_DESCR_32_0);
         break;
      default:
         tl_assert(0);
   }
   return descr;
}

static UShort pulldown_to_16 ( SVal* tree, UWord toff, UShort descr ) {
   stats__cline_32to16pulldown++;
   switch (toff) {
      case 0: case 2:
         if (!(descr & TREE_DESCR_32_0)) {
            descr = pulldown_to_32(tree, 0, descr);
         }
         tl_assert(descr & TREE_DESCR_32_0);
         tree[2] = tree[0];
         descr &= ~TREE_DESCR_32_0;
         descr |= (TREE_DESCR_16_1 | TREE_DESCR_16_0);
         break;
      case 4: case 6:
         if (!(descr & TREE_DESCR_32_1)) {
            descr = pulldown_to_32(tree, 4, descr);
         }
         tl_assert(descr & TREE_DESCR_32_1);
         tree[6] = tree[4];
         descr &= ~TREE_DESCR_32_1;
         descr |= (TREE_DESCR_16_3 | TREE_DESCR_16_2);
         break;
      default:
         tl_assert(0);
   }
   return descr;
}

static UShort pulldown_to_8 ( SVal* tree, UWord toff, UShort descr ) {
   stats__cline_16to8pulldown++;
   switch (toff) {
      case 0: case 1:
         if (!(descr & TREE_DESCR_16_0)) {
            descr = pulldown_to_16(tree, 0, descr);
         }
         tl_assert(descr & TREE_DESCR_16_0);
         tree[1] = tree[0];
         descr &= ~TREE_DESCR_16_0;
         descr |= (TREE_DESCR_8_1 | TREE_DESCR_8_0);
         break;
      case 2: case 3:
         if (!(descr & TREE_DESCR_16_1)) {
            descr = pulldown_to_16(tree, 2, descr);
         }
         tl_assert(descr & TREE_DESCR_16_1);
         tree[3] = tree[2];
         descr &= ~TREE_DESCR_16_1;
         descr |= (TREE_DESCR_8_3 | TREE_DESCR_8_2);
         break;
      case 4: case 5:
         if (!(descr & TREE_DESCR_16_2)) {
            descr = pulldown_to_16(tree, 4, descr);
         }
         tl_assert(descr & TREE_DESCR_16_2);
         tree[5] = tree[4];
         descr &= ~TREE_DESCR_16_2;
         descr |= (TREE_DESCR_8_5 | TREE_DESCR_8_4);
         break;
      case 6: case 7:
         if (!(descr & TREE_DESCR_16_3)) {
            descr = pulldown_to_16(tree, 6, descr);
         }
         tl_assert(descr & TREE_DESCR_16_3);
         tree[7] = tree[6];
         descr &= ~TREE_DESCR_16_3;
         descr |= (TREE_DESCR_8_7 | TREE_DESCR_8_6);
         break;
      default:
         tl_assert(0);
   }
   return descr;
}


static UShort pullup_descr_to_16 ( UShort descr, UWord toff ) {
   UShort mask;
   switch (toff) {
      case 0:
         mask = TREE_DESCR_8_1 | TREE_DESCR_8_0;
         tl_assert( (descr & mask) == mask );
         descr &= ~mask;
         descr |= TREE_DESCR_16_0;
         break;
      case 2:
         mask = TREE_DESCR_8_3 | TREE_DESCR_8_2;
         tl_assert( (descr & mask) == mask );
         descr &= ~mask;
         descr |= TREE_DESCR_16_1;
         break;
      case 4:
         mask = TREE_DESCR_8_5 | TREE_DESCR_8_4;
         tl_assert( (descr & mask) == mask );
         descr &= ~mask;
         descr |= TREE_DESCR_16_2;
         break;
      case 6:
         mask = TREE_DESCR_8_7 | TREE_DESCR_8_6;
         tl_assert( (descr & mask) == mask );
         descr &= ~mask;
         descr |= TREE_DESCR_16_3;
         break;
      default:
         tl_assert(0);
   }
   return descr;
}

static UShort pullup_descr_to_32 ( UShort descr, UWord toff ) {
   UShort mask;
   switch (toff) {
      case 0:
         if (!(descr & TREE_DESCR_16_0))
            descr = pullup_descr_to_16(descr, 0);
         if (!(descr & TREE_DESCR_16_1))
            descr = pullup_descr_to_16(descr, 2);
         mask = TREE_DESCR_16_1 | TREE_DESCR_16_0;
         tl_assert( (descr & mask) == mask );
         descr &= ~mask;
         descr |= TREE_DESCR_32_0;
         break;
      case 4:
         if (!(descr & TREE_DESCR_16_2))
            descr = pullup_descr_to_16(descr, 4);
         if (!(descr & TREE_DESCR_16_3))
            descr = pullup_descr_to_16(descr, 6);
         mask = TREE_DESCR_16_3 | TREE_DESCR_16_2;
         tl_assert( (descr & mask) == mask );
         descr &= ~mask;
         descr |= TREE_DESCR_32_1;
         break;
      default:
         tl_assert(0);
   }
   return descr;
}

static Bool valid_value_is_above_me_32 ( UShort descr, UWord toff ) {
   switch (toff) {
      case 0: case 4:
         return 0 != (descr & TREE_DESCR_64);
      default:
         tl_assert(0);
   }
}

static Bool valid_value_is_below_me_16 ( UShort descr, UWord toff ) {
   switch (toff) {
      case 0:
         return 0 != (descr & (TREE_DESCR_8_1 | TREE_DESCR_8_0));
      case 2:
         return 0 != (descr & (TREE_DESCR_8_3 | TREE_DESCR_8_2));
      case 4:
         return 0 != (descr & (TREE_DESCR_8_5 | TREE_DESCR_8_4));
      case 6:
         return 0 != (descr & (TREE_DESCR_8_7 | TREE_DESCR_8_6));
      default:
         tl_assert(0);
   }
}


static void zsm_flush_cache ( void )
{
   shmem__flush_and_invalidate_scache();
}


static void zsm_init ( void )
{
   tl_assert( sizeof(UWord) == sizeof(Addr) );

   tl_assert(map_shmem == NULL);
   map_shmem = VG_(newFM)( HG_(zalloc), "libhb.zsm_init.1 (map_shmem)",
                           HG_(free), 
                           NULL);
   
   tl_assert(!is_valid_scache_tag(1));
   for (UWord wix = 0; wix < N_WAY_NENT; wix++) {
      cache_shmem.tags0[wix] = 1;
   }

   LineF_pool_allocator = VG_(newPA) (
                             sizeof(LineF),
                             (4 * 1024 * 1024 - 200)/sizeof(LineF),
                             HG_(zalloc),
                             "libhb.LineF_storage.pool",
                             HG_(free)
                          );

   
   tl_assert(0 == (N_SECMAP_ARANGE % N_LINE_ARANGE));
   
   tl_assert(0 == (N_LINE_ARANGE % 8));
}






static ThrID Thr__to_ThrID   ( Thr*  thr   ); 
static Thr*  Thr__from_ThrID ( ThrID thrid ); 

__attribute__((noreturn))
static void scalarts_limitations_fail_NORETURN ( Bool due_to_nThrs )
{
   if (due_to_nThrs) {
      const HChar* s =
         "\n"
         "Helgrind: cannot continue, run aborted: too many threads.\n"
         "Sorry.  Helgrind can only handle programs that create\n"
         "%'llu or fewer threads over their entire lifetime.\n"
         "\n";
      VG_(umsg)(s, (ULong)(ThrID_MAX_VALID - 1024));
   } else {
      const HChar* s =
         "\n"
         "Helgrind: cannot continue, run aborted: too many\n"
         "synchronisation events.  Sorry. Helgrind can only handle\n"
         "programs which perform %'llu or fewer\n"
         "inter-thread synchronisation events (locks, unlocks, etc).\n"
         "\n";
      VG_(umsg)(s, (1ULL << SCALARTS_N_TYMBITS) - 1);
   }
   VG_(exit)(1);
   
   tl_assert(0); 
}


static XArray*  verydead_thread_table_not_pruned = NULL;
static XArray*  verydead_thread_table = NULL;

static Int cmp__ThrID ( const void* v1, const void* v2 ) {
   ThrID id1 = *(const ThrID*)v1;
   ThrID id2 = *(const ThrID*)v2;
   if (id1 < id2) return -1;
   if (id1 > id2) return 1;
   return 0;
}

static void verydead_thread_tables_init ( void )
{
   tl_assert(!verydead_thread_table);
   tl_assert(!verydead_thread_table_not_pruned);
   verydead_thread_table
     = VG_(newXA)( HG_(zalloc),
                   "libhb.verydead_thread_table_init.1",
                   HG_(free), sizeof(ThrID) );
   VG_(setCmpFnXA)(verydead_thread_table, cmp__ThrID);
   verydead_thread_table_not_pruned
     = VG_(newXA)( HG_(zalloc),
                   "libhb.verydead_thread_table_init.2",
                   HG_(free), sizeof(ThrID) );
   VG_(setCmpFnXA)(verydead_thread_table_not_pruned, cmp__ThrID);
}

static void verydead_thread_table_sort_and_check (XArray* thrids)
{
   UWord i;

   VG_(sortXA)( thrids );
   
   UWord nBT = VG_(sizeXA)( thrids );
   if (nBT > 0) {
      ThrID thrid1, thrid2;
      thrid2 = *(ThrID*)VG_(indexXA)( thrids, 0 );
      for (i = 1; i < nBT; i++) {
         thrid1 = thrid2;
         thrid2 = *(ThrID*)VG_(indexXA)( thrids, i );
         tl_assert(thrid1 < thrid2);
      }
   }
   
}

typedef
   struct {
      VtsID    id;
      UInt     usedTS;
      UInt     sizeTS;
      ScalarTS ts[0];
   }
   VTS;

static VTS* VTS__new ( const HChar* who, UInt sizeTS );

static VTS* VTS__clone ( const HChar* who, VTS* vts );

static VTS* VTS__subtract ( const HChar* who, VTS* vts, XArray* thridsToDel );

static void VTS__delete ( VTS* vts );

static void VTS__singleton ( VTS* out, Thr* thr, ULong tym );

static void VTS__tick ( VTS* out, Thr* me, VTS* vts );

static void VTS__join ( VTS* out, VTS* a, VTS* b );

static UInt VTS__cmpLEQ ( VTS* a, VTS* b );

static Word VTS__cmp_structural ( VTS* a, VTS* b );

static void VTS__show ( const VTS* vts );

static ULong VTS__indexAt_SLOW ( VTS* vts, Thr* idx );

static void VTS__declare_thread_very_dead ( Thr* idx );


static Bool is_sane_VTS ( VTS* vts )
{
   UWord     i, n;
   ScalarTS  *st1, *st2;
   if (!vts) return False;
   if (vts->usedTS > vts->sizeTS) return False;
   n = vts->usedTS;
   if (n == 1) {
      st1 = &vts->ts[0];
      if (st1->tym == 0)
         return False;
   }
   else
   if (n >= 2) {
      for (i = 0; i < n-1; i++) {
         st1 = &vts->ts[i];
         st2 = &vts->ts[i+1];
         if (st1->thrid >= st2->thrid)
            return False;
         if (st1->tym == 0 || st2->tym == 0)
            return False;
      }
   }
   return True;
}


static VTS* VTS__new ( const HChar* who, UInt sizeTS )
{
   VTS* vts = HG_(zalloc)(who, sizeof(VTS) + (sizeTS+1) * sizeof(ScalarTS));
   tl_assert(vts->usedTS == 0);
   vts->sizeTS = sizeTS;
   *(ULong*)(&vts->ts[sizeTS]) = 0x0ddC0ffeeBadF00dULL;
   return vts;
}

static VTS* VTS__clone ( const HChar* who, VTS* vts )
{
   tl_assert(vts);
   tl_assert( *(ULong*)(&vts->ts[vts->sizeTS]) == 0x0ddC0ffeeBadF00dULL);
   UInt nTS = vts->usedTS;
   VTS* clone = VTS__new(who, nTS);
   clone->id = vts->id;
   clone->sizeTS = nTS;
   clone->usedTS = nTS;
   UInt i;
   for (i = 0; i < nTS; i++) {
      clone->ts[i] = vts->ts[i];
   }
   tl_assert( *(ULong*)(&clone->ts[clone->sizeTS]) == 0x0ddC0ffeeBadF00dULL);
   return clone;
}


static VTS* VTS__subtract ( const HChar* who, VTS* vts, XArray* thridsToDel )
{
   UInt i, j;
   tl_assert(vts);
   tl_assert(thridsToDel);
   tl_assert( *(ULong*)(&vts->ts[vts->sizeTS]) == 0x0ddC0ffeeBadF00dULL);
   UInt nTS = vts->usedTS;
   
   UInt nReq = nTS;
   for (i = 0; i < nTS; i++) {
      ThrID thrid = vts->ts[i].thrid;
      if (VG_(lookupXA)(thridsToDel, &thrid, NULL, NULL))
         nReq--;
   }
   tl_assert(nReq <= nTS);
   
   VTS* res = VTS__new(who, nReq);
   j = 0;
   for (i = 0; i < nTS; i++) {
      ThrID thrid = vts->ts[i].thrid;
      if (VG_(lookupXA)(thridsToDel, &thrid, NULL, NULL))
         continue;
      res->ts[j++] = vts->ts[i];
   }
   tl_assert(j == nReq);
   tl_assert(j == res->sizeTS);
   res->usedTS = j;
   tl_assert( *(ULong*)(&res->ts[j]) == 0x0ddC0ffeeBadF00dULL);
   return res;
}


static void VTS__delete ( VTS* vts )
{
   tl_assert(vts);
   tl_assert(vts->usedTS <= vts->sizeTS);
   tl_assert( *(ULong*)(&vts->ts[vts->sizeTS]) == 0x0ddC0ffeeBadF00dULL);
   HG_(free)(vts);
}


static void VTS__singleton ( VTS* out, Thr* thr, ULong tym )
{
   tl_assert(thr);
   tl_assert(tym >= 1);
   tl_assert(out);
   tl_assert(out->usedTS == 0);
   tl_assert(out->sizeTS >= 1);
   UInt hi = out->usedTS++;
   out->ts[hi].thrid = Thr__to_ThrID(thr);
   out->ts[hi].tym   = tym;
}


static void VTS__tick ( VTS* out, Thr* me, VTS* vts )
{
   UInt      i, n;
   ThrID     me_thrid;
   Bool      found = False;

   stats__vts__tick++;

   tl_assert(out);
   tl_assert(out->usedTS == 0);
   if (vts->usedTS >= ThrID_MAX_VALID)
      scalarts_limitations_fail_NORETURN( True );
   tl_assert(out->sizeTS >= 1 + vts->usedTS);

   tl_assert(me);
   me_thrid = Thr__to_ThrID(me);
   tl_assert(is_sane_VTS(vts));
   n = vts->usedTS;

   
   for (i = 0; i < n; i++) {
      ScalarTS* here = &vts->ts[i];
      if (UNLIKELY(here->thrid >= me_thrid))
         break;
      UInt hi = out->usedTS++;
      out->ts[hi] = *here;
   }

   tl_assert(i >= 0 && i <= n);
   if (i == n) { 
      UInt hi = out->usedTS++;
      out->ts[hi].thrid = me_thrid;
      out->ts[hi].tym   = 1;
   } else {
      
      ScalarTS* here = &vts->ts[i];
      if (me_thrid == here->thrid) { 
         if (UNLIKELY(here->tym >= (1ULL << SCALARTS_N_TYMBITS) - 2ULL)) {
            
            scalarts_limitations_fail_NORETURN( False );
         }
         UInt hi = out->usedTS++;
         out->ts[hi].thrid = here->thrid;
         out->ts[hi].tym   = here->tym + 1;
         i++;
         found = True;
      } else { 
         UInt hi = out->usedTS++;
         out->ts[hi].thrid = me_thrid;
         out->ts[hi].tym   = 1;
      }
      
      for (; i < n; i++) {
         ScalarTS* here2 = &vts->ts[i];
         UInt hi = out->usedTS++;
         out->ts[hi] = *here2;
      }
   }

   tl_assert(is_sane_VTS(out));
   tl_assert(out->usedTS == vts->usedTS + (found ? 0 : 1));
   tl_assert(out->usedTS <= out->sizeTS);
}


static void VTS__join ( VTS* out, VTS* a, VTS* b )
{
   UInt     ia, ib, useda, usedb;
   ULong    tyma, tymb, tymMax;
   ThrID    thrid;
   UInt     ncommon = 0;

   stats__vts__join++;

   tl_assert(a);
   tl_assert(b);
   useda = a->usedTS;
   usedb = b->usedTS;

   tl_assert(out);
   tl_assert(out->usedTS == 0);
   if (useda + usedb >= ThrID_MAX_VALID)
      scalarts_limitations_fail_NORETURN( True );
   tl_assert(out->sizeTS >= useda + usedb);

   ia = ib = 0;

   while (1) {

      tl_assert(ia >= 0 && ia <= useda);
      tl_assert(ib >= 0 && ib <= usedb);

      if        (ia == useda && ib == usedb) {
         
         break;

      } else if (ia == useda && ib != usedb) {
         
         ScalarTS* tmpb = &b->ts[ib];
         thrid = tmpb->thrid;
         tyma  = 0;
         tymb  = tmpb->tym;
         ib++;

      } else if (ia != useda && ib == usedb) {
         
         ScalarTS* tmpa = &a->ts[ia];
         thrid = tmpa->thrid;
         tyma  = tmpa->tym;
         tymb  = 0;
         ia++;

      } else {
         
         ScalarTS* tmpa = &a->ts[ia];
         ScalarTS* tmpb = &b->ts[ib];
         if (tmpa->thrid < tmpb->thrid) {
            
            thrid = tmpa->thrid;
            tyma  = tmpa->tym;
            tymb  = 0;
            ia++;
         } else if (tmpa->thrid > tmpb->thrid) {
            
            thrid = tmpb->thrid;
            tyma  = 0;
            tymb  = tmpb->tym;
            ib++;
         } else {
            
            tl_assert(tmpa->thrid == tmpb->thrid);
            thrid = tmpa->thrid; 
            tyma  = tmpa->tym;
            tymb  = tmpb->tym;
            ia++;
            ib++;
            ncommon++;
         }
      }

      tymMax = tyma > tymb ? tyma : tymb;
      if (tymMax > 0) {
         UInt hi = out->usedTS++;
         out->ts[hi].thrid = thrid;
         out->ts[hi].tym   = tymMax;
      }

   }

   tl_assert(is_sane_VTS(out));
   tl_assert(out->usedTS <= out->sizeTS);
   tl_assert(out->usedTS == useda + usedb - ncommon);
}


static UInt VTS__cmpLEQ ( VTS* a, VTS* b )
{
   Word  ia, ib, useda, usedb;
   ULong tyma, tymb;

   stats__vts__cmpLEQ++;

   tl_assert(a);
   tl_assert(b);
   useda = a->usedTS;
   usedb = b->usedTS;

   ia = ib = 0;

   while (1) {

      ThrID thrid;

      tl_assert(ia >= 0 && ia <= useda);
      tl_assert(ib >= 0 && ib <= usedb);

      if        (ia == useda && ib == usedb) {
         
         break;

      } else if (ia == useda && ib != usedb) {
         
         ScalarTS* tmpb = &b->ts[ib];
         tyma  = 0;
         tymb  = tmpb->tym;
         thrid = tmpb->thrid;
         ib++;

      } else if (ia != useda && ib == usedb) {
         
         ScalarTS* tmpa = &a->ts[ia];
         tyma  = tmpa->tym;
         thrid = tmpa->thrid;
         tymb  = 0;
         ia++;

      } else {
         
         ScalarTS* tmpa = &a->ts[ia];
         ScalarTS* tmpb = &b->ts[ib];
         if (tmpa->thrid < tmpb->thrid) {
            
            tyma  = tmpa->tym;
            thrid = tmpa->thrid;
            tymb  = 0;
            ia++;
         }
         else
         if (tmpa->thrid > tmpb->thrid) {
            
            tyma  = 0;
            tymb  = tmpb->tym;
            thrid = tmpb->thrid;
            ib++;
         } else {
            
            tl_assert(tmpa->thrid == tmpb->thrid);
            tyma  = tmpa->tym;
            thrid = tmpa->thrid;
            tymb  = tmpb->tym;
            ia++;
            ib++;
         }
      }

      if (tyma > tymb) {
         tl_assert(thrid >= 1024);
         return thrid;
      }
   }

   return 0; 
}


Word VTS__cmp_structural ( VTS* a, VTS* b )
{
   Word     i;
   Word     useda = 0,    usedb = 0;
   ScalarTS *ctsa = NULL, *ctsb = NULL;

   stats__vts__cmp_structural++;

   tl_assert(a);
   tl_assert(b);

   ctsa = &a->ts[0]; useda = a->usedTS;
   ctsb = &b->ts[0]; usedb = b->usedTS;

   if (LIKELY(useda == usedb)) {
      ScalarTS *tmpa = NULL, *tmpb = NULL;
      stats__vts__cmp_structural_slow++;
      for (i = 0; i < useda; i++) {
         tmpa = &ctsa[i];
         tmpb = &ctsb[i];
         if (LIKELY(tmpa->tym == tmpb->tym
                    && tmpa->thrid == tmpb->thrid))
            continue;
         else
            break;
      }
      if (UNLIKELY(i == useda)) {
         
         return 0;
      } else {
         tl_assert(i >= 0 && i < useda);
         if (tmpa->tym < tmpb->tym) return -1;
         if (tmpa->tym > tmpb->tym) return 1;
         if (tmpa->thrid < tmpb->thrid) return -1;
         if (tmpa->thrid > tmpb->thrid) return 1;
         
      }
      
      tl_assert(0);
   }

   if (useda < usedb) return -1;
   if (useda > usedb) return 1;
   
   tl_assert(0);
}


static void VTS__show ( const VTS* vts )
{
   Word      i, n;
   tl_assert(vts);

   VG_(printf)("[");
   n =  vts->usedTS;
   for (i = 0; i < n; i++) {
      const ScalarTS *st = &vts->ts[i];
      VG_(printf)(i < n-1 ? "%u:%llu " : "%u:%llu", st->thrid, (ULong)st->tym);
   }
   VG_(printf)("]");
}


ULong VTS__indexAt_SLOW ( VTS* vts, Thr* idx )
{
   UWord i, n;
   ThrID idx_thrid = Thr__to_ThrID(idx);
   stats__vts__indexat_slow++;
   tl_assert(vts);
   n = vts->usedTS;
   for (i = 0; i < n; i++) {
      ScalarTS* st = &vts->ts[i];
      if (st->thrid == idx_thrid)
         return st->tym;
   }
   return 0;
}


static void VTS__declare_thread_very_dead ( Thr* thr )
{
   if (0) VG_(printf)("VTQ:  tae %p\n", thr);

   tl_assert(thr->llexit_done);
   tl_assert(thr->joinedwith_done);

   ThrID nyu;
   nyu = Thr__to_ThrID(thr);
   VG_(addToXA)( verydead_thread_table_not_pruned, &nyu );

   VtsID__rcdec(thr->viR);
   VtsID__rcdec(thr->viW);
   thr->viR = VtsID_INVALID;
   thr->viW = VtsID_INVALID;
}








static WordFM*  vts_set = NULL;

static void vts_set_init ( void )
{
   tl_assert(!vts_set);
   vts_set = VG_(newFM)( HG_(zalloc), "libhb.vts_set_init.1",
                         HG_(free),
                         (Word(*)(UWord,UWord))VTS__cmp_structural );
}

static Bool vts_set__find__or__clone_and_add ( VTS** res, VTS* cand )
{
   UWord keyW, valW;
   stats__vts_set__focaa++;
   tl_assert(cand->id == VtsID_INVALID);
   
   if (VG_(lookupFM)( vts_set, &keyW, &valW, (UWord)cand )) {
      
      tl_assert(valW == 0);
      
      tl_assert(keyW != (UWord)cand);
      *res = (VTS*)keyW;
      return True;
   } else {
      
      stats__vts_set__focaa_a++;
      VTS* clone = VTS__clone( "libhb.vts_set_focaa.1", cand );
      tl_assert(clone != cand);
      VG_(addToFM)( vts_set, (UWord)clone, 0 );
      *res = clone;
      return False;
   }
}



static void VtsID__invalidate_caches ( void ); 

typedef
   struct {
      VTS*  vts;      
      UWord rc;       
      union {
         VtsID freelink; 
         VtsID remap;    
      } u; 
   }
   VtsTE;

static XArray*  vts_tab = NULL;

static VtsID vts_tab_freelist = VtsID_INVALID;

static Word vts_next_GC_at = 1000;

static void vts_tab_init ( void )
{
   vts_tab = VG_(newXA)( HG_(zalloc), "libhb.vts_tab_init.1",
                         HG_(free), sizeof(VtsTE) );
   vts_tab_freelist = VtsID_INVALID;
}

static void add_to_free_list ( VtsID ii )
{
   VtsTE* ie = VG_(indexXA)( vts_tab, ii );
   tl_assert(ie->vts == NULL);
   tl_assert(ie->rc == 0);
   tl_assert(ie->u.freelink == VtsID_INVALID);
   ie->u.freelink = vts_tab_freelist;
   vts_tab_freelist = ii;
}

static VtsID get_from_free_list ( void )
{
   VtsID  ii;
   VtsTE* ie;
   if (vts_tab_freelist == VtsID_INVALID)
      return VtsID_INVALID;
   ii = vts_tab_freelist;
   ie = VG_(indexXA)( vts_tab, ii );
   tl_assert(ie->vts == NULL);
   tl_assert(ie->rc == 0);
   vts_tab_freelist = ie->u.freelink;
   return ii;
}

static VtsID get_new_VtsID ( void )
{
   VtsID ii;
   VtsTE te;
   ii = get_from_free_list();
   if (ii != VtsID_INVALID)
      return ii;
   te.vts = NULL;
   te.rc = 0;
   te.u.freelink = VtsID_INVALID;
   ii = (VtsID)VG_(addToXA)( vts_tab, &te );
   return ii;
}


static void VtsID__rcinc ( VtsID ii )
{
   VtsTE* ie;
   
   ie = VG_(indexXA)( vts_tab, ii );
   tl_assert(ie->vts); 
   tl_assert(ie->rc < ~0UL); 
   tl_assert(ie->vts->id == ii);
   ie->rc++;
}

static void VtsID__rcdec ( VtsID ii )
{
   VtsTE* ie;
   
   ie = VG_(indexXA)( vts_tab, ii );
   tl_assert(ie->vts); 
   tl_assert(ie->rc > 0); 
   tl_assert(ie->vts->id == ii);
   ie->rc--;
}


static VtsID vts_tab__find__or__clone_and_add ( VTS* cand )
{
   VTS* in_tab = NULL;
   tl_assert(cand->id == VtsID_INVALID);
   Bool already_have = vts_set__find__or__clone_and_add( &in_tab, cand );
   tl_assert(in_tab);
   if (already_have) {
      
      VtsTE* ie;
      tl_assert(in_tab->id != VtsID_INVALID);
      ie = VG_(indexXA)( vts_tab, in_tab->id );
      tl_assert(ie->vts == in_tab);
      return in_tab->id;
   } else {
      VtsID  ii = get_new_VtsID();
      VtsTE* ie = VG_(indexXA)( vts_tab, ii );
      ie->vts = in_tab;
      ie->rc = 0;
      ie->u.freelink = VtsID_INVALID;
      in_tab->id = ii;
      return ii;
   }
}


static void show_vts_stats ( const HChar* caller )
{
   UWord nSet, nTab, nLive;
   ULong totrc;
   UWord n, i;
   nSet = VG_(sizeFM)( vts_set );
   nTab = VG_(sizeXA)( vts_tab );
   totrc = 0;
   nLive = 0;
   n = VG_(sizeXA)( vts_tab );
   for (i = 0; i < n; i++) {
      VtsTE* ie = VG_(indexXA)( vts_tab, i );
      if (ie->vts) {
         nLive++;
         totrc += (ULong)ie->rc;
      } else {
         tl_assert(ie->rc == 0);
      }
   }
   VG_(printf)("  show_vts_stats %s\n", caller);
   VG_(printf)("    vts_tab size %4lu\n", nTab);
   VG_(printf)("    vts_tab live %4lu\n", nLive);
   VG_(printf)("    vts_set size %4lu\n", nSet);
   VG_(printf)("        total rc %4llu\n", totrc);
}



static
void remap_VtsID ( XArray*  old_tab,
                   XArray*  new_tab,
                   VtsID* ii )
{
   VtsTE *old_te, *new_te;
   VtsID old_id, new_id;
   old_id = *ii;
   old_te = VG_(indexXA)( old_tab, old_id );
   old_te->rc--;
   new_id = old_te->u.remap;
   new_te = VG_(indexXA)( new_tab, new_id );
   new_te->rc++;
   *ii = new_id;
}

static
void remap_VtsIDs_in_SVal ( XArray*  old_tab,
                            XArray*  new_tab,
                            SVal* s )
{
   SVal old_sv, new_sv;
   old_sv = *s;
   if (SVal__isC(old_sv)) {
      VtsID rMin, wMin;
      rMin = SVal__unC_Rmin(old_sv);
      wMin = SVal__unC_Wmin(old_sv);
      remap_VtsID( old_tab, new_tab, &rMin );
      remap_VtsID( old_tab, new_tab, &wMin );
      new_sv = SVal__mkC( rMin, wMin );
      *s = new_sv;
  }
}


__attribute__((noinline))
static void vts_tab__do_GC ( Bool show_stats )
{
   UWord i, nTab, nLive, nFreed;

   
   
   tl_assert(vts_tab_freelist == VtsID_INVALID);

   VtsID__invalidate_caches();

   
   zsm_flush_cache();

   nTab = VG_(sizeXA)( vts_tab );

   if (show_stats) {
      VG_(printf)("<<GC begins at vts_tab size %lu>>\n", nTab);
      show_vts_stats("before GC");
   }

   nFreed = 0;
   for (i = 0; i < nTab; i++) {
      Bool present;
      UWord oldK = 0, oldV = 12345;
      VtsTE* te = VG_(indexXA)( vts_tab, i );
      if (te->vts == NULL) {
         tl_assert(te->rc == 0);
         continue; 
      }
      if (te->rc > 0)
         continue; 
      
      tl_assert(te->vts->id == i);
      
      present = VG_(delFromFM)( vts_set,
                                &oldK, &oldV, (UWord)te->vts );
      tl_assert(present); 
      tl_assert(oldV == 0); 
      tl_assert(oldK == (UWord)te->vts); 
      
      VTS__delete(te->vts);
      te->vts = NULL;
      
      tl_assert(te->u.freelink == VtsID_INVALID); 
      add_to_free_list( i );
      nFreed++;
   }

   tl_assert(nFreed <= nTab);
   nLive = nTab - nFreed;
   tl_assert(nLive >= 0 && nLive <= nTab);
   vts_next_GC_at = 2 * nLive;
   if (vts_next_GC_at < nTab)
      vts_next_GC_at = nTab;

   if (show_stats) {
      show_vts_stats("after GC");
      VG_(printf)("<<GC ends, next gc at %ld>>\n", vts_next_GC_at);
   }

   stats__vts_tab_GC++;
   if (VG_(clo_stats)) {
      tl_assert(nTab > 0);
      VG_(message)(Vg_DebugMsg,
                   "libhb: VTS GC: #%lu  old size %lu  live %lu  (%2llu%%)\n",
                   stats__vts_tab_GC, 
                   nTab, nLive, (100ULL * (ULong)nLive) / (ULong)nTab);
   }
   

   static UInt pruning_auto_ctr = 0; 

   Bool do_pruning = False;
   switch (HG_(clo_vts_pruning)) {
      case 0: 
         break;
      case 1: 
         do_pruning = (++pruning_auto_ctr % 5) == 0;
         break;
      case 2: 
         do_pruning = True;
         break;
      default:
         tl_assert(0);
   }

   if (!do_pruning)
      return;
   if (VG_(sizeXA)( verydead_thread_table_not_pruned) == 0)
      return;

   
   verydead_thread_table_sort_and_check (verydead_thread_table_not_pruned);


   XArray*  new_tab
      = VG_(newXA)( HG_(zalloc), "libhb.vts_tab__do_GC.new_tab",
                    HG_(free), sizeof(VtsTE) );

   
   WordFM* new_set
      = VG_(newFM)( HG_(zalloc), "libhb.vts_tab__do_GC.new_set",
                    HG_(free),
                    (Word(*)(UWord,UWord))VTS__cmp_structural );


   UWord nBeforePruning = 0, nAfterPruning = 0;
   UWord nSTSsBefore = 0, nSTSsAfter = 0;
   VtsID new_VtsID_ctr = 0;

   for (i = 0; i < nTab; i++) {

      
      VtsTE* old_te  = VG_(indexXA)( vts_tab, i );
      VTS*   old_vts = old_te->vts;

      
      if (old_te->rc == 0) {
         tl_assert(old_vts == NULL);
         continue;
      }
      tl_assert(old_te->u.remap == VtsID_INVALID);
      tl_assert(old_vts != NULL);
      tl_assert(old_vts->id == i);
      tl_assert(old_vts->ts != NULL);

      
      nBeforePruning++;
      nSTSsBefore += old_vts->usedTS;
      VTS* new_vts = VTS__subtract("libhb.vts_tab__do_GC.new_vts",
                                   old_vts, verydead_thread_table_not_pruned);
      tl_assert(new_vts->sizeTS == new_vts->usedTS);
      tl_assert(*(ULong*)(&new_vts->ts[new_vts->usedTS])
                == 0x0ddC0ffeeBadF00dULL);

      UWord oldK = 0, oldV = 12345;
      Bool  present = VG_(delFromFM)( vts_set,
                                      &oldK, &oldV, (UWord)old_vts );
      tl_assert(present); 
      tl_assert(oldV == 0); 
      tl_assert(oldK == (UWord)old_vts); 
      
      VTS__delete(old_vts);
      old_te->vts = NULL;
      old_vts = NULL;

      

      VTS*  identical_version = NULL;
      UWord valW = 12345;
      if (VG_(lookupFM)(new_set, (UWord*)&identical_version, &valW,
                        (UWord)new_vts)) {
         
         tl_assert(valW == 0);
         tl_assert(identical_version != NULL);
         tl_assert(identical_version != new_vts);
         VTS__delete(new_vts);
         new_vts = identical_version;
         tl_assert(new_vts->id != VtsID_INVALID);
      } else {
         tl_assert(valW == 12345);
         tl_assert(identical_version == NULL);
         new_vts->id = new_VtsID_ctr++;
         Bool b = VG_(addToFM)(new_set, (UWord)new_vts, 0);
         tl_assert(!b);
         VtsTE new_te;
         new_te.vts      = new_vts;
         new_te.rc       = 0;
         new_te.u.freelink = VtsID_INVALID;
         Word j = VG_(addToXA)( new_tab, &new_te );
         tl_assert(j <= i);
         tl_assert(j == new_VtsID_ctr - 1);
         
         nAfterPruning++;
         nSTSsAfter += new_vts->usedTS;
      }
      old_te->u.remap = new_vts->id;

   } 

   {
      UWord nBT = VG_(sizeXA)( verydead_thread_table_not_pruned);

      for (i = 0; i < nBT; i++) {
         ThrID thrid = 
            *(ThrID*)VG_(indexXA)( verydead_thread_table_not_pruned, i );
         VG_(addToXA)( verydead_thread_table, &thrid );
      }
      verydead_thread_table_sort_and_check (verydead_thread_table);
      VG_(dropHeadXA) (verydead_thread_table_not_pruned, nBT);
   }

   tl_assert( VG_(sizeFM)(vts_set) == 0 );

   

   UWord secmapW = 0;
   VG_(initIterFM)( map_shmem );
   while (VG_(nextIterFM)( map_shmem, NULL, &secmapW )) {
      UWord   j;
      SecMap* sm = (SecMap*)secmapW;
      tl_assert(sm->magic == SecMap_MAGIC);
      
      for (i = 0; i < N_SECMAP_ZLINES; i++) {
         LineZ* lineZ = &sm->linesZ[i];
         if (lineZ->dict[0] != SVal_INVALID) {
            for (j = 0; j < 4; j++)
               remap_VtsIDs_in_SVal(vts_tab, new_tab, &lineZ->dict[j]);
         } else {
            LineF* lineF = SVal2Ptr (lineZ->dict[1]);
            for (j = 0; j < N_LINE_ARANGE; j++)
               remap_VtsIDs_in_SVal(vts_tab, new_tab, &lineF->w64s[j]);
         }
      }
   }
   VG_(doneIterFM)( map_shmem );

   Thread* hgthread = get_admin_threads();
   tl_assert(hgthread);
   while (hgthread) {
      Thr* hbthr = hgthread->hbthr;
      tl_assert(hbthr);
      if (hbthr->llexit_done && hbthr->joinedwith_done) {
         tl_assert(hbthr->viR == VtsID_INVALID);
         tl_assert(hbthr->viW == VtsID_INVALID);
         hgthread = hgthread->admin;
         continue;
      }
      remap_VtsID( vts_tab, new_tab, &hbthr->viR );
      remap_VtsID( vts_tab, new_tab, &hbthr->viW );
      hgthread = hgthread->admin;
   }

   
   SO* so = admin_SO;
   while (so) {
      if (so->viR != VtsID_INVALID)
         remap_VtsID( vts_tab, new_tab, &so->viR );
      if (so->viW != VtsID_INVALID)
         remap_VtsID( vts_tab, new_tab, &so->viW );
      so = so->admin_next;
   }

   for (i = 0; i < nTab; i++) {
      VtsTE* te = VG_(indexXA)( vts_tab, i );
      tl_assert(te->vts == NULL);
      tl_assert(te->rc == 0);
   }

   
   VG_(deleteFM)(vts_set, NULL, NULL);
   vts_set = new_set;
   VG_(deleteXA)( vts_tab );
   vts_tab = new_tab;

   vts_tab_freelist = VtsID_INVALID;

   

   
   tl_assert( VG_(sizeXA)( vts_tab ) == VG_(sizeFM)( vts_set ));

   UWord wordK = 0, wordV = 0;
   VG_(initIterFM)( vts_set );
   while (VG_(nextIterFM)( vts_set, &wordK, &wordV )) {
      tl_assert(wordK != 0);
      tl_assert(wordV == 0);
      VTS* vts = (VTS*)wordK;
      tl_assert(vts->id != VtsID_INVALID);
      VtsTE* te = VG_(indexXA)( vts_tab, vts->id );
      tl_assert(te->vts == vts);
   }
   VG_(doneIterFM)( vts_set );

   nTab = VG_(sizeXA)( vts_tab );
   for (i = 0; i < nTab; i++) {
      VtsTE* te = VG_(indexXA)( vts_tab, i );
      tl_assert(te->vts);
      tl_assert(te->vts->id == i);
      tl_assert(te->rc > 0); 
      tl_assert(te->u.freelink == VtsID_INVALID); 
      
   }

   
   stats__vts_pruning++;
   if (VG_(clo_stats)) {
      tl_assert(nTab > 0);
      VG_(message)(
         Vg_DebugMsg,
         "libhb: VTS PR: #%lu  before %lu (avg sz %lu)  "
            "after %lu (avg sz %lu)\n",
         stats__vts_pruning,
         nBeforePruning, nSTSsBefore / (nBeforePruning ? nBeforePruning : 1),
         nAfterPruning, nSTSsAfter / (nAfterPruning ? nAfterPruning : 1)
      );
   }
   
}



static VTS* temp_max_sized_VTS = NULL;

static ULong stats__cmpLEQ_queries = 0;
static ULong stats__cmpLEQ_misses  = 0;
static ULong stats__join2_queries  = 0;
static ULong stats__join2_misses   = 0;

static inline UInt ROL32 ( UInt w, Int n ) {
   w = (w << n) | (w >> (32-n));
   return w;
}
static inline UInt hash_VtsIDs ( VtsID vi1, VtsID vi2, UInt nTab ) {
   UInt hash = ROL32(vi1,19) ^ ROL32(vi2,13);
   return hash % nTab;
}

#define N_CMPLEQ_CACHE 1023
static
   struct { VtsID vi1; VtsID vi2; Bool leq; }
   cmpLEQ_cache[N_CMPLEQ_CACHE];

#define N_JOIN2_CACHE 1023
static
   struct { VtsID vi1; VtsID vi2; VtsID res; }
   join2_cache[N_JOIN2_CACHE];

static void VtsID__invalidate_caches ( void ) {
   Int i;
   for (i = 0; i < N_CMPLEQ_CACHE; i++) {
      cmpLEQ_cache[i].vi1 = VtsID_INVALID;
      cmpLEQ_cache[i].vi2 = VtsID_INVALID;
      cmpLEQ_cache[i].leq = False;
   }
   for (i = 0; i < N_JOIN2_CACHE; i++) {
     join2_cache[i].vi1 = VtsID_INVALID;
     join2_cache[i].vi2 = VtsID_INVALID;
     join2_cache[i].res = VtsID_INVALID;
   }
}


static VTS* VtsID__to_VTS ( VtsID vi ) {
   VtsTE* te = VG_(indexXA)( vts_tab, vi );
   tl_assert(te->vts);
   return te->vts;
}

static void VtsID__pp ( VtsID vi ) {
   VTS* vts = VtsID__to_VTS(vi);
   VTS__show( vts );
}

__attribute__((noinline))
static Bool VtsID__cmpLEQ_WRK ( VtsID vi1, VtsID vi2 ) {
   UInt hash;
   Bool leq;
   VTS  *v1, *v2;
   
   tl_assert(vi1 != vi2);
   
   stats__cmpLEQ_queries++;
   hash = hash_VtsIDs(vi1, vi2, N_CMPLEQ_CACHE);
   if (cmpLEQ_cache[hash].vi1 == vi1
       && cmpLEQ_cache[hash].vi2 == vi2)
      return cmpLEQ_cache[hash].leq;
   stats__cmpLEQ_misses++;
   
   v1  = VtsID__to_VTS(vi1);
   v2  = VtsID__to_VTS(vi2);
   leq = VTS__cmpLEQ( v1, v2 ) == 0;
   
   cmpLEQ_cache[hash].vi1 = vi1;
   cmpLEQ_cache[hash].vi2 = vi2;
   cmpLEQ_cache[hash].leq = leq;
   
   return leq;
}
static inline Bool VtsID__cmpLEQ ( VtsID vi1, VtsID vi2 ) {
   return LIKELY(vi1 == vi2)  ? True  : VtsID__cmpLEQ_WRK(vi1, vi2);
}

__attribute__((noinline))
static VtsID VtsID__join2_WRK ( VtsID vi1, VtsID vi2 ) {
   UInt  hash;
   VtsID res;
   VTS   *vts1, *vts2;
   
   tl_assert(vi1 != vi2);
   
   stats__join2_queries++;
   hash = hash_VtsIDs(vi1, vi2, N_JOIN2_CACHE);
   if (join2_cache[hash].vi1 == vi1
       && join2_cache[hash].vi2 == vi2)
      return join2_cache[hash].res;
   stats__join2_misses++;
   
   vts1 = VtsID__to_VTS(vi1);
   vts2 = VtsID__to_VTS(vi2);
   temp_max_sized_VTS->usedTS = 0;
   VTS__join(temp_max_sized_VTS, vts1,vts2);
   res = vts_tab__find__or__clone_and_add(temp_max_sized_VTS);
   
   join2_cache[hash].vi1 = vi1;
   join2_cache[hash].vi2 = vi2;
   join2_cache[hash].res = res;
   
   return res;
}
static inline VtsID VtsID__join2 ( VtsID vi1, VtsID vi2 ) {
   return LIKELY(vi1 == vi2)  ? vi1  : VtsID__join2_WRK(vi1, vi2);
}

static VtsID VtsID__mk_Singleton ( Thr* thr, ULong tym ) {
   temp_max_sized_VTS->usedTS = 0;
   VTS__singleton(temp_max_sized_VTS, thr,tym);
   return vts_tab__find__or__clone_and_add(temp_max_sized_VTS);
}

static VtsID VtsID__tick ( VtsID vi, Thr* idx ) {
   VTS* vts = VtsID__to_VTS(vi);
   temp_max_sized_VTS->usedTS = 0;
   VTS__tick(temp_max_sized_VTS, idx,vts);
   return vts_tab__find__or__clone_and_add(temp_max_sized_VTS);
}

static ULong VtsID__indexAt ( VtsID vi, Thr* idx ) {
   VTS* vts = VtsID__to_VTS(vi);
   return VTS__indexAt_SLOW( vts, idx );
}

static Thr* VtsID__findFirst_notLEQ ( VtsID vi1, VtsID vi2 )
{
   VTS  *vts1, *vts2;
   Thr*  diffthr;
   ThrID diffthrid;
   tl_assert(vi1 != vi2);
   vts1 = VtsID__to_VTS(vi1);
   vts2 = VtsID__to_VTS(vi2);
   tl_assert(vts1 != vts2);
   diffthrid = VTS__cmpLEQ(vts1, vts2);
   diffthr = Thr__from_ThrID(diffthrid);
   tl_assert(diffthr); 
   return diffthr;
}



static void Filter__clear ( Filter* fi, const HChar* who )
{
   UWord i;
   if (0) VG_(printf)("  Filter__clear(%p, %s)\n", fi, who);
   for (i = 0; i < FI_NUM_LINES; i += 8) {
      fi->tags[i+0] = 1; 
      fi->tags[i+1] = 1;
      fi->tags[i+2] = 1;
      fi->tags[i+3] = 1;
      fi->tags[i+4] = 1;
      fi->tags[i+5] = 1;
      fi->tags[i+6] = 1;
      fi->tags[i+7] = 1;
   }
   tl_assert(i == FI_NUM_LINES);
}


static void Filter__clear_1byte ( Filter* fi, Addr a )
{
   Addr    atag   = FI_GET_TAG(a);     
   UWord   lineno = FI_GET_LINENO(a);  
   FiLine* line   = &fi->lines[lineno];
   UWord   loff   = (a - atag) / 8;
   UShort  mask   = 0x3 << (2 * (a & 7));
   
   if (LIKELY( fi->tags[lineno] == atag )) {
      
      UShort  u16  = line->u16s[loff];
      line->u16s[loff] = u16 & ~mask; 
   } else {
      
   }
}

static void Filter__clear_8bytes_aligned ( Filter* fi, Addr a )
{
   Addr    atag   = FI_GET_TAG(a);     
   UWord   lineno = FI_GET_LINENO(a);  
   FiLine* line   = &fi->lines[lineno];
   UWord   loff   = (a - atag) / 8;
   if (LIKELY( fi->tags[lineno] == atag )) {
      line->u16s[loff] = 0;
   } else {
    
   }
}

__attribute__((unused))
static void Filter__clear_range_SLOW ( Filter* fi, Addr a, UWord len )
{
   tl_assert (CHECK_ZSM);

   
   while (UNLIKELY(!VG_IS_8_ALIGNED(a)) && LIKELY(len > 0)) {
      Filter__clear_1byte( fi, a );
      a++;
      len--;
   }
   
   while (len >= 8) {
      Filter__clear_8bytes_aligned( fi, a );
      a += 8;
      len -= 8;
   }
   
   while (UNLIKELY(len > 0)) {
      Filter__clear_1byte( fi, a );
      a++;
      len--;
   }
}

static void Filter__clear_range ( Filter* fi, Addr a, UWord len )
{
#  if CHECK_ZSM > 0
   Filter fi_check = *fi;
   Filter__clear_range_SLOW(&fi_check, a, len);
#  define RETURN goto check_and_return
#  else
#  define RETURN return
#  endif

   Addr    begtag = FI_GET_TAG(a);       

   Addr    end = a + len - 1;
   Addr    endtag = FI_GET_TAG(end); 

   UWord rlen = len; 

   Addr    c = a; 
   UWord   clineno = FI_GET_LINENO(c); 
   FiLine* cline; 
   UWord   cloff; 

   UShort u16;

   STATIC_ASSERT (FI_LINE_SZB == 32);
   

   if (LIKELY(fi->tags[clineno] == begtag)) {
      
      cline = &fi->lines[clineno];
      cloff = (c - begtag) / 8;

      
      if (UNLIKELY(!VG_IS_8_ALIGNED(c))) {
         UWord hiB = 8 - (c & 7);
         UShort mask = 0xFFFF << (16 - 2*hiB);

         u16  = cline->u16s[cloff];
         if (LIKELY(rlen >= hiB)) {
            cline->u16s[cloff] = u16 & ~mask; 
            rlen -= hiB;
            c += hiB;
            cloff += 1;
         } else {
            
            mask = mask & ~(0xFFFF << (16 - 2*(hiB-rlen)));
            cline->u16s[cloff] = u16 & ~mask; 
            RETURN;  
         }
      }
      while (!VG_IS_32_ALIGNED(c) && rlen >= 8) {
         cline->u16s[cloff] = 0;
         c += 8;
         rlen -= 8;
         cloff += 1;
      }
   } else {
      c = begtag + FI_LINE_SZB;
      if (c > end)
         RETURN;   
      rlen -= c - a;
   }
   
   clineno = FI_GET_LINENO(c);

   if (rlen >= FI_LINE_SZB) {
      UWord nfull = rlen / FI_LINE_SZB;
      UWord full_len = nfull * FI_LINE_SZB;
      rlen -= full_len;
      if (nfull > FI_NUM_LINES)
         nfull = FI_NUM_LINES; 

      for (UWord n = 0; n < nfull; n++) {
         if (UNLIKELY(address_in_range(fi->tags[clineno], c, full_len))) {
            cline = &fi->lines[clineno];
            cline->u16s[0] = 0;
            cline->u16s[1] = 0;
            cline->u16s[2] = 0;
            cline->u16s[3] = 0;
            STATIC_ASSERT (4 == sizeof(cline->u16s)/sizeof(cline->u16s[0]));
         }
         clineno++;
         if (UNLIKELY(clineno == FI_NUM_LINES))
            clineno = 0;
      }

      c += full_len;
      clineno = FI_GET_LINENO(c);
   }

   if (CHECK_ZSM) {
      tl_assert(VG_IS_8_ALIGNED(c));
      tl_assert(clineno == FI_GET_LINENO(c));
   }

   
   if (UNLIKELY(rlen > 0) && fi->tags[clineno] == endtag) {
      cline = &fi->lines[clineno];
      cloff = (c - endtag) / 8;
      if (CHECK_ZSM) tl_assert(FI_GET_TAG(c) == endtag);

      while (rlen >= 8) {
         cline->u16s[cloff] = 0;
         c += 8;
         rlen -= 8;
         cloff += 1;
      }
      
      if (rlen > 0) {
         
         UWord loB = rlen;
         UShort mask = 0xFFFF >> (16 - 2*loB);

         u16  = cline->u16s[cloff];
         cline->u16s[cloff] = u16 & ~mask; 
      }
   }

#  if CHECK_ZSM > 0
   check_and_return:
   tl_assert (VG_(memcmp)(&fi_check, fi, sizeof(fi_check)) == 0);
#  endif
#  undef RETURN
}


static inline Bool Filter__ok_to_skip_crd64 ( Filter* fi, Addr a )
{
   if (UNLIKELY( !VG_IS_8_ALIGNED(a) ))
      return False;
   { 
     Addr    atag   = FI_GET_TAG(a);     
     UWord   lineno = FI_GET_LINENO(a);  
     FiLine* line   = &fi->lines[lineno];
     UWord   loff   = (a - atag) / 8;
     UShort  mask   = 0xAAAA;
     if (LIKELY( fi->tags[lineno] == atag )) {
        
        UShort u16  = line->u16s[loff];
        Bool   ok   = (u16 & mask) == mask; 
        line->u16s[loff] = u16 | mask; 
        return ok;
     } else {
        
        UWord i;
        fi->tags[lineno] = atag;
        for (i = 0; i < FI_LINE_SZB / 8; i++)
           line->u16s[i] = 0;
        line->u16s[loff] = mask;
        return False;
     }
   }
}

static inline Bool Filter__ok_to_skip_crd32 ( Filter* fi, Addr a )
{
   if (UNLIKELY( !VG_IS_4_ALIGNED(a) ))
      return False;
   {
     Addr    atag   = FI_GET_TAG(a);     
     UWord   lineno = FI_GET_LINENO(a);  
     FiLine* line   = &fi->lines[lineno];
     UWord   loff   = (a - atag) / 8;
     UShort  mask   = 0xAA << (2 * (a & 4)); 
     if (LIKELY( fi->tags[lineno] == atag )) {
        
        UShort  u16  = line->u16s[loff];
        Bool    ok   = (u16 & mask) == mask; 
        line->u16s[loff] = u16 | mask; 
        return ok;
     } else {
        
        UWord   i;
        fi->tags[lineno] = atag;
        for (i = 0; i < FI_LINE_SZB / 8; i++)
           line->u16s[i] = 0;
        line->u16s[loff] = mask;
        return False;
     }
   }
}

static inline Bool Filter__ok_to_skip_crd16 ( Filter* fi, Addr a )
{
   if (UNLIKELY( !VG_IS_2_ALIGNED(a) ))
      return False;
   {
     Addr    atag   = FI_GET_TAG(a);     
     UWord   lineno = FI_GET_LINENO(a);  
     FiLine* line   = &fi->lines[lineno];
     UWord   loff   = (a - atag) / 8;
     UShort  mask   = 0xA << (2 * (a & 6));
     
     if (LIKELY( fi->tags[lineno] == atag )) {
        
        UShort  u16  = line->u16s[loff];
        Bool    ok   = (u16 & mask) == mask; 
        line->u16s[loff] = u16 | mask; 
        return ok;
     } else {
        
        UWord   i;
        fi->tags[lineno] = atag;
        for (i = 0; i < FI_LINE_SZB / 8; i++)
           line->u16s[i] = 0;
        line->u16s[loff] = mask;
        return False;
     }
   }
}

static inline Bool Filter__ok_to_skip_crd08 ( Filter* fi, Addr a )
{
   {
     Addr    atag   = FI_GET_TAG(a);     
     UWord   lineno = FI_GET_LINENO(a);  
     FiLine* line   = &fi->lines[lineno];
     UWord   loff   = (a - atag) / 8;
     UShort  mask   = 0x2 << (2 * (a & 7));
     
     if (LIKELY( fi->tags[lineno] == atag )) {
        
        UShort  u16  = line->u16s[loff];
        Bool    ok   = (u16 & mask) == mask; 
        line->u16s[loff] = u16 | mask; 
        return ok;
     } else {
        
        UWord   i;
        fi->tags[lineno] = atag;
        for (i = 0; i < FI_LINE_SZB / 8; i++)
           line->u16s[i] = 0;
        line->u16s[loff] = mask;
        return False;
     }
   }
}



static inline Bool Filter__ok_to_skip_cwr64 ( Filter* fi, Addr a )
{
   if (UNLIKELY( !VG_IS_8_ALIGNED(a) ))
      return False;
   { 
     Addr    atag   = FI_GET_TAG(a);     
     UWord   lineno = FI_GET_LINENO(a);  
     FiLine* line   = &fi->lines[lineno];
     UWord   loff   = (a - atag) / 8;
     UShort  mask   = 0xFFFF;
     if (LIKELY( fi->tags[lineno] == atag )) {
        
        UShort u16  = line->u16s[loff];
        Bool   ok   = (u16 & mask) == mask; 
        line->u16s[loff] = u16 | mask; 
        return ok;
     } else {
        
        UWord i;
        fi->tags[lineno] = atag;
        for (i = 0; i < FI_LINE_SZB / 8; i++)
           line->u16s[i] = 0;
        line->u16s[loff] = mask;
        return False;
     }
   }
}

static inline Bool Filter__ok_to_skip_cwr32 ( Filter* fi, Addr a )
{
   if (UNLIKELY( !VG_IS_4_ALIGNED(a) ))
      return False;
   {
     Addr    atag   = FI_GET_TAG(a);     
     UWord   lineno = FI_GET_LINENO(a);  
     FiLine* line   = &fi->lines[lineno];
     UWord   loff   = (a - atag) / 8;
     UShort  mask   = 0xFF << (2 * (a & 4)); 
     if (LIKELY( fi->tags[lineno] == atag )) {
        
        UShort  u16  = line->u16s[loff];
        Bool    ok   = (u16 & mask) == mask; 
        line->u16s[loff] = u16 | mask; 
        return ok;
     } else {
        
        UWord   i;
        fi->tags[lineno] = atag;
        for (i = 0; i < FI_LINE_SZB / 8; i++)
           line->u16s[i] = 0;
        line->u16s[loff] = mask;
        return False;
     }
   }
}

static inline Bool Filter__ok_to_skip_cwr16 ( Filter* fi, Addr a )
{
   if (UNLIKELY( !VG_IS_2_ALIGNED(a) ))
      return False;
   {
     Addr    atag   = FI_GET_TAG(a);     
     UWord   lineno = FI_GET_LINENO(a);  
     FiLine* line   = &fi->lines[lineno];
     UWord   loff   = (a - atag) / 8;
     UShort  mask   = 0xF << (2 * (a & 6));
     
     if (LIKELY( fi->tags[lineno] == atag )) {
        
        UShort  u16  = line->u16s[loff];
        Bool    ok   = (u16 & mask) == mask; 
        line->u16s[loff] = u16 | mask; 
        return ok;
     } else {
        
        UWord   i;
        fi->tags[lineno] = atag;
        for (i = 0; i < FI_LINE_SZB / 8; i++)
           line->u16s[i] = 0;
        line->u16s[loff] = mask;
        return False;
     }
   }
}

static inline Bool Filter__ok_to_skip_cwr08 ( Filter* fi, Addr a )
{
   {
     Addr    atag   = FI_GET_TAG(a);     
     UWord   lineno = FI_GET_LINENO(a);  
     FiLine* line   = &fi->lines[lineno];
     UWord   loff   = (a - atag) / 8;
     UShort  mask   = 0x3 << (2 * (a & 7));
     
     if (LIKELY( fi->tags[lineno] == atag )) {
        
        UShort  u16  = line->u16s[loff];
        Bool    ok   = (u16 & mask) == mask; 
        line->u16s[loff] = u16 | mask; 
        return ok;
     } else {
        
        UWord   i;
        fi->tags[lineno] = atag;
        for (i = 0; i < FI_LINE_SZB / 8; i++)
           line->u16s[i] = 0;
        line->u16s[loff] = mask;
        return False;
     }
   }
}



static XArray*  thrid_to_thr_map = NULL;

static ThrID thrid_counter = 1024; 

static ThrID Thr__to_ThrID ( Thr* thr ) {
   return thr->thrid;
}
static Thr* Thr__from_ThrID ( UInt thrid ) {
   Thr* thr = *(Thr**)VG_(indexXA)( thrid_to_thr_map, thrid - 1024 );
   tl_assert(thr->thrid == thrid);
   return thr;
}

static Thr* Thr__new ( void )
{
   Thr* thr = HG_(zalloc)( "libhb.Thr__new.1", sizeof(Thr) );
   thr->viR = VtsID_INVALID;
   thr->viW = VtsID_INVALID;
   thr->llexit_done = False;
   thr->joinedwith_done = False;
   thr->filter = HG_(zalloc)( "libhb.Thr__new.2", sizeof(Filter) );
   if (HG_(clo_history_level) == 1)
      thr->local_Kws_n_stacks
         = VG_(newXA)( HG_(zalloc),
                       "libhb.Thr__new.3 (local_Kws_and_stacks)",
                       HG_(free), sizeof(ULong_n_EC) );

   if (!thrid_to_thr_map) {
      thrid_to_thr_map = VG_(newXA)( HG_(zalloc), "libhb.Thr__new.4",
                                     HG_(free), sizeof(Thr*) );
   }

   if (thrid_counter >= ThrID_MAX_VALID) {
      
      scalarts_limitations_fail_NORETURN( True );
   }

   thr->thrid = thrid_counter++;
   Word ix = VG_(addToXA)( thrid_to_thr_map, &thr );
   tl_assert(ix + 1024 == thr->thrid);

   return thr;
}

static void note_local_Kw_n_stack_for ( Thr* thr )
{
   Word       nPresent;
   ULong_n_EC pair;
   tl_assert(thr);

   
   if (HG_(clo_history_level) != 1) 
      return;

   
   pair.ull = VtsID__indexAt( thr->viW, thr );
   pair.ec  = main_get_EC( thr );
   tl_assert(pair.ec);
   tl_assert(thr->local_Kws_n_stacks);

   
   nPresent = VG_(sizeXA)( thr->local_Kws_n_stacks );

   if (nPresent >= N_KWs_N_STACKs_PER_THREAD) {
      VG_(dropHeadXA)( thr->local_Kws_n_stacks, nPresent / 2 );
      nPresent = VG_(sizeXA)( thr->local_Kws_n_stacks );
      if (0)
         VG_(printf)("LOCAL Kw: thr %p,  Kw %llu,  ec %p (!!! gc !!!)\n",
                     thr, pair.ull, pair.ec );
   }

   if (nPresent > 0) {
      ULong_n_EC* prevPair
         = (ULong_n_EC*)VG_(indexXA)( thr->local_Kws_n_stacks, nPresent-1 );
      tl_assert( prevPair->ull <= pair.ull );
   }

   if (nPresent == 0)
      pair.ec = NULL;

   VG_(addToXA)( thr->local_Kws_n_stacks, &pair );

   if (0)
      VG_(printf)("LOCAL Kw: thr %p,  Kw %llu,  ec %p\n",
                  thr, pair.ull, pair.ec );
   if (0)
      VG_(pp_ExeContext)(pair.ec);
}

static Int cmp__ULong_n_EC__by_ULong ( const ULong_n_EC* pair1,
                                       const ULong_n_EC* pair2 )
{
   if (pair1->ull < pair2->ull) return -1;
   if (pair1->ull > pair2->ull) return 1;
   return 0;
}




#define SVAL_TAGMASK (3ULL << 62)

static inline Bool SVal__isC ( SVal s ) {
   return (0ULL << 62) == (s & SVAL_TAGMASK);
}
static inline SVal SVal__mkC ( VtsID rmini, VtsID wmini ) {
   
   
   return (((ULong)rmini) << 32) | ((ULong)wmini);
}
static inline VtsID SVal__unC_Rmin ( SVal s ) {
   tl_assert(SVal__isC(s));
   return (VtsID)(s >> 32);
}
static inline VtsID SVal__unC_Wmin ( SVal s ) {
   tl_assert(SVal__isC(s));
   return (VtsID)(s & 0xFFFFFFFFULL);
}

static inline Bool SVal__isA ( SVal s ) {
   return (2ULL << 62) == (s & SVAL_TAGMASK);
}
__attribute__((unused))
static inline SVal SVal__mkA ( void ) {
   return 2ULL << 62;
}

static inline void SVal__rcinc ( SVal s ) {
   if (SVal__isC(s)) {
      VtsID__rcinc( SVal__unC_Rmin(s) );
      VtsID__rcinc( SVal__unC_Wmin(s) );
   }
}

static inline void SVal__rcdec ( SVal s ) {
   if (SVal__isC(s)) {
      VtsID__rcdec( SVal__unC_Rmin(s) );
      VtsID__rcdec( SVal__unC_Wmin(s) );
   }
}

static inline void *SVal2Ptr (SVal s)
{
   return (void*)(UWord)s;
}

static inline SVal Ptr2SVal (void* ptr)
{
   return (SVal)(UWord)ptr;
}





static UWord stats__evm__lookup_found = 0;
static UWord stats__evm__lookup_notfound = 0;

static UWord stats__ctxt_eq_tsw_eq_rcec = 0;
static UWord stats__ctxt_eq_tsw_neq_rcec = 0;
static UWord stats__ctxt_neq_tsw_neq_rcec = 0;
static UWord stats__ctxt_rcdec_calls = 0;
static UWord stats__ctxt_rcec_gc_discards = 0;

static UWord stats__ctxt_tab_curr = 0;
static UWord stats__ctxt_tab_max  = 0;

static UWord stats__ctxt_tab_qs   = 0;
static UWord stats__ctxt_tab_cmps = 0;



#define N_FRAMES 8

#define RCEC_MAGIC 0xab88abb2UL

#define N_RCEC_TAB 196613 

typedef
   struct _RCEC {
      UWord magic;  
      struct _RCEC* next;
      UWord rc;
      UWord rcX; 
      UWord frames_hash;          
      UWord frames[N_FRAMES];
   }
   RCEC;

static PoolAlloc* rcec_pool_allocator;
static RCEC* alloc_RCEC ( void ) {
   return VG_(allocEltPA) ( rcec_pool_allocator );
}

static void free_RCEC ( RCEC* rcec ) {
   tl_assert(rcec->magic == RCEC_MAGIC);
   VG_(freeEltPA)( rcec_pool_allocator, rcec );
}

static RCEC** contextTab = NULL; 

static UWord RCEC_referenced = 0;

static Word RCEC__cmp_by_frames ( RCEC* ec1, RCEC* ec2 ) {
   Word i;
   tl_assert(ec1 && ec1->magic == RCEC_MAGIC);
   tl_assert(ec2 && ec2->magic == RCEC_MAGIC);
   if (ec1->frames_hash < ec2->frames_hash) return -1;
   if (ec1->frames_hash > ec2->frames_hash) return  1;
   for (i = 0; i < N_FRAMES; i++) {
      if (ec1->frames[i] < ec2->frames[i]) return -1;
      if (ec1->frames[i] > ec2->frames[i]) return  1;
   }
   return 0;
}


static void ctxt__rcdec ( RCEC* ec )
{
   stats__ctxt_rcdec_calls++;
   tl_assert(ec && ec->magic == RCEC_MAGIC);
   tl_assert(ec->rc > 0);
   ec->rc--;
   if (ec->rc == 0) 
      RCEC_referenced--;
}

static void ctxt__rcinc ( RCEC* ec )
{
   tl_assert(ec && ec->magic == RCEC_MAGIC);
   if (ec->rc == 0) 
      RCEC_referenced++;
   ec->rc++;
}


static void move_RCEC_one_step_forward ( RCEC** headp, RCEC* ec )
{
   RCEC *ec0, *ec1, *ec2;
   if (ec == *headp)
      tl_assert(0); 
   tl_assert(ec != NULL);
   ec0 = *headp;
   ec1 = NULL;
   ec2 = NULL;
   while (True) {
      if (ec0 == NULL || ec0 == ec) break;
      ec2 = ec1;
      ec1 = ec0;
      ec0 = ec0->next;
   }
   tl_assert(ec0 == ec);
   if (ec0 != NULL && ec1 != NULL && ec2 != NULL) {
      RCEC* tmp;
      tl_assert(ec2->next == ec1);
      tl_assert(ec1->next == ec0);
      tmp = ec0->next;
      ec2->next = ec0;
      ec0->next = ec1;
      ec1->next = tmp;
   }
   else
   if (ec0 != NULL && ec1 != NULL && ec2 == NULL) {
      
      tl_assert(*headp == ec1);
      tl_assert(ec1->next == ec0);
      ec1->next = ec0->next;
      ec0->next = ec1;
      *headp = ec0;
   }
}


__attribute__((noinline))
static RCEC* ctxt__find_or_add ( RCEC* example )
{
   UWord hent;
   RCEC* copy;
   tl_assert(example && example->magic == RCEC_MAGIC);
   tl_assert(example->rc == 0);

   
   stats__ctxt_tab_qs++;
   hent = example->frames_hash % N_RCEC_TAB;
   copy = contextTab[hent];
   while (1) {
      if (!copy) break;
      tl_assert(copy->magic == RCEC_MAGIC);
      stats__ctxt_tab_cmps++;
      if (0 == RCEC__cmp_by_frames(copy, example)) break;
      copy = copy->next;
   }

   if (copy) {
      tl_assert(copy != example);
      if (copy != contextTab[hent]) {
         move_RCEC_one_step_forward( &contextTab[hent], copy );
      }
   } else {
      copy = alloc_RCEC();
      tl_assert(copy != example);
      *copy = *example;
      copy->next = contextTab[hent];
      contextTab[hent] = copy;
      stats__ctxt_tab_curr++;
      if (stats__ctxt_tab_curr > stats__ctxt_tab_max)
         stats__ctxt_tab_max = stats__ctxt_tab_curr;
   }
   return copy;
}

static inline UWord ROLW ( UWord w, Int n )
{
   Int bpw = 8 * sizeof(UWord);
   w = (w << n) | (w >> (bpw-n));
   return w;
}

__attribute__((noinline))
static RCEC* get_RCEC ( Thr* thr )
{
   UWord hash, i;
   RCEC  example;
   example.magic = RCEC_MAGIC;
   example.rc = 0;
   example.rcX = 0;
   example.next = NULL;
   main_get_stacktrace( thr, &example.frames[0], N_FRAMES );
   hash = 0;
   for (i = 0; i < N_FRAMES; i++) {
      hash ^= example.frames[i];
      hash = ROLW(hash, 19);
   }
   example.frames_hash = hash;
   return ctxt__find_or_add( &example );
}


typedef 
   struct {
      UInt      thrid  : SCALARTS_N_THRBITS;
      UInt      szB    : 32 - SCALARTS_N_THRBITS - 1;
      UInt      isW    : 1;
   } TSW; 
typedef
   struct {
      TSW       tsw;
      WordSetID locksHeldW;
      RCEC*     rcec;
   }
   Thr_n_RCEC;

typedef
   struct OldRef {
      struct OldRef *ht_next; 
      UWord  ga; 
      struct OldRef *prev; 
      struct OldRef *next; 
      UWord stamp; 
      Thr_n_RCEC acc;
   }
   OldRef;

static inline UInt oldref_tsw (const OldRef* or)
{
   return *(const UInt*)(&or->acc.tsw);
}

static Word cmp_oldref_tsw (const void* node1, const void* node2 )
{
   const UInt tsw1 = oldref_tsw(node1);
   const UInt tsw2 = oldref_tsw(node2);

   if (tsw1 < tsw2) return -1;
   if (tsw1 > tsw2) return  1;
   return 0;
}


static PoolAlloc* oldref_pool_allocator;

static OldRef mru; 
static OldRef lru; 

static inline void OldRef_unchain(OldRef *r)
{
   r->next->prev = r->prev;
   r->prev->next = r->next;
}

static inline void OldRef_newest(OldRef *new)
{
   new->next = &mru;
   new->prev = mru.prev;
   mru.prev = new;
   new->prev->next = new;
}


static VgHashTable* oldrefHT    = NULL; 
static UWord     oldrefHTN    = 0;    


static OldRef* alloc_or_reuse_OldRef ( void )
{
   if (oldrefHTN < HG_(clo_conflict_cache_size)) {
      oldrefHTN++;
      return VG_(allocEltPA) ( oldref_pool_allocator );
   } else {
      OldRef *oldref_ht;
      OldRef *oldref = lru.next;

      OldRef_unchain(oldref);
      oldref_ht = VG_(HT_gen_remove) (oldrefHT, oldref, cmp_oldref_tsw);
      tl_assert (oldref == oldref_ht);
      ctxt__rcdec( oldref->acc.rcec );
      return oldref;
   }
}


inline static UInt min_UInt ( UInt a, UInt b ) {
   return a < b ? a : b;
}

static inline Word cmp_nonempty_intervals ( Addr a1, SizeT n1,
                                            Addr a2, SizeT n2 ) {
   UWord a1w = (UWord)a1;
   UWord n1w = (UWord)n1;
   UWord a2w = (UWord)a2;
   UWord n2w = (UWord)n2;
   tl_assert(n1w > 0 && n2w > 0);
   if (a1w + n1w <= a2w) return -1L;
   if (a2w + n2w <= a1w) return 1L;
   return 0;
}

static UWord event_map_stamp = 0; 

static void event_map_bind ( Addr a, SizeT szB, Bool isW, Thr* thr )
{
   OldRef  example;
   OldRef* ref;
   RCEC*   rcec;

   tl_assert(thr);
   ThrID thrid = thr->thrid;
   tl_assert(thrid != 0); 

   WordSetID locksHeldW = thr->hgthread->locksetW;

   rcec = get_RCEC( thr );

   tl_assert (szB == 4 || szB == 8 ||szB == 1 || szB == 2);
   
   

   example.ga = a;
   example.acc.tsw = (TSW) {.thrid = thrid,
                            .szB = szB,
                            .isW = (UInt)(isW & 1)};
   ref = VG_(HT_gen_lookup) (oldrefHT, &example, cmp_oldref_tsw);

   if (ref) {
      tl_assert (ref->ga == a);

      
      if (rcec == ref->acc.rcec)
         stats__ctxt_eq_tsw_eq_rcec++;
      else {
         stats__ctxt_eq_tsw_neq_rcec++;
         ctxt__rcdec( ref->acc.rcec );
         ctxt__rcinc(rcec);
         ref->acc.rcec       = rcec;
      }
      tl_assert(ref->acc.tsw.thrid == thrid);
      
      ref->stamp = event_map_stamp;
      ref->acc.locksHeldW = locksHeldW;

      OldRef_unchain(ref);
      OldRef_newest(ref);

   } else {
      
      stats__ctxt_neq_tsw_neq_rcec++;
      ref = alloc_or_reuse_OldRef();
      ref->ga = a;
      ref->acc.tsw = (TSW) {.thrid  = thrid,
                            .szB    = szB,
                            .isW    = (UInt)(isW & 1)};
      ref->stamp = event_map_stamp;
      ref->acc.locksHeldW = locksHeldW;
      ref->acc.rcec       = rcec;
      ctxt__rcinc(rcec);

      VG_(HT_add_node) ( oldrefHT, ref );
      OldRef_newest (ref);
   }
   event_map_stamp++;
}


Bool libhb_event_map_lookup ( ExeContext** resEC,
                              Thr**        resThr,
                              SizeT*       resSzB,
                              Bool*        resIsW,
                              WordSetID*   locksHeldW,
                              Thr* thr, Addr a, SizeT szB, Bool isW )
{
   Word    i, j;
   OldRef *ref = NULL;
   SizeT  ref_szB = 0;

   OldRef *cand_ref;
   SizeT  cand_ref_szB;
   Addr   cand_a;

   Addr toCheck[15];
   Int  nToCheck = 0;

   tl_assert(thr);
   tl_assert(szB == 8 || szB == 4 || szB == 2 || szB == 1);

   ThrID thrid = thr->thrid;

   toCheck[nToCheck++] = a;
   for (i = -7; i < (Word)szB; i++) {
      if (i != 0)
         toCheck[nToCheck++] = a + i;
   }
   tl_assert(nToCheck <= 15);

   for (j = 0; j < nToCheck; j++) {

      cand_a = toCheck[j];
      

      for (cand_ref = VG_(HT_lookup)( oldrefHT, cand_a ); 
           cand_ref; cand_ref = cand_ref->ht_next) {
         if (cand_ref->ga != cand_a)
            
            continue;

         if (cand_ref->acc.tsw.thrid == thrid)
            continue;

         if ((!cand_ref->acc.tsw.isW) && (!isW))
            continue;

         cand_ref_szB        = cand_ref->acc.tsw.szB;
         if (cmp_nonempty_intervals(a, szB, cand_a, cand_ref_szB) != 0)
            
            continue;

         if (!ref 
             || (ref->stamp - event_map_stamp) 
                   < (cand_ref->stamp - event_map_stamp)) {
            ref = cand_ref;
            ref_szB = cand_ref_szB;
         }
      }

      if (ref) {
         
         Int n, maxNFrames;
         RCEC*     ref_rcec = ref->acc.rcec;
         tl_assert(ref->acc.tsw.thrid);
         tl_assert(ref_rcec);
         tl_assert(ref_rcec->magic == RCEC_MAGIC);
         tl_assert(ref_szB >= 1);
         
         maxNFrames = min_UInt(N_FRAMES, VG_(clo_backtrace_size));
         for (n = 0; n < maxNFrames; n++) {
            if (0 == ref_rcec->frames[n]) break;
         }
         *resEC      = VG_(make_ExeContext_from_StackTrace)(ref_rcec->frames,
                                                            n);
         *resThr     = Thr__from_ThrID(ref->acc.tsw.thrid);
         *resSzB     = ref_szB;
         *resIsW     = ref->acc.tsw.isW;
         *locksHeldW = ref->acc.locksHeldW;
         stats__evm__lookup_found++;
         return True;
      }

      
   } 

   
   stats__evm__lookup_notfound++;
   return False;
}


void libhb_event_map_access_history ( Addr a, SizeT szB, Access_t fn )
{
   OldRef *ref = lru.next;
   SizeT ref_szB;
   Int n;
      
   while (ref != &mru) {
      ref_szB = ref->acc.tsw.szB;
      if (cmp_nonempty_intervals(a, szB, ref->ga, ref_szB) == 0) {
         RCEC* ref_rcec = ref->acc.rcec;
         for (n = 0; n < N_FRAMES; n++) {
            if (0 == ref_rcec->frames[n]) {
               break;
            }
         }
         (*fn)(ref_rcec->frames, n,
               Thr__from_ThrID(ref->acc.tsw.thrid),
               ref->ga,
               ref_szB,
               ref->acc.tsw.isW,
               ref->acc.locksHeldW);
      }
      tl_assert (ref->next == &mru
                 || ((ref->stamp - event_map_stamp)
                        < ref->next->stamp - event_map_stamp));
      ref = ref->next;
   }
}

static void event_map_init ( void )
{
   Word i;

   
   rcec_pool_allocator = VG_(newPA) (
                             sizeof(RCEC),
                             1000 ,
                             HG_(zalloc),
                             "libhb.event_map_init.1 (RCEC pools)",
                             HG_(free)
                          );

   
   tl_assert(!contextTab);
   contextTab = HG_(zalloc)( "libhb.event_map_init.2 (context table)",
                             N_RCEC_TAB * sizeof(RCEC*) );
   for (i = 0; i < N_RCEC_TAB; i++)
      contextTab[i] = NULL;

   
   oldref_pool_allocator = VG_(newPA)(
                               sizeof(OldRef),
                               1000 ,
                               HG_(zalloc),
                               "libhb.event_map_init.3 (OldRef pools)",
                               HG_(free)
                            );

   
   tl_assert(!oldrefHT);
   oldrefHT = VG_(HT_construct) ("libhb.event_map_init.4 (oldref hashtable)");

   oldrefHTN = 0;
   mru.prev = &lru;
   mru.next = NULL;
   lru.prev = NULL;
   lru.next = &mru;
   mru.acc = (Thr_n_RCEC) {.tsw = {.thrid = 0, 
                                   .szB = 0, 
                                   .isW = 0},
                           .locksHeldW = 0, 
                           .rcec = NULL};
   lru.acc = mru.acc;
}

static void event_map__check_reference_counts ( void )
{
   RCEC*   rcec;
   OldRef* oldref;
   Word    i;
   UWord   nEnts = 0;

   for (i = 0; i < N_RCEC_TAB; i++) {
      for (rcec = contextTab[i]; rcec; rcec = rcec->next) {
         nEnts++;
         tl_assert(rcec);
         tl_assert(rcec->magic == RCEC_MAGIC);
         rcec->rcX = 0;
      }
   }

   
   tl_assert(nEnts == stats__ctxt_tab_curr);
   tl_assert(stats__ctxt_tab_curr <= stats__ctxt_tab_max);

   
   VG_(HT_ResetIter)( oldrefHT );
   oldref = VG_(HT_Next)( oldrefHT );
   while (oldref) {
      tl_assert (oldref->acc.tsw.thrid);
      tl_assert (oldref->acc.rcec);
      tl_assert (oldref->acc.rcec->magic == RCEC_MAGIC);
      oldref->acc.rcec->rcX++;
      oldref = VG_(HT_Next)( oldrefHT );
   }

   
   for (i = 0; i < N_RCEC_TAB; i++) {
      for (rcec = contextTab[i]; rcec; rcec = rcec->next) {
         tl_assert(rcec->rc == rcec->rcX);
      }
   }
}

__attribute__((noinline))
static void do_RCEC_GC ( void )
{
   UInt i;

   if (VG_(clo_stats)) {
      static UInt ctr = 1;
      VG_(message)(Vg_DebugMsg,
                  "libhb: RCEC GC: #%u  %lu slots,"
                   " %lu cur ents(ref'd %lu),"
                   " %lu max ents\n",
                   ctr++,
                   (UWord)N_RCEC_TAB,
                   stats__ctxt_tab_curr, RCEC_referenced,
                   stats__ctxt_tab_max );
   }
   tl_assert (stats__ctxt_tab_curr > RCEC_referenced);

   
   for (i = 0; i < N_RCEC_TAB; i++) {
      RCEC** pp = &contextTab[i];
      RCEC*  p  = *pp;
      while (p) {
         if (p->rc == 0) {
            *pp = p->next;
            free_RCEC(p);
            p = *pp;
            tl_assert(stats__ctxt_tab_curr > 0);
            stats__ctxt_rcec_gc_discards++;
            stats__ctxt_tab_curr--;
         } else {
            pp = &p->next;
            p = p->next;
         }
      }
   }

   tl_assert (stats__ctxt_tab_curr == RCEC_referenced);
}



static ULong stats__msmcread         = 0;
static ULong stats__msmcread_change  = 0;
static ULong stats__msmcwrite        = 0;
static ULong stats__msmcwrite_change = 0;


__attribute__((noinline))
static void record_race_info ( Thr* acc_thr, 
                               Addr acc_addr, SizeT szB, Bool isWrite,
                               VtsID Cfailed,
                               VtsID Kfailed,
                               VtsID Cw )
{

   ExeContext* hist1_seg_start = NULL;
   ExeContext* hist1_seg_end   = NULL;
   Thread*     hist1_conf_thr  = NULL;

   tl_assert(acc_thr);
   tl_assert(acc_thr->hgthread);
   tl_assert(acc_thr->hgthread->hbthr == acc_thr);
   tl_assert(HG_(clo_history_level) >= 0 && HG_(clo_history_level) <= 2);

   if (HG_(clo_history_level) == 1) {
      Bool found;
      Word firstIx, lastIx;
      ULong_n_EC key;

      Thr*  confThr;
      ULong confTym = 0;
      confThr = VtsID__findFirst_notLEQ( Cfailed, Kfailed );
      tl_assert(confThr);

      confTym = VtsID__indexAt( Cfailed, confThr );

      key.ull = confTym;
      key.ec  = NULL;
      
      tl_assert(confThr->local_Kws_n_stacks);
      firstIx = lastIx = 0;
      found = VG_(lookupXA_UNSAFE)(
                 confThr->local_Kws_n_stacks,
                 &key, &firstIx, &lastIx,
                 (XACmpFn_t)cmp__ULong_n_EC__by_ULong
              );
      if (0) VG_(printf)("record_race_info %u %u %u  confThr %p "
                         "confTym %llu found %d (%lu,%lu)\n", 
                         Cfailed, Kfailed, Cw,
                         confThr, confTym, found, firstIx, lastIx);
      if (found) {
         ULong_n_EC *pair_start, *pair_end;
         pair_start 
            = (ULong_n_EC*)VG_(indexXA)( confThr->local_Kws_n_stacks, lastIx );
         hist1_seg_start = pair_start->ec;
         if (lastIx+1 < VG_(sizeXA)( confThr->local_Kws_n_stacks )) {
            pair_end
               = (ULong_n_EC*)VG_(indexXA)( confThr->local_Kws_n_stacks,
                                            lastIx+1 );
            
            tl_assert(pair_start->ull < pair_end->ull);
            hist1_seg_end = pair_end->ec;
         } else {
            if (!confThr->llexit_done)
               hist1_seg_end = main_get_EC( confThr );
         }
         
         
         
         hist1_conf_thr = confThr->hgthread;
      }
   }

   HG_(record_error_Race)( acc_thr->hgthread, acc_addr,
                           szB, isWrite,
                           hist1_conf_thr, hist1_seg_start, hist1_seg_end );
}

static Bool is_sane_SVal_C ( SVal sv ) {
   Bool leq;
   if (!SVal__isC(sv)) return True;
   leq = VtsID__cmpLEQ( SVal__unC_Rmin(sv), SVal__unC_Wmin(sv) );
   return leq;
}


static inline SVal msmcread ( SVal svOld,
                              Thr* acc_thr,
                              Addr acc_addr, SizeT szB )
{
   SVal svNew = SVal_INVALID;
   stats__msmcread++;

   
   if (CHECK_MSM) {
      tl_assert(is_sane_SVal_C(svOld));
   }

   if (LIKELY(SVal__isC(svOld))) {
      VtsID tviR  = acc_thr->viR;
      VtsID tviW  = acc_thr->viW;
      VtsID rmini = SVal__unC_Rmin(svOld);
      VtsID wmini = SVal__unC_Wmin(svOld);
      Bool  leq   = VtsID__cmpLEQ(rmini,tviR);
      if (LIKELY(leq)) {
         
         
         svNew = SVal__mkC( rmini, VtsID__join2(wmini, tviW) );
         goto out;
      } else {
         
         Bool leqxx = VtsID__cmpLEQ(rmini,wmini);
         tl_assert(leqxx);
         
         svNew = SVal__mkC( rmini, VtsID__join2(wmini, tviW) );
         record_race_info( acc_thr, acc_addr, szB, False,
                           rmini, 
                           tviR,  
                           wmini   );
         goto out;
      }
   }
   if (SVal__isA(svOld)) {
      
      
      tl_assert(svOld == SVal_NOACCESS);
      svNew = SVal_NOACCESS;
      goto out;
   }
   if (0) VG_(printf)("msmcread: bad svOld: 0x%016llx\n", svOld);
   tl_assert(0);

  out:
   if (CHECK_MSM) {
      tl_assert(is_sane_SVal_C(svNew));
   }
   if (UNLIKELY(svNew != svOld)) {
      tl_assert(svNew != SVal_INVALID);
      if (HG_(clo_history_level) >= 2
          && SVal__isC(svOld) && SVal__isC(svNew)) {
         event_map_bind( acc_addr, szB, False, acc_thr );
         stats__msmcread_change++;
      }
   }
   return svNew;
}


static inline SVal msmcwrite ( SVal svOld,
                              Thr* acc_thr,
                              Addr acc_addr, SizeT szB )
{
   SVal svNew = SVal_INVALID;
   stats__msmcwrite++;

   
   if (CHECK_MSM) {
      tl_assert(is_sane_SVal_C(svOld));
   }

   if (LIKELY(SVal__isC(svOld))) {
      VtsID tviW  = acc_thr->viW;
      VtsID wmini = SVal__unC_Wmin(svOld);
      Bool  leq   = VtsID__cmpLEQ(wmini,tviW);
      if (LIKELY(leq)) {
         
         svNew = SVal__mkC( tviW, tviW );
         goto out;
      } else {
         VtsID rmini = SVal__unC_Rmin(svOld);
         
         Bool leqxx = VtsID__cmpLEQ(rmini,wmini);
         tl_assert(leqxx);
         
         
         
         
         
         
         
         
         
         
         svNew = SVal__mkC( VtsID__join2(rmini, tviW),
                            VtsID__join2(wmini, tviW) );
         record_race_info( acc_thr, acc_addr, szB, True,
                           wmini, 
                           tviW,  
                           wmini   );
         goto out;
      }
   }
   if (SVal__isA(svOld)) {
      
      
      tl_assert(svOld == SVal_NOACCESS);
      svNew = SVal_NOACCESS;
      goto out;
   }
   if (0) VG_(printf)("msmcwrite: bad svOld: 0x%016llx\n", svOld);
   tl_assert(0);

  out:
   if (CHECK_MSM) {
      tl_assert(is_sane_SVal_C(svNew));
   }
   if (UNLIKELY(svNew != svOld)) {
      tl_assert(svNew != SVal_INVALID);
      if (HG_(clo_history_level) >= 2
          && SVal__isC(svOld) && SVal__isC(svNew)) {
         event_map_bind( acc_addr, szB, True, acc_thr );
         stats__msmcwrite_change++;
      }
   }
   return svNew;
}




static void zsm_sapply08__msmcread ( Thr* thr, Addr a ) {
   CacheLine* cl; 
   UWord      cloff, tno, toff;
   SVal       svOld, svNew;
   UShort     descr;
   stats__cline_cread08s++;
   cl    = get_cacheline(a);
   cloff = get_cacheline_offset(a);
   tno   = get_treeno(a);
   toff  = get_tree_offset(a); 
   descr = cl->descrs[tno];
   if (UNLIKELY( !(descr & (TREE_DESCR_8_0 << toff)) )) {
      SVal* tree = &cl->svals[tno << 3];
      cl->descrs[tno] = pulldown_to_8(tree, toff, descr);
      if (CHECK_ZSM)
         tl_assert(is_sane_CacheLine(cl)); 
   }
   svOld = cl->svals[cloff];
   svNew = msmcread( svOld, thr,a,1 );
   if (CHECK_ZSM)
      tl_assert(svNew != SVal_INVALID);
   cl->svals[cloff] = svNew;
}

static void zsm_sapply08__msmcwrite ( Thr* thr, Addr a ) {
   CacheLine* cl; 
   UWord      cloff, tno, toff;
   SVal       svOld, svNew;
   UShort     descr;
   stats__cline_cwrite08s++;
   cl    = get_cacheline(a);
   cloff = get_cacheline_offset(a);
   tno   = get_treeno(a);
   toff  = get_tree_offset(a); 
   descr = cl->descrs[tno];
   if (UNLIKELY( !(descr & (TREE_DESCR_8_0 << toff)) )) {
      SVal* tree = &cl->svals[tno << 3];
      cl->descrs[tno] = pulldown_to_8(tree, toff, descr);
      if (CHECK_ZSM)
         tl_assert(is_sane_CacheLine(cl)); 
   }
   svOld = cl->svals[cloff];
   svNew = msmcwrite( svOld, thr,a,1 );
   if (CHECK_ZSM)
      tl_assert(svNew != SVal_INVALID);
   cl->svals[cloff] = svNew;
}


static void zsm_sapply16__msmcread ( Thr* thr, Addr a ) {
   CacheLine* cl; 
   UWord      cloff, tno, toff;
   SVal       svOld, svNew;
   UShort     descr;
   stats__cline_cread16s++;
   if (UNLIKELY(!aligned16(a))) goto slowcase;
   cl    = get_cacheline(a);
   cloff = get_cacheline_offset(a);
   tno   = get_treeno(a);
   toff  = get_tree_offset(a); 
   descr = cl->descrs[tno];
   if (UNLIKELY( !(descr & (TREE_DESCR_16_0 << toff)) )) {
      if (valid_value_is_below_me_16(descr, toff)) {
         goto slowcase;
      } else {
         SVal* tree = &cl->svals[tno << 3];
         cl->descrs[tno] = pulldown_to_16(tree, toff, descr);
      }
      if (CHECK_ZSM)
         tl_assert(is_sane_CacheLine(cl)); 
   }
   svOld = cl->svals[cloff];
   svNew = msmcread( svOld, thr,a,2 );
   if (CHECK_ZSM)
      tl_assert(svNew != SVal_INVALID);
   cl->svals[cloff] = svNew;
   return;
  slowcase: 
   stats__cline_16to8splits++;
   zsm_sapply08__msmcread( thr, a + 0 );
   zsm_sapply08__msmcread( thr, a + 1 );
}

static void zsm_sapply16__msmcwrite ( Thr* thr, Addr a ) {
   CacheLine* cl; 
   UWord      cloff, tno, toff;
   SVal       svOld, svNew;
   UShort     descr;
   stats__cline_cwrite16s++;
   if (UNLIKELY(!aligned16(a))) goto slowcase;
   cl    = get_cacheline(a);
   cloff = get_cacheline_offset(a);
   tno   = get_treeno(a);
   toff  = get_tree_offset(a); 
   descr = cl->descrs[tno];
   if (UNLIKELY( !(descr & (TREE_DESCR_16_0 << toff)) )) {
      if (valid_value_is_below_me_16(descr, toff)) {
         goto slowcase;
      } else {
         SVal* tree = &cl->svals[tno << 3];
         cl->descrs[tno] = pulldown_to_16(tree, toff, descr);
      }
      if (CHECK_ZSM)
         tl_assert(is_sane_CacheLine(cl)); 
   }
   svOld = cl->svals[cloff];
   svNew = msmcwrite( svOld, thr,a,2 );
   if (CHECK_ZSM)
      tl_assert(svNew != SVal_INVALID);
   cl->svals[cloff] = svNew;
   return;
  slowcase: 
   stats__cline_16to8splits++;
   zsm_sapply08__msmcwrite( thr, a + 0 );
   zsm_sapply08__msmcwrite( thr, a + 1 );
}


static void zsm_sapply32__msmcread ( Thr* thr, Addr a ) {
   CacheLine* cl; 
   UWord      cloff, tno, toff;
   SVal       svOld, svNew;
   UShort     descr;
   stats__cline_cread32s++;
   if (UNLIKELY(!aligned32(a))) goto slowcase;
   cl    = get_cacheline(a);
   cloff = get_cacheline_offset(a);
   tno   = get_treeno(a);
   toff  = get_tree_offset(a); 
   descr = cl->descrs[tno];
   if (UNLIKELY( !(descr & (TREE_DESCR_32_0 << toff)) )) {
      if (valid_value_is_above_me_32(descr, toff)) {
         SVal* tree = &cl->svals[tno << 3];
         cl->descrs[tno] = pulldown_to_32(tree, toff, descr);
      } else {
         goto slowcase;
      }
      if (CHECK_ZSM)
         tl_assert(is_sane_CacheLine(cl)); 
   }
   svOld = cl->svals[cloff];
   svNew = msmcread( svOld, thr,a,4 );
   if (CHECK_ZSM)
      tl_assert(svNew != SVal_INVALID);
   cl->svals[cloff] = svNew;
   return;
  slowcase: 
   stats__cline_32to16splits++;
   zsm_sapply16__msmcread( thr, a + 0 );
   zsm_sapply16__msmcread( thr, a + 2 );
}

static void zsm_sapply32__msmcwrite ( Thr* thr, Addr a ) {
   CacheLine* cl; 
   UWord      cloff, tno, toff;
   SVal       svOld, svNew;
   UShort     descr;
   stats__cline_cwrite32s++;
   if (UNLIKELY(!aligned32(a))) goto slowcase;
   cl    = get_cacheline(a);
   cloff = get_cacheline_offset(a);
   tno   = get_treeno(a);
   toff  = get_tree_offset(a); 
   descr = cl->descrs[tno];
   if (UNLIKELY( !(descr & (TREE_DESCR_32_0 << toff)) )) {
      if (valid_value_is_above_me_32(descr, toff)) {
         SVal* tree = &cl->svals[tno << 3];
         cl->descrs[tno] = pulldown_to_32(tree, toff, descr);
      } else {
         goto slowcase;
      }
      if (CHECK_ZSM)
         tl_assert(is_sane_CacheLine(cl)); 
   }
   svOld = cl->svals[cloff];
   svNew = msmcwrite( svOld, thr,a,4 );
   if (CHECK_ZSM)
      tl_assert(svNew != SVal_INVALID);
   cl->svals[cloff] = svNew;
   return;
  slowcase: 
   stats__cline_32to16splits++;
   zsm_sapply16__msmcwrite( thr, a + 0 );
   zsm_sapply16__msmcwrite( thr, a + 2 );
}


static void zsm_sapply64__msmcread ( Thr* thr, Addr a ) {
   CacheLine* cl; 
   UWord      cloff, tno;
   
   SVal       svOld, svNew;
   UShort     descr;
   stats__cline_cread64s++;
   if (UNLIKELY(!aligned64(a))) goto slowcase;
   cl    = get_cacheline(a);
   cloff = get_cacheline_offset(a);
   tno   = get_treeno(a);
   
   descr = cl->descrs[tno];
   if (UNLIKELY( !(descr & TREE_DESCR_64) )) {
      goto slowcase;
   }
   svOld = cl->svals[cloff];
   svNew = msmcread( svOld, thr,a,8 );
   if (CHECK_ZSM)
      tl_assert(svNew != SVal_INVALID);
   cl->svals[cloff] = svNew;
   return;
  slowcase: 
   stats__cline_64to32splits++;
   zsm_sapply32__msmcread( thr, a + 0 );
   zsm_sapply32__msmcread( thr, a + 4 );
}

static void zsm_sapply64__msmcwrite ( Thr* thr, Addr a ) {
   CacheLine* cl; 
   UWord      cloff, tno;
   
   SVal       svOld, svNew;
   UShort     descr;
   stats__cline_cwrite64s++;
   if (UNLIKELY(!aligned64(a))) goto slowcase;
   cl    = get_cacheline(a);
   cloff = get_cacheline_offset(a);
   tno   = get_treeno(a);
   
   descr = cl->descrs[tno];
   if (UNLIKELY( !(descr & TREE_DESCR_64) )) {
      goto slowcase;
   }
   svOld = cl->svals[cloff];
   svNew = msmcwrite( svOld, thr,a,8 );
   if (CHECK_ZSM)
      tl_assert(svNew != SVal_INVALID);
   cl->svals[cloff] = svNew;
   return;
  slowcase: 
   stats__cline_64to32splits++;
   zsm_sapply32__msmcwrite( thr, a + 0 );
   zsm_sapply32__msmcwrite( thr, a + 4 );
}


static
void zsm_swrite08 ( Addr a, SVal svNew ) {
   CacheLine* cl; 
   UWord      cloff, tno, toff;
   UShort     descr;
   stats__cline_swrite08s++;
   cl    = get_cacheline(a);
   cloff = get_cacheline_offset(a);
   tno   = get_treeno(a);
   toff  = get_tree_offset(a); 
   descr = cl->descrs[tno];
   if (UNLIKELY( !(descr & (TREE_DESCR_8_0 << toff)) )) {
      SVal* tree = &cl->svals[tno << 3];
      cl->descrs[tno] = pulldown_to_8(tree, toff, descr);
      if (CHECK_ZSM)
         tl_assert(is_sane_CacheLine(cl)); 
   }
   tl_assert(svNew != SVal_INVALID);
   cl->svals[cloff] = svNew;
}


static
void zsm_swrite16 ( Addr a, SVal svNew ) {
   CacheLine* cl; 
   UWord      cloff, tno, toff;
   UShort     descr;
   stats__cline_swrite16s++;
   if (UNLIKELY(!aligned16(a))) goto slowcase;
   cl    = get_cacheline(a);
   cloff = get_cacheline_offset(a);
   tno   = get_treeno(a);
   toff  = get_tree_offset(a); 
   descr = cl->descrs[tno];
   if (UNLIKELY( !(descr & (TREE_DESCR_16_0 << toff)) )) {
      if (valid_value_is_below_me_16(descr, toff)) {
         
         cl->descrs[tno] = pullup_descr_to_16(descr, toff);
      } else {
         SVal* tree = &cl->svals[tno << 3];
         cl->descrs[tno] = pulldown_to_16(tree, toff, descr);
      if (CHECK_ZSM)
         tl_assert(is_sane_CacheLine(cl)); 
      }
   }
   tl_assert(svNew != SVal_INVALID);
   cl->svals[cloff + 0] = svNew;
   cl->svals[cloff + 1] = SVal_INVALID;
   return;
  slowcase: 
   stats__cline_16to8splits++;
   zsm_swrite08( a + 0, svNew );
   zsm_swrite08( a + 1, svNew );
}


static
void zsm_swrite32 ( Addr a, SVal svNew ) {
   CacheLine* cl; 
   UWord      cloff, tno, toff;
   UShort     descr;
   stats__cline_swrite32s++;
   if (UNLIKELY(!aligned32(a))) goto slowcase;
   cl    = get_cacheline(a);
   cloff = get_cacheline_offset(a);
   tno   = get_treeno(a);
   toff  = get_tree_offset(a); 
   descr = cl->descrs[tno];
   if (UNLIKELY( !(descr & (TREE_DESCR_32_0 << toff)) )) {
      if (valid_value_is_above_me_32(descr, toff)) {
         SVal* tree = &cl->svals[tno << 3];
         cl->descrs[tno] = pulldown_to_32(tree, toff, descr);
         if (CHECK_ZSM)
            tl_assert(is_sane_CacheLine(cl)); 
      } else {
         
         cl->descrs[tno] = pullup_descr_to_32(descr, toff);
      }
   }
   tl_assert(svNew != SVal_INVALID);
   cl->svals[cloff + 0] = svNew;
   cl->svals[cloff + 1] = SVal_INVALID;
   cl->svals[cloff + 2] = SVal_INVALID;
   cl->svals[cloff + 3] = SVal_INVALID;
   return;
  slowcase: 
   stats__cline_32to16splits++;
   zsm_swrite16( a + 0, svNew );
   zsm_swrite16( a + 2, svNew );
}


static
void zsm_swrite64 ( Addr a, SVal svNew ) {
   CacheLine* cl; 
   UWord      cloff, tno;
   
   stats__cline_swrite64s++;
   if (UNLIKELY(!aligned64(a))) goto slowcase;
   cl    = get_cacheline(a);
   cloff = get_cacheline_offset(a);
   tno   = get_treeno(a);
   
   cl->descrs[tno] = TREE_DESCR_64;
   tl_assert(svNew != SVal_INVALID);
   cl->svals[cloff + 0] = svNew;
   cl->svals[cloff + 1] = SVal_INVALID;
   cl->svals[cloff + 2] = SVal_INVALID;
   cl->svals[cloff + 3] = SVal_INVALID;
   cl->svals[cloff + 4] = SVal_INVALID;
   cl->svals[cloff + 5] = SVal_INVALID;
   cl->svals[cloff + 6] = SVal_INVALID;
   cl->svals[cloff + 7] = SVal_INVALID;
   return;
  slowcase: 
   stats__cline_64to32splits++;
   zsm_swrite32( a + 0, svNew );
   zsm_swrite32( a + 4, svNew );
}


static
SVal zsm_sread08 ( Addr a ) {
   CacheLine* cl; 
   UWord      cloff, tno, toff;
   UShort     descr;
   stats__cline_sread08s++;
   cl    = get_cacheline(a);
   cloff = get_cacheline_offset(a);
   tno   = get_treeno(a);
   toff  = get_tree_offset(a); 
   descr = cl->descrs[tno];
   if (UNLIKELY( !(descr & (TREE_DESCR_8_0 << toff)) )) {
      SVal* tree = &cl->svals[tno << 3];
      cl->descrs[tno] = pulldown_to_8(tree, toff, descr);
   }
   return cl->svals[cloff];
}

static void zsm_scopy08 ( Addr src, Addr dst, Bool uu_normalise ) {
   SVal       sv;
   stats__cline_scopy08s++;
   sv = zsm_sread08( src );
   zsm_swrite08( dst, sv );
}



static void zsm_scopy_range ( Addr src, Addr dst, SizeT len )
{
   SizeT i;
   if (len == 0)
      return;

   
   tl_assert(src+len <= dst || dst+len <= src);

   for (i = 0; i < len; i++) {
      Bool normalise
         = get_cacheline_offset( dst+i+1 ) == 0 
           || i == 0       
           || i == len-1;  
      zsm_scopy08( src+i, dst+i, normalise );
   }
}



static
void zsm_sset_range_SMALL ( Addr a, SizeT len, SVal svNew )
{
   
   if (len == 4 && aligned32(a)) {
      zsm_swrite32( a, svNew );
      return;
   }
   if (len == 8 && aligned64(a)) {
      zsm_swrite64( a, svNew );
      return;
   }

   
   if (len == 0) return;

   if (!aligned16(a) && len >= 1) {
      zsm_swrite08( a, svNew );
      a += 1;
      len -= 1;
      tl_assert(aligned16(a));
   }
   if (len == 0) return;

   if (!aligned32(a) && len >= 2) {
      zsm_swrite16( a, svNew );
      a += 2;
      len -= 2;
      tl_assert(aligned32(a));
   }
   if (len == 0) return;

   if (!aligned64(a) && len >= 4) {
      zsm_swrite32( a, svNew );
      a += 4;
      len -= 4;
      tl_assert(aligned64(a));
   }
   if (len == 0) return;

   if (len >= 8) {
      tl_assert(aligned64(a));
      while (len >= 8) {
         zsm_swrite64( a, svNew );
         a += 8;
         len -= 8;
      }
      tl_assert(aligned64(a));
   }
   if (len == 0) return;

   if (len >= 4)
      tl_assert(aligned32(a));
   if (len >= 4) {
      zsm_swrite32( a, svNew );
      a += 4;
      len -= 4;
   }
   if (len == 0) return;

   if (len >= 2)
      tl_assert(aligned16(a));
   if (len >= 2) {
      zsm_swrite16( a, svNew );
      a += 2;
      len -= 2;
   }
   if (len == 0) return;

   if (len >= 1) {
      zsm_swrite08( a, svNew );
      
      len -= 1;
   }
   tl_assert(len == 0);
}



static void zsm_sset_range ( Addr a, SizeT len, SVal svNew )
{
   tl_assert(svNew != SVal_INVALID);
   stats__cache_make_New_arange += (ULong)len;

   if (0 && len > 500)
      VG_(printf)("make New      ( %#lx, %ld )\n", a, len );

   if (0) {
      static UWord n_New_in_cache = 0;
      static UWord n_New_not_in_cache = 0;
      Addr       tag = a & ~(N_LINE_ARANGE - 1);
      UWord      wix = (a >> N_LINE_BITS) & (N_WAY_NENT - 1);
      if (LIKELY(tag == cache_shmem.tags0[wix])) {
         n_New_in_cache++;
      } else {
         n_New_not_in_cache++;
      }
      if (0 == ((n_New_in_cache + n_New_not_in_cache) % 100000))
         VG_(printf)("shadow_mem_make_New: IN %lu OUT %lu\n",
                     n_New_in_cache, n_New_not_in_cache );
   }

   if (LIKELY(len < 2 * N_LINE_ARANGE)) {
      zsm_sset_range_SMALL( a, len, svNew );
   } else {
      Addr  before_start  = a;
      Addr  aligned_start = cacheline_ROUNDUP(a);
      Addr  after_start   = cacheline_ROUNDDN(a + len);
      UWord before_len    = aligned_start - before_start;
      UWord aligned_len   = after_start - aligned_start;
      UWord after_len     = a + len - after_start;
      tl_assert(before_start <= aligned_start);
      tl_assert(aligned_start <= after_start);
      tl_assert(before_len < N_LINE_ARANGE);
      tl_assert(after_len < N_LINE_ARANGE);
      tl_assert(get_cacheline_offset(aligned_start) == 0);
      if (get_cacheline_offset(a) == 0) {
         tl_assert(before_len == 0);
         tl_assert(a == aligned_start);
      }
      if (get_cacheline_offset(a+len) == 0) {
         tl_assert(after_len == 0);
         tl_assert(after_start == a+len);
      }
      if (before_len > 0) {
         zsm_sset_range_SMALL( before_start, before_len, svNew );
      }
      if (after_len > 0) {
         zsm_sset_range_SMALL( after_start, after_len, svNew );
      }
      stats__cache_make_New_inZrep += (ULong)aligned_len;

      while (1) {
         Addr tag;
         UWord wix;
         if (aligned_start >= after_start)
            break;
         tl_assert(get_cacheline_offset(aligned_start) == 0);
         tag = aligned_start & ~(N_LINE_ARANGE - 1);
         wix = (aligned_start >> N_LINE_BITS) & (N_WAY_NENT - 1);
         if (tag == cache_shmem.tags0[wix]) {
            UWord i;
            for (i = 0; i < N_LINE_ARANGE / 8; i++)
               zsm_swrite64( aligned_start + i * 8, svNew );
         } else {
            UWord i;
            Word zix;
            SecMap* sm;
            LineZ* lineZ;
            find_Z_for_writing( &sm, &zix, tag );
            tl_assert(sm);
            tl_assert(zix >= 0 && zix < N_SECMAP_ZLINES);
            lineZ = &sm->linesZ[zix];
            lineZ->dict[0] = svNew;
            lineZ->dict[1] = lineZ->dict[2] = lineZ->dict[3] = SVal_INVALID;
            for (i = 0; i < N_LINE_ARANGE/4; i++)
               lineZ->ix2s[i] = 0; 
            rcinc_LineZ(lineZ);
         }
         aligned_start += N_LINE_ARANGE;
         aligned_len -= N_LINE_ARANGE;
      }
      tl_assert(aligned_start == after_start);
      tl_assert(aligned_len == 0);
   }
}



static UWord stats__f_ac = 0;
static UWord stats__f_sk = 0;

#if 0
#  define STATS__F_SHOW \
     do { \
        if (UNLIKELY(0 == (stats__f_ac & 0xFFFFFF))) \
           VG_(printf)("filters: ac %lu sk %lu\n",   \
           stats__f_ac, stats__f_sk); \
     } while (0)
#else
#  define STATS__F_SHOW 
#endif

void zsm_sapply08_f__msmcwrite ( Thr* thr, Addr a ) {
   stats__f_ac++;
   STATS__F_SHOW;
   if (LIKELY(Filter__ok_to_skip_cwr08(thr->filter, a))) {
      stats__f_sk++;
      return;
   }
   zsm_sapply08__msmcwrite(thr, a);
}

void zsm_sapply16_f__msmcwrite ( Thr* thr, Addr a ) {
   stats__f_ac++;
   STATS__F_SHOW;
   if (LIKELY(Filter__ok_to_skip_cwr16(thr->filter, a))) {
      stats__f_sk++;
      return;
   }
   zsm_sapply16__msmcwrite(thr, a);
}

void zsm_sapply32_f__msmcwrite ( Thr* thr, Addr a ) {
   stats__f_ac++;
   STATS__F_SHOW;
   if (LIKELY(Filter__ok_to_skip_cwr32(thr->filter, a))) {
      stats__f_sk++;
      return;
   }
   zsm_sapply32__msmcwrite(thr, a);
}

void zsm_sapply64_f__msmcwrite ( Thr* thr, Addr a ) {
   stats__f_ac++;
   STATS__F_SHOW;
   if (LIKELY(Filter__ok_to_skip_cwr64(thr->filter, a))) {
      stats__f_sk++;
      return;
   }
   zsm_sapply64__msmcwrite(thr, a);
}

void zsm_sapplyNN_f__msmcwrite ( Thr* thr, Addr a, SizeT len )
{
   
   if (len == 4 && aligned32(a)) {
      zsm_sapply32_f__msmcwrite( thr, a );
      return;
   }
   if (len == 8 && aligned64(a)) {
      zsm_sapply64_f__msmcwrite( thr, a );
      return;
   }

   
   if (len == 0) return;

   if (!aligned16(a) && len >= 1) {
      zsm_sapply08_f__msmcwrite( thr, a );
      a += 1;
      len -= 1;
      tl_assert(aligned16(a));
   }
   if (len == 0) return;

   if (!aligned32(a) && len >= 2) {
      zsm_sapply16_f__msmcwrite( thr, a );
      a += 2;
      len -= 2;
      tl_assert(aligned32(a));
   }
   if (len == 0) return;

   if (!aligned64(a) && len >= 4) {
      zsm_sapply32_f__msmcwrite( thr, a );
      a += 4;
      len -= 4;
      tl_assert(aligned64(a));
   }
   if (len == 0) return;

   if (len >= 8) {
      tl_assert(aligned64(a));
      while (len >= 8) {
         zsm_sapply64_f__msmcwrite( thr, a );
         a += 8;
         len -= 8;
      }
      tl_assert(aligned64(a));
   }
   if (len == 0) return;

   if (len >= 4)
      tl_assert(aligned32(a));
   if (len >= 4) {
      zsm_sapply32_f__msmcwrite( thr, a );
      a += 4;
      len -= 4;
   }
   if (len == 0) return;

   if (len >= 2)
      tl_assert(aligned16(a));
   if (len >= 2) {
      zsm_sapply16_f__msmcwrite( thr, a );
      a += 2;
      len -= 2;
   }
   if (len == 0) return;

   if (len >= 1) {
      zsm_sapply08_f__msmcwrite( thr, a );
      
      len -= 1;
   }
   tl_assert(len == 0);
}

void zsm_sapply08_f__msmcread ( Thr* thr, Addr a ) {
   stats__f_ac++;
   STATS__F_SHOW;
   if (LIKELY(Filter__ok_to_skip_crd08(thr->filter, a))) {
      stats__f_sk++;
      return;
   }
   zsm_sapply08__msmcread(thr, a);
}

void zsm_sapply16_f__msmcread ( Thr* thr, Addr a ) {
   stats__f_ac++;
   STATS__F_SHOW;
   if (LIKELY(Filter__ok_to_skip_crd16(thr->filter, a))) {
      stats__f_sk++;
      return;
   }
   zsm_sapply16__msmcread(thr, a);
}

void zsm_sapply32_f__msmcread ( Thr* thr, Addr a ) {
   stats__f_ac++;
   STATS__F_SHOW;
   if (LIKELY(Filter__ok_to_skip_crd32(thr->filter, a))) {
      stats__f_sk++;
      return;
   }
   zsm_sapply32__msmcread(thr, a);
}

void zsm_sapply64_f__msmcread ( Thr* thr, Addr a ) {
   stats__f_ac++;
   STATS__F_SHOW;
   if (LIKELY(Filter__ok_to_skip_crd64(thr->filter, a))) {
      stats__f_sk++;
      return;
   }
   zsm_sapply64__msmcread(thr, a);
}

void zsm_sapplyNN_f__msmcread ( Thr* thr, Addr a, SizeT len )
{
   
   if (len == 4 && aligned32(a)) {
      zsm_sapply32_f__msmcread( thr, a );
      return;
   }
   if (len == 8 && aligned64(a)) {
      zsm_sapply64_f__msmcread( thr, a );
      return;
   }

   
   if (len == 0) return;

   if (!aligned16(a) && len >= 1) {
      zsm_sapply08_f__msmcread( thr, a );
      a += 1;
      len -= 1;
      tl_assert(aligned16(a));
   }
   if (len == 0) return;

   if (!aligned32(a) && len >= 2) {
      zsm_sapply16_f__msmcread( thr, a );
      a += 2;
      len -= 2;
      tl_assert(aligned32(a));
   }
   if (len == 0) return;

   if (!aligned64(a) && len >= 4) {
      zsm_sapply32_f__msmcread( thr, a );
      a += 4;
      len -= 4;
      tl_assert(aligned64(a));
   }
   if (len == 0) return;

   if (len >= 8) {
      tl_assert(aligned64(a));
      while (len >= 8) {
         zsm_sapply64_f__msmcread( thr, a );
         a += 8;
         len -= 8;
      }
      tl_assert(aligned64(a));
   }
   if (len == 0) return;

   if (len >= 4)
      tl_assert(aligned32(a));
   if (len >= 4) {
      zsm_sapply32_f__msmcread( thr, a );
      a += 4;
      len -= 4;
   }
   if (len == 0) return;

   if (len >= 2)
      tl_assert(aligned16(a));
   if (len >= 2) {
      zsm_sapply16_f__msmcread( thr, a );
      a += 2;
      len -= 2;
   }
   if (len == 0) return;

   if (len >= 1) {
      zsm_sapply08_f__msmcread( thr, a );
      
      len -= 1;
   }
   tl_assert(len == 0);
}

void libhb_Thr_resumes ( Thr* thr )
{
   if (0) VG_(printf)("resume %p\n", thr);
   tl_assert(thr);
   tl_assert(!thr->llexit_done);
   Filter__clear(thr->filter, "libhb_Thr_resumes");
   if (HG_(clo_history_level) == 1) {
      tl_assert(thr->local_Kws_n_stacks);
      if (VG_(sizeXA)( thr->local_Kws_n_stacks ) == 0)
         note_local_Kw_n_stack_for(thr);
   }
}



SO* admin_SO = NULL;

static SO* SO__Alloc ( void )
{
   SO* so = HG_(zalloc)( "libhb.SO__Alloc.1", sizeof(SO) );
   so->viR   = VtsID_INVALID;
   so->viW   = VtsID_INVALID;
   so->magic = SO_MAGIC;
   
   if (admin_SO) {
      tl_assert(admin_SO->admin_prev == NULL);
      admin_SO->admin_prev = so;
      so->admin_next = admin_SO;
   } else {
      so->admin_next = NULL;
   }
   so->admin_prev = NULL;
   admin_SO = so;
   
   return so;
}

static void SO__Dealloc ( SO* so )
{
   tl_assert(so);
   tl_assert(so->magic == SO_MAGIC);
   if (so->viR == VtsID_INVALID) {
      tl_assert(so->viW == VtsID_INVALID);
   } else {
      tl_assert(so->viW != VtsID_INVALID);
      VtsID__rcdec(so->viR);
      VtsID__rcdec(so->viW);
   }
   so->magic = 0;
   
   if (so->admin_prev)
      so->admin_prev->admin_next = so->admin_next;
   if (so->admin_next)
      so->admin_next->admin_prev = so->admin_prev;
   if (so == admin_SO)
      admin_SO = so->admin_next;
   
   HG_(free)( so );
}



static void show_thread_state ( const HChar* str, Thr* t ) 
{
   if (1) return;
   if (t->viR == t->viW) {
      VG_(printf)("thr \"%s\" %p has vi* %u==", str, t, t->viR );
      VtsID__pp( t->viR );
      VG_(printf)("%s","\n");
   } else {
      VG_(printf)("thr \"%s\" %p has viR %u==", str, t, t->viR );
      VtsID__pp( t->viR );
      VG_(printf)(" viW %u==", t->viW);
      VtsID__pp( t->viW );
      VG_(printf)("%s","\n");
   }
}


Thr* libhb_init (
        void        (*get_stacktrace)( Thr*, Addr*, UWord ),
        ExeContext* (*get_EC)( Thr* )
     )
{
   Thr*  thr;
   VtsID vi;

   
   
   STATIC_ASSERT(sizeof(ScalarTS) == 8);

   
   STATIC_ASSERT(SCALARTS_N_THRBITS >= 11);
   STATIC_ASSERT(SCALARTS_N_THRBITS <= 27);

   STATIC_ASSERT(   (sizeof(UWord) == 8 && sizeof(Thr_n_RCEC) == 16)
                 || (sizeof(UWord) == 4 && sizeof(Thr_n_RCEC) == 12));
   STATIC_ASSERT(sizeof(TSW) == sizeof(UInt));

   
   STATIC_ASSERT(sizeof(WordSetID) == 4);
   STATIC_ASSERT(sizeof(WordSet) == sizeof(WordSetID));

   tl_assert(get_stacktrace);
   tl_assert(get_EC);
   main_get_stacktrace   = get_stacktrace;
   main_get_EC           = get_EC;

   
   

   temp_max_sized_VTS = VTS__new( "libhb.libhb_init.1", ThrID_MAX_VALID );
   temp_max_sized_VTS->id = VtsID_INVALID;
   verydead_thread_tables_init();
   vts_set_init();
   vts_tab_init();
   event_map_init();
   VtsID__invalidate_caches();

   
   zsm_init( );

   thr = Thr__new();
   vi  = VtsID__mk_Singleton( thr, 1 );
   thr->viR = vi;
   thr->viW = vi;
   VtsID__rcinc(thr->viR);
   VtsID__rcinc(thr->viW);

   show_thread_state("  root", thr);
   return thr;
}


Thr* libhb_create ( Thr* parent )
{
   Thr* child = Thr__new();

   child->viR = VtsID__tick( parent->viR, child );
   child->viW = VtsID__tick( parent->viW, child );
   Filter__clear(child->filter, "libhb_create(child)");
   VtsID__rcinc(child->viR);
   VtsID__rcinc(child->viW);

   tl_assert(VtsID__indexAt( child->viR, child ) == 1);
   tl_assert(VtsID__indexAt( child->viW, child ) == 1);

   
   VtsID__rcdec(parent->viR);
   VtsID__rcdec(parent->viW);
   parent->viR = VtsID__tick( parent->viR, parent );
   parent->viW = VtsID__tick( parent->viW, parent );
   Filter__clear(parent->filter, "libhb_create(parent)");
   VtsID__rcinc(parent->viR);
   VtsID__rcinc(parent->viW);
   note_local_Kw_n_stack_for( parent );

   show_thread_state(" child", child);
   show_thread_state("parent", parent);

   return child;
}

void libhb_shutdown ( Bool show_stats )
{
   if (show_stats) {
      VG_(printf)("%s","<<< BEGIN libhb stats >>>\n");
      VG_(printf)(" secmaps: %'10lu allocd (%'12lu g-a-range)\n",
                  stats__secmaps_allocd,
                  stats__secmap_ga_space_covered);
      VG_(printf)("  linesZ: %'10lu allocd (%'12lu bytes occupied)\n",
                  stats__secmap_linesZ_allocd,
                  stats__secmap_linesZ_bytes);
      VG_(printf)("  linesF: %'10lu allocd (%'12lu bytes occupied)"
                  " (%'10lu used)\n",
                  VG_(sizePA) (LineF_pool_allocator),
                  VG_(sizePA) (LineF_pool_allocator) * sizeof(LineF),
                  shmem__SecMap_used_linesF());
      VG_(printf)(" secmaps: %'10lu in map (can be scanGCed %'5lu)"
                  " #%lu scanGC \n",
                  stats__secmaps_in_map_shmem,
                  shmem__SecMap_do_GC(False ),
                  stats__secmaps_scanGC);
      tl_assert (VG_(sizeFM) (map_shmem) == stats__secmaps_in_map_shmem);
      VG_(printf)(" secmaps: %'10lu in freelist,"
                  " total (scanGCed %'lu, ssetGCed %'lu)\n",
                  SecMap_freelist_length(),
                  stats__secmaps_scanGCed,
                  stats__secmaps_ssetGCed);
      VG_(printf)(" secmaps: %'10lu searches (%'12lu slow)\n",
                  stats__secmaps_search, stats__secmaps_search_slow);

      VG_(printf)("%s","\n");
      VG_(printf)("   cache: %'lu totrefs (%'lu misses)\n",
                  stats__cache_totrefs, stats__cache_totmisses );
      VG_(printf)("   cache: %'14lu Z-fetch,    %'14lu F-fetch\n",
                  stats__cache_Z_fetches, stats__cache_F_fetches );
      VG_(printf)("   cache: %'14lu Z-wback,    %'14lu F-wback\n",
                  stats__cache_Z_wbacks, stats__cache_F_wbacks );
      VG_(printf)("   cache: %'14lu flushes_invals\n",
                  stats__cache_flushes_invals );
      VG_(printf)("   cache: %'14llu arange_New  %'14llu direct-to-Zreps\n",
                  stats__cache_make_New_arange,
                  stats__cache_make_New_inZrep);

      VG_(printf)("%s","\n");
      VG_(printf)("   cline: %'10lu normalises\n",
                  stats__cline_normalises );
      VG_(printf)("   cline: c rds 8/4/2/1: %'13lu %'13lu %'13lu %'13lu\n",
                  stats__cline_cread64s,
                  stats__cline_cread32s,
                  stats__cline_cread16s,
                  stats__cline_cread08s );
      VG_(printf)("   cline: c wrs 8/4/2/1: %'13lu %'13lu %'13lu %'13lu\n",
                  stats__cline_cwrite64s,
                  stats__cline_cwrite32s,
                  stats__cline_cwrite16s,
                  stats__cline_cwrite08s );
      VG_(printf)("   cline: s wrs 8/4/2/1: %'13lu %'13lu %'13lu %'13lu\n",
                  stats__cline_swrite64s,
                  stats__cline_swrite32s,
                  stats__cline_swrite16s,
                  stats__cline_swrite08s );
      VG_(printf)("   cline: s rd1s %'lu, s copy1s %'lu\n",
                  stats__cline_sread08s, stats__cline_scopy08s );
      VG_(printf)("   cline:    splits: 8to4 %'12lu    4to2 %'12lu"
                  "    2to1 %'12lu\n",
                  stats__cline_64to32splits, stats__cline_32to16splits,
                  stats__cline_16to8splits );
      VG_(printf)("   cline: pulldowns: 8to4 %'12lu    4to2 %'12lu"
                  "    2to1 %'12lu\n",
                  stats__cline_64to32pulldown, stats__cline_32to16pulldown,
                  stats__cline_16to8pulldown );
      if (0)
      VG_(printf)("   cline: sizeof(CacheLineZ) %ld,"
                  " covers %ld bytes of arange\n",
                  (Word)sizeof(LineZ),
                  (Word)N_LINE_ARANGE);

      VG_(printf)("%s","\n");

      VG_(printf)("   libhb: %'13llu msmcread  (%'llu dragovers)\n",
                  stats__msmcread, stats__msmcread_change);
      VG_(printf)("   libhb: %'13llu msmcwrite (%'llu dragovers)\n",
                  stats__msmcwrite, stats__msmcwrite_change);
      VG_(printf)("   libhb: %'13llu cmpLEQ queries (%'llu misses)\n",
                  stats__cmpLEQ_queries, stats__cmpLEQ_misses);
      VG_(printf)("   libhb: %'13llu join2  queries (%'llu misses)\n",
                  stats__join2_queries, stats__join2_misses);

      VG_(printf)("%s","\n");
      VG_(printf)("   libhb: VTSops: tick %'lu,  join %'lu,  cmpLEQ %'lu\n",
                  stats__vts__tick, stats__vts__join,  stats__vts__cmpLEQ );
      VG_(printf)("   libhb: VTSops: cmp_structural %'lu (%'lu slow)\n",
                  stats__vts__cmp_structural, stats__vts__cmp_structural_slow);
      VG_(printf)("   libhb: VTSset: find__or__clone_and_add %'lu"
                  " (%'lu allocd)\n",
                   stats__vts_set__focaa, stats__vts_set__focaa_a );
      VG_(printf)( "   libhb: VTSops: indexAt_SLOW %'lu\n",
                   stats__vts__indexat_slow );

      VG_(printf)("%s","\n");
      VG_(printf)(
         "   libhb: %ld entries in vts_table (approximately %lu bytes)\n",
         VG_(sizeXA)( vts_tab ), VG_(sizeXA)( vts_tab ) * sizeof(VtsTE)
      );
      VG_(printf)("   libhb: #%lu vts_tab GC    #%lu vts pruning\n",
                  stats__vts_tab_GC, stats__vts_pruning);
      VG_(printf)( "   libhb: %lu entries in vts_set\n",
                   VG_(sizeFM)( vts_set ) );

      VG_(printf)("%s","\n");
      {
         UInt live = 0;
         UInt llexit_done = 0;
         UInt joinedwith_done = 0;
         UInt llexit_and_joinedwith_done = 0;

         Thread* hgthread = get_admin_threads();
         tl_assert(hgthread);
         while (hgthread) {
            Thr* hbthr = hgthread->hbthr;
            tl_assert(hbthr);
            if (hbthr->llexit_done && hbthr->joinedwith_done)
               llexit_and_joinedwith_done++;
            else if (hbthr->llexit_done)
               llexit_done++;
            else if (hbthr->joinedwith_done)
               joinedwith_done++;
            else
               live++;
            hgthread = hgthread->admin;
         }
         VG_(printf)("   libhb: threads live: %d exit_and_joinedwith %d"
                     " exit %d joinedwith %d\n",
                     live, llexit_and_joinedwith_done,
                     llexit_done, joinedwith_done);
         VG_(printf)("   libhb: %d verydead_threads, "
                     "%d verydead_threads_not_pruned\n",
                     (int) VG_(sizeXA)( verydead_thread_table),
                     (int) VG_(sizeXA)( verydead_thread_table_not_pruned));
         tl_assert (VG_(sizeXA)( verydead_thread_table)
                    + VG_(sizeXA)( verydead_thread_table_not_pruned)
                    == llexit_and_joinedwith_done);
      }

      VG_(printf)("%s","\n");
      VG_(printf)( "   libhb: oldrefHTN %lu (%'d bytes)\n",
                   oldrefHTN, (int)(oldrefHTN * sizeof(OldRef)));
      tl_assert (oldrefHTN == VG_(HT_count_nodes) (oldrefHT));
      VG_(printf)( "   libhb: oldref lookup found=%lu notfound=%lu\n",
                   stats__evm__lookup_found, stats__evm__lookup_notfound);
      if (VG_(clo_verbosity) > 1)
         VG_(HT_print_stats) (oldrefHT, cmp_oldref_tsw);
      VG_(printf)( "   libhb: oldref bind tsw/rcec "
                   "==/==:%lu ==/!=:%lu !=/!=:%lu\n",
                   stats__ctxt_eq_tsw_eq_rcec, stats__ctxt_eq_tsw_neq_rcec,
                   stats__ctxt_neq_tsw_neq_rcec);
      VG_(printf)( "   libhb: ctxt__rcdec calls %lu. rcec gc discards %lu\n",
                   stats__ctxt_rcdec_calls, stats__ctxt_rcec_gc_discards);
      VG_(printf)( "   libhb: contextTab: %lu slots,"
                   " %lu cur ents(ref'd %lu),"
                   " %lu max ents\n",
                   (UWord)N_RCEC_TAB,
                   stats__ctxt_tab_curr, RCEC_referenced,
                   stats__ctxt_tab_max );
      {
#        define  MAXCHAIN 10
         UInt chains[MAXCHAIN+1]; 
         UInt non0chain = 0;
         UInt n;
         UInt i;
         RCEC *p;

         for (i = 0; i <= MAXCHAIN; i++) chains[i] = 0;
         for (i = 0; i < N_RCEC_TAB; i++) {
            n = 0;
            for (p = contextTab[i]; p; p = p->next)
               n++;
            if (n < MAXCHAIN)
               chains[n]++;
            else
               chains[MAXCHAIN]++;
            if (n > 0)
               non0chain++;
         }
         VG_(printf)( "   libhb: contextTab chain of [length]=nchain."
                      " Avg chain len %3.1f\n"
                      "        ",
                      (Double)stats__ctxt_tab_curr
                      / (Double)(non0chain ? non0chain : 1));
         for (i = 0; i <= MAXCHAIN; i++) {
            if (chains[i] != 0)
                VG_(printf)( "[%d%s]=%d ",
                             i, i == MAXCHAIN ? "+" : "",
                             chains[i]);
         }
         VG_(printf)( "\n");
#        undef MAXCHAIN
      }
      VG_(printf)( "   libhb: contextTab: %lu queries, %lu cmps\n",
                   stats__ctxt_tab_qs,
                   stats__ctxt_tab_cmps );
#if 0
      VG_(printf)("sizeof(AvlNode)     = %lu\n", sizeof(AvlNode));
      VG_(printf)("sizeof(WordBag)     = %lu\n", sizeof(WordBag));
      VG_(printf)("sizeof(MaybeWord)   = %lu\n", sizeof(MaybeWord));
      VG_(printf)("sizeof(CacheLine)   = %lu\n", sizeof(CacheLine));
      VG_(printf)("sizeof(LineZ)       = %lu\n", sizeof(LineZ));
      VG_(printf)("sizeof(LineF)       = %lu\n", sizeof(LineF));
      VG_(printf)("sizeof(SecMap)      = %lu\n", sizeof(SecMap));
      VG_(printf)("sizeof(Cache)       = %lu\n", sizeof(Cache));
      VG_(printf)("sizeof(SMCacheEnt)  = %lu\n", sizeof(SMCacheEnt));
      VG_(printf)("sizeof(CountedSVal) = %lu\n", sizeof(CountedSVal));
      VG_(printf)("sizeof(VTS)         = %lu\n", sizeof(VTS));
      VG_(printf)("sizeof(ScalarTS)    = %lu\n", sizeof(ScalarTS));
      VG_(printf)("sizeof(VtsTE)       = %lu\n", sizeof(VtsTE));
      VG_(printf)("sizeof(MSMInfo)     = %lu\n", sizeof(MSMInfo));

      VG_(printf)("sizeof(struct _XArray)     = %lu\n", sizeof(struct _XArray));
      VG_(printf)("sizeof(struct _WordFM)     = %lu\n", sizeof(struct _WordFM));
      VG_(printf)("sizeof(struct _Thr)     = %lu\n", sizeof(struct _Thr));
      VG_(printf)("sizeof(struct _SO)     = %lu\n", sizeof(struct _SO));
#endif

      VG_(printf)("%s","<<< END libhb stats >>>\n");
      VG_(printf)("%s","\n");

   }
}

void libhb_async_exit ( Thr* thr )
{
   tl_assert(thr);
   tl_assert(!thr->llexit_done);
   thr->llexit_done = True;

   tl_assert(thr->filter);
   HG_(free)(thr->filter);
   thr->filter = NULL;

   if (thr->joinedwith_done)
      VTS__declare_thread_very_dead(thr);

   
   
   
}

void libhb_joinedwith_done ( Thr* thr )
{
   tl_assert(thr);
   
   tl_assert(!thr->joinedwith_done);
   thr->joinedwith_done = True;
   if (thr->llexit_done)
      VTS__declare_thread_very_dead(thr);
}



SO* libhb_so_alloc ( void )
{
   return SO__Alloc();
}

void libhb_so_dealloc ( SO* so )
{
   tl_assert(so);
   tl_assert(so->magic == SO_MAGIC);
   SO__Dealloc(so);
}

void libhb_so_send ( Thr* thr, SO* so, Bool strong_send )
{

   tl_assert(so);
   tl_assert(so->magic == SO_MAGIC);

   { Bool leq = VtsID__cmpLEQ(thr->viW, thr->viR);
     tl_assert(leq);
   }

   if (so->viR == VtsID_INVALID) {
      tl_assert(so->viW == VtsID_INVALID);
      so->viR = thr->viR;
      so->viW = thr->viW;
      VtsID__rcinc(so->viR);
      VtsID__rcinc(so->viW);
   } else {
      tl_assert(so->viW != VtsID_INVALID);
      VtsID__rcdec(so->viR);
      VtsID__rcdec(so->viW);
      so->viR = strong_send ? thr->viR : VtsID__join2( so->viR, thr->viR );
      so->viW = strong_send ? thr->viW : VtsID__join2( so->viW, thr->viW );
      VtsID__rcinc(so->viR);
      VtsID__rcinc(so->viW);
   }

   
   VtsID__rcdec(thr->viR);
   VtsID__rcdec(thr->viW);
   thr->viR = VtsID__tick( thr->viR, thr );
   thr->viW = VtsID__tick( thr->viW, thr );
   if (!thr->llexit_done) {
      Filter__clear(thr->filter, "libhb_so_send");
      note_local_Kw_n_stack_for(thr);
   }
   VtsID__rcinc(thr->viR);
   VtsID__rcinc(thr->viW);

   if (strong_send)
      show_thread_state("s-send", thr);
   else
      show_thread_state("w-send", thr);
}

void libhb_so_recv ( Thr* thr, SO* so, Bool strong_recv )
{
   tl_assert(so);
   tl_assert(so->magic == SO_MAGIC);

   if (so->viR != VtsID_INVALID) {
      tl_assert(so->viW != VtsID_INVALID);

      VtsID__rcdec(thr->viR);
      thr->viR = VtsID__join2( thr->viR, so->viR );
      VtsID__rcinc(thr->viR);

      
      
      

      if (strong_recv) {
         VtsID__rcdec(thr->viW);
         thr->viW = VtsID__join2( thr->viW, so->viW );
         VtsID__rcinc(thr->viW);

         
         
         
         
      }

      if (thr->filter)
         Filter__clear(thr->filter, "libhb_so_recv");
      note_local_Kw_n_stack_for(thr);

      if (strong_recv) 
         show_thread_state("s-recv", thr);
      else 
         show_thread_state("w-recv", thr);

   } else {
      tl_assert(so->viW == VtsID_INVALID);
      show_thread_state("d-recv", thr);
   }
}

Bool libhb_so_everSent ( SO* so )
{
   if (so->viR == VtsID_INVALID) {
      tl_assert(so->viW == VtsID_INVALID);
      return False;
   } else {
      tl_assert(so->viW != VtsID_INVALID);
      return True;
   }
}

#define XXX1 0 
#define XXX2 0

static inline Bool TRACEME(Addr a, SizeT szB) {
   if (XXX1 && a <= XXX1 && XXX1 <= a+szB) return True;
   if (XXX2 && a <= XXX2 && XXX2 <= a+szB) return True;
   return False;
}
static void trace ( Thr* thr, Addr a, SizeT szB, const HChar* s ) 
{
  SVal sv = zsm_sread08(a);
  VG_(printf)("thr %p (%#lx,%lu) %s: 0x%016llx ", thr,a,szB,s,sv);
  show_thread_state("", thr);
  VG_(printf)("%s","\n");
}

void libhb_srange_new ( Thr* thr, Addr a, SizeT szB )
{
   SVal sv = SVal__mkC(thr->viW, thr->viW);
   tl_assert(is_sane_SVal_C(sv));
   if (0 && TRACEME(a,szB)) trace(thr,a,szB,"nw-before");
   zsm_sset_range( a, szB, sv );
   Filter__clear_range( thr->filter, a, szB );
   if (0 && TRACEME(a,szB)) trace(thr,a,szB,"nw-after ");
}

void libhb_srange_noaccess_NoFX ( Thr* thr, Addr a, SizeT szB )
{
   
}


static void zsm_secmap_line_range_noaccess (SecMap *sm,
                                            UInt zix_start, UInt zix_end)
{
   for (UInt lz = zix_start; lz <= zix_end; lz++) {
      LineZ* lineZ;
      lineZ = &sm->linesZ[lz];
      if (lineZ->dict[0] != SVal_INVALID) {
         rcdec_LineZ(lineZ);
         lineZ->dict[0] = SVal_NOACCESS;
         lineZ->dict[1] = lineZ->dict[2] = lineZ->dict[3] = SVal_INVALID;
      } else {
         clear_LineF_of_Z(lineZ);
      }
      for (UInt i = 0; i < N_LINE_ARANGE/4; i++)
         lineZ->ix2s[i] = 0; 
   }
}

static void zsm_sset_range_noaccess_in_secmap(Addr a, SizeT len)
{
   tl_assert (is_valid_scache_tag (a));
   tl_assert (0 == (len & (N_LINE_ARANGE - 1)));
   tl_assert (len < N_SECMAP_ARANGE);

   SecMap *sm1 = shmem__find_SecMap (a);
   SecMap *sm2 = shmem__find_SecMap (a + len - 1);
   UWord zix_start = shmem__get_SecMap_offset(a          ) >> N_LINE_BITS;
   UWord zix_end   = shmem__get_SecMap_offset(a + len - 1) >> N_LINE_BITS;

   if (sm1) {
      if (CHECK_ZSM) tl_assert(is_sane_SecMap(sm1));
      zsm_secmap_line_range_noaccess (sm1, zix_start,
                                      sm1 == sm2 ? zix_end : N_SECMAP_ZLINES-1);
   }
   if (sm2 && sm1 != sm2) {
      if (CHECK_ZSM) tl_assert(is_sane_SecMap(sm2));
      zsm_secmap_line_range_noaccess (sm2, 0, zix_end);
   }
}

static void zsm_sset_range_noaccess (Addr addr, SizeT len)
{
   Addr ARE = addr + len;
   Addr BPC = addr;
   Addr BFC = ROUNDUP(BPC, N_LINE_ARANGE);
   Addr FSM = ROUNDUP(BPC, N_SECMAP_ARANGE);
   Addr AFC = ROUNDDN(ARE, N_SECMAP_ARANGE);
   Addr APC = ROUNDDN(ARE, N_LINE_ARANGE);
   SizeT Plen = len; 
   SizeT BPClen;
   SizeT BFClen;
   SizeT FSMlen;
   SizeT AFClen;
   SizeT APClen;

#  define PlenCONSUME(from, to, round, consumed) \
   do {                                          \
   if (from < to) {                              \
      if (to - from < Plen)                      \
         consumed = to - from;                   \
      else                                       \
         consumed = ROUNDDN(Plen, round);        \
   } else {                                      \
      consumed = 0;                              \
   }                                             \
   Plen -= consumed; } while (0)
      
   PlenCONSUME(BPC, BFC, 1,               BPClen);
   PlenCONSUME(BFC, FSM, N_LINE_ARANGE,   BFClen);
   PlenCONSUME(FSM, AFC, N_SECMAP_ARANGE, FSMlen);
   PlenCONSUME(AFC, APC, N_LINE_ARANGE,   AFClen);
   PlenCONSUME(APC, ARE, 1,               APClen);

   if (0)
      VG_(printf) ("addr %p[%ld] ARE %p"
                   " BPC %p[%ld] BFC %p[%ld] FSM %p[%ld]"
                   " AFC %p[%ld] APC %p[%ld]\n",
                   (void*)addr, len, (void*)ARE,
                   (void*)BPC, BPClen, (void*)BFC, BFClen, (void*)FSM, FSMlen,
                   (void*)AFC, AFClen, (void*)APC, APClen);

   tl_assert (Plen == 0);

   

   
   if (BPClen > 0)
      zsm_sset_range_SMALL (BPC, BPClen, SVal_NOACCESS);
   if (APClen > 0)
      zsm_sset_range_SMALL (APC, APClen, SVal_NOACCESS);

   shmem__invalidate_scache_range (BFC, APC - BFC);

   if (BFClen > 0)
      zsm_sset_range_noaccess_in_secmap (BFC, BFClen);
   if (AFClen > 0)
      zsm_sset_range_noaccess_in_secmap (AFC, AFClen);

   if (FSMlen > 0) {
      Addr  sm_start = FSM;
      while (sm_start < AFC) {
         SecMap *sm = shmem__find_SecMap (sm_start);
         if (sm) {
            Addr gaKey;
            SecMap *fm_sm;

            if (CHECK_ZSM) tl_assert(is_sane_SecMap(sm));
            for (UInt lz = 0; lz < N_SECMAP_ZLINES; lz++) {
               LineZ *lineZ = &sm->linesZ[lz];
               if (LIKELY(lineZ->dict[0] != SVal_INVALID))
                  rcdec_LineZ(lineZ);
               else
                  clear_LineF_of_Z(lineZ);
            }
            if (!VG_(delFromFM)(map_shmem, &gaKey, (UWord*)&fm_sm, sm_start))
               tl_assert (0);
            stats__secmaps_in_map_shmem--;
            tl_assert (gaKey == sm_start);
            tl_assert (sm == fm_sm);
            stats__secmaps_ssetGCed++;
            push_SecMap_on_freelist (sm);
         }
         sm_start += N_SECMAP_ARANGE;
      }
      tl_assert (sm_start == AFC);

      if (address_in_range(smCache[0].gaKey, FSM, FSMlen)) {
         smCache[0].gaKey = 1;
         smCache[0].sm = NULL;
      }
      if (address_in_range(smCache[1].gaKey, FSM, FSMlen)) {
         smCache[1].gaKey = 1;
         smCache[1].sm = NULL;
      }
      if (address_in_range(smCache[2].gaKey, FSM, FSMlen)) {
         smCache[2].gaKey = 1;
         smCache[2].sm = NULL;
      }
      STATIC_ASSERT (3 == sizeof(smCache)/sizeof(SMCacheEnt));
   }
}

void libhb_srange_noaccess_AHAE ( Thr* thr, Addr a, SizeT szB )
{
   SVal sv = SVal_NOACCESS;
   tl_assert(is_sane_SVal_C(sv));
   if (LIKELY(szB < 2 * N_LINE_ARANGE))
      zsm_sset_range_SMALL (a, szB, SVal_NOACCESS);
   else
      zsm_sset_range_noaccess (a, szB);
   Filter__clear_range( thr->filter, a, szB );
}

UWord libhb_srange_get_abits (Addr a, UChar *abits, SizeT len)
{
   UWord anr = 0; 

   for (SizeT i = 0; i < len; i++) {
      SVal       sv = SVal_INVALID;
      Addr       b = a + i;
      Addr       tag = b & ~(N_LINE_ARANGE - 1);
      UWord      wix = (b >> N_LINE_BITS) & (N_WAY_NENT - 1);
      UWord      cloff = get_cacheline_offset(b);

      if (tag == cache_shmem.tags0[wix]) {
         CacheLine copy = cache_shmem.lyns0[wix];
         UWord toff = get_tree_offset(b); 
         UWord tno  = get_treeno(b);
         UShort descr = copy.descrs[tno];
         if (UNLIKELY( !(descr & (TREE_DESCR_8_0 << toff)) )) {
            SVal* tree = &copy.svals[tno << 3];
            copy.descrs[tno] = pulldown_to_8(tree, toff, descr);
         }
         sv = copy.svals[cloff];
      } else {
         
         SecMap *sm = shmem__find_SecMap(b);
         LineZ *lineZ;
         if (sm == NULL)
            sv = SVal_NOACCESS;
         else {
            UWord zix = shmem__get_SecMap_offset(b) >> N_LINE_BITS;
            lineZ = &sm->linesZ[zix];
            if (lineZ->dict[0] == SVal_INVALID) {
               LineF *lineF = SVal2Ptr(lineZ->dict[1]);
               sv = lineF->w64s[cloff];
            } else {
               UWord ix = read_twobit_array( lineZ->ix2s, cloff );
               sv = lineZ->dict[ix];
            }
         }
      }
      
      tl_assert (sv != SVal_INVALID);
      if (sv == SVal_NOACCESS) {
         if (abits)
            abits[i] = 0x00;
      } else {
         if (abits)
            abits[i] = 0xff;
         anr++;
      }
   }

   return anr;
}


void libhb_srange_untrack ( Thr* thr, Addr a, SizeT szB )
{
   SVal sv = SVal_NOACCESS;
   tl_assert(is_sane_SVal_C(sv));
   if (0 && TRACEME(a,szB)) trace(thr,a,szB,"untrack-before");
   if (LIKELY(szB < 2 * N_LINE_ARANGE))
      zsm_sset_range_SMALL (a, szB, SVal_NOACCESS);
   else
      zsm_sset_range_noaccess (a, szB);
   Filter__clear_range( thr->filter, a, szB );
   if (0 && TRACEME(a,szB)) trace(thr,a,szB,"untrack-after ");
}

Thread* libhb_get_Thr_hgthread ( Thr* thr ) {
   tl_assert(thr);
   return thr->hgthread;
}

void libhb_set_Thr_hgthread ( Thr* thr, Thread* hgthread ) {
   tl_assert(thr);
   thr->hgthread = hgthread;
}

void libhb_copy_shadow_state ( Thr* thr, Addr src, Addr dst, SizeT len )
{
   zsm_scopy_range(src, dst, len);
   Filter__clear_range( thr->filter, dst, len ); 
}

void libhb_maybe_GC ( void )
{
   if (UNLIKELY(stats__ctxt_tab_curr > N_RCEC_TAB/2
                && stats__ctxt_tab_curr + 1000 >= stats__ctxt_tab_max
                && (stats__ctxt_tab_curr * 3)/4 > RCEC_referenced))
      do_RCEC_GC();

   Bool vts_tab_GC = vts_tab_freelist == VtsID_INVALID
      && VG_(sizeXA)( vts_tab ) >= vts_next_GC_at;
   if (UNLIKELY (vts_tab_GC))
      vts_tab__do_GC( False );

   if (UNLIKELY(SecMap_freelist == NULL
                && stats__secmaps_in_map_shmem >= next_SecMap_GC_at)) {
      
      if (!vts_tab_GC)
         zsm_flush_cache();
      shmem__SecMap_do_GC(True);
   }

   
   if (CHECK_CEM)
      event_map__check_reference_counts();
}



