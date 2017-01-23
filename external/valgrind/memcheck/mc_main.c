

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
#include "pub_tool_aspacemgr.h"
#include "pub_tool_gdbserver.h"
#include "pub_tool_poolalloc.h"
#include "pub_tool_hashtable.h"     
#include "pub_tool_libcbase.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_machine.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_options.h"
#include "pub_tool_oset.h"
#include "pub_tool_rangemap.h"
#include "pub_tool_replacemalloc.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_threadstate.h"

#include "mc_include.h"
#include "memcheck.h"   


#define VG_DEBUG_MEMORY 0

#define DEBUG(fmt, args...) 

static void ocache_sarp_Set_Origins ( Addr, UWord, UInt ); 
static void ocache_sarp_Clear_Origins ( Addr, UWord ); 


 

#define PERF_FAST_LOADV    1
#define PERF_FAST_STOREV   1

#define PERF_FAST_SARP     1

#define PERF_FAST_STACK    1
#define PERF_FAST_STACK2   1

#define OC_ENABLE_ASSERTIONS 0











#if VG_WORDSIZE == 4

#  define N_PRIMARY_BITS  16

#else

#  define N_PRIMARY_BITS  20

#endif


#define N_PRIMARY_MAP  ( ((UWord)1) << N_PRIMARY_BITS)

#define MAX_PRIMARY_ADDRESS (Addr)((((Addr)65536) * N_PRIMARY_MAP)-1)




#define VA_BITS2_NOACCESS     0x0      
#define VA_BITS2_UNDEFINED    0x1      
#define VA_BITS2_DEFINED      0x2      
#define VA_BITS2_PARTDEFINED  0x3      

#define VA_BITS4_NOACCESS     0x0      
#define VA_BITS4_UNDEFINED    0x5      
#define VA_BITS4_DEFINED      0xa      

#define VA_BITS8_NOACCESS     0x00     
#define VA_BITS8_UNDEFINED    0x55     
#define VA_BITS8_DEFINED      0xaa     

#define VA_BITS16_NOACCESS    0x0000   
#define VA_BITS16_UNDEFINED   0x5555   
#define VA_BITS16_DEFINED     0xaaaa   


#define SM_CHUNKS             16384
#define SM_OFF(aaa)           (((aaa) & 0xffff) >> 2)
#define SM_OFF_16(aaa)        (((aaa) & 0xffff) >> 3)

#define INLINE    inline __attribute__((always_inline))

static INLINE Addr start_of_this_sm ( Addr a ) {
   return (a & (~SM_MASK));
}
static INLINE Bool is_start_of_sm ( Addr a ) {
   return (start_of_this_sm(a) == a);
}

typedef 
   struct {
      UChar vabits8[SM_CHUNKS];
   }
   SecMap;

#define SM_DIST_NOACCESS   0
#define SM_DIST_UNDEFINED  1
#define SM_DIST_DEFINED    2

static SecMap sm_distinguished[3];

static INLINE Bool is_distinguished_sm ( SecMap* sm ) {
   return sm >= &sm_distinguished[0] && sm <= &sm_distinguished[2];
}

static void update_SM_counts(SecMap* oldSM, SecMap* newSM);

static SecMap* copy_for_writing ( SecMap* dist_sm )
{
   SecMap* new_sm;
   tl_assert(dist_sm == &sm_distinguished[0]
          || dist_sm == &sm_distinguished[1]
          || dist_sm == &sm_distinguished[2]);

   new_sm = VG_(am_shadow_alloc)(sizeof(SecMap));
   if (new_sm == NULL)
      VG_(out_of_memory_NORETURN)( "memcheck:allocate new SecMap", 
                                   sizeof(SecMap) );
   VG_(memcpy)(new_sm, dist_sm, sizeof(SecMap));
   update_SM_counts(dist_sm, new_sm);
   return new_sm;
}


static Int   n_issued_SMs      = 0;
static Int   n_deissued_SMs    = 0;
static Int   n_noaccess_SMs    = N_PRIMARY_MAP; 
static Int   n_undefined_SMs   = 0;
static Int   n_defined_SMs     = 0;
static Int   n_non_DSM_SMs     = 0;
static Int   max_noaccess_SMs  = 0;
static Int   max_undefined_SMs = 0;
static Int   max_defined_SMs   = 0;
static Int   max_non_DSM_SMs   = 0;

static ULong n_auxmap_L1_searches  = 0;
static ULong n_auxmap_L1_cmps      = 0;
static ULong n_auxmap_L2_searches  = 0;
static ULong n_auxmap_L2_nodes     = 0;

static Int   n_sanity_cheap     = 0;
static Int   n_sanity_expensive = 0;

static Int   n_secVBit_nodes   = 0;
static Int   max_secVBit_nodes = 0;

static void update_SM_counts(SecMap* oldSM, SecMap* newSM)
{
   if      (oldSM == &sm_distinguished[SM_DIST_NOACCESS ]) n_noaccess_SMs --;
   else if (oldSM == &sm_distinguished[SM_DIST_UNDEFINED]) n_undefined_SMs--;
   else if (oldSM == &sm_distinguished[SM_DIST_DEFINED  ]) n_defined_SMs  --;
   else                                                  { n_non_DSM_SMs  --;
                                                           n_deissued_SMs ++; }

   if      (newSM == &sm_distinguished[SM_DIST_NOACCESS ]) n_noaccess_SMs ++;
   else if (newSM == &sm_distinguished[SM_DIST_UNDEFINED]) n_undefined_SMs++;
   else if (newSM == &sm_distinguished[SM_DIST_DEFINED  ]) n_defined_SMs  ++;
   else                                                  { n_non_DSM_SMs  ++;
                                                           n_issued_SMs   ++; }

   if (n_noaccess_SMs  > max_noaccess_SMs ) max_noaccess_SMs  = n_noaccess_SMs;
   if (n_undefined_SMs > max_undefined_SMs) max_undefined_SMs = n_undefined_SMs;
   if (n_defined_SMs   > max_defined_SMs  ) max_defined_SMs   = n_defined_SMs;
   if (n_non_DSM_SMs   > max_non_DSM_SMs  ) max_non_DSM_SMs   = n_non_DSM_SMs;   
}


static SecMap* primary_map[N_PRIMARY_MAP];


typedef
   struct { 
      Addr    base;
      SecMap* sm;
   }
   AuxMapEnt;

#define N_AUXMAP_L1 24

#define AUXMAP_L1_INSERT_IX 12

static struct {
          Addr       base;
          AuxMapEnt* ent; 
       } 
       auxmap_L1[N_AUXMAP_L1];

static OSet* auxmap_L2 = NULL;

static void init_auxmap_L1_L2 ( void )
{
   Int i;
   for (i = 0; i < N_AUXMAP_L1; i++) {
      auxmap_L1[i].base = 0;
      auxmap_L1[i].ent  = NULL;
   }

   tl_assert(0 == offsetof(AuxMapEnt,base));
   tl_assert(sizeof(Addr) == sizeof(void*));
   auxmap_L2 = VG_(OSetGen_Create)(   offsetof(AuxMapEnt,base),
                                     NULL,
                                    VG_(malloc), "mc.iaLL.1", VG_(free) );
}


static const HChar* check_auxmap_L1_L2_sanity ( Word* n_secmaps_found )
{
   Word i, j;
   *n_secmaps_found = 0;
   if (sizeof(void*) == 4) {
      
      if (VG_(OSetGen_Size)(auxmap_L2) != 0)
         return "32-bit: auxmap_L2 is non-empty";
      for (i = 0; i < N_AUXMAP_L1; i++) 
        if (auxmap_L1[i].base != 0 || auxmap_L1[i].ent != NULL)
      return "32-bit: auxmap_L1 is non-empty";
   } else {
      
      UWord elems_seen = 0;
      AuxMapEnt *elem, *res;
      AuxMapEnt key;
      
      VG_(OSetGen_ResetIter)(auxmap_L2);
      while ( (elem = VG_(OSetGen_Next)(auxmap_L2)) ) {
         elems_seen++;
         if (0 != (elem->base & (Addr)0xFFFF))
            return "64-bit: nonzero .base & 0xFFFF in auxmap_L2";
         if (elem->base <= MAX_PRIMARY_ADDRESS)
            return "64-bit: .base <= MAX_PRIMARY_ADDRESS in auxmap_L2";
         if (elem->sm == NULL)
            return "64-bit: .sm in _L2 is NULL";
         if (!is_distinguished_sm(elem->sm))
            (*n_secmaps_found)++;
      }
      if (elems_seen != n_auxmap_L2_nodes)
         return "64-bit: disagreement on number of elems in _L2";
      
      for (i = 0; i < N_AUXMAP_L1; i++) {
         if (auxmap_L1[i].base == 0 && auxmap_L1[i].ent == NULL)
            continue;
         if (0 != (auxmap_L1[i].base & (Addr)0xFFFF))
            return "64-bit: nonzero .base & 0xFFFF in auxmap_L1";
         if (auxmap_L1[i].base <= MAX_PRIMARY_ADDRESS)
            return "64-bit: .base <= MAX_PRIMARY_ADDRESS in auxmap_L1";
         if (auxmap_L1[i].ent == NULL)
            return "64-bit: .ent is NULL in auxmap_L1";
         if (auxmap_L1[i].ent->base != auxmap_L1[i].base)
            return "64-bit: _L1 and _L2 bases are inconsistent";
         
         key.base = auxmap_L1[i].base;
         key.sm   = 0;
         res = VG_(OSetGen_Lookup)(auxmap_L2, &key);
         if (res == NULL)
            return "64-bit: _L1 .base not found in _L2";
         if (res != auxmap_L1[i].ent)
            return "64-bit: _L1 .ent disagrees with _L2 entry";
      }
      
      for (i = 0; i < N_AUXMAP_L1; i++) {
         if (auxmap_L1[i].base == 0)
            continue;
	 for (j = i+1; j < N_AUXMAP_L1; j++) {
            if (auxmap_L1[j].base == 0)
               continue;
            if (auxmap_L1[j].base == auxmap_L1[i].base)
               return "64-bit: duplicate _L1 .base entries";
         }
      }
   }
   return NULL; 
}

static void insert_into_auxmap_L1_at ( Word rank, AuxMapEnt* ent )
{
   Word i;
   tl_assert(ent);
   tl_assert(rank >= 0 && rank < N_AUXMAP_L1);
   for (i = N_AUXMAP_L1-1; i > rank; i--)
      auxmap_L1[i] = auxmap_L1[i-1];
   auxmap_L1[rank].base = ent->base;
   auxmap_L1[rank].ent  = ent;
}

static INLINE AuxMapEnt* maybe_find_in_auxmap ( Addr a )
{
   AuxMapEnt  key;
   AuxMapEnt* res;
   Word       i;

   tl_assert(a > MAX_PRIMARY_ADDRESS);
   a &= ~(Addr)0xFFFF;


   if (LIKELY(auxmap_L1[0].base == a))
      return auxmap_L1[0].ent;
   if (LIKELY(auxmap_L1[1].base == a)) {
      Addr       t_base = auxmap_L1[0].base;
      AuxMapEnt* t_ent  = auxmap_L1[0].ent;
      auxmap_L1[0].base = auxmap_L1[1].base;
      auxmap_L1[0].ent  = auxmap_L1[1].ent;
      auxmap_L1[1].base = t_base;
      auxmap_L1[1].ent  = t_ent;
      return auxmap_L1[0].ent;
   }

   n_auxmap_L1_searches++;

   for (i = 0; i < N_AUXMAP_L1; i++) {
      if (auxmap_L1[i].base == a) {
         break;
      }
   }
   tl_assert(i >= 0 && i <= N_AUXMAP_L1);

   n_auxmap_L1_cmps += (ULong)(i+1);

   if (i < N_AUXMAP_L1) {
      if (i > 0) {
         Addr       t_base = auxmap_L1[i-1].base;
         AuxMapEnt* t_ent  = auxmap_L1[i-1].ent;
         auxmap_L1[i-1].base = auxmap_L1[i-0].base;
         auxmap_L1[i-1].ent  = auxmap_L1[i-0].ent;
         auxmap_L1[i-0].base = t_base;
         auxmap_L1[i-0].ent  = t_ent;
         i--;
      }
      return auxmap_L1[i].ent;
   }

   n_auxmap_L2_searches++;

   
   key.base = a;
   key.sm   = 0;

   res = VG_(OSetGen_Lookup)(auxmap_L2, &key);
   if (res)
      insert_into_auxmap_L1_at( AUXMAP_L1_INSERT_IX, res );
   return res;
}

static AuxMapEnt* find_or_alloc_in_auxmap ( Addr a )
{
   AuxMapEnt *nyu, *res;

   
   res = maybe_find_in_auxmap( a );
   if (LIKELY(res))
      return res;

   a &= ~(Addr)0xFFFF;

   nyu = (AuxMapEnt*) VG_(OSetGen_AllocNode)( auxmap_L2, sizeof(AuxMapEnt) );
   nyu->base = a;
   nyu->sm   = &sm_distinguished[SM_DIST_NOACCESS];
   VG_(OSetGen_Insert)( auxmap_L2, nyu );
   insert_into_auxmap_L1_at( AUXMAP_L1_INSERT_IX, nyu );
   n_auxmap_L2_nodes++;
   return nyu;
}



static INLINE SecMap** get_secmap_low_ptr ( Addr a )
{
   UWord pm_off = a >> 16;
#  if VG_DEBUG_MEMORY >= 1
   tl_assert(pm_off < N_PRIMARY_MAP);
#  endif
   return &primary_map[ pm_off ];
}

static INLINE SecMap** get_secmap_high_ptr ( Addr a )
{
   AuxMapEnt* am = find_or_alloc_in_auxmap(a);
   return &am->sm;
}

static INLINE SecMap** get_secmap_ptr ( Addr a )
{
   return ( a <= MAX_PRIMARY_ADDRESS 
          ? get_secmap_low_ptr(a) 
          : get_secmap_high_ptr(a));
}

static INLINE SecMap* get_secmap_for_reading_low ( Addr a )
{
   return *get_secmap_low_ptr(a);
}

static INLINE SecMap* get_secmap_for_reading_high ( Addr a )
{
   return *get_secmap_high_ptr(a);
}

static INLINE SecMap* get_secmap_for_writing_low(Addr a)
{
   SecMap** p = get_secmap_low_ptr(a);
   if (UNLIKELY(is_distinguished_sm(*p)))
      *p = copy_for_writing(*p);
   return *p;
}

static INLINE SecMap* get_secmap_for_writing_high ( Addr a )
{
   SecMap** p = get_secmap_high_ptr(a);
   if (UNLIKELY(is_distinguished_sm(*p)))
      *p = copy_for_writing(*p);
   return *p;
}

static INLINE SecMap* get_secmap_for_reading ( Addr a )
{
   return ( a <= MAX_PRIMARY_ADDRESS
          ? get_secmap_for_reading_low (a)
          : get_secmap_for_reading_high(a) );
}

static INLINE SecMap* get_secmap_for_writing ( Addr a )
{
   return ( a <= MAX_PRIMARY_ADDRESS
          ? get_secmap_for_writing_low (a)
          : get_secmap_for_writing_high(a) );
}

static SecMap* maybe_get_secmap_for ( Addr a )
{
   if (a <= MAX_PRIMARY_ADDRESS) {
      return get_secmap_for_reading_low(a);
   } else {
      AuxMapEnt* am = maybe_find_in_auxmap(a);
      return am ? am->sm : NULL;
   }
}


static INLINE
void insert_vabits2_into_vabits8 ( Addr a, UChar vabits2, UChar* vabits8 )
{
   UInt shift =  (a & 3)  << 1;        
   *vabits8  &= ~(0x3     << shift);   
   *vabits8  |=  (vabits2 << shift);   
}

static INLINE
void insert_vabits4_into_vabits8 ( Addr a, UChar vabits4, UChar* vabits8 )
{
   UInt shift;
   tl_assert(VG_IS_2_ALIGNED(a));      
   shift     =  (a & 2)   << 1;        
   *vabits8 &= ~(0xf      << shift);   
   *vabits8 |=  (vabits4 << shift);    
}

static INLINE
UChar extract_vabits2_from_vabits8 ( Addr a, UChar vabits8 )
{
   UInt shift = (a & 3) << 1;          
   vabits8 >>= shift;                  
   return 0x3 & vabits8;               
}

static INLINE
UChar extract_vabits4_from_vabits8 ( Addr a, UChar vabits8 )
{
   UInt shift;
   tl_assert(VG_IS_2_ALIGNED(a));      
   shift = (a & 2) << 1;               
   vabits8 >>= shift;                  
   return 0xf & vabits8;               
}


static INLINE
void set_vabits2 ( Addr a, UChar vabits2 )
{
   SecMap* sm       = get_secmap_for_writing(a);
   UWord   sm_off   = SM_OFF(a);
   insert_vabits2_into_vabits8( a, vabits2, &(sm->vabits8[sm_off]) );
}

static INLINE
UChar get_vabits2 ( Addr a )
{
   SecMap* sm       = get_secmap_for_reading(a);
   UWord   sm_off   = SM_OFF(a);
   UChar   vabits8  = sm->vabits8[sm_off];
   return extract_vabits2_from_vabits8(a, vabits8);
}

static INLINE
UChar get_vabits8_for_aligned_word32 ( Addr a )
{
   SecMap* sm       = get_secmap_for_reading(a);
   UWord   sm_off   = SM_OFF(a);
   UChar   vabits8  = sm->vabits8[sm_off];
   return vabits8;
}

static INLINE
void set_vabits8_for_aligned_word32 ( Addr a, UChar vabits8 )
{
   SecMap* sm       = get_secmap_for_writing(a);
   UWord   sm_off   = SM_OFF(a);
   sm->vabits8[sm_off] = vabits8;
}


static UWord get_sec_vbits8(Addr a);
static void  set_sec_vbits8(Addr a, UWord vbits8);

static INLINE
Bool set_vbits8 ( Addr a, UChar vbits8 )
{
   Bool  ok      = True;
   UChar vabits2 = get_vabits2(a);
   if ( VA_BITS2_NOACCESS != vabits2 ) {
      
      
      
      if      ( V_BITS8_DEFINED   == vbits8 ) { vabits2 = VA_BITS2_DEFINED;   }
      else if ( V_BITS8_UNDEFINED == vbits8 ) { vabits2 = VA_BITS2_UNDEFINED; }
      else                                    { vabits2 = VA_BITS2_PARTDEFINED;
                                                set_sec_vbits8(a, vbits8);  }
      set_vabits2(a, vabits2);

   } else {
      
      
      
      ok = False;
   }
   return ok;
}

static INLINE
Bool get_vbits8 ( Addr a, UChar* vbits8 )
{
   Bool  ok      = True;
   UChar vabits2 = get_vabits2(a);

   
   if      ( VA_BITS2_DEFINED   == vabits2 ) { *vbits8 = V_BITS8_DEFINED;   }
   else if ( VA_BITS2_UNDEFINED == vabits2 ) { *vbits8 = V_BITS8_UNDEFINED; }
   else if ( VA_BITS2_NOACCESS  == vabits2 ) {
      *vbits8 = V_BITS8_DEFINED;    
      ok = False;
   } else {
      tl_assert( VA_BITS2_PARTDEFINED == vabits2 );
      *vbits8 = get_sec_vbits8(a);
   }
   return ok;
}



// stale but shortly afterwards is rewritten with a PDB and so becomes

static OSet* secVBitTable;

static ULong sec_vbits_new_nodes = 0;
static ULong sec_vbits_updates   = 0;

#define BYTES_PER_SEC_VBIT_NODE     16

#define STEPUP_SURVIVOR_PROPORTION  0.5
#define STEPUP_GROWTH_FACTOR        1.414213562

#define DRIFTUP_SURVIVOR_PROPORTION 0.15
#define DRIFTUP_GROWTH_FACTOR       1.015
#define DRIFTUP_MAX_SIZE            80000

static Int  secVBitLimit = 1000;

static UInt GCs_done = 0;

typedef 
   struct {
      Addr  a;
      UChar vbits8[BYTES_PER_SEC_VBIT_NODE];
   } 
   SecVBitNode;

static OSet* createSecVBitTable(void)
{
   OSet* newSecVBitTable;
   newSecVBitTable = VG_(OSetGen_Create_With_Pool)
      ( offsetof(SecVBitNode, a), 
        NULL, 
        VG_(malloc), "mc.cSVT.1 (sec VBit table)", 
        VG_(free),
        1000,
        sizeof(SecVBitNode));
   return newSecVBitTable;
}

static void gcSecVBitTable(void)
{
   OSet*        secVBitTable2;
   SecVBitNode* n;
   Int          i, n_nodes = 0, n_survivors = 0;

   GCs_done++;

   
   secVBitTable2 = createSecVBitTable();

   
   VG_(OSetGen_ResetIter)(secVBitTable);
   while ( (n = VG_(OSetGen_Next)(secVBitTable)) ) {
      
      
      
      for (i = 0; i < BYTES_PER_SEC_VBIT_NODE; i++) {
         if (VA_BITS2_PARTDEFINED == get_vabits2(n->a + i)) {
            
            
            SecVBitNode* n2 = 
               VG_(OSetGen_AllocNode)(secVBitTable2, sizeof(SecVBitNode));
            *n2 = *n;
            VG_(OSetGen_Insert)(secVBitTable2, n2);
            break;
         }
      }
   }

   
   n_nodes     = VG_(OSetGen_Size)(secVBitTable);
   n_survivors = VG_(OSetGen_Size)(secVBitTable2);

   
   VG_(OSetGen_Destroy)(secVBitTable);
   secVBitTable = secVBitTable2;

   if (VG_(clo_verbosity) > 1 && n_nodes != 0) {
      VG_(message)(Vg_DebugMsg, "memcheck GC: %d nodes, %d survivors (%.1f%%)\n",
                   n_nodes, n_survivors, n_survivors * 100.0 / n_nodes);
   }

   
   if ((Double)n_survivors 
       > ((Double)secVBitLimit * STEPUP_SURVIVOR_PROPORTION)) {
      secVBitLimit = (Int)((Double)secVBitLimit * (Double)STEPUP_GROWTH_FACTOR);
      if (VG_(clo_verbosity) > 1)
         VG_(message)(Vg_DebugMsg,
                      "memcheck GC: %d new table size (stepup)\n",
                      secVBitLimit);
   }
   else
   if (secVBitLimit < DRIFTUP_MAX_SIZE
       && (Double)n_survivors 
          > ((Double)secVBitLimit * DRIFTUP_SURVIVOR_PROPORTION)) {
      secVBitLimit = (Int)((Double)secVBitLimit * (Double)DRIFTUP_GROWTH_FACTOR);
      if (VG_(clo_verbosity) > 1)
         VG_(message)(Vg_DebugMsg,
                      "memcheck GC: %d new table size (driftup)\n",
                      secVBitLimit);
   }
}

static UWord get_sec_vbits8(Addr a)
{
   Addr         aAligned = VG_ROUNDDN(a, BYTES_PER_SEC_VBIT_NODE);
   Int          amod     = a % BYTES_PER_SEC_VBIT_NODE;
   SecVBitNode* n        = VG_(OSetGen_Lookup)(secVBitTable, &aAligned);
   UChar        vbits8;
   tl_assert2(n, "get_sec_vbits8: no node for address %p (%p)\n", aAligned, a);
   
   
   vbits8 = n->vbits8[amod];
   tl_assert(V_BITS8_DEFINED != vbits8 && V_BITS8_UNDEFINED != vbits8);
   return vbits8;
}

