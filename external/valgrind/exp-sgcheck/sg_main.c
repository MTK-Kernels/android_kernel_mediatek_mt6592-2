

/*
   This file is part of Ptrcheck, a Valgrind tool for checking pointer
   use in programs.

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

   Neither the names of the U.S. Department of Energy nor the
   University of California nor the names of its contributors may be
   used to endorse or promote products derived from this software
   without prior written permission.
*/

#include "pub_tool_basics.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_wordfm.h"
#include "pub_tool_xarray.h"
#include "pub_tool_threadstate.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_machine.h"
#include "pub_tool_debuginfo.h"
#include "pub_tool_options.h"

#include "pc_common.h"

#include "sg_main.h"      


static
void preen_global_Invars ( Addr a, SizeT len ); 



static inline Bool is_sane_TId ( ThreadId tid )
{
   return tid >= 0 && tid < VG_N_THREADS
          && tid != VG_INVALID_THREADID;
}

static void* sg_malloc ( const HChar* cc, SizeT n ) {
   void* p;
   tl_assert(n > 0);
   p = VG_(malloc)( cc, n );
   return p;
}

static void sg_free ( void* p ) {
   tl_assert(p);
   VG_(free)(p);
}


inline
static Word cmp_nonempty_intervals ( Addr a1, SizeT n1, 
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

inline
static Bool is_subinterval_of ( Addr aBig, SizeT nBig,
                                Addr aSmall, SizeT nSmall ) {
   tl_assert(nBig > 0 && nSmall > 0);
   return aBig <= aSmall && aSmall + nSmall <= aBig + nBig;
}

inline
static Addr Addr__min ( Addr a1, Addr a2 ) {
   return a1 < a2 ? a1 : a2;
}

inline
static Addr Addr__max ( Addr a1, Addr a2 ) {
   return a1 < a2 ? a2 : a1;
}




static inline Bool StackBlock__sane ( const StackBlock* fb )
{
   if (fb->name[ sizeof(fb->name)-1 ] != 0)
      return False;
   if (fb->spRel != False && fb->spRel != True)
      return False;
   if (fb->isVec != False && fb->isVec != True)
      return False;
   return True;
}

static Word StackBlock__cmp ( const StackBlock* fb1, const StackBlock* fb2 )
{
   Word r;
   tl_assert(StackBlock__sane(fb1));
   tl_assert(StackBlock__sane(fb2));
   if (fb1->base < fb2->base) return -1;
   if (fb1->base > fb2->base) return 1;
   
   if (fb1->szB < fb2->szB) return -1;
   if (fb1->szB > fb2->szB) return 1;
   
   if (fb1->spRel < fb2->spRel) return -1;
   if (fb1->spRel > fb2->spRel) return 1;
   
   if (fb1->isVec < fb2->isVec) return -1;
   if (fb1->isVec > fb2->isVec) return 1;
   
   r = (Word)VG_(strcmp)(fb1->name, fb2->name);
   return r;
}

static Bool StackBlock__all_fields_except_szB_are_equal ( 
               StackBlock* fb1,
               StackBlock* fb2 
            )
{
   tl_assert(StackBlock__sane(fb1));
   tl_assert(StackBlock__sane(fb2));
   return fb1->base == fb2->base
          && fb1->spRel == fb2->spRel
          && fb1->isVec == fb2->isVec
          && 0 == VG_(strcmp)(fb1->name, fb2->name);
}


static Word StackBlocks__cmp ( XArray* fb1s, XArray* fb2s )
{
   Word i, r, n1, n2;
   n1 = VG_(sizeXA)( fb1s );
   n2 = VG_(sizeXA)( fb2s );
   if (n1 < n2) return -1;
   if (n1 > n2) return 1;
   for (i = 0; i < n1; i++) {
      StackBlock *fb1, *fb2;
      fb1 = VG_(indexXA)( fb1s, i );
      fb2 = VG_(indexXA)( fb2s, i );
      r = StackBlock__cmp( fb1, fb2 );
      if (r != 0) return r;
   }
   tl_assert(i == n1 && i == n2);
   return 0;
}

static void pp_StackBlocks ( XArray* sbs )
{
   Word i, n = VG_(sizeXA)( sbs );
   VG_(message)(Vg_DebugMsg, "<<< STACKBLOCKS\n" );
   for (i = 0; i < n; i++) {
      StackBlock* sb = (StackBlock*)VG_(indexXA)( sbs, i );
      VG_(message)(Vg_DebugMsg,
         "   StackBlock{ off %ld szB %lu spRel:%c isVec:%c \"%s\" }\n",
         sb->base, sb->szB, sb->spRel ? 'Y' : 'N',
         sb->isVec ? 'Y' : 'N', &sb->name[0] 
      );
   }
   VG_(message)(Vg_DebugMsg, ">>> STACKBLOCKS\n" );
}



static WordFM* 
       frameBlocks_set = NULL;

static void init_StackBlocks_set ( void )
{
   tl_assert(!frameBlocks_set);
   frameBlocks_set
      = VG_(newFM)( sg_malloc, "di.sg_main.iSBs.1", sg_free, 
                    (Word(*)(UWord,UWord))StackBlocks__cmp );
   tl_assert(frameBlocks_set);
}

static XArray* 
       StackBlocks__find_and_dealloc__or_add
          ( XArray*  orig )
{
   UWord key, val;

   
   VG_(setCmpFnXA)( orig, (XACmpFn_t)StackBlock__cmp );
   VG_(sortXA)( orig );

   
  nuke_dups:
   { Word r, w, nEQ, n = VG_(sizeXA)( orig );
     if (n >= 2) {
        w = 0;
        nEQ = 0;
        for (r = 0; r < n; r++) {
           if (r+1 < n) {
              StackBlock* pR0 = VG_(indexXA)( orig, r+0 );
              StackBlock* pR1 = VG_(indexXA)( orig, r+1 );
              Word c = StackBlock__cmp(pR0,pR1);
              tl_assert(c == -1 || c == 0);
              if (c == 0) { nEQ++; continue; }
           }
           if (w != r) {
              StackBlock* pW = VG_(indexXA)( orig, w );
              StackBlock* pR = VG_(indexXA)( orig, r );
              *pW = *pR;
           }
           w++;
        }
        tl_assert(r == n);
        tl_assert(w + nEQ == n);
        if (w < n) {
           VG_(dropTailXA)( orig, n-w );
        }
        if (0) VG_(printf)("delta %ld\n", n-w);
     }
   }

   { Word i, n = VG_(sizeXA)( orig );
     if (n >= 2) {
        for (i = 0; i < n-1; i++) {
           StackBlock* sb0 = VG_(indexXA)( orig, i+0 );
           StackBlock* sb1 = VG_(indexXA)( orig, i+1 );
           if (StackBlock__all_fields_except_szB_are_equal(sb0, sb1)) {
              tl_assert(sb0->szB < sb1->szB);
              sb0->szB = sb1->szB;
              tl_assert(StackBlock__cmp(sb0,sb1) == 0);
              goto nuke_dups;
           }
        }
     }
   }

   { Word i, n = VG_(sizeXA)( orig );
     static Int moans = 3;
     for (i = 0; i < n-1; i++) {
       StackBlock* sb1 = (StackBlock*)VG_(indexXA)( orig, i );
       StackBlock* sb2 = (StackBlock*)VG_(indexXA)( orig, i+1 );
       if (sb1->spRel == sb2->spRel
           && (sb1->base >= sb2->base
               || sb1->base + sb1->szB > sb2->base)) {
          if (moans > 0 && !VG_(clo_xml)) {
             moans--;
             VG_(message)(Vg_UserMsg, "Warning: bogus DWARF3 info: "
                                      "overlapping stack blocks\n");
             if (VG_(clo_verbosity) >= 2)
                pp_StackBlocks(orig);
             if (moans == 0)
                VG_(message)(Vg_UserMsg, "Further instances of this "
                                         "message will not be shown\n" );
          }
          VG_(dropTailXA)( orig, VG_(sizeXA)( orig ));
          break;
       }
     }
   }

   
   if (VG_(lookupFM)( frameBlocks_set, &key, &val, (UWord)orig )) {
      
      XArray* res;
      tl_assert(val == 0);
      tl_assert(key != (UWord)orig);
      VG_(deleteXA)(orig);
      res = (XArray*)key;
      return res;
   } else {
      
      VG_(addToFM)( frameBlocks_set, (UWord)orig, 0 );
      return orig;
   }
}


static XArray*  get_StackBlocks_for_IP ( Addr ip )
{
   XArray* blocks = VG_(di_get_stack_blocks_at_ip)( ip, True );
   tl_assert(blocks);
   return StackBlocks__find_and_dealloc__or_add( blocks );
}



static Word GlobalBlock__cmp ( GlobalBlock* gb1, GlobalBlock* gb2 )
{
   Word r;
   
   if (gb1->addr < gb2->addr) return -1;
   if (gb1->addr > gb2->addr) return 1;
   
   if (gb1->szB < gb2->szB) return -1;
   if (gb1->szB > gb2->szB) return 1;
   
   if (gb1->isVec < gb2->isVec) return -1;
   if (gb1->isVec > gb2->isVec) return 1;
   
   r = (Word)VG_(strcmp)(gb1->name, gb2->name);
   if (r != 0) return r;
   
   r = (Word)VG_(strcmp)(gb1->soname, gb2->soname);
   return r;
}

static WordFM* 
       globalBlock_set = NULL;

static void init_GlobalBlock_set ( void )
{
   tl_assert(!globalBlock_set);
    globalBlock_set
       = VG_(newFM)( sg_malloc, "di.sg_main.iGBs.1", sg_free, 
                     (Word(*)(UWord,UWord))GlobalBlock__cmp );
   tl_assert(globalBlock_set);
}



static GlobalBlock* get_persistent_GlobalBlock ( GlobalBlock* orig )
{
   UWord key, val;
   
   if (VG_(lookupFM)( globalBlock_set, &key, &val, (UWord)orig )) {
      
      GlobalBlock* res;
      tl_assert(val == 0);
      res = (GlobalBlock*)key;
      tl_assert(res != orig);
      return res;
   } else {
      GlobalBlock* clone = sg_malloc( "di.sg_main.gpGB.1",
                                      sizeof(GlobalBlock) );
      tl_assert(clone);
      *clone = *orig;
      VG_(addToFM)( globalBlock_set, (UWord)clone, 0 );
      return clone;
   }
}



typedef
   struct {
      Addr        addr;
      SizeT       szB;   
      StackBlock* descr; 
      UWord       depth; 
   }
   StackTreeNode;

static void pp_StackTree ( WordFM* sitree, const HChar* who )
{
   UWord keyW, valW;
   VG_(printf)("<<< BEGIN pp_StackTree %s\n", who );
   VG_(initIterFM)( sitree );
   while (VG_(nextIterFM)( sitree, &keyW, &valW )) {
      StackTreeNode* nd = (StackTreeNode*)keyW;
      VG_(printf)("  [%#lx,+%lu) descr=%p %s %lu\n", nd->addr, nd->szB,
                  nd->descr, nd->descr->name, nd->descr->szB);
   }
   VG_(printf)(">>> END   pp_StackTree %s\n", who );
}

static Word cmp_intervals_StackTreeNode ( StackTreeNode* sn1,
                                          StackTreeNode* sn2 )
{
   return cmp_nonempty_intervals(sn1->addr, sn1->szB,
                                 sn2->addr, sn2->szB);
}

static StackTreeNode* find_StackTreeNode ( WordFM* sitree, Addr a )
{
   UWord keyW, valW;
   StackTreeNode key;
   tl_assert(sitree);
   key.addr = a;
   key.szB  = 1;
   if (VG_(lookupFM)( sitree, &keyW, &valW, (UWord)&key )) {
      StackTreeNode* res = (StackTreeNode*)keyW;
      tl_assert(valW == 0);
      tl_assert(res != &key);
      return res;
   } else {
      return NULL;
   }
}

__attribute__((noinline))
static void add_blocks_to_StackTree (
               WordFM* sitree,
               XArray*  descrs,
               XArray*  bases,
               UWord depth
            )
{
   Bool debug = (Bool)0;
   Word i, nDescrs, nBases;

   nDescrs = VG_(sizeXA)( descrs ),
   nBases = VG_(sizeXA)( bases );
   tl_assert(nDescrs == nBases);

   if (nDescrs == 0) return;

   tl_assert(sitree);
   if (debug) {
      VG_(printf)("\ndepth = %lu\n", depth);
      pp_StackTree( sitree, "add_blocks_to_StackTree-pre" );
      pp_StackBlocks(descrs);
   }

   for (i = 0; i < nDescrs; i++) {
      Bool already_present;
      StackTreeNode* nyu;
      Addr        addr  = *(Addr*)VG_(indexXA)( bases, i );
      StackBlock* descr = (StackBlock*)VG_(indexXA)( descrs, i );
      tl_assert(descr->szB > 0);
      nyu = sg_malloc( "di.sg_main.abtST.1", sizeof(StackTreeNode) );
      nyu->addr  = addr;
      nyu->szB   = descr->szB;
      nyu->descr = descr;
      nyu->depth = depth;
      if (debug) VG_(printf)("ADD %#lx %lu\n", addr, descr->szB);
      already_present = VG_(addToFM)( sitree, (UWord)nyu, 0 );
      tl_assert(!already_present);
      if (debug) {
         pp_StackTree( sitree, "add_blocks_to_StackTree-step" );
      }
   }
   if (debug) {
      pp_StackTree( sitree, "add_blocks_to_StackTree-post" );
      VG_(printf)("\n");
   }
}

static void del_blocks_from_StackTree ( WordFM* sitree,
                                        XArray*  bases ) 
{
   UWord oldK, oldV;
   Word i, nBases = VG_(sizeXA)( bases );
   for (i = 0; i < nBases; i++) {
      Bool b;
      Addr addr = *(Addr*)VG_(indexXA)( bases, i );
      StackTreeNode* nd = find_StackTreeNode(sitree, addr);
      tl_assert(nd);
      b = VG_(delFromFM)( sitree, &oldK, &oldV, (UWord)nd );
      
      tl_assert(b);
      tl_assert(oldV == 0);
      tl_assert(nd == (StackTreeNode*)oldK);
      sg_free(nd);
   }
}


static void delete_StackTree__kFin ( UWord keyW ) {
   StackTreeNode* nd = (StackTreeNode*)keyW;
   tl_assert(nd);
   sg_free(nd);
}
static void delete_StackTree__vFin ( UWord valW ) {
   tl_assert(valW == 0);
}
static void delete_StackTree ( WordFM* sitree )
{
   VG_(deleteFM)( sitree,
                 delete_StackTree__kFin, delete_StackTree__vFin );
}

static WordFM* new_StackTree ( void ) {
   return VG_(newFM)( sg_malloc, "di.sg_main.nST.1", sg_free,
                      (Word(*)(UWord,UWord))cmp_intervals_StackTreeNode );
}



typedef
   struct {
      Addr         addr; 
      SizeT        szB; 
      GlobalBlock* descr; 
   }
   GlobalTreeNode;

static void GlobalTreeNode__pp ( GlobalTreeNode* nd ) {
   tl_assert(nd->descr);
   VG_(printf)("GTNode [%#lx,+%ld) %s", 
               nd->addr, nd->szB, nd->descr->name);
}

static void GlobalTree__pp ( WordFM*  gitree,
                             const HChar* who )
{
   UWord keyW, valW;
   GlobalTreeNode* nd;
   VG_(printf)("<<< GlobalBlockTree (%s)\n", who);
   VG_(initIterFM)( gitree );
   while (VG_(nextIterFM)( gitree, &keyW, &valW )) {
      tl_assert(valW == 0);
      nd = (GlobalTreeNode*)keyW;
      VG_(printf)("  ");
      GlobalTreeNode__pp(nd);
      VG_(printf)("\n");
   }
   VG_(doneIterFM)( gitree );
   VG_(printf)(">>>\n");
}

static Word cmp_intervals_GlobalTreeNode ( GlobalTreeNode* gn1,
                                           GlobalTreeNode* gn2 )
{
   return cmp_nonempty_intervals( gn1->addr, gn1->szB,
                                  gn2->addr, gn2->szB );
}

static GlobalTreeNode* find_GlobalTreeNode ( WordFM* gitree, Addr a )
{
   UWord keyW, valW;
   GlobalTreeNode key;
   key.addr = a;
   key.szB  = 1;
   if (VG_(lookupFM)( gitree, &keyW, &valW, (UWord)&key )) {
      GlobalTreeNode* res = (GlobalTreeNode*)keyW;
      tl_assert(valW == 0);
      tl_assert(res != &key);
      return res;
   } else {
      return NULL;
   }
}

static void add_block_to_GlobalTree (
               WordFM* gitree,
               GlobalBlock* descr
            )
{
   Bool already_present;
   GlobalTreeNode *nyu, *nd;
   UWord keyW, valW;
   static Int moans = 3;

   tl_assert(descr->szB > 0);
   nyu = sg_malloc( "di.sg_main.abtG.1", sizeof(GlobalTreeNode) );
   nyu->addr  = descr->addr;
   nyu->szB   = descr->szB;
   nyu->descr = descr;

   already_present = VG_(lookupFM)( gitree, &keyW, &valW, (UWord)nyu );
   if (already_present) {
      tl_assert(valW == 0);
      nd = (GlobalTreeNode*)keyW;
      tl_assert(nd);
      tl_assert(nd != nyu);
      tl_assert(nd->descr);
      tl_assert(nyu->descr);
      if (nd->addr == nyu->addr && nd->szB == nyu->szB
          
          && 0 == VG_(strcmp)(nd->descr->soname, nyu->descr->soname)) {
         
         sg_free(nyu);
         return;
      }
      
   }

   already_present = VG_(addToFM)( gitree, (UWord)nyu, 0 );
   if (already_present && moans > 0 && !VG_(clo_xml)) {
      moans--;
      VG_(message)(Vg_UserMsg, "Warning: bogus DWARF3 info: "
                               "overlapping global blocks\n");
      if (VG_(clo_verbosity) >= 2) {
         GlobalTree__pp( gitree,
                         "add_block_to_GlobalTree: non-exact duplicate" );
         VG_(printf)("Overlapping block: ");
         GlobalTreeNode__pp(nyu);
         VG_(printf)("\n");
      }
      if (moans == 0)
         VG_(message)(Vg_UserMsg, "Further instances of this "
                                  "message will not be shown\n" );
   }
   
}

static Bool del_GlobalTree_range ( WordFM* gitree,
                                   Addr a, SizeT szB )
{
   UWord keyW, valW, oldK, oldV;
   GlobalTreeNode key, *nd;
   Bool b, anyFound;

   tl_assert(szB > 0);

   anyFound = False;

   key.addr = a;
   key.szB  = szB;

   while (VG_(lookupFM)( gitree, &keyW, &valW, (UWord)&key )) {
      anyFound = True;
      nd = (GlobalTreeNode*)keyW;
      tl_assert(valW == 0);
      tl_assert(nd != &key);
      tl_assert(cmp_nonempty_intervals(a, szB, nd->addr, nd->szB) == 0);

      b = VG_(delFromFM)( gitree, &oldK, &oldV, (UWord)&key );
      tl_assert(b);
      tl_assert(oldV == 0);
      tl_assert(oldK == keyW); 
   }

   return anyFound;
}




typedef
   enum {
      Inv_Unset=1,  
      Inv_Unknown,  
      Inv_Stack0,   
      Inv_StackN,   
      Inv_Global,   
   }
   InvarTag;

typedef
   struct {
      InvarTag tag;
      union {
         struct {
         } Unset;
         struct {
         } Unknown;
         struct {
            Addr  addr;
            SizeT szB;
            StackBlock* descr;
         } Stack0; 
         struct {
            StackTreeNode* nd;
         } StackN; 
         struct {
           GlobalTreeNode* nd;
         } Global;
      }
      Inv;
   }
   Invar;

static void pp_Invar ( Invar* i )
{
   switch (i->tag) {
      case Inv_Unset: 
         VG_(printf)("Unset");
         break;
      case Inv_Unknown:
         VG_(printf)("Unknown");
         break;
      case Inv_Stack0:
         VG_(printf)("Stack0 [%#lx,+%lu)",
                     i->Inv.Stack0.addr, i->Inv.Stack0.szB);
         break;
      case Inv_StackN:
         VG_(printf)("StackN [%#lx,+%lu)",
                     i->Inv.StackN.nd->addr, i->Inv.StackN.nd->szB);
         break;
      case Inv_Global:
         VG_(printf)("Global [%#lx,+%lu)",
                     i->Inv.Global.nd->addr, i->Inv.Global.nd->szB);
         break;
      default:
         tl_assert(0);
   }
}

static Bool eq_Invar ( Invar* i1, Invar* i2 )
{
   if (i1->tag != i2->tag)
      return False;
   switch (i1->tag) {
      case Inv_Unset:
         return True;
      case Inv_Unknown:
         return True;
      case Inv_Stack0:
         return i1->Inv.Stack0.addr == i2->Inv.Stack0.addr
                && i1->Inv.Stack0.szB == i2->Inv.Stack0.szB;
      case Inv_StackN:
         return i1->Inv.StackN.nd == i2->Inv.StackN.nd;
      case Inv_Global:
         return i1->Inv.Global.nd == i2->Inv.Global.nd;
      default:
         tl_assert(0);
   }
   
   tl_assert(0);
}

static void gen_delta_str ( HChar* buf,
                            UWord* absDelta,
                            Invar* inv, Addr ea )
{
   Addr  block = 0;
   SizeT szB   = 0;

   buf[0] = 0;
   *absDelta = 0;

   switch (inv->tag) {
      case Inv_Unknown:
      case Inv_Unset:
         return; 
      case Inv_Stack0:
         block = inv->Inv.Stack0.addr;
         szB   = inv->Inv.Stack0.szB;
         break;
      case Inv_StackN:
         block = inv->Inv.StackN.nd->addr;
         szB   = inv->Inv.StackN.nd->szB;
         break;
      case Inv_Global:
         block = inv->Inv.Global.nd->addr;
         szB = inv->Inv.Global.nd->szB;
         break;
      default:
         tl_assert(0);
   }
   tl_assert(szB > 0);
   if (ea < block) {
      *absDelta = block - ea;
      VG_(sprintf)(buf, "%'lu before", *absDelta);
   }
   else if (ea >= block + szB) {
      *absDelta = ea - (block + szB);
      VG_(sprintf)(buf, "%'lu after", *absDelta);
   }
   else {
     
     VG_(sprintf)(buf, "%'lu inside", ea - block);
   }
}


static void show_Invar( HChar* buf, Word nBuf, Invar* inv, Word depth )
{
   const HChar* str;
   tl_assert(nBuf >= 128);
   buf[0] = 0;
   switch (inv->tag) {
      case Inv_Unknown:
         VG_(sprintf)(buf, "%s", "unknown");
         break;
      case Inv_Stack0:
         str = "array";
         VG_(sprintf)(buf, "stack %s \"%s\" of size %'lu in this frame",
                      str, inv->Inv.Stack0.descr->name,
                      inv->Inv.Stack0.szB );
         break;
      case Inv_StackN:
         str = "array";
         VG_(sprintf)(buf, "stack %s \"%s\" of size %'lu in frame %lu back from here",
                      str, inv->Inv.StackN.nd->descr->name,
                           inv->Inv.StackN.nd->descr->szB,
                           depth - inv->Inv.StackN.nd->depth );
         break;
      case Inv_Global:
         str = "array";
         VG_(sprintf)(buf, "global %s \"%s\" of size %'lu in object with soname \"%s\"",
                      str, inv->Inv.Global.nd->descr->name,
                           inv->Inv.Global.nd->descr->szB,
                           inv->Inv.Global.nd->descr->soname );
         break;
      case Inv_Unset:
         VG_(sprintf)(buf, "%s", "Unset!");
         break;
      default:
         tl_assert(0);
   }
}




#define N_QCACHE 16

#define QCACHE_ADVANCE_EVERY 16

typedef
   struct {
      Addr  addr;
      SizeT szB;
      Invar inv;
   }
   QCElem;

typedef
   struct {
      Word   nInUse;
      QCElem elems[N_QCACHE];
   }
   QCache;

static void QCache__invalidate ( QCache* qc ) {
   tl_assert(qc->nInUse >= 0);
   qc->nInUse = 0;
}

static void QCache__pp ( QCache* qc, const HChar* who )
{
   Word i;
   VG_(printf)("<<< QCache with %ld elements (%s)\n", qc->nInUse, who);
   for (i = 0; i < qc->nInUse; i++) {
      VG_(printf)("  [%#lx,+%#lx) ", qc->elems[i].addr, qc->elems[i].szB);
      pp_Invar(&qc->elems[i].inv);
      VG_(printf)("\n");
   }
   VG_(printf)(">>>\n");
}

static ULong stats__qcache_queries = 0;
static ULong stats__qcache_misses  = 0;
static ULong stats__qcache_probes  = 0;


static  struct _StackFrame**         shadowStacks;

static  WordFM**  siTrees;

static  QCache*                      qcaches;


static WordFM*  giTree;


static void invalidate_all_QCaches ( void )
{
   Word i;
   for (i = 0; i < VG_N_THREADS; i++) {
      QCache__invalidate( &qcaches[i] );
   }
}

static void ourGlobals_init ( void )
{
   Word i;

   shadowStacks = sg_malloc( "di.sg_main.oGi.2",
                             VG_N_THREADS * sizeof shadowStacks[0] );
   siTrees = sg_malloc( "di.sg_main.oGi.3", VG_N_THREADS * sizeof siTrees[0] );
   qcaches = sg_malloc( "di.sg_main.oGi.4", VG_N_THREADS * sizeof qcaches[0] );

   for (i = 0; i < VG_N_THREADS; i++) {
      shadowStacks[i] = NULL;
      siTrees[i] = NULL;
      qcaches[i] = (QCache){};
   }
   invalidate_all_QCaches();
   giTree = VG_(newFM)( sg_malloc, "di.sg_main.oGi.1", sg_free, 
                        (Word(*)(UWord,UWord))cmp_intervals_GlobalTreeNode );
}



static void acquire_globals ( ULong di_handle )
{
   Word n, i;
   XArray*  gbs;
   if (0) VG_(printf)("ACQUIRE GLOBALS %llu\n", di_handle );
   gbs = VG_(di_get_global_blocks_from_dihandle)
            (di_handle, True);
   if (0) VG_(printf)("   GOT %ld globals\n", VG_(sizeXA)( gbs ));

   n = VG_(sizeXA)( gbs );
   for (i = 0; i < n; i++) {
      GlobalBlock* gbp;
      GlobalBlock* gb = VG_(indexXA)( gbs, i );
      if (0) VG_(printf)("   new Global size %2lu at %#lx:  %s %s\n", 
                         gb->szB, gb->addr, gb->soname, gb->name );
      tl_assert(gb->szB > 0);
      gbp = get_persistent_GlobalBlock( gb );
      add_block_to_GlobalTree( giTree, gbp );
   }

   VG_(deleteXA)( gbs );
}


void sg_new_mem_mmap( Addr a, SizeT len,
                      Bool rr, Bool ww, Bool xx, ULong di_handle )
{
   if (di_handle > 0)
      acquire_globals(di_handle);
}
void sg_new_mem_startup( Addr a, SizeT len,
                         Bool rr, Bool ww, Bool xx, ULong di_handle )
{
   if (di_handle > 0)
      acquire_globals(di_handle);
}
void sg_die_mem_munmap ( Addr a, SizeT len )
{
   Bool debug = (Bool)0;
   Bool overlap = False;

   if (debug) VG_(printf)("MUNMAP %#lx %lu\n", a, len );

   if (len == 0)
      return;

   overlap = del_GlobalTree_range(giTree, a, len);

   { 
     UWord keyW, valW;
     VG_(initIterFM)( giTree );
     while (VG_(nextIterFM)( giTree, &keyW, &valW )) {
       GlobalTreeNode* nd = (GlobalTreeNode*)keyW;
        tl_assert(valW == 0);
        tl_assert(nd->szB > 0);
        tl_assert(nd->addr + nd->szB <= a
                  || a + len <= nd->addr);
     }
     VG_(doneIterFM)( giTree );
   }

   if (!overlap)
      return;

   tl_assert(len > 0);
   preen_global_Invars( a, len );
   invalidate_all_QCaches();
}



static ULong stats__total_accesses   = 0;
static ULong stats__classify_Stack0  = 0;
static ULong stats__classify_StackN  = 0;
static ULong stats__classify_Global  = 0;
static ULong stats__classify_Unknown = 0;
static ULong stats__Invars_preened   = 0;
static ULong stats__Invars_changed   = 0;
static ULong stats__t_i_b_empty      = 0;
static ULong stats__htab_fast        = 0;
static ULong stats__htab_searches    = 0;
static ULong stats__htab_probes      = 0;
static ULong stats__htab_resizes     = 0;


typedef
   struct {
      
      Addr    insn_addr; 
      XArray* blocks; 
      
      Invar invar;
   }
   IInstance;


#define N_HTAB_FIXED 64

typedef
   struct _StackFrame {
      Addr creation_sp;
      struct _StackFrame* inner;
      struct _StackFrame* outer;
      Word depth; 
      IInstance* htab;
      UWord      htab_size; 
      UWord      htab_used; 
      Addr sp_at_call;
      Addr fp_at_call;
      XArray*  blocks_added_by_call;
      
      IInstance htab_fixed[N_HTAB_FIXED];
   }
   StackFrame;






__attribute__((noinline))
static void preen_global_Invar ( Invar* inv, Addr a, SizeT len )
{
   stats__Invars_preened++;
   tl_assert(len > 0);
   tl_assert(inv);
   switch (inv->tag) {
      case Inv_Global:
         tl_assert(inv->Inv.Global.nd);
         tl_assert(inv->Inv.Global.nd->szB > 0);
         if (0) VG_(printf)("preen_Invar Global %#lx %lu\n",
                            inv->Inv.Global.nd->addr,
                            inv->Inv.Global.nd->szB);
         if (0 == cmp_nonempty_intervals(a, len, inv->Inv.Global.nd->addr,
                                                 inv->Inv.Global.nd->szB)) {
            inv->tag = Inv_Unknown;
            stats__Invars_changed++;
         }
         break;
      case Inv_Stack0:
      case Inv_StackN:
      case Inv_Unknown:
         break;
      case Inv_Unset: 
         
      default:
         tl_assert(0);
   }
}

__attribute__((noinline))
static void preen_global_Invars ( Addr a, SizeT len )
{
   Int         i;
   UWord       u;
   StackFrame* frame;
   tl_assert(len > 0);
   for (i = 0; i < VG_N_THREADS; i++) {
      frame = shadowStacks[i];
      if (!frame)
         continue; 
      
      while (frame->inner)
         frame = frame->inner;
      tl_assert(frame->outer);
      for (; frame; frame = frame->outer) {
         UWord xx = 0; 
         if (!frame->htab)
            continue; 
         for (u = 0; u < frame->htab_size; u++) {
            IInstance* ii = &frame->htab[u];
            if (ii->insn_addr == 0)
               continue; 
            if (0) { pp_Invar(&ii->invar); VG_(printf)(" x\n"); }
            preen_global_Invar( &ii->invar, a, len );
            xx++;           
         }
         tl_assert(xx == frame->htab_used);
      }
   }
}


inline static UWord compute_II_hash ( Addr ip, UWord htab_size ) {
   return (ip >> 0) & (htab_size - 1);
}

__attribute__((noinline))
static void initialise_II_hash_table ( StackFrame* sf )
{
   UWord i;
   sf->htab_size = N_HTAB_FIXED; 
   sf->htab = &sf->htab_fixed[0];
   tl_assert(sf->htab);
   sf->htab_used = 0;
   for (i = 0; i < sf->htab_size; i++)
      sf->htab[i].insn_addr = 0; 
}


__attribute__((noinline))
static void resize_II_hash_table ( StackFrame* sf )
{
   UWord     i, j, ix, old_size, new_size;
   IInstance *old_htab, *new_htab, *old;

   tl_assert(sf && sf->htab);
   old_size = sf->htab_size;
   new_size = 2 * old_size;
   old_htab = sf->htab;
   new_htab = sg_malloc( "di.sg_main.rIht.1",
                         new_size * sizeof(IInstance) );
   for (i = 0; i < new_size; i++) {
      new_htab[i].insn_addr = 0; 
   }
   for (i = 0; i < old_size; i++) {
      old = &old_htab[i];
      if (old->insn_addr == 0 )
         continue;
      ix = compute_II_hash(old->insn_addr, new_size);
      
      j = new_size;
      while (1) {
         if (new_htab[ix].insn_addr == 0)
            break;
         tl_assert(j > 0);
         j--;
         ix++; if (ix == new_size) ix = 0;
      }
      
      tl_assert(ix < new_size);
      tl_assert(new_htab[ix].insn_addr == 0);
      new_htab[ix] = *old;
      tl_assert(new_htab[ix].insn_addr != 0);
   }
   
   if (old_htab != &sf->htab_fixed[0])
      sg_free(old_htab);
   sf->htab = new_htab;
   sf->htab_size = new_size;
   j = 0;
   for (i = 0; i < new_size; i++) {
      if (new_htab[i].insn_addr != 0) {
         j++;
      }
   }
   tl_assert(j == sf->htab_used);
   if (0) VG_(printf)("resized tab for SF %p to %lu\n", sf, new_size);
}


__attribute__((noinline))
static IInstance* find_or_create_IInstance_SLOW (
                     StackFrame* sf, 
                     Addr ip,
                     XArray*  ip_frameblocks
                  )
{
   UWord i, ix;

   stats__htab_searches++;

   tl_assert(sf);
   tl_assert(sf->htab);

   
   if (UNLIKELY(2 * sf->htab_used >= 1 * sf->htab_size)) {
      stats__htab_resizes++;
      resize_II_hash_table(sf);
   }
   tl_assert(2 * sf->htab_used <= sf->htab_size);
  
   ix = compute_II_hash(ip, sf->htab_size);
   i = sf->htab_size;
   while (1) {
      stats__htab_probes++;
      if (sf->htab[ix].insn_addr == ip)
         return &sf->htab[ix];
      if (sf->htab[ix].insn_addr == 0)
         break;
      tl_assert(i > 0);
      i--;
      ix++;
      if (ix == sf->htab_size) ix = 0;
   }

   
   tl_assert(sf->htab[ix].insn_addr == 0);

   
   tl_assert(ip != 0); 
   sf->htab[ix].insn_addr = ip;
   sf->htab[ix].blocks    = ip_frameblocks;
   sf->htab[ix].invar.tag = Inv_Unset;
   sf->htab_used++;
   return &sf->htab[ix];
}


inline
static IInstance* find_or_create_IInstance (
                     StackFrame* sf, 
                     Addr ip,
                     XArray*  ip_frameblocks
                  )
{
   UWord ix = compute_II_hash(ip, sf->htab_size);
   
   if (LIKELY(sf->htab[ix].insn_addr == ip)) {
      stats__htab_fast++;
      return &sf->htab[ix];
   }
   
   if (LIKELY(sf->htab[ix].insn_addr == 0)) {
      stats__htab_fast++;
      tl_assert(ip != 0);
      sf->htab[ix].insn_addr = ip;
      sf->htab[ix].blocks    = ip_frameblocks;
      sf->htab[ix].invar.tag = Inv_Unset;
      sf->htab_used++;
      return &sf->htab[ix];
   }
   return find_or_create_IInstance_SLOW( sf, ip, ip_frameblocks );
}


__attribute__((noinline))
static Addr calculate_StackBlock_EA ( StackBlock* descr,
                                      Addr sp, Addr fp ) {
   UWord w1 = (UWord)descr->base;
   UWord w2 = (UWord)(descr->spRel ? sp : fp);
   UWord ea = w1 + w2;
   return ea;
}

__attribute__((noinline))
static XArray*  calculate_StackBlock_EAs (
                             XArray*  blocks,
                             Addr sp, Addr fp
                          )
{
   XArray* res;
   Word i, n = VG_(sizeXA)( blocks );
   tl_assert(n > 0);
   res = VG_(newXA)( sg_malloc, "di.sg_main.cSBE.1", sg_free, sizeof(Addr) );
   for (i = 0; i < n; i++) {
      StackBlock* blk = VG_(indexXA)( blocks, i );
      Addr ea = calculate_StackBlock_EA( blk, sp, fp );
      VG_(addToXA)( res, &ea );
   }
   return res;
}


__attribute__((noinline)) 
static void classify_address ( Invar* inv,
                               ThreadId tid,
                               Addr ea, Addr sp, Addr fp,
                               UWord szB,
                               XArray*  thisInstrBlocks )
{
   tl_assert(szB > 0);
   { 
     Word i, nBlocks = VG_(sizeXA)( thisInstrBlocks );
     if (nBlocks == 0) stats__t_i_b_empty++;
     for (i = 0; i < nBlocks; i++) {
        StackBlock* descr = VG_(indexXA)( thisInstrBlocks, i );
        Addr bea = calculate_StackBlock_EA( descr, sp, fp );
        if (bea <= ea && ea + szB <= bea + descr->szB) {
           
           inv->tag = Inv_Stack0;
           inv->Inv.Stack0.addr  = bea;
           inv->Inv.Stack0.szB   = descr->szB;
           inv->Inv.Stack0.descr = descr;
           stats__classify_Stack0++;
           return;
        }
     }
   }
   
   { Word i;
     QCache* cache = &qcaches[tid];
     static UWord ctr = 0;
     stats__qcache_queries++;
     for (i = 0; i < cache->nInUse; i++) {
        if (0) 
               tl_assert(cache->elems[i].addr + cache->elems[i].szB != 0);
        stats__qcache_probes++;
        if (is_subinterval_of(cache->elems[i].addr,
                              cache->elems[i].szB, ea, szB)) {
           if (i > 0
               && (ctr++ & (QCACHE_ADVANCE_EVERY-1)) == 0) {
              QCElem tmp;
              tmp = cache->elems[i-1];
              cache->elems[i-1] = cache->elems[i];
              cache->elems[i] = tmp;
              i--;
           }
           *inv = cache->elems[i].inv;
           return;
        }
     }
     stats__qcache_misses++;
   }
   { StackTreeNode* nd = find_StackTreeNode( siTrees[tid], ea );
     if (nd && !is_subinterval_of(nd->addr, nd->szB, ea, szB)) {
        nd = NULL;
     }
     if (nd) {
        
        inv->tag = Inv_StackN;
        inv->Inv.StackN.nd = nd;
        stats__classify_StackN++;
        goto out;
     }
   }
   
   { GlobalTreeNode* nd = find_GlobalTreeNode(giTree, ea);
     if (nd && !is_subinterval_of(nd->addr, nd->szB, ea, szB)) {
        nd = NULL;
     }
     if (nd) {
        
        inv->tag = Inv_Global;
        inv->Inv.Global.nd = nd;
        stats__classify_Global++;
        goto out;
     }
   }
   
   inv->tag = Inv_Unknown;
   stats__classify_Unknown++;

   
  out:
   { Addr    toadd_addr = 0;
     SizeT   toadd_szB  = 0;
     QCache* cache      = &qcaches[tid];

     static UWord ctr = 0;
     Bool show = False;
     if (0 && 0 == (ctr++ & 0x1FFFFF)) show = True;

     if (show) QCache__pp(cache, "before upd");

     switch (inv->tag) {
        case Inv_Global:
           toadd_addr = inv->Inv.Global.nd->addr;
           toadd_szB  = inv->Inv.Global.nd->szB;
           break;
        case Inv_StackN:
           toadd_addr = inv->Inv.StackN.nd->addr;
           toadd_szB  = inv->Inv.StackN.nd->szB;
           break;
        case Inv_Unknown: {
           StackTreeNode  sNegInf, sPosInf, sKey, *sLB, *sUB;
           GlobalTreeNode gNegInf, gPosInf, gKey, *gLB, *gUB;
           Addr gMin, gMax, sMin, sMax, uMin, uMax;
           Bool sOK, gOK;
           sNegInf.addr = 0;
           sNegInf.szB  = 1;
           sPosInf.addr = ~(UWord)0;
           sPosInf.szB  = 1;
           gNegInf.addr = 0;
           gNegInf.szB  = 1;
           gPosInf.addr = ~(UWord)0;
           gPosInf.szB  = 1;
           sKey.addr = ea;
           sKey.szB  = szB;
           gKey.addr = ea;
           gKey.szB  = szB;
           if (0) VG_(printf)("Tree sizes %ld %ld\n",
                              VG_(sizeFM)(siTrees[tid]), VG_(sizeFM)(giTree));
           sOK = VG_(findBoundsFM)( siTrees[tid], 
                                    (UWord*)&sLB,    NULL,
                                    (UWord*)&sUB,    NULL,
                                    (UWord)&sNegInf, 0,
                                    (UWord)&sPosInf, 0,
                                    (UWord)&sKey );
           gOK = VG_(findBoundsFM)( giTree,
                                    (UWord*)&gLB,    NULL,
                                    (UWord*)&gUB,    NULL,
                                    (UWord)&gNegInf, 0,
                                    (UWord)&gPosInf, 0,
                                    (UWord)&gKey );
           if (!(sOK && gOK)) {
              if (!sOK) {
                 StackTreeNode *ndFirst, *ndLast;
                 ndFirst = find_StackTreeNode( siTrees[tid], ea );
                 ndLast  = find_StackTreeNode( siTrees[tid], ea+szB-1 );
                 if (ndFirst && ndLast)
                    tl_assert(ndFirst != ndLast);
                 if (ndFirst)
                    tl_assert(!is_subinterval_of(ndFirst->addr,
                                                 ndFirst->szB, ea, szB));
                 if (ndLast)
                    tl_assert(!is_subinterval_of(ndLast->addr,
                                                 ndLast->szB, ea, szB));
              }
              if (!gOK) {
                 GlobalTreeNode *ndFirst, *ndLast;
                 ndFirst = find_GlobalTreeNode( giTree, ea );
                 ndLast  = find_GlobalTreeNode( giTree, ea+szB-1 );
                 if (ndFirst && ndLast)
                    tl_assert(ndFirst != ndLast);
                 if (ndFirst)
                    tl_assert(!is_subinterval_of(ndFirst->addr,
                                                 ndFirst->szB, ea, szB));
                 if (ndLast)
                    tl_assert(!is_subinterval_of(ndLast->addr,
                                                 ndLast->szB, ea, szB));
              }
              if (0) VG_(printf)("overlapping blocks in cache\n");
              return;
           }
           sMin = sLB == &sNegInf  ? 0         : (sLB->addr + sLB->szB);
           sMax = sUB == &sPosInf  ? ~(UWord)0 : (sUB->addr - 1);
           gMin = gLB == &gNegInf  ? 0         : (gLB->addr + gLB->szB);
           gMax = gUB == &gPosInf  ? ~(UWord)0 : (gUB->addr - 1);
           if (0) VG_(printf)("sMin %lx sMax %lx gMin %lx gMax %lx\n",
                              sMin, sMax, gMin, gMax);
           tl_assert(sMin <= ea && ea+szB-1 <= sMax);
           tl_assert(gMin <= ea && ea+szB-1 <= gMax);
           
           uMin = Addr__max( sMin, gMin );
           uMax = Addr__min( sMax, gMax );
           if (0) VG_(printf)("uMin %lx uMax %lx\n", uMin, uMax);
           tl_assert(uMin <= uMax);
           tl_assert(uMin <= ea && ea+szB-1 <= uMax);
           if (uMin < uMax && uMax == ~(UWord)0)
              uMax--;
           toadd_addr = uMin;
           toadd_szB  = uMax - uMin + 1;
           break;
        }
        default:
           
          tl_assert(0);
     } 

     { 
       Word i;
       Word ip = cache->nInUse / 2; 

       if (cache->nInUse < N_QCACHE)
          cache->nInUse++;
       for (i = cache->nInUse-1; i > ip; i--) {
          cache->elems[i] = cache->elems[i-1];
       }

       tl_assert(toadd_szB > 0);
       cache->elems[ip].addr = toadd_addr;
       cache->elems[ip].szB  = toadd_szB;
       cache->elems[ip].inv  = *inv;
     }

     if (show) QCache__pp(cache, "after upd");

   }
}


static 
VG_REGPARM(3)
void helperc__mem_access ( 
                           Addr ea, Addr sp, Addr fp,
                           
                           Word sszB, Addr ip, XArray* ip_frameBlocks )
{
   UWord szB;
   IInstance* iinstance;
   Invar* inv;
   Invar new_inv;
   ThreadId tid = VG_(get_running_tid)();
   StackFrame* frame;
   HChar bufE[160], bufA[160], bufD[32];

   stats__total_accesses++;

   tl_assert(is_sane_TId(tid));
   frame = shadowStacks[tid];
   tl_assert(frame);

   
   tl_assert(ip_frameBlocks);
   iinstance = find_or_create_IInstance( frame, ip, ip_frameBlocks );
   tl_assert(iinstance);
   tl_assert(iinstance->blocks == ip_frameBlocks);

   szB = (sszB < 0) ? (-sszB) : sszB;
   tl_assert(szB > 0);

   inv = &iinstance->invar;

   
   if (inv->tag == Inv_Unset) {
      classify_address( inv,
                        tid, ea, sp, fp, szB, iinstance->blocks );
      tl_assert(inv->tag != Inv_Unset);
      return;
   }

   classify_address( &new_inv,
                     tid, ea, sp, fp, szB, iinstance->blocks );
   tl_assert(new_inv.tag != Inv_Unset);

   tl_assert(inv->tag != Inv_Unset);

   if (LIKELY(eq_Invar(&new_inv, inv)))
      return;

   VG_(memset)(bufE, 0, sizeof(bufE));
   show_Invar( bufE, sizeof(bufE)-1, inv, frame->depth );

   VG_(memset)(bufA, 0, sizeof(bufA));
   show_Invar( bufA, sizeof(bufA)-1, &new_inv, frame->depth );

   VG_(memset)(bufD, 0, sizeof(bufD));
   UWord absDelta;
   gen_delta_str( bufD, &absDelta, inv, ea );

   if (absDelta < 1024)
      sg_record_error_SorG( tid, ea, sszB, bufE, bufA, bufD );

   *inv = new_inv;
}



static UWord stats__max_sitree_size = 0;
static UWord stats__max_gitree_size = 0;

static
void shadowStack_new_frame ( ThreadId tid,
                             Addr     sp_at_call_insn,
                             Addr     sp_post_call_insn,
                             Addr     fp_at_call_insn,
                             Addr     ip_post_call_insn,
                             XArray*  descrs_at_call_insn )
{
   StackFrame *callee, *caller;
   tl_assert(is_sane_TId(tid));

   caller = shadowStacks[tid];
   tl_assert(caller);

   if (caller->outer) { 
      tl_assert(caller->outer->inner == caller);
      tl_assert(caller->outer->depth >= 0);
      tl_assert(1 + caller->outer->depth == caller->depth);
   } else {
      tl_assert(caller->depth == 0);
   }

   caller->sp_at_call = sp_at_call_insn;
   caller->fp_at_call = fp_at_call_insn;

   if (descrs_at_call_insn) {
      tl_assert( VG_(sizeXA)(descrs_at_call_insn) > 0 );
      caller->blocks_added_by_call
         = calculate_StackBlock_EAs( descrs_at_call_insn,
                                     sp_at_call_insn, fp_at_call_insn );
      if (caller->blocks_added_by_call)
         add_blocks_to_StackTree( siTrees[tid], 
                                  descrs_at_call_insn,
                                  caller->blocks_added_by_call,
                                  caller->depth 
 );
      if (1) {
         UWord s  = VG_(sizeFM)( siTrees[tid] );
         UWord g  = VG_(sizeFM)( giTree );
         Bool  sb = s > stats__max_sitree_size;
         Bool  gb = g > stats__max_gitree_size;
         if (sb) stats__max_sitree_size = s;
         if (gb) stats__max_gitree_size = g;
         if (0 && (sb || gb))
            VG_(message)(Vg_DebugMsg, 
                         "exp-sgcheck: new max tree sizes: "
                         "StackTree %ld, GlobalTree %ld\n",
                         stats__max_sitree_size, stats__max_gitree_size );
      }
   } else {
      caller->blocks_added_by_call = NULL;
   }


   if (caller->inner) {
      callee = caller->inner;
   } else {
      callee = sg_malloc("di.sg_main.sSnf.1", sizeof(StackFrame));
      VG_(memset)(callee, 0, sizeof(StackFrame));
      callee->outer = caller;
      caller->inner = callee;
      callee->depth = 1 + caller->depth;
      tl_assert(callee->inner == NULL);
   }

   
   initialise_II_hash_table( callee );

   callee->creation_sp    = sp_post_call_insn;
   callee->sp_at_call     = 0; 
   callee->fp_at_call     = 0; 
   callee->blocks_added_by_call = NULL; 

   
   shadowStacks[tid] = callee;

   
   QCache__invalidate( &qcaches[tid] );

   if (0)
   { Word d = callee->depth;
     const HChar *fnname;
     Bool ok;
     Addr ip = ip_post_call_insn;
     ok = VG_(get_fnname_w_offset)( ip, &fnname );
     while (d > 0) {
        VG_(printf)(" ");
        d--;
     }
     VG_(printf)("> %s %#lx\n", ok ? fnname : "???", ip);
   }
}

static
VG_REGPARM(3)
void helperc__new_frame ( Addr sp_post_call_insn,
                          Addr fp_at_call_insn,
                          Addr ip_post_call_insn,
                          XArray* blocks_at_call_insn,
                          Word sp_adjust )
{
   ThreadId tid = VG_(get_running_tid)();
   Addr     sp_at_call_insn = sp_post_call_insn + sp_adjust;
   shadowStack_new_frame( tid,
                          sp_at_call_insn,
                          sp_post_call_insn,
                          fp_at_call_insn,
                          ip_post_call_insn,
                          blocks_at_call_insn );
}



__attribute__((noinline))
static void shadowStack_unwind ( ThreadId tid, Addr sp_now )
{
   StackFrame *innermost, *innermostOrig;
   tl_assert(is_sane_TId(tid));
   innermost = shadowStacks[tid];
   tl_assert(innermost);
   innermostOrig = innermost;
   
   while (1) {
      if (!innermost->outer)
         break;
      if (innermost->inner)
         tl_assert(innermost->inner->outer == innermost);
      tl_assert(innermost->outer->inner == innermost);
      tl_assert(innermost->blocks_added_by_call == NULL);
      if (sp_now <= innermost->creation_sp) break;
      
      tl_assert(innermost->htab);
      if (innermost->htab != &innermost->htab_fixed[0])
         sg_free(innermost->htab);
      
      innermost->creation_sp = 0;
      innermost->htab = NULL;
      innermost->htab_size = 0;
      innermost->htab_used = 0;
      innermost->sp_at_call = 0;
      innermost->fp_at_call = 0;
      innermost->blocks_added_by_call = NULL;
      innermost = innermost->outer;


      if (innermost->outer) { 
         if (innermost->blocks_added_by_call == NULL) {
         } else {
            del_blocks_from_StackTree( siTrees[tid],
                                       innermost->blocks_added_by_call );
            VG_(deleteXA)( innermost->blocks_added_by_call );
            innermost->blocks_added_by_call = NULL;
         }
      }

      if (0) {
         Word d = innermost->depth;
         while (d > 0) {
            VG_(printf)(" ");
            d--;
         }
         VG_(printf)("X\n");
      }

   }

   tl_assert(innermost);

   if (innermost != innermostOrig) {
      shadowStacks[tid] = innermost;
      
      QCache__invalidate( &qcaches[tid] );
   }
}






struct _SGEnv {
   
   Addr curr_IP;
   
   Bool curr_IP_known;
   Bool firstRef;
   
   IRTemp (*newIRTemp_cb)(IRType,void*);
   void* newIRTemp_opaque;
};



static IRTemp gen_Get_SP ( struct _SGEnv*  sge,
                           IRSB*           bbOut,
                           const VexGuestLayout* layout,
                           Int             hWordTy_szB )
{
   IRExpr* sp_expr;
   IRTemp  sp_temp;
   IRType  sp_type;
   tl_assert(hWordTy_szB == layout->sizeof_SP);
   sp_type = layout->sizeof_SP == 8 ? Ity_I64 : Ity_I32;
   sp_expr = IRExpr_Get( layout->offset_SP, sp_type );
   sp_temp = sge->newIRTemp_cb( sp_type, sge->newIRTemp_opaque );
   addStmtToIRSB( bbOut, IRStmt_WrTmp( sp_temp, sp_expr ) );
   return sp_temp;
}

static IRTemp gen_Get_FP ( struct _SGEnv*  sge,
                           IRSB*           bbOut,
                           const VexGuestLayout* layout,
                           Int             hWordTy_szB )
{
   IRExpr* fp_expr;
   IRTemp  fp_temp;
   IRType  fp_type;
   tl_assert(hWordTy_szB == layout->sizeof_SP);
   fp_type = layout->sizeof_FP == 8 ? Ity_I64 : Ity_I32;
   fp_expr = IRExpr_Get( layout->offset_FP, fp_type );
   fp_temp = sge->newIRTemp_cb( fp_type, sge->newIRTemp_opaque );
   addStmtToIRSB( bbOut, IRStmt_WrTmp( fp_temp, fp_expr ) );
   return fp_temp;
}

static void instrument_mem_access ( struct _SGEnv* sge,
                                    IRSB*   bbOut, 
                                    IRExpr* addr,
                                    Int     szB,
                                    Bool    isStore,
                                    Int     hWordTy_szB,
                                    Addr    curr_IP,
                                    const VexGuestLayout* layout )
{
   IRType  tyAddr      = Ity_INVALID;
   XArray* frameBlocks = NULL;

   tl_assert(isIRAtom(addr));
   tl_assert(hWordTy_szB == 4 || hWordTy_szB == 8);

   tyAddr = typeOfIRExpr( bbOut->tyenv, addr );
   tl_assert(tyAddr == Ity_I32 || tyAddr == Ity_I64);

#if defined(VGA_x86)
   { UChar* p = (UChar*)curr_IP;
     
     if (p[0] == 0xc3 && p[-1] == 0x5d) return;
     
     if (p[0] == 0xc2 && p[-1] == 0x5d) return;
     
     if (p[0] == 0x55 && p[1] == 0x89 && p[2] == 0xe5) return;
   }
#endif

   
   frameBlocks = get_StackBlocks_for_IP( curr_IP );
   tl_assert(frameBlocks);
   
   

   { IRTemp t_SP = gen_Get_SP( sge, bbOut, layout, hWordTy_szB );
     IRTemp t_FP = gen_Get_FP( sge, bbOut, layout, hWordTy_szB );
     IRExpr** args
        = mkIRExprVec_6( addr,
                         IRExpr_RdTmp(t_SP),
                         IRExpr_RdTmp(t_FP),
                         mkIRExpr_HWord( isStore ? (-szB) : szB ),
                         mkIRExpr_HWord( curr_IP ),
                         mkIRExpr_HWord( (HWord)frameBlocks ) );
     IRDirty* di
        = unsafeIRDirty_0_N( 3, 
                             "helperc__mem_access", 
                            VG_(fnptr_to_fnentry)( &helperc__mem_access ),
                             args );

     addStmtToIRSB( bbOut, IRStmt_Dirty(di) );
   }
}



struct _SGEnv * sg_instrument_init ( IRTemp (*newIRTemp_cb)(IRType,void*),
                                     void* newIRTemp_opaque )
{
   struct _SGEnv * env = sg_malloc("di.sg_main.sii.1",
                                   sizeof(struct _SGEnv));
   tl_assert(env);
   env->curr_IP          = 0;
   env->curr_IP_known    = False;
   env->firstRef         = True;
   env->newIRTemp_cb     = newIRTemp_cb;
   env->newIRTemp_opaque = newIRTemp_opaque;
   return env;
}

void sg_instrument_fini ( struct _SGEnv * env )
{
   sg_free(env);
}

void sg_instrument_IRStmt ( struct _SGEnv * env, 
                            IRSB* sbOut,
                            IRStmt* st,
                            const VexGuestLayout* layout,
                            IRType gWordTy, IRType hWordTy )
{
   if (!sg_clo_enable_sg_checks)
      return;

   tl_assert(st);
   tl_assert(isFlatIRStmt(st));
   switch (st->tag) {
      case Ist_NoOp:
      case Ist_AbiHint:
      case Ist_Put:
      case Ist_PutI:
      case Ist_MBE:
         
         break;

      case Ist_Exit:
         tl_assert(st->Ist.Exit.jk != Ijk_Call);
         
         break;

      case Ist_IMark:
         env->curr_IP_known = True;
         env->curr_IP       = st->Ist.IMark.addr;
         env->firstRef      = True;
         break;

      case Ist_Store:
         tl_assert(env->curr_IP_known);
         if (env->firstRef) {
            instrument_mem_access( 
               env, sbOut, 
               st->Ist.Store.addr, 
               sizeofIRType(typeOfIRExpr(sbOut->tyenv, st->Ist.Store.data)),
               True,
               sizeofIRType(hWordTy),
               env->curr_IP, layout
            );
            env->firstRef = False;
         }
         break;

      case Ist_WrTmp: {
         IRExpr* data = st->Ist.WrTmp.data;
         if (data->tag == Iex_Load) {
            tl_assert(env->curr_IP_known);
            if (env->firstRef) {
               instrument_mem_access(
                  env, sbOut,
                  data->Iex.Load.addr,
                  sizeofIRType(data->Iex.Load.ty),
                  False,
                  sizeofIRType(hWordTy),
                  env->curr_IP, layout
               );
               env->firstRef = False;
            }
         }
         break;
      }

      case Ist_Dirty: {
         Int      dataSize;
         IRDirty* d = st->Ist.Dirty.details;
         if (d->mFx != Ifx_None) {
            tl_assert(env->curr_IP_known);
            if (env->firstRef) {
               tl_assert(d->mAddr != NULL);
               tl_assert(d->mSize != 0);
               dataSize = d->mSize;
               if (d->mFx == Ifx_Read || d->mFx == Ifx_Modify) {
                  instrument_mem_access( 
                     env, sbOut, d->mAddr, dataSize, False,
                     sizeofIRType(hWordTy), env->curr_IP, layout
                  );
               }
               if (d->mFx == Ifx_Write || d->mFx == Ifx_Modify) {
                  instrument_mem_access( 
                     env, sbOut, d->mAddr, dataSize, True,
                     sizeofIRType(hWordTy), env->curr_IP, layout
                  );
               }
               env->firstRef = False;
            }
         } else {
            tl_assert(d->mAddr == NULL);
            tl_assert(d->mSize == 0);
         }
         break;
      }

      case Ist_CAS: {
         if (env->firstRef) {
            Int    dataSize;
            IRCAS* cas = st->Ist.CAS.details;
            tl_assert(cas->addr != NULL);
            tl_assert(cas->dataLo != NULL);
            dataSize = sizeofIRType(typeOfIRExpr(sbOut->tyenv, cas->dataLo));
            if (cas->dataHi != NULL)
               dataSize *= 2; 
            instrument_mem_access(
               env, sbOut, cas->addr, dataSize, False,
               sizeofIRType(hWordTy), env->curr_IP, layout
            );
            instrument_mem_access(
               env, sbOut, cas->addr, dataSize, True,
               sizeofIRType(hWordTy), env->curr_IP, layout
            );
            env->firstRef = False;
         }
         break;
      }

      default:
         tl_assert(0);

   } 
}


void sg_instrument_final_jump ( struct _SGEnv * env, 
                                IRSB* sbOut,
                                IRExpr* next,
                                IRJumpKind jumpkind,
                                const VexGuestLayout* layout,
                                IRType gWordTy, IRType hWordTy )
{
   if (!sg_clo_enable_sg_checks)
      return;

   if (jumpkind == Ijk_Call) {
      
      IRTemp   sp_post_call_insn, fp_post_call_insn;
      XArray*  frameBlocks;
      IRExpr** args;
      IRDirty* di;
      sp_post_call_insn
         = gen_Get_SP( env, sbOut, layout, sizeofIRType(hWordTy) );
      fp_post_call_insn
         = gen_Get_FP( env, sbOut, layout, sizeofIRType(hWordTy) );
      tl_assert(env->curr_IP_known);
      frameBlocks = get_StackBlocks_for_IP( env->curr_IP );
      tl_assert(frameBlocks);
      if (VG_(sizeXA)(frameBlocks) == 0)
         frameBlocks = NULL;
      args
         = mkIRExprVec_5(
              IRExpr_RdTmp(sp_post_call_insn),
              IRExpr_RdTmp(fp_post_call_insn), 
                         
              next,
              mkIRExpr_HWord( (HWord)frameBlocks ),
              mkIRExpr_HWord( sizeofIRType(gWordTy) )
           );
      di = unsafeIRDirty_0_N(
              3,
              "helperc__new_frame",
              VG_(fnptr_to_fnentry)( &helperc__new_frame ),
              args ); 
      addStmtToIRSB( sbOut, IRStmt_Dirty(di) );
   }
}





static StackFrame* new_root_StackFrame ( void )
{
   StackFrame* sframe = sg_malloc("di.sg_main.nrS.1", sizeof(StackFrame));
   VG_(memset)( sframe, 0, sizeof(*sframe) );
   sframe->creation_sp = ~0UL;

   
   initialise_II_hash_table( sframe );

   

   return sframe;
}

static void shadowStack_thread_create ( ThreadId parent, ThreadId child )
{
   tl_assert(is_sane_TId(child));
   if (parent == VG_INVALID_THREADID) {
      
   } else {
      tl_assert(is_sane_TId(parent));
      tl_assert(parent != child);
      tl_assert(shadowStacks[parent] != NULL);
      tl_assert(siTrees[parent] != NULL);
   }

   if (shadowStacks[child] == NULL) {
      
      tl_assert(siTrees[child] == NULL);
   } else {
      StackFrame *frame, *frame2;
      
      
      tl_assert(siTrees[child] != NULL);
      delete_StackTree( siTrees[child] );
      siTrees[child] = NULL;
      
      frame = shadowStacks[child];
      while (frame->outer)
         frame = frame->outer;
      tl_assert(frame->depth == 0);
      while (frame) {
         frame2 = frame->inner;
         if (frame2) tl_assert(1 + frame->depth == frame2->depth);
         sg_free(frame);
         frame = frame2;
      }
      shadowStacks[child] = NULL;
   }

   tl_assert(shadowStacks[child] == NULL);
   tl_assert(siTrees[child] == NULL);

   
   shadowStacks[child] = new_root_StackFrame();

   
   siTrees[child] = new_StackTree();
}

static void shadowStack_set_initial_SP ( ThreadId tid )
{
   StackFrame* sf;
   tl_assert(is_sane_TId(tid));
   sf = shadowStacks[tid];
   tl_assert(sf != NULL);
   tl_assert(sf->outer == NULL);
   tl_assert(sf->inner == NULL);
   tl_assert(sf->creation_sp == ~0UL);
   shadowStack_new_frame( tid, 0, VG_(get_SP)(tid),
                               0, VG_(get_IP)(tid), NULL );
}



void sg_die_mem_stack ( Addr old_SP, SizeT len ) {
   ThreadId  tid = VG_(get_running_tid)();
   shadowStack_unwind( tid, old_SP+len );
}

void sg_pre_clo_init ( void ) {
   ourGlobals_init();
   init_StackBlocks_set();
   init_GlobalBlock_set();
}

void sg_post_clo_init ( void ) {
}

void sg_pre_thread_ll_create ( ThreadId parent, ThreadId child ) {
   shadowStack_thread_create(parent, child);
}

void sg_pre_thread_first_insn ( ThreadId tid ) {
   shadowStack_set_initial_SP(tid);
}

void sg_fini(Int exitcode)
{
   if (VG_(clo_stats)) {
      VG_(message)(Vg_DebugMsg,
         " sg_:  %'llu total accesses, of which:\n", stats__total_accesses);
      VG_(message)(Vg_DebugMsg,
         " sg_:     stack0: %'12llu classify\n",
         stats__classify_Stack0);
      VG_(message)(Vg_DebugMsg,
         " sg_:     stackN: %'12llu classify\n",
         stats__classify_StackN);
      VG_(message)(Vg_DebugMsg,
         " sg_:     global: %'12llu classify\n",
         stats__classify_Global);
      VG_(message)(Vg_DebugMsg,
         " sg_:    unknown: %'12llu classify\n",
         stats__classify_Unknown);
      VG_(message)(Vg_DebugMsg,
         " sg_:  %'llu Invars preened, of which %'llu changed\n",
         stats__Invars_preened, stats__Invars_changed);
      VG_(message)(Vg_DebugMsg,
         " sg_:   t_i_b_MT: %'12llu\n", stats__t_i_b_empty);
      VG_(message)(Vg_DebugMsg, 
         " sg_:     qcache: %'llu searches, %'llu probes, %'llu misses\n",
         stats__qcache_queries, stats__qcache_probes, stats__qcache_misses);
      VG_(message)(Vg_DebugMsg, 
         " sg_:  htab-fast: %'llu hits\n",
         stats__htab_fast);
      VG_(message)(Vg_DebugMsg, 
         " sg_:  htab-slow: %'llu searches, %'llu probes, %'llu resizes\n",
         stats__htab_searches, stats__htab_probes, stats__htab_resizes);
   }
}


