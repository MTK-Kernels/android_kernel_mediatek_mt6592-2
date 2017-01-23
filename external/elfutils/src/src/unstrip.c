/* Combine stripped files with separate symbols and debug information.
   Copyright (C) 2007-2012, 2014 Red Hat, Inc.
   This file is part of elfutils.
   Written by Roland McGrath <roland@redhat.com>, 2007.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */


#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <argp.h>
#include <assert.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <libintl.h>
#include <locale.h>
#include <mcheck.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <gelf.h>
#include <libebl.h>
#include <libdwfl.h>
#include "system.h"

#ifndef _
# define _(str) gettext (str)
#endif

static void print_version (FILE *stream, struct argp_state *state);
ARGP_PROGRAM_VERSION_HOOK_DEF = print_version;

ARGP_PROGRAM_BUG_ADDRESS_DEF = PACKAGE_BUGREPORT;

static const struct argp_option options[] =
{
  
  { "match-file-names", 'f', NULL, 0,
    N_("Match MODULE against file names, not module names"), 2 },
  { "ignore-missing", 'i', NULL, 0, N_("Silently skip unfindable files"), 0 },

  { NULL, 0, NULL, 0, N_("Output options:"), 0 },
  { "output", 'o', "FILE", 0, N_("Place output into FILE"), 0 },
  { "output-directory", 'd', "DIRECTORY",
    0, N_("Create multiple output files under DIRECTORY"), 0 },
  { "module-names", 'm', NULL, 0, N_("Use module rather than file names"), 0 },
  { "all", 'a', NULL, 0,
    N_("Create output for modules that have no separate debug information"),
    0 },
  { "relocate", 'R', NULL, 0,
    N_("Apply relocations to section contents in ET_REL files"), 0 },
  { "list-only", 'n', NULL, 0,
    N_("Only list module and file names, build IDs"), 0 },
 { "force", 'F', NULL, 0,
    N_("Force combining files even if some ELF headers don't seem to match"),
   0 },
  { NULL, 0, NULL, 0, NULL, 0 }
};

