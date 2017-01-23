/* FDE reading.
   Copyright (C) 2009-2010, 2014 Red Hat, Inc.
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

#include "cfi.h"
#include <search.h>
#include <stdlib.h>

#include "encoded-value.h"

static int
compare_fde (const void *a, const void *b)
{
  const struct dwarf_fde *fde1 = a;
  const struct dwarf_fde *fde2 = b;

  if (fde1->end == 0)
    {
      if (fde1->start < fde2->start)
	return -1;
      if (fde1->start >= fde2->end)
	return 1;
    }
  else
    {
      if (fde2->start < fde1->start)
	return 1;
      if (fde2->start >= fde1->end)
	return -1;
    }

  return 0;
}

static struct dwarf_fde *
intern_fde (Dwarf_CFI *cache, const Dwarf_FDE *entry)
{
  
  struct dwarf_cie *cie = __libdw_find_cie (cache, entry->CIE_pointer);
  if (cie == NULL)
    return (void *) -1l;

  struct dwarf_fde *fde = malloc (sizeof (struct dwarf_fde));
  if (fde == NULL)
    {
      __libdw_seterrno (DWARF_E_NOMEM);
      return NULL;
    }

  fde->instructions = entry->start;
  fde->instructions_end = entry->end;
  if (unlikely (read_encoded_value (cache, cie->fde_encoding,
				    &fde->instructions, &fde->start))
      || unlikely (read_encoded_value (cache, cie->fde_encoding & 0x0f,
				       &fde->instructions, &fde->end)))
    {
      free (fde);
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return NULL;
    }
  fde->end += fde->start;

  fde->cie = cie;

  if (cie->sized_augmentation_data)
    {
      Dwarf_Word len;
      get_uleb128 (len, fde->instructions, fde->instructions_end);
      if ((Dwarf_Word) (fde->instructions_end - fde->instructions) < len)
	{
	  free (fde);
	  __libdw_seterrno (DWARF_E_INVALID_DWARF);
	  return NULL;
	}
      fde->instructions += len;
    }
  else
    fde->instructions += cie->fde_augmentation_data_size;

  
  if (tsearch (fde, &cache->fde_tree, &compare_fde) == NULL)
    {
      free (fde);
      __libdw_seterrno (DWARF_E_NOMEM);
      return NULL;
    }

  return fde;
}

struct dwarf_fde *
internal_function
__libdw_fde_by_offset (Dwarf_CFI *cache, Dwarf_Off offset)
{
  Dwarf_CFI_Entry entry;
  Dwarf_Off next_offset;
  int result = INTUSE(dwarf_next_cfi) (cache->e_ident,
				       &cache->data->d, CFI_IS_EH (cache),
				       offset, &next_offset, &entry);
  if (result != 0)
    {
      if (result > 0)
      invalid:
	__libdw_seterrno (DWARF_E_INVALID_DWARF);
      return NULL;
    }

  if (unlikely (dwarf_cfi_cie_p (&entry)))
    goto invalid;

  
  struct dwarf_fde *fde = intern_fde (cache, &entry.fde);
  if (fde == (void *) -1l || fde == NULL)
    return NULL;

  
  if (cache->next_offset == offset)
    cache->next_offset = next_offset;

  return fde;
}

static Dwarf_Off
binary_search_fde (Dwarf_CFI *cache, Dwarf_Addr address)
{
  const size_t size = 2 * encoded_value_size (&cache->data->d, cache->e_ident,
					      cache->search_table_encoding,
					      NULL);

  
  Dwarf_CFI dummy_cfi =
    {
      .e_ident = cache->e_ident,
      .datarel = cache->search_table_vaddr,
      .frame_vaddr = cache->search_table_vaddr,
    };

  size_t l = 0, u = cache->search_table_entries;
  while (l < u)
    {
      size_t idx = (l + u) / 2;

      const uint8_t *p = &cache->search_table[idx * size];
      Dwarf_Addr start;
      if (unlikely (read_encoded_value (&dummy_cfi,
					cache->search_table_encoding, &p,
					&start)))
	break;
      if (address < start)
	u = idx;
      else
	{
	  l = idx + 1;

	  Dwarf_Addr fde;
	  if (unlikely (read_encoded_value (&dummy_cfi,
					    cache->search_table_encoding, &p,
					    &fde)))
	    break;

	  if (l < cache->search_table_entries)
	    {
	      
	      Dwarf_Addr end;
	      if (unlikely (read_encoded_value
			    (&dummy_cfi, cache->search_table_encoding, &p,
			     &end)))
		break;
	      if (address >= end)
		continue;
	    }

	  return fde - cache->frame_vaddr;
	}
    }

  return (Dwarf_Off) -1l;
}

struct dwarf_fde *
internal_function
__libdw_find_fde (Dwarf_CFI *cache, Dwarf_Addr address)
{
  

  const struct dwarf_fde fde_key = { .start = address, .end = 0 };
  struct dwarf_fde **found = tfind (&fde_key, &cache->fde_tree, &compare_fde);
  if (found != NULL)
    return *found;

  
  if (cache->search_table != NULL)
    {
      Dwarf_Off offset = binary_search_fde (cache, address);
      if (offset == (Dwarf_Off) -1l)
	goto no_match;
      struct dwarf_fde *fde = __libdw_fde_by_offset (cache, offset);
      if (likely (fde != NULL))
	{
	  
	  if (unlikely (address < fde->start))
	    {
	      __libdw_seterrno (DWARF_E_INVALID_DWARF);
	      return NULL;
	    }
	  
	  if (unlikely (address >= fde->end))
	    goto no_match;
	}
      return fde;
    }

  
  while (1)
    {
      Dwarf_Off last_offset = cache->next_offset;
      Dwarf_CFI_Entry entry;
      int result = INTUSE(dwarf_next_cfi) (cache->e_ident,
					   &cache->data->d, CFI_IS_EH (cache),
					   last_offset, &cache->next_offset,
					   &entry);
      if (result > 0)
	break;
      if (result < 0)
	{
	  if (cache->next_offset == last_offset)
	    
	    break;
	  
	  continue;
	}

      if (dwarf_cfi_cie_p (&entry))
	{
	  __libdw_intern_cie (cache, last_offset, &entry.cie);
	  continue;
	}

      
      struct dwarf_fde *fde = intern_fde (cache, &entry.fde);

      if (fde == (void *) -1l)	
	continue;

      if (fde == NULL)		
	return NULL;

      
      if (fde->start <= address && fde->end > address)
	return fde;
    }

 no_match:
  
  __libdw_seterrno (DWARF_E_NO_MATCH);
  return NULL;
}
