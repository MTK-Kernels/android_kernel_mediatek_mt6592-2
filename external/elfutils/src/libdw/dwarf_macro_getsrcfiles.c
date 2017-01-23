/* Find line information for a given macro.
   Copyright (C) 2014 Red Hat, Inc.
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

int
dwarf_macro_getsrcfiles (Dwarf *dbg, Dwarf_Macro *macro,
			 Dwarf_Files **files, size_t *nfiles)
{
  if (macro == NULL)
    return -1;

  Dwarf_Macro_Op_Table *const table = macro->table;
  if (table->files == NULL)
    {
      Dwarf_Off line_offset = table->line_offset;
      if (line_offset == (Dwarf_Off) -1)
	{
	  *files = NULL;
	  *nfiles = 0;
	  return 0;
	}


      if (__libdw_getsrclines (dbg, line_offset, table->comp_dir,
			       table->is_64bit ? 8 : 4,
			       NULL, &table->files) < 0)
	table->files = (void *) -1;
    }

  if (table->files == (void *) -1)
    return -1;

  *files = table->files;
  *nfiles = table->files->nfiles;
  return 0;
}
