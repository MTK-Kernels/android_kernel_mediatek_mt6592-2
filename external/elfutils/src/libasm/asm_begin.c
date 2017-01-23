/* Create descriptor for assembling.
   Copyright (C) 2002 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2002.

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
#include <errno.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gelf.h>
#include "libasmP.h"
#include <system.h>


static AsmCtx_t *
prepare_text_output (AsmCtx_t *result)
{
  if (result->fd == -1)
    result->out.file = stdout;
  else
    {
      result->out.file = fdopen (result->fd, "a");
      if (result->out.file == NULL)
	{
	  close (result->fd);
	  free (result);
	  result = NULL;
	}

      __fsetlocking (result->out.file, FSETLOCKING_BYCALLER);
    }

  return result;
}


static AsmCtx_t *
prepare_binary_output (AsmCtx_t *result, Ebl *ebl)
{
  GElf_Ehdr *ehdr;
  GElf_Ehdr ehdr_mem;

  
  result->out.elf = elf_begin (result->fd, ELF_C_WRITE_MMAP, NULL);
  if (result->out.elf == NULL)
    {
    err_libelf:
      unlink (result->tmp_fname);
      close (result->fd);
      free (result);
      __libasm_seterrno (ASM_E_LIBELF);
      return NULL;
    }

  
  int class = ebl_get_elfclass (ebl);
  if (gelf_newehdr (result->out.elf, class) == 0)
    goto err_libelf;

  ehdr = gelf_getehdr (result->out.elf, &ehdr_mem);
  
  assert (ehdr != NULL);

  
  ehdr->e_type = ET_REL;
  
  ehdr->e_version = EV_CURRENT;

  
  ehdr->e_machine = ebl_get_elfmachine (ebl);
  ehdr->e_ident[EI_CLASS] = class;
  ehdr->e_ident[EI_DATA] = ebl_get_elfdata (ebl);

  memcpy (&ehdr->e_ident[EI_MAG0], ELFMAG, SELFMAG);

  
  (void) gelf_update_ehdr (result->out.elf, ehdr);

  
  result->section_list = NULL;

  
  asm_symbol_tab_init (&result->symbol_tab, 67);
  result->nsymbol_tab = 0;
  
  result->section_strtab = ebl_strtabinit (true);
  result->symbol_strtab = ebl_strtabinit (true);

  
  result->groups = NULL;
  result->ngroups = 0;

  return result;
}


AsmCtx_t *
asm_begin (fname, ebl, textp)
     const char *fname;
     Ebl *ebl;
     bool textp;
{
  if (fname == NULL && ! textp)
    return NULL;

  size_t fname_len = fname != NULL ? strlen (fname) : 0;

  AsmCtx_t *result
    = (AsmCtx_t *) malloc (sizeof (AsmCtx_t) + 2 * fname_len + 9);
  if (result == NULL)
    return NULL;

      
      rwlock_init (result->lock);

  if (fname != NULL)
    {
      
      result->fname = stpcpy (mempcpy (result->tmp_fname, fname, fname_len),
			      ".XXXXXX") + 1;
      memcpy (result->fname, fname, fname_len + 1);

      
      result->fd = mkstemp (result->tmp_fname);
      if (result->fd == -1)
	{
	  int save_errno = errno;
	  free (result);
	  __libasm_seterrno (ASM_E_CANNOT_CREATE);
	  errno = save_errno;
	  return NULL;
	}
    }
  else
    result->fd = -1;

  
  result->tempsym_count = 0;

  
  result->textp = textp;
  if (textp)
    result = prepare_text_output (result);
  else
    result = prepare_binary_output (result, ebl);

  return result;
}
