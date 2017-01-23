/* Return list address ranges.
   Copyright (C) 2000-2010 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

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
#include <assert.h>
#include "libdwP.h"
#include <dwarf.h>

struct arangelist
{
  Dwarf_Arange arange;
  struct arangelist *next;
};

static int
compare_aranges (const void *a, const void *b)
{
  struct arangelist *const *p1 = a, *const *p2 = b;
  struct arangelist *l1 = *p1, *l2 = *p2;
  if (l1->arange.addr != l2->arange.addr)
    return (l1->arange.addr < l2->arange.addr) ? -1 : 1;
  return 0;
}

int
dwarf_getaranges (dbg, aranges, naranges)
     Dwarf *dbg;
     Dwarf_Aranges **aranges;
     size_t *naranges;
{
  if (dbg == NULL)
    return -1;

  if (dbg->aranges != NULL)
    {
      *aranges = dbg->aranges;
      if (naranges != NULL)
	*naranges = dbg->aranges->naranges;
      return 0;
    }

  if (dbg->sectiondata[IDX_debug_aranges] == NULL)
    {
      
      *aranges = NULL;
      if (naranges != NULL)
	*naranges = 0;
      return 0;
    }

  if (dbg->sectiondata[IDX_debug_aranges]->d_buf == NULL)
    return -1;

  struct arangelist *arangelist = NULL;
  unsigned int narangelist = 0;

  const unsigned char *readp = dbg->sectiondata[IDX_debug_aranges]->d_buf;
  const unsigned char *readendp
    = readp + dbg->sectiondata[IDX_debug_aranges]->d_size;

  while (readp < readendp)
    {
      const unsigned char *hdrstart = readp;

      Dwarf_Word length = read_4ubyte_unaligned_inc (dbg, readp);
      unsigned int length_bytes = 4;
      if (length == DWARF3_LENGTH_64_BIT)
	{
	  length = read_8ubyte_unaligned_inc (dbg, readp);
	  length_bytes = 8;
	}
      else if (unlikely (length >= DWARF3_LENGTH_MIN_ESCAPE_CODE
			 && length <= DWARF3_LENGTH_MAX_ESCAPE_CODE))
	goto invalid;

      unsigned int version = read_2ubyte_unaligned_inc (dbg, readp);
      if (version != 2)
	{
	invalid:
	  __libdw_seterrno (DWARF_E_INVALID_DWARF);
	fail:
	  while (arangelist != NULL)
	    {
	      struct arangelist *next = arangelist->next;
	      free (arangelist);
	      arangelist = next;
	    }
	  return -1;
	}

      Dwarf_Word offset;
      if (__libdw_read_offset_inc (dbg,
				   IDX_debug_aranges, &readp,
				   length_bytes, &offset, IDX_debug_info, 4))
	goto fail;

      unsigned int address_size = *readp++;
      if (address_size != 4 && address_size != 8)
	goto invalid;

      
      unsigned int segment_size = *readp++;
      if (segment_size != 0)
	goto invalid;

      
      readp += ((2 * address_size - ((readp - hdrstart) % (2 * address_size)))
		% (2 * address_size));

      while (1)
	{
	  Dwarf_Word range_address;
	  Dwarf_Word range_length;

	  if (__libdw_read_address_inc (dbg, IDX_debug_aranges, &readp,
					address_size, &range_address))
	    goto fail;

	  if (address_size == 4)
	    range_length = read_4ubyte_unaligned_inc (dbg, readp);
	  else
	    range_length = read_8ubyte_unaligned_inc (dbg, readp);

	  
	  if (range_address == 0 && range_length == 0)
	    break;

	  struct arangelist *new_arange = malloc (sizeof *new_arange);
	  if (unlikely (new_arange == NULL))
	    {
	      __libdw_seterrno (DWARF_E_NOMEM);
	      goto fail;
	    }

	  new_arange->arange.addr = range_address;
	  new_arange->arange.length = range_length;

	  
	  const char *cu_header = (dbg->sectiondata[IDX_debug_info]->d_buf
				   + offset);
	  unsigned int offset_size;
	  if (read_4ubyte_unaligned_noncvt (cu_header) == DWARF3_LENGTH_64_BIT)
	    offset_size = 8;
	  else
	    offset_size = 4;
	  new_arange->arange.offset = DIE_OFFSET_FROM_CU_OFFSET (offset,
								 offset_size,
								 false);

	  new_arange->next = arangelist;
	  arangelist = new_arange;
	  ++narangelist;

	  
	  if (unlikely (new_arange->arange.offset
			>= dbg->sectiondata[IDX_debug_info]->d_size))
	    goto invalid;
	}
    }

  if (narangelist == 0)
    {
      assert (arangelist == NULL);
      if (naranges != NULL)
	*naranges = 0;
      *aranges = NULL;
      return 0;
    }

  
  void *buf = libdw_alloc (dbg, Dwarf_Aranges,
			   sizeof (Dwarf_Aranges)
			   + narangelist * sizeof (Dwarf_Arange), 1);

  assert (sizeof (Dwarf_Arange) >= sizeof (Dwarf_Arange *));
  struct arangelist **sortaranges
    = (buf + sizeof (Dwarf_Aranges)
       + ((sizeof (Dwarf_Arange) - sizeof sortaranges[0]) * narangelist));

  unsigned int i = narangelist;
  while (i-- > 0)
    {
      sortaranges[i] = arangelist;
      arangelist = arangelist->next;
    }
  assert (arangelist == NULL);

  
  qsort (sortaranges, narangelist, sizeof sortaranges[0], &compare_aranges);

  *aranges = buf;
  (*aranges)->dbg = dbg;
  (*aranges)->naranges = narangelist;
  dbg->aranges = *aranges;
  if (naranges != NULL)
    *naranges = narangelist;
  for (i = 0; i < narangelist; ++i)
    {
      struct arangelist *elt = sortaranges[i];
      (*aranges)->info[i] = elt->arange;
      free (elt);
    }

  return 0;
}
INTDEF(dwarf_getaranges)
