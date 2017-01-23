

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2007-2013 Julian Seward
      jseward@acm.org

   This code is based on previous work by Nicholas Nethercote
   (coregrind/m_oset.c) which is

   Copyright (C) 2005-2013 Nicholas Nethercote
       njn@valgrind.org

   which in turn was derived partially from:

      AVL C library
      Copyright (C) 2000,2002  Daniel Nagy

      This program is free software; you can redistribute it and/or
      modify it under the terms of the GNU General Public License as
      published by the Free Software Foundation; either version 2 of
      the License, or (at your option) any later version.
      [...]

      (taken from libavl-0.4/debian/copyright)

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
#include "pub_core_libcassert.h"
#include "pub_core_libcbase.h"
#include "pub_core_wordfm.h"   



typedef
   struct _AvlNode {
      UWord key;
      UWord val;
      struct _AvlNode* child[2]; 
      Char balance; 
   }
   AvlNode;

typedef 
   struct {
      UWord w;
      Bool b;
   }
   MaybeWord;

#define WFM_STKMAX    32    

struct _WordFM {
   AvlNode* root;
   void*    (*alloc_nofail)( const HChar*, SizeT );
   const HChar* cc;
   void     (*dealloc)(void*);
   Word     (*kCmp)(UWord,UWord);
   AvlNode* nodeStack[WFM_STKMAX]; 
   Int      numStack[WFM_STKMAX];  
   Int      stackTop;              
}; 

static Bool avl_removeroot_wrk(AvlNode** t, Word(*kCmp)(UWord,UWord));

static void avl_swl ( AvlNode** root )
{
   AvlNode* a  = *root;
   AvlNode* b  = a->child[1];
   *root       = b;
   a->child[1] = b->child[0];
   b->child[0] = a;
}

static void avl_swr ( AvlNode** root )
{
   AvlNode* a  = *root;
   AvlNode* b  = a->child[0];
   *root       = b;
   a->child[0] = b->child[1];
   b->child[1] = a;
}

static void avl_nasty ( AvlNode* root )
{
   switch (root->balance) {
      case -1: 
         root->child[0]->balance = 0;
         root->child[1]->balance = 1;
         break;
      case 1:
         root->child[0]->balance = -1;
         root->child[1]->balance = 0;
         break;
      case 0:
         root->child[0]->balance = 0;
         root->child[1]->balance = 0;
         break;
      default:
         vg_assert(0);
   }
   root->balance=0;
}

static UWord size_avl_nonNull ( const AvlNode* nd )
{
   return 1 + (nd->child[0] ? size_avl_nonNull(nd->child[0]) : 0)
            + (nd->child[1] ? size_avl_nonNull(nd->child[1]) : 0);
}

static inline Word cmp_unsigned_Words ( UWord w1, UWord w2 ) {
   if (w1 < w2) return -1;
   if (w1 > w2) return 1;
   return 0;
}