static void set_sec_vbits8(Addr a, UWord vbits8)
{
   Addr         aAligned = VG_ROUNDDN(a, BYTES_PER_SEC_VBIT_NODE);
   Int          i, amod  = a % BYTES_PER_SEC_VBIT_NODE;
   SecVBitNode* n        = VG_(OSetGen_Lookup)(secVBitTable, &aAligned);
   
   
   tl_assert(V_BITS8_DEFINED != vbits8 && V_BITS8_UNDEFINED != vbits8);
   if (n) {
      n->vbits8[amod] = vbits8;     
      sec_vbits_updates++;
   } else {
      
      
      if (secVBitLimit == VG_(OSetGen_Size)(secVBitTable)) {
         gcSecVBitTable();
      }

      
      
      n = VG_(OSetGen_AllocNode)(secVBitTable, sizeof(SecVBitNode));
      n->a            = aAligned;
      for (i = 0; i < BYTES_PER_SEC_VBIT_NODE; i++) {
         n->vbits8[i] = V_BITS8_UNDEFINED;
      }
      n->vbits8[amod] = vbits8;

      
      VG_(OSetGen_Insert)(secVBitTable, n);
      sec_vbits_new_nodes++;

      n_secVBit_nodes = VG_(OSetGen_Size)(secVBitTable);
      if (n_secVBit_nodes > max_secVBit_nodes)
         max_secVBit_nodes = n_secVBit_nodes;
   }
}


static INLINE UWord byte_offset_w ( UWord wordszB, Bool bigendian, 
                                    UWord byteno ) {
   return bigendian ? (wordszB-1-byteno) : byteno;
}



typedef
   enum { IAR_INVALID=99,
          IAR_NotIgnored,
          IAR_CommandLine,
          IAR_ClientReq }
   IARKind;

static const HChar* showIARKind ( IARKind iark )
{
   switch (iark) {
      case IAR_INVALID:     return "INVALID";
      case IAR_NotIgnored:  return "NotIgnored";
      case IAR_CommandLine: return "CommandLine";
      case IAR_ClientReq:   return "ClientReq";
      default:              return "???";
   }
}

static RangeMap* gIgnoredAddressRanges = NULL;

static void init_gIgnoredAddressRanges ( void )
{
   if (LIKELY(gIgnoredAddressRanges != NULL))
      return;
   gIgnoredAddressRanges = VG_(newRangeMap)( VG_(malloc), "mc.igIAR.1",
                                             VG_(free), IAR_NotIgnored );
}

Bool MC_(in_ignored_range) ( Addr a )
{
   if (LIKELY(gIgnoredAddressRanges == NULL))
      return False;
   UWord how     = IAR_INVALID;
   UWord key_min = ~(UWord)0;
   UWord key_max =  (UWord)0;
   VG_(lookupRangeMap)(&key_min, &key_max, &how, gIgnoredAddressRanges, a);
   tl_assert(key_min <= a && a <= key_max);
   switch (how) {
      case IAR_NotIgnored:  return False;
      case IAR_CommandLine: return True;
      case IAR_ClientReq:   return True;
      default: break; 
   }
   VG_(tool_panic)("MC_(in_ignore_range)");
   
}


static Bool parse_range ( const HChar** ppc, Addr* result1, Addr* result2 )
{
   Bool ok = VG_(parse_Addr) (ppc, result1);
   if (!ok)
      return False;
   if (**ppc != '-')
      return False;
   (*ppc)++;
   ok = VG_(parse_Addr) (ppc, result2);
   if (!ok)
      return False;
   return True;
}

static Bool parse_ignore_ranges ( const HChar* str0 )
{
   init_gIgnoredAddressRanges();
   const HChar*  str = str0;
   const HChar** ppc = &str;
   while (1) {
      Addr start = ~(Addr)0;
      Addr end   = (Addr)0;
      Bool ok    = parse_range(ppc, &start, &end);
      if (!ok)
         return False;
      if (start > end)
         return False;
      VG_(bindRangeMap)( gIgnoredAddressRanges, start, end, IAR_CommandLine );
      if (**ppc == 0)
         return True;
      if (**ppc != ',')
         return False;
      (*ppc)++;
   }
   
   return False;
}

static Bool modify_ignore_ranges ( Bool addRange, Addr start, Addr len )
{
   init_gIgnoredAddressRanges();
   const Bool verbose = (VG_(clo_verbosity) > 1);
   if (len == 0) {
      return False;
   }
   if (addRange) {
      VG_(bindRangeMap)(gIgnoredAddressRanges,
                        start, start+len-1, IAR_ClientReq);
      if (verbose)
         VG_(dmsg)("memcheck: modify_ignore_ranges: add %p %p\n",
                   (void*)start, (void*)(start+len-1));
   } else {
      VG_(bindRangeMap)(gIgnoredAddressRanges,
                        start, start+len-1, IAR_NotIgnored);
      if (verbose)
         VG_(dmsg)("memcheck: modify_ignore_ranges: del %p %p\n",
                   (void*)start, (void*)(start+len-1));
   }
   if (verbose) {
      VG_(dmsg)("memcheck:   now have %ld ranges:\n",
                VG_(sizeRangeMap)(gIgnoredAddressRanges));
      Word i;
      for (i = 0; i < VG_(sizeRangeMap)(gIgnoredAddressRanges); i++) {
         UWord val     = IAR_INVALID;
         UWord key_min = ~(UWord)0;
         UWord key_max = (UWord)0;
         VG_(indexRangeMap)( &key_min, &key_max, &val,
                             gIgnoredAddressRanges, i );
         VG_(dmsg)("memcheck:      [%ld]  %016llx-%016llx  %s\n",
                   i, (ULong)key_min, (ULong)key_max, showIARKind(val));
      }
   }
   return True;
}



static
__attribute__((noinline))
void mc_LOADV_128_or_256_slow ( ULong* res,
                                Addr a, SizeT nBits, Bool bigendian )
{
   ULong  pessim[4];     
   SSizeT szB            = nBits / 8;
   SSizeT szL            = szB / 8;  
   SSizeT i, j;          
   SizeT  n_addrs_bad = 0;
   Addr   ai;
   UChar  vbits8;
   Bool   ok;

   tl_assert((szB & (szB-1)) == 0 && szL > 0);

   tl_assert(szL <= sizeof(pessim) / sizeof(pessim[0]));

   for (j = 0; j < szL; j++) {
      pessim[j] = V_BITS64_DEFINED;
      res[j] = V_BITS64_UNDEFINED;
   }

   for (j = szL-1; j >= 0; j--) {
      ULong vbits64    = V_BITS64_UNDEFINED;
      ULong pessim64   = V_BITS64_DEFINED;
      UWord long_index = byte_offset_w(szL, bigendian, j);
      for (i = 8-1; i >= 0; i--) {
         PROF_EVENT(29, "mc_LOADV_128_or_256_slow(loop)");
         ai = a + 8*long_index + byte_offset_w(8, bigendian, i);
         ok = get_vbits8(ai, &vbits8);
         vbits64 <<= 8;
         vbits64 |= vbits8;
         if (!ok) n_addrs_bad++;
         pessim64 <<= 8;
         pessim64 |= (ok ? V_BITS8_DEFINED : V_BITS8_UNDEFINED);
      }
      res[long_index] = vbits64;
      pessim[long_index] = pessim64;
   }

   if (LIKELY(n_addrs_bad == 0))
      return;

   if (!MC_(clo_partial_loads_ok)) {
      MC_(record_address_error)( VG_(get_running_tid)(), a, szB, False );
      return;
   }



   
   ok = False;
   for (j = 0; j < szL; j++)
      ok |= pessim[j] != V_BITS64_DEFINED;
   tl_assert(ok);

   if (0 == (a & (szB - 1)) && n_addrs_bad < szB) {
      
      tl_assert(V_BIT_UNDEFINED == 1 && V_BIT_DEFINED == 0);
      for (j = szL-1; j >= 0; j--)
         res[j] |= pessim[j];
      return;
   }

   MC_(record_address_error)( VG_(get_running_tid)(), a, szB, False );
}


static
__attribute__((noinline))
ULong mc_LOADVn_slow ( Addr a, SizeT nBits, Bool bigendian )
{
   PROF_EVENT(30, "mc_LOADVn_slow");

   
   if (LIKELY(sizeof(void*) == 8 
                      && nBits == 64 && VG_IS_8_ALIGNED(a))) {
      SecMap* sm       = get_secmap_for_reading(a);
      UWord   sm_off16 = SM_OFF_16(a);
      UWord   vabits16 = ((UShort*)(sm->vabits8))[sm_off16];
      if (LIKELY(vabits16 == VA_BITS16_DEFINED))
         return V_BITS64_DEFINED;
      if (LIKELY(vabits16 == VA_BITS16_UNDEFINED))
         return V_BITS64_UNDEFINED;
      
   }
   if (LIKELY(sizeof(void*) == 8 
                      && nBits == 32 && VG_IS_4_ALIGNED(a))) {
      SecMap* sm = get_secmap_for_reading(a);
      UWord sm_off = SM_OFF(a);
      UWord vabits8 = sm->vabits8[sm_off];
      if (LIKELY(vabits8 == VA_BITS8_DEFINED))
         return ((UWord)0xFFFFFFFF00000000ULL | (UWord)V_BITS32_DEFINED);
      if (LIKELY(vabits8 == VA_BITS8_UNDEFINED))
         return ((UWord)0xFFFFFFFF00000000ULL | (UWord)V_BITS32_UNDEFINED);
      
   }
   

   ULong  vbits64     = V_BITS64_UNDEFINED; 
   ULong  pessim64    = V_BITS64_DEFINED;   
   SSizeT szB         = nBits / 8;
   SSizeT i;          
   SizeT  n_addrs_bad = 0;
   Addr   ai;
   UChar  vbits8;
   Bool   ok;

   tl_assert(nBits == 64 || nBits == 32 || nBits == 16 || nBits == 8);

   for (i = szB-1; i >= 0; i--) {
      PROF_EVENT(31, "mc_LOADVn_slow(loop)");
      ai = a + byte_offset_w(szB, bigendian, i);
      ok = get_vbits8(ai, &vbits8);
      vbits64 <<= 8; 
      vbits64 |= vbits8;
      if (!ok) n_addrs_bad++;
      pessim64 <<= 8;
      pessim64 |= (ok ? V_BITS8_DEFINED : V_BITS8_UNDEFINED);
   }

   if (LIKELY(n_addrs_bad == 0))
      return vbits64;

   if (!MC_(clo_partial_loads_ok)) {
      MC_(record_address_error)( VG_(get_running_tid)(), a, szB, False );
      return vbits64;
   }



   
   tl_assert(pessim64 != V_BITS64_DEFINED);

   if (szB == VG_WORDSIZE && VG_IS_WORD_ALIGNED(a)
       && n_addrs_bad < VG_WORDSIZE) {
      
      tl_assert(V_BIT_UNDEFINED == 1 && V_BIT_DEFINED == 0);
      vbits64 |= pessim64;
      return vbits64;
   }

   if (VG_WORDSIZE == 8
       && VG_IS_4_ALIGNED(a) && nBits == 32 && n_addrs_bad < 4) {
      tl_assert(V_BIT_UNDEFINED == 1 && V_BIT_DEFINED == 0);
      vbits64 |= pessim64;
      vbits64 |= (((ULong)V_BITS32_UNDEFINED) << 32);
      return vbits64;
   }

   MC_(record_address_error)( VG_(get_running_tid)(), a, szB, False );

   return vbits64;
}


static
__attribute__((noinline))
void mc_STOREVn_slow ( Addr a, SizeT nBits, ULong vbytes, Bool bigendian )
{
   SizeT szB = nBits / 8;
   SizeT i, n_addrs_bad = 0;
   UChar vbits8;
   Addr  ai;
   Bool  ok;

   PROF_EVENT(35, "mc_STOREVn_slow");

   
   if (LIKELY(sizeof(void*) == 8 
                      && nBits == 64 && VG_IS_8_ALIGNED(a))) {
      SecMap* sm       = get_secmap_for_reading(a);
      UWord   sm_off16 = SM_OFF_16(a);
      UWord   vabits16 = ((UShort*)(sm->vabits8))[sm_off16];
      if (LIKELY( !is_distinguished_sm(sm) && 
                          (VA_BITS16_DEFINED   == vabits16 ||
                           VA_BITS16_UNDEFINED == vabits16) )) {
         
         
         
         if (LIKELY(V_BITS64_DEFINED == vbytes)) {
            ((UShort*)(sm->vabits8))[sm_off16] = (UShort)VA_BITS16_DEFINED;
            return;
         } else if (V_BITS64_UNDEFINED == vbytes) {
            ((UShort*)(sm->vabits8))[sm_off16] = (UShort)VA_BITS16_UNDEFINED;
            return;
         }
         
      }
      
   }
   if (LIKELY(sizeof(void*) == 8
                      && nBits == 32 && VG_IS_4_ALIGNED(a))) {
      SecMap* sm      = get_secmap_for_reading(a);
      UWord   sm_off  = SM_OFF(a);
      UWord   vabits8 = sm->vabits8[sm_off];
      if (LIKELY( !is_distinguished_sm(sm) && 
                          (VA_BITS8_DEFINED   == vabits8 ||
                           VA_BITS8_UNDEFINED == vabits8) )) {
         
         
         
         if (LIKELY(V_BITS32_DEFINED == (vbytes & 0xFFFFFFFF))) {
            sm->vabits8[sm_off] = VA_BITS8_DEFINED;
            return;
         } else if (V_BITS32_UNDEFINED == (vbytes & 0xFFFFFFFF)) {
            sm->vabits8[sm_off] = VA_BITS8_UNDEFINED;
            return;
         }
         
      }
      
   }
   

   tl_assert(nBits == 64 || nBits == 32 || nBits == 16 || nBits == 8);

   for (i = 0; i < szB; i++) {
      PROF_EVENT(36, "mc_STOREVn_slow(loop)");
      ai     = a + byte_offset_w(szB, bigendian, i);
      vbits8 = vbytes & 0xff;
      ok     = set_vbits8(ai, vbits8);
      if (!ok) n_addrs_bad++;
      vbytes >>= 8;
   }

   
   if (n_addrs_bad > 0)
      MC_(record_address_error)( VG_(get_running_tid)(), a, szB, True );
}



static void set_address_range_perms ( Addr a, SizeT lenT, UWord vabits16,
                                      UWord dsm_num )
{
   UWord    sm_off, sm_off16;
   UWord    vabits2 = vabits16 & 0x3;
   SizeT    lenA, lenB, len_to_next_secmap;
   Addr     aNext;
   SecMap*  sm;
   SecMap** sm_ptr;
   SecMap*  example_dsm;

   PROF_EVENT(150, "set_address_range_perms");

   
   tl_assert(VA_BITS16_NOACCESS  == vabits16 ||
             VA_BITS16_UNDEFINED == vabits16 ||
             VA_BITS16_DEFINED   == vabits16);

   
   
   tl_assert(VA_BITS2_PARTDEFINED != vabits2);

   if (lenT == 0)
      return;

   if (lenT > 256 * 1024 * 1024) {
      if (VG_(clo_verbosity) > 0 && !VG_(clo_xml)) {
         const HChar* s = "unknown???";
         if (vabits16 == VA_BITS16_NOACCESS ) s = "noaccess";
         if (vabits16 == VA_BITS16_UNDEFINED) s = "undefined";
         if (vabits16 == VA_BITS16_DEFINED  ) s = "defined";
         VG_(message)(Vg_UserMsg, "Warning: set address range perms: "
                                  "large range [0x%lx, 0x%lx) (%s)\n",
                                  a, a + lenT, s);
      }
   }

#ifndef PERF_FAST_SARP
   
   {
      
      
      
      
      
      SizeT i;
      for (i = 0; i < lenT; i++) {
         set_vabits2(a + i, vabits2);
      }
      return;
   }
#endif

   

   example_dsm = &sm_distinguished[dsm_num];

   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   

   
   
   aNext = start_of_this_sm(a) + SM_SIZE;
   len_to_next_secmap = aNext - a;
   if ( lenT <= len_to_next_secmap ) {
      
      PROF_EVENT(151, "set_address_range_perms-single-secmap");
      lenA = lenT;
      lenB = 0;
   } else if (is_start_of_sm(a)) {
      
      
      PROF_EVENT(152, "set_address_range_perms-startof-secmap");
      lenA = 0;
      lenB = lenT;
      goto part2;
   } else {
      
      PROF_EVENT(153, "set_address_range_perms-multiple-secmaps");
      lenA = len_to_next_secmap;
      lenB = lenT - lenA;
   }

   
   
   
   
   
   

   
   sm_ptr = get_secmap_ptr(a);
   if (is_distinguished_sm(*sm_ptr)) {
      if (*sm_ptr == example_dsm) {
         
         PROF_EVENT(154, "set_address_range_perms-dist-sm1-quick");
         a    = aNext;
         lenA = 0;
      } else {
         PROF_EVENT(155, "set_address_range_perms-dist-sm1");
         *sm_ptr = copy_for_writing(*sm_ptr);
      }
   }
   sm = *sm_ptr;

   
   while (True) {
      if (VG_IS_8_ALIGNED(a)) break;
      if (lenA < 1)           break;
      PROF_EVENT(156, "set_address_range_perms-loop1a");
      sm_off = SM_OFF(a);
      insert_vabits2_into_vabits8( a, vabits2, &(sm->vabits8[sm_off]) );
      a    += 1;
      lenA -= 1;
   }
   
   while (True) {
      if (lenA < 8) break;
      PROF_EVENT(157, "set_address_range_perms-loop8a");
      sm_off16 = SM_OFF_16(a);
      ((UShort*)(sm->vabits8))[sm_off16] = vabits16;
      a    += 8;
      lenA -= 8;
   }
   
   while (True) {
      if (lenA < 1) break;
      PROF_EVENT(158, "set_address_range_perms-loop1b");
      sm_off = SM_OFF(a);
      insert_vabits2_into_vabits8( a, vabits2, &(sm->vabits8[sm_off]) );
      a    += 1;
      lenA -= 1;
   }

   
   if (lenB == 0)
      return;

   
   
   
  part2:
   
   
   tl_assert(0 == lenA);
   while (True) {
      if (lenB < SM_SIZE) break;
      tl_assert(is_start_of_sm(a));
      PROF_EVENT(159, "set_address_range_perms-loop64K");
      sm_ptr = get_secmap_ptr(a);
      if (!is_distinguished_sm(*sm_ptr)) {
         PROF_EVENT(160, "set_address_range_perms-loop64K-free-dist-sm");
         
         
         SysRes sres = VG_(am_munmap_valgrind)((Addr)*sm_ptr, sizeof(SecMap));
         tl_assert2(! sr_isError(sres), "SecMap valgrind munmap failure\n");
      }
      update_SM_counts(*sm_ptr, example_dsm);
      
      *sm_ptr = example_dsm;
      lenB -= SM_SIZE;
      a    += SM_SIZE;
   }

   
   if (lenB == 0)
      return;

   
   
   

   tl_assert(is_start_of_sm(a) && lenB < SM_SIZE);

   
   sm_ptr = get_secmap_ptr(a);
   if (is_distinguished_sm(*sm_ptr)) {
      if (*sm_ptr == example_dsm) {
         
         PROF_EVENT(161, "set_address_range_perms-dist-sm2-quick");
         return;
      } else {
         PROF_EVENT(162, "set_address_range_perms-dist-sm2");
         *sm_ptr = copy_for_writing(*sm_ptr);
      }
   }
   sm = *sm_ptr;

   
   while (True) {
      if (lenB < 8) break;
      PROF_EVENT(163, "set_address_range_perms-loop8b");
      sm_off16 = SM_OFF_16(a);
      ((UShort*)(sm->vabits8))[sm_off16] = vabits16;
      a    += 8;
      lenB -= 8;
   }
   
   while (True) {
      if (lenB < 1) return;
      PROF_EVENT(164, "set_address_range_perms-loop1c");
      sm_off = SM_OFF(a);
      insert_vabits2_into_vabits8( a, vabits2, &(sm->vabits8[sm_off]) );
      a    += 1;
      lenB -= 1;
   }
}



void MC_(make_mem_noaccess) ( Addr a, SizeT len )
{
   PROF_EVENT(40, "MC_(make_mem_noaccess)");
   DEBUG("MC_(make_mem_noaccess)(%p, %lu)\n", a, len);
   set_address_range_perms ( a, len, VA_BITS16_NOACCESS, SM_DIST_NOACCESS );
   if (UNLIKELY( MC_(clo_mc_level) == 3 ))
      ocache_sarp_Clear_Origins ( a, len );
}

static void make_mem_undefined ( Addr a, SizeT len )
{
   PROF_EVENT(41, "make_mem_undefined");
   DEBUG("make_mem_undefined(%p, %lu)\n", a, len);
   set_address_range_perms ( a, len, VA_BITS16_UNDEFINED, SM_DIST_UNDEFINED );
}

void MC_(make_mem_undefined_w_otag) ( Addr a, SizeT len, UInt otag )
{
   PROF_EVENT(43, "MC_(make_mem_undefined)");
   DEBUG("MC_(make_mem_undefined)(%p, %lu)\n", a, len);
   set_address_range_perms ( a, len, VA_BITS16_UNDEFINED, SM_DIST_UNDEFINED );
   if (UNLIKELY( MC_(clo_mc_level) == 3 ))
      ocache_sarp_Set_Origins ( a, len, otag );
}

static
void make_mem_undefined_w_tid_and_okind ( Addr a, SizeT len,
                                          ThreadId tid, UInt okind )
{
   UInt        ecu;
   ExeContext* here;
   tl_assert(okind <= 3);
   here = VG_(record_ExeContext)( tid, 0 );
   tl_assert(here);
   ecu = VG_(get_ECU_from_ExeContext)(here);
   tl_assert(VG_(is_plausible_ECU)(ecu));
   MC_(make_mem_undefined_w_otag) ( a, len, ecu | okind );
}

static
void mc_new_mem_w_tid_make_ECU  ( Addr a, SizeT len, ThreadId tid )
{
   make_mem_undefined_w_tid_and_okind ( a, len, tid, MC_OKIND_UNKNOWN );
}

static
void mc_new_mem_w_tid_no_ECU  ( Addr a, SizeT len, ThreadId tid )
{
   MC_(make_mem_undefined_w_otag) ( a, len, MC_OKIND_UNKNOWN );
}

void MC_(make_mem_defined) ( Addr a, SizeT len )
{
   PROF_EVENT(42, "MC_(make_mem_defined)");
   DEBUG("MC_(make_mem_defined)(%p, %lu)\n", a, len);
   set_address_range_perms ( a, len, VA_BITS16_DEFINED, SM_DIST_DEFINED );
   if (UNLIKELY( MC_(clo_mc_level) == 3 ))
      ocache_sarp_Clear_Origins ( a, len );
}

static void make_mem_defined_if_addressable ( Addr a, SizeT len )
{
   SizeT i;
   UChar vabits2;
   DEBUG("make_mem_defined_if_addressable(%p, %llu)\n", a, (ULong)len);
   for (i = 0; i < len; i++) {
      vabits2 = get_vabits2( a+i );
      if (LIKELY(VA_BITS2_NOACCESS != vabits2)) {
         set_vabits2(a+i, VA_BITS2_DEFINED);
         if (UNLIKELY(MC_(clo_mc_level) >= 3)) {
            MC_(helperc_b_store1)( a+i, 0 ); 
         } 
      }
   }
}

static void make_mem_defined_if_noaccess ( Addr a, SizeT len )
{
   SizeT i;
   UChar vabits2;
   DEBUG("make_mem_defined_if_noaccess(%p, %llu)\n", a, (ULong)len);
   for (i = 0; i < len; i++) {
      vabits2 = get_vabits2( a+i );
      if (LIKELY(VA_BITS2_NOACCESS == vabits2)) {
         set_vabits2(a+i, VA_BITS2_DEFINED);
         if (UNLIKELY(MC_(clo_mc_level) >= 3)) {
            MC_(helperc_b_store1)( a+i, 0 ); 
         } 
      }
   }
}


