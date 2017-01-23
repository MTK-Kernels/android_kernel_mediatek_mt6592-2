/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2011-2013 OpenWorks LLP info@open-works.co.uk,
                           Philippe Waroquiers philippe.waroquiers@skynet.be

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
#include "pub_core_libcbase.h"
#include "pub_core_libcassert.h"
#include "pub_core_xarray.h"
#include "pub_core_poolalloc.h" 

struct _PoolAlloc {
   UWord   nrRef;         
   UWord   elemSzB;       
   UWord   nPerPool;      
   void*   (*alloc_fn)(const HChar*, SizeT); 
   const HChar*  cc; 
   void    (*free_fn)(void*); 
   XArray* pools;
   void* nextFree;
};

PoolAlloc* VG_(newPA) ( UWord  elemSzB,
                        UWord  nPerPool,
                        void*  (*alloc_fn)(const HChar*, SizeT),
                        const  HChar* cc,
                        void   (*free_fn)(void*) )
{
   PoolAlloc* pa;
   vg_assert(0 == (elemSzB % sizeof(UWord)));
   vg_assert(elemSzB >= sizeof(UWord));
   vg_assert(nPerPool >= 100); 
   vg_assert(alloc_fn);
   vg_assert(cc);
   vg_assert(free_fn);
   pa = alloc_fn(cc, sizeof(*pa));
   VG_(memset)(pa, 0, sizeof(*pa));
   pa->nrRef    = 0;
   pa->elemSzB  = elemSzB;
   pa->nPerPool = nPerPool;
   pa->pools    = NULL;
   pa->alloc_fn = alloc_fn;
   pa->cc       = cc;
   pa->free_fn  = free_fn;
   pa->pools    = VG_(newXA)( alloc_fn, cc, free_fn, sizeof(void*) );
   pa->nextFree = NULL;

   return pa;
}

void VG_(deletePA) ( PoolAlloc* pa)
{
   Word i;
   vg_assert(pa->nrRef == 0);
   for (i = 0; i < VG_(sizeXA) (pa->pools); i++)
      pa->free_fn (*(UWord **)VG_(indexXA) ( pa->pools, i ));
   VG_(deleteXA) (pa->pools);
   pa->free_fn (pa);
}

__attribute__((noinline))
static void pal_add_new_pool ( PoolAlloc* pa ) 
{
   Word   i;
   UWord* pool;
   vg_assert(pa);
   vg_assert(pa->nextFree == NULL);
   pool = pa->alloc_fn( pa->cc, pa->elemSzB * pa->nPerPool );
   for (i = pa->nPerPool-1; i >= 0; i--) {
      UChar* elemC = ((UChar*)pool) + i * pa->elemSzB;
      UWord* elem  = (UWord*)elemC;
      vg_assert(0 == (((UWord)elem) % sizeof(UWord)));
      *elem = (UWord)pa->nextFree;
      pa->nextFree = elem;
   }
   
   VG_(addToXA)( pa->pools, &pool );
}

UWord VG_(sizePA) ( PoolAlloc* pa)
{
   vg_assert(pa);
   return pa->nPerPool * VG_(sizeXA) (pa->pools);
}

void* VG_(allocEltPA) ( PoolAlloc* pa)
{
   UWord* elem;
   if (UNLIKELY(pa->nextFree == NULL)) {
      pal_add_new_pool(pa);
   }
   elem = pa->nextFree;
   pa->nextFree = (void*)*elem;
   *elem = 0; 
   return elem;
}

void VG_(freeEltPA) ( PoolAlloc* pa, void* p)
{
   UWord* elem = (UWord*)p;
   *elem = (UWord)pa->nextFree;
   pa->nextFree = elem;
}


void VG_(addRefPA) ( PoolAlloc* pa)
{
   pa->nrRef++;
}

UWord VG_(releasePA)(PoolAlloc* pa)
{
   UWord nrRef;

   vg_assert(pa->nrRef > 0);
   nrRef = --pa->nrRef;
   if (nrRef == 0)
      VG_(deletePA)(pa);
   return nrRef;
}
