

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2000-2013 Julian Seward 
      jseward@acm.org

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

#if defined(VGO_linux) || defined(VGO_darwin)


#include "priv_aspacemgr.h"
#include "config.h"









#if defined(VGPV_arm_linux_android) \
    || defined(VGPV_x86_linux_android) \
    || defined(VGPV_mips32_linux_android) \
    || defined(VGPV_arm64_linux_android)
# define VG_N_SEGMENTS 5000
#else
# define VG_N_SEGMENTS 30000
#endif


static NSegment nsegments[VG_N_SEGMENTS];
static Int      nsegments_used = 0;

#define Addr_MIN ((Addr)0)
#define Addr_MAX ((Addr)(-1ULL))



Addr VG_(clo_aspacem_minAddr)
#if defined(VGO_darwin)
# if VG_WORDSIZE == 4
   = (Addr) 0x00001000;
# else
   = (Addr) 0x100000000;  
# endif
#else
   = (Addr) 0x04000000; 
#endif


static Addr aspacem_minAddr = 0;

static Addr aspacem_maxAddr = 0;

static Addr aspacem_cStart = 0;

static Addr aspacem_vStart = 0;


#define AM_SANITY_CHECK                                      \
   do {                                                      \
      if (VG_(clo_sanity_level >= 3))                        \
         aspacem_assert(VG_(am_do_sync_check)                \
            (__PRETTY_FUNCTION__,__FILE__,__LINE__));        \
   } while (0) 


inline
static Int  find_nsegment_idx ( Addr a );

static void parse_procselfmaps (
      void (*record_mapping)( Addr addr, SizeT len, UInt prot,
                              ULong dev, ULong ino, Off64T offset, 
                              const HChar* filename ),
      void (*record_gap)( Addr addr, SizeT len )
   );

#if defined(VGP_arm_linux)
#  define ARM_LINUX_FAKE_COMMPAGE_START 0xFFFF0000
#  define ARM_LINUX_FAKE_COMMPAGE_END1  0xFFFF1000
#endif




static const HChar* show_SegKind ( SegKind sk )
{
   switch (sk) {
      case SkFree:  return "    ";
      case SkAnonC: return "anon";
      case SkAnonV: return "ANON";
      case SkFileC: return "file";
      case SkFileV: return "FILE";
      case SkShmC:  return "shm ";
      case SkResvn: return "RSVN";
      default:      return "????";
   }
}

static const HChar* show_ShrinkMode ( ShrinkMode sm )
{
   switch (sm) {
      case SmLower: return "SmLower";
      case SmUpper: return "SmUpper";
      case SmFixed: return "SmFixed";
      default: return "Sm?????";
   }
}

static void show_len_concisely ( HChar* buf, Addr start, Addr end )
{
   const HChar* fmt;
   ULong len = ((ULong)end) - ((ULong)start) + 1;

   if (len < 10*1000*1000ULL) {
      fmt = "%7llu";
   } 
   else if (len < 999999ULL * (1ULL<<20)) {
      fmt = "%6llum";
      len >>= 20;
   }
   else if (len < 999999ULL * (1ULL<<30)) {
      fmt = "%6llug";
      len >>= 30;
   }
   else if (len < 999999ULL * (1ULL<<40)) {
      fmt = "%6llut";
      len >>= 40;
   }
   else {
      fmt = "%6llue";
      len >>= 50;
   }
   ML_(am_sprintf)(buf, fmt, len);
}


static void show_nsegment_full ( Int logLevel, Int segNo, const NSegment* seg )
{
   HChar len_buf[20];
   const HChar* name = ML_(am_get_segname)( seg->fnIdx );

   if (name == NULL)
      name = "(none)";

   show_len_concisely(len_buf, seg->start, seg->end);

   VG_(debugLog)(
      logLevel, "aspacem",
      "%3d: %s %010llx-%010llx %s %c%c%c%c%c %s "
      "d=0x%03llx i=%-7lld o=%-7lld (%d,%d) %s\n",
      segNo, show_SegKind(seg->kind),
      (ULong)seg->start, (ULong)seg->end, len_buf,
      seg->hasR ? 'r' : '-', seg->hasW ? 'w' : '-', 
      seg->hasX ? 'x' : '-', seg->hasT ? 'T' : '-',
      seg->isCH ? 'H' : '-',
      show_ShrinkMode(seg->smode),
      seg->dev, seg->ino, seg->offset,
      ML_(am_segname_get_seqnr)(seg->fnIdx), seg->fnIdx,
      name
   );
}



static void show_nsegment ( Int logLevel, Int segNo, const NSegment* seg )
{
   HChar len_buf[20];
   show_len_concisely(len_buf, seg->start, seg->end);

   switch (seg->kind) {

      case SkFree:
         VG_(debugLog)(
            logLevel, "aspacem",
            "%3d: %s %010llx-%010llx %s\n",
            segNo, show_SegKind(seg->kind),
            (ULong)seg->start, (ULong)seg->end, len_buf
         );
         break;

      case SkAnonC: case SkAnonV: case SkShmC:
         VG_(debugLog)(
            logLevel, "aspacem",
            "%3d: %s %010llx-%010llx %s %c%c%c%c%c\n",
            segNo, show_SegKind(seg->kind),
            (ULong)seg->start, (ULong)seg->end, len_buf,
            seg->hasR ? 'r' : '-', seg->hasW ? 'w' : '-', 
            seg->hasX ? 'x' : '-', seg->hasT ? 'T' : '-',
            seg->isCH ? 'H' : '-'
         );
         break;

      case SkFileC: case SkFileV:
         VG_(debugLog)(
            logLevel, "aspacem",
            "%3d: %s %010llx-%010llx %s %c%c%c%c%c d=0x%03llx "
            "i=%-7lld o=%-7lld (%d,%d)\n",
            segNo, show_SegKind(seg->kind),
            (ULong)seg->start, (ULong)seg->end, len_buf,
            seg->hasR ? 'r' : '-', seg->hasW ? 'w' : '-', 
            seg->hasX ? 'x' : '-', seg->hasT ? 'T' : '-', 
            seg->isCH ? 'H' : '-',
            seg->dev, seg->ino, seg->offset,
            ML_(am_segname_get_seqnr)(seg->fnIdx), seg->fnIdx
         );
         break;

      case SkResvn:
         VG_(debugLog)(
            logLevel, "aspacem",
            "%3d: %s %010llx-%010llx %s %c%c%c%c%c %s\n",
            segNo, show_SegKind(seg->kind),
            (ULong)seg->start, (ULong)seg->end, len_buf,
            seg->hasR ? 'r' : '-', seg->hasW ? 'w' : '-', 
            seg->hasX ? 'x' : '-', seg->hasT ? 'T' : '-', 
            seg->isCH ? 'H' : '-',
            show_ShrinkMode(seg->smode)
         );
         break;

      default:
         VG_(debugLog)(
            logLevel, "aspacem",
            "%3d: ???? UNKNOWN SEGMENT KIND\n", 
            segNo 
         );
         break;
   }
}

void VG_(am_show_nsegments) ( Int logLevel, const HChar* who )
{
   Int i;
   VG_(debugLog)(logLevel, "aspacem",
                 "<<< SHOW_SEGMENTS: %s (%d segments)\n", 
                 who, nsegments_used);
   ML_(am_show_segnames)( logLevel, who);
   for (i = 0; i < nsegments_used; i++)
     show_nsegment( logLevel, i, &nsegments[i] );
   VG_(debugLog)(logLevel, "aspacem",
                 ">>>\n");
}


const HChar* VG_(am_get_filename)( NSegment const * seg )
{
   aspacem_assert(seg);
   return ML_(am_get_segname)( seg->fnIdx );
}

/* Collect up the start addresses of segments whose kind matches one of
   the kinds specified in kind_mask.
   The interface is a bit strange in order to avoid potential
   segment-creation races caused by dynamic allocation of the result
   buffer *starts.

   The function first computes how many entries in the result
   buffer *starts will be needed.  If this number <= nStarts,
   they are placed in starts[0..], and the number is returned.
   If nStarts is not large enough, nothing is written to
   starts[0..], and the negation of the size is returned.

   Correct use of this function may mean calling it multiple times in
   order to establish a suitably-sized buffer. */

Int VG_(am_get_segment_starts)( UInt kind_mask, Addr* starts, Int nStarts )
{
   Int i, j, nSegs;

   
   aspacem_assert(nStarts > 0);

   nSegs = 0;
   for (i = 0; i < nsegments_used; i++) {
      if ((nsegments[i].kind & kind_mask) != 0)
         nSegs++;
   }

   if (nSegs > nStarts) {
      return -nSegs;
   }

   
   aspacem_assert(nSegs <= nStarts);

   j = 0;
   for (i = 0; i < nsegments_used; i++) {
      if ((nsegments[i].kind & kind_mask) != 0)
         starts[j++] = nsegments[i].start;
   }

   aspacem_assert(j == nSegs); 
   return nSegs;
}




static Bool sane_NSegment ( const NSegment* s )
{
   if (s == NULL) return False;

   
   if (s->start > s->end) return False;

   
   if (!VG_IS_PAGE_ALIGNED(s->start)) return False;
   if (!VG_IS_PAGE_ALIGNED(s->end+1)) return False;

   switch (s->kind) {

      case SkFree:
         return 
            s->smode == SmFixed
            && s->dev == 0 && s->ino == 0 && s->offset == 0 && s->fnIdx == -1 
            && !s->hasR && !s->hasW && !s->hasX && !s->hasT
            && !s->isCH;

      case SkAnonC: case SkAnonV: case SkShmC:
         return 
            s->smode == SmFixed 
            && s->dev == 0 && s->ino == 0 && s->offset == 0 && s->fnIdx == -1
            && (s->kind==SkAnonC ? True : !s->isCH);

      case SkFileC: case SkFileV:
         return 
            s->smode == SmFixed
            && ML_(am_sane_segname)(s->fnIdx)
            && !s->isCH;

      case SkResvn: 
         return 
            s->dev == 0 && s->ino == 0 && s->offset == 0 && s->fnIdx == -1 
            && !s->hasR && !s->hasW && !s->hasX && !s->hasT
            && !s->isCH;

      default:
         return False;
   }
}



