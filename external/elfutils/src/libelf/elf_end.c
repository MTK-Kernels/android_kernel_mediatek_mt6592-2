/* Free resources associated with Elf descriptor.
   Copyright (C) 1998,1999,2000,2001,2002,2004,2005,2007 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 1998.

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
#include <stddef.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "libelfP.h"


int
elf_end (elf)
     Elf *elf;
{
  Elf *parent;

  if (elf == NULL)
    
    return 0;

  
  rwlock_wrlock (elf->lock);

  if (elf->ref_count != 0 && --elf->ref_count != 0)
    {
      
      int result = elf->ref_count;
      rwlock_unlock (elf->lock);
      return result;
    }

  if (elf->kind == ELF_K_AR)
    {
      if (elf->state.ar.ar_sym != (Elf_Arsym *) -1l)
	free (elf->state.ar.ar_sym);
      elf->state.ar.ar_sym = NULL;

      if (elf->state.ar.children != NULL)
	return 0;
    }

  
  parent = elf->parent;
  if (parent != NULL)
    {
      rwlock_unlock (elf->lock);
      rwlock_rdlock (parent->lock);
      rwlock_wrlock (elf->lock);

      if (parent->state.ar.children == elf)
	parent->state.ar.children = elf->next;
      else
	{
	  struct Elf *child = parent->state.ar.children;

	  while (child->next != elf)
	    child = child->next;

	  child->next = elf->next;
	}

      rwlock_unlock (parent->lock);
    }

  
  switch (elf->kind)
    {
    case ELF_K_AR:
      if (elf->state.ar.long_names != NULL)
	free (elf->state.ar.long_names);
      break;

    case ELF_K_ELF:
      {
	Elf_Data_Chunk *rawchunks
	  = (elf->class == ELFCLASS32
	     || (offsetof (struct Elf, state.elf32.rawchunks)
		 == offsetof (struct Elf, state.elf64.rawchunks))
	     ? elf->state.elf32.rawchunks
	     : elf->state.elf64.rawchunks);
	while (rawchunks != NULL)
	  {
	    Elf_Data_Chunk *next = rawchunks->next;
	    if (rawchunks->dummy_scn.flags & ELF_F_MALLOCED)
	      free (rawchunks->data.d.d_buf);
	    free (rawchunks);
	    rawchunks = next;
	  }

	Elf_ScnList *list = (elf->class == ELFCLASS32
			     || (offsetof (struct Elf, state.elf32.scns)
				 == offsetof (struct Elf, state.elf64.scns))
			     ? &elf->state.elf32.scns
			     : &elf->state.elf64.scns);

	do
	  {
	    
	    size_t cnt = list->max;

	    while (cnt-- > 0)
	      {
		Elf_Scn *scn = &list->data[cnt];
		Elf_Data_List *runp;

		if ((scn->shdr_flags & ELF_F_MALLOCED) != 0)
		  
		  free (scn->shdr.e32);

		if (scn->data_base != scn->rawdata_base)
		  free (scn->data_base);

		if (elf->map_address == NULL)
		  free (scn->rawdata_base);

		runp = scn->data_list.next;
		while (runp != NULL)
		  {
		    Elf_Data_List *oldp = runp;
		    runp = runp->next;
		    if ((oldp->flags & ELF_F_MALLOCED) != 0)
		      free (oldp);
		  }
	      }

	    
	    Elf_ScnList *oldp = list;
	    list = list->next;
	    assert (list == NULL || oldp->cnt == oldp->max);
	    if (oldp != (elf->class == ELFCLASS32
			 || (offsetof (struct Elf, state.elf32.scns)
			     == offsetof (struct Elf, state.elf64.scns))
			 ? &elf->state.elf32.scns
			 : &elf->state.elf64.scns))
	      free (oldp);
	  }
	while (list != NULL);
      }

      
      if (elf->state.elf.shdr_malloced  != 0)
	free (elf->class == ELFCLASS32
	      || (offsetof (struct Elf, state.elf32.shdr)
		  == offsetof (struct Elf, state.elf64.shdr))
	      ? (void *) elf->state.elf32.shdr
	      : (void *) elf->state.elf64.shdr);

      
      if ((elf->state.elf.phdr_flags & ELF_F_MALLOCED) != 0)
	free (elf->class == ELFCLASS32
	      || (offsetof (struct Elf, state.elf32.phdr)
		  == offsetof (struct Elf, state.elf64.phdr))
	      ? (void *) elf->state.elf32.phdr
	      : (void *) elf->state.elf64.phdr);
      break;

    default:
      break;
    }

  if (elf->map_address != NULL && parent == NULL)
    {
      
      if ((elf->flags & ELF_F_MALLOCED) != 0)
	free (elf->map_address);
      else if ((elf->flags & ELF_F_MMAPPED) != 0)
	munmap (elf->map_address, elf->maximum_size);
    }

  rwlock_unlock (elf->lock);
  rwlock_fini (elf->lock);

  
  free (elf);

  return (parent != NULL && parent->ref_count == 0
	  ? INTUSE(elf_end) (parent) : 0);
}
INTDEF(elf_end)
