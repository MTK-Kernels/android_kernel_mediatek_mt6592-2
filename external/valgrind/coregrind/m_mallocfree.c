

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

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

#include "pub_core_basics.h"
#include "pub_core_vki.h"
#include "pub_core_debuglog.h"
#include "pub_core_libcbase.h"
#include "pub_core_aspacemgr.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcprint.h"
#include "pub_core_mallocfree.h"
#include "pub_core_options.h"
#include "pub_core_threadstate.h"   
#include "pub_core_gdbserver.h"
#include "pub_core_transtab.h"
#include "pub_core_tooliface.h"

#include "pub_core_inner.h"
#if defined(ENABLE_INNER_CLIENT_REQUEST)
#include "memcheck/memcheck.h"
#endif


Long VG_(free_queue_volume) = 0;
Long VG_(free_queue_length) = 0;

static void cc_analyse_alloc_arena ( ArenaId aid ); 


#define N_MALLOC_LISTS     112    

#define MAX_PSZB              (~((SizeT)0x0))

#define SBLOCKS_SIZE_INITIAL 50

typedef UChar UByte;

typedef
   struct {
      
      
      
      
      
      
      UByte dummy;
   } 
   Block;

typedef
   struct _Superblock {
      SizeT n_payload_bytes;
      struct _Superblock* unsplittable;
      UByte padding[ VG_MIN_MALLOC_SZB -
                        ((sizeof(struct _Superblock*) + sizeof(SizeT)) %
                         VG_MIN_MALLOC_SZB) ];
      UByte payload_bytes[0];
   }
   Superblock;

typedef
   struct {
      const HChar* name;
      Bool         clientmem;        
      SizeT        rz_szB;           
      SizeT        min_sblock_szB;   
      SizeT        min_unsplittable_sblock_szB;
      
      
      
      
      
      
      
      
      
      Block*       freelist[N_MALLOC_LISTS];
      
      
      
      
      
      
      
      Superblock** sblocks;
      SizeT        sblocks_size;
      SizeT        sblocks_used;
      Superblock*  sblocks_initial[SBLOCKS_SIZE_INITIAL];
      Superblock*  deferred_reclaimed_sb;

      
      
      
      Addr         perm_malloc_current; 
      Addr         perm_malloc_limit; 

      
      SizeT        stats__perm_bytes_on_loan;
      SizeT        stats__perm_blocks;

      ULong        stats__nreclaim_unsplit;
      ULong        stats__nreclaim_split;
      
      SizeT        stats__bytes_on_loan;
      SizeT        stats__bytes_mmaped;
      SizeT        stats__bytes_on_loan_max;
      ULong        stats__tot_blocks; 
      ULong        stats__tot_bytes; 
      ULong        stats__nsearches; 
      
      
      SizeT        next_profile_at;
      SizeT        stats__bytes_mmaped_max;
   }
   Arena;



#define SIZE_T_0x1      ((SizeT)0x1)

static const char* probably_your_fault =
   "This is probably caused by your program erroneously writing past the\n"
   "end of a heap block and corrupting heap metadata.  If you fix any\n"
   "invalid writes reported by Memcheck, this assertion failure will\n"
   "probably go away.  Please try that before reporting this as a bug.\n";

static __inline__
SizeT mk_inuse_bszB ( SizeT bszB )
{
   vg_assert2(bszB != 0, probably_your_fault);
   return bszB & (~SIZE_T_0x1);
}
static __inline__
SizeT mk_free_bszB ( SizeT bszB )
{
   vg_assert2(bszB != 0, probably_your_fault);
   return bszB | SIZE_T_0x1;
}
static __inline__
SizeT mk_plain_bszB ( SizeT bszB )
{
   vg_assert2(bszB != 0, probably_your_fault);
   return bszB & (~SIZE_T_0x1);
}

static
void ensure_mm_init ( ArenaId aid );

#define hp_overhead_szB() set_at_init_hp_overhead_szB
static SizeT set_at_init_hp_overhead_szB = -1000000; 


static __inline__
SizeT get_bszB_as_is ( Block* b )
{
   UByte* b2     = (UByte*)b;
   SizeT bszB_lo = *(SizeT*)&b2[0 + hp_overhead_szB()];
   SizeT bszB_hi = *(SizeT*)&b2[mk_plain_bszB(bszB_lo) - sizeof(SizeT)];
   vg_assert2(bszB_lo == bszB_hi, 
      "Heap block lo/hi size mismatch: lo = %llu, hi = %llu.\n%s",
      (ULong)bszB_lo, (ULong)bszB_hi, probably_your_fault);
   return bszB_lo;
}

static __inline__
SizeT get_bszB ( Block* b )
{
   return mk_plain_bszB(get_bszB_as_is(b));
}

static __inline__
void set_bszB ( Block* b, SizeT bszB )
{
   UByte* b2 = (UByte*)b;
   *(SizeT*)&b2[0 + hp_overhead_szB()]               = bszB;
   *(SizeT*)&b2[mk_plain_bszB(bszB) - sizeof(SizeT)] = bszB;
}


static __inline__
Bool is_inuse_block ( Block* b )
{
   SizeT bszB = get_bszB_as_is(b);
   vg_assert2(bszB != 0, probably_your_fault);
   return (0 != (bszB & SIZE_T_0x1)) ? False : True;
}


static __inline__
SizeT overhead_szB_lo ( Arena* a )
{
   return hp_overhead_szB() + sizeof(SizeT) + a->rz_szB;
}
static __inline__
SizeT overhead_szB_hi ( Arena* a )
{
   return a->rz_szB + sizeof(SizeT);
}
static __inline__
SizeT overhead_szB ( Arena* a )
{
   return overhead_szB_lo(a) + overhead_szB_hi(a);
}


static __inline__
SizeT min_useful_bszB ( Arena* a )
{
   return overhead_szB(a);
}


static __inline__
SizeT pszB_to_bszB ( Arena* a, SizeT pszB )
{
   return pszB + overhead_szB(a);
}
static __inline__
SizeT bszB_to_pszB ( Arena* a, SizeT bszB )
{
   vg_assert2(bszB >= overhead_szB(a), probably_your_fault);
   return bszB - overhead_szB(a);
}


static __inline__
SizeT get_pszB ( Arena* a, Block* b )
{
   return bszB_to_pszB(a, get_bszB(b));
}


static __inline__
UByte* get_block_payload ( Arena* a, Block* b )
{
   UByte* b2 = (UByte*)b;
   return & b2[ overhead_szB_lo(a) ];
}
static __inline__
Block* get_payload_block ( Arena* a, UByte* payload )
{
   return (Block*)&payload[ -overhead_szB_lo(a) ];
}


static __inline__
void set_prev_b ( Block* b, Block* prev_p )
{ 
   UByte* b2 = (UByte*)b;
   *(Block**)&b2[hp_overhead_szB() + sizeof(SizeT)] = prev_p;
}
static __inline__
void set_next_b ( Block* b, Block* next_p )
{
   UByte* b2 = (UByte*)b;
   *(Block**)&b2[get_bszB(b) - sizeof(SizeT) - sizeof(void*)] = next_p;
}
static __inline__
Block* get_prev_b ( Block* b )
{ 
   UByte* b2 = (UByte*)b;
   return *(Block**)&b2[hp_overhead_szB() + sizeof(SizeT)];
}
static __inline__
Block* get_next_b ( Block* b )
{ 
   UByte* b2 = (UByte*)b;
   return *(Block**)&b2[get_bszB(b) - sizeof(SizeT) - sizeof(void*)];
}


static __inline__
void set_cc ( Block* b, const HChar* cc )
{ 
   UByte* b2 = (UByte*)b;
   vg_assert( VG_(clo_profile_heap) );
   *(const HChar**)&b2[0] = cc;
}
static __inline__
const HChar* get_cc ( Block* b )
{
   UByte* b2 = (UByte*)b;
   vg_assert( VG_(clo_profile_heap) );
   return *(const HChar**)&b2[0];
}


static __inline__
Block* get_predecessor_block ( Block* b )
{
   UByte* b2 = (UByte*)b;
   SizeT  bszB = mk_plain_bszB( (*(SizeT*)&b2[-sizeof(SizeT)]) );
   return (Block*)&b2[-bszB];
}


static __inline__
void set_rz_lo_byte ( Block* b, UInt rz_byteno, UByte v )
{
   UByte* b2 = (UByte*)b;
   b2[hp_overhead_szB() + sizeof(SizeT) + rz_byteno] = v;
}
static __inline__
void set_rz_hi_byte ( Block* b, UInt rz_byteno, UByte v )
{
   UByte* b2 = (UByte*)b;
   b2[get_bszB(b) - sizeof(SizeT) - rz_byteno - 1] = v;
}
static __inline__
UByte get_rz_lo_byte ( Block* b, UInt rz_byteno )
{
   UByte* b2 = (UByte*)b;
   return b2[hp_overhead_szB() + sizeof(SizeT) + rz_byteno];
}
static __inline__
UByte get_rz_hi_byte ( Block* b, UInt rz_byteno )
{
   UByte* b2 = (UByte*)b;
   return b2[get_bszB(b) - sizeof(SizeT) - rz_byteno - 1];
}

