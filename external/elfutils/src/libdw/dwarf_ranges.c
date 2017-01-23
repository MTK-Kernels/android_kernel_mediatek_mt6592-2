/* Enumerate the PC ranges covered by a DIE.
   Copyright (C) 2005, 2007, 2009 Red Hat, Inc.
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

#include "libdwP.h"
#include <dwarf.h>
#include <assert.h>

internal_function int
__libdw_read_begin_end_pair_inc (Dwarf *dbg, int sec_index,
				 unsigned char **addrp, int width,
				 Dwarf_Addr *beginp, Dwarf_Addr *endp,
				 Dwarf_Addr *basep)
{
  Dwarf_Addr escape = (width == 8 ? (Elf64_Addr) -1
		       : (Elf64_Addr) (Elf32_Addr) -1);
  Dwarf_Addr begin;
  Dwarf_Addr end;

  unsigned char *addr = *addrp;
  bool begin_relocated = READ_AND_RELOCATE (__libdw_relocate_address, begin);
  bool end_relocated = READ_AND_RELOCATE (__libdw_relocate_address, end);
  *addrp = addr;

  
  if (begin == escape && !begin_relocated)
    {
      if (unlikely (end == escape))
	{
	  __libdw_seterrno (DWARF_E_INVALID_DWARF);
	  return -1;
	}

      if (basep != NULL)
	*basep = end;
      return 1;
    }

  
  if (begin == 0 && end == 0 && !begin_relocated && !end_relocated)
    return 2;

  *beginp = begin;
  *endp = end;

  return 0;
}

ptrdiff_t
dwarf_ranges (Dwarf_Die *die, ptrdiff_t offset, Dwarf_Addr *basep,
	      Dwarf_Addr *startp, Dwarf_Addr *endp)
{
  if (die == NULL)
    return -1;

  if (offset == 0
      
      && INTUSE(dwarf_highpc) (die, endp) == 0
      && INTUSE(dwarf_lowpc) (die, startp) == 0)
    return 1;

  if (offset == 1)
    return 0;

  

  const Elf_Data *d = die->cu->dbg->sectiondata[IDX_debug_ranges];
  if (d == NULL && offset != 0)
    {
      __libdw_seterrno (DWARF_E_NO_DEBUG_RANGES);
      return -1;
    }

  unsigned char *readp;
  unsigned char *readendp;
  if (offset == 0)
    {
      Dwarf_Attribute attr_mem;
      Dwarf_Attribute *attr = INTUSE(dwarf_attr) (die, DW_AT_ranges,
						  &attr_mem);
      if (attr == NULL)
	
	return 0;

      Dwarf_Word start_offset;
      if ((readp = __libdw_formptr (attr, IDX_debug_ranges,
				    DWARF_E_NO_DEBUG_RANGES,
				    &readendp, &start_offset)) == NULL)
	return -1;

      offset = start_offset;
      assert ((Dwarf_Word) offset == start_offset);

      
      Dwarf_Die cudie = CUDIE (attr->cu);

      if (unlikely (INTUSE(dwarf_lowpc) (&cudie, basep) != 0)
	  && INTUSE(dwarf_formaddr) (INTUSE(dwarf_attr) (&cudie,
							 DW_AT_entry_pc,
							 &attr_mem),
				     basep) != 0)
	{
	  if (INTUSE(dwarf_errno) () == 0)
	    {
	    invalid:
	      __libdw_seterrno (DWARF_E_INVALID_DWARF);
	    }
	  return -1;
	}
    }
  else
    {
      if (__libdw_offset_in_section (die->cu->dbg,
				     IDX_debug_ranges, offset, 1))
	return -1l;

      readp = d->d_buf + offset;
      readendp = d->d_buf + d->d_size;
    }

 next:
  if (readendp - readp < die->cu->address_size * 2)
    goto invalid;

  Dwarf_Addr begin;
  Dwarf_Addr end;

  switch (__libdw_read_begin_end_pair_inc (die->cu->dbg, IDX_debug_ranges,
					   &readp, die->cu->address_size,
					   &begin, &end, basep))
    {
    case 0:
      break;
    case 1:
      goto next;
    case 2:
      return 0;
    default:
      return -1l;
    }

  
  *startp = *basep + begin;
  *endp = *basep + end;
  return readp - (unsigned char *) d->d_buf;
}
INTDEF (dwarf_ranges)
