

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2005-2013 Nicholas Nethercote
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

//   Released under GNU General Public License (GPL) version 2


#include "pub_core_basics.h"
#include "pub_core_libcbase.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcprint.h"
#include "pub_core_oset.h"
#include "pub_core_poolalloc.h"


typedef struct _OSetNode OSetNode;

typedef OSet     AvlTree;
typedef OSetNode AvlNode;

struct _OSetNode {
   AvlNode* left;
   AvlNode* right;
   Char     balance;
   Char     padding[sizeof(void*)-sizeof(Char)-sizeof(Short)];
   Short    magic;
};

#define STACK_MAX    32    
#define OSET_MAGIC   0x5b1f

struct _OSet {
   SizeT       keyOff;     
   OSetCmp_t   cmp;        
   OSetAlloc_t alloc_fn;   
   const HChar* cc;        
   OSetFree_t  free_fn;    
   PoolAlloc*  node_pa;    
   SizeT       maxEltSize; 
   Word        nElems;     
   AvlNode*    root;       

   AvlNode*    nodeStack[STACK_MAX];   
   Int          numStack[STACK_MAX];   
   Int         stackTop;               
};


static inline
AvlNode* node_of_elem(const void *elem)
{
   AvlNode* n = (AvlNode*)((Addr)elem - sizeof(AvlNode));
   vg_assert2(n->magic == OSET_MAGIC,
              "bad magic on node %p = %x (expected %x)\n"
              "possible causes:\n"
              " - node not allocated with VG_(OSetGen_AllocNode)()?\n"
              " - node metadata corrupted by underwriting start of element?\n",
              n, n->magic, OSET_MAGIC);
   return n;
}

static inline
void* elem_of_node(const AvlNode *n)
{
   vg_assert2(n->magic == OSET_MAGIC,
              "bad magic on node %p = %x (expected %x)\n"
              "possible causes:\n"
              " - node metadata corrupted by overwriting end of element?\n",
              n, n->magic, OSET_MAGIC);
   return (void*)((Addr)n + sizeof(AvlNode));
}

static inline
void* elem_of_node_no_check(const AvlNode *n)
{
   return (void*)((Addr)n + sizeof(AvlNode));
}

static inline
void* slow_key_of_node(const AvlTree* t, const AvlNode* n)
{
   return (void*)((Addr)elem_of_node(n) + t->keyOff);
}

static inline
void* fast_key_of_node(const AvlNode* n)
{
   return elem_of_node(n);
}

static inline Word fast_cmp(const void* k, const AvlNode* n)
{
   UWord w1 = *(const UWord*)k;
   UWord w2 = *(const UWord*)elem_of_node(n);
   
   
   
   
   
   
   if (w1 > w2) return  1;
   if (w1 < w2) return -1;
   return 0;
}

static 
inline Word slow_cmp(const AvlTree* t, const void* k, const AvlNode* n)
{
   return t->cmp(k, elem_of_node(n));
}


static void avl_swl ( AvlNode** root )
{
   AvlNode* a = *root;
   AvlNode* b = a->right;
   *root    = b;
   a->right = b->left;
   b->left  = a;
}

static void avl_swr ( AvlNode** root )
{
   AvlNode* a = *root;
   AvlNode* b = a->left;
   *root    = b;
   a->left  = b->right;
   b->right = a;
}

static void avl_nasty ( AvlNode* root )
{
   switch (root->balance) {
   case -1:
      root->left->balance  = 0;
      root->right->balance = 1;
      break;
   case 1:
      root->left->balance  =-1;
      root->right->balance = 0;
      break;
   case 0:
      root->left->balance  = 0;
      root->right->balance = 0;
   }
   root->balance = 0;
}


static void stackClear(AvlTree* t)
{
   Int i;
   vg_assert(t);
   for (i = 0; i < STACK_MAX; i++) {
      t->nodeStack[i] = NULL;
      t->numStack[i]  = 0;
   }
   t->stackTop = 0;
}

static inline void stackPush(AvlTree* t, AvlNode* n, Int i)
{
   vg_assert(t->stackTop < STACK_MAX);
   vg_assert(1 <= i && i <= 3);
   t->nodeStack[t->stackTop] = n;
   t-> numStack[t->stackTop] = i;
   t->stackTop++;
}