#if defined(ENABLE_INNER_CLIENT_REQUEST)
static void mkBhdrAccess( Arena* a, Block* b )
{
   VALGRIND_MAKE_MEM_DEFINED (b,
                              hp_overhead_szB() + sizeof(SizeT) + a->rz_szB);
   VALGRIND_MAKE_MEM_DEFINED (b + get_bszB(b) - a->rz_szB - sizeof(SizeT),
                              a->rz_szB + sizeof(SizeT));
}

static void mkBhdrNoAccess( Arena* a, Block* b )
{
   VALGRIND_MAKE_MEM_NOACCESS (b + hp_overhead_szB() + sizeof(SizeT),
                               a->rz_szB);
   VALGRIND_MAKE_MEM_NOACCESS (b + get_bszB(b) - sizeof(SizeT) - a->rz_szB,
                               a->rz_szB);
}

static void mkBhdrSzAccess( Arena* a, Block* b )
{
   VALGRIND_MAKE_MEM_DEFINED (b,
                              hp_overhead_szB() + sizeof(SizeT));
   SizeT bszB_lo = mk_plain_bszB(*(SizeT*)&b[0 + hp_overhead_szB()]);
   VALGRIND_MAKE_MEM_DEFINED (b + bszB_lo - sizeof(SizeT),
                              sizeof(SizeT));
}
#endif


#define CORE_ARENA_MIN_SZB    1048576

static Arena vg_arena[VG_N_ARENAS];

static Arena* arenaId_to_ArenaP ( ArenaId arena )
{
   vg_assert(arena >= 0 && arena < VG_N_ARENAS);
   return & vg_arena[arena];
}

static ArenaId arenaP_to_ArenaId ( Arena *a )
{
   ArenaId arena = a -vg_arena; 
   vg_assert(arena >= 0 && arena < VG_N_ARENAS);
   return arena;
}

static
void arena_init ( ArenaId aid, const HChar* name, SizeT rz_szB,
                  SizeT min_sblock_szB, SizeT min_unsplittable_sblock_szB )
{
   SizeT  i;
   Arena* a = arenaId_to_ArenaP(aid);

   
   vg_assert(rz_szB <= MAX_REDZONE_SZB);
   
   if (VG_AR_CLIENT == aid) {
      if (VG_(clo_redzone_size) != -1)
         rz_szB = VG_(clo_redzone_size);
   } else {
      if (VG_(clo_core_redzone_size) != rz_szB)
         rz_szB = VG_(clo_core_redzone_size);
   }

   
   
   if (rz_szB < sizeof(void*)) rz_szB = sizeof(void*);

   
   
   
   a->rz_szB = rz_szB;
   while (0 != overhead_szB_lo(a) % VG_MIN_MALLOC_SZB) a->rz_szB++;
   vg_assert(overhead_szB_lo(a) - hp_overhead_szB() == overhead_szB_hi(a));

   


   vg_assert((min_sblock_szB % VKI_PAGE_SIZE) == 0);
   a->name      = name;
   a->clientmem = ( VG_AR_CLIENT == aid ? True : False );

   a->min_sblock_szB = min_sblock_szB;
   a->min_unsplittable_sblock_szB = min_unsplittable_sblock_szB;
   for (i = 0; i < N_MALLOC_LISTS; i++) a->freelist[i] = NULL;

   a->sblocks                  = & a->sblocks_initial[0];
   a->sblocks_size             = SBLOCKS_SIZE_INITIAL;
   a->sblocks_used             = 0;
   a->deferred_reclaimed_sb    = 0;
   a->perm_malloc_current      = 0;
   a->perm_malloc_limit        = 0;
   a->stats__perm_bytes_on_loan= 0;
   a->stats__perm_blocks       = 0;
   a->stats__nreclaim_unsplit  = 0;
   a->stats__nreclaim_split    = 0;
   a->stats__bytes_on_loan     = 0;
   a->stats__bytes_mmaped      = 0;
   a->stats__bytes_on_loan_max = 0;
   a->stats__bytes_mmaped_max  = 0;
   a->stats__tot_blocks        = 0;
   a->stats__tot_bytes         = 0;
   a->stats__nsearches         = 0;
   a->next_profile_at          = 25 * 1000 * 1000;
   vg_assert(sizeof(a->sblocks_initial) 
             == SBLOCKS_SIZE_INITIAL * sizeof(Superblock*));
}

void VG_(print_all_arena_stats) ( void )
{
   UInt i;
   for (i = 0; i < VG_N_ARENAS; i++) {
      Arena* a = arenaId_to_ArenaP(i);
      VG_(message)(Vg_DebugMsg,
                   "%-8s: %'13lu/%'13lu max/curr mmap'd, "
                   "%llu/%llu unsplit/split sb unmmap'd,  "
                   "%'13lu/%'13lu max/curr,  "
                   "%10llu/%10llu totalloc-blocks/bytes,"
                   "  %10llu searches %lu rzB\n",
                   a->name,
                   a->stats__bytes_mmaped_max, a->stats__bytes_mmaped,
                   a->stats__nreclaim_unsplit, a->stats__nreclaim_split,
                   a->stats__bytes_on_loan_max,
                   a->stats__bytes_on_loan,
                   a->stats__tot_blocks, a->stats__tot_bytes,
                   a->stats__nsearches,
                   a->rz_szB
      );
   }
}

void VG_(print_arena_cc_analysis) ( void )
{
   UInt i;
   vg_assert( VG_(clo_profile_heap) );
   for (i = 0; i < VG_N_ARENAS; i++) {
      cc_analyse_alloc_arena(i);
   }
}


static Bool     client_inited = False;
static Bool  nonclient_inited = False;

static
void ensure_mm_init ( ArenaId aid )
{
   static SizeT client_rz_szB = 8;     

   if (VG_AR_CLIENT == aid) {
      Int ar_client_sbszB;
      if (client_inited) {
         
         
         
         if (VG_(needs).malloc_replacement)
            vg_assert(client_rz_szB == VG_(tdict).tool_client_redzone_szB);
         return;
      }

      
      if (VG_(needs).malloc_replacement) {
         client_rz_szB = VG_(tdict).tool_client_redzone_szB;
         if (client_rz_szB > MAX_REDZONE_SZB) {
            VG_(printf)( "\nTool error:\n"
                         "  specified redzone size is too big (%llu)\n", 
                         (ULong)client_rz_szB);
            VG_(exit)(1);
         }
      }
      
      
      
      ar_client_sbszB = 4194304;
      
      
      arena_init ( VG_AR_CLIENT,    "client",   client_rz_szB, 
                   ar_client_sbszB, ar_client_sbszB+1);
      client_inited = True;

   } else {
      if (nonclient_inited) {
         return;
      }
      set_at_init_hp_overhead_szB = 
         VG_(clo_profile_heap)  ? VG_MIN_MALLOC_SZB  : 0;
      
      
      arena_init ( VG_AR_CORE,      "core",     CORE_REDZONE_DEFAULT_SZB,
                   4194304, 4194304+1 );
      arena_init ( VG_AR_DINFO,     "dinfo",    CORE_REDZONE_DEFAULT_SZB,
                   1048576, 1048576+1 );
      arena_init ( VG_AR_DEMANGLE,  "demangle", CORE_REDZONE_DEFAULT_SZB,
                   65536,   65536+1 );
      arena_init ( VG_AR_TTAUX,     "ttaux",    CORE_REDZONE_DEFAULT_SZB,
                   65536,   65536+1 );
      nonclient_inited = True;
   }

#  ifdef DEBUG_MALLOC
   VG_(printf)("ZZZ1\n");
   VG_(sanity_check_malloc_all)();
   VG_(printf)("ZZZ2\n");
#  endif
}