static Bool maybe_merge_nsegments ( NSegment* s1, const NSegment* s2 )
{
   if (s1->kind != s2->kind) 
      return False;

   if (s1->end+1 != s2->start)
      return False;

   
   if (s1->start > s2->end)
      return False;

   switch (s1->kind) {

      case SkFree:
         s1->end = s2->end;
         return True;

      case SkAnonC: case SkAnonV:
         if (s1->hasR == s2->hasR && s1->hasW == s2->hasW 
             && s1->hasX == s2->hasX && s1->isCH == s2->isCH) {
            s1->end = s2->end;
            s1->hasT |= s2->hasT;
            return True;
         }
         break;

      case SkFileC: case SkFileV:
         if (s1->hasR == s2->hasR 
             && s1->hasW == s2->hasW && s1->hasX == s2->hasX
             && s1->dev == s2->dev && s1->ino == s2->ino
             && s2->offset == s1->offset
                              + ((ULong)s2->start) - ((ULong)s1->start) ) {
            s1->end = s2->end;
            s1->hasT |= s2->hasT;
            ML_(am_dec_refcount)(s1->fnIdx);
            return True;
         }
         break;

      case SkShmC:
         return False;

      case SkResvn:
         if (s1->smode == SmFixed && s2->smode == SmFixed) {
            s1->end = s2->end;
            return True;
         }

      default:
         break;
   
   }

   return False;
}



static Bool preen_nsegments ( void )
{
   Int i, r, w, nsegments_used_old = nsegments_used;

   aspacem_assert(nsegments_used > 0);
   aspacem_assert(nsegments[0].start == Addr_MIN);
   aspacem_assert(nsegments[nsegments_used-1].end == Addr_MAX);

   aspacem_assert(sane_NSegment(&nsegments[0]));
   for (i = 1; i < nsegments_used; i++) {
      aspacem_assert(sane_NSegment(&nsegments[i]));
      aspacem_assert(nsegments[i-1].end+1 == nsegments[i].start);
   }

   w = 0;
   for (r = 1; r < nsegments_used; r++) {
      if (maybe_merge_nsegments(&nsegments[w], &nsegments[r])) {
         
      } else {
         w++;
         if (w != r) 
            nsegments[w] = nsegments[r];
      }
   }
   w++;
   aspacem_assert(w > 0 && w <= nsegments_used);
   nsegments_used = w;

   return nsegments_used != nsegments_used_old;
}



static Bool sync_check_ok = False;

static void sync_check_mapping_callback ( Addr addr, SizeT len, UInt prot,
                                          ULong dev, ULong ino, Off64T offset, 
                                          const HChar* filename )
{
   Int  iLo, iHi, i;
   Bool sloppyXcheck, sloppyRcheck;

#if !defined(VGO_darwin)
   
   if (!sync_check_ok)
      return;
#endif
   if (len == 0)
      return;

   
   aspacem_assert(addr <= addr + len - 1); 

   iLo = find_nsegment_idx( addr );
   iHi = find_nsegment_idx( addr + len - 1 );

   
   aspacem_assert(0 <= iLo && iLo < nsegments_used);
   aspacem_assert(0 <= iHi && iHi < nsegments_used);
   aspacem_assert(iLo <= iHi);
   aspacem_assert(nsegments[iLo].start <= addr );
   aspacem_assert(nsegments[iHi].end   >= addr + len - 1 );

#  if defined(VGA_x86) || defined (VGA_s390x)
   sloppyXcheck = True;
#  else
   sloppyXcheck = False;
#  endif

#  if defined(VGA_s390x) || defined(VGO_darwin)
   sloppyRcheck = True;
#  else
   sloppyRcheck = False;
#  endif

   for (i = iLo; i <= iHi; i++) {

      Bool same, cmp_offsets, cmp_devino;
      UInt seg_prot;
   
      
      same = nsegments[i].kind == SkAnonC
             || nsegments[i].kind == SkAnonV
             || nsegments[i].kind == SkFileC
             || nsegments[i].kind == SkFileV
             || nsegments[i].kind == SkShmC;

      seg_prot = 0;
      if (nsegments[i].hasR) seg_prot |= VKI_PROT_READ;
      if (nsegments[i].hasW) seg_prot |= VKI_PROT_WRITE;
      if (nsegments[i].hasX) seg_prot |= VKI_PROT_EXEC;

      cmp_offsets
         = nsegments[i].kind == SkFileC || nsegments[i].kind == SkFileV;

      cmp_devino
         = nsegments[i].dev != 0 || nsegments[i].ino != 0;

      
#if defined(VGO_linux)
      if (filename && 0==VG_(strcmp)(filename, "/dev/zero (deleted)"))
         cmp_devino = False;

      
      if (filename && VG_(strstr)(filename, "/.lib-ro/"))
         cmp_devino = False;
#endif

#if defined(VGO_darwin)
      
      cmp_devino = False;
      
      
      cmp_offsets = False;
#endif
      
      if (sloppyXcheck && (prot & VKI_PROT_EXEC) != 0) {
         seg_prot |= VKI_PROT_EXEC;
      }

      if (sloppyRcheck && (prot & (VKI_PROT_EXEC | VKI_PROT_READ)) ==
          (VKI_PROT_EXEC | VKI_PROT_READ)) {
         seg_prot |= VKI_PROT_READ;
      }

      same = same
             && seg_prot == prot
             && (cmp_devino
                   ? (nsegments[i].dev == dev && nsegments[i].ino == ino)
                   : True)
             && (cmp_offsets 
                   ? nsegments[i].start-nsegments[i].offset == addr-offset
                   : True);
      if (!same) {
         Addr start = addr;
         Addr end = start + len - 1;
         HChar len_buf[20];
         show_len_concisely(len_buf, start, end);

         sync_check_ok = False;

         VG_(debugLog)(
            0,"aspacem",
              "segment mismatch: V's seg 1st, kernel's 2nd:\n");
         show_nsegment_full( 0, i, &nsegments[i] );
         VG_(debugLog)(0,"aspacem", 
            "...: .... %010llx-%010llx %s %c%c%c.. ....... "
            "d=0x%03llx i=%-7lld o=%-7lld (.) m=. %s\n",
            (ULong)start, (ULong)end, len_buf,
            prot & VKI_PROT_READ  ? 'r' : '-',
            prot & VKI_PROT_WRITE ? 'w' : '-',
            prot & VKI_PROT_EXEC  ? 'x' : '-',
            dev, ino, offset, filename ? filename : "(none)" );

         return;
      }
   }

   
   return;
}

static void sync_check_gap_callback ( Addr addr, SizeT len )
{
   Int iLo, iHi, i;

#if !defined(VGO_darwin)
   
   if (!sync_check_ok)
      return;
#endif 
   if (len == 0)
      return;

   
   aspacem_assert(addr <= addr + len - 1); 

   iLo = find_nsegment_idx( addr );
   iHi = find_nsegment_idx( addr + len - 1 );

   
   aspacem_assert(0 <= iLo && iLo < nsegments_used);
   aspacem_assert(0 <= iHi && iHi < nsegments_used);
   aspacem_assert(iLo <= iHi);
   aspacem_assert(nsegments[iLo].start <= addr );
   aspacem_assert(nsegments[iHi].end   >= addr + len - 1 );

   for (i = iLo; i <= iHi; i++) {

      Bool same;
   
      
      same = nsegments[i].kind == SkFree
             || nsegments[i].kind == SkResvn;

      if (!same) {
         Addr start = addr;
         Addr end = start + len - 1;
         HChar len_buf[20];
         show_len_concisely(len_buf, start, end);

         sync_check_ok = False;

         VG_(debugLog)(
            0,"aspacem",
              "segment mismatch: V's gap 1st, kernel's 2nd:\n");
         show_nsegment_full( 0, i, &nsegments[i] );
         VG_(debugLog)(0,"aspacem", 
            "   : .... %010llx-%010llx %s\n",
            (ULong)start, (ULong)end, len_buf);
         return;
      }
   }

   
   return;
}



Bool VG_(am_do_sync_check) ( const HChar* fn, 
                             const HChar* file, Int line )
{
   sync_check_ok = True;
   if (0)
      VG_(debugLog)(0,"aspacem", "do_sync_check %s:%d\n", file,line);
   parse_procselfmaps( sync_check_mapping_callback,
                       sync_check_gap_callback );
   if (!sync_check_ok) {
      VG_(debugLog)(0,"aspacem", 
                      "sync check at %s:%d (%s): FAILED\n",
                      file, line, fn);
      VG_(debugLog)(0,"aspacem", "\n");

#     if 0
      {
         HChar buf[100];   
         VG_(am_show_nsegments)(0,"post syncheck failure");
         VG_(sprintf)(buf, "/bin/cat /proc/%d/maps", VG_(getpid)());
         VG_(system)(buf);
      }
#     endif

   }
   return sync_check_ok;
}

void ML_(am_do_sanity_check)( void )
{
   AM_SANITY_CHECK;
}



__attribute__((noinline))
static Int find_nsegment_idx_WRK ( Addr a )
{
   Addr a_mid_lo, a_mid_hi;
   Int  mid,
        lo = 0,
        hi = nsegments_used-1;
   while (True) {
      
      if (lo > hi) {
         
         ML_(am_barf)("find_nsegment_idx: not found");
      }
      mid      = (lo + hi) / 2;
      a_mid_lo = nsegments[mid].start;
      a_mid_hi = nsegments[mid].end;

      if (a < a_mid_lo) { hi = mid-1; continue; }
      if (a > a_mid_hi) { lo = mid+1; continue; }
      aspacem_assert(a >= a_mid_lo && a <= a_mid_hi);
      aspacem_assert(0 <= mid && mid < nsegments_used);
      return mid;
   }
}

inline static Int find_nsegment_idx ( Addr a )
{
#  define N_CACHE 131 
   static Addr cache_pageno[N_CACHE];
   static Int  cache_segidx[N_CACHE];
   static Bool cache_inited = False;

   static UWord n_q = 0;
   static UWord n_m = 0;

   UWord ix;

   if (LIKELY(cache_inited)) {
      
   } else {
      for (ix = 0; ix < N_CACHE; ix++) {
         cache_pageno[ix] = 0;
         cache_segidx[ix] = -1;
      }
      cache_inited = True;
   }

   ix = (a >> 12) % N_CACHE;

   n_q++;
   if (0 && 0 == (n_q & 0xFFFF))
      VG_(debugLog)(0,"xxx","find_nsegment_idx: %lu %lu\n", n_q, n_m);

   if ((a >> 12) == cache_pageno[ix]
       && cache_segidx[ix] >= 0
       && cache_segidx[ix] < nsegments_used
       && nsegments[cache_segidx[ix]].start <= a
       && a <= nsegments[cache_segidx[ix]].end) {
      
      
      return cache_segidx[ix];
   }
   
   n_m++;
   cache_segidx[ix] = find_nsegment_idx_WRK(a);
   cache_pageno[ix] = a >> 12;
   return cache_segidx[ix];
#  undef N_CACHE
}


