

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2005-2013 Apple Inc.
      Greg Parker gparker@apple.com

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

#if defined(VGO_darwin)

#include "pub_core_basics.h"
#include "pub_core_vki.h"
#include "pub_core_libcbase.h"
#include "pub_core_libcprint.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcfile.h"
#include "pub_core_libcproc.h"
#include "pub_core_aspacemgr.h"    
#include "pub_core_machine.h"      
#include "pub_core_options.h"
#include "pub_core_oset.h"
#include "pub_core_tooliface.h"    
#include "pub_core_xarray.h"
#include "pub_core_clientstate.h"
#include "pub_core_debuginfo.h"

#include "priv_misc.h"
#include "priv_image.h"
#include "priv_d3basics.h"
#include "priv_tytypes.h"
#include "priv_storage.h"
#include "priv_readmacho.h"
#include "priv_readdwarf.h"
#include "priv_readdwarf3.h"

#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/fat.h>

#if VG_WORDSIZE == 4
# define MAGIC MH_MAGIC
# define MACH_HEADER mach_header
# define LC_SEGMENT_CMD LC_SEGMENT
# define SEGMENT_COMMAND segment_command
# define SECTION section
# define NLIST nlist
#else
# define MAGIC MH_MAGIC_64
# define MACH_HEADER mach_header_64
# define LC_SEGMENT_CMD LC_SEGMENT_64
# define SEGMENT_COMMAND segment_command_64
# define SECTION section_64
# define NLIST nlist_64
#endif




Bool ML_(is_macho_object_file)( const void* buf, SizeT szB )
{


   const struct fat_header*  fh_be = buf;
   const struct MACH_HEADER* mh    = buf;

   vg_assert(buf);
   if (szB < sizeof(struct fat_header))
      return False;
   if (VG_(ntohl)(fh_be->magic) == FAT_MAGIC)
      return True;

   if (szB < sizeof(struct MACH_HEADER))
      return False;
   if (mh->magic == MAGIC)
      return True;

   return False;
}


static void unmap_image ( DiSlice* sli )
{
   vg_assert(sli);
   if (ML_(sli_is_valid)(*sli)) {
      ML_(img_done)(sli->img);
      *sli = DiSlice_INVALID;
   }
}


