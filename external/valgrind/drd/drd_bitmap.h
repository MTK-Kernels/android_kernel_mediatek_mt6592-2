/*
  This file is part of drd, a thread error detector.

  Copyright (C) 2006-2013 Bart Van Assche <bvanassche@acm.org>.

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


#ifndef __DRD_BITMAP_H
#define __DRD_BITMAP_H


#include "pub_drd_bitmap.h"
#include "pub_tool_basics.h"
#include "pub_tool_oset.h"
#include "pub_tool_libcbase.h"
#ifdef ENABLE_DRD_CONSISTENCY_CHECKS
#include "pub_tool_libcassert.h"
#endif


/* Bitmap representation. A bitmap is a data structure in which two bits are
 * reserved per 32 bit address: one bit that indicates that the data at the
 * specified address has been read, and one bit that indicates that the data
 * has been written to.
 */






#define ADDR_IGNORED_BITS 0
#define ADDR_IGNORED_MASK ((1U << ADDR_IGNORED_BITS) - 1U)
#define ADDR_GRANULARITY  (1U << ADDR_IGNORED_BITS)

#define SCALED_SIZE(a)                                                  \
   (((((a) - 1U) | ADDR_IGNORED_MASK) + 1U) >> ADDR_IGNORED_BITS)

#define ADDR_LSB_BITS 12

#define ADDR_LSB_MASK (((UWord)1 << ADDR_LSB_BITS) - 1U)

static __inline__
UWord address_lsb(const Addr a)
{ return (a >> ADDR_IGNORED_BITS) & ADDR_LSB_MASK; }

static __inline__
Addr first_address_with_same_lsb(const Addr a)
{
   return ((a | ADDR_IGNORED_MASK) ^ ADDR_IGNORED_MASK);
}

static __inline__
Addr first_address_with_higher_lsb(const Addr a)
{
   return ((a | ADDR_IGNORED_MASK) + 1U);
}

static __inline__
UWord address_msb(const Addr a)
{ return a >> (ADDR_LSB_BITS + ADDR_IGNORED_BITS); }

static __inline__
Addr first_address_with_higher_msb(const Addr a)
{
   return ((a | ((ADDR_LSB_MASK << ADDR_IGNORED_BITS) | ADDR_IGNORED_MASK))
           + 1U);
}

static __inline__
Addr make_address(const UWord a1, const UWord a0)
{
   return ((a1 << (ADDR_LSB_BITS + ADDR_IGNORED_BITS))
           | (a0 << ADDR_IGNORED_BITS));
}





#define BITS_PER_UWORD (8U * sizeof(UWord))

#if defined(VGA_x86) || defined(VGA_ppc32) || defined(VGA_arm) \
    || defined(VGA_mips32)
#define BITS_PER_BITS_PER_UWORD 5
#elif defined(VGA_amd64) || defined(VGA_ppc64be) || defined(VGA_ppc64le) \
      || defined(VGA_s390x) || defined(VGA_mips64) || defined(VGA_arm64) \
      || defined(VGA_tilegx)
#define BITS_PER_BITS_PER_UWORD 6
#else
#error Unknown platform.
#endif

#define BITMAP1_UWORD_COUNT (1U << (ADDR_LSB_BITS - BITS_PER_BITS_PER_UWORD))

#define UWORD_LSB_MASK (((UWord)1 << BITS_PER_BITS_PER_UWORD) - 1)

static __inline__
UWord uword_msb(const UWord a)
{
#ifdef ENABLE_DRD_CONSISTENCY_CHECKS
   tl_assert(a < (1U << ADDR_LSB_BITS));
#endif
   return a >> BITS_PER_BITS_PER_UWORD;
}

static __inline__
UWord uword_lsb(const UWord a)
{
#ifdef ENABLE_DRD_CONSISTENCY_CHECKS
   tl_assert(a < (1U << ADDR_LSB_BITS));
#endif
   return a & UWORD_LSB_MASK;
}

static __inline__
Addr first_address_with_same_uword_lsb(const Addr a)
{
   return (a & (~UWORD_LSB_MASK << ADDR_IGNORED_BITS));
}

static __inline__
Addr first_address_with_higher_uword_msb(const Addr a)
{
   return ((a | ((UWORD_LSB_MASK << ADDR_IGNORED_BITS) | ADDR_IGNORED_MASK))
           + 1);
}




static ULong s_bitmap2_creation_count;





struct bitmap1
{
   UWord bm0_r[BITMAP1_UWORD_COUNT];
   UWord bm0_w[BITMAP1_UWORD_COUNT];
};