__attribute__((noreturn))
void VG_(out_of_memory_NORETURN) ( const HChar* who, SizeT szB )
{
   static Int outputTrial = 0;
   
   
   
   
   
   ULong tot_alloc = VG_(am_get_anonsize_total)();
   const HChar* s1 = 
      "\n"
      "    Valgrind's memory management: out of memory:\n"
      "       %s's request for %llu bytes failed.\n"
      "       %'13llu bytes have already been mmap-ed ANONYMOUS.\n"
      "    Valgrind cannot continue.  Sorry.\n\n"
      "    There are several possible reasons for this.\n"
      "    - You have some kind of memory limit in place.  Look at the\n"
      "      output of 'ulimit -a'.  Is there a limit on the size of\n"
      "      virtual memory or address space?\n"
      "    - You have run out of swap space.\n"
      "    - Valgrind has a bug.  If you think this is the case or you are\n"
      "    not sure, please let us know and we'll try to fix it.\n"
      "    Please note that programs can take substantially more memory than\n"
      "    normal when running under Valgrind tools, eg. up to twice or\n"
      "    more, depending on the tool.  On a 64-bit machine, Valgrind\n"
      "    should be able to make use of up 32GB memory.  On a 32-bit\n"
      "    machine, Valgrind should be able to use all the memory available\n"
      "    to a single process, up to 4GB if that's how you have your\n"
      "    kernel configured.  Most 32-bit Linux setups allow a maximum of\n"
      "    3GB per process.\n\n"
      "    Whatever the reason, Valgrind cannot continue.  Sorry.\n";

   if (outputTrial <= 1) {
      if (outputTrial == 0) {
         outputTrial++;
         
         VG_(am_show_nsegments) (0, "out_of_memory");
         VG_(print_all_arena_stats) ();
         if (VG_(clo_profile_heap))
            VG_(print_arena_cc_analysis) ();
         
         VG_(print_all_stats) (False, 
                               True );
         VG_(show_sched_status) (True,  
                                 True,  
                                 True); 
         INNER_REQUEST(VALGRIND_MONITOR_COMMAND("v.set log_output"));
         INNER_REQUEST(VALGRIND_MONITOR_COMMAND("v.info memory aspacemgr"));
      }
      outputTrial++;
      VG_(message)(Vg_UserMsg, s1, who, (ULong)szB, tot_alloc);
   } else {
      VG_(debugLog)(0,"mallocfree", s1, who, (ULong)szB, tot_alloc);
   }

   VG_(exit)(1);
}


static
void* align_upwards ( void* p, SizeT align )
{
   Addr a = (Addr)p;
   if ((a % align) == 0) return (void*)a;
   return (void*)(a - (a % align) + align);
}

static
void deferred_reclaimSuperblock ( Arena* a, Superblock* sb);

static
Superblock* newSuperblock ( Arena* a, SizeT cszB )
{
   Superblock* sb;
   SysRes      sres;
   Bool        unsplittable;
   ArenaId     aid;

   
   
   
   for (aid = 0; aid < VG_N_ARENAS; aid++) {
      Arena* arena = arenaId_to_ArenaP(aid);
      if (arena->deferred_reclaimed_sb != NULL)
         deferred_reclaimSuperblock (arena, NULL);
   }

   
   cszB += sizeof(Superblock);

   if (cszB < a->min_sblock_szB) cszB = a->min_sblock_szB;
   cszB = VG_PGROUNDUP(cszB);

   if (cszB >= a->min_unsplittable_sblock_szB)
      unsplittable = True;
   else
      unsplittable = False;   


   if (a->clientmem) {
      
      sres = VG_(am_mmap_client_heap)
         ( cszB, VKI_PROT_READ|VKI_PROT_WRITE|VKI_PROT_EXEC );
      if (sr_isError(sres))
         return 0;
      sb = (Superblock*)(Addr)sr_Res(sres);
   } else {
      
      sres = VG_(am_mmap_anon_float_valgrind)( cszB );
      if (sr_isError(sres)) {
         VG_(out_of_memory_NORETURN)("newSuperblock", cszB);
         
         sb = NULL; 
      } else {
         sb = (Superblock*)(Addr)sr_Res(sres);
      }
   }
   vg_assert(NULL != sb);
   INNER_REQUEST(VALGRIND_MAKE_MEM_UNDEFINED(sb, cszB));
   vg_assert(0 == (Addr)sb % VG_MIN_MALLOC_SZB);
   sb->n_payload_bytes = cszB - sizeof(Superblock);
   sb->unsplittable = (unsplittable ? sb : NULL);
   a->stats__bytes_mmaped += cszB;
   if (a->stats__bytes_mmaped > a->stats__bytes_mmaped_max)
      a->stats__bytes_mmaped_max = a->stats__bytes_mmaped;
   VG_(debugLog)(1, "mallocfree",
                    "newSuperblock at %p (pszB %7ld) %s owner %s/%s\n", 
                    sb, sb->n_payload_bytes,
                    (unsplittable ? "unsplittable" : ""),
                    a->clientmem ? "CLIENT" : "VALGRIND", a->name );
   return sb;
}

static
void reclaimSuperblock ( Arena* a, Superblock* sb)
{
   SysRes sres;
   SizeT  cszB;
   UInt   i, j;

   VG_(debugLog)(1, "mallocfree",
                    "reclaimSuperblock at %p (pszB %7ld) %s owner %s/%s\n", 
                    sb, sb->n_payload_bytes,
                    (sb->unsplittable ? "unsplittable" : ""),
                    a->clientmem ? "CLIENT" : "VALGRIND", a->name );

   
   cszB = sizeof(Superblock) + sb->n_payload_bytes;

   
   for (i = 0; i < a->sblocks_used; i++) {
      if (a->sblocks[i] == sb)
         break;
   }
   vg_assert(i >= 0 && i < a->sblocks_used);
   for (j = i; j < a->sblocks_used; j++)
      a->sblocks[j] = a->sblocks[j+1];
   a->sblocks_used--;
   a->sblocks[a->sblocks_used] = NULL;
   

   a->stats__bytes_mmaped -= cszB;
   if (sb->unsplittable)
      a->stats__nreclaim_unsplit++;
   else
      a->stats__nreclaim_split++;

   
   if (a->clientmem) {
      
      Bool need_discard = False;
      sres = VG_(am_munmap_client)(&need_discard, (Addr) sb, cszB);
      vg_assert2(! sr_isError(sres), "superblock client munmap failure\n");
      if (need_discard)
         VG_(discard_translations) ((Addr) sb, cszB, "reclaimSuperblock");
   } else {
      
      sres = VG_(am_munmap_valgrind)((Addr) sb, cszB);
      vg_assert2(! sr_isError(sres), "superblock valgrind munmap failure\n");
   }

}

static
Superblock* findSb ( Arena* a, Block* b )
{
   SizeT min = 0;
   SizeT max = a->sblocks_used;

   while (min <= max) {
      Superblock * sb; 
      SizeT pos = min + (max - min)/2;

      vg_assert(pos >= 0 && pos < a->sblocks_used);
      sb = a->sblocks[pos];
      if ((Block*)&sb->payload_bytes[0] <= b
          && b < (Block*)&sb->payload_bytes[sb->n_payload_bytes])
      {
         return sb;
      } else if ((Block*)&sb->payload_bytes[0] <= b) {
         min = pos + 1;
      } else {
         max = pos - 1;
      }
   }
   VG_(printf)("findSb: can't find pointer %p in arena '%s'\n",
                b, a->name );
   VG_(core_panic)("findSb: VG_(arena_free)() in wrong arena?");
   return NULL; 
}


static
Superblock* maybe_findSb ( Arena* a, Addr ad )
{
   SizeT min = 0;
   SizeT max = a->sblocks_used;

   while (min <= max) {
      Superblock * sb; 
      SizeT pos = min + (max - min)/2;
      if (pos < 0 || pos >= a->sblocks_used)
         return NULL;
      sb = a->sblocks[pos];
      if ((Addr)&sb->payload_bytes[0] <= ad
          && ad < (Addr)&sb->payload_bytes[sb->n_payload_bytes]) {
         return sb;
      } else if ((Addr)&sb->payload_bytes[0] <= ad) {
         min = pos + 1;
      } else {
         max = pos - 1;
      }
   }
   return NULL;
}




static
UInt pszB_to_listNo ( SizeT pszB )
{
   SizeT n = pszB / VG_MIN_MALLOC_SZB;
   vg_assert(0 == pszB % VG_MIN_MALLOC_SZB);

   
   
   if (n < 64)   return (UInt)n;
   
   if (n < 67) return 64;
   if (n < 70) return 65;
   if (n < 74) return 66;
   if (n < 77) return 67;
   if (n < 81) return 68;
   if (n < 85) return 69;
   if (n < 90) return 70;
   if (n < 94) return 71;
   if (n < 99) return 72;
   if (n < 104) return 73;
   if (n < 109) return 74;
   if (n < 114) return 75;
   if (n < 120) return 76;
   if (n < 126) return 77;
   if (n < 133) return 78;
   if (n < 139) return 79;
   
   if (n < 153) return 80;
   if (n < 169) return 81;
   if (n < 185) return 82;
   if (n < 204) return 83;
   if (n < 224) return 84;
   if (n < 247) return 85;
   if (n < 272) return 86;
   if (n < 299) return 87;
   if (n < 329) return 88;
   if (n < 362) return 89;
   if (n < 398) return 90;
   if (n < 438) return 91;
   if (n < 482) return 92;
   if (n < 530) return 93;
   if (n < 583) return 94;
   if (n < 641) return 95;
   
   if (n < 770) return 96;
   if (n < 924) return 97;
   if (n < 1109) return 98;
   if (n < 1331) return 99;
   if (n < 1597) return 100;
   if (n < 1916) return 101;
   if (n < 2300) return 102;
   if (n < 2760) return 103;
   if (n < 3312) return 104;
   if (n < 3974) return 105;
   if (n < 4769) return 106;
   if (n < 5723) return 107;
   if (n < 6868) return 108;
   if (n < 8241) return 109;
   if (n < 9890) return 110;
   return 111;
}

