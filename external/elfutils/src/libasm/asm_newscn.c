/* Create new section in output file.
   Copyright (C) 2002-2011 Red Hat, Inc.
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
#include <error.h>
#include <libintl.h>
#include <stdlib.h>
#include <string.h>

#include <libasmP.h>
#include <libelf.h>
#include <system.h>


static const struct
{
  struct FillPattern pattern;
  char zero;
} xdefault_pattern =
  {
    .pattern =
    {
      .len = 1
    },
    .zero = '\0'
  };
const struct FillPattern *__libasm_default_pattern = &xdefault_pattern.pattern;


static AsmScn_t *
text_newscn (AsmScn_t *result, GElf_Word type, GElf_Xword flags)
{
  
  char flagstr[sizeof (GElf_Xword) * 8 + 5];
  char *wp = flagstr;
  const char *typestr = "";

  wp = stpcpy (wp, ", \"");

  if (flags & SHF_WRITE)
    *wp++ = 'w';
  if (flags & SHF_ALLOC)
    *wp++ = 'a';
  if (flags & SHF_EXECINSTR)
    *wp++ = 'x';
  if (flags & SHF_MERGE)
    *wp++ = 'M';
  if (flags & SHF_STRINGS)
    *wp++ = 'S';
  if (flags & SHF_LINK_ORDER)
    *wp++ = 'L';

  *wp++ = '"';

  if (type == SHT_PROGBITS)
    typestr = ",@progbits";
  else if (type == SHT_NOBITS)
    typestr = ",@nobits";

  
  *wp = '\0';

  fprintf (result->ctx->out.file, "\t.section \"%s\"%s%s\n",
	   result->name, flagstr, typestr);

  return result;
}


static AsmScn_t *
binary_newscn (AsmScn_t *result, GElf_Word type, GElf_Xword flags,
	       size_t scnname_len)
{
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr;
  Elf_Scn *scn;

  
  result->subsection_id = 0;

  
  result->offset = 0;
  
  result->max_align = 1;

  
  result->content = NULL;

  
  result->pattern = (struct FillPattern *) __libasm_default_pattern;

  
  result->subnext = NULL;

  
  result->data.main.strent = ebl_strtabadd (result->ctx->section_strtab,
					    result->name, scnname_len);
  assert (result->data.main.strent != NULL);

  
  result->data.main.scn = scn = elf_newscn (result->ctx->out.elf);
  if (scn == NULL)
    {
      free (result);
      __libasm_seterrno (ASM_E_LIBELF);
      return NULL;
    }

  
  result->data.main.next_in_group = NULL;

  
  shdr = gelf_getshdr (scn, &shdr_mem);

  shdr->sh_flags = flags;
  result->type = shdr->sh_type = type;

  (void) gelf_update_shdr (scn, shdr);

  return result;
}


AsmScn_t *
asm_newscn (ctx, scnname, type, flags)
     AsmCtx_t *ctx;
     const char *scnname;
     GElf_Word type;
     GElf_Xword flags;
{
  size_t scnname_len = strlen (scnname) + 1;
  AsmScn_t *result;

  
  if (ctx == NULL)
    return NULL;

  
  if (unlikely ((flags & ~(SHF_WRITE | SHF_ALLOC | SHF_EXECINSTR | SHF_MERGE
			   | SHF_STRINGS | SHF_LINK_ORDER)) != 0)
      || (type != SHT_PROGBITS && unlikely (type != SHT_NOBITS)))
    {
      __libasm_seterrno (ASM_E_INVALID);
      return NULL;
    }

  rwlock_wrlock (ctx->lock);

  
  result = (AsmScn_t *) malloc (sizeof (AsmScn_t) + scnname_len);
  if (result != NULL)
    {
      
      memcpy (result->name, scnname, scnname_len);

      
      result->ctx = ctx;

      
      result = (unlikely (ctx->textp)
		? text_newscn (result, type, flags)
		: binary_newscn (result, type, flags, scnname_len));

      if (result != NULL)
	{
	  result->allnext = ctx->section_list;
	  ctx->section_list = result;
	}
    }

  rwlock_unlock (ctx->lock);

  return result;
}
INTDEF(asm_newscn)
