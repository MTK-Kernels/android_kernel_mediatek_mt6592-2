

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


#include "pub_core_basics.h"
#include "pub_core_options.h"      
#include "pub_core_debuginfo.h"
#include "pub_core_debuglog.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcbase.h"
#include "pub_core_libcprint.h"
#include "pub_core_xarray.h"
#include "pub_core_oset.h"
#include "pub_core_deduppoolalloc.h"

#include "priv_misc.h"         
#include "priv_image.h"
#include "priv_d3basics.h"     
#include "priv_tytypes.h"
#include "priv_storage.h"      



void ML_(symerr) ( const DebugInfo* di, Bool serious, const HChar* msg )
{
   
   if (VG_(clo_xml))
      return;

   if (serious) {

      VG_(message)(Vg_DebugMsg, "WARNING: Serious error when "
                                "reading debug info\n");
      if (True || VG_(clo_verbosity) < 2) {
         VG_(message)(Vg_DebugMsg, 
                      "When reading debug info from %s:\n",
                      (di && di->fsm.filename) ? di->fsm.filename
                                               : "???");
      }
      VG_(message)(Vg_DebugMsg, "%s\n", msg);

   } else { 

      if (VG_(clo_verbosity) >= 2)
         VG_(message)(Vg_DebugMsg, "%s\n", msg);

   }
}


void ML_(ppSym) ( Int idx, const DiSym* sym )
{
   const HChar** sec_names = sym->sec_names;
   vg_assert(sym->pri_name);
   if (sec_names)
      vg_assert(sec_names);
   VG_(printf)( "%5d:  %c%c %#8lx .. %#8lx (%d)      %s%s",
                idx,
                sym->isText ? 'T' : '-',
                sym->isIFunc ? 'I' : '-',
                sym->avmas.main, 
                sym->avmas.main + sym->size - 1, sym->size,
                sym->pri_name, sec_names ? " " : "" );
   if (sec_names) {
      while (*sec_names) {
         VG_(printf)("%s%s", *sec_names, *(sec_names+1) ? " " : "");
         sec_names++;
      }
   }
   VG_(printf)("\n");
}

void ML_(ppDiCfSI) ( const XArray*  exprs,
                     Addr base, UInt len,
                     const DiCfSI_m* si_m )
{
#  define SHOW_HOW(_how, _off)                   \
      do {                                       \
         if (_how == CFIR_UNKNOWN) {             \
            VG_(printf)("Unknown");              \
         } else                                  \
         if (_how == CFIR_SAME) {                \
            VG_(printf)("Same");                 \
         } else                                  \
         if (_how == CFIR_CFAREL) {              \
            VG_(printf)("cfa+%d", _off);         \
         } else                                  \
         if (_how == CFIR_MEMCFAREL) {           \
            VG_(printf)("*(cfa+%d)", _off);      \
         } else                                  \
         if (_how == CFIR_EXPR) {                \
            VG_(printf)("{");                    \
            ML_(ppCfiExpr)(exprs, _off);         \
            VG_(printf)("}");                    \
         } else {                                \
            vg_assert(0+0);                      \
         }                                       \
      } while (0)

   VG_(printf)("[%#lx .. %#lx]: ", base,
                                   base + (UWord)len - 1);
   switch (si_m->cfa_how) {
      case CFIC_IA_SPREL: 
         VG_(printf)("let cfa=oldSP+%d", si_m->cfa_off); 
         break;
      case CFIC_IA_BPREL: 
         VG_(printf)("let cfa=oldBP+%d", si_m->cfa_off); 
         break;
      case CFIC_ARM_R13REL: 
         VG_(printf)("let cfa=oldR13+%d", si_m->cfa_off); 
         break;
      case CFIC_ARM_R12REL: 
         VG_(printf)("let cfa=oldR12+%d", si_m->cfa_off); 
         break;
      case CFIC_ARM_R11REL: 
         VG_(printf)("let cfa=oldR11+%d", si_m->cfa_off); 
         break;
      case CFIR_SAME:
         VG_(printf)("let cfa=Same");
         break;
      case CFIC_ARM_R7REL: 
         VG_(printf)("let cfa=oldR7+%d", si_m->cfa_off); 
         break;
      case CFIC_ARM64_SPREL: 
         VG_(printf)("let cfa=oldSP+%d", si_m->cfa_off); 
         break;
      case CFIC_ARM64_X29REL: 
         VG_(printf)("let cfa=oldX29+%d", si_m->cfa_off); 
         break;
      case CFIC_EXPR: 
         VG_(printf)("let cfa={"); 
         ML_(ppCfiExpr)(exprs, si_m->cfa_off);
         VG_(printf)("}"); 
         break;
      default: 
         vg_assert(0);
   }

   VG_(printf)(" in RA=");
   SHOW_HOW(si_m->ra_how, si_m->ra_off);
#  if defined(VGA_x86) || defined(VGA_amd64)
   VG_(printf)(" SP=");
   SHOW_HOW(si_m->sp_how, si_m->sp_off);
   VG_(printf)(" BP=");
   SHOW_HOW(si_m->bp_how, si_m->bp_off);
#  elif defined(VGA_arm)
   VG_(printf)(" R14=");
   SHOW_HOW(si_m->r14_how, si_m->r14_off);
   VG_(printf)(" R13=");
   SHOW_HOW(si_m->r13_how, si_m->r13_off);
   VG_(printf)(" R12=");
   SHOW_HOW(si_m->r12_how, si_m->r12_off);
   VG_(printf)(" R11=");
   SHOW_HOW(si_m->r11_how, si_m->r11_off);
   VG_(printf)(" R7=");
   SHOW_HOW(si_m->r7_how, si_m->r7_off);
#  elif defined(VGA_ppc32) || defined(VGA_ppc64be) || defined(VGA_ppc64le)
#  elif defined(VGA_s390x) || defined(VGA_mips32) || defined(VGA_mips64)
   VG_(printf)(" SP=");
   SHOW_HOW(si_m->sp_how, si_m->sp_off);
   VG_(printf)(" FP=");
   SHOW_HOW(si_m->fp_how, si_m->fp_off);
#  elif defined(VGA_arm64)
   VG_(printf)(" SP=");
   SHOW_HOW(si_m->sp_how, si_m->sp_off);
   VG_(printf)(" X30=");
   SHOW_HOW(si_m->x30_how, si_m->x30_off);
   VG_(printf)(" X29=");
   SHOW_HOW(si_m->x29_how, si_m->x29_off);
#  elif defined(VGA_tilegx)
   VG_(printf)(" SP=");
   SHOW_HOW(si_m->sp_how, si_m->sp_off);
   VG_(printf)(" FP=");
   SHOW_HOW(si_m->fp_how, si_m->fp_off);
#  else
#    error "Unknown arch"
#  endif
   VG_(printf)("\n");
#  undef SHOW_HOW
}



const HChar* ML_(addStr) ( DebugInfo* di, const HChar* str, Int len )
{
   if (len == -1) {
      len = VG_(strlen)(str);
   } else {
      vg_assert(len >= 0);
   }
   if (UNLIKELY(di->strpool == NULL))
      di->strpool = VG_(newDedupPA)(SEGINFO_STRPOOLSIZE,
                                    1,
                                    ML_(dinfo_zalloc),
                                    "di.storage.addStr.1",
                                    ML_(dinfo_free));
   return VG_(allocEltDedupPA) (di->strpool, len+1, str);
}

UInt ML_(addFnDn) (struct _DebugInfo* di,
                   const HChar* filename, 
                   const HChar* dirname)
{
   FnDn fndn;
   UInt fndn_ix;

   if (UNLIKELY(di->fndnpool == NULL))
      di->fndnpool = VG_(newDedupPA)(500,
                                     vg_alignof(FnDn),
                                     ML_(dinfo_zalloc),
                                     "di.storage.addFnDn.1",
                                     ML_(dinfo_free));
   fndn.filename = ML_(addStr)(di, filename, -1);
   fndn.dirname = dirname ? ML_(addStr)(di, dirname, -1) : NULL;
   fndn_ix = VG_(allocFixedEltDedupPA) (di->fndnpool, sizeof(FnDn), &fndn);
   return fndn_ix;
}

const HChar* ML_(fndn_ix2filename) (const DebugInfo* di,
                                    UInt fndn_ix)
{
   FnDn *fndn;
   if (fndn_ix == 0)
      return "???";
   else {
      fndn = VG_(indexEltNumber) (di->fndnpool, fndn_ix);
      return fndn->filename;
   }
}

const HChar* ML_(fndn_ix2dirname) (const DebugInfo* di,
                                   UInt fndn_ix)
{
   FnDn *fndn;
   if (fndn_ix == 0)
      return "";
   else {
      fndn = VG_(indexEltNumber) (di->fndnpool, fndn_ix);
      if (fndn->dirname)
         return fndn->dirname;
      else
         return "";
   }
}

const HChar* ML_(addStrFromCursor)( DebugInfo* di, DiCursor c )
{
   vg_assert(ML_(cur_is_valid)(c));
   HChar* str = ML_(cur_read_strdup)(c, "di.addStrFromCursor.1");
   const HChar* res = ML_(addStr)(di, str, -1);
   ML_(dinfo_free)(str);
   return res;
}


