

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2015-2015  Florian Krohm

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
#include "priv_aspacemgr.h"

enum {
   refcount_size = sizeof(UShort),
   slotsize_size = sizeof(UShort),
   overhead = refcount_size + slotsize_size,
   max_refcount  = 0x7fff,      
   max_slotsize  = 0xffff,      
   max_slotindex = 0x7fffffff,  
   fbit_mask = 0x80,
   end_of_chain = 0
};

#define VG_TABLE_SIZE 1000000


static HChar segnames[VG_TABLE_SIZE];  
static SizeT segnames_used = 0;        
static UInt  num_segnames = 0;         
static UInt  num_slots = 0;            
static UInt  freeslot_chain = end_of_chain;

static Bool
is_freeslot(UInt ix)
{
   aspacem_assert(ix >= overhead && ix <= segnames_used);
   return (segnames[ix - 4] & fbit_mask) != 0;
}

static void
put_slotindex(UInt ix, UInt slotindex)
{
   aspacem_assert(ix >= overhead && ix <= segnames_used);
   if (slotindex != 0)
      aspacem_assert(slotindex >= overhead && slotindex <= segnames_used);

   slotindex |= fbit_mask << 24;
   segnames[ix - 1] = slotindex & 0xFF;   slotindex >>= 8;
   segnames[ix - 2] = slotindex & 0xFF;   slotindex >>= 8;
   segnames[ix - 3] = slotindex & 0xFF;   slotindex >>= 8;
   segnames[ix - 4] = slotindex & 0xFF;
}

static UInt
get_slotindex(UInt ix)
{
   aspacem_assert(ix >= overhead && ix <= segnames_used);
   aspacem_assert(is_freeslot(ix));

   
   const UChar *unames = (const UChar *)segnames;

   UInt slotindex = 0;
   slotindex |= unames[ix - 4];   slotindex <<= 8;
   slotindex |= unames[ix - 3];   slotindex <<= 8;
   slotindex |= unames[ix - 2];   slotindex <<= 8;
   slotindex |= unames[ix - 1];

   return slotindex & max_slotindex;   
}

static void
put_slotsize(UInt ix, UInt size)
{
   aspacem_assert(ix >= overhead && ix <= segnames_used);
   aspacem_assert(size <= max_slotsize);
   segnames[ix - 1] = size & 0xff;
   segnames[ix - 2] = size >> 8;
}

static UInt
get_slotsize(UInt ix)
{
   aspacem_assert(ix >= overhead && ix <= segnames_used);

   
   const UChar *unames = (const UChar *)segnames;
   if (is_freeslot(ix))
      return (unames[ix] << 8) | unames[ix+1];
   else
      return (unames[ix - 2] << 8) | unames[ix - 1];
}

static void
put_refcount(UInt ix, UInt rc)
{
   aspacem_assert(ix >= overhead && ix <= segnames_used);
   aspacem_assert(rc <= max_refcount);
   
   segnames[ix - 3] = rc & 0xff;
   segnames[ix - 4] = rc >> 8;
}

static UInt
get_refcount(UInt ix)
{
   aspacem_assert(ix >= overhead && ix <= segnames_used);
   
   aspacem_assert(! is_freeslot(ix));

   
   const UChar *unames = (const UChar *)segnames;
   return (unames[ix - 4] << 8) | unames[ix - 3];
}

static void
inc_refcount(UInt ix)
{
   aspacem_assert(ix >= overhead && ix <= segnames_used);
   UInt rc = get_refcount(ix);
   if (rc != max_refcount)
      put_refcount(ix, rc + 1);
}

static void
dec_refcount(UInt ix)
{
   aspacem_assert(ix >= overhead && ix <= segnames_used);
   UInt rc = get_refcount(ix);
   aspacem_assert(rc > 0);
   if (rc != max_refcount) {
      --rc;
      if (rc != 0) {
         put_refcount(ix, rc);
      } else {
         UInt size = get_slotsize(ix);
         
         put_slotindex(ix, freeslot_chain);
         get_slotindex(ix);
         put_slotsize(ix + slotsize_size, size);
         get_slotindex(ix);
         freeslot_chain = ix;
         --num_segnames;
         if (0) VG_(am_show_nsegments)(0, "AFTER DECREASE rc -> 0");
      }
   }
}

