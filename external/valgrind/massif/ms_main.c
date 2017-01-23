
/*
   This file is part of Massif, a Valgrind tool for profiling memory
   usage of programs.

   Copyright (C) 2003-2013 Nicholas Nethercote
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


#if 0
desc: --heap-admin=foo
cmd: date
time_unit: ms
#-----------
snapshot=0
#-----------
time=0
mem_heap_B=0
mem_heap_admin_B=0
mem_stacks_B=0
heap_tree=empty
#-----------
snapshot=1
#-----------
time=353
mem_heap_B=5
mem_heap_admin_B=0
mem_stacks_B=0
heap_tree=detailed
n1: 5 (heap allocation functions) malloc/new/new[], --alloc-fns, etc.
 n1: 5 0x27F6E0: _nl_normalize_codeset (in /lib/libc-2.3.5.so)
  n1: 5 0x279DE6: _nl_load_locale_from_archive (in /lib/libc-2.3.5.so)
   n1: 5 0x278E97: _nl_find_locale (in /lib/libc-2.3.5.so)
    n1: 5 0x278871: setlocale (in /lib/libc-2.3.5.so)
     n1: 5 0x8049821: (within /bin/date)
      n0: 5 0x26ED5E: (below main) (in /lib/libc-2.3.5.so)


n_events: n  time(ms)  total(B)    useful-heap(B)  admin-heap(B)  stacks(B)
t_events: B
n 0 0 0 0 0 
n 0 0 0 0 0
t1: 5 <string...>
 t1: 6 <string...>

Ideas:
- each snapshot specifies an x-axis value and one or more y-axis values.
- can display the y-axis values separately if you like
- can completely separate connection between snapshots and trees.

Challenges:
- how to specify and scale/abbreviate units on axes?
- how to combine multiple values into the y-axis?

--------------------------------------------------------------------------------Command:            date
Massif arguments:   --heap-admin=foo
ms_print arguments: massif.out
--------------------------------------------------------------------------------
    KB
6.472^                                                       :#
     |                                                       :#  ::  .    .
     ...
     |                                     ::@  :@    :@ :@:::#  ::  :    ::::
   0 +-----------------------------------@---@---@-----@--@---#-------------->ms     0                                                                     713

Number of snapshots: 50
 Detailed snapshots: [2, 11, 13, 19, 25, 32 (peak)]
--------------------------------------------------------------------------------  n       time(ms)         total(B)   useful-heap(B) admin-heap(B)    stacks(B)
--------------------------------------------------------------------------------  0              0                0                0             0            0
  1            345                5                5             0            0
  2            353                5                5             0            0
100.00% (5B) (heap allocation functions) malloc/new/new[], --alloc-fns, etc.
->100.00% (5B) 0x27F6E0: _nl_normalize_codeset (in /lib/libc-2.3.5.so)
#endif


#include "pub_tool_basics.h"
#include "pub_tool_vki.h"
#include "pub_tool_aspacemgr.h"
#include "pub_tool_debuginfo.h"
#include "pub_tool_hashtable.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_libcfile.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_libcproc.h"
#include "pub_tool_machine.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_options.h"
#include "pub_tool_replacemalloc.h"
#include "pub_tool_stacktrace.h"
#include "pub_tool_threadstate.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_xarray.h"
#include "pub_tool_clientstate.h"
#include "pub_tool_gdbserver.h"

#include "pub_tool_clreq.h"           



#define VERB(verb, format, args...) \
   if (VG_(clo_verbosity) > verb) { \
      VG_(dmsg)("Massif: " format, ##args); \
   }



static UInt n_heap_allocs           = 0;
static UInt n_heap_reallocs         = 0;
static UInt n_heap_frees            = 0;
static UInt n_ignored_heap_allocs   = 0;
static UInt n_ignored_heap_frees    = 0;
static UInt n_ignored_heap_reallocs = 0;
static UInt n_stack_allocs          = 0;
static UInt n_stack_frees           = 0;
static UInt n_xpts                  = 0;
static UInt n_xpt_init_expansions   = 0;
static UInt n_xpt_later_expansions  = 0;
static UInt n_sxpt_allocs           = 0;
static UInt n_sxpt_frees            = 0;
static UInt n_skipped_snapshots     = 0;
static UInt n_real_snapshots        = 0;
static UInt n_detailed_snapshots    = 0;
static UInt n_peak_snapshots        = 0;
static UInt n_cullings              = 0;
static UInt n_XCon_redos            = 0;


static Long guest_instrs_executed = 0;

static SizeT heap_szB       = 0; 
static SizeT heap_extra_szB = 0; 
static SizeT stacks_szB     = 0; 

static SizeT peak_snapshot_total_szB = 0;

static ULong total_allocs_deallocs_szB = 0;

static Bool have_started_executing_code = False;


static XArray* alloc_fns;
static XArray* ignore_fns;

static void init_alloc_fns(void)
{
   
   alloc_fns = VG_(newXA)(VG_(malloc), "ms.main.iaf.1",
                                       VG_(free), sizeof(HChar*));
   #define DO(x)  { const HChar* s = x; VG_(addToXA)(alloc_fns, &s); }

   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   DO("malloc"                                              );
   DO("__builtin_new"                                       );
   DO("operator new(unsigned)"                              );
   DO("operator new(unsigned long)"                         );
   DO("__builtin_vec_new"                                   );
   DO("operator new[](unsigned)"                            );
   DO("operator new[](unsigned long)"                       );
   DO("calloc"                                              );
   DO("realloc"                                             );
   DO("memalign"                                            );
   DO("posix_memalign"                                      );
   DO("valloc"                                              );
   DO("operator new(unsigned, std::nothrow_t const&)"       );
   DO("operator new[](unsigned, std::nothrow_t const&)"     );
   DO("operator new(unsigned long, std::nothrow_t const&)"  );
   DO("operator new[](unsigned long, std::nothrow_t const&)");
#if defined(VGO_darwin)
   DO("malloc_zone_malloc"                                  );
   DO("malloc_zone_calloc"                                  );
   DO("malloc_zone_realloc"                                 );
   DO("malloc_zone_memalign"                                );
   DO("malloc_zone_valloc"                                  );
#endif
}

static void init_ignore_fns(void)
{
   
   ignore_fns = VG_(newXA)(VG_(malloc), "ms.main.iif.1",
                                        VG_(free), sizeof(HChar*));
}

static Bool is_member_fn(const XArray* fns, const HChar* fnname)
{
   HChar** fn_ptr;
   Int i;
 
   
   
   
   
   for (i = 0; i < VG_(sizeXA)(fns); i++) {
      fn_ptr = VG_(indexXA)(fns, i);
      if (VG_STREQ(fnname, *fn_ptr))
         return True;
   }
   return False;
}



#define MAX_DEPTH       200

typedef enum { TimeI, TimeMS, TimeB } TimeUnit;

static const HChar* TimeUnit_to_string(TimeUnit time_unit)
{
   switch (time_unit) {
   case TimeI:  return "i";
   case TimeMS: return "ms";
   case TimeB:  return "B";
   default:     tl_assert2(0, "TimeUnit_to_string: unrecognised TimeUnit");
   }
}

static Bool   clo_heap            = True;
   
   
   
   
static SSizeT clo_heap_admin      = 8;
static Bool   clo_pages_as_heap   = False;
static Bool   clo_stacks          = False;
static Int    clo_depth           = 30;
static double clo_threshold       = 1.0;  
static double clo_peak_inaccuracy = 1.0;  
static Int    clo_time_unit       = TimeI;
static Int    clo_detailed_freq   = 10;
static Int    clo_max_snapshots   = 100;
static const HChar* clo_massif_out_file = "massif.out.%p";

static XArray* args_for_massif;

static Bool ms_process_cmd_line_option(const HChar* arg)
{
   const HChar* tmp_str;

   
   VG_(addToXA)(args_for_massif, &arg);

        if VG_BOOL_CLO(arg, "--heap",           clo_heap)   {}
   else if VG_BINT_CLO(arg, "--heap-admin",     clo_heap_admin, 0, 1024) {}

   else if VG_BOOL_CLO(arg, "--stacks",         clo_stacks) {}

   else if VG_BOOL_CLO(arg, "--pages-as-heap",  clo_pages_as_heap) {}

   else if VG_BINT_CLO(arg, "--depth",          clo_depth, 1, MAX_DEPTH) {}

   else if VG_STR_CLO(arg, "--alloc-fn",        tmp_str) {
      VG_(addToXA)(alloc_fns, &tmp_str);
   }
   else if VG_STR_CLO(arg, "--ignore-fn",       tmp_str) {
      VG_(addToXA)(ignore_fns, &tmp_str);
   }

   else if VG_DBL_CLO(arg, "--threshold",  clo_threshold) {
      if (clo_threshold < 0 || clo_threshold > 100) {
         VG_(fmsg_bad_option)(arg,
            "--threshold must be between 0.0 and 100.0\n");
      }
   }

   else if VG_DBL_CLO(arg, "--peak-inaccuracy", clo_peak_inaccuracy) {}

   else if VG_XACT_CLO(arg, "--time-unit=i",    clo_time_unit, TimeI)  {}
   else if VG_XACT_CLO(arg, "--time-unit=ms",   clo_time_unit, TimeMS) {}
   else if VG_XACT_CLO(arg, "--time-unit=B",    clo_time_unit, TimeB)  {}

   else if VG_BINT_CLO(arg, "--detailed-freq",  clo_detailed_freq, 1, 1000000) {}

   else if VG_BINT_CLO(arg, "--max-snapshots",  clo_max_snapshots, 10, 1000) {}

   else if VG_STR_CLO(arg, "--massif-out-file", clo_massif_out_file) {}

   else
      return VG_(replacement_malloc_process_cmd_line_option)(arg);

   return True;
}

static void ms_print_usage(void)
{
   VG_(printf)(
"    --heap=no|yes             profile heap blocks [yes]\n"
"    --heap-admin=<size>       average admin bytes per heap block;\n"
"                               ignored if --heap=no [8]\n"
"    --stacks=no|yes           profile stack(s) [no]\n"
"    --pages-as-heap=no|yes    profile memory at the page level [no]\n"
"    --depth=<number>          depth of contexts [30]\n"
"    --alloc-fn=<name>         specify <name> as an alloc function [empty]\n"
"    --ignore-fn=<name>        ignore heap allocations within <name> [empty]\n"
"    --threshold=<m.n>         significance threshold, as a percentage [1.0]\n"
"    --peak-inaccuracy=<m.n>   maximum peak inaccuracy, as a percentage [1.0]\n"
"    --time-unit=i|ms|B        time unit: instructions executed, milliseconds\n"
"                              or heap bytes alloc'd/dealloc'd [i]\n"
"    --detailed-freq=<N>       every Nth snapshot should be detailed [10]\n"
"    --max-snapshots=<N>       maximum number of snapshots recorded [100]\n"
"    --massif-out-file=<file>  output file name [massif.out.%%p]\n"
   );
}

static void ms_print_debug_usage(void)
{
   VG_(printf)(
"    (none)\n"
   );
}





typedef struct _XPt XPt;
struct _XPt {
   Addr  ip;              

   
   
   
   SizeT szB;

   XPt*  parent;           

   
   
   
   
   UInt  n_children;       
   UInt  max_children;     
   XPt** children;         
};

typedef
   enum {
      SigSXPt,
      InsigSXPt
   }
   SXPtTag;

typedef struct _SXPt SXPt;
struct _SXPt {
   SXPtTag tag;
   SizeT szB;              
   union {
      
      
      struct {
         Addr   ip;
         UInt   n_children;
         SXPt** children;
      } 
      Sig;

      
      
      struct {
         Int   n_xpts;     
      } 
      Insig;
   };
};

static XPt* alloc_xpt;

static XPt* new_XPt(Addr ip, XPt* parent)
{
   
   
   
   XPt* xpt    = VG_(perm_malloc)(sizeof(XPt), vg_alignof(XPt));
   xpt->ip     = ip;
   xpt->szB    = 0;
   xpt->parent = parent;

   
   
   
   xpt->n_children   = 0;
   xpt->max_children = 0;
   xpt->children     = NULL;

   
   n_xpts++;

   return xpt;
}

static void add_child_xpt(XPt* parent, XPt* child)
{
   
   tl_assert(parent->n_children <= parent->max_children);
   if (parent->n_children == parent->max_children) {
      if (0 == parent->max_children) {
         parent->max_children = 4;
         parent->children = VG_(malloc)( "ms.main.acx.1",
                                         parent->max_children * sizeof(XPt*) );
         n_xpt_init_expansions++;
      } else {
         parent->max_children *= 2;    
         parent->children = VG_(realloc)( "ms.main.acx.2",
                                          parent->children,
                                          parent->max_children * sizeof(XPt*) );
         n_xpt_later_expansions++;
      }
   }

   
   parent->children[ parent->n_children++ ] = child;
}

static Int SXPt_revcmp_szB(const void* n1, const void* n2)
{
   const SXPt* sxpt1 = *(const SXPt *const *)n1;
   const SXPt* sxpt2 = *(const SXPt *const *)n2;
   return ( sxpt1->szB < sxpt2->szB ?  1
          : sxpt1->szB > sxpt2->szB ? -1
          :                            0);
}


static SXPt* dup_XTree(XPt* xpt, SizeT total_szB)
{
   Int  i, n_sig_children, n_insig_children, n_child_sxpts;
   SizeT sig_child_threshold_szB;
   SXPt* sxpt;

   
   
   
   
   
   

   
   
   
   
   
   
   
   
   if (0 == total_szB && 0 != clo_threshold) {
      sig_child_threshold_szB = 1;
   } else {
      sig_child_threshold_szB = (SizeT)((total_szB * clo_threshold) / 100);
   }

   
   n_sig_children = 0;
   for (i = 0; i < xpt->n_children; i++) {
      if (xpt->children[i]->szB >= sig_child_threshold_szB) {
         n_sig_children++;
      }
   }
   n_insig_children = xpt->n_children - n_sig_children;
   n_child_sxpts = n_sig_children + ( n_insig_children > 0 ? 1 : 0 );

   
   sxpt                 = VG_(malloc)("ms.main.dX.1", sizeof(SXPt));
   n_sxpt_allocs++;
   sxpt->tag            = SigSXPt;
   sxpt->szB            = xpt->szB;
   sxpt->Sig.ip         = xpt->ip;
   sxpt->Sig.n_children = n_child_sxpts;

   
   if (n_child_sxpts > 0) {
      Int j;
      SizeT sig_children_szB = 0, insig_children_szB = 0;
      sxpt->Sig.children = VG_(malloc)("ms.main.dX.2", 
                                       n_child_sxpts * sizeof(SXPt*));

      
      
      j = 0;
      for (i = 0; i < xpt->n_children; i++) {
         if (xpt->children[i]->szB >= sig_child_threshold_szB) {
            sxpt->Sig.children[j++] = dup_XTree(xpt->children[i], total_szB);
            sig_children_szB   += xpt->children[i]->szB;
         } else {
            insig_children_szB += xpt->children[i]->szB;
         }
      }

      
      
      if (n_insig_children > 0) {
         
         
         SXPt* insig_sxpt = VG_(malloc)("ms.main.dX.3", sizeof(SXPt));
         n_sxpt_allocs++;
         insig_sxpt->tag = InsigSXPt;
         insig_sxpt->szB = insig_children_szB;
         insig_sxpt->Insig.n_xpts = n_insig_children;
         sxpt->Sig.children[n_sig_children] = insig_sxpt;
      }
   } else {
      sxpt->Sig.children = NULL;
   }

   return sxpt;
}

static void free_SXTree(SXPt* sxpt)
{
   Int  i;
   tl_assert(sxpt != NULL);

   switch (sxpt->tag) {
    case SigSXPt:
      
      for (i = 0; i < sxpt->Sig.n_children; i++) {
         free_SXTree(sxpt->Sig.children[i]);
         sxpt->Sig.children[i] = NULL;
      }
      VG_(free)(sxpt->Sig.children);  sxpt->Sig.children = NULL;
      break;

    case InsigSXPt:
      break;

    default: tl_assert2(0, "free_SXTree: unknown SXPt tag");
   }
   
   
   VG_(free)(sxpt);     sxpt = NULL;
   n_sxpt_frees++;
}

static void sanity_check_XTree(XPt* xpt, XPt* parent)
{
   tl_assert(xpt != NULL);

   
   tl_assert2(xpt->parent == parent,
      "xpt->parent = %p, parent = %p\n", xpt->parent, parent);

   
   tl_assert(xpt->n_children <= xpt->max_children);

   
   
}

static void sanity_check_SXTree(SXPt* sxpt)
{
   Int i;

   tl_assert(sxpt != NULL);

   
   
   switch (sxpt->tag) {
    case SigSXPt: {
      if (sxpt->Sig.n_children > 0) {
         for (i = 0; i < sxpt->Sig.n_children; i++) {
            sanity_check_SXTree(sxpt->Sig.children[i]);
         }
      }
      break;
    }
    case InsigSXPt:
      break;         

    default: tl_assert2(0, "sanity_check_SXTree: unknown SXPt tag");
   }
}



#define MAX_OVERESTIMATE   50
#define MAX_IPS            (MAX_DEPTH + MAX_OVERESTIMATE)

static Bool fn_should_be_ignored(Addr ip)
{
   const HChar *buf;
   return
      ( VG_(get_fnname)(ip, &buf) && is_member_fn(ignore_fns, buf)
      ? True : False );
}

static
Int get_IPs( ThreadId tid, Bool exclude_first_entry, Addr ips[])
{
   Int n_ips, i, n_alloc_fns_removed;
   Int overestimate;
   Bool redo;

   
   
   
   
   
   
   
   
   

   
   redo = True;      
   for (overestimate = 3; redo; overestimate += 6) {
      
      
      if (overestimate > MAX_OVERESTIMATE)
         VG_(tool_panic)("get_IPs: ips[] too small, inc. MAX_OVERESTIMATE?");

      
      n_ips = VG_(get_StackTrace)( tid, ips, clo_depth + overestimate,
                                   NULL,
                                   NULL,
                                   0 );
      tl_assert(n_ips > 0);

      
      if (n_ips < clo_depth + overestimate) { redo = False; }

      
      
      
      
      n_alloc_fns_removed = ( exclude_first_entry ? 1 : 0 );
      for (i = n_alloc_fns_removed; i < n_ips; i++) {
         const HChar *buf;
         if (VG_(get_fnname)(ips[i], &buf)) {
            if (is_member_fn(alloc_fns, buf)) {
               n_alloc_fns_removed++;
            } else {
               break;
            }
         }
      }
      
      n_ips -= n_alloc_fns_removed;
      for (i = 0; i < n_ips; i++) {
         ips[i] = ips[i + n_alloc_fns_removed];
      }

      
      if (n_ips >= clo_depth) {
         redo = False;
         n_ips = clo_depth;      
      }

      if (redo) {
         n_XCon_redos++;
      }
   }
   return n_ips;
}

static XPt* get_XCon( ThreadId tid, Bool exclude_first_entry )
{
   static Addr ips[MAX_IPS];
   Int i;
   XPt* xpt = alloc_xpt;

   
   Int n_ips = get_IPs(tid, exclude_first_entry, ips);

   
   
   if (n_ips > 0 && fn_should_be_ignored(ips[0])) {
      return NULL;
   }

   
   for (i = 0; i < n_ips; i++) {
      Addr ip = ips[i];
      Int ch;
      
      
      
      
      for (ch = 0; True; ch++) {
         if (ch == xpt->n_children) {
            
            
            XPt* new_child_xpt = new_XPt(ip, xpt);
            add_child_xpt(xpt, new_child_xpt);
            xpt = new_child_xpt;
            break;

         } else if (ip == xpt->children[ch]->ip) {
            
            xpt = xpt->children[ch];
            break;
         }
      }
   }

   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   if (0 != xpt->n_children) {
      static Int n_moans = 0;
      if (n_moans < 3) {
         VG_(umsg)(
            "Warning: Malformed stack trace detected.  In Massif's output,\n");
         VG_(umsg)(
            "         the size of an entry's child entries may not sum up\n");
         VG_(umsg)(
            "         to the entry's size as they normally do.\n");
         n_moans++;
         if (3 == n_moans)
            VG_(umsg)(
            "         (And Massif now won't warn about this again.)\n");
      }
   }
   return xpt;
}

static void update_XCon(XPt* xpt, SSizeT space_delta)
{
   tl_assert(clo_heap);
   tl_assert(NULL != xpt);

   if (0 == space_delta)
      return;

   while (xpt != alloc_xpt) {
      if (space_delta < 0) tl_assert(xpt->szB >= -space_delta);
      xpt->szB += space_delta;
      xpt = xpt->parent;
   }
   if (space_delta < 0) tl_assert(alloc_xpt->szB >= -space_delta);
   alloc_xpt->szB += space_delta;
}




typedef Long Time;

#define UNUSED_SNAPSHOT_TIME  -333  

typedef
   enum {
      Normal = 77,
      Peak,
      Unused
   }
   SnapshotKind;

typedef
   struct {
      SnapshotKind kind;
      Time  time;
      SizeT heap_szB;
      SizeT heap_extra_szB;
      SizeT stacks_szB;
      SXPt* alloc_sxpt;    
   }                       
   Snapshot;

static UInt      next_snapshot_i = 0;  
static Snapshot* snapshots;            

static Bool is_snapshot_in_use(Snapshot* snapshot)
{
   if (Unused == snapshot->kind) {
      
      tl_assert(snapshot->time           == UNUSED_SNAPSHOT_TIME);
      tl_assert(snapshot->heap_extra_szB == 0);
      tl_assert(snapshot->heap_szB       == 0);
      tl_assert(snapshot->stacks_szB     == 0);
      tl_assert(snapshot->alloc_sxpt     == NULL);
      return False;
   } else {
      tl_assert(snapshot->time           != UNUSED_SNAPSHOT_TIME);
      return True;
   }
}

static Bool is_detailed_snapshot(Snapshot* snapshot)
{
   return (snapshot->alloc_sxpt ? True : False);
}

static Bool is_uncullable_snapshot(Snapshot* snapshot)
{
   return &snapshots[0] == snapshot                   
       || &snapshots[next_snapshot_i-1] == snapshot   
       || snapshot->kind == Peak;                     
}

static void sanity_check_snapshot(Snapshot* snapshot)
{
   if (snapshot->alloc_sxpt) {
      sanity_check_SXTree(snapshot->alloc_sxpt);
   }
}

static void sanity_check_snapshots_array(void)
{
   Int i;
   for (i = 0; i < next_snapshot_i; i++) {
      tl_assert( is_snapshot_in_use( & snapshots[i] ));
   }
   for (    ; i < clo_max_snapshots; i++) {
      tl_assert(!is_snapshot_in_use( & snapshots[i] ));
   }
}

static void clear_snapshot(Snapshot* snapshot, Bool do_sanity_check)
{
   if (do_sanity_check) sanity_check_snapshot(snapshot);
   snapshot->kind           = Unused;
   snapshot->time           = UNUSED_SNAPSHOT_TIME;
   snapshot->heap_extra_szB = 0;
   snapshot->heap_szB       = 0;
   snapshot->stacks_szB     = 0;
   snapshot->alloc_sxpt     = NULL;
}

static void delete_snapshot(Snapshot* snapshot)
{
   
   
   
   SXPt* tmp_sxpt = snapshot->alloc_sxpt;
   clear_snapshot(snapshot, True);
   if (tmp_sxpt) {
      free_SXTree(tmp_sxpt);
   }
}

static void VERB_snapshot(Int verbosity, const HChar* prefix, Int i)
{
   Snapshot* snapshot = &snapshots[i];
   const HChar* suffix;
   switch (snapshot->kind) {
   case Peak:   suffix = "p";                                            break;
   case Normal: suffix = ( is_detailed_snapshot(snapshot) ? "d" : "." ); break;
   case Unused: suffix = "u";                                            break;
   default:
      tl_assert2(0, "VERB_snapshot: unknown snapshot kind: %d", snapshot->kind);
   }
   VERB(verbosity, "%s S%s%3d (t:%lld, hp:%ld, ex:%ld, st:%ld)\n",
      prefix, suffix, i,
      snapshot->time,
      snapshot->heap_szB,
      snapshot->heap_extra_szB,
      snapshot->stacks_szB
   );
}

static UInt cull_snapshots(void)
{
   Int  i, jp, j, jn, min_timespan_i;
   Int  n_deleted = 0;
   Time min_timespan;

   n_cullings++;

   
   #define FIND_SNAPSHOT(i, j) \
      for (j = i; \
           j < clo_max_snapshots && !is_snapshot_in_use(&snapshots[j]); \
           j++) { }

   VERB(2, "Culling...\n");

   
   
   for (i = 0; i < clo_max_snapshots/2; i++) {
      
      
      
      
      Snapshot* min_snapshot;
      Int min_j;

      
      
      jp = 0;
      FIND_SNAPSHOT(1,   j);
      FIND_SNAPSHOT(j+1, jn);
      min_timespan = 0x7fffffffffffffffLL;
      min_j        = -1;
      while (jn < clo_max_snapshots) {
         Time timespan = snapshots[jn].time - snapshots[jp].time;
         tl_assert(timespan >= 0);
         
         if (Peak != snapshots[j].kind && timespan < min_timespan) {
            min_timespan = timespan;
            min_j        = j;
         }
         
         jp = j;
         j  = jn;
         FIND_SNAPSHOT(jn+1, jn);
      }
      
      
      tl_assert(-1 != min_j);    
      min_snapshot = & snapshots[ min_j ];
      if (VG_(clo_verbosity) > 1) {
         HChar buf[64];   
         VG_(snprintf)(buf, 64, " %3d (t-span = %lld)", i, min_timespan);
         VERB_snapshot(2, buf, min_j);
      }
      delete_snapshot(min_snapshot);
      n_deleted++;
   }

   
   
   
   for (i = 0;  is_snapshot_in_use( &snapshots[i] ); i++) { }
   for (j = i; !is_snapshot_in_use( &snapshots[j] ); j++) { }
   for (  ; j < clo_max_snapshots; j++) {
      if (is_snapshot_in_use( &snapshots[j] )) {
         snapshots[i++] = snapshots[j];
         clear_snapshot(&snapshots[j], True);
      }
   }
   next_snapshot_i = i;

   
   sanity_check_snapshots_array();

   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   tl_assert(next_snapshot_i > 1);
   min_timespan = 0x7fffffffffffffffLL;
   min_timespan_i = -1;
   for (i = 1; i < next_snapshot_i; i++) {
      if (is_uncullable_snapshot(&snapshots[i]) &&
          is_uncullable_snapshot(&snapshots[i-1]))
      {
         VERB(2, "(Ignoring interval %d--%d when computing minimum)\n", i-1, i);
      } else {
         Time timespan = snapshots[i].time - snapshots[i-1].time;
         tl_assert(timespan >= 0);
         if (timespan < min_timespan) {
            min_timespan = timespan;
            min_timespan_i = i;
         }
      }
   }
   tl_assert(-1 != min_timespan_i);    

   
   if (VG_(clo_verbosity) > 1) {
      VERB(2, "Finished culling (%3d of %3d deleted)\n",
         n_deleted, clo_max_snapshots);
      for (i = 0; i < next_snapshot_i; i++) {
         VERB_snapshot(2, "  post-cull", i);
      }
      VERB(2, "New time interval = %lld (between snapshots %d and %d)\n",
         min_timespan, min_timespan_i-1, min_timespan_i);
   }

   return min_timespan;
}

static Time get_time(void)
{
   
   if (clo_time_unit == TimeI) {
      return guest_instrs_executed;
   } else if (clo_time_unit == TimeMS) {
      
      
      
      
      
      
      
      
      
      static Bool is_first_get_time = True;
      static Time start_time_ms;
      if (is_first_get_time) {
         start_time_ms = VG_(read_millisecond_timer)();
         is_first_get_time = False;
         return 0;
      } else {
         return VG_(read_millisecond_timer)() - start_time_ms;
      }
   } else if (clo_time_unit == TimeB) {
      return total_allocs_deallocs_szB;
   } else {
      tl_assert2(0, "bad --time-unit value");
   }
}

static void
take_snapshot(Snapshot* snapshot, SnapshotKind kind, Time my_time,
              Bool is_detailed)
{
   tl_assert(!is_snapshot_in_use(snapshot));
   if (!clo_pages_as_heap) {
      tl_assert(have_started_executing_code);
   }

   
   if (clo_heap) {
      snapshot->heap_szB = heap_szB;
      if (is_detailed) {
         SizeT total_szB = heap_szB + heap_extra_szB + stacks_szB;
         snapshot->alloc_sxpt = dup_XTree(alloc_xpt, total_szB);
         tl_assert(           alloc_xpt->szB == heap_szB);
         tl_assert(snapshot->alloc_sxpt->szB == heap_szB);
      }
      snapshot->heap_extra_szB = heap_extra_szB;
   }

   
   if (clo_stacks) {
      snapshot->stacks_szB = stacks_szB;
   }

   
   snapshot->kind = kind;
   snapshot->time = my_time;
   sanity_check_snapshot(snapshot);

   
   if (Peak == kind) n_peak_snapshots++;
   if (is_detailed)  n_detailed_snapshots++;
   n_real_snapshots++;
}


static void
maybe_take_snapshot(SnapshotKind kind, const HChar* what)
{
   
   
   
   
   
   static Time min_time_interval = 0;
   
   static Time earliest_possible_time_of_next_snapshot = 0;
   static Int  n_snapshots_since_last_detailed         = 0;
   static Int  n_skipped_snapshots_since_last_snapshot = 0;

   Snapshot* snapshot;
   Bool      is_detailed;
   
   
   Time      my_time = get_time();

   switch (kind) {
    case Normal: 
      
      if (my_time < earliest_possible_time_of_next_snapshot) {
         n_skipped_snapshots++;
         n_skipped_snapshots_since_last_snapshot++;
         return;
      }
      is_detailed = (clo_detailed_freq-1 == n_snapshots_since_last_detailed);
      break;

    case Peak: {
      
      
      
      
      
      SizeT total_szB = heap_szB + heap_extra_szB + stacks_szB;
      SizeT excess_szB_for_new_peak =
         (SizeT)((peak_snapshot_total_szB * clo_peak_inaccuracy) / 100);
      if (total_szB <= peak_snapshot_total_szB + excess_szB_for_new_peak) {
         return;
      }
      is_detailed = True;
      break;
    }

    default:
      tl_assert2(0, "maybe_take_snapshot: unrecognised snapshot kind");
   }

   
   snapshot = & snapshots[next_snapshot_i];
   take_snapshot(snapshot, kind, my_time, is_detailed);

   
   if (is_detailed) {
      n_snapshots_since_last_detailed = 0;
   } else {
      n_snapshots_since_last_detailed++;
   }

   
   if (Peak == kind) {
      Int i, number_of_peaks_snapshots_found = 0;

      
      SizeT snapshot_total_szB =
         snapshot->heap_szB + snapshot->heap_extra_szB + snapshot->stacks_szB;
      tl_assert2(snapshot_total_szB > peak_snapshot_total_szB,
         "%ld, %ld\n", snapshot_total_szB, peak_snapshot_total_szB);
      peak_snapshot_total_szB = snapshot_total_szB;

      
      for (i = 0; i < next_snapshot_i; i++) {
         if (Peak == snapshots[i].kind) {
            snapshots[i].kind = Normal;
            number_of_peaks_snapshots_found++;
         }
      }
      tl_assert(number_of_peaks_snapshots_found <= 1);
   }

   
   if (n_skipped_snapshots_since_last_snapshot > 0) {
      VERB(2, "  (skipped %d snapshot%s)\n",
         n_skipped_snapshots_since_last_snapshot,
         ( 1 == n_skipped_snapshots_since_last_snapshot ? "" : "s") );
   }
   VERB_snapshot(2, what, next_snapshot_i);
   n_skipped_snapshots_since_last_snapshot = 0;

   
   next_snapshot_i++;
   if (clo_max_snapshots == next_snapshot_i) {
      min_time_interval = cull_snapshots();
   }

   
   earliest_possible_time_of_next_snapshot = my_time + min_time_interval;
}



static Bool ms_cheap_sanity_check ( void )
{
   return True;   
}

static Bool ms_expensive_sanity_check ( void )
{
   sanity_check_XTree(alloc_xpt, NULL);
   sanity_check_snapshots_array();
   return True;
}



typedef
   struct _HP_Chunk {
      struct _HP_Chunk* next;
      Addr              data;       
      SizeT             req_szB;    
      SizeT             slop_szB;   
      XPt*              where;      
   }
   HP_Chunk;

static VgHashTable *malloc_list  = NULL;   

static void update_alloc_stats(SSizeT szB_delta)
{
   
   if (szB_delta < 0) szB_delta = -szB_delta;
   total_allocs_deallocs_szB += szB_delta;
}

static void update_heap_stats(SSizeT heap_szB_delta, Int heap_extra_szB_delta)
{
   if (heap_szB_delta < 0)
      tl_assert(heap_szB >= -heap_szB_delta);
   if (heap_extra_szB_delta < 0)
      tl_assert(heap_extra_szB >= -heap_extra_szB_delta);

   heap_extra_szB += heap_extra_szB_delta;
   heap_szB       += heap_szB_delta;

   update_alloc_stats(heap_szB_delta + heap_extra_szB_delta);
}

static
void* record_block( ThreadId tid, void* p, SizeT req_szB, SizeT slop_szB,
                    Bool exclude_first_entry, Bool maybe_snapshot )
{
   
   HP_Chunk* hc = VG_(malloc)("ms.main.rb.1", sizeof(HP_Chunk));
   hc->req_szB  = req_szB;
   hc->slop_szB = slop_szB;
   hc->data     = (Addr)p;
   hc->where    = NULL;
   VG_(HT_add_node)(malloc_list, hc);

   if (clo_heap) {
      VERB(3, "<<< record_block (%lu, %lu)\n", req_szB, slop_szB);

      hc->where = get_XCon( tid, exclude_first_entry );

      if (hc->where) {
         
         n_heap_allocs++;

         
         update_heap_stats(req_szB, clo_heap_admin + slop_szB);

         
         update_XCon(hc->where, req_szB);

         
         if (maybe_snapshot) {
            maybe_take_snapshot(Normal, "  alloc");
         }

      } else {
         
         n_ignored_heap_allocs++;

         VERB(3, "(ignored)\n");
      }

      VERB(3, ">>>\n");
   }

   return p;
}

static __inline__
void* alloc_and_record_block ( ThreadId tid, SizeT req_szB, SizeT req_alignB,
                               Bool is_zeroed )
{
   SizeT actual_szB, slop_szB;
   void* p;

   if ((SSizeT)req_szB < 0) return NULL;

   
   p = VG_(cli_malloc)( req_alignB, req_szB );
   if (!p) {
      return NULL;
   }
   if (is_zeroed) VG_(memset)(p, 0, req_szB);
   actual_szB = VG_(cli_malloc_usable_size)(p);
   tl_assert(actual_szB >= req_szB);
   slop_szB = actual_szB - req_szB;

   
   record_block(tid, p, req_szB, slop_szB, True,
                True);

   return p;
}

static __inline__
void unrecord_block ( void* p, Bool maybe_snapshot )
{
   
   HP_Chunk* hc = VG_(HT_remove)(malloc_list, (UWord)p);
   if (NULL == hc) {
      return;   
   }

   if (clo_heap) {
      VERB(3, "<<< unrecord_block\n");

      if (hc->where) {
         
         n_heap_frees++;

         
         if (maybe_snapshot) {
            maybe_take_snapshot(Peak, "de-PEAK");
         }

         
         update_heap_stats(-hc->req_szB, -clo_heap_admin - hc->slop_szB);

         
         update_XCon(hc->where, -hc->req_szB);

         
         if (maybe_snapshot) {
            maybe_take_snapshot(Normal, "dealloc");
         }

      } else {
         n_ignored_heap_frees++;

         VERB(3, "(ignored)\n");
      }

      VERB(3, ">>> (-%lu, -%lu)\n", hc->req_szB, hc->slop_szB);
   }

   
   VG_(free)( hc );  hc = NULL;
}

static __inline__
void* realloc_block ( ThreadId tid, void* p_old, SizeT new_req_szB )
{
   HP_Chunk* hc;
   void*     p_new;
   SizeT     old_req_szB, old_slop_szB, new_slop_szB, new_actual_szB;
   XPt      *old_where, *new_where;
   Bool      is_ignored = False;

   
   hc = VG_(HT_remove)(malloc_list, (UWord)p_old);
   if (hc == NULL) {
      return NULL;   
   }

   old_req_szB  = hc->req_szB;
   old_slop_szB = hc->slop_szB;

   tl_assert(!clo_pages_as_heap);  
   if (clo_heap) {
      VERB(3, "<<< realloc_block (%lu)\n", new_req_szB);

      if (hc->where) {
         
         n_heap_reallocs++;

         
         if (new_req_szB < old_req_szB) {
            maybe_take_snapshot(Peak, "re-PEAK");
         }
      } else {
         
         
         is_ignored = True;
      }
   }

   
   if (new_req_szB <= old_req_szB + old_slop_szB) {
      
      p_new = p_old;
      new_slop_szB = old_slop_szB + (old_req_szB - new_req_szB);

   } else {
      
      p_new = VG_(cli_malloc)(VG_(clo_alignment), new_req_szB);
      if (!p_new) {
         
         
         return NULL;
      }
      VG_(memcpy)(p_new, p_old, old_req_szB + old_slop_szB);
      VG_(cli_free)(p_old);
      new_actual_szB = VG_(cli_malloc_usable_size)(p_new);
      tl_assert(new_actual_szB >= new_req_szB);
      new_slop_szB = new_actual_szB - new_req_szB;
   }

   if (p_new) {
      
      hc->data     = (Addr)p_new;
      hc->req_szB  = new_req_szB;
      hc->slop_szB = new_slop_szB;
      old_where    = hc->where;
      hc->where    = NULL;

      
      if (clo_heap) {
         new_where = get_XCon( tid, True);
         if (!is_ignored && new_where) {
            hc->where = new_where;
            update_XCon(old_where, -old_req_szB);
            update_XCon(new_where,  new_req_szB);
         } else {
            
            is_ignored = True;

            
            n_ignored_heap_reallocs++;
         }
      }
   }

   
   
   
   
   
   VG_(HT_add_node)(malloc_list, hc);

   if (clo_heap) {
      if (!is_ignored) {
         
         update_heap_stats(new_req_szB - old_req_szB,
                          new_slop_szB - old_slop_szB);

         
         maybe_take_snapshot(Normal, "realloc");
      } else {

         VERB(3, "(ignored)\n");
      }

      VERB(3, ">>> (%ld, %ld)\n",
         new_req_szB - old_req_szB, new_slop_szB - old_slop_szB);
   }

   return p_new;
}



static void* ms_malloc ( ThreadId tid, SizeT szB )
{
   return alloc_and_record_block( tid, szB, VG_(clo_alignment), False );
}

static void* ms___builtin_new ( ThreadId tid, SizeT szB )
{
   return alloc_and_record_block( tid, szB, VG_(clo_alignment), False );
}

static void* ms___builtin_vec_new ( ThreadId tid, SizeT szB )
{
   return alloc_and_record_block( tid, szB, VG_(clo_alignment), False );
}

static void* ms_calloc ( ThreadId tid, SizeT m, SizeT szB )
{
   return alloc_and_record_block( tid, m*szB, VG_(clo_alignment), True );
}

static void *ms_memalign ( ThreadId tid, SizeT alignB, SizeT szB )
{
   return alloc_and_record_block( tid, szB, alignB, False );
}

static void ms_free ( ThreadId tid __attribute__((unused)), void* p )
{
   unrecord_block(p, True);
   VG_(cli_free)(p);
}

static void ms___builtin_delete ( ThreadId tid, void* p )
{
   unrecord_block(p, True);
   VG_(cli_free)(p);
}

static void ms___builtin_vec_delete ( ThreadId tid, void* p )
{
   unrecord_block(p, True);
   VG_(cli_free)(p);
}

static void* ms_realloc ( ThreadId tid, void* p_old, SizeT new_szB )
{
   return realloc_block(tid, p_old, new_szB);
}

static SizeT ms_malloc_usable_size ( ThreadId tid, void* p )
{                                                            
   HP_Chunk* hc = VG_(HT_lookup)( malloc_list, (UWord)p );

   return ( hc ? hc->req_szB + hc->slop_szB : 0 );
}                                                            


static
void ms_record_page_mem ( Addr a, SizeT len )
{
   ThreadId tid = VG_(get_running_tid)();
   Addr end;
   tl_assert(VG_IS_PAGE_ALIGNED(len));
   tl_assert(len >= VKI_PAGE_SIZE);
   
   for (end = a + len - VKI_PAGE_SIZE; a < end; a += VKI_PAGE_SIZE) {
      record_block( tid, (void*)a, VKI_PAGE_SIZE, 0,
                    False, False );
   }
   
   record_block( tid, (void*)a, VKI_PAGE_SIZE, 0,
                 False, True );
}

static
void ms_unrecord_page_mem( Addr a, SizeT len )
{
   Addr end;
   tl_assert(VG_IS_PAGE_ALIGNED(len));
   tl_assert(len >= VKI_PAGE_SIZE);
   for (end = a + len - VKI_PAGE_SIZE; a < end; a += VKI_PAGE_SIZE) {
      unrecord_block((void*)a, False);
   }
   unrecord_block((void*)a, True);
}


static
void ms_new_mem_mmap ( Addr a, SizeT len,
                       Bool rr, Bool ww, Bool xx, ULong di_handle )
{
   tl_assert(VG_IS_PAGE_ALIGNED(len));
   ms_record_page_mem(a, len);
}

static
void ms_new_mem_startup( Addr a, SizeT len,
                         Bool rr, Bool ww, Bool xx, ULong di_handle )
{
   
   
   
   len = VG_PGROUNDUP(len);
   ms_record_page_mem(a, len);
}

static
void ms_new_mem_brk ( Addr a, SizeT len, ThreadId tid )
{
   
   
   
   
   Addr old_bottom_page = VG_PGROUNDDN(a - 1);
   Addr new_top_page = VG_PGROUNDDN(a + len - 1);
   if (old_bottom_page != new_top_page)
      ms_record_page_mem(VG_PGROUNDDN(a),
                         (new_top_page - old_bottom_page));
}

static
void ms_copy_mem_remap( Addr from, Addr to, SizeT len)
{
   tl_assert(VG_IS_PAGE_ALIGNED(len));
   ms_unrecord_page_mem(from, len);
   ms_record_page_mem(to, len);
}

static
void ms_die_mem_munmap( Addr a, SizeT len )
{
   tl_assert(VG_IS_PAGE_ALIGNED(len));
   ms_unrecord_page_mem(a, len);
}

static
void ms_die_mem_brk( Addr a, SizeT len )
{
   
   
   Addr new_bottom_page = VG_PGROUNDDN(a - 1);
   Addr old_top_page = VG_PGROUNDDN(a + len - 1);
   if (old_top_page != new_bottom_page)
      ms_unrecord_page_mem(VG_PGROUNDDN(a),
                           (old_top_page - new_bottom_page));

}


#define INLINE    inline __attribute__((always_inline))

static void update_stack_stats(SSizeT stack_szB_delta)
{
   if (stack_szB_delta < 0) tl_assert(stacks_szB >= -stack_szB_delta);
   stacks_szB += stack_szB_delta;

   update_alloc_stats(stack_szB_delta);
}

static INLINE void new_mem_stack_2(SizeT len, const HChar* what)
{
   if (have_started_executing_code) {
      VERB(3, "<<< new_mem_stack (%ld)\n", len);
      n_stack_allocs++;
      update_stack_stats(len);
      maybe_take_snapshot(Normal, what);
      VERB(3, ">>>\n");
   }
}

static INLINE void die_mem_stack_2(SizeT len, const HChar* what)
{
   if (have_started_executing_code) {
      VERB(3, "<<< die_mem_stack (%ld)\n", -len);
      n_stack_frees++;
      maybe_take_snapshot(Peak,   "stkPEAK");
      update_stack_stats(-len);
      maybe_take_snapshot(Normal, what);
      VERB(3, ">>>\n");
   }
}

static void new_mem_stack(Addr a, SizeT len)
{
   new_mem_stack_2(len, "stk-new");
}

static void die_mem_stack(Addr a, SizeT len)
{
   die_mem_stack_2(len, "stk-die");
}

static void new_mem_stack_signal(Addr a, SizeT len, ThreadId tid)
{
   new_mem_stack_2(len, "sig-new");
}

static void die_mem_stack_signal(Addr a, SizeT len)
{
   die_mem_stack_2(len, "sig-die");
}



static void print_monitor_help ( void )
{
   VG_(gdb_printf) ("\n");
   VG_(gdb_printf) ("massif monitor commands:\n");
   VG_(gdb_printf) ("  snapshot [<filename>]\n");
   VG_(gdb_printf) ("  detailed_snapshot [<filename>]\n");
   VG_(gdb_printf) ("      takes a snapshot (or a detailed snapshot)\n");
   VG_(gdb_printf) ("      and saves it in <filename>\n");
   VG_(gdb_printf) ("             default <filename> is massif.vgdb.out\n");
   VG_(gdb_printf) ("  all_snapshots [<filename>]\n");
   VG_(gdb_printf) ("      saves all snapshot(s) taken so far in <filename>\n");
   VG_(gdb_printf) ("             default <filename> is massif.vgdb.out\n");
   VG_(gdb_printf) ("\n");
}


static Bool handle_gdb_monitor_command (ThreadId tid, HChar *req);
static Bool ms_handle_client_request ( ThreadId tid, UWord* argv, UWord* ret )
{
   switch (argv[0]) {
   case VG_USERREQ__MALLOCLIKE_BLOCK: {
      void* p   = (void*)argv[1];
      SizeT szB =        argv[2];
      record_block( tid, p, szB, 0, False,
                    True );
      *ret = 0;
      return True;
   }
   case VG_USERREQ__RESIZEINPLACE_BLOCK: {
      void* p        = (void*)argv[1];
      SizeT newSizeB =       argv[3];

      unrecord_block(p, True);
      record_block(tid, p, newSizeB, 0,
                   False, True);
      return True;
   }
   case VG_USERREQ__FREELIKE_BLOCK: {
      void* p = (void*)argv[1];
      unrecord_block(p, True);
      *ret = 0;
      return True;
   }
   case VG_USERREQ__GDB_MONITOR_COMMAND: {
     Bool handled = handle_gdb_monitor_command (tid, (HChar*)argv[1]);
     if (handled)
       *ret = 1;
     else
       *ret = 0;
     return handled;
   }

   default:
      *ret = 0;
      return False;
   }
}


static void add_counter_update(IRSB* sbOut, Int n)
{
   #if defined(VG_BIGENDIAN)
   # define END Iend_BE
   #elif defined(VG_LITTLEENDIAN)
   # define END Iend_LE
   #else
   # error "Unknown endianness"
   #endif
   
   
   
   
   IRTemp t1 = newIRTemp(sbOut->tyenv, Ity_I64);
   IRTemp t2 = newIRTemp(sbOut->tyenv, Ity_I64);
   IRExpr* counter_addr = mkIRExpr_HWord( (HWord)&guest_instrs_executed );

   IRStmt* st1 = IRStmt_WrTmp(t1, IRExpr_Load(END, Ity_I64, counter_addr));
   IRStmt* st2 =
      IRStmt_WrTmp(t2,
                   IRExpr_Binop(Iop_Add64, IRExpr_RdTmp(t1),
                                           IRExpr_Const(IRConst_U64(n))));
   IRStmt* st3 = IRStmt_Store(END, counter_addr, IRExpr_RdTmp(t2));

   addStmtToIRSB( sbOut, st1 );
   addStmtToIRSB( sbOut, st2 );
   addStmtToIRSB( sbOut, st3 );
}

static IRSB* ms_instrument2( IRSB* sbIn )
{
   Int   i, n = 0;
   IRSB* sbOut;

   
   
   
   
   
   sbOut = deepCopyIRSBExceptStmts(sbIn);
   
   for (i = 0; i < sbIn->stmts_used; i++) {
      IRStmt* st = sbIn->stmts[i];
      
      if (!st || st->tag == Ist_NoOp) continue;
      
      if (st->tag == Ist_IMark) {
         n++;
      } else if (st->tag == Ist_Exit) {
         if (n > 0) {
            
            add_counter_update(sbOut, n);
            n = 0;
         }
      }
      addStmtToIRSB( sbOut, st );
   }

   if (n > 0) {
      
      add_counter_update(sbOut, n);
   }
   return sbOut;
}

static
IRSB* ms_instrument ( VgCallbackClosure* closure,
                      IRSB* sbIn,
                      const VexGuestLayout* layout,
                      const VexGuestExtents* vge,
                      const VexArchInfo* archinfo_host,
                      IRType gWordTy, IRType hWordTy )
{
   if (! have_started_executing_code) {
      
      
      
      have_started_executing_code = True;
      maybe_take_snapshot(Normal, "startup");
   }

   if      (clo_time_unit == TimeI)  { return ms_instrument2(sbIn); }
   else if (clo_time_unit == TimeMS) { return sbIn; }
   else if (clo_time_unit == TimeB)  { return sbIn; }
   else                              { tl_assert2(0, "bad --time-unit value"); }
}



#define FP(format, args...) ({ VG_(fprintf)(fp, format, ##args); })

static void pp_snapshot_SXPt(VgFile *fp, SXPt* sxpt, Int depth,
                             HChar* depth_str, Int depth_str_len,
                             SizeT snapshot_heap_szB, SizeT snapshot_total_szB)
{
   Int   i, j, n_insig_children_sxpts;
   SXPt* child = NULL;

   
   
   
   const HChar* ip_desc;

   switch (sxpt->tag) {
    case SigSXPt:
      
      if (0 == depth) {
         if (clo_heap) {
            ip_desc = 
               ( clo_pages_as_heap
               ? "(page allocation syscalls) mmap/mremap/brk, --alloc-fns, etc."
               : "(heap allocation functions) malloc/new/new[], --alloc-fns, etc."
               );
         } else {
            

            
            
            tl_assert2(0, "pp_snapshot_SXPt: unexpected");
         }
      } else {
         
         
         if ( ! VG_(clo_show_below_main) ) {
            Vg_FnNameKind kind = VG_(get_fnname_kind_from_IP)(sxpt->Sig.ip);
            if (Vg_FnNameMain == kind || Vg_FnNameBelowMain == kind) {
               sxpt->Sig.n_children = 0;
            }
         }

         
         ip_desc = VG_(describe_IP)(sxpt->Sig.ip-1, NULL);
      }
      
      
      FP("%sn%d: %lu ", depth_str, sxpt->Sig.n_children, sxpt->szB);

      
      
      
      j = 0;
      if ('0' == ip_desc[0] && 'x' == ip_desc[1]) {
         j = 2;
         while (True) {
            if (ip_desc[j]) {
               if (':' == ip_desc[j]) break;
               j++;
            } else {
               tl_assert2(0, "ip_desc has unexpected form: %s\n", ip_desc);
            }
         }
      }
      
      
      
      
      
      
      FP("%s\n", ip_desc);

      
      tl_assert(depth+1 < depth_str_len-1);    
      depth_str[depth+0] = ' ';
      depth_str[depth+1] = '\0';

      
      
      
      
      
      
      VG_(ssort)(sxpt->Sig.children, sxpt->Sig.n_children, sizeof(SXPt*),
                 SXPt_revcmp_szB);

      
      n_insig_children_sxpts = 0;
      for (i = 0; i < sxpt->Sig.n_children; i++) {
         child = sxpt->Sig.children[i];

         if (InsigSXPt == child->tag)
            n_insig_children_sxpts++;

         
         
         
         pp_snapshot_SXPt(fp, child, depth+1, depth_str, depth_str_len,
            snapshot_heap_szB, snapshot_total_szB);
      }

      
      depth_str[depth+0] = '\0';
      depth_str[depth+1] = '\0';

      
      tl_assert(n_insig_children_sxpts <= 1);
      break;

    case InsigSXPt: {
      const HChar* s = ( 1 == sxpt->Insig.n_xpts ? "," : "s, all" );
      FP("%sn0: %lu in %d place%s below massif's threshold (%.2f%%)\n",
         depth_str, sxpt->szB, sxpt->Insig.n_xpts, s, clo_threshold);
      break;
    }

    default:
      tl_assert2(0, "pp_snapshot_SXPt: unrecognised SXPt tag");
   }
}

static void pp_snapshot(VgFile *fp, Snapshot* snapshot, Int snapshot_n)
{
   sanity_check_snapshot(snapshot);

   FP("#-----------\n");
   FP("snapshot=%d\n", snapshot_n);
   FP("#-----------\n");
   FP("time=%lld\n",            snapshot->time);
   FP("mem_heap_B=%lu\n",       snapshot->heap_szB);
   FP("mem_heap_extra_B=%lu\n", snapshot->heap_extra_szB);
   FP("mem_stacks_B=%lu\n",     snapshot->stacks_szB);

   if (is_detailed_snapshot(snapshot)) {
      
      Int   depth_str_len = clo_depth + 3;
      HChar* depth_str = VG_(malloc)("ms.main.pps.1", 
                                     sizeof(HChar) * depth_str_len);
      SizeT snapshot_total_szB =
         snapshot->heap_szB + snapshot->heap_extra_szB + snapshot->stacks_szB;
      depth_str[0] = '\0';   

      FP("heap_tree=%s\n", ( Peak == snapshot->kind ? "peak" : "detailed" ));
      pp_snapshot_SXPt(fp, snapshot->alloc_sxpt, 0, depth_str,
                       depth_str_len, snapshot->heap_szB,
                       snapshot_total_szB);

      VG_(free)(depth_str);

   } else {
      FP("heap_tree=empty\n");
   }
}

static void write_snapshots_to_file(const HChar* massif_out_file, 
                                    Snapshot snapshots_array[], 
                                    Int nr_elements)
{
   Int i;
   VgFile *fp;

   fp = VG_(fopen)(massif_out_file, VKI_O_CREAT|VKI_O_TRUNC|VKI_O_WRONLY,
                                    VKI_S_IRUSR|VKI_S_IWUSR);
   if (fp == NULL) {
      
      
      VG_(umsg)("error: can't open output file '%s'\n", massif_out_file );
      VG_(umsg)("       ... so profiling results will be missing.\n");
      return;
   }

   
   
   
   
   FP("desc:");
   for (i = 0; i < VG_(sizeXA)(args_for_massif); i++) {
      HChar* arg = *(HChar**)VG_(indexXA)(args_for_massif, i);
      FP(" %s", arg);
   }
   if (0 == i) FP(" (none)");
   FP("\n");

   
   FP("cmd: ");
   FP("%s", VG_(args_the_exename));
   for (i = 0; i < VG_(sizeXA)( VG_(args_for_client) ); i++) {
      HChar* arg = * (HChar**) VG_(indexXA)( VG_(args_for_client), i );
      FP(" %s", arg);
   }
   FP("\n");

   FP("time_unit: %s\n", TimeUnit_to_string(clo_time_unit));

   for (i = 0; i < nr_elements; i++) {
      Snapshot* snapshot = & snapshots_array[i];
      pp_snapshot(fp, snapshot, i);     
   }
   VG_(fclose) (fp);
}

static void write_snapshots_array_to_file(void)
{
   
   
   
   
   
   HChar* massif_out_file =
      VG_(expand_file_name)("--massif-out-file", clo_massif_out_file);
   write_snapshots_to_file (massif_out_file, snapshots, next_snapshot_i);
   VG_(free)(massif_out_file);
}

static void handle_snapshot_monitor_command (const HChar *filename,
                                             Bool detailed)
{
   Snapshot snapshot;

   if (!clo_pages_as_heap && !have_started_executing_code) {
      
      VG_(gdb_printf) 
         ("error: cannot take snapshot before execution has started\n");
      return;
   }

   clear_snapshot(&snapshot,  False);
   take_snapshot(&snapshot, Normal, get_time(), detailed);
   write_snapshots_to_file ((filename == NULL) ? 
                            "massif.vgdb.out" : filename,
                            &snapshot,
                            1);
   delete_snapshot(&snapshot);
}

static void handle_all_snapshots_monitor_command (const HChar *filename)
{
   if (!clo_pages_as_heap && !have_started_executing_code) {
      
      VG_(gdb_printf) 
         ("error: cannot take snapshot before execution has started\n");
      return;
   }

   write_snapshots_to_file ((filename == NULL) ? 
                            "massif.vgdb.out" : filename,
                            snapshots, next_snapshot_i);
}

static Bool handle_gdb_monitor_command (ThreadId tid, HChar *req)
{
   HChar* wcmd;
   HChar s[VG_(strlen(req)) + 1]; 
   HChar *ssaveptr;

   VG_(strcpy) (s, req);

   wcmd = VG_(strtok_r) (s, " ", &ssaveptr);
   switch (VG_(keyword_id) ("help snapshot detailed_snapshot all_snapshots", 
                            wcmd, kwd_report_duplicated_matches)) {
   case -2: 
      return True;
   case -1: 
      return False;
   case  0: 
      print_monitor_help();
      return True;
   case  1: { 
      HChar* filename;
      filename = VG_(strtok_r) (NULL, " ", &ssaveptr);
      handle_snapshot_monitor_command (filename, False );
      return True;
   }
   case  2: { 
      HChar* filename;
      filename = VG_(strtok_r) (NULL, " ", &ssaveptr);
      handle_snapshot_monitor_command (filename, True );
      return True;
   }
   case  3: { 
      HChar* filename;
      filename = VG_(strtok_r) (NULL, " ", &ssaveptr);
      handle_all_snapshots_monitor_command (filename);
      return True;
   }
   default: 
      tl_assert(0);
      return False;
   }
}

static void ms_print_stats (void)
{
#define STATS(format, args...) \
      VG_(dmsg)("Massif: " format, ##args)

   STATS("heap allocs:           %u\n", n_heap_allocs);
   STATS("heap reallocs:         %u\n", n_heap_reallocs);
   STATS("heap frees:            %u\n", n_heap_frees);
   STATS("ignored heap allocs:   %u\n", n_ignored_heap_allocs);
   STATS("ignored heap frees:    %u\n", n_ignored_heap_frees);
   STATS("ignored heap reallocs: %u\n", n_ignored_heap_reallocs);
   STATS("stack allocs:          %u\n", n_stack_allocs);
   STATS("stack frees:           %u\n", n_stack_frees);
   STATS("XPts:                  %u\n", n_xpts);
   STATS("top-XPts:              %u (%d%%)\n",
      alloc_xpt->n_children,
      ( n_xpts ? alloc_xpt->n_children * 100 / n_xpts : 0));
   STATS("XPt init expansions:   %u\n", n_xpt_init_expansions);
   STATS("XPt later expansions:  %u\n", n_xpt_later_expansions);
   STATS("SXPt allocs:           %u\n", n_sxpt_allocs);
   STATS("SXPt frees:            %u\n", n_sxpt_frees);
   STATS("skipped snapshots:     %u\n", n_skipped_snapshots);
   STATS("real snapshots:        %u\n", n_real_snapshots);
   STATS("detailed snapshots:    %u\n", n_detailed_snapshots);
   STATS("peak snapshots:        %u\n", n_peak_snapshots);
   STATS("cullings:              %u\n", n_cullings);
   STATS("XCon redos:            %u\n", n_XCon_redos);
#undef STATS
}


static void ms_fini(Int exit_status)
{
   
   write_snapshots_array_to_file();

   
   tl_assert(n_xpts > 0);  

   if (VG_(clo_stats))
      ms_print_stats();
}



static void ms_post_clo_init(void)
{
   Int i;
   HChar* LD_PRELOAD_val;
   HChar* s;
   HChar* s2;

   
   if (clo_pages_as_heap) {
      if (clo_stacks) {
         VG_(fmsg_bad_option)("--pages-as-heap=yes",
            "Cannot be used together with --stacks=yes");
      }
   }
   if (!clo_heap) {
      clo_pages_as_heap = False;
   }

   
   
   
   
   
   
   if (clo_pages_as_heap) {
      clo_heap_admin = 0;     

      LD_PRELOAD_val = VG_(getenv)( VG_(LD_PRELOAD_var_name) );
      tl_assert(LD_PRELOAD_val);

      
      s2 = VG_(strstr)(LD_PRELOAD_val, "vgpreload_core");
      tl_assert(s2);

      
      s2 = VG_(strstr)(LD_PRELOAD_val, "vgpreload_massif");
      tl_assert(s2);

      
      
      for (s = s2; *s != ':'; s--) {
         *s = ' ';
      }

      
      
      for (s = s2; *s != ':' && *s != '\0'; s++) {
         *s = ' ';
      }
   }

   
   if (VG_(clo_verbosity) > 1) {
      VERB(1, "alloc-fns:\n");
      for (i = 0; i < VG_(sizeXA)(alloc_fns); i++) {
         HChar** fn_ptr = VG_(indexXA)(alloc_fns, i);
         VERB(1, "  %s\n", *fn_ptr);
      }

      VERB(1, "ignore-fns:\n");
      if (0 == VG_(sizeXA)(ignore_fns)) {
         VERB(1, "  <empty>\n");
      }
      for (i = 0; i < VG_(sizeXA)(ignore_fns); i++) {
         HChar** fn_ptr = VG_(indexXA)(ignore_fns, i);
         VERB(1, "  %d: %s\n", i, *fn_ptr);
      }
   }

   
   if (clo_stacks) {
      VG_(track_new_mem_stack)        ( new_mem_stack        );
      VG_(track_die_mem_stack)        ( die_mem_stack        );
      VG_(track_new_mem_stack_signal) ( new_mem_stack_signal );
      VG_(track_die_mem_stack_signal) ( die_mem_stack_signal );
   }

   if (clo_pages_as_heap) {
      VG_(track_new_mem_startup) ( ms_new_mem_startup );
      VG_(track_new_mem_brk)     ( ms_new_mem_brk     );
      VG_(track_new_mem_mmap)    ( ms_new_mem_mmap    );

      VG_(track_copy_mem_remap)  ( ms_copy_mem_remap  );

      VG_(track_die_mem_brk)     ( ms_die_mem_brk     );
      VG_(track_die_mem_munmap)  ( ms_die_mem_munmap  ); 
   }

   
   snapshots = VG_(malloc)("ms.main.mpoci.1", 
                           sizeof(Snapshot) * clo_max_snapshots);
   
   
   for (i = 0; i < clo_max_snapshots; i++) {
      clear_snapshot( & snapshots[i], False );
   }
   sanity_check_snapshots_array();
}

static void ms_pre_clo_init(void)
{
   VG_(details_name)            ("Massif");
   VG_(details_version)         (NULL);
   VG_(details_description)     ("a heap profiler");
   VG_(details_copyright_author)(
      "Copyright (C) 2003-2013, and GNU GPL'd, by Nicholas Nethercote");
   VG_(details_bug_reports_to)  (VG_BUGS_TO);

   VG_(details_avg_translation_sizeB) ( 330 );

   VG_(clo_vex_control).iropt_register_updates_default
      = VG_(clo_px_file_backed)
      = VexRegUpdSpAtMemAccess; 

   
   VG_(basic_tool_funcs)          (ms_post_clo_init,
                                   ms_instrument,
                                   ms_fini);

   
   VG_(needs_libc_freeres)();
   VG_(needs_command_line_options)(ms_process_cmd_line_option,
                                   ms_print_usage,
                                   ms_print_debug_usage);
   VG_(needs_client_requests)     (ms_handle_client_request);
   VG_(needs_sanity_checks)       (ms_cheap_sanity_check,
                                   ms_expensive_sanity_check);
   VG_(needs_print_stats)         (ms_print_stats);
   VG_(needs_malloc_replacement)  (ms_malloc,
                                   ms___builtin_new,
                                   ms___builtin_vec_new,
                                   ms_memalign,
                                   ms_calloc,
                                   ms_free,
                                   ms___builtin_delete,
                                   ms___builtin_vec_delete,
                                   ms_realloc,
                                   ms_malloc_usable_size,
                                   0 );

   
   malloc_list = VG_(HT_construct)( "Massif's malloc list" );

   
   alloc_xpt = new_XPt(0, NULL);

   
   init_alloc_fns();
   init_ignore_fns();

   
   args_for_massif = VG_(newXA)(VG_(malloc), "ms.main.mprci.1", 
                                VG_(free), sizeof(HChar*));
}

VG_DETERMINE_INTERFACE_VERSION(ms_pre_clo_init)