void ML_(addSym) ( struct _DebugInfo* di, DiSym* sym )
{
   UInt   new_sz, i;
   DiSym* new_tab;

   vg_assert(sym->pri_name != NULL);
   vg_assert(sym->sec_names == NULL);

   
   if (sym->size == 0) return;

   if (di->symtab_used == di->symtab_size) {
      new_sz = 2 * di->symtab_size;
      if (new_sz == 0) new_sz = 500;
      new_tab = ML_(dinfo_zalloc)( "di.storage.addSym.1", 
                                   new_sz * sizeof(DiSym) );
      if (di->symtab != NULL) {
         for (i = 0; i < di->symtab_used; i++)
            new_tab[i] = di->symtab[i];
         ML_(dinfo_free)(di->symtab);
      }
      di->symtab = new_tab;
      di->symtab_size = new_sz;
   }

   di->symtab[di->symtab_used++] = *sym;
   vg_assert(di->symtab_used <= di->symtab_size);
}

UInt ML_(fndn_ix) (const DebugInfo* di, Word locno)
{
   UInt fndn_ix;

   switch(di->sizeof_fndn_ix) {
      case 1: fndn_ix = ((UChar*)  di->loctab_fndn_ix)[locno]; break;
      case 2: fndn_ix = ((UShort*) di->loctab_fndn_ix)[locno]; break;
      case 4: fndn_ix = ((UInt*)   di->loctab_fndn_ix)[locno]; break;
      default: vg_assert(0);
   }
   return fndn_ix;
}

static inline void set_fndn_ix (struct _DebugInfo* di, Word locno, UInt fndn_ix)
{
   Word i;

   switch(di->sizeof_fndn_ix) {
      case 1: 
         if (LIKELY (fndn_ix <= 255)) {
            ((UChar*) di->loctab_fndn_ix)[locno] = fndn_ix;
            return;
         }
         {
            UChar* old = (UChar*) di->loctab_fndn_ix;
            UShort* new = ML_(dinfo_zalloc)( "di.storage.sfix.1",
                                             di->loctab_size * 2 );
            for (i = 0; i < di->loctab_used; i++)
               new[i] = old[i];
            ML_(dinfo_free)(old);
            di->sizeof_fndn_ix = 2;
            di->loctab_fndn_ix = new;
         }
         

      case 2:
         if (LIKELY (fndn_ix <= 65535)) {
            ((UShort*) di->loctab_fndn_ix)[locno] = fndn_ix;
            return;
         }
         {
            UShort* old = (UShort*) di->loctab_fndn_ix;
            UInt* new = ML_(dinfo_zalloc)( "di.storage.sfix.2",
                                           di->loctab_size * 4 );
            for (i = 0; i < di->loctab_used; i++)
               new[i] = old[i];
            ML_(dinfo_free)(old);
            di->sizeof_fndn_ix = 4;
            di->loctab_fndn_ix = new;
         }
         

      case 4:
         ((UInt*) di->loctab_fndn_ix)[locno] = fndn_ix;
         return;

      default: vg_assert(0);
   }
}

static void addLoc ( struct _DebugInfo* di, DiLoc* loc, UInt fndn_ix )
{
   
   vg_assert(loc->size > 0);

   if (di->loctab_used == di->loctab_size) {
      UInt   new_sz;
      DiLoc* new_loctab;
      void*  new_loctab_fndn_ix;

      new_sz = 2 * di->loctab_size;
      if (new_sz == 0) new_sz = 500;
      new_loctab = ML_(dinfo_zalloc)( "di.storage.addLoc.1",
                                      new_sz * sizeof(DiLoc) );
      if (di->sizeof_fndn_ix == 0)
         di->sizeof_fndn_ix = 1; 
      new_loctab_fndn_ix = ML_(dinfo_zalloc)( "di.storage.addLoc.2",
                                              new_sz * di->sizeof_fndn_ix );
      if (di->loctab != NULL) {
         VG_(memcpy)(new_loctab, di->loctab,
                     di->loctab_used * sizeof(DiLoc));
         VG_(memcpy)(new_loctab_fndn_ix, di->loctab_fndn_ix,
                     di->loctab_used * di->sizeof_fndn_ix);
         ML_(dinfo_free)(di->loctab);
         ML_(dinfo_free)(di->loctab_fndn_ix);
      }
      di->loctab = new_loctab;
      di->loctab_fndn_ix = new_loctab_fndn_ix;
      di->loctab_size = new_sz;
   }

   di->loctab[di->loctab_used] = *loc;
   set_fndn_ix (di, di->loctab_used, fndn_ix);
   di->loctab_used++;
   vg_assert(di->loctab_used <= di->loctab_size);
}


static void shrinkLocTab ( struct _DebugInfo* di )
{
   UWord new_sz = di->loctab_used;
   if (new_sz == di->loctab_size) return;
   vg_assert(new_sz < di->loctab_size);
   ML_(dinfo_shrink_block)( di->loctab, new_sz * sizeof(DiLoc));
   ML_(dinfo_shrink_block)( di->loctab_fndn_ix, new_sz * di->sizeof_fndn_ix);
   di->loctab_size = new_sz;
}


void ML_(addLineInfo) ( struct _DebugInfo* di,
                        UInt     fndn_ix,
                        Addr     this,
                        Addr     next,
                        Int      lineno,
                        Int      entry 
     )
{
   static const Bool debug = False;
   DiLoc loc;
   UWord size = next - this;

   
   if (this == next) return;

   if (debug) {
      FnDn *fndn = VG_(indexEltNumber) (di->fndnpool, fndn_ix);
      VG_(printf)( "  src ix %u %s %s line %d %#lx-%#lx\n",
                   fndn_ix, 
                   fndn->dirname ? fndn->dirname : "(unknown)",
                   fndn->filename, lineno, this, next );
   }

   if (this > next) {
       if (VG_(clo_verbosity) > 2) {
           VG_(message)(Vg_DebugMsg, 
                        "warning: line info addresses out of order "
                        "at entry %d: 0x%lx 0x%lx\n", entry, this, next);
       }
       size = 1;
   }

   if (size > MAX_LOC_SIZE) {
       if (0)
       VG_(message)(Vg_DebugMsg, 
                    "warning: line info address range too large "
                    "at entry %d: %lu\n", entry, size);
       size = 1;
   }

   vg_assert(size >= 1);
   vg_assert(size <= MAX_LOC_SIZE);

   vg_assert(di->fsm.have_rx_map && di->fsm.have_rw_map);
   if (ML_(find_rx_mapping)(di, this, this + size - 1) == NULL) {
       if (0)
          VG_(message)(Vg_DebugMsg, 
                       "warning: ignoring line info entry falling "
                       "outside current DebugInfo: %#lx %#lx %#lx %#lx\n",
                       di->text_avma, 
                       di->text_avma + di->text_size, 
                       this, this + size - 1);
       return;
   }

   vg_assert(lineno >= 0);
   if (lineno > MAX_LINENO) {
      static Bool complained = False;
      if (!complained) {
         complained = True;
         VG_(message)(Vg_UserMsg, 
                      "warning: ignoring line info entry with "
                      "huge line number (%d)\n", lineno);
         VG_(message)(Vg_UserMsg, 
                      "         Can't handle line numbers "
                      "greater than %d, sorry\n", MAX_LINENO);
         VG_(message)(Vg_UserMsg, 
                      "(Nb: this message is only shown once)\n");
      }
      return;
   }

   loc.addr      = this;
   loc.size      = (UShort)size;
   loc.lineno    = lineno;

   if (0) VG_(message)(Vg_DebugMsg, 
		       "addLoc: addr %#lx, size %lu, line %d, fndn_ix %u\n",
		       this,size,lineno,fndn_ix);

   addLoc ( di, &loc, fndn_ix );
}

static void addInl ( struct _DebugInfo* di, DiInlLoc* inl )
{
   UInt   new_sz, i;
   DiInlLoc* new_tab;

   
   vg_assert(inl->addr_lo < inl->addr_hi);

   if (di->inltab_used == di->inltab_size) {
      new_sz = 2 * di->inltab_size;
      if (new_sz == 0) new_sz = 500;
      new_tab = ML_(dinfo_zalloc)( "di.storage.addInl.1",
                                   new_sz * sizeof(DiInlLoc) );
      if (di->inltab != NULL) {
         for (i = 0; i < di->inltab_used; i++)
            new_tab[i] = di->inltab[i];
         ML_(dinfo_free)(di->inltab);
      }
      di->inltab = new_tab;
      di->inltab_size = new_sz;
   }

   di->inltab[di->inltab_used] = *inl;
   if (inl->addr_hi - inl->addr_lo > di->maxinl_codesz)
      di->maxinl_codesz = inl->addr_hi - inl->addr_lo;
   di->inltab_used++;
   vg_assert(di->inltab_used <= di->inltab_size);
}


static void shrinkInlTab ( struct _DebugInfo* di )
{
   UWord new_sz = di->inltab_used;
   if (new_sz == di->inltab_size) return;
   vg_assert(new_sz < di->inltab_size);
   ML_(dinfo_shrink_block)( di->inltab, new_sz * sizeof(DiInlLoc));
   di->inltab_size = new_sz;
}