static void
put_sentinel(UInt ix)
{
   aspacem_assert(ix >= overhead && ix <= segnames_used);

   put_refcount(ix, 0);
   put_slotsize(ix, 0);
}


Int
ML_(am_allocate_segname)(const HChar *name)
{
   UInt len, ix, size, next_freeslot;

   aspacem_assert(name);

   if (0) VG_(debugLog)(0, "aspacem", "allocate_segname %s\n", name);

   len = VG_(strlen)(name);

   
   for (ix = overhead; (size = get_slotsize(ix)) != 0; ix += size + overhead) {
      if (is_freeslot(ix)) continue;
      if (VG_(strcmp)(name, segnames + ix) == 0) {
         inc_refcount(ix);
         return ix;
      }
   }

   Int prev;
   for (prev = -1, ix = freeslot_chain; ix != end_of_chain;
        prev = ix, ix = next_freeslot) {
      next_freeslot = get_slotindex(ix);  
      size = get_slotsize(ix);

      if (size >= len + 1) {
         if (prev == -1)
            freeslot_chain = next_freeslot;
         else
            put_slotindex(prev, next_freeslot);
         put_refcount(ix, 1);
         put_slotsize(ix, size);
         VG_(strcpy)(segnames + ix, name);
         ++num_segnames;
         return ix;
      }
   }

   

   if (len == 0) len = 1;
   
   SizeT need = len + 1 + overhead;
   if (need > (sizeof segnames) - segnames_used) {
      return -1;
   }

   ++num_segnames;
   ++num_slots;

   
   ix = segnames_used;
   put_refcount(ix, 1);
   put_slotsize(ix, len + 1);
   VG_(strcpy)(segnames + ix, name);
   segnames_used += need;

   
   put_sentinel(segnames_used);

   return ix;
}

void
ML_(am_show_segnames)(Int logLevel, const HChar *prefix)
{
   UInt size, ix, i;

   VG_(debugLog)(logLevel, "aspacem", "%u segment names in %u slots\n",
                 num_segnames, num_slots);

   if (freeslot_chain == end_of_chain)
      VG_(debugLog)(logLevel, "aspacem", "freelist is empty\n");
   else
      VG_(debugLog)(logLevel, "aspacem", "freelist begins at %u\n",
                    freeslot_chain);
   for (i = 0, ix = overhead; (size = get_slotsize(ix)) != 0;
        ix += size + overhead, ++i) {
      if (is_freeslot(ix))
         VG_(debugLog)(logLevel, "aspacem",
                       "(%u,%u,0) [free slot: size=%u  next=%u]\n", i, ix,
                       get_slotsize(ix), get_slotindex(ix));
      else
         VG_(debugLog)(logLevel, "aspacem",
                       "(%u,%u,%u) %s\n", i, ix, get_refcount(ix),
                       segnames + ix);
   }
}

Int
ML_(am_segname_get_seqnr)(Int fnIdx)
{
   SizeT ix, size;
   Int seqnr = -1;

   if (fnIdx == -1) return -1;   

   for (ix = overhead; (size = get_slotsize(ix)) != 0; ix += size + overhead) {
      seqnr++;
      if (ix == fnIdx)
         return seqnr;
   }

   
   aspacem_assert(0);
   return -1;
}

void
ML_(am_segnames_init)(void)
{
   aspacem_assert(sizeof segnames >= overhead);

   segnames_used = overhead;
   put_sentinel(segnames_used);
}

void
ML_(am_inc_refcount)(Int ix)
{
   if (ix != -1)
      inc_refcount(ix);
}

void
ML_(am_dec_refcount)(Int ix)
{
   if (ix != -1)
      dec_refcount(ix);
}

Bool
ML_(am_sane_segname)(Int ix)
{
   return ix == -1 || (ix >= overhead && ix < segnames_used);
}

const HChar *
ML_(am_get_segname)(Int ix)
{
   return (ix == -1) ? NULL : segnames + ix;
}