static __inline__ UWord bm0_mask(const UWord a)
{
#ifdef ENABLE_DRD_CONSISTENCY_CHECKS
   tl_assert(address_msb(make_address(0, a)) == 0);
#endif
   return ((UWord)1 << uword_lsb(a));
}

static __inline__ void bm0_set(UWord* bm0, const UWord a)
{
#ifdef ENABLE_DRD_CONSISTENCY_CHECKS
   tl_assert(address_msb(make_address(0, a)) == 0);
#endif
   bm0[uword_msb(a)] |= (UWord)1 << uword_lsb(a);
}

static __inline__ void bm0_set_range(UWord* bm0,
                                     const UWord a, const SizeT size)
{
#ifdef ENABLE_DRD_CONSISTENCY_CHECKS
   tl_assert(size > 0);
   tl_assert(address_msb(make_address(0, a)) == 0);
   tl_assert(address_msb(make_address(0, a + size - 1)) == 0);
   tl_assert(uword_msb(a) == uword_msb(a + size - 1));
#endif
   bm0[uword_msb(a)]
      |= (((UWord)1 << size) - 1) << uword_lsb(a);
}

static __inline__ void bm0_clear(UWord* bm0, const UWord a)
{
#ifdef ENABLE_DRD_CONSISTENCY_CHECKS
   tl_assert(address_msb(make_address(0, a)) == 0);
#endif
   bm0[uword_msb(a)] &= ~((UWord)1 << uword_lsb(a));
}

static __inline__ void bm0_clear_range(UWord* bm0,
                                       const UWord a, const SizeT size)
{
#ifdef ENABLE_DRD_CONSISTENCY_CHECKS
   tl_assert(address_msb(make_address(0, a)) == 0);
   tl_assert(size == 0 || address_msb(make_address(0, a + size - 1)) == 0);
   tl_assert(size == 0 || uword_msb(a) == uword_msb(a + size - 1));
#endif
   if (size > 0)
   {
      bm0[uword_msb(a)]
         &= ~((((UWord)1 << size) - 1) << uword_lsb(a));
   }
}

static __inline__ UWord bm0_is_set(const UWord* bm0, const UWord a)
{
#ifdef ENABLE_DRD_CONSISTENCY_CHECKS
   tl_assert(address_msb(make_address(0, a)) == 0);
#endif
   return (bm0[uword_msb(a)] & ((UWord)1 << uword_lsb(a)));
}

static __inline__ UWord bm0_is_any_set(const UWord* bm0,
                                       const Addr a, const SizeT size)
{
#ifdef ENABLE_DRD_CONSISTENCY_CHECKS
   tl_assert(size > 0);
   tl_assert(address_msb(make_address(0, a)) == 0);
   tl_assert(address_msb(make_address(0, a + size - 1)) == 0);
   tl_assert(uword_msb(a) == uword_msb(a + size - 1));
#endif
   return (bm0[uword_msb(a)] & ((((UWord)1 << size) - 1) << uword_lsb(a)));
}





struct bitmap2
{
   Addr           addr;   
   Bool           recalc;
   struct bitmap1 bm1;
};


static void bm2_clear(struct bitmap2* const bm2);
static __inline__
struct bitmap2* bm2_insert(struct bitmap* const bm, const UWord a1);



static __inline__
void bm_cache_rotate(struct bm_cache_elem cache[], const int n)
{
#if 0
   struct bm_cache_elem t;

   tl_assert(2 <= n && n <= 8);

   t = cache[0];
   if (n > 1)
      cache[0] = cache[1];
   if (n > 2)
      cache[1] = cache[2];
   if (n > 3)
      cache[2] = cache[3];
   if (n > 4)
      cache[3] = cache[4];
   if (n > 5)
      cache[4] = cache[5];
   if (n > 6)
      cache[5] = cache[6];
   if (n > 7)
      cache[6] = cache[7];
   cache[n - 1] = t;
#endif
}

