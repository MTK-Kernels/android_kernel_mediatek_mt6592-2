/* Return tag of given DIE.
   Copyright (C) 2003-2011, 2014 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2003.

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


Dwarf_Abbrev *
internal_function
__libdw_findabbrev (struct Dwarf_CU *cu, unsigned int code)
{
  Dwarf_Abbrev *abb;

  
  if (unlikely (code == 0))
    return DWARF_END_ABBREV;

  
  abb = Dwarf_Abbrev_Hash_find (&cu->abbrev_hash, code, NULL);
  if (abb == NULL)
    while (cu->last_abbrev_offset != (size_t) -1l)
      {
	size_t length;

	abb = __libdw_getabbrev (cu->dbg, cu, cu->last_abbrev_offset, &length,
				 NULL);
	if (abb == NULL || abb == DWARF_END_ABBREV)
	  {
	    
	    cu->last_abbrev_offset = (size_t) -1l;
	    return DWARF_END_ABBREV;
	  }

	cu->last_abbrev_offset += length;

	
	if (abb->code == code)
	  break;
      }

  if (unlikely (abb == NULL))
    abb = DWARF_END_ABBREV;

  return abb;
}


int
dwarf_tag (die)
     Dwarf_Die *die;
{
  
  Dwarf_Abbrev *abbrevp = __libdw_dieabbrev (die, NULL);
  if (unlikely (abbrevp == DWARF_END_ABBREV))
    {
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return DW_TAG_invalid;
    }

  return abbrevp->tag;
}
INTDEF(dwarf_tag)
