/* Return raw section content.
   Copyright (C) 1998, 1999, 2000, 2002 Red Hat, Inc.
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

#include <stdlib.h>

#include "libelfP.h"


Elf_Data *
elf_rawdata (scn, data)
     Elf_Scn *scn;
     Elf_Data *data;
{
  if (scn == NULL || scn->elf->kind != ELF_K_ELF)
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  if (data != NULL
      || (scn->data_read != 0 && (scn->flags & ELF_F_FILEDATA) == 0))
    {
      __libelf_seterrno (ELF_E_DATA_MISMATCH);
      return NULL;
    }

  
  if (scn->data_read == 0)
    {
      if (__libelf_set_rawdata (scn) != 0)
	
	return NULL;
    }

  
  return &scn->rawdata.d;
}
INTDEF(elf_rawdata)
