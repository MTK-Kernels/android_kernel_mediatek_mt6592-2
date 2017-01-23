
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pub_core_basics.h"
#include "pub_core_libcbase.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcprint.h"


#undef vg_assert
#define vg_assert(e)                   assert(e)
#undef vg_assert2
#define vg_assert2(e, fmt, args...)    assert(e)

#define vgPlain_printf                 printf
#define vgPlain_memset                 memset
#define vgPlain_memcpy                 memcpy
#define vgPlain_memmove                memmove

#define vgPlain_ssort(a,b,c,d) assert(a)
#define vgPlain_vcbprintf(a,b,...) assert(a == b)
#include "coregrind/m_xarray.c"
#undef vgPlain_ssort
#undef vgPlain_vcbprintf
#include "coregrind/m_poolalloc.c"
#include "coregrind/m_oset.c"

#define NN  1000       



#define random error_do_not_use_libc_random

static UInt seed = 0;
static UInt myrandom( void )
{
  seed = (1103515245 * seed + 12345);
  return seed;
}

static void* allocate_node(const HChar* cc, SizeT szB)
{ return malloc(szB); }

static void free_node(void* p)
{ return free(p); }




__attribute__((unused))
static const HChar *wordToStr(const void *p)
{
   static HChar buf[32];
   sprintf(buf, "%ld", *(Word*)p);
   return buf;
}

__attribute__((unused))
static Word wordCmp(void* vkey, void* velem)
{
   return *(Word*)vkey - *(Word*)velem;
}

void example1singleset(OSet* oset, char *descr)
{
   Int  i, n;
   UWord v, prev;
   UWord* vs[NN];
   UWord *pv;
   UWord  sorted_elts[NN]; 

   
   vg_assert( ! VG_(OSetGen_Contains)(oset, &v) );
   vg_assert( ! VG_(OSetGen_Lookup)(oset, &v) );
   vg_assert( ! VG_(OSetGen_Remove)(oset, &v) );
   vg_assert( ! VG_(OSetGen_Next)(oset) );
   vg_assert( 0 == VG_(OSetGen_Size)(oset) );

   
   
   for (i = 0; i < NN; i++) {
      vs[i] = VG_(OSetGen_AllocNode)(oset, sizeof(Word));
      *(vs[i]) = 2*(i+1);
      sorted_elts[i] = *(vs[i]);
   }
   seed = 0;
   for (i = 0; i < NN; i++) {
      UWord r1  = myrandom() % NN;
      UWord r2  = myrandom() % NN;
      UWord* tmp= vs[r1];
      vs[r1]   = vs[r2];
      vs[r2]   = tmp;
   }

   
   for (i = 0; i < NN; i++) {
      VG_(OSetGen_Insert)(oset, vs[i]);
   }

   
   vg_assert( NN == VG_(OSetGen_Size)(oset) );

   
   for (i = 0; i < NN; i++) {
      assert( VG_(OSetGen_Contains)(oset, vs[i]) );
   }

#define FULLCHECKEVERY 20
   
   
   for (i = 0; i < NN; i++) {
      UWord k;
      UWord *pval;
      Int j;

      
      k = sorted_elts[i] - 1;
      VG_(OSetGen_ResetIterAt) (oset, &k);
      
      for (j = i; j < NN; j++) {
         pval = VG_(OSetGen_Next)(oset);
         assert (*pval == sorted_elts[j]);
         if (i % FULLCHECKEVERY != 0) break;
      }

      
      k = sorted_elts[i];
      VG_(OSetGen_ResetIterAt) (oset, &k);
      
      for (j = i; j < NN; j++) {
         pval = VG_(OSetGen_Next)(oset);
         assert (*pval == sorted_elts[j]);
         if (i % FULLCHECKEVERY != 0) break;
      }

      
      
      k = sorted_elts[i] + 1;
      VG_(OSetGen_ResetIterAt) (oset, &k);
      if (i < NN - 1) {
         for (j = i+1; j < NN; j++) {
            pval = VG_(OSetGen_Next)(oset);
            assert (*pval == sorted_elts[j]);
            if (i % FULLCHECKEVERY != 0) break;
         }
      } else {
         pval = VG_(OSetGen_Next)(oset);
         assert (pval == NULL);
      }
      
   }

   
   
   v = 0;
   assert( ! VG_(OSetGen_Contains)(oset, &v) );
   for (i = 0; i < NN; i++) {
      v = *(vs[i]) + 1;
      assert( ! VG_(OSetGen_Contains)(oset, &v) );
   }
   v = 2*(NN+1);
   assert( ! VG_(OSetGen_Contains)(oset, &v) );

   
   
   for (i = 0; i < NN; i++) {
      assert( vs[i] == VG_(OSetGen_Lookup)(oset, vs[i]) );
   }

   
   
   n = 0;
   pv = NULL;
   prev = 0;
   VG_(OSetGen_ResetIter)(oset);
   while ( (pv = VG_(OSetGen_Next)(oset)) ) {
      UWord curr = *pv;
      assert(prev < curr); 
      prev = curr;
      n++;
   }
   assert(NN == n);
   vg_assert( ! VG_(OSetGen_Next)(oset) );
   vg_assert( ! VG_(OSetGen_Next)(oset) );

   
   
   for (i = 0; i < NN; i += 2) {
      assert( pv = VG_(OSetGen_Remove)(oset, vs[i]) );
      assert( pv == vs[i] );
   }

   
   vg_assert( NN/2 == VG_(OSetGen_Size)(oset) );

   
   for (i = 1; i < NN; i += 2) {
      assert( pv = VG_(OSetGen_LookupWithCmp)(oset, vs[i], NULL) );
      assert( pv == vs[i] );
   }

   
   for (i = 0; i < NN; i += 2) {
      assert( ! VG_(OSetGen_Contains)(oset, vs[i]) );
   }

   
   
   for (i = 1; i < NN; i += 2) {
      assert( pv = VG_(OSetGen_Remove)(oset, vs[i]) );
      assert( pv == vs[i] );
   }

   
   vg_assert( ! VG_(OSetGen_Contains)(oset, &v) );
   vg_assert( ! VG_(OSetGen_Lookup)(oset, &v) );
   vg_assert( ! VG_(OSetGen_Remove)(oset, &v) );
   vg_assert( ! VG_(OSetGen_Next)(oset) );
   vg_assert( 0 == VG_(OSetGen_Size)(oset) );

   
   VG_(OSetGen_FreeNode)(oset, vs[0]);
   VG_(OSetGen_FreeNode)(oset, vs[1]);
   VG_(OSetGen_FreeNode)(oset, vs[2]);

   
   
   for (i = 3; i < NN; i++) {
      VG_(OSetGen_Insert)(oset, vs[i]);
   }

   
   OSet_Print(oset, descr, wordToStr);

}