static __inline__
Bool bm_cache_lookup(struct bitmap* const bm, const UWord a1,
                     struct bitmap2** bm2)
{
#ifdef ENABLE_DRD_CONSISTENCY_CHECKS
   tl_assert(bm);
   tl_assert(bm2);
#endif

#if DRD_BITMAP_N_CACHE_ELEM > 8
#error Please update the code below.
#endif
#if DRD_BITMAP_N_CACHE_ELEM >= 1
   if (a1 == bm->cache[0].a1)
   {
      *bm2 = bm->cache[0].bm2;
      return True;
   }
#endif
#if DRD_BITMAP_N_CACHE_ELEM >= 2
   if (a1 == bm->cache[1].a1)
   {
      *bm2 = bm->cache[1].bm2;
      return True;
   }
#endif
#if DRD_BITMAP_N_CACHE_ELEM >= 3
   if (a1 == bm->cache[2].a1)
   {
      *bm2 = bm->cache[2].bm2;
      bm_cache_rotate(bm->cache, 3);
      return True;
   }
#endif
#if DRD_BITMAP_N_CACHE_ELEM >= 4
   if (a1 == bm->cache[3].a1)
   {
      *bm2 = bm->cache[3].bm2;
      bm_cache_rotate(bm->cache, 4);
      return True;
   }
#endif
#if DRD_BITMAP_N_CACHE_ELEM >= 5
   if (a1 == bm->cache[4].a1)
   {
      *bm2 = bm->cache[4].bm2;
      bm_cache_rotate(bm->cache, 5);
      return True;
   }
#endif
#if DRD_BITMAP_N_CACHE_ELEM >= 6
   if (a1 == bm->cache[5].a1)
   {
      *bm2 = bm->cache[5].bm2;
      bm_cache_rotate(bm->cache, 6);
      return True;
   }
#endif
#if DRD_BITMAP_N_CACHE_ELEM >= 7
   if (a1 == bm->cache[6].a1)
   {
      *bm2 = bm->cache[6].bm2;
      bm_cache_rotate(bm->cache, 7);
      return True;
   }
#endif
#if DRD_BITMAP_N_CACHE_ELEM >= 8
   if (a1 == bm->cache[7].a1)
   {
      *bm2 = bm->cache[7].bm2;
      bm_cache_rotate(bm->cache, 8);
      return True;
   }
#endif
   *bm2 = 0;
   return False;
}

static __inline__
void bm_update_cache(struct bitmap* const bm,
                     const UWord a1,
                     struct bitmap2* const bm2)
{
#ifdef ENABLE_DRD_CONSISTENCY_CHECKS
   tl_assert(bm);
#endif

#if DRD_BITMAP_N_CACHE_ELEM > 8
#error Please update the code below.
#endif
#if DRD_BITMAP_N_CACHE_ELEM >= 8
   bm->cache[7] = bm->cache[6];
#endif
#if DRD_BITMAP_N_CACHE_ELEM >= 7
   bm->cache[6] = bm->cache[5];
#endif
#if DRD_BITMAP_N_CACHE_ELEM >= 6
   bm->cache[5] = bm->cache[4];
#endif
#if DRD_BITMAP_N_CACHE_ELEM >= 5
   bm->cache[4] = bm->cache[3];
#endif
#if DRD_BITMAP_N_CACHE_ELEM >= 4
   bm->cache[3] = bm->cache[2];
#endif
#if DRD_BITMAP_N_CACHE_ELEM >= 3
   bm->cache[2] = bm->cache[1];
#endif
#if DRD_BITMAP_N_CACHE_ELEM >= 2
   bm->cache[1] = bm->cache[0];
#endif
   bm->cache[0].a1  = a1;
   bm->cache[0].bm2 = bm2;
}

static __inline__
const struct bitmap2* bm2_lookup(struct bitmap* const bm, const UWord a1)
{
   struct bitmap2* bm2;

#ifdef ENABLE_DRD_CONSISTENCY_CHECKS
   tl_assert(bm);
#endif

   if (! bm_cache_lookup(bm, a1, &bm2))
   {
      bm2 = VG_(OSetGen_Lookup)(bm->oset, &a1);
      bm_update_cache(bm, a1, bm2);
   }
   return bm2;
}

static __inline__
struct bitmap2*
bm2_lookup_exclusive(struct bitmap* const bm, const UWord a1)
{
   struct bitmap2* bm2;

#ifdef ENABLE_DRD_CONSISTENCY_CHECKS
   tl_assert(bm);
#endif

   if (! bm_cache_lookup(bm, a1, &bm2))
   {
      bm2 = VG_(OSetGen_Lookup)(bm->oset, &a1);
   }

   return bm2;
}

static __inline__
void bm2_clear(struct bitmap2* const bm2)
{
#ifdef ENABLE_DRD_CONSISTENCY_CHECKS
   tl_assert(bm2);
#endif
   VG_(memset)(&bm2->bm1, 0, sizeof(bm2->bm1));
}

static __inline__
struct bitmap2* bm2_insert(struct bitmap* const bm, const UWord a1)
{
   struct bitmap2* bm2;

#ifdef ENABLE_DRD_CONSISTENCY_CHECKS
   tl_assert(bm);
#endif

