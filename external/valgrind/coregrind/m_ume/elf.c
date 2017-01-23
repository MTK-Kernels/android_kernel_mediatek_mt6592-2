

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

#if defined(VGO_linux)

#include "pub_core_basics.h"
#include "pub_core_vki.h"

#include "pub_core_aspacemgr.h"     
#include "pub_core_debuglog.h"
#include "pub_core_libcassert.h"    
#include "pub_core_libcbase.h"      
#include "pub_core_libcprint.h"
#include "pub_core_libcfile.h"      
#include "pub_core_machine.h"       
#include "pub_core_mallocfree.h"    
#include "pub_core_syscall.h"       
#include "pub_core_ume.h"           

#include "priv_ume.h"

#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include <elf.h>


#if     VG_WORDSIZE == 8
#define ESZ(x)  Elf64_##x
#elif   VG_WORDSIZE == 4
#define ESZ(x)  Elf32_##x
#else
#error VG_WORDSIZE needs to ==4 or ==8
#endif

struct elfinfo
{
   ESZ(Ehdr)    e;
   ESZ(Phdr)    *p;
   Int          fd;
};

static void check_mmap(SysRes res, Addr base, SizeT len)
{
   if (sr_isError(res)) {
      VG_(printf)("valgrind: mmap(0x%llx, %lld) failed in UME "
                  "with error %lu (%s).\n", 
                  (ULong)base, (Long)len, 
                  sr_Err(res), VG_(strerror)(sr_Err(res)) );
      if (sr_Err(res) == VKI_EINVAL) {
         VG_(printf)("valgrind: this can be caused by executables with "
                     "very large text, data or bss segments.\n");
      }
      VG_(exit)(1);
   }
}


static 
struct elfinfo *readelf(Int fd, const HChar *filename)
{
   SysRes sres;
   struct elfinfo *e = VG_(malloc)("ume.re.1", sizeof(*e));
   Int phsz;

   e->fd = fd;

   sres = VG_(pread)(fd, &e->e, sizeof(e->e), 0);
   if (sr_isError(sres) || sr_Res(sres) != sizeof(e->e)) {
      VG_(printf)("valgrind: %s: can't read ELF header: %s\n", 
                  filename, VG_(strerror)(sr_Err(sres)));
      goto bad;
   }

   if (VG_(memcmp)(&e->e.e_ident[0], ELFMAG, SELFMAG) != 0) {
      VG_(printf)("valgrind: %s: bad ELF magic number\n", filename);
      goto bad;
   }
   if (e->e.e_ident[EI_CLASS] != VG_ELF_CLASS) {
      VG_(printf)("valgrind: wrong ELF executable class "
                  "(eg. 32-bit instead of 64-bit)\n");
      goto bad;
   }
   if (e->e.e_ident[EI_DATA] != VG_ELF_DATA2XXX) {
      VG_(printf)("valgrind: executable has wrong endian-ness\n");
      goto bad;
   }
   if (!(e->e.e_type == ET_EXEC || e->e.e_type == ET_DYN)) {
      VG_(printf)("valgrind: this is not an executable\n");
      goto bad;
   }

   if (e->e.e_machine != VG_ELF_MACHINE) {
      VG_(printf)("valgrind: executable is not for "
                  "this architecture\n");
      goto bad;
   }

   if (e->e.e_phentsize != sizeof(ESZ(Phdr))) {
      VG_(printf)("valgrind: sizeof ELF Phdr wrong\n");
      goto bad;
   }

   phsz = sizeof(ESZ(Phdr)) * e->e.e_phnum;
   e->p = VG_(malloc)("ume.re.2", phsz);

   sres = VG_(pread)(fd, e->p, phsz, e->e.e_phoff);
   if (sr_isError(sres) || sr_Res(sres) != phsz) {
      VG_(printf)("valgrind: can't read phdr: %s\n", 
                  VG_(strerror)(sr_Err(sres)));
      VG_(free)(e->p);
      goto bad;
   }

   return e;

  bad:
   VG_(free)(e);
   return NULL;
}