static inline Bool stackPop(AvlTree* t, AvlNode** n, Int* i)
{
   vg_assert(t->stackTop <= STACK_MAX);

   if (t->stackTop > 0) {
      t->stackTop--;
      *n = t->nodeStack[t->stackTop];
      *i = t-> numStack[t->stackTop];
      vg_assert(1 <= *i && *i <= 3);
      t->nodeStack[t->stackTop] = NULL;
      t-> numStack[t->stackTop] = 0;
      return True;
   } else {
      return False;
   }
}


AvlTree* VG_(OSetGen_Create)(PtrdiffT keyOff, OSetCmp_t cmp,
                             OSetAlloc_t alloc_fn, const HChar* cc,
                             OSetFree_t free_fn)
{
   AvlTree* t;

   
   vg_assert(sizeof(AvlNode) == 3*sizeof(void*));

   
   vg_assert(alloc_fn);
   vg_assert(free_fn);
   if (!cmp) vg_assert(0 == keyOff);    

   t           = alloc_fn(cc, sizeof(AvlTree));
   t->keyOff   = keyOff;
   t->cmp      = cmp;
   t->alloc_fn = alloc_fn;
   t->cc       = cc;
   t->free_fn  = free_fn;
   t->node_pa  = NULL;
   t->maxEltSize = 0; 
   t->nElems   = 0;
   t->root     = NULL;
   stackClear(t);

   return t;
}

AvlTree* VG_(OSetGen_Create_With_Pool)(PtrdiffT keyOff, OSetCmp_t cmp,
                                       OSetAlloc_t alloc_fn, const HChar* cc,
                                       OSetFree_t free_fn,
                                       SizeT poolSize,
                                       SizeT maxEltSize)
{
   AvlTree* t;

   t = VG_(OSetGen_Create) (keyOff, cmp, alloc_fn, cc, free_fn);

   vg_assert (poolSize > 0);
   vg_assert (maxEltSize > 0);
   t->maxEltSize = maxEltSize;
   t->node_pa = VG_(newPA)(sizeof(AvlNode) 
                           + VG_ROUNDUP(maxEltSize, sizeof(void*)),
                           poolSize,
                           t->alloc_fn,
                           cc,
                           t->free_fn);
   VG_(addRefPA) (t->node_pa);

   return t;
}

AvlTree* VG_(OSetGen_EmptyClone) (const AvlTree* os)
{
   AvlTree* t;

   vg_assert(os);

   t           = os->alloc_fn(os->cc, sizeof(AvlTree));
   t->keyOff   = os->keyOff;
   t->cmp      = os->cmp;
   t->alloc_fn = os->alloc_fn;
   t->cc       = os->cc;
   t->free_fn  = os->free_fn;
   t->node_pa  = os->node_pa;
   if (t->node_pa)
      VG_(addRefPA) (t->node_pa);
   t->maxEltSize = os->maxEltSize;
   t->nElems   = 0;
   t->root     = NULL;
   stackClear(t);

   return t;
}

AvlTree* VG_(OSetWord_Create)(OSetAlloc_t alloc_fn, const HChar* cc, 
                              OSetFree_t free_fn)
{
   return VG_(OSetGen_Create)(0, NULL, alloc_fn, cc, free_fn);
}

void VG_(OSetGen_Destroy)(AvlTree* t)
{
   Bool has_node_pa;
   vg_assert(t);

   has_node_pa = t->node_pa != NULL;

   if (!has_node_pa || VG_(releasePA)(t->node_pa) > 0) {
      AvlNode* n = NULL;
      Int i = 0;
      Word sz = 0;
   
      stackClear(t);
      if (t->root)
         stackPush(t, t->root, 1);

      
      
      while (stackPop(t, &n, &i)) {
         switch (i) {
         case 1: 
            stackPush(t, n, 2);
            if (n->left)  stackPush(t, n->left, 1);
            break;
         case 2: 
            stackPush(t, n, 3);
            if (n->right) stackPush(t, n->right, 1);
            break;
         case 3:
            if (has_node_pa)
               VG_(freeEltPA) (t->node_pa, n);
            else
               t->free_fn(n);
            sz++;
            break;
         }
      }
      vg_assert(sz == t->nElems);
   }

   
   t->free_fn(t);
}