void MC_(copy_address_range_state) ( Addr src, Addr dst, SizeT len )
{
   SizeT i, j;
   UChar vabits2, vabits8;
   Bool  aligned, nooverlap;

   DEBUG("MC_(copy_address_range_state)\n");
   PROF_EVENT(50, "MC_(copy_address_range_state)");

   if (len == 0 || src == dst)
      return;

   aligned   = VG_IS_4_ALIGNED(src) && VG_IS_4_ALIGNED(dst);
   nooverlap = src+len <= dst || dst+len <= src;

   if (nooverlap && aligned) {

      
      
      i = 0;
      while (len >= 4) {
         vabits8 = get_vabits8_for_aligned_word32( src+i );
         set_vabits8_for_aligned_word32( dst+i, vabits8 );
         if (LIKELY(VA_BITS8_DEFINED == vabits8 
                            || VA_BITS8_UNDEFINED == vabits8 
                            || VA_BITS8_NOACCESS == vabits8)) {
            
         } else {
            
            if (VA_BITS2_PARTDEFINED == get_vabits2( src+i+0 ))
               set_sec_vbits8( dst+i+0, get_sec_vbits8( src+i+0 ) );
            if (VA_BITS2_PARTDEFINED == get_vabits2( src+i+1 ))
               set_sec_vbits8( dst+i+1, get_sec_vbits8( src+i+1 ) );
            if (VA_BITS2_PARTDEFINED == get_vabits2( src+i+2 ))
               set_sec_vbits8( dst+i+2, get_sec_vbits8( src+i+2 ) );
            if (VA_BITS2_PARTDEFINED == get_vabits2( src+i+3 ))
               set_sec_vbits8( dst+i+3, get_sec_vbits8( src+i+3 ) );
         }
         i += 4;
         len -= 4;
      }
      
      while (len >= 1) {
         vabits2 = get_vabits2( src+i );
         set_vabits2( dst+i, vabits2 );
         if (VA_BITS2_PARTDEFINED == vabits2) {
            set_sec_vbits8( dst+i, get_sec_vbits8( src+i ) );
         }
         i++;
         len--;
      }

   } else {

      
      if (src < dst) {
         for (i = 0, j = len-1; i < len; i++, j--) {
            PROF_EVENT(51, "MC_(copy_address_range_state)(loop)");
            vabits2 = get_vabits2( src+j );
            set_vabits2( dst+j, vabits2 );
            if (VA_BITS2_PARTDEFINED == vabits2) {
               set_sec_vbits8( dst+j, get_sec_vbits8( src+j ) );
            }
         }
      }

      if (src > dst) {
         for (i = 0; i < len; i++) {
            PROF_EVENT(52, "MC_(copy_address_range_state)(loop)");
            vabits2 = get_vabits2( src+i );
            set_vabits2( dst+i, vabits2 );
            if (VA_BITS2_PARTDEFINED == vabits2) {
               set_sec_vbits8( dst+i, get_sec_vbits8( src+i ) );
            }
         }
      }
   }

}




static UWord stats_ocacheL1_find           = 0;
static UWord stats_ocacheL1_found_at_1     = 0;
static UWord stats_ocacheL1_found_at_N     = 0;
static UWord stats_ocacheL1_misses         = 0;
static UWord stats_ocacheL1_lossage        = 0;
static UWord stats_ocacheL1_movefwds       = 0;

static UWord stats__ocacheL2_refs          = 0;
static UWord stats__ocacheL2_misses        = 0;
static UWord stats__ocacheL2_n_nodes_max   = 0;


#define OC_BITS_PER_LINE 5
#define OC_W32S_PER_LINE (1 << (OC_BITS_PER_LINE - 2))

static INLINE UWord oc_line_offset ( Addr a ) {
   return (a >> 2) & (OC_W32S_PER_LINE - 1);
}
static INLINE Bool is_valid_oc_tag ( Addr tag ) {
   return 0 == (tag & ((1 << OC_BITS_PER_LINE) - 1));
}

#define OC_LINES_PER_SET 2

#define OC_N_SET_BITS    20
#define OC_N_SETS        (1 << OC_N_SET_BITS)


#define OC_MOVE_FORWARDS_EVERY_BITS 7


typedef
   struct {
      Addr  tag;
      UInt  w32[OC_W32S_PER_LINE];
      UChar descr[OC_W32S_PER_LINE];
   }
   OCacheLine;

static UChar classify_OCacheLine ( OCacheLine* line )
{
   UWord i;
   if (line->tag == 1)
      return 'e'; 
   tl_assert(is_valid_oc_tag(line->tag));
   for (i = 0; i < OC_W32S_PER_LINE; i++) {
      tl_assert(0 == ((~0xF) & line->descr[i]));
      if (line->w32[i] > 0 && line->descr[i] > 0)
         return 'n'; 
   }
   return 'z'; 
}

typedef
   struct {
      OCacheLine line[OC_LINES_PER_SET];
   }
   OCacheSet;

typedef
   struct {
      OCacheSet set[OC_N_SETS];
   }
   OCache;

static OCache* ocacheL1 = NULL;
static UWord   ocacheL1_event_ctr = 0;

static void init_ocacheL2 ( void ); 
static void init_OCache ( void )
{
   UWord line, set;
   tl_assert(MC_(clo_mc_level) >= 3);
   tl_assert(ocacheL1 == NULL);
   ocacheL1 = VG_(am_shadow_alloc)(sizeof(OCache));
   if (ocacheL1 == NULL) {
      VG_(out_of_memory_NORETURN)( "memcheck:allocating ocacheL1", 
                                   sizeof(OCache) );
   }
   tl_assert(ocacheL1 != NULL);
   for (set = 0; set < OC_N_SETS; set++) {
      for (line = 0; line < OC_LINES_PER_SET; line++) {
         ocacheL1->set[set].line[line].tag = 1;
      }
   }
   init_ocacheL2();
}

static void moveLineForwards ( OCacheSet* set, UWord lineno )
{
   OCacheLine tmp;
   stats_ocacheL1_movefwds++;
   tl_assert(lineno > 0 && lineno < OC_LINES_PER_SET);
   tmp = set->line[lineno-1];
   set->line[lineno-1] = set->line[lineno];
   set->line[lineno] = tmp;
}

static void zeroise_OCacheLine ( OCacheLine* line, Addr tag ) {
   UWord i;
   for (i = 0; i < OC_W32S_PER_LINE; i++) {
      line->w32[i] = 0; 
      line->descr[i] = 0; 
   }
   line->tag = tag;
}


static OSet* ocacheL2 = NULL;

static void* ocacheL2_malloc ( const HChar* cc, SizeT szB ) {
   return VG_(malloc)(cc, szB);
}
static void ocacheL2_free ( void* v ) {
   VG_(free)( v );
}

static UWord stats__ocacheL2_n_nodes = 0;

static void init_ocacheL2 ( void )
{
   tl_assert(!ocacheL2);
   tl_assert(sizeof(Word) == sizeof(Addr)); 
   tl_assert(0 == offsetof(OCacheLine,tag));
   ocacheL2 
      = VG_(OSetGen_Create)( offsetof(OCacheLine,tag), 
                             NULL, 
                             ocacheL2_malloc, "mc.ioL2", ocacheL2_free);
   stats__ocacheL2_n_nodes = 0;
}

static OCacheLine* ocacheL2_find_tag ( Addr tag )
{
   OCacheLine* line;
   tl_assert(is_valid_oc_tag(tag));
   stats__ocacheL2_refs++;
   line = VG_(OSetGen_Lookup)( ocacheL2, &tag );
   return line;
}

static void ocacheL2_del_tag ( Addr tag )
{
   OCacheLine* line;
   tl_assert(is_valid_oc_tag(tag));
   stats__ocacheL2_refs++;
   line = VG_(OSetGen_Remove)( ocacheL2, &tag );
   if (line) {
      VG_(OSetGen_FreeNode)(ocacheL2, line);
      tl_assert(stats__ocacheL2_n_nodes > 0);
      stats__ocacheL2_n_nodes--;
   }
}

static void ocacheL2_add_line ( OCacheLine* line )
{
   OCacheLine* copy;
   tl_assert(is_valid_oc_tag(line->tag));
   copy = VG_(OSetGen_AllocNode)( ocacheL2, sizeof(OCacheLine) );
   *copy = *line;
   stats__ocacheL2_refs++;
   VG_(OSetGen_Insert)( ocacheL2, copy );
   stats__ocacheL2_n_nodes++;
   if (stats__ocacheL2_n_nodes > stats__ocacheL2_n_nodes_max)
      stats__ocacheL2_n_nodes_max = stats__ocacheL2_n_nodes;
}


__attribute__((noinline))
static OCacheLine* find_OCacheLine_SLOW ( Addr a )
{
   OCacheLine *victim, *inL2;
   UChar c;
   UWord line;
   UWord setno   = (a >> OC_BITS_PER_LINE) & (OC_N_SETS - 1);
   UWord tagmask = ~((1 << OC_BITS_PER_LINE) - 1);
   UWord tag     = a & tagmask;
   tl_assert(setno >= 0 && setno < OC_N_SETS);

   
   for (line = 1; line < OC_LINES_PER_SET; line++) {
      if (ocacheL1->set[setno].line[line].tag == tag) {
         if (line == 1) {
            stats_ocacheL1_found_at_1++;
         } else {
            stats_ocacheL1_found_at_N++;
         }
         if (UNLIKELY(0 == (ocacheL1_event_ctr++ 
                            & ((1<<OC_MOVE_FORWARDS_EVERY_BITS)-1)))) {
            moveLineForwards( &ocacheL1->set[setno], line );
            line--;
         }
         return &ocacheL1->set[setno].line[line];
      }
   }

   stats_ocacheL1_misses++;
   tl_assert(line == OC_LINES_PER_SET);
   line--;
   tl_assert(line > 0);

   
   victim = &ocacheL1->set[setno].line[line];
   c = classify_OCacheLine(victim);
   switch (c) {
      case 'e':
         
         break;
      case 'z':
         ocacheL2_del_tag( victim->tag );
         break;
      case 'n':
         stats_ocacheL1_lossage++;
         inL2 = ocacheL2_find_tag( victim->tag );
         if (inL2) {
            *inL2 = *victim;
         } else {
            ocacheL2_add_line( victim );
         }
         break;
      default:
         tl_assert(0);
   }

   tl_assert(tag != victim->tag); 
   inL2 = ocacheL2_find_tag( tag );
   if (inL2) {
      
      ocacheL1->set[setno].line[line] = *inL2;
   } else {
      stats__ocacheL2_misses++;
      zeroise_OCacheLine( &ocacheL1->set[setno].line[line], tag );
   }

   
   moveLineForwards( &ocacheL1->set[setno], line );
   line--;

   return &ocacheL1->set[setno].line[line];
}

static INLINE OCacheLine* find_OCacheLine ( Addr a )
{
   UWord setno   = (a >> OC_BITS_PER_LINE) & (OC_N_SETS - 1);
   UWord tagmask = ~((1 << OC_BITS_PER_LINE) - 1);
   UWord tag     = a & tagmask;

   stats_ocacheL1_find++;

   if (OC_ENABLE_ASSERTIONS) {
      tl_assert(setno >= 0 && setno < OC_N_SETS);
      tl_assert(0 == (tag & (4 * OC_W32S_PER_LINE - 1)));
   }

   if (LIKELY(ocacheL1->set[setno].line[0].tag == tag)) {
      return &ocacheL1->set[setno].line[0];
   }

   return find_OCacheLine_SLOW( a );
}

static INLINE void set_aligned_word64_Origin_to_undef ( Addr a, UInt otag )
{
   
   
   { OCacheLine* line;
     UWord lineoff = oc_line_offset(a);
     if (OC_ENABLE_ASSERTIONS) {
        tl_assert(lineoff >= 0 
                  && lineoff < OC_W32S_PER_LINE -1);
     }
     line = find_OCacheLine( a );
     line->descr[lineoff+0] = 0xF;
     line->descr[lineoff+1] = 0xF;
     line->w32[lineoff+0]   = otag;
     line->w32[lineoff+1]   = otag;
   }
   
}





static INLINE void make_aligned_word32_undefined ( Addr a )
{
   PROF_EVENT(300, "make_aligned_word32_undefined");

#ifndef PERF_FAST_STACK2
   make_mem_undefined(a, 4);
#else
   {
      UWord   sm_off;
      SecMap* sm;

      if (UNLIKELY(a > MAX_PRIMARY_ADDRESS)) {
         PROF_EVENT(301, "make_aligned_word32_undefined-slow1");
         make_mem_undefined(a, 4);
         return;
      }

      sm                  = get_secmap_for_writing_low(a);
      sm_off              = SM_OFF(a);
      sm->vabits8[sm_off] = VA_BITS8_UNDEFINED;
   }
#endif
}

static INLINE
void make_aligned_word32_undefined_w_otag ( Addr a, UInt otag )
{
   make_aligned_word32_undefined(a);
   
   
   { OCacheLine* line;
     UWord lineoff = oc_line_offset(a);
     if (OC_ENABLE_ASSERTIONS) {
        tl_assert(lineoff >= 0 && lineoff < OC_W32S_PER_LINE);
     }
     line = find_OCacheLine( a );
     line->descr[lineoff] = 0xF;
     line->w32[lineoff]   = otag;
   }
   
}

static INLINE
void make_aligned_word32_noaccess ( Addr a )
{
   PROF_EVENT(310, "make_aligned_word32_noaccess");

#ifndef PERF_FAST_STACK2
   MC_(make_mem_noaccess)(a, 4);
#else
   {
      UWord   sm_off;
      SecMap* sm;

      if (UNLIKELY(a > MAX_PRIMARY_ADDRESS)) {
         PROF_EVENT(311, "make_aligned_word32_noaccess-slow1");
         MC_(make_mem_noaccess)(a, 4);
         return;
      }

      sm                  = get_secmap_for_writing_low(a);
      sm_off              = SM_OFF(a);
      sm->vabits8[sm_off] = VA_BITS8_NOACCESS;

      
      
      if (UNLIKELY( MC_(clo_mc_level) == 3 )) {
         OCacheLine* line;
         UWord lineoff = oc_line_offset(a);
         if (OC_ENABLE_ASSERTIONS) {
            tl_assert(lineoff >= 0 && lineoff < OC_W32S_PER_LINE);
         }
         line = find_OCacheLine( a );
         line->descr[lineoff] = 0;
      }
      
   }
#endif
}



static INLINE void make_aligned_word64_undefined ( Addr a )
{
   PROF_EVENT(320, "make_aligned_word64_undefined");

#ifndef PERF_FAST_STACK2
   make_mem_undefined(a, 8);
#else
   {
      UWord   sm_off16;
      SecMap* sm;

      if (UNLIKELY(a > MAX_PRIMARY_ADDRESS)) {
         PROF_EVENT(321, "make_aligned_word64_undefined-slow1");
         make_mem_undefined(a, 8);
         return;
      }

      sm       = get_secmap_for_writing_low(a);
      sm_off16 = SM_OFF_16(a);
      ((UShort*)(sm->vabits8))[sm_off16] = VA_BITS16_UNDEFINED;
   }
#endif
}

static INLINE
void make_aligned_word64_undefined_w_otag ( Addr a, UInt otag )
{
   make_aligned_word64_undefined(a);
   
   
   { OCacheLine* line;
     UWord lineoff = oc_line_offset(a);
     tl_assert(lineoff >= 0 
               && lineoff < OC_W32S_PER_LINE -1);
     line = find_OCacheLine( a );
     line->descr[lineoff+0] = 0xF;
     line->descr[lineoff+1] = 0xF;
     line->w32[lineoff+0]   = otag;
     line->w32[lineoff+1]   = otag;
   }
   
}

static INLINE
void make_aligned_word64_noaccess ( Addr a )
{
   PROF_EVENT(330, "make_aligned_word64_noaccess");

#ifndef PERF_FAST_STACK2
   MC_(make_mem_noaccess)(a, 8);
#else
   {
      UWord   sm_off16;
      SecMap* sm;

      if (UNLIKELY(a > MAX_PRIMARY_ADDRESS)) {
         PROF_EVENT(331, "make_aligned_word64_noaccess-slow1");
         MC_(make_mem_noaccess)(a, 8);
         return;
      }

      sm       = get_secmap_for_writing_low(a);
      sm_off16 = SM_OFF_16(a);
      ((UShort*)(sm->vabits8))[sm_off16] = VA_BITS16_NOACCESS;

      
      
      if (UNLIKELY( MC_(clo_mc_level) == 3 )) {
         OCacheLine* line;
         UWord lineoff = oc_line_offset(a);
         tl_assert(lineoff >= 0 
                   && lineoff < OC_W32S_PER_LINE -1);
         line = find_OCacheLine( a );
         line->descr[lineoff+0] = 0;
         line->descr[lineoff+1] = 0;
      }
      
   }
#endif
}



#ifdef PERF_FAST_STACK
#  define MAYBE_USED
#else
#  define MAYBE_USED __attribute__((unused))
#endif


MAYBE_USED
static void VG_REGPARM(2) mc_new_mem_stack_4_w_ECU(Addr new_SP, UInt ecu)
{
   UInt otag = ecu | MC_OKIND_STACK;
   PROF_EVENT(110, "new_mem_stack_4");
   if (VG_IS_4_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word32_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP, otag );
   } else {
      MC_(make_mem_undefined_w_otag) ( -VG_STACK_REDZONE_SZB + new_SP, 4, otag );
   }
}

MAYBE_USED
static void VG_REGPARM(1) mc_new_mem_stack_4(Addr new_SP)
{
   PROF_EVENT(110, "new_mem_stack_4");
   if (VG_IS_4_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word32_undefined ( -VG_STACK_REDZONE_SZB + new_SP );
   } else {
      make_mem_undefined ( -VG_STACK_REDZONE_SZB + new_SP, 4 );
   }
}

MAYBE_USED
static void VG_REGPARM(1) mc_die_mem_stack_4(Addr new_SP)
{
   PROF_EVENT(120, "die_mem_stack_4");
   if (VG_IS_4_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word32_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-4 );
   } else {
      MC_(make_mem_noaccess) ( -VG_STACK_REDZONE_SZB + new_SP-4, 4 );
   }
}


MAYBE_USED
static void VG_REGPARM(2) mc_new_mem_stack_8_w_ECU(Addr new_SP, UInt ecu)
{
   UInt otag = ecu | MC_OKIND_STACK;
   PROF_EVENT(111, "new_mem_stack_8");
   if (VG_IS_8_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP, otag );
   } else if (VG_IS_4_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word32_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP  , otag );
      make_aligned_word32_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+4, otag );
   } else {
      MC_(make_mem_undefined_w_otag) ( -VG_STACK_REDZONE_SZB + new_SP, 8, otag );
   }
}

MAYBE_USED
static void VG_REGPARM(1) mc_new_mem_stack_8(Addr new_SP)
{
   PROF_EVENT(111, "new_mem_stack_8");
   if (VG_IS_8_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP );
   } else if (VG_IS_4_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word32_undefined ( -VG_STACK_REDZONE_SZB + new_SP );
      make_aligned_word32_undefined ( -VG_STACK_REDZONE_SZB + new_SP+4 );
   } else {
      make_mem_undefined ( -VG_STACK_REDZONE_SZB + new_SP, 8 );
   }
}

MAYBE_USED
static void VG_REGPARM(1) mc_die_mem_stack_8(Addr new_SP)
{
   PROF_EVENT(121, "die_mem_stack_8");
   if (VG_IS_8_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-8 );
   } else if (VG_IS_4_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word32_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-8 );
      make_aligned_word32_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-4 );
   } else {
      MC_(make_mem_noaccess) ( -VG_STACK_REDZONE_SZB + new_SP-8, 8 );
   }
}


MAYBE_USED
static void VG_REGPARM(2) mc_new_mem_stack_12_w_ECU(Addr new_SP, UInt ecu)
{
   UInt otag = ecu | MC_OKIND_STACK;
   PROF_EVENT(112, "new_mem_stack_12");
   if (VG_IS_8_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP  , otag );
      make_aligned_word32_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+8, otag );
   } else if (VG_IS_4_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word32_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP  , otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+4, otag );
   } else {
      MC_(make_mem_undefined_w_otag) ( -VG_STACK_REDZONE_SZB + new_SP, 12, otag );
   }
}

MAYBE_USED
static void VG_REGPARM(1) mc_new_mem_stack_12(Addr new_SP)
{
   PROF_EVENT(112, "new_mem_stack_12");
   if (VG_IS_8_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP );
      make_aligned_word32_undefined ( -VG_STACK_REDZONE_SZB + new_SP+8 );
   } else if (VG_IS_4_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word32_undefined ( -VG_STACK_REDZONE_SZB + new_SP );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+4 );
   } else {
      make_mem_undefined ( -VG_STACK_REDZONE_SZB + new_SP, 12 );
   }
}

MAYBE_USED
static void VG_REGPARM(1) mc_die_mem_stack_12(Addr new_SP)
{
   PROF_EVENT(122, "die_mem_stack_12");
   
   if (VG_IS_8_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP-12 )) {
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-12 );
      make_aligned_word32_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-4  );
   } else if (VG_IS_4_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word32_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-12 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-8  );
   } else {
      MC_(make_mem_noaccess) ( -VG_STACK_REDZONE_SZB + new_SP-12, 12 );
   }
}


MAYBE_USED
static void VG_REGPARM(2) mc_new_mem_stack_16_w_ECU(Addr new_SP, UInt ecu)
{
   UInt otag = ecu | MC_OKIND_STACK;
   PROF_EVENT(113, "new_mem_stack_16");
   if (VG_IS_8_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP  , otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+8, otag );
   } else if (VG_IS_4_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word32_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP   , otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+4 , otag );
      make_aligned_word32_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+12, otag );
   } else {
      MC_(make_mem_undefined_w_otag) ( -VG_STACK_REDZONE_SZB + new_SP, 16, otag );
   }
}

MAYBE_USED
static void VG_REGPARM(1) mc_new_mem_stack_16(Addr new_SP)
{
   PROF_EVENT(113, "new_mem_stack_16");
   if (VG_IS_8_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+8 );
   } else if (VG_IS_4_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word32_undefined ( -VG_STACK_REDZONE_SZB + new_SP );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+4  );
      make_aligned_word32_undefined ( -VG_STACK_REDZONE_SZB + new_SP+12 );
   } else {
      make_mem_undefined ( -VG_STACK_REDZONE_SZB + new_SP, 16 );
   }
}

MAYBE_USED
static void VG_REGPARM(1) mc_die_mem_stack_16(Addr new_SP)
{
   PROF_EVENT(123, "die_mem_stack_16");
   if (VG_IS_8_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-16 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-8  );
   } else if (VG_IS_4_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      
      make_aligned_word32_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-16 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-12 );
      make_aligned_word32_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-4  );
   } else {
      MC_(make_mem_noaccess) ( -VG_STACK_REDZONE_SZB + new_SP-16, 16 );
   }
}


MAYBE_USED
static void VG_REGPARM(2) mc_new_mem_stack_32_w_ECU(Addr new_SP, UInt ecu)
{
   UInt otag = ecu | MC_OKIND_STACK;
   PROF_EVENT(114, "new_mem_stack_32");
   if (VG_IS_8_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP   , otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+8 , otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+16, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+24, otag );
   } else if (VG_IS_4_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word32_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP   , otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+4 , otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+12, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+20, otag );
      make_aligned_word32_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+28, otag );
   } else {
      MC_(make_mem_undefined_w_otag) ( -VG_STACK_REDZONE_SZB + new_SP, 32, otag );
   }
}

MAYBE_USED
static void VG_REGPARM(1) mc_new_mem_stack_32(Addr new_SP)
{
   PROF_EVENT(114, "new_mem_stack_32");
   if (VG_IS_8_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+8 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+16 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+24 );
   } else if (VG_IS_4_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word32_undefined ( -VG_STACK_REDZONE_SZB + new_SP );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+4 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+12 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+20 );
      make_aligned_word32_undefined ( -VG_STACK_REDZONE_SZB + new_SP+28 );
   } else {
      make_mem_undefined ( -VG_STACK_REDZONE_SZB + new_SP, 32 );
   }
}