static
ESZ(Addr) mapelf(struct elfinfo *e, ESZ(Addr) base)
{
   Int    i;
   SysRes res;
   ESZ(Addr) elfbrk = 0;

   for (i = 0; i < e->e.e_phnum; i++) {
      ESZ(Phdr) *ph = &e->p[i];
      ESZ(Addr) addr, brkaddr;
      ESZ(Word) memsz;

      if (ph->p_type != PT_LOAD)
         continue;

      addr    = ph->p_vaddr+base;
      memsz   = ph->p_memsz;
      brkaddr = addr+memsz;

      if (brkaddr > elfbrk)
         elfbrk = brkaddr;
   }

   for (i = 0; i < e->e.e_phnum; i++) {
      ESZ(Phdr) *ph = &e->p[i];
      ESZ(Addr) addr, bss, brkaddr;
      ESZ(Off) off;
      ESZ(Word) filesz;
      ESZ(Word) memsz;
      unsigned prot = 0;

      if (ph->p_type != PT_LOAD)
         continue;

      if (ph->p_flags & PF_X) prot |= VKI_PROT_EXEC;
      if (ph->p_flags & PF_W) prot |= VKI_PROT_WRITE;
      if (ph->p_flags & PF_R) prot |= VKI_PROT_READ;

      addr    = ph->p_vaddr+base;
      off     = ph->p_offset;
      filesz  = ph->p_filesz;
      bss     = addr+filesz;
      memsz   = ph->p_memsz;
      brkaddr = addr+memsz;

      
      
      
      
      
      
      if (VG_PGROUNDUP(bss)-VG_PGROUNDDN(addr) > 0) {
         if (0) VG_(debugLog)(0,"ume","mmap_file_fixed_client #1\n");
         res = VG_(am_mmap_file_fixed_client)(
                  VG_PGROUNDDN(addr),
                  VG_PGROUNDUP(bss)-VG_PGROUNDDN(addr),
                  prot, 
                  e->fd, VG_PGROUNDDN(off)
               );
         if (0) VG_(am_show_nsegments)(0,"after #1");
         check_mmap(res, VG_PGROUNDDN(addr),
                         VG_PGROUNDUP(bss)-VG_PGROUNDDN(addr));
      }

      
      if (memsz > filesz) {
         UInt bytes;

         bytes = VG_PGROUNDUP(brkaddr)-VG_PGROUNDUP(bss);
         if (bytes > 0) {
            if (0) VG_(debugLog)(0,"ume","mmap_anon_fixed_client #2\n");
            res = VG_(am_mmap_anon_fixed_client)(
                     VG_PGROUNDUP(bss), bytes,
                     prot
                  );
            if (0) VG_(am_show_nsegments)(0,"after #2");
            check_mmap(res, VG_PGROUNDUP(bss), bytes);
         }

         bytes = bss & (VKI_PAGE_SIZE - 1);

         
         if ((prot & VKI_PROT_WRITE) && (bytes > 0)) {
            bytes = VKI_PAGE_SIZE - bytes;
            VG_(memset)((void *)bss, 0, bytes);
         }
      }
   }

   return elfbrk;
}

Bool VG_(match_ELF)(const void *hdr, SizeT len)
{
   const ESZ(Ehdr) *e = hdr;
   return (len > sizeof(*e)) && VG_(memcmp)(&e->e_ident[0], ELFMAG, SELFMAG) == 0;
}