void VG_(OSetWord_Destroy)(AvlTree* t)
{
   VG_(OSetGen_Destroy)(t);
}

void* VG_(OSetGen_AllocNode)(const AvlTree* t, SizeT elemSize)
{
   AvlNode* n;
   Int nodeSize = sizeof(AvlNode) + elemSize;
   vg_assert(elemSize > 0);
   if (t->node_pa) {
      vg_assert(elemSize <= t->maxEltSize);
      n = VG_(allocEltPA) (t->node_pa);
   } else {
      n = t->alloc_fn( t->cc, nodeSize );
   }
   VG_(memset)(n, 0, nodeSize);
   n->magic = OSET_MAGIC;
   return elem_of_node(n);
}

void VG_(OSetGen_FreeNode)(const AvlTree* t, void* e)
{
   if (t->node_pa)
      VG_(freeEltPA) (t->node_pa, node_of_elem (e));
   else
      t->free_fn( node_of_elem(e) );
}


static inline Word cmp_key_root(const AvlTree* t, const AvlNode* n)
{
   return t->cmp
          ? slow_cmp(t, slow_key_of_node(t, n), t->root)
          : fast_cmp(   fast_key_of_node(   n), t->root);
}

static Bool avl_insert(AvlTree* t, AvlNode* n)
{
   Word cmpres = cmp_key_root(t, n);

   if (cmpres < 0) {
      
      if (t->root->left) {
         
         AvlTree left_subtree;
         left_subtree.root   = t->root->left;
         left_subtree.cmp    = t->cmp;
         left_subtree.keyOff = t->keyOff;
         if (avl_insert(&left_subtree, n)) {
             switch (t->root->balance--) {
             case 1: return False;
             case 0: return True;
             }
             if (t->root->left->balance < 0) {
                avl_swr(&(t->root));
                t->root->balance = 0;
                t->root->right->balance = 0;
             } else {
                avl_swl(&(t->root->left));
                avl_swr(&(t->root));
                avl_nasty(t->root);
             }
         } else {
            t->root->left=left_subtree.root;
         }
         return False;
      } else {
         t->root->left = n;
         if (t->root->balance--) return False;
         return True;
      }

   } else if (cmpres > 0) {
      
      if (t->root->right) {
         
         AvlTree right_subtree;
         right_subtree.root   = t->root->right;
         right_subtree.cmp    = t->cmp;
         right_subtree.keyOff = t->keyOff;
         if (avl_insert(&right_subtree, n)) {
            switch (t->root->balance++) {
            case -1: return False;
            case  0: return True;
            }
            if (t->root->right->balance > 0) {
               avl_swl(&(t->root));
               t->root->balance = 0;
               t->root->left->balance = 0;
            } else {
               avl_swr(&(t->root->right));
               avl_swl(&(t->root));
               avl_nasty(t->root);
            }
         } else {
            t->root->right=right_subtree.root;
         }
         return False;
      } else {
         t->root->right = n;
         if (t->root->balance++) return False;
         return True;
      }

   } else {
      vg_assert2(0, "OSet{Word,Gen}_Insert: duplicate element added");
   }
}

void VG_(OSetGen_Insert)(AvlTree* t, void* e)
{
   AvlNode* n;

   vg_assert(t);

   
   
   
   n          = node_of_elem(e);
   n->left    = 0;
   n->right   = 0;
   n->balance = 0;

   
   if (!t->root) {
      t->root = n;
   } else {
      avl_insert(t, n);
   }

   t->nElems++;
   t->stackTop = 0;  
}

void VG_(OSetWord_Insert)(AvlTree* t, UWord val)
{
   Word* node = VG_(OSetGen_AllocNode)(t, sizeof(UWord));
   *node = val;
   VG_(OSetGen_Insert)(t, node);
}