MAYBE_USED
static void VG_REGPARM(1) mc_die_mem_stack_32(Addr new_SP)
{
   PROF_EVENT(124, "die_mem_stack_32");
   if (VG_IS_8_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-32 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-24 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-16 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP- 8 );
   } else if (VG_IS_4_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word32_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-32 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-28 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-20 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-12 );
      make_aligned_word32_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-4  );
   } else {
      MC_(make_mem_noaccess) ( -VG_STACK_REDZONE_SZB + new_SP-32, 32 );
   }
}


MAYBE_USED
static void VG_REGPARM(2) mc_new_mem_stack_112_w_ECU(Addr new_SP, UInt ecu)
{
   UInt otag = ecu | MC_OKIND_STACK;
   PROF_EVENT(115, "new_mem_stack_112");
   if (VG_IS_8_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP   , otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+8 , otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+16, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+24, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+32, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+40, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+48, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+56, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+64, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+72, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+80, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+88, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+96, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+104, otag );
   } else {
      MC_(make_mem_undefined_w_otag) ( -VG_STACK_REDZONE_SZB + new_SP, 112, otag );
   }
}

MAYBE_USED
static void VG_REGPARM(1) mc_new_mem_stack_112(Addr new_SP)
{
   PROF_EVENT(115, "new_mem_stack_112");
   if (VG_IS_8_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+8 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+16 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+24 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+32 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+40 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+48 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+56 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+64 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+72 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+80 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+88 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+96 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+104 );
   } else {
      make_mem_undefined ( -VG_STACK_REDZONE_SZB + new_SP, 112 );
   }
}

MAYBE_USED
static void VG_REGPARM(1) mc_die_mem_stack_112(Addr new_SP)
{
   PROF_EVENT(125, "die_mem_stack_112");
   if (VG_IS_8_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-112);
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-104);
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-96 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-88 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-80 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-72 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-64 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-56 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-48 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-40 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-32 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-24 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-16 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP- 8 );
   } else {
      MC_(make_mem_noaccess) ( -VG_STACK_REDZONE_SZB + new_SP-112, 112 );
   }
}


MAYBE_USED
static void VG_REGPARM(2) mc_new_mem_stack_128_w_ECU(Addr new_SP, UInt ecu)
{
   UInt otag = ecu | MC_OKIND_STACK;
   PROF_EVENT(116, "new_mem_stack_128");
   if (VG_IS_8_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP   , otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+8 , otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+16, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+24, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+32, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+40, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+48, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+56, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+64, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+72, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+80, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+88, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+96, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+104, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+112, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+120, otag );
   } else {
      MC_(make_mem_undefined_w_otag) ( -VG_STACK_REDZONE_SZB + new_SP, 128, otag );
   }
}

MAYBE_USED
static void VG_REGPARM(1) mc_new_mem_stack_128(Addr new_SP)
{
   PROF_EVENT(116, "new_mem_stack_128");
   if (VG_IS_8_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+8 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+16 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+24 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+32 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+40 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+48 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+56 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+64 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+72 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+80 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+88 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+96 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+104 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+112 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+120 );
   } else {
      make_mem_undefined ( -VG_STACK_REDZONE_SZB + new_SP, 128 );
   }
}

MAYBE_USED
static void VG_REGPARM(1) mc_die_mem_stack_128(Addr new_SP)
{
   PROF_EVENT(126, "die_mem_stack_128");
   if (VG_IS_8_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-128);
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-120);
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-112);
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-104);
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-96 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-88 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-80 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-72 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-64 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-56 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-48 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-40 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-32 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-24 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-16 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP- 8 );
   } else {
      MC_(make_mem_noaccess) ( -VG_STACK_REDZONE_SZB + new_SP-128, 128 );
   }
}


MAYBE_USED
static void VG_REGPARM(2) mc_new_mem_stack_144_w_ECU(Addr new_SP, UInt ecu)
{
   UInt otag = ecu | MC_OKIND_STACK;
   PROF_EVENT(117, "new_mem_stack_144");
   if (VG_IS_8_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP,     otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+8,   otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+16,  otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+24,  otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+32,  otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+40,  otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+48,  otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+56,  otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+64,  otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+72,  otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+80,  otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+88,  otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+96,  otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+104, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+112, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+120, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+128, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+136, otag );
   } else {
      MC_(make_mem_undefined_w_otag) ( -VG_STACK_REDZONE_SZB + new_SP, 144, otag );
   }
}

MAYBE_USED
static void VG_REGPARM(1) mc_new_mem_stack_144(Addr new_SP)
{
   PROF_EVENT(117, "new_mem_stack_144");
   if (VG_IS_8_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+8 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+16 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+24 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+32 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+40 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+48 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+56 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+64 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+72 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+80 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+88 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+96 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+104 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+112 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+120 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+128 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+136 );
   } else {
      make_mem_undefined ( -VG_STACK_REDZONE_SZB + new_SP, 144 );
   }
}

MAYBE_USED
static void VG_REGPARM(1) mc_die_mem_stack_144(Addr new_SP)
{
   PROF_EVENT(127, "die_mem_stack_144");
   if (VG_IS_8_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-144);
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-136);
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-128);
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-120);
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-112);
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-104);
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-96 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-88 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-80 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-72 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-64 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-56 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-48 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-40 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-32 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-24 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-16 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP- 8 );
   } else {
      MC_(make_mem_noaccess) ( -VG_STACK_REDZONE_SZB + new_SP-144, 144 );
   }
}


MAYBE_USED
static void VG_REGPARM(2) mc_new_mem_stack_160_w_ECU(Addr new_SP, UInt ecu)
{
   UInt otag = ecu | MC_OKIND_STACK;
   PROF_EVENT(118, "new_mem_stack_160");
   if (VG_IS_8_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP,     otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+8,   otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+16,  otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+24,  otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+32,  otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+40,  otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+48,  otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+56,  otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+64,  otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+72,  otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+80,  otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+88,  otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+96,  otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+104, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+112, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+120, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+128, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+136, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+144, otag );
      make_aligned_word64_undefined_w_otag ( -VG_STACK_REDZONE_SZB + new_SP+152, otag );
   } else {
      MC_(make_mem_undefined_w_otag) ( -VG_STACK_REDZONE_SZB + new_SP, 160, otag );
   }
}

MAYBE_USED
static void VG_REGPARM(1) mc_new_mem_stack_160(Addr new_SP)
{
   PROF_EVENT(118, "new_mem_stack_160");
   if (VG_IS_8_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+8 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+16 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+24 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+32 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+40 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+48 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+56 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+64 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+72 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+80 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+88 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+96 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+104 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+112 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+120 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+128 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+136 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+144 );
      make_aligned_word64_undefined ( -VG_STACK_REDZONE_SZB + new_SP+152 );
   } else {
      make_mem_undefined ( -VG_STACK_REDZONE_SZB + new_SP, 160 );
   }
}

MAYBE_USED
static void VG_REGPARM(1) mc_die_mem_stack_160(Addr new_SP)
{
   PROF_EVENT(128, "die_mem_stack_160");
   if (VG_IS_8_ALIGNED( -VG_STACK_REDZONE_SZB + new_SP )) {
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-160);
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-152);
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-144);
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-136);
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-128);
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-120);
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-112);
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-104);
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-96 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-88 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-80 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-72 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-64 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-56 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-48 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-40 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-32 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-24 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP-16 );
      make_aligned_word64_noaccess ( -VG_STACK_REDZONE_SZB + new_SP- 8 );
   } else {
      MC_(make_mem_noaccess) ( -VG_STACK_REDZONE_SZB + new_SP-160, 160 );
   }
}


static void mc_new_mem_stack_w_ECU ( Addr a, SizeT len, UInt ecu )
{
   UInt otag = ecu | MC_OKIND_STACK;
   PROF_EVENT(115, "new_mem_stack_w_otag");
   MC_(make_mem_undefined_w_otag) ( -VG_STACK_REDZONE_SZB + a, len, otag );
}

static void mc_new_mem_stack ( Addr a, SizeT len )
{
   PROF_EVENT(115, "new_mem_stack");
   make_mem_undefined ( -VG_STACK_REDZONE_SZB + a, len );
}

static void mc_die_mem_stack ( Addr a, SizeT len )
{
   PROF_EVENT(125, "die_mem_stack");
   MC_(make_mem_noaccess) ( -VG_STACK_REDZONE_SZB + a, len );
}





static UWord stats__nia_cache_queries = 0;
static UWord stats__nia_cache_misses  = 0;

typedef
   struct { UWord nia0; UWord ecu0;   
            UWord nia1; UWord ecu1; } 
   WCacheEnt;

#define N_NIA_TO_ECU_CACHE 511

static WCacheEnt nia_to_ecu_cache[N_NIA_TO_ECU_CACHE];

static void init_nia_to_ecu_cache ( void )
{
   UWord       i;
   Addr        zero_addr = 0;
   ExeContext* zero_ec;
   UInt        zero_ecu;
   zero_ec = VG_(make_depth_1_ExeContext_from_Addr)(zero_addr);
   tl_assert(zero_ec);
   zero_ecu = VG_(get_ECU_from_ExeContext)(zero_ec);
   tl_assert(VG_(is_plausible_ECU)(zero_ecu));
   for (i = 0; i < N_NIA_TO_ECU_CACHE; i++) {
      nia_to_ecu_cache[i].nia0 = zero_addr;
      nia_to_ecu_cache[i].ecu0 = zero_ecu;
      nia_to_ecu_cache[i].nia1 = zero_addr;
      nia_to_ecu_cache[i].ecu1 = zero_ecu;
   }
}

static inline UInt convert_nia_to_ecu ( Addr nia )
{
   UWord i;
   UInt        ecu;
   ExeContext* ec;

   tl_assert( sizeof(nia_to_ecu_cache[0].nia1) == sizeof(nia) );

   stats__nia_cache_queries++;
   i = nia % N_NIA_TO_ECU_CACHE;
   tl_assert(i >= 0 && i < N_NIA_TO_ECU_CACHE);

   if (LIKELY( nia_to_ecu_cache[i].nia0 == nia ))
      return nia_to_ecu_cache[i].ecu0;

   if (LIKELY( nia_to_ecu_cache[i].nia1 == nia )) {
#     define SWAP(_w1,_w2) { UWord _t = _w1; _w1 = _w2; _w2 = _t; }
      SWAP( nia_to_ecu_cache[i].nia0, nia_to_ecu_cache[i].nia1 );
      SWAP( nia_to_ecu_cache[i].ecu0, nia_to_ecu_cache[i].ecu1 );
#     undef SWAP
      return nia_to_ecu_cache[i].ecu0;
   }

   stats__nia_cache_misses++;
   ec = VG_(make_depth_1_ExeContext_from_Addr)(nia);
   tl_assert(ec);
   ecu = VG_(get_ECU_from_ExeContext)(ec);
   tl_assert(VG_(is_plausible_ECU)(ecu));

   nia_to_ecu_cache[i].nia1 = nia_to_ecu_cache[i].nia0;
   nia_to_ecu_cache[i].ecu1 = nia_to_ecu_cache[i].ecu0;

   nia_to_ecu_cache[i].nia0 = nia;
   nia_to_ecu_cache[i].ecu0 = (UWord)ecu;
   return ecu;
}


void MC_(helperc_MAKE_STACK_UNINIT) ( Addr base, UWord len, Addr nia )
{
   UInt otag;
   tl_assert(sizeof(UWord) == sizeof(SizeT));
   if (0)
      VG_(printf)("helperc_MAKE_STACK_UNINIT (%#lx,%lu,nia=%#lx)\n",
                  base, len, nia );

   if (UNLIKELY( MC_(clo_mc_level) == 3 )) {
      UInt ecu = convert_nia_to_ecu ( nia );
      tl_assert(VG_(is_plausible_ECU)(ecu));
      otag = ecu | MC_OKIND_STACK;
   } else {
      tl_assert(nia == 0);
      otag = 0;
   }

#  if 0
   
   MC_(make_mem_undefined)(base, len, otag);
#  endif

#  if 0
   if (LIKELY( VG_IS_8_ALIGNED(base) && len==128 )) {
      make_aligned_word64_undefined(base +   0, otag);
      make_aligned_word64_undefined(base +   8, otag);
      make_aligned_word64_undefined(base +  16, otag);
      make_aligned_word64_undefined(base +  24, otag);

      make_aligned_word64_undefined(base +  32, otag);
      make_aligned_word64_undefined(base +  40, otag);
      make_aligned_word64_undefined(base +  48, otag);
      make_aligned_word64_undefined(base +  56, otag);

      make_aligned_word64_undefined(base +  64, otag);
      make_aligned_word64_undefined(base +  72, otag);
      make_aligned_word64_undefined(base +  80, otag);
      make_aligned_word64_undefined(base +  88, otag);

      make_aligned_word64_undefined(base +  96, otag);
      make_aligned_word64_undefined(base + 104, otag);
      make_aligned_word64_undefined(base + 112, otag);
      make_aligned_word64_undefined(base + 120, otag);
   } else {
      MC_(make_mem_undefined)(base, len, otag);
   }
#  endif 


   if (LIKELY( len == 128 && VG_IS_8_ALIGNED(base) )) {
      
      UWord a_lo = (UWord)(base);
      UWord a_hi = (UWord)(base + 128 - 1);
      tl_assert(a_lo < a_hi);             
      if (a_hi <= MAX_PRIMARY_ADDRESS) {
         
         SecMap* sm    = get_secmap_for_writing_low(a_lo);
         SecMap* sm_hi = get_secmap_for_writing_low(a_hi);
         if (LIKELY(sm == sm_hi)) {
            
            UWord   v_off = SM_OFF(a_lo);
            UShort* p     = (UShort*)(&sm->vabits8[v_off]);
            p[ 0] = VA_BITS16_UNDEFINED;
            p[ 1] = VA_BITS16_UNDEFINED;
            p[ 2] = VA_BITS16_UNDEFINED;
            p[ 3] = VA_BITS16_UNDEFINED;
            p[ 4] = VA_BITS16_UNDEFINED;
            p[ 5] = VA_BITS16_UNDEFINED;
            p[ 6] = VA_BITS16_UNDEFINED;
            p[ 7] = VA_BITS16_UNDEFINED;
            p[ 8] = VA_BITS16_UNDEFINED;
            p[ 9] = VA_BITS16_UNDEFINED;
            p[10] = VA_BITS16_UNDEFINED;
            p[11] = VA_BITS16_UNDEFINED;
            p[12] = VA_BITS16_UNDEFINED;
            p[13] = VA_BITS16_UNDEFINED;
            p[14] = VA_BITS16_UNDEFINED;
            p[15] = VA_BITS16_UNDEFINED;
            if (UNLIKELY( MC_(clo_mc_level) == 3 )) {
               set_aligned_word64_Origin_to_undef( base + 8 * 0, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 1, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 2, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 3, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 4, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 5, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 6, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 7, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 8, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 9, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 10, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 11, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 12, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 13, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 14, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 15, otag );
            }
            return;
         }
      }
   }

   
   if (LIKELY( len == 288 && VG_IS_8_ALIGNED(base) )) {
      
      UWord a_lo = (UWord)(base);
      UWord a_hi = (UWord)(base + 288 - 1);
      tl_assert(a_lo < a_hi);             
      if (a_hi <= MAX_PRIMARY_ADDRESS) {
         
         SecMap* sm    = get_secmap_for_writing_low(a_lo);
         SecMap* sm_hi = get_secmap_for_writing_low(a_hi);
         if (LIKELY(sm == sm_hi)) {
            
            UWord   v_off = SM_OFF(a_lo);
            UShort* p     = (UShort*)(&sm->vabits8[v_off]);
            p[ 0] = VA_BITS16_UNDEFINED;
            p[ 1] = VA_BITS16_UNDEFINED;
            p[ 2] = VA_BITS16_UNDEFINED;
            p[ 3] = VA_BITS16_UNDEFINED;
            p[ 4] = VA_BITS16_UNDEFINED;
            p[ 5] = VA_BITS16_UNDEFINED;
            p[ 6] = VA_BITS16_UNDEFINED;
            p[ 7] = VA_BITS16_UNDEFINED;
            p[ 8] = VA_BITS16_UNDEFINED;
            p[ 9] = VA_BITS16_UNDEFINED;
            p[10] = VA_BITS16_UNDEFINED;
            p[11] = VA_BITS16_UNDEFINED;
            p[12] = VA_BITS16_UNDEFINED;
            p[13] = VA_BITS16_UNDEFINED;
            p[14] = VA_BITS16_UNDEFINED;
            p[15] = VA_BITS16_UNDEFINED;
            p[16] = VA_BITS16_UNDEFINED;
            p[17] = VA_BITS16_UNDEFINED;
            p[18] = VA_BITS16_UNDEFINED;
            p[19] = VA_BITS16_UNDEFINED;
            p[20] = VA_BITS16_UNDEFINED;
            p[21] = VA_BITS16_UNDEFINED;
            p[22] = VA_BITS16_UNDEFINED;
            p[23] = VA_BITS16_UNDEFINED;
            p[24] = VA_BITS16_UNDEFINED;
            p[25] = VA_BITS16_UNDEFINED;
            p[26] = VA_BITS16_UNDEFINED;
            p[27] = VA_BITS16_UNDEFINED;
            p[28] = VA_BITS16_UNDEFINED;
            p[29] = VA_BITS16_UNDEFINED;
            p[30] = VA_BITS16_UNDEFINED;
            p[31] = VA_BITS16_UNDEFINED;
            p[32] = VA_BITS16_UNDEFINED;
            p[33] = VA_BITS16_UNDEFINED;
            p[34] = VA_BITS16_UNDEFINED;
            p[35] = VA_BITS16_UNDEFINED;
            if (UNLIKELY( MC_(clo_mc_level) == 3 )) {
               set_aligned_word64_Origin_to_undef( base + 8 * 0, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 1, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 2, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 3, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 4, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 5, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 6, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 7, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 8, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 9, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 10, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 11, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 12, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 13, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 14, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 15, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 16, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 17, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 18, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 19, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 20, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 21, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 22, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 23, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 24, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 25, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 26, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 27, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 28, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 29, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 30, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 31, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 32, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 33, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 34, otag );
               set_aligned_word64_Origin_to_undef( base + 8 * 35, otag );
            }
            return;
         }
      }
   }

   
   MC_(make_mem_undefined_w_otag)(base, len, otag);
}



typedef 
   enum {
      MC_Ok = 5, 
      MC_AddrErr = 6, 
      MC_ValueErr = 7
   } 
   MC_ReadResult;



Bool MC_(check_mem_is_noaccess) ( Addr a, SizeT len, Addr* bad_addr )
{
   SizeT i;
   UWord vabits2;

   PROF_EVENT(60, "check_mem_is_noaccess");
   for (i = 0; i < len; i++) {
      PROF_EVENT(61, "check_mem_is_noaccess(loop)");
      vabits2 = get_vabits2(a);
      if (VA_BITS2_NOACCESS != vabits2) {
         if (bad_addr != NULL) *bad_addr = a;
         return False;
      }
      a++;
   }
   return True;
}

static Bool is_mem_addressable ( Addr a, SizeT len, 
                                 Addr* bad_addr )
{
   SizeT i;
   UWord vabits2;

   PROF_EVENT(62, "is_mem_addressable");
   for (i = 0; i < len; i++) {
      PROF_EVENT(63, "is_mem_addressable(loop)");
      vabits2 = get_vabits2(a);
      if (VA_BITS2_NOACCESS == vabits2) {
         if (bad_addr != NULL) *bad_addr = a;
         return False;
      }
      a++;
   }
   return True;
}

static MC_ReadResult is_mem_defined ( Addr a, SizeT len,
                                      Addr* bad_addr,
                                      UInt* otag )
{
   SizeT i;
   UWord vabits2;

   PROF_EVENT(64, "is_mem_defined");
   DEBUG("is_mem_defined\n");

   if (otag)     *otag = 0;
   if (bad_addr) *bad_addr = 0;
   for (i = 0; i < len; i++) {
      PROF_EVENT(65, "is_mem_defined(loop)");
      vabits2 = get_vabits2(a);
      if (VA_BITS2_DEFINED != vabits2) {
         
         
         
         if (bad_addr) {
            *bad_addr = a;
         }
         if (VA_BITS2_NOACCESS == vabits2) {
            return MC_AddrErr;
         }
         if (MC_(clo_mc_level) >= 2) {
            if (otag && MC_(clo_mc_level) == 3) {
               *otag = MC_(helperc_b_load1)( a );
            }
            return MC_ValueErr;
         }
      }
      a++;
   }
   return MC_Ok;
}


static void is_mem_defined_comprehensive (
               Addr a, SizeT len,
               Bool* errorV,    
               Addr* bad_addrV, 
               UInt* otagV,     
               Bool* errorA,    
               Addr* bad_addrA  
            )
{
   SizeT i;
   UWord vabits2;
   Bool  already_saw_errV = False;

   PROF_EVENT(64, "is_mem_defined"); 
   DEBUG("is_mem_defined_comprehensive\n");

   tl_assert(!(*errorV || *errorA));

   for (i = 0; i < len; i++) {
      PROF_EVENT(65, "is_mem_defined(loop)"); 
      vabits2 = get_vabits2(a);
      switch (vabits2) {
         case VA_BITS2_DEFINED: 
            a++; 
            break;
         case VA_BITS2_UNDEFINED:
         case VA_BITS2_PARTDEFINED:
            if (!already_saw_errV) {
               *errorV    = True;
               *bad_addrV = a;
               if (MC_(clo_mc_level) == 3) {
                  *otagV = MC_(helperc_b_load1)( a );
               } else {
                  *otagV = 0;
               }
               already_saw_errV = True;
            }
            a++; 
            break;
         case VA_BITS2_NOACCESS:
            *errorA    = True;
            *bad_addrA = a;
            return; 
         default:
            tl_assert(0);
      }
   }
}



static Bool mc_is_defined_asciiz ( Addr a, Addr* bad_addr, UInt* otag )
{
   UWord vabits2;

   PROF_EVENT(66, "mc_is_defined_asciiz");
   DEBUG("mc_is_defined_asciiz\n");

   if (otag)     *otag = 0;
   if (bad_addr) *bad_addr = 0;
   while (True) {
      PROF_EVENT(67, "mc_is_defined_asciiz(loop)");
      vabits2 = get_vabits2(a);
      if (VA_BITS2_DEFINED != vabits2) {
         
         
         
         if (bad_addr) {
            *bad_addr = a;
         }
         if (VA_BITS2_NOACCESS == vabits2) {
            return MC_AddrErr;
         }
         if (MC_(clo_mc_level) >= 2) {
            if (otag && MC_(clo_mc_level) == 3) {
               *otag = MC_(helperc_b_load1)( a );
            }
            return MC_ValueErr;
         }
      }
      
      if (* ((UChar*)a) == 0) {
         return MC_Ok;
      }
      a++;
   }
}



static
void check_mem_is_addressable ( CorePart part, ThreadId tid, const HChar* s,
                                Addr base, SizeT size )
{
   Addr bad_addr;
   Bool ok = is_mem_addressable ( base, size, &bad_addr );

   if (!ok) {
      switch (part) {
      case Vg_CoreSysCall:
         MC_(record_memparam_error) ( tid, bad_addr, 
                                      True, s, 0 );
         break;

      case Vg_CoreSignal:
         MC_(record_core_mem_error)( tid, s );
         break;

      default:
         VG_(tool_panic)("check_mem_is_addressable: unexpected CorePart");
      }
   }
}