NSegment const * VG_(am_find_nsegment) ( Addr a )
{
   Int i = find_nsegment_idx(a);
   aspacem_assert(i >= 0 && i < nsegments_used);
   aspacem_assert(nsegments[i].start <= a);
   aspacem_assert(a <= nsegments[i].end);
   if (nsegments[i].kind == SkFree) 
      return NULL;
   else
      return &nsegments[i];
}


static Int segAddr_to_index ( const NSegment* seg )
{
   aspacem_assert(seg >= &nsegments[0] && seg < &nsegments[nsegments_used]);

   return seg - &nsegments[0];
}


NSegment const * VG_(am_next_nsegment) ( const NSegment* here, Bool fwds )
{
   Int i = segAddr_to_index(here);

   if (fwds) {
      i++;
      if (i >= nsegments_used)
         return NULL;
   } else {
      i--;
      if (i < 0)
         return NULL;
   }
   if (nsegments[i].kind == SkFree) 
      return NULL;
   else
      return &nsegments[i];
}


ULong VG_(am_get_anonsize_total)( void )
{
   Int   i;
   ULong total = 0;
   for (i = 0; i < nsegments_used; i++) {
      if (nsegments[i].kind == SkAnonC || nsegments[i].kind == SkAnonV) {
         total += (ULong)nsegments[i].end 
                  - (ULong)nsegments[i].start + 1ULL;
      }
   }
   return total;
}


static
Bool is_valid_for( UInt kinds, Addr start, SizeT len, UInt prot )
{
   Int  i, iLo, iHi;
   Bool needR, needW, needX;

   if (len == 0)
      return True; 
   if (start + len < start)
      return False; 

   needR = toBool(prot & VKI_PROT_READ);
   needW = toBool(prot & VKI_PROT_WRITE);
   needX = toBool(prot & VKI_PROT_EXEC);

   iLo = find_nsegment_idx(start);
   aspacem_assert(start >= nsegments[iLo].start);

   if (start+len-1 <= nsegments[iLo].end) {
      iHi = iLo;
   } else {
      iHi = find_nsegment_idx(start + len - 1);
   }

   for (i = iLo; i <= iHi; i++) {
      if ( (nsegments[i].kind & kinds) != 0
           && (needR ? nsegments[i].hasR : True)
           && (needW ? nsegments[i].hasW : True)
           && (needX ? nsegments[i].hasX : True) ) {
         
      } else {
         return False;
      }
   }

   return True;
}

Bool VG_(am_is_valid_for_client)( Addr start, SizeT len, 
                                  UInt prot )
{
   const UInt kinds = SkFileC | SkAnonC | SkShmC;

   return is_valid_for(kinds, start, len, prot);
}

Bool VG_(am_is_valid_for_client_or_free_or_resvn)
   ( Addr start, SizeT len, UInt prot )
{
   const UInt kinds = SkFileC | SkAnonC | SkShmC | SkFree | SkResvn;

   return is_valid_for(kinds, start, len, prot);
}


Bool VG_(am_is_valid_for_valgrind) ( Addr start, SizeT len, UInt prot )
{
   const UInt kinds = SkFileV | SkAnonV;

   return is_valid_for(kinds, start, len, prot);
}



static Bool any_Ts_in_range ( Addr start, SizeT len )
{
   Int iLo, iHi, i;
   aspacem_assert(len > 0);
   aspacem_assert(start + len > start);
   iLo = find_nsegment_idx(start);
   iHi = find_nsegment_idx(start + len - 1);
   for (i = iLo; i <= iHi; i++) {
      if (nsegments[i].hasT)
         return True;
   }
   return False;
}


Bool VG_(am_addr_is_in_extensible_client_stack)( Addr addr )
{
   const NSegment *seg = nsegments + find_nsegment_idx(addr);

   switch (seg->kind) {
   case SkFree:
   case SkAnonV:
   case SkFileV:
   case SkFileC:
   case SkShmC:
      return False;

   case SkResvn: {
      if (seg->smode != SmUpper) return False;
      const NSegment *next = VG_(am_next_nsegment)(seg,  True);
      if (next == NULL || next->kind != SkAnonC) return False;

      
      return True;
   }

   case SkAnonC: {
      const NSegment *next = VG_(am_next_nsegment)(seg,  False);
      if (next == NULL || next->kind != SkResvn || next->smode != SmUpper)
         return False;

      
      return True;
   }

   default:
      aspacem_assert(0);   
   }
}



static void split_nsegment_at ( Addr a )
{
   Int i, j;

   aspacem_assert(a > 0);
   aspacem_assert(VG_IS_PAGE_ALIGNED(a));
 
   i = find_nsegment_idx(a);
   aspacem_assert(i >= 0 && i < nsegments_used);

   if (nsegments[i].start == a)
      return;

   
   if (nsegments_used >= VG_N_SEGMENTS)
      ML_(am_barf_toolow)("VG_N_SEGMENTS");
   for (j = nsegments_used-1; j > i; j--)
      nsegments[j+1] = nsegments[j];
   nsegments_used++;

   nsegments[i+1]       = nsegments[i];
   nsegments[i+1].start = a;
   nsegments[i].end     = a-1;

   if (nsegments[i].kind == SkFileV || nsegments[i].kind == SkFileC)
      nsegments[i+1].offset 
         += ((ULong)nsegments[i+1].start) - ((ULong)nsegments[i].start);

   ML_(am_inc_refcount)(nsegments[i].fnIdx);

   aspacem_assert(sane_NSegment(&nsegments[i]));
   aspacem_assert(sane_NSegment(&nsegments[i+1]));
}



static 
void split_nsegments_lo_and_hi ( Addr sLo, Addr sHi,
                                 Int* iLo,
                                 Int* iHi )
{
   aspacem_assert(sLo < sHi);
   aspacem_assert(VG_IS_PAGE_ALIGNED(sLo));
   aspacem_assert(VG_IS_PAGE_ALIGNED(sHi+1));

   if (sLo > 0)
      split_nsegment_at(sLo);
   if (sHi < sHi+1)
      split_nsegment_at(sHi+1);

   *iLo = find_nsegment_idx(sLo);
   *iHi = find_nsegment_idx(sHi);
   aspacem_assert(0 <= *iLo && *iLo < nsegments_used);
   aspacem_assert(0 <= *iHi && *iHi < nsegments_used);
   aspacem_assert(*iLo <= *iHi);
   aspacem_assert(nsegments[*iLo].start == sLo);
   aspacem_assert(nsegments[*iHi].end == sHi);
   
}



static void add_segment ( const NSegment* seg )
{
   Int  i, iLo, iHi, delta;
   Bool segment_is_sane;

   Addr sStart = seg->start;
   Addr sEnd   = seg->end;

   aspacem_assert(sStart <= sEnd);
   aspacem_assert(VG_IS_PAGE_ALIGNED(sStart));
   aspacem_assert(VG_IS_PAGE_ALIGNED(sEnd+1));

   segment_is_sane = sane_NSegment(seg);
   if (!segment_is_sane) show_nsegment_full(0,-1,seg);
   aspacem_assert(segment_is_sane);

   split_nsegments_lo_and_hi( sStart, sEnd, &iLo, &iHi );

   for (i = iLo; i <= iHi; ++i)
      ML_(am_dec_refcount)(nsegments[i].fnIdx);
   delta = iHi - iLo;
   aspacem_assert(delta >= 0);
   if (delta > 0) {
      for (i = iLo; i < nsegments_used-delta; i++)
         nsegments[i] = nsegments[i+delta];
      nsegments_used -= delta;
   }

   nsegments[iLo] = *seg;

   (void)preen_nsegments();
   if (0) VG_(am_show_nsegments)(0,"AFTER preen (add_segment)");
}



static void init_nsegment ( NSegment* seg )
{
   seg->kind     = SkFree;
   seg->start    = 0;
   seg->end      = 0;
   seg->smode    = SmFixed;
   seg->dev      = 0;
   seg->ino      = 0;
   seg->mode     = 0;
   seg->offset   = 0;
   seg->fnIdx    = -1;
   seg->hasR = seg->hasW = seg->hasX = seg->hasT = seg->isCH = False;
}


static void init_resvn ( NSegment* seg, Addr start, Addr end )
{
   aspacem_assert(start < end);
   aspacem_assert(VG_IS_PAGE_ALIGNED(start));
   aspacem_assert(VG_IS_PAGE_ALIGNED(end+1));
   init_nsegment(seg);
   seg->kind  = SkResvn;
   seg->start = start;
   seg->end   = end;
}



static void read_maps_callback ( Addr addr, SizeT len, UInt prot,
                                 ULong dev, ULong ino, Off64T offset, 
                                 const HChar* filename )
{
   NSegment seg;
   init_nsegment( &seg );
   seg.start  = addr;
   seg.end    = addr+len-1;
   seg.dev    = dev;
   seg.ino    = ino;
   seg.offset = offset;
   seg.hasR   = toBool(prot & VKI_PROT_READ);
   seg.hasW   = toBool(prot & VKI_PROT_WRITE);
   seg.hasX   = toBool(prot & VKI_PROT_EXEC);
   seg.hasT   = False;

   seg.kind = SkAnonV;
   if (dev != 0 && ino != 0) 
      seg.kind = SkFileV;

#  if defined(VGO_darwin)
   
   if (offset != 0) 
      seg.kind = SkFileV;
#  endif 

#  if defined(VGP_arm_linux)
   if (addr == ARM_LINUX_FAKE_COMMPAGE_START
       && addr + len == ARM_LINUX_FAKE_COMMPAGE_END1
       && seg.kind == SkAnonV)
      seg.kind = SkAnonC;
#  endif 

   if (filename)
      seg.fnIdx = ML_(am_allocate_segname)( filename );

   if (0) show_nsegment( 2,0, &seg );
   add_segment( &seg );
}

Bool
VG_(am_is_valid_for_aspacem_minAddr)( Addr addr, const HChar **errmsg )
{
   const Addr min = VKI_PAGE_SIZE;
#if VG_WORDSIZE == 4
   const Addr max = 0x40000000;  
#else
   const Addr max = 0x200000000; 
#endif
   Bool ok = VG_IS_PAGE_ALIGNED(addr) && addr >= min && addr <= max;

   if (errmsg) {
      *errmsg = "";
      if (! ok) {
         const HChar fmt[] = "Must be a page aligned address between "
                             "0x%lx and 0x%lx";
         static HChar buf[sizeof fmt + 2 * 16];   
         ML_(am_sprintf)(buf, fmt, min, max);
         *errmsg = buf;
      }
   }
   return ok;
}