void ML_(addInlInfo) ( struct _DebugInfo* di, 
                       Addr addr_lo, Addr addr_hi,
                       const HChar* inlinedfn,
                       UInt fndn_ix,
                       Int lineno, UShort level)
{
   DiInlLoc inl;

   
   if (addr_lo >= addr_hi) {
       if (VG_(clo_verbosity) > 2) {
           VG_(message)(Vg_DebugMsg, 
                        "warning: inlined info addresses out of order "
                        "at: 0x%lx 0x%lx\n", addr_lo, addr_hi);
       }
       addr_hi = addr_lo + 1;
   }

   vg_assert(lineno >= 0);
   if (lineno > MAX_LINENO) {
      static Bool complained = False;
      if (!complained) {
         complained = True;
         VG_(message)(Vg_UserMsg, 
                      "warning: ignoring inlined call info entry with "
                      "huge line number (%d)\n", lineno);
         VG_(message)(Vg_UserMsg, 
                      "         Can't handle line numbers "
                      "greater than %d, sorry\n", MAX_LINENO);
         VG_(message)(Vg_UserMsg, 
                      "(Nb: this message is only shown once)\n");
      }
      return;
   }

   
   inl.addr_lo   = addr_lo;
   inl.addr_hi   = addr_hi;
   inl.inlinedfn = inlinedfn;
   
   inl.fndn_ix   = fndn_ix;
   inl.lineno    = lineno;
   inl.level     = level;

   if (0) VG_(message)
             (Vg_DebugMsg, 
              "addInlInfo: fn %s inlined as addr_lo %#lx,addr_hi %#lx,"
              "caller fndn_ix %d %s:%d\n",
              inlinedfn, addr_lo, addr_hi, fndn_ix,
              ML_(fndn_ix2filename) (di, fndn_ix), lineno);

   addInl ( di, &inl );
}

DiCfSI_m* ML_(get_cfsi_m) (const DebugInfo* di, UInt pos)
{
   UInt cfsi_m_ix;

   vg_assert(pos >= 0 && pos < di->cfsi_used);
   switch (di->sizeof_cfsi_m_ix) {
      case 1: cfsi_m_ix = ((UChar*)  di->cfsi_m_ix)[pos]; break;
      case 2: cfsi_m_ix = ((UShort*) di->cfsi_m_ix)[pos]; break;
      case 4: cfsi_m_ix = ((UInt*)   di->cfsi_m_ix)[pos]; break;
      default: vg_assert(0);
   }
   if (cfsi_m_ix == 0)
      return NULL; 
   else
      return VG_(indexEltNumber) (di->cfsi_m_pool, cfsi_m_ix);
}

void ML_(addDiCfSI) ( struct _DebugInfo* di,
                      Addr base, UInt len, DiCfSI_m* cfsi_m )
{
   static const Bool debug = False;
   UInt    new_sz;
   DiCfSI* new_tab;
   SSizeT  delta;
   DebugInfoMapping* map;
   DebugInfoMapping* map2;

   if (debug) {
      VG_(printf)("adding DiCfSI: ");
      ML_(ppDiCfSI)(di->cfsi_exprs, base, len, cfsi_m);
   }

   
   vg_assert(len > 0);
   if (len >= 5000000)
      VG_(message)(Vg_DebugMsg,
                   "warning: DiCfSI %#lx .. %#lx is huge; length = %u (%s)\n",
                   base, base + len - 1, len, di->soname);

   vg_assert(di->fsm.have_rx_map && di->fsm.have_rw_map);
   
   map  = ML_(find_rx_mapping)(di, base, base);
   map2 = ML_(find_rx_mapping)(di, base + len - 1,
                                   base + len - 1);
   if (map == NULL)
      map = map2;
   else if (map2 == NULL)
      map2 = map;

   if (map == NULL || map != map2) {
      static Int complaints = 10;
      if (VG_(clo_trace_cfi) || complaints > 0) {
         complaints--;
         if (VG_(clo_verbosity) > 1) {
            VG_(message)(
               Vg_DebugMsg,
               "warning: DiCfSI %#lx .. %#lx outside mapped rx segments (%s)\n",
               base, 
               base + len - 1,
               di->soname
            );
         }
         if (VG_(clo_trace_cfi)) 
            ML_(ppDiCfSI)(di->cfsi_exprs, base, len, cfsi_m);
      }
      return;
   }

   if (base < map->avma) {
      if (0) VG_(printf)("XXX truncate lower\n");
      vg_assert(base + len - 1 >= map->avma);
      delta = (SSizeT)(map->avma - base);
      vg_assert(delta > 0);
      vg_assert(delta < (SSizeT)len);
      base += delta;
      len -= delta;
   }
   else
   if (base + len - 1 > map->avma + map->size - 1) {
      if (0) VG_(printf)("XXX truncate upper\n");
      vg_assert(base <= map->avma + map->size - 1);
      delta = (SSizeT)( (base + len - 1) 
                        - (map->avma + map->size - 1) );
      vg_assert(delta > 0);
      vg_assert(delta < (SSizeT)len);
      len -= delta;
   }

   

   vg_assert(len > 0);

   
   vg_assert(base >= map->avma);
   vg_assert(base + len - 1
             <= map->avma + map->size - 1);

   if (di->cfsi_used == di->cfsi_size) {
      new_sz = 2 * di->cfsi_size;
      if (new_sz == 0) new_sz = 20;
      new_tab = ML_(dinfo_zalloc)( "di.storage.addDiCfSI.1",
                                   new_sz * sizeof(DiCfSI) );
      if (di->cfsi_rd != NULL) {
         VG_(memcpy)(new_tab, di->cfsi_rd,
                     di->cfsi_used * sizeof(DiCfSI));
         ML_(dinfo_free)(di->cfsi_rd);
      }
      di->cfsi_rd = new_tab;
      di->cfsi_size = new_sz;
      if (di->cfsi_m_pool == NULL)
         di->cfsi_m_pool = VG_(newDedupPA)(1000 * sizeof(DiCfSI_m),
                                           vg_alignof(DiCfSI_m),
                                           ML_(dinfo_zalloc),
                                           "di.storage.DiCfSI_m_pool",
                                           ML_(dinfo_free));
   }

   di->cfsi_rd[di->cfsi_used].base = base;
   di->cfsi_rd[di->cfsi_used].len  = len;
   di->cfsi_rd[di->cfsi_used].cfsi_m_ix 
      = VG_(allocFixedEltDedupPA)(di->cfsi_m_pool,
                                  sizeof(DiCfSI_m),
                                  cfsi_m);
   di->cfsi_used++;
   vg_assert(di->cfsi_used <= di->cfsi_size);
}


Int ML_(CfiExpr_Undef)( XArray* dst )
{
   CfiExpr e;
   VG_(memset)( &e, 0, sizeof(e) );
   e.tag = Cex_Undef;
   return (Int)VG_(addToXA)( dst, &e );
}
Int ML_(CfiExpr_Deref)( XArray* dst, Int ixAddr )
{
   CfiExpr e;
   VG_(memset)( &e, 0, sizeof(e) );
   e.tag = Cex_Deref;
   e.Cex.Deref.ixAddr = ixAddr;
   return (Int)VG_(addToXA)( dst, &e );
}
Int ML_(CfiExpr_Const)( XArray* dst, UWord con )
{
   CfiExpr e;
   VG_(memset)( &e, 0, sizeof(e) );
   e.tag = Cex_Const;
   e.Cex.Const.con = con;
   return (Int)VG_(addToXA)( dst, &e );
}
Int ML_(CfiExpr_Unop)( XArray* dst, CfiUnop op, Int ix )
{
   CfiExpr e;
   VG_(memset)( &e, 0, sizeof(e) );
   e.tag = Cex_Unop;
   e.Cex.Unop.op  = op;
   e.Cex.Unop.ix = ix;
   return (Int)VG_(addToXA)( dst, &e );
}
Int ML_(CfiExpr_Binop)( XArray* dst, CfiBinop op, Int ixL, Int ixR )
{
   CfiExpr e;
   VG_(memset)( &e, 0, sizeof(e) );
   e.tag = Cex_Binop;
   e.Cex.Binop.op  = op;
   e.Cex.Binop.ixL = ixL;
   e.Cex.Binop.ixR = ixR;
   return (Int)VG_(addToXA)( dst, &e );
}
Int ML_(CfiExpr_CfiReg)( XArray* dst, CfiReg reg )
{
   CfiExpr e;
   VG_(memset)( &e, 0, sizeof(e) );
   e.tag = Cex_CfiReg;
   e.Cex.CfiReg.reg = reg;
   return (Int)VG_(addToXA)( dst, &e );
}
Int ML_(CfiExpr_DwReg)( XArray* dst, Int reg )
{
   CfiExpr e;
   VG_(memset)( &e, 0, sizeof(e) );
   e.tag = Cex_DwReg;
   e.Cex.DwReg.reg = reg;
   return (Int)VG_(addToXA)( dst, &e );
}

static void ppCfiUnop ( CfiUnop op ) 
{
   switch (op) {
      case Cunop_Abs: VG_(printf)("abs"); break;
      case Cunop_Neg: VG_(printf)("-"); break;
      case Cunop_Not: VG_(printf)("~"); break;
      default:        vg_assert(0);
   }
}

