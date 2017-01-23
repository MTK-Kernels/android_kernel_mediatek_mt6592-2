/* Get function information.
   Copyright (C) 2005, 2013 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2005.

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


struct visitor_info
{
  
  int (*callback) (Dwarf_Die *, void *);

  
  void *arg;

  
  void *start_addr;

  
  void *last_addr;

  
  bool c_cu;
};

static int
tree_visitor (unsigned int depth __attribute__ ((unused)),
	      struct Dwarf_Die_Chain *chain, void *arg)
{
  struct visitor_info *const v = arg;
  Dwarf_Die *die = &chain->die;
  void *start_addr = v->start_addr;
  void *die_addr = die->addr;

  int tag = INTUSE(dwarf_tag) (die);
  if (v->c_cu
      && tag != DW_TAG_subprogram
      && tag != DW_TAG_lexical_block
      && tag != DW_TAG_inlined_subroutine)
    {
      chain->prune = true;
      return DWARF_CB_OK;
    }

  
  if (start_addr != NULL)
    {
      if (die_addr == start_addr)
	v->start_addr = NULL;
      return DWARF_CB_OK;
    }

  
  if (tag != DW_TAG_subprogram
      || INTUSE(dwarf_hasattr) (die, DW_AT_declaration))
    return DWARF_CB_OK;

  v->last_addr = die_addr;
  return (*v->callback) (die, v->arg);
}

ptrdiff_t
dwarf_getfuncs (Dwarf_Die *cudie, int (*callback) (Dwarf_Die *, void *),
		void *arg, ptrdiff_t offset)
{
  if (unlikely (cudie == NULL
		|| INTUSE(dwarf_tag) (cudie) != DW_TAG_compile_unit))
    return -1;

  int lang = INTUSE(dwarf_srclang) (cudie);
  bool c_cu = (lang == DW_LANG_C89
	       || lang == DW_LANG_C
	       || lang == DW_LANG_C99
	       || lang == DW_LANG_C11);

  struct visitor_info v = { callback, arg, (void *) offset, NULL, c_cu };
  struct Dwarf_Die_Chain chain = { .die = CUDIE (cudie->cu),
				   .parent = NULL };
  int res = __libdw_visit_scopes (0, &chain, &tree_visitor, NULL, &v);

  if (res == DWARF_CB_ABORT)
    return (ptrdiff_t) v.last_addr;
  else
    return res;
}