Addr VG_(am_startup) ( Addr sp_at_startup )
{
   NSegment seg;
   Addr     suggested_clstack_end;

   aspacem_assert(sizeof(Word)   == sizeof(void*));
   aspacem_assert(sizeof(Addr)   == sizeof(void*));
   aspacem_assert(sizeof(SizeT)  == sizeof(void*));
   aspacem_assert(sizeof(SSizeT) == sizeof(void*));

   
   ML_(am_segnames_init)();

   aspacem_assert(sizeof(seg.dev)    == 8);
   aspacem_assert(sizeof(seg.ino)    == 8);
   aspacem_assert(sizeof(seg.offset) == 8);
   aspacem_assert(sizeof(seg.mode)   == 4);

   
   init_nsegment(&seg);
   seg.kind        = SkFree;
   seg.start       = Addr_MIN;
   seg.end         = Addr_MAX;
   nsegments[0]    = seg;
   nsegments_used  = 1;

   aspacem_minAddr = VG_(clo_aspacem_minAddr);

#if defined(VGO_darwin)

# if VG_WORDSIZE == 4
   aspacem_maxAddr = (Addr) 0xffffffff;

   aspacem_cStart = aspacem_minAddr;
   aspacem_vStart = 0xf0000000;  
# else
   aspacem_maxAddr = (Addr) 0x7fffffffffff;

   aspacem_cStart = aspacem_minAddr;
   aspacem_vStart = 0x700000000000; 
   
# endif

   suggested_clstack_end = -1; 

#else 


   VG_(debugLog)(2, "aspacem", 
                    "        sp_at_startup = 0x%010llx (supplied)\n", 
                    (ULong)sp_at_startup );

#  if VG_WORDSIZE == 8
     aspacem_maxAddr = (Addr)0x1000000000ULL - 1; 
#    ifdef ENABLE_INNER
     { Addr cse = VG_PGROUNDDN( sp_at_startup ) - 1;
       if (aspacem_maxAddr > cse)
          aspacem_maxAddr = cse;
     }
#    endif
#  else
     aspacem_maxAddr = VG_PGROUNDDN( sp_at_startup ) - 1;
#  endif

   aspacem_cStart = aspacem_minAddr;
   aspacem_vStart = VG_PGROUNDUP(aspacem_minAddr 
                                 + (aspacem_maxAddr - aspacem_minAddr + 1) / 2);
#  ifdef ENABLE_INNER
   aspacem_vStart -= 0x10000000; 
#  endif

   suggested_clstack_end = aspacem_maxAddr - 16*1024*1024ULL
                                           + VKI_PAGE_SIZE;

#endif 

   aspacem_assert(VG_IS_PAGE_ALIGNED(aspacem_minAddr));
   aspacem_assert(VG_IS_PAGE_ALIGNED(aspacem_maxAddr + 1));
   aspacem_assert(VG_IS_PAGE_ALIGNED(aspacem_cStart));
   aspacem_assert(VG_IS_PAGE_ALIGNED(aspacem_vStart));
   aspacem_assert(VG_IS_PAGE_ALIGNED(suggested_clstack_end + 1));

   VG_(debugLog)(2, "aspacem", 
                    "              minAddr = 0x%010llx (computed)\n", 
                    (ULong)aspacem_minAddr);
   VG_(debugLog)(2, "aspacem", 
                    "              maxAddr = 0x%010llx (computed)\n", 
                    (ULong)aspacem_maxAddr);
   VG_(debugLog)(2, "aspacem", 
                    "               cStart = 0x%010llx (computed)\n", 
                    (ULong)aspacem_cStart);
   VG_(debugLog)(2, "aspacem", 
                    "               vStart = 0x%010llx (computed)\n", 
                    (ULong)aspacem_vStart);
   VG_(debugLog)(2, "aspacem", 
                    "suggested_clstack_end = 0x%010llx (computed)\n", 
                    (ULong)suggested_clstack_end);

   if (aspacem_cStart > Addr_MIN) {
      init_resvn(&seg, Addr_MIN, aspacem_cStart-1);
      add_segment(&seg);
   }
   if (aspacem_maxAddr < Addr_MAX) {
      init_resvn(&seg, aspacem_maxAddr+1, Addr_MAX);
      add_segment(&seg);
   }

   init_resvn(&seg, aspacem_vStart,  aspacem_vStart + VKI_PAGE_SIZE - 1);
   add_segment(&seg);

   VG_(am_show_nsegments)(2, "Initial layout");

   VG_(debugLog)(2, "aspacem", "Reading /proc/self/maps\n");
   parse_procselfmaps( read_maps_callback, NULL );

   VG_(am_show_nsegments)(2, "With contents of /proc/self/maps");

   AM_SANITY_CHECK;
   return suggested_clstack_end;
}




Addr VG_(am_get_advisory) ( const MapRequest*  req, 
                            Bool  forClient, 
                            Bool* ok )
{
   Int  i, j;
   Addr holeStart, holeEnd, holeLen;
   Bool fixed_not_required;

   Addr startPoint = forClient ? aspacem_cStart : aspacem_vStart;

   Addr reqStart = req->rkind==MAny ? 0 : req->start;
   Addr reqEnd   = reqStart + req->len - 1;
   Addr reqLen   = req->len;

   Int floatIdx = -1;
   Int fixedIdx = -1;

   aspacem_assert(nsegments_used > 0);

   if (0) {
      VG_(am_show_nsegments)(0,"getAdvisory");
      VG_(debugLog)(0,"aspacem", "getAdvisory 0x%llx %lld\n", 
                      (ULong)req->start, (ULong)req->len);
   }

   
   if (req->len == 0) {
      *ok = False;
      return 0;
   }

   
   if ((req->rkind==MFixed || req->rkind==MHint)
       && req->start + req->len < req->start) {
      *ok = False;
      return 0;
   }

   

   if (forClient && req->rkind == MFixed) {
      Int  iLo   = find_nsegment_idx(reqStart);
      Int  iHi   = find_nsegment_idx(reqEnd);
      Bool allow = True;
      for (i = iLo; i <= iHi; i++) {
         if (nsegments[i].kind == SkFree
             || nsegments[i].kind == SkFileC
             || nsegments[i].kind == SkAnonC
             || nsegments[i].kind == SkShmC
             || nsegments[i].kind == SkResvn) {
            
         } else {
            allow = False;
            break;
         }
      }
      if (allow) {
         
         *ok = True;
         return reqStart;
      }
      
      *ok = False;
      return 0;
   }

   

   if (forClient && req->rkind == MHint) {
      Int  iLo   = find_nsegment_idx(reqStart);
      Int  iHi   = find_nsegment_idx(reqEnd);
      Bool allow = True;
      for (i = iLo; i <= iHi; i++) {
         if (nsegments[i].kind == SkFree
             || nsegments[i].kind == SkResvn) {
            
         } else {
            allow = False;
            break;
         }
      }
      if (allow) {
         
         *ok = True;
         return reqStart;
      }
      
   }

   

   
   fixed_not_required = req->rkind == MAny;

   i = find_nsegment_idx(startPoint);

   for (j = 0; j < nsegments_used; j++) {

      if (nsegments[i].kind != SkFree) {
         i++;
         if (i >= nsegments_used) i = 0;
         continue;
      }

      holeStart = nsegments[i].start;
      holeEnd   = nsegments[i].end;

      
      aspacem_assert(holeStart <= holeEnd);
      aspacem_assert(aspacem_minAddr <= holeStart);
      aspacem_assert(holeEnd <= aspacem_maxAddr);

      
      holeLen = holeEnd - holeStart + 1;

      if (fixedIdx == -1 && holeStart <= reqStart && reqEnd <= holeEnd)
         fixedIdx = i;

      if (floatIdx == -1 && holeLen >= reqLen)
         floatIdx = i;
  
      
      if ((fixed_not_required || fixedIdx >= 0) && floatIdx >= 0)
         break;

      i++;
      if (i >= nsegments_used) i = 0;
   }

   aspacem_assert(fixedIdx >= -1 && fixedIdx < nsegments_used);
   if (fixedIdx >= 0) 
      aspacem_assert(nsegments[fixedIdx].kind == SkFree);

   aspacem_assert(floatIdx >= -1 && floatIdx < nsegments_used);
   if (floatIdx >= 0) 
      aspacem_assert(nsegments[floatIdx].kind == SkFree);

   AM_SANITY_CHECK;

   
   switch (req->rkind) {
      case MFixed:
         if (fixedIdx >= 0) {
            *ok = True;
            return req->start;
         } else {
            *ok = False;
            return 0;
         }
         break;
      case MHint:
         if (fixedIdx >= 0) {
            *ok = True;
            return req->start;
         }
         if (floatIdx >= 0) {
            *ok = True;
            return nsegments[floatIdx].start;
         }
         *ok = False;
         return 0;
      case MAny:
         if (floatIdx >= 0) {
            *ok = True;
            return nsegments[floatIdx].start;
         }
         *ok = False;
         return 0;
      default: 
         break;
   }

   
   ML_(am_barf)("getAdvisory: unknown request kind");
   *ok = False;
   return 0;
}


Addr VG_(am_get_advisory_client_simple) ( Addr start, SizeT len, 
                                          Bool* ok )
{
   MapRequest mreq;
   mreq.rkind = start==0 ? MAny : MFixed;
   mreq.start = start;
   mreq.len   = len;
   return VG_(am_get_advisory)( &mreq, True, ok );
}

static NSegment const * VG_(am_find_free_nsegment) ( Addr a )
{
   Int i = find_nsegment_idx(a);
   aspacem_assert(i >= 0 && i < nsegments_used);
   aspacem_assert(nsegments[i].start <= a);
   aspacem_assert(a <= nsegments[i].end);
   if (nsegments[i].kind == SkFree) 
      return &nsegments[i];
   else
      return NULL;
}

Bool VG_(am_covered_by_single_free_segment)
   ( Addr start, SizeT len)
{
   NSegment const* segLo = VG_(am_find_free_nsegment)( start );
   NSegment const* segHi = VG_(am_find_free_nsegment)( start + len - 1 );

   return segLo != NULL && segHi != NULL && segLo == segHi;
}