static DiSlice map_image_aboard ( DebugInfo* di, 
                                  const HChar* filename )
{
   DiSlice sli = DiSlice_INVALID;

   
   DiImage* mimg = ML_(img_from_local_file)(filename);
   if (mimg == NULL) {
      VG_(message)(Vg_UserMsg, "warning: connection to image %s failed\n",
                               filename );
      VG_(message)(Vg_UserMsg, "         no symbols or debug info loaded\n" );
      return DiSlice_INVALID;
   }

   DiOffT            fh_be_ioff = 0;
   struct fat_header fh_be;
   struct fat_header fh;
     
   
   
   
   
   sli  = ML_(sli_from_img)(mimg);
   mimg = NULL;

   
   if (ML_(img_size)(sli.img) < sizeof(struct fat_header)) {
      ML_(symerr)(di, True, "Invalid Mach-O file (0 too small).");
      goto close_and_fail;
   }

   
   ML_(img_get)(&fh_be, sli.img, fh_be_ioff, sizeof(fh_be));
   VG_(memset)(&fh, 0, sizeof(fh));
   fh.magic     = VG_(ntohl)(fh_be.magic);
   fh.nfat_arch = VG_(ntohl)(fh_be.nfat_arch);
   if (fh.magic == FAT_MAGIC) {
      
      if (ML_(img_size)(sli.img) < sizeof(struct fat_header)
                                   + fh.nfat_arch * sizeof(struct fat_arch)) {
         ML_(symerr)(di, True, "Invalid Mach-O file (1 too small).");
         goto close_and_fail;
      }
      DiOffT arch_be_ioff;
      Int    f;
      for (f = 0, arch_be_ioff = sizeof(struct fat_header);
           f < fh.nfat_arch;
           f++, arch_be_ioff += sizeof(struct fat_arch)) {
#        if defined(VGA_ppc)
         Int cputype = CPU_TYPE_POWERPC;
#        elif defined(VGA_ppc64be)
         Int cputype = CPU_TYPE_POWERPC64BE;
#        elif defined(VGA_ppc64le)
         Int cputype = CPU_TYPE_POWERPC64LE;
#        elif defined(VGA_x86)
         Int cputype = CPU_TYPE_X86;
#        elif defined(VGA_amd64)
         Int cputype = CPU_TYPE_X86_64;
#        else
#          error "unknown architecture"
#        endif
         struct fat_arch arch_be;
         struct fat_arch arch;
         ML_(img_get)(&arch_be, sli.img, arch_be_ioff, sizeof(arch_be));
         VG_(memset)(&arch, 0, sizeof(arch));
         arch.cputype    = VG_(ntohl)(arch_be.cputype);
         arch.cpusubtype = VG_(ntohl)(arch_be.cpusubtype);
         arch.offset     = VG_(ntohl)(arch_be.offset);
         arch.size       = VG_(ntohl)(arch_be.size);
         if (arch.cputype == cputype) {
            if (ML_(img_size)(sli.img) < arch.offset + arch.size) {
               ML_(symerr)(di, True, "Invalid Mach-O file (2 too small).");
               goto close_and_fail;
            }
            
            sli.ioff = arch.offset;
            sli.szB  = arch.size;
            break;
         }
      }
      if (f == fh.nfat_arch) {
         ML_(symerr)(di, True,
                     "No acceptable architecture found in fat file.");
         goto close_and_fail;
      }
   }

   

   
   vg_assert(ML_(img_size)(sli.img) >= sizeof(struct fat_header));

   if (sli.szB < sizeof(struct MACH_HEADER)) {
      ML_(symerr)(di, True, "Invalid Mach-O file (3 too small).");
      goto close_and_fail;
   }

   if (sli.szB > ML_(img_size)(sli.img)) {
      ML_(symerr)(di, True, "Invalid Mach-O file (thin bigger than fat).");
      goto close_and_fail;
   }

   if (sli.ioff >= 0 && sli.ioff + sli.szB <= ML_(img_size)(sli.img)) {
      
   } else {
      ML_(symerr)(di, True, "Invalid Mach-O file (thin not inside fat).");
      goto close_and_fail;
   }

   struct MACH_HEADER mh;
   ML_(cur_read_get)(&mh, ML_(cur_from_sli)(sli), sizeof(mh));
   if (mh.magic != MAGIC) {
      ML_(symerr)(di, True, "Invalid Mach-O file (bad magic).");
      goto close_and_fail;
   }

   if (sli.szB < sizeof(struct MACH_HEADER) + mh.sizeofcmds) {
      ML_(symerr)(di, True, "Invalid Mach-O file (4 too small).");
      goto close_and_fail;
   }

   
   vg_assert(sli.img);
   vg_assert(ML_(img_size)(sli.img) > 0);
   
   vg_assert(sli.ioff >= 0);
   vg_assert(sli.szB > 0);
   vg_assert(sli.ioff + sli.szB <= ML_(img_size)(sli.img));
   return sli;  
   

  close_and_fail:
   unmap_image(&sli);
   return DiSlice_INVALID; 
}



static
void read_symtab( XArray*  syms,
                  struct _DebugInfo* di, 
                  DiCursor symtab_cur, UInt symtab_count,
                  DiCursor strtab_cur, UInt strtab_sz )
{
   Int    i;
   DiSym  disym;

   
   static const HChar* s_a_t_v = NULL; 

   for (i = 0; i < symtab_count; i++) {
      struct NLIST nl;
      ML_(cur_read_get)(&nl,
                        ML_(cur_plus)(symtab_cur, i * sizeof(struct NLIST)),
                        sizeof(nl));

      Addr sym_addr = 0;
      if ((nl.n_type & N_TYPE) == N_SECT) {
         sym_addr = di->text_bias + nl.n_value;
      } else {
         continue;
      }
      
      if (di->trace_symtab) {
         HChar* str = ML_(cur_read_strdup)(
                         ML_(cur_plus)(strtab_cur, nl.n_un.n_strx),
                         "di.read_symtab.1");
         VG_(printf)("nlist raw: avma %010lx  %s\n", sym_addr, str );
         ML_(dinfo_free)(str);
      }

      if (sym_addr <= di->text_avma
          || sym_addr >= di->text_avma+di->text_size) {
         continue;
      }

      if (nl.n_un.n_strx < 0 || nl.n_un.n_strx >= strtab_sz) {
         continue;
      }

      HChar* name
         = ML_(cur_read_strdup)( ML_(cur_plus)(strtab_cur, nl.n_un.n_strx),
                                 "di.read_symtab.2");

      if (*name == 0) {
         ML_(dinfo_free)(name);
         continue;
      }

      VG_(bzero_inline)(&disym, sizeof(disym));
      disym.avmas.main = sym_addr;
      SET_TOCPTR_AVMA(disym, 0);
      SET_LOCAL_EP_AVMA(disym, 0);
      disym.pri_name   = ML_(addStr)(di, name, -1);
      disym.sec_names  = NULL;
      disym.size       = 
                         di->text_avma+di->text_size - sym_addr;
      disym.isText     = True;
      disym.isIFunc    = False;
      
      
      
      
      
      
      if (disym.pri_name[0] == '_') {
         disym.pri_name++;
      } 
      else if (!VG_(clo_show_below_main) && VG_STREQ(disym.pri_name, "start")) {
         if (s_a_t_v == NULL)
            s_a_t_v = ML_(addStr)(di, "start_according_to_valgrind", -1);
         vg_assert(s_a_t_v);
         disym.pri_name = s_a_t_v;
      }

      vg_assert(disym.pri_name);
      VG_(addToXA)( syms, &disym );
      ML_(dinfo_free)(name);
   }
}