static
void check_mem_is_defined ( CorePart part, ThreadId tid, const HChar* s,
                            Addr base, SizeT size )
{     
   UInt otag = 0;
   Addr bad_addr;
   MC_ReadResult res = is_mem_defined ( base, size, &bad_addr, &otag );

   if (MC_Ok != res) {
      Bool isAddrErr = ( MC_AddrErr == res ? True : False );

      switch (part) {
      case Vg_CoreSysCall:
         MC_(record_memparam_error) ( tid, bad_addr, isAddrErr, s,
                                      isAddrErr ? 0 : otag );
         break;
      
      case Vg_CoreSysCallArgInMem:
         MC_(record_regparam_error) ( tid, s, otag );
         break;

      case Vg_CoreTranslate:
         MC_(record_jump_error)( tid, bad_addr );
         break;

      default:
         VG_(tool_panic)("check_mem_is_defined: unexpected CorePart");
      }
   }
}

static
void check_mem_is_defined_asciiz ( CorePart part, ThreadId tid,
                                   const HChar* s, Addr str )
{
   MC_ReadResult res;
   Addr bad_addr = 0;   
   UInt otag = 0;

   tl_assert(part == Vg_CoreSysCall);
   res = mc_is_defined_asciiz ( (Addr)str, &bad_addr, &otag );
   if (MC_Ok != res) {
      Bool isAddrErr = ( MC_AddrErr == res ? True : False );
      MC_(record_memparam_error) ( tid, bad_addr, isAddrErr, s,
                                   isAddrErr ? 0 : otag );
   }
}

static
void mc_new_mem_mmap ( Addr a, SizeT len, Bool rr, Bool ww, Bool xx,
                       ULong di_handle )
{
   if (rr || ww || xx) {
      
      MC_(make_mem_defined)(a, len);
   } else {
      
      MC_(make_mem_noaccess)(a, len);
   }
}

static
void mc_new_mem_mprotect ( Addr a, SizeT len, Bool rr, Bool ww, Bool xx )
{
   if (rr || ww || xx) {
      
      make_mem_defined_if_noaccess(a, len);
   } else {
      
      
   }
}


static
void mc_new_mem_startup( Addr a, SizeT len,
                         Bool rr, Bool ww, Bool xx, ULong di_handle )
{
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   DEBUG("mc_new_mem_startup(%#lx, %llu, rr=%u, ww=%u, xx=%u)\n",
         a, (ULong)len, rr, ww, xx);
   mc_new_mem_mmap(a, len, rr, ww, xx, di_handle);
}

static
void mc_post_mem_write(CorePart part, ThreadId tid, Addr a, SizeT len)
{
   MC_(make_mem_defined)(a, len);
}



static UInt mb_get_origin_for_guest_offset ( ThreadId tid,
                                             Int offset, SizeT size )
{
   Int   sh2off;
   UInt  area[3];
   UInt  otag;
   sh2off = MC_(get_otrack_shadow_offset)( offset, size );
   if (sh2off == -1)
      return 0;  
   tl_assert(sh2off >= 0);
   tl_assert(0 == (sh2off % 4));
   area[0] = 0x31313131;
   area[2] = 0x27272727;
   VG_(get_shadow_regs_area)( tid, (UChar *)&area[1], 2,sh2off,4 );
   tl_assert(area[0] == 0x31313131);
   tl_assert(area[2] == 0x27272727);
   otag = area[1];
   return otag;
}


/* When some chunk of guest state is written, mark the corresponding
   shadow area as valid.  This is used to initialise arbitrarily large
   chunks of guest state, hence the _SIZE value, which has to be as
   big as the biggest guest state.
*/
static void mc_post_reg_write ( CorePart part, ThreadId tid, 
                                PtrdiffT offset, SizeT size)
{
#  define MAX_REG_WRITE_SIZE 1712
   UChar area[MAX_REG_WRITE_SIZE];
   tl_assert(size <= MAX_REG_WRITE_SIZE);
   VG_(memset)(area, V_BITS8_DEFINED, size);
   VG_(set_shadow_regs_area)( tid, 1,offset,size, area );
#  undef MAX_REG_WRITE_SIZE
}

static 
void mc_post_reg_write_clientcall ( ThreadId tid, 
                                    PtrdiffT offset, SizeT size, Addr f)
{
   mc_post_reg_write(0, tid, offset, size);
}

static void mc_pre_reg_read ( CorePart part, ThreadId tid, const HChar* s, 
                              PtrdiffT offset, SizeT size)
{
   Int   i;
   Bool  bad;
   UInt  otag;

   UChar area[16];
   tl_assert(size <= 16);

   VG_(get_shadow_regs_area)( tid, area, 1,offset,size );

   bad = False;
   for (i = 0; i < size; i++) {
      if (area[i] != V_BITS8_DEFINED) {
         bad = True;
         break;
      }
   }

   if (!bad)
      return;

   otag = mb_get_origin_for_guest_offset( tid, offset, size );
   MC_(record_regparam_error) ( tid, s, otag );
}




#define MASK(_szInBytes) \
   ( ~((0x10000UL-(_szInBytes)) | ((N_PRIMARY_MAP-1) << 16)) )

#define UNALIGNED_OR_HIGH(_a,_szInBits) \
   ((_a) & MASK((_szInBits>>3)))




static INLINE
void mc_LOADV_128_or_256 ( ULong* res,
                           Addr a, SizeT nBits, Bool isBigEndian )
{
   PROF_EVENT(200, "mc_LOADV_128_or_256");

#ifndef PERF_FAST_LOADV
   mc_LOADV_128_or_256_slow( res, a, nBits, isBigEndian );
   return;
#else
   {
      UWord   sm_off16, vabits16, j;
      UWord   nBytes  = nBits / 8;
      UWord   nULongs = nBytes / 8;
      SecMap* sm;

      if (UNLIKELY( UNALIGNED_OR_HIGH(a,nBits) )) {
         PROF_EVENT(201, "mc_LOADV_128_or_256-slow1");
         mc_LOADV_128_or_256_slow( res, a, nBits, isBigEndian );
         return;
      }

      for (j = 0; j < nULongs; j++) {
         sm       = get_secmap_for_reading_low(a + 8*j);
         sm_off16 = SM_OFF_16(a + 8*j);
         vabits16 = ((UShort*)(sm->vabits8))[sm_off16];

         
         
         if (LIKELY(vabits16 == VA_BITS16_DEFINED)) {
            res[j] = V_BITS64_DEFINED;
         } else if (LIKELY(vabits16 == VA_BITS16_UNDEFINED)) {
            res[j] = V_BITS64_UNDEFINED;
         } else {
            PROF_EVENT(202, "mc_LOADV_128_or_256-slow2");
            mc_LOADV_128_or_256_slow( res, a, nBits, isBigEndian );
            return;
         }
      }
      return;
   }
#endif
}

VG_REGPARM(2) void MC_(helperc_LOADV256be) ( V256* res, Addr a )
{
   mc_LOADV_128_or_256(&res->w64[0], a, 256, True);
}
VG_REGPARM(2) void MC_(helperc_LOADV256le) ( V256* res, Addr a )
{
   mc_LOADV_128_or_256(&res->w64[0], a, 256, False);
}

VG_REGPARM(2) void MC_(helperc_LOADV128be) ( V128* res, Addr a )
{
   mc_LOADV_128_or_256(&res->w64[0], a, 128, True);
}
VG_REGPARM(2) void MC_(helperc_LOADV128le) ( V128* res, Addr a )
{
   mc_LOADV_128_or_256(&res->w64[0], a, 128, False);
}


static INLINE
ULong mc_LOADV64 ( Addr a, Bool isBigEndian )
{
   PROF_EVENT(200, "mc_LOADV64");

#ifndef PERF_FAST_LOADV
   return mc_LOADVn_slow( a, 64, isBigEndian );
#else
   {
      UWord   sm_off16, vabits16;
      SecMap* sm;

      if (UNLIKELY( UNALIGNED_OR_HIGH(a,64) )) {
         PROF_EVENT(201, "mc_LOADV64-slow1");
         return (ULong)mc_LOADVn_slow( a, 64, isBigEndian );
      }

      sm       = get_secmap_for_reading_low(a);
      sm_off16 = SM_OFF_16(a);
      vabits16 = ((UShort*)(sm->vabits8))[sm_off16];

      
      
      
      if (LIKELY(vabits16 == VA_BITS16_DEFINED)) {
         return V_BITS64_DEFINED;
      } else if (LIKELY(vabits16 == VA_BITS16_UNDEFINED)) {
         return V_BITS64_UNDEFINED;
      } else {
         
         PROF_EVENT(202, "mc_LOADV64-slow2");
         return mc_LOADVn_slow( a, 64, isBigEndian );
      }
   }
#endif
}

VG_REGPARM(1) ULong MC_(helperc_LOADV64be) ( Addr a )
{
   return mc_LOADV64(a, True);
}
VG_REGPARM(1) ULong MC_(helperc_LOADV64le) ( Addr a )
{
   return mc_LOADV64(a, False);
}


static INLINE
void mc_STOREV64 ( Addr a, ULong vbits64, Bool isBigEndian )
{
   PROF_EVENT(210, "mc_STOREV64");

#ifndef PERF_FAST_STOREV
   
   
   mc_STOREVn_slow( a, 64, vbits64, isBigEndian );
#else
   {
      UWord   sm_off16, vabits16;
      SecMap* sm;

      if (UNLIKELY( UNALIGNED_OR_HIGH(a,64) )) {
         PROF_EVENT(211, "mc_STOREV64-slow1");
         mc_STOREVn_slow( a, 64, vbits64, isBigEndian );
         return;
      }

      sm       = get_secmap_for_reading_low(a);
      sm_off16 = SM_OFF_16(a);
      vabits16 = ((UShort*)(sm->vabits8))[sm_off16];

      
      
      if (LIKELY(V_BITS64_DEFINED == vbits64)) {
         if (LIKELY(vabits16 == (UShort)VA_BITS16_DEFINED)) {
            return;
         }
         if (!is_distinguished_sm(sm) && VA_BITS16_UNDEFINED == vabits16) {
            ((UShort*)(sm->vabits8))[sm_off16] = (UShort)VA_BITS16_DEFINED;
            return;
         }
         PROF_EVENT(232, "mc_STOREV64-slow2");
         mc_STOREVn_slow( a, 64, vbits64, isBigEndian );
         return;
      }
      if (V_BITS64_UNDEFINED == vbits64) {
         if (vabits16 == (UShort)VA_BITS16_UNDEFINED) {
            return;
         }
         if (!is_distinguished_sm(sm) && VA_BITS16_DEFINED == vabits16) {
            ((UShort*)(sm->vabits8))[sm_off16] = (UShort)VA_BITS16_UNDEFINED;
            return;
         } 
         PROF_EVENT(232, "mc_STOREV64-slow3");
         mc_STOREVn_slow( a, 64, vbits64, isBigEndian );
         return;
      }

      PROF_EVENT(212, "mc_STOREV64-slow4");
      mc_STOREVn_slow( a, 64, vbits64, isBigEndian );
   }
#endif
}

VG_REGPARM(1) void MC_(helperc_STOREV64be) ( Addr a, ULong vbits64 )
{
   mc_STOREV64(a, vbits64, True);
}
VG_REGPARM(1) void MC_(helperc_STOREV64le) ( Addr a, ULong vbits64 )
{
   mc_STOREV64(a, vbits64, False);
}



static INLINE
UWord mc_LOADV32 ( Addr a, Bool isBigEndian )
{
   PROF_EVENT(220, "mc_LOADV32");

#ifndef PERF_FAST_LOADV
   return (UWord)mc_LOADVn_slow( a, 32, isBigEndian );
#else
   {
      UWord   sm_off, vabits8;
      SecMap* sm;

      if (UNLIKELY( UNALIGNED_OR_HIGH(a,32) )) {
         PROF_EVENT(221, "mc_LOADV32-slow1");
         return (UWord)mc_LOADVn_slow( a, 32, isBigEndian );
      }

      sm      = get_secmap_for_reading_low(a);
      sm_off  = SM_OFF(a);
      vabits8 = sm->vabits8[sm_off];

      
      
      
      
      
      if (LIKELY(vabits8 == VA_BITS8_DEFINED)) {
         return ((UWord)0xFFFFFFFF00000000ULL | (UWord)V_BITS32_DEFINED);
      } else if (LIKELY(vabits8 == VA_BITS8_UNDEFINED)) {
         return ((UWord)0xFFFFFFFF00000000ULL | (UWord)V_BITS32_UNDEFINED);
      } else {
         
         PROF_EVENT(222, "mc_LOADV32-slow2");
         return (UWord)mc_LOADVn_slow( a, 32, isBigEndian );
      }
   }
#endif
}

VG_REGPARM(1) UWord MC_(helperc_LOADV32be) ( Addr a )
{
   return mc_LOADV32(a, True);
}
VG_REGPARM(1) UWord MC_(helperc_LOADV32le) ( Addr a )
{
   return mc_LOADV32(a, False);
}


static INLINE
void mc_STOREV32 ( Addr a, UWord vbits32, Bool isBigEndian )
{
   PROF_EVENT(230, "mc_STOREV32");

#ifndef PERF_FAST_STOREV
   mc_STOREVn_slow( a, 32, (ULong)vbits32, isBigEndian );
#else
   {
      UWord   sm_off, vabits8;
      SecMap* sm;

      if (UNLIKELY( UNALIGNED_OR_HIGH(a,32) )) {
         PROF_EVENT(231, "mc_STOREV32-slow1");
         mc_STOREVn_slow( a, 32, (ULong)vbits32, isBigEndian );
         return;
      }

      sm      = get_secmap_for_reading_low(a);
      sm_off  = SM_OFF(a);
      vabits8 = sm->vabits8[sm_off];

      
      
      if (LIKELY(V_BITS32_DEFINED == vbits32)) {
         if (LIKELY(vabits8 == (UInt)VA_BITS8_DEFINED)) {
            return;
         }
         if (!is_distinguished_sm(sm)  && VA_BITS8_UNDEFINED == vabits8) {
            sm->vabits8[sm_off] = (UInt)VA_BITS8_DEFINED;
            return;
         }
         PROF_EVENT(232, "mc_STOREV32-slow2");
         mc_STOREVn_slow( a, 32, (ULong)vbits32, isBigEndian );
         return;
      }
      if (V_BITS32_UNDEFINED == vbits32) {
         if (vabits8 == (UInt)VA_BITS8_UNDEFINED) {
            return;
         }
         if (!is_distinguished_sm(sm) && VA_BITS8_DEFINED == vabits8) {
            sm->vabits8[sm_off] = (UInt)VA_BITS8_UNDEFINED;
            return;
         }
         PROF_EVENT(233, "mc_STOREV32-slow3");
         mc_STOREVn_slow( a, 32, (ULong)vbits32, isBigEndian );
         return;
      }

      PROF_EVENT(234, "mc_STOREV32-slow4");
      mc_STOREVn_slow( a, 32, (ULong)vbits32, isBigEndian );
   }
#endif
}

VG_REGPARM(2) void MC_(helperc_STOREV32be) ( Addr a, UWord vbits32 )
{
   mc_STOREV32(a, vbits32, True);
}
VG_REGPARM(2) void MC_(helperc_STOREV32le) ( Addr a, UWord vbits32 )
{
   mc_STOREV32(a, vbits32, False);
}



static INLINE
UWord mc_LOADV16 ( Addr a, Bool isBigEndian )
{
   PROF_EVENT(240, "mc_LOADV16");

#ifndef PERF_FAST_LOADV
   return (UWord)mc_LOADVn_slow( a, 16, isBigEndian );
#else
   {
      UWord   sm_off, vabits8;
      SecMap* sm;

      if (UNLIKELY( UNALIGNED_OR_HIGH(a,16) )) {
         PROF_EVENT(241, "mc_LOADV16-slow1");
         return (UWord)mc_LOADVn_slow( a, 16, isBigEndian );
      }

      sm      = get_secmap_for_reading_low(a);
      sm_off  = SM_OFF(a);
      vabits8 = sm->vabits8[sm_off];
      
      
      
      if      (LIKELY(vabits8 == VA_BITS8_DEFINED  )) { return V_BITS16_DEFINED;   }
      else if (LIKELY(vabits8 == VA_BITS8_UNDEFINED)) { return V_BITS16_UNDEFINED; }
      else {
         
         
         UChar vabits4 = extract_vabits4_from_vabits8(a, vabits8);
         if      (vabits4 == VA_BITS4_DEFINED  ) { return V_BITS16_DEFINED;   }
         else if (vabits4 == VA_BITS4_UNDEFINED) { return V_BITS16_UNDEFINED; }
         else {
            
            PROF_EVENT(242, "mc_LOADV16-slow2");
            return (UWord)mc_LOADVn_slow( a, 16, isBigEndian );
         }
      }
   }
#endif
}

VG_REGPARM(1) UWord MC_(helperc_LOADV16be) ( Addr a )
{
   return mc_LOADV16(a, True);
}
VG_REGPARM(1) UWord MC_(helperc_LOADV16le) ( Addr a )
{
   return mc_LOADV16(a, False);
}

static INLINE
Bool accessible_vabits4_in_vabits8 ( Addr a, UChar vabits8 )
{
   UInt shift;
   tl_assert(VG_IS_2_ALIGNED(a));      
   shift = (a & 2) << 1;               
   vabits8 >>= shift;                  
    
   return ((0x3 & vabits8) != VA_BITS2_NOACCESS)
      &&  ((0xc & vabits8) != VA_BITS2_NOACCESS << 2);
}

static INLINE
void mc_STOREV16 ( Addr a, UWord vbits16, Bool isBigEndian )
{
   PROF_EVENT(250, "mc_STOREV16");

#ifndef PERF_FAST_STOREV
   mc_STOREVn_slow( a, 16, (ULong)vbits16, isBigEndian );
#else
   {
      UWord   sm_off, vabits8;
      SecMap* sm;

      if (UNLIKELY( UNALIGNED_OR_HIGH(a,16) )) {
         PROF_EVENT(251, "mc_STOREV16-slow1");
         mc_STOREVn_slow( a, 16, (ULong)vbits16, isBigEndian );
         return;
      }

      sm      = get_secmap_for_reading_low(a);
      sm_off  = SM_OFF(a);
      vabits8 = sm->vabits8[sm_off];

      
      
      if (LIKELY(V_BITS16_DEFINED == vbits16)) {
         if (LIKELY(vabits8 == VA_BITS8_DEFINED)) {
            return;
         }
         if (!is_distinguished_sm(sm) 
             && accessible_vabits4_in_vabits8(a, vabits8)) {
            insert_vabits4_into_vabits8( a, VA_BITS4_DEFINED,
                                         &(sm->vabits8[sm_off]) );
            return;
         }
         PROF_EVENT(232, "mc_STOREV16-slow2");
         mc_STOREVn_slow( a, 16, (ULong)vbits16, isBigEndian );
      }
      if (V_BITS16_UNDEFINED == vbits16) {
         if (vabits8 == VA_BITS8_UNDEFINED) {
            return;
         }
         if (!is_distinguished_sm(sm)  
             && accessible_vabits4_in_vabits8(a, vabits8)) {
            insert_vabits4_into_vabits8( a, VA_BITS4_UNDEFINED,
                                         &(sm->vabits8[sm_off]) );
            return;
         }
         PROF_EVENT(233, "mc_STOREV16-slow3");
         mc_STOREVn_slow( a, 16, (ULong)vbits16, isBigEndian );
         return;
      }

      PROF_EVENT(234, "mc_STOREV16-slow4");
      mc_STOREVn_slow( a, 16, (ULong)vbits16, isBigEndian );
   }
#endif
}

VG_REGPARM(2) void MC_(helperc_STOREV16be) ( Addr a, UWord vbits16 )
{
   mc_STOREV16(a, vbits16, True);
}
VG_REGPARM(2) void MC_(helperc_STOREV16le) ( Addr a, UWord vbits16 )
{
   mc_STOREV16(a, vbits16, False);
}



VG_REGPARM(1)
UWord MC_(helperc_LOADV8) ( Addr a )
{
   PROF_EVENT(260, "mc_LOADV8");

#ifndef PERF_FAST_LOADV
   return (UWord)mc_LOADVn_slow( a, 8, False );
#else
   {
      UWord   sm_off, vabits8;
      SecMap* sm;

      if (UNLIKELY( UNALIGNED_OR_HIGH(a,8) )) {
         PROF_EVENT(261, "mc_LOADV8-slow1");
         return (UWord)mc_LOADVn_slow( a, 8, False );
      }

      sm      = get_secmap_for_reading_low(a);
      sm_off  = SM_OFF(a);
      vabits8 = sm->vabits8[sm_off];
      
      
      
      if      (LIKELY(vabits8 == VA_BITS8_DEFINED  )) { return V_BITS8_DEFINED;   }
      else if (LIKELY(vabits8 == VA_BITS8_UNDEFINED)) { return V_BITS8_UNDEFINED; }
      else {
         
         
         UChar vabits2 = extract_vabits2_from_vabits8(a, vabits8);
         if      (vabits2 == VA_BITS2_DEFINED  ) { return V_BITS8_DEFINED;   }
         else if (vabits2 == VA_BITS2_UNDEFINED) { return V_BITS8_UNDEFINED; }
         else {
            
            PROF_EVENT(262, "mc_LOADV8-slow2");
            return (UWord)mc_LOADVn_slow( a, 8, False );
         }
      }
   }
#endif
}


VG_REGPARM(2)
void MC_(helperc_STOREV8) ( Addr a, UWord vbits8 )
{
   PROF_EVENT(270, "mc_STOREV8");

#ifndef PERF_FAST_STOREV
   mc_STOREVn_slow( a, 8, (ULong)vbits8, False );
#else
   {
      UWord   sm_off, vabits8;
      SecMap* sm;

      if (UNLIKELY( UNALIGNED_OR_HIGH(a,8) )) {
         PROF_EVENT(271, "mc_STOREV8-slow1");
         mc_STOREVn_slow( a, 8, (ULong)vbits8, False );
         return;
      }

      sm      = get_secmap_for_reading_low(a);
      sm_off  = SM_OFF(a);
      vabits8 = sm->vabits8[sm_off];

      
      
      
      
      
      
      
      
      
      
      
      // be written in the secondary map. V bits can be directly written
      
      //   * The address for which V bits are written is naturally aligned
      
      
      
      
      //   * V bits being written are either fully defined or fully undefined.
      //     (for partially defined V bits, V bits cannot be directly written,
      
      
      
      //   * the memory corresponding to the V bits being written is
      
      
      
      
      
      
      
      
      
      
      
      
      if (LIKELY(V_BITS8_DEFINED == vbits8)) {
         if (LIKELY(vabits8 == VA_BITS8_DEFINED)) {
            return; 
         }
         if (!is_distinguished_sm(sm) 
             && VA_BITS2_NOACCESS != extract_vabits2_from_vabits8(a, vabits8)) {
            
            insert_vabits2_into_vabits8( a, VA_BITS2_DEFINED,
                                         &(sm->vabits8[sm_off]) );
            return;
         }
         PROF_EVENT(232, "mc_STOREV8-slow2");
         mc_STOREVn_slow( a, 8, (ULong)vbits8, False );
         return;
      }
      if (V_BITS8_UNDEFINED == vbits8) {
         if (vabits8 == VA_BITS8_UNDEFINED) {
            return; 
         }
         if (!is_distinguished_sm(sm) 
             && (VA_BITS2_NOACCESS 
                 != extract_vabits2_from_vabits8(a, vabits8))) {
            
            insert_vabits2_into_vabits8( a, VA_BITS2_UNDEFINED,
                                         &(sm->vabits8[sm_off]) );
            return;
         }
         PROF_EVENT(233, "mc_STOREV8-slow3");
         mc_STOREVn_slow( a, 8, (ULong)vbits8, False );
         return;
      }

      
      PROF_EVENT(234, "mc_STOREV8-slow4");
      mc_STOREVn_slow( a, 8, (ULong)vbits8, False );
   }
#endif
}