Bool
VG_(am_notify_client_mmap)( Addr a, SizeT len, UInt prot, UInt flags,
                            Int fd, Off64T offset )
{
   HChar    buf[VKI_PATH_MAX];
   ULong    dev, ino;
   UInt     mode;
   NSegment seg;
   Bool     needDiscard;

   aspacem_assert(len > 0);
   aspacem_assert(VG_IS_PAGE_ALIGNED(a));
   aspacem_assert(VG_IS_PAGE_ALIGNED(len));
   aspacem_assert(VG_IS_PAGE_ALIGNED(offset));

   
   needDiscard = any_Ts_in_range( a, len );

   init_nsegment( &seg );
   seg.kind   = (flags & VKI_MAP_ANONYMOUS) ? SkAnonC : SkFileC;
   seg.start  = a;
   seg.end    = a + len - 1;
   seg.hasR   = toBool(prot & VKI_PROT_READ);
   seg.hasW   = toBool(prot & VKI_PROT_WRITE);
   seg.hasX   = toBool(prot & VKI_PROT_EXEC);
   if (!(flags & VKI_MAP_ANONYMOUS)) {
      
      seg.offset = offset;
      if (ML_(am_get_fd_d_i_m)(fd, &dev, &ino, &mode)) {
         seg.dev = dev;
         seg.ino = ino;
         seg.mode = mode;
      }
      if (ML_(am_resolve_filename)(fd, buf, VKI_PATH_MAX)) {
         seg.fnIdx = ML_(am_allocate_segname)( buf );
      }
   }
   add_segment( &seg );
   AM_SANITY_CHECK;
   return needDiscard;
}


Bool
VG_(am_notify_client_shmat)( Addr a, SizeT len, UInt prot )
{
   NSegment seg;
   Bool     needDiscard;

   aspacem_assert(len > 0);
   aspacem_assert(VG_IS_PAGE_ALIGNED(a));
   aspacem_assert(VG_IS_PAGE_ALIGNED(len));

   
   needDiscard = any_Ts_in_range( a, len );

   init_nsegment( &seg );
   seg.kind   = SkShmC;
   seg.start  = a;
   seg.end    = a + len - 1;
   seg.offset = 0;
   seg.hasR   = toBool(prot & VKI_PROT_READ);
   seg.hasW   = toBool(prot & VKI_PROT_WRITE);
   seg.hasX   = toBool(prot & VKI_PROT_EXEC);
   add_segment( &seg );
   AM_SANITY_CHECK;
   return needDiscard;
}


Bool VG_(am_notify_mprotect)( Addr start, SizeT len, UInt prot )
{
   Int  i, iLo, iHi;
   Bool newR, newW, newX, needDiscard;

   aspacem_assert(VG_IS_PAGE_ALIGNED(start));
   aspacem_assert(VG_IS_PAGE_ALIGNED(len));

   if (len == 0)
      return False;

   newR = toBool(prot & VKI_PROT_READ);
   newW = toBool(prot & VKI_PROT_WRITE);
   newX = toBool(prot & VKI_PROT_EXEC);

   
   needDiscard = any_Ts_in_range( start, len ) && !newX;

   split_nsegments_lo_and_hi( start, start+len-1, &iLo, &iHi );

   iLo = find_nsegment_idx(start);
   iHi = find_nsegment_idx(start + len - 1);

   for (i = iLo; i <= iHi; i++) {
      
      switch (nsegments[i].kind) {
         case SkAnonC: case SkAnonV: case SkFileC: case SkFileV: case SkShmC:
            nsegments[i].hasR = newR;
            nsegments[i].hasW = newW;
            nsegments[i].hasX = newX;
            aspacem_assert(sane_NSegment(&nsegments[i]));
            break;
         default:
            break;
      }
   }

   (void)preen_nsegments();
   AM_SANITY_CHECK;
   return needDiscard;
}



Bool VG_(am_notify_munmap)( Addr start, SizeT len )
{
   NSegment seg;
   Bool     needDiscard;
   aspacem_assert(VG_IS_PAGE_ALIGNED(start));
   aspacem_assert(VG_IS_PAGE_ALIGNED(len));

   if (len == 0)
      return False;

   needDiscard = any_Ts_in_range( start, len );

   init_nsegment( &seg );
   seg.start = start;
   seg.end   = start + len - 1;

   if (start > aspacem_maxAddr 
       && 
          aspacem_maxAddr < Addr_MAX)
      seg.kind = SkResvn;
   else 
   
   if (seg.end < aspacem_minAddr && aspacem_minAddr > 0)
      seg.kind = SkResvn;
   else
      seg.kind = SkFree;

   add_segment( &seg );

   AM_SANITY_CHECK;
   return needDiscard;
}





SysRes VG_(am_mmap_file_fixed_client)
     ( Addr start, SizeT length, UInt prot, Int fd, Off64T offset )
{
   return VG_(am_mmap_named_file_fixed_client)(start, length, prot, fd, offset, NULL);
}

SysRes VG_(am_mmap_named_file_fixed_client)
     ( Addr start, SizeT length, UInt prot, Int fd, Off64T offset, const HChar *name )
{
   SysRes     sres;
   NSegment   seg;
   Addr       advised;
   Bool       ok;
   MapRequest req;
   ULong      dev, ino;
   UInt       mode;
   HChar      buf[VKI_PATH_MAX];

   
   if (length == 0 
       || !VG_IS_PAGE_ALIGNED(start)
       || !VG_IS_PAGE_ALIGNED(offset))
      return VG_(mk_SysRes_Error)( VKI_EINVAL );

   
   req.rkind = MFixed;
   req.start = start;
   req.len   = length;
   advised = VG_(am_get_advisory)( &req, True, &ok );
   if (!ok || advised != start)
      return VG_(mk_SysRes_Error)( VKI_EINVAL );

   
   sres = VG_(am_do_mmap_NO_NOTIFY)( 
             start, length, prot, 
             VKI_MAP_FIXED|VKI_MAP_PRIVATE, 
             fd, offset 
          );
   if (sr_isError(sres))
      return sres;

   if (sr_Res(sres) != start) {
      (void)ML_(am_do_munmap_NO_NOTIFY)( sr_Res(sres), length );
      return VG_(mk_SysRes_Error)( VKI_EINVAL );
   }

   
   init_nsegment( &seg );
   seg.kind   = SkFileC;
   seg.start  = start;
   seg.end    = seg.start + VG_PGROUNDUP(length) - 1;
   seg.offset = offset;
   seg.hasR   = toBool(prot & VKI_PROT_READ);
   seg.hasW   = toBool(prot & VKI_PROT_WRITE);
   seg.hasX   = toBool(prot & VKI_PROT_EXEC);
   if (ML_(am_get_fd_d_i_m)(fd, &dev, &ino, &mode)) {
      seg.dev = dev;
      seg.ino = ino;
      seg.mode = mode;
   }
   if (name) {
      seg.fnIdx = ML_(am_allocate_segname)( name );
   } else if (ML_(am_resolve_filename)(fd, buf, VKI_PATH_MAX)) {
      seg.fnIdx = ML_(am_allocate_segname)( buf );
   }
   add_segment( &seg );

   AM_SANITY_CHECK;
   return sres;
}



SysRes VG_(am_mmap_anon_fixed_client) ( Addr start, SizeT length, UInt prot )
{
   SysRes     sres;
   NSegment   seg;
   Addr       advised;
   Bool       ok;
   MapRequest req;
 
   
   if (length == 0 || !VG_IS_PAGE_ALIGNED(start))
      return VG_(mk_SysRes_Error)( VKI_EINVAL );

   
   req.rkind = MFixed;
   req.start = start;
   req.len   = length;
   advised = VG_(am_get_advisory)( &req, True, &ok );
   if (!ok || advised != start)
      return VG_(mk_SysRes_Error)( VKI_EINVAL );

   
   sres = VG_(am_do_mmap_NO_NOTIFY)( 
             start, length, prot, 
             VKI_MAP_FIXED|VKI_MAP_PRIVATE|VKI_MAP_ANONYMOUS, 
             0, 0 
          );
   if (sr_isError(sres))
      return sres;

   if (sr_Res(sres) != start) {
      (void)ML_(am_do_munmap_NO_NOTIFY)( sr_Res(sres), length );
      return VG_(mk_SysRes_Error)( VKI_EINVAL );
   }

   
   init_nsegment( &seg );
   seg.kind  = SkAnonC;
   seg.start = start;
   seg.end   = seg.start + VG_PGROUNDUP(length) - 1;
   seg.hasR  = toBool(prot & VKI_PROT_READ);
   seg.hasW  = toBool(prot & VKI_PROT_WRITE);
   seg.hasX  = toBool(prot & VKI_PROT_EXEC);
   add_segment( &seg );

   AM_SANITY_CHECK;
   return sres;
}



SysRes VG_(am_mmap_anon_float_client) ( SizeT length, Int prot )
{
   SysRes     sres;
   NSegment   seg;
   Addr       advised;
   Bool       ok;
   MapRequest req;
 
   
   if (length == 0)
      return VG_(mk_SysRes_Error)( VKI_EINVAL );

   
   req.rkind = MAny;
   req.start = 0;
   req.len   = length;
   advised = VG_(am_get_advisory)( &req, True, &ok );
   if (!ok)
      return VG_(mk_SysRes_Error)( VKI_EINVAL );

   
   sres = VG_(am_do_mmap_NO_NOTIFY)( 
             advised, length, prot, 
             VKI_MAP_FIXED|VKI_MAP_PRIVATE|VKI_MAP_ANONYMOUS, 
             0, 0 
          );
   if (sr_isError(sres))
      return sres;

   if (sr_Res(sres) != advised) {
      (void)ML_(am_do_munmap_NO_NOTIFY)( sr_Res(sres), length );
      return VG_(mk_SysRes_Error)( VKI_EINVAL );
   }

   
   init_nsegment( &seg );
   seg.kind  = SkAnonC;
   seg.start = advised;
   seg.end   = seg.start + VG_PGROUNDUP(length) - 1;
   seg.hasR  = toBool(prot & VKI_PROT_READ);
   seg.hasW  = toBool(prot & VKI_PROT_WRITE);
   seg.hasX  = toBool(prot & VKI_PROT_EXEC);
   add_segment( &seg );

   AM_SANITY_CHECK;
   return sres;
}