static
SizeT listNo_to_pszB_min ( UInt listNo )
{
   static SizeT cache[N_MALLOC_LISTS];
   static Bool  cache_valid = False;
   if (!cache_valid) {
      UInt i;
      for (i = 0; i < N_MALLOC_LISTS; i++) {
         SizeT pszB = 0;
         while (pszB_to_listNo(pszB) < i)
            pszB += VG_MIN_MALLOC_SZB;
         cache[i] = pszB;
      }
      cache_valid = True;
   }
   
   vg_assert(listNo <= N_MALLOC_LISTS);
   return cache[listNo];
}

static
SizeT listNo_to_pszB_max ( UInt listNo )
{
   vg_assert(listNo <= N_MALLOC_LISTS);
   if (listNo == N_MALLOC_LISTS-1) {
      return MAX_PSZB;
   } else {
      return listNo_to_pszB_min(listNo+1) - 1;
   }
}


static 
void swizzle ( Arena* a, UInt lno )
{
   Block* p_best;
   Block* pp;
   Block* pn;
   UInt   i;

   p_best = a->freelist[lno];
   if (p_best == NULL) return;

   pn = pp = p_best;

   
   
   
   
   for (i = 0; i < 5; i++) {
      pn = get_next_b(pn);
      pp = get_prev_b(pp);
      if (pn < p_best) p_best = pn;
      if (pp < p_best) p_best = pp;
   }
   if (p_best < a->freelist[lno]) {
#     ifdef VERBOSE_MALLOC
      VG_(printf)("retreat by %ld\n", (Word)(a->freelist[lno] - p_best));
#     endif
      a->freelist[lno] = p_best;
   }
}



#define REDZONE_LO_MASK    0x31
#define REDZONE_HI_MASK    0x7c

static 
Bool blockSane ( Arena* a, Block* b )
{
#  define BLEAT(str) VG_(printf)("blockSane: fail -- %s\n",str)
   UInt i;
   
   
   if (!a->clientmem && is_inuse_block(b)) {
      
      INNER_REQUEST(mkBhdrAccess(a,b));
      for (i = 0; i < a->rz_szB; i++) {
         if (get_rz_lo_byte(b, i) != 
            (UByte)(((Addr)b&0xff) ^ REDZONE_LO_MASK))
               {BLEAT("redzone-lo");return False;}
         if (get_rz_hi_byte(b, i) != 
            (UByte)(((Addr)b&0xff) ^ REDZONE_HI_MASK))
               {BLEAT("redzone-hi");return False;}
      }
      INNER_REQUEST(mkBhdrNoAccess(a,b));
   }
   return True;
#  undef BLEAT
}

static 
Bool unsplittableBlockSane ( Arena* a, Superblock *sb, Block* b )
{
#  define BLEAT(str) VG_(printf)("unsplittableBlockSane: fail -- %s\n",str)
   Block*      other_b;
   UByte* sb_start;
   UByte* sb_end;

   if (!blockSane (a, b))
      {BLEAT("blockSane");return False;}
   
   if (sb->unsplittable != sb)
      {BLEAT("unsplittable");return False;}

   sb_start = &sb->payload_bytes[0];
   sb_end   = &sb->payload_bytes[sb->n_payload_bytes - 1];

   
   if ((Block*)sb_start != b)
      {BLEAT("sb_start");return False;}

   
   other_b = b + get_bszB(b);
   if (other_b-1 != (Block*)sb_end)
      {BLEAT("sb_end");return False;}
   
   return True;
#  undef BLEAT
}

static 
void ppSuperblocks ( Arena* a )
{
   UInt i, j, blockno = 1;
   SizeT b_bszB;

   for (j = 0; j < a->sblocks_used; ++j) {
      Superblock * sb = a->sblocks[j];

      VG_(printf)( "\n" );
      VG_(printf)( "superblock %d at %p %s, sb->n_pl_bs = %lu\n",
                   blockno++, sb, (sb->unsplittable ? "unsplittable" : ""),
                   sb->n_payload_bytes);
      for (i = 0; i < sb->n_payload_bytes; i += b_bszB) {
         Block* b = (Block*)&sb->payload_bytes[i];
         b_bszB   = get_bszB(b);
         VG_(printf)( "   block at %d, bszB %lu: ", i, b_bszB );
         VG_(printf)( "%s, ", is_inuse_block(b) ? "inuse" : "free");
         VG_(printf)( "%s\n", blockSane(a, b) ? "ok" : "BAD" );
      }
      vg_assert(i == sb->n_payload_bytes);   
   }
   VG_(printf)( "end of superblocks\n\n" );
}

static void sanity_check_malloc_arena ( ArenaId aid )
{
   UInt        i, j, superblockctr, blockctr_sb, blockctr_li;
   UInt        blockctr_sb_free, listno;
   SizeT       b_bszB, b_pszB, list_min_pszB, list_max_pszB;
   Bool        thisFree, lastWasFree, sblockarrOK;
   Block*      b;
   Block*      b_prev;
   SizeT       arena_bytes_on_loan;
   Arena*      a;

#  define BOMB VG_(core_panic)("sanity_check_malloc_arena")

   a = arenaId_to_ArenaP(aid);

   
   sblockarrOK
      = a->sblocks != NULL
        && a->sblocks_size >= SBLOCKS_SIZE_INITIAL
        && a->sblocks_used <= a->sblocks_size
        && (a->sblocks_size == SBLOCKS_SIZE_INITIAL 
            ? (a->sblocks == &a->sblocks_initial[0])
            : (a->sblocks != &a->sblocks_initial[0]));
   if (!sblockarrOK) {
      VG_(printf)("sanity_check_malloc_arena: sblock array BAD\n");
      BOMB;
   }

   
   superblockctr = blockctr_sb = blockctr_sb_free = 0;
   arena_bytes_on_loan = 0;
   for (j = 0; j < a->sblocks_used; ++j) {
      Superblock * sb = a->sblocks[j];
      lastWasFree = False;
      superblockctr++;
      for (i = 0; i < sb->n_payload_bytes; i += mk_plain_bszB(b_bszB)) {
         blockctr_sb++;
         b     = (Block*)&sb->payload_bytes[i];
         b_bszB = get_bszB_as_is(b);
         if (!blockSane(a, b)) {
            VG_(printf)("sanity_check_malloc_arena: sb %p, block %d "
                        "(bszB %lu):  BAD\n", sb, i, b_bszB );
            BOMB;
         }
         thisFree = !is_inuse_block(b);
         if (thisFree && lastWasFree) {
            VG_(printf)("sanity_check_malloc_arena: sb %p, block %d "
                        "(bszB %lu): UNMERGED FREES\n", sb, i, b_bszB );
            BOMB;
         }
         if (thisFree) blockctr_sb_free++;
         if (!thisFree)
            arena_bytes_on_loan += bszB_to_pszB(a, b_bszB);
         lastWasFree = thisFree;
      }
      if (i > sb->n_payload_bytes) {
         VG_(printf)( "sanity_check_malloc_arena: sb %p: last block "
                      "overshoots end\n", sb);
         BOMB;
      }
   }

   arena_bytes_on_loan += a->stats__perm_bytes_on_loan;

   if (arena_bytes_on_loan != a->stats__bytes_on_loan) {
#     ifdef VERBOSE_MALLOC
      VG_(printf)( "sanity_check_malloc_arena: a->bytes_on_loan %lu, "
                   "arena_bytes_on_loan %lu: "
                   "MISMATCH\n", a->stats__bytes_on_loan, arena_bytes_on_loan);
#     endif
      ppSuperblocks(a);
      BOMB;
   }

   blockctr_li = 0;
   for (listno = 0; listno < N_MALLOC_LISTS; listno++) {
      list_min_pszB = listNo_to_pszB_min(listno);
      list_max_pszB = listNo_to_pszB_max(listno);
      b = a->freelist[listno];
      if (b == NULL) continue;
      while (True) {
         b_prev = b;
         b = get_next_b(b);
         if (get_prev_b(b) != b_prev) {
            VG_(printf)( "sanity_check_malloc_arena: list %d at %p: "
                         "BAD LINKAGE\n",
                         listno, b );
            BOMB;
         }
         b_pszB = get_pszB(a, b);
         if (b_pszB < list_min_pszB || b_pszB > list_max_pszB) {
            VG_(printf)(
               "sanity_check_malloc_arena: list %d at %p: "
               "WRONG CHAIN SIZE %luB (%luB, %luB)\n",
               listno, b, b_pszB, list_min_pszB, list_max_pszB );
            BOMB;
         }
         blockctr_li++;
         if (b == a->freelist[listno]) break;
      }
   }

   if (blockctr_sb_free != blockctr_li) {
#     ifdef VERBOSE_MALLOC
      VG_(printf)( "sanity_check_malloc_arena: BLOCK COUNT MISMATCH "
                   "(via sbs %d, via lists %d)\n",
                   blockctr_sb_free, blockctr_li );
#     endif
      ppSuperblocks(a);
      BOMB;
   }

   if (VG_(clo_verbosity) > 2) 
      VG_(message)(Vg_DebugMsg,
                   "%-8s: %2d sbs, %5d bs, %2d/%-2d free bs, "
                   "%7ld mmap, %7ld loan\n",
                   a->name,
                   superblockctr,
                   blockctr_sb, blockctr_sb_free, blockctr_li, 
                   a->stats__bytes_mmaped, a->stats__bytes_on_loan);   
#  undef BOMB
}