static 
Bool avl_insert_wrk ( AvlNode**         rootp, 
                      MaybeWord* oldV,
                      AvlNode*          a, 
                      Word              (*kCmp)(UWord,UWord) )
{
   Word cmpres;

   
   a->child[0] = 0;
   a->child[1] = 0;
   a->balance  = 0;
   oldV->b     = False;

   
   if (!(*rootp)) {
      (*rootp) = a;
      return True;
   }

   cmpres = kCmp ?    kCmp( (*rootp)->key, a->key )
                 :  cmp_unsigned_Words( (UWord)(*rootp)->key,
                                                   (UWord)a->key );

   if (cmpres > 0) {
      
      if ((*rootp)->child[0]) {
         AvlNode* left_subtree = (*rootp)->child[0];
         if (avl_insert_wrk(&left_subtree, oldV, a, kCmp)) {
            switch ((*rootp)->balance--) {
               case  1: return False;
               case  0: return True;
               case -1: break;
               default: vg_assert(0);
            }
            if ((*rootp)->child[0]->balance < 0) {
               avl_swr( rootp );
               (*rootp)->balance = 0;
               (*rootp)->child[1]->balance = 0;
            } else {
               avl_swl( &((*rootp)->child[0]) );
               avl_swr( rootp );
               avl_nasty( *rootp );
            }
         } else {
            (*rootp)->child[0] = left_subtree;
         }
         return False;
      } else {
         (*rootp)->child[0] = a;
         if ((*rootp)->balance--) 
            return False;
         return True;
      }
      vg_assert(0);
   }
   else 
   if (cmpres < 0) {
      
      if ((*rootp)->child[1]) {
         AvlNode* right_subtree = (*rootp)->child[1];
         if (avl_insert_wrk(&right_subtree, oldV, a, kCmp)) {
            switch((*rootp)->balance++){
               case -1: return False;
               case  0: return True;
               case  1: break;
               default: vg_assert(0);
            }
            if ((*rootp)->child[1]->balance > 0) {
               avl_swl( rootp );
               (*rootp)->balance = 0;
               (*rootp)->child[0]->balance = 0;
            } else {
               avl_swr( &((*rootp)->child[1]) );
               avl_swl( rootp );
               avl_nasty( *rootp );
            }
         } else {
            (*rootp)->child[1] = right_subtree;
         }
         return False;
      } else {
         (*rootp)->child[1] = a;
         if ((*rootp)->balance++) 
            return False;
         return True;
      }
      vg_assert(0);
   }
   else {
      oldV->b = True;
      oldV->w = (*rootp)->val;
      (*rootp)->val = a->val;
      return False;
   }
}

static
Bool avl_remove_wrk ( AvlNode** rootp, 
                      AvlNode*  a, 
                      Word(*kCmp)(UWord,UWord) )
{
   Bool ch;
   Word cmpres;
   cmpres = kCmp ?    kCmp( (*rootp)->key, a->key )
                 :  cmp_unsigned_Words( (UWord)(*rootp)->key,
                                                   (UWord)a->key );

   if (cmpres > 0){
      
      AvlNode* left_subtree = (*rootp)->child[0];
      vg_assert(left_subtree);
      ch = avl_remove_wrk(&left_subtree, a, kCmp);
      (*rootp)->child[0]=left_subtree;
      if (ch) {
         switch ((*rootp)->balance++) {
            case -1: return True;
            case  0: return False;
            case  1: break;
            default: vg_assert(0);
         }
         switch ((*rootp)->child[1]->balance) {
            case 0:
               avl_swl( rootp );
               (*rootp)->balance = -1;
               (*rootp)->child[0]->balance = 1;
               return False;
            case 1: 
               avl_swl( rootp );
               (*rootp)->balance = 0;
               (*rootp)->child[0]->balance = 0;
               return True;
            case -1:
               break;
            default:
               vg_assert(0);
         }
         avl_swr( &((*rootp)->child[1]) );
         avl_swl( rootp );
         avl_nasty( *rootp );
         return True;
      }
   }
   else
   if (cmpres < 0) {
      
      AvlNode* right_subtree = (*rootp)->child[1];
      vg_assert(right_subtree);
      ch = avl_remove_wrk(&right_subtree, a, kCmp);
      (*rootp)->child[1] = right_subtree;
      if (ch) {
         switch ((*rootp)->balance--) {
            case  1: return True;
            case  0: return False;
            case -1: break;
            default: vg_assert(0);
         }
         switch ((*rootp)->child[0]->balance) {
            case 0:
               avl_swr( rootp );
               (*rootp)->balance = 1;
               (*rootp)->child[1]->balance = -1;
               return False;
            case -1:
               avl_swr( rootp );
               (*rootp)->balance = 0;
               (*rootp)->child[1]->balance = 0;
               return True;
            case 1:
               break;
            default:
               vg_assert(0);
         }
         avl_swl( &((*rootp)->child[0]) );
         avl_swr( rootp );
         avl_nasty( *rootp );
         return True;
      }
   }
   else {
      vg_assert(cmpres == 0);
      vg_assert((*rootp)==a);
      return avl_removeroot_wrk(rootp, kCmp);
   }
   return 0;
}

