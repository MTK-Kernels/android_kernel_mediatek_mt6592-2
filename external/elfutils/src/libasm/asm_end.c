/* Finalize operations on the assembler context, free all resources.
   Copyright (C) 2002, 2003, 2005 Red Hat, Inc.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <libasmP.h>
#include <libelf.h>
#include <system.h>


static int
text_end (AsmCtx_t *ctx __attribute__ ((unused)))
{
  if (fclose (ctx->out.file) != 0)
    {
      __libasm_seterrno (ASM_E_IOERROR);
      return -1;
    }

  return 0;
}


static int
binary_end (AsmCtx_t *ctx)
{
  void *symtab = NULL;
  struct Ebl_Strent *symscn_strent = NULL;
  struct Ebl_Strent *strscn_strent = NULL;
  struct Ebl_Strent *xndxscn_strent = NULL;
  Elf_Scn *shstrscn;
  struct Ebl_Strent *shstrscn_strent;
  size_t shstrscnndx;
  size_t symscnndx = 0;
  size_t strscnndx = 0;
  size_t xndxscnndx = 0;
  Elf_Data *data;
  Elf_Data *shstrtabdata;
  Elf_Data *strtabdata = NULL;
  Elf_Data *xndxdata = NULL;
  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr;
  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr;
  AsmScn_t *asmscn;
  int result = 0;

  for (asmscn = ctx->section_list; asmscn != NULL; asmscn = asmscn->allnext)
    {
#if 0
      Elf_Scn *scn = elf_getscn (ctx->out.elf, asmscn->data.main.scnndx);
#else
      Elf_Scn *scn = asmscn->data.main.scn;
#endif
      off_t offset = 0;
      AsmScn_t *asmsubscn = asmscn;

      do
	{
	  struct AsmData *content = asmsubscn->content;
	  bool first = true;

	  offset = ((offset + asmsubscn->max_align - 1)
		    & ~(asmsubscn->max_align - 1));

	  asmsubscn->offset = offset;

	  
	  if (content != NULL)
	    do
	      {
		Elf_Data *newdata = elf_newdata (scn);

		if (newdata == NULL)
		  {
		    __libasm_seterrno (ASM_E_LIBELF);
		    return -1;
		  }

		newdata->d_buf = content->data;
		newdata->d_type = ELF_T_BYTE;
		newdata->d_size = content->len;
		newdata->d_off = offset;
		newdata->d_align = first ? asmsubscn->max_align : 1;

		offset += content->len;
	      }
	    while ((content = content->next) != asmsubscn->content);
	}
      while ((asmsubscn = asmsubscn->subnext) != NULL);
    }


  
  if (ctx->nsymbol_tab > 0)
    {
      
      symscn_strent = ebl_strtabadd (ctx->section_strtab, ".symtab", 8);
      strscn_strent = ebl_strtabadd (ctx->section_strtab, ".strtab", 8);

      
      Elf_Scn *strscn = elf_newscn (ctx->out.elf);
      strtabdata = elf_newdata (strscn);
      shdr = gelf_getshdr (strscn, &shdr_mem);
      if (strtabdata == NULL || shdr == NULL)
	{
	  __libasm_seterrno (ASM_E_LIBELF);
	  return -1;
	}
      strscnndx = elf_ndxscn (strscn);

      ebl_strtabfinalize (ctx->symbol_strtab, strtabdata);

      shdr->sh_type = SHT_STRTAB;
      assert (shdr->sh_entsize == 0);

      (void) gelf_update_shdr (strscn, shdr);

      
      Elf_Scn *symscn = elf_newscn (ctx->out.elf);
      data = elf_newdata (symscn);
      shdr = gelf_getshdr (symscn, &shdr_mem);
      if (data == NULL || shdr == NULL)
	{
	  __libasm_seterrno (ASM_E_LIBELF);
	  return -1;
	}
      symscnndx = elf_ndxscn (symscn);

      
      data->d_size = gelf_fsize (ctx->out.elf, ELF_T_SYM,
				 ctx->nsymbol_tab + 1, EV_CURRENT);
      symtab = malloc (data->d_size);
      if (symtab == NULL)
	return -1;
      data->d_buf = symtab;
      data->d_type = ELF_T_SYM;
      data->d_off = 0;

      
      GElf_Sym syment;
      memset (&syment, '\0', sizeof (syment));
      (void) gelf_update_sym (data, 0, &syment);

      
      void *runp = NULL;
      int ptr_local = 1;	
      int ptr_nonlocal = ctx->nsymbol_tab;
      uint32_t *xshndx = NULL;
      AsmSym_t *sym;
      while ((sym = asm_symbol_tab_iterate (&ctx->symbol_tab, &runp)) != NULL)
	if (asm_emit_symbol_p (ebl_string (sym->strent)))
	  {
	    assert (ptr_local <= ptr_nonlocal);

	    syment.st_name = ebl_strtaboffset (sym->strent);
	    syment.st_info = GELF_ST_INFO (sym->binding, sym->type);
	    syment.st_other = 0;
	    syment.st_value = sym->scn->offset + sym->offset;
	    syment.st_size = sym->size;

	    int ptr = sym->binding == STB_LOCAL ? ptr_local++ : ptr_nonlocal--;

	    Elf_Scn *scn = (sym->scn->subsection_id == 0
			    ? sym->scn->data.main.scn
			    : sym->scn->data.up->data.main.scn);

	    Elf32_Word ndx;
	    if (unlikely (scn == ASM_ABS_SCN))
	      ndx = SHN_ABS;
	    else if (unlikely (scn == ASM_COM_SCN))
	      ndx = SHN_COMMON;
	    else if (unlikely ((ndx = elf_ndxscn (scn)) >= SHN_LORESERVE))
	      {
		if (unlikely (xshndx == NULL))
		  {
		    Elf_Scn *xndxscn;

		    xndxscn = elf_newscn (ctx->out.elf);
		    xndxdata = elf_newdata (xndxscn);
		    shdr = gelf_getshdr (xndxscn, &shdr_mem);
		    if (xndxdata == NULL || shdr == NULL)
		      {
			__libasm_seterrno (ASM_E_LIBELF);
			return -1;
		      }
		    xndxscnndx = elf_ndxscn (xndxscn);

		    shdr->sh_type = SHT_SYMTAB_SHNDX;
		    shdr->sh_entsize = sizeof (Elf32_Word);
		    shdr->sh_addralign = sizeof (Elf32_Word);
		    shdr->sh_link = symscnndx;

		    (void) gelf_update_shdr (xndxscn, shdr);

		    xndxscn_strent = ebl_strtabadd (ctx->section_strtab,
						    ".symtab_shndx", 14);

		    xndxdata->d_size = elf32_fsize (ELF_T_WORD,
						    ctx->nsymbol_tab + 1,
						    EV_CURRENT);
		    xshndx = xndxdata->d_buf = calloc (1, xndxdata->d_size);
		    if (xshndx == NULL)
		      return -1;
		    xndxdata->d_type = ELF_T_WORD;
		    xndxdata->d_off = 0;
		  }

		assert ((size_t) ptr < ctx->nsymbol_tab + 1);
		xshndx[ptr] = ndx;

		
		ndx = SHN_XINDEX;
	      }
	    syment.st_shndx = ndx;

	    
	    sym->symidx = ptr;

	    (void) gelf_update_sym (data, ptr, &syment);
	  }

      assert (ptr_local == ptr_nonlocal + 1);

      shdr->sh_type = SHT_SYMTAB;
      shdr->sh_link = strscnndx;
      shdr->sh_info = ptr_local;
      shdr->sh_entsize = gelf_fsize (ctx->out.elf, ELF_T_SYM, 1, EV_CURRENT);
      shdr->sh_addralign = gelf_fsize (ctx->out.elf, ELF_T_ADDR, 1,
				       EV_CURRENT);

      (void) gelf_update_shdr (symscn, shdr);
    }


  shstrscn = elf_newscn (ctx->out.elf);
  shstrtabdata = elf_newdata (shstrscn);
  shdr = gelf_getshdr (shstrscn, &shdr_mem);
  if (shstrscn == NULL || shstrtabdata == NULL || shdr == NULL)
    {
      __libasm_seterrno (ASM_E_LIBELF);
      return -1;
    }


  
  shstrscn_strent = ebl_strtabadd (ctx->section_strtab, ".shstrtab", 10);

  ebl_strtabfinalize (ctx->section_strtab, shstrtabdata);

  shdr->sh_type = SHT_STRTAB;
  assert (shdr->sh_entsize == 0);
  shdr->sh_name = ebl_strtaboffset (shstrscn_strent);

  (void) gelf_update_shdr (shstrscn, shdr);


  
  if (ctx->groups != NULL)
    {
      AsmScnGrp_t *runp = ctx->groups->next;

      do
	{
	  Elf_Scn *scn;
	  Elf32_Word *grpdata;

	  scn = runp->scn;
	  assert (scn != NULL);
	  shdr = gelf_getshdr (scn, &shdr_mem);
	  assert (shdr != NULL);

	  data = elf_newdata (scn);
	  if (data == NULL)
	    {
	      __libasm_seterrno (ASM_E_LIBELF);
	      return -1;
	    }

	  data->d_size = elf32_fsize (ELF_T_WORD, runp->nmembers + 1,
				      EV_CURRENT);
	  grpdata = data->d_buf = malloc (data->d_size);
	  if (grpdata == NULL)
	    return -1;
	  data->d_type = ELF_T_WORD;
	  data->d_off = 0;
	  data->d_align = elf32_fsize (ELF_T_WORD, 1, EV_CURRENT);

	  
	  *grpdata++ = runp->flags;

	  if (runp->members != NULL)
	    {
	      AsmScn_t *member = runp->members->data.main.next_in_group;

	      do
		{
		  assert (member->subsection_id == 0);

		  *grpdata++ = elf_ndxscn (member->data.main.scn);
		}
	      while ((member = member->data.main.next_in_group)
		     != runp->members->data.main.next_in_group);
	    }

	  
	  shdr->sh_name = ebl_strtaboffset (runp->strent);
	  shdr->sh_type = SHT_GROUP;
	  shdr->sh_flags = 0;
	  shdr->sh_link = symscnndx;
	  shdr->sh_info = (runp->signature != NULL
			   ? runp->signature->symidx : 0);

	  (void) gelf_update_shdr (scn, shdr);
	}
      while ((runp = runp->next) != ctx->groups->next);
    }


  
  if (likely (symscnndx != 0))
    {
      Elf_Scn *scn = elf_getscn (ctx->out.elf, symscnndx);

      shdr = gelf_getshdr (scn, &shdr_mem);

      shdr->sh_name = ebl_strtaboffset (symscn_strent);

      (void) gelf_update_shdr (scn, shdr);


      
      assert (strscnndx != 0);
      scn = elf_getscn (ctx->out.elf, strscnndx);

      shdr = gelf_getshdr (scn, &shdr_mem);

      shdr->sh_name = ebl_strtaboffset (strscn_strent);

      (void) gelf_update_shdr (scn, shdr);


      
      if (xndxscnndx != 0)
	{
	  scn = elf_getscn (ctx->out.elf, xndxscnndx);

	  shdr = gelf_getshdr (scn, &shdr_mem);

	  shdr->sh_name = ebl_strtaboffset (xndxscn_strent);

	  (void) gelf_update_shdr (scn, shdr);
	}
    }


  
  for (asmscn = ctx->section_list; asmscn != NULL; asmscn = asmscn->allnext)
    {
      shdr = gelf_getshdr (asmscn->data.main.scn, &shdr_mem);
      
      assert (shdr != NULL);

      shdr->sh_name = ebl_strtaboffset (asmscn->data.main.strent);

      
      shdr->sh_addralign = asmscn->max_align;

      (void) gelf_update_shdr (asmscn->data.main.scn, shdr);
    }

  ehdr = gelf_getehdr (ctx->out.elf, &ehdr_mem);
  assert (ehdr != NULL);

  shstrscnndx = elf_ndxscn (shstrscn);
  if (unlikely (shstrscnndx > SHN_HIRESERVE)
      || unlikely (shstrscnndx == SHN_XINDEX))
    {
      
      Elf_Scn *scn = elf_getscn (ctx->out.elf, 0);

      
      shdr = gelf_getshdr (scn, &shdr_mem);
      
      assert (shdr != NULL);

      
      shdr->sh_link = shstrscnndx;

      (void) gelf_update_shdr (scn, shdr);

      
      ehdr->e_shstrndx = SHN_XINDEX;
    }
  else
    ehdr->e_shstrndx = elf_ndxscn (shstrscn);

  gelf_update_ehdr (ctx->out.elf, ehdr);

  
  if (unlikely (elf_update (ctx->out.elf, ELF_C_WRITE_MMAP)) < 0)
    {
      __libasm_seterrno (ASM_E_LIBELF);
      result = -1;
    }

  
  free (shstrtabdata->d_buf);
  if (strtabdata != NULL)
    free (strtabdata->d_buf);
  
  if (xndxdata != NULL)
    free (xndxdata->d_buf);

  
  AsmScnGrp_t *scngrp = ctx->groups;
  if (scngrp != NULL)
    do
      free (elf_getdata (scngrp->scn, NULL)->d_buf);
    while ((scngrp = scngrp->next) != ctx->groups);

  
  if (unlikely (elf_end (ctx->out.elf)) != 0)
    {
      __libasm_seterrno (ASM_E_LIBELF);
      result = -1;
    }

  
  free (symtab);

  return result;
}


int
asm_end (ctx)
     AsmCtx_t *ctx;
{
  int result;

  if (ctx == NULL)
    
    return -1;

  result = unlikely (ctx->textp) ? text_end (ctx) : binary_end (ctx);
  if (result != 0)
    return result;

  
  if (fchmod (ctx->fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH) != 0)
    {
      __libasm_seterrno (ASM_E_CANNOT_CHMOD);
      return -1;
    }

  
  if (rename (ctx->tmp_fname, ctx->fname) != 0)
    {
      __libasm_seterrno (ASM_E_CANNOT_RENAME);
      return -1;
    }

  
  __libasm_finictx (ctx);

  return 0;
}


static void
free_section (AsmScn_t *scnp)
{
  void *oldp;

  if (scnp->subnext != NULL)
    free_section (scnp->subnext);

  struct AsmData *data = scnp->content;
  if (data != NULL)
    do
      {
	oldp = data;
	data = data->next;
	free (oldp);
      }
    while (oldp != scnp->content);

  free (scnp);
}


void
__libasm_finictx (ctx)
     AsmCtx_t *ctx;
{
  
  AsmScn_t *scn = ctx->section_list;
  while (scn != NULL)
    {
      AsmScn_t *oldp = scn;
      scn = scn->allnext;
      free_section (oldp);
    }

  
  void *runp = NULL;
  AsmSym_t *sym;
  while ((sym = asm_symbol_tab_iterate (&ctx->symbol_tab, &runp)) != NULL)
    free (sym);
  asm_symbol_tab_free (&ctx->symbol_tab);


  
  AsmScnGrp_t *scngrp = ctx->groups;
  if (scngrp != NULL)
    do
      {
	AsmScnGrp_t *oldp = scngrp;

	scngrp = scngrp->next;
	free (oldp);
      }
    while (scngrp != ctx->groups);


  if (unlikely (ctx->textp))
    {
      
      fclose (ctx->out.file);
    }
  else
    {
      
      (void) close (ctx->fd);

      
      ebl_strtabfree (ctx->section_strtab);
      ebl_strtabfree (ctx->symbol_strtab);
    }

  
  rwlock_fini (ctx->lock);

  
  free (ctx);
}
