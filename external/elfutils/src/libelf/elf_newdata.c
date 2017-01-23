/* Create new, empty section data.
   Copyright (C) 1998, 1999, 2000, 2001, 2002 Red Hat, Inc.
   This file is part of elfutils.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 1998.

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

#include <stddef.h>
#include <stdlib.h>

#include "libelfP.h"


Elf_Data *
elf_newdata (Elf_Scn *scn)
{
  Elf_Data_List *result = NULL;

  if (scn == NULL)
    return NULL;

  if (unlikely (scn->index == 0))
    {
      
      __libelf_seterrno (ELF_E_NOT_NUL_SECTION);
      return NULL;
    }

  if (scn->elf->class == ELFCLASS32
      || (offsetof (struct Elf, state.elf32.ehdr)
	  == offsetof (struct Elf, state.elf64.ehdr))
      ? scn->elf->state.elf32.ehdr == NULL
      : scn->elf->state.elf64.ehdr == NULL)
    {
      __libelf_seterrno (ELF_E_WRONG_ORDER_EHDR);
      return NULL;
    }

  rwlock_wrlock (scn->elf->lock);

  if (scn->data_read && scn->data_list_rear == NULL)
    {
      result = &scn->data_list;
      result->flags = ELF_F_DIRTY;
    }
  else
    {
      
      result = (Elf_Data_List *) calloc (1, sizeof (Elf_Data_List));
      if (result == NULL)
	{
	  __libelf_seterrno (ELF_E_NOMEM);
	  goto out;
	}

      result->flags = ELF_F_DIRTY | ELF_F_MALLOCED;

      if (scn->data_list_rear == NULL)
	scn->data_list_rear = &scn->data_list;
    }

  
  result->data.d.d_version = __libelf_version;

  result->data.s = scn;

  
  if (scn->data_list_rear != NULL)
    scn->data_list_rear->next = result;

  scn->data_list_rear = result;

 out:
  rwlock_unlock (scn->elf->lock);

  return &result->data.d;
}