static 
Bool avl_removeroot_wrk ( AvlNode** rootp, 
                          Word(*kCmp)(UWord,UWord) )
{
   Bool     ch;
   AvlNode* a;
   if (!(*rootp)->child[0]) {
      if (!(*rootp)->child[1]) {
         (*rootp) = 0;
         return True;
      }
      (*rootp) = (*rootp)->child[1];
      return True;
   }
   if (!(*rootp)->child[1]) {
      (*rootp) = (*rootp)->child[0];
      return True;
   }
   if ((*rootp)->balance < 0) {
      
      a = (*rootp)->child[0];
      while (a->child[1]) a = a->child[1];
   } else {
      
      a = (*rootp)->child[1];
      while (a->child[0]) a = a->child[0];
   }
   ch = avl_remove_wrk(rootp, a, kCmp);
   a->child[0] = (*rootp)->child[0];
   a->child[1] = (*rootp)->child[1];
   a->balance  = (*rootp)->balance;
   (*rootp)    = a;
   if(a->balance == 0) return ch;
   return False;
}

static 
AvlNode* avl_find_node ( AvlNode* t, Word k, Word(*kCmp)(UWord,UWord) )
{
   if (kCmp) {
      
      Word cmpresS;
      while (True) {
         if (t == NULL) return NULL;
         cmpresS = kCmp(t->key, k);
         if (cmpresS > 0) t = t->child[0]; else
         if (cmpresS < 0) t = t->child[1]; else
         return t;
      }
   } else {
      
      Word  cmpresS; 
      UWord cmpresU; 
      while (True) {
         if (t == NULL) return NULL; 
         cmpresS = cmp_unsigned_Words( (UWord)t->key, (UWord)k );
         if (cmpresS == 0) return t; 
         cmpresU = (UWord)cmpresS;
         cmpresU >>= (8 * sizeof(cmpresU) - 1);
         t = t->child[cmpresU];
      }
   }
}

static
Bool avl_find_bounds ( const AvlNode* t, 
                       UWord* kMinP, UWord* vMinP,
                       UWord* kMaxP, UWord* vMaxP,
                       UWord minKey, UWord minVal,
                       UWord maxKey, UWord maxVal,
                       UWord key,
                       Word(*kCmp)(UWord,UWord) )
{
   UWord kLowerBound = minKey;
   UWord vLowerBound = minVal;
   UWord kUpperBound = maxKey;
   UWord vUpperBound = maxVal;
   while (t) {
      Word cmpresS = kCmp ? kCmp(t->key, key)
                          : cmp_unsigned_Words(t->key, key);
      if (cmpresS < 0) {
         kLowerBound = t->key;
         vLowerBound = t->val;
         t = t->child[1];
         continue;
      }
      if (cmpresS > 0) {
         kUpperBound = t->key;
         vUpperBound = t->val;
         t = t->child[0];
         continue;
      }
      return False;
   }
   if (kMinP) *kMinP = kLowerBound;
   if (vMinP) *vMinP = vLowerBound;
   if (kMaxP) *kMaxP = kUpperBound;
   if (vMaxP) *vMaxP = vUpperBound;
   return True;
}

static void stackClear(WordFM* fm)
{
   Int i;
   vg_assert(fm);
   for (i = 0; i < WFM_STKMAX; i++) {
      fm->nodeStack[i] = NULL;
      fm->numStack[i]  = 0;
   }
   fm->stackTop = 0;
}

static inline void stackPush(WordFM* fm, AvlNode* n, Int i)
{
   vg_assert(fm->stackTop < WFM_STKMAX);
   vg_assert(1 <= i && i <= 3);
   fm->nodeStack[fm->stackTop] = n;
   fm-> numStack[fm->stackTop] = i;
   fm->stackTop++;
}