   s_bitmap2_creation_count++;

   bm2 = VG_(OSetGen_AllocNode)(bm->oset, sizeof(*bm2));
   bm2->addr = a1;
   VG_(OSetGen_Insert)(bm->oset, bm2);

   bm_update_cache(bm, a1, bm2);

   return bm2;
}

static __inline__
struct bitmap2* bm2_insert_copy(struct bitmap* const bm,
                                struct bitmap2* const bm2)
{
   struct bitmap2* bm2_copy;

   bm2_copy = bm2_insert(bm, bm2->addr);
   VG_(memcpy)(&bm2_copy->bm1, &bm2->bm1, sizeof(bm2->bm1));
   return bm2_copy;
}

static __inline__
struct bitmap2* bm2_lookup_or_insert(struct bitmap* const bm, const UWord a1)
{
   struct bitmap2* bm2;

#ifdef ENABLE_DRD_CONSISTENCY_CHECKS
   tl_assert(bm);
#endif

   if (bm_cache_lookup(bm, a1, &bm2))
   {
      if (bm2 == 0)
      {
         bm2 = bm2_insert(bm, a1);
         bm2_clear(bm2);
      }
   }
   else
   {
      bm2 = VG_(OSetGen_Lookup)(bm->oset, &a1);
      if (! bm2)
      {
         bm2 = bm2_insert(bm, a1);
         bm2_clear(bm2);
      }
      bm_update_cache(bm, a1, bm2);
   }
   return bm2;
}

static __inline__
struct bitmap2* bm2_lookup_or_insert_exclusive(struct bitmap* const bm,
                                               const UWord a1)
{
   return bm2_lookup_or_insert(bm, a1);
}

static __inline__
void bm2_remove(struct bitmap* const bm, const UWord a1)
{
   struct bitmap2* bm2;

#ifdef ENABLE_DRD_CONSISTENCY_CHECKS
   tl_assert(bm);
#endif

   bm2 = VG_(OSetGen_Remove)(bm->oset, &a1);
   VG_(OSetGen_FreeNode)(bm->oset, bm2);

   bm_update_cache(bm, a1, NULL);
}

static __inline__
void bm_access_aligned_load(struct bitmap* const bm,
                            const Addr a1, const SizeT size)
{
   struct bitmap2* bm2;

#ifdef ENABLE_DRD_CONSISTENCY_CHECKS
   tl_assert(bm);
#endif

   bm2 = bm2_lookup_or_insert_exclusive(bm, address_msb(a1));
   bm0_set_range(bm2->bm1.bm0_r,
                 (a1 >> ADDR_IGNORED_BITS) & ADDR_LSB_MASK,
                 SCALED_SIZE(size));
}

static __inline__
void bm_access_aligned_store(struct bitmap* const bm,
                             const Addr a1, const SizeT size)
{
   struct bitmap2* bm2;

#ifdef ENABLE_DRD_CONSISTENCY_CHECKS
   tl_assert(bm);
#endif

   bm2 = bm2_lookup_or_insert_exclusive(bm, address_msb(a1));
   bm0_set_range(bm2->bm1.bm0_w,
                 (a1 >> ADDR_IGNORED_BITS) & ADDR_LSB_MASK,
                 SCALED_SIZE(size));
}

static __inline__
Bool bm_aligned_load_has_conflict_with(struct bitmap* const bm,
                                       const Addr a, const SizeT size)
{
   const struct bitmap2* bm2;

#ifdef ENABLE_DRD_CONSISTENCY_CHECKS
   tl_assert(bm);
#endif

   bm2 = bm2_lookup(bm, address_msb(a));
   return (bm2
           && bm0_is_any_set(bm2->bm1.bm0_w,
                             address_lsb(a),
                             SCALED_SIZE(size)));
}

static __inline__
Bool bm_aligned_store_has_conflict_with(struct bitmap* const bm,
                                        const Addr a, const SizeT size)
{
   const struct bitmap2* bm2;

#ifdef ENABLE_DRD_CONSISTENCY_CHECKS
   tl_assert(bm);
#endif

   bm2 = bm2_lookup(bm, address_msb(a));
   if (bm2)
   {
      if (bm0_is_any_set(bm2->bm1.bm0_r, address_lsb(a), SCALED_SIZE(size))
          | bm0_is_any_set(bm2->bm1.bm0_w, address_lsb(a), SCALED_SIZE(size)))
      {
         return True;
      }
   }
   return False;
}

#endif 
