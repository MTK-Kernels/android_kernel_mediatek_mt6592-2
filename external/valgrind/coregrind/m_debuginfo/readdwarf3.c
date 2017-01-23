

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2008-2013 OpenWorks LLP
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

#if defined(VGO_linux) || defined(VGO_darwin)


#include "pub_core_basics.h"
#include "pub_core_debuginfo.h"
#include "pub_core_libcbase.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcprint.h"
#include "pub_core_libcsetjmp.h"   
#include "pub_core_hashtable.h"
#include "pub_core_options.h"
#include "pub_core_tooliface.h"    
#include "pub_core_xarray.h"
#include "pub_core_wordfm.h"
#include "priv_misc.h"             
#include "priv_image.h"
#include "priv_tytypes.h"
#include "priv_d3basics.h"
#include "priv_storage.h"
#include "priv_readdwarf3.h"       



#define TRACE_D3(format, args...) \
   if (UNLIKELY(td3)) { VG_(printf)(format, ## args); }
#define TD3 (UNLIKELY(td3))

#define D3_INVALID_CUOFF  ((UWord)(-1UL))
#define D3_FAKEVOID_CUOFF ((UWord)(-2UL))

typedef
   struct {
      DiSlice sli;      
      DiOffT  sli_next; 
      void (*barf)( const HChar* ) __attribute__((noreturn));
      const HChar* barfstr;
   }
   Cursor;

static inline Bool is_sane_Cursor ( const Cursor* c ) {
   if (!c)                return False;
   if (!c->barf)          return False;
   if (!c->barfstr)       return False;
   if (!ML_(sli_is_valid)(c->sli))    return False;
   if (c->sli.ioff == DiOffT_INVALID) return False;
   if (c->sli_next < c->sli.ioff)     return False;
   return True;
}

static void init_Cursor ( Cursor* c,
                          DiSlice sli,
                          ULong   sli_initial_offset,
                          __attribute__((noreturn)) void (*barf)(const HChar*),
                          const HChar* barfstr )
{
   vg_assert(c);
   VG_(bzero_inline)(c, sizeof(*c));
   c->sli              = sli;
   c->sli_next         = c->sli.ioff + sli_initial_offset;
   c->barf             = barf;
   c->barfstr          = barfstr;
   vg_assert(is_sane_Cursor(c));
}

static Bool is_at_end_Cursor ( const Cursor* c ) {
   vg_assert(is_sane_Cursor(c));
   return c->sli_next >= c->sli.ioff + c->sli.szB;
}

static inline ULong get_position_of_Cursor ( const Cursor* c ) {
   vg_assert(is_sane_Cursor(c));
   return c->sli_next - c->sli.ioff;
}
static inline void set_position_of_Cursor ( Cursor* c, ULong pos ) {
   c->sli_next = c->sli.ioff + pos;
   vg_assert(is_sane_Cursor(c));
}
static inline void advance_position_of_Cursor ( Cursor* c, ULong delta ) {
   c->sli_next += delta;
   vg_assert(is_sane_Cursor(c));
}

static Long get_remaining_length_Cursor ( const Cursor* c ) {
   vg_assert(is_sane_Cursor(c));
   return c->sli.ioff + c->sli.szB - c->sli_next;
}


static DiCursor get_DiCursor_from_Cursor ( const Cursor* c ) {
   return mk_DiCursor(c->sli.img, c->sli_next);
}

static inline UChar get_UChar ( Cursor* c ) {
   UChar r;
   vg_assert(is_sane_Cursor(c));
   if (c->sli_next + sizeof(UChar) > c->sli.ioff + c->sli.szB) {
      c->barf(c->barfstr);
      
      vg_assert(0);
   }
   r = ML_(img_get_UChar)(c->sli.img, c->sli_next);
   c->sli_next += sizeof(UChar);
   return r;
}
static UShort get_UShort ( Cursor* c ) {
   UShort r;
   vg_assert(is_sane_Cursor(c));
   if (c->sli_next + sizeof(UShort) > c->sli.ioff + c->sli.szB) {
      c->barf(c->barfstr);
      
      vg_assert(0);
   }
   r = ML_(img_get_UShort)(c->sli.img, c->sli_next);
   c->sli_next += sizeof(UShort);
   return r;
}
static UInt get_UInt ( Cursor* c ) {
   UInt r;
   vg_assert(is_sane_Cursor(c));
   if (c->sli_next + sizeof(UInt) > c->sli.ioff + c->sli.szB) {
      c->barf(c->barfstr);
      
      vg_assert(0);
   }
   r = ML_(img_get_UInt)(c->sli.img, c->sli_next);
   c->sli_next += sizeof(UInt);
   return r;
}
static ULong get_ULong ( Cursor* c ) {
   ULong r;
   vg_assert(is_sane_Cursor(c));
   if (c->sli_next + sizeof(ULong) > c->sli.ioff + c->sli.szB) {
      c->barf(c->barfstr);
      
      vg_assert(0);
   }
   r = ML_(img_get_ULong)(c->sli.img, c->sli_next);
   c->sli_next += sizeof(ULong);
   return r;
}
static ULong get_ULEB128 ( Cursor* c ) {
   ULong result;
   Int   shift;
   UChar byte;
   
   byte = get_UChar( c );
   result = (ULong)(byte & 0x7f);
   if (LIKELY(!(byte & 0x80))) return result;
   shift = 7;
   
   do {
      byte = get_UChar( c );
      result |= ((ULong)(byte & 0x7f)) << shift;
      shift += 7;
   } while (byte & 0x80);
   return result;
}
static Long get_SLEB128 ( Cursor* c ) {
   ULong  result = 0;
   Int    shift = 0;
   UChar  byte;
   do {
      byte = get_UChar(c);
      result |= ((ULong)(byte & 0x7f)) << shift;
      shift += 7;
   } while (byte & 0x80);
   if (shift < 64 && (byte & 0x40))
      result |= -(1ULL << shift);
   return result;
}

static DiCursor get_AsciiZ ( Cursor* c ) {
   UChar uc;
   DiCursor res = get_DiCursor_from_Cursor(c);
   do { uc = get_UChar(c); } while (uc != 0);
   return res;
}

static ULong peek_ULEB128 ( Cursor* c ) {
   DiOffT here = c->sli_next;
   ULong  r    = get_ULEB128( c );
   c->sli_next = here;
   return r;
}
static UChar peek_UChar ( Cursor* c ) {
   DiOffT here = c->sli_next;
   UChar  r    = get_UChar( c );
   c->sli_next = here;
   return r;
}

static ULong get_Dwarfish_UWord ( Cursor* c, Bool is_dw64 ) {
   return is_dw64 ? get_ULong(c) : (ULong) get_UInt(c);
}

static UWord get_UWord ( Cursor* c ) {
   vg_assert(sizeof(UWord) == sizeof(void*));
   if (sizeof(UWord) == 4) return get_UInt(c);
   if (sizeof(UWord) == 8) return get_ULong(c);
   vg_assert(0);
}

static ULong get_Initial_Length ( Bool* is64,
                                  Cursor* c, 
                                  const HChar* barfMsg )
{
   ULong w64;
   UInt  w32;
   *is64 = False;
   w32 = get_UInt( c );
   if (w32 >= 0xFFFFFFF0 && w32 < 0xFFFFFFFF) {
      c->barf( barfMsg );
   }
   else if (w32 == 0xFFFFFFFF) {
      *is64 = True;
      w64   = get_ULong( c );
   } else {
      *is64 = False;
      w64 = (ULong)w32;
   }
   return w64;
}



typedef
   struct _name_form {
      ULong at_name;  
      ULong at_form;  
      UInt  skip_szB; 
      UInt  next_nf;  
   } name_form;

typedef
   struct _g_abbv {
      struct _g_abbv *next; 
      UWord  abbv_code;     
      ULong  atag;
      ULong  has_children;
      name_form nf[0];
    } g_abbv;

typedef
   struct {
      
      void (*barf)( const HChar* ) __attribute__((noreturn));
      
      Bool   is_dw64;
      
      UShort version;
      ULong  unit_length;
      
      UWord  cu_start_offset;
      Addr   cu_svma;
      Bool   cu_svma_known;

      
      
      
      
      DiSlice debug_abbv;

      
      DiSlice escn_debug_str;
      DiSlice escn_debug_ranges;
      DiSlice escn_debug_loc;
      DiSlice escn_debug_line;
      DiSlice escn_debug_info;
      DiSlice escn_debug_types;
      DiSlice escn_debug_info_alt;
      DiSlice escn_debug_str_alt;
      UWord  types_cuOff_bias;
      UWord  alt_cuOff_bias;
      
      struct _DebugInfo* di;
      
      VgHashTable *ht_abbvs;

      Bool is_type_unit;
      ULong type_signature;
      ULong type_offset;

      VgHashTable *signature_types;

      Bool is_alt_info;
   }
   CUConst;


static UWord cook_die( const CUConst* cc, UWord die )
{
   if (cc->is_type_unit)
      die += cc->types_cuOff_bias;
   else if (cc->is_alt_info)
      die += cc->alt_cuOff_bias;
   return die;
}

static UWord cook_die_using_form( const CUConst *cc, UWord die, DW_FORM form)
{
   if (form == DW_FORM_ref_sig8)
      return die;
   if (form == DW_FORM_GNU_ref_alt)
      return die + cc->alt_cuOff_bias;
   return cook_die( cc, die );
}

static UWord uncook_die( const CUConst *cc, UWord die, Bool *type_flag,
                         Bool *alt_flag )
{
   *alt_flag = False;
   *type_flag = False;
   if (die >= cc->escn_debug_info.szB) {
      if (die >= cc->escn_debug_info.szB + cc->escn_debug_types.szB) {
         *alt_flag = True;
         die -= cc->escn_debug_info.szB + cc->escn_debug_types.szB;
      } else {
         *type_flag = True;
         die -= cc->escn_debug_info.szB;
      }
   }
   return die;
}




static void bias_GX ( GExpr* gx, const DebugInfo* di )
{
   UShort nbytes;
   UChar* p = &gx->payload[0];
   UChar* pA;
   UChar  uc;
   uc = *p++; 
   if (uc == 0)
      return;
   vg_assert(uc == 1);
   p[-1] = 0; 
   while (True) {
      uc = *p++;
      if (uc == 1)
         break; 
      vg_assert(uc == 0);
      
      pA = (UChar*)p;
      ML_(write_Addr)(pA, ML_(read_Addr)(pA) + di->text_debug_bias);
      p += sizeof(Addr);
      
      pA = (UChar*)p;
      ML_(write_Addr)(pA, ML_(read_Addr)(pA) + di->text_debug_bias);
      p += sizeof(Addr);
      
      nbytes = ML_(read_UShort)(p); p += sizeof(UShort);
      p += nbytes;
   }
}

__attribute__((noinline))
static GExpr* make_singleton_GX ( DiCursor block, ULong nbytes )
{
   SizeT  bytesReqd;
   GExpr* gx;
   UChar *p, *pstart;

   vg_assert(sizeof(UWord) == sizeof(Addr));
   vg_assert(nbytes <= 0xFFFF); 
   bytesReqd
      =   sizeof(UChar)      + sizeof(UChar) 
        + sizeof(UWord)        + sizeof(UWord) 
        + sizeof(UShort)     + (SizeT)nbytes
        + sizeof(UChar); 

   gx = ML_(dinfo_zalloc)( "di.readdwarf3.msGX.1", 
                           sizeof(GExpr) + bytesReqd );

   p = pstart = &gx->payload[0];

   p = ML_(write_UChar)(p, 0);        
   p = ML_(write_UChar)(p, 0);        
   p = ML_(write_Addr)(p, 0);         
   p = ML_(write_Addr)(p, ~0);        
   p = ML_(write_UShort)(p, nbytes);  
   ML_(cur_read_get)(p, block, nbytes); p += nbytes;
   p = ML_(write_UChar)(p, 1);        

   vg_assert( (SizeT)(p - pstart) == bytesReqd);
   vg_assert( &gx->payload[bytesReqd] 
              == ((UChar*)gx) + sizeof(GExpr) + bytesReqd );

   return gx;
}

__attribute__((noinline))
static GExpr* make_general_GX ( const CUConst* cc,
                                Bool     td3,
                                ULong    debug_loc_offset,
                                Addr     svma_of_referencing_CU )
{
   Addr      base;
   Cursor    loc;
   XArray*   xa; 
   GExpr*    gx;
   Word      nbytes;

   vg_assert(sizeof(UWord) == sizeof(Addr));
   if (!ML_(sli_is_valid)(cc->escn_debug_loc) || cc->escn_debug_loc.szB == 0)
      cc->barf("make_general_GX: .debug_loc is empty/missing");

   init_Cursor( &loc, cc->escn_debug_loc, 0, cc->barf,
                "Overrun whilst reading .debug_loc section(2)" );
   set_position_of_Cursor( &loc, debug_loc_offset );

   TRACE_D3("make_general_GX (.debug_loc_offset = %llu, ioff = %llu) {\n",
            debug_loc_offset, (ULong)get_DiCursor_from_Cursor(&loc).ioff );

   
   xa = VG_(newXA)( ML_(dinfo_zalloc), "di.readdwarf3.mgGX.1", 
                    ML_(dinfo_free),
                    sizeof(UChar) );

   { UChar c = 1;  VG_(addBytesToXA)( xa, &c, sizeof(c) ); }

   base = 0;
   while (True) {
      Bool  acquire;
      UWord len;
      UWord w1 = get_UWord( &loc );
      UWord w2 = get_UWord( &loc );

      TRACE_D3("   %08lx %08lx\n", w1, w2);
      if (w1 == 0 && w2 == 0)
         break; 

      if (w1 == -1UL) {
         
         base = w2;
         continue;
      }

      
      
      if (w1 > w2) {
         TRACE_D3("negative range is for .debug_loc expr at "
                  "file offset %llu\n", 
                  debug_loc_offset);
         cc->barf( "negative range in .debug_loc section" );
      }

      
      acquire = w1 < w2;
      len     = (UWord)get_UShort( &loc );

      if (acquire) {
         UWord  w;
         UShort s;
         UChar  c;
         c = 0; 
         VG_(addBytesToXA)( xa, &c, sizeof(c) );
         w = w1    + base + svma_of_referencing_CU;
         VG_(addBytesToXA)( xa, &w, sizeof(w) );
         w = w2 -1 + base + svma_of_referencing_CU;
         VG_(addBytesToXA)( xa, &w, sizeof(w) );
         s = (UShort)len;
         VG_(addBytesToXA)( xa, &s, sizeof(s) );
      }

      while (len > 0) {
         UChar byte = get_UChar( &loc );
         TRACE_D3("%02x", (UInt)byte);
         if (acquire)
            VG_(addBytesToXA)( xa, &byte, 1 );
         len--;
      }
      TRACE_D3("\n");
   }

   { UChar c = 1;  VG_(addBytesToXA)( xa, &c, sizeof(c) ); }

   nbytes = VG_(sizeXA)( xa );
   vg_assert(nbytes >= 1);

   gx = ML_(dinfo_zalloc)( "di.readdwarf3.mgGX.2", sizeof(GExpr) + nbytes );
   VG_(memcpy)( &gx->payload[0], (UChar*)VG_(indexXA)(xa,0), nbytes );
   vg_assert( &gx->payload[nbytes] 
              == ((UChar*)gx) + sizeof(GExpr) + nbytes );

   VG_(deleteXA)( xa );

   TRACE_D3("}\n");

   return gx;
}



typedef 
   struct { Addr aMin; Addr aMax; }
   AddrRange;


static Word cmp__XArrays_of_AddrRange ( const XArray* rngs1,
                                        const XArray* rngs2 )
{
   Word n1, n2, i;
   vg_assert(rngs1 && rngs2);
   n1 = VG_(sizeXA)( rngs1 );  
   n2 = VG_(sizeXA)( rngs2 );
   if (n1 < n2) return -1;
   if (n1 > n2) return 1;
   for (i = 0; i < n1; i++) {
      AddrRange* rng1 = (AddrRange*)VG_(indexXA)( rngs1, i );
      AddrRange* rng2 = (AddrRange*)VG_(indexXA)( rngs2, i );
      if (rng1->aMin < rng2->aMin) return -1;
      if (rng1->aMin > rng2->aMin) return 1;
      if (rng1->aMax < rng2->aMax) return -1;
      if (rng1->aMax > rng2->aMax) return 1;
   }
   return 0;
}


__attribute__((noinline))
static XArray*  empty_range_list ( void )
{
   XArray* xa; 
   
   xa = VG_(newXA)( ML_(dinfo_zalloc), "di.readdwarf3.erl.1",
                    ML_(dinfo_free),
                    sizeof(AddrRange) );
   return xa;
}


__attribute__((noinline))
static XArray* unitary_range_list ( Addr aMin, Addr aMax )
{
   XArray*   xa;
   AddrRange pair;
   vg_assert(aMin <= aMax);
   
   xa = VG_(newXA)( ML_(dinfo_zalloc),  "di.readdwarf3.url.1",
                    ML_(dinfo_free),
                    sizeof(AddrRange) );
   pair.aMin = aMin;
   pair.aMax = aMax;
   VG_(addToXA)( xa, &pair );
   return xa;
}


__attribute__((noinline))
static XArray* 
get_range_list ( const CUConst* cc,
                 Bool     td3,
                 UWord    debug_ranges_offset,
                 Addr     svma_of_referencing_CU )
{
   Addr      base;
   Cursor    ranges;
   XArray*   xa; 
   AddrRange pair;

   if (!ML_(sli_is_valid)(cc->escn_debug_ranges)
       || cc->escn_debug_ranges.szB == 0)
      cc->barf("get_range_list: .debug_ranges is empty/missing");

   init_Cursor( &ranges, cc->escn_debug_ranges, 0, cc->barf,
                "Overrun whilst reading .debug_ranges section(2)" );
   set_position_of_Cursor( &ranges, debug_ranges_offset );

   
   xa = VG_(newXA)( ML_(dinfo_zalloc), "di.readdwarf3.grl.1", ML_(dinfo_free),
                    sizeof(AddrRange) );
   base = 0;
   while (True) {
      UWord w1 = get_UWord( &ranges );
      UWord w2 = get_UWord( &ranges );

      if (w1 == 0 && w2 == 0)
         break; 

      if (w1 == -1UL) {
         
         base = w2;
         continue;
      }

      
      if (w1 > w2)
         cc->barf( "negative range in .debug_ranges section" );
      if (w1 < w2) {
         pair.aMin = w1     + base + svma_of_referencing_CU;
         pair.aMax = w2 - 1 + base + svma_of_referencing_CU;
         vg_assert(pair.aMin <= pair.aMax);
         VG_(addToXA)( xa, &pair );
      }
   }
   return xa;
}

#define VARSZ_FORM 0xffffffff
static UInt get_Form_szB (const CUConst* cc, DW_FORM form );

static void init_ht_abbvs (CUConst* cc,
                           Bool td3)
{
   Cursor c;
   g_abbv *ta; 
   UInt ta_nf_maxE; 
   UInt ta_nf_n;    
   g_abbv *ht_ta; 
   Int i;

   #define SZ_G_ABBV(_nf_szE) (sizeof(g_abbv) + _nf_szE * sizeof(name_form))

   ta_nf_maxE = 10; 
   ta = ML_(dinfo_zalloc) ("di.readdwarf3.ht_ta_nf", SZ_G_ABBV(ta_nf_maxE));
   cc->ht_abbvs = VG_(HT_construct) ("di.readdwarf3.ht_abbvs");

   init_Cursor( &c, cc->debug_abbv, 0, cc->barf,
               "Overrun whilst parsing .debug_abbrev section(2)" );
   while (True) {
      ta->abbv_code = get_ULEB128( &c );
      if (ta->abbv_code == 0) break; 

      ta->atag = get_ULEB128( &c );
      ta->has_children = get_UChar( &c );
      ta_nf_n = 0;
      while (True) {
         if (ta_nf_n >= ta_nf_maxE) {
            g_abbv *old_ta = ta;
            ta = ML_(dinfo_zalloc) ("di.readdwarf3.ht_ta_nf",
                                    SZ_G_ABBV(2 * ta_nf_maxE));
            ta_nf_maxE = 2 * ta_nf_maxE;
            VG_(memcpy) (ta, old_ta, SZ_G_ABBV(ta_nf_n));
            ML_(dinfo_free) (old_ta);
         }
         ta->nf[ta_nf_n].at_name = get_ULEB128( &c );
         ta->nf[ta_nf_n].at_form = get_ULEB128( &c );
         if (ta->nf[ta_nf_n].at_name == 0 && ta->nf[ta_nf_n].at_form == 0) {
            ta_nf_n++;
            break; 
         }
        ta_nf_n++;
      }

      
      
      
      
      ta->nf[ta_nf_n - 1].skip_szB = 0;
      ta->nf[ta_nf_n - 1].next_nf = 0;
      for (i = ta_nf_n - 2; i >= 0; i--) {
         const UInt form_szB = get_Form_szB (cc, (DW_FORM)ta->nf[i].at_form);
          
         if (ta->nf[i+1].at_name == DW_AT_sibling
             || ta->nf[i+1].skip_szB == VARSZ_FORM) {
            ta->nf[i].skip_szB = form_szB;
            ta->nf[i].next_nf  = i+1;
         } else if (form_szB == VARSZ_FORM) {
            ta->nf[i].skip_szB = form_szB;
            ta->nf[i].next_nf  = i+1;
         } else {
            ta->nf[i].skip_szB = ta->nf[i+1].skip_szB + form_szB;
            ta->nf[i].next_nf  = ta->nf[i+1].next_nf;
         }
      }

      ht_ta = ML_(dinfo_zalloc) ("di.readdwarf3.ht_ta", SZ_G_ABBV(ta_nf_n));
      VG_(memcpy) (ht_ta, ta, SZ_G_ABBV(ta_nf_n));
      VG_(HT_add_node) ( cc->ht_abbvs, ht_ta );
      if (TD3) {
         TRACE_D3("  Adding abbv_code %llu TAG  %s [%s] nf %d ",
                  (ULong) ht_ta->abbv_code, ML_(pp_DW_TAG)(ht_ta->atag),
                  ML_(pp_DW_children)(ht_ta->has_children),
                  ta_nf_n);
         TRACE_D3("  ");
         for (i = 0; i < ta_nf_n; i++)
            TRACE_D3("[%u,%u] ", ta->nf[i].skip_szB, ta->nf[i].next_nf);
         TRACE_D3("\n");
      }
   }

   ML_(dinfo_free) (ta);
   #undef SZ_G_ABBV
}

static g_abbv* get_abbv (const CUConst* cc, ULong abbv_code)
{
   g_abbv *abbv;

   abbv = VG_(HT_lookup) (cc->ht_abbvs, abbv_code);
   if (!abbv)
      cc->barf ("abbv_code not found in ht_abbvs table");
   return abbv;
}

static void clear_CUConst (CUConst* cc)
{
   VG_(HT_destruct) ( cc->ht_abbvs, ML_(dinfo_free));
   cc->ht_abbvs = NULL;
}

static __attribute__((noinline))
void parse_CU_Header ( CUConst* cc,
                       Bool td3,
                       Cursor* c, 
                       DiSlice escn_debug_abbv,
		       Bool type_unit,
                       Bool alt_info )
{
   UChar  address_size;
   ULong  debug_abbrev_offset;

   VG_(memset)(cc, 0, sizeof(*cc));
   vg_assert(c && c->barf);
   cc->barf = c->barf;

   
   cc->unit_length 
      = get_Initial_Length( &cc->is_dw64, c, 
           "parse_CU_Header: invalid initial-length field" );

   TRACE_D3("   Length:        %lld\n", cc->unit_length );

   
   cc->version = get_UShort( c );
   if (cc->version != 2 && cc->version != 3 && cc->version != 4)
      cc->barf( "parse_CU_Header: is neither DWARF2 nor DWARF3 nor DWARF4" );
   TRACE_D3("   Version:       %d\n", (Int)cc->version );

   
   debug_abbrev_offset = get_Dwarfish_UWord( c, cc->is_dw64 );
   if (debug_abbrev_offset >= escn_debug_abbv.szB)
      cc->barf( "parse_CU_Header: invalid debug_abbrev_offset" );
   TRACE_D3("   Abbrev Offset: %lld\n", debug_abbrev_offset );

   address_size = get_UChar( c );
   if (address_size != sizeof(void*))
      cc->barf( "parse_CU_Header: invalid address_size" );
   TRACE_D3("   Pointer Size:  %d\n", (Int)address_size );

   cc->is_type_unit = type_unit;
   cc->is_alt_info = alt_info;

   if (type_unit) {
      cc->type_signature = get_ULong( c );
      cc->type_offset = get_Dwarfish_UWord( c, cc->is_dw64 );
   }

   vg_assert(debug_abbrev_offset < escn_debug_abbv.szB);
   cc->debug_abbv      = escn_debug_abbv;
   cc->debug_abbv.ioff += debug_abbrev_offset;
   cc->debug_abbv.szB  -= debug_abbrev_offset;

   init_ht_abbvs(cc, td3);
}

typedef
   struct D3SignatureType {
      struct D3SignatureType *next;
      UWord data;
      ULong type_signature;
      UWord die;
   }
   D3SignatureType;

static void record_signatured_type ( VgHashTable *tab,
                                     ULong type_signature,
                                     UWord die )
{
   D3SignatureType *dstype = ML_(dinfo_zalloc) ( "di.readdwarf3.sigtype",
                                                 sizeof(D3SignatureType) );
   dstype->data = (UWord) type_signature;
   dstype->type_signature = type_signature;
   dstype->die = die;
   VG_(HT_add_node) ( tab, dstype );
}

static UWord lookup_signatured_type ( const VgHashTable *tab,
                                      ULong type_signature,
                                      void (*barf)( const HChar* ) __attribute__((noreturn)) )
{
   D3SignatureType *dstype = VG_(HT_lookup) ( tab, (UWord) type_signature );
   while ( dstype != NULL && dstype->type_signature != type_signature)
      dstype = dstype->next;
   if (dstype == NULL) {
      barf("lookup_signatured_type: could not find signatured type");
      
      vg_assert(0);
   }
   return dstype->die;
}


typedef
   struct {
      Long szB; 
      union { ULong val; DiCursor cur; } u;
   }
   FormContents;

static
void get_Form_contents ( FormContents* cts,
                         const CUConst* cc, Cursor* c,
                         Bool td3, DW_FORM form )
{
   VG_(bzero_inline)(cts, sizeof(*cts));
   
   
   
   switch (form) {
      case DW_FORM_data1:
         cts->u.val = (ULong)(UChar)get_UChar(c);
         cts->szB   = 1;
         TRACE_D3("%u", (UInt)cts->u.val);
         break;
      case DW_FORM_data2:
         cts->u.val = (ULong)(UShort)get_UShort(c);
         cts->szB   = 2;
         TRACE_D3("%u", (UInt)cts->u.val);
         break;
      case DW_FORM_data4:
         cts->u.val = (ULong)(UInt)get_UInt(c);
         cts->szB   = 4;
         TRACE_D3("%u", (UInt)cts->u.val);
         break;
      case DW_FORM_data8:
         cts->u.val = get_ULong(c);
         cts->szB   = 8;
         TRACE_D3("%llu", cts->u.val);
         break;
      case DW_FORM_sec_offset:
         cts->u.val = (ULong)get_Dwarfish_UWord( c, cc->is_dw64 );
         cts->szB   = cc->is_dw64 ? 8 : 4;
         TRACE_D3("%llu", cts->u.val);
         break;
      case DW_FORM_sdata:
         cts->u.val = (ULong)(Long)get_SLEB128(c);
         cts->szB   = 8;
         TRACE_D3("%lld", (Long)cts->u.val);
         break;
      case DW_FORM_udata:
         cts->u.val = (ULong)(Long)get_ULEB128(c);
         cts->szB   = 8;
         TRACE_D3("%llu", (Long)cts->u.val);
         break;
      case DW_FORM_addr:
         cts->u.val = (ULong)(UWord)get_UWord(c);
         cts->szB   = sizeof(UWord);
         TRACE_D3("0x%lx", (UWord)cts->u.val);
         break;

      case DW_FORM_ref_addr:
         
         if (cc->version == 2) {
            cts->u.val = (ULong)(UWord)get_UWord(c);
            cts->szB   = sizeof(UWord);
         } else {
            cts->u.val = get_Dwarfish_UWord(c, cc->is_dw64);
            cts->szB   = cc->is_dw64 ? sizeof(ULong) : sizeof(UInt);
         }
         TRACE_D3("0x%lx", (UWord)cts->u.val);
         if (0) VG_(printf)("DW_FORM_ref_addr 0x%lx\n", (UWord)cts->u.val);
         if (
             !ML_(sli_is_valid)(cc->escn_debug_info)
             || cts->u.val >= (ULong)cc->escn_debug_info.szB) {
            cc->barf("get_Form_contents: DW_FORM_ref_addr points "
                     "outside .debug_info");
         }
         break;

      case DW_FORM_strp: {
         
         UWord uw = (UWord)get_Dwarfish_UWord( c, cc->is_dw64 );
         if (!ML_(sli_is_valid)(cc->escn_debug_str)
             || uw >= cc->escn_debug_str.szB)
            cc->barf("get_Form_contents: DW_FORM_strp "
                     "points outside .debug_str");
         DiCursor str
            = ML_(cur_plus)( ML_(cur_from_sli)(cc->escn_debug_str), uw );
         if (TD3) {
            HChar* tmp = ML_(cur_read_strdup)(str, "di.getFC.1");
            TRACE_D3("(indirect string, offset: 0x%lx): %s", uw, tmp);
            ML_(dinfo_free)(tmp);
         }
         cts->u.cur = str;
         cts->szB   = - (Long)(1 + (ULong)ML_(cur_strlen)(str));
         break;
      }
      case DW_FORM_string: {
         DiCursor str = get_AsciiZ(c);
         if (TD3) {
            HChar* tmp = ML_(cur_read_strdup)(str, "di.getFC.2");
            TRACE_D3("%s", tmp);
            ML_(dinfo_free)(tmp);
         }
         cts->u.cur = str;
         cts->szB   = - (Long)(1 + (ULong)ML_(cur_strlen)(str));
         break;
      }
      case DW_FORM_ref1: {
         UChar u8   = get_UChar(c);
         UWord res  = cc->cu_start_offset + (UWord)u8;
         cts->u.val = (ULong)res;
         cts->szB   = sizeof(UWord);
         TRACE_D3("<%lx>", res);
         break;
      }
      case DW_FORM_ref2: {
         UShort u16 = get_UShort(c);
         UWord  res = cc->cu_start_offset + (UWord)u16;
         cts->u.val = (ULong)res;
         cts->szB   = sizeof(UWord);
         TRACE_D3("<%lx>", res);
         break;
      }
      case DW_FORM_ref4: {
         UInt  u32  = get_UInt(c);
         UWord res  = cc->cu_start_offset + (UWord)u32;
         cts->u.val = (ULong)res;
         cts->szB   = sizeof(UWord);
         TRACE_D3("<%lx>", res);
         break;
      }
      case DW_FORM_ref8: {
         ULong u64  = get_ULong(c);
         UWord res  = cc->cu_start_offset + (UWord)u64;
         cts->u.val = (ULong)res;
         cts->szB   = sizeof(UWord);
         TRACE_D3("<%lx>", res);
         break;
      }
      case DW_FORM_ref_udata: {
         ULong u64  = get_ULEB128(c);
         UWord res  = cc->cu_start_offset + (UWord)u64;
         cts->u.val = (ULong)res;
         cts->szB   = sizeof(UWord);
         TRACE_D3("<%lx>", res);
         break;
      }
      case DW_FORM_flag: {
         UChar u8 = get_UChar(c);
         TRACE_D3("%u", (UInt)u8);
         cts->u.val = (ULong)u8;
         cts->szB   = 1;
         break;
      }
      case DW_FORM_flag_present:
         TRACE_D3("1");
         cts->u.val = 1;
         cts->szB   = 1;
         break;
      case DW_FORM_block1: {
         ULong    u64b;
         ULong    u64   = (ULong)get_UChar(c);
         DiCursor block = get_DiCursor_from_Cursor(c);
         TRACE_D3("%llu byte block: ", u64);
         for (u64b = u64; u64b > 0; u64b--) {
            UChar u8 = get_UChar(c);
            TRACE_D3("%x ", (UInt)u8);
         }
         cts->u.cur = block;
         cts->szB   = - (Long)u64;
         break;
      }
      case DW_FORM_block2: {
         ULong    u64b;
         ULong    u64   = (ULong)get_UShort(c);
         DiCursor block = get_DiCursor_from_Cursor(c);
         TRACE_D3("%llu byte block: ", u64);
         for (u64b = u64; u64b > 0; u64b--) {
            UChar u8 = get_UChar(c);
            TRACE_D3("%x ", (UInt)u8);
         }
         cts->u.cur = block;
         cts->szB   = - (Long)u64;
         break;
      }
      case DW_FORM_block4: {
         ULong    u64b;
         ULong    u64   = (ULong)get_UInt(c);
         DiCursor block = get_DiCursor_from_Cursor(c);
         TRACE_D3("%llu byte block: ", u64);
         for (u64b = u64; u64b > 0; u64b--) {
            UChar u8 = get_UChar(c);
            TRACE_D3("%x ", (UInt)u8);
         }
         cts->u.cur = block;
         cts->szB   = - (Long)u64;
         break;
      }
      case DW_FORM_exprloc:
      case DW_FORM_block: {
         ULong    u64b;
         ULong    u64   = (ULong)get_ULEB128(c);
         DiCursor block = get_DiCursor_from_Cursor(c);
         TRACE_D3("%llu byte block: ", u64);
         for (u64b = u64; u64b > 0; u64b--) {
            UChar u8 = get_UChar(c);
            TRACE_D3("%x ", (UInt)u8);
         }
         cts->u.cur = block;
         cts->szB   = - (Long)u64;
         break;
      }
      case DW_FORM_ref_sig8: {
         ULong  u64b;
         ULong  signature = get_ULong (c);
         ULong  work = signature;
         TRACE_D3("8 byte signature: ");
         for (u64b = 8; u64b > 0; u64b--) {
            UChar u8 = work & 0xff;
            TRACE_D3("%x ", (UInt)u8);
            work >>= 8;
         }

         if (VG_(clo_read_var_info)) {
            cts->u.val = lookup_signatured_type (cc->signature_types, signature,
                                                 c->barf);
         } else {
            vg_assert (td3);
            vg_assert (VG_(clo_read_inline_info));
            TRACE_D3("<not dereferencing signature type>");
            cts->u.val = 0; 
         }
         cts->szB   = sizeof(UWord);
         break;
      }
      case DW_FORM_indirect:
         get_Form_contents (cts, cc, c, td3, (DW_FORM)get_ULEB128(c));
         return;

      case DW_FORM_GNU_ref_alt:
         cts->u.val = get_Dwarfish_UWord(c, cc->is_dw64);
         cts->szB   = cc->is_dw64 ? sizeof(ULong) : sizeof(UInt);
         TRACE_D3("0x%lx", (UWord)cts->u.val);
         if (0) VG_(printf)("DW_FORM_GNU_ref_alt 0x%lx\n", (UWord)cts->u.val);
         if (
             !ML_(sli_is_valid)(cc->escn_debug_info_alt))
            cc->barf("get_Form_contents: DW_FORM_GNU_ref_addr used, "
                     "but no alternate .debug_info");
         else if (cts->u.val >= (ULong)cc->escn_debug_info_alt.szB) {
            cc->barf("get_Form_contents: DW_FORM_GNU_ref_addr points "
                     "outside alternate .debug_info");
         }
         break;

      case DW_FORM_GNU_strp_alt: {
         
         SizeT uw = (UWord)get_Dwarfish_UWord( c, cc->is_dw64 );
         if (!ML_(sli_is_valid)(cc->escn_debug_str_alt))
            cc->barf("get_Form_contents: DW_FORM_GNU_strp_alt used, "
                     "but no alternate .debug_str");
         else if (uw >= cc->escn_debug_str_alt.szB)
            cc->barf("get_Form_contents: DW_FORM_GNU_strp_alt "
                     "points outside alternate .debug_str");
         DiCursor str
            = ML_(cur_plus)( ML_(cur_from_sli)(cc->escn_debug_str_alt), uw);
         if (TD3) {
            HChar* tmp = ML_(cur_read_strdup)(str, "di.getFC.3");
            TRACE_D3("(indirect alt string, offset: 0x%lx): %s", uw, tmp);
            ML_(dinfo_free)(tmp);
         }
         cts->u.cur = str;
         cts->szB   = - (Long)(1 + (ULong)ML_(cur_strlen)(str));
         break;
      }

      default:
         VG_(printf)(
            "get_Form_contents: unhandled %d (%s) at <%llx>\n",
            form, ML_(pp_DW_FORM)(form), get_position_of_Cursor(c));
         c->barf("get_Form_contents: unhandled DW_FORM");
   }
}

static inline UInt sizeof_Dwarfish_UWord (Bool is_dw64)
{
   if (is_dw64)
      return sizeof(ULong);
   else
      return sizeof(UInt);
}

#define VARSZ_FORM 0xffffffff
static
UInt get_Form_szB (const CUConst* cc, DW_FORM form )
{
   
   
   
   
   switch (form) {
      case DW_FORM_data1: return 1;
      case DW_FORM_data2: return 2;
      case DW_FORM_data4: return 4;
      case DW_FORM_data8: return 8;
      case DW_FORM_sec_offset:
         if (cc->is_dw64)
            return 8;
         else
            return 4;
      case DW_FORM_sdata:
         return VARSZ_FORM;
      case DW_FORM_udata:
         return VARSZ_FORM;
      case DW_FORM_addr: 
         return sizeof(UWord);
      case DW_FORM_ref_addr: 
         if (cc->version == 2)
            return sizeof(UWord);
         else 
            return sizeof_Dwarfish_UWord (cc->is_dw64);
      case DW_FORM_strp:
         return sizeof_Dwarfish_UWord (cc->is_dw64);
      case DW_FORM_string: 
         return VARSZ_FORM;
      case DW_FORM_ref1:
         return 1;
      case DW_FORM_ref2:
         return 2;
      case DW_FORM_ref4:
         return 4;
      case DW_FORM_ref8:
         return 8;
      case DW_FORM_ref_udata:
         return VARSZ_FORM;
      case DW_FORM_flag: 
         return 1;
      case DW_FORM_flag_present:
         return 0; 
      case DW_FORM_block1:
         return VARSZ_FORM;
      case DW_FORM_block2:
         return VARSZ_FORM;
      case DW_FORM_block4:
         return VARSZ_FORM;
      case DW_FORM_exprloc:
      case DW_FORM_block:
         return VARSZ_FORM;
      case DW_FORM_ref_sig8:
         return 8;
      case DW_FORM_indirect:
         return VARSZ_FORM;
      case DW_FORM_GNU_ref_alt:
         return sizeof_Dwarfish_UWord(cc->is_dw64);
      case DW_FORM_GNU_strp_alt:
         return sizeof_Dwarfish_UWord(cc->is_dw64);
      default:
         VG_(printf)(
            "get_Form_szB: unhandled %d (%s)\n",
            form, ML_(pp_DW_FORM)(form));
         cc->barf("get_Form_contents: unhandled DW_FORM");
   }
}

static
void skip_DIE (UWord  *sibling,
               Cursor* c_die,
               const g_abbv *abbv,
               const CUConst* cc)
{
   UInt nf_i;
   FormContents cts;
   nf_i = 0;
   while (True) {
      if (abbv->nf[nf_i].at_name == DW_AT_sibling) {
         get_Form_contents( &cts, cc, c_die, False ,
                            (DW_FORM)abbv->nf[nf_i].at_form );
         if ( cts.szB > 0 ) 
            *sibling = cts.u.val;
         nf_i++;
      } else if (abbv->nf[nf_i].skip_szB == VARSZ_FORM) {
         get_Form_contents( &cts, cc, c_die, False ,
                            (DW_FORM)abbv->nf[nf_i].at_form );
         nf_i++;
      } else {
         advance_position_of_Cursor (c_die, (ULong)abbv->nf[nf_i].skip_szB);
         nf_i = abbv->nf[nf_i].next_nf;
      }
      if (nf_i == 0)
         break;
   }
}



typedef
   struct _TempVar {
      const HChar*  name; 
      UWord   nRanges;
      Addr    rngOneMin;
      Addr    rngOneMax;
      XArray* rngMany; 
      
      Int     level;
      UWord   typeR; 
      GExpr*  gexpr; 
      GExpr*  fbGX;  
      UInt    fndn_ix; 
      Int     fLine; 
      UWord   dioff;
      UWord   absOri; 
   }
   TempVar;

typedef
   struct {
      Int     sp; 
      Int     stack_size;
      XArray **ranges; 
      Int     *level;  
      Bool    *isFunc; 
      GExpr  **fbGX;   
      XArray*  fndn_ix_Table;
   }
   D3VarParser;

static void
var_parser_init ( D3VarParser *parser )
{
   parser->sp = -1;
   parser->stack_size = 0;
   parser->ranges = NULL;
   parser->level  = NULL;
   parser->isFunc = NULL;
   parser->fbGX = NULL;
   parser->fndn_ix_Table = NULL;
}

static void
var_parser_release ( D3VarParser *parser )
{
   ML_(dinfo_free)( parser->ranges );
   ML_(dinfo_free)( parser->level );
   ML_(dinfo_free)( parser->isFunc );
   ML_(dinfo_free)( parser->fbGX );
}

static void varstack_show ( const D3VarParser* parser, const HChar* str )
{
   Word i, j;
   VG_(printf)("  varstack (%s) {\n", str);
   for (i = 0; i <= parser->sp; i++) {
      XArray* xa = parser->ranges[i];
      vg_assert(xa);
      VG_(printf)("    [%ld] (level %d)", i, parser->level[i]);
      if (parser->isFunc[i]) {
         VG_(printf)(" (fbGX=%p)", parser->fbGX[i]);
      } else {
         vg_assert(parser->fbGX[i] == NULL);
      }
      VG_(printf)(": ");
      if (VG_(sizeXA)( xa ) == 0) {
         VG_(printf)("** empty PC range array **");
      } else {
         for (j = 0; j < VG_(sizeXA)( xa ); j++) {
            AddrRange* range = (AddrRange*) VG_(indexXA)( xa, j );
            vg_assert(range);
            VG_(printf)("[%#lx,%#lx] ", range->aMin, range->aMax);
         }
      }
      VG_(printf)("\n");
   }
   VG_(printf)("  }\n");
}

static 
void varstack_preen ( D3VarParser* parser, Bool td3, Int level )
{
   Bool changed = False;
   vg_assert(parser->sp < parser->stack_size);
   while (True) {
      vg_assert(parser->sp >= -1);
      if (parser->sp == -1) break;
      if (parser->level[parser->sp] <= level) break;
      if (0) 
         TRACE_D3("BBBBAAAA varstack_pop [newsp=%d]\n", parser->sp-1);
      vg_assert(parser->ranges[parser->sp]);
      VG_(deleteXA)( parser->ranges[parser->sp] );
      parser->sp--;
      changed = True;
   }
   if (changed && td3)
      varstack_show( parser, "after preen" );
}

static void varstack_push ( const CUConst* cc,
                            D3VarParser* parser,
                            Bool td3,
                            XArray* ranges, Int level,
                            Bool    isFunc, GExpr* fbGX ) {
   if (0)
   TRACE_D3("BBBBAAAA varstack_push[newsp=%d]: %d  %p\n",
            parser->sp+1, level, ranges);

   varstack_preen(parser, False, level-1);

   vg_assert(parser->sp >= -1);
   vg_assert(parser->sp < parser->stack_size);
   if (parser->sp == parser->stack_size - 1) {
      parser->stack_size += 48;
      parser->ranges =
         ML_(dinfo_realloc)("di.readdwarf3.varpush.1", parser->ranges,
                            parser->stack_size * sizeof parser->ranges[0]);
      parser->level =
         ML_(dinfo_realloc)("di.readdwarf3.varpush.2", parser->level,
                            parser->stack_size * sizeof parser->level[0]);
      parser->isFunc =
         ML_(dinfo_realloc)("di.readdwarf3.varpush.3", parser->isFunc,
                            parser->stack_size * sizeof parser->isFunc[0]);
      parser->fbGX =
         ML_(dinfo_realloc)("di.readdwarf3.varpush.4", parser->fbGX,
                            parser->stack_size * sizeof parser->fbGX[0]);
   }
   if (parser->sp >= 0)
      vg_assert(parser->level[parser->sp] < level);
   parser->sp++;
   vg_assert(ranges != NULL);
   if (!isFunc) vg_assert(fbGX == NULL);
   parser->ranges[parser->sp] = ranges;
   parser->level[parser->sp]  = level;
   parser->isFunc[parser->sp] = isFunc;
   parser->fbGX[parser->sp]   = fbGX;
   if (TD3)
      varstack_show( parser, "after push" );
}


__attribute__((noinline))
static GExpr* get_GX ( const CUConst* cc, Bool td3, const FormContents* cts )
{
   GExpr* gexpr = NULL;
   if (cts->szB < 0) {
      gexpr = make_singleton_GX( cts->u.cur, (ULong)(- cts->szB) );
   }
   else 
   if (cts->szB > 0) {
      if (!cc->cu_svma_known)
         cc->barf("get_GX: location list, but CU svma is unknown");
      gexpr = make_general_GX( cc, td3, cts->u.val, cc->cu_svma );
   }
   else {
      vg_assert(0); 
   }
   return gexpr;
}

static
XArray* read_dirname_xa (DebugInfo* di, const HChar *compdir,
                         Cursor *c,
                         Bool td3 )
{
   XArray*        dirname_xa;   
   const HChar*   dirname;
   UInt           compdir_len;

   dirname_xa = VG_(newXA) (ML_(dinfo_zalloc), "di.rdxa.1", ML_(dinfo_free),
                            sizeof(HChar*) );

   if (compdir == NULL) {
      dirname = ".";
      compdir_len = 1;
   } else {
      dirname = compdir;
      compdir_len = VG_(strlen)(compdir);
   }
   VG_(addToXA) (dirname_xa, &dirname);

   TRACE_D3(" The Directory Table%s\n", 
            peek_UChar(c) == 0 ? " is empty." : ":" );

   while (peek_UChar(c) != 0) {

      DiCursor cur = get_AsciiZ(c);
      HChar* data_str = ML_(cur_read_strdup)( cur, "dirname_xa.1" );
      TRACE_D3("  %s\n", data_str);


      if (data_str[0] != '/' 
          
          && compdir
          
          && compdir_len)
      {
         SizeT  len = compdir_len + 1 + VG_(strlen)(data_str);
         HChar *buf = ML_(dinfo_zalloc)("dirname_xa.2", len + 1);

         VG_(strcpy)(buf, compdir);
         VG_(strcat)(buf, "/");
         VG_(strcat)(buf, data_str);

         dirname = ML_(addStr)(di, buf, len);
         VG_(addToXA) (dirname_xa, &dirname);
         if (0) VG_(printf)("rel path  %s\n", buf);
         ML_(dinfo_free)(buf);
      } else {
         
         dirname = ML_(addStr)(di,data_str,-1);
         VG_(addToXA) (dirname_xa, &dirname);
         if (0) VG_(printf)("abs path  %s\n", data_str);
      }

      ML_(dinfo_free)(data_str);
   }

   TRACE_D3 ("\n");

   if (get_UChar (c) != 0) {
      ML_(symerr)(NULL, True,
                  "could not get NUL at end of DWARF directory table");
      VG_(deleteXA)(dirname_xa);
      return NULL;
   }

   return dirname_xa;
}

static 
void read_filename_table( XArray*  fndn_ix_Table,
                          const HChar* compdir,
                          const CUConst* cc, ULong debug_line_offset,
                          Bool td3 )
{
   Bool   is_dw64;
   Cursor c;
   Word   i;
   UShort version;
   UChar  opcode_base;
   const HChar* str;
   XArray* dirname_xa;   
   ULong  dir_xa_ix;     
   const HChar* dirname;
   UInt   fndn_ix;

   vg_assert(fndn_ix_Table && cc && cc->barf);
   if (!ML_(sli_is_valid)(cc->escn_debug_line)
       || cc->escn_debug_line.szB <= debug_line_offset) {
      cc->barf("read_filename_table: .debug_line is missing?");
   }

   init_Cursor( &c, cc->escn_debug_line, debug_line_offset, cc->barf, 
                "Overrun whilst reading .debug_line section(1)" );

   
   get_Initial_Length( &is_dw64, &c,
                       "read_filename_table: invalid initial-length field" );
   version = get_UShort( &c );
   if (version != 2 && version != 3 && version != 4)
     cc->barf("read_filename_table: Only DWARF version 2, 3 and 4 line info "
              "is currently supported.");
    get_Dwarfish_UWord( &c, is_dw64 );
    get_UChar( &c );
   if (version >= 4)
       get_UChar( &c );
    get_UChar( &c );
    get_UChar( &c );
    get_UChar( &c );
   opcode_base                = get_UChar( &c );
   
   for (i = 1; i < (Word)opcode_base; i++)
     (void)get_UChar( &c );

   dirname_xa = read_dirname_xa(cc->di, compdir, &c, td3);

   
   vg_assert( VG_(sizeXA)( fndn_ix_Table ) == 0 );
   fndn_ix = ML_(addFnDn) ( cc->di, "<unknown_file>", NULL );
   VG_(addToXA)( fndn_ix_Table, &fndn_ix );
   while (peek_UChar(&c) != 0) {
      DiCursor cur = get_AsciiZ(&c);
      str = ML_(addStrFromCursor)( cc->di, cur );
      dir_xa_ix = get_ULEB128( &c );
      if (dirname_xa != NULL 
          && dir_xa_ix >= 0 && dir_xa_ix < VG_(sizeXA) (dirname_xa))
         dirname = *(HChar**)VG_(indexXA) ( dirname_xa, dir_xa_ix );
      else
         dirname = NULL;
      fndn_ix = ML_(addFnDn)( cc->di, str, dirname);
      TRACE_D3("  read_filename_table: %ld fndn_ix %d %s %s\n",
               VG_(sizeXA)(fndn_ix_Table), fndn_ix, 
               dirname, str);
      VG_(addToXA)( fndn_ix_Table, &fndn_ix );
      (void)get_ULEB128( &c ); 
      (void)get_ULEB128( &c ); 
   }
   
   if (dirname_xa != NULL)
      VG_(deleteXA)(dirname_xa);
}

static void setup_cu_svma(CUConst* cc, Bool have_lo, Addr ip_lo, Bool td3)
{
   Addr cu_svma;

   if (have_lo)
      cu_svma = ip_lo;
   else {
      cu_svma = 0;
   }

   if (cc->cu_svma_known) {
      vg_assert (cu_svma == cc->cu_svma);
   } else {
      cc->cu_svma_known = True;
      cc->cu_svma = cu_svma;
      if (0)
         TRACE_D3("setup_cu_svma: acquire CU_SVMA of %p\n", (void*) cc->cu_svma);
   }
}

static void trace_DIE(
   DW_TAG dtag,
   UWord posn,
   Int level,
   UWord saved_die_c_offset,
   const g_abbv *abbv,
   const CUConst* cc)
{
   Cursor c;
   FormContents cts;
   UWord sibling = 0;
   UInt nf_i;
   Bool  debug_types_flag;
   Bool  alt_flag;
   Cursor check_skip;
   UWord check_sibling = 0;

   posn = uncook_die( cc, posn, &debug_types_flag, &alt_flag );
   init_Cursor (&c, 
                debug_types_flag ? cc->escn_debug_types :
                alt_flag ? cc->escn_debug_info_alt : cc->escn_debug_info,
                saved_die_c_offset, cc->barf, 
                "Overrun trace_DIE");
   check_skip = c;
   VG_(printf)(" <%d><%lx>: Abbrev Number: %llu (%s)%s%s\n",
               level, posn, (ULong) abbv->abbv_code, ML_(pp_DW_TAG)( dtag ),
               debug_types_flag ? " (in .debug_types)" : "",
               alt_flag ? " (in alternate .debug_info)" : "");
   nf_i = 0;
   while (True) {
      DW_AT   attr = (DW_AT)  abbv->nf[nf_i].at_name;
      DW_FORM form = (DW_FORM)abbv->nf[nf_i].at_form;
      nf_i++;
      if (attr == 0 && form == 0) break;
      VG_(printf)("     %-18s: ", ML_(pp_DW_AT)(attr));
      
      get_Form_contents( &cts, cc, &c, True, form );
      if (attr == DW_AT_sibling && cts.szB > 0) {
         sibling = cts.u.val;
      }
      VG_(printf)("\t\n");
   }

   skip_DIE (&check_sibling, &check_skip, abbv, cc);
   vg_assert (check_sibling == sibling);
   vg_assert (get_position_of_Cursor (&check_skip) 
              == get_position_of_Cursor (&c));
}

__attribute__((noreturn))
static void dump_bad_die_and_barf(
   const HChar *whichparser,
   DW_TAG dtag,
   UWord posn,
   Int level,
   Cursor* c_die,
   UWord saved_die_c_offset,
   const g_abbv *abbv,
   const CUConst* cc)
{
   trace_DIE (dtag, posn, level, saved_die_c_offset, abbv, cc);
   VG_(printf)("%s:\n", whichparser);
   cc->barf("confused by the above DIE");
}

__attribute__((noinline))
static void bad_DIE_confusion(int linenr)
{
   VG_(printf)("\nparse DIE(readdwarf3.c:%d): confused by:\n", linenr);
}
#define goto_bad_DIE do {bad_DIE_confusion(__LINE__); goto bad_DIE;} while (0)

__attribute__((noinline))
static void parse_var_DIE (
   WordFM*  rangestree,
   XArray*  tempvars,
   XArray*  gexprs,
   D3VarParser* parser,
   DW_TAG dtag,
   UWord posn,
   Int level,
   Cursor* c_die,
   const g_abbv *abbv,
   CUConst* cc,
   Bool td3
)
{
   FormContents cts;
   UInt nf_i;

   UWord saved_die_c_offset  = get_position_of_Cursor( c_die );

   varstack_preen( parser, td3, level-1 );

   if (dtag == DW_TAG_compile_unit
       || dtag == DW_TAG_type_unit
       || dtag == DW_TAG_partial_unit) {
      Bool have_lo    = False;
      Bool have_hi1   = False;
      Bool hiIsRelative = False;
      Bool have_range = False;
      Addr ip_lo    = 0;
      Addr ip_hi1   = 0;
      Addr rangeoff = 0;
      const HChar *compdir = NULL;
      nf_i = 0;
      while (True) {
         DW_AT   attr = (DW_AT)  abbv->nf[nf_i].at_name;
         DW_FORM form = (DW_FORM)abbv->nf[nf_i].at_form;
         nf_i++;
         if (attr == 0 && form == 0) break;
         get_Form_contents( &cts, cc, c_die, False, form );
         if (attr == DW_AT_low_pc && cts.szB > 0) {
            ip_lo   = cts.u.val;
            have_lo = True;
         }
         if (attr == DW_AT_high_pc && cts.szB > 0) {
            ip_hi1   = cts.u.val;
            have_hi1 = True;
            if (form != DW_FORM_addr)
               hiIsRelative = True;
         }
         if (attr == DW_AT_ranges && cts.szB > 0) {
            rangeoff   = cts.u.val;
            have_range = True;
         }
         if (attr == DW_AT_comp_dir) {
            if (cts.szB >= 0)
               cc->barf("parse_var_DIE compdir: expecting indirect string");
            HChar *str = ML_(cur_read_strdup)( cts.u.cur,
                                               "parse_var_DIE.compdir" );
            compdir = ML_(addStr)(cc->di, str, -1);
            ML_(dinfo_free) (str);
         }
         if (attr == DW_AT_stmt_list && cts.szB > 0) {
            read_filename_table( parser->fndn_ix_Table, compdir,
                                 cc, cts.u.val, td3 );
         }
      }
      if (have_lo && have_hi1 && hiIsRelative)
         ip_hi1 += ip_lo;

      if (level == 0)
         setup_cu_svma(cc, have_lo, ip_lo, td3);

      
      if (have_lo && have_hi1 && (!have_range)) {
         if (ip_lo < ip_hi1)
            varstack_push( cc, parser, td3, 
                           unitary_range_list(ip_lo, ip_hi1 - 1),
                           level,
                           False, NULL );
         else if (ip_lo == 0 && ip_hi1 == 0)
            varstack_push( cc, parser, td3,
                           empty_range_list(),
                           level,
                           False, NULL );
      } else
      if ((!have_lo) && (!have_hi1) && have_range) {
         varstack_push( cc, parser, td3, 
                        get_range_list( cc, td3,
                                        rangeoff, cc->cu_svma ),
                        level,
                        False, NULL );
      } else
      if ((!have_lo) && (!have_hi1) && (!have_range)) {
         
         varstack_push( cc, parser, td3, 
                        empty_range_list(),
                        level,
                        False, NULL );
      } else
      if (have_lo && (!have_hi1) && have_range && ip_lo == 0) {
         varstack_push( cc, parser, td3, 
                        get_range_list( cc, td3,
                                        rangeoff, cc->cu_svma ),
                        level,
                        False, NULL );
      } else {
         if (0) VG_(printf)("I got hlo %d hhi1 %d hrange %d\n",
                            (Int)have_lo, (Int)have_hi1, (Int)have_range);
         goto_bad_DIE;
      }
   }

   if (dtag == DW_TAG_lexical_block || dtag == DW_TAG_subprogram) {
      Bool   have_lo    = False;
      Bool   have_hi1   = False;
      Bool   have_range = False;
      Bool   hiIsRelative = False;
      Addr   ip_lo      = 0;
      Addr   ip_hi1     = 0;
      Addr   rangeoff   = 0;
      Bool   isFunc     = dtag == DW_TAG_subprogram;
      GExpr* fbGX       = NULL;
      nf_i = 0;
      while (True) {
         DW_AT   attr = (DW_AT)  abbv->nf[nf_i].at_name;
         DW_FORM form = (DW_FORM)abbv->nf[nf_i].at_form;
         nf_i++;
         if (attr == 0 && form == 0) break;
         get_Form_contents( &cts, cc, c_die, False, form );
         if (attr == DW_AT_low_pc && cts.szB > 0) {
            ip_lo   = cts.u.val;
            have_lo = True;
         }
         if (attr == DW_AT_high_pc && cts.szB > 0) {
            ip_hi1   = cts.u.val;
            have_hi1 = True;
            if (form != DW_FORM_addr)
               hiIsRelative = True;
         }
         if (attr == DW_AT_ranges && cts.szB > 0) {
            rangeoff   = cts.u.val;
            have_range = True;
         }
         if (isFunc
             && attr == DW_AT_frame_base
             && cts.szB != 0 ) {
            fbGX = get_GX( cc, False, &cts );
            vg_assert(fbGX);
            VG_(addToXA)(gexprs, &fbGX);
         }
      }
      if (have_lo && have_hi1 && hiIsRelative)
         ip_hi1 += ip_lo;
      
      if (dtag == DW_TAG_subprogram 
          && (!have_lo) && (!have_hi1) && (!have_range)) {
      } else
      if (dtag == DW_TAG_lexical_block
          && (!have_lo) && (!have_hi1) && (!have_range)) {
      } else
      if (have_lo && have_hi1 && (!have_range)) {
         
         if (ip_lo < ip_hi1)
            varstack_push( cc, parser, td3, 
                           unitary_range_list(ip_lo, ip_hi1 - 1),
                           level, isFunc, fbGX );
      } else
      if ((!have_lo) && (!have_hi1) && have_range) {
         varstack_push( cc, parser, td3, 
                        get_range_list( cc, td3,
                                        rangeoff, cc->cu_svma ),
                        level, isFunc, fbGX );
      } else
      if (have_lo && (!have_hi1) && (!have_range)) {
         
      } else
         goto_bad_DIE;
   }

   if (dtag == DW_TAG_variable || dtag == DW_TAG_formal_parameter) {
      const  HChar* name = NULL;
      UWord  typeR       = D3_INVALID_CUOFF;
      Bool   global      = False;
      GExpr* gexpr       = NULL;
      Int    n_attrs     = 0;
      UWord  abs_ori     = (UWord)D3_INVALID_CUOFF;
      Int    lineNo      = 0;
      UInt   fndn_ix     = 0;
      nf_i = 0;
      while (True) {
         DW_AT   attr = (DW_AT)  abbv->nf[nf_i].at_name;
         DW_FORM form = (DW_FORM)abbv->nf[nf_i].at_form;
         nf_i++;
         if (attr == 0 && form == 0) break;
         get_Form_contents( &cts, cc, c_die, False, form );
         n_attrs++;
         if (attr == DW_AT_name && cts.szB < 0) {
            name = ML_(addStrFromCursor)( cc->di, cts.u.cur );
         }
         if (attr == DW_AT_location
             && cts.szB != 0 ) {
            gexpr = get_GX( cc, False, &cts );
            vg_assert(gexpr);
            VG_(addToXA)(gexprs, &gexpr);
         }
         if (attr == DW_AT_type && cts.szB > 0) {
            typeR = cook_die_using_form( cc, cts.u.val, form );
         }
         if (attr == DW_AT_external && cts.szB > 0 && cts.u.val > 0) {
            global = True;
         }
         if (attr == DW_AT_abstract_origin && cts.szB > 0) {
            abs_ori = (UWord)cts.u.val;
         }
         if (attr == DW_AT_declaration && cts.szB > 0 && cts.u.val > 0) {
            
         }
         if (attr == DW_AT_decl_line && cts.szB > 0) {
            lineNo = (Int)cts.u.val;
         }
         if (attr == DW_AT_decl_file && cts.szB > 0) {
            Int ftabIx = (Int)cts.u.val;
            if (ftabIx >= 1
                && ftabIx < VG_(sizeXA)( parser->fndn_ix_Table )) {
               fndn_ix = *(UInt*)VG_(indexXA)( parser->fndn_ix_Table, ftabIx );
            }
            if (0) VG_(printf)("XXX filename fndn_ix = %d %s\n", fndn_ix,
                               ML_(fndn_ix2filename) (cc->di, fndn_ix));
         }
      }
      if (!global && dtag == DW_TAG_variable && level == 1) {
         global = True;
      }

      if (  (gexpr && typeR != D3_INVALID_CUOFF) 
            || (typeR != D3_INVALID_CUOFF)
            || (gexpr && abs_ori != (UWord)D3_INVALID_CUOFF) ) {

         GExpr*   fbGX = NULL;
         Word     i, nRanges;
         const XArray*   xa;
         TempVar* tv;
         vg_assert(parser->sp >= 0);

         if (!global) {
            Bool found = False;
            for (i = parser->sp; i >= 0; i--) {
               if (parser->isFunc[i]) {
                  fbGX = parser->fbGX[i];
                  found = True;
                  break;
               }
            }
            if (!found) {
               if (0 && VG_(clo_verbosity) >= 0) {
                  VG_(message)(Vg_DebugMsg, 
                     "warning: parse_var_DIE: non-global variable "
                     "outside DW_TAG_subprogram\n");
               }
               
            }
         }

         xa = parser->ranges[global ? 0 : parser->sp];
         nRanges = VG_(sizeXA)(xa);
         vg_assert(nRanges >= 0);

         tv = ML_(dinfo_zalloc)( "di.readdwarf3.pvD.1", sizeof(TempVar) );
         tv->name   = name;
         tv->level  = global ? 0 : parser->sp;
         tv->typeR  = typeR;
         tv->gexpr  = gexpr;
         tv->fbGX   = fbGX;
         tv->fndn_ix= fndn_ix;
         tv->fLine  = lineNo;
         tv->dioff  = posn;
         tv->absOri = abs_ori;

         tv->nRanges = nRanges;
         tv->rngOneMin = 0;
         tv->rngOneMax = 0;
         tv->rngMany = NULL;
         if (nRanges == 1) {
            AddrRange* range = VG_(indexXA)(xa, 0);
            tv->rngOneMin = range->aMin;
            tv->rngOneMax = range->aMax;
         }
         else if (nRanges > 1) {
            UWord keyW, valW;
            if (VG_(lookupFM)( rangestree, &keyW, &valW, (UWord)xa )) {
               XArray* old = (XArray*)keyW;
               vg_assert(valW == 0);
               vg_assert(old != xa);
               tv->rngMany = old;
            } else {
               XArray* cloned = VG_(cloneXA)( "di.readdwarf3.pvD.2", xa );
               tv->rngMany = cloned;
               VG_(addToFM)( rangestree, (UWord)cloned, 0 );
            }
         }

         VG_(addToXA)( tempvars, &tv );

         TRACE_D3("  Recording this variable, with %ld PC range(s)\n",
                  VG_(sizeXA)(xa) );
         if (0) {
            static Int ntot=0, ngt=0;
            ntot++;
            if (tv->rngMany) ngt++;
            if (0 == (ntot % 100000))
               VG_(printf)("XXXX %d tot, %d cloned\n", ntot, ngt);
         }

      }

      /* Here are some other weird cases seen in the wild:

            We have a variable with a name and a type, but no
            location.  I guess that's a sign that it has been
            optimised away.  Ignore it.  Here's an example:

            static Int lc_compar(void* n1, void* n2) {
               MC_Chunk* mc1 = *(MC_Chunk**)n1;
               MC_Chunk* mc2 = *(MC_Chunk**)n2;
               return (mc1->data < mc2->data ? -1 : 1);
            }

            Both mc1 and mc2 are like this
            <2><5bc>: Abbrev Number: 21 (DW_TAG_variable)
                DW_AT_name        : mc1
                DW_AT_decl_file   : 1
                DW_AT_decl_line   : 216
                DW_AT_type        : <5d3>

            whereas n1 and n2 do have locations specified.

            ---------------------------------------------

            We see a DW_TAG_formal_parameter with a type, but
            no name and no location.  It's probably part of a function type
            construction, thusly, hence ignore it:
         <1><2b4>: Abbrev Number: 12 (DW_TAG_subroutine_type)
             DW_AT_sibling     : <2c9>
             DW_AT_prototyped  : 1
             DW_AT_type        : <114>
         <2><2be>: Abbrev Number: 13 (DW_TAG_formal_parameter)
             DW_AT_type        : <13e>
         <2><2c3>: Abbrev Number: 13 (DW_TAG_formal_parameter)
             DW_AT_type        : <133>

            ---------------------------------------------

            Is very minimal, like this:
            <4><81d>: Abbrev Number: 44 (DW_TAG_variable)
                DW_AT_abstract_origin: <7ba>
            What that signifies I have no idea.  Ignore.

            ----------------------------------------------

            Is very minimal, like this:
            <200f>: DW_TAG_formal_parameter
                DW_AT_abstract_ori: <1f4c>
                DW_AT_location    : 13440
            What that signifies I have no idea.  Ignore. 
            It might be significant, though: the variable at least
            has a location and so might exist somewhere.
            Maybe we should handle this.

            ---------------------------------------------

            <22407>: DW_TAG_variable
              DW_AT_name        : (indirect string, offset: 0x6579):
                                  vgPlain_trampoline_stuff_start
              DW_AT_decl_file   : 29
              DW_AT_decl_line   : 56
              DW_AT_external    : 1
              DW_AT_declaration : 1

            Nameless and typeless variable that has a location?  Who
            knows.  Not me.
            <2><3d178>: Abbrev Number: 22 (DW_TAG_variable)
                 DW_AT_location    : 9 byte block: 3 c0 c7 13 38 0 0 0 0
                                     (DW_OP_addr: 3813c7c0)

            No, really.  Check it out.  gcc is quite simply borked.
            <3><168cc>: Abbrev Number: 141 (DW_TAG_variable)
            // followed by no attributes, and the next DIE is a sibling,
            // not a child
            */
   }
   return;

  bad_DIE:
   dump_bad_die_and_barf("parse_var_DIE", dtag, posn, level,
                         c_die, saved_die_c_offset,
                         abbv,
                         cc);
   
}

typedef
   struct {
      XArray*  fndn_ix_Table;
      UWord sibling; 
   }
   D3InlParser;

static const HChar* get_inlFnName (Int absori, const CUConst* cc, Bool td3)
{
   Cursor c;
   const g_abbv *abbv;
   ULong  atag, abbv_code;
   UInt   has_children;
   UWord  posn;
   Bool type_flag, alt_flag;
   const HChar *ret = NULL;
   FormContents cts;
   UInt nf_i;

   posn = uncook_die( cc, absori, &type_flag, &alt_flag);
   if (type_flag)
      cc->barf("get_inlFnName: uncooked absori in type debug info");

   if (alt_flag != cc->is_alt_info
       || posn < cc->cu_start_offset
       || posn >= cc->cu_start_offset + cc->unit_length) {
      static Bool reported = False;
      if (!reported && VG_(clo_verbosity) > 1) {
         VG_(message)(Vg_DebugMsg,
                      "Warning: cross-CU LIMITATION: some inlined fn names\n"
                      "might be shown as UnknownInlinedFun\n");
         reported = True;
      }
      TRACE_D3(" <get_inlFnName><%lx>: cross-CU LIMITATION", posn);
      return ML_(addStr)(cc->di, "UnknownInlinedFun", -1);
   }

   init_Cursor (&c, cc->escn_debug_info, posn, cc->barf, 
                "Overrun get_inlFnName absori");

   abbv_code = get_ULEB128( &c );
   abbv      = get_abbv ( cc, abbv_code);
   atag      = abbv->atag;
   TRACE_D3(" <get_inlFnName><%lx>: Abbrev Number: %llu (%s)\n",
            posn, abbv_code, ML_(pp_DW_TAG)( atag ) );

   if (atag == 0)
      cc->barf("get_inlFnName: invalid zero tag on DIE");

   has_children = abbv->has_children;
   if (has_children != DW_children_no && has_children != DW_children_yes)
      cc->barf("get_inlFnName: invalid has_children value");

   if (atag != DW_TAG_subprogram)
      cc->barf("get_inlFnName: absori not a subprogram");

   nf_i = 0;
   while (True) {
      DW_AT   attr = (DW_AT)  abbv->nf[nf_i].at_name;
      DW_FORM form = (DW_FORM)abbv->nf[nf_i].at_form;
      nf_i++;
      if (attr == 0 && form == 0) break;
      get_Form_contents( &cts, cc, &c, False, form );
      if (attr == DW_AT_name) {
         HChar *fnname;
         if (cts.szB >= 0)
            cc->barf("get_inlFnName: expecting indirect string");
         fnname = ML_(cur_read_strdup)( cts.u.cur,
                                        "get_inlFnName.1" );
         ret = ML_(addStr)(cc->di, fnname, -1);
         ML_(dinfo_free) (fnname);
         break; 
      }
      if (attr == DW_AT_specification) {
         UWord cdie;

         if (cts.szB == 0)
            cc->barf("get_inlFnName: AT specification missing");

         cdie = cook_die_using_form(cc, (UWord)cts.u.val, form);

         
         ret = get_inlFnName (cdie, cc, td3);
      }
   }

   if (ret)
      return ret;
   else {
      TRACE_D3("AbsOriFnNameNotFound");
      return ML_(addStr)(cc->di, "AbsOriFnNameNotFound", -1);
   }
}

__attribute__((noinline))
static Bool parse_inl_DIE (
   D3InlParser* parser,
   DW_TAG dtag,
   UWord posn,
   Int level,
   Cursor* c_die,
   const g_abbv *abbv,
   CUConst* cc,
   Bool td3
)
{
   FormContents cts;
   UInt nf_i;

   UWord saved_die_c_offset  = get_position_of_Cursor( c_die );

   if (dtag == DW_TAG_compile_unit || dtag == DW_TAG_partial_unit) {
      Bool have_lo    = False;
      Addr ip_lo    = 0;
      const HChar *compdir = NULL;

      nf_i = 0;
      while (True) {
         DW_AT   attr = (DW_AT)  abbv->nf[nf_i].at_name;
         DW_FORM form = (DW_FORM)abbv->nf[nf_i].at_form;
         nf_i++;
         if (attr == 0 && form == 0) break;
         get_Form_contents( &cts, cc, c_die, False, form );
         if (attr == DW_AT_low_pc && cts.szB > 0) {
            ip_lo   = cts.u.val;
            have_lo = True;
         }
         if (attr == DW_AT_comp_dir) {
            if (cts.szB >= 0)
               cc->barf("parse_inl_DIE compdir: expecting indirect string");
            HChar *str = ML_(cur_read_strdup)( cts.u.cur,
                                               "parse_inl_DIE.compdir" );
            compdir = ML_(addStr)(cc->di, str, -1);
            ML_(dinfo_free) (str);
         }
         if (attr == DW_AT_stmt_list && cts.szB > 0) {
            read_filename_table( parser->fndn_ix_Table, compdir,
                                 cc, cts.u.val, td3 );
         }
         if (attr == DW_AT_sibling && cts.szB > 0) {
            parser->sibling = cts.u.val;
         }
      }
      if (level == 0)
         setup_cu_svma (cc, have_lo, ip_lo, td3);
   }

   if (dtag == DW_TAG_inlined_subroutine) {
      Bool   have_lo    = False;
      Bool   have_hi1   = False;
      Bool   have_range = False;
      Bool   hiIsRelative = False;
      Addr   ip_lo      = 0;
      Addr   ip_hi1     = 0;
      Addr   rangeoff   = 0;
      UInt   caller_fndn_ix = 0;
      Int caller_lineno = 0;
      Int inlinedfn_abstract_origin = 0;

      nf_i = 0;
      while (True) {
         DW_AT   attr = (DW_AT)  abbv->nf[nf_i].at_name;
         DW_FORM form = (DW_FORM)abbv->nf[nf_i].at_form;
         nf_i++;
         if (attr == 0 && form == 0) break;
         get_Form_contents( &cts, cc, c_die, False, form );
         if (attr == DW_AT_call_file && cts.szB > 0) {
            Int ftabIx = (Int)cts.u.val;
            if (ftabIx >= 1
                && ftabIx < VG_(sizeXA)( parser->fndn_ix_Table )) {
               caller_fndn_ix = *(UInt*)
                          VG_(indexXA)( parser->fndn_ix_Table, ftabIx );
            }
            if (0) VG_(printf)("XXX caller_fndn_ix = %d %s\n", caller_fndn_ix,
                               ML_(fndn_ix2filename) (cc->di, caller_fndn_ix));
         }  
         if (attr == DW_AT_call_line && cts.szB > 0) {
            caller_lineno = cts.u.val;
         }  

         if (attr == DW_AT_abstract_origin  && cts.szB > 0) {
            inlinedfn_abstract_origin
               = cook_die_using_form (cc, (UWord)cts.u.val, form);
         }

         if (attr == DW_AT_low_pc && cts.szB > 0) {
            ip_lo   = cts.u.val;
            have_lo = True;
         }
         if (attr == DW_AT_high_pc && cts.szB > 0) {
            ip_hi1   = cts.u.val;
            have_hi1 = True;
            if (form != DW_FORM_addr)
               hiIsRelative = True;
         }
         if (attr == DW_AT_ranges && cts.szB > 0) {
            rangeoff   = cts.u.val;
            have_range = True;
         }
         if (attr == DW_AT_sibling && cts.szB > 0) {
            parser->sibling = cts.u.val;
         }
      }
      if (have_lo && have_hi1 && hiIsRelative)
         ip_hi1 += ip_lo;
      
      if (dtag == DW_TAG_inlined_subroutine
          && (!have_lo) && (!have_hi1) && (!have_range)) {
         goto_bad_DIE;
      } else
      if (have_lo && have_hi1 && (!have_range)) {
         
         if (ip_lo < ip_hi1) {
            
            ip_lo += cc->di->text_debug_bias;
            ip_hi1 += cc->di->text_debug_bias;
            ML_(addInlInfo) (cc->di,
                             ip_lo, ip_hi1, 
                             get_inlFnName (inlinedfn_abstract_origin, cc, td3),
                             caller_fndn_ix,
                             caller_lineno, level);
         }
      } else if (have_range) {
         
         XArray *ranges;
         Word j;
         const HChar *inlfnname =
            get_inlFnName (inlinedfn_abstract_origin, cc, td3);

         ranges = get_range_list( cc, td3,
                                  rangeoff, cc->cu_svma );
         for (j = 0; j < VG_(sizeXA)( ranges ); j++) {
            AddrRange* range = (AddrRange*) VG_(indexXA)( ranges, j );
            ML_(addInlInfo) (cc->di,
                             range->aMin   + cc->di->text_debug_bias,
                             range->aMax+1 + cc->di->text_debug_bias,
                             
                             
                             
                             inlfnname,
                             caller_fndn_ix,
                             caller_lineno, level);
         }
         VG_(deleteXA)( ranges );
      } else
         goto_bad_DIE;
   }

   
   
   return dtag == DW_TAG_lexical_block || dtag == DW_TAG_subprogram
      || dtag == DW_TAG_inlined_subroutine
      || dtag == DW_TAG_compile_unit || dtag == DW_TAG_partial_unit;

  bad_DIE:
   dump_bad_die_and_barf("parse_inl_DIE", dtag, posn, level,
                         c_die, saved_die_c_offset,
                         abbv,
                         cc);
   
}



typedef
   struct {
      UChar language;
      
      Int   sp; 
      Int   stack_size;
      TyEnt *qparentE; 
      Int   *qlevel;
   }
   D3TypeParser;

static void
type_parser_init ( D3TypeParser *parser )
{
   parser->sp = -1;
   parser->language = '?';
   parser->stack_size = 0;
   parser->qparentE = NULL;
   parser->qlevel   = NULL;
}

static void
type_parser_release ( D3TypeParser *parser )
{
   ML_(dinfo_free)( parser->qparentE );
   ML_(dinfo_free)( parser->qlevel );
}

static void typestack_show ( const D3TypeParser* parser, const HChar* str )
{
   Word i;
   VG_(printf)("  typestack (%s) {\n", str);
   for (i = 0; i <= parser->sp; i++) {
      VG_(printf)("    [%ld] (level %d): ", i, parser->qlevel[i]);
      ML_(pp_TyEnt)( &parser->qparentE[i] );
      VG_(printf)("\n");
   }
   VG_(printf)("  }\n");
}

static 
void typestack_preen ( D3TypeParser* parser, Bool td3, Int level )
{
   Bool changed = False;
   vg_assert(parser->sp < parser->stack_size);
   while (True) {
      vg_assert(parser->sp >= -1);
      if (parser->sp == -1) break;
      if (parser->qlevel[parser->sp] <= level) break;
      if (0) 
         TRACE_D3("BBBBAAAA typestack_pop [newsp=%d]\n", parser->sp-1);
      vg_assert(ML_(TyEnt__is_type)(&parser->qparentE[parser->sp]));
      parser->sp--;
      changed = True;
   }
   if (changed && td3)
      typestack_show( parser, "after preen" );
}

static Bool typestack_is_empty ( const D3TypeParser* parser )
{
   vg_assert(parser->sp >= -1 && parser->sp < parser->stack_size);
   return parser->sp == -1;
}

static void typestack_push ( const CUConst* cc,
                             D3TypeParser* parser,
                             Bool td3,
                             const TyEnt* parentE, Int level )
{
   if (0)
   TRACE_D3("BBBBAAAA typestack_push[newsp=%d]: %d  %05lx\n",
            parser->sp+1, level, parentE->cuOff);

   typestack_preen(parser, False, level-1);

   vg_assert(parser->sp >= -1);
   vg_assert(parser->sp < parser->stack_size);
   if (parser->sp == parser->stack_size - 1) {
      parser->stack_size += 16;
      parser->qparentE =
         ML_(dinfo_realloc)("di.readdwarf3.typush.1", parser->qparentE,
                            parser->stack_size * sizeof parser->qparentE[0]);
      parser->qlevel =
         ML_(dinfo_realloc)("di.readdwarf3.typush.2", parser->qlevel,
                            parser->stack_size * sizeof parser->qlevel[0]);
   }
   if (parser->sp >= 0)
      vg_assert(parser->qlevel[parser->sp] < level);
   parser->sp++;
   vg_assert(parentE);
   vg_assert(ML_(TyEnt__is_type)(parentE));
   vg_assert(parentE->cuOff != D3_INVALID_CUOFF);
   parser->qparentE[parser->sp] = *parentE;
   parser->qlevel[parser->sp]  = level;
   if (TD3)
      typestack_show( parser, "after push" );
}

static Bool subrange_type_denotes_array_bounds ( const D3TypeParser* parser,
                                                 DW_TAG dtag ) {
   vg_assert(dtag == DW_TAG_subrange_type);
   if (parser->language != 'A')
      
      return True;
   else
      
      return (! typestack_is_empty(parser)
              && parser->qparentE[parser->sp].tag == Te_TyArray);
}

__attribute__((noinline))
static void parse_type_DIE ( XArray*  tyents,
                             D3TypeParser* parser,
                             DW_TAG dtag,
                             UWord posn,
                             Int level,
                             Cursor* c_die,
                             const g_abbv *abbv,
                             const CUConst* cc,
                             Bool td3 )
{
   FormContents cts;
   UInt nf_i;
   TyEnt typeE;
   TyEnt atomE;
   TyEnt fieldE;
   TyEnt boundE;

   UWord saved_die_c_offset  = get_position_of_Cursor( c_die );

   VG_(memset)( &typeE,  0xAA, sizeof(typeE) );
   VG_(memset)( &atomE,  0xAA, sizeof(atomE) );
   VG_(memset)( &fieldE, 0xAA, sizeof(fieldE) );
   VG_(memset)( &boundE, 0xAA, sizeof(boundE) );

   typestack_preen( parser, td3, level-1 );

   if (dtag == DW_TAG_compile_unit
       || dtag == DW_TAG_type_unit
       || dtag == DW_TAG_partial_unit) {
      nf_i = 0;
      while (True) {
         DW_AT   attr = (DW_AT)  abbv->nf[nf_i].at_name;
         DW_FORM form = (DW_FORM)abbv->nf[nf_i].at_form;
         nf_i++;
         if (attr == 0 && form == 0) break;
         get_Form_contents( &cts, cc, c_die, False, form );
         if (attr != DW_AT_language)
            continue;
         if (cts.szB <= 0)
           goto_bad_DIE;
         switch (cts.u.val) {
            case DW_LANG_C89: case DW_LANG_C:
            case DW_LANG_C_plus_plus: case DW_LANG_ObjC:
            case DW_LANG_ObjC_plus_plus: case DW_LANG_UPC:
            case DW_LANG_Upc: case DW_LANG_C99: case DW_LANG_C11:
            case DW_LANG_C_plus_plus_11: case DW_LANG_C_plus_plus_14:
               parser->language = 'C'; break;
            case DW_LANG_Fortran77: case DW_LANG_Fortran90:
            case DW_LANG_Fortran95: case DW_LANG_Fortran03:
            case DW_LANG_Fortran08:
               parser->language = 'F'; break;
            case DW_LANG_Ada83: case DW_LANG_Ada95: 
               parser->language = 'A'; break;
            case DW_LANG_Cobol74:
            case DW_LANG_Cobol85: case DW_LANG_Pascal83:
            case DW_LANG_Modula2: case DW_LANG_Java:
            case DW_LANG_PLI:
            case DW_LANG_D: case DW_LANG_Python: case DW_LANG_Go:
            case DW_LANG_Mips_Assembler:
               parser->language = '?'; break;
            default:
               goto_bad_DIE;
         }
      }
   }

   if (dtag == DW_TAG_base_type) {
      
      VG_(memset)(&typeE, 0, sizeof(typeE));
      typeE.cuOff = D3_INVALID_CUOFF;
      typeE.tag   = Te_TyBase;
      nf_i = 0;
      while (True) {
         DW_AT   attr = (DW_AT)  abbv->nf[nf_i].at_name;
         DW_FORM form = (DW_FORM)abbv->nf[nf_i].at_form;
         nf_i++;
         if (attr == 0 && form == 0) break;
         get_Form_contents( &cts, cc, c_die, False, form );
         if (attr == DW_AT_name && cts.szB < 0) {
            typeE.Te.TyBase.name
               = ML_(cur_read_strdup)( cts.u.cur,
                                       "di.readdwarf3.ptD.base_type.1" );
         }
         if (attr == DW_AT_byte_size && cts.szB > 0) {
            typeE.Te.TyBase.szB = cts.u.val;
         }
         if (attr == DW_AT_encoding && cts.szB > 0) {
            switch (cts.u.val) {
               case DW_ATE_unsigned: case DW_ATE_unsigned_char:
               case DW_ATE_UTF: 
               case DW_ATE_boolean:
               case DW_ATE_unsigned_fixed:
                  typeE.Te.TyBase.enc = 'U'; break;
               case DW_ATE_signed: case DW_ATE_signed_char:
               case DW_ATE_signed_fixed:
                  typeE.Te.TyBase.enc = 'S'; break;
               case DW_ATE_float:
                  typeE.Te.TyBase.enc = 'F'; break;
               case DW_ATE_complex_float:
                  typeE.Te.TyBase.enc = 'C'; break;
               default:
                  goto_bad_DIE;
            }
         }
      }

      if (!typeE.Te.TyBase.name)
         typeE.Te.TyBase.name 
            = ML_(dinfo_strdup)( "di.readdwarf3.ptD.base_type.2",
                                 "<anon_base_type>" );

      
      if (
          typeE.Te.TyBase.name == NULL
          || typeE.Te.TyBase.szB < 0 || typeE.Te.TyBase.szB > 32
          
          || (typeE.Te.TyBase.enc != 'U'
              && typeE.Te.TyBase.enc != 'S' 
              && typeE.Te.TyBase.enc != 'F'
              && typeE.Te.TyBase.enc != 'C'))
         goto_bad_DIE;
      if (typeE.Te.TyBase.szB == 0
          && 0 == VG_(strcmp)("void", typeE.Te.TyBase.name)) {
         ML_(TyEnt__make_EMPTY)(&typeE);
         typeE.tag = Te_TyVoid;
         typeE.Te.TyVoid.isFake = False; 
      }

      goto acquire_Type;
   }

   if (dtag == DW_TAG_pointer_type || dtag == DW_TAG_reference_type
       || dtag == DW_TAG_ptr_to_member_type
       || dtag == DW_TAG_rvalue_reference_type) {
      VG_(memset)(&typeE, 0, sizeof(typeE));
      typeE.cuOff = D3_INVALID_CUOFF;
      switch (dtag) {
      case DW_TAG_pointer_type:
         typeE.tag = Te_TyPtr;
         break;
      case DW_TAG_reference_type:
         typeE.tag = Te_TyRef;
         break;
      case DW_TAG_ptr_to_member_type:
         typeE.tag = Te_TyPtrMbr;
         break;
      case DW_TAG_rvalue_reference_type:
         typeE.tag = Te_TyRvalRef;
         break;
      default:
         vg_assert(False);
      }
      
      typeE.Te.TyPorR.typeR = D3_FAKEVOID_CUOFF;
      typeE.Te.TyPorR.szB = sizeof(UWord);
      nf_i = 0;
      while (True) {
         DW_AT   attr = (DW_AT)  abbv->nf[nf_i].at_name;
         DW_FORM form = (DW_FORM)abbv->nf[nf_i].at_form;
         nf_i++;
         if (attr == 0 && form == 0) break;
         get_Form_contents( &cts, cc, c_die, False, form );
         if (attr == DW_AT_byte_size && cts.szB > 0) {
            typeE.Te.TyPorR.szB = cts.u.val;
         }
         if (attr == DW_AT_type && cts.szB > 0) {
            typeE.Te.TyPorR.typeR
               = cook_die_using_form( cc, (UWord)cts.u.val, form );
         }
      }
      
      if (typeE.Te.TyPorR.szB != sizeof(UWord))
         goto_bad_DIE;
      else
         goto acquire_Type;
   }

   if (dtag == DW_TAG_enumeration_type) {
      
      VG_(memset)(&typeE, 0, sizeof(typeE));
      typeE.cuOff = posn;
      typeE.tag   = Te_TyEnum;
      Bool is_decl = False;
      typeE.Te.TyEnum.atomRs
         = VG_(newXA)( ML_(dinfo_zalloc), "di.readdwarf3.ptD.enum_type.1", 
                       ML_(dinfo_free),
                       sizeof(UWord) );
      nf_i=0;
      while (True) {
         DW_AT   attr = (DW_AT)  abbv->nf[nf_i].at_name;
         DW_FORM form = (DW_FORM)abbv->nf[nf_i].at_form;
         nf_i++;
         if (attr == 0 && form == 0) break;
         get_Form_contents( &cts, cc, c_die, False, form );
         if (attr == DW_AT_name && cts.szB < 0) {
            typeE.Te.TyEnum.name
               = ML_(cur_read_strdup)( cts.u.cur,
                                       "di.readdwarf3.pTD.enum_type.2" );
         }
         if (attr == DW_AT_byte_size && cts.szB > 0) {
            typeE.Te.TyEnum.szB = cts.u.val;
         }
         if (attr == DW_AT_declaration) {
            is_decl = True;
         }
      }

      if (!typeE.Te.TyEnum.name)
         typeE.Te.TyEnum.name 
            = ML_(dinfo_strdup)( "di.readdwarf3.pTD.enum_type.3",
                                 "<anon_enum_type>" );

      
      if (typeE.Te.TyEnum.szB == 0 
          
          && (parser->language != 'A' && !is_decl)) {
         goto_bad_DIE;
      }

      
      typestack_push( cc, parser, td3, &typeE, level );
      goto acquire_Type;
   }

   if (dtag == DW_TAG_enumerator) {
      VG_(memset)( &atomE, 0, sizeof(atomE) );
      atomE.cuOff = posn;
      atomE.tag   = Te_Atom;
      nf_i = 0;
      while (True) {
         DW_AT   attr = (DW_AT)  abbv->nf[nf_i].at_name;
         DW_FORM form = (DW_FORM)abbv->nf[nf_i].at_form;
         nf_i++;
         if (attr == 0 && form == 0) break;
         get_Form_contents( &cts, cc, c_die, False, form );
         if (attr == DW_AT_name && cts.szB < 0) {
            atomE.Te.Atom.name 
              = ML_(cur_read_strdup)( cts.u.cur,
                                      "di.readdwarf3.pTD.enumerator.1" );
         }
         if (attr == DW_AT_const_value && cts.szB > 0) {
            atomE.Te.Atom.value      = cts.u.val;
            atomE.Te.Atom.valueKnown = True;
         }
      }
      
      if (atomE.Te.Atom.name == NULL)
         goto_bad_DIE;
      
      if (typestack_is_empty(parser)) goto_bad_DIE;
      vg_assert(ML_(TyEnt__is_type)(&parser->qparentE[parser->sp]));
      vg_assert(parser->qparentE[parser->sp].cuOff != D3_INVALID_CUOFF);
      if (level != parser->qlevel[parser->sp]+1) goto_bad_DIE;
      if (parser->qparentE[parser->sp].tag != Te_TyEnum) goto_bad_DIE;
      
      vg_assert(parser->qparentE[parser->sp].Te.TyEnum.atomRs);
      VG_(addToXA)( parser->qparentE[parser->sp].Te.TyEnum.atomRs,
                    &atomE );
      
      goto acquire_Atom;
   }

   if (dtag == DW_TAG_structure_type || dtag == DW_TAG_class_type
       || dtag == DW_TAG_union_type) {
      Bool have_szB = False;
      Bool is_decl  = False;
      Bool is_spec  = False;
      
      VG_(memset)(&typeE, 0, sizeof(typeE));
      typeE.cuOff = posn;
      typeE.tag   = Te_TyStOrUn;
      typeE.Te.TyStOrUn.name = NULL;
      typeE.Te.TyStOrUn.typeR = D3_INVALID_CUOFF;
      typeE.Te.TyStOrUn.fieldRs
         = VG_(newXA)( ML_(dinfo_zalloc), "di.readdwarf3.pTD.struct_type.1", 
                       ML_(dinfo_free),
                       sizeof(UWord) );
      typeE.Te.TyStOrUn.complete = True;
      typeE.Te.TyStOrUn.isStruct = dtag == DW_TAG_structure_type 
                                   || dtag == DW_TAG_class_type;
      nf_i = 0;
      while (True) {
         DW_AT   attr = (DW_AT)  abbv->nf[nf_i].at_name;
         DW_FORM form = (DW_FORM)abbv->nf[nf_i].at_form;
         nf_i++;
         if (attr == 0 && form == 0) break;
         get_Form_contents( &cts, cc, c_die, False, form );
         if (attr == DW_AT_name && cts.szB < 0) {
            typeE.Te.TyStOrUn.name
               = ML_(cur_read_strdup)( cts.u.cur,
                                       "di.readdwarf3.ptD.struct_type.2" );
         }
         if (attr == DW_AT_byte_size && cts.szB >= 0) {
            typeE.Te.TyStOrUn.szB = cts.u.val;
            have_szB = True;
         }
         if (attr == DW_AT_declaration && cts.szB > 0 && cts.u.val > 0) {
            is_decl = True;
         }
         if (attr == DW_AT_specification && cts.szB > 0 && cts.u.val > 0) {
            is_spec = True;
         }
         if (attr == DW_AT_signature && form == DW_FORM_ref_sig8
             && cts.szB > 0) {
            have_szB = True;
            typeE.Te.TyStOrUn.szB = 8;
            typeE.Te.TyStOrUn.typeR
               = cook_die_using_form( cc, (UWord)cts.u.val, form );
         }
      }
      
      if (is_decl && (!is_spec)) {
         if (typeE.Te.TyStOrUn.name == NULL)
            typeE.Te.TyStOrUn.name
               = ML_(dinfo_strdup)( "di.readdwarf3.ptD.struct_type.3",
                                    "<anon_struct_type>" );
         typeE.Te.TyStOrUn.complete = False;
         
         typestack_push( cc, parser, td3, &typeE, level );
         
         goto acquire_Type;
      }
      if ((!is_decl) ) {
         
         
         if (!have_szB) { 
            typeE.Te.TyStOrUn.szB = 1;
         }
         
         typestack_push( cc, parser, td3, &typeE, level );
         goto acquire_Type;
      }
      else {
         
         goto_bad_DIE;
      }
   }

   if (dtag == DW_TAG_member) {
      Bool parent_is_struct;
      VG_(memset)( &fieldE, 0, sizeof(fieldE) );
      fieldE.cuOff = posn;
      fieldE.tag   = Te_Field;
      fieldE.Te.Field.typeR = D3_INVALID_CUOFF;
      nf_i = 0;
      while (True) {
         DW_AT   attr = (DW_AT)  abbv->nf[nf_i].at_name;
         DW_FORM form = (DW_FORM)abbv->nf[nf_i].at_form;
         nf_i++;
         if (attr == 0 && form == 0) break;
         get_Form_contents( &cts, cc, c_die, False, form );
         if (attr == DW_AT_name && cts.szB < 0) {
            fieldE.Te.Field.name
               = ML_(cur_read_strdup)( cts.u.cur,
                                       "di.readdwarf3.ptD.member.1" );
         }
         if (attr == DW_AT_type && cts.szB > 0) {
            fieldE.Te.Field.typeR
               = cook_die_using_form( cc, (UWord)cts.u.val, form );
         }
         if (attr == DW_AT_data_member_location && cts.szB > 0) {
            fieldE.Te.Field.nLoc = -1;
            fieldE.Te.Field.pos.offset = cts.u.val;
         }
         if (attr == DW_AT_data_member_location && cts.szB <= 0) {
            fieldE.Te.Field.nLoc = (UWord)(-cts.szB);
            fieldE.Te.Field.pos.loc
               = ML_(cur_read_memdup)( cts.u.cur, 
                                       (SizeT)fieldE.Te.Field.nLoc,
                                       "di.readdwarf3.ptD.member.2" );
         }
      }
      
      if (typestack_is_empty(parser)) goto_bad_DIE;
      vg_assert(ML_(TyEnt__is_type)(&parser->qparentE[parser->sp]));
      vg_assert(parser->qparentE[parser->sp].cuOff != D3_INVALID_CUOFF);
      if (level != parser->qlevel[parser->sp]+1) goto_bad_DIE;
      if (parser->qparentE[parser->sp].tag != Te_TyStOrUn) goto_bad_DIE;
      parent_is_struct
         = parser->qparentE[parser->sp].Te.TyStOrUn.isStruct;
      if (!fieldE.Te.Field.name)
         fieldE.Te.Field.name
            = ML_(dinfo_strdup)( "di.readdwarf3.ptD.member.3",
                                 "<anon_field>" );
      if (fieldE.Te.Field.typeR == D3_INVALID_CUOFF)
         goto_bad_DIE;
      if (fieldE.Te.Field.nLoc) {
         if (!parent_is_struct) {
            if (fieldE.Te.Field.nLoc > 0)
               ML_(dinfo_free)(fieldE.Te.Field.pos.loc);
            fieldE.Te.Field.pos.loc = NULL;
            fieldE.Te.Field.nLoc = 0;
         }
         
         fieldE.Te.Field.isStruct = parent_is_struct;
         vg_assert(parser->qparentE[parser->sp].Te.TyStOrUn.fieldRs);
         VG_(addToXA)( parser->qparentE[parser->sp].Te.TyStOrUn.fieldRs,
                       &posn );
         
         goto acquire_Field;
      } else {
         ML_(TyEnt__make_EMPTY)(&fieldE);
      }
   }

   if (dtag == DW_TAG_array_type) {
      VG_(memset)(&typeE, 0, sizeof(typeE));
      typeE.cuOff = posn;
      typeE.tag   = Te_TyArray;
      typeE.Te.TyArray.typeR = D3_INVALID_CUOFF;
      typeE.Te.TyArray.boundRs
         = VG_(newXA)( ML_(dinfo_zalloc), "di.readdwarf3.ptD.array_type.1",
                       ML_(dinfo_free),
                       sizeof(UWord) );
      nf_i = 0;
      while (True) {
         DW_AT   attr = (DW_AT)  abbv->nf[nf_i].at_name;
         DW_FORM form = (DW_FORM)abbv->nf[nf_i].at_form;
         nf_i++;
         if (attr == 0 && form == 0) break;
         get_Form_contents( &cts, cc, c_die, False, form );
         if (attr == DW_AT_type && cts.szB > 0) {
            typeE.Te.TyArray.typeR
               = cook_die_using_form( cc, (UWord)cts.u.val, form );
         }
      }
      if (typeE.Te.TyArray.typeR == D3_INVALID_CUOFF)
         goto_bad_DIE;
      
      typestack_push( cc, parser, td3, &typeE, level );
      goto acquire_Type;
   }

   
   if (dtag == DW_TAG_subrange_type 
       && subrange_type_denotes_array_bounds(parser, dtag)) {
      Bool have_lower = False;
      Bool have_upper = False;
      Bool have_count = False;
      Long lower = 0;
      Long upper = 0;

      switch (parser->language) {
         case 'C': have_lower = True;  lower = 0; break;
         case 'F': have_lower = True;  lower = 1; break;
         case '?': have_lower = False; break;
         case 'A': have_lower = False; break;
         default:  vg_assert(0); 
      }

      VG_(memset)( &boundE, 0, sizeof(boundE) );
      boundE.cuOff = D3_INVALID_CUOFF;
      boundE.tag   = Te_Bound;
      nf_i = 0;
      while (True) {
         DW_AT   attr = (DW_AT)  abbv->nf[nf_i].at_name;
         DW_FORM form = (DW_FORM)abbv->nf[nf_i].at_form;
         nf_i++;
         if (attr == 0 && form == 0) break;
         get_Form_contents( &cts, cc, c_die, False, form );
         if (attr == DW_AT_lower_bound && cts.szB > 0) {
            lower      = (Long)cts.u.val;
            have_lower = True;
         }
         if (attr == DW_AT_upper_bound && cts.szB > 0) {
            upper      = (Long)cts.u.val;
            have_upper = True;
         }
         if (attr == DW_AT_count && cts.szB > 0) {
            
            have_count = True;
         }
      }
      
      if (typestack_is_empty(parser)) goto_bad_DIE;
      vg_assert(ML_(TyEnt__is_type)(&parser->qparentE[parser->sp]));
      vg_assert(parser->qparentE[parser->sp].cuOff != D3_INVALID_CUOFF);
      if (level != parser->qlevel[parser->sp]+1) goto_bad_DIE;
      if (parser->qparentE[parser->sp].tag != Te_TyArray) goto_bad_DIE;

      
      if (have_lower && have_upper && (!have_count)) {
         boundE.Te.Bound.knownL = True;
         boundE.Te.Bound.knownU = True;
         boundE.Te.Bound.boundL = lower;
         boundE.Te.Bound.boundU = upper;
      }
      else if (have_lower && (!have_upper) && (!have_count)) {
         boundE.Te.Bound.knownL = True;
         boundE.Te.Bound.knownU = False;
         boundE.Te.Bound.boundL = lower;
         boundE.Te.Bound.boundU = 0;
      }
      else if ((!have_lower) && have_upper && (!have_count)) {
         boundE.Te.Bound.knownL = False;
         boundE.Te.Bound.knownU = True;
         boundE.Te.Bound.boundL = 0;
         boundE.Te.Bound.boundU = upper;
      }
      else if ((!have_lower) && (!have_upper) && (!have_count)) {
         boundE.Te.Bound.knownL = False;
         boundE.Te.Bound.knownU = False;
         boundE.Te.Bound.boundL = 0;
         boundE.Te.Bound.boundU = 0;
      } else {
         
         goto_bad_DIE;
      }

      
      boundE.cuOff = posn;
      vg_assert(parser->qparentE[parser->sp].Te.TyArray.boundRs);
      VG_(addToXA)( parser->qparentE[parser->sp].Te.TyArray.boundRs,
                    &boundE.cuOff );
      
      goto acquire_Bound;
   }

   
   if (dtag == DW_TAG_typedef 
       || (dtag == DW_TAG_subrange_type 
           && !subrange_type_denotes_array_bounds(parser, dtag))) {
      
      vg_assert (dtag == DW_TAG_typedef || parser->language == 'A');
      
      VG_(memset)(&typeE, 0, sizeof(typeE));
      typeE.cuOff = D3_INVALID_CUOFF;
      typeE.tag   = Te_TyTyDef;
      typeE.Te.TyTyDef.name = NULL;
      typeE.Te.TyTyDef.typeR = D3_INVALID_CUOFF;
      nf_i = 0;
      while (True) {
         DW_AT   attr = (DW_AT)  abbv->nf[nf_i].at_name;
         DW_FORM form = (DW_FORM)abbv->nf[nf_i].at_form;
         nf_i++;
         if (attr == 0 && form == 0) break;
         get_Form_contents( &cts, cc, c_die, False, form );
         if (attr == DW_AT_name && cts.szB < 0) {
            typeE.Te.TyTyDef.name
               = ML_(cur_read_strdup)( cts.u.cur,
                                       "di.readdwarf3.ptD.typedef.1" );
         }
         if (attr == DW_AT_type && cts.szB > 0) {
            typeE.Te.TyTyDef.typeR
               = cook_die_using_form( cc, (UWord)cts.u.val, form );
         }
      }
      goto acquire_Type;
   }

   if (dtag == DW_TAG_subroutine_type) {
      VG_(memset)(&typeE, 0, sizeof(typeE));
      typeE.cuOff = D3_INVALID_CUOFF;
      typeE.tag   = Te_TyFn;
      goto acquire_Type;
   }

   if (dtag == DW_TAG_volatile_type || dtag == DW_TAG_const_type
       || dtag == DW_TAG_restrict_type) {
      Int have_ty = 0;
      VG_(memset)(&typeE, 0, sizeof(typeE));
      typeE.cuOff = D3_INVALID_CUOFF;
      typeE.tag   = Te_TyQual;
      typeE.Te.TyQual.qual
         = (dtag == DW_TAG_volatile_type ? 'V'
            : (dtag == DW_TAG_const_type ? 'C' : 'R'));
      
      typeE.Te.TyQual.typeR = D3_FAKEVOID_CUOFF;
      nf_i = 0;
      while (True) {
         DW_AT   attr = (DW_AT)  abbv->nf[nf_i].at_name;
         DW_FORM form = (DW_FORM)abbv->nf[nf_i].at_form;
         nf_i++;
         if (attr == 0 && form == 0) break;
         get_Form_contents( &cts, cc, c_die, False, form );
         if (attr == DW_AT_type && cts.szB > 0) {
            typeE.Te.TyQual.typeR
               = cook_die_using_form( cc, (UWord)cts.u.val, form );
            have_ty++;
         }
      }
      if (have_ty == 1 || have_ty == 0)
         goto acquire_Type;
      else
         goto_bad_DIE;
   }

   if (dtag == DW_TAG_unspecified_type) {
      VG_(memset)(&typeE, 0, sizeof(typeE));
      typeE.cuOff           = D3_INVALID_CUOFF;
      typeE.tag             = Te_TyQual;
      typeE.Te.TyQual.typeR = D3_FAKEVOID_CUOFF;
      goto acquire_Type;
   }

   
   return;
   

  acquire_Type:
   if (0) VG_(printf)("YYYY Acquire Type\n");
   vg_assert(ML_(TyEnt__is_type)( &typeE ));
   vg_assert(typeE.cuOff == D3_INVALID_CUOFF || typeE.cuOff == posn);
   typeE.cuOff = posn;
   VG_(addToXA)( tyents, &typeE );
   return;
   

  acquire_Atom:
   if (0) VG_(printf)("YYYY Acquire Atom\n");
   vg_assert(atomE.tag == Te_Atom);
   vg_assert(atomE.cuOff == D3_INVALID_CUOFF || atomE.cuOff == posn);
   atomE.cuOff = posn;
   VG_(addToXA)( tyents, &atomE );
   return;
   

  acquire_Field:
   
   if (0) VG_(printf)("YYYY Acquire Field\n");
   vg_assert(fieldE.tag == Te_Field);
   vg_assert(fieldE.Te.Field.nLoc <= 0 || fieldE.Te.Field.pos.loc != NULL);
   vg_assert(fieldE.Te.Field.nLoc != 0 || fieldE.Te.Field.pos.loc == NULL);
   if (fieldE.Te.Field.isStruct) {
      vg_assert(fieldE.Te.Field.nLoc != 0);
   } else {
      vg_assert(fieldE.Te.Field.nLoc == 0);
   }
   vg_assert(fieldE.cuOff == D3_INVALID_CUOFF || fieldE.cuOff == posn);
   fieldE.cuOff = posn;
   VG_(addToXA)( tyents, &fieldE );
   return;
   

  acquire_Bound:
   if (0) VG_(printf)("YYYY Acquire Bound\n");
   vg_assert(boundE.tag == Te_Bound);
   vg_assert(boundE.cuOff == D3_INVALID_CUOFF || boundE.cuOff == posn);
   boundE.cuOff = posn;
   VG_(addToXA)( tyents, &boundE );
   return;
   

  bad_DIE:
   dump_bad_die_and_barf("parse_type_DIE", dtag, posn, level,
                         c_die, saved_die_c_offset,
                         abbv,
                         cc);
   
}



static UWord chase_cuOff ( Bool* changed,
                           const XArray*  ents,
                           TyEntIndexCache* ents_cache,
                           UWord cuOff )
{
   TyEnt* ent;
   ent = ML_(TyEnts__index_by_cuOff)( ents, ents_cache, cuOff );

   if (!ent) {
      VG_(printf)("chase_cuOff: no entry for 0x%05lx\n", cuOff);
      *changed = False;
      return cuOff;
   }

   vg_assert(ent->tag != Te_EMPTY);
   if (ent->tag != Te_INDIR) {
      *changed = False;
      return cuOff;
   } else {
      vg_assert(ent->Te.INDIR.indR < cuOff);
      *changed = True;
      return ent->Te.INDIR.indR;
   }
}

static
void chase_cuOffs_in_XArray ( Bool* changed,
                              const XArray*  ents,
                              TyEntIndexCache* ents_cache,
                              XArray*  cuOffs )
{
   Bool b2 = False;
   Word i, n = VG_(sizeXA)( cuOffs );
   for (i = 0; i < n; i++) {
      Bool   b = False;
      UWord* p = VG_(indexXA)( cuOffs, i );
      *p = chase_cuOff( &b, ents, ents_cache, *p );
      if (b)
         b2 = True;
   }
   *changed = b2;
}

static Bool TyEnt__subst_R_fields ( const XArray*  ents,
                                    TyEntIndexCache* ents_cache,
                                    TyEnt* te )
{
   Bool b, changed = False;
   switch (te->tag) {
      case Te_EMPTY:
         break;
      case Te_INDIR:
         te->Te.INDIR.indR
            = chase_cuOff( &b, ents, ents_cache, te->Te.INDIR.indR );
         if (b) changed = True;
         break;
      case Te_UNKNOWN:
         break;
      case Te_Atom:
         break;
      case Te_Field:
         te->Te.Field.typeR
            = chase_cuOff( &b, ents, ents_cache, te->Te.Field.typeR );
         if (b) changed = True;
         break;
      case Te_Bound:
         break;
      case Te_TyBase:
         break;
      case Te_TyPtr:
      case Te_TyRef:
      case Te_TyPtrMbr:
      case Te_TyRvalRef:
         te->Te.TyPorR.typeR
            = chase_cuOff( &b, ents, ents_cache, te->Te.TyPorR.typeR );
         if (b) changed = True;
         break;
      case Te_TyTyDef:
         te->Te.TyTyDef.typeR
            = chase_cuOff( &b, ents, ents_cache, te->Te.TyTyDef.typeR );
         if (b) changed = True;
         break;
      case Te_TyStOrUn:
         chase_cuOffs_in_XArray( &b, ents, ents_cache, te->Te.TyStOrUn.fieldRs );
         if (b) changed = True;
         break;
      case Te_TyEnum:
         chase_cuOffs_in_XArray( &b, ents, ents_cache, te->Te.TyEnum.atomRs );
         if (b) changed = True;
         break;
      case Te_TyArray:
         te->Te.TyArray.typeR
            = chase_cuOff( &b, ents, ents_cache, te->Te.TyArray.typeR );
         if (b) changed = True;
         chase_cuOffs_in_XArray( &b, ents, ents_cache, te->Te.TyArray.boundRs );
         if (b) changed = True;
         break;
      case Te_TyFn:
         break;
      case Te_TyQual:
         te->Te.TyQual.typeR
            = chase_cuOff( &b, ents, ents_cache, te->Te.TyQual.typeR );
         if (b) changed = True;
         break;
      case Te_TyVoid:
         break;
      default:
         ML_(pp_TyEnt)(te);
         vg_assert(0);
   }
   return changed;
}

static
Word dedup_types_substitution_pass ( XArray*  ents,
                                     TyEntIndexCache* ents_cache )
{
   Word i, n, nChanged = 0;
   Bool b;
   n = VG_(sizeXA)( ents );
   for (i = 0; i < n; i++) {
      TyEnt* ent = VG_(indexXA)( ents, i );
      vg_assert(ent->tag != Te_EMPTY);
      b = TyEnt__subst_R_fields( ents, ents_cache, ent );
      if (b)
         nChanged++;
   }

   return nChanged;
}


static
Word dedup_types_commoning_pass ( XArray*  ents )
{
   Word    n, i, nDeleted;
   WordFM* dict; 
   TyEnt*  ent;
   UWord   keyW, valW;

   dict = VG_(newFM)(
             ML_(dinfo_zalloc), "di.readdwarf3.dtcp.1", 
             ML_(dinfo_free),
             (Word(*)(UWord,UWord)) ML_(TyEnt__cmp_by_all_except_cuOff)
          );

   nDeleted = 0;
   n = VG_(sizeXA)( ents );
   for (i = 0; i < n; i++) {
      ent = VG_(indexXA)( ents, i );
      vg_assert(ent->tag != Te_EMPTY);
     
      if (ent->tag == Te_INDIR) {
         vg_assert(ent->Te.INDIR.indR < ent->cuOff);
         continue;
      }

      keyW = valW = 0;
      if (VG_(lookupFM)( dict, &keyW, &valW, (UWord)ent )) {
         
         TyEnt* old = (TyEnt*)keyW;
         vg_assert(valW == 0);
         vg_assert(old != ent);
         vg_assert(old->tag != Te_INDIR);
         vg_assert(old->cuOff < ent->cuOff); 
         ML_(TyEnt__make_EMPTY)( ent );
         ent->tag = Te_INDIR;
         ent->Te.INDIR.indR = old->cuOff;
         nDeleted++;
      } else {
         
         VG_(addToFM)( dict, (UWord)ent, 0 );
      }
   }

   VG_(deleteFM)( dict, NULL, NULL );

   return nDeleted;
}


static
void dedup_types ( Bool td3, 
                   XArray*  ents,
                   TyEntIndexCache* ents_cache )
{
   Word m, n, i, nDel, nSubst, nThresh;
   if (0) td3 = True;

   n = VG_(sizeXA)( ents );

   nThresh = n / 200;

   VG_(setCmpFnXA)( ents, (XACmpFn_t) ML_(TyEnt__cmp_by_cuOff_only) );
   VG_(sortXA)( ents );

   do {
      nDel   = dedup_types_commoning_pass ( ents );
      nSubst = dedup_types_substitution_pass ( ents, ents_cache );
      vg_assert(nDel >= 0 && nSubst >= 0);
      TRACE_D3("   %ld deletions, %ld substitutions\n", nDel, nSubst);
   } while (nDel > nThresh || nSubst > nThresh);

   m = 0;
   n = VG_(sizeXA)( ents );
   for (i = 0; i < n; i++) {
      TyEnt *ent, *ind;
      ent = VG_(indexXA)( ents, i );
      if (ent->tag != Te_INDIR) continue;
      m++;
      ind = ML_(TyEnts__index_by_cuOff)( ents, ents_cache,
                                         ent->Te.INDIR.indR );
      vg_assert(ind);
      vg_assert(ind->tag != Te_INDIR);
   }

   TRACE_D3("Overall: %ld before, %ld after\n", n, n-m);
}




__attribute__((noinline))
static void resolve_variable_types (
               void (*barf)( const HChar* ) __attribute__((noreturn)),
               XArray*  ents,
               TyEntIndexCache* ents_cache,
               XArray*  vars
            )
{
   Word i, n;
   n = VG_(sizeXA)( vars );
   for (i = 0; i < n; i++) {
      TempVar* var = *(TempVar**)VG_(indexXA)( vars, i );
      TyEnt* ent = ML_(TyEnts__index_by_cuOff)( ents, ents_cache,
                                                var->typeR );
      if (ent && ent->tag == Te_INDIR) {
         ent = ML_(TyEnts__index_by_cuOff)( ents, ents_cache, 
                                            ent->Te.INDIR.indR );
         vg_assert(ent);
         vg_assert(ent->tag != Te_INDIR);
      }

      
      if (ent && ML_(TyEnt__is_type)(ent)) {
         var->typeR = ent->cuOff;
         continue;
      }

      if (ent == NULL) {
         VG_(printf)("\n: Invalid cuOff = 0x%05lx\n", var->typeR );
         barf("resolve_variable_types: "
              "cuOff does not refer to a known type");
      }
      vg_assert(ent);
      vg_assert(ent->tag == Te_UNKNOWN);
      var->typeR = ent->cuOff;
   }
}



static Int cmp_TempVar_by_dioff ( const void* v1, const void* v2 ) {
   const TempVar* t1 = *(const TempVar *const *)v1;
   const TempVar* t2 = *(const TempVar *const *)v2;
   if (t1->dioff < t2->dioff) return -1;
   if (t1->dioff > t2->dioff) return 1;
   return 0;
}

static void read_DIE ( 
   WordFM*  rangestree,
   XArray*  tyents,
   XArray*  tempvars,
   XArray*  gexprs,
   D3TypeParser* typarser,
   D3VarParser* varparser,
   D3InlParser* inlparser,
   Cursor* c, Bool td3, CUConst* cc, Int level
)
{
   const g_abbv *abbv;
   ULong  atag, abbv_code;
   UWord  posn;
   UInt   has_children;
   UWord  start_die_c_offset;
   UWord  after_die_c_offset;
   
   
   
   
   UWord  sibling = 0;
   Bool   parse_children = False;

   
   posn      = cook_die( cc, get_position_of_Cursor( c ) );
   abbv_code = get_ULEB128( c );
   abbv = get_abbv(cc, abbv_code);
   atag      = abbv->atag;

   if (TD3) {
      TRACE_D3("\n");
      trace_DIE ((DW_TAG)atag, posn, level,
                 get_position_of_Cursor( c ), abbv, cc);
   }

   if (atag == 0)
      cc->barf("read_DIE: invalid zero tag on DIE");

   has_children = abbv->has_children;
   if (has_children != DW_children_no && has_children != DW_children_yes)
      cc->barf("read_DIE: invalid has_children value");

   start_die_c_offset  = get_position_of_Cursor( c );
   after_die_c_offset  = 0; 

   if (VG_(clo_read_var_info)) {
      parse_type_DIE( tyents,
                      typarser,
                      (DW_TAG)atag,
                      posn,
                      level,
                      c,     
                      abbv,  
                      cc,
                      td3 );
      if (get_position_of_Cursor( c ) != start_die_c_offset) {
         after_die_c_offset = get_position_of_Cursor( c );
         set_position_of_Cursor( c, start_die_c_offset );
      }

      parse_var_DIE( rangestree,
                     tempvars,
                     gexprs,
                     varparser,
                     (DW_TAG)atag,
                     posn,
                     level,
                     c,     
                     abbv,  
                     cc,
                     td3 );
      if (get_position_of_Cursor( c ) != start_die_c_offset) {
         after_die_c_offset = get_position_of_Cursor( c );
         set_position_of_Cursor( c, start_die_c_offset );
      }

      parse_children = True;
      
      
   }

   if (VG_(clo_read_inline_info)) {
      inlparser->sibling = 0;
      parse_children = 
         parse_inl_DIE( inlparser,
                        (DW_TAG)atag,
                        posn,
                        level,
                        c,     
                        abbv, 
                        cc,
                        td3 )
         || parse_children;
      if (get_position_of_Cursor( c ) != start_die_c_offset) {
         after_die_c_offset = get_position_of_Cursor( c );
         
      }
      if (sibling == 0)
         sibling = inlparser->sibling;
      vg_assert (inlparser->sibling == 0 || inlparser->sibling == sibling);
   }

   if (after_die_c_offset > 0) {
      
      set_position_of_Cursor( c, after_die_c_offset );
   } else {
      TRACE_D3("    uninteresting DIE -> skipping ...\n");
      skip_DIE (&sibling, c, abbv, cc);
   }

   if (has_children == DW_children_yes) {
      if (parse_children || sibling == 0) {
         if (0) TRACE_D3("BEGIN children of level %d\n", level);
         while (True) {
            atag = peek_ULEB128( c );
            if (atag == 0) break;
            read_DIE( rangestree, tyents, tempvars, gexprs,
                      typarser, varparser, inlparser,
                      c, td3, cc, level+1 );
         }
         
         atag = get_ULEB128( c );
         vg_assert(atag == 0);
         if (0) TRACE_D3("END children of level %d\n", level);
      } else {
         
         TRACE_D3("    SKIPPING DIE's children,"
                  "jumping to sibling <%d><%lx>\n",
                  level, sibling);
         set_position_of_Cursor( c, sibling );
      }
   }

}

static void trace_debug_loc (const DebugInfo* di,
                             __attribute__((noreturn)) void (*barf)( const HChar* ),
                             DiSlice escn_debug_loc)
{
#if 0
   
   Addr  dl_base;
   UWord dl_offset;
   Cursor loc; 
   Bool td3 = di->trace_symtab;

   TRACE_SYMTAB("\n");
   TRACE_SYMTAB("\n------ The contents of .debug_loc ------\n");
   TRACE_SYMTAB("    Offset   Begin    End      Expression\n");
   if (ML_(sli_is_valid)(escn_debug_loc)) {
      init_Cursor( &loc, escn_debug_loc, 0, barf, 
                   "Overrun whilst reading .debug_loc section(1)" );
      dl_base = 0;
      dl_offset = 0;
      while (True) {
         UWord  w1, w2;
         UWord  len;
         if (is_at_end_Cursor( &loc ))
            break;

         w1 = get_UWord( &loc );
         w2 = get_UWord( &loc );

         if (w1 == 0 && w2 == 0) {
            
            TRACE_D3("    %08lx <End of list>\n", dl_offset);
            dl_base = 0;
            dl_offset = get_position_of_Cursor( &loc );
            continue;
         }

         if (w1 == -1UL) {
            
            TRACE_D3("    %08lx %16lx %08lx (base address)\n",
                     dl_offset, w1, w2);
            dl_base = w2;
            continue;
         }

         
         TRACE_D3("    %08lx %08lx %08lx ",
                  dl_offset, w1 + dl_base, w2 + dl_base);
         len = (UWord)get_UShort( &loc );
         while (len > 0) {
            UChar byte = get_UChar( &loc );
            TRACE_D3("%02x", (UInt)byte);
            len--;
         }
         TRACE_SYMTAB("\n");
      }
   }
#endif
}

static void trace_debug_ranges (const DebugInfo* di,
                                __attribute__((noreturn)) void (*barf)( const HChar* ),
                                DiSlice escn_debug_ranges)
{
   Cursor ranges; 
   Addr  dr_base;
   UWord dr_offset;
   Bool td3 = di->trace_symtab;

   
   TRACE_SYMTAB("\n");
   TRACE_SYMTAB("\n------ The contents of .debug_ranges ------\n");
   TRACE_SYMTAB("    Offset   Begin    End\n");
   if (ML_(sli_is_valid)(escn_debug_ranges)) {
      init_Cursor( &ranges, escn_debug_ranges, 0, barf, 
                   "Overrun whilst reading .debug_ranges section(1)" );
      dr_base = 0;
      dr_offset = 0;
      while (True) {
         UWord  w1, w2;

         if (is_at_end_Cursor( &ranges ))
            break;

         w1 = get_UWord( &ranges );
         w2 = get_UWord( &ranges );

         if (w1 == 0 && w2 == 0) {
            
            TRACE_D3("    %08lx <End of list>\n", dr_offset);
            dr_base = 0;
            dr_offset = get_position_of_Cursor( &ranges );
            continue;
         }

         if (w1 == -1UL) {
            
            TRACE_D3("    %08lx %16lx %08lx (base address)\n",
                     dr_offset, w1, w2);
            dr_base = w2;
            continue;
         }

         
         TRACE_D3("    %08lx %08lx %08lx\n",
                  dr_offset, w1 + dr_base, w2 + dr_base);
      }
   }
}

static void trace_debug_abbrev (const DebugInfo* di,
                                __attribute__((noreturn)) void (*barf)( const HChar* ),
                                DiSlice escn_debug_abbv)
{
   Cursor abbv; 
   Bool td3 = di->trace_symtab;

   
   TRACE_SYMTAB("\n");
   TRACE_SYMTAB("\n------ The contents of .debug_abbrev ------\n");
   if (ML_(sli_is_valid)(escn_debug_abbv)) {
      init_Cursor( &abbv, escn_debug_abbv, 0, barf, 
                   "Overrun whilst reading .debug_abbrev section" );
      while (True) {
         if (is_at_end_Cursor( &abbv ))
            break;
         
         TRACE_D3("  Number TAG\n");
         while (True) {
            ULong atag;
            UInt  has_children;
            ULong acode = get_ULEB128( &abbv );
            if (acode == 0) break; 
            atag = get_ULEB128( &abbv );
            has_children = get_UChar( &abbv );
            TRACE_D3("   %llu      %s    [%s]\n", 
                     acode, ML_(pp_DW_TAG)(atag),
                            ML_(pp_DW_children)(has_children));
            while (True) {
               ULong at_name = get_ULEB128( &abbv );
               ULong at_form = get_ULEB128( &abbv );
               if (at_name == 0 && at_form == 0) break;
               TRACE_D3("    %-18s %s\n", 
                        ML_(pp_DW_AT)(at_name), ML_(pp_DW_FORM)(at_form));
            }
         }
      }
   }
}

static
void new_dwarf3_reader_wrk ( 
   DebugInfo* di,
   __attribute__((noreturn)) void (*barf)( const HChar* ),
   DiSlice escn_debug_info,      DiSlice escn_debug_types,
   DiSlice escn_debug_abbv,      DiSlice escn_debug_line,
   DiSlice escn_debug_str,       DiSlice escn_debug_ranges,
   DiSlice escn_debug_loc,       DiSlice escn_debug_info_alt,
   DiSlice escn_debug_abbv_alt,  DiSlice escn_debug_line_alt,
   DiSlice escn_debug_str_alt
)
{
   XArray*      tyents = NULL;
   XArray*      tyents_to_keep = NULL;
   XArray*     gexprs = NULL;
   XArray*   tempvars = NULL;
   WordFM*  rangestree = NULL;
   TyEntIndexCache* tyents_cache = NULL;
   TyEntIndexCache* tyents_to_keep_cache = NULL;
   TempVar *varp, *varp2;
   GExpr* gexpr;
   Cursor info; 
   D3TypeParser typarser;
   D3VarParser varparser;
   D3InlParser inlparser;
   Word  i, j, n;
   Bool td3 = di->trace_symtab;
   XArray*  dioff_lookup_tab;
   Int pass;
   VgHashTable *signature_types = NULL;

   
   if (TD3) {
      trace_debug_loc    (di, barf, escn_debug_loc);
      trace_debug_ranges (di, barf, escn_debug_ranges);
      trace_debug_abbrev (di, barf, escn_debug_abbv);
      TRACE_SYMTAB("\n");
   }

   VG_(memset)( &inlparser, 0, sizeof(inlparser) );

   if (VG_(clo_read_var_info)) {
      tyents = VG_(newXA)( ML_(dinfo_zalloc), 
                           "di.readdwarf3.ndrw.1 (TyEnt temp array)",
                           ML_(dinfo_free), sizeof(TyEnt) );
      { TyEnt tyent;
        VG_(memset)(&tyent, 0, sizeof(tyent));
        tyent.tag   = Te_TyVoid;
        tyent.cuOff = D3_FAKEVOID_CUOFF;
        tyent.Te.TyVoid.isFake = True;
        VG_(addToXA)( tyents, &tyent );
      }
      { TyEnt tyent;
        VG_(memset)(&tyent, 0, sizeof(tyent));
        tyent.tag   = Te_UNKNOWN;
        tyent.cuOff = D3_INVALID_CUOFF;
        VG_(addToXA)( tyents, &tyent );
      }

      rangestree = VG_(newFM)( ML_(dinfo_zalloc),
                               "di.readdwarf3.ndrw.2 (rangestree)",
                               ML_(dinfo_free),
                               (Word(*)(UWord,UWord))cmp__XArrays_of_AddrRange );

      tempvars = VG_(newXA)( ML_(dinfo_zalloc),
                             "di.readdwarf3.ndrw.3 (TempVar*s array)",
                             ML_(dinfo_free), 
                             sizeof(TempVar*) );

      gexprs = VG_(newXA)( ML_(dinfo_zalloc), "di.readdwarf3.ndrw.4",
                           ML_(dinfo_free), sizeof(GExpr*) );

      type_parser_init(&typarser);

      var_parser_init(&varparser);

      signature_types = VG_(HT_construct) ("signature_types");
   }

   if (VG_(clo_read_var_info) && ML_(sli_is_valid)(escn_debug_types)) {
      init_Cursor( &info, escn_debug_types, 0, barf,
                   "Overrun whilst reading .debug_types section" );
      TRACE_D3("\n------ Collecting signatures from "
               ".debug_types section ------\n");

      while (True) {
         UWord   cu_start_offset, cu_offset_now;
         CUConst cc;

         cu_start_offset = get_position_of_Cursor( &info );
         TRACE_D3("\n");
         TRACE_D3("  Compilation Unit @ offset 0x%lx:\n", cu_start_offset);
         
         parse_CU_Header( &cc, td3, &info, escn_debug_abbv, True, False );

         
         cc.types_cuOff_bias = escn_debug_info.szB;

         record_signatured_type( signature_types, cc.type_signature,
                                 cook_die( &cc, cc.type_offset ));

         cu_offset_now = (cu_start_offset + cc.unit_length
                          + (cc.is_dw64 ? 12 : 4));

         if (cu_offset_now >= escn_debug_types.szB) {
            clear_CUConst ( &cc);
            break;
         }

         set_position_of_Cursor ( &info, cu_offset_now );
      }
   }

   for (pass = 0; pass < 3; ++pass) {
      ULong section_size;

      if (pass == 0) {
         if (!ML_(sli_is_valid)(escn_debug_info_alt))
	    continue;
         init_Cursor( &info, escn_debug_info_alt, 0, barf,
                      "Overrun whilst reading alternate .debug_info section" );
         section_size = escn_debug_info_alt.szB;

         TRACE_D3("\n------ Parsing alternate .debug_info section ------\n");
      } else if (pass == 1) {
         init_Cursor( &info, escn_debug_info, 0, barf,
                      "Overrun whilst reading .debug_info section" );
         section_size = escn_debug_info.szB;

         TRACE_D3("\n------ Parsing .debug_info section ------\n");
      } else {
         if (!ML_(sli_is_valid)(escn_debug_types))
            continue;
         if (!VG_(clo_read_var_info))
            continue; 
         init_Cursor( &info, escn_debug_types, 0, barf,
                      "Overrun whilst reading .debug_types section" );
         section_size = escn_debug_types.szB;

         TRACE_D3("\n------ Parsing .debug_types section ------\n");
      }

      while (True) {
         ULong   cu_start_offset, cu_offset_now;
         CUConst cc;
         UWord cu_size_including_IniLen, cu_amount_used;

         Word avail = get_remaining_length_Cursor( &info );
         if (avail < 11) {
            if (avail > 0)
               TRACE_D3("new_dwarf3_reader_wrk: warning: "
                        "%ld unused bytes after end of DIEs\n", avail);
            break;
         }

         if (VG_(clo_read_var_info)) {
            
            vg_assert(varparser.sp == -1);
            
            vg_assert(typarser.sp == -1);
         }

         cu_start_offset = get_position_of_Cursor( &info );
         TRACE_D3("\n");
         TRACE_D3("  Compilation Unit @ offset 0x%llx:\n", cu_start_offset);
         
         if (pass == 0) {
            parse_CU_Header( &cc, td3, &info, escn_debug_abbv_alt,
                             False, True );
         } else {
            parse_CU_Header( &cc, td3, &info, escn_debug_abbv,
                             pass == 2, False );
         }
         cc.escn_debug_str      = pass == 0 ? escn_debug_str_alt
                                            : escn_debug_str;
         cc.escn_debug_ranges   = escn_debug_ranges;
         cc.escn_debug_loc      = escn_debug_loc;
         cc.escn_debug_line     = pass == 0 ? escn_debug_line_alt
                                            : escn_debug_line;
         cc.escn_debug_info     = pass == 0 ? escn_debug_info_alt
                                            : escn_debug_info;
         cc.escn_debug_types    = escn_debug_types;
         cc.escn_debug_info_alt = escn_debug_info_alt;
         cc.escn_debug_str_alt  = escn_debug_str_alt;
         cc.types_cuOff_bias    = escn_debug_info.szB;
         cc.alt_cuOff_bias      = escn_debug_info.szB + escn_debug_types.szB;
         cc.cu_start_offset     = cu_start_offset;
         cc.di = di;
         cc.cu_svma_known = False;
         cc.cu_svma       = 0;

         if (VG_(clo_read_var_info)) {
            cc.signature_types = signature_types;

            varstack_push( &cc, &varparser, td3, 
                           unitary_range_list(0UL, ~0UL),
                           -1, False, NULL );

            vg_assert(!varparser.fndn_ix_Table );
            varparser.fndn_ix_Table 
               = VG_(newXA)( ML_(dinfo_zalloc), "di.readdwarf3.ndrw.5var",
                             ML_(dinfo_free),
                             sizeof(UInt) );
         }

         if (VG_(clo_read_inline_info)) {
            
            vg_assert(!inlparser.fndn_ix_Table );
            inlparser.fndn_ix_Table 
               = VG_(newXA)( ML_(dinfo_zalloc), "di.readdwarf3.ndrw.5inl",
                             ML_(dinfo_free),
                             sizeof(UInt) );
         }

         
         vg_assert(!VG_(clo_read_var_info) || varparser.sp == 0);
         read_DIE( rangestree,
                   tyents, tempvars, gexprs,
                   &typarser, &varparser, &inlparser,
                   &info, td3, &cc, 0 );

         cu_offset_now = get_position_of_Cursor( &info );

         if (0) VG_(printf)("Travelled: %llu  size %llu\n",
                            cu_offset_now - cc.cu_start_offset,
                            cc.unit_length + (cc.is_dw64 ? 12 : 4));

         
         cu_size_including_IniLen = cc.unit_length + (cc.is_dw64 ? 12 : 4);
         
         cu_amount_used = cu_offset_now - cc.cu_start_offset;

         if (1) TRACE_D3("offset now %lld, d-i-size %lld\n",
                         cu_offset_now, section_size);
         if (cu_offset_now > section_size)
            barf("toplevel DIEs beyond end of CU");

         if (cu_amount_used > cu_size_including_IniLen)
            barf("CU's actual size appears to be larger than it claims it is");

         while (cu_amount_used < cu_size_including_IniLen
                && get_remaining_length_Cursor( &info ) > 0) {
            if (0) VG_(printf)("SKIP\n");
            (void)get_UChar( &info );
            cu_offset_now = get_position_of_Cursor( &info );
            cu_amount_used = cu_offset_now - cc.cu_start_offset;
         }

         if (VG_(clo_read_var_info)) {
            TRACE_D3("\n");
            varstack_preen( &varparser, td3, -2 );
            
            typestack_preen( &typarser, td3, -2 );
         }

         if (VG_(clo_read_var_info)) {
            vg_assert(varparser.fndn_ix_Table );
            VG_(deleteXA)( varparser.fndn_ix_Table );
            varparser.fndn_ix_Table = NULL;
         }
         if (VG_(clo_read_inline_info)) {
            vg_assert(inlparser.fndn_ix_Table );
            VG_(deleteXA)( inlparser.fndn_ix_Table );
            inlparser.fndn_ix_Table = NULL;
         }
         clear_CUConst(&cc);

         if (cu_offset_now == section_size)
            break;
         
      }
   }


   if (VG_(clo_read_var_info)) {
      if (TD3) {
         TRACE_D3("\n");
         ML_(pp_TyEnts)(tyents, "Initial type entity (TyEnt) array");
         TRACE_D3("\n");
         TRACE_D3("------ Compressing type entries ------\n");
      }

      tyents_cache = ML_(dinfo_zalloc)( "di.readdwarf3.ndrw.6",
                                        sizeof(TyEntIndexCache) );
      ML_(TyEntIndexCache__invalidate)( tyents_cache );
      dedup_types( td3, tyents, tyents_cache );
      if (TD3) {
         TRACE_D3("\n");
         ML_(pp_TyEnts)(tyents, "After type entity (TyEnt) compression");
      }

      TRACE_D3("\n");
      TRACE_D3("------ Resolving the types of variables ------\n" );
      resolve_variable_types( barf, tyents, tyents_cache, tempvars );

      tyents_to_keep
         = VG_(newXA)( ML_(dinfo_zalloc), 
                       "di.readdwarf3.ndrw.7 (TyEnt to-keep array)",
                       ML_(dinfo_free), sizeof(TyEnt) );
      n = VG_(sizeXA)( tyents );
      for (i = 0; i < n; i++) {
         TyEnt* ent = VG_(indexXA)( tyents, i );
         if (ent->tag != Te_INDIR)
            VG_(addToXA)( tyents_to_keep, ent );
      }

      VG_(deleteXA)( tyents );
      tyents = NULL;
      ML_(dinfo_free)( tyents_cache );
      tyents_cache = NULL;

      VG_(setCmpFnXA)( tyents_to_keep, (XACmpFn_t) ML_(TyEnt__cmp_by_cuOff_only) );
      VG_(sortXA)( tyents_to_keep );

      
      tyents_to_keep_cache
         = ML_(dinfo_zalloc)( "di.readdwarf3.ndrw.8",
                              sizeof(TyEntIndexCache) );
      ML_(TyEntIndexCache__invalidate)( tyents_to_keep_cache );

      vg_assert(!di->admin_tyents);
      di->admin_tyents = tyents_to_keep;

      
      TRACE_D3("\n");
      TRACE_D3("------ Biasing the location expressions ------\n" );

      n = VG_(sizeXA)( gexprs );
      for (i = 0; i < n; i++) {
         gexpr = *(GExpr**)VG_(indexXA)( gexprs, i );
         bias_GX( gexpr, di );
      }

      TRACE_D3("\n");
      TRACE_D3("------ Acquired the following variables: ------\n\n");

      dioff_lookup_tab
         = VG_(newXA)( ML_(dinfo_zalloc), "di.readdwarf3.ndrw.9",
                       ML_(dinfo_free), 
                       sizeof(TempVar*) );

      n = VG_(sizeXA)( tempvars );
      Word first_primary_var = 0;
      for (first_primary_var = 0;
           escn_debug_info_alt.szB && first_primary_var < n;
           first_primary_var++) {
         varp = *(TempVar**)VG_(indexXA)( tempvars, first_primary_var );
         if (varp->dioff < escn_debug_info.szB + escn_debug_types.szB)
            break;
      }
      for (i = 0; i < n; i++) {
         varp = *(TempVar**)VG_(indexXA)( tempvars, (i + first_primary_var) % n );
         if (i > first_primary_var) {
            varp2 = *(TempVar**)VG_(indexXA)( tempvars,
                                              (i + first_primary_var - 1) % n );
            vg_assert(varp2->dioff < varp->dioff);
         }
         VG_(addToXA)( dioff_lookup_tab, &varp );
      }
      VG_(setCmpFnXA)( dioff_lookup_tab, cmp_TempVar_by_dioff );
      VG_(sortXA)( dioff_lookup_tab ); 

      n = VG_(sizeXA)( tempvars );
      for (j = 0; j < n; j++) {
         TyEnt* ent;
         varp = *(TempVar**)VG_(indexXA)( tempvars, j );

         
         if (TD3) {
            VG_(printf)("<%lx> addVar: level %d: %s :: ",
                        varp->dioff,
                        varp->level,
                        varp->name ? varp->name : "<anon_var>" );
            if (varp->typeR) {
               ML_(pp_TyEnt_C_ishly)( tyents_to_keep, varp->typeR );
            } else {
               VG_(printf)("NULL");
            }
            VG_(printf)("\n  Loc=");
            if (varp->gexpr) {
               ML_(pp_GX)(varp->gexpr);
            } else {
               VG_(printf)("NULL");
            }
            VG_(printf)("\n");
            if (varp->fbGX) {
               VG_(printf)("  FrB=");
               ML_(pp_GX)( varp->fbGX );
               VG_(printf)("\n");
            } else {
               VG_(printf)("  FrB=none\n");
            }
            VG_(printf)("  declared at: %d %s:%d\n",
                        varp->fndn_ix,
                        ML_(fndn_ix2filename) (di, varp->fndn_ix),
                        varp->fLine );
            if (varp->absOri != (UWord)D3_INVALID_CUOFF)
               VG_(printf)("  abstract origin: <%lx>\n", varp->absOri);
         }

         if (!varp->gexpr) {
            TRACE_D3("  SKIP (no location)\n\n");
            continue;
         }

         if (varp->absOri != (UWord)D3_INVALID_CUOFF) {
            Bool found;
            Word ixFirst, ixLast;
            TempVar key;
            TempVar* keyp = &key;
            TempVar *varAI;
            VG_(memset)(&key, 0, sizeof(key)); 
            key.dioff = varp->absOri; 
            found = VG_(lookupXA)( dioff_lookup_tab, &keyp,
                                   &ixFirst, &ixLast );
            if (!found) {
               
               TRACE_D3("  SKIP (DW_AT_abstract_origin can't be resolved)\n\n");
               continue;
            }
            vg_assert(ixFirst == ixLast);
            varAI = *(TempVar**)VG_(indexXA)( dioff_lookup_tab, ixFirst );
            
            vg_assert(varAI);
            vg_assert(varAI->dioff == varp->absOri);

            
            if (varAI->typeR && !varp->typeR)
               varp->typeR = varAI->typeR;
            if (varAI->name && !varp->name)
               varp->name = varAI->name;
            if (varAI->fndn_ix && !varp->fndn_ix)
               varp->fndn_ix = varAI->fndn_ix;
            if (varAI->fLine > 0 && varp->fLine == 0)
               varp->fLine = varAI->fLine;
         }

         
         if (!varp->name)
            varp->name = ML_(addStr)( di, "<anon_var>", -1 );

         
         ent = ML_(TyEnts__index_by_cuOff)( tyents_to_keep,
                                            tyents_to_keep_cache, 
                                            varp->typeR );
         vg_assert(ent);
         vg_assert(ML_(TyEnt__is_type)(ent) || ent->tag == Te_UNKNOWN);

         if (ent->tag == Te_UNKNOWN) continue;

         vg_assert(varp->gexpr);
         vg_assert(varp->name);
         vg_assert(varp->typeR);
         vg_assert(varp->level >= 0);

         TRACE_D3("  ACQUIRE for range(s) ");
         { AddrRange  oneRange;
           AddrRange* varPcRanges;
           Word       nVarPcRanges;
           if (varp->nRanges == 0 || varp->nRanges == 1) {
              vg_assert(!varp->rngMany);
              if (varp->nRanges == 0) {
                 vg_assert(varp->rngOneMin == 0);
                 vg_assert(varp->rngOneMax == 0);
              }
              nVarPcRanges = varp->nRanges;
              oneRange.aMin = varp->rngOneMin;
              oneRange.aMax = varp->rngOneMax;
              varPcRanges = &oneRange;
           } else {
              vg_assert(varp->rngMany);
              vg_assert(varp->rngOneMin == 0);
              vg_assert(varp->rngOneMax == 0);
              nVarPcRanges = VG_(sizeXA)(varp->rngMany);
              vg_assert(nVarPcRanges >= 2);
              vg_assert(nVarPcRanges == (Word)varp->nRanges);
              varPcRanges = VG_(indexXA)(varp->rngMany, 0);
           }
           if (varp->level == 0)
              vg_assert( nVarPcRanges == 1 );
           
           for (i = 0; i < nVarPcRanges; i++) {
              Addr pcMin = varPcRanges[i].aMin;
              Addr pcMax = varPcRanges[i].aMax;
              vg_assert(pcMin <= pcMax);
              if (varp->level == 0) {
                 vg_assert(pcMin == (Addr)0);
                 vg_assert(pcMax == ~(Addr)0);
              } else {
                 vg_assert(pcMax < ~(Addr)0);
              }

              
              if (varp->level > 0) {
                 pcMin += di->text_debug_bias;
                 pcMax += di->text_debug_bias;
              } 

              if (i > 0 && (i%2) == 0) 
                 TRACE_D3("\n                       ");
              TRACE_D3("[%#lx,%#lx] ", pcMin, pcMax );

              ML_(addVar)(
                 di, varp->level, 
                     pcMin, pcMax,
                     varp->name,  varp->typeR,
                     varp->gexpr, varp->fbGX,
                     varp->fndn_ix, varp->fLine, td3 
              );
           }
         }

         TRACE_D3("\n\n");
         
      }

      
      n = VG_(sizeXA)( tempvars );
      for (i = 0; i < n; i++) {
         varp = *(TempVar**)VG_(indexXA)( tempvars, i );
         ML_(dinfo_free)(varp);
      }
      VG_(deleteXA)( tempvars );
      tempvars = NULL;

      
      VG_(deleteXA)( dioff_lookup_tab );

      VG_(deleteFM)( rangestree, (void(*)(UWord))VG_(deleteXA), NULL );

      
      ML_(dinfo_free)( tyents_to_keep_cache );
      tyents_to_keep_cache = NULL;

      vg_assert( varparser.fndn_ix_Table == NULL );

      
      VG_(HT_destruct) ( signature_types, ML_(dinfo_free) );

      
      vg_assert(!di->admin_gexprs);
      di->admin_gexprs = gexprs;
   }

   
   if (VG_(clo_read_var_info)) {
      type_parser_release(&typarser);
      var_parser_release(&varparser);
   }
}



static Bool               d3rd_jmpbuf_valid  = False;
static const HChar*       d3rd_jmpbuf_reason = NULL;
static VG_MINIMAL_JMP_BUF(d3rd_jmpbuf);

static __attribute__((noreturn)) void barf ( const HChar* reason ) {
   vg_assert(d3rd_jmpbuf_valid);
   d3rd_jmpbuf_reason = reason;
   VG_MINIMAL_LONGJMP(d3rd_jmpbuf);
   
   vg_assert(0);
}


void 
ML_(new_dwarf3_reader) (
   DebugInfo* di,
   DiSlice escn_debug_info,      DiSlice escn_debug_types,
   DiSlice escn_debug_abbv,      DiSlice escn_debug_line,
   DiSlice escn_debug_str,       DiSlice escn_debug_ranges,
   DiSlice escn_debug_loc,       DiSlice escn_debug_info_alt,
   DiSlice escn_debug_abbv_alt,  DiSlice escn_debug_line_alt,
   DiSlice escn_debug_str_alt
)
{
   volatile Int  jumped;
   volatile Bool td3 = di->trace_symtab;

   vg_assert(d3rd_jmpbuf_valid  == False);
   vg_assert(d3rd_jmpbuf_reason == NULL);

   d3rd_jmpbuf_valid = True;
   jumped = VG_MINIMAL_SETJMP(d3rd_jmpbuf);
   if (jumped == 0) {
      
      new_dwarf3_reader_wrk( di, barf,
                             escn_debug_info,     escn_debug_types,
                             escn_debug_abbv,     escn_debug_line,
                             escn_debug_str,      escn_debug_ranges,
                             escn_debug_loc,      escn_debug_info_alt,
                             escn_debug_abbv_alt, escn_debug_line_alt,
                             escn_debug_str_alt );
      d3rd_jmpbuf_valid = False;
      TRACE_D3("\n------ .debug_info reading was successful ------\n");
   } else {
      
      d3rd_jmpbuf_valid = False;
      
      vg_assert(d3rd_jmpbuf_reason != NULL);

      TRACE_D3("\n------ .debug_info reading failed ------\n");

      ML_(symerr)(di, True, d3rd_jmpbuf_reason);
   }

   d3rd_jmpbuf_valid  = False;
   d3rd_jmpbuf_reason = NULL;
}




#if 0
   
   TRACE_SYMTAB("\n");
   TRACE_SYMTAB("\n------ The contents of .debug_arange ------\n");
   init_Cursor( &aranges, debug_aranges_img, 
                debug_aranges_sz, 0, barf, 
                "Overrun whilst reading .debug_aranges section" );
   while (True) {
      ULong  len, d_i_offset;
      Bool   is64;
      UShort version;
      UChar  asize, segsize;

      if (is_at_end_Cursor( &aranges ))
         break;
      
      
      len = get_Initial_Length( &is64, &aranges, 
               "in .debug_aranges: invalid initial-length field" );
      version    = get_UShort( &aranges );
      d_i_offset = get_Dwarfish_UWord( &aranges, is64 );
      asize      = get_UChar( &aranges );
      segsize    = get_UChar( &aranges );
      TRACE_D3("  Length:                   %llu\n", len);
      TRACE_D3("  Version:                  %d\n", (Int)version);
      TRACE_D3("  Offset into .debug_info:  %llx\n", d_i_offset);
      TRACE_D3("  Pointer Size:             %d\n", (Int)asize);
      TRACE_D3("  Segment Size:             %d\n", (Int)segsize);
      TRACE_D3("\n");
      TRACE_D3("    Address            Length\n");

      while ((get_position_of_Cursor( &aranges ) % (2 * asize)) > 0) {
         (void)get_UChar( & aranges );
      }
      while (True) {
         ULong address = get_Dwarfish_UWord( &aranges, asize==8 );
         ULong length = get_Dwarfish_UWord( &aranges, asize==8 );
         TRACE_D3("    0x%016llx 0x%llx\n", address, length);
         if (address == 0 && length == 0) break;
      }
   }
   TRACE_SYMTAB("\n");
#endif

#endif 

