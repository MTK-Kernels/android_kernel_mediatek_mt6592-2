/* Define new symbol for current position in given section.
   Copyright (C) 2002, 2005 Red Hat, Inc.
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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libasmP.h>
#include <system.h>


AsmSym_t *
asm_newsym (asmscn, name, size, type, binding)
     AsmScn_t *asmscn;
     const char *name;
     GElf_Xword size;
     int type;
     int binding;
{
#define TEMPSYMLEN 10
  char tempsym[TEMPSYMLEN];
  AsmSym_t *result;

  if (asmscn == NULL)
    
    return NULL;

  
  if (name == NULL)
    {
      if (binding != STB_LOCAL)
	{
	  __libasm_seterrno (ASM_E_INVALID);
	  return NULL;
	}

      
      snprintf (tempsym, TEMPSYMLEN, ".L%07u", asmscn->ctx->tempsym_count++);

      name = tempsym;
    }

  size_t name_len = strlen (name) + 1;

  result = (AsmSym_t *) malloc (sizeof (AsmSym_t) + name_len);
  if (result == NULL)
    return NULL;

  rwlock_wrlock (asmscn->ctx->lock);

  result->scn = asmscn;
  result->offset = asmscn->offset;
  result->size = size;
  result->type = type;
  result->binding = binding;
  result->symidx = 0;
  result->strent = ebl_strtabadd (asmscn->ctx->symbol_strtab,
				  memcpy (result + 1, name, name_len), 0);

  if (unlikely (asmscn->ctx->textp))
    {
      
      if (binding == STB_GLOBAL)
	fprintf (asmscn->ctx->out.file, "\t.globl\t%s\n", name);
      else if (binding == STB_WEAK)
	fprintf (asmscn->ctx->out.file, "\t.weak\t%s\n", name);

      
      if (type == STT_OBJECT)
	fprintf (asmscn->ctx->out.file, "\t.type\t%s,@object\n", name);
      else if (type == STT_FUNC)
	fprintf (asmscn->ctx->out.file, "\t.type\t%s,@function\n", name);

      
      fprintf (asmscn->ctx->out.file, "\t.size\t%s,%" PRIuMAX "\n%s:\n",
	       name, (uintmax_t) size, name);
    }
  else
    {
      
      if (asm_symbol_tab_insert (&asmscn->ctx->symbol_tab, elf_hash (name),
				 result) != 0)
	{
	  
	  __libasm_seterrno (ASM_E_DUPLSYM);
	  free (result);
	  result = NULL;
	}
      else if (name != tempsym && asm_emit_symbol_p (name))
	
	++asmscn->ctx->nsymbol_tab;
    }

  rwlock_unlock (asmscn->ctx->lock);

  return result;
}