static inline Bool stackPop(WordFM* fm, AvlNode** n, Int* i)
{
   vg_assert(fm->stackTop <= WFM_STKMAX);

   if (fm->stackTop > 0) {
      fm->stackTop--;
      *n = fm->nodeStack[fm->stackTop];
      *i = fm-> numStack[fm->stackTop];
      vg_assert(1 <= *i && *i <= 3);
      fm->nodeStack[fm->stackTop] = NULL;
      fm-> numStack[fm->stackTop] = 0;
      return True;
   } else {
      return False;
   }
}

static 
AvlNode* avl_dopy ( const AvlNode* nd, 
                    UWord(*dopyK)(UWord), 
                    UWord(*dopyV)(UWord),
                    void*(alloc_nofail)(const HChar*,SizeT),
                    const HChar* cc )
{
   AvlNode* nyu;

   vg_assert(nd != NULL);

   nyu = alloc_nofail(cc, sizeof(AvlNode));
   
   nyu->child[0] = nd->child[0];
   nyu->child[1] = nd->child[1];
   nyu->balance = nd->balance;

   
   if (dopyK) {
      nyu->key = dopyK( nd->key );
   } else {
      
      nyu->key = nd->key;
   }

   
   if (dopyV) {
      nyu->val = dopyV( nd->val );
   } else {
      
      nyu->val = nd->val;
   }

   
   if (nyu->child[0]) {
      nyu->child[0] = avl_dopy( nyu->child[0], dopyK, dopyV, 
                                alloc_nofail, cc );
   }
   if (nyu->child[1]) {
      nyu->child[1] = avl_dopy( nyu->child[1], dopyK, dopyV,
                                alloc_nofail, cc );
   }

   return nyu;
}

static void initFM ( WordFM* fm,
                     void*   (*alloc_nofail)( const HChar*, SizeT ),
                     const HChar* cc,
                     void    (*dealloc)(void*),
                     Word    (*kCmp)(UWord,UWord) )
{
   fm->root         = 0;
   fm->kCmp         = kCmp;
   fm->alloc_nofail = alloc_nofail;
   fm->cc           = cc;
   fm->dealloc      = dealloc;
   fm->stackTop     = 0;
}


WordFM* VG_(newFM) ( void* (*alloc_nofail)( const HChar*, SizeT ),
                     const HChar* cc,
                     void  (*dealloc)(void*),
                     Word  (*kCmp)(UWord,UWord) )
{
   WordFM* fm = alloc_nofail(cc, sizeof(WordFM));
   initFM(fm, alloc_nofail, cc, dealloc, kCmp);
   return fm;
}

static void avl_free ( AvlNode* nd, 
                       void(*kFin)(UWord),
                       void(*vFin)(UWord),
                       void(*dealloc)(void*) )
{
   if (!nd)
      return;
   if (nd->child[0])
      avl_free(nd->child[0], kFin, vFin, dealloc);
   if (nd->child[1])
      avl_free(nd->child[1], kFin, vFin, dealloc);
   if (kFin)
      kFin( nd->key );
   if (vFin)
      vFin( nd->val );
   VG_(memset)(nd, 0, sizeof(AvlNode));
   dealloc(nd);
}

void VG_(deleteFM) ( WordFM* fm, void(*kFin)(UWord), void(*vFin)(UWord) )
{
   void(*dealloc)(void*) = fm->dealloc;
   avl_free( fm->root, kFin, vFin, dealloc );
   VG_(memset)(fm, 0, sizeof(WordFM) );
   dealloc(fm);
}

Bool VG_(addToFM) ( WordFM* fm, UWord k, UWord v )
{
   MaybeWord oldV;
   AvlNode* node;
   node = fm->alloc_nofail( fm->cc, sizeof(AvlNode) );
   node->key = k;
   node->val = v;
   oldV.b = False;
   oldV.w = 0;
   avl_insert_wrk( &fm->root, &oldV, node, fm->kCmp );
   
   
   if (oldV.b)
      fm->dealloc(node);
   return oldV.b;
}