static Int cmp_DiSym_by_start_then_name ( const void* v1, const void* v2 )
{
   const DiSym* s1 = (DiSym*)v1;
   const DiSym* s2 = (DiSym*)v2;
   if (s1->avmas.main < s2->avmas.main) return -1;
   if (s1->avmas.main > s2->avmas.main) return 1;
   return VG_(strcmp)(s1->pri_name, s2->pri_name);
}

static void tidy_up_cand_syms ( XArray*  syms,
                                Bool trace_symtab )
{
   Word nsyms, i, j, k, m;

   nsyms = VG_(sizeXA)(syms);

   VG_(setCmpFnXA)(syms, cmp_DiSym_by_start_then_name);
   VG_(sortXA)(syms);


   for (i = 0; i < nsyms; i++) {
      for (k = i+1;
           k < nsyms
             && ((DiSym*)VG_(indexXA)(syms,i))->avmas.main
                 == ((DiSym*)VG_(indexXA)(syms,k))->avmas.main;
           k++)
         ;
      if (k < nsyms) {
         DiSym* next = (DiSym*)VG_(indexXA)(syms,k);
         for (m = i; m < k; m++) {
            DiSym* here = (DiSym*)VG_(indexXA)(syms,m);
            vg_assert(here->avmas.main < next->avmas.main);
            if (here->avmas.main + here->size > next->avmas.main)
               here->size = next->avmas.main - here->avmas.main;
         }
      }
      i = k-1;
      vg_assert(i <= nsyms);
   }

   j = 0;
   if (nsyms > 0) {
      j = 1;
      for (i = 1; i < nsyms; i++) {
         DiSym *s_j1, *s_j, *s_i;
         vg_assert(j <= i);
         s_j1 = (DiSym*)VG_(indexXA)(syms, j-1);
         s_j  = (DiSym*)VG_(indexXA)(syms, j);
         s_i  = (DiSym*)VG_(indexXA)(syms, i);
         if (s_i->avmas.main != s_j1->avmas.main
             || s_i->size != s_j1->size
             || 0 != VG_(strcmp)(s_i->pri_name, s_j1->pri_name)) {
            *s_j = *s_i;
            j++;
         } else {
            if (trace_symtab)
               VG_(printf)("nlist cleanup: dump duplicate avma %010lx  %s\n",
                           s_i->avmas.main, s_i->pri_name );
         }
      }
   }
   vg_assert(j >= 0 && j <= nsyms);
   VG_(dropTailXA)(syms, nsyms - j);
}



#if !defined(APPLE_DSYM_EXT_AND_SUBDIRECTORY)
#define APPLE_DSYM_EXT_AND_SUBDIRECTORY ".dSYM/Contents/Resources/DWARF/"
#endif


static Bool file_exists_p(const HChar *path)
{
   struct vg_stat sbuf;
   SysRes res = VG_(stat)(path, &sbuf);
   return sr_isError(res) ? False : True;
}