struct arg_info
{
  const char *output_file;
  const char *output_dir;
  Dwfl *dwfl;
  char **args;
  bool list;
  bool all;
  bool ignore;
  bool modnames;
  bool match_files;
  bool relocate;
  bool force;
};

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  struct arg_info *info = state->input;

  switch (key)
    {
    case ARGP_KEY_INIT:
      state->child_inputs[0] = &info->dwfl;
      break;

    case 'o':
      if (info->output_file != NULL)
	{
	  argp_error (state, _("-o option specified twice"));
	  return EINVAL;
	}
      info->output_file = arg;
      break;

    case 'd':
      if (info->output_dir != NULL)
	{
	  argp_error (state, _("-d option specified twice"));
	  return EINVAL;
	}
      info->output_dir = arg;
      break;

    case 'm':
      info->modnames = true;
      break;
    case 'f':
      info->match_files = true;
      break;
    case 'a':
      info->all = true;
      break;
    case 'i':
      info->ignore = true;
      break;
    case 'n':
      info->list = true;
      break;
    case 'R':
      info->relocate = true;
      break;
    case 'F':
      info->force = true;
      break;

    case ARGP_KEY_ARGS:
    case ARGP_KEY_NO_ARGS:
      
      info->args = &state->argv[state->next];

      if (info->output_file != NULL && info->output_dir != NULL)
	{
	  argp_error (state, _("only one of -o or -d allowed"));
	  return EINVAL;
	}

      if (info->list && (info->dwfl == NULL
			 || info->output_dir != NULL
			 || info->output_file != NULL))
	{
	  argp_error (state,
		      _("-n cannot be used with explicit files or -o or -d"));
	  return EINVAL;
	}

      if (info->output_dir != NULL)
	{
	  struct stat64 st;
	  error_t fail = 0;
	  if (stat64 (info->output_dir, &st) < 0)
	    fail = errno;
	  else if (!S_ISDIR (st.st_mode))
	    fail = ENOTDIR;
	  if (fail)
	    {
	      argp_failure (state, EXIT_FAILURE, fail,
			    _("output directory '%s'"), info->output_dir);
	      return fail;
	    }
	}

      if (info->dwfl == NULL)
	{
	  if (state->next + 2 != state->argc)
	    {
	      argp_error (state, _("exactly two file arguments are required"));
	      return EINVAL;
	    }

	  if (info->ignore || info->all || info->modnames || info->relocate)
	    {
	      argp_error (state, _("\
-m, -a, -R, and -i options not allowed with explicit files"));
	      return EINVAL;
	    }

	  return ENOSYS;
	}
      else if (info->output_file == NULL && info->output_dir == NULL
	       && !info->list)
	{
	  argp_error (state,
		      _("-o or -d is required when using implicit files"));
	  return EINVAL;
	}
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

static void
print_version (FILE *stream, struct argp_state *state __attribute__ ((unused)))
{
  fprintf (stream, "unstrip (%s) %s\n", PACKAGE_NAME, PACKAGE_VERSION);
  fprintf (stream, _("\
Copyright (C) %s Red Hat, Inc.\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
"), "2012");
  fprintf (stream, gettext ("Written by %s.\n"), "Roland McGrath");
}

#define ELF_CHECK(call, msg)						      \
  do									      \
    {									      \
      if (unlikely (!(call)))						      \
	error (EXIT_FAILURE, 0, msg, elf_errmsg (-1));			      \
    } while (0)

static void
copy_elf (Elf *outelf, Elf *inelf)
{
  ELF_CHECK (gelf_newehdr (outelf, gelf_getclass (inelf)),
	     _("cannot create ELF header: %s"));

  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr = gelf_getehdr (inelf, &ehdr_mem);
  ELF_CHECK (gelf_update_ehdr (outelf, ehdr),
	     _("cannot copy ELF header: %s"));

  size_t phnum;
  ELF_CHECK (elf_getphdrnum (inelf, &phnum) == 0,
	     _("cannot get number of program headers: %s"));

  if (phnum > 0)
    {
      ELF_CHECK (gelf_newphdr (outelf, phnum),
		 _("cannot create program headers: %s"));

      GElf_Phdr phdr_mem;
      for (size_t i = 0; i < phnum; ++i)
	ELF_CHECK (gelf_update_phdr (outelf, i,
				     gelf_getphdr (inelf, i, &phdr_mem)),
		   _("cannot copy program header: %s"));
    }

  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (inelf, scn)) != NULL)
    {
      Elf_Scn *newscn = elf_newscn (outelf);

      GElf_Shdr shdr_mem;
      ELF_CHECK (gelf_update_shdr (newscn, gelf_getshdr (scn, &shdr_mem)),
		 _("cannot copy section header: %s"));

      Elf_Data *data = elf_getdata (scn, NULL);
      ELF_CHECK (data != NULL, _("cannot get section data: %s"));
      Elf_Data *newdata = elf_newdata (newscn);
      ELF_CHECK (newdata != NULL, _("cannot copy section data: %s"));
      *newdata = *data;
      elf_flagdata (newdata, ELF_C_SET, ELF_F_DIRTY);
    }
}

static void
make_directories (const char *path)
{
  const char *lastslash = strrchr (path, '/');
  if (lastslash == NULL)
    return;

  while (lastslash > path && lastslash[-1] == '/')
    --lastslash;
  if (lastslash == path)
    return;

  char *dir = strndupa (path, lastslash - path);
  while (mkdir (dir, 0777) < 0 && errno != EEXIST)
    if (errno == ENOENT)
      make_directories (dir);
    else
      error (EXIT_FAILURE, errno, _("cannot create directory '%s'"), dir);
}


static bool
section_can_shrink (const GElf_Shdr *shdr)
{
  switch (shdr->sh_type)
    {
    case SHT_SYMTAB:
    case SHT_DYNSYM:
    case SHT_HASH:
    case SHT_GNU_versym:
      return true;
    }
  return false;
}

static size_t
symtab_count_leading_section_symbols (Elf *elf, Elf_Scn *scn, size_t shnum,
				      Elf_Data *newsymdata)
{
  Elf_Data *data = elf_getdata (scn, NULL);
  Elf_Data *shndxdata = NULL;	

  for (size_t i = 1; i < shnum; ++i)
    {
      GElf_Sym sym_mem;
      GElf_Word shndx = SHN_UNDEF;
      GElf_Sym *sym = gelf_getsymshndx (data, shndxdata, i, &sym_mem, &shndx);
      ELF_CHECK (sym != NULL, _("cannot get symbol table entry: %s"));

      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (elf_getscn (elf, i), &shdr_mem);
      ELF_CHECK (shdr != NULL, _("cannot get section header: %s"));

      if (sym->st_shndx != SHN_XINDEX)
	shndx = sym->st_shndx;

      if (shndx != i || GELF_ST_TYPE (sym->st_info) != STT_SECTION)
	return i;

      sym->st_value = shdr->sh_addr;
      if (sym->st_shndx != SHN_XINDEX)
	shndx = SHN_UNDEF;
      ELF_CHECK (gelf_update_symshndx (newsymdata, shndxdata, i, sym, shndx),
		 _("cannot update symbol table: %s"));
    }

  return shnum;
}

static void
update_shdr (Elf_Scn *outscn, GElf_Shdr *newshdr)
{
  ELF_CHECK (gelf_update_shdr (outscn, newshdr),
	     _("cannot update section header: %s"));
}

static void
update_sh_size (Elf_Scn *outscn, const Elf_Data *data)
{
  GElf_Shdr shdr_mem;
  GElf_Shdr *newshdr = gelf_getshdr (outscn, &shdr_mem);
  ELF_CHECK (newshdr != NULL, _("cannot get section header: %s"));

  newshdr->sh_size = data->d_size;

  update_shdr (outscn, newshdr);
}

static void
adjust_relocs (Elf_Scn *outscn, Elf_Scn *inscn, const GElf_Shdr *shdr,
	       size_t map[], const GElf_Shdr *symshdr)
{
  Elf_Data *data = elf_getdata (outscn, NULL);

  inline void adjust_reloc (GElf_Xword *info)
    {
      size_t ndx = GELF_R_SYM (*info);
      if (ndx != STN_UNDEF)
	*info = GELF_R_INFO (map[ndx - 1], GELF_R_TYPE (*info));
    }

  switch (shdr->sh_type)
    {
    case SHT_REL:
      for (size_t i = 0; i < shdr->sh_size / shdr->sh_entsize; ++i)
	{
	  GElf_Rel rel_mem;
	  GElf_Rel *rel = gelf_getrel (data, i, &rel_mem);
	  adjust_reloc (&rel->r_info);
	  ELF_CHECK (gelf_update_rel (data, i, rel),
		     _("cannot update relocation: %s"));
	}
      break;

    case SHT_RELA:
      for (size_t i = 0; i < shdr->sh_size / shdr->sh_entsize; ++i)
	{
	  GElf_Rela rela_mem;
	  GElf_Rela *rela = gelf_getrela (data, i, &rela_mem);
	  adjust_reloc (&rela->r_info);
	  ELF_CHECK (gelf_update_rela (data, i, rela),
		     _("cannot update relocation: %s"));
	}
      break;

    case SHT_GROUP:
      {
	GElf_Shdr shdr_mem;
	GElf_Shdr *newshdr = gelf_getshdr (outscn, &shdr_mem);
	ELF_CHECK (newshdr != NULL, _("cannot get section header: %s"));
	if (newshdr->sh_info != STN_UNDEF)
	  {
	    newshdr->sh_info = map[newshdr->sh_info - 1];
	    update_shdr (outscn, newshdr);
	  }
	break;
      }

    case SHT_HASH:
      
      {
	const size_t nsym = symshdr->sh_size / symshdr->sh_entsize;
	const size_t onent = shdr->sh_size / shdr->sh_entsize;
	assert (data->d_size == shdr->sh_size);

#define CONVERT_HASH(Hash_Word)						      \
	{								      \
	  const Hash_Word *const old_hash = data->d_buf;		      \
	  const size_t nbucket = old_hash[0];				      \
	  const size_t nchain = old_hash[1];				      \
	  const Hash_Word *const old_bucket = &old_hash[2];		      \
	  const Hash_Word *const old_chain = &old_bucket[nbucket];	      \
	  assert (onent == 2 + nbucket + nchain);			      \
									      \
	  const size_t nent = 2 + nbucket + nsym;			      \
	  Hash_Word *const new_hash = xcalloc (nent, sizeof new_hash[0]);     \
	  Hash_Word *const new_bucket = &new_hash[2];			      \
	  Hash_Word *const new_chain = &new_bucket[nbucket];		      \
									      \
	  new_hash[0] = nbucket;					      \
	  new_hash[1] = nsym;						      \
	  for (size_t i = 0; i < nbucket; ++i)				      \
	    if (old_bucket[i] != STN_UNDEF)				      \
	      new_bucket[i] = map[old_bucket[i] - 1];			      \
									      \
	  for (size_t i = 1; i < nchain; ++i)				      \
	    if (old_chain[i] != STN_UNDEF)				      \
	      new_chain[map[i - 1]] = map[old_chain[i] - 1];		      \
									      \
	  data->d_buf = new_hash;					      \
	  data->d_size = nent * sizeof new_hash[0];			      \
	}

	switch (shdr->sh_entsize)
	  {
	  case 4:
	    CONVERT_HASH (Elf32_Word);
	    break;
	  case 8:
	    CONVERT_HASH (Elf64_Xword);
	    break;
	  default:
	    abort ();
	  }

	elf_flagdata (data, ELF_C_SET, ELF_F_DIRTY);
	update_sh_size (outscn, data);

#undef	CONVERT_HASH
      }
      break;

    case SHT_GNU_versym:
      
      {
	const size_t nent = symshdr->sh_size / symshdr->sh_entsize;
	const size_t onent = shdr->sh_size / shdr->sh_entsize;
	assert (nent >= onent);

	assert (sizeof (Elf32_Versym) == sizeof (GElf_Versym));
	assert (sizeof (Elf64_Versym) == sizeof (GElf_Versym));
	GElf_Versym *versym = xcalloc (nent, sizeof versym[0]);

	for (size_t i = 1; i < onent; ++i)
	  {
	    GElf_Versym *v = gelf_getversym (data, i, &versym[map[i - 1]]);
	    ELF_CHECK (v != NULL, _("cannot get symbol version: %s"));
	  }

	data->d_buf = versym;
	data->d_size = nent * shdr->sh_entsize;
	elf_flagdata (data, ELF_C_SET, ELF_F_DIRTY);
	update_sh_size (outscn, data);
      }
      break;

    default:
      error (EXIT_FAILURE, 0,
	     _("unexpected section type in [%Zu] with sh_link to symtab"),
	     elf_ndxscn (inscn));
    }
}

static void
adjust_all_relocs (Elf *elf, Elf_Scn *symtab, const GElf_Shdr *symshdr,
		   size_t map[])
{
  size_t new_sh_link = elf_ndxscn (symtab);
  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    if (scn != symtab)
      {
	GElf_Shdr shdr_mem;
	GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
	ELF_CHECK (shdr != NULL, _("cannot get section header: %s"));
	if (shdr->sh_type != SHT_NOBITS && shdr->sh_link == new_sh_link)
	  adjust_relocs (scn, scn, shdr, map, symshdr);
      }
}

static Elf_Data *
add_new_section_symbols (Elf_Scn *old_symscn, size_t old_shnum,
			 Elf *elf, bool rel, Elf_Scn *symscn, size_t shnum)
{
  const size_t added = shnum - old_shnum;

  GElf_Shdr shdr_mem;
  GElf_Shdr *shdr = gelf_getshdr (symscn, &shdr_mem);
  ELF_CHECK (shdr != NULL, _("cannot get section header: %s"));

  const size_t nsym = shdr->sh_size / shdr->sh_entsize;
  size_t symndx_map[nsym - 1];

  shdr->sh_info += added;
  shdr->sh_size += added * shdr->sh_entsize;
  update_shdr (symscn, shdr);

  Elf_Data *symdata = elf_getdata (symscn, NULL);
  Elf_Data *shndxdata = NULL;	

  symdata->d_size = shdr->sh_size;
  symdata->d_buf = xmalloc (symdata->d_size);

  
  Elf_Data *old_symdata = elf_getdata (old_symscn, NULL);
  for (size_t i = 0; i < old_shnum; ++i)
    {
      GElf_Sym sym_mem;
      GElf_Word shndx = SHN_UNDEF;
      GElf_Sym *sym = gelf_getsymshndx (old_symdata, shndxdata,
					i, &sym_mem, &shndx);
      ELF_CHECK (gelf_update_symshndx (symdata, shndxdata, i,
				       sym, shndx),
		 _("cannot update symbol table: %s"));

      if (i > 0)
	symndx_map[i - 1] = i;
    }

  
  for (size_t i = old_shnum; i < shnum; ++i)
    {
      GElf_Shdr i_shdr_mem;
      GElf_Shdr *i_shdr = gelf_getshdr (elf_getscn (elf, i), &i_shdr_mem);
      ELF_CHECK (i_shdr != NULL, _("cannot get section header: %s"));
      GElf_Sym sym =
	{
	  .st_value = rel ? 0 : i_shdr->sh_addr,
	  .st_info = GELF_ST_INFO (STB_LOCAL, STT_SECTION),
	  .st_shndx = i < SHN_LORESERVE ? i : SHN_XINDEX
	};
      GElf_Word shndx = i < SHN_LORESERVE ? SHN_UNDEF : i;
      ELF_CHECK (gelf_update_symshndx (symdata, shndxdata, i,
				       &sym, shndx),
		 _("cannot update symbol table: %s"));
    }

  
  for (size_t i = old_shnum; i < nsym; ++i)
    {
      GElf_Sym sym_mem;
      GElf_Word shndx = SHN_UNDEF;
      GElf_Sym *sym = gelf_getsymshndx (old_symdata, shndxdata,
					i, &sym_mem, &shndx);
      ELF_CHECK (gelf_update_symshndx (symdata, shndxdata,
				       i + added, sym, shndx),
		 _("cannot update symbol table: %s"));

      symndx_map[i - 1] = i + added;
    }

  
  adjust_all_relocs (elf, symscn, shdr, symndx_map);

  return symdata;
}

static Elf_Data *
check_symtab_section_symbols (Elf *elf, bool rel, Elf_Scn *scn,
			      size_t shnum, size_t shstrndx,
			      Elf_Scn *oscn, size_t oshnum, size_t oshstrndx,
			      size_t debuglink)
{
  size_t n = symtab_count_leading_section_symbols (elf, oscn, oshnum,
						   elf_getdata (scn, NULL));

  if (n == oshnum)
    return add_new_section_symbols (oscn, n, elf, rel, scn, shnum);

  if (n == oshstrndx || (n == debuglink && n == oshstrndx - 1))
    return add_new_section_symbols (oscn, n, elf, rel, scn, shstrndx);

  return NULL;
}

struct section
{
  Elf_Scn *scn;
  const char *name;
  Elf_Scn *outscn;
  struct Ebl_Strent *strent;
  GElf_Shdr shdr;
};

static int
compare_alloc_sections (const struct section *s1, const struct section *s2,
			bool rel)
{
  if (!rel)
    {
      
      if (s1->shdr.sh_addr < s2->shdr.sh_addr)
	return -1;
      if (s1->shdr.sh_addr > s2->shdr.sh_addr)
	return 1;
    }

  
  return (ssize_t) elf_ndxscn (s1->scn) - (ssize_t) elf_ndxscn (s2->scn);
}

static int
compare_unalloc_sections (const GElf_Shdr *shdr1, const GElf_Shdr *shdr2,
			  const char *name1, const char *name2)
{
  
  if (shdr1->sh_flags < shdr2->sh_flags)
    return -1;
  if (shdr1->sh_flags > shdr2->sh_flags)
    return 1;

  
  return strcmp (name1, name2);
}

static int
compare_sections (const void *a, const void *b, bool rel)
{
  const struct section *s1 = a;
  const struct section *s2 = b;

  
  if ((s1->shdr.sh_flags ^ s2->shdr.sh_flags) & SHF_ALLOC)
    return (s1->shdr.sh_flags & SHF_ALLOC) ? -1 : 1;

  return ((s1->shdr.sh_flags & SHF_ALLOC)
	  ? compare_alloc_sections (s1, s2, rel)
	  : compare_unalloc_sections (&s1->shdr, &s2->shdr,
				      s1->name, s2->name));
}

static int
compare_sections_rel (const void *a, const void *b)
{
  return compare_sections (a, b, true);
}

static int
compare_sections_nonrel (const void *a, const void *b)
{
  return compare_sections (a, b, false);
}


struct symbol
{
  size_t *map;

  union
  {
    const char *name;
    struct Ebl_Strent *strent;
  };
  union
  {
    struct
    {
      GElf_Addr value;
      GElf_Xword size;
      GElf_Word shndx;
      union
      {
	struct
	{
	  uint8_t info;
	  uint8_t other;
	} info;
	int16_t compare;
      };
    };

    size_t *duplicate;
  };
};

static void
collect_symbols (Elf *outelf, bool rel, Elf_Scn *symscn, Elf_Scn *strscn,
		 const size_t nent, const GElf_Addr bias,
		 const size_t scnmap[], struct symbol *table, size_t *map,
		 struct section *split_bss)
{
  Elf_Data *symdata = elf_getdata (symscn, NULL);
  Elf_Data *strdata = elf_getdata (strscn, NULL);
  Elf_Data *shndxdata = NULL;	

  for (size_t i = 1; i < nent; ++i)
    {
      GElf_Sym sym_mem;
      GElf_Word shndx = SHN_UNDEF;
      GElf_Sym *sym = gelf_getsymshndx (symdata, shndxdata, i,
					&sym_mem, &shndx);
      ELF_CHECK (sym != NULL, _("cannot get symbol table entry: %s"));
      if (sym->st_shndx != SHN_XINDEX)
	shndx = sym->st_shndx;

      if (sym->st_name >= strdata->d_size)
	error (EXIT_FAILURE, 0,
	       _("invalid string offset in symbol [%Zu]"), i);

      struct symbol *s = &table[i - 1];
      s->map = &map[i - 1];
      s->name = strdata->d_buf + sym->st_name;
      s->value = sym->st_value + bias;
      s->size = sym->st_size;
      s->shndx = shndx;
      s->info.info = sym->st_info;
      s->info.other = sym->st_other;

      if (scnmap != NULL && shndx != SHN_UNDEF && shndx < SHN_LORESERVE)
	s->shndx = scnmap[shndx - 1];

      if (GELF_ST_TYPE (s->info.info) == STT_SECTION && !rel)
	{
	  
	  GElf_Shdr shdr_mem;
	  GElf_Shdr *shdr = gelf_getshdr (elf_getscn (outelf, s->shndx),
					  &shdr_mem);
	  ELF_CHECK (shdr != NULL, _("cannot get section header: %s"));
	  s->value = shdr->sh_addr;
	}
      else if (split_bss != NULL
	       && s->value < split_bss->shdr.sh_addr
	       && s->value >= split_bss[-1].shdr.sh_addr
	       && shndx == elf_ndxscn (split_bss->outscn))
	
	s->shndx = elf_ndxscn (split_bss[-1].outscn);
    }
}


#define CMP(value)							      \
  if (s1->value < s2->value)						      \
    return -1;								      \
  if (s1->value > s2->value)						      \
    return 1

static int
compare_symbols (const void *a, const void *b)
{
  const struct symbol *s1 = a;
  const struct symbol *s2 = b;

  CMP (value);
  CMP (size);
  CMP (shndx);

  return (s1->compare - s2->compare) ?: strcmp (s1->name, s2->name);
}

static int
compare_symbols_output (const void *a, const void *b)
{
  const struct symbol *s1 = a;
  const struct symbol *s2 = b;
  int cmp;

  
  cmp = (s1->name == NULL) - (s2->name == NULL);

  if (cmp == 0)
    
    cmp = ((GELF_ST_BIND (s2->info.info) == STB_LOCAL)
	   - (GELF_ST_BIND (s1->info.info) == STB_LOCAL));

  if (cmp == 0)
    
    cmp = ((GELF_ST_TYPE (s2->info.info) == STT_SECTION)
	   - (GELF_ST_TYPE (s1->info.info) == STT_SECTION));

  if (cmp == 0)
    {
      if (GELF_ST_TYPE (s1->info.info) == STT_SECTION)
	{
	  
	  CMP (shndx);
	  else
	    assert (s1 == s2);
	}

      
      CMP (map);
      else
	assert (s1 == s2);
    }

  return cmp;
}

#undef CMP

static bool
sections_match (const struct section *sections, size_t i,
		const GElf_Shdr *shdr, const char *name)
{
  return (sections[i].shdr.sh_flags == shdr->sh_flags
	  && (sections[i].shdr.sh_size == shdr->sh_size
	      || (sections[i].shdr.sh_size < shdr->sh_size
		  && section_can_shrink (&sections[i].shdr)))
	  && !strcmp (sections[i].name, name));
}

static struct section *
find_alloc_section (const GElf_Shdr *shdr, GElf_Addr bias, const char *name,
		    struct section sections[], size_t nalloc)
{
  const GElf_Addr addr = shdr->sh_addr + bias;
  size_t l = 0, u = nalloc;
  while (l < u)
    {
      size_t i = (l + u) / 2;
      if (addr < sections[i].shdr.sh_addr)
	u = i;
      else if (addr > sections[i].shdr.sh_addr)
	l = i + 1;
      else
	{
	  while (i > 0 && sections[i - 1].shdr.sh_addr == addr)
	    --i;
	  for (; i < nalloc && sections[i].shdr.sh_addr == addr;
	       ++i)
	    if (sections_match (sections, i, shdr, name))
	      return &sections[i];
	  break;
	}
    }
  return NULL;
}

static inline const char *
get_section_name (size_t ndx, const GElf_Shdr *shdr, const Elf_Data *shstrtab)
{
  if (shdr->sh_name >= shstrtab->d_size)
    error (EXIT_FAILURE, 0, _("cannot read section [%Zu] name: %s"),
	   ndx, elf_errmsg (-1));
  return shstrtab->d_buf + shdr->sh_name;
}

static struct section *
find_alloc_sections_prelink (Elf *debug, Elf_Data *debug_shstrtab,
			     Elf *main, const GElf_Ehdr *main_ehdr,
			     Elf_Data *main_shstrtab, GElf_Addr bias,
			     struct section *sections,
			     size_t nalloc, size_t nsections)
{
  
  for (size_t i = 0; i < nalloc; ++i)
    sections[i].outscn = NULL;

  Elf_Scn *undo = NULL;
  for (size_t i = nalloc; i < nsections; ++i)
    {
      const struct section *sec = &sections[i];
      if (sec->shdr.sh_type == SHT_PROGBITS
	  && !(sec->shdr.sh_flags & SHF_ALLOC)
	  && !strcmp (sec->name, ".gnu.prelink_undo"))
	{
	  undo = sec->scn;
	  break;
	}
    }

  
  struct section *undo_sections = NULL;
  size_t undo_nalloc = 0;
  if (undo != NULL)
    {
      Elf_Data *undodata = elf_rawdata (undo, NULL);
      ELF_CHECK (undodata != NULL,
		 _("cannot read '.gnu.prelink_undo' section: %s"));

      union
      {
	Elf32_Ehdr e32;
	Elf64_Ehdr e64;
      } ehdr;
      Elf_Data dst =
	{
	  .d_buf = &ehdr,
	  .d_size = sizeof ehdr,
	  .d_type = ELF_T_EHDR,
	  .d_version = EV_CURRENT
	};
      Elf_Data src = *undodata;
      src.d_size = gelf_fsize (main, ELF_T_EHDR, 1, EV_CURRENT);
      src.d_type = ELF_T_EHDR;
      ELF_CHECK (gelf_xlatetom (main, &dst, &src,
				main_ehdr->e_ident[EI_DATA]) != NULL,
		 _("cannot read '.gnu.prelink_undo' section: %s"));

      uint_fast16_t phnum;
      uint_fast16_t shnum;
      if (ehdr.e32.e_ident[EI_CLASS] == ELFCLASS32)
	{
	  phnum = ehdr.e32.e_phnum;
	  shnum = ehdr.e32.e_shnum;
	}
      else
	{
	  phnum = ehdr.e64.e_phnum;
	  shnum = ehdr.e64.e_shnum;
	}

      size_t phsize = gelf_fsize (main, ELF_T_PHDR, phnum, EV_CURRENT);
      src.d_buf += src.d_size + phsize;
      src.d_size = gelf_fsize (main, ELF_T_SHDR, shnum - 1, EV_CURRENT);
      src.d_type = ELF_T_SHDR;
      if ((size_t) (src.d_buf - undodata->d_buf) > undodata->d_size
	  || undodata->d_size - (src.d_buf - undodata->d_buf) != src.d_size)
	error (EXIT_FAILURE, 0, _("invalid contents in '%s' section"),
	       ".gnu.prelink_undo");

      union
      {
	Elf32_Shdr s32[shnum - 1];
	Elf64_Shdr s64[shnum - 1];
      } shdr;
      dst.d_buf = &shdr;
      dst.d_size = sizeof shdr;
      ELF_CHECK (gelf_xlatetom (main, &dst, &src,
				main_ehdr->e_ident[EI_DATA]) != NULL,
		 _("cannot read '.gnu.prelink_undo' section: %s"));

      undo_sections = xmalloc ((shnum - 1) * sizeof undo_sections[0]);
      for (size_t i = 0; i < shnum - 1; ++i)
	{
	  struct section *sec = &undo_sections[undo_nalloc];
	  if (ehdr.e32.e_ident[EI_CLASS] == ELFCLASS32)
	    {
#define COPY(field) sec->shdr.field = shdr.s32[i].field
	      COPY (sh_name);
	      COPY (sh_type);
	      COPY (sh_flags);
	      COPY (sh_addr);
	      COPY (sh_offset);
	      COPY (sh_size);
	      COPY (sh_link);
	      COPY (sh_info);
	      COPY (sh_addralign);
	      COPY (sh_entsize);
#undef	COPY
	    }
	  else
	    sec->shdr = shdr.s64[i];
	  if (sec->shdr.sh_flags & SHF_ALLOC)
	    {
	      sec->shdr.sh_addr += bias;
	      sec->name = get_section_name (i + 1, &sec->shdr, main_shstrtab);
	      sec->scn = elf_getscn (main, i + 1); 
	      sec->outscn = NULL;
	      sec->strent = NULL;
	      ++undo_nalloc;
	    }
	}
      qsort (undo_sections, undo_nalloc,
	     sizeof undo_sections[0], compare_sections_nonrel);
    }

  bool fail = false;
  inline void check_match (bool match, Elf_Scn *scn, const char *name)
    {
      if (!match)
	{
	  fail = true;
	  error (0, 0, _("cannot find matching section for [%Zu] '%s'"),
		 elf_ndxscn (scn), name);
	}
    }

  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (debug, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
      ELF_CHECK (shdr != NULL, _("cannot get section header: %s"));

      if (!(shdr->sh_flags & SHF_ALLOC))
	continue;

      const char *name = get_section_name (elf_ndxscn (scn), shdr,
					   debug_shstrtab);

      if (undo_sections != NULL)
	{
	  struct section *sec = find_alloc_section (shdr, 0, name,
						    undo_sections,
						    undo_nalloc);
	  if (sec != NULL)
	    {
	      sec->outscn = scn;
	      continue;
	    }
	}

      for (size_t i = 0; shdr != NULL && i < nalloc; ++i)
	if (sections[i].outscn == scn)
	  shdr = NULL;
      check_match (shdr == NULL, scn, name);
    }

  if (fail)
    exit (EXIT_FAILURE);

  struct section *split_bss = NULL;
  for (size_t i = 0; i < undo_nalloc; ++i)
    {
      const struct section *undo_sec = &undo_sections[i];

      const char *name = undo_sec->name;
      scn = undo_sec->scn; 

      for (size_t j = 0; j < nalloc; ++j)
	{
	  struct section *sec = &sections[j];
#define RELA_SCALED(field) \
	  (2 * sec->shdr.field == 3 * undo_sec->shdr.field)
	  if (sec->outscn == NULL
	      && sec->shdr.sh_name == undo_sec->shdr.sh_name
	      && sec->shdr.sh_flags == undo_sec->shdr.sh_flags
	      && sec->shdr.sh_addralign == undo_sec->shdr.sh_addralign
	      && (((sec->shdr.sh_type == undo_sec->shdr.sh_type
		    && sec->shdr.sh_entsize == undo_sec->shdr.sh_entsize
		    && (sec->shdr.sh_size == undo_sec->shdr.sh_size
			|| (sec->shdr.sh_size > undo_sec->shdr.sh_size
			    && main_ehdr->e_type == ET_EXEC
			    && !strcmp (sec->name, ".dynstr"))))
		   || (sec->shdr.sh_size == undo_sec->shdr.sh_size
		       && ((sec->shdr.sh_entsize == undo_sec->shdr.sh_entsize
			    && undo_sec->shdr.sh_type == SHT_NOBITS)
			   || undo_sec->shdr.sh_type == SHT_PROGBITS)
		       && !strcmp (sec->name, ".plt")))
		  || (sec->shdr.sh_type == SHT_RELA
		      && undo_sec->shdr.sh_type == SHT_REL
		      && RELA_SCALED (sh_entsize) && RELA_SCALED (sh_size))
		  || (sec->shdr.sh_entsize == undo_sec->shdr.sh_entsize
		      && (sec->shdr.sh_type == undo_sec->shdr.sh_type
			  || (sec->shdr.sh_type == SHT_PROGBITS
			      && undo_sec->shdr.sh_type == SHT_NOBITS))
		      && sec->shdr.sh_size <= undo_sec->shdr.sh_size
		      && (!strcmp (sec->name, ".bss")
			  || !strcmp (sec->name, ".sbss"))
		      && (sec->shdr.sh_size == undo_sec->shdr.sh_size
			  || (split_bss = sec) > sections))))
	    {
	      sec->outscn = undo_sec->outscn;
	      undo_sec = NULL;
	      break;
	    }
	}

      check_match (undo_sec == NULL, scn, name);
    }

  free (undo_sections);

  if (fail)
    exit (EXIT_FAILURE);

  return split_bss;
}

static Elf_Data *
new_shstrtab (Elf *unstripped, size_t unstripped_shnum,
	      Elf_Data *shstrtab, size_t unstripped_shstrndx,
	      struct section *sections, size_t stripped_shnum,
	      struct Ebl_Strtab *strtab)
{
  if (strtab == NULL)
    return NULL;

  struct Ebl_Strent *unstripped_strent[unstripped_shnum - 1];
  memset (unstripped_strent, 0, sizeof unstripped_strent);
  for (struct section *sec = sections;
       sec < &sections[stripped_shnum - 1];
       ++sec)
    if (sec->outscn != NULL)
      {
	if (sec->strent == NULL)
	  {
	    sec->strent = ebl_strtabadd (strtab, sec->name, 0);
	    ELF_CHECK (sec->strent != NULL,
		       _("cannot add section name to string table: %s"));
	  }
	unstripped_strent[elf_ndxscn (sec->outscn) - 1] = sec->strent;
      }

  
  for (size_t i = 0; i < unstripped_shnum - 1; ++i)
    if (unstripped_strent[i] == NULL)
      {
	Elf_Scn *scn = elf_getscn (unstripped, i + 1);
	GElf_Shdr shdr_mem;
	GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
	const char *name = get_section_name (i + 1, shdr, shstrtab);
	unstripped_strent[i] = ebl_strtabadd (strtab, name, 0);
	ELF_CHECK (unstripped_strent[i] != NULL,
		   _("cannot add section name to string table: %s"));
      }
    else
      unstripped_strent[i] = NULL;

  
  Elf_Data *strtab_data = elf_getdata (elf_getscn (unstripped,
						   unstripped_shstrndx), NULL);
  ELF_CHECK (elf_flagdata (strtab_data, ELF_C_SET, ELF_F_DIRTY),
	     _("cannot update section header string table data: %s"));
  ebl_strtabfinalize (strtab, strtab_data);

  
  for (size_t i = 0; i < unstripped_shnum - 1; ++i)
    if (unstripped_strent[i] != NULL)
      {
	Elf_Scn *scn = elf_getscn (unstripped, i + 1);
	GElf_Shdr shdr_mem;
	GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
	shdr->sh_name = ebl_strtaboffset (unstripped_strent[i]);
	if (i + 1 == unstripped_shstrndx)
	  shdr->sh_size = strtab_data->d_size;
	update_shdr (scn, shdr);
      }

  return strtab_data;
}

static void
copy_elided_sections (Elf *unstripped, Elf *stripped,
		      const GElf_Ehdr *stripped_ehdr, GElf_Addr bias)
{
  size_t unstripped_shstrndx;
  ELF_CHECK (elf_getshdrstrndx (unstripped, &unstripped_shstrndx) == 0,
	     _("cannot get section header string table section index: %s"));

  size_t stripped_shstrndx;
  ELF_CHECK (elf_getshdrstrndx (stripped, &stripped_shstrndx) == 0,
	     _("cannot get section header string table section index: %s"));

  size_t unstripped_shnum;
  ELF_CHECK (elf_getshdrnum (unstripped, &unstripped_shnum) == 0,
	     _("cannot get section count: %s"));

  size_t stripped_shnum;
  ELF_CHECK (elf_getshdrnum (stripped, &stripped_shnum) == 0,
	     _("cannot get section count: %s"));

  if (unlikely (stripped_shnum > unstripped_shnum))
    error (EXIT_FAILURE, 0, _("\
more sections in stripped file than debug file -- arguments reversed?"));

  
  struct section sections[stripped_shnum - 1];
  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (stripped, scn)) != NULL)
    {
      size_t i = elf_ndxscn (scn) - 1;
      GElf_Shdr *shdr = gelf_getshdr (scn, &sections[i].shdr);
      ELF_CHECK (shdr != NULL, _("cannot get section header: %s"));
      sections[i].name = elf_strptr (stripped, stripped_shstrndx,
				     shdr->sh_name);
      if (sections[i].name == NULL)
	error (EXIT_FAILURE, 0, _("cannot read section [%Zu] name: %s"),
	       elf_ndxscn (scn), elf_errmsg (-1));
      sections[i].scn = scn;
      sections[i].outscn = NULL;
      sections[i].strent = NULL;
    }

  const struct section *stripped_symtab = NULL;

  
  qsort (sections, stripped_shnum - 1, sizeof sections[0],
	 stripped_ehdr->e_type == ET_REL
	 ? compare_sections_rel : compare_sections_nonrel);
  size_t nalloc = stripped_shnum - 1;
  while (nalloc > 0 && !(sections[nalloc - 1].shdr.sh_flags & SHF_ALLOC))
    {
      --nalloc;
      if (sections[nalloc].shdr.sh_type == SHT_SYMTAB)
	stripped_symtab = &sections[nalloc];
    }

  
  inline struct section *find_unalloc_section (const GElf_Shdr *shdr,
					       const char *name)
    {
      size_t l = nalloc, u = stripped_shnum - 1;
      while (l < u)
	{
	  size_t i = (l + u) / 2;
	  struct section *sec = &sections[i];
	  int cmp = compare_unalloc_sections (shdr, &sec->shdr,
					      name, sec->name);
	  if (cmp < 0)
	    u = i;
	  else if (cmp > 0)
	    l = i + 1;
	  else
	    return sec;
	}
      return NULL;
    }

  Elf_Data *shstrtab = elf_getdata (elf_getscn (unstripped,
						unstripped_shstrndx), NULL);
  ELF_CHECK (shstrtab != NULL,
	     _("cannot read section header string table: %s"));

  
  bool check_prelink = false;
  Elf_Scn *unstripped_symtab = NULL;
  size_t alloc_avail = 0;
  scn = NULL;
  while ((scn = elf_nextscn (unstripped, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
      ELF_CHECK (shdr != NULL, _("cannot get section header: %s"));

      if (shdr->sh_type == SHT_SYMTAB)
	{
	  unstripped_symtab = scn;
	  continue;
	}

      const size_t ndx = elf_ndxscn (scn);
      if (ndx == unstripped_shstrndx)
	continue;

      const char *name = get_section_name (ndx, shdr, shstrtab);

      struct section *sec = NULL;
      if (shdr->sh_flags & SHF_ALLOC)
	{
	  if (stripped_ehdr->e_type != ET_REL)
	    {
	      
	      sec = find_alloc_section (shdr, bias, name, sections, nalloc);
	      if (sec == NULL)
		{
		  
		  check_prelink = true;
		  continue;
		}
	    }
	  else
	    {
	      if (likely (sections_match (sections, alloc_avail, shdr, name)))
		sec = &sections[alloc_avail++];
	      else
		for (size_t i = alloc_avail + 1; i < nalloc; ++i)
		  if (sections_match (sections, i, shdr, name))
		    {
		      sec = &sections[i];
		      break;
		    }
	    }
	}
      else
	{
	  
	  sec = find_unalloc_section (shdr, name);
	  if (sec == NULL)
	    {
	      if (shdr->sh_type != SHT_NOBITS)
		continue;

	      if (!strcmp (name, ".comment"))
		continue;
	    }
	}

      if (sec == NULL)
	error (EXIT_FAILURE, 0,
	       _("cannot find matching section for [%Zu] '%s'"),
	       elf_ndxscn (scn), name);

      sec->outscn = scn;
    }

  struct section *split_bss = NULL;
  if (check_prelink)
    {
      Elf_Data *data = elf_getdata (elf_getscn (stripped, stripped_shstrndx),
				    NULL);
      ELF_CHECK (data != NULL,
		 _("cannot read section header string table: %s"));
      split_bss = find_alloc_sections_prelink (unstripped, shstrtab,
					       stripped, stripped_ehdr,
					       data, bias, sections,
					       nalloc, stripped_shnum - 1);
    }

  
  const struct section *stripped_dynsym = NULL;
  size_t debuglink = SHN_UNDEF;
  size_t ndx_section[stripped_shnum - 1];
  struct Ebl_Strtab *strtab = NULL;
  for (struct section *sec = sections;
       sec < &sections[stripped_shnum - 1];
       ++sec)
    {
      size_t secndx = elf_ndxscn (sec->scn);

      if (sec->outscn == NULL)
	{
	  

	  if (secndx == stripped_shstrndx)
	    {
	      
	      ndx_section[secndx - 1] = unstripped_shstrndx;
	      continue;
	    }

	  if (unstripped_symtab != NULL && sec == stripped_symtab)
	    {
	      
	      ndx_section[secndx - 1] = elf_ndxscn (unstripped_symtab);
	      continue;
	    }

	  if (unstripped_symtab != NULL && stripped_symtab != NULL
	      && secndx == stripped_symtab->shdr.sh_link)
	    {
	      
	      GElf_Shdr shdr_mem;
	      GElf_Shdr *shdr = gelf_getshdr (unstripped_symtab, &shdr_mem);
	      ELF_CHECK (shdr != NULL, _("cannot get section header: %s"));
	      ndx_section[secndx - 1] = shdr->sh_link;
	      continue;
	    }

	  if (!(sec->shdr.sh_flags & SHF_ALLOC)
	      && !strcmp (sec->name, ".gnu_debuglink"))
	    {
	      
	      debuglink = secndx;
	      ndx_section[secndx - 1] = SHN_UNDEF;
	      continue;
	    }

	  sec->outscn = elf_newscn (unstripped);
	  Elf_Data *newdata = elf_newdata (sec->outscn);
	  ELF_CHECK (newdata != NULL && gelf_update_shdr (sec->outscn,
							  &sec->shdr),
		     _("cannot add new section: %s"));

	  if (strtab == NULL)
	    strtab = ebl_strtabinit (true);
	  sec->strent = ebl_strtabadd (strtab, sec->name, 0);
	  ELF_CHECK (sec->strent != NULL,
		     _("cannot add section name to string table: %s"));
	}

      
      ndx_section[secndx - 1] = elf_ndxscn (sec->outscn);
    }

  
  Elf_Data *strtab_data = new_shstrtab (unstripped, unstripped_shnum,
					shstrtab, unstripped_shstrndx,
					sections, stripped_shnum,
					strtab);

  
  ELF_CHECK (elf_getshdrnum (unstripped, &unstripped_shnum) == 0,
	     _("cannot get section count: %s"));

  bool placed[unstripped_shnum - 1];
  memset (placed, 0, sizeof placed);

  
  GElf_Off offset = 0;
  for (const struct section *sec = sections;
       sec < &sections[stripped_shnum - 1];
       ++sec)
    if (sec->outscn != NULL)
      {
	GElf_Shdr shdr_mem;
	GElf_Shdr *shdr = gelf_getshdr (sec->outscn, &shdr_mem);
	ELF_CHECK (shdr != NULL, _("cannot get section header: %s"));

	if (stripped_ehdr->e_type != ET_REL
	    || !(shdr_mem.sh_flags & SHF_ALLOC)
	    || shdr_mem.sh_addr == 0)
	  shdr_mem.sh_addr = sec->shdr.sh_addr;

	shdr_mem.sh_type = sec->shdr.sh_type;
	shdr_mem.sh_size = sec->shdr.sh_size;
	shdr_mem.sh_info = sec->shdr.sh_info;
	shdr_mem.sh_link = sec->shdr.sh_link;
	if (sec->shdr.sh_link != SHN_UNDEF)
	  shdr_mem.sh_link = ndx_section[sec->shdr.sh_link - 1];
	if (shdr_mem.sh_flags & SHF_INFO_LINK)
	  shdr_mem.sh_info = ndx_section[sec->shdr.sh_info - 1];

	if (strtab != NULL)
	  shdr_mem.sh_name = ebl_strtaboffset (sec->strent);

	Elf_Data *indata = elf_getdata (sec->scn, NULL);
	ELF_CHECK (indata != NULL, _("cannot get section data: %s"));
	Elf_Data *outdata = elf_getdata (sec->outscn, NULL);
	ELF_CHECK (outdata != NULL, _("cannot copy section data: %s"));
	*outdata = *indata;
	elf_flagdata (outdata, ELF_C_SET, ELF_F_DIRTY);

	
	if (stripped_ehdr->e_type != ET_REL && (shdr_mem.sh_flags & SHF_ALLOC))
	  {
	    shdr_mem.sh_offset = sec->shdr.sh_offset;
	    placed[elf_ndxscn (sec->outscn) - 1] = true;

	    const GElf_Off end_offset = (shdr_mem.sh_offset
					 + (shdr_mem.sh_type == SHT_NOBITS
					    ? 0 : shdr_mem.sh_size));
	    if (end_offset > offset)
	      offset = end_offset;
	  }

	update_shdr (sec->outscn, &shdr_mem);

	if (shdr_mem.sh_type == SHT_SYMTAB || shdr_mem.sh_type == SHT_DYNSYM)
	  {
	    

	    Elf_Data *shndxdata = NULL;	

	    for (size_t i = 1; i < shdr_mem.sh_size / shdr_mem.sh_entsize; ++i)
	      {
		GElf_Sym sym_mem;
		GElf_Word shndx = SHN_UNDEF;
		GElf_Sym *sym = gelf_getsymshndx (outdata, shndxdata,
						  i, &sym_mem, &shndx);
		ELF_CHECK (sym != NULL,
			   _("cannot get symbol table entry: %s"));
		if (sym->st_shndx != SHN_XINDEX)
		  shndx = sym->st_shndx;

		if (shndx != SHN_UNDEF && shndx < SHN_LORESERVE)
		  {
		    if (shndx >= stripped_shnum)
		      error (EXIT_FAILURE, 0,
			     _("symbol [%Zu] has invalid section index"), i);

		    shndx = ndx_section[shndx - 1];
		    if (shndx < SHN_LORESERVE)
		      {
			sym->st_shndx = shndx;
			shndx = SHN_UNDEF;
		      }
		    else
		      sym->st_shndx = SHN_XINDEX;

		    ELF_CHECK (gelf_update_symshndx (outdata, shndxdata,
						     i, sym, shndx),
			       _("cannot update symbol table: %s"));
		  }
	      }

	    if (shdr_mem.sh_type == SHT_SYMTAB)
	      stripped_symtab = sec;
	    if (shdr_mem.sh_type == SHT_DYNSYM)
	      stripped_dynsym = sec;
	  }
      }

  
  Elf_Data *symdata = NULL;
  struct Ebl_Strtab *symstrtab = NULL;
  Elf_Data *symstrdata = NULL;
  if (unstripped_symtab != NULL && (stripped_symtab != NULL
				    || check_prelink 
				    || (stripped_ehdr->e_type != ET_REL
					&& bias != 0)))
    {
      
      const size_t stripped_nsym = (stripped_symtab == NULL ? 1
				    : (stripped_symtab->shdr.sh_size
				       / stripped_symtab->shdr.sh_entsize));

      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (unstripped_symtab, &shdr_mem);
      ELF_CHECK (shdr != NULL, _("cannot get section header: %s"));
      const size_t unstripped_nsym = shdr->sh_size / shdr->sh_entsize;

      

      const size_t total_syms = stripped_nsym - 1 + unstripped_nsym - 1;
      struct symbol symbols[total_syms];
      size_t symndx_map[total_syms];

      if (stripped_symtab != NULL)
	collect_symbols (unstripped, stripped_ehdr->e_type == ET_REL,
			 stripped_symtab->scn,
			 elf_getscn (stripped, stripped_symtab->shdr.sh_link),
			 stripped_nsym, 0, ndx_section,
			 symbols, symndx_map, NULL);

      Elf_Scn *unstripped_strtab = elf_getscn (unstripped, shdr->sh_link);
      collect_symbols (unstripped, stripped_ehdr->e_type == ET_REL,
		       unstripped_symtab, unstripped_strtab, unstripped_nsym,
		       stripped_ehdr->e_type == ET_REL ? 0 : bias, NULL,
		       &symbols[stripped_nsym - 1],
		       &symndx_map[stripped_nsym - 1], split_bss);

      
      qsort (symbols, total_syms, sizeof symbols[0], compare_symbols);

      size_t nsym = 0;
      for (struct symbol *s = symbols; s < &symbols[total_syms]; ++s)
	{
	  
	  if (s->shndx == SHN_UNDEF
	      && GELF_ST_TYPE (s->info.info) == STT_SECTION)
	    {
	      s->name = NULL;	
	      *s->map = STN_UNDEF;
	      s->duplicate = NULL;
	      continue;
	    }

	  struct symbol *n = s;
	  while (n + 1 < &symbols[total_syms] && !compare_symbols (s, n + 1))
	    ++n;

	  while (s < n)
	    {
	      
	      s->name = NULL;	
	      s->duplicate = n->map;
	      ++s;
	    }

	  
	  *s->map = ++nsym;
	}

      
      qsort (symbols, total_syms, sizeof symbols[0], compare_symbols_output);

      if (nsym < total_syms)
	
	assert (symbols[nsym].name == NULL);

      symstrtab = ebl_strtabinit (true);
      for (size_t i = 0; i < nsym; ++i)
	{
	  assert (symbols[i].name != NULL);
	  assert (*symbols[i].map != 0);
	  *symbols[i].map = 1 + i;
	  symbols[i].strent = ebl_strtabadd (symstrtab, symbols[i].name, 0);
	}

      for (size_t i = nsym; i < total_syms; ++i)
	{
	  assert (symbols[i].name == NULL);
	  if (symbols[i].duplicate == NULL)
	    assert (*symbols[i].map == STN_UNDEF);
	  else
	    {
	      assert (*symbols[i].duplicate != STN_UNDEF);
	      *symbols[i].map = *symbols[i].duplicate;
	    }
	}

      
      symdata = elf_getdata (unstripped_symtab, NULL);
      symstrdata = elf_getdata (unstripped_strtab, NULL);
      Elf_Data *shndxdata = NULL;	

      ebl_strtabfinalize (symstrtab, symstrdata);
      elf_flagdata (symstrdata, ELF_C_SET, ELF_F_DIRTY);

      shdr->sh_size = symdata->d_size = (1 + nsym) * shdr->sh_entsize;
      symdata->d_buf = xmalloc (symdata->d_size);

      GElf_Sym sym;
      memset (&sym, 0, sizeof sym);
      ELF_CHECK (gelf_update_symshndx (symdata, shndxdata, 0, &sym, SHN_UNDEF),
		 _("cannot update symbol table: %s"));

      shdr->sh_info = 1;
      for (size_t i = 0; i < nsym; ++i)
	{
	  struct symbol *s = &symbols[i];

	  
	  sym.st_name = ebl_strtaboffset (s->strent);
	  sym.st_value = s->value; 
	  sym.st_size = s->size;
	  sym.st_shndx = s->shndx; 
	  sym.st_info = s->info.info;
	  sym.st_other = s->info.other;

	  
	  if (GELF_ST_BIND (sym.st_info) == STB_LOCAL)
	    {
	      assert (shdr->sh_info == 1 + i);
	      shdr->sh_info = 1 + i + 1;
	    }

	  ELF_CHECK (gelf_update_symshndx (symdata, shndxdata, 1 + i,
					   &sym, SHN_UNDEF),
		     _("cannot update symbol table: %s"));

	}
      elf_flagdata (symdata, ELF_C_SET, ELF_F_DIRTY);
      update_shdr (unstripped_symtab, shdr);

      if (stripped_symtab != NULL)
	{
	  
	  const size_t old_sh_link = elf_ndxscn (stripped_symtab->scn);
	  for (const struct section *sec = sections;
	       sec < &sections[stripped_shnum - 1];
	       ++sec)
	    if (sec->outscn != NULL && sec->shdr.sh_link == old_sh_link)
	      adjust_relocs (sec->outscn, sec->scn, &sec->shdr,
			     symndx_map, shdr);
	}

      
      adjust_all_relocs (unstripped, unstripped_symtab, shdr,
			 &symndx_map[stripped_nsym - 1]);
    }
  else if (stripped_symtab != NULL && stripped_shnum != unstripped_shnum)
    check_symtab_section_symbols (unstripped,
				  stripped_ehdr->e_type == ET_REL,
				  stripped_symtab->scn,
				  unstripped_shnum, unstripped_shstrndx,
				  stripped_symtab->outscn,
				  stripped_shnum, stripped_shstrndx,
				  debuglink);

  if (stripped_dynsym != NULL)
    (void) check_symtab_section_symbols (unstripped,
					 stripped_ehdr->e_type == ET_REL,
					 stripped_dynsym->outscn,
					 unstripped_shnum,
					 unstripped_shstrndx,
					 stripped_dynsym->scn, stripped_shnum,
					 stripped_shstrndx, debuglink);


  elf_flagelf (unstripped, ELF_C_SET, ELF_F_LAYOUT);

  if (offset == 0)
    
    offset = gelf_fsize (unstripped, ELF_T_EHDR, 1, EV_CURRENT);

  bool skip_reloc = false;
  do
    {
      skip_reloc = !skip_reloc;
      for (size_t i = 0; i < unstripped_shnum - 1; ++i)
	if (!placed[i])
	  {
	    scn = elf_getscn (unstripped, 1 + i);

	    GElf_Shdr shdr_mem;
	    GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
	    ELF_CHECK (shdr != NULL, _("cannot get section header: %s"));

	    /* We must make sure we have read in the data of all sections
	       beforehand and marked them to be written out.  When we're
	       modifying the existing file in place, we might overwrite
	       this part of the file before we get to handling the section.  */

	    ELF_CHECK (elf_flagdata (elf_getdata (scn, NULL),
				     ELF_C_SET, ELF_F_DIRTY),
		       _("cannot read section data: %s"));

	    if (skip_reloc
		&& (shdr->sh_type == SHT_REL || shdr->sh_type == SHT_RELA))
	      continue;

	    GElf_Off align = shdr->sh_addralign ?: 1;
	    offset = (offset + align - 1) & -align;
	    shdr->sh_offset = offset;
	    if (shdr->sh_type != SHT_NOBITS)
	      offset += shdr->sh_size;

	    update_shdr (scn, shdr);

	    if (unstripped_shstrndx == 1 + i)
	      {

		GElf_Ehdr ehdr_mem;
		GElf_Ehdr *ehdr = gelf_getehdr (unstripped, &ehdr_mem);
		ELF_CHECK (ehdr != NULL, _("cannot get ELF header: %s"));

		GElf_Off sh_align = gelf_getclass (unstripped) * 4;
		offset = (offset + sh_align - 1) & -sh_align;
		ehdr->e_shnum = unstripped_shnum;
		ehdr->e_shoff = offset;
		offset += unstripped_shnum * ehdr->e_shentsize;
		ELF_CHECK (gelf_update_ehdr (unstripped, ehdr),
			   _("cannot update ELF header: %s"));
	      }

	    placed[i] = true;
	  }
    }
  while (skip_reloc);

  size_t phnum;
  ELF_CHECK (elf_getphdrnum (stripped, &phnum) == 0,
	     _("cannot get number of program headers: %s"));

  if (phnum > 0)
    ELF_CHECK (gelf_newphdr (unstripped, phnum),
	       _("cannot create program headers: %s"));

  
  for (size_t i = 0; i < phnum; ++i)
    {
      GElf_Phdr phdr_mem;
      GElf_Phdr *phdr = gelf_getphdr (stripped, i, &phdr_mem);
      ELF_CHECK (phdr != NULL, _("cannot get program header: %s"));

      ELF_CHECK (gelf_update_phdr (unstripped, i, phdr),
		 _("cannot update program header: %s"));
    }

  
  ELF_CHECK (elf_update (unstripped, ELF_C_WRITE) > 0,
	     _("cannot write output file: %s"));

  if (strtab != NULL)
    {
      ebl_strtabfree (strtab);
      free (strtab_data->d_buf);
    }

  if (symdata != NULL)
    free (symdata->d_buf);
  if (symstrtab != NULL)
    {
      ebl_strtabfree (symstrtab);
      free (symstrdata->d_buf);
    }
}

static void
handle_file (const char *output_file, bool create_dirs,
	     Elf *stripped, const GElf_Ehdr *stripped_ehdr,
	     Elf *unstripped)
{
  size_t phnum;
  ELF_CHECK (elf_getphdrnum (stripped, &phnum) == 0,
	     _("cannot get number of program headers: %s"));

  GElf_Addr bias = 0;
  if (unstripped != NULL)
    for (size_t i = 0; i < phnum; ++i)
      {
	GElf_Phdr phdr_mem;
	GElf_Phdr *phdr = gelf_getphdr (stripped, i, &phdr_mem);
	ELF_CHECK (phdr != NULL, _("cannot get program header: %s"));
	if (phdr->p_type == PT_LOAD)
	  {
	    GElf_Phdr unstripped_phdr_mem;
	    GElf_Phdr *unstripped_phdr = gelf_getphdr (unstripped, i,
						       &unstripped_phdr_mem);
	    ELF_CHECK (unstripped_phdr != NULL,
		       _("cannot get program header: %s"));
	    bias = phdr->p_vaddr - unstripped_phdr->p_vaddr;
	    break;
	  }
      }

  
  if (bias != 0)
    {
      if (output_file == NULL)
	error (0, 0, _("\
DWARF data not adjusted for prelinking bias; consider prelink -u"));
      else
	error (0, 0, _("\
DWARF data in '%s' not adjusted for prelinking bias; consider prelink -u"),
	       output_file);
    }

  if (output_file == NULL)
    
    copy_elided_sections (unstripped, stripped, stripped_ehdr, bias);
  else
    {
      if (create_dirs)
	make_directories (output_file);

      
      int outfd = open64 (output_file, O_RDWR | O_CREAT,
			  stripped_ehdr->e_type == ET_REL ? 0666 : 0777);
      if (outfd < 0)
	error (EXIT_FAILURE, errno, _("cannot open '%s'"), output_file);
      Elf *outelf = elf_begin (outfd, ELF_C_WRITE, NULL);
      ELF_CHECK (outelf != NULL, _("cannot create ELF descriptor: %s"));

      if (unstripped == NULL)
	{
	  
	  copy_elf (outelf, stripped);
	  if (stripped_ehdr->e_type != ET_REL)
	    elf_flagelf (outelf, ELF_C_SET, ELF_F_LAYOUT);
	  ELF_CHECK (elf_update (outelf, ELF_C_WRITE) > 0,
		     _("cannot write output file: %s"));
	}
      else
	{
	  copy_elf (outelf, unstripped);
	  copy_elided_sections (outelf, stripped, stripped_ehdr, bias);
	}

      elf_end (outelf);
      close (outfd);
    }
}

static int
open_file (const char *file, bool writable)
{
  int fd = open64 (file, writable ? O_RDWR : O_RDONLY);
  if (fd < 0)
    error (EXIT_FAILURE, errno, _("cannot open '%s'"), file);
  return fd;
}

static void
handle_explicit_files (const char *output_file, bool create_dirs, bool force,
		       const char *stripped_file, const char *unstripped_file)
{

  void warn (const char *msg)
  {
    error (force ? 0 : EXIT_FAILURE, 0, "%s'%s' and '%s' %s%s.",
	   force ? _("WARNING: ") : "",
	   stripped_file, unstripped_file, msg,
	   force ? "" : _(", use --force"));
  }

  int stripped_fd = open_file (stripped_file, false);
  Elf *stripped = elf_begin (stripped_fd, ELF_C_READ, NULL);
  GElf_Ehdr stripped_ehdr;
  ELF_CHECK (gelf_getehdr (stripped, &stripped_ehdr),
	     _("cannot create ELF descriptor: %s"));

  int unstripped_fd = -1;
  Elf *unstripped = NULL;
  if (unstripped_file != NULL)
    {
      unstripped_fd = open_file (unstripped_file, output_file == NULL);
      unstripped = elf_begin (unstripped_fd,
			      (output_file == NULL ? ELF_C_RDWR : ELF_C_READ),
			      NULL);
      GElf_Ehdr unstripped_ehdr;
      ELF_CHECK (gelf_getehdr (unstripped, &unstripped_ehdr),
		 _("cannot create ELF descriptor: %s"));

      if (memcmp (stripped_ehdr.e_ident,
		  unstripped_ehdr.e_ident, EI_NIDENT) != 0)
	warn (_("ELF header identification (e_ident) different"));

      if (stripped_ehdr.e_type != unstripped_ehdr.e_type)
	warn (_("ELF header type (e_type) different"));

      if (stripped_ehdr.e_machine != unstripped_ehdr.e_machine)
	warn (_("ELF header machine type (e_machine) different"));

      if (stripped_ehdr.e_phnum < unstripped_ehdr.e_phnum)
	warn (_("stripped program header (e_phnum) smaller than unstripped"));
    }

  handle_file (output_file, create_dirs, stripped, &stripped_ehdr, unstripped);

  elf_end (stripped);
  close (stripped_fd);

  elf_end (unstripped);
  close (unstripped_fd);
}


static void
handle_dwfl_module (const char *output_file, bool create_dirs, bool force,
		    Dwfl_Module *mod, bool all, bool ignore, bool relocate)
{
  GElf_Addr bias;
  Elf *stripped = dwfl_module_getelf (mod, &bias);
  if (stripped == NULL)
    {
      if (ignore)
	return;

      const char *file;
      const char *modname = dwfl_module_info (mod, NULL, NULL, NULL,
					      NULL, NULL, &file, NULL);
      if (file == NULL)
	error (EXIT_FAILURE, 0,
	       _("cannot find stripped file for module '%s': %s"),
	       modname, dwfl_errmsg (-1));
      else
	error (EXIT_FAILURE, 0,
	       _("cannot open stripped file '%s' for module '%s': %s"),
	       modname, file, dwfl_errmsg (-1));
    }

  Elf *debug = dwarf_getelf (dwfl_module_getdwarf (mod, &bias));
  if (debug == NULL && !all)
    {
      if (ignore)
	return;

      const char *file;
      const char *modname = dwfl_module_info (mod, NULL, NULL, NULL,
					      NULL, NULL, NULL, &file);
      if (file == NULL)
	error (EXIT_FAILURE, 0,
	       _("cannot find debug file for module '%s': %s"),
	       modname, dwfl_errmsg (-1));
      else
	error (EXIT_FAILURE, 0,
	       _("cannot open debug file '%s' for module '%s': %s"),
	       modname, file, dwfl_errmsg (-1));
    }

  if (debug == stripped)
    {
      if (all)
	debug = NULL;
      else
	{
	  const char *file;
	  const char *modname = dwfl_module_info (mod, NULL, NULL, NULL,
						  NULL, NULL, &file, NULL);
	  error (EXIT_FAILURE, 0, _("module '%s' file '%s' is not stripped"),
		 modname, file);
	}
    }

  GElf_Ehdr stripped_ehdr;
  ELF_CHECK (gelf_getehdr (stripped, &stripped_ehdr),
	     _("cannot create ELF descriptor: %s"));

  if (stripped_ehdr.e_type == ET_REL)
    {
      if (!relocate)
	{

	  const char *stripped_file = NULL;
	  const char *unstripped_file = NULL;
	  (void) dwfl_module_info (mod, NULL, NULL, NULL, NULL, NULL,
				   &stripped_file, &unstripped_file);

	  handle_explicit_files (output_file, create_dirs, force,
				 stripped_file, unstripped_file);
	  return;
	}

      if (dwfl_module_relocations (mod) < 0)
	error (EXIT_FAILURE, 0,
	       _("cannot cache section addresses for module '%s': %s"),
	       dwfl_module_info (mod, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	       dwfl_errmsg (-1));
    }

  handle_file (output_file, create_dirs, stripped, &stripped_ehdr, debug);
}

/* Handle one module being written to the output directory.  */
static void
handle_output_dir_module (const char *output_dir, Dwfl_Module *mod, bool force,
			  bool all, bool ignore, bool modnames, bool relocate)
{
  if (! modnames)
    {
      
      GElf_Addr bias;
      (void) dwfl_module_getelf (mod, &bias);
    }

  const char *file;
  const char *name = dwfl_module_info (mod, NULL, NULL, NULL,
				       NULL, NULL, &file, NULL);

  if (file == NULL && ignore)
    return;

  char *output_file;
  if (asprintf (&output_file, "%s/%s", output_dir, modnames ? name : file) < 0)
    error (EXIT_FAILURE, 0, _("memory exhausted"));

  handle_dwfl_module (output_file, true, force, mod, all, ignore, relocate);
}


static void
list_module (Dwfl_Module *mod)
{
  
  GElf_Addr bias;
  bool have_elf = dwfl_module_getelf (mod, &bias) != NULL;
  bool have_dwarf = dwfl_module_getdwarf (mod, &bias) != NULL;

  const char *file;
  const char *debug;
  Dwarf_Addr start;
  Dwarf_Addr end;
  const char *name = dwfl_module_info (mod, NULL, &start, &end,
				       NULL, NULL, &file, &debug);
  if (file != NULL && debug != NULL && (debug == file || !strcmp (debug, file)))
    debug = ".";

  const unsigned char *id;
  GElf_Addr id_vaddr;
  int id_len = dwfl_module_build_id (mod, &id, &id_vaddr);

  printf ("%#" PRIx64 "+%#" PRIx64 " ", start, end - start);

  if (id_len > 0)
    {
      do
	printf ("%02" PRIx8, *id++);
      while (--id_len > 0);
      if (id_vaddr != 0)
	printf ("@%#" PRIx64, id_vaddr);
    }
  else
    putchar ('-');

  printf (" %s %s %s\n",
	  file ?: have_elf ? "." : "-",
	  debug ?: have_dwarf ? "." : "-",
	  name);
}


struct match_module_info
{
  char **patterns;
  Dwfl_Module *found;
  bool match_files;
};

static int
match_module (Dwfl_Module *mod,
	      void **userdata __attribute__ ((unused)),
	      const char *name,
	      Dwarf_Addr start __attribute__ ((unused)),
	      void *arg)
{
  struct match_module_info *info = arg;

  if (info->patterns[0] == NULL) 
    {
    match:
      info->found = mod;
      return DWARF_CB_ABORT;
    }

  if (info->match_files)
    {
      
      GElf_Addr bias;
      (void) dwfl_module_getelf (mod, &bias);

      const char *file;
      const char *check = dwfl_module_info (mod, NULL, NULL, NULL,
					    NULL, NULL, &file, NULL);
      assert (check == name);
      if (file == NULL)
	return DWARF_CB_OK;

      name = file;
    }

  for (char **p = info->patterns; *p != NULL; ++p)
    if (fnmatch (*p, name, 0) == 0)
      goto match;

  return DWARF_CB_OK;
}

static void
handle_implicit_modules (const struct arg_info *info)
{
  struct match_module_info mmi = { info->args, NULL, info->match_files };
  inline ptrdiff_t next (ptrdiff_t offset)
    {
      return dwfl_getmodules (info->dwfl, &match_module, &mmi, offset);
    }
  ptrdiff_t offset = next (0);
  if (offset == 0)
    error (EXIT_FAILURE, 0, _("no matching modules found"));

  if (info->list)
    do
      list_module (mmi.found);
    while ((offset = next (offset)) > 0);
  else if (info->output_dir == NULL)
    {
      if (next (offset) != 0)
	error (EXIT_FAILURE, 0, _("matched more than one module"));
      handle_dwfl_module (info->output_file, false, info->force, mmi.found,
			  info->all, info->ignore, info->relocate);
    }
  else
    do
      handle_output_dir_module (info->output_dir, mmi.found, info->force,
				info->all, info->ignore,
				info->modnames, info->relocate);
    while ((offset = next (offset)) > 0);
}

int
main (int argc, char **argv)
{
  
  mtrace ();

  
  __fsetlocking (stdin, FSETLOCKING_BYCALLER);
  __fsetlocking (stdout, FSETLOCKING_BYCALLER);
  __fsetlocking (stderr, FSETLOCKING_BYCALLER);

  
  setlocale (LC_ALL, "");

  
  bindtextdomain (PACKAGE_TARNAME, LOCALEDIR);

  
  textdomain (PACKAGE_TARNAME);

  
  const struct argp_child argp_children[] =
    {
      {
	.argp = dwfl_standard_argp (),
	.header = N_("Input selection options:"),
	.group = 1,
      },
      { .argp = NULL },
    };
  const struct argp argp =
    {
      .options = options,
      .parser = parse_opt,
      .children = argp_children,
      .args_doc = N_("STRIPPED-FILE DEBUG-FILE\n[MODULE...]"),
      .doc = N_("\
Combine stripped files with separate symbols and debug information.\v\
The first form puts the result in DEBUG-FILE if -o was not given.\n\
\n\
MODULE arguments give file name patterns matching modules to process.\n\
With -f these match the file name of the main (stripped) file \
(slashes are never special), otherwise they match the simple module names.  \
With no arguments, process all modules found.\n\
\n\
Multiple modules are written to files under OUTPUT-DIRECTORY, \
creating subdirectories as needed.  \
With -m these files have simple module names, otherwise they have the \
name of the main file complete with directory underneath OUTPUT-DIRECTORY.\n\
\n\
With -n no files are written, but one line to standard output for each module:\
\n\tSTART+SIZE BUILDID FILE DEBUGFILE MODULENAME\n\
START and SIZE are hexadecimal giving the address bounds of the module.  \
BUILDID is hexadecimal for the build ID bits, or - if no ID is known; \
the hexadecimal may be followed by @0xADDR giving the address where the \
ID resides if that is known.  \
FILE is the file name found for the module, or - if none was found, \
or . if an ELF image is available but not from any named file.  \
DEBUGFILE is the separate debuginfo file name, \
or - if no debuginfo was found, or . if FILE contains the debug information.\
")
    };

  int remaining;
  struct arg_info info = { .args = NULL };
  error_t result = argp_parse (&argp, argc, argv, 0, &remaining, &info);
  if (result == ENOSYS)
    assert (info.dwfl == NULL);
  else if (result)
    return EXIT_FAILURE;
  assert (info.args != NULL);

  
  elf_version (EV_CURRENT);

  if (info.dwfl == NULL)
    {
      assert (result == ENOSYS);

      if (info.output_dir != NULL)
	{
	  char *file;
	  if (asprintf (&file, "%s/%s", info.output_dir, info.args[0]) < 0)
	    error (EXIT_FAILURE, 0, _("memory exhausted"));
	  handle_explicit_files (file, true, info.force,
				 info.args[0], info.args[1]);
	  free (file);
	}
      else
	handle_explicit_files (info.output_file, false, info.force,
			       info.args[0], info.args[1]);
    }
  else
    {
      
      assert (info.output_file != NULL || info.output_dir != NULL || info.list);

      handle_implicit_modules (&info);

      dwfl_end (info.dwfl);
    }

  return 0;
}


#include "debugpred.h"
