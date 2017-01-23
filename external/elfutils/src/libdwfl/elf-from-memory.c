/* Reconstruct an ELF file by reading the segments out of remote memory.
   Copyright (C) 2005-2011, 2014 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>
#include "../libelf/libelfP.h"
#undef _

#include "libdwflP.h"

#include <gelf.h>
#include <sys/types.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


Elf *
elf_from_remote_memory (GElf_Addr ehdr_vma,
			GElf_Xword pagesize,
			GElf_Addr *loadbasep,
			ssize_t (*read_memory) (void *arg, void *data,
						GElf_Addr address,
						size_t minread,
						size_t maxread),
			void *arg)
{
  

  const size_t initial_bufsize = 256;
  unsigned char *buffer = malloc (initial_bufsize);
  if (buffer == NULL)
    {
    no_memory:
      __libdwfl_seterrno (DWFL_E_NOMEM);
      return NULL;
    }

  ssize_t nread = (*read_memory) (arg, buffer, ehdr_vma,
				  sizeof (Elf32_Ehdr), initial_bufsize);
  if (nread <= 0)
    {
    read_error:
      free (buffer);
      __libdwfl_seterrno (nread < 0 ? DWFL_E_ERRNO : DWFL_E_TRUNCATED);
      return NULL;
    }

  if (memcmp (buffer, ELFMAG, SELFMAG) != 0)
    {
    bad_elf:
      free (buffer);
      __libdwfl_seterrno (DWFL_E_BADELF);
      return NULL;
    }

  

  union
  {
    Elf32_Ehdr e32;
    Elf64_Ehdr e64;
  } ehdr;
  Elf_Data xlatefrom =
    {
      .d_type = ELF_T_EHDR,
      .d_buf = buffer,
      .d_version = EV_CURRENT,
    };
  Elf_Data xlateto =
    {
      .d_type = ELF_T_EHDR,
      .d_buf = &ehdr,
      .d_size = sizeof ehdr,
      .d_version = EV_CURRENT,
    };

  GElf_Off phoff;
  uint_fast16_t phnum;
  uint_fast16_t phentsize;
  GElf_Off shdrs_end;

  switch (buffer[EI_CLASS])
    {
    case ELFCLASS32:
      xlatefrom.d_size = sizeof (Elf32_Ehdr);
      if (elf32_xlatetom (&xlateto, &xlatefrom, buffer[EI_DATA]) == NULL)
	{
	libelf_error:
	  __libdwfl_seterrno (DWFL_E_LIBELF);
	  return NULL;
	}
      phoff = ehdr.e32.e_phoff;
      phnum = ehdr.e32.e_phnum;
      phentsize = ehdr.e32.e_phentsize;
      if (phentsize != sizeof (Elf32_Phdr) || phnum == 0)
	goto bad_elf;
      shdrs_end = ehdr.e32.e_shoff + ehdr.e32.e_shnum * ehdr.e32.e_shentsize;
      break;

    case ELFCLASS64:
      xlatefrom.d_size = sizeof (Elf64_Ehdr);
      if (elf64_xlatetom (&xlateto, &xlatefrom, buffer[EI_DATA]) == NULL)
	goto libelf_error;
      phoff = ehdr.e64.e_phoff;
      phnum = ehdr.e64.e_phnum;
      phentsize = ehdr.e64.e_phentsize;
      if (phentsize != sizeof (Elf64_Phdr) || phnum == 0)
	goto bad_elf;
      shdrs_end = ehdr.e64.e_shoff + ehdr.e64.e_shnum * ehdr.e64.e_shentsize;
      break;

    default:
      goto bad_elf;
    }



  xlatefrom.d_type = xlateto.d_type = ELF_T_PHDR;
  xlatefrom.d_size = phnum * phentsize;

  if ((size_t) nread >= phoff + phnum * phentsize)
    
    xlatefrom.d_buf = buffer + phoff;
  else
    {
      

      if (initial_bufsize < phnum * phentsize)
	{
	  unsigned char *newbuf = realloc (buffer, phnum * phentsize);
	  if (newbuf == NULL)
	    {
	      free (buffer);
	      goto no_memory;
	    }
	  buffer = newbuf;
	}
      nread = (*read_memory) (arg, buffer, ehdr_vma + phoff,
			      phnum * phentsize, phnum * phentsize);
      if (nread <= 0)
	goto read_error;

      xlatefrom.d_buf = buffer;
    }

  union
  {
    Elf32_Phdr p32[phnum];
    Elf64_Phdr p64[phnum];
  } phdrs;

  xlateto.d_buf = &phdrs;
  xlateto.d_size = sizeof phdrs;

  
  size_t contents_size = 0;
  GElf_Off segments_end = 0;
  GElf_Off segments_end_mem = 0;
  GElf_Addr loadbase = ehdr_vma;
  bool found_base = false;
  switch (ehdr.e32.e_ident[EI_CLASS])
    {
      inline bool handle_segment (GElf_Addr vaddr, GElf_Off offset,
				  GElf_Xword filesz, GElf_Xword memsz)
	{
	  
	  if (((vaddr - offset) & (pagesize - 1)) != 0)
	    return true;

	  GElf_Off segment_end = ((offset + filesz + pagesize - 1)
				  & -pagesize);

	  if (segment_end > (GElf_Off) contents_size)
	    contents_size = segment_end;

	  if (!found_base && (offset & -pagesize) == 0)
	    {
	      loadbase = ehdr_vma - (vaddr & -pagesize);
	      found_base = true;
	    }

	  segments_end = offset + filesz;
	  segments_end_mem = offset + memsz;
	  return false;
	}

    case ELFCLASS32:
      if (elf32_xlatetom (&xlateto, &xlatefrom,
			  ehdr.e32.e_ident[EI_DATA]) == NULL)
	goto libelf_error;
      for (uint_fast16_t i = 0; i < phnum; ++i)
	if (phdrs.p32[i].p_type == PT_LOAD)
	  if (handle_segment (phdrs.p32[i].p_vaddr, phdrs.p32[i].p_offset,
			      phdrs.p32[i].p_filesz, phdrs.p32[i].p_memsz))
	    goto bad_elf;
      break;

    case ELFCLASS64:
      if (elf64_xlatetom (&xlateto, &xlatefrom,
			  ehdr.e64.e_ident[EI_DATA]) == NULL)
	goto libelf_error;
      for (uint_fast16_t i = 0; i < phnum; ++i)
	if (phdrs.p64[i].p_type == PT_LOAD)
	  if (handle_segment (phdrs.p64[i].p_vaddr, phdrs.p64[i].p_offset,
			      phdrs.p64[i].p_filesz, phdrs.p64[i].p_memsz))
	    goto bad_elf;
      break;

    default:
      abort ();
      break;
    }

  if ((GElf_Off) contents_size > segments_end
      && (GElf_Off) contents_size >= shdrs_end
      && segments_end == segments_end_mem)
    {
      contents_size = segments_end;
      if ((GElf_Off) contents_size < shdrs_end)
	contents_size = shdrs_end;
    }
  else
    contents_size = segments_end;

  free (buffer);

  
  buffer = calloc (1, contents_size);
  if (buffer == NULL)
    goto no_memory;

  switch (ehdr.e32.e_ident[EI_CLASS])
    {
      inline bool handle_segment (GElf_Addr vaddr, GElf_Off offset,
				  GElf_Xword filesz)
	{
	  GElf_Off start = offset & -pagesize;
	  GElf_Off end = (offset + filesz + pagesize - 1) & -pagesize;
	  if (end > (GElf_Off) contents_size)
	    end = contents_size;
	  nread = (*read_memory) (arg, buffer + start,
				  (loadbase + vaddr) & -pagesize,
				  end - start, end - start);
	  return nread <= 0;
	}

    case ELFCLASS32:
      for (uint_fast16_t i = 0; i < phnum; ++i)
	if (phdrs.p32[i].p_type == PT_LOAD)
	  if (handle_segment (phdrs.p32[i].p_vaddr, phdrs.p32[i].p_offset,
			      phdrs.p32[i].p_filesz))
	    goto read_error;

      if (contents_size < shdrs_end)
	{
	  ehdr.e32.e_shoff = 0;
	  ehdr.e32.e_shnum = 0;
	  ehdr.e32.e_shstrndx = 0;
	}

      xlatefrom.d_type = xlateto.d_type = ELF_T_EHDR;
      xlatefrom.d_size = xlateto.d_size = sizeof ehdr.e32;
      xlatefrom.d_buf = &ehdr.e32;
      xlateto.d_buf = buffer;
      if (elf32_xlatetof (&xlateto, &xlatefrom,
			  ehdr.e32.e_ident[EI_DATA]) == NULL)
	goto libelf_error;
      break;

    case ELFCLASS64:
      for (uint_fast16_t i = 0; i < phnum; ++i)
	if (phdrs.p64[i].p_type == PT_LOAD)
	  if (handle_segment (phdrs.p64[i].p_vaddr, phdrs.p64[i].p_offset,
			      phdrs.p64[i].p_filesz))
	    goto read_error;

      if (contents_size < shdrs_end)
	{
	  ehdr.e64.e_shoff = 0;
	  ehdr.e64.e_shnum = 0;
	  ehdr.e64.e_shstrndx = 0;
	}

      xlatefrom.d_type = xlateto.d_type = ELF_T_EHDR;
      xlatefrom.d_size = xlateto.d_size = sizeof ehdr.e64;
      xlatefrom.d_buf = &ehdr.e64;
      xlateto.d_buf = buffer;
      if (elf64_xlatetof (&xlateto, &xlatefrom,
			  ehdr.e64.e_ident[EI_DATA]) == NULL)
	goto libelf_error;
      break;

    default:
      abort ();
      break;
    }

  

  Elf *elf = elf_memory ((char *) buffer, contents_size);
  if (elf == NULL)
    {
      free (buffer);
      goto libelf_error;
    }

  elf->flags |= ELF_F_MALLOCED;
  if (loadbasep != NULL)
    *loadbasep = loadbase;
  return elf;
}