static HChar *
find_separate_debug_file (const HChar *executable_name)
{
   const HChar *basename_str;
   HChar *dot_ptr;
   HChar *slash_ptr;
   HChar *dsymfile;
    
   if (VG_(strcasestr) (executable_name, ".dSYM") == NULL)
   {
      
      basename_str = VG_(basename) (executable_name);
      dsymfile = ML_(dinfo_zalloc)("di.readmacho.dsymfile", 
                    VG_(strlen) (executable_name)
                    + VG_(strlen) (APPLE_DSYM_EXT_AND_SUBDIRECTORY)
                    + VG_(strlen) (basename_str)
                    + 1
                 );
        
      
      VG_(strcpy) (dsymfile, executable_name);
      VG_(strcat) (dsymfile, APPLE_DSYM_EXT_AND_SUBDIRECTORY);
      VG_(strcat) (dsymfile, basename_str);
        
      if (file_exists_p (dsymfile))
         return dsymfile;
        
      VG_(strcpy) (dsymfile, VG_(dirname) (executable_name));
      while ((dot_ptr = VG_(strrchr) (dsymfile, '.')))
      {
         slash_ptr = VG_(strchr) (dot_ptr, '/');
         if (slash_ptr)
         {
            *slash_ptr = '\0';
            VG_(strcat) (slash_ptr, APPLE_DSYM_EXT_AND_SUBDIRECTORY);
            VG_(strcat) (slash_ptr, basename_str);
            if (file_exists_p (dsymfile))
               return dsymfile;
         }
         
         *dot_ptr = '\0';
         VG_(strcat) (dot_ptr, APPLE_DSYM_EXT_AND_SUBDIRECTORY);
         VG_(strcat) (dot_ptr, basename_str);
         if (file_exists_p (dsymfile))
            return dsymfile;
         
         *dot_ptr = '\0';
         
         slash_ptr = VG_(strrchr) (dsymfile, '/');
         if (!slash_ptr)
            break;
         *slash_ptr = '\0';
      }
   }

   return NULL;
}


static DiSlice getsectdata ( DiSlice img,
                             const HChar *segname, const HChar *sectname,
                             Addr* svma )
{
   DiCursor cur = ML_(cur_from_sli)(img);

   struct MACH_HEADER mh;
   ML_(cur_step_get)(&mh, &cur, sizeof(mh));

   Int c;
   for (c = 0; c < mh.ncmds; c++) {
      struct load_command cmd;          
      ML_(cur_read_get)(&cmd, cur, sizeof(cmd));
      if (cmd.cmd == LC_SEGMENT_CMD) {
         struct SEGMENT_COMMAND seg;
         ML_(cur_read_get)(&seg, cur, sizeof(seg));
         if (0 == VG_(strncmp(&seg.segname[0],
                              segname, sizeof(seg.segname)))) {
            DiCursor sects_cur = ML_(cur_plus)(cur, sizeof(seg));
            Int s;
            for (s = 0; s < seg.nsects; s++) {
               struct SECTION sect;
               ML_(cur_step_get)(&sect, &sects_cur, sizeof(sect));
               if (0 == VG_(strncmp(sect.sectname, sectname, 
                                    sizeof(sect.sectname)))) {
                  DiSlice res = img;
                  res.ioff = sect.offset;
                  res.szB = sect.size;
                  if (svma) *svma = (Addr)sect.addr;
                  return res;
               }
            }

         }
      }
      cur = ML_(cur_plus)(cur, cmd.cmdsize);
   }

   return DiSlice_INVALID;
}


static Bool check_uuid_matches ( DiSlice sli, UChar* uuid )
{
   if (sli.szB < 16)
      return False;

   
   UChar  first    = uuid[0];
   DiOffT min_off  = sli.ioff;
   DiOffT max1_off = sli.ioff + sli.szB;
   DiOffT curr_off = min_off;
   vg_assert(min_off < max1_off);
   while (1) {
      vg_assert(curr_off >= min_off && curr_off <= max1_off);
      if (curr_off == max1_off) break;
      DiOffT avail = max1_off - curr_off;
      vg_assert(avail > 0 && avail <= max1_off);
      if (avail > 1024) avail = 1024;
      UChar buf[1024];
      SizeT nGot = ML_(img_get_some)(buf, sli.img, curr_off, avail);
      vg_assert(nGot >= 1 && nGot <= avail);
      UInt i;
      
      for (i = 0; i < (UInt)nGot; i++) {
         if (LIKELY(buf[i] != first))
            continue;
         if (curr_off + i < max1_off && max1_off - (curr_off + i) >= 16) {
            UChar buff16[16];
            ML_(img_get)(&buff16[0], sli.img, curr_off + i, 16);
            if (0 == VG_(memcmp)(&buff16[0], &uuid[0], 16))
               return True;
         }
      }
      curr_off += nGot;
   }
   return False;
}