void example1(void)
{
   OSet *oset, *oset_empty_clone;

   
   

   
   oset = VG_(OSetGen_Create)(0,
                              NULL,
                              allocate_node, "oset_test.1", free_node);
   example1singleset(oset, "single oset, no pool allocator");

   
   VG_(OSetGen_Destroy)(oset);

   
   oset = VG_(OSetGen_Create_With_Pool)(0,
                                        NULL,
                                        allocate_node, "oset_test.1",
                                        free_node,
                                        101, sizeof(Word));
   example1singleset(oset, "single oset, pool allocator");

   
   VG_(OSetGen_Destroy)(oset);


   
   oset = VG_(OSetGen_Create_With_Pool)
      (0,
       NULL,
       allocate_node, "oset_test.1", free_node,
       101, sizeof(Word));
   oset_empty_clone = VG_(OSetGen_EmptyClone) (oset);
   example1singleset(oset, "oset, shared pool");
   example1singleset(oset_empty_clone, "cloned oset, shared pool");
   
   
   VG_(OSetGen_Destroy)(oset_empty_clone);
   VG_(OSetGen_Destroy)(oset);
   
}

void example1b(void)
{
   Int  i, n;
   UWord v = 0, prev;
   UWord vs[NN];

   
   
   OSet* oset = VG_(OSetWord_Create)(allocate_node, "oset_test.2", free_node);

   
   vg_assert( ! VG_(OSetWord_Contains)(oset, v) );
   vg_assert( ! VG_(OSetWord_Remove)(oset, v) );
   vg_assert( ! VG_(OSetWord_Next)(oset, (UWord *)&v) );
   vg_assert( 0 == VG_(OSetWord_Size)(oset) );

   
   
   for (i = 0; i < NN; i++) {
      vs[i] = 2*i;
   }
   seed = 0;
   for (i = 0; i < NN; i++) {
      UWord r1  = myrandom() % NN;
      UWord r2  = myrandom() % NN;
      UWord tmp = vs[r1];
      vs[r1]   = vs[r2];
      vs[r2]   = tmp;
   }

   
   for (i = 0; i < NN; i++) {
      VG_(OSetWord_Insert)(oset, vs[i]);
   }

   
   vg_assert( NN == VG_(OSetWord_Size)(oset) );

   
   for (i = 0; i < NN; i++) {
      assert( VG_(OSetWord_Contains)(oset, vs[i]) );
   }

   
   
   v = 0xffffffff;
   assert( ! VG_(OSetWord_Contains)(oset, v) );
   for (i = 0; i < NN; i++) {
      v = vs[i] + 1;
      assert( ! VG_(OSetWord_Contains)(oset, v) );
   }
   v = NN*2;
   assert( ! VG_(OSetWord_Contains)(oset, v) );

   
   for (i = 0; i < NN; i++) {
      assert( VG_(OSetWord_Contains)(oset, vs[i]) );
   }

   
   
   n = 0;
   prev = 0;
   VG_(OSetWord_ResetIter)(oset);
   while ( VG_(OSetWord_Next)(oset, (UWord *)&v) ) {
      UWord curr = v;
      if (n == 0)
         assert(prev == curr); 
      else
         assert(prev < curr); 
      prev = curr;
      n++;
   }
   assert(NN == n);
   vg_assert( ! VG_(OSetWord_Next)(oset, (UWord *)&v) );
   vg_assert( ! VG_(OSetWord_Next)(oset, (UWord *)&v) );

   
   for (i = 0; i < NN; i += 2) {
      assert( VG_(OSetWord_Remove)(oset, vs[i]) );
   }

   
   vg_assert( NN/2 == VG_(OSetWord_Size)(oset) );

   
   for (i = 1; i < NN; i += 2) {
      assert( VG_(OSetWord_Contains)(oset, vs[i]) );
   }

   
   for (i = 0; i < NN; i += 2) {
      assert( ! VG_(OSetWord_Contains)(oset, vs[i]) );
   }

   
   for (i = 1; i < NN; i += 2) {
      assert( VG_(OSetWord_Remove)(oset, vs[i]) );
   }

   
   vg_assert( ! VG_(OSetWord_Contains)(oset, v) );
   vg_assert( ! VG_(OSetWord_Remove)(oset, v) );
   vg_assert( ! VG_(OSetWord_Next)(oset, (UWord *)&v) );
   vg_assert( 0 == VG_(OSetWord_Size)(oset) );

   
   
   for (i = 3; i < NN; i++) {
      VG_(OSetWord_Insert)(oset, vs[i]);
   }

   
   OSet_Print(oset, "oset1b", wordToStr);

   
   VG_(OSetWord_Destroy)(oset);
}




