

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2013-2013 Mozilla Foundation

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


#ifndef __PRIV_IMAGE_H
#define __PRIV_IMAGE_H

#include "pub_core_basics.h"    
#include "priv_misc.h"          



typedef  struct _DiImage  DiImage;

typedef  ULong  DiOffT;

#define DiOffT_INVALID ((DiOffT)(0xFFFFFFFFFFFFFFFFULL))

DiImage* ML_(img_from_local_file)(const HChar* fullpath);

DiImage* ML_(img_from_di_server)(const HChar* filename,
                                 const HChar* serverAddr);

void ML_(img_done)(DiImage*);

DiOffT ML_(img_size)(const DiImage* img);

Bool ML_(img_valid)(const DiImage* img, DiOffT offset, SizeT size);

void ML_(img_get)(void* dst,
                  DiImage* img, DiOffT offset, SizeT size);

SizeT ML_(img_get_some)(void* dst,
                        DiImage* img, DiOffT offset, SizeT size);

HChar* ML_(img_strdup)(DiImage* img, const HChar* cc, DiOffT offset);

Int ML_(img_strcmp)(DiImage* img, DiOffT off1, DiOffT off2);

Int ML_(img_strcmp_c)(DiImage* img, DiOffT off1, const HChar* str2);

SizeT ML_(img_strlen)(DiImage* img, DiOffT off);

UChar  ML_(img_get_UChar) (DiImage* img, DiOffT offset);
UShort ML_(img_get_UShort)(DiImage* img, DiOffT offset);
UInt   ML_(img_get_UInt)  (DiImage* img, DiOffT offset);
ULong  ML_(img_get_ULong) (DiImage* img, DiOffT offset);

UInt ML_(img_calc_gnu_debuglink_crc32)(DiImage* img);



typedef
   struct { DiImage* img; DiOffT ioff; }
   DiCursor;

#define DiCursor_INVALID ((DiCursor){NULL,DiOffT_INVALID})

static inline DiCursor mk_DiCursor ( DiImage* img, DiOffT ioff ) {
   return (DiCursor){img, ioff};
}

static inline Bool ML_(cur_is_valid)(DiCursor c) {
   return c.img != NULL;
}



typedef
   struct { DiImage* img; DiOffT ioff; DiOffT szB; }
   DiSlice;

#define DiSlice_INVALID ((DiSlice){NULL,DiOffT_INVALID,0})

static inline DiSlice mk_DiSlice ( DiImage* img, DiOffT ioff, DiOffT szB ) {
   return (DiSlice){img, ioff, szB};
}

static inline Bool ML_(sli_is_valid)( DiSlice sli ) {
   return sli.img != NULL;
}

static inline DiSlice ML_(sli_from_cur)( DiCursor cur, DiOffT size ) {
   if (ML_(cur_is_valid)(cur)) {
      vg_assert(size != DiOffT_INVALID);
      vg_assert(cur.ioff + size <= ML_(img_size)(cur.img));
      return mk_DiSlice(cur.img, cur.ioff, size);
   } else {
      return DiSlice_INVALID;
   }
}

static inline DiSlice ML_(sli_from_img)( DiImage* img ) {
   if (img) {
      return mk_DiSlice(img, 0, ML_(img_size)(img));
   } else {
      return DiSlice_INVALID;
   }
}



static inline DiCursor ML_(cur_from_sli)( DiSlice sl ) {
   if (ML_(sli_is_valid)(sl)) {
      DiCursor c;
      c.img  = sl.img;
      c.ioff = sl.ioff;
      return c;
   } else {
      return DiCursor_INVALID;
   }
}

static inline Bool ML_(cur_cmpLT)( DiCursor c1, DiCursor c2 ) {
   vg_assert(c1.img == c2.img);
   return c1.ioff < c2.ioff;
}
static inline Bool ML_(cur_cmpEQ)( DiCursor c1, DiCursor c2 ) {
   vg_assert(c1.img == c2.img);
   return c1.ioff == c2.ioff;
}
static inline Bool ML_(cur_cmpGT)( DiCursor c1, DiCursor c2 ) {
   vg_assert(c1.img == c2.img);
   return c1.ioff > c2.ioff;
}

