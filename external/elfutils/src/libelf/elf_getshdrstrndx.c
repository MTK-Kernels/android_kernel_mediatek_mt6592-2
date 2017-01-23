/* Return section index of section header string table.
   Copyright (C) 2002, 2005, 2009, 2014 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

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
#include <gelf.h>
#include <stddef.h>
#include <unistd.h>

#include <system.h>
#include "libelfP.h"
#include "common.h"


int
elf_getshdrstrndx (elf, dst)
     Elf *elf;
     size_t *dst;
{
  int result = 0;

  if (elf == NULL)
    return -1;

  if (unlikely (elf->kind != ELF_K_ELF))
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return -1;
    }

  rwlock_rdlock (elf->lock);

  assert (offsetof (struct Elf, state.elf.ehdr)
	  == offsetof (struct Elf, state.elf32.ehdr));
  assert (sizeof (elf->state.elf.ehdr)
	  == sizeof (elf->state.elf32.ehdr));
  assert (offsetof (struct Elf, state.elf.ehdr)
	  == offsetof (struct Elf, state.elf64.ehdr));
  assert (sizeof (elf->state.elf.ehdr)
	  == sizeof (elf->state.elf64.ehdr));

  if (unlikely (elf->state.elf.ehdr == NULL))
    {
      __libelf_seterrno (ELF_E_WRONG_ORDER_EHDR);
      result = -1;
    }
  else
    {
      Elf32_Word num;

      num = (elf->class == ELFCLASS32
	     ? elf->state.elf32.ehdr->e_shstrndx
	     : elf->state.elf64.ehdr->e_shstrndx);

      if (unlikely (num == SHN_XINDEX))
	{
	  
	  if (elf->class == ELFCLASS32)
	    {
	      size_t offset;
	      if (unlikely (elf->state.elf32.scns.cnt == 0))
		{
		  
		  __libelf_seterrno (ELF_E_INVALID_SECTION_HEADER);
		  result = -1;
		  goto out;
		}

	      if (elf->state.elf32.scns.data[0].shdr.e32 != NULL)
		{
		  num = elf->state.elf32.scns.data[0].shdr.e32->sh_link;
		  goto success;
		}

	      offset = elf->state.elf32.ehdr->e_shoff;

	      if (elf->map_address != NULL
		  && elf->state.elf32.ehdr->e_ident[EI_DATA] == MY_ELFDATA
		  && (ALLOW_UNALIGNED
		      || (((size_t) ((char *) elf->map_address
			   + elf->start_offset + offset))
			  & (__alignof__ (Elf32_Shdr) - 1)) == 0))
		{
		  if (unlikely (elf->maximum_size - offset
				< sizeof (Elf32_Shdr)))
		    {
		      
		      __libelf_seterrno (ELF_E_INVALID_SECTION_HEADER);
		      result = -1;
		      goto out;
		    }

		  
		  num = ((Elf32_Shdr *) (elf->map_address + elf->start_offset
					 + offset))->sh_link;
		}
	      else
		{
		  Elf32_Shdr shdr_mem;

		  if (unlikely (pread_retry (elf->fildes, &shdr_mem,
					     sizeof (Elf32_Shdr), offset)
				!= sizeof (Elf32_Shdr)))
		    {
		      
		      __libelf_seterrno (ELF_E_INVALID_FILE);
		      result = -1;
		      goto out;
		    }

		  if (elf->state.elf32.ehdr->e_ident[EI_DATA] != MY_ELFDATA)
		    CONVERT (shdr_mem.sh_link);
		  num = shdr_mem.sh_link;
		}
	    }
	  else
	    {
	      if (unlikely (elf->state.elf64.scns.cnt == 0))
		{
		  
		  __libelf_seterrno (ELF_E_INVALID_SECTION_HEADER);
		  result = -1;
		  goto out;
		}

	      if (elf->state.elf64.scns.data[0].shdr.e64 != NULL)
		{
		  num = elf->state.elf64.scns.data[0].shdr.e64->sh_link;
		  goto success;
		}

	      size_t offset = elf->state.elf64.ehdr->e_shoff;

	      if (elf->map_address != NULL
		  && elf->state.elf64.ehdr->e_ident[EI_DATA] == MY_ELFDATA
		  && (ALLOW_UNALIGNED
		      || (((size_t) ((char *) elf->map_address
			   + elf->start_offset + offset))
			  & (__alignof__ (Elf64_Shdr) - 1)) == 0))
		{
		  if (unlikely (elf->maximum_size - offset
				< sizeof (Elf64_Shdr)))
		    {
		      
		      __libelf_seterrno (ELF_E_INVALID_SECTION_HEADER);
		      result = -1;
		      goto out;
		    }

		  
		  num = ((Elf64_Shdr *) (elf->map_address + elf->start_offset
					 + offset))->sh_link;
		}
	      else
		{
		  Elf64_Shdr shdr_mem;

		  if (unlikely (pread_retry (elf->fildes, &shdr_mem,
					     sizeof (Elf64_Shdr), offset)
				!= sizeof (Elf64_Shdr)))
		    {
		      
		      __libelf_seterrno (ELF_E_INVALID_FILE);
		      result = -1;
		      goto out;
		    }

		  if (elf->state.elf64.ehdr->e_ident[EI_DATA] != MY_ELFDATA)
		    CONVERT (shdr_mem.sh_link);
		  num = shdr_mem.sh_link;
		}
	    }
	}

      
    success:
      *dst = num;
    }

 out:
  rwlock_unlock (elf->lock);

  return result;
}
INTDEF(elf_getshdrstrndx)
strong_alias (elf_getshdrstrndx, elf_getshstrndx)
