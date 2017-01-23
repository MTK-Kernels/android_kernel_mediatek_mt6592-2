/* Return converted data from raw chunk of ELF file.
   Copyright (C) 2007, 2014 Red Hat, Inc.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <system.h>
#include "libelfP.h"
#include "common.h"

Elf_Data *
elf_getdata_rawchunk (elf, offset, size, type)
     Elf *elf;
     off64_t offset;
     size_t size;
     Elf_Type type;
{
  if (unlikely (elf == NULL))
    return NULL;

  if (unlikely (elf->kind != ELF_K_ELF))
    {
      
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  if (unlikely (offset < 0 || (uint64_t) offset > elf->maximum_size
		|| elf->maximum_size - (uint64_t) offset < size))

    {
      
      __libelf_seterrno (ELF_E_INVALID_OP);
      return NULL;
    }

  if (type >= ELF_T_NUM)
    {
      __libelf_seterrno (ELF_E_UNKNOWN_TYPE);
      return NULL;
    }

  
  void *rawchunk;
  int flags = 0;
  Elf_Data *result = NULL;

  rwlock_rdlock (elf->lock);

  
  if (elf->map_address != NULL)
    rawchunk = elf->map_address + elf->start_offset + offset;
  else
    {
      
      rawchunk = malloc (size);
      if (rawchunk == NULL)
	{
	nomem:
	  __libelf_seterrno (ELF_E_NOMEM);
	  goto out;
	}

      
      if (unlikely ((size_t) pread_retry (elf->fildes, rawchunk, size,
					  elf->start_offset + offset)
		    != size))
	{
	  
	  free (rawchunk);
	  __libelf_seterrno (ELF_E_READ_ERROR);
	  goto out;
	}

      flags = ELF_F_MALLOCED;
    }

  
  size_t align = __libelf_type_align (elf->class, type);
  void *buffer;
  if (elf->state.elf32.ehdr->e_ident[EI_DATA] == MY_ELFDATA)
    {
      if (((uintptr_t) rawchunk & (align - 1)) == 0)
	
	buffer = rawchunk;
      else
	{
	  
	  assert (flags == 0);

	  buffer = malloc (size);
	  if (unlikely (buffer == NULL))
	    goto nomem;
	  flags = ELF_F_MALLOCED;

	  
	  memcpy (buffer, rawchunk, size);
	}
    }
  else
    {
      if (flags)
	buffer = rawchunk;
      else
	{
	  buffer = malloc (size);
	  if (unlikely (buffer == NULL))
	    goto nomem;
	  flags = ELF_F_MALLOCED;
	}

      
      (*__elf_xfctstom[LIBELF_EV_IDX][LIBELF_EV_IDX][elf->class - 1][type])
	(buffer, rawchunk, size, 0);
    }

  
  Elf_Data_Chunk *chunk = calloc (1, sizeof *chunk);
  if (chunk == NULL)
    {
      if (flags)
	free (buffer);
      goto nomem;
    }

  chunk->dummy_scn.elf = elf;
  chunk->dummy_scn.flags = flags;
  chunk->data.s = &chunk->dummy_scn;
  chunk->data.d.d_buf = buffer;
  chunk->data.d.d_size = size;
  chunk->data.d.d_type = type;
  chunk->data.d.d_align = align;
  chunk->data.d.d_version = __libelf_version;

  rwlock_unlock (elf->lock);
  rwlock_wrlock (elf->lock);

  chunk->next = elf->state.elf.rawchunks;
  elf->state.elf.rawchunks = chunk;
  result = &chunk->data.d;

 out:
  rwlock_unlock (elf->lock);
  return result;
}