static inline DiCursor ML_(cur_plus)( DiCursor c, Long n ) {
   c.ioff += (DiOffT)n;
   return c;
}

static inline Long ML_(cur_minus)( DiCursor c1, DiCursor c2 ) {
   vg_assert(c1.img == c2.img);
   return (Long)(c1.ioff) - (Long)(c2.ioff);
}

static inline SizeT ML_(cur_strlen)( DiCursor c ) {
   return ML_(img_strlen)( c.img, c.ioff );
}

static inline HChar* ML_(cur_read_strdup)( DiCursor c, const HChar* cc ) {
   vg_assert(c.ioff != DiOffT_INVALID);
   HChar* res = ML_(img_strdup)(c.img, cc, c.ioff);
   return res;
}
static inline HChar* ML_(cur_step_strdup)( DiCursor* c, const HChar* cc ) {
   vg_assert(c->ioff != DiOffT_INVALID);
   HChar* res = ML_(img_strdup)(c->img, cc, c->ioff);
   c->ioff += VG_(strlen)(res) + 1;
   return res;
}

static inline void ML_(cur_read_get) ( void* dst,
                                       DiCursor c, SizeT size) {
   ML_(img_get)(dst, c.img, c.ioff, size);
}

static inline void ML_(cur_step_get) ( void* dst,
                                       DiCursor* c, SizeT size) {
   ML_(img_get)(dst, c->img, c->ioff, size);
   c->ioff += size;
}

static inline UChar* ML_(cur_read_memdup)( DiCursor c, SizeT size,
                                           const HChar* cc )
{
   UChar* dst = ML_(dinfo_zalloc)(cc, size);
   if (size > 0)
      ML_(cur_read_get)(dst, c, size);
   return dst;
}

static inline UChar ML_(cur_read_UChar) ( DiCursor c ) {
   UChar r = ML_(img_get_UChar)( c.img, c.ioff );
   return r;
}
static inline UChar ML_(cur_step_UChar)( DiCursor* c ) {
   UChar r = ML_(img_get_UChar)( c->img, c->ioff );
   c->ioff += sizeof(UChar);
   return r;
}

static inline UShort ML_(cur_read_UShort) ( DiCursor c ) {
   UShort r = ML_(img_get_UShort)( c.img, c.ioff );
   return r;
}
static inline UShort ML_(cur_step_UShort) ( DiCursor* c ) {
   UShort r = ML_(img_get_UShort)( c->img, c->ioff );
   c->ioff += sizeof(UShort);
   return r;
}
static inline Short ML_(cur_step_Short) ( DiCursor* c ) {
   return (Short)ML_(cur_step_UShort)( c );
}

static inline UInt ML_(cur_read_UInt) ( DiCursor c ) {
   UInt r = ML_(img_get_UInt)( c.img, c.ioff );
   return r;
}
static inline UInt ML_(cur_step_UInt) ( DiCursor* c ) {
   UInt r = ML_(img_get_UInt)( c->img, c->ioff );
   c->ioff += sizeof(UInt);
   return r;
}
static inline Int ML_(cur_step_Int) ( DiCursor* c ) {
   return (Int)ML_(cur_step_UInt)( c );
}

static inline ULong ML_(cur_read_ULong) ( DiCursor c ) {
   ULong r = ML_(img_get_ULong)( c.img, c.ioff );
   return r;
}
static inline ULong ML_(cur_step_ULong) ( DiCursor* c ) {
   ULong r = ML_(img_get_ULong)( c->img, c->ioff );
   c->ioff += sizeof(ULong);
   return r;
}
static inline Long ML_(cur_step_Long) ( DiCursor* c ) {
   return (Long)ML_(cur_step_ULong)( c );
}

static inline Addr ML_(cur_step_Addr) ( DiCursor* c ) {
   if (sizeof(Addr) == sizeof(UInt)) {
      return ML_(cur_step_UInt)(c);
   } else if  (sizeof(Addr) == sizeof(ULong)) {
      return ML_(cur_step_ULong)(c);
   } else {
      vg_assert(0);
   }
}

#endif 