static Bool is_systemish_library_name ( const HChar* name )
{
   vg_assert(name);
   if (0 == VG_(strncasecmp)(name, "/usr/", 5)
       || 0 == VG_(strncasecmp)(name, "/bin/", 5)
       || 0 == VG_(strncasecmp)(name, "/sbin/", 6)
       || 0 == VG_(strncasecmp)(name, "/opt/", 5)
       || 0 == VG_(strncasecmp)(name, "/sw/", 4)
       || 0 == VG_(strncasecmp)(name, "/System/", 8)
       || 0 == VG_(strncasecmp)(name, "/Library/", 9)
       || 0 == VG_(strncasecmp)(name, "/Applications/", 14)) {
      return True;
   } else {
      return False;
   }
}


Bool ML_(read_macho_debug_info)( struct _DebugInfo* di )
{
   DiSlice  msli         = DiSlice_INVALID; 
   DiSlice  dsli         = DiSlice_INVALID; 
   DiCursor sym_cur      = DiCursor_INVALID;
   DiCursor dysym_cur    = DiCursor_INVALID;
   HChar*   dsymfilename = NULL;
   Bool     have_uuid    = False;
   UChar    uuid[16];
   Word     i;
   const DebugInfoMapping* rx_map = NULL;
   const DebugInfoMapping* rw_map = NULL;


   vg_assert(di->fsm.have_rx_map);
   vg_assert(di->fsm.have_rw_map);

   for (i = 0; i < VG_(sizeXA)(di->fsm.maps); i++) {
      const DebugInfoMapping* map = VG_(indexXA)(di->fsm.maps, i);
      if (map->rx && !rx_map)
         rx_map = map;
      if (map->rw && !rw_map)
         rw_map = map;
      if (rx_map && rw_map)
         break;
   }
   vg_assert(rx_map);
   vg_assert(rw_map);

   if (VG_(clo_verbosity) > 1)
      VG_(message)(Vg_DebugMsg,
                   "%s (rx at %#lx, rw at %#lx)\n", di->fsm.filename,
                   rx_map->avma, rw_map->avma );

   VG_(memset)(&uuid, 0, sizeof(uuid));

   msli = map_image_aboard( di, di->fsm.filename );
   if (!ML_(sli_is_valid)(msli)) {
      ML_(symerr)(di, False, "Connect to main image failed.");
      goto fail;
   }

   vg_assert(msli.img != NULL && msli.szB > 0);

   
   
   
   
   
   

   di->text_bias = 0;

   { 
      DiCursor cmd_cur = ML_(cur_from_sli)(msli);

      struct MACH_HEADER mh;
      ML_(cur_step_get)(&mh, &cmd_cur, sizeof(mh));


      Int c;
      for (c = 0; c < mh.ncmds; c++) {
         struct load_command cmd;
         ML_(cur_read_get)(&cmd, cmd_cur, sizeof(cmd));
 
         if (cmd.cmd == LC_SYMTAB) {
            sym_cur = cmd_cur;
         } 
         else if (cmd.cmd == LC_DYSYMTAB) {
            dysym_cur = cmd_cur;
         } 
         else if (cmd.cmd == LC_ID_DYLIB && mh.filetype == MH_DYLIB) {
            
            struct dylib_command dcmd;
            ML_(cur_read_get)(&dcmd, cmd_cur, sizeof(dcmd));
            DiCursor dylibname_cur
               = ML_(cur_plus)(cmd_cur, dcmd.dylib.name.offset);
            HChar* dylibname
               = ML_(cur_read_strdup)(dylibname_cur, "di.rmdi.1");
            HChar* soname = VG_(strrchr)(dylibname, '/');
            if (!soname) soname = dylibname;
            else soname++;
            di->soname = ML_(dinfo_strdup)("di.readmacho.dylibname",
                                           soname);
            ML_(dinfo_free)(dylibname);
         }
         else if (cmd.cmd==LC_ID_DYLINKER  &&  mh.filetype==MH_DYLINKER) {
            struct dylinker_command dcmd;
            ML_(cur_read_get)(&dcmd, cmd_cur, sizeof(dcmd));
            DiCursor dylinkername_cur
               = ML_(cur_plus)(cmd_cur, dcmd.name.offset);
            HChar* dylinkername
               = ML_(cur_read_strdup)(dylinkername_cur, "di.rmdi.2");
            HChar* soname = VG_(strrchr)(dylinkername, '/');
            if (!soname) soname = dylinkername;
            else soname++;
            di->soname = ML_(dinfo_strdup)("di.readmacho.dylinkername",
                                           soname);
            ML_(dinfo_free)(dylinkername);
         }

         
         
         
         
         
         
         
         
         
         
         
         
         
         
         
         
         
         
         
         
         
         
         
         
         
         
         
         else if (cmd.cmd == LC_SEGMENT_CMD) {
            struct SEGMENT_COMMAND seg;
            ML_(cur_read_get)(&seg, cmd_cur, sizeof(seg));
            
            if (!di->text_present
                && 0 == VG_(strcmp)(&seg.segname[0], "__TEXT")
                
                && seg.fileoff == 0 && seg.filesize != 0) {
               di->text_present = True;
               di->text_svma = (Addr)seg.vmaddr;
               di->text_avma = rx_map->avma;
               di->text_size = seg.vmsize;
               di->text_bias = di->text_avma - di->text_svma;
               di->text_debug_svma = di->text_svma;
               di->text_debug_bias = di->text_bias;
            }
            
            if (!di->data_present
                && 0 == VG_(strcmp)(&seg.segname[0], "__DATA")
                 && seg.filesize != 0) {
               di->data_present = True;
               di->data_svma = (Addr)seg.vmaddr;
               di->data_avma = rw_map->avma;
               di->data_size = seg.vmsize;
               di->data_bias = di->data_avma - di->data_svma;
               di->data_debug_svma = di->data_svma;
               di->data_debug_bias = di->data_bias;
            }
         }
         else if (cmd.cmd == LC_UUID) {
             ML_(cur_read_get)(&uuid, cmd_cur, sizeof(uuid));
             have_uuid = True;
         }
         
         cmd_cur = ML_(cur_plus)(cmd_cur, cmd.cmdsize);
      }
   }

   if (!di->soname) {
      di->soname = ML_(dinfo_strdup)("di.readmacho.noname", "NONE");
   }

   if (di->trace_symtab) {
      VG_(printf)("\n");
      VG_(printf)("SONAME = %s\n", di->soname);
      VG_(printf)("\n");
   }

   

   
   vg_assert(msli.img != NULL && msli.szB > 0);

   if (ML_(cur_is_valid)(sym_cur) && ML_(cur_is_valid)(dysym_cur)) {

      struct symtab_command   symcmd;
      struct dysymtab_command dysymcmd;

      ML_(cur_read_get)(&symcmd,   sym_cur,   sizeof(symcmd));
      ML_(cur_read_get)(&dysymcmd, dysym_cur, sizeof(dysymcmd));

      
      DiCursor syms = DiCursor_INVALID;
      DiCursor strs = DiCursor_INVALID;
      XArray*  candSyms = NULL;
      Word nCandSyms;

      if (msli.szB < symcmd.stroff + symcmd.strsize
          || msli.szB < symcmd.symoff + symcmd.nsyms
                                        * sizeof(struct NLIST)) {
         ML_(symerr)(di, False, "Invalid Mach-O file (5 too small).");
         goto fail;
      }   
      if (dysymcmd.ilocalsym + dysymcmd.nlocalsym > symcmd.nsyms
          || dysymcmd.iextdefsym + dysymcmd.nextdefsym > symcmd.nsyms) {
         ML_(symerr)(di, False, "Invalid Mach-O file (bad symbol table).");
         goto fail;
      }

      syms = ML_(cur_plus)(ML_(cur_from_sli)(msli), symcmd.symoff);
      strs = ML_(cur_plus)(ML_(cur_from_sli)(msli), symcmd.stroff);
      
      if (VG_(clo_verbosity) > 1)
         VG_(message)(Vg_DebugMsg,
            "   reading syms   from primary file (%d %d)\n",
            dysymcmd.nextdefsym, dysymcmd.nlocalsym );

      candSyms = VG_(newXA)(
                    ML_(dinfo_zalloc), "di.readmacho.candsyms.1",
                    ML_(dinfo_free), sizeof(DiSym)
                 );

      
      read_symtab(candSyms,
                  di,
                  ML_(cur_plus)(syms,
                                dysymcmd.iextdefsym * sizeof(struct NLIST)),
                  dysymcmd.nextdefsym, strs, symcmd.strsize);
      
      read_symtab(candSyms,
                  di,
                  ML_(cur_plus)(syms,
                                dysymcmd.ilocalsym * sizeof(struct NLIST)),
                  dysymcmd.nlocalsym, strs, symcmd.strsize);

      tidy_up_cand_syms( candSyms, di->trace_symtab );

      
      nCandSyms = VG_(sizeXA)( candSyms );
      for (i = 0; i < nCandSyms; i++) {
         DiSym* cand = (DiSym*) VG_(indexXA)( candSyms, i );
         vg_assert(cand->pri_name != NULL);
         vg_assert(cand->sec_names == NULL);
         if (di->trace_symtab)
            VG_(printf)("nlist final: acquire  avma %010lx-%010lx  %s\n",
                        cand->avmas.main, cand->avmas.main + cand->size - 1,
                        cand->pri_name );
         ML_(addSym)( di, cand );
      }
      VG_(deleteXA)( candSyms );
   }

   if (!have_uuid)
      goto success;


   dsymfilename = find_separate_debug_file( di->fsm.filename );

   
   if (dsymfilename) {
      Bool valid;

      if (VG_(clo_verbosity) > 1)
         VG_(message)(Vg_DebugMsg, "   dSYM= %s\n", dsymfilename);

      dsli = map_image_aboard( di, dsymfilename );
      if (!ML_(sli_is_valid)(dsli)) {
         ML_(symerr)(di, False, "Connect to debuginfo image failed "
                                "(first attempt).");
         goto fail;
      }

      
      vg_assert(have_uuid);
      valid = dsli.img && dsli.szB > 0 && check_uuid_matches( dsli, uuid );
      if (valid)
         goto read_the_dwarf;

      if (VG_(clo_verbosity) > 1)
         VG_(message)(Vg_DebugMsg, "   dSYM does not have "
                                   "correct UUID (out of date?)\n");
   }


   vg_assert(di->fsm.filename);
   if (is_systemish_library_name(di->fsm.filename))
      goto success;

   if (!VG_(clo_dsymutil)) {
      if (VG_(clo_verbosity) == 1) {
         VG_(message)(Vg_DebugMsg, "%s:\n", di->fsm.filename);
      }
      if (VG_(clo_verbosity) > 0)
         VG_(message)(Vg_DebugMsg, "%sdSYM directory %s; consider using "
                      "--dsymutil=yes\n",
                      VG_(clo_verbosity) > 1 ? "   " : "",
                      dsymfilename ? "has wrong UUID" : "is missing"); 
      goto success;
   }

   

   { Int r;
     const HChar* dsymutil = "/usr/bin/dsymutil ";
     HChar* cmd = ML_(dinfo_zalloc)( "di.readmacho.tmp1", 
                                     VG_(strlen)(dsymutil)
                                     + VG_(strlen)(di->fsm.filename)
                                     + 32  );
     VG_(strcpy)(cmd, dsymutil);
     if (0) VG_(strcat)(cmd, "--verbose ");
     VG_(strcat)(cmd, "\"");
     VG_(strcat)(cmd, di->fsm.filename);
     VG_(strcat)(cmd, "\"");
     VG_(message)(Vg_DebugMsg, "run: %s\n", cmd);
     r = VG_(system)( cmd );
     if (r)
        VG_(message)(Vg_DebugMsg, "run: %s FAILED\n", dsymutil);
     ML_(dinfo_free)(cmd);
     dsymfilename = find_separate_debug_file(di->fsm.filename);
   }

   
   if (dsymfilename) {
      Bool valid;

      if (VG_(clo_verbosity) > 1)
         VG_(message)(Vg_DebugMsg, "   dsyms= %s\n", dsymfilename);

      dsli = map_image_aboard( di, dsymfilename );
      if (!ML_(sli_is_valid)(dsli)) {
         ML_(symerr)(di, False, "Connect to debuginfo image failed "
                                "(second attempt).");
         goto fail;
      }

      
      vg_assert(have_uuid);
      vg_assert(have_uuid);
      valid = dsli.img && dsli.szB > 0 && check_uuid_matches( dsli, uuid );
      if (!valid) {
         if (VG_(clo_verbosity) > 0) {
            VG_(message)(Vg_DebugMsg,
               "WARNING: did not find expected UUID %02X%02X%02X%02X"
               "-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X"
               " in dSYM dir\n",
               (UInt)uuid[0], (UInt)uuid[1], (UInt)uuid[2], (UInt)uuid[3],
               (UInt)uuid[4], (UInt)uuid[5], (UInt)uuid[6], (UInt)uuid[7],
               (UInt)uuid[8], (UInt)uuid[9], (UInt)uuid[10],
               (UInt)uuid[11], (UInt)uuid[12], (UInt)uuid[13],
               (UInt)uuid[14], (UInt)uuid[15] );
            VG_(message)(Vg_DebugMsg,
                         "WARNING: for %s\n", di->fsm.filename);
         }
         unmap_image( &dsli );
         goto fail;
      }
   }


  read_the_dwarf:
   if (ML_(sli_is_valid)(dsli) && dsli.szB > 0) {
      
      DiSlice debug_info_mscn
         = getsectdata(dsli, "__DWARF", "__debug_info", NULL);
      DiSlice debug_abbv_mscn
         = getsectdata(dsli, "__DWARF", "__debug_abbrev", NULL);
      DiSlice debug_line_mscn
         = getsectdata(dsli, "__DWARF", "__debug_line", NULL);
      DiSlice debug_str_mscn
         = getsectdata(dsli, "__DWARF", "__debug_str", NULL);
      DiSlice debug_ranges_mscn
         = getsectdata(dsli, "__DWARF", "__debug_ranges", NULL);
      DiSlice debug_loc_mscn
         = getsectdata(dsli, "__DWARF", "__debug_loc", NULL);

      Addr eh_frame_svma = 0;
      DiSlice eh_frame_mscn
         = getsectdata(msli, "__TEXT", "__eh_frame", &eh_frame_svma);

      if (ML_(sli_is_valid)(eh_frame_mscn)) {
         vg_assert(di->text_bias == di->text_debug_bias);
         ML_(read_callframe_info_dwarf3)(di, eh_frame_mscn,
                                         eh_frame_svma + di->text_bias,
                                         True);
      }
   
      if (ML_(sli_is_valid)(debug_info_mscn)) {
         if (VG_(clo_verbosity) > 1) {
            if (0)
            VG_(message)(Vg_DebugMsg,
                         "Reading dwarf3 for %s (%#lx) from %s"
                         " (%lld %lld %lld %lld %lld %lld)\n",
                         di->fsm.filename, di->text_avma, dsymfilename,
                         debug_info_mscn.szB, debug_abbv_mscn.szB,
                         debug_line_mscn.szB, debug_str_mscn.szB,
                         debug_ranges_mscn.szB, debug_loc_mscn.szB
                         );
            VG_(message)(Vg_DebugMsg,
               "   reading dwarf3 from dsyms file\n");
         }
         
         ML_(read_debuginfo_dwarf3) ( di,
                                      debug_info_mscn,
				      DiSlice_INVALID, 
                                      debug_abbv_mscn,
                                      debug_line_mscn,
                                      debug_str_mscn,
                                      DiSlice_INVALID  );

         if (VG_(clo_read_var_info) 
             || VG_(clo_read_inline_info)) {
            ML_(new_dwarf3_reader)(
               di, debug_info_mscn,
                   DiSlice_INVALID, 
                   debug_abbv_mscn,
                   debug_line_mscn,
                   debug_str_mscn,
                   debug_ranges_mscn,
                   debug_loc_mscn,
                   DiSlice_INVALID, 
                   DiSlice_INVALID, 
                   DiSlice_INVALID, 
                   DiSlice_INVALID  
            );
         }
      }
   }

   if (dsymfilename) ML_(dinfo_free)(dsymfilename);

  success:
   unmap_image(&msli);
   unmap_image(&dsli);
   return True;

   

  fail:
   ML_(symerr)(di, True, "Error reading Mach-O object.");
   unmap_image(&msli);
   unmap_image(&dsli);
   return False;
}

#endif 