#define N_AN_CCS 1000

typedef struct {
   ULong nBytes;
   ULong nBlocks;
   const HChar* cc;
} AnCC;

static AnCC anCCs[N_AN_CCS];

static Int cmp_AnCC_by_vol ( const void* v1, const void* v2 ) {
   const AnCC* ancc1 = v1;
   const AnCC* ancc2 = v2;
   if (ancc1->nBytes < ancc2->nBytes) return 1;
   if (ancc1->nBytes > ancc2->nBytes) return -1;
   return 0;
}

static void cc_analyse_alloc_arena ( ArenaId aid )
{
   Word i, j, k;
   Arena*      a;
   Block*      b;
   Bool        thisFree, lastWasFree;
   SizeT       b_bszB;

   const HChar* cc;
   UInt n_ccs = 0;
   
   a = arenaId_to_ArenaP(aid);
   if (a->name == NULL) {
      return;
   }

   sanity_check_malloc_arena(aid);

   VG_(printf)(
      "-------- Arena \"%s\": %'lu/%'lu max/curr mmap'd, "
      "%llu/%llu unsplit/split sb unmmap'd, "
      "%'lu/%'lu max/curr on_loan %lu rzB --------\n",
      a->name, a->stats__bytes_mmaped_max, a->stats__bytes_mmaped,
      a->stats__nreclaim_unsplit, a->stats__nreclaim_split,
      a->stats__bytes_on_loan_max, a->stats__bytes_on_loan,
      a->rz_szB
   );

   for (j = 0; j < a->sblocks_used; ++j) {
      Superblock * sb = a->sblocks[j];
      lastWasFree = False;
      for (i = 0; i < sb->n_payload_bytes; i += mk_plain_bszB(b_bszB)) {
         b     = (Block*)&sb->payload_bytes[i];
         b_bszB = get_bszB_as_is(b);
         if (!blockSane(a, b)) {
            VG_(printf)("sanity_check_malloc_arena: sb %p, block %ld "
                        "(bszB %lu):  BAD\n", sb, i, b_bszB );
            vg_assert(0);
         }
         thisFree = !is_inuse_block(b);
         if (thisFree && lastWasFree) {
            VG_(printf)("sanity_check_malloc_arena: sb %p, block %ld "
                        "(bszB %lu): UNMERGED FREES\n", sb, i, b_bszB );
            vg_assert(0);
         }
         lastWasFree = thisFree;

         if (thisFree) continue;

         if (VG_(clo_profile_heap))
            cc = get_cc(b);
         else
            cc = "(--profile-heap=yes for details)";
         if (0)
         VG_(printf)("block: inUse=%d pszB=%d cc=%s\n", 
                     (Int)(!thisFree), 
                     (Int)bszB_to_pszB(a, b_bszB),
                     get_cc(b));
         vg_assert(cc);
         for (k = 0; k < n_ccs; k++) {
           vg_assert(anCCs[k].cc);
            if (0 == VG_(strcmp)(cc, anCCs[k].cc))
               break;
         }
         vg_assert(k >= 0 && k <= n_ccs);

         if (k == n_ccs) {
            vg_assert(n_ccs < N_AN_CCS-1);
            n_ccs++;
            anCCs[k].nBytes  = 0;
            anCCs[k].nBlocks = 0;
            anCCs[k].cc      = cc;
         }

         vg_assert(k >= 0 && k < n_ccs && k < N_AN_CCS);
         anCCs[k].nBytes += (ULong)bszB_to_pszB(a, b_bszB);
         anCCs[k].nBlocks++;
      }
      if (i > sb->n_payload_bytes) {
         VG_(printf)( "sanity_check_malloc_arena: sb %p: last block "
                      "overshoots end\n", sb);
         vg_assert(0);
      }
   }

   if (a->stats__perm_bytes_on_loan > 0) {
      vg_assert(n_ccs < N_AN_CCS-1);
      anCCs[n_ccs].nBytes  = a->stats__perm_bytes_on_loan;
      anCCs[n_ccs].nBlocks = a->stats__perm_blocks;
      anCCs[n_ccs].cc      = "perm_malloc";
      n_ccs++;
   }

   VG_(ssort)( &anCCs[0], n_ccs, sizeof(anCCs[0]), cmp_AnCC_by_vol );

   for (k = 0; k < n_ccs; k++) {
      VG_(printf)("%'13llu in %'9llu: %s\n",
                  anCCs[k].nBytes, anCCs[k].nBlocks, anCCs[k].cc );
   }

   VG_(printf)("\n");
}


void VG_(sanity_check_malloc_all) ( void )
{
   UInt i;
   for (i = 0; i < VG_N_ARENAS; i++) {
      if (i == VG_AR_CLIENT && !client_inited)
         continue;
      sanity_check_malloc_arena ( i );
   }
}

void VG_(describe_arena_addr) ( Addr a, AddrArenaInfo* aai )
{
   UInt i;
   Superblock *sb;
   Arena      *arena;

   for (i = 0; i < VG_N_ARENAS; i++) {
      if (i == VG_AR_CLIENT && !client_inited)
         continue;
      arena = arenaId_to_ArenaP(i);
      sb = maybe_findSb( arena, a );
      if (sb != NULL) {
         Word   j;
         SizeT  b_bszB;
         Block *b = NULL;

         aai->aid = i;
         aai->name = arena->name;
         for (j = 0; j < sb->n_payload_bytes; j += mk_plain_bszB(b_bszB)) {
            b     = (Block*)&sb->payload_bytes[j];
            b_bszB = get_bszB_as_is(b);
            if (a < (Addr)b + mk_plain_bszB(b_bszB))
               break;
         }
         vg_assert (b);
         aai->block_szB = get_pszB(arena, b);
         aai->rwoffset = a - (Addr)get_block_payload(arena, b);
         aai->free = !is_inuse_block(b);
         return;
      }
   }
   aai->aid = 0;
   aai->name = NULL;
   aai->block_szB = 0;
   aai->rwoffset = 0;
   aai->free = False;
}



static
void mkFreeBlock ( Arena* a, Block* b, SizeT bszB, UInt b_lno )
{
   SizeT pszB = bszB_to_pszB(a, bszB);
   vg_assert(b_lno == pszB_to_listNo(pszB));
   INNER_REQUEST(VALGRIND_MAKE_MEM_UNDEFINED(b, bszB));
   
   set_bszB(b, mk_free_bszB(bszB));

   
   if (a->freelist[b_lno] == NULL) {
      set_prev_b(b, b);
      set_next_b(b, b);
      a->freelist[b_lno] = b;
   } else {
      Block* b_prev = get_prev_b(a->freelist[b_lno]);
      Block* b_next = a->freelist[b_lno];
      set_next_b(b_prev, b);
      set_prev_b(b_next, b);
      set_next_b(b, b_next);
      set_prev_b(b, b_prev);
   }
#  ifdef DEBUG_MALLOC
   (void)blockSane(a,b);
#  endif
}

static
void mkInuseBlock ( Arena* a, Block* b, SizeT bszB )
{
   UInt i;
   vg_assert(bszB >= min_useful_bszB(a));
   INNER_REQUEST(VALGRIND_MAKE_MEM_UNDEFINED(b, bszB));
   set_bszB(b, mk_inuse_bszB(bszB));
   set_prev_b(b, NULL);    
   set_next_b(b, NULL);    
   if (!a->clientmem) {
      for (i = 0; i < a->rz_szB; i++) {
         set_rz_lo_byte(b, i, (UByte)(((Addr)b&0xff) ^ REDZONE_LO_MASK));
         set_rz_hi_byte(b, i, (UByte)(((Addr)b&0xff) ^ REDZONE_HI_MASK));
      }
   }
#  ifdef DEBUG_MALLOC
   (void)blockSane(a,b);
#  endif
}

static
void shrinkInuseBlock ( Arena* a, Block* b, SizeT bszB )
{
   UInt i;

   vg_assert(bszB >= min_useful_bszB(a));
   INNER_REQUEST(mkBhdrAccess(a,b));
   set_bszB(b, mk_inuse_bszB(bszB));
   if (!a->clientmem) {
      for (i = 0; i < a->rz_szB; i++) {
         set_rz_lo_byte(b, i, (UByte)(((Addr)b&0xff) ^ REDZONE_LO_MASK));
         set_rz_hi_byte(b, i, (UByte)(((Addr)b&0xff) ^ REDZONE_HI_MASK));
      }
   }
   INNER_REQUEST(mkBhdrNoAccess(a,b));
   
#  ifdef DEBUG_MALLOC
   (void)blockSane(a,b);
#  endif
}

