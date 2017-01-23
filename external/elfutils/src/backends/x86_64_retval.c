/* Function return value location for Linux/x86-64 ABI.
   Copyright (C) 2005-2010, 2014 Red Hat, Inc.
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
#include <dwarf.h>

#define BACKEND x86_64_
#include "libebl_CPU.h"


static const Dwarf_Op loc_intreg[] =
  {
    { .atom = DW_OP_reg0 }, { .atom = DW_OP_piece, .number = 8 },
    { .atom = DW_OP_reg1 }, { .atom = DW_OP_piece, .number = 8 },
  };
#define nloc_intreg	1
#define nloc_intregpair	4

static const Dwarf_Op loc_x87reg[] =
  {
    { .atom = DW_OP_regx, .number = 33 },
    { .atom = DW_OP_piece, .number = 10 },
    { .atom = DW_OP_regx, .number = 34 },
    { .atom = DW_OP_piece, .number = 10 },
  };
#define nloc_x87reg	1
#define nloc_x87regpair	4

static const Dwarf_Op loc_ssereg[] =
  {
    { .atom = DW_OP_reg17 }, { .atom = DW_OP_piece, .number = 16 },
    { .atom = DW_OP_reg18 }, { .atom = DW_OP_piece, .number = 16 },
  };
#define nloc_ssereg	1
#define nloc_sseregpair	4

static const Dwarf_Op loc_aggregate[] =
  {
    { .atom = DW_OP_breg0, .number = 0 }
  };
#define nloc_aggregate 1


int
x86_64_return_value_location (Dwarf_Die *functypedie, const Dwarf_Op **locp)
{
  Dwarf_Die die_mem, *typedie = &die_mem;
  int tag = dwarf_peeled_die_type (functypedie, typedie);
  if (tag <= 0)
    return tag;

  Dwarf_Word size;
  switch (tag)
    {
    case -1:
      return -1;

    case DW_TAG_subrange_type:
      if (! dwarf_hasattr_integrate (typedie, DW_AT_byte_size))
	{
	  Dwarf_Attribute attr_mem, *attr;
	  attr = dwarf_attr_integrate (typedie, DW_AT_type, &attr_mem);
	  typedie = dwarf_formref_die (attr, &die_mem);
	  tag = DWARF_TAG_OR_RETURN (typedie);
	}
      

    case DW_TAG_base_type:
    case DW_TAG_enumeration_type:
    case DW_TAG_pointer_type:
    case DW_TAG_ptr_to_member_type:
      {
	Dwarf_Attribute attr_mem;
	if (dwarf_formudata (dwarf_attr_integrate (typedie, DW_AT_byte_size,
						   &attr_mem), &size) != 0)
	  {
	    if (tag == DW_TAG_pointer_type || tag == DW_TAG_ptr_to_member_type)
	      size = 8;
	    else
	      return -1;
	  }
      }

      if (tag == DW_TAG_base_type)
	{
	  Dwarf_Attribute attr_mem;
	  Dwarf_Word encoding;
	  if (dwarf_formudata (dwarf_attr_integrate (typedie, DW_AT_encoding,
						     &attr_mem),
			       &encoding) != 0)
	    return -1;

	  switch (encoding)
	    {
	    case DW_ATE_complex_float:
	      switch (size)
		{
		case 4 * 2:	
		case 8 * 2:	
		  *locp = loc_ssereg;
		  return nloc_sseregpair;
		case 16 * 2:	
		  *locp = loc_x87reg;
		  return nloc_x87regpair;
		}
	      return -2;

	    case DW_ATE_float:
	      switch (size)
		{
		case 4:	
		case 8:	
		  *locp = loc_ssereg;
		  return nloc_ssereg;
		case 16:	
		  
		  *locp = loc_x87reg;
		  return nloc_x87reg;
		}
	      return -2;
	    }
	}

    intreg:
      *locp = loc_intreg;
      if (size <= 8)
	return nloc_intreg;
      if (size <= 16)
	return nloc_intregpair;

    large:
      *locp = loc_aggregate;
      return nloc_aggregate;

    case DW_TAG_structure_type:
    case DW_TAG_class_type:
    case DW_TAG_union_type:
    case DW_TAG_array_type:
      if (dwarf_aggregate_size (typedie, &size) != 0)
	goto large;
      if (size > 16)
	goto large;


      goto intreg;
    }

  return -2;
}