Bool VG_(delFromFM) ( WordFM* fm,
                      UWord* oldK, UWord* oldV, UWord key )
{
   AvlNode* node = avl_find_node( fm->root, key, fm->kCmp );
   if (node) {
      avl_remove_wrk( &fm->root, node, fm->kCmp );
      if (oldK)
         *oldK = node->key;
      if (oldV)
         *oldV = node->val;
      fm->dealloc(node);
      return True;
   } else {
      return False;
   }
}

Bool VG_(lookupFM) ( const WordFM* fm, 
                     UWord* keyP, UWord* valP, UWord key )
{
   AvlNode* node = avl_find_node( fm->root, key, fm->kCmp );
   if (node) {
      if (keyP)
         *keyP = node->key;
      if (valP)
         *valP = node->val;
      return True;
   } else {
      return False;
   }
}

Bool VG_(findBoundsFM)( const WordFM* fm,
                        UWord* kMinP, UWord* vMinP,
                        UWord* kMaxP, UWord* vMaxP,
                        UWord minKey, UWord minVal,
                        UWord maxKey, UWord maxVal,
                        UWord key )
{
   return avl_find_bounds( fm->root, kMinP, vMinP,
                                     kMaxP, vMaxP,
                                     minKey, minVal, 
                                     maxKey, maxVal,
                                     key, fm->kCmp );
}

UWord VG_(sizeFM) ( const WordFM* fm )
{
   
   return fm->root ? size_avl_nonNull( fm->root ) : 0;
}


void VG_(initIterFM) ( WordFM* fm )
{
   vg_assert(fm);
   stackClear(fm);
   if (fm->root)
      stackPush(fm, fm->root, 1);
}

void VG_(initIterAtFM) ( WordFM* fm, UWord start_at )
{
   Int     i;
   AvlNode *n, *t;
   Word    cmpresS; 
   UWord   cmpresU; 

   vg_assert(fm);
   stackClear(fm);

   if (!fm->root) 
      return;

   n = NULL;
   
   t = fm->root;

   while (True) {
      if (t == NULL) return;

      cmpresS 
         = fm->kCmp ?    fm->kCmp( t->key, start_at )
                    :  cmp_unsigned_Words( t->key, start_at );

      if (cmpresS == 0) {
         
         
         stackPush(fm, t, 2);
         
         return;
      }
      cmpresU = (UWord)cmpresS;
      cmpresU >>= (8 * sizeof(cmpresU) - 1);
      if (!cmpresU) {
         
         stackPush(fm, t, 2);
      }
      t = t->child[cmpresU];
   }
   if (stackPop(fm, &n, &i)) {
      
      
      vg_assert(i == 2);
      stackPush(fm, n, 3);
      
   }
}

Bool VG_(nextIterFM) ( WordFM* fm, UWord* pKey, UWord* pVal )
{
   Int i = 0;
   AvlNode* n = NULL;
   
   vg_assert(fm);

   
   
   
   
   while (stackPop(fm, &n, &i)) {
      switch (i) {
      case 1: case_1:
         stackPush(fm, n, 2);
         
         if (n->child[0]) { n = n->child[0]; goto case_1; }
         break;
      case 2: 
         stackPush(fm, n, 3);
         if (pKey) *pKey = n->key;
         if (pVal) *pVal = n->val;
         return True;
      case 3:
         
         if (n->child[1]) { n = n->child[1]; goto case_1; }
         break;
      default:
         vg_assert(0);
      }
   }

   
   return False;
}

void VG_(doneIterFM) ( WordFM* fm )
{
}

WordFM* VG_(dopyFM) ( WordFM* fm, UWord(*dopyK)(UWord),
                      UWord(*dopyV)(UWord) )
{
   WordFM* nyu; 

   
   vg_assert(fm->stackTop == 0);

   nyu = fm->alloc_nofail( fm->cc, sizeof(WordFM) );

   *nyu = *fm;

   fm->stackTop = 0;
   VG_(memset)(fm->nodeStack, 0, sizeof(fm->nodeStack));
   VG_(memset)(fm->numStack, 0,  sizeof(fm->numStack));

   if (nyu->root) {
      nyu->root = avl_dopy( nyu->root, dopyK, dopyV,
                            fm->alloc_nofail, fm->cc );
      if (! nyu->root)
         return NULL;
   }

   return nyu;
}