static void ppCfiBinop ( CfiBinop op ) 
{
   switch (op) {
      case Cbinop_Add: VG_(printf)("+"); break;
      case Cbinop_Sub: VG_(printf)("-"); break;
      case Cbinop_And: VG_(printf)("&"); break;
      case Cbinop_Mul: VG_(printf)("*"); break;
      case Cbinop_Shl: VG_(printf)("<<"); break;
      case Cbinop_Shr: VG_(printf)(">>"); break;
      case Cbinop_Eq:  VG_(printf)("=="); break;
      case Cbinop_Ge:  VG_(printf)(">="); break;
      case Cbinop_Gt:  VG_(printf)(">"); break;
      case Cbinop_Le:  VG_(printf)("<="); break;
      case Cbinop_Lt:  VG_(printf)("<"); break;
      case Cbinop_Ne:  VG_(printf)("!="); break;
      default:         vg_assert(0);
   }
}

static void ppCfiReg ( CfiReg reg )
{
   switch (reg) {
      case Creg_INVALID:   VG_(printf)("Creg_INVALID"); break;
      case Creg_IA_SP:     VG_(printf)("xSP"); break;
      case Creg_IA_BP:     VG_(printf)("xBP"); break;
      case Creg_IA_IP:     VG_(printf)("xIP"); break;
      case Creg_ARM_R13:   VG_(printf)("R13"); break;
      case Creg_ARM_R12:   VG_(printf)("R12"); break;
      case Creg_ARM_R15:   VG_(printf)("R15"); break;
      case Creg_ARM_R14:   VG_(printf)("R14"); break;
      case Creg_ARM_R7:    VG_(printf)("R7");  break;
      case Creg_ARM64_X30: VG_(printf)("X30"); break;
      case Creg_MIPS_RA:   VG_(printf)("RA"); break;
      case Creg_S390_IA:   VG_(printf)("IA"); break;
      case Creg_S390_SP:   VG_(printf)("SP"); break;
      case Creg_S390_FP:   VG_(printf)("FP"); break;
      case Creg_S390_LR:   VG_(printf)("LR"); break;
      case Creg_TILEGX_IP: VG_(printf)("PC");  break;
      case Creg_TILEGX_SP: VG_(printf)("SP");  break;
      case Creg_TILEGX_BP: VG_(printf)("BP");  break;
      case Creg_TILEGX_LR: VG_(printf)("R55"); break;
      default: vg_assert(0);
   }
}

void ML_(ppCfiExpr)( const XArray* src, Int ix )
{
   const CfiExpr* e = VG_(indexXA)( src, ix );
   switch (e->tag) {
      case Cex_Undef: 
         VG_(printf)("Undef"); 
         break;
      case Cex_Deref: 
         VG_(printf)("*("); 
         ML_(ppCfiExpr)(src, e->Cex.Deref.ixAddr);
         VG_(printf)(")"); 
         break;
      case Cex_Const: 
         VG_(printf)("0x%lx", e->Cex.Const.con); 
         break;
      case Cex_Unop: 
         ppCfiUnop(e->Cex.Unop.op);
         VG_(printf)("(");
         ML_(ppCfiExpr)(src, e->Cex.Unop.ix);
         VG_(printf)(")");
         break;
      case Cex_Binop: 
         VG_(printf)("(");
         ML_(ppCfiExpr)(src, e->Cex.Binop.ixL);
         VG_(printf)(")");
         ppCfiBinop(e->Cex.Binop.op);
         VG_(printf)("(");
         ML_(ppCfiExpr)(src, e->Cex.Binop.ixR);
         VG_(printf)(")");
         break;
      case Cex_CfiReg:
         ppCfiReg(e->Cex.CfiReg.reg);
         break;
      case Cex_DwReg:
         VG_(printf)("dwr%d", e->Cex.DwReg.reg);
         break;
      default: 
         VG_(core_panic)("ML_(ppCfiExpr)"); 
         
         break;
   }
}


Word ML_(cmp_for_DiAddrRange_range) ( const void* keyV,
                                      const void* elemV ) {
   const Addr* key = (const Addr*)keyV;
   const DiAddrRange* elem = (const DiAddrRange*)elemV;
   if (0)
      VG_(printf)("cmp_for_DiAddrRange_range: %#lx vs %#lx\n",
                  *key, elem->aMin);
   if ((*key) < elem->aMin) return -1;
   if ((*key) > elem->aMax) return 1;
   return 0;
}

static
void show_scope ( OSet*  scope, const HChar* who )
{
   DiAddrRange* range;
   VG_(printf)("Scope \"%s\" = {\n", who);
   VG_(OSetGen_ResetIter)( scope );
   while (True) {
      range = VG_(OSetGen_Next)( scope );
      if (!range) break;
      VG_(printf)("   %#lx .. %#lx: %lu vars\n", range->aMin, range->aMax,
                  range->vars ? VG_(sizeXA)(range->vars) : 0);
   }
   VG_(printf)("}\n");
}

static void add_var_to_arange ( 
               OSet*  scope,
               Addr aMin, 
               Addr aMax,
               DiVariable* var
            )
{
   DiAddrRange *first, *last, *range;
   DiAddrRange *xxRangep, *xxFirst, *xxLast;
   UWord       xxIters;

   vg_assert(aMin <= aMax);

   if (0) VG_(printf)("add_var_to_arange: %#lx .. %#lx\n", aMin, aMax);
   if (0) show_scope( scope, "add_var_to_arange(1)" );

   first = VG_(OSetGen_Lookup)( scope, &aMin );
   vg_assert(first);
   vg_assert(first->aMin <= first->aMax);
   vg_assert(first->aMin <= aMin && aMin <= first->aMax);

   if (first->aMin == aMin && first->aMax == aMax) {
      vg_assert(first->vars);
      VG_(addToXA)( first->vars, var );
      return;
   }

   if (first->aMin < aMin) {
      DiAddrRange* nyu;
      
      
      Addr tmp = first->aMax;
      first->aMax = aMin-1;
      vg_assert(first->aMin <= first->aMax);
      
      nyu = VG_(OSetGen_AllocNode)( scope, sizeof(DiAddrRange) );
      nyu->aMin = aMin;
      nyu->aMax = tmp;
      vg_assert(nyu->aMin <= nyu->aMax);
      
      vg_assert(first->vars);
      nyu->vars = VG_(cloneXA)( "di.storage.avta.1", first->vars );
      VG_(OSetGen_Insert)( scope, nyu );
      first = nyu;
   }

   vg_assert(first->aMin == aMin);

   last = VG_(OSetGen_Lookup)( scope, &aMax );
   vg_assert(last->aMin <= last->aMax);
   vg_assert(last->aMin <= aMax && aMax <= last->aMax);

   if (aMax < last->aMax) {
      DiAddrRange* nyu;
      
      
      Addr tmp = last->aMin;
      last->aMin = aMax+1;
      vg_assert(last->aMin <= last->aMax);
      
      nyu = VG_(OSetGen_AllocNode)( scope, sizeof(DiAddrRange) );
      nyu->aMin = tmp;
      nyu->aMax = aMax;
      vg_assert(nyu->aMin <= nyu->aMax);
      
      vg_assert(last->vars);
      nyu->vars = VG_(cloneXA)( "di.storage.avta.2", last->vars );
      VG_(OSetGen_Insert)( scope, nyu );
      last = nyu;
   }

   vg_assert(aMax == last->aMax);

   xxFirst = (DiAddrRange*)VG_(OSetGen_Lookup)(scope, &aMin);
   xxLast  = (DiAddrRange*)VG_(OSetGen_Lookup)(scope, &aMax);
   vg_assert(xxFirst);
   vg_assert(xxLast);
   vg_assert(xxFirst->aMin == aMin);
   vg_assert(xxLast->aMax == aMax);
   if (xxFirst != xxLast)
      vg_assert(xxFirst->aMax < xxLast->aMin);

   if (0) {
      static UWord ctr = 0;
      ctr++;
      VG_(printf)("ctr = %lu\n", ctr);
      if (ctr >= 33263) show_scope( scope, "add_var_to_arange(2)" );
   }

   xxIters = 0;
   range = xxRangep = NULL;
   VG_(OSetGen_ResetIterAt)( scope, &aMin );
   while (True) {
      xxRangep = range;
      range    = VG_(OSetGen_Next)( scope );
      if (!range) break;
      if (range->aMin > aMax) break;
      xxIters++;
      if (0) VG_(printf)("have range %#lx %#lx\n",
                         range->aMin, range->aMax);

      
      if (!xxRangep) {
         
         vg_assert(range->aMin == aMin);
      } else {
         vg_assert(xxRangep->aMax + 1 == range->aMin);
      }

      vg_assert(range->vars);
      VG_(addToXA)( range->vars, var );
   }
   
   vg_assert(xxIters >= 1);
   if (xxIters == 1) vg_assert(xxFirst == xxLast);
   if (xxFirst == xxLast) vg_assert(xxIters == 1);
   vg_assert(xxRangep);
   vg_assert(xxRangep->aMax == aMax);
   vg_assert(xxRangep == xxLast);
}