VG_REGPARM(1)
void MC_(helperc_value_check0_fail_w_o) ( UWord origin ) {
   MC_(record_cond_error) ( VG_(get_running_tid)(), (UInt)origin );
}

VG_REGPARM(1)
void MC_(helperc_value_check1_fail_w_o) ( UWord origin ) {
   MC_(record_value_error) ( VG_(get_running_tid)(), 1, (UInt)origin );
}

VG_REGPARM(1)
void MC_(helperc_value_check4_fail_w_o) ( UWord origin ) {
   MC_(record_value_error) ( VG_(get_running_tid)(), 4, (UInt)origin );
}

VG_REGPARM(1)
void MC_(helperc_value_check8_fail_w_o) ( UWord origin ) {
   MC_(record_value_error) ( VG_(get_running_tid)(), 8, (UInt)origin );
}

VG_REGPARM(2) 
void MC_(helperc_value_checkN_fail_w_o) ( HWord sz, UWord origin ) {
   MC_(record_value_error) ( VG_(get_running_tid)(), (Int)sz, (UInt)origin );
}


VG_REGPARM(0)
void MC_(helperc_value_check0_fail_no_o) ( void ) {
   MC_(record_cond_error) ( VG_(get_running_tid)(), 0 );
}

VG_REGPARM(0)
void MC_(helperc_value_check1_fail_no_o) ( void ) {
   MC_(record_value_error) ( VG_(get_running_tid)(), 1, 0 );
}

VG_REGPARM(0)
void MC_(helperc_value_check4_fail_no_o) ( void ) {
   MC_(record_value_error) ( VG_(get_running_tid)(), 4, 0 );
}

VG_REGPARM(0)
void MC_(helperc_value_check8_fail_no_o) ( void ) {
   MC_(record_value_error) ( VG_(get_running_tid)(), 8, 0 );
}

VG_REGPARM(1) 
void MC_(helperc_value_checkN_fail_no_o) ( HWord sz ) {
   MC_(record_value_error) ( VG_(get_running_tid)(), (Int)sz, 0 );
}




static Int mc_get_or_set_vbits_for_client ( 
   Addr a, 
   Addr vbits, 
   SizeT szB, 
   Bool setting,  
   Bool is_client_request 
 
)
{
   SizeT i;
   Bool  ok;
   UChar vbits8;

   for (i = 0; i < szB; i++) {
      if (VA_BITS2_NOACCESS == get_vabits2(a + i) ||
          (is_client_request && VA_BITS2_NOACCESS == get_vabits2(vbits + i))) {
         return 3;
      }
   }

   
   if (setting) {
      
      for (i = 0; i < szB; i++) {
         ok = set_vbits8(a + i, ((UChar*)vbits)[i]);
         tl_assert(ok);
      }
   } else {
      
      for (i = 0; i < szB; i++) {
         ok = get_vbits8(a + i, &vbits8);
         tl_assert(ok);
         ((UChar*)vbits)[i] = vbits8;
      }
      if (is_client_request)
        
        MC_(make_mem_defined)(vbits, szB);
   }

   return 1;
}



Bool MC_(is_within_valid_secondary) ( Addr a )
{
   SecMap* sm = maybe_get_secmap_for ( a );
   if (sm == NULL || sm == &sm_distinguished[SM_DIST_NOACCESS]) {
      
      return False;
   } else {
      return True;
   }
}


Bool MC_(is_valid_aligned_word) ( Addr a )
{
   tl_assert(sizeof(UWord) == 4 || sizeof(UWord) == 8);
   tl_assert(VG_IS_WORD_ALIGNED(a));
   if (get_vabits8_for_aligned_word32 (a) != VA_BITS8_DEFINED)
      return False;
   if (sizeof(UWord) == 8) {
      if (get_vabits8_for_aligned_word32 (a + 4) != VA_BITS8_DEFINED)
         return False;
   }
   if (UNLIKELY(MC_(in_ignored_range)(a)))
      return False;
   else
      return True;
}



static void init_shadow_memory ( void )
{
   Int     i;
   SecMap* sm;

   tl_assert(V_BIT_UNDEFINED   == 1);
   tl_assert(V_BIT_DEFINED     == 0);
   tl_assert(V_BITS8_UNDEFINED == 0xFF);
   tl_assert(V_BITS8_DEFINED   == 0);

   
   sm = &sm_distinguished[SM_DIST_NOACCESS];
   for (i = 0; i < SM_CHUNKS; i++) sm->vabits8[i] = VA_BITS8_NOACCESS;

   sm = &sm_distinguished[SM_DIST_UNDEFINED];
   for (i = 0; i < SM_CHUNKS; i++) sm->vabits8[i] = VA_BITS8_UNDEFINED;

   sm = &sm_distinguished[SM_DIST_DEFINED];
   for (i = 0; i < SM_CHUNKS; i++) sm->vabits8[i] = VA_BITS8_DEFINED;

   
   /* These entries gradually get overwritten as the used address
      space expands. */
   for (i = 0; i < N_PRIMARY_MAP; i++)
      primary_map[i] = &sm_distinguished[SM_DIST_NOACCESS];

   
   init_auxmap_L1_L2();


   
   secVBitTable = createSecVBitTable();
}



static Bool mc_cheap_sanity_check ( void )
{
   n_sanity_cheap++;
   PROF_EVENT(490, "cheap_sanity_check");
   
   if (MC_(clo_mc_level) < 1 || MC_(clo_mc_level) > 3)
      return False;
   
   return True;
}

static Bool mc_expensive_sanity_check ( void )
{
   Int     i;
   Word    n_secmaps_found;
   SecMap* sm;
   const HChar*  errmsg;
   Bool    bad = False;

   if (0) VG_(printf)("expensive sanity check\n");
   if (0) return True;

   n_sanity_expensive++;
   PROF_EVENT(491, "expensive_sanity_check");

   
   if (MC_(clo_mc_level) < 1 || MC_(clo_mc_level) > 3)
      return False;

   

   
   sm = &sm_distinguished[SM_DIST_NOACCESS];
   for (i = 0; i < SM_CHUNKS; i++)
      if (sm->vabits8[i] != VA_BITS8_NOACCESS)
         bad = True;

   
   sm = &sm_distinguished[SM_DIST_UNDEFINED];
   for (i = 0; i < SM_CHUNKS; i++)
      if (sm->vabits8[i] != VA_BITS8_UNDEFINED)
         bad = True;

   
   sm = &sm_distinguished[SM_DIST_DEFINED];
   for (i = 0; i < SM_CHUNKS; i++)
      if (sm->vabits8[i] != VA_BITS8_DEFINED)
         bad = True;

   if (bad) {
      VG_(printf)("memcheck expensive sanity: "
                  "distinguished_secondaries have changed\n");
      return False;
   }

   if (MC_(clo_mc_level) == 1) {
      if (0 != VG_(OSetGen_Size)(secVBitTable))
         return False;
   }

   
   n_secmaps_found = 0;
   errmsg = check_auxmap_L1_L2_sanity( &n_secmaps_found );
   if (errmsg) {
      VG_(printf)("memcheck expensive sanity, auxmaps:\n\t%s", errmsg);
      return False;
   }

   for (i = 0; i < N_PRIMARY_MAP; i++) {
      if (primary_map[i] == NULL) {
         bad = True;
      } else {
         if (!is_distinguished_sm(primary_map[i]))
            n_secmaps_found++;
      }
   }

   if (n_secmaps_found != (n_issued_SMs - n_deissued_SMs))
      bad = True;

   if (bad) {
      VG_(printf)("memcheck expensive sanity: "
                  "apparent secmap leakage\n");
      return False;
   }

   if (bad) {
      VG_(printf)("memcheck expensive sanity: "
                  "auxmap covers wrong address space\n");
      return False;
   }

   

   return True;
}


#if defined(VGO_darwin)
Bool          MC_(clo_partial_loads_ok)       = True;
#else
Bool          MC_(clo_partial_loads_ok)       = False;
#endif

Long          MC_(clo_freelist_vol)           = 20*1000*1000LL;
Long          MC_(clo_freelist_big_blocks)    =  1*1000*1000LL;
LeakCheckMode MC_(clo_leak_check)             = LC_Summary;
VgRes         MC_(clo_leak_resolution)        = Vg_HighRes;
UInt          MC_(clo_show_leak_kinds)        = R2S(Possible) | R2S(Unreached);
UInt          MC_(clo_error_for_leak_kinds)   = R2S(Possible) | R2S(Unreached);
UInt          MC_(clo_leak_check_heuristics)  = 0;
Bool          MC_(clo_workaround_gcc296_bugs) = False;
Int           MC_(clo_malloc_fill)            = -1;
Int           MC_(clo_free_fill)              = -1;
KeepStacktraces MC_(clo_keep_stacktraces)     = KS_alloc_then_free;
Int           MC_(clo_mc_level)               = 2;
Bool          MC_(clo_show_mismatched_frees)  = True;

static const HChar * MC_(parse_leak_heuristics_tokens) =
   "-,stdstring,length64,newarray,multipleinheritance";

static Bool mc_process_cmd_line_options(const HChar* arg)
{
   const HChar* tmp_str;
   Int   tmp_show;

   tl_assert( MC_(clo_mc_level) >= 1 && MC_(clo_mc_level) <= 3 );

   if (0 == VG_(strcmp)(arg, "--undef-value-errors=no")) {
      if (MC_(clo_mc_level) == 3) {
         goto bad_level;
      } else {
         MC_(clo_mc_level) = 1;
         return True;
      }
   }
   if (0 == VG_(strcmp)(arg, "--undef-value-errors=yes")) {
      if (MC_(clo_mc_level) == 1)
         MC_(clo_mc_level) = 2;
      return True;
   }
   if (0 == VG_(strcmp)(arg, "--track-origins=no")) {
      if (MC_(clo_mc_level) == 3)
         MC_(clo_mc_level) = 2;
      return True;
   }
   if (0 == VG_(strcmp)(arg, "--track-origins=yes")) {
      if (MC_(clo_mc_level) == 1) {
         goto bad_level;
      } else {
         MC_(clo_mc_level) = 3;
         return True;
      }
   }

        if VG_BOOL_CLO(arg, "--partial-loads-ok", MC_(clo_partial_loads_ok)) {}
   else if VG_USET_CLO(arg, "--errors-for-leak-kinds",
                       MC_(parse_leak_kinds_tokens),
                       MC_(clo_error_for_leak_kinds)) {}
   else if VG_USET_CLO(arg, "--show-leak-kinds",
                       MC_(parse_leak_kinds_tokens),
                       MC_(clo_show_leak_kinds)) {}
   else if VG_USET_CLO(arg, "--leak-check-heuristics",
                       MC_(parse_leak_heuristics_tokens),
                       MC_(clo_leak_check_heuristics)) {}
   else if (VG_BOOL_CLO(arg, "--show-reachable", tmp_show)) {
      if (tmp_show) {
         MC_(clo_show_leak_kinds) = MC_(all_Reachedness)();
      } else {
         MC_(clo_show_leak_kinds) &= ~R2S(Reachable);
      }
   }
   else if VG_BOOL_CLO(arg, "--show-possibly-lost", tmp_show) {
      if (tmp_show) {
         MC_(clo_show_leak_kinds) |= R2S(Possible);
      } else {
         MC_(clo_show_leak_kinds) &= ~R2S(Possible);
      }
   }
   else if VG_BOOL_CLO(arg, "--workaround-gcc296-bugs",
                                            MC_(clo_workaround_gcc296_bugs)) {}

   else if VG_BINT_CLO(arg, "--freelist-vol",  MC_(clo_freelist_vol), 
                                               0, 10*1000*1000*1000LL) {}

   else if VG_BINT_CLO(arg, "--freelist-big-blocks",
                       MC_(clo_freelist_big_blocks),
                       0, 10*1000*1000*1000LL) {}

   else if VG_XACT_CLO(arg, "--leak-check=no",
                            MC_(clo_leak_check), LC_Off) {}
   else if VG_XACT_CLO(arg, "--leak-check=summary",
                            MC_(clo_leak_check), LC_Summary) {}
   else if VG_XACT_CLO(arg, "--leak-check=yes",
                            MC_(clo_leak_check), LC_Full) {}
   else if VG_XACT_CLO(arg, "--leak-check=full",
                            MC_(clo_leak_check), LC_Full) {}

   else if VG_XACT_CLO(arg, "--leak-resolution=low",
                            MC_(clo_leak_resolution), Vg_LowRes) {}
   else if VG_XACT_CLO(arg, "--leak-resolution=med",
                            MC_(clo_leak_resolution), Vg_MedRes) {}
   else if VG_XACT_CLO(arg, "--leak-resolution=high",
                            MC_(clo_leak_resolution), Vg_HighRes) {}

   else if VG_STR_CLO(arg, "--ignore-ranges", tmp_str) {
      Bool ok = parse_ignore_ranges(tmp_str);
      if (!ok) {
         VG_(message)(Vg_DebugMsg, 
            "ERROR: --ignore-ranges: "
            "invalid syntax, or end <= start in range\n");
         return False;
      }
      if (gIgnoredAddressRanges) {
         Word i;
         for (i = 0; i < VG_(sizeRangeMap)(gIgnoredAddressRanges); i++) {
            UWord val     = IAR_INVALID;
            UWord key_min = ~(UWord)0;
            UWord key_max = (UWord)0;
            VG_(indexRangeMap)( &key_min, &key_max, &val,
                                gIgnoredAddressRanges, i );
            tl_assert(key_min <= key_max);
            UWord limit = 0x4000000; 
            if (key_max - key_min > limit) {
               VG_(message)(Vg_DebugMsg, 
                  "ERROR: --ignore-ranges: suspiciously large range:\n");
               VG_(message)(Vg_DebugMsg, 
                   "       0x%lx-0x%lx (size %ld)\n", key_min, key_max,
                   key_max - key_min + 1);
               return False;
            }
         }
      }
   }

   else if VG_BHEX_CLO(arg, "--malloc-fill", MC_(clo_malloc_fill), 0x00,0xFF) {}
   else if VG_BHEX_CLO(arg, "--free-fill",   MC_(clo_free_fill),   0x00,0xFF) {}

   else if VG_XACT_CLO(arg, "--keep-stacktraces=alloc",
                       MC_(clo_keep_stacktraces), KS_alloc) {}
   else if VG_XACT_CLO(arg, "--keep-stacktraces=free",
                       MC_(clo_keep_stacktraces), KS_free) {}
   else if VG_XACT_CLO(arg, "--keep-stacktraces=alloc-and-free",
                       MC_(clo_keep_stacktraces), KS_alloc_and_free) {}
   else if VG_XACT_CLO(arg, "--keep-stacktraces=alloc-then-free",
                       MC_(clo_keep_stacktraces), KS_alloc_then_free) {}
   else if VG_XACT_CLO(arg, "--keep-stacktraces=none",
                       MC_(clo_keep_stacktraces), KS_none) {}

   else if VG_BOOL_CLO(arg, "--show-mismatched-frees",
                       MC_(clo_show_mismatched_frees)) {}

   else
      return VG_(replacement_malloc_process_cmd_line_option)(arg);

   return True;


  bad_level:
   VG_(fmsg_bad_option)(arg,
      "--track-origins=yes has no effect when --undef-value-errors=no.\n");
}

static void mc_print_usage(void)
{
   const HChar* plo_default = "no";
#  if defined(VGO_darwin)
   plo_default = "yes";
#  endif

   VG_(printf)(
"    --leak-check=no|summary|full     search for memory leaks at exit?  [summary]\n"
"    --leak-resolution=low|med|high   differentiation of leak stack traces [high]\n"
"    --show-leak-kinds=kind1,kind2,.. which leak kinds to show?\n"
"                                            [definite,possible]\n"
"    --errors-for-leak-kinds=kind1,kind2,..  which leak kinds are errors?\n"
"                                            [definite,possible]\n"
"        where kind is one of:\n"
"          definite indirect possible reachable all none\n"
"    --leak-check-heuristics=heur1,heur2,... which heuristics to use for\n"
"        improving leak search false positive [none]\n"
"        where heur is one of:\n"
"          stdstring length64 newarray multipleinheritance all none\n"
"    --show-reachable=yes             same as --show-leak-kinds=all\n"
"    --show-reachable=no --show-possibly-lost=yes\n"
"                                     same as --show-leak-kinds=definite,possible\n"
"    --show-reachable=no --show-possibly-lost=no\n"
"                                     same as --show-leak-kinds=definite\n"
"    --undef-value-errors=no|yes      check for undefined value errors [yes]\n"
"    --track-origins=no|yes           show origins of undefined values? [no]\n"
"    --partial-loads-ok=no|yes        too hard to explain here; see manual [%s]\n"
"    --freelist-vol=<number>          volume of freed blocks queue     [20000000]\n"
"    --freelist-big-blocks=<number>   releases first blocks with size>= [1000000]\n"
"    --workaround-gcc296-bugs=no|yes  self explanatory [no]\n"
"    --ignore-ranges=0xPP-0xQQ[,0xRR-0xSS]   assume given addresses are OK\n"
"    --malloc-fill=<hexnumber>        fill malloc'd areas with given value\n"
"    --free-fill=<hexnumber>          fill free'd areas with given value\n"
"    --keep-stacktraces=alloc|free|alloc-and-free|alloc-then-free|none\n"
"        stack trace(s) to keep for malloc'd/free'd areas       [alloc-then-free]\n"
"    --show-mismatched-frees=no|yes   show frees that don't match the allocator? [yes]\n"
, plo_default
   );
}

static void mc_print_debug_usage(void)
{  
   VG_(printf)(
"    (none)\n"
   );
}





static UWord      cgb_size = 0;
static UWord      cgb_used = 0;
static CGenBlock* cgbs     = NULL;

static ULong cgb_used_MAX = 0;   
static ULong cgb_allocs   = 0;   
static ULong cgb_discards = 0;   
static ULong cgb_search   = 0;   


void MC_(get_ClientBlock_array)( CGenBlock** blocks,
                                 UWord* nBlocks )
{
   *blocks  = cgbs;
   *nBlocks = cgb_used;
}


static
Int alloc_client_block ( void )
{
   UWord      i, sz_new;
   CGenBlock* cgbs_new;

   cgb_allocs++;

   for (i = 0; i < cgb_used; i++) {
      cgb_search++;
      if (cgbs[i].start == 0 && cgbs[i].size == 0)
         return i;
   }

   
   if (cgb_used < cgb_size) {
      cgb_used++;
      return cgb_used-1;
   }

   
   tl_assert(cgb_used == cgb_size);
   sz_new = (cgbs == NULL) ? 10 : (2 * cgb_size);

   cgbs_new = VG_(malloc)( "mc.acb.1", sz_new * sizeof(CGenBlock) );
   for (i = 0; i < cgb_used; i++) 
      cgbs_new[i] = cgbs[i];

   if (cgbs != NULL)
      VG_(free)( cgbs );
   cgbs = cgbs_new;

   cgb_size = sz_new;
   cgb_used++;
   if (cgb_used > cgb_used_MAX)
      cgb_used_MAX = cgb_used;
   return cgb_used-1;
}


static void show_client_block_stats ( void )
{
   VG_(message)(Vg_DebugMsg, 
      "general CBs: %llu allocs, %llu discards, %llu maxinuse, %llu search\n",
      cgb_allocs, cgb_discards, cgb_used_MAX, cgb_search 
   );
}
static void print_monitor_help ( void )
{
   VG_(gdb_printf) 
      (
"\n"
"memcheck monitor commands:\n"
"  get_vbits <addr> [<len>]\n"
"        returns validity bits for <len> (or 1) bytes at <addr>\n"
"            bit values 0 = valid, 1 = invalid, __ = unaddressable byte\n"
"        Example: get_vbits 0x8049c78 10\n"
"  make_memory [noaccess|undefined\n"
"                     |defined|Definedifaddressable] <addr> [<len>]\n"
"        mark <len> (or 1) bytes at <addr> with the given accessibility\n"
"  check_memory [addressable|defined] <addr> [<len>]\n"
"        check that <len> (or 1) bytes at <addr> have the given accessibility\n"
"            and outputs a description of <addr>\n"
"  leak_check [full*|summary]\n"
"                [kinds kind1,kind2,...|reachable|possibleleak*|definiteleak]\n"
"                [heuristics heur1,heur2,...]\n"
"                [increased*|changed|any]\n"
"                [unlimited*|limited <max_loss_records_output>]\n"
"            * = defaults\n"
"       where kind is one of:\n"
"         definite indirect possible reachable all none\n"
"       where heur is one of:\n"
"         stdstring length64 newarray multipleinheritance all none*\n"
"       Examples: leak_check\n"
"                 leak_check summary any\n"
"                 leak_check full kinds indirect,possible\n"
"                 leak_check full reachable any limited 100\n"
"  block_list <loss_record_nr>\n"
"        after a leak search, shows the list of blocks of <loss_record_nr>\n"
"  who_points_at <addr> [<len>]\n"
"        shows places pointing inside <len> (default 1) bytes at <addr>\n"
"        (with len 1, only shows \"start pointers\" pointing exactly to <addr>,\n"
"         with len > 1, will also show \"interior pointers\")\n"
"\n");
}

