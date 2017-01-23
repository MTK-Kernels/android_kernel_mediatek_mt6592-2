/* Get abbreviation at given offset.
   Copyright (C) 2003, 2004, 2005, 2006, 2014 Red Hat, Inc.
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

#include <dwarf.h>
#include "libdwP.h"


Dwarf_Abbrev *
internal_function
__libdw_getabbrev (dbg, cu, offset, lengthp, result)
     Dwarf *dbg;
     struct Dwarf_CU *cu;
     Dwarf_Off offset;
     size_t *lengthp;
     Dwarf_Abbrev *result;
{
  
  if (dbg->sectiondata[IDX_debug_abbrev] == NULL)
    return NULL;

  if (offset >= dbg->sectiondata[IDX_debug_abbrev]->d_size)
    {
      __libdw_seterrno (DWARF_E_INVALID_OFFSET);
      return NULL;
    }

  const unsigned char *abbrevp
    = (unsigned char *) dbg->sectiondata[IDX_debug_abbrev]->d_buf + offset;

  if (*abbrevp == '\0')
    
    return DWARF_END_ABBREV;

  const unsigned char *end = (dbg->sectiondata[IDX_debug_abbrev]->d_buf
			      + dbg->sectiondata[IDX_debug_abbrev]->d_size);
  const unsigned char *start_abbrevp = abbrevp;
  unsigned int code;
  get_uleb128 (code, abbrevp, end);

  
  bool foundit = false;
  Dwarf_Abbrev *abb = NULL;
  if (cu == NULL
      || (abb = Dwarf_Abbrev_Hash_find (&cu->abbrev_hash, code, NULL)) == NULL)
    {
      if (result == NULL)
	abb = libdw_typed_alloc (dbg, Dwarf_Abbrev);
      else
	abb = result;
    }
  else
    {
      foundit = true;

      if (unlikely (abb->offset != offset))
	{
	invalid:
	  __libdw_seterrno (DWARF_E_INVALID_DWARF);
	  return NULL;
	}

      
      if (lengthp == NULL)
	goto out;
    }

  abb->code = code;
  if (abbrevp >= end)
    goto invalid;
  get_uleb128 (abb->tag, abbrevp, end);
  if (abbrevp + 1 >= end)
    goto invalid;
  abb->has_children = *abbrevp++ == DW_CHILDREN_yes;
  abb->attrp = (unsigned char *) abbrevp;
  abb->offset = offset;

  
  abb->attrcnt = 0;
  unsigned int attrname;
  unsigned int attrform;
  do
    {
      if (abbrevp >= end)
	goto invalid;
      get_uleb128 (attrname, abbrevp, end);
      if (abbrevp >= end)
	goto invalid;
      get_uleb128 (attrform, abbrevp, end);
    }
  while (attrname != 0 && attrform != 0 && ++abb->attrcnt);

  
  if (lengthp != NULL)
    *lengthp = abbrevp - start_abbrevp;

  
  if (cu != NULL && ! foundit)
    (void) Dwarf_Abbrev_Hash_insert (&cu->abbrev_hash, abb->code, abb);

 out:
  return abb;
}


Dwarf_Abbrev *
dwarf_getabbrev (die, offset, lengthp)
     Dwarf_Die *die;
     Dwarf_Off offset;
     size_t *lengthp;
{
  return __libdw_getabbrev (die->cu->dbg, die->cu,
			    die->cu->orig_abbrev_offset + offset, lengthp,
			    NULL);
}