static
void unlinkBlock ( Arena* a, Block* b, UInt listno )
{
   vg_assert(listno < N_MALLOC_LISTS);
   if (get_prev_b(b) == b) {
      
      vg_assert(get_next_b(b) == b);
      a->freelist[listno] = NULL;
   } else {
      Block* b_prev = get_prev_b(b);
      Block* b_next = get_next_b(b);
      a->freelist[listno] = b_prev;
      set_next_b(b_prev, b_next);
      set_prev_b(b_next, b_prev);
      swizzle ( a, listno );
   }
   set_prev_b(b, NULL);
   set_next_b(b, NULL);
}



static __inline__
SizeT align_req_pszB ( SizeT req_pszB )
{
   SizeT n = VG_MIN_MALLOC_SZB-1;
   return ((req_pszB + n) & (~n));
}

static
void add_one_block_to_stats (Arena* a, SizeT loaned)
{
   a->stats__bytes_on_loan += loaned;
   if (a->stats__bytes_on_loan > a->stats__bytes_on_loan_max) {
      a->stats__bytes_on_loan_max = a->stats__bytes_on_loan;
      if (a->stats__bytes_on_loan_max >= a->next_profile_at) {
         
         a->next_profile_at 
            = (SizeT)( 
                 (((ULong)a->stats__bytes_on_loan_max) * 105ULL) / 100ULL );
         if (VG_(clo_profile_heap))
            cc_analyse_alloc_arena(arenaP_to_ArenaId (a));
      }
   }
   a->stats__tot_blocks += (ULong)1;
   a->stats__tot_bytes  += (ULong)loaned;
}

void* VG_(arena_malloc) ( ArenaId aid, const HChar* cc, SizeT req_pszB )
{
   SizeT       req_bszB, frag_bszB, b_bszB;
   UInt        lno, i;
   Superblock* new_sb = NULL;
   Block*      b = NULL;
   Arena*      a;
   void*       v;
   UWord       stats__nsearches = 0;

   ensure_mm_init(aid);
   a = arenaId_to_ArenaP(aid);

   vg_assert(req_pszB < MAX_PSZB);
   req_pszB = align_req_pszB(req_pszB);
   req_bszB = pszB_to_bszB(a, req_pszB);

   
   
   vg_assert(cc);

   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   for (lno = pszB_to_listNo(req_pszB); lno < N_MALLOC_LISTS; lno++) {
      UWord nsearches_this_level = 0;
      b = a->freelist[lno];
      if (NULL == b) continue;   
      while (True) {
         stats__nsearches++;
         nsearches_this_level++;
         if (UNLIKELY(nsearches_this_level >= 100) 
             && lno < N_MALLOC_LISTS-1) {
            b = a->freelist[lno];
            vg_assert(b); 
            a->freelist[lno] = get_next_b(b); 
            break;
         }
         b_bszB = get_bszB(b);
         if (b_bszB >= req_bszB) goto obtained_block;    
         b = get_next_b(b);
         if (b == a->freelist[lno]) break;   
      }
   }

   
   vg_assert(lno == N_MALLOC_LISTS);
   new_sb = newSuperblock(a, req_bszB);
   if (NULL == new_sb) {
      
      
      vg_assert(VG_AR_CLIENT == aid);
      return NULL;
   }

   vg_assert(a->sblocks_used <= a->sblocks_size);
   if (a->sblocks_used == a->sblocks_size) {
      Superblock ** array;
      SysRes sres = VG_(am_mmap_anon_float_valgrind)(sizeof(Superblock *) *
                                                     a->sblocks_size * 2);
      if (sr_isError(sres)) {
         VG_(out_of_memory_NORETURN)("arena_init", sizeof(Superblock *) * 
                                                   a->sblocks_size * 2);
         
      }
      array = (Superblock**)(Addr)sr_Res(sres);
      for (i = 0; i < a->sblocks_used; ++i) array[i] = a->sblocks[i];

      a->sblocks_size *= 2;
      a->sblocks = array;
      VG_(debugLog)(1, "mallocfree", 
                       "sblock array for arena `%s' resized to %ld\n", 
                       a->name, a->sblocks_size);
   }

   vg_assert(a->sblocks_used < a->sblocks_size);
   
   i = a->sblocks_used;
   while (i > 0) {
      if (a->sblocks[i-1] > new_sb) {
         a->sblocks[i] = a->sblocks[i-1];
      } else {
         break;
      }
      --i;
   }   
   a->sblocks[i] = new_sb;
   a->sblocks_used++;

   b = (Block*)&new_sb->payload_bytes[0];
   lno = pszB_to_listNo(bszB_to_pszB(a, new_sb->n_payload_bytes));
   mkFreeBlock ( a, b, new_sb->n_payload_bytes, lno);
   if (VG_(clo_profile_heap))
      set_cc(b, "admin.free-new-sb-1");
   

  obtained_block:
   
   vg_assert(b != NULL);
   vg_assert(lno < N_MALLOC_LISTS);
   vg_assert(a->freelist[lno] != NULL);
   b_bszB = get_bszB(b);
   
   
   vg_assert(b_bszB >= req_bszB);

   
   
   frag_bszB = b_bszB - req_bszB;
   if (frag_bszB >= min_useful_bszB(a)
       && (NULL == new_sb || ! new_sb->unsplittable)) {
      
      
      
      unlinkBlock(a, b, lno);
      mkInuseBlock(a, b, req_bszB);
      if (VG_(clo_profile_heap))
         set_cc(b, cc);
      mkFreeBlock(a, &b[req_bszB], frag_bszB, 
                     pszB_to_listNo(bszB_to_pszB(a, frag_bszB)));
      if (VG_(clo_profile_heap))
         set_cc(&b[req_bszB], "admin.fragmentation-1");
      b_bszB = get_bszB(b);
   } else {
      
      unlinkBlock(a, b, lno);
      mkInuseBlock(a, b, b_bszB);
      if (VG_(clo_profile_heap))
         set_cc(b, cc);
   }

   
   SizeT loaned = bszB_to_pszB(a, b_bszB);
   add_one_block_to_stats (a, loaned);
   a->stats__nsearches  += (ULong)stats__nsearches;

#  ifdef DEBUG_MALLOC
   sanity_check_malloc_arena(aid);
#  endif

   v = get_block_payload(a, b);
   vg_assert( (((Addr)v) & (VG_MIN_MALLOC_SZB-1)) == 0 );

   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   INNER_REQUEST
      (VALGRIND_MALLOCLIKE_BLOCK(v, 
                                 VG_(arena_malloc_usable_size)(aid, v), 
                                 a->rz_szB, False));

   if (0 && aid != VG_AR_CLIENT)
      VG_(memset)(v, 0xAA, (SizeT)req_pszB);

   return v;
}

static
void deferred_reclaimSuperblock ( Arena* a, Superblock* sb)
{
   
   if (sb == NULL) {
      if (!a->deferred_reclaimed_sb)
         
         
         return;

      VG_(debugLog)(1, "mallocfree",
                    "deferred_reclaimSuperblock NULL "
                    "(prev %p) owner %s/%s\n",
                    a->deferred_reclaimed_sb,
                    a->clientmem ? "CLIENT" : "VALGRIND", a->name );
   } else
      VG_(debugLog)(1, "mallocfree",
                    "deferred_reclaimSuperblock at %p (pszB %7ld) %s "
                    "(prev %p) owner %s/%s\n",
                    sb, sb->n_payload_bytes,
                    (sb->unsplittable ? "unsplittable" : ""),
                    a->deferred_reclaimed_sb,
                    a->clientmem ? "CLIENT" : "VALGRIND", a->name );

   if (a->deferred_reclaimed_sb && a->deferred_reclaimed_sb != sb) {
      
      
      
      
      
      
      UByte*      def_sb_start;
      UByte*      def_sb_end;
      Superblock* def_sb;
      Block*      b;

      def_sb = a->deferred_reclaimed_sb;
      def_sb_start = &def_sb->payload_bytes[0];
      def_sb_end   = &def_sb->payload_bytes[def_sb->n_payload_bytes - 1];
      b = (Block *)def_sb_start;
      vg_assert (blockSane(a, b));

      
      
      if (!is_inuse_block(b)) {
         
         UInt        b_listno;
         SizeT       b_bszB, b_pszB;
         b_bszB   = get_bszB(b);
         b_pszB   = bszB_to_pszB(a, b_bszB);
         if (b + b_bszB-1 == (Block*)def_sb_end) {
            
            
            
            b_listno = pszB_to_listNo(b_pszB);
            unlinkBlock( a, b, b_listno );
            reclaimSuperblock (a, def_sb);
            a->deferred_reclaimed_sb = NULL;
         }
      }
   }

   
   a->deferred_reclaimed_sb = sb;
}

