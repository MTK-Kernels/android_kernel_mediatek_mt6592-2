/* Compute simple checksum from permanent parts of the ELF file.
   Copyright (C) 2002, 2003, 2004, 2005, 2009 Red Hat, Inc.
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
#include <endian.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "gelf.h"
#include "libelfP.h"
#include "elf-knowledge.h"

#ifndef LIBELFBITS
# define LIBELFBITS 32
#endif


#define process_block(crc, data) \
  __libelf_crc32 (crc, data->d_buf, data->d_size)


long int
elfw2(LIBELFBITS,checksum) (elf)
     Elf *elf;
{
  size_t shstrndx;
  Elf_Scn *scn;
  long int result = 0;
  unsigned char *ident;
  bool same_byte_order;

  if (elf == NULL)
    return -1l;

  
  if  (INTUSE(elf_getshdrstrndx) (elf, &shstrndx) < 0)
    {
      
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return -1l;
    }

  ident = elf->state.ELFW(elf,LIBELFBITS).ehdr->e_ident;
  same_byte_order = ((ident[EI_DATA] == ELFDATA2LSB
		      && __BYTE_ORDER == __LITTLE_ENDIAN)
		     || (ident[EI_DATA] == ELFDATA2MSB
			 && __BYTE_ORDER == __BIG_ENDIAN));

  if (!likely (same_byte_order))
    rwlock_wrlock (elf->lock);
  else
    rwlock_rdlock (elf->lock);

  
  scn = NULL;
  while ((scn = INTUSE(elf_nextscn) (elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr;
      Elf_Data *data;

      
      shdr = INTUSE(gelf_getshdr) (scn, &shdr_mem);
      if (shdr == NULL)
	{
	  __libelf_seterrno (ELF_E_INVALID_SECTION_HEADER);
	  result = -1l;
	  goto out;
	}

      if (SECTION_STRIP_P (shdr,
			   INTUSE(elf_strptr) (elf, shstrndx, shdr->sh_name),
			   true))
	
	continue;

      
      if (shdr->sh_type == SHT_NOBITS)
	continue;

      data = INTUSE(elf_rawdata) (scn, NULL);
      if (data != NULL)
	{
	  
	  result = process_block (result, data);

	}

      
      while ((data = INTUSE(elf_getdata) (scn, data)) != NULL)
	if (likely (same_byte_order) || data->d_type == ELF_T_BYTE)
	  result = process_block (result, data);
	else
	  {
	    
	    if (INTUSE(elfw2(LIBELFBITS,xlatetof)) (data, data, ident[EI_DATA])
		== NULL)
	      {
		result = -1l;
		goto out;
	      }

	    result = process_block (result, data);

	    
	    if (INTUSE(elfw2(LIBELFBITS,xlatetom)) (data, data, ident[EI_DATA])
		== NULL)
	      {
		result = -1l;
		goto out;
	      }
	  }
    }

 out:
  rwlock_unlock (elf->lock);
  return result;
}
INTDEF(elfw2(LIBELFBITS,checksum))
