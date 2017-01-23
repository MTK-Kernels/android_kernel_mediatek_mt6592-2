/* Get symbol information and separate section index from symbol table
   at the given index.
   Copyright (C) 2000, 2001, 2002, 2005, 2009, 2014 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

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

#include <assert.h>
#include <gelf.h>
#include <string.h>

#include "libelfP.h"


GElf_Sym *
gelf_getsymshndx (symdata, shndxdata, ndx, dst, dstshndx)
     Elf_Data *symdata;
     Elf_Data *shndxdata;
     int ndx;
     GElf_Sym *dst;
     Elf32_Word *dstshndx;
{
  Elf_Data_Scn *symdata_scn = (Elf_Data_Scn *) symdata;
  Elf_Data_Scn *shndxdata_scn = (Elf_Data_Scn *) shndxdata;
  GElf_Sym *result = NULL;
  Elf32_Word shndx = 0;

  if (symdata == NULL)
    return NULL;

  if (unlikely (symdata->d_type != ELF_T_SYM)
      || (likely (shndxdata_scn != NULL)
	  && unlikely (shndxdata->d_type != ELF_T_WORD)))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  rwlock_rdlock (symdata_scn->s->elf->lock);

  if (likely (shndxdata_scn != NULL))
    {
      if (INVALID_NDX (ndx, Elf32_Word, &shndxdata_scn->d))
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  goto out;
	}

      shndx = ((Elf32_Word *) shndxdata_scn->d.d_buf)[ndx];
    }

  if (symdata_scn->s->elf->class == ELFCLASS32)
    {
      Elf32_Sym *src;

      if (INVALID_NDX (ndx, Elf32_Sym, symdata))
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  goto out;
	}

      src = &((Elf32_Sym *) symdata->d_buf)[ndx];

#define COPY(name) \
      dst->name = src->name
      COPY (st_name);
      COPY (st_info);
      COPY (st_other);
      COPY (st_shndx);
      COPY (st_value);
      COPY (st_size);
    }
  else
    {
      
      assert (sizeof (GElf_Sym) == sizeof (Elf64_Sym));

      if (INVALID_NDX (ndx, GElf_Sym, symdata))
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  goto out;
	}

      *dst = ((GElf_Sym *) symdata->d_buf)[ndx];
    }

  
  if (dstshndx != NULL)
    *dstshndx = shndx;

  result = dst;

 out:
  rwlock_unlock (symdata_scn->s->elf->lock);

  return result;
}
