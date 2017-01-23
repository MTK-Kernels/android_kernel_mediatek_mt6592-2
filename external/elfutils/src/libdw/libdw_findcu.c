/* Find CU for given offset.
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

#include <assert.h>
#include <search.h>
#include "libdwP.h"

static int
findcu_cb (const void *arg1, const void *arg2)
{
  struct Dwarf_CU *cu1 = (struct Dwarf_CU *) arg1;
  struct Dwarf_CU *cu2 = (struct Dwarf_CU *) arg2;

  if (cu1->end == 0)
    {
      if (cu1->start < cu2->start)
	return -1;
      if (cu1->start >= cu2->end)
	return 1;
    }
  else
    {
      if (cu2->start < cu1->start)
	return 1;
      if (cu2->start >= cu1->end)
	return -1;
    }

  return 0;
}

struct Dwarf_CU *
internal_function
__libdw_intern_next_unit (dbg, debug_types)
     Dwarf *dbg;
     bool debug_types;
{
  Dwarf_Off *const offsetp
    = debug_types ? &dbg->next_tu_offset : &dbg->next_cu_offset;
  void **tree = debug_types ? &dbg->tu_tree : &dbg->cu_tree;

  Dwarf_Off oldoff = *offsetp;
  uint16_t version;
  uint8_t address_size;
  uint8_t offset_size;
  Dwarf_Off abbrev_offset;
  uint64_t type_sig8 = 0;
  Dwarf_Off type_offset = 0;

  if (INTUSE(dwarf_next_unit) (dbg, oldoff, offsetp, NULL,
			       &version, &abbrev_offset,
			       &address_size, &offset_size,
			       debug_types ? &type_sig8 : NULL,
			       debug_types ? &type_offset : NULL) != 0)
    
    return NULL;

  
  if (unlikely (version < 2) || unlikely (version > 4))
    {
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return NULL;
    }

  
  Elf_Data *data = dbg->sectiondata[debug_types
				    ? IDX_debug_types : IDX_debug_info];
  if (unlikely (*offsetp > data->d_size))
    *offsetp = data->d_size;

  
  struct Dwarf_CU *newp = libdw_typed_alloc (dbg, struct Dwarf_CU);

  newp->dbg = dbg;
  newp->start = oldoff;
  newp->end = *offsetp;
  newp->address_size = address_size;
  newp->offset_size = offset_size;
  newp->version = version;
  newp->type_sig8 = type_sig8;
  newp->type_offset = type_offset;
  Dwarf_Abbrev_Hash_init (&newp->abbrev_hash, 41);
  newp->orig_abbrev_offset = newp->last_abbrev_offset = abbrev_offset;
  newp->lines = NULL;
  newp->locs = NULL;

  if (debug_types)
    Dwarf_Sig8_Hash_insert (&dbg->sig8_hash, type_sig8, newp);

  newp->startp = data->d_buf + newp->start;
  newp->endp = data->d_buf + newp->end;

  
  if (tsearch (newp, tree, findcu_cb) == NULL)
    {
      
      *offsetp = oldoff;
      __libdw_seterrno (DWARF_E_NOMEM);
      return NULL;
    }

  return newp;
}

struct Dwarf_CU *
__libdw_findcu (dbg, start, debug_types)
     Dwarf *dbg;
     Dwarf_Off start;
     bool debug_types;
{
  void **tree = debug_types ? &dbg->tu_tree : &dbg->cu_tree;
  Dwarf_Off *next_offset
    = debug_types ? &dbg->next_tu_offset : &dbg->next_cu_offset;

  
  struct Dwarf_CU fake = { .start = start, .end = 0 };
  struct Dwarf_CU **found = tfind (&fake, tree, findcu_cb);
  if (found != NULL)
    return *found;

  if (start < *next_offset)
    {
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return NULL;
    }

  
  while (1)
    {
      struct Dwarf_CU *newp = __libdw_intern_next_unit (dbg, debug_types);
      if (newp == NULL)
	return NULL;

      
      if (start < *next_offset)
	
	return newp;
    }
  
}