Int VG_(load_ELF)(Int fd, const HChar* name, ExeInfo* info)
{
   SysRes sres;
   struct elfinfo *e;
   struct elfinfo *interp = NULL;
   ESZ(Addr) minaddr = ~0;      
   ESZ(Addr) maxaddr = 0;       
   ESZ(Addr) interp_addr = 0;   
   ESZ(Word) interp_size = 0;   
    
   Int i;
   void *entry;
   ESZ(Addr) ebase = 0;

#  if defined(HAVE_PIE)
   ebase = info->exe_base;
#  endif

   e = readelf(fd, name);

   if (e == NULL)
      return VKI_ENOEXEC;

   if (e->e.e_type == ET_DYN && ebase == 0) {
      ebase = VG_PGROUNDDN(info->exe_base 
                           + (info->exe_end - info->exe_base) * 2 / 3);
#     if defined(VGP_mips64_linux)
      if (ebase < 0x100000)
         ebase = 0x100000;
#     else
      vg_assert(VKI_PAGE_SIZE >= 4096); 
      ESZ(Addr) hacky_load_address = 0x100000 + 8 * VKI_PAGE_SIZE;
      if (ebase < hacky_load_address)
         ebase = hacky_load_address;
#     endif
   }

   info->phnum = e->e.e_phnum;
   info->entry = e->e.e_entry + ebase;
   info->phdr = 0;
   info->stack_prot = VKI_PROT_READ|VKI_PROT_WRITE|VKI_PROT_EXEC;

   for (i = 0; i < e->e.e_phnum; i++) {
      ESZ(Phdr) *ph = &e->p[i];

      switch(ph->p_type) {
      case PT_PHDR:
         info->phdr = ph->p_vaddr + ebase;
         break;

      case PT_LOAD:
         if (ph->p_vaddr < minaddr)
            minaddr = ph->p_vaddr;
         if (ph->p_vaddr+ph->p_memsz > maxaddr)
            maxaddr = ph->p_vaddr+ph->p_memsz;
         break;
                        
      case PT_INTERP: {
         HChar *buf = VG_(malloc)("ume.LE.1", ph->p_filesz+1);
         Int j;
         Int intfd;
         Int baseaddr_set;

         VG_(pread)(fd, buf, ph->p_filesz, ph->p_offset);
         buf[ph->p_filesz] = '\0';

         sres = VG_(open)(buf, VKI_O_RDONLY, 0);
         if (sr_isError(sres)) {
            VG_(printf)("valgrind: m_ume.c: can't open interpreter\n");
            VG_(exit)(1);
         }
         intfd = sr_Res(sres);

         interp = readelf(intfd, buf);
         if (interp == NULL) {
            VG_(printf)("valgrind: m_ume.c: can't read interpreter\n");
            return 1;
         }
         VG_(free)(buf);

         baseaddr_set = 0;
         for (j = 0; j < interp->e.e_phnum; j++) {
            ESZ(Phdr) *iph = &interp->p[j];
            ESZ(Addr) end;

            if (iph->p_type != PT_LOAD || iph->p_memsz == 0)
               continue;
            
            if (!baseaddr_set) {
               interp_addr  = iph->p_vaddr;
                
               baseaddr_set = 1;
            }

            
            end = (iph->p_vaddr - interp_addr) + iph->p_memsz;

            if (end > interp_size)
               interp_size = end;
         }
         break;

#     if defined(PT_GNU_STACK)
      
      case PT_GNU_STACK:
         if ((ph->p_flags & PF_X) == 0) info->stack_prot &= ~VKI_PROT_EXEC;
         if ((ph->p_flags & PF_W) == 0) info->stack_prot &= ~VKI_PROT_WRITE;
         if ((ph->p_flags & PF_R) == 0) info->stack_prot &= ~VKI_PROT_READ;
         break;
#     endif

      default:
         
         break;
      }
      }
   }

   if (info->phdr == 0)
      info->phdr = minaddr + ebase + e->e.e_phoff;

   if (info->exe_base != info->exe_end) {
      if (minaddr >= maxaddr ||
          (minaddr + ebase < info->exe_base ||
           maxaddr + ebase > info->exe_end)) {
         VG_(printf)("Executable range %p-%p is outside the\n"
                     "acceptable range %p-%p\n",
                     (char *)minaddr + ebase, (char *)maxaddr + ebase,
                     (char *)info->exe_base,  (char *)info->exe_end);
         return VKI_ENOMEM;
      }
   }

   info->brkbase = mapelf(e, ebase);    

   if (info->brkbase == 0)
      return VKI_ENOMEM;

   if (interp != NULL) {
      
      MapRequest mreq;
      Addr       advised;
      Bool       ok;

      if (interp_addr == 0) {
         mreq.rkind = MAny;
         mreq.start = 0;
         mreq.len   = interp_size;
      } else {
         mreq.rkind = MHint;
         mreq.start = interp_addr;
         mreq.len   = interp_size;
      }

      advised = VG_(am_get_advisory)( &mreq, True, &ok );

      if (!ok) {
         
         SysRes res = VG_(mk_SysRes_Error)(VKI_EINVAL);
         if (0) VG_(printf)("reserve for interp: failed\n");
         check_mmap(res, (Addr)interp_addr, interp_size);
         
      }

      (void)mapelf(interp, (ESZ(Addr))advised - interp_addr);

      VG_(close)(interp->fd);

      entry = (void *)(advised - interp_addr + interp->e.e_entry);

      info->interp_offset = advised - interp_addr;

      VG_(free)(interp->p);
      VG_(free)(interp);
   } else
      entry = (void *)(ebase + e->e.e_entry);

   info->exe_base = minaddr + ebase;
   info->exe_end  = maxaddr + ebase;

#if defined(VGP_ppc64be_linux)
   info->init_ip  = ((ULong*)entry)[0];
   info->init_toc = ((ULong*)entry)[1];
   info->init_ip  += info->interp_offset;
   info->init_toc += info->interp_offset;
#elif defined(VGP_ppc64le_linux)
   
   info->init_ip  = (Addr)entry;
   info->init_toc = 0; 
#else
   info->init_ip  = (Addr)entry;
   info->init_toc = 0; 
#endif
   VG_(free)(e->p);
   VG_(free)(e);

   return 0;
}

#endif 

