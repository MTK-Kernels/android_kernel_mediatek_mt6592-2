/* Convert from memory to file representation.
   Copyright (C) 1998, 1999, 2000, 2002 Red Hat, Inc.
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

#include <assert.h>
#include <endian.h>
#include <string.h>

#include "libelfP.h"

#ifndef LIBELFBITS
# define LIBELFBITS	32
#endif


Elf_Data *
elfw2(LIBELFBITS, xlatetof) (dest, src, encode)
     Elf_Data *dest;
     const Elf_Data *src;
     unsigned int encode;
{
#if EV_NUM != 2
  size_t recsize = __libelf_type_sizes[src->d_version - 1][ELFW(ELFCLASS,LIBELFBITS) - 1][src->d_type];
#else
  size_t recsize = __libelf_type_sizes[0][ELFW(ELFCLASS,LIBELFBITS) - 1][src->d_type];
#endif

  if (src->d_size % recsize != 0)
    {
      __libelf_seterrno (ELF_E_INVALID_DATA);
      return NULL;
    }

  
  if (src->d_size > dest->d_size)
    {
      __libelf_seterrno (ELF_E_DEST_SIZE);
      return NULL;
    }

  
  if (encode != ELFDATA2LSB && encode != ELFDATA2MSB)
    {
      __libelf_seterrno (ELF_E_INVALID_ENCODING);
      return NULL;
    }

  if ((__BYTE_ORDER == __LITTLE_ENDIAN && encode == ELFDATA2LSB)
      || (__BYTE_ORDER == __BIG_ENDIAN && encode == ELFDATA2MSB))
    {
      
      if (src->d_buf != dest->d_buf)
	memmove (dest->d_buf, src->d_buf, src->d_size);
    }
  else
    {
      xfct_t fctp;

#if EV_NUM != 2
      fctp = __elf_xfctstom[dest->d_version - 1][src->d_version - 1][ELFW(ELFCLASS, LIBELFBITS) - 1][src->d_type];
#else
      fctp = __elf_xfctstom[0][0][ELFW(ELFCLASS, LIBELFBITS) - 1][src->d_type];
#endif

      
      (*fctp) (dest->d_buf, src->d_buf, src->d_size, 1);
    }

  dest->d_type = src->d_type;
  dest->d_size = src->d_size;

  return dest;
}
INTDEF(elfw2(LIBELFBITS, xlatetof))