void ML_(addVar)( struct _DebugInfo* di,
                  Int    level,
                  Addr   aMin,
                  Addr   aMax,
                  const  HChar* name, 
                  UWord  typeR, 
                  const GExpr* gexpr,
                  const GExpr* fbGX,
                  UInt   fndn_ix, 
                  Int    lineNo, 
                  Bool   show )
{
   OSet*  scope;
   DiVariable var;
   Bool       all;
   TyEnt*     ent;
   MaybeULong mul;
   const HChar* badness;

   vg_assert(di && di->admin_tyents);

   if (0) {
      VG_(printf)("  ML_(addVar): level %d  %#lx-%#lx  %s :: ",
                  level, aMin, aMax, name );
      ML_(pp_TyEnt_C_ishly)( di->admin_tyents, typeR );
      VG_(printf)("\n  Var=");
      ML_(pp_GX)(gexpr);
      VG_(printf)("\n");
      if (fbGX) {
         VG_(printf)("  FrB=");
         ML_(pp_GX)( fbGX );
         VG_(printf)("\n");
      } else {
         VG_(printf)("  FrB=none\n");
      }
      VG_(printf)("\n");
   }

   vg_assert(level >= 0);
   vg_assert(aMin <= aMax);
   vg_assert(name);
   vg_assert(gexpr);

   ent = ML_(TyEnts__index_by_cuOff)( di->admin_tyents, NULL, typeR);
   vg_assert(ent);
   vg_assert(ML_(TyEnt__is_type)(ent));

   vg_assert(di->fsm.have_rx_map && di->fsm.have_rw_map);
   if (level > 0 && ML_(find_rx_mapping)(di, aMin, aMax) == NULL) {
      if (VG_(clo_verbosity) >= 0) {
         VG_(message)(Vg_DebugMsg, 
            "warning: addVar: in range %#lx .. %#lx outside "
            "all rx mapped areas (%s)\n",
            aMin, aMax, name
         );
      }
      return;
   }

   mul = ML_(sizeOfType)(di->admin_tyents, typeR);

   badness = NULL;
   if (mul.b != True) 
      badness = "unknown size";
   else if (mul.ul == 0)
      badness = "zero size   ";
   else if (sizeof(void*) == 4 && mul.ul >= (1ULL<<32))
      badness = "implausibly large";

   if (badness) {
      static Int complaints = 10;
      if (VG_(clo_verbosity) >= 2 && complaints > 0) {
         VG_(message)(Vg_DebugMsg, "warning: addVar: %s (%s)\n",
                                   badness, name );
         complaints--;
      }
      return;
   }

   if (!di->varinfo) {
      di->varinfo = VG_(newXA)( ML_(dinfo_zalloc), 
                                "di.storage.addVar.1",
                                ML_(dinfo_free),
                                sizeof(OSet*) );
   }

   vg_assert(level < 256); 
   
   while ( VG_(sizeXA)(di->varinfo) <= level ) {
      DiAddrRange* nyu;
      scope = VG_(OSetGen_Create)( offsetof(DiAddrRange,aMin), 
                                   ML_(cmp_for_DiAddrRange_range),
                                   ML_(dinfo_zalloc), "di.storage.addVar.2",
                                   ML_(dinfo_free) );
      if (0) VG_(printf)("create: scope = %p, adding at %ld\n",
                         scope, VG_(sizeXA)(di->varinfo));
      VG_(addToXA)( di->varinfo, &scope );
      nyu = VG_(OSetGen_AllocNode)( scope, sizeof(DiAddrRange) );
      nyu->aMin = (Addr)0;
      nyu->aMax = ~(Addr)0;
      nyu->vars = VG_(newXA)( ML_(dinfo_zalloc), "di.storage.addVar.3",
                              ML_(dinfo_free),
                              sizeof(DiVariable) );
      VG_(OSetGen_Insert)( scope, nyu );
   }

   vg_assert( VG_(sizeXA)(di->varinfo) > level );
   scope = *(OSet**)VG_(indexXA)( di->varinfo, level );
   vg_assert(scope);

   var.name     = name;
   var.typeR    = typeR;
   var.gexpr    = gexpr;
   var.fbGX     = fbGX;
   var.fndn_ix  = fndn_ix;
   var.lineNo   = lineNo;

   all = aMin == (Addr)0 && aMax == ~(Addr)0;
   vg_assert(level == 0 ? all : !all);

   add_var_to_arange( scope, aMin, aMax, &var );
}


static void canonicaliseVarInfo ( struct _DebugInfo* di )
{
   Word i, nInThisScope;

   if (!di->varinfo)
      return;

   for (i = 0; i < VG_(sizeXA)(di->varinfo); i++) {

      DiAddrRange *range, *rangep;
      OSet* scope = *(OSet**)VG_(indexXA)(di->varinfo, i);
      if (!scope) continue;

      
      if (i == 0) {
         Addr zero = 0;
         vg_assert(VG_(OSetGen_Size)( scope ) == 1);
         range = VG_(OSetGen_Lookup)( scope, &zero );
         vg_assert(range);
         vg_assert(range->aMin == (Addr)0);
         vg_assert(range->aMax == ~(Addr)0);
         continue;
      }

      
      
      nInThisScope = 0;
      rangep = NULL;
      VG_(OSetGen_ResetIter)(scope);
      while (True) {
         range = VG_(OSetGen_Next)(scope);
         if (!range) {
           vg_assert(rangep);
           vg_assert(rangep->aMax == ~(Addr)0);
           break;
         }

         vg_assert(range->aMin <= range->aMax);
         vg_assert(range->vars);

         if (!rangep) {
           
           vg_assert(range->aMin == 0);
         } else {
           vg_assert(rangep->aMax + 1 == range->aMin);
         }

         rangep = range;
         nInThisScope++;
      } 


      vg_assert(nInThisScope > 0);
      if (nInThisScope == 1) {
         Addr zero = 0;
         vg_assert(VG_(OSetGen_Size)( scope ) == 1);
         range = VG_(OSetGen_Lookup)( scope, &zero );
         vg_assert(range);
         vg_assert(range->aMin == (Addr)0);
         vg_assert(range->aMax == ~(Addr)0);
         vg_assert(range->vars);
         vg_assert(VG_(sizeXA)(range->vars) == 0);
      }

   } 
}



static Int compare_DiSym ( const void* va, const void* vb ) 
{
   const DiSym* a = va;
   const DiSym* b = vb;
   if (a->avmas.main < b->avmas.main) return -1;
   if (a->avmas.main > b->avmas.main) return  1;
   return 0;
}


static
Bool preferName ( const DebugInfo* di,
                  const HChar* a_name, const HChar* b_name,
                  Addr sym_avma )
{
   Word cmp;
   Word vlena, vlenb;		
   const HChar *vpa, *vpb;

   Bool preferA = False;
   Bool preferB = False;

   vg_assert(a_name);
   vg_assert(b_name);
   
   
   

   vlena = VG_(strlen)(a_name);
   vlenb = VG_(strlen)(b_name);

#  if defined(VGO_linux)
#    define VERSION_CHAR '@'
#  elif defined(VGO_darwin)
#    define VERSION_CHAR '$'
#  else
#    error Unknown OS
#  endif

   vpa = VG_(strchr)(a_name, VERSION_CHAR);
   vpb = VG_(strchr)(b_name, VERSION_CHAR);

#  undef VERSION_CHAR

   if (vpa)
      vlena = vpa - a_name;
   if (vpb)
      vlenb = vpb - b_name;

   
   if (0==VG_(strncmp)(a_name, "MPI_", 4)
       && 0==VG_(strncmp)(b_name, "PMPI_", 5)
       && 0==VG_(strcmp)(a_name, 1+b_name)) {
      preferB = True; goto out;
   } 
   if (0==VG_(strncmp)(b_name, "MPI_", 4)
       && 0==VG_(strncmp)(a_name, "PMPI_", 5)
       && 0==VG_(strcmp)(b_name, 1+a_name)) {
      preferA = True; goto out;
   }

   
   if (vlena  &&  !vlenb) {
      preferA = True; goto out;
   }
   if (vlenb  &&  !vlena) {
      preferB = True; goto out;
   }

   
   {
      Bool blankA = True;
      Bool blankB = True;
      const HChar *s;
      s = a_name;
      while (*s) {
         if (!VG_(isspace)(*s++)) {
            blankA = False;
            break;
         }
      }
      s = b_name;
      while (*s) {
         if (!VG_(isspace)(*s++)) {
            blankB = False;
            break;
         }
      }

      if (!blankA  &&  blankB) {
         preferA = True; goto out;
      }
      if (!blankB  &&  blankA) {
         preferB = True; goto out;
      }
   }

   
   if (vlena < vlenb) {
      preferA = True; goto out;
   } 
   if (vlenb < vlena) {
      preferB = True; goto out;
   }

   
   if (vpa && !vpb) {
      preferA = True; goto out;
   }
   if (vpb && !vpa) {
      preferB = True; goto out;
   }

   cmp = VG_(strcmp)(a_name, b_name);
   if (cmp < 0) {
      preferA = True; goto out;
   }
   if (cmp > 0) {
      preferB = True; goto out;
   }

   

   return a_name <= b_name  ? True  : False;

   
   vg_assert(0);
  out:
   if (preferA && !preferB) {
      TRACE_SYMTAB("sym at %#lx: prefer '%s' to '%s'\n",
                   sym_avma, a_name, b_name );
      return True;
   }
   if (preferB && !preferA) {
      TRACE_SYMTAB("sym at %#lx: prefer '%s' to '%s'\n",
                   sym_avma, b_name, a_name );
      return False;
   }
   
   vg_assert(0);
}