SysRes VG_(am_mmap_anon_float_valgrind)( SizeT length )
{
   SysRes     sres;
   NSegment   seg;
   Addr       advised;
   Bool       ok;
   MapRequest req;
 
   
   if (length == 0)
      return VG_(mk_SysRes_Error)( VKI_EINVAL );

   
   req.rkind = MAny;
   req.start = 0;
   req.len   = length;
   advised = VG_(am_get_advisory)( &req, False, &ok );
   if (!ok)
      return VG_(mk_SysRes_Error)( VKI_EINVAL );

#ifndef VM_TAG_VALGRIND
#  define VM_TAG_VALGRIND 0
#endif

   sres = VG_(am_do_mmap_NO_NOTIFY)( 
             advised, length, 
             VKI_PROT_READ|VKI_PROT_WRITE|VKI_PROT_EXEC, 
             VKI_MAP_FIXED|VKI_MAP_PRIVATE|VKI_MAP_ANONYMOUS, 
             VM_TAG_VALGRIND, 0
          );
#if defined(VGO_darwin) || defined(ENABLE_INNER)
   
   if (sr_isError(sres)) {
       
       sres = VG_(am_do_mmap_NO_NOTIFY)( 
             0, length, 
             VKI_PROT_READ|VKI_PROT_WRITE|VKI_PROT_EXEC, 
             VKI_MAP_PRIVATE|VKI_MAP_ANONYMOUS, 
             VM_TAG_VALGRIND, 0
          );
   }
#endif
   if (sr_isError(sres))
      return sres;

#if defined(VGO_linux) && !defined(ENABLE_INNER)
   if (sr_Res(sres) != advised) {
      (void)ML_(am_do_munmap_NO_NOTIFY)( sr_Res(sres), length );
      return VG_(mk_SysRes_Error)( VKI_EINVAL );
   }
#endif

   
   init_nsegment( &seg );
   seg.kind  = SkAnonV;
   seg.start = sr_Res(sres);
   seg.end   = seg.start + VG_PGROUNDUP(length) - 1;
   seg.hasR  = True;
   seg.hasW  = True;
   seg.hasX  = True;
   add_segment( &seg );

   AM_SANITY_CHECK;
   return sres;
}


void* VG_(am_shadow_alloc)(SizeT size)
{
   SysRes sres = VG_(am_mmap_anon_float_valgrind)( size );
   return sr_isError(sres) ? NULL : (void*)sr_Res(sres);
}


static SysRes VG_(am_mmap_file_float_valgrind_flags) ( SizeT length, UInt prot,
                                                       UInt flags,
                                                       Int fd, Off64T offset )
{
   SysRes     sres;
   NSegment   seg;
   Addr       advised;
   Bool       ok;
   MapRequest req;
   ULong      dev, ino;
   UInt       mode;
   HChar      buf[VKI_PATH_MAX];
 
   
   if (length == 0 || !VG_IS_PAGE_ALIGNED(offset))
      return VG_(mk_SysRes_Error)( VKI_EINVAL );

   
   req.rkind = MAny;
   req.start = 0;
   #if defined(VGA_arm) || defined(VGA_arm64) \
      || defined(VGA_mips32) || defined(VGA_mips64)
   aspacem_assert(VKI_SHMLBA >= VKI_PAGE_SIZE);
   #else
   aspacem_assert(VKI_SHMLBA == VKI_PAGE_SIZE);
   #endif
   if ((VKI_SHMLBA > VKI_PAGE_SIZE) && (VKI_MAP_SHARED & flags)) {
      
      req.len = length + VKI_SHMLBA - VKI_PAGE_SIZE;
   } else {
      req.len = length;
   }
   advised = VG_(am_get_advisory)( &req, False, &ok );
   if (!ok)
      return VG_(mk_SysRes_Error)( VKI_EINVAL );
   if ((VKI_SHMLBA > VKI_PAGE_SIZE) && (VKI_MAP_SHARED & flags))
      advised = VG_ROUNDUP(advised, VKI_SHMLBA);

   sres = VG_(am_do_mmap_NO_NOTIFY)( 
             advised, length, prot, 
             flags,
             fd, offset 
          );
   if (sr_isError(sres))
      return sres;

   if (sr_Res(sres) != advised) {
      (void)ML_(am_do_munmap_NO_NOTIFY)( sr_Res(sres), length );
      return VG_(mk_SysRes_Error)( VKI_EINVAL );
   }

   
   init_nsegment( &seg );
   seg.kind   = SkFileV;
   seg.start  = sr_Res(sres);
   seg.end    = seg.start + VG_PGROUNDUP(length) - 1;
   seg.offset = offset;
   seg.hasR   = toBool(prot & VKI_PROT_READ);
   seg.hasW   = toBool(prot & VKI_PROT_WRITE);
   seg.hasX   = toBool(prot & VKI_PROT_EXEC);
   if (ML_(am_get_fd_d_i_m)(fd, &dev, &ino, &mode)) {
      seg.dev  = dev;
      seg.ino  = ino;
      seg.mode = mode;
   }
   if (ML_(am_resolve_filename)(fd, buf, VKI_PATH_MAX)) {
      seg.fnIdx = ML_(am_allocate_segname)( buf );
   }
   add_segment( &seg );

   AM_SANITY_CHECK;
   return sres;
}

SysRes VG_(am_mmap_file_float_valgrind) ( SizeT length, UInt prot, 
                                          Int fd, Off64T offset )
{
   return VG_(am_mmap_file_float_valgrind_flags) (length, prot,
                                                  VKI_MAP_FIXED|VKI_MAP_PRIVATE,
                                                  fd, offset );
}

SysRes VG_(am_shared_mmap_file_float_valgrind)
   ( SizeT length, UInt prot, Int fd, Off64T offset )
{
   return VG_(am_mmap_file_float_valgrind_flags) (length, prot,
                                                  VKI_MAP_FIXED|VKI_MAP_SHARED,
                                                  fd, offset );
}

SysRes VG_(am_mmap_client_heap) ( SizeT length, Int prot )
{
   SysRes res = VG_(am_mmap_anon_float_client)(length, prot);

   if (! sr_isError(res)) {
      Addr addr = sr_Res(res);
      Int ix = find_nsegment_idx(addr);

      nsegments[ix].isCH = True;
   }
   return res;
}


static 
SysRes am_munmap_both_wrk ( Bool* need_discard,
                            Addr start, SizeT len, Bool forClient )
{
   Bool   d;
   SysRes sres;

   if (!VG_IS_PAGE_ALIGNED(start))
      goto eINVAL;

   if (len == 0) {
      *need_discard = False;
      return VG_(mk_SysRes_Success)( 0 );
   }

   if (start + len < len)
      goto eINVAL;

   len = VG_PGROUNDUP(len);
   aspacem_assert(VG_IS_PAGE_ALIGNED(start));
   aspacem_assert(VG_IS_PAGE_ALIGNED(len));

   if (forClient) {
      if (!VG_(am_is_valid_for_client_or_free_or_resvn)
            ( start, len, VKI_PROT_NONE ))
         goto eINVAL;
   } else {
      if (!VG_(am_is_valid_for_valgrind)
            ( start, len, VKI_PROT_NONE ))
         goto eINVAL;
   }

   d = any_Ts_in_range( start, len );

   sres = ML_(am_do_munmap_NO_NOTIFY)( start, len );
   if (sr_isError(sres))
      return sres;

   VG_(am_notify_munmap)( start, len );
   AM_SANITY_CHECK;
   *need_discard = d;
   return sres;

  eINVAL:
   return VG_(mk_SysRes_Error)( VKI_EINVAL );
}


SysRes VG_(am_munmap_client)( Bool* need_discard,
                              Addr start, SizeT len )
{
   return am_munmap_both_wrk( need_discard, start, len, True );
}


SysRes VG_(am_munmap_valgrind)( Addr start, SizeT len )
{
   Bool need_discard;
   SysRes r = am_munmap_both_wrk( &need_discard, 
                                  start, len, False );
   if (!sr_isError(r))
      aspacem_assert(!need_discard);
   return r;
}


Bool VG_(am_change_ownership_v_to_c)( Addr start, SizeT len )
{
   Int i, iLo, iHi;

   if (len == 0)
      return True;
   if (start + len < start)
      return False;
   if (!VG_IS_PAGE_ALIGNED(start) || !VG_IS_PAGE_ALIGNED(len))
      return False;

   i = find_nsegment_idx(start);
   if (nsegments[i].kind != SkFileV && nsegments[i].kind != SkAnonV)
      return False;
   if (start+len-1 > nsegments[i].end)
      return False;

   aspacem_assert(start >= nsegments[i].start);
   aspacem_assert(start+len-1 <= nsegments[i].end);

   split_nsegments_lo_and_hi( start, start+len-1, &iLo, &iHi );
   aspacem_assert(iLo == iHi);
   switch (nsegments[iLo].kind) {
      case SkFileV: nsegments[iLo].kind = SkFileC; break;
      case SkAnonV: nsegments[iLo].kind = SkAnonC; break;
      default: aspacem_assert(0); 
   }

   preen_nsegments();
   return True;
}

void VG_(am_set_segment_hasT)( Addr addr )
{
   Int i = find_nsegment_idx(addr);
   SegKind kind = nsegments[i].kind;
   aspacem_assert(kind == SkAnonC || kind == SkFileC || kind == SkShmC);
   nsegments[i].hasT = True;
}




Bool VG_(am_create_reservation) ( Addr start, SizeT length, 
                                  ShrinkMode smode, SSizeT extra )
{
   Int      startI, endI;
   NSegment seg;

   
   Addr start1 = start;
   Addr end1   = start + length - 1;

   
   Addr start2 = start1;
   Addr end2   = end1;

   if (extra < 0) start2 += extra; 
   if (extra > 0) end2 += extra;

   aspacem_assert(VG_IS_PAGE_ALIGNED(start));
   aspacem_assert(VG_IS_PAGE_ALIGNED(start+length));
   aspacem_assert(VG_IS_PAGE_ALIGNED(start2));
   aspacem_assert(VG_IS_PAGE_ALIGNED(end2+1));

   startI = find_nsegment_idx( start2 );
   endI = find_nsegment_idx( end2 );

   if (startI != endI)
      return False;

   if (nsegments[startI].kind != SkFree)
      return False;

   
   aspacem_assert(nsegments[startI].start <= start2);
   aspacem_assert(end2 <= nsegments[startI].end);

   init_nsegment( &seg );
   seg.kind  = SkResvn;
   seg.start = start1;  
   seg.end   = end1;
   seg.smode = smode;
   add_segment( &seg );

   AM_SANITY_CHECK;
   return True;
}



