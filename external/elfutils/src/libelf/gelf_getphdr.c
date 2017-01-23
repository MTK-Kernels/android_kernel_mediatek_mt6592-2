/* Return program header table entry.
   Copyright (C) 1998-2010 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 1998.

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <gelf.h>
#include <string.h>
#include <stdbool.h>

#include "libelfP.h"


GElf_Phdr *
gelf_getphdr (elf, ndx, dst)
     Elf *elf;
     int ndx;
     GElf_Phdr *dst;
{
  GElf_Phdr *result = NULL;

  if (elf == NULL)
    return NULL;

  if (unlikely (elf->kind != ELF_K_ELF))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  if (dst == NULL)
    {
      __libelf_seterrno (ELF_E_INVALID_OPERAND);
      return NULL;
    }

  rwlock_rdlock (elf->lock);

  if (elf->class == ELFCLASS32)
    {
      
      Elf32_Phdr *phdr = elf->state.elf32.phdr;

      if (phdr == NULL)
	{
	  rwlock_unlock (elf->lock);
	  phdr = INTUSE(elf32_getphdr) (elf);
	  if (phdr == NULL)
	    
	    return NULL;
	  rwlock_rdlock (elf->lock);
	}

      
      size_t phnum;
      if (ndx >= elf->state.elf32.ehdr->e_phnum
	  && (elf->state.elf32.ehdr->e_phnum != PN_XNUM
	      || __elf_getphdrnum_rdlock (elf, &phnum) != 0
	      || (size_t) ndx >= phnum))
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  goto out;
	}

      
      result = dst;

      
      phdr += ndx;

#define COPY(Name) result->Name = phdr->Name
      COPY (p_type);
      COPY (p_offset);
      COPY (p_vaddr);
      COPY (p_paddr);
      COPY (p_filesz);
      COPY (p_memsz);
      COPY (p_flags);
      COPY (p_align);
    }
  else
    {
      
      Elf64_Phdr *phdr = elf->state.elf64.phdr;

      if (phdr == NULL)
	{
	  rwlock_unlock (elf->lock);
	  phdr = INTUSE(elf64_getphdr) (elf);
	  if (phdr == NULL)
	    
	    return NULL;
	  rwlock_rdlock (elf->lock);
	}

      
      size_t phnum;
      if (ndx >= elf->state.elf64.ehdr->e_phnum
	  && (elf->state.elf64.ehdr->e_phnum != PN_XNUM
	      || __elf_getphdrnum_rdlock (elf, &phnum) != 0
	      || (size_t) ndx >= phnum))
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  goto out;
	}

      
      result = memcpy (dst, phdr + ndx, sizeof (GElf_Phdr));
    }

 out:
  rwlock_unlock (elf->lock);

  return result;
}