static
void add_DiSym_names_to_from ( const DebugInfo* di, DiSym* to,
                               const DiSym* from )
{
   vg_assert(to->pri_name);
   vg_assert(from->pri_name);
   const HChar** to_sec   = to->sec_names;
   const HChar** from_sec = from->sec_names;
   Word n_new_sec = 1;
   if (from_sec) {
      while (*from_sec) {
         n_new_sec++;
         from_sec++;
      }
   }
   if (to_sec) {
      while (*to_sec) {
         n_new_sec++;
         to_sec++;
      }
   }
   if (0)
      TRACE_SYMTAB("merge: -> %ld\n", n_new_sec);
   const HChar** new_sec = ML_(dinfo_zalloc)( "di.storage.aDntf.1",
                                              (n_new_sec+1) * sizeof(HChar*) );
   from_sec = from->sec_names;
   to_sec   = to->sec_names;
   Word i = 0;
   if (to_sec) {
      while (*to_sec) {
         new_sec[i++] = *to_sec;
         to_sec++;
      }
   }
   new_sec[i++] = from->pri_name;
   if (from_sec) {
      while (*from_sec) {
         new_sec[i++] = *from_sec;
         from_sec++;
      }
   }
   vg_assert(i == n_new_sec);
   vg_assert(new_sec[i] == NULL);
   
   if (to->sec_names) {
      ML_(dinfo_free)(to->sec_names);
   }
   to->sec_names = new_sec;
}


static void canonicaliseSymtab ( struct _DebugInfo* di )
{
   Word  i, j, n_truncated;
   Addr  sta1, sta2, end1, end2, toc1, toc2;
   const HChar *pri1, *pri2, **sec1, **sec2;
   Bool  ist1, ist2, isf1, isf2;

#  define SWAP(ty,aa,bb) \
      do { ty tt = (aa); (aa) = (bb); (bb) = tt; } while (0)

   if (di->symtab_used == 0)
      return;

   
   for (i = 0; i < di->symtab_used; i++) {
      DiSym* sym = &di->symtab[i];
      vg_assert(sym->pri_name);
      vg_assert(!sym->sec_names);
   }

   
   VG_(ssort)(di->symtab, di->symtab_used, 
                          sizeof(*di->symtab), compare_DiSym);

  cleanup_more:
 
   
   while (1) {
      Word r, w, n_merged;
      n_merged = 0;
      w = 0;
      for (r = 1; r < di->symtab_used; r++) {
         vg_assert(w < r);
         if (   di->symtab[w].avmas.main == di->symtab[r].avmas.main
             && di->symtab[w].size       == di->symtab[r].size
             && !!di->symtab[w].isText   == !!di->symtab[r].isText) {
            
            n_merged++;
            if (di->symtab[r].sec_names 
                || (0 != VG_(strcmp)(di->symtab[r].pri_name,
                                     di->symtab[w].pri_name))) {
               add_DiSym_names_to_from(di, &di->symtab[w], &di->symtab[r]);
            }
            
            di->symtab[w].isIFunc = di->symtab[w].isIFunc || di->symtab[r].isIFunc;
            
            di->symtab[r].pri_name = NULL;
            if (di->symtab[r].sec_names) {
               ML_(dinfo_free)(di->symtab[r].sec_names);
               di->symtab[r].sec_names = NULL;
            }
            VG_(memset)(&di->symtab[r], 0, sizeof(DiSym));
         } else {
            w = r;
         }
      }

      for (r = 1; r < di->symtab_used; r++) {
         if (di->symtab[r-1].pri_name == NULL) continue;
         if (di->symtab[r-0].pri_name == NULL) continue;
         
         if (di->symtab[r-1].avmas.main != di->symtab[r-0].avmas.main) continue;
         if (di->symtab[r-1].size       != di->symtab[r-0].size) continue;
         if (di->symtab[r-1].isText && di->symtab[r-0].isText) vg_assert(0);
         if (!di->symtab[r-1].isText && !di->symtab[r-0].isText) vg_assert(0);
         Word to_zap  = di->symtab[r-1].isText ? (r-0) : (r-1);
         Word to_keep = di->symtab[r-1].isText ? (r-1) : (r-0);
         vg_assert(!di->symtab[to_zap].isText);
         vg_assert(di->symtab[to_keep].isText);
         if (di->symtab[to_zap].sec_names 
             || (0 != VG_(strcmp)(di->symtab[to_zap].pri_name,
                                  di->symtab[to_keep].pri_name))) {
            add_DiSym_names_to_from(di, &di->symtab[to_keep],
                                        &di->symtab[to_zap]);
         }
         di->symtab[to_zap].pri_name = NULL;
         if (di->symtab[to_zap].sec_names) {
            ML_(dinfo_free)(di->symtab[to_zap].sec_names);
            di->symtab[to_zap].sec_names = NULL;
         }
         VG_(memset)(&di->symtab[to_zap], 0, sizeof(DiSym));
         n_merged++;
      }

      TRACE_SYMTAB( "canonicaliseSymtab: %ld symbols merged\n", n_merged);
      if (n_merged == 0)
         break;
      
      w = 0;
      for (r = 0; r < di->symtab_used; r++) {
         vg_assert(w <= r);
         if (di->symtab[r].pri_name == NULL)
            continue;
         if (w < r) {
            di->symtab[w] = di->symtab[r];
         }
         w++;
      }
      vg_assert(w + n_merged == di->symtab_used);
      di->symtab_used = w;
   } 
   

   
   n_truncated = 0;

   for (i = 0; i < ((Word)di->symtab_used) -1; i++) {

      vg_assert(di->symtab[i].avmas.main <= di->symtab[i+1].avmas.main);

       
      if (di->symtab[i].avmas.main + di->symtab[i].size 
          <= di->symtab[i+1].avmas.main)
         continue;

      
      if (di->trace_symtab) {
         VG_(printf)("overlapping address ranges in symbol table\n\t");
         ML_(ppSym)( i, &di->symtab[i] );
         VG_(printf)("\t");
         ML_(ppSym)( i+1, &di->symtab[i+1] );
         VG_(printf)("\n");
      }

      
      sta1 = di->symtab[i].avmas.main;
      end1 = sta1 + di->symtab[i].size - 1;
      toc1 = GET_TOCPTR_AVMA(di->symtab[i].avmas);
      
      pri1 = di->symtab[i].pri_name;
      sec1 = di->symtab[i].sec_names;
      ist1 = di->symtab[i].isText;
      isf1 = di->symtab[i].isIFunc;

      sta2 = di->symtab[i+1].avmas.main;
      end2 = sta2 + di->symtab[i+1].size - 1;
      toc2 = GET_TOCPTR_AVMA(di->symtab[i+1].avmas);
      
      pri2 = di->symtab[i+1].pri_name;
      sec2 = di->symtab[i+1].sec_names;
      ist2 = di->symtab[i+1].isText;
      isf2 = di->symtab[i+1].isIFunc;

      if (sta1 < sta2) {
         end1 = sta2 - 1;
      } else {
         vg_assert(sta1 == sta2);
         if (end1 > end2) { 
            sta1 = end2 + 1;
            SWAP(Addr,sta1,sta2); SWAP(Addr,end1,end2); SWAP(Addr,toc1,toc2);
            SWAP(const HChar*,pri1,pri2); SWAP(const HChar**,sec1,sec2);
            SWAP(Bool,ist1,ist2); SWAP(Bool,isf1,isf2);
         } else 
         if (end1 < end2) {
            sta2 = end1 + 1;
         } else {
	 }
      }
      di->symtab[i].avmas.main = sta1;
      di->symtab[i].size       = end1 - sta1 + 1;
      SET_TOCPTR_AVMA(di->symtab[i].avmas, toc1);
      
      di->symtab[i].pri_name  = pri1;
      di->symtab[i].sec_names = sec1;
      di->symtab[i].isText    = ist1;
      di->symtab[i].isIFunc   = isf1;

      di->symtab[i+1].avmas.main = sta2;
      di->symtab[i+1].size       = end2 - sta2 + 1;
      SET_TOCPTR_AVMA(di->symtab[i+1].avmas, toc2);
      
      di->symtab[i+1].pri_name  = pri2;
      di->symtab[i+1].sec_names = sec2;
      di->symtab[i+1].isText    = ist2;
      di->symtab[i+1].isIFunc   = isf2;

      vg_assert(sta1 <= sta2);
      vg_assert(di->symtab[i].size > 0);
      vg_assert(di->symtab[i+1].size > 0);
      j = i+1;
      while (j < ((Word)di->symtab_used)-1 
             && di->symtab[j].avmas.main > di->symtab[j+1].avmas.main) {
         SWAP(DiSym,di->symtab[j],di->symtab[j+1]);
         j++;
      }
      n_truncated++;
   }
   

   if (n_truncated > 0) goto cleanup_more;

   
   for (i = 0; i < ((Word)di->symtab_used)-1; i++) {
      
      vg_assert(di->symtab[i].size > 0);
      
      vg_assert(di->symtab[i].avmas.main < di->symtab[i+1].avmas.main);
      
      vg_assert(di->symtab[i].avmas.main + di->symtab[i].size - 1
                < di->symtab[i+1].avmas.main);
      
      vg_assert(di->symtab[i].pri_name);
      if (di->symtab[i].sec_names) {
         vg_assert(di->symtab[i].sec_names[0]);
      }
   }

   for (i = 0; i < ((Word)di->symtab_used)-1; i++) {
      DiSym*  sym = &di->symtab[i];
      const HChar** sec = sym->sec_names;
      if (!sec)
         continue;
      Word n_tmp = 1;
      while (*sec) { n_tmp++; sec++; }
      j = 0;
      const HChar** tmp = ML_(dinfo_zalloc)( "di.storage.cS.1",
                                             (n_tmp+1) * sizeof(HChar*) );
      tmp[j++] = sym->pri_name;
      sec = sym->sec_names;
      while (*sec) { tmp[j++] = *sec; sec++; }
      vg_assert(j == n_tmp);
      vg_assert(tmp[n_tmp] == NULL); 
      
      Word best = 0;
      for (j = 1; j < n_tmp; j++) {
         if (preferName(di, tmp[best], tmp[j], di->symtab[i].avmas.main)) {
            
         } else {
            best = j;
         }
      }
      vg_assert(best >= 0 && best < n_tmp);
      
      sym->pri_name = tmp[best];
      const HChar** cursor = sym->sec_names;
      for (j = 0; j < n_tmp; j++) {
         if (j == best)
            continue;
         *cursor = tmp[j];
         cursor++;
      }
      vg_assert(*cursor == NULL);
      ML_(dinfo_free)( tmp );
   }

#  undef SWAP
}


