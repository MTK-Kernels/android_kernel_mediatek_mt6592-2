
/*
   This file is part of Callgrind, a Valgrind tool for call graph
   profiling programs.

   Copyright (C) 2003-2013, Josef Weidendorfer (Josef.Weidendorfer@gmx.de)

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

#include "global.h"



#include "cg_arch.c"

typedef struct {
  UInt count;
  UInt mask; 
} line_use;

typedef struct {
  Addr memline, iaddr;
  line_use* dep_use; 
  ULong* use_base;
} line_loaded;  

typedef struct {
   const HChar* name;
   int          size;                   
   int          assoc;
   int          line_size;              
   Bool         sectored;  
   int          sets;
   int          sets_min_1;
   int          line_size_bits;
   int          tag_shift;
   UWord        tag_mask;
   HChar        desc_line[128];    
   UWord*       tags;

  
   int          line_size_mask;
   int*         line_start_mask;
   int*         line_end_mask;
   line_loaded* loaded;
   line_use*    use;
} cache_t2;

static cache_t2 I1, D1, LL;

#define CACHELINE_FLAGMASK (MIN_LINE_SIZE-1)
#define CACHELINE_DIRTY    1


static Bool clo_simulate_writeback = False;
static Bool clo_simulate_hwpref = False;
static Bool clo_simulate_sectors = False;
static Bool clo_collect_cacheuse = False;


Addr   CLG_(bb_base);
ULong* CLG_(cost_base);

static InstrInfo* current_ii;

static Int off_I1_AcCost  = 0;
static Int off_I1_SpLoss  = 1;
static Int off_D1_AcCost  = 0;
static Int off_D1_SpLoss  = 1;
static Int off_LL_AcCost  = 2;
static Int off_LL_SpLoss  = 3;

typedef enum { Read = 0, Write = CACHELINE_DIRTY } RefType;

typedef enum { Hit  = 0, Miss, MissDirty } CacheResult;

typedef enum {
    L1_Hit, 
    LL_Hit,
    MemAccess,
    WriteBackMemAccess } CacheModelResult;

typedef CacheModelResult (*simcall_type)(Addr, UChar);

static struct {
    simcall_type I1_Read;
    simcall_type D1_Read;
    simcall_type D1_Write;
} simulator;


static void cachesim_clearcache(cache_t2* c)
{
  Int i;

  for (i = 0; i < c->sets * c->assoc; i++)
    c->tags[i] = 0;
  if (c->use) {
    for (i = 0; i < c->sets * c->assoc; i++) {
      c->loaded[i].memline  = 0;
      c->loaded[i].use_base = 0;
      c->loaded[i].dep_use = 0;
      c->loaded[i].iaddr = 0;
      c->use[i].mask    = 0;
      c->use[i].count   = 0;
      c->tags[i] = i % c->assoc; 
    }
  }
}

static void cacheuse_initcache(cache_t2* c);

static void cachesim_initcache(cache_t config, cache_t2* c)
{
   c->size      = config.size;
   c->assoc     = config.assoc;
   c->line_size = config.line_size;
   c->sectored  = False; 

   c->sets           = (c->size / c->line_size) / c->assoc;
   c->sets_min_1     = c->sets - 1;
   c->line_size_bits = VG_(log2)(c->line_size);
   c->tag_shift     = c->line_size_bits + VG_(log2)(c->sets);
   c->tag_mask       = ~((1u<<c->tag_shift)-1);

   CLG_ASSERT( (c->tag_mask & CACHELINE_FLAGMASK) == 0);

   if (c->assoc == 1) {
      VG_(sprintf)(c->desc_line, "%d B, %d B, direct-mapped%s",
		   c->size, c->line_size,
		   c->sectored ? ", sectored":"");
   } else {
      VG_(sprintf)(c->desc_line, "%d B, %d B, %d-way associative%s",
		   c->size, c->line_size, c->assoc,
		   c->sectored ? ", sectored":"");
   }

   c->tags = (UWord*) CLG_MALLOC("cl.sim.cs_ic.1",
                                 sizeof(UWord) * c->sets * c->assoc);
   if (clo_collect_cacheuse)
       cacheuse_initcache(c);
   else
     c->use = 0;
   cachesim_clearcache(c);
}


#if 0
static void print_cache(cache_t2* c)
{
   UInt set, way, i;

   
   for (i = 0, set = 0; set < c->sets; set++) {
      for (way = 0; way < c->assoc; way++, i++) {
         VG_(printf)("%8x ", c->tags[i]);
      }
      VG_(printf)("\n");
   }
}
#endif 



__attribute__((always_inline))
static __inline__
CacheResult cachesim_setref(cache_t2* c, UInt set_no, UWord tag)
{
    int i, j;
    UWord *set;

    set = &(c->tags[set_no * c->assoc]);

    
    
    
    if (tag == set[0])
        return Hit;

    
    
    for (i = 1; i < c->assoc; i++) {
        if (tag == set[i]) {
            for (j = i; j > 0; j--) {
                set[j] = set[j - 1];
            }
            set[0] = tag;
            return Hit;
        }
    }

    
    for (j = c->assoc - 1; j > 0; j--) {
        set[j] = set[j - 1];
    }
    set[0] = tag;

    return Miss;
}

__attribute__((always_inline))
static __inline__
CacheResult cachesim_ref(cache_t2* c, Addr a, UChar size)
{
    UWord block1 =  a         >> c->line_size_bits;
    UWord block2 = (a+size-1) >> c->line_size_bits;
    UInt  set1   = block1 & c->sets_min_1;
    UWord tag1   = block1;

    
    if (block1 == block2)
	return cachesim_setref(c, set1, tag1);

    
    else if (block1 + 1 == block2) {
        UInt  set2 = block2 & c->sets_min_1;
        UWord tag2 = block2;

	
	CacheResult res1 =  cachesim_setref(c, set1, tag1);
	CacheResult res2 =  cachesim_setref(c, set2, tag2);
	return ((res1 == Miss) || (res2 == Miss)) ? Miss : Hit;

   } else {
       VG_(printf)("addr: %lx  size: %u  blocks: %ld %ld",
		   a, size, block1, block2);
       VG_(tool_panic)("item straddles more than two cache sets");
   }
   return Hit;
}

static
CacheModelResult cachesim_I1_ref(Addr a, UChar size)
{
    if ( cachesim_ref( &I1, a, size) == Hit ) return L1_Hit;
    if ( cachesim_ref( &LL, a, size) == Hit ) return LL_Hit;
    return MemAccess;
}

static
CacheModelResult cachesim_D1_ref(Addr a, UChar size)
{
    if ( cachesim_ref( &D1, a, size) == Hit ) return L1_Hit;
    if ( cachesim_ref( &LL, a, size) == Hit ) return LL_Hit;
    return MemAccess;
}




__attribute__((always_inline))
static __inline__
CacheResult cachesim_setref_wb(cache_t2* c, RefType ref, UInt set_no, UWord tag)
{
    int i, j;
    UWord *set, tmp_tag;

    set = &(c->tags[set_no * c->assoc]);

    
    
    
    if (tag == (set[0] & ~CACHELINE_DIRTY)) {
	set[0] |= ref;
        return Hit;
    }
    
    
    for (i = 1; i < c->assoc; i++) {
	if (tag == (set[i] & ~CACHELINE_DIRTY)) {
	    tmp_tag = set[i] | ref; 
            for (j = i; j > 0; j--) {
                set[j] = set[j - 1];
            }
            set[0] = tmp_tag;
            return Hit;
        }
    }

    
    tmp_tag = set[c->assoc - 1];
    for (j = c->assoc - 1; j > 0; j--) {
        set[j] = set[j - 1];
    }
    set[0] = tag | ref;

    return (tmp_tag & CACHELINE_DIRTY) ? MissDirty : Miss;
}

__attribute__((always_inline))
static __inline__
CacheResult cachesim_ref_wb(cache_t2* c, RefType ref, Addr a, UChar size)
{
    UInt set1 = ( a         >> c->line_size_bits) & (c->sets_min_1);
    UInt set2 = ((a+size-1) >> c->line_size_bits) & (c->sets_min_1);
    UWord tag = a & c->tag_mask;

    
    if (set1 == set2)
	return cachesim_setref_wb(c, ref, set1, tag);

    
    
    else if (((set1 + 1) & (c->sets_min_1)) == set2) {
	UWord tag2  = (a+size-1) & c->tag_mask;

	
	CacheResult res1 =  cachesim_setref_wb(c, ref, set1, tag);
	CacheResult res2 =  cachesim_setref_wb(c, ref, set2, tag2);

	if ((res1 == MissDirty) || (res2 == MissDirty)) return MissDirty;
	return ((res1 == Miss) || (res2 == Miss)) ? Miss : Hit;

   } else {
       VG_(printf)("addr: %lx  size: %u  sets: %d %d", a, size, set1, set2);
       VG_(tool_panic)("item straddles more than two cache sets");
   }
   return Hit;
}


static
CacheModelResult cachesim_I1_Read(Addr a, UChar size)
{
    if ( cachesim_ref( &I1, a, size) == Hit ) return L1_Hit;
    switch( cachesim_ref_wb( &LL, Read, a, size) ) {
	case Hit: return LL_Hit;
	case Miss: return MemAccess;
	default: break;
    }
    return WriteBackMemAccess;
}

static
CacheModelResult cachesim_D1_Read(Addr a, UChar size)
{
    if ( cachesim_ref( &D1, a, size) == Hit ) return L1_Hit;
    switch( cachesim_ref_wb( &LL, Read, a, size) ) {
	case Hit: return LL_Hit;
	case Miss: return MemAccess;
	default: break;
    }
    return WriteBackMemAccess;
}

static
CacheModelResult cachesim_D1_Write(Addr a, UChar size)
{
    if ( cachesim_ref( &D1, a, size) == Hit ) {
	cachesim_ref_wb( &LL, Write, a, size);
	return L1_Hit;
    }
    switch( cachesim_ref_wb( &LL, Write, a, size) ) {
	case Hit: return LL_Hit;
	case Miss: return MemAccess;
	default: break;
    }
    return WriteBackMemAccess;
}



static ULong prefetch_up = 0;
static ULong prefetch_down = 0;

#define PF_STREAMS  8
#define PF_PAGEBITS 12

static UInt pf_lastblock[PF_STREAMS];
static Int  pf_seqblocks[PF_STREAMS];

static
void prefetch_clear(void)
{
  int i;
  for(i=0;i<PF_STREAMS;i++)
    pf_lastblock[i] = pf_seqblocks[i] = 0;
}

static __inline__
void prefetch_LL_doref(Addr a)
{
  UInt stream = (a >> PF_PAGEBITS) % PF_STREAMS;
  UInt block = ( a >> LL.line_size_bits);

  if (block != pf_lastblock[stream]) {
    if (pf_seqblocks[stream] == 0) {
      if (pf_lastblock[stream] +1 == block) pf_seqblocks[stream]++;
      else if (pf_lastblock[stream] -1 == block) pf_seqblocks[stream]--;
    }
    else if (pf_seqblocks[stream] >0) {
      if (pf_lastblock[stream] +1 == block) {
	pf_seqblocks[stream]++;
	if (pf_seqblocks[stream] >= 2) {
	  prefetch_up++;
	  cachesim_ref(&LL, a + 5 * LL.line_size,1);
	}
      }
      else pf_seqblocks[stream] = 0;
    }
    else if (pf_seqblocks[stream] <0) {
      if (pf_lastblock[stream] -1 == block) {
	pf_seqblocks[stream]--;
	if (pf_seqblocks[stream] <= -2) {
	  prefetch_down++;
	  cachesim_ref(&LL, a - 5 * LL.line_size,1);
	}
      }
      else pf_seqblocks[stream] = 0;
    }
    pf_lastblock[stream] = block;
  }
}  


static
CacheModelResult prefetch_I1_ref(Addr a, UChar size)
{
    if ( cachesim_ref( &I1, a, size) == Hit ) return L1_Hit;
    prefetch_LL_doref(a);
    if ( cachesim_ref( &LL, a, size) == Hit ) return LL_Hit;
    return MemAccess;
}

static
CacheModelResult prefetch_D1_ref(Addr a, UChar size)
{
    if ( cachesim_ref( &D1, a, size) == Hit ) return L1_Hit;
    prefetch_LL_doref(a);
    if ( cachesim_ref( &LL, a, size) == Hit ) return LL_Hit;
    return MemAccess;
}



static
CacheModelResult prefetch_I1_Read(Addr a, UChar size)
{
    if ( cachesim_ref( &I1, a, size) == Hit ) return L1_Hit;
    prefetch_LL_doref(a);
    switch( cachesim_ref_wb( &LL, Read, a, size) ) {
	case Hit: return LL_Hit;
	case Miss: return MemAccess;
	default: break;
    }
    return WriteBackMemAccess;
}

static
CacheModelResult prefetch_D1_Read(Addr a, UChar size)
{
    if ( cachesim_ref( &D1, a, size) == Hit ) return L1_Hit;
    prefetch_LL_doref(a);
    switch( cachesim_ref_wb( &LL, Read, a, size) ) {
	case Hit: return LL_Hit;
	case Miss: return MemAccess;
	default: break;
    }
    return WriteBackMemAccess;
}

static
CacheModelResult prefetch_D1_Write(Addr a, UChar size)
{
    prefetch_LL_doref(a);
    if ( cachesim_ref( &D1, a, size) == Hit ) {
	cachesim_ref_wb( &LL, Write, a, size);
	return L1_Hit;
    }
    switch( cachesim_ref_wb( &LL, Write, a, size) ) {
	case Hit: return LL_Hit;
	case Miss: return MemAccess;
	default: break;
    }
    return WriteBackMemAccess;
}




static
void cacheuse_initcache(cache_t2* c)
{
    int i;
    unsigned int start_mask, start_val;
    unsigned int end_mask, end_val;

    c->use    = CLG_MALLOC("cl.sim.cu_ic.1",
                           sizeof(line_use) * c->sets * c->assoc);
    c->loaded = CLG_MALLOC("cl.sim.cu_ic.2",
                           sizeof(line_loaded) * c->sets * c->assoc);
    c->line_start_mask = CLG_MALLOC("cl.sim.cu_ic.3",
                                    sizeof(int) * c->line_size);
    c->line_end_mask = CLG_MALLOC("cl.sim.cu_ic.4",
                                  sizeof(int) * c->line_size);
    
    c->line_size_mask = c->line_size-1;

    start_val = end_val = ~0;
    if (c->line_size < 32) {
	int bits_per_byte = 32/c->line_size;
	start_mask = (1<<bits_per_byte)-1;
	end_mask   = start_mask << (32-bits_per_byte);
	for(i=0;i<c->line_size;i++) {
	    c->line_start_mask[i] = start_val;
	    start_val  = start_val & ~start_mask;
	    start_mask = start_mask << bits_per_byte;
	    
	    c->line_end_mask[c->line_size-i-1] = end_val;
	    end_val  = end_val & ~end_mask;
	    end_mask = end_mask >> bits_per_byte;
	}
    }
    else {
	int bytes_per_bit = c->line_size/32;
	start_mask = 1;
	end_mask   = 1u << 31;
	for(i=0;i<c->line_size;i++) {
	    c->line_start_mask[i] = start_val;
	    c->line_end_mask[c->line_size-i-1] = end_val;
	    if ( ((i+1)%bytes_per_bit) == 0) {
		start_val   &= ~start_mask;
		end_val     &= ~end_mask;
		start_mask <<= 1;
		end_mask   >>= 1;
	    }
	}
    }
    
    CLG_DEBUG(6, "Config %s:\n", c->desc_line);
    for(i=0;i<c->line_size;i++) {
	CLG_DEBUG(6, " [%2d]: start mask %8x, end mask %8x\n",
		  i, c->line_start_mask[i], c->line_end_mask[i]);
    }
    
    if ( (1<<c->tag_shift) < c->assoc) {
	VG_(message)(Vg_DebugMsg,
		     "error: Use associativity < %d for cache use statistics!\n",
		     (1<<c->tag_shift) );
	VG_(tool_panic)("Unsupported cache configuration");
    }
}
    

#define CACHEUSE(L)                                                         \
                                                                            \
static CacheModelResult cacheuse##_##L##_doRead(Addr a, UChar size)         \
{                                                                           \
   UInt set1 = ( a         >> L.line_size_bits) & (L.sets_min_1);           \
   UInt set2 = ((a+size-1) >> L.line_size_bits) & (L.sets_min_1);           \
   UWord tag  = a & L.tag_mask;                                             \
   UWord tag2;                                                              \
   int i, j, idx;                                                           \
   UWord *set, tmp_tag; 						    \
   UInt use_mask;							    \
                                                                            \
   CLG_DEBUG(6,"%s.Acc(Addr %#lx, size %d): Sets [%d/%d]\n",                  \
	    L.name, a, size, set1, set2);				    \
                                                                            \
                                \
   if (set1 == set2) {                                                      \
                                                                            \
      set = &(L.tags[set1 * L.assoc]);                                      \
      use_mask = L.line_start_mask[a & L.line_size_mask] &                  \
	         L.line_end_mask[(a+size-1) & L.line_size_mask];	    \
                                                                            \
      \
      \
      \
      if (tag == (set[0] & L.tag_mask)) {                                   \
        idx = (set1 * L.assoc) + (set[0] & ~L.tag_mask);                    \
        L.use[idx].count ++;                                                \
        L.use[idx].mask |= use_mask;                                        \
	CLG_DEBUG(6," Hit0 [idx %d] (line %#lx from %#lx): %x => %08x, count %d\n",\
		 idx, L.loaded[idx].memline,  L.loaded[idx].iaddr,          \
		 use_mask, L.use[idx].mask, L.use[idx].count);              \
	return L1_Hit;							    \
      }                                                                     \
      \
      \
      for (i = 1; i < L.assoc; i++) {                                       \
	 if (tag == (set[i] & L.tag_mask)) {			            \
  	    tmp_tag = set[i];                                               \
            for (j = i; j > 0; j--) {                                       \
               set[j] = set[j - 1];                                         \
            }                                                               \
            set[0] = tmp_tag;			                            \
            idx = (set1 * L.assoc) + (tmp_tag & ~L.tag_mask);               \
            L.use[idx].count ++;                                            \
            L.use[idx].mask |= use_mask;                                    \
	CLG_DEBUG(6," Hit%d [idx %d] (line %#lx from %#lx): %x => %08x, count %d\n",\
		 i, idx, L.loaded[idx].memline,  L.loaded[idx].iaddr,       \
		 use_mask, L.use[idx].mask, L.use[idx].count);              \
            return L1_Hit;                                                  \
         }                                                                  \
      }                                                                     \
                                                                            \
                  \
      tmp_tag = set[L.assoc - 1] & ~L.tag_mask;                             \
      for (j = L.assoc - 1; j > 0; j--) {                                   \
         set[j] = set[j - 1];                                               \
      }                                                                     \
      set[0] = tag | tmp_tag;                                               \
      idx = (set1 * L.assoc) + tmp_tag;                                     \
      return update_##L##_use(&L, idx,         			            \
		       use_mask, a &~ L.line_size_mask);		    \
                                                                            \
                                \
                   \
   } else if (((set1 + 1) & (L.sets_min_1)) == set2) {                      \
      Int miss1=0, miss2=0;            \
      set = &(L.tags[set1 * L.assoc]);                                      \
      use_mask = L.line_start_mask[a & L.line_size_mask];		    \
      if (tag == (set[0] & L.tag_mask)) {                                   \
         idx = (set1 * L.assoc) + (set[0] & ~L.tag_mask);                   \
         L.use[idx].count ++;                                               \
         L.use[idx].mask |= use_mask;                                       \
	CLG_DEBUG(6," Hit0 [idx %d] (line %#lx from %#lx): %x => %08x, count %d\n",\
		 idx, L.loaded[idx].memline,  L.loaded[idx].iaddr,          \
		 use_mask, L.use[idx].mask, L.use[idx].count);              \
         goto block2;                                                       \
      }                                                                     \
      for (i = 1; i < L.assoc; i++) {                                       \
	 if (tag == (set[i] & L.tag_mask)) {			            \
  	    tmp_tag = set[i];                                               \
            for (j = i; j > 0; j--) {                                       \
               set[j] = set[j - 1];                                         \
            }                                                               \
            set[0] = tmp_tag;                                               \
            idx = (set1 * L.assoc) + (tmp_tag & ~L.tag_mask);               \
            L.use[idx].count ++;                                            \
            L.use[idx].mask |= use_mask;                                    \
	CLG_DEBUG(6," Hit%d [idx %d] (line %#lx from %#lx): %x => %08x, count %d\n",\
		 i, idx, L.loaded[idx].memline,  L.loaded[idx].iaddr,       \
		 use_mask, L.use[idx].mask, L.use[idx].count);              \
            goto block2;                                                    \
         }                                                                  \
      }                                                                     \
      tmp_tag = set[L.assoc - 1] & ~L.tag_mask;                             \
      for (j = L.assoc - 1; j > 0; j--) {                                   \
         set[j] = set[j - 1];                                               \
      }                                                                     \
      set[0] = tag | tmp_tag;                                               \
      idx = (set1 * L.assoc) + tmp_tag;                                     \
      miss1 = update_##L##_use(&L, idx,        			            \
		       use_mask, a &~ L.line_size_mask);		    \
block2:                                                                     \
      set = &(L.tags[set2 * L.assoc]);                                      \
      use_mask = L.line_end_mask[(a+size-1) & L.line_size_mask];  	    \
      tag2  = (a+size-1) & L.tag_mask;                                      \
      if (tag2 == (set[0] & L.tag_mask)) {                                  \
         idx = (set2 * L.assoc) + (set[0] & ~L.tag_mask);                   \
         L.use[idx].count ++;                                               \
         L.use[idx].mask |= use_mask;                                       \
	CLG_DEBUG(6," Hit0 [idx %d] (line %#lx from %#lx): %x => %08x, count %d\n",\
		 idx, L.loaded[idx].memline,  L.loaded[idx].iaddr,          \
		 use_mask, L.use[idx].mask, L.use[idx].count);              \
         return miss1;                                                      \
      }                                                                     \
      for (i = 1; i < L.assoc; i++) {                                       \
	 if (tag2 == (set[i] & L.tag_mask)) {			            \
  	    tmp_tag = set[i];                                               \
            for (j = i; j > 0; j--) {                                       \
               set[j] = set[j - 1];                                         \
            }                                                               \
            set[0] = tmp_tag;                                               \
            idx = (set2 * L.assoc) + (tmp_tag & ~L.tag_mask);               \
            L.use[idx].count ++;                                            \
            L.use[idx].mask |= use_mask;                                    \
	CLG_DEBUG(6," Hit%d [idx %d] (line %#lx from %#lx): %x => %08x, count %d\n",\
		 i, idx, L.loaded[idx].memline,  L.loaded[idx].iaddr,       \
		 use_mask, L.use[idx].mask, L.use[idx].count);              \
            return miss1;                                                   \
         }                                                                  \
      }                                                                     \
      tmp_tag = set[L.assoc - 1] & ~L.tag_mask;                             \
      for (j = L.assoc - 1; j > 0; j--) {                                   \
         set[j] = set[j - 1];                                               \
      }                                                                     \
      set[0] = tag2 | tmp_tag;                                              \
      idx = (set2 * L.assoc) + tmp_tag;                                     \
      miss2 = update_##L##_use(&L, idx,			                    \
		       use_mask, (a+size-1) &~ L.line_size_mask);	    \
      return (miss1==MemAccess || miss2==MemAccess) ? MemAccess:LL_Hit;     \
                                                                            \
   } else {                                                                 \
       VG_(printf)("addr: %#lx  size: %u  sets: %d %d", a, size, set1, set2); \
       VG_(tool_panic)("item straddles more than two cache sets");          \
   }                                                                        \
   return 0;                                                                \
}


static __inline__ unsigned int countBits(unsigned int bits)
{
  unsigned int c; 
  const int S[] = {1, 2, 4, 8, 16}; 
  const int B[] = {0x55555555, 0x33333333, 0x0F0F0F0F, 0x00FF00FF, 0x0000FFFF};

  c = bits;
  c = ((c >> S[0]) & B[0]) + (c & B[0]);
  c = ((c >> S[1]) & B[1]) + (c & B[1]);
  c = ((c >> S[2]) & B[2]) + (c & B[2]);
  c = ((c >> S[3]) & B[3]) + (c & B[3]);
  c = ((c >> S[4]) & B[4]) + (c & B[4]);
  return c;
}

static void update_LL_use(int idx, Addr memline)
{
  line_loaded* loaded = &(LL.loaded[idx]);
  line_use* use = &(LL.use[idx]);
  int i = ((32 - countBits(use->mask)) * LL.line_size)>>5;
  
  CLG_DEBUG(2, " LL.miss [%d]: at %#lx accessing memline %#lx\n",
           idx, CLG_(bb_base) + current_ii->instr_offset, memline);
  if (use->count>0) {
    CLG_DEBUG(2, "   old: used %d, loss bits %d (%08x) [line %#lx from %#lx]\n",
	     use->count, i, use->mask, loaded->memline, loaded->iaddr);
    CLG_DEBUG(2, "   collect: %d, use_base %p\n",
	     CLG_(current_state).collect, loaded->use_base);
    
    if (CLG_(current_state).collect && loaded->use_base) {
      (loaded->use_base)[off_LL_AcCost] += 1000 / use->count;
      (loaded->use_base)[off_LL_SpLoss] += i;
    }
   }

   use->count = 0;
   use->mask  = 0;

  loaded->memline = memline;
  loaded->iaddr   = CLG_(bb_base) + current_ii->instr_offset;
  loaded->use_base = (CLG_(current_state).nonskipped) ?
    CLG_(current_state).nonskipped->skipped :
    CLG_(cost_base) + current_ii->cost_offset;
}

static
CacheModelResult cacheuse_LL_access(Addr memline, line_loaded* l1_loaded)
{
   UInt setNo = (memline >> LL.line_size_bits) & (LL.sets_min_1);
   UWord* set = &(LL.tags[setNo * LL.assoc]);
   UWord tag  = memline & LL.tag_mask;

   int i, j, idx;
   UWord tmp_tag;
   
   CLG_DEBUG(6,"LL.Acc(Memline %#lx): Set %d\n", memline, setNo);

   if (tag == (set[0] & LL.tag_mask)) {
     idx = (setNo * LL.assoc) + (set[0] & ~LL.tag_mask);
     l1_loaded->dep_use = &(LL.use[idx]);

     CLG_DEBUG(6," Hit0 [idx %d] (line %#lx from %#lx): => %08x, count %d\n",
		 idx, LL.loaded[idx].memline,  LL.loaded[idx].iaddr,
		 LL.use[idx].mask, LL.use[idx].count);
     return LL_Hit;
   }
   for (i = 1; i < LL.assoc; i++) {
     if (tag == (set[i] & LL.tag_mask)) {
       tmp_tag = set[i];
       for (j = i; j > 0; j--) {
	 set[j] = set[j - 1];
       }
       set[0] = tmp_tag;
       idx = (setNo * LL.assoc) + (tmp_tag & ~LL.tag_mask);
       l1_loaded->dep_use = &(LL.use[idx]);

	CLG_DEBUG(6," Hit%d [idx %d] (line %#lx from %#lx): => %08x, count %d\n",
		 i, idx, LL.loaded[idx].memline,  LL.loaded[idx].iaddr,
		 LL.use[idx].mask, LL.use[idx].count);
	return LL_Hit;
     }
   }

   
   tmp_tag = set[LL.assoc - 1] & ~LL.tag_mask;
   for (j = LL.assoc - 1; j > 0; j--) {
     set[j] = set[j - 1];
   }
   set[0] = tag | tmp_tag;
   idx = (setNo * LL.assoc) + tmp_tag;
   l1_loaded->dep_use = &(LL.use[idx]);

   update_LL_use(idx, memline);

   return MemAccess;
}




#define UPDATE_USE(L)					             \
                                                                     \
static CacheModelResult update##_##L##_use(cache_t2* cache, int idx, \
			       UInt mask, Addr memline)		     \
{                                                                    \
  line_loaded* loaded = &(cache->loaded[idx]);			     \
  line_use* use = &(cache->use[idx]);				     \
  int c = ((32 - countBits(use->mask)) * cache->line_size)>>5;       \
                                                                     \
  CLG_DEBUG(2, " %s.miss [%d]: at %#lx accessing memline %#lx (mask %08x)\n", \
           cache->name, idx, CLG_(bb_base) + current_ii->instr_offset, memline, mask); \
  if (use->count>0) {                                                \
    CLG_DEBUG(2, "   old: used %d, loss bits %d (%08x) [line %#lx from %#lx]\n",\
	     use->count, c, use->mask, loaded->memline, loaded->iaddr);	\
    CLG_DEBUG(2, "   collect: %d, use_base %p\n", \
	     CLG_(current_state).collect, loaded->use_base);	     \
                                                                     \
    if (CLG_(current_state).collect && loaded->use_base) {           \
      (loaded->use_base)[off_##L##_AcCost] += 1000 / use->count;     \
      (loaded->use_base)[off_##L##_SpLoss] += c;                     \
                                                                     \
                    \
      loaded->dep_use->mask |= use->mask;                            \
      loaded->dep_use->count += use->count;                          \
    }                                                                \
  }                                                                  \
                                                                     \
  use->count = 1;                                                    \
  use->mask  = mask;                                                 \
  loaded->memline = memline;                                         \
  loaded->iaddr   = CLG_(bb_base) + current_ii->instr_offset;        \
  loaded->use_base = (CLG_(current_state).nonskipped) ?              \
    CLG_(current_state).nonskipped->skipped :                        \
    CLG_(cost_base) + current_ii->cost_offset;                       \
                                                                     \
  if (memline == 0) return LL_Hit;                                   \
  return cacheuse_LL_access(memline, loaded);                        \
}

UPDATE_USE(I1);
UPDATE_USE(D1);

CACHEUSE(I1);
CACHEUSE(D1);


static
void cacheuse_finish(void)
{
  int i;
  InstrInfo ii = { 0,0,0,0 };

  if (!CLG_(current_state).collect) return;

  CLG_(bb_base) = 0;
  current_ii = &ii; 
  CLG_(cost_base) = 0;

  
  if (I1.use)
    for (i = 0; i < I1.sets * I1.assoc; i++)
      if (I1.loaded[i].use_base)
	update_I1_use( &I1, i, 0,0);

  if (D1.use)
    for (i = 0; i < D1.sets * D1.assoc; i++)
      if (D1.loaded[i].use_base)
	update_D1_use( &D1, i, 0,0);

  if (LL.use)
    for (i = 0; i < LL.sets * LL.assoc; i++)
      if (LL.loaded[i].use_base)
	update_LL_use(i, 0);

  current_ii = 0;
}
  




static __inline__
void inc_costs(CacheModelResult r, ULong* c1, ULong* c2)
{
    switch(r) {
	case WriteBackMemAccess:
	    if (clo_simulate_writeback) {
		c1[3]++;
		c2[3]++;
	    }
	    

	case MemAccess:
	    c1[2]++;
	    c2[2]++;
	    

	case LL_Hit:
	    c1[1]++;
	    c2[1]++;
	    

	default:
	    c1[0]++;
	    c2[0]++;
    }
}

static
const HChar* cacheRes(CacheModelResult r)
{
    switch(r) {
    case L1_Hit:    return "L1 Hit ";
    case LL_Hit:    return "LL Hit ";
    case MemAccess: return "LL Miss";
    case WriteBackMemAccess: return "LL Miss (dirty)";
    default:
	tl_assert(0);
    }
    return "??";
}

VG_REGPARM(1)
static void log_1I0D(InstrInfo* ii)
{
    CacheModelResult IrRes;

    current_ii = ii;
    IrRes = (*simulator.I1_Read)(CLG_(bb_base) + ii->instr_offset, ii->instr_size);

    CLG_DEBUG(6, "log_1I0D:  Ir  %#lx/%u => %s\n",
              CLG_(bb_base) + ii->instr_offset, ii->instr_size, cacheRes(IrRes));

    if (CLG_(current_state).collect) {
	ULong* cost_Ir;

	if (CLG_(current_state).nonskipped)
	    cost_Ir = CLG_(current_state).nonskipped->skipped + fullOffset(EG_IR);
	else
            cost_Ir = CLG_(cost_base) + ii->cost_offset + ii->eventset->offset[EG_IR];

	inc_costs(IrRes, cost_Ir, 
		  CLG_(current_state).cost + fullOffset(EG_IR) );
    }
}

VG_REGPARM(2)
static void log_2I0D(InstrInfo* ii1, InstrInfo* ii2)
{
    CacheModelResult Ir1Res, Ir2Res;
    ULong *global_cost_Ir;

    current_ii = ii1;
    Ir1Res = (*simulator.I1_Read)(CLG_(bb_base) + ii1->instr_offset, ii1->instr_size);
    current_ii = ii2;
    Ir2Res = (*simulator.I1_Read)(CLG_(bb_base) + ii2->instr_offset, ii2->instr_size);

    CLG_DEBUG(6, "log_2I0D:  Ir1 %#lx/%u => %s, Ir2 %#lx/%u => %s\n",
              CLG_(bb_base) + ii1->instr_offset, ii1->instr_size, cacheRes(Ir1Res),
              CLG_(bb_base) + ii2->instr_offset, ii2->instr_size, cacheRes(Ir2Res) );

    if (!CLG_(current_state).collect) return;

    global_cost_Ir = CLG_(current_state).cost + fullOffset(EG_IR);
    if (CLG_(current_state).nonskipped) {
	ULong* skipped_cost_Ir =
	    CLG_(current_state).nonskipped->skipped + fullOffset(EG_IR);

	inc_costs(Ir1Res, global_cost_Ir, skipped_cost_Ir);
	inc_costs(Ir2Res, global_cost_Ir, skipped_cost_Ir);
	return;
    }

    inc_costs(Ir1Res, global_cost_Ir,
              CLG_(cost_base) + ii1->cost_offset + ii1->eventset->offset[EG_IR]);
    inc_costs(Ir2Res, global_cost_Ir,
              CLG_(cost_base) + ii2->cost_offset + ii2->eventset->offset[EG_IR]);
}

VG_REGPARM(3)
static void log_3I0D(InstrInfo* ii1, InstrInfo* ii2, InstrInfo* ii3)
{
    CacheModelResult Ir1Res, Ir2Res, Ir3Res;
    ULong *global_cost_Ir;

    current_ii = ii1;
    Ir1Res = (*simulator.I1_Read)(CLG_(bb_base) + ii1->instr_offset, ii1->instr_size);
    current_ii = ii2;
    Ir2Res = (*simulator.I1_Read)(CLG_(bb_base) + ii2->instr_offset, ii2->instr_size);
    current_ii = ii3;
    Ir3Res = (*simulator.I1_Read)(CLG_(bb_base) + ii3->instr_offset, ii3->instr_size);

    CLG_DEBUG(6, "log_3I0D:  Ir1 %#lx/%u => %s, Ir2 %#lx/%u => %s, Ir3 %#lx/%u => %s\n",
              CLG_(bb_base) + ii1->instr_offset, ii1->instr_size, cacheRes(Ir1Res),
              CLG_(bb_base) + ii2->instr_offset, ii2->instr_size, cacheRes(Ir2Res),
              CLG_(bb_base) + ii3->instr_offset, ii3->instr_size, cacheRes(Ir3Res) );

    if (!CLG_(current_state).collect) return;

    global_cost_Ir = CLG_(current_state).cost + fullOffset(EG_IR);
    if (CLG_(current_state).nonskipped) {
	ULong* skipped_cost_Ir =
	    CLG_(current_state).nonskipped->skipped + fullOffset(EG_IR);
	inc_costs(Ir1Res, global_cost_Ir, skipped_cost_Ir);
	inc_costs(Ir2Res, global_cost_Ir, skipped_cost_Ir);
	inc_costs(Ir3Res, global_cost_Ir, skipped_cost_Ir);
	return;
    }

    inc_costs(Ir1Res, global_cost_Ir,
              CLG_(cost_base) + ii1->cost_offset + ii1->eventset->offset[EG_IR]);
    inc_costs(Ir2Res, global_cost_Ir,
              CLG_(cost_base) + ii2->cost_offset + ii2->eventset->offset[EG_IR]);
    inc_costs(Ir3Res, global_cost_Ir,
              CLG_(cost_base) + ii3->cost_offset + ii3->eventset->offset[EG_IR]);
}


VG_REGPARM(3)
static void log_1I1Dr(InstrInfo* ii, Addr data_addr, Word data_size)
{
    CacheModelResult IrRes, DrRes;

    current_ii = ii;
    IrRes = (*simulator.I1_Read)(CLG_(bb_base) + ii->instr_offset, ii->instr_size);
    DrRes = (*simulator.D1_Read)(data_addr, data_size);

    CLG_DEBUG(6, "log_1I1Dr: Ir  %#lx/%u => %s, Dr  %#lx/%lu => %s\n",
              CLG_(bb_base) + ii->instr_offset, ii->instr_size, cacheRes(IrRes),
	      data_addr, data_size, cacheRes(DrRes));

    if (CLG_(current_state).collect) {
	ULong *cost_Ir, *cost_Dr;
	
	if (CLG_(current_state).nonskipped) {
	    cost_Ir = CLG_(current_state).nonskipped->skipped + fullOffset(EG_IR);
	    cost_Dr = CLG_(current_state).nonskipped->skipped + fullOffset(EG_DR);
	}
	else {
            cost_Ir = CLG_(cost_base) + ii->cost_offset + ii->eventset->offset[EG_IR];
            cost_Dr = CLG_(cost_base) + ii->cost_offset + ii->eventset->offset[EG_DR];
	}
       
	inc_costs(IrRes, cost_Ir, 
		  CLG_(current_state).cost + fullOffset(EG_IR) );
	inc_costs(DrRes, cost_Dr,
		  CLG_(current_state).cost + fullOffset(EG_DR) );
    }
}


VG_REGPARM(3)
static void log_0I1Dr(InstrInfo* ii, Addr data_addr, Word data_size)
{
    CacheModelResult DrRes;

    current_ii = ii;
    DrRes = (*simulator.D1_Read)(data_addr, data_size);

    CLG_DEBUG(6, "log_0I1Dr: Dr  %#lx/%lu => %s\n",
	      data_addr, data_size, cacheRes(DrRes));

    if (CLG_(current_state).collect) {
	ULong *cost_Dr;
	
	if (CLG_(current_state).nonskipped)
	    cost_Dr = CLG_(current_state).nonskipped->skipped + fullOffset(EG_DR);
	else
            cost_Dr = CLG_(cost_base) + ii->cost_offset + ii->eventset->offset[EG_DR];

	inc_costs(DrRes, cost_Dr,
		  CLG_(current_state).cost + fullOffset(EG_DR) );
    }
}



VG_REGPARM(3)
static void log_1I1Dw(InstrInfo* ii, Addr data_addr, Word data_size)
{
    CacheModelResult IrRes, DwRes;

    current_ii = ii;
    IrRes = (*simulator.I1_Read)(CLG_(bb_base) + ii->instr_offset, ii->instr_size);
    DwRes = (*simulator.D1_Write)(data_addr, data_size);

    CLG_DEBUG(6, "log_1I1Dw: Ir  %#lx/%u => %s, Dw  %#lx/%lu => %s\n",
              CLG_(bb_base) + ii->instr_offset, ii->instr_size, cacheRes(IrRes),
	      data_addr, data_size, cacheRes(DwRes));

    if (CLG_(current_state).collect) {
	ULong *cost_Ir, *cost_Dw;
	
	if (CLG_(current_state).nonskipped) {
	    cost_Ir = CLG_(current_state).nonskipped->skipped + fullOffset(EG_IR);
	    cost_Dw = CLG_(current_state).nonskipped->skipped + fullOffset(EG_DW);
	}
	else {
            cost_Ir = CLG_(cost_base) + ii->cost_offset + ii->eventset->offset[EG_IR];
            cost_Dw = CLG_(cost_base) + ii->cost_offset + ii->eventset->offset[EG_DW];
	}
       
	inc_costs(IrRes, cost_Ir,
		  CLG_(current_state).cost + fullOffset(EG_IR) );
	inc_costs(DwRes, cost_Dw,
		  CLG_(current_state).cost + fullOffset(EG_DW) );
    }
}

VG_REGPARM(3)
static void log_0I1Dw(InstrInfo* ii, Addr data_addr, Word data_size)
{
    CacheModelResult DwRes;

    current_ii = ii;
    DwRes = (*simulator.D1_Write)(data_addr, data_size);

    CLG_DEBUG(6, "log_0I1Dw: Dw  %#lx/%lu => %s\n",
	      data_addr, data_size, cacheRes(DwRes));

    if (CLG_(current_state).collect) {
	ULong *cost_Dw;
	
	if (CLG_(current_state).nonskipped)
	    cost_Dw = CLG_(current_state).nonskipped->skipped + fullOffset(EG_DW);
	else
            cost_Dw = CLG_(cost_base) + ii->cost_offset + ii->eventset->offset[EG_DW];
       
	inc_costs(DwRes, cost_Dw,
		  CLG_(current_state).cost + fullOffset(EG_DW) );
    }
}




static cache_t clo_I1_cache = UNDEFINED_CACHE;
static cache_t clo_D1_cache = UNDEFINED_CACHE;
static cache_t clo_LL_cache = UNDEFINED_CACHE;

static void cachesim_post_clo_init(void)
{
  
  cache_t  I1c, D1c, LLc;

  
  if (!CLG_(clo).simulate_cache) {
    CLG_(cachesim).log_1I0D  = 0;
    CLG_(cachesim).log_1I0D_name = "(no function)";
    CLG_(cachesim).log_2I0D  = 0;
    CLG_(cachesim).log_2I0D_name = "(no function)";
    CLG_(cachesim).log_3I0D  = 0;
    CLG_(cachesim).log_3I0D_name = "(no function)";

    CLG_(cachesim).log_1I1Dr = 0;
    CLG_(cachesim).log_1I1Dr_name = "(no function)";
    CLG_(cachesim).log_1I1Dw = 0;
    CLG_(cachesim).log_1I1Dw_name = "(no function)";

    CLG_(cachesim).log_0I1Dr = 0;
    CLG_(cachesim).log_0I1Dr_name = "(no function)";
    CLG_(cachesim).log_0I1Dw = 0;
    CLG_(cachesim).log_0I1Dw_name = "(no function)";
    return;
  }

  
  VG_(post_clo_init_configure_caches)(&I1c, &D1c, &LLc,
                                      &clo_I1_cache,
                                      &clo_D1_cache,
                                      &clo_LL_cache);

  I1.name = "I1";
  D1.name = "D1";
  LL.name = "LL";

  
  
  
  CLG_(min_line_size) = (I1c.line_size < D1c.line_size)
                           ? I1c.line_size : D1c.line_size;
  CLG_(min_line_size) = (LLc.line_size < CLG_(min_line_size))
                           ? LLc.line_size : CLG_(min_line_size);

  Int largest_load_or_store_size
     = VG_(machine_get_size_of_largest_guest_register)();
  if (CLG_(min_line_size) < largest_load_or_store_size) {
     VG_(umsg)("Callgrind: cannot continue: the minimum line size (%d)\n",
               (Int)CLG_(min_line_size));
     VG_(umsg)("  must be equal to or larger than the maximum register size (%d)\n",
               largest_load_or_store_size );
     VG_(umsg)("  but it is not.  Exiting now.\n");
     VG_(exit)(1);
  }

  cachesim_initcache(I1c, &I1);
  cachesim_initcache(D1c, &D1);
  cachesim_initcache(LLc, &LL);


  CLG_(cachesim).log_1I0D  = log_1I0D;
  CLG_(cachesim).log_1I0D_name  = "log_1I0D";
  CLG_(cachesim).log_2I0D  = log_2I0D;
  CLG_(cachesim).log_2I0D_name  = "log_2I0D";
  CLG_(cachesim).log_3I0D  = log_3I0D;
  CLG_(cachesim).log_3I0D_name  = "log_3I0D";

  CLG_(cachesim).log_1I1Dr = log_1I1Dr;
  CLG_(cachesim).log_1I1Dw = log_1I1Dw;
  CLG_(cachesim).log_1I1Dr_name = "log_1I1Dr";
  CLG_(cachesim).log_1I1Dw_name = "log_1I1Dw";

  CLG_(cachesim).log_0I1Dr = log_0I1Dr;
  CLG_(cachesim).log_0I1Dw = log_0I1Dw;
  CLG_(cachesim).log_0I1Dr_name = "log_0I1Dr";
  CLG_(cachesim).log_0I1Dw_name = "log_0I1Dw";

  if (clo_collect_cacheuse) {

      
      if (clo_simulate_hwpref) {
	  VG_(message)(Vg_DebugMsg,
		       "warning: prefetch simulation can not be "
                       "used with cache usage\n");
	  clo_simulate_hwpref = False;
      }

      if (clo_simulate_writeback) {
	  VG_(message)(Vg_DebugMsg,
		       "warning: write-back simulation can not be "
                       "used with cache usage\n");
	  clo_simulate_writeback = False;
      }

      simulator.I1_Read  = cacheuse_I1_doRead;
      simulator.D1_Read  = cacheuse_D1_doRead;
      simulator.D1_Write = cacheuse_D1_doRead;
      return;
  }

  if (clo_simulate_hwpref) {
    prefetch_clear();

    if (clo_simulate_writeback) {
      simulator.I1_Read  = prefetch_I1_Read;
      simulator.D1_Read  = prefetch_D1_Read;
      simulator.D1_Write = prefetch_D1_Write;
    }
    else {
      simulator.I1_Read  = prefetch_I1_ref;
      simulator.D1_Read  = prefetch_D1_ref;
      simulator.D1_Write = prefetch_D1_ref;
    }

    return;
  }

  if (clo_simulate_writeback) {
      simulator.I1_Read  = cachesim_I1_Read;
      simulator.D1_Read  = cachesim_D1_Read;
      simulator.D1_Write = cachesim_D1_Write;
  }
  else {
      simulator.I1_Read  = cachesim_I1_ref;
      simulator.D1_Read  = cachesim_D1_ref;
      simulator.D1_Write = cachesim_D1_ref;
  }
}


static
void cachesim_clear(void)
{
  cachesim_clearcache(&I1);
  cachesim_clearcache(&D1);
  cachesim_clearcache(&LL);

  prefetch_clear();
}


static void cachesim_dump_desc(VgFile *fp)
{
  VG_(fprintf)(fp, "\ndesc: I1 cache: %s\n", I1.desc_line);
  VG_(fprintf)(fp, "desc: D1 cache: %s\n", D1.desc_line);
  VG_(fprintf)(fp, "desc: LL cache: %s\n", LL.desc_line);
}

static
void cachesim_print_opts(void)
{
  VG_(printf)(
"\n   cache simulator options (does cache simulation if used):\n"
"    --simulate-wb=no|yes      Count write-back events [no]\n"
"    --simulate-hwpref=no|yes  Simulate hardware prefetch [no]\n"
#if CLG_EXPERIMENTAL
"    --simulate-sectors=no|yes Simulate sectored behaviour [no]\n"
#endif
"    --cacheuse=no|yes         Collect cache block use [no]\n");
  VG_(print_cache_clo_opts)();
}

static Bool cachesim_parse_opt(const HChar* arg)
{
   if      VG_BOOL_CLO(arg, "--simulate-wb",      clo_simulate_writeback) {}
   else if VG_BOOL_CLO(arg, "--simulate-hwpref",  clo_simulate_hwpref)    {}
   else if VG_BOOL_CLO(arg, "--simulate-sectors", clo_simulate_sectors)   {}

   else if VG_BOOL_CLO(arg, "--cacheuse", clo_collect_cacheuse) {
      if (clo_collect_cacheuse) {
         
         CLG_(clo).dump_instr = True;
      }
   }

   else if (VG_(str_clo_cache_opt)(arg,
                                   &clo_I1_cache,
                                   &clo_D1_cache,
                                   &clo_LL_cache)) {}

   else
     return False;

  return True;
}

static
void cachesim_printstat(Int l1, Int l2, Int l3)
{
  FullCost total = CLG_(total_cost), D_total = 0;
  ULong LL_total_m, LL_total_mr, LL_total_mw,
    LL_total, LL_total_r, LL_total_w;

  if ((VG_(clo_verbosity) >1) && clo_simulate_hwpref) {
    VG_(message)(Vg_DebugMsg, "Prefetch Up:       %llu\n", 
		 prefetch_up);
    VG_(message)(Vg_DebugMsg, "Prefetch Down:     %llu\n", 
		 prefetch_down);
    VG_(message)(Vg_DebugMsg, "\n");
  }

  VG_(message)(Vg_UserMsg, "I1  misses:    %'*llu\n", l1,
               total[fullOffset(EG_IR) +1]);

  VG_(message)(Vg_UserMsg, "LLi misses:    %'*llu\n", l1,
               total[fullOffset(EG_IR) +2]);

  if (0 == total[fullOffset(EG_IR)])
    total[fullOffset(EG_IR)] = 1;

  VG_(message)(Vg_UserMsg, "I1  miss rate: %*.2f%%\n", l1,
               total[fullOffset(EG_IR)+1] * 100.0 / total[fullOffset(EG_IR)]);
       
  VG_(message)(Vg_UserMsg, "LLi miss rate: %*.2f%%\n", l1,
               total[fullOffset(EG_IR)+2] * 100.0 / total[fullOffset(EG_IR)]);

  VG_(message)(Vg_UserMsg, "\n");
   

  D_total = CLG_(get_eventset_cost)( CLG_(sets).full );
  CLG_(init_cost)( CLG_(sets).full, D_total);
  
  CLG_(copy_cost)( CLG_(get_event_set)(EG_DR), D_total, total + fullOffset(EG_DR) );
  CLG_(add_cost) ( CLG_(get_event_set)(EG_DW), D_total, total + fullOffset(EG_DW) );

  VG_(message)(Vg_UserMsg, "D   refs:      %'*llu  (%'*llu rd + %'*llu wr)\n",
               l1, D_total[0],
               l2, total[fullOffset(EG_DR)],
               l3, total[fullOffset(EG_DW)]);

  VG_(message)(Vg_UserMsg, "D1  misses:    %'*llu  (%'*llu rd + %'*llu wr)\n",
               l1, D_total[1],
               l2, total[fullOffset(EG_DR)+1],
               l3, total[fullOffset(EG_DW)+1]);

  VG_(message)(Vg_UserMsg, "LLd misses:    %'*llu  (%'*llu rd + %'*llu wr)\n",
               l1, D_total[2],
               l2, total[fullOffset(EG_DR)+2],
               l3, total[fullOffset(EG_DW)+2]);

  if (0 == D_total[0])   D_total[0] = 1;
  if (0 == total[fullOffset(EG_DR)]) total[fullOffset(EG_DR)] = 1;
  if (0 == total[fullOffset(EG_DW)]) total[fullOffset(EG_DW)] = 1;
  
  VG_(message)(Vg_UserMsg, "D1  miss rate: %*.1f%% (%*.1f%%   + %*.1f%%  )\n", 
           l1, D_total[1] * 100.0 / D_total[0],
           l2, total[fullOffset(EG_DR)+1] * 100.0 / total[fullOffset(EG_DR)],
           l3, total[fullOffset(EG_DW)+1] * 100.0 / total[fullOffset(EG_DW)]);
  
  VG_(message)(Vg_UserMsg, "LLd miss rate: %*.1f%% (%*.1f%%   + %*.1f%%  )\n", 
           l1, D_total[2] * 100.0 / D_total[0],
           l2, total[fullOffset(EG_DR)+2] * 100.0 / total[fullOffset(EG_DR)],
           l3, total[fullOffset(EG_DW)+2] * 100.0 / total[fullOffset(EG_DW)]);
  VG_(message)(Vg_UserMsg, "\n");


  
  
  
  LL_total   =
    total[fullOffset(EG_DR) +1] +
    total[fullOffset(EG_DW) +1] +
    total[fullOffset(EG_IR) +1];
  LL_total_r =
    total[fullOffset(EG_DR) +1] +
    total[fullOffset(EG_IR) +1];
  LL_total_w = total[fullOffset(EG_DW) +1];
  VG_(message)(Vg_UserMsg, "LL refs:       %'*llu  (%'*llu rd + %'*llu wr)\n",
               l1, LL_total, l2, LL_total_r, l3, LL_total_w);
  
  LL_total_m  =
    total[fullOffset(EG_DR) +2] +
    total[fullOffset(EG_DW) +2] +
    total[fullOffset(EG_IR) +2];
  LL_total_mr =
    total[fullOffset(EG_DR) +2] +
    total[fullOffset(EG_IR) +2];
  LL_total_mw = total[fullOffset(EG_DW) +2];
  VG_(message)(Vg_UserMsg, "LL misses:     %'*llu  (%'*llu rd + %'*llu wr)\n",
               l1, LL_total_m, l2, LL_total_mr, l3, LL_total_mw);
  
  VG_(message)(Vg_UserMsg, "LL miss rate:  %*.1f%% (%*.1f%%   + %*.1f%%  )\n",
          l1, LL_total_m  * 100.0 / (total[fullOffset(EG_IR)] + D_total[0]),
          l2, LL_total_mr * 100.0 / (total[fullOffset(EG_IR)] + total[fullOffset(EG_DR)]),
          l3, LL_total_mw * 100.0 / total[fullOffset(EG_DW)]);
}



struct event_sets CLG_(sets);

void CLG_(init_eventsets)()
{
    
    
    if (clo_collect_cacheuse)
	CLG_(register_event_group4)(EG_USE,
				    "AcCost1", "SpLoss1", "AcCost2", "SpLoss2");

    if (!CLG_(clo).simulate_cache)
	CLG_(register_event_group)(EG_IR, "Ir");
    else if (!clo_simulate_writeback) {
	CLG_(register_event_group3)(EG_IR, "Ir", "I1mr", "ILmr");
	CLG_(register_event_group3)(EG_DR, "Dr", "D1mr", "DLmr");
	CLG_(register_event_group3)(EG_DW, "Dw", "D1mw", "DLmw");
    }
    else { 
	CLG_(register_event_group4)(EG_IR, "Ir", "I1mr", "ILmr", "ILdmr");
        CLG_(register_event_group4)(EG_DR, "Dr", "D1mr", "DLmr", "DLdmr");
        CLG_(register_event_group4)(EG_DW, "Dw", "D1mw", "DLmw", "DLdmw");
    }

    if (CLG_(clo).simulate_branch) {
        CLG_(register_event_group2)(EG_BC, "Bc", "Bcm");
        CLG_(register_event_group2)(EG_BI, "Bi", "Bim");
    }

    if (CLG_(clo).collect_bus)
	CLG_(register_event_group)(EG_BUS, "Ge");

    if (CLG_(clo).collect_alloc)
	CLG_(register_event_group2)(EG_ALLOC, "allocCount", "allocSize");

    if (CLG_(clo).collect_systime)
	CLG_(register_event_group2)(EG_SYS, "sysCount", "sysTime");

    
    CLG_(sets).base = CLG_(get_event_set2)(EG_USE, EG_IR);

    
    CLG_(sets).full = CLG_(add_event_group2)(CLG_(sets).base, EG_DR, EG_DW);
    CLG_(sets).full = CLG_(add_event_group2)(CLG_(sets).full, EG_BC, EG_BI);
    CLG_(sets).full = CLG_(add_event_group) (CLG_(sets).full, EG_BUS);
    CLG_(sets).full = CLG_(add_event_group2)(CLG_(sets).full, EG_ALLOC, EG_SYS);

    CLG_DEBUGIF(1) {
	CLG_DEBUG(1, "EventSets:\n");
	CLG_(print_eventset)(-2, CLG_(sets).base);
	CLG_(print_eventset)(-2, CLG_(sets).full);
    }

    
    CLG_(dumpmap) = CLG_(get_eventmapping)(CLG_(sets).full);
    CLG_(append_event)(CLG_(dumpmap), "Ir");
    CLG_(append_event)(CLG_(dumpmap), "Dr");
    CLG_(append_event)(CLG_(dumpmap), "Dw");
    CLG_(append_event)(CLG_(dumpmap), "I1mr");
    CLG_(append_event)(CLG_(dumpmap), "D1mr");
    CLG_(append_event)(CLG_(dumpmap), "D1mw");
    CLG_(append_event)(CLG_(dumpmap), "ILmr");
    CLG_(append_event)(CLG_(dumpmap), "DLmr");
    CLG_(append_event)(CLG_(dumpmap), "DLmw");
    CLG_(append_event)(CLG_(dumpmap), "ILdmr");
    CLG_(append_event)(CLG_(dumpmap), "DLdmr");
    CLG_(append_event)(CLG_(dumpmap), "DLdmw");
    CLG_(append_event)(CLG_(dumpmap), "Bc");
    CLG_(append_event)(CLG_(dumpmap), "Bcm");
    CLG_(append_event)(CLG_(dumpmap), "Bi");
    CLG_(append_event)(CLG_(dumpmap), "Bim");
    CLG_(append_event)(CLG_(dumpmap), "AcCost1");
    CLG_(append_event)(CLG_(dumpmap), "SpLoss1");
    CLG_(append_event)(CLG_(dumpmap), "AcCost2");
    CLG_(append_event)(CLG_(dumpmap), "SpLoss2");
    CLG_(append_event)(CLG_(dumpmap), "Ge");
    CLG_(append_event)(CLG_(dumpmap), "allocCount");
    CLG_(append_event)(CLG_(dumpmap), "allocSize");
    CLG_(append_event)(CLG_(dumpmap), "sysCount");
    CLG_(append_event)(CLG_(dumpmap), "sysTime");
}


static void cachesim_add_icost(SimCost cost, BBCC* bbcc,
			       InstrInfo* ii, ULong exe_count)
{
    if (!CLG_(clo).simulate_cache)
	cost[ fullOffset(EG_IR) ] += exe_count;

    if (ii->eventset)
	CLG_(add_and_zero_cost2)( CLG_(sets).full, cost,
				  ii->eventset, bbcc->cost + ii->cost_offset);
}

static
void cachesim_finish(void)
{
  if (clo_collect_cacheuse)
    cacheuse_finish();
}


struct cachesim_if CLG_(cachesim) = {
  .print_opts    = cachesim_print_opts,
  .parse_opt     = cachesim_parse_opt,
  .post_clo_init = cachesim_post_clo_init,
  .clear         = cachesim_clear,
  .dump_desc     = cachesim_dump_desc,
  .printstat     = cachesim_printstat,
  .add_icost     = cachesim_add_icost,
  .finish        = cachesim_finish,

  
  .log_1I0D        = 0,
  .log_2I0D        = 0,
  .log_3I0D        = 0,

  .log_1I1Dr       = 0,
  .log_1I1Dw       = 0,

  .log_0I1Dr       = 0,
  .log_0I1Dw       = 0,

  .log_1I0D_name = "(no function)",
  .log_2I0D_name = "(no function)",
  .log_3I0D_name = "(no function)",

  .log_1I1Dr_name = "(no function)",
  .log_1I1Dw_name = "(no function)",

  .log_0I1Dr_name = "(no function)",
  .log_0I1Dw_name = "(no function)",
};