static AvlNode* avl_lookup(const AvlTree* t, const void* k)
{
   Word     cmpres;
   AvlNode* curr = t->root;

   if (t->cmp) {
      
      while (True) {
         if (curr == NULL) return NULL;
         cmpres = slow_cmp(t, k, curr);
         if      (cmpres < 0) curr = curr->left;
         else if (cmpres > 0) curr = curr->right;
         else return curr;
      }
   } else {
      
      
      
      
      UWord w1 = *(const UWord*)k;
      UWord w2;
      while (True) {
         if (curr == NULL) return NULL;
         w2 = *(UWord*)elem_of_node_no_check(curr);
         if      (w1 < w2) curr = curr->left;
         else if (w1 > w2) curr = curr->right;
         else return curr;
      }
   }
}

void* VG_(OSetGen_Lookup)(const AvlTree* t, const void* k)
{
   AvlNode* n;
   vg_assert(t);
   n = avl_lookup(t, k);
   return ( n ? elem_of_node(n) : NULL );
}

void* VG_(OSetGen_LookupWithCmp)(AvlTree* t, const void* k, OSetCmp_t cmp)
{
   
   void* e;
   OSetCmp_t tmpcmp;
   vg_assert(t);
   tmpcmp = t->cmp;
   t->cmp = cmp;
   e = VG_(OSetGen_Lookup)(t, k);
   t->cmp = tmpcmp;
   return e;
}

Bool VG_(OSetGen_Contains)(const AvlTree* t, const void* k)
{
   return (NULL != VG_(OSetGen_Lookup)(t, k));
}

Bool VG_(OSetWord_Contains)(const AvlTree* t, UWord val)
{
   return (NULL != VG_(OSetGen_Lookup)(t, &val));
}


static Bool avl_removeroot(AvlTree* t);

static Bool avl_remove(AvlTree* t, const AvlNode* n)
{
   Bool ch;
   Word cmpres = cmp_key_root(t, n);

   if (cmpres < 0) {
      AvlTree left_subtree;
      
      vg_assert(t->root->left);
      
      left_subtree.root   = t->root->left;
      left_subtree.cmp    = t->cmp;
      left_subtree.keyOff = t->keyOff;
      ch = avl_remove(&left_subtree, n);
      t->root->left = left_subtree.root;
      if (ch) {
         switch (t->root->balance++) {
         case -1: return True;
         case  0: return False;
         }
         switch (t->root->right->balance) {
         case 0:
            avl_swl(&(t->root));
            t->root->balance = -1;
            t->root->left->balance = 1;
            return False;
         case 1:
            avl_swl(&(t->root));
            t->root->balance = 0;
            t->root->left->balance = 0;
            return True;
         }
         avl_swr(&(t->root->right));
         avl_swl(&(t->root));
         avl_nasty(t->root);
         return True;
      } else {
         return False;
      }
   
   } else if (cmpres > 0) {
      
      AvlTree right_subtree;
      vg_assert(t->root->right);
      
      right_subtree.root   = t->root->right;
      right_subtree.cmp    = t->cmp;
      right_subtree.keyOff = t->keyOff;
      ch = avl_remove(&right_subtree, n);
      t->root->right = right_subtree.root;
      if (ch) {
         switch (t->root->balance--) {
         case 1: return True;
         case 0: return False;
         }
         switch (t->root->left->balance) {
         case 0:
            avl_swr(&(t->root));
            t->root->balance = 1;
            t->root->right->balance = -1;
            return False;
         case -1:
            avl_swr(&(t->root));
            t->root->balance = 0;
            t->root->right->balance = 0;
            return True;
         }
         avl_swl(&(t->root->left));
         avl_swr(&(t->root));
         avl_nasty(t->root);
         return True;
      } else {
         return False;
      }

   } else {
      
      vg_assert(t->root == n);
      return avl_removeroot(t);
   }
}

static Bool avl_removeroot(AvlTree* t)
{
   Bool     ch;
   AvlNode* n;

   if (!t->root->left) {
      if (!t->root->right) {
         t->root = NULL;
         return True;
      }
      t->root = t->root->right;
      return True;
   }
   if (!t->root->right) {
      t->root = t->root->left;
      return True;
   }
   if (t->root->balance < 0) {
      
      n = t->root->left;
      while (n->right) n = n->right;
   } else {
      
      n = t->root->right;
      while (n->left) n = n->left;
   }
   ch = avl_remove(t, n);
   n->left    = t->root->left;
   n->right   = t->root->right;
   n->balance = t->root->balance;
   t->root    = n;
   if (n->balance == 0) return ch;
   return False;
}