static DiLoc* sorting_loctab = NULL;
static Int compare_DiLoc_via_ix ( const void* va, const void* vb ) 
{
   const DiLoc* a = &sorting_loctab[*(const UInt*)va];
   const DiLoc* b = &sorting_loctab[*(const UInt*)vb];
   if (a->addr < b->addr) return -1;
   if (a->addr > b->addr) return  1;
   return 0;
}
static void sort_loctab_and_loctab_fndn_ix (struct _DebugInfo* di )
{
   UInt *sort_ix = ML_(dinfo_zalloc)("di.storage.six",
                                     di->loctab_used*sizeof(UInt));
   Word i, j, k;

   for (i = 0; i < di->loctab_used; i++) sort_ix[i] = i;
   sorting_loctab = di->loctab;
   VG_(ssort)(sort_ix, di->loctab_used, 
              sizeof(*sort_ix), compare_DiLoc_via_ix);
   sorting_loctab = NULL;

   
   for (i=0; i < di->loctab_used; i++) {
      DiLoc tmp_diloc;
      UInt  tmp_fndn_ix;

      if (i == sort_ix[i])
         continue; 

      tmp_diloc = di->loctab[i];
      tmp_fndn_ix = ML_(fndn_ix)(di, i);
      j = i;
      for (;;) {
         k = sort_ix[j];
         sort_ix[j] = j;
         if (k == i)
            break;
         di->loctab[j] = di->loctab[k];
         set_fndn_ix (di, j, ML_(fndn_ix)(di, k));
         j = k;
      }
      di->loctab[j] = tmp_diloc;
      set_fndn_ix (di, j, tmp_fndn_ix);
   }
   ML_(dinfo_free)(sort_ix);
}

static void canonicaliseLoctab ( struct _DebugInfo* di )
{
   Word i, j;

   if (di->loctab_used == 0)
      return;

   
   sort_loctab_and_loctab_fndn_ix (di);

   
   for (i = 0; i < ((Word)di->loctab_used)-1; i++) {
      vg_assert(di->loctab[i].size < 10000);
      if (di->loctab[i].addr + di->loctab[i].size > di->loctab[i+1].addr) {
         Int new_size = di->loctab[i+1].addr - di->loctab[i].addr;
         if (new_size < 0) {
            di->loctab[i].size = 0;
         } else
         if (new_size > MAX_LOC_SIZE) {
           di->loctab[i].size = MAX_LOC_SIZE;
         } else {
           di->loctab[i].size = (UShort)new_size;
         }
      }
   }

   j = 0;
   for (i = 0; i < (Word)di->loctab_used; i++) {
      if (di->loctab[i].size > 0) {
         if (j != i) {
            di->loctab[j] = di->loctab[i];
            set_fndn_ix(di, j, ML_(fndn_ix)(di, i));
         }
         j++;
      }
   }
   di->loctab_used = j;

   
   for (i = 0; i < ((Word)di->loctab_used)-1; i++) {
      if (0)
         VG_(printf)("%lu  0x%p  lno:%d sz:%d fndn_ix:%d  i+1 0x%p\n", 
                     i,
                     (void*)di->loctab[i].addr,
                     di->loctab[i].lineno, 
                     di->loctab[i].size,
                     ML_(fndn_ix)(di, i),
                     (void*)di->loctab[i+1].addr);
      
      
      vg_assert(di->loctab[i].size > 0);
      
      vg_assert(di->loctab[i].addr < di->loctab[i+1].addr);
      
      vg_assert(di->loctab[i].addr + di->loctab[i].size - 1
                < di->loctab[i+1].addr);
   }

   
   shrinkLocTab(di);
}

static Int compare_DiInlLoc ( const void* va, const void* vb ) 
{
   const DiInlLoc* a = va;
   const DiInlLoc* b = vb;
   if (a->addr_lo < b->addr_lo) return -1;
   if (a->addr_lo > b->addr_lo) return  1;
   return 0;
}

static void canonicaliseInltab ( struct _DebugInfo* di )
{
   Word i;

   if (di->inltab_used == 0)
      return;

   
   VG_(ssort)(di->inltab, di->inltab_used, 
                          sizeof(*di->inltab), compare_DiInlLoc);

   
   for (i = 0; i < ((Word)di->inltab_used)-1; i++) {
      
      vg_assert(di->inltab[i].addr_lo < di->inltab[i].addr_hi);
      
      vg_assert(di->inltab[i].addr_lo <= di->inltab[i+1].addr_lo);
   }

   
   shrinkInlTab(di);
}


static Int compare_DiCfSI ( const void* va, const void* vb )
{
   const DiCfSI* a = va;
   const DiCfSI* b = vb;
   if (a->base < b->base) return -1;
   if (a->base > b->base) return  1;
   return 0;
}

static void get_cfsi_rd_stats ( const DebugInfo* di,
                                UWord *n_mergeables, UWord *n_holes )
{
   Word i;

   *n_mergeables = 0;
   *n_holes = 0;

   vg_assert (di->cfsi_used == 0 || di->cfsi_rd);
   for (i = 1; i < (Word)di->cfsi_used; i++) {
      Addr here_min = di->cfsi_rd[i].base;
      Addr prev_max = di->cfsi_rd[i-1].base + di->cfsi_rd[i-1].len - 1;
      Addr sep = here_min - prev_max;
      if (sep > 1)
         (*n_holes)++;
      if (sep == 1 && di->cfsi_rd[i-1].cfsi_m_ix == di->cfsi_rd[i].cfsi_m_ix)
         (*n_mergeables)++;
   }
}