static void mergeWithFreeNeighbours (Arena* a, Superblock* sb,
                                     Block* b, SizeT b_bszB)
{
   UByte*      sb_start;
   UByte*      sb_end;
   Block*      other_b;
   SizeT       other_bszB;
   UInt        b_listno;

   sb_start = &sb->payload_bytes[0];
   sb_end   = &sb->payload_bytes[sb->n_payload_bytes - 1];

   b_listno = pszB_to_listNo(bszB_to_pszB(a, b_bszB));

   
   
   
   other_b = b + b_bszB;
   if (other_b+min_useful_bszB(a)-1 <= (Block*)sb_end) {
      
      other_bszB = get_bszB(other_b);
      if (!is_inuse_block(other_b)) {
         
#        ifdef DEBUG_MALLOC
         vg_assert(blockSane(a, other_b));
#        endif
         unlinkBlock( a, b, b_listno );
         unlinkBlock( a, other_b,
                      pszB_to_listNo(bszB_to_pszB(a,other_bszB)) );
         b_bszB += other_bszB;
         b_listno = pszB_to_listNo(bszB_to_pszB(a, b_bszB));
         mkFreeBlock( a, b, b_bszB, b_listno );
         if (VG_(clo_profile_heap))
            set_cc(b, "admin.free-2");
      }
   } else {
      
      
      vg_assert(other_b-1 == (Block*)sb_end);
   }

   
   
   
   if (b >= (Block*)sb_start + min_useful_bszB(a)) {
      
      other_b = get_predecessor_block( b );
      other_bszB = get_bszB(other_b);
      if (!is_inuse_block(other_b)) {
         
         unlinkBlock( a, b, b_listno );
         unlinkBlock( a, other_b,
                      pszB_to_listNo(bszB_to_pszB(a, other_bszB)) );
         b = other_b;
         b_bszB += other_bszB;
         b_listno = pszB_to_listNo(bszB_to_pszB(a, b_bszB));
         mkFreeBlock( a, b, b_bszB, b_listno );
         if (VG_(clo_profile_heap))
            set_cc(b, "admin.free-3");
      }
   } else {
      
      
      vg_assert((Block*)sb_start == b);
   }

   if ( ((Block*)sb_start == b) && (b + b_bszB-1 == (Block*)sb_end) ) {
      deferred_reclaimSuperblock (a, sb);
   }
}
 
void VG_(arena_free) ( ArenaId aid, void* ptr )
{
   Superblock* sb;
   Block*      b;
   SizeT       b_bszB, b_pszB;
   UInt        b_listno;
   Arena*      a;

   ensure_mm_init(aid);
   a = arenaId_to_ArenaP(aid);

   if (ptr == NULL) {
      return;
   }
      
   b = get_payload_block(a, ptr);

   if (aid != VG_AR_CLIENT)
      vg_assert(blockSane(a, b));

   b_bszB   = get_bszB(b);
   b_pszB   = bszB_to_pszB(a, b_bszB);
   sb       = findSb( a, b );

   a->stats__bytes_on_loan -= b_pszB;

   if (aid != VG_AR_CLIENT)
      VG_(memset)(ptr, 0xDD, (SizeT)b_pszB);

   if (! sb->unsplittable) {
      
      b_listno = pszB_to_listNo(b_pszB);
      mkFreeBlock( a, b, b_bszB, b_listno );
      if (VG_(clo_profile_heap))
         set_cc(b, "admin.free-1");

      
      mergeWithFreeNeighbours (a, sb, b, b_bszB);

      
      
      INNER_REQUEST(VALGRIND_FREELIKE_BLOCK(ptr, 0));
      
      
      
      
      
      
      
      
      
      
      

      
      INNER_REQUEST(VALGRIND_MAKE_MEM_NOACCESS(b, b_bszB));
      
      
      
      INNER_REQUEST(VALGRIND_MAKE_MEM_DEFINED(b + hp_overhead_szB(),
                                              sizeof(SizeT) + sizeof(void*)));
      INNER_REQUEST(VALGRIND_MAKE_MEM_DEFINED(b + b_bszB
                                              - sizeof(SizeT) - sizeof(void*),
                                              sizeof(SizeT) + sizeof(void*)));
   } else {
      vg_assert(unsplittableBlockSane(a, sb, b));

      
      
      
      
      INNER_REQUEST(VALGRIND_FREELIKE_BLOCK(ptr, 0));

      
      reclaimSuperblock (a, sb);
   }

#  ifdef DEBUG_MALLOC
   sanity_check_malloc_arena(aid);
#  endif

}


void* VG_(arena_memalign) ( ArenaId aid, const HChar* cc, 
                            SizeT req_alignB, SizeT req_pszB )
{
   SizeT  base_pszB_req, base_pszB_act, frag_bszB;
   Block  *base_b, *align_b;
   UByte  *base_p, *align_p;
   SizeT  saved_bytes_on_loan;
   Arena* a;

   ensure_mm_init(aid);
   a = arenaId_to_ArenaP(aid);

   vg_assert(req_pszB < MAX_PSZB);

   
   
   vg_assert(cc);

   
   
   
   if (req_alignB < VG_MIN_MALLOC_SZB
       || req_alignB > 16 * 1024 * 1024
       || VG_(log2)( req_alignB ) == -1 ) {
      VG_(printf)("VG_(arena_memalign)(%p, %lu, %lu)\n"
                  "bad alignment value %lu\n"
                  "(it is too small, too big, or not a power of two)",
                  a, req_alignB, req_pszB, req_alignB );
      VG_(core_panic)("VG_(arena_memalign)");
      
   }
   
   vg_assert(req_alignB % VG_MIN_MALLOC_SZB == 0);

   
   req_pszB = align_req_pszB(req_pszB);
   
   
   base_pszB_req = req_pszB + min_useful_bszB(a) + req_alignB;

   saved_bytes_on_loan = a->stats__bytes_on_loan;
   {
      const SizeT save_min_unsplittable_sblock_szB 
         = a->min_unsplittable_sblock_szB;
      a->min_unsplittable_sblock_szB = MAX_PSZB;
      base_p = VG_(arena_malloc) ( aid, cc, base_pszB_req );
      a->min_unsplittable_sblock_szB = save_min_unsplittable_sblock_szB;
   }
   a->stats__bytes_on_loan = saved_bytes_on_loan;

   
   if (base_p == 0)
      return 0;
   INNER_REQUEST(VALGRIND_FREELIKE_BLOCK(base_p, a->rz_szB));
   INNER_REQUEST(VALGRIND_MAKE_MEM_UNDEFINED(base_p, base_pszB_req));

   
   base_b = get_payload_block ( a, base_p );

   align_p = align_upwards ( base_b + 2 * overhead_szB_lo(a)
                                    + overhead_szB_hi(a),
                             req_alignB );
   align_b = get_payload_block(a, align_p);

   frag_bszB = align_b - base_b;

   vg_assert(frag_bszB >= min_useful_bszB(a));

   
   base_pszB_act = get_pszB(a, base_b);

   
   mkFreeBlock ( a, base_b, frag_bszB,
                 pszB_to_listNo(bszB_to_pszB(a, frag_bszB)) );
   if (VG_(clo_profile_heap))
      set_cc(base_b, "admin.frag-memalign-1");

   
   mkInuseBlock ( a, align_b,
                  base_p + base_pszB_act 
                         + overhead_szB_hi(a) - (UByte*)align_b );
   if (VG_(clo_profile_heap))
      set_cc(align_b, cc);

   
   vg_assert( is_inuse_block(get_payload_block(a, align_p)) );

   vg_assert(req_pszB <= get_pszB(a, get_payload_block(a, align_p)));

   a->stats__bytes_on_loan += get_pszB(a, get_payload_block(a, align_p));
   if (a->stats__bytes_on_loan > a->stats__bytes_on_loan_max) {
      a->stats__bytes_on_loan_max = a->stats__bytes_on_loan;
   }

#  ifdef DEBUG_MALLOC
   sanity_check_malloc_arena(aid);
#  endif

   vg_assert( (((Addr)align_p) % req_alignB) == 0 );

   INNER_REQUEST(VALGRIND_MALLOCLIKE_BLOCK(align_p,
                                           req_pszB, a->rz_szB, False));

   return align_p;
}


SizeT VG_(arena_malloc_usable_size) ( ArenaId aid, void* ptr )
{
   Arena* a = arenaId_to_ArenaP(aid);
   Block* b = get_payload_block(a, ptr);
   return get_pszB(a, b);
}