struct _WordBag { 
   WordFM* fm; 
};

WordBag* VG_(newBag) ( void* (*alloc_nofail)( const HChar*, SizeT ),
                       const HChar* cc,
                       void  (*dealloc)(void*) )
{
   WordBag* bag = alloc_nofail(cc, sizeof(WordBag));
   bag->fm = VG_(newFM)( alloc_nofail, cc, dealloc, NULL );
   return bag;
}

void VG_(deleteBag) ( WordBag* bag )
{
   void (*dealloc)(void*) = bag->fm->dealloc;
   VG_(deleteFM)( bag->fm, NULL, NULL );
   VG_(memset)(bag, 0, sizeof(WordBag));
   dealloc(bag);
}

void VG_(addToBag)( WordBag* bag, UWord w )
{
   UWord key, count;
   if (VG_(lookupFM)(bag->fm, &key, &count, w)) {
      vg_assert(key == w);
      vg_assert(count >= 1);
      VG_(addToFM)(bag->fm, w, count+1);
   } else {
      VG_(addToFM)(bag->fm, w, 1);
   }
}

UWord VG_(elemBag) ( const WordBag* bag, UWord w )
{
   UWord key, count;
   if (VG_(lookupFM)( bag->fm, &key, &count, w)) {
      vg_assert(key == w);
      vg_assert(count >= 1);
      return count;
   } else {
      return 0;
   }
}

UWord VG_(sizeUniqueBag) ( const WordBag* bag )
{
   return VG_(sizeFM)( bag->fm );
}

static UWord sizeTotalBag_wrk ( const AvlNode* nd )
{
   
   UWord w = nd->val;
   vg_assert(w >= 1);
   if (nd->child[0])
      w += sizeTotalBag_wrk(nd->child[0]);
   if (nd->child[1])
      w += sizeTotalBag_wrk(nd->child[1]);
   return w;
}
UWord VG_(sizeTotalBag)( const WordBag* bag )
{
   if (bag->fm->root)
      return sizeTotalBag_wrk(bag->fm->root);
   else
      return 0;
}

Bool VG_(delFromBag)( WordBag* bag, UWord w )
{
   UWord key, count;
   if (VG_(lookupFM)(bag->fm, &key, &count, w)) {
      vg_assert(key == w);
      vg_assert(count >= 1);
      if (count > 1) {
         VG_(addToFM)(bag->fm, w, count-1);
      } else {
         vg_assert(count == 1);
         VG_(delFromFM)( bag->fm, NULL, NULL, w );
      }
      return True;
   } else {
      return False;
   }
}

Bool VG_(isEmptyBag)( const WordBag* bag )
{
   return VG_(sizeFM)(bag->fm) == 0;
}

Bool VG_(isSingletonTotalBag)( const WordBag* bag )
{
   AvlNode* nd;
   if (VG_(sizeFM)(bag->fm) != 1)
      return False;
   nd = bag->fm->root;
   vg_assert(nd);
   vg_assert(!nd->child[0]);
   vg_assert(!nd->child[1]);
   return nd->val == 1;
}

UWord VG_(anyElementOfBag)( const WordBag* bag )
{
   AvlNode* nd = bag->fm->root;
   vg_assert(nd); 
   vg_assert(nd->val >= 1);
   return nd->key;
}

void VG_(initIterBag)( WordBag* bag )
{
   VG_(initIterFM)(bag->fm);
}

Bool VG_(nextIterBag)( WordBag* bag, UWord* pVal, UWord* pCount )
{
   return VG_(nextIterFM)( bag->fm, pVal, pCount );
}

void VG_(doneIterBag)( WordBag* bag )
{
   VG_(doneIterFM)( bag->fm );
}