static Bool handle_gdb_monitor_command (ThreadId tid, HChar *req)
{
   HChar* wcmd;
   HChar s[VG_(strlen(req)) + 1]; 
   HChar *ssaveptr;

   VG_(strcpy) (s, req);

   wcmd = VG_(strtok_r) (s, " ", &ssaveptr);
   switch (VG_(keyword_id) 
           ("help get_vbits leak_check make_memory check_memory "
            "block_list who_points_at", 
            wcmd, kwd_report_duplicated_matches)) {
   case -2: 
      return True;
   case -1: 
      return False;
   case  0: 
      print_monitor_help();
      return True;
   case  1: { 
      Addr address;
      SizeT szB = 1;
      if (VG_(strtok_get_address_and_size) (&address, &szB, &ssaveptr)) {
         UChar vbits;
         Int i;
         Int unaddressable = 0;
         for (i = 0; i < szB; i++) {
            Int res = mc_get_or_set_vbits_for_client 
               (address+i, (Addr) &vbits, 1, 
                False, 
                False   ); 
            
            if ((i % 32) == 0 && i != 0)
               VG_(printf) ("\n");
            
            else if ((i % 4) == 0 && i != 0)
               VG_(printf) (" ");
            if (res == 1) {
               VG_(printf) ("%02x", vbits);
            } else {
               tl_assert(3 == res);
               unaddressable++;
               VG_(printf) ("__");
            }
         }
         VG_(printf) ("\n");
         if (unaddressable) {
            VG_(printf)
               ("Address %p len %ld has %d bytes unaddressable\n",
                (void *)address, szB, unaddressable);
         }
      }
      return True;
   }
   case  2: { 
      Int err = 0;
      LeakCheckParams lcp;
      HChar* kw;
      
      lcp.mode               = LC_Full;
      lcp.show_leak_kinds    = R2S(Possible) | R2S(Unreached);
      lcp.errors_for_leak_kinds = 0; 
      lcp.heuristics         = 0;
      lcp.deltamode          = LCD_Increased;
      lcp.max_loss_records_output = 999999999;
      lcp.requested_by_monitor_command = True;
      
      for (kw = VG_(strtok_r) (NULL, " ", &ssaveptr); 
           kw != NULL; 
           kw = VG_(strtok_r) (NULL, " ", &ssaveptr)) {
         switch (VG_(keyword_id) 
                 ("full summary "
                  "kinds reachable possibleleak definiteleak "
                  "heuristics "
                  "increased changed any "
                  "unlimited limited ",
                  kw, kwd_report_all)) {
         case -2: err++; break;
         case -1: err++; break;
         case  0: 
            lcp.mode = LC_Full; break;
         case  1: 
            lcp.mode = LC_Summary; break;
         case  2: { 
            wcmd = VG_(strtok_r) (NULL, " ", &ssaveptr);
            if (wcmd == NULL 
                || !VG_(parse_enum_set)(MC_(parse_leak_kinds_tokens),
                                        True,
                                        wcmd,
                                        &lcp.show_leak_kinds)) {
               VG_(gdb_printf) ("missing or malformed leak kinds set\n");
               err++;
            }
            break;
         }
         case  3: 
            lcp.show_leak_kinds = MC_(all_Reachedness)();
            break;
         case  4: 
            lcp.show_leak_kinds 
               = R2S(Possible) | R2S(IndirectLeak) | R2S(Unreached);
            break;
         case  5: 
            lcp.show_leak_kinds = R2S(Unreached);
            break;
         case  6: { 
            wcmd = VG_(strtok_r) (NULL, " ", &ssaveptr);
            if (wcmd == NULL 
                || !VG_(parse_enum_set)(MC_(parse_leak_heuristics_tokens),
                                        True,
                                        wcmd,
                                        &lcp.heuristics)) {
               VG_(gdb_printf) ("missing or malformed heuristics set\n");
               err++;
            }
            break;
         }
         case  7: 
            lcp.deltamode = LCD_Increased; break;
         case  8: 
            lcp.deltamode = LCD_Changed; break;
         case  9: 
            lcp.deltamode = LCD_Any; break;
         case 10: 
            lcp.max_loss_records_output = 999999999; break;
         case 11: { 
            Int int_value;
            const HChar* endptr;

            wcmd = VG_(strtok_r) (NULL, " ", &ssaveptr);
            if (wcmd == NULL) {
               int_value = 0;
               endptr = "empty"; 
            } else {
               HChar *the_end;
               int_value = VG_(strtoll10) (wcmd, &the_end);
               endptr = the_end;
            }
            if (*endptr != '\0')
               VG_(gdb_printf) ("missing or malformed integer value\n");
            else if (int_value > 0)
               lcp.max_loss_records_output = (UInt) int_value;
            else
               VG_(gdb_printf) ("max_loss_records_output must be >= 1, got %d\n",
                                int_value);
            break;
         }
         default:
            tl_assert (0);
         }
      }
      if (!err)
         MC_(detect_memory_leaks)(tid, &lcp);
      return True;
   }
      
   case  3: { 
      Addr address;
      SizeT szB = 1;
      Int kwdid = VG_(keyword_id) 
         ("noaccess undefined defined Definedifaddressable",
          VG_(strtok_r) (NULL, " ", &ssaveptr), kwd_report_all);
      if (!VG_(strtok_get_address_and_size) (&address, &szB, &ssaveptr))
         return True;
      switch (kwdid) {
      case -2: break;
      case -1: break;
      case  0: MC_(make_mem_noaccess) (address, szB); break;
      case  1: make_mem_undefined_w_tid_and_okind ( address, szB, tid, 
                                                    MC_OKIND_USER ); break;
      case  2: MC_(make_mem_defined) ( address, szB ); break;
      case  3: make_mem_defined_if_addressable ( address, szB ); break;;
      default: tl_assert(0);
      }
      return True;
   }

   case  4: { 
      Addr address;
      SizeT szB = 1;
      Addr bad_addr;
      UInt okind;
      const HChar* src;
      UInt otag;
      UInt ecu;
      ExeContext* origin_ec;
      MC_ReadResult res;

      Int kwdid = VG_(keyword_id) 
         ("addressable defined",
          VG_(strtok_r) (NULL, " ", &ssaveptr), kwd_report_all);
      if (!VG_(strtok_get_address_and_size) (&address, &szB, &ssaveptr))
         return True;
      switch (kwdid) {
      case -2: break;
      case -1: break;
      case  0: 
         if (is_mem_addressable ( address, szB, &bad_addr ))
            VG_(printf) ("Address %p len %ld addressable\n", 
                             (void *)address, szB);
         else
            VG_(printf)
               ("Address %p len %ld not addressable:\nbad address %p\n",
                (void *)address, szB, (void *) bad_addr);
         MC_(pp_describe_addr) (address);
         break;
      case  1: 
         res = is_mem_defined ( address, szB, &bad_addr, &otag );
         if (MC_AddrErr == res)
            VG_(printf)
               ("Address %p len %ld not addressable:\nbad address %p\n",
                (void *)address, szB, (void *) bad_addr);
         else if (MC_ValueErr == res) {
            okind = otag & 3;
            switch (okind) {
            case MC_OKIND_STACK:   
               src = " was created by a stack allocation"; break;
            case MC_OKIND_HEAP:    
               src = " was created by a heap allocation"; break;
            case MC_OKIND_USER:    
               src = " was created by a client request"; break;
            case MC_OKIND_UNKNOWN: 
               src = ""; break;
            default: tl_assert(0);
            }
            VG_(printf) 
               ("Address %p len %ld not defined:\n"
                "Uninitialised value at %p%s\n",
                (void *)address, szB, (void *) bad_addr, src);
            ecu = otag & ~3;
            if (VG_(is_plausible_ECU)(ecu)) {
               origin_ec = VG_(get_ExeContext_from_ECU)( ecu );
               VG_(pp_ExeContext)( origin_ec );
            }
         }
         else
            VG_(printf) ("Address %p len %ld defined\n",
                         (void *)address, szB);
         MC_(pp_describe_addr) (address);
         break;
      default: tl_assert(0);
      }
      return True;
   }

   case  5: { 
      HChar* wl;
      HChar *endptr;
      UInt lr_nr = 0;
      wl = VG_(strtok_r) (NULL, " ", &ssaveptr);
      if (wl != NULL)
         lr_nr = VG_(strtoull10) (wl, &endptr);
      if (wl == NULL || *endptr != '\0') {
         VG_(gdb_printf) ("malformed or missing integer\n");
      } else {
         
         if (lr_nr == 0 || ! MC_(print_block_list) (lr_nr-1))
            VG_(gdb_printf) ("invalid loss record nr\n");
      }
      return True;
   }

   case  6: { 
      Addr address;
      SizeT szB = 1;

      if (!VG_(strtok_get_address_and_size) (&address, &szB, &ssaveptr))
         return True;
      if (address == (Addr) 0) {
         VG_(gdb_printf) ("Cannot search who points at 0x0\n");
         return True;
      }
      MC_(who_points_at) (address, szB);
      return True;
   }

   default: 
      tl_assert(0);
      return False;
   }
}


static Bool mc_handle_client_request ( ThreadId tid, UWord* arg, UWord* ret )
{
   Int   i;
   Addr  bad_addr;

   if (!VG_IS_TOOL_USERREQ('M','C',arg[0])
       && VG_USERREQ__MALLOCLIKE_BLOCK != arg[0]
       && VG_USERREQ__RESIZEINPLACE_BLOCK != arg[0]
       && VG_USERREQ__FREELIKE_BLOCK   != arg[0]
       && VG_USERREQ__CREATE_MEMPOOL   != arg[0]
       && VG_USERREQ__DESTROY_MEMPOOL  != arg[0]
       && VG_USERREQ__MEMPOOL_ALLOC    != arg[0]
       && VG_USERREQ__MEMPOOL_FREE     != arg[0]
       && VG_USERREQ__MEMPOOL_TRIM     != arg[0]
       && VG_USERREQ__MOVE_MEMPOOL     != arg[0]
       && VG_USERREQ__MEMPOOL_CHANGE   != arg[0]
       && VG_USERREQ__MEMPOOL_EXISTS   != arg[0]
       && VG_USERREQ__GDB_MONITOR_COMMAND   != arg[0]
       && VG_USERREQ__ENABLE_ADDR_ERROR_REPORTING_IN_RANGE != arg[0]
       && VG_USERREQ__DISABLE_ADDR_ERROR_REPORTING_IN_RANGE != arg[0])
      return False;

   switch (arg[0]) {
      case VG_USERREQ__CHECK_MEM_IS_ADDRESSABLE: {
         Bool ok = is_mem_addressable ( arg[1], arg[2], &bad_addr );
         if (!ok)
            MC_(record_user_error) ( tid, bad_addr, True, 0 );
         *ret = ok ? (UWord)NULL : bad_addr;
         break;
      }

      case VG_USERREQ__CHECK_MEM_IS_DEFINED: {
         Bool errorV    = False;
         Addr bad_addrV = 0;
         UInt otagV     = 0;
         Bool errorA    = False;
         Addr bad_addrA = 0;
         is_mem_defined_comprehensive( 
            arg[1], arg[2],
            &errorV, &bad_addrV, &otagV, &errorA, &bad_addrA
         );
         if (errorV) {
            MC_(record_user_error) ( tid, bad_addrV,
                                     False, otagV );
         }
         if (errorA) {
            MC_(record_user_error) ( tid, bad_addrA,
                                     True, 0 );
         }
         
         *ret = 0;
         if (errorV && !errorA) {
            *ret = bad_addrV;
         }
         if (!errorV && errorA) {
            *ret = bad_addrA;
         }
         if (errorV && errorA) {
            *ret = bad_addrV < bad_addrA ? bad_addrV : bad_addrA;
         }
         break;
      }

      case VG_USERREQ__DO_LEAK_CHECK: {
         LeakCheckParams lcp;
         
         if (arg[1] == 0)
            lcp.mode = LC_Full;
         else if (arg[1] == 1)
            lcp.mode = LC_Summary;
         else {
            VG_(message)(Vg_UserMsg, 
                         "Warning: unknown memcheck leak search mode\n");
            lcp.mode = LC_Full;
         }
          
         lcp.show_leak_kinds = MC_(clo_show_leak_kinds);
         lcp.errors_for_leak_kinds = MC_(clo_error_for_leak_kinds);
         lcp.heuristics = MC_(clo_leak_check_heuristics);

         if (arg[2] == 0)
            lcp.deltamode = LCD_Any;
         else if (arg[2] == 1)
            lcp.deltamode = LCD_Increased;
         else if (arg[2] == 2)
            lcp.deltamode = LCD_Changed;
         else {
            VG_(message)
               (Vg_UserMsg, 
                "Warning: unknown memcheck leak search deltamode\n");
            lcp.deltamode = LCD_Any;
         }
         lcp.max_loss_records_output = 999999999;
         lcp.requested_by_monitor_command = False;
         
         MC_(detect_memory_leaks)(tid, &lcp);
         *ret = 0; 
         break;
      }

      case VG_USERREQ__MAKE_MEM_NOACCESS:
         MC_(make_mem_noaccess) ( arg[1], arg[2] );
         *ret = -1;
         break;

      case VG_USERREQ__MAKE_MEM_UNDEFINED:
         make_mem_undefined_w_tid_and_okind ( arg[1], arg[2], tid, 
                                              MC_OKIND_USER );
         *ret = -1;
         break;

      case VG_USERREQ__MAKE_MEM_DEFINED:
         MC_(make_mem_defined) ( arg[1], arg[2] );
         *ret = -1;
         break;

      case VG_USERREQ__MAKE_MEM_DEFINED_IF_ADDRESSABLE:
         make_mem_defined_if_addressable ( arg[1], arg[2] );
         *ret = -1;
         break;

      case VG_USERREQ__CREATE_BLOCK: 
         if (arg[1] != 0 && arg[2] != 0) {
            i = alloc_client_block();
            
            cgbs[i].start = arg[1];
            cgbs[i].size  = arg[2];
            cgbs[i].desc  = VG_(strdup)("mc.mhcr.1", (HChar *)arg[3]);
            cgbs[i].where = VG_(record_ExeContext) ( tid, 0 );
            *ret = i;
         } else
            *ret = -1;
         break;

      case VG_USERREQ__DISCARD: 
         if (cgbs == NULL 
             || arg[2] >= cgb_used ||
             (cgbs[arg[2]].start == 0 && cgbs[arg[2]].size == 0)) {
            *ret = 1;
         } else {
            tl_assert(arg[2] >= 0 && arg[2] < cgb_used);
            cgbs[arg[2]].start = cgbs[arg[2]].size = 0;
            VG_(free)(cgbs[arg[2]].desc);
            cgb_discards++;
            *ret = 0;
         }
         break;

      case VG_USERREQ__GET_VBITS:
         *ret = mc_get_or_set_vbits_for_client
                   ( arg[1], arg[2], arg[3],
                     False , 
                     True  );
         break;

      case VG_USERREQ__SET_VBITS:
         *ret = mc_get_or_set_vbits_for_client
                   ( arg[1], arg[2], arg[3],
                     True ,
                     True  );
         break;

      case VG_USERREQ__COUNT_LEAKS: { 
         UWord** argp = (UWord**)arg;
         
         
         *argp[1] = MC_(bytes_leaked) + MC_(bytes_indirect);
         *argp[2] = MC_(bytes_dubious);
         *argp[3] = MC_(bytes_reachable);
         *argp[4] = MC_(bytes_suppressed);
         
         
         
         
         *ret = 0;
         return True;
      }
      case VG_USERREQ__COUNT_LEAK_BLOCKS: { 
         UWord** argp = (UWord**)arg;
         
         
         *argp[1] = MC_(blocks_leaked) + MC_(blocks_indirect);
         *argp[2] = MC_(blocks_dubious);
         *argp[3] = MC_(blocks_reachable);
         *argp[4] = MC_(blocks_suppressed);
         
         
         
         
         *ret = 0;
         return True;
      }
      case VG_USERREQ__MALLOCLIKE_BLOCK: {
         Addr p         = (Addr)arg[1];
         SizeT sizeB    =       arg[2];
         UInt rzB       =       arg[3];
         Bool is_zeroed = (Bool)arg[4];

         MC_(new_block) ( tid, p, sizeB, 0, is_zeroed, 
                          MC_AllocCustom, MC_(malloc_list) );
         if (rzB > 0) {
            MC_(make_mem_noaccess) ( p - rzB, rzB);
            MC_(make_mem_noaccess) ( p + sizeB, rzB);
         }
         return True;
      }
      case VG_USERREQ__RESIZEINPLACE_BLOCK: {
         Addr p         = (Addr)arg[1];
         SizeT oldSizeB =       arg[2];
         SizeT newSizeB =       arg[3];
         UInt rzB       =       arg[4];

         MC_(handle_resizeInPlace) ( tid, p, oldSizeB, newSizeB, rzB );
         return True;
      }
      case VG_USERREQ__FREELIKE_BLOCK: {
         Addr p         = (Addr)arg[1];
         UInt rzB       =       arg[2];

         MC_(handle_free) ( tid, p, rzB, MC_AllocCustom );
         return True;
      }

      case _VG_USERREQ__MEMCHECK_RECORD_OVERLAP_ERROR: {
         HChar* s  = (HChar*)arg[1];
         Addr  dst = (Addr) arg[2];
         Addr  src = (Addr) arg[3];
         SizeT len = (SizeT)arg[4];
         MC_(record_overlap_error)(tid, s, src, dst, len);
         return True;
      }

      case VG_USERREQ__CREATE_MEMPOOL: {
         Addr pool      = (Addr)arg[1];
         UInt rzB       =       arg[2];
         Bool is_zeroed = (Bool)arg[3];

         MC_(create_mempool) ( pool, rzB, is_zeroed );
         return True;
      }

      case VG_USERREQ__DESTROY_MEMPOOL: {
         Addr pool      = (Addr)arg[1];

         MC_(destroy_mempool) ( pool );
         return True;
      }

      case VG_USERREQ__MEMPOOL_ALLOC: {
         Addr pool      = (Addr)arg[1];
         Addr addr      = (Addr)arg[2];
         UInt size      =       arg[3];

         MC_(mempool_alloc) ( tid, pool, addr, size );
         return True;
      }

      case VG_USERREQ__MEMPOOL_FREE: {
         Addr pool      = (Addr)arg[1];
         Addr addr      = (Addr)arg[2];

         MC_(mempool_free) ( pool, addr );
         return True;
      }

      case VG_USERREQ__MEMPOOL_TRIM: {
         Addr pool      = (Addr)arg[1];
         Addr addr      = (Addr)arg[2];
         UInt size      =       arg[3];

         MC_(mempool_trim) ( pool, addr, size );
         return True;
      }

      case VG_USERREQ__MOVE_MEMPOOL: {
         Addr poolA     = (Addr)arg[1];
         Addr poolB     = (Addr)arg[2];

         MC_(move_mempool) ( poolA, poolB );
         return True;
      }

      case VG_USERREQ__MEMPOOL_CHANGE: {
         Addr pool      = (Addr)arg[1];
         Addr addrA     = (Addr)arg[2];
         Addr addrB     = (Addr)arg[3];
         UInt size      =       arg[4];

         MC_(mempool_change) ( pool, addrA, addrB, size );
         return True;
      }

      case VG_USERREQ__MEMPOOL_EXISTS: {
         Addr pool      = (Addr)arg[1];

         *ret = (UWord) MC_(mempool_exists) ( pool );
	 return True;
      }

      case VG_USERREQ__GDB_MONITOR_COMMAND: {
         Bool handled = handle_gdb_monitor_command (tid, (HChar*)arg[1]);
         if (handled)
            *ret = 1;
         else
            *ret = 0;
         return handled;
      }

      case VG_USERREQ__DISABLE_ADDR_ERROR_REPORTING_IN_RANGE:
      case VG_USERREQ__ENABLE_ADDR_ERROR_REPORTING_IN_RANGE: {
         Bool addRange
            = arg[0] == VG_USERREQ__DISABLE_ADDR_ERROR_REPORTING_IN_RANGE;
         Bool ok
            = modify_ignore_ranges(addRange, arg[1], arg[2]);
         *ret = ok ? 1 : 0;
         return True;
      }

      default:
         VG_(message)(
            Vg_UserMsg, 
            "Warning: unknown memcheck client request code %llx\n",
            (ULong)arg[0]
         );
         return False;
   }
   return True;
}




#ifdef MC_PROFILE_MEMORY

UInt   MC_(event_ctr)[N_PROF_EVENTS];
HChar* MC_(event_ctr_name)[N_PROF_EVENTS];

static void init_prof_mem ( void )
{
   Int i;
   for (i = 0; i < N_PROF_EVENTS; i++) {
      MC_(event_ctr)[i] = 0;
      MC_(event_ctr_name)[i] = NULL;
   }
}

static void done_prof_mem ( void )
{
   Int  i;
   Bool spaced = False;
   for (i = 0; i < N_PROF_EVENTS; i++) {
      if (!spaced && (i % 10) == 0) {
         VG_(printf)("\n");
         spaced = True;
      }
      if (MC_(event_ctr)[i] > 0) {
         spaced = False;
         VG_(printf)( "prof mem event %3d: %9d   %s\n", 
                      i, MC_(event_ctr)[i],
                      MC_(event_ctr_name)[i] 
                         ? MC_(event_ctr_name)[i] : "unnamed");
      }
   }
}

#else

static void init_prof_mem ( void ) { }
static void done_prof_mem ( void ) { }

#endif




static INLINE UInt merge_origins ( UInt or1, UInt or2 ) {
   return or1 > or2 ? or1 : or2;
}

UWord VG_REGPARM(1) MC_(helperc_b_load1)( Addr a ) {
   OCacheLine* line;
   UChar descr;
   UWord lineoff = oc_line_offset(a);
   UWord byteoff = a & 3; 

   if (OC_ENABLE_ASSERTIONS) {
      tl_assert(lineoff >= 0 && lineoff < OC_W32S_PER_LINE);
   }

   line = find_OCacheLine( a );

   descr = line->descr[lineoff];
   if (OC_ENABLE_ASSERTIONS) {
      tl_assert(descr < 0x10);
   }

   if (LIKELY(0 == (descr & (1 << byteoff))))  {
      return 0;
   } else {
      return line->w32[lineoff];
   }
}

UWord VG_REGPARM(1) MC_(helperc_b_load2)( Addr a ) {
   OCacheLine* line;
   UChar descr;
   UWord lineoff, byteoff;

   if (UNLIKELY(a & 1)) {
      
      UInt oLo   = (UInt)MC_(helperc_b_load1)( a + 0 );
      UInt oHi   = (UInt)MC_(helperc_b_load1)( a + 1 );
      return merge_origins(oLo, oHi);
   }

   lineoff = oc_line_offset(a);
   byteoff = a & 3; 

   if (OC_ENABLE_ASSERTIONS) {
      tl_assert(lineoff >= 0 && lineoff < OC_W32S_PER_LINE);
   }
   line = find_OCacheLine( a );

   descr = line->descr[lineoff];
   if (OC_ENABLE_ASSERTIONS) {
      tl_assert(descr < 0x10);
   }

   if (LIKELY(0 == (descr & (3 << byteoff)))) {
      return 0;
   } else {
      return line->w32[lineoff];
   }
}

UWord VG_REGPARM(1) MC_(helperc_b_load4)( Addr a ) {
   OCacheLine* line;
   UChar descr;
   UWord lineoff;

   if (UNLIKELY(a & 3)) {
      
      UInt oLo   = (UInt)MC_(helperc_b_load2)( a + 0 );
      UInt oHi   = (UInt)MC_(helperc_b_load2)( a + 2 );
      return merge_origins(oLo, oHi);
   }

   lineoff = oc_line_offset(a);
   if (OC_ENABLE_ASSERTIONS) {
      tl_assert(lineoff >= 0 && lineoff < OC_W32S_PER_LINE);
   }

   line = find_OCacheLine( a );

   descr = line->descr[lineoff];
   if (OC_ENABLE_ASSERTIONS) {
      tl_assert(descr < 0x10);
   }

   if (LIKELY(0 == descr)) {
      return 0;
   } else {
      return line->w32[lineoff];
   }
}

UWord VG_REGPARM(1) MC_(helperc_b_load8)( Addr a ) {
   OCacheLine* line;
   UChar descrLo, descrHi, descr;
   UWord lineoff;

   if (UNLIKELY(a & 7)) {
      
      UInt oLo   = (UInt)MC_(helperc_b_load4)( a + 0 );
      UInt oHi   = (UInt)MC_(helperc_b_load4)( a + 4 );
      return merge_origins(oLo, oHi);
   }

   lineoff = oc_line_offset(a);
   if (OC_ENABLE_ASSERTIONS) {
      tl_assert(lineoff == (lineoff & 6)); 
   }

   line = find_OCacheLine( a );

   descrLo = line->descr[lineoff + 0];
   descrHi = line->descr[lineoff + 1];
   descr   = descrLo | descrHi;
   if (OC_ENABLE_ASSERTIONS) {
      tl_assert(descr < 0x10);
   }

   if (LIKELY(0 == descr)) {
      return 0; 
   } else {
      UInt oLo = descrLo == 0 ? 0 : line->w32[lineoff + 0];
      UInt oHi = descrHi == 0 ? 0 : line->w32[lineoff + 1];
      return merge_origins(oLo, oHi);
   }
}

UWord VG_REGPARM(1) MC_(helperc_b_load16)( Addr a ) {
   UInt oLo   = (UInt)MC_(helperc_b_load8)( a + 0 );
   UInt oHi   = (UInt)MC_(helperc_b_load8)( a + 8 );
   UInt oBoth = merge_origins(oLo, oHi);
   return (UWord)oBoth;
}

UWord VG_REGPARM(1) MC_(helperc_b_load32)( Addr a ) {
   UInt oQ0   = (UInt)MC_(helperc_b_load8)( a + 0 );
   UInt oQ1   = (UInt)MC_(helperc_b_load8)( a + 8 );
   UInt oQ2   = (UInt)MC_(helperc_b_load8)( a + 16 );
   UInt oQ3   = (UInt)MC_(helperc_b_load8)( a + 24 );
   UInt oAll  = merge_origins(merge_origins(oQ0, oQ1),
                              merge_origins(oQ2, oQ3));
   return (UWord)oAll;
}