void VG_(mallinfo) ( ThreadId tid, struct vg_mallinfo* mi )
{
   UWord  i, free_blocks, free_blocks_size;
   Arena* a = arenaId_to_ArenaP(VG_AR_CLIENT);

   
   
   free_blocks_size = free_blocks = 0;
   for (i = 0; i < N_MALLOC_LISTS; i++) {
      Block* b = a->freelist[i];
      if (b == NULL) continue;
      for (;;) {
         free_blocks++;
         free_blocks_size += (UWord)get_pszB(a, b);
         b = get_next_b(b);
         if (b == a->freelist[i]) break;
      }
   }

   
   
   mi->arena    = a->stats__bytes_mmaped;
   mi->ordblks  = free_blocks + VG_(free_queue_length);
   mi->smblks   = 0;
   mi->hblks    = 0;
   mi->hblkhd   = 0;
   mi->usmblks  = 0;
   mi->fsmblks  = 0;
   mi->uordblks = a->stats__bytes_on_loan - VG_(free_queue_volume);
   mi->fordblks = free_blocks_size + VG_(free_queue_volume);
   mi->keepcost = 0; 
}

SizeT VG_(arena_redzone_size) ( ArenaId aid )
{
   ensure_mm_init (VG_AR_CLIENT);
   return arenaId_to_ArenaP(aid)->rz_szB;
}


void* VG_(arena_calloc) ( ArenaId aid, const HChar* cc,
                          SizeT nmemb, SizeT bytes_per_memb )
{
   SizeT  size;
   void*  p;

   size = nmemb * bytes_per_memb;
   vg_assert(size >= nmemb && size >= bytes_per_memb);

   p = VG_(arena_malloc) ( aid, cc, size );

   if (p != NULL)
     VG_(memset)(p, 0, size);

   return p;
}


void* VG_(arena_realloc) ( ArenaId aid, const HChar* cc, 
                           void* ptr, SizeT req_pszB )
{
   Arena* a;
   SizeT  old_pszB;
   void*  p_new;
   Block* b;

   ensure_mm_init(aid);
   a = arenaId_to_ArenaP(aid);

   vg_assert(req_pszB < MAX_PSZB);

   if (NULL == ptr) {
      return VG_(arena_malloc)(aid, cc, req_pszB);
   }

   if (req_pszB == 0) {
      VG_(arena_free)(aid, ptr);
      return NULL;
   }

   b = get_payload_block(a, ptr);
   vg_assert(blockSane(a, b));

   vg_assert(is_inuse_block(b));
   old_pszB = get_pszB(a, b);

   if (req_pszB <= old_pszB) {
      return ptr;
   }

   p_new = VG_(arena_malloc) ( aid, cc, req_pszB );
      
   VG_(memcpy)(p_new, ptr, old_pszB);

   VG_(arena_free)(aid, ptr);

   return p_new;
}


void VG_(arena_realloc_shrink) ( ArenaId aid,
                                 void* ptr, SizeT req_pszB )
{
   SizeT  req_bszB, frag_bszB, b_bszB;
   Superblock* sb;
   Arena* a;
   SizeT  old_pszB;
   Block* b;

   ensure_mm_init(aid);

   a = arenaId_to_ArenaP(aid);
   b = get_payload_block(a, ptr);
   vg_assert(blockSane(a, b));
   vg_assert(is_inuse_block(b));

   old_pszB = get_pszB(a, b);
   req_pszB = align_req_pszB(req_pszB);
   vg_assert(old_pszB >= req_pszB);
   if (old_pszB == req_pszB)
      return;

   sb = findSb( a, b );
   if (sb->unsplittable) {
      const UByte* sb_start = &sb->payload_bytes[0];
      const UByte* sb_end = &sb->payload_bytes[sb->n_payload_bytes - 1];
      Addr  frag;

      vg_assert(unsplittableBlockSane(a, sb, b));

      frag = VG_PGROUNDUP((Addr) sb 
                          + sizeof(Superblock) + pszB_to_bszB(a, req_pszB));
      frag_bszB = (Addr)sb_end - frag + 1;
      
      if (frag_bszB >= VKI_PAGE_SIZE) {
         SysRes sres;
         
         a->stats__bytes_on_loan -= old_pszB;
         b_bszB = (UByte*)frag - sb_start;
         shrinkInuseBlock(a, b, b_bszB);
         INNER_REQUEST
            (VALGRIND_RESIZEINPLACE_BLOCK(ptr,
                                          old_pszB,
                                          VG_(arena_malloc_usable_size)(aid, ptr),
                                          a->rz_szB));
         
         INNER_REQUEST(mkBhdrSzAccess(a, b));
         a->stats__bytes_on_loan += bszB_to_pszB(a, b_bszB);

         sb->n_payload_bytes -= frag_bszB;
         VG_(debugLog)(1, "mallocfree",
                       "shrink superblock %p to (pszB %7ld) "
                       "owner %s/%s (munmap-ing %p %7ld)\n",
                       sb, sb->n_payload_bytes,
                       a->clientmem ? "CLIENT" : "VALGRIND", a->name,
                       (void*) frag, frag_bszB);
         if (a->clientmem) {
            Bool need_discard = False;
            sres = VG_(am_munmap_client)(&need_discard,
                                         frag,
                                         frag_bszB);
            vg_assert (!need_discard);
         } else {
            sres = VG_(am_munmap_valgrind)(frag,
                                           frag_bszB);
         }
         vg_assert2(! sr_isError(sres), "shrink superblock munmap failure\n");
         a->stats__bytes_mmaped -= frag_bszB;

         vg_assert(unsplittableBlockSane(a, sb, b));
      }
   } else {
      req_bszB = pszB_to_bszB(a, req_pszB);
      b_bszB = get_bszB(b);
      frag_bszB = b_bszB - req_bszB;
      if (frag_bszB < min_useful_bszB(a))
         return;
      
      a->stats__bytes_on_loan -= old_pszB;
      shrinkInuseBlock(a, b, req_bszB);
      INNER_REQUEST
         (VALGRIND_RESIZEINPLACE_BLOCK(ptr,
                                       old_pszB,
                                       VG_(arena_malloc_usable_size)(aid, ptr),
                                       a->rz_szB));
      
      INNER_REQUEST(mkBhdrSzAccess(a, b));

      mkFreeBlock(a, &b[req_bszB], frag_bszB,
                  pszB_to_listNo(bszB_to_pszB(a, frag_bszB)));
      
      INNER_REQUEST(mkBhdrAccess(a, &b[req_bszB]));
      if (VG_(clo_profile_heap))
         set_cc(&b[req_bszB], "admin.fragmentation-2");
      
      mergeWithFreeNeighbours(a, sb, &b[req_bszB], frag_bszB);
      
      b_bszB = get_bszB(b);
      a->stats__bytes_on_loan += bszB_to_pszB(a, b_bszB);
   }

   vg_assert (blockSane(a, b));
#  ifdef DEBUG_MALLOC
   sanity_check_malloc_arena(aid);
#  endif
}

__inline__ HChar* VG_(arena_strdup) ( ArenaId aid, const HChar* cc, 
                                      const HChar* s )
{
   Int   i;
   Int   len;
   HChar* res;

   if (s == NULL)
      return NULL;

   len = VG_(strlen)(s) + 1;
   res = VG_(arena_malloc) (aid, cc, len);

   for (i = 0; i < len; i++)
      res[i] = s[i];
   return res;
}

void* VG_(arena_perm_malloc) ( ArenaId aid, SizeT size, Int align  )
{
   Arena*      a;

   ensure_mm_init(aid);
   a = arenaId_to_ArenaP(aid);

   align = align - 1;
   size = (size + align) & ~align;

   if (UNLIKELY(a->perm_malloc_current + size > a->perm_malloc_limit)) {
      
      
      
      
      Superblock* new_sb = newSuperblock (a, size);
      a->perm_malloc_limit = (Addr)&new_sb->payload_bytes[new_sb->n_payload_bytes - 1];

      
      
      a->perm_malloc_current = (Addr)new_sb;
   }

   a->stats__perm_blocks += 1;
   a->stats__perm_bytes_on_loan  += size;
   add_one_block_to_stats (a, size);

   a->perm_malloc_current        += size;
   return (void*)(a->perm_malloc_current - size);
}



void* VG_(malloc) ( const HChar* cc, SizeT nbytes )
{
   return VG_(arena_malloc) ( VG_AR_CORE, cc, nbytes );
}

void  VG_(free) ( void* ptr )
{
   VG_(arena_free) ( VG_AR_CORE, ptr );
}

void* VG_(calloc) ( const HChar* cc, SizeT nmemb, SizeT bytes_per_memb )
{
   return VG_(arena_calloc) ( VG_AR_CORE, cc, nmemb, bytes_per_memb );
}

void* VG_(realloc) ( const HChar* cc, void* ptr, SizeT size )
{
   return VG_(arena_realloc) ( VG_AR_CORE, cc, ptr, size );
}

void VG_(realloc_shrink) ( void* ptr, SizeT size )
{
   VG_(arena_realloc_shrink) ( VG_AR_CORE, ptr, size );
}

HChar* VG_(strdup) ( const HChar* cc, const HChar* s )
{
   return VG_(arena_strdup) ( VG_AR_CORE, cc, s ); 
}

void* VG_(perm_malloc) ( SizeT size, Int align  )
{
   return VG_(arena_perm_malloc) ( VG_AR_CORE, size, align );
}