const NSegment *VG_(am_extend_into_adjacent_reservation_client)( Addr addr, 
                                                                 SSizeT delta,
                                                                 Bool *overflow)
{
   Int    segA, segR;
   UInt   prot;
   SysRes sres;

   *overflow = False;

   segA = find_nsegment_idx(addr);
   aspacem_assert(nsegments[segA].kind == SkAnonC);

   if (delta == 0)
      return nsegments + segA;

   prot =   (nsegments[segA].hasR ? VKI_PROT_READ : 0)
          | (nsegments[segA].hasW ? VKI_PROT_WRITE : 0)
          | (nsegments[segA].hasX ? VKI_PROT_EXEC : 0);

   aspacem_assert(VG_IS_PAGE_ALIGNED(delta<0 ? -delta : delta));

   if (delta > 0) {

      
      segR = segA+1;
      if (segR >= nsegments_used
          || nsegments[segR].kind != SkResvn
          || nsegments[segR].smode != SmLower)
         return NULL;

      if (delta + VKI_PAGE_SIZE 
                > (nsegments[segR].end - nsegments[segR].start + 1)) {
         *overflow = True;
         return NULL;
      }
        
      
      
      sres = VG_(am_do_mmap_NO_NOTIFY)( 
                nsegments[segR].start, delta,
                prot,
                VKI_MAP_FIXED|VKI_MAP_PRIVATE|VKI_MAP_ANONYMOUS, 
                0, 0 
             );
      if (sr_isError(sres))
         return NULL; 
      if (sr_Res(sres) != nsegments[segR].start) {
         
        (void)ML_(am_do_munmap_NO_NOTIFY)( sr_Res(sres), delta );
        return NULL;
      }

      
      nsegments[segR].start += delta;
      nsegments[segA].end += delta;
      aspacem_assert(nsegments[segR].start <= nsegments[segR].end);

   } else {

      
      delta = -delta;
      aspacem_assert(delta > 0);

      segR = segA-1;
      if (segR < 0
          || nsegments[segR].kind != SkResvn
          || nsegments[segR].smode != SmUpper)
         return NULL;

      if (delta + VKI_PAGE_SIZE 
                > (nsegments[segR].end - nsegments[segR].start + 1)) {
         *overflow = True;
         return NULL;
      }
        
      
      
      sres = VG_(am_do_mmap_NO_NOTIFY)( 
                nsegments[segA].start-delta, delta,
                prot,
                VKI_MAP_FIXED|VKI_MAP_PRIVATE|VKI_MAP_ANONYMOUS, 
                0, 0 
             );
      if (sr_isError(sres))
         return NULL; 
      if (sr_Res(sres) != nsegments[segA].start-delta) {
         
        (void)ML_(am_do_munmap_NO_NOTIFY)( sr_Res(sres), delta );
        return NULL;
      }

      
      nsegments[segR].end -= delta;
      nsegments[segA].start -= delta;
      aspacem_assert(nsegments[segR].start <= nsegments[segR].end);
   }

   AM_SANITY_CHECK;
   return nsegments + segA;
}



#if HAVE_MREMAP

const NSegment *VG_(am_extend_map_client)( Addr addr, SizeT delta )
{
   Addr     xStart;
   SysRes   sres;

   if (0)
      VG_(am_show_nsegments)(0, "VG_(am_extend_map_client) BEFORE");

   
   Int ix = find_nsegment_idx(addr);
   aspacem_assert(ix >= 0 && ix < nsegments_used);

   NSegment *seg = nsegments + ix;

   aspacem_assert(seg->kind == SkFileC || seg->kind == SkAnonC ||
                  seg->kind == SkShmC);
   aspacem_assert(delta > 0 && VG_IS_PAGE_ALIGNED(delta)) ;

   xStart = seg->end+1;
   aspacem_assert(xStart + delta >= delta);   

   NSegment *segf = seg + 1;
   aspacem_assert(segf->kind == SkFree);
   aspacem_assert(segf->start == xStart);
   aspacem_assert(xStart + delta - 1 <= segf->end);

   SizeT seg_old_len = seg->end + 1 - seg->start;

   AM_SANITY_CHECK;
   sres = ML_(am_do_extend_mapping_NO_NOTIFY)( seg->start, 
                                               seg_old_len,
                                               seg_old_len + delta );
   if (sr_isError(sres)) {
      AM_SANITY_CHECK;
      return NULL;
   } else {
      
      aspacem_assert(sr_Res(sres) == seg->start);
   }

   NSegment seg_copy = *seg;
   seg_copy.end += delta;
   add_segment( &seg_copy );

   if (0)
      VG_(am_show_nsegments)(0, "VG_(am_extend_map_client) AFTER");

   AM_SANITY_CHECK;
   return nsegments + find_nsegment_idx(addr);
}



Bool VG_(am_relocate_nooverlap_client)( Bool* need_discard,
                                        Addr old_addr, SizeT old_len,
                                        Addr new_addr, SizeT new_len )
{
   Int      iLo, iHi;
   SysRes   sres;
   NSegment seg;

   if (old_len == 0 || new_len == 0)
      return False;

   if (!VG_IS_PAGE_ALIGNED(old_addr) || !VG_IS_PAGE_ALIGNED(old_len)
       || !VG_IS_PAGE_ALIGNED(new_addr) || !VG_IS_PAGE_ALIGNED(new_len))
      return False;

   if (old_addr + old_len < old_addr
       || new_addr + new_len < new_addr)
      return False;

   if (old_addr + old_len - 1 < new_addr
       || new_addr + new_len - 1 < old_addr) {
      
   } else
      return False;

   iLo = find_nsegment_idx( old_addr );
   iHi = find_nsegment_idx( old_addr + old_len - 1 );
   if (iLo != iHi)
      return False;

   if (nsegments[iLo].kind != SkFileC && nsegments[iLo].kind != SkAnonC &&
       nsegments[iLo].kind != SkShmC)
      return False;

   sres = ML_(am_do_relocate_nooverlap_mapping_NO_NOTIFY)
             ( old_addr, old_len, new_addr, new_len );
   if (sr_isError(sres)) {
      AM_SANITY_CHECK;
      return False;
   } else {
      aspacem_assert(sr_Res(sres) == new_addr);
   }

   *need_discard = any_Ts_in_range( old_addr, old_len )
                   || any_Ts_in_range( new_addr, new_len );

   seg = nsegments[iLo];

   
   if (seg.kind == SkFileC) {
      seg.offset += ((ULong)old_addr) - ((ULong)seg.start);
   }
   seg.start = new_addr;
   seg.end   = new_addr + new_len - 1;
   add_segment( &seg );

   
   init_nsegment( &seg );
   seg.start = old_addr;
   seg.end   = old_addr + old_len - 1;
   if (old_addr > aspacem_maxAddr 
       && 
          aspacem_maxAddr < Addr_MAX)
      seg.kind = SkResvn;
   else
      seg.kind = SkFree;

   add_segment( &seg );

   AM_SANITY_CHECK;
   return True;
}

#endif 


#if defined(VGO_linux)



#define M_PROCMAP_BUF 100000

static HChar procmap_buf[M_PROCMAP_BUF];

static Int  buf_n_tot;


static Int hexdigit ( HChar c )
{
   if (c >= '0' && c <= '9') return (Int)(c - '0');
   if (c >= 'a' && c <= 'f') return 10 + (Int)(c - 'a');
   if (c >= 'A' && c <= 'F') return 10 + (Int)(c - 'A');
   return -1;
}

static Int decdigit ( HChar c )
{
   if (c >= '0' && c <= '9') return (Int)(c - '0');
   return -1;
}

static Int readchar ( const HChar* buf, HChar* ch )
{
   if (*buf == 0) return 0;
   *ch = *buf;
   return 1;
}

static Int readhex ( const HChar* buf, UWord* val )
{
   
   Int n = 0;
   *val = 0;
   while (hexdigit(*buf) >= 0) {
      *val = (*val << 4) + hexdigit(*buf);
      n++; buf++;
   }
   return n;
}

static Int readhex64 ( const HChar* buf, ULong* val )
{
   
   Int n = 0;
   *val = 0;
   while (hexdigit(*buf) >= 0) {
      *val = (*val << 4) + hexdigit(*buf);
      n++; buf++;
   }
   return n;
}

static Int readdec64 ( const HChar* buf, ULong* val )
{
   Int n = 0;
   *val = 0;
   while (decdigit(*buf) >= 0) {
      *val = (*val * 10) + decdigit(*buf);
      n++; buf++;
   }
   return n;
}



static void read_procselfmaps_into_buf ( void )
{
   Int    n_chunk;
   SysRes fd;
   
   
   fd = ML_(am_open)( "/proc/self/maps", VKI_O_RDONLY, 0 );
   if (sr_isError(fd))
      ML_(am_barf)("can't open /proc/self/maps");

   buf_n_tot = 0;
   do {
      n_chunk = ML_(am_read)( sr_Res(fd), &procmap_buf[buf_n_tot],
                              M_PROCMAP_BUF - buf_n_tot );
      if (n_chunk >= 0)
         buf_n_tot += n_chunk;
   } while ( n_chunk > 0 && buf_n_tot < M_PROCMAP_BUF );

   ML_(am_close)(sr_Res(fd));

   if (buf_n_tot >= M_PROCMAP_BUF-5)
      ML_(am_barf_toolow)("M_PROCMAP_BUF");
   if (buf_n_tot == 0)
      ML_(am_barf)("I/O error on /proc/self/maps");

   procmap_buf[buf_n_tot] = 0;
}