typedef struct {
   Int   b1;
   Addr  first;
   Addr  last;
   Int   b2;
}
Block;

__attribute__((unused))
static HChar *blockToStr(void *p)
{
   static HChar buf[32];
   Block* b = (Block*)p;
   sprintf(buf, "<(%d) %lu..%lu (%d)>", b->b1, b->first, b->last, b->b2);
   return buf;
}

static Word blockCmp(const void* vkey, const void* velem)
{
   Addr   key  = *(const Addr*)vkey;
   const Block* elem = (const Block*)velem;

   assert(elem->first <= elem->last);
   if (key < elem->first) return -1;
   if (key > elem->last)  return  1;
   return 0;
}

void example2(void)
{
   Int i, n;
   Addr a;
   Block* vs[NN];
   Block v, prev;
   Block *pv;

   
   
   OSet* oset = VG_(OSetGen_Create)(offsetof(Block, first),
                                    blockCmp,
                                    allocate_node, "oset_test.3", free_node);

   
   vg_assert( ! VG_(OSetGen_Contains)(oset, &v) );
   vg_assert( ! VG_(OSetGen_Lookup)(oset, &v) );
   vg_assert( ! VG_(OSetGen_Remove)(oset, &v) );
   vg_assert( ! VG_(OSetGen_Next)(oset) );
   vg_assert( 0 == VG_(OSetGen_Size)(oset) );

   
   
   for (i = 0; i < NN; i++) {
      vs[i] = VG_(OSetGen_AllocNode)(oset, sizeof(Block));
      vs[i]->b1    = i;
      vs[i]->first = i*10 + 1;
      vs[i]->last  = vs[i]->first + 2;
      vs[i]->b2    = i+1;
   }
   seed = 0;
   for (i = 0; i < NN; i++) {
      Int r1  = myrandom() % NN;
      Int r2  = myrandom() % NN;
      Block* tmp = vs[r1];
      vs[r1]  = vs[r2];
      vs[r2]  = tmp;
   }

   
   for (i = 0; i < NN; i++) {
      VG_(OSetGen_Insert)(oset, vs[i]);
   }

   
   vg_assert( NN == VG_(OSetGen_Size)(oset) );

   
   
   for (i = 0; i < NN; i++) {
      a = vs[i]->first + 0;    assert( VG_(OSetGen_Contains)(oset, &a) );
      a = vs[i]->first + 1;    assert( VG_(OSetGen_Contains)(oset, &a) );
      a = vs[i]->first + 2;    assert( VG_(OSetGen_Contains)(oset, &a) );
   }

   
   
   a = 0;
   assert( ! VG_(OSetGen_Contains)(oset, &a) );
   for (i = 0; i < NN; i++) {
      a = vs[i]->first - 1;    assert( ! VG_(OSetGen_Contains)(oset, &a) );
      a = vs[i]->first + 3;    assert( ! VG_(OSetGen_Contains)(oset, &a) );
   }

   
   
   for (i = 0; i < NN; i++) {
      a = vs[i]->first + 0;    assert( vs[i] == VG_(OSetGen_Lookup)(oset, &a) );
      a = vs[i]->first + 1;    assert( vs[i] == VG_(OSetGen_Lookup)(oset, &a) );
      a = vs[i]->first + 2;    assert( vs[i] == VG_(OSetGen_Lookup)(oset, &a) );
      assert( vs[i] == VG_(OSetGen_LookupWithCmp)(oset, &a, blockCmp) );
   }

   
   
   n = 0;
   pv = NULL;
   prev.last = 0;
   VG_(OSetGen_ResetIter)(oset);
   while ( (pv = VG_(OSetGen_Next)(oset)) ) {
      Block curr = *pv;
      assert(prev.last < curr.first); 
      prev = curr;
      n++;
   }
   assert(NN == n);
   vg_assert( ! VG_(OSetGen_Next)(oset) );
   vg_assert( ! VG_(OSetGen_Next)(oset) );

   
   
   for (i = 0; i < NN; i += 2) {
      a = vs[i]->first;    assert( vs[i] == VG_(OSetGen_Remove)(oset, &a) );
   }

   
   vg_assert( NN/2 == VG_(OSetGen_Size)(oset) );

   
   for (i = 1; i < NN; i += 2) {
      a = vs[i]->first + 0;    assert( vs[i] == VG_(OSetGen_Lookup)(oset, &a) );
      a = vs[i]->first + 1;    assert( vs[i] == VG_(OSetGen_Lookup)(oset, &a) );
      a = vs[i]->first + 2;    assert( vs[i] == VG_(OSetGen_Lookup)(oset, &a) );
   }

   
   for (i = 0; i < NN; i += 2) {
      a = vs[i]->first + 0;    assert( ! VG_(OSetGen_Contains)(oset, &a) );
      a = vs[i]->first + 1;    assert( ! VG_(OSetGen_Contains)(oset, &a) );
      a = vs[i]->first + 2;    assert( ! VG_(OSetGen_Contains)(oset, &a) );
   }

   
   
   for (i = 1; i < NN; i += 2) {
      a = vs[i]->first;    assert( vs[i] == VG_(OSetGen_Remove)(oset, &a) );
   }

   
   vg_assert( ! VG_(OSetGen_Contains)(oset, &v) );
   vg_assert( ! VG_(OSetGen_Lookup)(oset, &v) );
   vg_assert( ! VG_(OSetGen_Remove)(oset, &v) );
   vg_assert( ! VG_(OSetGen_Next)(oset) );
   vg_assert( 0 == VG_(OSetGen_Size)(oset) );

   
   for (i = 0; i < NN; i++) {
      VG_(OSetGen_Insert)(oset, vs[i]);
   }

   
   VG_(OSetGen_Destroy)(oset);
}


int main(void)
{
   example1();
   example1b();
   example2();
   return 0;
}
