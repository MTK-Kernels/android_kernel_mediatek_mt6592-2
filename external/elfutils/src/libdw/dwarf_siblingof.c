/* Return sibling of given DIE.
   Copyright (C) 2003-2010, 2014 Red Hat, Inc.
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
#include <dwarf.h>
#include <string.h>


int
dwarf_siblingof (die, result)
     Dwarf_Die *die;
     Dwarf_Die *result;
{
  
  if (die == NULL)
    return -1;

  if (result == NULL)
    return -1;

  if (result != die)
    result->addr = NULL;

  unsigned int level = 0;

  
  Dwarf_Die this_die = *die;
  
  Dwarf_Attribute sibattr;
  
  sibattr.cu = this_die.cu;
  
  unsigned char *addr = this_die.addr;
  
  unsigned char *endp = sibattr.cu->endp;

  do
    {
      
      addr = __libdw_find_attr (&this_die, DW_AT_sibling, &sibattr.code,
				&sibattr.form);
      if (addr != NULL && sibattr.code == DW_AT_sibling)
	{
	  Dwarf_Off offset;
	  sibattr.valp = addr;
	  if (unlikely (__libdw_formref (&sibattr, &offset) != 0))
	    
	    return -1;

	  
	  addr = sibattr.cu->startp + offset;
	}
      else if (unlikely (addr == NULL)
	       || unlikely (this_die.abbrev == DWARF_END_ABBREV))
	return -1;
      else if (this_die.abbrev->has_children)
	
	++level;


      while (1)
	{
	  if (addr >= endp)
	    return 1;

	  if (*addr != '\0')
	    break;

	  if (level-- == 0)
	    {
	      if (result != die)
		result->addr = addr;
	      
	      return 1;
	    }

	  ++addr;
	}

      
      this_die.addr = addr;
      this_die.abbrev = NULL;
    }
  while (level > 0);

  
  if (addr >= endp)
    return 1;

  memset (result, '\0', sizeof (Dwarf_Die));

  
  result->addr = addr;

  
  result->cu = sibattr.cu;

  return 0;
}
INTDEF(dwarf_siblingof)