void ML_(canonicaliseCFI) ( struct _DebugInfo* di )
{
   Word  i, j;
   const Addr minAvma = 0;
   const Addr maxAvma = ~minAvma;

   if (di->cfsi_rd == NULL) {
      vg_assert(di->cfsi_used == 0);
      vg_assert(di->cfsi_size == 0);
      vg_assert(di->cfsi_m_pool == NULL);
   } else {
      vg_assert(di->cfsi_size != 0);
      vg_assert(di->cfsi_m_pool != NULL);
   }

   di->cfsi_minavma = maxAvma; 
   di->cfsi_maxavma = minAvma;
   for (i = 0; i < (Word)di->cfsi_used; i++) {
      Addr here_min = di->cfsi_rd[i].base;
      Addr here_max = di->cfsi_rd[i].base + di->cfsi_rd[i].len - 1;
      if (here_min < di->cfsi_minavma)
         di->cfsi_minavma = here_min;
      if (here_max > di->cfsi_maxavma)
         di->cfsi_maxavma = here_max;
   }

   if (di->trace_cfi)
      VG_(printf)("canonicaliseCfiSI: %ld entries, %#lx .. %#lx\n",
                  di->cfsi_used,
	          di->cfsi_minavma, di->cfsi_maxavma);

   
   VG_(ssort)(di->cfsi_rd, di->cfsi_used, sizeof(*di->cfsi_rd), compare_DiCfSI);

   
   for (i = 0; i < (Word)di->cfsi_used-1; i++) {
      if (di->cfsi_rd[i].base + di->cfsi_rd[i].len > di->cfsi_rd[i+1].base) {
         Word new_len = di->cfsi_rd[i+1].base - di->cfsi_rd[i].base;
         
         vg_assert(new_len >= 0);
	 vg_assert(new_len <= di->cfsi_rd[i].len);
         di->cfsi_rd[i].len = new_len;
      }
   }

   j = 0;
   for (i = 0; i < (Word)di->cfsi_used; i++) {
      if (di->cfsi_rd[i].len > 0) {
         if (j != i)
            di->cfsi_rd[j] = di->cfsi_rd[i];
         j++;
      }
   }
   
   di->cfsi_used = j;

   
   for (i = 0; i < (Word)di->cfsi_used; i++) {
      
      vg_assert(di->cfsi_rd[i].len > 0);
      
      vg_assert(di->cfsi_rd[i].base >= di->cfsi_minavma);
      vg_assert(di->cfsi_rd[i].base + di->cfsi_rd[i].len - 1
                <= di->cfsi_maxavma);

      if (i < di->cfsi_used - 1) {
         
         vg_assert(di->cfsi_rd[i].base < di->cfsi_rd[i+1].base);
         
         vg_assert(di->cfsi_rd[i].base + di->cfsi_rd[i].len - 1
                   < di->cfsi_rd[i+1].base);
      }
   }

   if (VG_(clo_stats) && VG_(clo_verbosity) >= 3) {
      UWord n_mergeables, n_holes;
      get_cfsi_rd_stats (di, &n_mergeables, &n_holes);
      VG_(dmsg)("CFSI total %lu mergeables %lu holes %lu uniq cfsi_m %u\n",
                di->cfsi_used, n_mergeables, n_holes,
                di->cfsi_m_pool ? VG_(sizeDedupPA) (di->cfsi_m_pool) : 0);
   }
}

void ML_(finish_CFSI_arrays) ( struct _DebugInfo* di )
{
   UWord n_mergeables, n_holes;
   UWord new_used;
   UWord i;
   UWord pos;
   UWord f_mergeables, f_holes;
   UInt sz_cfsi_m_pool;

   get_cfsi_rd_stats (di, &n_mergeables, &n_holes);

   if (di->cfsi_used == 0) {
      vg_assert (di->cfsi_rd == NULL);
      vg_assert (di->cfsi_m_pool == NULL);
      vg_assert (n_mergeables == 0);
      vg_assert (n_holes == 0);
      return;
   }

   vg_assert (di->cfsi_used > n_mergeables);
   new_used = di->cfsi_used - n_mergeables + n_holes;

   sz_cfsi_m_pool = VG_(sizeDedupPA)(di->cfsi_m_pool);
   vg_assert (sz_cfsi_m_pool > 0);
   if (sz_cfsi_m_pool <= 255)
      di->sizeof_cfsi_m_ix = 1;
   else if (sz_cfsi_m_pool <= 65535)
      di->sizeof_cfsi_m_ix = 2;
   else
      di->sizeof_cfsi_m_ix = 4;

   di->cfsi_base = ML_(dinfo_zalloc)( "di.storage.finCfSI.1",
                                       new_used * sizeof(Addr) );
   di->cfsi_m_ix = ML_(dinfo_zalloc)( "di.storage.finCfSI.2",
                                      new_used * sizeof(UChar)*di->sizeof_cfsi_m_ix);

   pos = 0;
   f_mergeables = 0;
   f_holes = 0;
   for (i = 0; i < (Word)di->cfsi_used; i++) {
      if (i > 0) {
         Addr here_min = di->cfsi_rd[i].base;
         Addr prev_max = di->cfsi_rd[i-1].base + di->cfsi_rd[i-1].len - 1;
         SizeT sep = here_min - prev_max;

         
         if (sep == 1) {
            if (di->cfsi_rd[i-1].cfsi_m_ix == di->cfsi_rd[i].cfsi_m_ix) {
               f_mergeables++;
               continue;
            }
         }
         
         if (sep > 1) {
            f_holes++;
            di->cfsi_base[pos] = prev_max + 1;
            switch (di->sizeof_cfsi_m_ix) {
               case 1: ((UChar*) di->cfsi_m_ix)[pos] = 0; break;
               case 2: ((UShort*)di->cfsi_m_ix)[pos] = 0; break;
               case 4: ((UInt*)  di->cfsi_m_ix)[pos] = 0; break;
               default: vg_assert(0);
            }
            pos++;
         }
      }

      
      di->cfsi_base[pos] = di->cfsi_rd[i].base;
      switch (di->sizeof_cfsi_m_ix) {
         case 1: ((UChar*) di->cfsi_m_ix)[pos] = di->cfsi_rd[i].cfsi_m_ix; break;
         case 2: ((UShort*)di->cfsi_m_ix)[pos] = di->cfsi_rd[i].cfsi_m_ix; break;
         case 4: ((UInt*)  di->cfsi_m_ix)[pos] = di->cfsi_rd[i].cfsi_m_ix; break;
         default: vg_assert(0);
      }
      pos++;
   }

   vg_assert (f_mergeables == n_mergeables);
   vg_assert (f_holes == n_holes);
   vg_assert (pos == new_used);

   di->cfsi_used = new_used;
   di->cfsi_size = new_used;
   ML_(dinfo_free) (di->cfsi_rd);
   di->cfsi_rd = NULL;
}


void ML_(canonicaliseTables) ( struct _DebugInfo* di )
{
   canonicaliseSymtab ( di );
   canonicaliseLoctab ( di );
   canonicaliseInltab ( di );
   ML_(canonicaliseCFI) ( di );
   if (di->cfsi_m_pool)
      VG_(freezeDedupPA) (di->cfsi_m_pool, ML_(dinfo_shrink_block));
   canonicaliseVarInfo ( di );
   if (di->strpool)
      VG_(freezeDedupPA) (di->strpool, ML_(dinfo_shrink_block));
   if (di->fndnpool)
      VG_(freezeDedupPA) (di->fndnpool, ML_(dinfo_shrink_block));
}




Word ML_(search_one_symtab) ( const DebugInfo* di, Addr ptr,
                              Bool match_anywhere_in_sym,
                              Bool findText )
{
   Addr a_mid_lo, a_mid_hi;
   Word mid, size, 
        lo = 0, 
        hi = di->symtab_used-1;
   while (True) {
      
      if (lo > hi) return -1; 
      mid      = (lo + hi) / 2;
      a_mid_lo = di->symtab[mid].avmas.main;
      size = ( match_anywhere_in_sym
             ? di->symtab[mid].size
             : 1);
      a_mid_hi = ((Addr)di->symtab[mid].avmas.main) + size - 1;

      if (ptr < a_mid_lo) { hi = mid-1; continue; } 
      if (ptr > a_mid_hi) { lo = mid+1; continue; }
      vg_assert(ptr >= a_mid_lo && ptr <= a_mid_hi);
      if (  findText   &&   di->symtab[mid].isText  ) return mid;
      if ( (!findText) && (!di->symtab[mid].isText) ) return mid;
      return -1;
   }
}



Word ML_(search_one_loctab) ( const DebugInfo* di, Addr ptr )
{
   Addr a_mid_lo, a_mid_hi;
   Word mid, 
        lo = 0, 
        hi = di->loctab_used-1;
   while (True) {
      
      if (lo > hi) return -1; 
      mid      = (lo + hi) / 2;
      a_mid_lo = di->loctab[mid].addr;
      a_mid_hi = ((Addr)di->loctab[mid].addr) + di->loctab[mid].size - 1;

      if (ptr < a_mid_lo) { hi = mid-1; continue; } 
      if (ptr > a_mid_hi) { lo = mid+1; continue; }
      vg_assert(ptr >= a_mid_lo && ptr <= a_mid_hi);
      return mid;
   }
}



Word ML_(search_one_cfitab) ( const DebugInfo* di, Addr ptr )
{
   Word mid, 
        lo = 0, 
        hi = di->cfsi_used-1;

   while (lo <= hi) {
      mid      = (lo + hi) / 2;
      if (ptr < di->cfsi_base[mid]) { hi = mid-1; continue; } 
      if (ptr > di->cfsi_base[mid]) { lo = mid+1; continue; }
      lo = mid+1; break;
   }

#if 0
   for (mid = 0; mid <= di->cfsi_used-1; mid++)
      if (ptr < di->cfsi_base[mid])
         break;
   vg_assert (lo - 1 == mid - 1);
#endif
   return lo - 1;
}



Word ML_(search_one_fpotab) ( const DebugInfo* di, Addr ptr )
{
   Addr const addr = ptr - di->fpo_base_avma;
   Addr a_mid_lo, a_mid_hi;
   Word mid, size,
        lo = 0,
        hi = di->fpo_size-1;
   while (True) {
      
      if (lo > hi) return -1; 
      mid      = (lo + hi) / 2;
      a_mid_lo = di->fpo[mid].ulOffStart;
      size     = di->fpo[mid].cbProcSize;
      a_mid_hi = a_mid_lo + size - 1;
      vg_assert(a_mid_hi >= a_mid_lo);
      if (addr < a_mid_lo) { hi = mid-1; continue; }
      if (addr > a_mid_hi) { lo = mid+1; continue; }
      vg_assert(addr >= a_mid_lo && addr <= a_mid_hi);
      return mid;
   }
}