void VG_REGPARM(2) MC_(helperc_b_store1)( Addr a, UWord d32 ) {
   OCacheLine* line;
   UWord lineoff = oc_line_offset(a);
   UWord byteoff = a & 3; 

   if (OC_ENABLE_ASSERTIONS) {
      tl_assert(lineoff >= 0 && lineoff < OC_W32S_PER_LINE);
   }

   line = find_OCacheLine( a );

   if (d32 == 0) {
      line->descr[lineoff] &= ~(1 << byteoff);
   } else {
      line->descr[lineoff] |= (1 << byteoff);
      line->w32[lineoff] = d32;
   }
}

void VG_REGPARM(2) MC_(helperc_b_store2)( Addr a, UWord d32 ) {
   OCacheLine* line;
   UWord lineoff, byteoff;

   if (UNLIKELY(a & 1)) {
      
      MC_(helperc_b_store1)( a + 0, d32 );
      MC_(helperc_b_store1)( a + 1, d32 );
      return;
   }

   lineoff = oc_line_offset(a);
   byteoff = a & 3; 

   if (OC_ENABLE_ASSERTIONS) {
      tl_assert(lineoff >= 0 && lineoff < OC_W32S_PER_LINE);
   }

   line = find_OCacheLine( a );

   if (d32 == 0) {
      line->descr[lineoff] &= ~(3 << byteoff);
   } else {
      line->descr[lineoff] |= (3 << byteoff);
      line->w32[lineoff] = d32;
   }
}

void VG_REGPARM(2) MC_(helperc_b_store4)( Addr a, UWord d32 ) {
   OCacheLine* line;
   UWord lineoff;

   if (UNLIKELY(a & 3)) {
      
      MC_(helperc_b_store2)( a + 0, d32 );
      MC_(helperc_b_store2)( a + 2, d32 );
      return;
   }

   lineoff = oc_line_offset(a);
   if (OC_ENABLE_ASSERTIONS) {
      tl_assert(lineoff >= 0 && lineoff < OC_W32S_PER_LINE);
   }

   line = find_OCacheLine( a );

   if (d32 == 0) {
      line->descr[lineoff] = 0;
   } else {
      line->descr[lineoff] = 0xF;
      line->w32[lineoff] = d32;
   }
}

void VG_REGPARM(2) MC_(helperc_b_store8)( Addr a, UWord d32 ) {
   OCacheLine* line;
   UWord lineoff;

   if (UNLIKELY(a & 7)) {
      
      MC_(helperc_b_store4)( a + 0, d32 );
      MC_(helperc_b_store4)( a + 4, d32 );
      return;
   }

   lineoff = oc_line_offset(a);
   if (OC_ENABLE_ASSERTIONS) {
      tl_assert(lineoff == (lineoff & 6)); 
   }

   line = find_OCacheLine( a );

   if (d32 == 0) {
      line->descr[lineoff + 0] = 0;
      line->descr[lineoff + 1] = 0;
   } else {
      line->descr[lineoff + 0] = 0xF;
      line->descr[lineoff + 1] = 0xF;
      line->w32[lineoff + 0] = d32;
      line->w32[lineoff + 1] = d32;
   }
}

void VG_REGPARM(2) MC_(helperc_b_store16)( Addr a, UWord d32 ) {
   MC_(helperc_b_store8)( a + 0, d32 );
   MC_(helperc_b_store8)( a + 8, d32 );
}

void VG_REGPARM(2) MC_(helperc_b_store32)( Addr a, UWord d32 ) {
   MC_(helperc_b_store8)( a +  0, d32 );
   MC_(helperc_b_store8)( a +  8, d32 );
   MC_(helperc_b_store8)( a + 16, d32 );
   MC_(helperc_b_store8)( a + 24, d32 );
}



__attribute__((noinline))
static void ocache_sarp_Set_Origins ( Addr a, UWord len, UInt otag ) {
   if ((a & 1) && len >= 1) {
      MC_(helperc_b_store1)( a, otag );
      a++;
      len--;
   }
   if ((a & 2) && len >= 2) {
      MC_(helperc_b_store2)( a, otag );
      a += 2;
      len -= 2;
   }
   if (len >= 4) 
      tl_assert(0 == (a & 3));
   while (len >= 4) {
      MC_(helperc_b_store4)( a, otag );
      a += 4;
      len -= 4;
   }
   if (len >= 2) {
      MC_(helperc_b_store2)( a, otag );
      a += 2;
      len -= 2;
   }
   if (len >= 1) {
      MC_(helperc_b_store1)( a, otag );
      
      len--;
   }
   tl_assert(len == 0);
}

__attribute__((noinline))
static void ocache_sarp_Clear_Origins ( Addr a, UWord len ) {
   if ((a & 1) && len >= 1) {
      MC_(helperc_b_store1)( a, 0 );
      a++;
      len--;
   }
   if ((a & 2) && len >= 2) {
      MC_(helperc_b_store2)( a, 0 );
      a += 2;
      len -= 2;
   }
   if (len >= 4) 
      tl_assert(0 == (a & 3));
   while (len >= 4) {
      MC_(helperc_b_store4)( a, 0 );
      a += 4;
      len -= 4;
   }
   if (len >= 2) {
      MC_(helperc_b_store2)( a, 0 );
      a += 2;
      len -= 2;
   }
   if (len >= 1) {
      MC_(helperc_b_store1)( a, 0 );
      
      len--;
   }
   tl_assert(len == 0);
}



static void mc_post_clo_init ( void )
{
   if (VG_(clo_xml)) {
      
      MC_(clo_leak_check) = LC_Full;
   }

   if (MC_(clo_freelist_big_blocks) >= MC_(clo_freelist_vol))
      VG_(message)(Vg_UserMsg,
                   "Warning: --freelist-big-blocks value %lld has no effect\n"
                   "as it is >= to --freelist-vol value %lld\n",
                   MC_(clo_freelist_big_blocks),
                   MC_(clo_freelist_vol));

   tl_assert( MC_(clo_mc_level) >= 1 && MC_(clo_mc_level) <= 3 );

   if (MC_(clo_mc_level) == 3) {
      
#     ifdef PERF_FAST_STACK
      VG_(track_new_mem_stack_4_w_ECU)   ( mc_new_mem_stack_4_w_ECU   );
      VG_(track_new_mem_stack_8_w_ECU)   ( mc_new_mem_stack_8_w_ECU   );
      VG_(track_new_mem_stack_12_w_ECU)  ( mc_new_mem_stack_12_w_ECU  );
      VG_(track_new_mem_stack_16_w_ECU)  ( mc_new_mem_stack_16_w_ECU  );
      VG_(track_new_mem_stack_32_w_ECU)  ( mc_new_mem_stack_32_w_ECU  );
      VG_(track_new_mem_stack_112_w_ECU) ( mc_new_mem_stack_112_w_ECU );
      VG_(track_new_mem_stack_128_w_ECU) ( mc_new_mem_stack_128_w_ECU );
      VG_(track_new_mem_stack_144_w_ECU) ( mc_new_mem_stack_144_w_ECU );
      VG_(track_new_mem_stack_160_w_ECU) ( mc_new_mem_stack_160_w_ECU );
#     endif
      VG_(track_new_mem_stack_w_ECU)     ( mc_new_mem_stack_w_ECU     );
      VG_(track_new_mem_stack_signal)    ( mc_new_mem_w_tid_make_ECU );
   } else {
      
#     ifdef PERF_FAST_STACK
      VG_(track_new_mem_stack_4)   ( mc_new_mem_stack_4   );
      VG_(track_new_mem_stack_8)   ( mc_new_mem_stack_8   );
      VG_(track_new_mem_stack_12)  ( mc_new_mem_stack_12  );
      VG_(track_new_mem_stack_16)  ( mc_new_mem_stack_16  );
      VG_(track_new_mem_stack_32)  ( mc_new_mem_stack_32  );
      VG_(track_new_mem_stack_112) ( mc_new_mem_stack_112 );
      VG_(track_new_mem_stack_128) ( mc_new_mem_stack_128 );
      VG_(track_new_mem_stack_144) ( mc_new_mem_stack_144 );
      VG_(track_new_mem_stack_160) ( mc_new_mem_stack_160 );
#     endif
      VG_(track_new_mem_stack)     ( mc_new_mem_stack     );
      VG_(track_new_mem_stack_signal) ( mc_new_mem_w_tid_no_ECU );
   }

   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   if (MC_(clo_mc_level) == 3)
      VG_(track_new_mem_brk)         ( mc_new_mem_w_tid_make_ECU );
   else
      VG_(track_new_mem_brk)         ( mc_new_mem_w_tid_no_ECU );

   if (MC_(clo_mc_level) >= 3) {
      init_OCache();
      tl_assert(ocacheL1 != NULL);
      tl_assert(ocacheL2 != NULL);
   } else {
      tl_assert(ocacheL1 == NULL);
      tl_assert(ocacheL2 == NULL);
   }

   MC_(chunk_poolalloc) = VG_(newPA)
      (sizeof(MC_Chunk) + MC_(n_where_pointers)() * sizeof(ExeContext*),
       1000,
       VG_(malloc),
       "mc.cMC.1 (MC_Chunk pools)",
       VG_(free));

   
   if (MC_(clo_mc_level) >= 2)
      VG_(track_pre_reg_read) ( mc_pre_reg_read );
}

static void print_SM_info(const HChar* type, Int n_SMs)
{
   VG_(message)(Vg_DebugMsg,
      " memcheck: SMs: %s = %d (%ldk, %ldM)\n",
      type,
      n_SMs,
      n_SMs * sizeof(SecMap) / 1024UL,
      n_SMs * sizeof(SecMap) / (1024 * 1024UL) );
}

static void mc_print_stats (void)
{
   SizeT max_secVBit_szB, max_SMs_szB, max_shmem_szB;

   VG_(message)(Vg_DebugMsg, " memcheck: freelist: vol %lld length %lld\n",
                VG_(free_queue_volume), VG_(free_queue_length));
   VG_(message)(Vg_DebugMsg,
      " memcheck: sanity checks: %d cheap, %d expensive\n",
      n_sanity_cheap, n_sanity_expensive );
   VG_(message)(Vg_DebugMsg,
      " memcheck: auxmaps: %lld auxmap entries (%lldk, %lldM) in use\n",
      n_auxmap_L2_nodes, 
      n_auxmap_L2_nodes * 64, 
      n_auxmap_L2_nodes / 16 );
   VG_(message)(Vg_DebugMsg,
      " memcheck: auxmaps_L1: %lld searches, %lld cmps, ratio %lld:10\n",
      n_auxmap_L1_searches, n_auxmap_L1_cmps,
      (10ULL * n_auxmap_L1_cmps) 
         / (n_auxmap_L1_searches ? n_auxmap_L1_searches : 1) 
   );   
   VG_(message)(Vg_DebugMsg,
      " memcheck: auxmaps_L2: %lld searches, %lld nodes\n",
      n_auxmap_L2_searches, n_auxmap_L2_nodes
   );   

   print_SM_info("n_issued     ", n_issued_SMs);
   print_SM_info("n_deissued   ", n_deissued_SMs);
   print_SM_info("max_noaccess ", max_noaccess_SMs);
   print_SM_info("max_undefined", max_undefined_SMs);
   print_SM_info("max_defined  ", max_defined_SMs);
   print_SM_info("max_non_DSM  ", max_non_DSM_SMs);

   
   max_SMs_szB = (3 + max_non_DSM_SMs) * sizeof(SecMap);
   
   
   
   
   
   
   max_secVBit_szB = max_secVBit_nodes * 
         (3*sizeof(Word) + VG_ROUNDUP(sizeof(SecVBitNode), sizeof(void*)));
   max_shmem_szB   = sizeof(primary_map) + max_SMs_szB + max_secVBit_szB;

   VG_(message)(Vg_DebugMsg,
      " memcheck: max sec V bit nodes:    %d (%ldk, %ldM)\n",
      max_secVBit_nodes, max_secVBit_szB / 1024,
                         max_secVBit_szB / (1024 * 1024));
   VG_(message)(Vg_DebugMsg,
      " memcheck: set_sec_vbits8 calls: %llu (new: %llu, updates: %llu)\n",
      sec_vbits_new_nodes + sec_vbits_updates,
      sec_vbits_new_nodes, sec_vbits_updates );
   VG_(message)(Vg_DebugMsg,
      " memcheck: max shadow mem size:   %ldk, %ldM\n",
      max_shmem_szB / 1024, max_shmem_szB / (1024 * 1024));

   if (MC_(clo_mc_level) >= 3) {
      VG_(message)(Vg_DebugMsg,
                   " ocacheL1: %'12lu refs   %'12lu misses (%'lu lossage)\n",
                   stats_ocacheL1_find, 
                   stats_ocacheL1_misses,
                   stats_ocacheL1_lossage );
      VG_(message)(Vg_DebugMsg,
                   " ocacheL1: %'12lu at 0   %'12lu at 1\n",
                   stats_ocacheL1_find - stats_ocacheL1_misses 
                      - stats_ocacheL1_found_at_1 
                      - stats_ocacheL1_found_at_N,
                   stats_ocacheL1_found_at_1 );
      VG_(message)(Vg_DebugMsg,
                   " ocacheL1: %'12lu at 2+  %'12lu move-fwds\n",
                   stats_ocacheL1_found_at_N,
                   stats_ocacheL1_movefwds );
      VG_(message)(Vg_DebugMsg,
                   " ocacheL1: %'12lu sizeB  %'12u useful\n",
                   (UWord)sizeof(OCache),
                   4 * OC_W32S_PER_LINE * OC_LINES_PER_SET * OC_N_SETS );
      VG_(message)(Vg_DebugMsg,
                   " ocacheL2: %'12lu refs   %'12lu misses\n",
                   stats__ocacheL2_refs, 
                   stats__ocacheL2_misses );
      VG_(message)(Vg_DebugMsg,
                   " ocacheL2:    %'9lu max nodes %'9lu curr nodes\n",
                   stats__ocacheL2_n_nodes_max,
                   stats__ocacheL2_n_nodes );
      VG_(message)(Vg_DebugMsg,
                   " niacache: %'12lu refs   %'12lu misses\n",
                   stats__nia_cache_queries, stats__nia_cache_misses);
   } else {
      tl_assert(ocacheL1 == NULL);
      tl_assert(ocacheL2 == NULL);
   }
}


static void mc_fini ( Int exitcode )
{
   MC_(print_malloc_stats)();

   if (MC_(clo_leak_check) != LC_Off) {
      LeakCheckParams lcp;
      lcp.mode = MC_(clo_leak_check);
      lcp.show_leak_kinds = MC_(clo_show_leak_kinds);
      lcp.heuristics = MC_(clo_leak_check_heuristics);
      lcp.errors_for_leak_kinds = MC_(clo_error_for_leak_kinds);
      lcp.deltamode = LCD_Any;
      lcp.max_loss_records_output = 999999999;
      lcp.requested_by_monitor_command = False;
      MC_(detect_memory_leaks)(1, &lcp);
   } else {
      if (VG_(clo_verbosity) == 1 && !VG_(clo_xml)) {
         VG_(umsg)(
            "For a detailed leak analysis, rerun with: --leak-check=full\n"
            "\n"
         );
      }
   }

   if (VG_(clo_verbosity) == 1 && !VG_(clo_xml)) {
      VG_(message)(Vg_UserMsg, 
                   "For counts of detected and suppressed errors, rerun with: -v\n");
   }

   if (MC_(any_value_errors) && !VG_(clo_xml) && VG_(clo_verbosity) >= 1
       && MC_(clo_mc_level) == 2) {
      VG_(message)(Vg_UserMsg,
                   "Use --track-origins=yes to see where "
                   "uninitialised values come from\n");
   }

   /* Print a warning if any client-request generated ignore-ranges
      still exist.  It would be reasonable to expect that a properly
      written program would remove any such ranges before exiting, and
      since they are a bit on the dangerous side, let's comment.  By
      contrast ranges which are specified on the command line normally
      pertain to hardware mapped into the address space, and so we
      can't expect the client to have got rid of them. */
   if (gIgnoredAddressRanges) {
      Word i, nBad = 0;
      for (i = 0; i < VG_(sizeRangeMap)(gIgnoredAddressRanges); i++) {
         UWord val     = IAR_INVALID;
         UWord key_min = ~(UWord)0;
         UWord key_max = (UWord)0;
         VG_(indexRangeMap)( &key_min, &key_max, &val,
                             gIgnoredAddressRanges, i );
         if (val != IAR_ClientReq)
           continue;
         nBad++;
         if (nBad == 1) {
            VG_(umsg)(
              "WARNING: exiting program has the following client-requested\n"
              "WARNING: address error disablement range(s) still in force,\n"
              "WARNING: "
                 "possibly as a result of some mistake in the use of the\n"
              "WARNING: "
                 "VALGRIND_{DISABLE,ENABLE}_ERROR_REPORTING_IN_RANGE macros.\n"
            );
         }
         VG_(umsg)("   [%ld]  0x%016llx-0x%016llx  %s\n",
                   i, (ULong)key_min, (ULong)key_max, showIARKind(val));
      }
   }

   done_prof_mem();

   if (VG_(clo_stats))
      mc_print_stats();

   if (0) {
      VG_(message)(Vg_DebugMsg, 
        "------ Valgrind's client block stats follow ---------------\n" );
      show_client_block_stats();
   }
}

static Bool mc_mark_unaddressable_for_watchpoint (PointKind kind, Bool insert,
                                                  Addr addr, SizeT len)
{
   if (insert)
      MC_(make_mem_noaccess) (addr, len);
   else
      MC_(make_mem_defined)  (addr, len);
   return True;
}

static void mc_pre_clo_init(void)
{
   VG_(details_name)            ("Memcheck");
   VG_(details_version)         (NULL);
   VG_(details_description)     ("a memory error detector");
   VG_(details_copyright_author)(
      "Copyright (C) 2002-2013, and GNU GPL'd, by Julian Seward et al.");
   VG_(details_bug_reports_to)  (VG_BUGS_TO);
   VG_(details_avg_translation_sizeB) ( 640 );

   VG_(basic_tool_funcs)          (mc_post_clo_init,
                                   MC_(instrument),
                                   mc_fini);

   VG_(needs_final_IR_tidy_pass)  ( MC_(final_tidy) );


   VG_(needs_core_errors)         ();
   VG_(needs_tool_errors)         (MC_(eq_Error),
                                   MC_(before_pp_Error),
                                   MC_(pp_Error),
                                   True,
                                   MC_(update_Error_extra),
                                   MC_(is_recognised_suppression),
                                   MC_(read_extra_suppression_info),
                                   MC_(error_matches_suppression),
                                   MC_(get_error_name),
                                   MC_(get_extra_suppression_info),
                                   MC_(print_extra_suppression_use),
                                   MC_(update_extra_suppression_use));
   VG_(needs_libc_freeres)        ();
   VG_(needs_command_line_options)(mc_process_cmd_line_options,
                                   mc_print_usage,
                                   mc_print_debug_usage);
   VG_(needs_client_requests)     (mc_handle_client_request);
   VG_(needs_sanity_checks)       (mc_cheap_sanity_check,
                                   mc_expensive_sanity_check);
   VG_(needs_print_stats)         (mc_print_stats);
   VG_(needs_info_location)       (MC_(pp_describe_addr));
   VG_(needs_malloc_replacement)  (MC_(malloc),
                                   MC_(__builtin_new),
                                   MC_(__builtin_vec_new),
                                   MC_(memalign),
                                   MC_(calloc),
                                   MC_(free),
                                   MC_(__builtin_delete),
                                   MC_(__builtin_vec_delete),
                                   MC_(realloc),
                                   MC_(malloc_usable_size), 
                                   MC_MALLOC_DEFAULT_REDZONE_SZB );
   MC_(Malloc_Redzone_SzB) = VG_(malloc_effective_client_redzone_size)();

   VG_(needs_xml_output)          ();

   VG_(track_new_mem_startup)     ( mc_new_mem_startup );

   
   
   
   VG_(track_new_mem_mmap)        ( mc_new_mem_mmap );
   VG_(track_change_mem_mprotect) ( mc_new_mem_mprotect );
   
   VG_(track_copy_mem_remap)      ( MC_(copy_address_range_state) );

   VG_(track_die_mem_stack_signal)( MC_(make_mem_noaccess) ); 
   VG_(track_die_mem_brk)         ( MC_(make_mem_noaccess) );
   VG_(track_die_mem_munmap)      ( MC_(make_mem_noaccess) ); 


#  ifdef PERF_FAST_STACK
   VG_(track_die_mem_stack_4)     ( mc_die_mem_stack_4   );
   VG_(track_die_mem_stack_8)     ( mc_die_mem_stack_8   );
   VG_(track_die_mem_stack_12)    ( mc_die_mem_stack_12  );
   VG_(track_die_mem_stack_16)    ( mc_die_mem_stack_16  );
   VG_(track_die_mem_stack_32)    ( mc_die_mem_stack_32  );
   VG_(track_die_mem_stack_112)   ( mc_die_mem_stack_112 );
   VG_(track_die_mem_stack_128)   ( mc_die_mem_stack_128 );
   VG_(track_die_mem_stack_144)   ( mc_die_mem_stack_144 );
   VG_(track_die_mem_stack_160)   ( mc_die_mem_stack_160 );
#  endif
   VG_(track_die_mem_stack)       ( mc_die_mem_stack     );
   
   VG_(track_ban_mem_stack)       ( MC_(make_mem_noaccess) );

   VG_(track_pre_mem_read)        ( check_mem_is_defined );
   VG_(track_pre_mem_read_asciiz) ( check_mem_is_defined_asciiz );
   VG_(track_pre_mem_write)       ( check_mem_is_addressable );
   VG_(track_post_mem_write)      ( mc_post_mem_write );

   VG_(track_post_reg_write)                  ( mc_post_reg_write );
   VG_(track_post_reg_write_clientcall_return)( mc_post_reg_write_clientcall );

   VG_(needs_watchpoint)          ( mc_mark_unaddressable_for_watchpoint );

   init_shadow_memory();
   
   tl_assert(MC_(chunk_poolalloc) == NULL);
   MC_(malloc_list)  = VG_(HT_construct)( "MC_(malloc_list)" );
   MC_(mempool_list) = VG_(HT_construct)( "MC_(mempool_list)" );
   init_prof_mem();

   tl_assert( mc_expensive_sanity_check() );

   
   tl_assert(sizeof(UWord) == sizeof(Addr));
   
   tl_assert(sizeof(void*) == sizeof(Addr));

   
   tl_assert(-1 != VG_(log2)(BYTES_PER_SEC_VBIT_NODE));

   
   init_nia_to_ecu_cache();

   tl_assert(ocacheL1 == NULL);
   tl_assert(ocacheL2 == NULL);

#  if VG_WORDSIZE == 4
   tl_assert(sizeof(void*) == 4);
   tl_assert(sizeof(Addr)  == 4);
   tl_assert(sizeof(UWord) == 4);
   tl_assert(sizeof(Word)  == 4);
   tl_assert(MAX_PRIMARY_ADDRESS == 0xFFFFFFFFUL);
   tl_assert(MASK(1) == 0UL);
   tl_assert(MASK(2) == 1UL);
   tl_assert(MASK(4) == 3UL);
   tl_assert(MASK(8) == 7UL);
#  else
   tl_assert(VG_WORDSIZE == 8);
   tl_assert(sizeof(void*) == 8);
   tl_assert(sizeof(Addr)  == 8);
   tl_assert(sizeof(UWord) == 8);
   tl_assert(sizeof(Word)  == 8);
   tl_assert(MAX_PRIMARY_ADDRESS == 0xFFFFFFFFFULL);
   tl_assert(MASK(1) == 0xFFFFFFF000000000ULL);
   tl_assert(MASK(2) == 0xFFFFFFF000000001ULL);
   tl_assert(MASK(4) == 0xFFFFFFF000000003ULL);
   tl_assert(MASK(8) == 0xFFFFFFF000000007ULL);
#  endif
}

VG_DETERMINE_INTERFACE_VERSION(mc_pre_clo_init)