static void parse_procselfmaps (
      void (*record_mapping)( Addr addr, SizeT len, UInt prot,
                              ULong dev, ULong ino, Off64T offset, 
                              const HChar* filename ),
      void (*record_gap)( Addr addr, SizeT len )
   )
{
   Int    i, j, i_eol;
   Addr   start, endPlusOne, gapStart;
   HChar* filename;
   HChar  rr, ww, xx, pp, ch, tmp;
   UInt	  prot;
   UWord  maj, min;
   ULong  foffset, dev, ino;

   foffset = ino = 0; 

   read_procselfmaps_into_buf();

   aspacem_assert('\0' != procmap_buf[0] && 0 != buf_n_tot);

   if (0)
      VG_(debugLog)(0, "procselfmaps", "raw:\n%s\n", procmap_buf);

   
   i = 0;
   gapStart = Addr_MIN;
   while (True) {
      if (i >= buf_n_tot) break;

      
      j = readhex(&procmap_buf[i], &start);
      if (j > 0) i += j; else goto syntaxerror;
      j = readchar(&procmap_buf[i], &ch);
      if (j == 1 && ch == '-') i += j; else goto syntaxerror;
      j = readhex(&procmap_buf[i], &endPlusOne);
      if (j > 0) i += j; else goto syntaxerror;

      j = readchar(&procmap_buf[i], &ch);
      if (j == 1 && ch == ' ') i += j; else goto syntaxerror;

      j = readchar(&procmap_buf[i], &rr);
      if (j == 1 && (rr == 'r' || rr == '-')) i += j; else goto syntaxerror;
      j = readchar(&procmap_buf[i], &ww);
      if (j == 1 && (ww == 'w' || ww == '-')) i += j; else goto syntaxerror;
      j = readchar(&procmap_buf[i], &xx);
      if (j == 1 && (xx == 'x' || xx == '-')) i += j; else goto syntaxerror;
      
      j = readchar(&procmap_buf[i], &pp);
      if (j == 1 && (pp == 'p' || pp == '-' || pp == 's')) 
                                              i += j; else goto syntaxerror;

      j = readchar(&procmap_buf[i], &ch);
      if (j == 1 && ch == ' ') i += j; else goto syntaxerror;

      j = readhex64(&procmap_buf[i], &foffset);
      if (j > 0) i += j; else goto syntaxerror;

      j = readchar(&procmap_buf[i], &ch);
      if (j == 1 && ch == ' ') i += j; else goto syntaxerror;

      j = readhex(&procmap_buf[i], &maj);
      if (j > 0) i += j; else goto syntaxerror;
      j = readchar(&procmap_buf[i], &ch);
      if (j == 1 && ch == ':') i += j; else goto syntaxerror;
      j = readhex(&procmap_buf[i], &min);
      if (j > 0) i += j; else goto syntaxerror;

      j = readchar(&procmap_buf[i], &ch);
      if (j == 1 && ch == ' ') i += j; else goto syntaxerror;

      j = readdec64(&procmap_buf[i], &ino);
      if (j > 0) i += j; else goto syntaxerror;
 
      goto read_line_ok;

    syntaxerror:
      VG_(debugLog)(0, "Valgrind:", 
                       "FATAL: syntax error reading /proc/self/maps\n");
      { Int k, m;
        HChar buf50[51];
        m = 0;
        buf50[m] = 0;
        k = i - 50;
        if (k < 0) k = 0;
        for (; k <= i; k++) {
           buf50[m] = procmap_buf[k];
           buf50[m+1] = 0;
           if (m < 50-1) m++;
        }
        VG_(debugLog)(0, "procselfmaps", "Last 50 chars: '%s'\n", buf50);
      }
      ML_(am_exit)(1);

    read_line_ok:

      aspacem_assert(i < buf_n_tot);


      
      
      while (procmap_buf[i] == ' ') i++;
      
      
      i_eol = i;
      while (procmap_buf[i_eol] != '\n') i_eol++;

      
      if (procmap_buf[i] == '/') {
         filename = &procmap_buf[i];
         tmp = filename[i_eol - i];
         filename[i_eol - i] = '\0';
      } else {
	 tmp = 0;
         filename = NULL;
         foffset = 0;
      }

      prot = 0;
      if (rr == 'r') prot |= VKI_PROT_READ;
      if (ww == 'w') prot |= VKI_PROT_WRITE;
      if (xx == 'x') prot |= VKI_PROT_EXEC;

      dev = (min & 0xff) | (maj << 8) | ((min & ~0xff) << 12);

      if (record_gap && gapStart < start)
         (*record_gap) ( gapStart, start-gapStart );

      if (record_mapping && start < endPlusOne)
         (*record_mapping) ( start, endPlusOne-start,
                             prot, dev, ino,
                             foffset, filename );

      if ('\0' != tmp) {
         filename[i_eol - i] = tmp;
      }

      i = i_eol + 1;
      gapStart = endPlusOne;
   }

#  if defined(VGP_arm_linux)
   { const Addr commpage_start = ARM_LINUX_FAKE_COMMPAGE_START;
     const Addr commpage_end1  = ARM_LINUX_FAKE_COMMPAGE_END1;
     if (gapStart < commpage_start) {
        if (record_gap)
           (*record_gap)( gapStart, commpage_start - gapStart );
        if (record_mapping)
           (*record_mapping)( commpage_start, commpage_end1 - commpage_start,
                              VKI_PROT_READ|VKI_PROT_EXEC,
                              0, 0, 0,
                              NULL);
        gapStart = commpage_end1;
     }
   }
#  endif

   if (record_gap && gapStart < Addr_MAX)
      (*record_gap) ( gapStart, Addr_MAX - gapStart + 1 );
}



#elif defined(VGO_darwin)
#include <mach/mach.h>
#include <mach/mach_vm.h>

static unsigned int mach2vki(unsigned int vm_prot)
{
   return
      ((vm_prot & VM_PROT_READ)    ? VKI_PROT_READ    : 0) |
      ((vm_prot & VM_PROT_WRITE)   ? VKI_PROT_WRITE   : 0) |
      ((vm_prot & VM_PROT_EXECUTE) ? VKI_PROT_EXEC    : 0) ;
}

static UInt stats_machcalls = 0;

static void parse_procselfmaps (
      void (*record_mapping)( Addr addr, SizeT len, UInt prot,
                              ULong dev, ULong ino, Off64T offset, 
                              const HChar* filename ),
      void (*record_gap)( Addr addr, SizeT len )
   )
{
   vm_address_t iter;
   unsigned int depth;
   vm_address_t last;

   iter = 0;
   depth = 0;
   last = 0;
   while (1) {
      mach_vm_address_t addr = iter;
      mach_vm_size_t size;
      vm_region_submap_short_info_data_64_t info;
      kern_return_t kr;

      while (1) {
         mach_msg_type_number_t info_count
            = VM_REGION_SUBMAP_SHORT_INFO_COUNT_64;
         stats_machcalls++;
         kr = mach_vm_region_recurse(mach_task_self(), &addr, &size, &depth,
                                     (vm_region_info_t)&info, &info_count);
         if (kr) 
            return;
         if (info.is_submap) {
            depth++;
            continue;
         }
         break;
      }
      iter = addr + size;

      if (addr > last  &&  record_gap) {
         (*record_gap)(last, addr - last);
      }
      if (record_mapping) {
         (*record_mapping)(addr, size, mach2vki(info.protection),
                           0, 0, info.offset, NULL);
      }
      last = addr + size;
   }

   if ((Addr)-1 > last  &&  record_gap)
      (*record_gap)(last, (Addr)-1 - last);
}

static Bool        css_overflowed;
static ChangedSeg* css_local;
static Int         css_size_local;
static Int         css_used_local;

static Addr Addr__max ( Addr a, Addr b ) { return a > b ? a : b; }
static Addr Addr__min ( Addr a, Addr b ) { return a < b ? a : b; }

static void add_mapping_callback(Addr addr, SizeT len, UInt prot, 
                                 ULong dev, ULong ino, Off64T offset, 
                                 const HChar *filename)
{
   


   Int iLo, iHi, i;

   if (len == 0) return;

   
   aspacem_assert(addr <= addr + len - 1); 

   iLo = find_nsegment_idx( addr );
   iHi = find_nsegment_idx( addr + len - 1 );

   for (i = iLo; i <= iHi; i++) {

      UInt seg_prot;

      if (nsegments[i].kind == SkAnonV  ||  nsegments[i].kind == SkFileV) {
         
         continue;
      } 
      else if (nsegments[i].kind == SkFree || nsegments[i].kind == SkResvn) {
         
         ChangedSeg* cs = &css_local[css_used_local];
         if (css_used_local < css_size_local) {
            cs->is_added = True;
            cs->start    = addr;
            cs->end      = addr + len - 1;
            cs->prot     = prot;
            cs->offset   = offset;
            css_used_local++;
         } else {
            css_overflowed = True;
         }
         return;

      }
      else if (nsegments[i].kind == SkAnonC ||
               nsegments[i].kind == SkFileC ||
               nsegments[i].kind == SkShmC)
      {
         
         
         seg_prot = 0;
         if (nsegments[i].hasR) seg_prot |= VKI_PROT_READ;
         if (nsegments[i].hasW) seg_prot |= VKI_PROT_WRITE;
#        if defined(VGA_x86)
         
         
         seg_prot |= (prot & VKI_PROT_EXEC);
#        else
         if (nsegments[i].hasX) seg_prot |= VKI_PROT_EXEC;
#        endif
         if (seg_prot != prot) {
             if (VG_(clo_trace_syscalls)) 
                 VG_(debugLog)(0,"aspacem","region %p..%p permission "
                                 "mismatch (kernel %x, V %x)\n", 
                                 (void*)nsegments[i].start,
                                 (void*)(nsegments[i].end+1), prot, seg_prot);
            
            ChangedSeg* cs = &css_local[css_used_local];
            if (css_used_local < css_size_local) {
               cs->is_added = True;
               cs->start    = addr;
               cs->end      = addr + len - 1;
               cs->prot     = prot;
               cs->offset   = offset;
               css_used_local++;
            } else {
               css_overflowed = True;
            }
	    return;

         }

      } else {
         aspacem_assert(0);
      }
   }
}

static void remove_mapping_callback(Addr addr, SizeT len)
{
   

   Int iLo, iHi, i;

   if (len == 0)
      return;

   
   aspacem_assert(addr <= addr + len - 1); 

   iLo = find_nsegment_idx( addr );
   iHi = find_nsegment_idx( addr + len - 1 );

   
   for (i = iLo; i <= iHi; i++) {
      if (nsegments[i].kind != SkFree && nsegments[i].kind != SkResvn) {
         ChangedSeg* cs = &css_local[css_used_local];
         if (css_used_local < css_size_local) {
            cs->is_added = False;
            cs->start    = Addr__max(nsegments[i].start, addr);
            cs->end      = Addr__min(nsegments[i].end,   addr + len - 1);
            aspacem_assert(VG_IS_PAGE_ALIGNED(cs->start));
            aspacem_assert(VG_IS_PAGE_ALIGNED(cs->end+1));
            aspacem_assert(cs->start < cs->end);
            cs->prot     = 0;
            cs->offset   = 0;
            css_used_local++;
         } else {
            css_overflowed = True;
         }
      }
   }
}


Bool VG_(get_changed_segments)(
      const HChar* when, const HChar* where, ChangedSeg* css,
      Int css_size, Int* css_used)
{
   static UInt stats_synccalls = 1;
   aspacem_assert(when && where);

   if (0)
      VG_(debugLog)(0,"aspacem",
         "[%u,%u] VG_(get_changed_segments)(%s, %s)\n",
         stats_synccalls++, stats_machcalls, when, where
      );

   css_overflowed = False;
   css_local = css;
   css_size_local = css_size;
   css_used_local = 0;

   
   parse_procselfmaps(&add_mapping_callback, &remove_mapping_callback);

   *css_used = css_used_local;

   if (css_overflowed) {
      aspacem_assert(css_used_local == css_size_local);
   }

   return !css_overflowed;
}

#endif 


#endif 