void* VG_(OSetGen_Remove)(AvlTree* t, const void* k)
{
   
   AvlNode* n = avl_lookup(t, k);
   if (n) {
      avl_remove(t, n);
      t->nElems--;
      t->stackTop = 0;     
      return elem_of_node(n);
   } else {
      return NULL;
   }
}

Bool VG_(OSetWord_Remove)(AvlTree* t, UWord val)
{
   void* n = VG_(OSetGen_Remove)(t, &val);
   if (n) {
      VG_(OSetGen_FreeNode)(t, n);
      return True;
   } else {
      return False;
   }
}



void VG_(OSetGen_ResetIter)(AvlTree* t)
{
   vg_assert(t);
   stackClear(t);
   if (t->root)
      stackPush(t, t->root, 1);
}

void VG_(OSetWord_ResetIter)(AvlTree* t)
{
   VG_(OSetGen_ResetIter)(t);
}

void* VG_(OSetGen_Next)(AvlTree* t)
{
   Int i = 0;
   OSetNode* n = NULL;
   
   vg_assert(t);

   
   
   
   
   while (stackPop(t, &n, &i)) {
      switch (i) {
      case 1: case_1:
         stackPush(t, n, 2);
         
         if (n->left) { n = n->left; goto case_1; }
         break;
      case 2: 
         stackPush(t, n, 3);
         return elem_of_node(n);
      case 3:
         
         if (n->right) { n = n->right; goto case_1; }
         break;
      }
   }

   
   return NULL;
}

Bool VG_(OSetWord_Next)(AvlTree* t, UWord* val)
{
   UWord* n = VG_(OSetGen_Next)(t);
   if (n) {
      *val = *n;
      return True;
   } else {
      return False;
   }
}

void VG_(OSetGen_ResetIterAt)(AvlTree* oset, const void* k)
{
   AvlNode *t;
   Word    cmpresS; 
   UWord   cmpresU; 

   vg_assert(oset);
   stackClear(oset);

   if (!oset->root)
      return;

   
   t = oset->root;

   while (True) {
      if (t == NULL) return;

      if (oset->cmp) {
         cmpresS = (Word)slow_cmp(oset, k, t);
      } else {
         cmpresS = fast_cmp(k, t);
      }

      if (cmpresS < 0) { cmpresS = 1; } 
      else if (cmpresS > 0) { cmpresS = -1; }

      if (cmpresS == 0) {
         
         
         stackPush(oset, t, 2);
         
         return;
      }
      cmpresU = (UWord)cmpresS;
      cmpresU >>= (8 * sizeof(cmpresU) - 1);
      vg_assert(cmpresU == 0 || cmpresU == 1);
      if (!cmpresU) {
         
         stackPush(oset, t, 2);
      }
      t = cmpresU==0 ? t->left : t->right;
   }
}


Word VG_(OSetGen_Size)(const AvlTree* t)
{
   vg_assert(t);
   return t->nElems;
}

Word VG_(OSetWord_Size)(const AvlTree* t)
{
   return VG_(OSetGen_Size)(t);
}

static void OSet_Print2( const AvlTree* t, const AvlNode* n,
                         const HChar*(*strElem)(const void *), Int p )
{
   
   Int q = p;
   if (NULL == n) return;
   if (n->right) OSet_Print2(t, n->right, strElem, p+1);
   while (q--) VG_(printf)(".. ");
   VG_(printf)("%s\n", strElem(elem_of_node(n)));
   if (n->left) OSet_Print2(t, n->left, strElem, p+1);
}

__attribute__((unused))
static void OSet_Print( const AvlTree* t, const HChar *where,
                        const HChar*(*strElem)(const void *) )
{
   VG_(printf)("-- start %s ----------------\n", where);
   OSet_Print2(t, t->root, strElem, 0);
   VG_(printf)("-- end   %s ----------------\n", where);
}

