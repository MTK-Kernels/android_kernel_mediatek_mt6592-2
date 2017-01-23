/* Get information from dynamic table at the given index.
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


GElf_Dyn *
gelf_getdyn (data, ndx, dst)
     Elf_Data *data;
     int ndx;
     GElf_Dyn *dst;
{
  Elf_Data_Scn *data_scn = (Elf_Data_Scn *) data;
  GElf_Dyn *result = NULL;
  Elf *elf;

  if (data_scn == NULL)
    return NULL;

  if (unlikely (data_scn->d.d_type != ELF_T_DYN))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  elf = data_scn->s->elf;

  rwlock_rdlock (elf->lock);

  if (elf->class == ELFCLASS32)
    {
      Elf32_Dyn *src;

      if (INVALID_NDX (ndx, Elf32_Dyn, &data_scn->d))
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  goto out;
	}

      src = &((Elf32_Dyn *) data_scn->d.d_buf)[ndx];

      dst->d_tag = src->d_tag;
      
      dst->d_un.d_val = src->d_un.d_val;
    }
  else
    {
      
      assert (sizeof (GElf_Dyn) == sizeof (Elf64_Dyn));

      if (INVALID_NDX (ndx, GElf_Dyn, &data_scn->d))
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  goto out;
	}

      *dst = ((GElf_Dyn *) data_scn->d.d_buf)[ndx];
    }

  result = dst;

 out:
  rwlock_unlock (elf->lock);

  return result;
}
