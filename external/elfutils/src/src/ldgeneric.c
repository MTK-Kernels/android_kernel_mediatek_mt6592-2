/* Copyright (C) 2001-2011 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2001.

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

#include <assert.h>
#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <gelf.h>
#include <inttypes.h>
#include <libintl.h>
#include <stdbool.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <elf-knowledge.h>
#include "ld.h"
#include "list.h"
#include <md5.h>
#include <sha1.h>
#include <system.h>


struct unw_eh_frame_hdr
{
  unsigned char version;
  unsigned char eh_frame_ptr_enc;
  unsigned char fde_count_enc;
  unsigned char table_enc;
};
#define EH_FRAME_HDR_VERSION 1


static const char **ld_generic_lib_extensions (struct ld_state *)
     __attribute__ ((__const__));
static int ld_generic_file_close (struct usedfiles *fileinfo,
				  struct ld_state *statep);
static int ld_generic_file_process (int fd, struct usedfiles *fileinfo,
				    struct ld_state *statep,
				    struct usedfiles **nextp);
static void ld_generic_generate_sections (struct ld_state *statep);
static void ld_generic_create_sections (struct ld_state *statep);
static int ld_generic_flag_unresolved (struct ld_state *statep);
static int ld_generic_open_outfile (struct ld_state *statep, int machine,
				    int class, int data);
static int ld_generic_create_outfile (struct ld_state *statep);
static void ld_generic_relocate_section (struct ld_state *statep,
					 Elf_Scn *outscn,
					 struct scninfo *firstp,
					 const Elf32_Word *dblindirect);
static int ld_generic_finalize (struct ld_state *statep);
static bool ld_generic_special_section_number_p (struct ld_state *statep,
						 size_t number);
static bool ld_generic_section_type_p (struct ld_state *statep,
				       XElf_Word type);
static XElf_Xword ld_generic_dynamic_section_flags (struct ld_state *statep);
static void ld_generic_initialize_plt (struct ld_state *statep, Elf_Scn *scn);
static void ld_generic_initialize_pltrel (struct ld_state *statep,
					  Elf_Scn *scn);
static void ld_generic_initialize_got (struct ld_state *statep, Elf_Scn *scn);
static void ld_generic_initialize_gotplt (struct ld_state *statep,
					  Elf_Scn *scn);
static void ld_generic_finalize_plt (struct ld_state *statep, size_t nsym,
				     size_t nsym_dyn,
				     struct symbol **ndxtosymp);
static int ld_generic_rel_type (struct ld_state *statep);
static void ld_generic_count_relocations (struct ld_state *statep,
					  struct scninfo *scninfo);
static void ld_generic_create_relocations (struct ld_state *statep,
					   const Elf32_Word *dblindirect);

static int file_process2 (struct usedfiles *fileinfo);
static void mark_section_used (struct scninfo *scninfo, Elf32_Word shndx,
			       struct scninfo **grpscnp);


static struct symbol **ndxtosym;

static struct Ebl_Strent **symstrent;


static bool
is_dso_p (int fd)
{
  XElf_Half e_type;

  return (pread (fd, &e_type, sizeof (e_type), offsetof (XElf_Ehdr, e_type))
	  == sizeof (e_type)
	  && e_type == ET_DYN);
}


static int
print_file_name (FILE *s, struct usedfiles *fileinfo, int first_level,
		 int newline)
{
  int npar = 0;

  if (fileinfo->archive_file != NULL)
    {
      npar = print_file_name (s, fileinfo->archive_file, 0, 0) + 1;
      fputc_unlocked ('(', s);
      fputs_unlocked (fileinfo->rfname, s);

      if (first_level)
	while (npar-- > 0)
	  fputc_unlocked (')', s);
    }
  else
    fputs_unlocked (fileinfo->rfname, s);

  if (first_level && newline)
    fputc_unlocked ('\n', s);

  return npar;
}


bool
dynamically_linked_p (void)
{
  return (ld_state.file_type == dso_file_type || ld_state.nplt > 0
	  || ld_state.ngot > 0);
}


bool
linked_from_dso_p (struct scninfo *scninfo, size_t symidx)
{
  struct usedfiles *file = scninfo->fileinfo;

  if (symidx < file->nlocalsymbols)
    return false;

  struct symbol *sym = file->symref[symidx];

  return sym->defined && sym->in_dso;
}


int
ld_prepare_state (const char *emulation)
{
  
  ld_state.nodefs = true;

  ld_state.add_ld_comment = true;

  ld_state.nextveridx = 2;

  
  ld_symbol_tab_init (&ld_state.symbol_tab, 1027);
  ld_section_tab_init (&ld_state.section_tab, 67);
  ld_version_str_tab_init (&ld_state.version_str_tab, 67);

  
  ld_state.shstrtab = ebl_strtabinit (true);
  if (ld_state.shstrtab == NULL)
    error (EXIT_FAILURE, errno, gettext ("cannot create string table"));

  ld_state.callbacks.lib_extensions = ld_generic_lib_extensions;
  ld_state.callbacks.file_process = ld_generic_file_process;
  ld_state.callbacks.file_close = ld_generic_file_close;
  ld_state.callbacks.generate_sections = ld_generic_generate_sections;
  ld_state.callbacks.create_sections = ld_generic_create_sections;
  ld_state.callbacks.flag_unresolved = ld_generic_flag_unresolved;
  ld_state.callbacks.open_outfile = ld_generic_open_outfile;
  ld_state.callbacks.create_outfile = ld_generic_create_outfile;
  ld_state.callbacks.relocate_section = ld_generic_relocate_section;
  ld_state.callbacks.finalize = ld_generic_finalize;
  ld_state.callbacks.special_section_number_p =
    ld_generic_special_section_number_p;
  ld_state.callbacks.section_type_p = ld_generic_section_type_p;
  ld_state.callbacks.dynamic_section_flags = ld_generic_dynamic_section_flags;
  ld_state.callbacks.initialize_plt = ld_generic_initialize_plt;
  ld_state.callbacks.initialize_pltrel = ld_generic_initialize_pltrel;
  ld_state.callbacks.initialize_got = ld_generic_initialize_got;
  ld_state.callbacks.initialize_gotplt = ld_generic_initialize_gotplt;
  ld_state.callbacks.finalize_plt = ld_generic_finalize_plt;
  ld_state.callbacks.rel_type = ld_generic_rel_type;
  ld_state.callbacks.count_relocations = ld_generic_count_relocations;
  ld_state.callbacks.create_relocations = ld_generic_create_relocations;

#ifndef BASE_ELF_NAME
  if (emulation == NULL)
    {
      emulation = ebl_backend_name (ld_state.ebl);
      assert (emulation != NULL);
    }
  size_t emulation_len = strlen (emulation);

  
  char *fname = (char *) alloca (sizeof "libld_" - 1 + emulation_len
				 + sizeof ".so");
  strcpy (mempcpy (stpcpy (fname, "libld_"), emulation, emulation_len), ".so");

  
  void *h = dlopen (fname, RTLD_LAZY);
  if (h == NULL)
    error (EXIT_FAILURE, 0,
	   gettext ("cannot load ld backend library '%s': %s"),
	   fname, dlerror ());

  
  char *initname = (char *) alloca (emulation_len + sizeof "_ld_init");
  strcpy (mempcpy (initname, emulation, emulation_len), "_ld_init");
  int (*initfct) (struct ld_state *)
    = (int (*) (struct ld_state *)) dlsym (h, initname);

  if (initfct == NULL)
    error (EXIT_FAILURE, 0, gettext ("\
cannot find init function in ld backend library '%s': %s"),
	   fname, dlerror ());

  
  ld_state.ldlib = h;

  
  return initfct (&ld_state);
#else
# define INIT_FCT_NAME(base) _INIT_FCT_NAME(base)
# define _INIT_FCT_NAME(base) base##_ld_init
  
  extern int INIT_FCT_NAME(BASE_ELF_NAME) (struct ld_state *);
  return INIT_FCT_NAME(BASE_ELF_NAME) (&ld_state);
#endif
}


static int
check_for_duplicate2 (struct usedfiles *newp, struct usedfiles *list)
{
  struct usedfiles *first;

  if (list == NULL)
    return 0;

  list = first = list->next;
  do
    {
      if (likely (list->status == not_opened))
	break;

      if (unlikely (list->ino == newp->ino)
	  && unlikely (list->dev == newp->dev))
	{
	  close (newp->fd);
	  newp->fd = -1;
	  newp->status = closed;
	  if (newp->file_type == relocatable_file_type)
	    error (0, 0, gettext ("%s listed more than once as input"),
		   newp->rfname);

	  return 1;
	}
      list = list->next;
    }
  while (likely (list != first));

  return 0;
}


static int
check_for_duplicate (struct usedfiles *newp)
{
  struct stat st;

  if (unlikely (fstat (newp->fd, &st) < 0))
    {
      close (newp->fd);
      return errno;
    }

  newp->dev = st.st_dev;
  newp->ino = st.st_ino;

  return (check_for_duplicate2 (newp, ld_state.relfiles)
	  || check_for_duplicate2 (newp, ld_state.dsofiles)
	  || check_for_duplicate2 (newp, ld_state.needed));
}


static int
open_along_path2 (struct usedfiles *fileinfo, struct pathelement *path)
{
  const char *fname = fileinfo->fname;
  size_t fnamelen = strlen (fname);
  int err = ENOENT;
  struct pathelement *firstp = path;

  if (path == NULL)
    
    return ENOENT;

  do
    {
      if (likely (path->exist >= 0))
	{
	  
	  char *rfname = NULL;
	  size_t dirlen = strlen (path->pname);
	  int fd = -1;

	  if (fileinfo->file_type == archive_file_type)
	    {
	      const char **exts = (ld_state.statically
				   ? (const char *[2]) { ".a", NULL }
				   : LIB_EXTENSION (&ld_state));

	      while (*exts != NULL)
		{
		  size_t extlen = strlen (*exts);
		  rfname = (char *) alloca (dirlen + 5 + fnamelen + extlen);
		  memcpy (mempcpy (stpcpy (mempcpy (rfname, path->pname,
						    dirlen),
					   "/lib"),
				   fname, fnamelen),
			  *exts, extlen + 1);

		  fd = open (rfname, O_RDONLY);
		  if (likely (fd != -1) || errno != ENOENT)
		    {
		      err = fd == -1 ? errno : 0;
		      break;
		    }

		  
		  ++exts;
		}
	    }
	  else
	    {
	      assert (fileinfo->file_type == dso_file_type
		      || fileinfo->file_type == dso_needed_file_type);

	      rfname = (char *) alloca (dirlen + 1 + fnamelen + 1);
	      memcpy (stpcpy (mempcpy (rfname, path->pname, dirlen), "/"),
		      fname, fnamelen + 1);

	      fd = open (rfname, O_RDONLY);
	      if (unlikely (fd == -1))
		err = errno;
	    }

	  if (likely (fd != -1))
	    {
	      fileinfo->fd = fd;
	      path->exist = 1;

	      
	      if (unlikely (check_for_duplicate (fileinfo) != 0))
		return EAGAIN;

	      
	      fileinfo->rfname = obstack_strdup (&ld_state.smem, rfname);

	      if (unlikely (ld_state.trace_files))
		printf (fileinfo->file_type == archive_file_type
			? gettext ("%s (for -l%s)\n")
			: gettext ("%s (for DT_NEEDED %s)\n"),
			rfname, fname);

	      return 0;
	    }

	  if (unlikely (path->exist == 0))
	    {
	      struct stat st;

	      rfname[dirlen] = '\0';
	      if (unlikely (stat (rfname, &st) < 0) || ! S_ISDIR (st.st_mode))
		path->exist = -1;
	      else
		path->exist = 1;
	    }
	}

      
      path = path->next;
    }
  while (likely (err == ENOENT && path != firstp));

  return err;
}


static int
open_along_path (struct usedfiles *fileinfo)
{
  const char *fname = fileinfo->fname;
  int err = ENOENT;

  if (fileinfo->file_type == relocatable_file_type)
    {
      
      fileinfo->fd = open (fname, O_RDONLY);

      if (likely (fileinfo->fd != -1))
	{
	  
	  if (unlikely (ld_state.trace_files))
	    print_file_name (stdout, fileinfo, 1, 1);

	  return check_for_duplicate (fileinfo);
	}

      
      err = errno;
    }
  else
    {
      err = open_along_path2 (fileinfo, ld_state.ld_library_path1);

      
      if (err == ENOENT)
	err = open_along_path2 (fileinfo,
				fileinfo->file_type == archive_file_type
				? ld_state.paths : ld_state.rpath_link);

      
      if (unlikely (err == ENOENT))
	{
	  err = open_along_path2 (fileinfo, ld_state.ld_library_path2);

	  
	  if (err == ENOENT)
	    {
	      if (fileinfo->file_type == dso_file_type)
		err = open_along_path2 (fileinfo, ld_state.runpath_link);

	      
	      if (err == ENOENT)
		err = open_along_path2 (fileinfo, ld_state.default_paths);
	    }
	}
    }

  if (unlikely (err != 0)
      && (err != EAGAIN || fileinfo->file_type == relocatable_file_type))
    error (0, err, gettext ("cannot open %s"), fileinfo->fname);

  return err;
}


static int
matching_group_comdat_scn (const XElf_Sym *sym, size_t shndx,
			   struct usedfiles *fileinfo, struct symbol *oldp)
{
  if ((shndx >= SHN_LORESERVE && shndx <= SHN_HIRESERVE)
      || (oldp->scndx >= SHN_LORESERVE && oldp->scndx <= SHN_HIRESERVE))
    
    return 0;

  size_t newgrpid = fileinfo->scninfo[shndx].grpid;
  size_t oldgrpid = oldp->file->scninfo[oldp->scndx].grpid;
  if (newgrpid == 0 || oldgrpid == 0)
    return 0;

  assert (SCNINFO_SHDR (fileinfo->scninfo[newgrpid].shdr).sh_type
	  == SHT_GROUP);
  assert (SCNINFO_SHDR (oldp->file->scninfo[oldgrpid].shdr).sh_type
	  == SHT_GROUP);

  if (! fileinfo->scninfo[newgrpid].comdat_group
      || ! oldp->file->scninfo[oldgrpid].comdat_group)
    return 0;

  if (strcmp (fileinfo->scninfo[newgrpid].symbols->name,
	      oldp->file->scninfo[oldgrpid].symbols->name) != 0)
    return 0;

  
  return 1;
}


static void
check_type_and_size (const XElf_Sym *sym, struct usedfiles *fileinfo,
		     struct symbol *oldp)
{

  if (XELF_ST_TYPE (sym->st_info) != STT_NOTYPE && oldp->type != STT_NOTYPE
      && unlikely (oldp->type != XELF_ST_TYPE (sym->st_info)))
    {
      char buf1[64];
      char buf2[64];

      error (0, 0, gettext ("\
Warning: type of `%s' changed from %s in %s to %s in %s"),
	     oldp->name,
	     ebl_symbol_type_name (ld_state.ebl, oldp->type,
				   buf1, sizeof (buf1)),
	     oldp->file->rfname,
	     ebl_symbol_type_name (ld_state.ebl, XELF_ST_TYPE (sym->st_info),
				   buf2, sizeof (buf2)),
	     fileinfo->rfname);
    }
  else if (XELF_ST_TYPE (sym->st_info) == STT_OBJECT
	   && oldp->size != 0
	   && unlikely (oldp->size != sym->st_size))
    error (0, 0, gettext ("\
Warning: size of `%s' changed from %" PRIu64 " in %s to %" PRIu64 " in %s"),
	   oldp->name, (uint64_t) oldp->size, oldp->file->rfname,
	   (uint64_t) sym->st_size, fileinfo->rfname);
}


static int
check_definition (const XElf_Sym *sym, size_t shndx, size_t symidx,
		  struct usedfiles *fileinfo, struct symbol *oldp)
{
  int result = 0;
  bool old_in_dso = FILEINFO_EHDR (oldp->file->ehdr).e_type == ET_DYN;
  bool new_in_dso = FILEINFO_EHDR (fileinfo->ehdr).e_type == ET_DYN;
  bool use_new_def = false;

  if (shndx != SHN_UNDEF
      && (! oldp->defined
	  || (shndx != SHN_COMMON && oldp->common && ! new_in_dso)
	  || (old_in_dso && ! new_in_dso)))
    {
      check_type_and_size (sym, fileinfo, oldp);

      if (! oldp->defined)
	{
	  
	  --ld_state.nunresolved;
	  if (! oldp->weak)
	    --ld_state.nunresolved_nonweak;
	  CDBL_LIST_DEL (ld_state.unresolved, oldp);
	}
      else if (oldp->common)
	
	CDBL_LIST_DEL (ld_state.common_syms, oldp);

      
      use_new_def = true;
    }
  else if (shndx != SHN_UNDEF
	   && oldp->defined
	   && matching_group_comdat_scn (sym, shndx, fileinfo, oldp))
    ;
  else if (shndx != SHN_UNDEF
	   && unlikely (! oldp->common)
	   && oldp->defined
	   && shndx != SHN_COMMON
	   && (!ld_state.muldefs || verbose)
	   && ! old_in_dso && fileinfo->file_type == relocatable_file_type)
    {
      
      char buf[64];
      XElf_Sym_vardef (oldsym);
      struct usedfiles *oldfile;
      const char *scnname;
      Elf32_Word xndx;
      size_t shnum;

      if (elf_getshdrnum (fileinfo->elf, &shnum) < 0)
	error (EXIT_FAILURE, 0,
	       gettext ("cannot determine number of sections: %s"),
	       elf_errmsg (-1));

      
      if (shndx < SHN_LORESERVE || (shndx > SHN_HIRESERVE && shndx < shnum))
	scnname = elf_strptr (fileinfo->elf,
			      fileinfo->shstrndx,
			      SCNINFO_SHDR (fileinfo->scninfo[shndx].shdr).sh_name);
      else
	
	scnname = ebl_section_name (ld_state.ebl, shndx, 0, buf, sizeof (buf),
				    NULL, shnum);

      
      print_file_name (stderr, fileinfo, 1, 0);
      fprintf (stderr,
	       gettext ("(%s+%#" PRIx64 "): multiple definition of %s `%s'\n"),
	       scnname,
	       (uint64_t) sym->st_value,
	       ebl_symbol_type_name (ld_state.ebl, XELF_ST_TYPE (sym->st_info),
				     buf, sizeof (buf)),
	       oldp->name);

      oldfile = oldp->file;
      xelf_getsymshndx (oldfile->symtabdata, oldfile->xndxdata, oldp->symidx,
			oldsym, xndx);
      assert (oldsym != NULL);

      
      if (oldp->scndx < SHN_LORESERVE || oldp->scndx > SHN_HIRESERVE)
	scnname = elf_strptr (oldfile->elf,
			      oldfile->shstrndx,
			      SCNINFO_SHDR (oldfile->scninfo[shndx].shdr).sh_name);
      else
	scnname = ebl_section_name (ld_state.ebl, oldp->scndx, oldp->scndx,
				    buf, sizeof (buf), NULL, shnum);

      
      print_file_name (stderr, oldfile, 1, 0);
      fprintf (stderr, gettext ("(%s+%#" PRIx64 "): first defined here\n"),
	       scnname, (uint64_t) oldsym->st_value);

      if (likely (!ld_state.muldefs))
	result = 1;
    }
  else if (old_in_dso && fileinfo->file_type == relocatable_file_type
	   && shndx != SHN_UNDEF)
    use_new_def = true;
  else if (old_in_dso && !new_in_dso && oldp->defined && !oldp->on_dsolist)
    {
      CDBL_LIST_ADD_REAR (ld_state.from_dso, oldp);
      ++ld_state.nfrom_dso;

      if (oldp->type == STT_FUNC)
	++ld_state.nplt;
      else
	++ld_state.ngot;

      oldp->on_dsolist = 1;
    }
  else if (oldp->common && shndx == SHN_COMMON)
    {
      
      oldp->size = MAX (oldp->size, sym->st_size);
      
      oldp->merge.value = MAX (oldp->merge.value, sym->st_value);
    }

  if (unlikely (use_new_def))
    {
      if (old_in_dso && fileinfo->file_type == relocatable_file_type)
	{
	  CDBL_LIST_DEL (ld_state.from_dso, oldp);
	  --ld_state.nfrom_dso;

	  if (likely (oldp->type == STT_FUNC))
	    --ld_state.nplt;
	  else
	    --ld_state.ngot;

	  oldp->on_dsolist = 0;
	}

      
      oldp->size = sym->st_size;
      oldp->type = XELF_ST_TYPE (sym->st_info);
      oldp->symidx = symidx;
      oldp->scndx = shndx;
      
      oldp->file = fileinfo;
      oldp->defined = 1;
      oldp->in_dso = new_in_dso;
      oldp->common = shndx == SHN_COMMON;
      if (likely (fileinfo->file_type == relocatable_file_type))
	{
	  oldp->weak = XELF_ST_BIND (sym->st_info) == STB_WEAK;

	  
	  if (shndx != SHN_COMMON && shndx != SHN_ABS)
	    {
	      struct scninfo *ignore;
	      mark_section_used (&fileinfo->scninfo[shndx], shndx, &ignore);
	    }
	}

      
      if (new_in_dso && !old_in_dso)
	{
	  CDBL_LIST_ADD_REAR (ld_state.from_dso, oldp);
	  ++ld_state.nfrom_dso;

	  if (oldp->type == STT_FUNC)
	    ++ld_state.nplt;
	  else
	    ++ld_state.ngot;

	  oldp->on_dsolist = 1;
	}
      else if (shndx == SHN_COMMON)
	{
	  
	  oldp->merge.value = sym->st_value;

	  CDBL_LIST_ADD_REAR (ld_state.common_syms, oldp);
	}
    }

  return result;
}


static struct scninfo *
find_section_group (struct usedfiles *fileinfo, Elf32_Word shndx,
		    Elf_Data **datap)
{
  struct scninfo *runp;

  for (runp = fileinfo->groups; runp != NULL; runp = runp->next)
    if (!runp->used)
      {
	Elf32_Word *grpref;
	size_t cnt;
	Elf_Data *data;

	data = elf_getdata (runp->scn, NULL);
	if (data == NULL)
	  error (EXIT_FAILURE, 0,
		 gettext ("%s: cannot get section group data: %s"),
		 fileinfo->fname, elf_errmsg (-1));

	
	assert (elf_getdata (runp->scn, data) == NULL);

	grpref = (Elf32_Word *) data->d_buf;
	cnt = data->d_size / sizeof (Elf32_Word);
	while (cnt > 1)
	  if (grpref[--cnt] == shndx)
	    {
	      *datap = data;
	      return runp;
	    }
      }

  error (EXIT_FAILURE, 0, gettext ("\
%s: section '%s' with group flag set does not belong to any group"),
	 fileinfo->fname,
	 elf_strptr (fileinfo->elf, fileinfo->shstrndx,
		     SCNINFO_SHDR (fileinfo->scninfo[shndx].shdr).sh_name));
  return NULL;
}


static void
mark_section_group (struct usedfiles *fileinfo, Elf32_Word shndx,
		    struct scninfo **grpscnp)
{
  size_t cnt;
  Elf32_Word *grpref;
  Elf_Data *data;
  struct scninfo *grpscn = find_section_group (fileinfo, shndx, &data);
  *grpscnp = grpscn;


  
  grpscn->used = true;

  grpref = (Elf32_Word *) data->d_buf;
  cnt = data->d_size / sizeof (Elf32_Word);
  while (cnt > 1)
    {
      Elf32_Word idx = grpref[--cnt];
      XElf_Shdr *shdr = &SCNINFO_SHDR (fileinfo->scninfo[idx].shdr);

      if (fileinfo->scninfo[idx].grpid != grpscn->grpid)
	error (EXIT_FAILURE, 0, gettext ("\
%s: section [%2d] '%s' is not in the correct section group"),
	       fileinfo->fname, (int) idx,
	       elf_strptr (fileinfo->elf, fileinfo->shstrndx, shdr->sh_name));

      if (ld_state.strip == strip_none
	  
	  || (!ebl_debugscn_p (ld_state.ebl,
			       elf_strptr (fileinfo->elf, fileinfo->shstrndx,
					   shdr->sh_name))
	      
	      && ((shdr->sh_type != SHT_RELA && shdr->sh_type != SHT_REL)
		  || !ebl_debugscn_p (ld_state.ebl,
				      elf_strptr (fileinfo->elf,
						  fileinfo->shstrndx,
						  SCNINFO_SHDR (fileinfo->scninfo[shdr->sh_info].shdr).sh_name)))))
	{
	  struct scninfo *ignore;

	  mark_section_used (&fileinfo->scninfo[idx], idx, &ignore);
	}
    }
}


static void
mark_section_used (struct scninfo *scninfo, Elf32_Word shndx,
		   struct scninfo **grpscnp)
{
  if (likely (scninfo->used))
    
    return;

  
  scninfo->used = true;

  
  XElf_Shdr *shdr = &SCNINFO_SHDR (scninfo->shdr);
#if NATIVE_ELF
  if (unlikely (scninfo->shdr == NULL))
#else
  if (unlikely (scninfo->shdr.sh_type == SHT_NULL))
#endif
    {
#if NATIVE_ELF != 0
      shdr = xelf_getshdr (scninfo->scn, scninfo->shdr);
#else
      xelf_getshdr_copy (scninfo->scn, shdr, scninfo->shdr);
#endif
      if (unlikely (shdr == NULL))
	return;
    }

  
  if (unlikely (shdr->sh_link != 0))
    {
      struct scninfo *ignore;
      mark_section_used (&scninfo->fileinfo->scninfo[shdr->sh_link],
			 shdr->sh_link, &ignore);
    }

  
  if (unlikely (shdr->sh_info != 0) && (shdr->sh_flags & SHF_INFO_LINK))
    {
      struct scninfo *ignore;
      mark_section_used (&scninfo->fileinfo->scninfo[shdr->sh_info],
			 shdr->sh_info, &ignore);
    }

  if (unlikely (shdr->sh_flags & SHF_GROUP) && ld_state.gc_sections)
    
    mark_section_group (scninfo->fileinfo, shndx, grpscnp);
}


static void
add_section (struct usedfiles *fileinfo, struct scninfo *scninfo)
{
  struct scnhead *queued;
  struct scnhead search;
  unsigned long int hval;
  XElf_Shdr *shdr = &SCNINFO_SHDR (scninfo->shdr);
  struct scninfo *grpscn = NULL;
  Elf_Data *grpscndata = NULL;

  if (!scninfo->used
      && (ld_state.strip == strip_none
	  || (shdr->sh_flags & SHF_ALLOC) != 0
	  || shdr->sh_type == SHT_NOTE
	  || (shdr->sh_type == SHT_PROGBITS
	      && strcmp (elf_strptr (fileinfo->elf,
				     fileinfo->shstrndx,
				     shdr->sh_name), ".comment") == 0))
      && (fileinfo->status != in_archive || !ld_state.gc_sections))
    
    mark_section_used (scninfo, elf_ndxscn (scninfo->scn), &grpscn);

  if ((shdr->sh_flags & SHF_GROUP) && grpscn == NULL)
    grpscn = find_section_group (fileinfo, elf_ndxscn (scninfo->scn),
				 &grpscndata);
  assert (grpscn == NULL || grpscn->symbols->name != NULL);

  
  search.name = elf_strptr (fileinfo->elf, fileinfo->shstrndx, shdr->sh_name);
  search.type = shdr->sh_type;
  search.flags = shdr->sh_flags;
  search.entsize = shdr->sh_entsize;
  search.grp_signature = grpscn != NULL ? grpscn->symbols->name : NULL;
  search.kind = scn_normal;
  hval = elf_hash (search.name);

  
  queued = ld_section_tab_find (&ld_state.section_tab, hval, &search);
  if (queued != NULL)
    {
      bool is_comdat = false;

      if (unlikely (shdr->sh_flags & SHF_GROUP))
	{
	  
	  if (grpscndata == NULL)
	    {
	      grpscndata = elf_getdata (grpscn->scn, NULL);
	      assert (grpscndata != NULL);
	    }

	  
	  if ((((Elf32_Word *) grpscndata->d_buf)[0] & GRP_COMDAT) != 0)
	    {
	      struct scninfo *runp = queued->last;
	      do
		{
		  if (SCNINFO_SHDR (runp->shdr).sh_flags & SHF_GROUP)
		    {
		      struct scninfo *grpscn2
			= find_section_group (runp->fileinfo,
					      elf_ndxscn (runp->scn),
					      &grpscndata);

		      if (strcmp (grpscn->symbols->name,
				  grpscn2->symbols->name) == 0)
			{
			  scninfo->unused_comdat = is_comdat = true;
			  break;
			}
		    }

		  runp = runp->next;
		}
	      while (runp != queued->last);
	    }
	}

      if (!is_comdat)
	{
	  
	  scninfo->next = queued->last->next;
	  queued->last = queued->last->next = scninfo;

	  queued->flags = ebl_sh_flags_combine (ld_state.ebl, queued->flags,
						shdr->sh_flags);
	  queued->align = MAX (queued->align, shdr->sh_addralign);
	}
    }
  else
    {
      queued = (struct scnhead *) xcalloc (sizeof (struct scnhead), 1);
      queued->kind = scn_normal;
      queued->name = search.name;
      queued->type = shdr->sh_type;
      queued->flags = shdr->sh_flags;
      queued->align = shdr->sh_addralign;
      queued->entsize = shdr->sh_entsize;
      queued->grp_signature = grpscn != NULL ? grpscn->symbols->name : NULL;
      queued->segment_nr = ~0;
      queued->last = scninfo->next = scninfo;

      
      ld_state.need_tls |= (shdr->sh_flags & SHF_TLS) != 0;

      
      ld_section_tab_insert (&ld_state.section_tab, hval, queued);
    }
}


static int
add_relocatable_file (struct usedfiles *fileinfo, GElf_Word secttype)
{
  size_t scncnt;
  size_t cnt;
  Elf_Data *symtabdata = NULL;
  Elf_Data *xndxdata = NULL;
  Elf_Data *versymdata = NULL;
  Elf_Data *verdefdata = NULL;
  Elf_Data *verneeddata = NULL;
  size_t symstridx = 0;
  size_t nsymbols = 0;
  size_t nlocalsymbols = 0;
  bool has_merge_sections = false;
  bool has_tls_symbols = false;
  enum execstack execstack = execstack_true;

  
  assert (fileinfo->elf != NULL);

  
  if (unlikely (elf_getshdrnum (fileinfo->elf, &scncnt) < 0))
    error (EXIT_FAILURE, 0,
	   gettext ("cannot determine number of sections: %s"),
	   elf_errmsg (-1));

  fileinfo->scninfo = (struct scninfo *)
    obstack_calloc (&ld_state.smem, scncnt * sizeof (struct scninfo));

  for (cnt = 0; cnt < scncnt; ++cnt)
    {
      
      fileinfo->scninfo[cnt].scn = elf_getscn (fileinfo->elf, cnt);

      
      XElf_Shdr *shdr;
#if NATIVE_ELF != 0
      if (fileinfo->scninfo[cnt].shdr == NULL)
#else
      if (fileinfo->scninfo[cnt].shdr.sh_type == SHT_NULL)
#endif
	{
#if NATIVE_ELF != 0
	  shdr = xelf_getshdr (fileinfo->scninfo[cnt].scn,
			       fileinfo->scninfo[cnt].shdr);
#else
	  xelf_getshdr_copy (fileinfo->scninfo[cnt].scn, shdr,
			     fileinfo->scninfo[cnt].shdr);
#endif
	  if (shdr == NULL)
	    {
	      
	      fprintf (stderr, gettext ("%s: invalid ELF file (%s:%d)\n"),
		       fileinfo->rfname, __FILE__, __LINE__);
	      return 1;
	    }
	}
      else
	shdr = &SCNINFO_SHDR (fileinfo->scninfo[cnt].shdr);

      Elf_Data *data = elf_getdata (fileinfo->scninfo[cnt].scn, NULL);

      
      has_merge_sections |= (shdr->sh_flags & SHF_MERGE) != 0;
      has_tls_symbols |= (shdr->sh_flags & SHF_TLS) != 0;

      
      
      fileinfo->scninfo[cnt].fileinfo = fileinfo;

      if (unlikely (shdr->sh_type == SHT_SYMTAB)
	  || unlikely (shdr->sh_type == SHT_DYNSYM))
	{
	  if (shdr->sh_type == SHT_SYMTAB)
	    {
	      assert (fileinfo->symtabdata == NULL);
	      fileinfo->symtabdata = data;
	      fileinfo->nsymtab = shdr->sh_size / shdr->sh_entsize;
	      fileinfo->nlocalsymbols = shdr->sh_info;
	      fileinfo->symstridx = shdr->sh_link;
	    }
	  else
	    {
	      assert (fileinfo->dynsymtabdata == NULL);
	      fileinfo->dynsymtabdata = data;
	      fileinfo->ndynsymtab = shdr->sh_size / shdr->sh_entsize;
	      fileinfo->dynsymstridx = shdr->sh_link;
	    }

	  if (secttype == shdr->sh_type)
	    {
	      assert (symtabdata == NULL);
	      symtabdata = data;
	      symstridx = shdr->sh_link;
	      nsymbols = shdr->sh_size / shdr->sh_entsize;
	      nlocalsymbols = shdr->sh_info;
	    }
	}
      else if (unlikely (shdr->sh_type == SHT_SYMTAB_SHNDX))
	{
	  assert (xndxdata == NULL);
	  fileinfo->xndxdata = xndxdata = data;
	}
      else if (unlikely (shdr->sh_type == SHT_GNU_versym))
	{
	  assert (versymdata == 0);
	  fileinfo->versymdata = versymdata = data;
	}
      else if (unlikely (shdr->sh_type == SHT_GNU_verdef))
	{
	  size_t nversions;

	  assert (verdefdata == 0);
	  fileinfo->verdefdata = verdefdata = data;

	  fileinfo->nverdef = nversions = shdr->sh_info;
	  fileinfo->verdefused = (XElf_Versym *)
	    obstack_calloc (&ld_state.smem,
			    sizeof (XElf_Versym) * (nversions + 1));
	  fileinfo->verdefent = (struct Ebl_Strent **)
	    obstack_alloc (&ld_state.smem,
			   sizeof (struct Ebl_Strent *) * (nversions + 1));
	}
      else if (unlikely (shdr->sh_type == SHT_GNU_verneed))
	{
	  assert (verneeddata == 0);
	  fileinfo->verneeddata = verneeddata = data;
	}
      else if (unlikely (shdr->sh_type == SHT_DYNAMIC))
	{
	  assert (fileinfo->dynscn == NULL);
	  fileinfo->dynscn = fileinfo->scninfo[cnt].scn;
	}
      else if (unlikely (shdr->sh_type == SHT_GROUP))
	{
	  Elf_Scn *symscn;
	  XElf_Shdr_vardef (symshdr);
	  Elf_Data *symdata;

	  if (FILEINFO_EHDR (fileinfo->ehdr).e_type != ET_REL)
	    error (EXIT_FAILURE, 0, gettext ("\
%s: only files of type ET_REL might contain section groups"),
		   fileinfo->fname);

	  fileinfo->scninfo[cnt].next = fileinfo->groups;
	  fileinfo->scninfo[cnt].grpid = cnt;
	  fileinfo->groups = &fileinfo->scninfo[cnt];

	  fileinfo->scninfo[cnt].symbols = (struct symbol *)
	    obstack_calloc (&ld_state.smem, sizeof (struct symbol));

	  symscn = elf_getscn (fileinfo->elf, shdr->sh_link);
	  xelf_getshdr (symscn, symshdr);
	  symdata = elf_getdata (symscn, NULL);

	  if (symshdr != NULL)
	    {
	      XElf_Sym_vardef (sym);

	      xelf_getsym (symdata, shdr->sh_info, sym);
	      if (sym != NULL)
		{
		  struct symbol *symbol = fileinfo->scninfo[cnt].symbols;

#ifndef NO_HACKS
		  if (XELF_ST_TYPE (sym->st_info) == STT_SECTION)
		    {
		      XElf_Shdr_vardef (buggyshdr);
		      xelf_getshdr (elf_getscn (fileinfo->elf, sym->st_shndx),
				    buggyshdr);

		      symbol->name = elf_strptr (fileinfo->elf,
						 FILEINFO_EHDR (fileinfo->ehdr).e_shstrndx,
						 buggyshdr->sh_name);
		      symbol->symidx = -1;
		    }
		  else
#endif
		    {
		      symbol->name = elf_strptr (fileinfo->elf,
						 symshdr->sh_link,
						 sym->st_name);
		      symbol->symidx = shdr->sh_info;
		    }
		  symbol->file = fileinfo;
		}
	    }
	  if (fileinfo->scninfo[cnt].symbols->name == NULL)
	    error (EXIT_FAILURE, 0, gettext ("\
%s: cannot determine signature of section group [%2zd] '%s': %s"),
		   fileinfo->fname,
		   elf_ndxscn (fileinfo->scninfo[cnt].scn),
		   elf_strptr (fileinfo->elf, fileinfo->shstrndx,
			       shdr->sh_name),
		   elf_errmsg (-1));


	  if (data == NULL)
	    error (EXIT_FAILURE, 0, gettext ("\
%s: cannot get content of section group [%2zd] '%s': %s'"),
		   fileinfo->fname, elf_ndxscn (fileinfo->scninfo[cnt].scn),
		   elf_strptr (fileinfo->elf, fileinfo->shstrndx,
			       shdr->sh_name),
		   elf_errmsg (-1));

	  Elf32_Word *grpdata = (Elf32_Word *) data->d_buf;
	  if (grpdata[0] & GRP_COMDAT)
	    fileinfo->scninfo[cnt].comdat_group = true;
	  for (size_t inner = 1; inner < data->d_size / sizeof (Elf32_Word);
	       ++inner)
	    {
	      if (grpdata[inner] >= scncnt)
		error (EXIT_FAILURE, 0, gettext ("\
%s: group member %zu of section group [%2zd] '%s' has too high index: %" PRIu32),
		       fileinfo->fname,
		       inner, elf_ndxscn (fileinfo->scninfo[cnt].scn),
		       elf_strptr (fileinfo->elf, fileinfo->shstrndx,
				   shdr->sh_name),
		       grpdata[inner]);

	      fileinfo->scninfo[grpdata[inner]].grpid = cnt;
	    }

	  assert (fileinfo->scninfo[cnt].used == false);
	}
      else if (! SECTION_TYPE_P (&ld_state, shdr->sh_type)
	       && unlikely ((shdr->sh_flags & SHF_OS_NONCONFORMING) != 0))
	error (EXIT_FAILURE, 0,
	       gettext ("%s: section '%s' has unknown type: %d"),
	       fileinfo->fname,
	       elf_strptr (fileinfo->elf, fileinfo->shstrndx,
			   shdr->sh_name),
	       (int) shdr->sh_type);
      else if (likely (fileinfo->file_type == relocatable_file_type)
	       && likely (cnt > 0)
	       && likely (shdr->sh_type == SHT_PROGBITS
			  || shdr->sh_type == SHT_RELA
			  || shdr->sh_type == SHT_REL
			  || shdr->sh_type == SHT_NOTE
			  || shdr->sh_type == SHT_NOBITS
			  || shdr->sh_type == SHT_INIT_ARRAY
			  || shdr->sh_type == SHT_FINI_ARRAY
			  || shdr->sh_type == SHT_PREINIT_ARRAY))
	{
	  
	  if (shdr->sh_type == SHT_PROGBITS
	      && (shdr->sh_flags & SHF_EXECINSTR) == 0
	      && strcmp (elf_strptr (fileinfo->elf, fileinfo->shstrndx,
				     shdr->sh_name),
			 ".note.GNU-stack") == 0)
	    execstack = execstack_false;

	  add_section (fileinfo, &fileinfo->scninfo[cnt]);
	}
    }

  if (fileinfo->file_type == relocatable_file_type
      && execstack == execstack_true
      && ld_state.execstack != execstack_false_force)
    ld_state.execstack = execstack_true;

  if (likely (symtabdata != NULL))
    {
      fileinfo->has_merge_sections = has_merge_sections;
      if (likely (has_merge_sections || has_tls_symbols))
	{
	  fileinfo->symref = (struct symbol **)
	    obstack_calloc (&ld_state.smem,
			    nsymbols * sizeof (struct symbol *));

	  
	  for (cnt = 0; cnt < nlocalsymbols; ++cnt)
	    {
	      Elf32_Word shndx;
	      XElf_Sym_vardef (sym);

	      xelf_getsymshndx (symtabdata, xndxdata, cnt, sym, shndx);
	      if (sym == NULL)
		{
		  
		  fprintf (stderr, gettext ("%s: invalid ELF file (%s:%d)\n"),
			   fileinfo->rfname, __FILE__, __LINE__);
		  return 1;
		}

	      if (likely (shndx != SHN_XINDEX))
		shndx = sym->st_shndx;
	      else if (unlikely (shndx == 0))
		{
		  fprintf (stderr, gettext ("%s: invalid ELF file (%s:%d)\n"),
			   fileinfo->rfname, __FILE__, __LINE__);
		  return 1;
		}

	      if (XELF_ST_TYPE (sym->st_info) != STT_SECTION
		  && (shndx < SHN_LORESERVE || shndx > SHN_HIRESERVE)
		  && ((SCNINFO_SHDR (fileinfo->scninfo[shndx].shdr).sh_flags
		       & SHF_MERGE)
		      || XELF_ST_TYPE (sym->st_info) == STT_TLS))
		{
		  struct symbol *newp;

		  newp = (struct symbol *)
		    obstack_calloc (&ld_state.smem, sizeof (struct symbol));

		  newp->symidx = cnt;
		  newp->scndx = shndx;
		  newp->file = fileinfo;
		  newp->defined = 1;
		  fileinfo->symref[cnt] = newp;

		  if (fileinfo->scninfo[shndx].symbols == NULL)
		    fileinfo->scninfo[shndx].symbols = newp->next_in_scn
		      = newp;
		  else
		    {
		      newp->next_in_scn
			= fileinfo->scninfo[shndx].symbols->next_in_scn;
		      fileinfo->scninfo[shndx].symbols
			= fileinfo->scninfo[shndx].symbols->next_in_scn = newp;
		    }
		}
	    }
	}
      else
	fileinfo->symref = (struct symbol **)
	  obstack_calloc (&ld_state.smem, ((nsymbols - nlocalsymbols)
					   * sizeof (struct symbol *)))
	  - nlocalsymbols;

      for (cnt = nlocalsymbols; cnt < nsymbols; ++cnt)
	{
	  XElf_Sym_vardef (sym);
	  Elf32_Word shndx;
	  xelf_getsymshndx (symtabdata, xndxdata, cnt, sym, shndx);

	  if (sym == NULL)
	    {
	      
	      fprintf (stderr, gettext ("%s: invalid ELF file (%s:%d)\n"),
		       fileinfo->rfname, __FILE__, __LINE__);
	      return 1;
	    }

	  if (likely (shndx != SHN_XINDEX))
	    shndx = sym->st_shndx;
	  else if (unlikely (shndx == 0))
	    {
	      fprintf (stderr, gettext ("%s: invalid ELF file (%s:%d)\n"),
		       fileinfo->rfname, __FILE__, __LINE__);
	      return 1;
	    }

	  
	  
	  if (unlikely (shndx == SHN_ABS) && secttype == SHT_DYNSYM)
	    continue;

	  if ((shndx < SHN_LORESERVE || shndx > SHN_HIRESERVE)
	      && fileinfo->scninfo[shndx].unused_comdat)
	    
	    continue;

	  if (versymdata != NULL)
	    {
	      XElf_Versym versym;

	      if (xelf_getversym_copy (versymdata, cnt, versym) == NULL)
		
		assert (! "xelf_getversym failed");

	      if ((versym & 0x8000) != 0)
		
		continue;
	    }

	  
	  struct symbol search;
	  search.name = elf_strptr (fileinfo->elf, symstridx, sym->st_name);
	  unsigned long int hval = elf_hash (search.name);

	  
	  
	  if (((unlikely (hval == 165832675ul)
		&& strcmp (search.name, "_DYNAMIC") == 0)
	       || (unlikely (hval == 102264335ul)
		   && strcmp (search.name, "_GLOBAL_OFFSET_TABLE_") == 0))
	      && sym->st_shndx != SHN_UNDEF
	      && fileinfo->file_type != relocatable_file_type)
	    continue;

	  struct symbol *oldp = ld_symbol_tab_find (&ld_state.symbol_tab,
						    hval, &search);
	  struct symbol *newp;
	  if (likely (oldp == NULL))
	    {
	      
	      newp = (struct symbol *) obstack_alloc (&ld_state.smem,
						      sizeof (*newp));
	      newp->name = search.name;
	      newp->size = sym->st_size;
	      newp->type = XELF_ST_TYPE (sym->st_info);
	      newp->symidx = cnt;
	      newp->outsymidx = 0;
	      newp->outdynsymidx = 0;
	      newp->scndx = shndx;
	      newp->file = fileinfo;
	      newp->defined = newp->scndx != SHN_UNDEF;
	      newp->common = newp->scndx == SHN_COMMON;
	      newp->weak = XELF_ST_BIND (sym->st_info) == STB_WEAK;
	      newp->added = 0;
	      newp->merged = 0;
	      newp->local = 0;
	      newp->hidden = 0;
	      newp->need_copy = 0;
	      newp->on_dsolist = 0;
	      newp->in_dso = secttype == SHT_DYNSYM;
	      newp->next_in_scn = NULL;
#ifndef NDEBUG
	      newp->next = NULL;
	      newp->previous = NULL;
#endif

	      if (newp->scndx == SHN_UNDEF)
		{
		  CDBL_LIST_ADD_REAR (ld_state.unresolved, newp);
		  ++ld_state.nunresolved;
		  if (! newp->weak)
		    ++ld_state.nunresolved_nonweak;
		}
	      else if (newp->scndx == SHN_COMMON)
		{
		  
		  newp->merge.value = sym->st_value;

		  CDBL_LIST_ADD_REAR (ld_state.common_syms, newp);
		}

	      
	      if (unlikely (ld_symbol_tab_insert (&ld_state.symbol_tab,
						  hval, newp) != 0))
		
		abort ();

	      fileinfo->symref[cnt] = newp;

	      if (hval == 6685956 && strcmp (newp->name, "_init") == 0)
		ld_state.init_symbol = newp;
	      else if (hval == 6672457 && strcmp (newp->name, "_fini") == 0)
		ld_state.fini_symbol = newp;
	    }
	  else if (unlikely (check_definition (sym, shndx, cnt, fileinfo, oldp)
			     != 0))
	    return 1;
	  else
	    newp = fileinfo->symref[cnt] = oldp;

	  
	  if (shndx != SHN_UNDEF
	      && (shndx < SHN_LORESERVE || shndx > SHN_HIRESERVE))
	    {
	      struct scninfo *ignore;

#ifndef NDEBUG
	      size_t shnum;
	      assert (elf_getshdrnum (fileinfo->elf, &shnum) == 0);
	      assert (shndx < shnum);
#endif

	      
	      mark_section_used (&fileinfo->scninfo[shndx], shndx, &ignore);

	      if (SCNINFO_SHDR (fileinfo->scninfo[shndx].shdr).sh_flags
		  & SHF_MERGE)
		{
		  if (fileinfo->scninfo[shndx].symbols == NULL)
		    fileinfo->scninfo[shndx].symbols = newp->next_in_scn
		      = newp;
		  else
		    {
		      newp->next_in_scn
			= fileinfo->scninfo[shndx].symbols->next_in_scn;
		      fileinfo->scninfo[shndx].symbols
			= fileinfo->scninfo[shndx].symbols->next_in_scn = newp;
		    }
		}
	    }
	}

      
      if (likely (fileinfo->file_type == relocatable_file_type))
	{
	  if (unlikely (ld_state.relfiles == NULL))
	    ld_state.relfiles = fileinfo->next = fileinfo;
	  else
	    {
	      fileinfo->next = ld_state.relfiles->next;
	      ld_state.relfiles = ld_state.relfiles->next = fileinfo;
	    }

	  
	  ld_state.nsymtab += fileinfo->nsymtab;
	  ld_state.nlocalsymbols += fileinfo->nlocalsymbols;
	}
      else if (likely (fileinfo->file_type == dso_file_type))
	{
	  CSNGL_LIST_ADD_REAR (ld_state.dsofiles, fileinfo);
	  ++ld_state.ndsofiles;

	  if (fileinfo->lazyload)
	    ++ld_state.ndsofiles;
	}
    }

  return 0;
}


int
ld_handle_filename_list (struct filename_list *fnames)
{
  struct filename_list *runp;
  int res = 0;

  for (runp = fnames; runp != NULL; runp = runp->next)
    {
      struct usedfiles *curp;

      
      curp = runp->real = ld_new_inputfile (runp->name, relocatable_file_type);

      
      curp->group_start = runp->group_start;
      curp->group_end = runp->group_end;

      
      curp->as_needed = runp->as_needed;

      do
	res |= FILE_PROCESS (-1, curp, &ld_state, &curp);
      while (curp != NULL);
    }

  
  while (fnames != NULL)
    {
      runp = fnames;
      fnames = fnames->next;
      free (runp);
    }

  return res;
}


static int
open_elf (struct usedfiles *fileinfo, Elf *elf)
{
  int res = 0;

  if (elf == NULL)
    error (EXIT_FAILURE, 0,
	   gettext ("cannot get descriptor for ELF file (%s:%d): %s\n"),
	   __FILE__, __LINE__, elf_errmsg (-1));

  if (unlikely (elf_kind (elf) == ELF_K_NONE))
    {
      struct filename_list *fnames;

      
      fileinfo->status = closed;

      
      if (fileinfo->fd != -1)
	
	ldin = fdopen (fileinfo->fd, "r");
      else
	{
	  
	  char *content;
	  size_t contentsize;

	  
	  content = elf_rawfile (elf, &contentsize);
	  if (content == NULL)
	    {
	      fprintf (stderr, gettext ("%s: invalid ELF file (%s:%d)\n"),
		       fileinfo->rfname, __FILE__, __LINE__);
	      return 1;
	    }

	  ldin = fmemopen (content, contentsize, "r");
	}

      
      __fsetlocking (ldin, FSETLOCKING_BYCALLER);

      if (ldin == NULL)
	error (EXIT_FAILURE, errno, gettext ("cannot open '%s'"),
	       fileinfo->rfname);

      ld_state.srcfiles = NULL;
      ldlineno = 1;
      ld_scan_version_script = 0;
      ldin_fname = fileinfo->rfname;
      res = ldparse ();

      fclose (ldin);
      if (fileinfo->fd != -1 && !fileinfo->fd_passed)
	{
	  
	  close (fileinfo->fd);
	  fileinfo->fd = -1;
	}

      elf_end (elf);

      if (unlikely (res != 0))
	
	return 1;

      
      fileinfo->elf = NULL;

      fnames = ld_state.srcfiles;
      if (fnames != NULL)
	{
	  struct filename_list *oldp;

	  
	  oldp = fnames;
	  fnames = fnames->next;
	  oldp->next = NULL;

	  
	  ld_state.srcfiles = NULL;

	  if (unlikely (ld_handle_filename_list (fnames) != 0))
	    return 1;
	}

      return 0;
    }

  
  fileinfo->elf = elf;

  
  fileinfo->status = opened;

  return 0;
}


static int
add_whole_archive (struct usedfiles *fileinfo)
{
  Elf *arelf;
  Elf_Cmd cmd = ELF_C_READ_MMAP_PRIVATE;
  int res = 0;

  while ((arelf = elf_begin (fileinfo->fd, cmd, fileinfo->elf)) != NULL)
    {
      Elf_Arhdr *arhdr = elf_getarhdr (arelf);
      struct usedfiles *newp;

      if (arhdr == NULL)
	abort ();

      assert (strcmp (arhdr->ar_name, "/") != 0);
      assert (strcmp (arhdr->ar_name, "//") != 0);

      newp = ld_new_inputfile (arhdr->ar_name, relocatable_file_type);
      newp->archive_file = fileinfo;

      if (unlikely (ld_state.trace_files))
	print_file_name (stdout, newp, 1, 1);

      
      newp->fd = -1;
      
      newp->elf = arelf;
      
      newp->status = opened;

      
      res = file_process2 (newp);
      if (unlikely (res != 0))
	    break;

      
      cmd = elf_next (arelf);
    }

  return res;
}


static int
extract_from_archive (struct usedfiles *fileinfo)
{
  static int archive_seq;
  int res = 0;

  if (fileinfo->archive_seq == 0)
    fileinfo->archive_seq = ++archive_seq;

  
  assert (ld_state.extract_rule == defaultextract
	  || ld_state.extract_rule == weakextract);
  if ((likely (ld_state.extract_rule == defaultextract)
       ? ld_state.nunresolved_nonweak : ld_state.nunresolved) == 0)
    return 0;

  Elf_Arsym *syms;
  size_t nsyms;

  
  syms = elf_getarsym (fileinfo->elf, &nsyms);
  if (syms == NULL)
    {
    cannot_read_archive:
      error (0, 0, gettext ("cannot read archive `%s': %s"),
	     fileinfo->rfname, elf_errmsg (-1));

      
      fileinfo->status = closed;

      return 1;
    }

  
  bool any_used;
  do
    {
      any_used = false;

      size_t cnt;
      for (cnt = 0; cnt < nsyms; ++cnt)
	{
	  struct symbol search = { .name = syms[cnt].as_name };
	  struct symbol *sym = ld_symbol_tab_find (&ld_state.symbol_tab,
						   syms[cnt].as_hash, &search);
	  if (sym != NULL && ! sym->defined)
	    {
	      
	      Elf *arelf;
	      Elf_Arhdr *arhdr;
	      struct usedfiles *newp;

	      
	      if (unlikely (elf_rand (fileinfo->elf, syms[cnt].as_off)
			    != syms[cnt].as_off))
		goto cannot_read_archive;

	      arelf = elf_begin (fileinfo->fd, ELF_C_READ_MMAP_PRIVATE,
				 fileinfo->elf);
	      arhdr = elf_getarhdr (arelf);
	      if (arhdr == NULL)
		goto cannot_read_archive;

	      newp = ld_new_inputfile (obstack_strdup (&ld_state.smem,
						       arhdr->ar_name),
				       relocatable_file_type);
	      newp->archive_file = fileinfo;

	      if (unlikely (ld_state.trace_files))
		print_file_name (stdout, newp, 1, 1);

	      
	      newp->fd = -1;
	      
	      newp->elf = arelf;
	      
	      newp->status = in_archive;

	      
	      res = file_process2 (newp);
	      if (unlikely (res != 0))
		return res;

	      any_used = true;
	    }
	}

      if (any_used)
	{
	  
	  assert (fileinfo->archive_seq != 0);
	  ld_state.last_archive_used = fileinfo->archive_seq;
	}
    }
  while (any_used);

  return res;
}


static int
file_process2 (struct usedfiles *fileinfo)
{
  int res;

  if (likely (elf_kind (fileinfo->elf) == ELF_K_ELF))
    {
      
#if NATIVE_ELF != 0
      if (likely (fileinfo->ehdr == NULL))
#else
      if (likely (FILEINFO_EHDR (fileinfo->ehdr).e_type == ET_NONE))
#endif
	{
	  XElf_Ehdr *ehdr;
#if NATIVE_ELF != 0
	  ehdr = xelf_getehdr (fileinfo->elf, fileinfo->ehdr);
#else
	  xelf_getehdr_copy (fileinfo->elf, ehdr, fileinfo->ehdr);
#endif
	  if (ehdr == NULL)
	    {
	      fprintf (stderr, gettext ("%s: invalid ELF file (%s:%d)\n"),
		       fileinfo->rfname, __FILE__, __LINE__);
	      fileinfo->status = closed;
	      return 1;
	    }

	  if (FILEINFO_EHDR (fileinfo->ehdr).e_type != ET_REL
	      && unlikely (FILEINFO_EHDR (fileinfo->ehdr).e_type != ET_DYN))
	    {
	      char buf[64];

	      print_file_name (stderr, fileinfo, 1, 0);
	      fprintf (stderr,
		       gettext ("file of type %s cannot be linked in\n"),
		       ebl_object_type_name (ld_state.ebl,
					     FILEINFO_EHDR (fileinfo->ehdr).e_type,
					     buf, sizeof (buf)));
	      fileinfo->status = closed;
	      return 1;
	    }

	  
	  if (FILEINFO_EHDR (fileinfo->ehdr).e_machine
	      != ebl_get_elfmachine (ld_state.ebl))
	    {
	      fprintf (stderr, gettext ("\
%s: input file incompatible with ELF machine type %s\n"),
		       fileinfo->rfname,
		       ebl_backend_name (ld_state.ebl));
	      fileinfo->status = closed;
	      return 1;
	    }

	  
	  if (unlikely (elf_getshdrstrndx (fileinfo->elf, &fileinfo->shstrndx)
			< 0))
	    {
	      fprintf (stderr, gettext ("\
%s: cannot get section header string table index: %s\n"),
		       fileinfo->rfname, elf_errmsg (-1));
	      fileinfo->status = closed;
	      return 1;
	    }
	}

      
      if (FILEINFO_EHDR (fileinfo->ehdr).e_type == ET_REL)
	{
	  res = add_relocatable_file (fileinfo, SHT_SYMTAB);
	}
      else
	{
	  bool has_l_name = fileinfo->file_type == archive_file_type;

	  assert (FILEINFO_EHDR (fileinfo->ehdr).e_type == ET_DYN);

	  if (fileinfo->file_type != dso_needed_file_type)
	    fileinfo->file_type = dso_file_type;

	  
	  if (ld_state.file_type == relocatable_file_type)
	    {
	      error (0, 0, gettext ("\
cannot use DSO '%s' when generating relocatable object file"),
		     fileinfo->fname);
	      return 1;
	    }

	  res = add_relocatable_file (fileinfo, SHT_DYNSYM);

	  
	  assert (fileinfo->dynscn != NULL);

	  XElf_Shdr_vardef (dynshdr);
	  xelf_getshdr (fileinfo->dynscn, dynshdr);

	  Elf_Data *dyndata = elf_getdata (fileinfo->dynscn, NULL);
	  
	  if (dynshdr != NULL)
	    {
	      int cnt = dynshdr->sh_size / dynshdr->sh_entsize;
	      XElf_Dyn_vardef (dyn);

	      while (--cnt >= 0)
		{
		  xelf_getdyn (dyndata, cnt, dyn);
		  if (dyn != NULL)
		    {
		      if(dyn->d_tag == DT_NEEDED)
			{
			  struct usedfiles *newp;

			  newp = ld_new_inputfile (elf_strptr (fileinfo->elf,
							       dynshdr->sh_link,
							       dyn->d_un.d_val),
						   dso_needed_file_type);

			  
			  
			  
			  CSNGL_LIST_ADD_REAR (ld_state.needed, newp);
			}
		      else if (dyn->d_tag == DT_SONAME)
			{
			  fileinfo->soname = elf_strptr (fileinfo->elf,
							 dynshdr->sh_link,
							 dyn->d_un.d_val);
			  has_l_name = false;
			}
		    }
		}
	    }

	  if (unlikely (has_l_name))
	    {
	      size_t len = strlen (fileinfo->fname) + 7;
	      char *newp;

	      newp = (char *) obstack_alloc (&ld_state.smem, len);
	      strcpy (stpcpy (stpcpy (newp, "lib"), fileinfo->fname), ".so");

	      fileinfo->soname = newp;
	    }
	}
    }
  else if (likely (elf_kind (fileinfo->elf) == ELF_K_AR))
    {
      if (unlikely (ld_state.extract_rule == allextract))
	res = add_whole_archive (fileinfo);
      else if (ld_state.file_type == relocatable_file_type)
	{
	  if (verbose)
	    error (0, 0, gettext ("input file '%s' ignored"), fileinfo->fname);

	  res = 0;
	}
      else
	{
	  if (ld_state.group_start_requested
	      && ld_state.group_start_archive == NULL)
	    ld_state.group_start_archive = fileinfo;

	  if (ld_state.archives == NULL)
	    ld_state.archives = fileinfo;

	  if (ld_state.tailarchives != NULL)
	    ld_state.tailarchives->next = fileinfo;
	  ld_state.tailarchives = fileinfo;

	  res = extract_from_archive (fileinfo);
	}
    }
  else
    
    abort ();

  return res;
}


static int
ld_generic_file_process (int fd, struct usedfiles *fileinfo,
			 struct ld_state *statep, struct usedfiles **nextp)
{
  int res = 0;

  
  *nextp = fileinfo->next;

  
  if (unlikely (fileinfo->group_start))
    {
      ld_state.group_start_requested = true;
      fileinfo->group_start = false;
    }

  
  if (likely (fileinfo->status == not_opened))
    {
      bool fd_passed = true;

      if (likely (fd == -1))
	{
	  
	  int err = open_along_path (fileinfo);
	  if (unlikely (err != 0))
	    return err == EAGAIN ? 0 : err;

	  fd_passed = false;
	}
      else
	fileinfo->fd = fd;

      
      fileinfo->fd_passed = fd_passed;

      res = open_elf (fileinfo, elf_begin (fileinfo->fd,
					   is_dso_p (fileinfo->fd)
					   ? ELF_C_READ_MMAP
					   : ELF_C_READ_MMAP_PRIVATE, NULL));
      if (unlikely (res != 0))
	return res;
    }

  
  if (likely (fileinfo->status != closed))
    res = file_process2 (fileinfo);

  
  if (unlikely (fileinfo->group_backref != NULL))
    {
      if (ld_state.last_archive_used > fileinfo->group_backref->archive_seq)
	{
	  *nextp = fileinfo->group_backref;
	  ld_state.last_archive_used = 0;
	}
      else
	{
	  struct usedfiles *runp = ld_state.archives;

	  do
	    {
	      elf_end (runp->elf);
	      runp->elf = NULL;

	      runp = runp->next;
	    }
	  while (runp != fileinfo->next);

	  
	  ld_state.archives = NULL;

	  
	  *nextp = fileinfo->next = NULL;
	}
    }
  else if (unlikely (fileinfo->group_end))
    {
      if (ld_state.group_start_requested)
	{
	  if (ld_state.group_start_archive != ld_state.tailarchives)
	    
	    {
	      *nextp = ld_state.tailarchives->group_backref =
		ld_state.group_start_archive;
	      ld_state.last_archive_used = 0;
	    }
	  else
	    if (ld_state.tailarchives != fileinfo)
	      {
		*nextp = ld_state.group_start_archive;
		ld_state.last_archive_used = 0;
	      }
	}

      
      ld_state.group_start_requested = false;
      ld_state.group_start_archive = NULL;
      fileinfo->group_end = false;
    }

  return res;
}


static const char **
ld_generic_lib_extensions (struct ld_state *statep __attribute__ ((__unused__)))
{
  static const char *exts[] =
    {
      ".so", ".a", NULL
    };

  return exts;
}


static int
ld_generic_flag_unresolved (struct ld_state *statep)
{
  int retval = 0;

  if (ld_state.nunresolved_nonweak > 0)
    {
      
      struct symbol *first;
      struct symbol *s;

      s = first = ld_state.unresolved->next;
      do
	{
	  if (! s->defined && ! s->weak)
	    {
	      if (strcmp (s->name, "_GLOBAL_OFFSET_TABLE_") == 0
		  || strcmp (s->name, "_DYNAMIC") == 0)
		{
		  
		  ld_state.need_got = true;

		  
		  if (s->name[1] == 'G')
		    ld_state.got_symbol = s;
		  else
		    ld_state.dyn_symbol = s;
		}
	      else if (ld_state.file_type != dso_file_type || !ld_state.nodefs)
		{
		  error (0, 0, gettext ("undefined symbol `%s' in %s"),
			 s->name, s->file->fname);

		  retval = 1;
		}
	    }


	  s = s->next;
	}
      while (s != first);
    }

  return retval;
}


static int
ld_generic_file_close (struct usedfiles *fileinfo, struct ld_state *statep)
{
  
  elf_end (fileinfo->elf);

  if (!fileinfo->fd_passed && fileinfo->fd != -1)
    close (fileinfo->fd);

  
  if (fileinfo->fname != fileinfo->rfname)
    free ((char *) fileinfo->rfname);

  return 0;
}


static void
new_generated_scn (enum scn_kind kind, const char *name, int type, int flags,
		   int entsize, int align)
{
  struct scnhead *newp;

  newp = (struct scnhead *) obstack_calloc (&ld_state.smem,
					    sizeof (struct scnhead));
  newp->kind = kind;
  newp->name = name;
  newp->nameent = ebl_strtabadd (ld_state.shstrtab, name, 0);
  newp->type = type;
  newp->flags = flags;
  newp->entsize = entsize;
  newp->align = align;
  newp->grp_signature = NULL;
  newp->used = true;

  ld_section_tab_insert (&ld_state.section_tab, elf_hash (name), newp);
}


static void
ld_generic_generate_sections (struct ld_state *statep)
{
  
  int rel_type = REL_TYPE (&ld_state) == DT_REL ? SHT_REL : SHT_RELA;

  
  if (statep->build_id != NULL)
    new_generated_scn (scn_dot_note_gnu_build_id, ".note.gnu.build-id",
		       SHT_NOTE, SHF_ALLOC, 0, 4);

  if (dynamically_linked_p ())
    {
      
      bool use_versioning = false;
      
      bool need_version = false;

      
      if (ld_state.interp != NULL || ld_state.file_type != dso_file_type)
	new_generated_scn (scn_dot_interp, ".interp", SHT_PROGBITS, SHF_ALLOC,
			   0, 1);

      
      new_generated_scn (scn_dot_dynamic, ".dynamic", SHT_DYNAMIC,
			 DYNAMIC_SECTION_FLAGS (&ld_state),
			 xelf_fsize (ld_state.outelf, ELF_T_DYN, 1),
			 xelf_fsize (ld_state.outelf, ELF_T_ADDR, 1));

      ld_state.need_dynsym = true;
      new_generated_scn (scn_dot_dynsym, ".dynsym", SHT_DYNSYM, SHF_ALLOC,
			 xelf_fsize (ld_state.outelf, ELF_T_SYM, 1),
			 xelf_fsize (ld_state.outelf, ELF_T_ADDR, 1));
      
      new_generated_scn (scn_dot_dynstr, ".dynstr", SHT_STRTAB, SHF_ALLOC,
			 0, 1);
      
      
      if (GENERATE_SYSV_HASH)
	new_generated_scn (scn_dot_hash, ".hash", SHT_HASH, SHF_ALLOC,
			   sizeof (Elf32_Word), sizeof (Elf32_Word));
      if (GENERATE_GNU_HASH)
	new_generated_scn (scn_dot_gnu_hash, ".gnu.hash", SHT_GNU_HASH,
			   SHF_ALLOC, sizeof (Elf32_Word),
			   sizeof (Elf32_Word));

      
      if (ld_state.nplt > 0)
	{
	  
	  
	  new_generated_scn (scn_dot_plt, ".plt", SHT_PROGBITS,
			     SHF_ALLOC | SHF_EXECINSTR,
			     
			     xelf_fsize (ld_state.outelf, ELF_T_ADDR, 1),
			     xelf_fsize (ld_state.outelf, ELF_T_ADDR, 1));

	  new_generated_scn (scn_dot_pltrel, ".rel.plt", rel_type, SHF_ALLOC,
			     rel_type == SHT_REL
			     ? xelf_fsize (ld_state.outelf, ELF_T_REL, 1)
			     : xelf_fsize (ld_state.outelf, ELF_T_RELA, 1),
			     xelf_fsize (ld_state.outelf, ELF_T_ADDR, 1));

	  
	  new_generated_scn (scn_dot_gotplt, ".got.plt", SHT_PROGBITS,
			     SHF_ALLOC | SHF_WRITE,
			     xelf_fsize (ld_state.outelf, ELF_T_ADDR, 1),
			     xelf_fsize (ld_state.outelf, ELF_T_ADDR, 1));

	  if (ld_state.from_dso != NULL)
	    {
	      struct symbol *srunp = ld_state.from_dso;

	      do
		{
		  srunp->file->used = true;

		  if (srunp->file->verdefdata != NULL)
		    {
		      XElf_Versym versym;

		      
		      use_versioning = true;
		      
		      need_version = true;

		      if (xelf_getversym_copy (srunp->file->versymdata,
					       srunp->symidx, versym) == NULL)
			assert (! "xelf_getversym failed");

		      assert ((versym & 0x8000) == 0);
		      assert (versym > 1);

		      if (! srunp->file->verdefused[versym])
			{
			  srunp->file->verdefused[versym] = 1;

			  if (++srunp->file->nverdefused == 1)
			    
			    ++ld_state.nverdeffile;
			  ++ld_state.nverdefused;
			}
		    }
		}
	      while ((srunp = srunp->next) != ld_state.from_dso);
	    }

	  
	  if (need_version)
	    new_generated_scn (scn_dot_version_r, ".gnu.version_r",
			       SHT_GNU_verneed, SHF_ALLOC, 0,
			       xelf_fsize (ld_state.outelf, ELF_T_WORD, 1));
	}

      int ndt_needed = 0;
      if (ld_state.ndsofiles > 0)
	{
	  struct usedfiles *frunp = ld_state.dsofiles;

	  do
	    if (! frunp->as_needed || frunp->used)
	      {
		++ndt_needed;
		if (frunp->lazyload)
		  ++ndt_needed;
	      }
	  while ((frunp = frunp->next) != ld_state.dsofiles);
	}

      if (use_versioning)
	new_generated_scn (scn_dot_version, ".gnu.version", SHT_GNU_versym,
			   SHF_ALLOC,
			   xelf_fsize (ld_state.outelf, ELF_T_HALF, 1),
			   xelf_fsize (ld_state.outelf, ELF_T_HALF, 1));

      
      ld_state.ndynamic = (7 + (ld_state.runpath != NULL
				|| ld_state.rpath != NULL)
			   + ndt_needed
			   + (ld_state.init_symbol != NULL ? 1 : 0)
			   + (ld_state.fini_symbol != NULL ? 1 : 0)
			   + (use_versioning ? 1 : 0)
			   + (need_version ? 2 : 0)
			   + (ld_state.nplt > 0 ? 4 : 0)
			   + (ld_state.relsize_total > 0 ? 3 : 0));
    }

  ld_state.need_symtab = (ld_state.file_type == relocatable_file_type
			  || ld_state.strip == strip_none);

  
  if (ld_state.need_got)
    
    new_generated_scn (scn_dot_got, ".got", SHT_PROGBITS,
		       SHF_ALLOC | SHF_WRITE,
		       xelf_fsize (ld_state.outelf, ELF_T_ADDR, 1),
		       xelf_fsize (ld_state.outelf, ELF_T_ADDR, 1));

  
  if (ld_state.relsize_total > 0)
    new_generated_scn (scn_dot_dynrel, ".rel.dyn", rel_type, SHF_ALLOC,
		       rel_type == SHT_REL
		       ? xelf_fsize (ld_state.outelf, ELF_T_REL, 1)
		       : xelf_fsize (ld_state.outelf, ELF_T_RELA, 1),
		       xelf_fsize (ld_state.outelf, ELF_T_ADDR, 1));
}


static void
remove_tempfile (int status, void *arg)
{
  if (status != 0 && ld_state.tempfname != NULL)
    unlink (ld_state.tempfname);
}


static int
ld_generic_open_outfile (struct ld_state *statep, int machine, int klass,
			 int data)
{
  if (ld_state.outfname == NULL)
    ld_state.outfname = "a.out";

  size_t outfname_len = strlen (ld_state.outfname);
  char *tempfname = (char *) obstack_alloc (&ld_state.smem,
					    outfname_len + sizeof (".XXXXXX"));
  ld_state.tempfname = tempfname;

  int fd;
  int try = 0;
  while (1)
    {
      strcpy (mempcpy (tempfname, ld_state.outfname, outfname_len), ".XXXXXX");

      if (mktemp (tempfname) != NULL
	  && (fd = open (tempfname, O_RDWR | O_EXCL | O_CREAT | O_NOFOLLOW,
			 ld_state.file_type == relocatable_file_type
			 ? DEFFILEMODE : ACCESSPERMS)) != -1)
	break;

      
      if (++try >= 10)
	error (EXIT_FAILURE, errno, gettext ("cannot create output file"));
    }
  ld_state.outfd = fd;

  on_exit (remove_tempfile, NULL);

  
  Elf *elf = ld_state.outelf = elf_begin (fd,
					  conserve_memory
					  ? ELF_C_WRITE : ELF_C_WRITE_MMAP,
					  NULL);
  if (elf == NULL)
    error (EXIT_FAILURE, 0,
	   gettext ("cannot create ELF descriptor for output file: %s"),
	   elf_errmsg (-1));

  
  if (! xelf_newehdr (elf, klass))
    
    error (EXIT_FAILURE, 0,
	   gettext ("could not create ELF header for output file: %s"),
	   elf_errmsg (-1));

  
  XElf_Ehdr_vardef (ehdr);
  xelf_getehdr (elf, ehdr);
  assert (ehdr != NULL);

  
  ehdr->e_machine = machine;

  
  if (ld_state.file_type == executable_file_type)
    ehdr->e_type = ET_EXEC;
  else if (ld_state.file_type == dso_file_type)
    ehdr->e_type = ET_DYN;
  else
    {
      assert (ld_state.file_type == relocatable_file_type);
      ehdr->e_type = ET_REL;
    }

  
  ehdr->e_version = EV_CURRENT;

  
  ehdr->e_ident[EI_DATA] = data;

  
  (void) xelf_update_ehdr (elf, ehdr);

  return 0;
}


static void
compute_copy_reloc_offset (XElf_Shdr *shdr)
{
  struct symbol *runp = ld_state.from_dso;
  assert (runp != NULL);

  XElf_Off maxalign = 1;
  XElf_Off offset = 0;

  do
    if (runp->need_copy)
      {
	
	
	
	

	
	
	
	XElf_Off symalign = MAX (SCNINFO_SHDR (runp->file->scninfo[runp->scndx].shdr).sh_addralign, 1);
	
	maxalign = MAX (maxalign, symalign);

	
	offset = (offset + symalign - 1) & ~(symalign - 1);

	runp->merge.value = offset;

	offset += runp->size;
      }
  while ((runp = runp->next) != ld_state.from_dso);

  shdr->sh_type = SHT_NOBITS;
  shdr->sh_size = offset;
  shdr->sh_addralign = maxalign;
}


static void
compute_common_symbol_offset (XElf_Shdr *shdr)
{
  struct symbol *runp = ld_state.common_syms;
  assert (runp != NULL);

  XElf_Off maxalign = 1;
  XElf_Off offset = 0;

  do
    {
      
      XElf_Off symalign = runp->merge.value;

      
      maxalign = MAX (maxalign, symalign);

      
      offset = (offset + symalign - 1) & ~(symalign - 1);

      runp->merge.value = offset;

      offset += runp->size;
    }
  while ((runp = runp->next) != ld_state.common_syms);

  shdr->sh_type = SHT_NOBITS;
  shdr->sh_size = offset;
  shdr->sh_addralign = maxalign;
}


static void
sort_sections_generic (void)
{
  
  abort ();
}


static int
match_section (const char *osectname, struct filemask_section_name *sectmask,
	       struct scnhead **scnhead, bool new_section, size_t segment_nr)
{
  struct scninfo *prevp;
  struct scninfo *runp;
  struct scninfo *notused;

  if (fnmatch (sectmask->section_name->name, (*scnhead)->name, 0) != 0)
    
    return new_section;

  if ((*scnhead)->kind != scn_normal)
    {
      (*scnhead)->name = osectname;
      (*scnhead)->segment_nr = segment_nr;

      if ((*scnhead)->type == SHT_NOTE)
	++ld_state.nnotesections;

      ld_state.allsections[ld_state.nallsections++] = (*scnhead);
      return true;
    }

  runp = (*scnhead)->last->next;
  prevp = (*scnhead)->last;
  notused = NULL;

  do
    {
      
      const char *brfname = basename (runp->fileinfo->rfname);

      if (!runp->used
	  || (sectmask->filemask != NULL
	      && fnmatch (sectmask->filemask, brfname, 0) != 0)
	  || (sectmask->excludemask != NULL
	      && fnmatch (sectmask->excludemask, brfname, 0) == 0))
	{
	  
	  if (notused == NULL)
	    notused = runp;

	  prevp = runp;
	  runp = runp->next;
	  if (runp == notused)
	    runp = NULL;
	}
      else
	{
	  struct scninfo *found = runp;

	  
	  if (prevp != runp)
	    runp = prevp->next = runp->next;
	  else
	    {
	      free (*scnhead);
	      *scnhead = NULL;
	      runp = NULL;
	    }

	  if (new_section)
	    {
	      struct scnhead *newp;

	      newp = (struct scnhead *) obstack_calloc (&ld_state.smem,
							sizeof (*newp));
	      newp->kind = scn_normal;
	      newp->name = osectname;
	      newp->type = SCNINFO_SHDR (found->shdr).sh_type;
	      newp->flags = SCNINFO_SHDR (found->shdr).sh_flags & ~SHF_GROUP;
	      newp->segment_nr = segment_nr;
	      newp->last = found->next = found;
	      newp->used = true;
	      newp->relsize = found->relsize;
	      newp->entsize = SCNINFO_SHDR (found->shdr).sh_entsize;

	      if (newp->type == SHT_NOTE)
		++ld_state.nnotesections;

	      ld_state.allsections[ld_state.nallsections++] = newp;
	      new_section = false;
	    }
	  else
	    {
	      struct scnhead *queued;

	      queued = ld_state.allsections[ld_state.nallsections - 1];

	      found->next = queued->last->next;
	      queued->last = queued->last->next = found;

	      if (queued->type != SCNINFO_SHDR (found->shdr).sh_type)
		
		queued->type = SHT_PROGBITS;
	      if (queued->flags != SCNINFO_SHDR (found->shdr).sh_flags)
		queued->flags = ebl_sh_flags_combine (ld_state.ebl,
						      queued->flags,
						      SCNINFO_SHDR (found->shdr).sh_flags
						      & ~SHF_GROUP);

	      
	      queued->relsize += found->relsize;
	    }
	}
    }
  while (runp != NULL);

  return new_section;
}


static void
sort_sections_lscript (void)
{
  struct scnhead *temp[ld_state.nallsections];

  
  memcpy (temp, ld_state.allsections,
	  ld_state.nallsections * sizeof (temp[0]));
  size_t nallsections = ld_state.nallsections;

  
  struct output_segment *segment = ld_state.output_segments->next;
  ld_state.output_segments->next = NULL;
  ld_state.output_segments = segment;

  ld_state.nallsections = 0;
  size_t segment_nr;
  size_t last_writable = ~0ul;
  for (segment_nr = 0; segment != NULL; segment = segment->next, ++segment_nr)
    {
      struct output_rule *orule;

      for (orule = segment->output_rules; orule != NULL; orule = orule->next)
	if (orule->tag == output_section)
	  {
	    struct input_rule *irule;
	    bool new_section = true;

	    for (irule = orule->val.section.input; irule != NULL;
		 irule = irule->next)
	      if (irule->tag == input_section)
		{
		  size_t cnt;

		  for (cnt = 0; cnt < nallsections; ++cnt)
		    if (temp[cnt] != NULL)
		      new_section =
			match_section (orule->val.section.name,
				       irule->val.section, &temp[cnt],
				       new_section, segment_nr);
		}
	  }

      if ((segment->mode & PF_W) != 0)
	last_writable = ld_state.nallsections - 1;
    }

  if (ld_state.ncopy > 0 || ld_state.common_syms !=  NULL)
    {
      if (last_writable == ~0ul)
	error (EXIT_FAILURE, 0, "no writable segment");

      if (ld_state.allsections[last_writable]->type != SHT_NOBITS)
	{
	  ++last_writable;
	  memmove (&ld_state.allsections[last_writable + 1],
		   &ld_state.allsections[last_writable],
		   (ld_state.nallsections - last_writable)
		   * sizeof (ld_state.allsections[0]));

	  ld_state.allsections[last_writable] = (struct scnhead *)
	    obstack_calloc (&ld_state.smem, sizeof (struct scnhead));

	  
	  ld_state.allsections[last_writable]->name = ".bss";
	  
	  ld_state.allsections[last_writable]->type = SHT_NOBITS;
	  
	  ld_state.allsections[last_writable]->segment_nr
	    = ld_state.allsections[last_writable - 1]->segment_nr;
	}
    }

  
  if (ld_state.ncopy > 0)
    {
#if NATIVE_ELF
      struct scninfo *si = (struct scninfo *)
	obstack_calloc (&ld_state.smem, sizeof (*si) + sizeof (XElf_Shdr));
      si->shdr = (XElf_Shdr *) (si + 1);
#else
      struct scninfo *si = (struct scninfo *) obstack_calloc (&ld_state.smem,
							      sizeof (*si));
#endif

      
      compute_copy_reloc_offset (&SCNINFO_SHDR (si->shdr));

      
      si->used = true;
      
      ld_state.copy_section = si;

      if (likely (ld_state.allsections[last_writable]->last != NULL))
	{
	  si->next = ld_state.allsections[last_writable]->last->next;
	  ld_state.allsections[last_writable]->last->next = si;
	  ld_state.allsections[last_writable]->last = si;
	}
      else
	ld_state.allsections[last_writable]->last = si->next = si;
    }

  
  if (ld_state.common_syms != NULL)
    {
#if NATIVE_ELF
      struct scninfo *si = (struct scninfo *)
	obstack_calloc (&ld_state.smem, sizeof (*si) + sizeof (XElf_Shdr));
      si->shdr = (XElf_Shdr *) (si + 1);
#else
      struct scninfo *si = (struct scninfo *) obstack_calloc (&ld_state.smem,
							      sizeof (*si));
#endif

      
      compute_common_symbol_offset (&SCNINFO_SHDR (si->shdr));

      
      si->used = true;
      
      ld_state.common_section = si;

      if (likely (ld_state.allsections[last_writable]->last != NULL))
	{
	  si->next = ld_state.allsections[last_writable]->last->next;
	  ld_state.allsections[last_writable]->last->next = si;
	  ld_state.allsections[last_writable]->last = si;
	}
      else
	ld_state.allsections[last_writable]->last = si->next = si;
    }
}


static void
ld_generic_create_sections (struct ld_state *statep)
{
  struct scngroup *groups;
  size_t cnt;

  if (ld_state.file_type != relocatable_file_type)
    {
      struct scninfo *list = NULL;
      for (cnt = 0; cnt < ld_state.nallsections; ++cnt)
	if ((ld_state.allsections[cnt]->type == SHT_REL
	     || ld_state.allsections[cnt]->type == SHT_RELA)
	    && ld_state.allsections[cnt]->last != NULL)
	  {
	    if (list == NULL)
	      list = ld_state.allsections[cnt]->last;
	    else
	      {
		
		struct scninfo *first = list->next;
		list->next = ld_state.allsections[cnt]->last->next;
		ld_state.allsections[cnt]->last->next = first;
		list = ld_state.allsections[cnt]->last;
	      }

	    
	    ld_state.allsections[cnt] = NULL;
	  }
      ld_state.rellist = list;

      if (ld_state.output_segments == NULL)
	
	sort_sections_generic ();
      else
	sort_sections_lscript ();
    }

  for (cnt = 0; cnt < ld_state.nallsections; ++cnt)
    {
      struct scnhead *head = ld_state.allsections[cnt];
      Elf_Scn *scn;
      XElf_Shdr_vardef (shdr);

      
      if (!head->used)
	continue;

      if (ld_state.file_type == relocatable_file_type
	  && unlikely (head->flags & SHF_GROUP))
	{
	  struct scninfo *runp;
	  Elf32_Word here_groupidx = 0;
	  struct scngroup *here_group;
	  struct member *newp;

	  runp = head->last;
	  do
	    {
	      assert (runp->grpid != 0);

	      here_groupidx = runp->fileinfo->scninfo[runp->grpid].outscnndx;
	      if (here_groupidx != 0)
		break;
	    }
	  while ((runp = runp->next) != head->last);

	  if (here_groupidx == 0)
	    {
	      
	      scn = elf_newscn (ld_state.outelf);
	      xelf_getshdr (scn, shdr);
	      if (shdr == NULL)
		error (EXIT_FAILURE, 0,
		       gettext ("cannot create section for output file: %s"),
		       elf_errmsg (-1));

	      here_group = (struct scngroup *) xmalloc (sizeof (*here_group));
	      here_group->outscnidx = here_groupidx = elf_ndxscn (scn);
	      here_group->nscns = 0;
	      here_group->member = NULL;
	      here_group->next = ld_state.groups;
	      here_group->nameent
		= ebl_strtabadd (ld_state.shstrtab,
				 elf_strptr (runp->fileinfo->elf,
					     runp->fileinfo->shstrndx,
					     SCNINFO_SHDR (runp->shdr).sh_name),
				 0);
	      
	      here_group->symbol
		= runp->fileinfo->scninfo[runp->grpid].symbols;

	      ld_state.groups = here_group;
	    }
	  else
	    {
	      
	      here_group = ld_state.groups;
	      while (here_group->outscnidx != here_groupidx)
		here_group = here_group->next;
	    }

	  
	  newp = (struct member *) alloca (sizeof (*newp));
	  newp->scn = head;
#ifndef NDT_NEEDED
	  newp->next = NULL;
#endif
	  CSNGL_LIST_ADD_REAR (here_group->member, newp);
	  ++here_group->nscns;

	  
	  runp = head->last;
	  do
	    {
	      assert (runp->grpid != 0);

	      if (runp->fileinfo->scninfo[runp->grpid].outscnndx == 0)
		runp->fileinfo->scninfo[runp->grpid].outscnndx = here_groupidx;
	      else
		assert (runp->fileinfo->scninfo[runp->grpid].outscnndx
			== here_groupidx);
	    }
	  while ((runp = runp->next) != head->last);
	}

      if (head->kind == scn_normal)
	head->nameent = ebl_strtabadd (ld_state.shstrtab, head->name, 0);

      scn = elf_newscn (ld_state.outelf);
      head->scnidx = elf_ndxscn (scn);
      xelf_getshdr (scn, shdr);
      if (shdr == NULL)
	error (EXIT_FAILURE, 0,
	       gettext ("cannot create section for output file: %s"),
	       elf_errmsg (-1));

      assert (head->type != SHT_NULL);
      assert (head->type != SHT_SYMTAB);
      assert (head->type != SHT_DYNSYM || head->kind != scn_normal);
      assert (head->type != SHT_STRTAB || head->kind != scn_normal);
      assert (head->type != SHT_GROUP);
      shdr->sh_type = head->type;
      shdr->sh_flags = head->flags;
      shdr->sh_addralign = head->align;
      shdr->sh_entsize = head->entsize;
      assert (shdr->sh_entsize != 0 || (shdr->sh_flags & SHF_MERGE) == 0);
      (void) xelf_update_shdr (scn, shdr);

      if (head->kind == scn_dot_dynsym)
	ld_state.dynsymscnidx = elf_ndxscn (scn);
    }

  
  groups = ld_state.groups;
  while (groups != NULL)
    {
      Elf_Scn *scn;
      Elf_Data *data;
      Elf32_Word *grpdata;
      struct member *runp;

      scn = elf_getscn (ld_state.outelf, groups->outscnidx);
      assert (scn != NULL);

      data = elf_newdata (scn);
      if (data == NULL)
	error (EXIT_FAILURE, 0,
	       gettext ("cannot create section for output file: %s"),
	       elf_errmsg (-1));

      data->d_size = (groups->nscns + 1) * sizeof (Elf32_Word);
      data->d_buf = grpdata = (Elf32_Word *) xmalloc (data->d_size);
      data->d_type = ELF_T_WORD;
      data->d_version = EV_CURRENT;
      data->d_off = 0;
      
      data->d_align = sizeof (Elf32_Word);

      
      
      grpdata[0] = 0;

      runp = groups->member->next;
      cnt = 1;
      do
	
	grpdata[cnt++] = runp->scn->scnidx;
      while ((runp = runp->next) != groups->member->next);

      groups = groups->next;
    }
}


static bool
reduce_symbol_p (XElf_Sym *sym, struct Ebl_Strent *strent)
{
  const char *str;
  const char *version;
  struct id_list search;
  struct id_list *verp;
  bool result = ld_state.default_bind_local;

  if (XELF_ST_BIND (sym->st_info) == STB_LOCAL || sym->st_shndx == SHN_UNDEF)
    
    return false;

  
  assert (XELF_ST_BIND (sym->st_info) == STB_GLOBAL
	  || XELF_ST_BIND (sym->st_info) == STB_WEAK);

  str = ebl_string (strent);
  version = strchr (str, VER_CHR);
  if (version != NULL)
    {
      search.id = strndupa (str, version - str);
      if (*++version == VER_CHR)
	
	++version;
    }
  else
    {
      search.id = str;
      version = "";
    }

  verp = ld_version_str_tab_find (&ld_state.version_str_tab,
				  elf_hash (search.id), &search);
  while (verp != NULL)
    {
      if (strcmp (verp->u.s.versionname, version) == 0)
	
	return verp->u.s.local;

      verp = verp->next;
    }

  

  return result;
}


static XElf_Addr
eval_expression (struct expression *expr, XElf_Addr addr)
{
  XElf_Addr val = ~((XElf_Addr) 0);

  switch (expr->tag)
    {
    case exp_num:
      val = expr->val.num;
      break;

    case exp_sizeof_headers:
      {
	XElf_Shdr_vardef (shdr);

	xelf_getshdr (elf_getscn (ld_state.outelf, 1), shdr);
	assert (shdr != NULL);

	val = shdr->sh_offset;
      }
      break;

    case exp_pagesize:
      val = ld_state.pagesize;
      break;

    case exp_id:
      if (strcmp (expr->val.str, ".") != 0)
	error (EXIT_FAILURE, 0, gettext ("\
address computation expression contains variable '%s'"),
	       expr->val.str);

      val = addr;
      break;

    case exp_mult:
      val = (eval_expression (expr->val.binary.left, addr)
	     * eval_expression (expr->val.binary.right, addr));
      break;

    case exp_div:
      val = (eval_expression (expr->val.binary.left, addr)
	     / eval_expression (expr->val.binary.right, addr));
      break;

    case exp_mod:
      val = (eval_expression (expr->val.binary.left, addr)
	     % eval_expression (expr->val.binary.right, addr));
      break;

    case exp_plus:
      val = (eval_expression (expr->val.binary.left, addr)
	     + eval_expression (expr->val.binary.right, addr));
      break;

    case exp_minus:
      val = (eval_expression (expr->val.binary.left, addr)
	     - eval_expression (expr->val.binary.right, addr));
      break;

    case exp_and:
      val = (eval_expression (expr->val.binary.left, addr)
	     & eval_expression (expr->val.binary.right, addr));
      break;

    case exp_or:
      val = (eval_expression (expr->val.binary.left, addr)
	     | eval_expression (expr->val.binary.right, addr));
      break;

    case exp_align:
      val = eval_expression (expr->val.child, addr);
      if ((val & (val - 1)) != 0)
	error (EXIT_FAILURE, 0, gettext ("argument '%" PRIuMAX "' of ALIGN in address computation expression is no power of two"),
	       (uintmax_t) val);
      val = (addr + val - 1) & ~(val - 1);
      break;
    }

  return val;
}


static size_t
optimal_bucket_size (Elf32_Word *hashcodes, size_t maxcnt, int optlevel)
{
  size_t minsize;
  size_t maxsize;
  size_t bestsize;
  uint64_t bestcost;
  size_t size;
  uint32_t *counts;
  uint32_t *lengths;

  if (maxcnt == 0)
    return 0;

  
  if (optlevel <= 0)
    {
      minsize = maxcnt;
      maxsize = maxcnt + 10000 / maxcnt;
    }
  else
    {
      minsize = MAX (1, maxcnt / 4);
      maxsize = 2 * maxcnt + (6 * MIN (optlevel, 100) * maxcnt) / 100;
    }
  bestsize = maxcnt;
  bestcost = UINT_MAX;

  
  counts = (uint32_t *) xmalloc ((maxcnt + 1 + maxsize) * sizeof (uint32_t));
  lengths = &counts[maxcnt + 1];

  for (size = minsize; size <= maxsize; ++size)
    {
      size_t inner;
      uint64_t cost;
      uint32_t maxlength;
      uint64_t success;
      uint32_t acc;
      double factor;

      memset (lengths, '\0', size * sizeof (uint32_t));
      memset (counts, '\0', (maxcnt + 1) * sizeof (uint32_t));

      
      assert (hashcodes[0] == 0);
      for (inner = 1; inner < maxcnt; ++inner)
	++lengths[hashcodes[inner] % size];

      
      maxlength = 0;
      for (inner = 0; inner < size; ++inner)
	{
	  ++counts[lengths[inner]];

	  if (lengths[inner] > maxlength)
	    maxlength = lengths[inner];
	}

      
      acc = 0;
      success = 0;
      for (inner = 0; inner <= maxlength; ++inner)
	{
	  acc += inner;
	  success += counts[inner] * acc;
	}

      if (ld_state.is_system_library)
	factor = (1.0 * (double) success / (double) maxcnt
		  + 0.3 * (double) maxcnt / (double) size);
      else
	factor = (0.3 * (double) success / (double) maxcnt
		  + 1.0 * (double) maxcnt / (double) size);

      cost = (2 + maxcnt + size) * (factor + 1.0 / 16.0);

#if 0
      printf ("maxcnt = %d, size = %d, cost = %Ld, success = %g, fail = %g, factor = %g\n",
	      maxcnt, size, cost, (double) success / (double) maxcnt, (double) maxcnt / (double) size, factor);
#endif

      
      if (cost < bestcost)
	{
	  bestcost = cost;
	  bestsize = size;
	}
    }

  free (counts);

  return bestsize;
}


static void
optimal_gnu_hash_size (Elf32_Word *hashcodes, size_t maxcnt, int optlevel,
		       size_t *bitmask_nwords, size_t *shift, size_t *nbuckets)
{
  
  *bitmask_nwords = 256;
  *shift = 6;
  *nbuckets = 3 * maxcnt / 2;
}


static XElf_Addr
find_entry_point (void)
{
  XElf_Addr result;

  if (ld_state.entry != NULL)
    {
      struct symbol search = { .name = ld_state.entry };
      struct symbol *syment;

      syment = ld_symbol_tab_find (&ld_state.symbol_tab,
				   elf_hash (ld_state.entry), &search);
      if (syment != NULL && syment->defined)
	{
	  
	  Elf_Data *data = elf_getdata (elf_getscn (ld_state.outelf,
						    ld_state.symscnidx), NULL);

	  XElf_Sym_vardef (sym);

	  sym = NULL;
	  if (data != NULL)
	    xelf_getsym (data, ld_state.dblindirect[syment->outsymidx], sym);

	  if (sym == NULL && ld_state.need_dynsym && syment->outdynsymidx != 0)
	    {
	      
	      data = elf_getdata (elf_getscn (ld_state.outelf,
					      ld_state.dynsymscnidx), NULL);

	      sym = NULL;
	      if (data != NULL)
		xelf_getsym (data, syment->outdynsymidx, sym);
	    }

	  if (sym != NULL)
	    return sym->st_value;

	  assert (ld_state.need_symtab);
	  assert (ld_state.symscnidx != 0);
	}
    }



  result = 0;

  if (ld_state.file_type == executable_file_type)
    {
      if (ld_state.entry != NULL)
	error (0, 0, gettext ("\
cannot find entry symbol '%s': defaulting to %#0*" PRIx64),
	       ld_state.entry,
	       xelf_getclass (ld_state.outelf) == ELFCLASS32 ? 10 : 18,
	       (uint64_t) result);
      else
	error (0, 0, gettext ("\
no entry symbol specified: defaulting to %#0*" PRIx64),
	       xelf_getclass (ld_state.outelf) == ELFCLASS32 ? 10 : 18,
	       (uint64_t) result);
    }

  return result;
}


static void
fillin_special_symbol (struct symbol *symst, size_t scnidx, size_t nsym,
		       Elf_Data *symdata, struct Ebl_Strtab *strtab)
{
  assert (ld_state.file_type != relocatable_file_type);

  XElf_Sym_vardef (sym);
  xelf_getsym_ptr (symdata, nsym, sym);

  
  sym->st_name = 0;
  
  sym->st_info = XELF_ST_INFO (symst->local ? STB_LOCAL : STB_GLOBAL,
			       symst->type);
  sym->st_other = symst->hidden ? STV_HIDDEN : STV_DEFAULT;
  assert (scnidx != 0);
  assert (scnidx < SHN_LORESERVE || scnidx == SHN_ABS);
  sym->st_shndx = scnidx;
  
  sym->st_value = 0;
  
  sym->st_size = 0;

  
  if (scnidx != SHN_ABS)
    {
      Elf_Data *data = elf_getdata (elf_getscn (ld_state.outelf, scnidx),
				    NULL);
      assert (data != NULL);
      sym->st_size = data->d_size;
      
      assert (elf_getdata (elf_getscn (ld_state.outelf, scnidx), data)
	      == NULL);
    }

  (void) xelf_update_sym (symdata, nsym, sym);

  
  ndxtosym[nsym] = symst;
  symst->outsymidx = nsym;

  
  symstrent[nsym] = ebl_strtabadd (strtab, symst->name, 0);
}


static void
new_dynamic_entry (Elf_Data *data, int idx, XElf_Sxword tag, XElf_Addr val)
{
  XElf_Dyn_vardef (dyn);
  xelf_getdyn_ptr (data, idx, dyn);
  dyn->d_tag = tag;
  dyn->d_un.d_ptr = val;
  (void) xelf_update_dyn (data, idx, dyn);
}


static void
allocate_version_names (struct usedfiles *runp, struct Ebl_Strtab *dynstrtab)
{
  
  if (runp->status != opened || runp->verdefdata == NULL)
    return;

  
  int offset = 0;
  while (1)
    {
      XElf_Verdef_vardef (def);
      XElf_Verdaux_vardef (aux);

      
      xelf_getverdef (runp->verdefdata, offset, def);
      assert (def != NULL);
      xelf_getverdaux (runp->verdefdata, offset + def->vd_aux, aux);
      assert (aux != NULL);

      assert (def->vd_ndx <= runp->nverdef);
      if (def->vd_ndx == 1 || runp->verdefused[def->vd_ndx] != 0)
	{
	  runp->verdefent[def->vd_ndx]
	    = ebl_strtabadd (dynstrtab, elf_strptr (runp->elf,
						    runp->dynsymstridx,
						    aux->vda_name), 0);

	  if (def->vd_ndx > 1)
	    runp->verdefused[def->vd_ndx] = ld_state.nextveridx++;
	}

      if (def->vd_next == 0)
	
	break;

      offset += def->vd_next;
    }
}


static XElf_Off
create_verneed_data (XElf_Off offset, Elf_Data *verneeddata,
		     struct usedfiles *runp, int *ntotal)
{
  size_t verneed_size = xelf_fsize (ld_state.outelf, ELF_T_VNEED, 1);
  size_t vernaux_size = xelf_fsize (ld_state.outelf, ELF_T_VNAUX, 1);
  int need_offset;
  bool filled = false;
  GElf_Verneed verneed;
  GElf_Vernaux vernaux;
  int ndef = 0;
  size_t cnt;

  
  if (runp->nverdefused == 0)
    return offset;

  
  need_offset = offset;
  offset += verneed_size;

  for (cnt = 2; cnt <= runp->nverdef; ++cnt)
    if (runp->verdefused[cnt] != 0)
      {
	assert (runp->verdefent[cnt] != NULL);

	if (filled)
	  {
	    vernaux.vna_next = vernaux_size;
	    (void) gelf_update_vernaux (verneeddata, offset, &vernaux);
	    offset += vernaux_size;
	  }

	vernaux.vna_hash = elf_hash (ebl_string (runp->verdefent[cnt]));
	vernaux.vna_flags = 0;
	vernaux.vna_other = runp->verdefused[cnt];
	vernaux.vna_name = ebl_strtaboffset (runp->verdefent[cnt]);
	filled = true;
	++ndef;
      }

  assert (filled);
  vernaux.vna_next = 0;
  (void) gelf_update_vernaux (verneeddata, offset, &vernaux);
  offset += vernaux_size;

  verneed.vn_version = VER_NEED_CURRENT;
  verneed.vn_cnt = ndef;
  verneed.vn_file = ebl_strtaboffset (runp->verdefent[1]);
  verneed.vn_aux = verneed_size;
  verneed.vn_next = --*ntotal > 0 ? offset - need_offset : 0;
  (void) gelf_update_verneed (verneeddata, need_offset, &verneed);

  return offset;
}


static Elf32_Word *global_hashcodes;
static size_t global_nbuckets;
static int
sortfct_hashval (const void *p1, const void *p2)
{
  size_t idx1 = *(size_t *) p1;
  size_t idx2 = *(size_t *) p2;

  int def1 = ndxtosym[idx1]->defined && !ndxtosym[idx1]->in_dso;
  int def2 = ndxtosym[idx2]->defined && !ndxtosym[idx2]->in_dso;

  if (! def1 && def2)
    return -1;
  if (def1 && !def2)
    return 1;
  if (! def1)
    return 0;

  Elf32_Word hval1 = (global_hashcodes[ndxtosym[idx1]->outdynsymidx]
		      % global_nbuckets);
  Elf32_Word hval2 = (global_hashcodes[ndxtosym[idx2]->outdynsymidx]
		      % global_nbuckets);

  if (hval1 < hval2)
    return -1;
  if (hval1 > hval2)
    return 1;
  return 0;
}


static void
create_gnu_hash (size_t nsym_local, size_t nsym, size_t nsym_dyn,
		 Elf32_Word *gnuhashcodes)
{
  size_t gnu_bitmask_nwords = 0;
  size_t gnu_shift = 0;
  size_t gnu_nbuckets = 0;
  Elf32_Word *gnu_bitmask = NULL;
  Elf32_Word *gnu_buckets = NULL;
  Elf32_Word *gnu_chain = NULL;
  XElf_Shdr_vardef (shdr);

  
  optimal_gnu_hash_size (gnuhashcodes, nsym_dyn, ld_state.optlevel,
			 &gnu_bitmask_nwords, &gnu_shift, &gnu_nbuckets);

  
  Elf_Scn *hashscn = elf_getscn (ld_state.outelf, ld_state.gnuhashscnidx);
  xelf_getshdr (hashscn, shdr);
  Elf_Data *hashdata = elf_newdata (hashscn);
  if (shdr == NULL || hashdata == NULL)
    error (EXIT_FAILURE, 0, gettext ("\
cannot create GNU hash table section for output file: %s"),
	   elf_errmsg (-1));

  shdr->sh_link = ld_state.dynsymscnidx;
  (void) xelf_update_shdr (hashscn, shdr);

  hashdata->d_size = (xelf_fsize (ld_state.outelf, ELF_T_ADDR,
				  gnu_bitmask_nwords)
		      + (4 + gnu_nbuckets + nsym_dyn) * sizeof (Elf32_Word));
  hashdata->d_buf = xcalloc (1, hashdata->d_size);
  hashdata->d_align = sizeof (Elf32_Word);
  hashdata->d_type = ELF_T_WORD;
  hashdata->d_off = 0;

  ((Elf32_Word *) hashdata->d_buf)[0] = gnu_nbuckets;
  ((Elf32_Word *) hashdata->d_buf)[2] = gnu_bitmask_nwords;
  ((Elf32_Word *) hashdata->d_buf)[3] = gnu_shift;
  gnu_bitmask = &((Elf32_Word *) hashdata->d_buf)[4];
  gnu_buckets = &gnu_bitmask[xelf_fsize (ld_state.outelf, ELF_T_ADDR,
					 gnu_bitmask_nwords)
			     / sizeof (*gnu_buckets)];
  gnu_chain = &gnu_buckets[gnu_nbuckets];
#ifndef NDEBUG
  void *endp = &gnu_chain[nsym_dyn];
#endif
  assert (endp == (void *) ((char *) hashdata->d_buf + hashdata->d_size));


  size_t *remap = xmalloc (nsym_dyn * sizeof (size_t));
#ifndef NDEBUG
  size_t nsym_dyn_cnt = 1;
#endif
  for (size_t cnt = nsym_local; cnt < nsym; ++cnt)
    if (symstrent[cnt] != NULL)
      {
	assert (ndxtosym[cnt]->outdynsymidx > 0);
	assert (ndxtosym[cnt]->outdynsymidx < nsym_dyn);
	remap[ndxtosym[cnt]->outdynsymidx] = cnt;
#ifndef NDEBUG
	++nsym_dyn_cnt;
#endif
      }
  assert (nsym_dyn_cnt == nsym_dyn);

  
  global_hashcodes = gnuhashcodes;
  global_nbuckets = gnu_nbuckets;
  qsort (remap + 1, nsym_dyn - 1, sizeof (size_t), sortfct_hashval);

  bool bm32 = (xelf_fsize (ld_state.outelf, ELF_T_ADDR, 1)
	       ==  sizeof (Elf32_Word));

  size_t first_defined = 0;
  Elf64_Word bitmask_idxbits = gnu_bitmask_nwords - 1;
  Elf32_Word last_bucket = 0;
  for (size_t cnt = 1; cnt < nsym_dyn; ++cnt)
    {
      if (first_defined == 0)
	{
	  if (! ndxtosym[remap[cnt]]->defined
	      || ndxtosym[remap[cnt]]->in_dso)
	    goto next;

	  ((Elf32_Word *) hashdata->d_buf)[1] = first_defined = cnt;
	}

      Elf32_Word hval = gnuhashcodes[ndxtosym[remap[cnt]]->outdynsymidx];

      if (bm32)
	{
	  Elf32_Word *bsw = &gnu_bitmask[(hval / 32) & bitmask_idxbits];
	  assert ((void *) gnu_bitmask <= (void *) bsw);
	  assert ((void *) bsw < (void *) gnu_buckets);
	  *bsw |= 1 << (hval & 31);
	  *bsw |= 1 << ((hval >> gnu_shift) & 31);
	}
      else
	{
	  Elf64_Word *bsw = &((Elf64_Word *) gnu_bitmask)[(hval / 64)
							  & bitmask_idxbits];
	  assert ((void *) gnu_bitmask <= (void *) bsw);
	  assert ((void *) bsw < (void *) gnu_buckets);
	  *bsw |= 1 << (hval & 63);
	  *bsw |= 1 << ((hval >> gnu_shift) & 63);
	}

      size_t this_bucket = hval % gnu_nbuckets;
      if (cnt == first_defined || this_bucket != last_bucket)
	{
	  if (cnt != first_defined)
	    {
	      
	      assert ((void *) &gnu_chain[cnt - first_defined - 1] < endp);
	      gnu_chain[cnt - first_defined - 1] |= 1;
	    }

	  assert (this_bucket < gnu_nbuckets);
	  gnu_buckets[this_bucket] = cnt;
	  last_bucket = this_bucket;
	}

      assert (cnt >= first_defined);
      assert (cnt - first_defined < nsym_dyn);
      gnu_chain[cnt - first_defined] = hval & ~1u;

    next:
      ndxtosym[remap[cnt]]->outdynsymidx = cnt;
    }

  
  if (first_defined != 0)
    {
      assert (nsym_dyn > first_defined);
      assert (nsym_dyn - first_defined - 1 < nsym_dyn);
      gnu_chain[nsym_dyn - first_defined - 1] |= 1;

      hashdata->d_size -= first_defined * sizeof (Elf32_Word);
    }
  else
    
    
    do { } while (0);

  free (remap);
}


static void
create_hash (size_t nsym_local, size_t nsym, size_t nsym_dyn,
	     Elf32_Word *hashcodes)
{
  size_t nbucket = 0;
  Elf32_Word *bucket = NULL;
  Elf32_Word *chain = NULL;
  XElf_Shdr_vardef (shdr);

  if (GENERATE_GNU_HASH)
    nbucket = 1;
  else
    nbucket = optimal_bucket_size (hashcodes, nsym_dyn, ld_state.optlevel);
  
  Elf_Scn *hashscn = elf_getscn (ld_state.outelf, ld_state.hashscnidx);
  xelf_getshdr (hashscn, shdr);
  Elf_Data *hashdata = elf_newdata (hashscn);
  if (shdr == NULL || hashdata == NULL)
    error (EXIT_FAILURE, 0, gettext ("\
cannot create hash table section for output file: %s"),
	   elf_errmsg (-1));

  shdr->sh_link = ld_state.dynsymscnidx;
  (void) xelf_update_shdr (hashscn, shdr);

  hashdata->d_size = (2 + nsym_dyn + nbucket) * sizeof (Elf32_Word);
  hashdata->d_buf = xcalloc (1, hashdata->d_size);
  hashdata->d_align = sizeof (Elf32_Word);
  hashdata->d_type = ELF_T_WORD;
  hashdata->d_off = 0;

  ((Elf32_Word *) hashdata->d_buf)[0] = nbucket;
  ((Elf32_Word *) hashdata->d_buf)[1] = nsym_dyn;
  bucket = &((Elf32_Word *) hashdata->d_buf)[2];
  chain = &((Elf32_Word *) hashdata->d_buf)[2 + nbucket];

  for (size_t cnt = nsym_local; cnt < nsym; ++cnt)
    if (symstrent[cnt] != NULL)
      {
	size_t dynidx = ndxtosym[cnt]->outdynsymidx;
	size_t hashidx = hashcodes[dynidx] % nbucket;
	if (bucket[hashidx] == 0)
	  bucket[hashidx] = dynidx;
	else
	  {
	    hashidx = bucket[hashidx];
	    while (chain[hashidx] != 0)
	      hashidx = chain[hashidx];

	    chain[hashidx] = dynidx;
	  }
      }
}


static void
create_build_id_section (Elf_Scn *scn)
{
  
  Elf_Data *d = elf_newdata (scn);
  if (d == NULL)
    error (EXIT_FAILURE, 0, gettext ("cannot create build ID section: %s"),
	   elf_errmsg (-1));

  d->d_type = ELF_T_BYTE;
  d->d_version = EV_CURRENT;

  
  assert (sizeof (Elf32_Nhdr) == sizeof (Elf64_Nhdr));
  d->d_size = sizeof (GElf_Nhdr);
  
  d->d_size += sizeof (ELF_NOTE_GNU);
  assert (d->d_size % 4 == 0);

  if (strcmp (ld_state.build_id, "md5") == 0
      || strcmp (ld_state.build_id, "uuid") == 0)
    d->d_size += 16;
  else if (strcmp (ld_state.build_id, "sha1") == 0)
    d->d_size += 20;
  else
    {
      assert (ld_state.build_id[0] == '0' && ld_state.build_id[1] == 'x');
      d->d_size += strlen (ld_state.build_id) / 2;
    }

  d->d_buf = xcalloc (d->d_size, 1);
  d->d_off = 0;
  d->d_align = 0;
}


static void
compute_hash_sum (void (*hashfct) (const void *, size_t, void *), void *ctx)
{
  
  size_t shstrndx;
  (void) elf_getshdrstrndx (ld_state.outelf, &shstrndx);

  const char *ident = elf_getident (ld_state.outelf, NULL);
  bool same_byte_order = ((ident[EI_DATA] == ELFDATA2LSB
			   && __BYTE_ORDER == __LITTLE_ENDIAN)
			  || (ident[EI_DATA] == ELFDATA2MSB
			      && __BYTE_ORDER == __BIG_ENDIAN));

  
  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (ld_state.outelf, scn)) != NULL)
    {
      
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);
      assert (shdr != NULL);

      if (SECTION_STRIP_P (shdr, elf_strptr (ld_state.outelf, shstrndx,
					     shdr->sh_name), true))
	
	continue;

      
      if (shdr->sh_type == SHT_NOBITS)
	continue;

      
      Elf_Data *data = NULL;
      while ((data = INTUSE(elf_getdata) (scn, data)) != NULL)
	if (likely (same_byte_order) || data->d_type == ELF_T_BYTE)
	  hashfct (data->d_buf, data->d_size, ctx);
	else
	  {
	    
	    if (gelf_xlatetof (ld_state.outelf, data, data, ident[EI_DATA])
		== NULL)
	      error (EXIT_FAILURE, 0, gettext ("\
cannot convert section data to file format: %s"),
		     elf_errmsg (-1));

	    hashfct (data->d_buf, data->d_size, ctx);

	    
	    if (gelf_xlatetom (ld_state.outelf, data, data, ident[EI_DATA])
		== NULL)
	      error (EXIT_FAILURE, 0, gettext ("\
cannot convert section data to memory format: %s"),
		     elf_errmsg (-1));
	  }
    }
}


static void
compute_build_id (void)
{
  Elf_Data *d = elf_getdata (elf_getscn (ld_state.outelf,
					 ld_state.buildidscnidx), NULL);
  assert (d != NULL);

  GElf_Nhdr *hdr = d->d_buf;
  hdr->n_namesz = sizeof (ELF_NOTE_GNU);
  hdr->n_type = NT_GNU_BUILD_ID;
  char *dp = mempcpy (hdr + 1, ELF_NOTE_GNU, sizeof (ELF_NOTE_GNU));

  if (strcmp (ld_state.build_id, "sha1") == 0)
    {
      struct sha1_ctx ctx;
      sha1_init_ctx (&ctx);

      
      compute_hash_sum ((void (*) (const void *, size_t, void *)) sha1_process_bytes,
			&ctx);

      
      (void) sha1_finish_ctx (&ctx, dp);

      hdr->n_descsz = SHA1_DIGEST_SIZE;
    }
  else if (strcmp (ld_state.build_id, "md5") == 0)
    {
      struct md5_ctx ctx;
      md5_init_ctx (&ctx);

      
      compute_hash_sum ((void (*) (const void *, size_t, void *)) md5_process_bytes,
			&ctx);

      
      (void) md5_finish_ctx (&ctx, dp);

      hdr->n_descsz = MD5_DIGEST_SIZE;
    }
  else if (strcmp (ld_state.build_id, "uuid") == 0)
    {
      int fd = open ("/dev/urandom", O_RDONLY);
      if (fd == -1)
	error (EXIT_FAILURE, errno, gettext ("cannot open '%s'"),
	       "/dev/urandom");

      if (TEMP_FAILURE_RETRY (read (fd, dp, 16)) != 16)
	error (EXIT_FAILURE, 0, gettext ("cannot read enough data for UUID"));

      close (fd);

      hdr->n_descsz = 16;
    }
  else
    {
      const char *cp = ld_state.build_id + 2;

      do
	{
	  if (isxdigit (cp[0]))
	    {
	      char ch1 = tolower (cp[0]);
	      char ch2 = tolower (cp[1]);

	      *dp++ = (((isdigit (ch1) ? ch1 - '0' : ch1 - 'a' + 10) << 4)
		       | (isdigit (ch2) ? ch2 - '0' : ch2 - 'a' + 10));
	    }
	  else
	    ++cp;
	}
      while (*cp != '\0');
    }
}


/* Create the output file.

   For relocatable files what basically has to happen is that all
   sections from all input files are written into the output file.
   Sections with the same name are combined (offsets adjusted
   accordingly).  The symbol tables are combined in one single table.
   When stripping certain symbol table entries are omitted.

   For executables (shared or not) we have to create the program header,
   additional sections like the .interp, eventually (in addition) create
   a dynamic symbol table and a dynamic section.  Also the relocations
   have to be processed differently.  */
static int
ld_generic_create_outfile (struct ld_state *statep)
{
  struct scnlist
  {
    size_t scnidx;
    struct scninfo *scninfo;
    struct scnlist *next;
  };
  struct scnlist *rellist = NULL;
  size_t cnt;
  Elf_Scn *symscn = NULL;
  Elf_Scn *xndxscn = NULL;
  Elf_Scn *strscn = NULL;
  struct Ebl_Strtab *strtab = NULL;
  struct Ebl_Strtab *dynstrtab = NULL;
  XElf_Shdr_vardef (shdr);
  Elf_Data *data;
  Elf_Data *symdata = NULL;
  Elf_Data *xndxdata = NULL;
  struct usedfiles *file;
  size_t nsym;
  size_t nsym_local;
  size_t nsym_allocated;
  size_t nsym_dyn = 0;
  Elf32_Word *dblindirect = NULL;
#ifndef NDEBUG
  bool need_xndx;
#endif
  Elf_Scn *shstrtab_scn;
  size_t shstrtab_ndx;
  XElf_Ehdr_vardef (ehdr);
  struct Ebl_Strent *symtab_ent = NULL;
  struct Ebl_Strent *xndx_ent = NULL;
  struct Ebl_Strent *strtab_ent = NULL;
  struct Ebl_Strent *shstrtab_ent;
  struct scngroup *groups;
  Elf_Scn *dynsymscn = NULL;
  Elf_Data *dynsymdata = NULL;
  Elf_Data *dynstrdata = NULL;
  Elf32_Word *hashcodes = NULL;
  Elf32_Word *gnuhashcodes = NULL;
  size_t nsym_dyn_allocated = 0;
  Elf_Scn *versymscn = NULL;
  Elf_Data *versymdata = NULL;

  if (ld_state.need_symtab)
    {
      symscn = elf_newscn (ld_state.outelf);
      ld_state.symscnidx = elf_ndxscn (symscn);
      symdata = elf_newdata (symscn);
      if (symdata == NULL)
	error (EXIT_FAILURE, 0,
	       gettext ("cannot create symbol table for output file: %s"),
	       elf_errmsg (-1));

      symdata->d_type = ELF_T_SYM;
      nsym_allocated = (1 + ld_state.nsymtab + ld_state.nplt + ld_state.ngot
			+ ld_state.nusedsections + ld_state.nlscript_syms);
      symdata->d_size = xelf_fsize (ld_state.outelf, ELF_T_SYM,
				    nsym_allocated);

      
      
      if (unlikely (ld_state.nusedsections >= SHN_LORESERVE))
	{
	  xndxscn = elf_newscn (ld_state.outelf);
	  ld_state.xndxscnidx = elf_ndxscn (xndxscn);

	  xndxdata = elf_newdata (xndxscn);
	  if (xndxdata == NULL)
	    error (EXIT_FAILURE, 0,
		   gettext ("cannot create symbol table for output file: %s"),
		   elf_errmsg (-1));

	  xndxdata->d_type = ELF_T_WORD;
	  xndxdata->d_size = xelf_fsize (ld_state.outelf, ELF_T_WORD,
					 nsym_allocated);
	  
	  xndxdata->d_buf = memset (xmalloc (xndxdata->d_size), '\0',
				    xelf_fsize (ld_state.outelf, ELF_T_WORD,
						1));
	  xndxdata->d_off = 0;
	  
	  xndxdata->d_align = sizeof (Elf32_Word);
	}
    }
  else
    {
      assert (ld_state.need_dynsym);

      symscn = elf_getscn (ld_state.outelf, ld_state.dynsymscnidx);
      symdata = elf_newdata (symscn);
      if (symdata == NULL)
	error (EXIT_FAILURE, 0,
	       gettext ("cannot create symbol table for output file: %s"),
	       elf_errmsg (-1));

      symdata->d_version = EV_CURRENT;
      symdata->d_type = ELF_T_SYM;
      nsym_allocated = (1 + ld_state.nsymtab + ld_state.nplt + ld_state.ngot
			- ld_state.nlocalsymbols + ld_state.nlscript_syms);
      symdata->d_size = xelf_fsize (ld_state.outelf, ELF_T_SYM,
				    nsym_allocated);
    }

  
  symdata->d_buf = memset (xmalloc (symdata->d_size), '\0',
			   xelf_fsize (ld_state.outelf, ELF_T_SYM, 1));
  symdata->d_off = 0;
  
  symdata->d_align = xelf_fsize (ld_state.outelf, ELF_T_ADDR, 1);

  symstrent = (struct Ebl_Strent **) xcalloc (nsym_allocated,
					      sizeof (struct Ebl_Strent *));

  
  nsym = 1;

  
  for (cnt = 0; cnt < ld_state.nallsections; ++cnt)
    {
      struct scnhead *head = ld_state.allsections[cnt];
      Elf_Scn *scn;
      struct scninfo *runp;
      XElf_Off offset;
      Elf32_Word xndx;

      
      if (!head->used)
	continue;

      
      scn = elf_getscn (ld_state.outelf, head->scnidx);

      if (unlikely (head->kind == scn_dot_interp))
	{
	  Elf_Data *outdata = elf_newdata (scn);
	  if (outdata == NULL)
	    error (EXIT_FAILURE, 0,
		   gettext ("cannot create section for output file: %s"),
		   elf_errmsg (-1));

	  
	  const char *interp = ld_state.interp ?: "/lib/ld.so.1";

	  
	  outdata->d_buf = (void *) interp;
	  outdata->d_size = strlen (interp) + 1;
	  outdata->d_type = ELF_T_BYTE;
	  outdata->d_off = 0;
	  outdata->d_align = 1;
	  outdata->d_version = EV_CURRENT;

	  
	  ld_state.interpscnidx = head->scnidx;

	  continue;
	}

      if (unlikely (head->kind == scn_dot_got))
	{
	  
	  ld_state.gotscnidx = elf_ndxscn (scn);

	  
	  INITIALIZE_GOT (&ld_state, scn);

	  continue;
	}

      if (unlikely (head->kind == scn_dot_gotplt))
	{
	  
	  ld_state.gotpltscnidx = elf_ndxscn (scn);

	  
	  INITIALIZE_GOTPLT (&ld_state, scn);

	  continue;
	}

      if (unlikely (head->kind == scn_dot_dynrel))
	{
	  Elf_Data *outdata;

	  outdata = elf_newdata (scn);
	  if (outdata == NULL)
	    error (EXIT_FAILURE, 0,
		   gettext ("cannot create section for output file: %s"),
		   elf_errmsg (-1));

	  outdata->d_size = ld_state.relsize_total;
	  outdata->d_buf = xmalloc (outdata->d_size);
	  outdata->d_type = (REL_TYPE (&ld_state) == DT_REL
			     ? ELF_T_REL : ELF_T_RELA);
	  outdata->d_off = 0;
	  outdata->d_align = xelf_fsize (ld_state.outelf, ELF_T_ADDR, 1);

	  
	  ld_state.reldynscnidx = elf_ndxscn (scn);

	  continue;
	}

      if (unlikely (head->kind == scn_dot_dynamic))
	{
	  
	  Elf_Data *outdata;

	  
	  if (ld_state.dt_flags != 0)
	    ++ld_state.ndynamic;
	  if (ld_state.dt_flags_1 != 0)
	    ++ld_state.ndynamic;
	  if (ld_state.dt_feature_1 != 0)
	    ++ld_state.ndynamic;

	  outdata = elf_newdata (scn);
	  if (outdata == NULL)
	    error (EXIT_FAILURE, 0,
		   gettext ("cannot create section for output file: %s"),
		   elf_errmsg (-1));

	  
	  outdata->d_size = xelf_fsize (ld_state.outelf, ELF_T_DYN,
					ld_state.ndynamic);
	  outdata->d_buf = xcalloc (1, outdata->d_size);
	  outdata->d_type = ELF_T_DYN;
	  outdata->d_off = 0;
	  outdata->d_align = xelf_fsize (ld_state.outelf, ELF_T_ADDR, 1);

	  
	  ld_state.dynamicscnidx = elf_ndxscn (scn);

	  continue;
	}

      if (unlikely (head->kind == scn_dot_dynsym))
	{
	  
	  assert (ld_state.dynsymscnidx == elf_ndxscn (scn));

	  continue;
	}

      if (unlikely (head->kind == scn_dot_dynstr))
	{
	  
	  ld_state.dynstrscnidx = elf_ndxscn (scn);

	  
	  dynstrtab = ebl_strtabinit (true);

	  if (ld_state.ndsofiles > 0)
	    {
	      struct usedfiles *frunp = ld_state.dsofiles;

	      do
		if (! frunp->as_needed || frunp->used)
		  frunp->sonameent = ebl_strtabadd (dynstrtab, frunp->soname,
						    0);
	      while ((frunp = frunp->next) != ld_state.dsofiles);
	    }


	  if (ld_state.runpath != NULL || ld_state.rpath != NULL)
	    {
	      struct pathelement *startp;
	      struct pathelement *prunp;
	      int tag;
	      size_t len;
	      char *str;
	      char *cp;

	      if (ld_state.runpath != NULL)
		{
		  startp = ld_state.runpath;
		  tag = DT_RUNPATH;
		}
	      else
		{
		  startp = ld_state.rpath;
		  tag = DT_RPATH;
		}

	      
	      for (len = 0, prunp = startp; prunp != NULL; prunp = prunp->next)
		len += strlen (prunp->pname) + 1;

	      cp = str = (char *) obstack_alloc (&ld_state.smem, len);
	      
	      for (prunp = startp; prunp != NULL; prunp = prunp->next)
		{
		  cp = stpcpy (cp, prunp->pname);
		  *cp++ = ':';
		}
	      
	      cp[-1] = '\0';

	      ld_state.rxxpath_strent = ebl_strtabadd (dynstrtab, str, len);
	      ld_state.rxxpath_tag = tag;
	    }

	  continue;
	}

      if (unlikely (head->kind == scn_dot_hash))
	{
	  
	  ld_state.hashscnidx = elf_ndxscn (scn);

	  continue;
	}

      if (unlikely (head->kind == scn_dot_gnu_hash))
	{
	  
	  ld_state.gnuhashscnidx = elf_ndxscn (scn);

	  continue;
	}

      if (unlikely (head->kind == scn_dot_plt))
	{
	  
	  ld_state.pltscnidx = elf_ndxscn (scn);

	  
	  INITIALIZE_PLT (&ld_state, scn);

	  continue;
	}

      if (unlikely (head->kind == scn_dot_pltrel))
	{
	  
	  ld_state.pltrelscnidx = elf_ndxscn (scn);

	  
	  INITIALIZE_PLTREL (&ld_state, scn);

	  continue;
	}

      if (unlikely (head->kind == scn_dot_version))
	{
	  
	  ld_state.versymscnidx = elf_ndxscn (scn);

	  continue;
	}

      if (unlikely (head->kind == scn_dot_version_r))
	{
	  
	  ld_state.verneedscnidx = elf_ndxscn (scn);

	  continue;
	}

      if (unlikely (head->kind == scn_dot_note_gnu_build_id))
	{
	  
	  ld_state.buildidscnidx = elf_ndxscn (scn);

	  create_build_id_section (scn);

	  continue;
	}

      
      assert (head->kind == scn_normal);

      if (ld_state.need_symtab)
	{
	  
	  XElf_Sym_vardef (sym);

	  xelf_getsym_ptr (symdata, nsym, sym);

	  sym->st_name = 0;
	  sym->st_info = XELF_ST_INFO (STB_LOCAL, STT_SECTION);
	  sym->st_other = 0;
	  sym->st_value = 0;
	  sym->st_size = 0;
	  if (likely (head->scnidx < SHN_LORESERVE))
	    {
	      sym->st_shndx = head->scnidx;
	      xndx = 0;
	    }
	  else
	    {
	      sym->st_shndx = SHN_XINDEX;
	      xndx = head->scnidx;
	    }
	  /* Commit the change.  See the optimization above, this does
	     not change the symbol table entry.  But the extended
	     section index table entry is always written, if there is
	     such a table.  */
	  assert (nsym < nsym_allocated);
	  xelf_update_symshndx (symdata, xndxdata, nsym, sym, xndx, 0);

	  
	  head->scnsymidx = nsym++;
	}

      if (head->type == SHT_REL || head->type == SHT_RELA)
	{
	  if (ld_state.file_type == relocatable_file_type)
	    {
	      struct scnlist *newp;

	      newp = (struct scnlist *) alloca (sizeof (*newp));
	      newp->scnidx = head->scnidx;
	      newp->scninfo = head->last->next;
#ifndef NDEBUG
	      newp->next = NULL;
#endif
	      SNGL_LIST_PUSH (rellist, newp);
	    }
	  else
	    {
	      int type = head->type == SHT_REL ? ELF_T_REL : ELF_T_RELA;

	      data = elf_newdata (scn);
	      if (data == NULL)
		error (EXIT_FAILURE, 0,
		       gettext ("cannot create section for output file: %s"),
		       elf_errmsg (-1));

	      data->d_size = xelf_fsize (ld_state.outelf, type, head->relsize);
	      data->d_buf = xcalloc (data->d_size, 1);
	      data->d_type = type;
	      data->d_align = xelf_fsize (ld_state.outelf, ELF_T_ADDR, 1);
	      data->d_off = 0;

	      continue;
	    }
	}

      
      if (head->flags & SHF_MERGE)
	{
	  Elf_Data *outdata;

	  runp = head->last->next;
	  if (runp->symbols == NULL)
	    {
	      head->flags &= ~SHF_MERGE;
	      goto no_merge;
	    }
	  head->symbols = runp->symbols;

	  while ((runp = runp->next) != head->last->next)
	    {
	      if (runp->symbols == NULL)
		{
		  head->flags &= ~SHF_MERGE;
		  head->symbols = NULL;
		  goto no_merge;
		}

	      struct symbol *oldhead = head->symbols->next_in_scn;

	      head->symbols->next_in_scn = runp->symbols->next_in_scn;
	      runp->symbols->next_in_scn = oldhead;
	      head->symbols = runp->symbols;
	    }

	  
	  outdata = elf_newdata (scn);
	  if (outdata == NULL)
	    error (EXIT_FAILURE, 0,
		   gettext ("cannot create section for output file: %s"),
		   elf_errmsg (-1));

	  if (SCNINFO_SHDR (head->last->shdr).sh_entsize == 1
	      && (head->flags & SHF_STRINGS))
	    {
	      
	      struct Ebl_Strtab *mergestrtab;
	      struct symbol *symrunp;
	      Elf_Data *locsymdata = NULL;
	      Elf_Data *locdata = NULL;

	      mergestrtab = ebl_strtabinit (false);

	      symrunp = head->symbols->next_in_scn;
	      file = NULL;
	      do
		{
		  if (symrunp->file != file)
		    {
		      
		      file = symrunp->file;
		      
		      locsymdata = file->symtabdata;
		      
		      locdata = elf_rawdata (file->scninfo[symrunp->scndx].scn,
					     NULL);
		      assert (locdata != NULL);
		      file->scninfo[symrunp->scndx].outscnndx = head->scnidx;
		    }

		  XElf_Sym_vardef (sym);
		  xelf_getsym (locsymdata, symrunp->symidx, sym);
		  assert (sym != NULL);

		  symrunp->merge.handle
		    = ebl_strtabadd (mergestrtab,
				     (char *) locdata->d_buf + sym->st_value,
				     0);
		}
	      while ((symrunp = symrunp->next_in_scn)
		     != head->symbols->next_in_scn);

	      
	      ebl_strtabfinalize (mergestrtab, outdata);

	      
	      symrunp = runp->symbols;
	      do
		{
		  symrunp->merge.value
		    = ebl_strtaboffset (symrunp->merge.handle);
		  symrunp->merged = 1;
		}
	      while ((symrunp = symrunp->next_in_scn) != runp->symbols);

	      
	      ebl_strtabfree (mergestrtab);
	    }
	  else if (likely (SCNINFO_SHDR (head->last->shdr).sh_entsize
			   == sizeof (wchar_t))
		   && likely (head->flags & SHF_STRINGS))
	    {
	      
	      struct Ebl_WStrtab *mergestrtab;
	      struct symbol *symrunp;
	      Elf_Data *locsymdata = NULL;
	      Elf_Data *locdata = NULL;

	      mergestrtab = ebl_wstrtabinit (false);

	      symrunp = runp->symbols;
	      file = NULL;
	      do
		{
		  if (symrunp->file != file)
		    {
		      
		      file = symrunp->file;
		      
		      locsymdata = file->symtabdata;
		      
		      locdata = elf_rawdata (file->scninfo[symrunp->scndx].scn,
					     NULL);
		      assert (locdata != NULL);

		      file->scninfo[symrunp->scndx].outscnndx = head->scnidx;
		    }

		  XElf_Sym_vardef (sym);
		  xelf_getsym (locsymdata, symrunp->symidx, sym);
		  assert (sym != NULL);

		  symrunp->merge.handle
		    = ebl_wstrtabadd (mergestrtab,
				      (wchar_t *) ((char *) locdata->d_buf
						   + sym->st_value), 0);
		}
	      while ((symrunp = symrunp->next_in_scn) != runp->symbols);

	      
	      ebl_wstrtabfinalize (mergestrtab, outdata);

	      
	      symrunp = runp->symbols;
	      do
		{
		  symrunp->merge.value
		    = ebl_wstrtaboffset (symrunp->merge.handle);
		  symrunp->merged = 1;
		}
	      while ((symrunp = symrunp->next_in_scn) != runp->symbols);

	      
	      ebl_wstrtabfree (mergestrtab);
	    }
	  else
	    {
	      
	      struct Ebl_GStrtab *mergestrtab;
	      struct symbol *symrunp;
	      Elf_Data *locsymdata = NULL;
	      Elf_Data *locdata = NULL;
	      unsigned int len = (head->flags & SHF_STRINGS) ? 0 : 1;

	      mergestrtab
		= ebl_gstrtabinit (SCNINFO_SHDR (head->last->shdr).sh_entsize,
				   false);

	      symrunp = runp->symbols;
	      file = NULL;
	      do
		{
		  if (symrunp->file != file)
		    {
		      
		      file = symrunp->file;
		      
		      locsymdata = file->symtabdata;
		      
		      locdata = elf_rawdata (file->scninfo[symrunp->scndx].scn,
					     NULL);
		      assert (locdata != NULL);

		      file->scninfo[symrunp->scndx].outscnndx = head->scnidx;
		    }

		  XElf_Sym_vardef (sym);
		  xelf_getsym (locsymdata, symrunp->symidx, sym);
		  assert (sym != NULL);

		  symrunp->merge.handle
		    = ebl_gstrtabadd (mergestrtab,
				      (char *) locdata->d_buf + sym->st_value,
				      len);
		}
	      while ((symrunp = symrunp->next_in_scn) != runp->symbols);

	      
	      ebl_gstrtabfinalize (mergestrtab, outdata);

	      
	      symrunp = runp->symbols;
	      do
		{
		  symrunp->merge.value
		    = ebl_gstrtaboffset (symrunp->merge.handle);
		  symrunp->merged = 1;
		}
	      while ((symrunp = symrunp->next_in_scn) != runp->symbols);

	      
	      ebl_gstrtabfree (mergestrtab);
	    }
	}
      else
	{
	no_merge:
	  assert (head->scnidx == elf_ndxscn (scn));

	  runp = head->last->next;
	  offset = 0;
	  do
	    {
	      Elf_Data *outdata = elf_newdata (scn);
	      if (outdata == NULL)
		error (EXIT_FAILURE, 0,
		       gettext ("cannot create section for output file: %s"),
		       elf_errmsg (-1));

	      if (likely (runp->scn != NULL))
		{
		  data = elf_getdata (runp->scn, NULL);
		  assert (data != NULL);

		  
		  *outdata = *data;

		  assert (elf_getdata (runp->scn, data) == NULL);
		}
	      else
		{
		  
		  assert  (SCNINFO_SHDR (runp->shdr).sh_type == SHT_NOBITS);

		  outdata->d_buf = NULL;	
		  outdata->d_type = ELF_T_BYTE;
		  outdata->d_version = EV_CURRENT;
		  outdata->d_size = SCNINFO_SHDR (runp->shdr).sh_size;
		  outdata->d_align = SCNINFO_SHDR (runp->shdr).sh_addralign;
		}

	      XElf_Off align =  MAX (1, outdata->d_align);
	      assert (powerof2 (align));
	      offset = ((offset + align - 1) & ~(align - 1));

	      runp->offset = offset;
	      runp->outscnndx = head->scnidx;
	      runp->allsectionsidx = cnt;

	      outdata->d_off = offset;

	      offset += outdata->d_size;
	    }
	  while ((runp = runp->next) != head->last->next);

	  
	  if (ld_state.add_ld_comment
	      && head->flags == 0
	      && head->type == SHT_PROGBITS
	      && strcmp (head->name, ".comment") == 0
	      && head->entsize == 0)
	    {
	      Elf_Data *outdata = elf_newdata (scn);

	      if (outdata == NULL)
		error (EXIT_FAILURE, 0,
		       gettext ("cannot create section for output file: %s"),
		       elf_errmsg (-1));

	      outdata->d_buf = (void *) "\0ld (" PACKAGE_NAME ") " PACKAGE_VERSION;
	      outdata->d_size = strlen ((char *) outdata->d_buf + 1) + 2;
	      outdata->d_off = offset;
	      outdata->d_type = ELF_T_BYTE;
	      outdata->d_align = 1;
	    }
	}
    }

  
  strtab = ebl_strtabinit (true);
  if (strtab == NULL)
    error (EXIT_FAILURE, errno, gettext ("cannot create string table"));


#ifndef NDEBUG
  
  need_xndx = false;
#endif

  ndxtosym = (struct symbol **) xcalloc (nsym_allocated,
					 sizeof (struct symbol));

  
  if (ld_state.got_symbol != NULL)
    {
      assert (nsym < nsym_allocated);
      
      fillin_special_symbol (ld_state.got_symbol, ld_state.gotpltscnidx,
			     nsym++, symdata, strtab);
    }

  
  if (ld_state.dyn_symbol != NULL)
    {
      assert (nsym < nsym_allocated);
      fillin_special_symbol (ld_state.dyn_symbol, ld_state.dynamicscnidx,
			     nsym++, symdata, strtab);
    }

  if (ld_state.lscript_syms != NULL)
    {
      struct symbol *rsym = ld_state.lscript_syms;
      do
	{
	  assert (nsym < nsym_allocated);
	  fillin_special_symbol (rsym, SHN_ABS, nsym++, symdata, strtab);
	}
      while ((rsym = rsym->next) != NULL);
    }

  
  file = ld_state.relfiles->next;
  symdata = elf_getdata (elf_getscn (ld_state.outelf, ld_state.symscnidx),
			 NULL);

  do
    {
      size_t maxcnt;
      Elf_Data *insymdata;
      Elf_Data *inxndxdata;

      assert (ld_state.file_type != relocatable_file_type
	      || file->dynsymtabdata == NULL);

      insymdata = file->symtabdata;
      assert (insymdata != NULL);
      inxndxdata = file->xndxdata;

      maxcnt = file->nsymtab;

      file->symindirect = (Elf32_Word *) xcalloc (maxcnt, sizeof (Elf32_Word));

      for (cnt = ld_state.need_symtab ? 1 : file->nlocalsymbols; cnt < maxcnt;
	   ++cnt)
	{
	  XElf_Sym_vardef (sym);
	  Elf32_Word xndx;
	  struct symbol *defp = NULL;

	  xelf_getsymshndx (insymdata, inxndxdata, cnt, sym, xndx);
	  assert (sym != NULL);

	  if (unlikely (XELF_ST_TYPE (sym->st_info) == STT_SECTION))
	    {
	      
	      if (ld_state.need_symtab)
		{
		  if (sym->st_shndx != SHN_XINDEX)
		    xndx = sym->st_shndx;

		  assert (file->scninfo[xndx].allsectionsidx
			  < ld_state.nallsections);
		  file->symindirect[cnt] = ld_state.allsections[file->scninfo[xndx].allsectionsidx]->scnsymidx;
		}
	      continue;
	    }

	  if ((ld_state.strip >= strip_all || !ld_state.need_symtab)
	      
	      && XELF_ST_TYPE (sym->st_info) == STT_FILE)
	    continue;

#if NATIVE_ELF != 0
	  XElf_Sym sym_mem;
	  sym_mem = *sym;
	  sym = &sym_mem;
#endif

	  if (sym->st_shndx != SHN_UNDEF
	      && (sym->st_shndx < SHN_LORESERVE
		  || sym->st_shndx == SHN_XINDEX))
	    {
	      if (!ld_state.export_all_dynamic && !ld_state.need_symtab)
		{
		  assert (cnt >= file->nlocalsymbols);
		  defp = file->symref[cnt];
		  assert (defp != NULL);

		  if (!defp->in_dso)
		    
		    continue;
		}

	      if (sym->st_shndx != SHN_XINDEX)
		xndx = sym->st_shndx;

	      sym->st_value += file->scninfo[xndx].offset;

	      assert (file->scninfo[xndx].outscnndx < SHN_LORESERVE
		      || file->scninfo[xndx].outscnndx > SHN_HIRESERVE);
	      if (unlikely (file->scninfo[xndx].outscnndx > SHN_LORESERVE))
		{
		  if (!ld_state.need_symtab)
		    error (EXIT_FAILURE, 0, gettext ("\
section index too large in dynamic symbol table"));

		  assert (xndxdata != NULL);
		  sym->st_shndx = SHN_XINDEX;
		  xndx = file->scninfo[xndx].outscnndx;
#ifndef NDEBUG
		  need_xndx = true;
#endif
		}
	      else
		{
		  sym->st_shndx = file->scninfo[xndx].outscnndx;
		  xndx = 0;
		}
	    }
	  else if (sym->st_shndx == SHN_COMMON || sym->st_shndx == SHN_UNDEF)
	    {
	      assert (cnt >= file->nlocalsymbols);
	      defp = file->symref[cnt];
	      assert (defp != NULL);

	      assert (sym->st_shndx != SHN_COMMON || defp->defined);

	      if ((sym->st_shndx == SHN_COMMON && !defp->common)
		  || (sym->st_shndx == SHN_UNDEF && defp->defined)
		  || defp->added)
		continue;

	      
	      defp->added = 1;

	      
	      if (sym->st_shndx == SHN_COMMON)
		{
		  sym->st_value = (ld_state.common_section->offset
				   + file->symref[cnt]->merge.value);
		  assert (ld_state.common_section->outscnndx < SHN_LORESERVE);
		  sym->st_shndx = ld_state.common_section->outscnndx;
		  xndx = 0;
		}
	    }
	  else if (unlikely (sym->st_shndx != SHN_ABS))
	    {
	      if (SPECIAL_SECTION_NUMBER_P (&ld_state, sym->st_shndx))
		abort ();
	    }

	  if (sym->st_name != 0
	      && (ld_state.strip < strip_everything
		  || XELF_ST_BIND (sym->st_info) != STB_LOCAL))
	    symstrent[nsym] = ebl_strtabadd (strtab,
					     elf_strptr (file->elf,
							 file->symstridx,
							 sym->st_name), 0);

	  GElf_Word st_name = sym->st_name;
	  sym->st_name = 0;

	  if (file->has_merge_sections && file->symref[cnt] != NULL
	      && file->symref[cnt]->merged)
	    sym->st_value = file->symref[cnt]->merge.value;

	  
	  assert (nsym < nsym_allocated);
	  xelf_update_symshndx (symdata, xndxdata, nsym, sym, xndx, 1);

	  if (defp == NULL && cnt >= file->nlocalsymbols)
	    {
	      defp = file->symref[cnt];

	      if (defp == NULL)
		{
		  
		  
		  
		  struct symbol search;
		  search.name = elf_strptr (file->elf, file->symstridx,
					    st_name);
		  struct symbol *realp
		    = ld_symbol_tab_find (&ld_state.symbol_tab,
					  elf_hash (search.name), &search);
		  if (realp == NULL)
		    
		    error (EXIT_FAILURE, 0,
			   "couldn't find symbol from COMDAT section");

		  file->symref[cnt] = realp;

		  continue;
		}
	    }

	  ndxtosym[nsym] = defp;

	  
	  if (cnt >= file->nlocalsymbols)
	    {
	      assert (file->symref[cnt]->outsymidx == 0);
	      file->symref[cnt]->outsymidx = nsym;
	    }
	  file->symindirect[cnt] = nsym++;
	}
    }
  while ((file = file->next) != ld_state.relfiles->next);
  assert (xndxdata == NULL || need_xndx);

  
  if (ld_state.verneedscnidx != 0)
    {
      struct usedfiles *runp;

      runp = ld_state.dsofiles->next;
      do
	allocate_version_names (runp, dynstrtab);
      while ((runp = runp->next) != ld_state.dsofiles->next);

      if (ld_state.needed != NULL)
	{
	  runp = ld_state.needed->next;
	  do
	    allocate_version_names (runp, dynstrtab);
	  while ((runp = runp->next) != ld_state.needed->next);
	}
    }

  
  if (ld_state.default_bind_local || ld_state.version_str_tab.filled > 0)
    {
    
      bool any_reduced = false;

      for (cnt = 1; cnt < nsym; ++cnt)
	{
	  XElf_Sym_vardef (sym);

	  xelf_getsym (symdata, cnt, sym);
	  assert (sym != NULL);

	  if (reduce_symbol_p (sym, symstrent[cnt]))
	    {
	      
	      assert (ndxtosym[cnt]->outdynsymidx != 0);
	      ndxtosym[cnt]->outdynsymidx = 0;

	      sym->st_info = XELF_ST_INFO (STB_LOCAL,
					   XELF_ST_TYPE (sym->st_info));
	      (void) xelf_update_sym (symdata, cnt, sym);

	      
	      if (ld_state.strip == strip_everything)
		{
		  symstrent[cnt] = NULL;
		  any_reduced = true;
		}
	    }
	}

      if (unlikely (any_reduced))
	{
	  struct Ebl_Strtab *newp = ebl_strtabinit (true);

	  for (cnt = 1; cnt < nsym; ++cnt)
	    if (symstrent[cnt] != NULL)
	      symstrent[cnt] = ebl_strtabadd (newp,
					      ebl_string (symstrent[cnt]), 0);

	  ebl_strtabfree (strtab);
	  strtab = newp;
	}
    }

  if (ld_state.from_dso != NULL)
    {
      struct symbol *runp;
      size_t plt_base = nsym + ld_state.nfrom_dso - ld_state.nplt;
      size_t plt_idx = 0;
      size_t obj_idx = 0;

      assert (ld_state.nfrom_dso >= ld_state.nplt);
      runp = ld_state.from_dso;
      do
	{
	  
	  
	  size_t idx;
	  if (runp->type == STT_FUNC)
	    {
	      
	      runp->merge.value = plt_idx + 1;
	      idx = plt_base + plt_idx++;
	    }
	  else
	    idx = nsym + obj_idx++;

	  XElf_Sym_vardef (sym);
	  xelf_getsym_ptr (symdata, idx, sym);

	  sym->st_value = 0;
	  sym->st_size = runp->size;
	  sym->st_info = XELF_ST_INFO (runp->weak ? STB_WEAK : STB_GLOBAL,
				       runp->type);
	  sym->st_other = STV_DEFAULT;
	  sym->st_shndx = SHN_UNDEF;

	  
	  xelf_update_symshndx (symdata, xndxdata, idx, sym, 0, 0);

	  const char *name = runp->name;
	  size_t namelen = 0;

	  if (runp->file->verdefdata != NULL)
	    {
	      
	      XElf_Versym versym;

	      (void) xelf_getversym_copy (runp->file->versymdata, runp->symidx,
					  versym);

	      
	      assert ((versym & 0x8000) == 0);

	      const char *versname
		= ebl_string (runp->file->verdefent[versym]);

	      size_t versname_len = strlen (versname) + 1;
	      namelen = strlen (name) + versname_len + 2;
	      char *newp = (char *) obstack_alloc (&ld_state.smem, namelen);
	      memcpy (stpcpy (stpcpy (newp, name), "@@"),
		      versname, versname_len);
	      name = newp;
	    }

	  symstrent[idx] = ebl_strtabadd (strtab, name, namelen);

	  
	  runp->outsymidx = idx;

	  
	  ndxtosym[idx] = runp;
	}
      while ((runp = runp->next) != ld_state.from_dso);

      assert (nsym + obj_idx == plt_base);
      assert (plt_idx == ld_state.nplt);
      nsym = plt_base + plt_idx;
    }

  symdata->d_size = xelf_fsize (ld_state.outelf, ELF_T_SYM, nsym);
  if (unlikely (xndxdata != NULL))
    xndxdata->d_size = xelf_fsize (ld_state.outelf, ELF_T_WORD, nsym);

  
  strscn = elf_newscn (ld_state.outelf);
  ld_state.strscnidx = elf_ndxscn (strscn);
  data = elf_newdata (strscn);
  xelf_getshdr (strscn, shdr);
  if (data == NULL || shdr == NULL)
    error (EXIT_FAILURE, 0,
	   gettext ("cannot create section for output file: %s"),
	   elf_errmsg (-1));

  ebl_strtabfinalize (strtab, data);

  shdr->sh_type = SHT_STRTAB;
  assert (shdr->sh_entsize == 0);

  if (unlikely (xelf_update_shdr (strscn, shdr) == 0))
    error (EXIT_FAILURE, 0,
	   gettext ("cannot create section for output file: %s"),
	   elf_errmsg (-1));

  
  for (cnt = 1; cnt < nsym; ++cnt)
    if (symstrent[cnt] != NULL)
      {
	XElf_Sym_vardef (sym);

	xelf_getsym (symdata, cnt, sym);
	
	assert (sym != NULL);
	sym->st_name = ebl_strtaboffset (symstrent[cnt]);
	(void) xelf_update_sym (symdata, cnt, sym);
      }

  ld_state.dblindirect = dblindirect
    = (Elf32_Word *) xmalloc (nsym * sizeof (Elf32_Word));

  
  nsym_local = 1;
  cnt = nsym - 1;
  while (nsym_local < cnt)
    {
      XElf_Sym_vardef (locsym);
      Elf32_Word locxndx;
      XElf_Sym_vardef (globsym);
      Elf32_Word globxndx;

      do
	{
	  xelf_getsymshndx (symdata, xndxdata, nsym_local, locsym, locxndx);
	  
	  assert (locsym != NULL);

	  if (XELF_ST_BIND (locsym->st_info) != STB_LOCAL
	      && (ld_state.need_symtab || ld_state.export_all_dynamic))
	    {
	      do
		{
		  xelf_getsymshndx (symdata, xndxdata, cnt, globsym, globxndx);
		  
		  assert (globsym != NULL);

		  if (unlikely (XELF_ST_BIND (globsym->st_info) == STB_LOCAL))
		    {
		      
#if NATIVE_ELF != 0
		      XElf_Sym locsym_copy = *locsym;
		      locsym = &locsym_copy;
#endif
		      xelf_update_symshndx (symdata, xndxdata, nsym_local,
					    globsym, globxndx, 1);
		      xelf_update_symshndx (symdata, xndxdata, cnt,
					    locsym, locxndx, 1);

		      
		      dblindirect[nsym_local] = cnt;
		      dblindirect[cnt] = nsym_local;

		      
		      struct Ebl_Strent *strtmp = symstrent[nsym_local];
		      symstrent[nsym_local] = symstrent[cnt];
		      symstrent[cnt] = strtmp;

		      struct symbol *symtmp = ndxtosym[nsym_local];
		      ndxtosym[nsym_local] = ndxtosym[cnt];
		      ndxtosym[cnt] = symtmp;

		      
		      ++nsym_local;
		      --cnt;

		      break;
		    }

		  dblindirect[cnt] = cnt;
		}
	      while (nsym_local < --cnt);

	      break;
	    }

	  dblindirect[nsym_local] = nsym_local;
	}
      while (++nsym_local < cnt);
    }

  if (likely (nsym_local < nsym))
    {
      XElf_Sym_vardef (locsym);

      
      dblindirect[nsym_local] = nsym_local;

      
      xelf_getsym (symdata, nsym_local, locsym);
      
      assert (locsym != NULL);

      if (XELF_ST_BIND (locsym->st_info) == STB_LOCAL)
	++nsym_local;
    }


  if (ld_state.versymscnidx != 0)
    {
      versymscn = elf_getscn (ld_state.outelf, ld_state.versymscnidx);
      versymdata = elf_newdata (versymscn);
      if (versymdata == NULL)
	error (EXIT_FAILURE, 0,
	       gettext ("cannot create versioning section: %s"),
	       elf_errmsg (-1));

      versymdata->d_size = xelf_fsize (ld_state.outelf, ELF_T_HALF,
				       nsym - nsym_local + 1);
      versymdata->d_buf = xcalloc (1, versymdata->d_size);
      versymdata->d_align = xelf_fsize (ld_state.outelf, ELF_T_HALF, 1);
      versymdata->d_off = 0;
      versymdata->d_type = ELF_T_HALF;
    }


  if (unlikely (!ld_state.need_symtab))
    {
      size_t reduce = xelf_fsize (ld_state.outelf, ELF_T_SYM, nsym_local - 1);

      XElf_Sym_vardef (nullsym);
      xelf_getsym_ptr (symdata, nsym_local - 1, nullsym);

      (void) xelf_update_sym (symdata, nsym_local - 1,
			      memset (nullsym, '\0', sizeof (*nullsym)));

      
      symdata->d_buf = (char *) symdata->d_buf + reduce;
      symdata->d_size -= reduce;

      
      if (versymdata != NULL)
	{
	  nsym_dyn = 1;
	  for (cnt = nsym_local; cnt < nsym; ++cnt, ++nsym_dyn)
	    {
	      struct symbol *symp = ndxtosym[cnt];

	      if (symp->file->versymdata != NULL)
		{
		  GElf_Versym versym;

		  gelf_getversym (symp->file->versymdata, symp->symidx,
				  &versym);

		  (void) gelf_update_versym (versymdata, symp->outdynsymidx,
					     &symp->file->verdefused[versym]);
		}
	      }
	}

      nsym_dyn = nsym - nsym_local + 1;

      
      abort ();
    }
  else if (ld_state.need_dynsym)
    {
      dynsymscn = elf_getscn (ld_state.outelf, ld_state.dynsymscnidx);
      dynsymdata = elf_newdata (dynsymscn);

      dynstrdata = elf_newdata (elf_getscn (ld_state.outelf,
					    ld_state.dynstrscnidx));
      if (dynsymdata == NULL || dynstrdata == NULL)
	error (EXIT_FAILURE, 0, gettext ("\
cannot create dynamic symbol table for output file: %s"),
	       elf_errmsg (-1));

      nsym_dyn_allocated = nsym - nsym_local + 1;
      dynsymdata->d_size = xelf_fsize (ld_state.outelf, ELF_T_SYM,
				       nsym_dyn_allocated);
      dynsymdata->d_buf = memset (xmalloc (dynsymdata->d_size), '\0',
				  xelf_fsize (ld_state.outelf, ELF_T_SYM, 1));
      dynsymdata->d_type = ELF_T_SYM;
      dynsymdata->d_off = 0;
      dynsymdata->d_align = xelf_fsize (ld_state.outelf, ELF_T_ADDR, 1);

      hashcodes = (Elf32_Word *) xcalloc (__builtin_popcount ((int) ld_state.hash_style)
					  * nsym_dyn_allocated,
					  sizeof (Elf32_Word));
      gnuhashcodes = hashcodes;
      if (GENERATE_SYSV_HASH)
	gnuhashcodes += nsym_dyn_allocated;

      
      nsym_dyn = 1;

      
      for (cnt = nsym_local; cnt < nsym; ++cnt)
	{
	  XElf_Sym_vardef (sym);

	  xelf_getsym (symdata, cnt, sym);
	  assert (sym != NULL);

	  if (sym->st_shndx == SHN_XINDEX)
	    error (EXIT_FAILURE, 0, gettext ("\
section index too large in dynamic symbol table"));

	  if (XELF_ST_TYPE (sym->st_info) == STT_FILE
	      || XELF_ST_VISIBILITY (sym->st_other) == STV_INTERNAL
	      || XELF_ST_VISIBILITY (sym->st_other) == STV_HIDDEN
	      || (!ld_state.export_all_dynamic
		  && !ndxtosym[cnt]->in_dso && ndxtosym[cnt]->defined))
	    {
	      symstrent[cnt] = NULL;
	      continue;
	    }

	  ndxtosym[cnt]->outdynsymidx = nsym_dyn;

	  
	  const char *str = ndxtosym[cnt]->name;
	  symstrent[cnt] = ebl_strtabadd (dynstrtab, str, 0);
	  if (GENERATE_SYSV_HASH)
	    hashcodes[nsym_dyn] = elf_hash (str);
	  if (GENERATE_GNU_HASH)
	    gnuhashcodes[nsym_dyn] = elf_gnu_hash (str);
	  ++nsym_dyn;
	}

      if (ld_state.file_type != relocatable_file_type)
	{
	  
	  ebl_strtabfinalize (dynstrtab, dynstrdata);

	  assert (ld_state.hashscnidx != 0 || ld_state.gnuhashscnidx != 0);

	  
	  if (GENERATE_GNU_HASH)
	    create_gnu_hash (nsym_local, nsym, nsym_dyn, gnuhashcodes);

	  if (GENERATE_SYSV_HASH)
	    create_hash (nsym_local, nsym, nsym_dyn, hashcodes);
	}

	  
      if (versymdata != NULL)
	for (cnt = nsym_local; cnt < nsym; ++cnt)
	  if (symstrent[cnt] != NULL)
	    {
	      struct symbol *symp = ndxtosym[cnt];

	      if (symp->file != NULL && symp->file->verdefdata != NULL)
		{
		  GElf_Versym versym;

		  gelf_getversym (symp->file->versymdata, symp->symidx,
				  &versym);

		  (void) gelf_update_versym (versymdata, symp->outdynsymidx,
					     &symp->file->verdefused[versym]);
		}
	      else
		{
		  
		  GElf_Versym global = VER_NDX_GLOBAL;
		  (void) gelf_update_versym (versymdata, nsym_dyn, &global);
		}
	    }

      
      if (versymdata != NULL)
	{
	  versymdata->d_size = xelf_fsize (ld_state.outelf, ELF_T_HALF,
					   nsym_dyn);

	  
	  xelf_getshdr (versymscn, shdr);
	  assert (shdr != NULL);

	  shdr->sh_link = ld_state.dynsymscnidx;

	  (void) xelf_update_shdr (versymscn, shdr);
	}
    }

  if (ld_state.file_type != relocatable_file_type)
    {
      
      for (cnt = nsym_local; cnt < nsym; ++cnt)
	if (symstrent[cnt] != NULL)
	  {
	    XElf_Sym_vardef (sym);
	    size_t dynidx = ndxtosym[cnt]->outdynsymidx;

#if NATIVE_ELF != 0
	    XElf_Sym *osym;
	    memcpy (xelf_getsym (dynsymdata, dynidx, sym),
		    xelf_getsym (symdata, cnt, osym),
		    sizeof (XElf_Sym));
#else
	    xelf_getsym (symdata, cnt, sym);
	    assert (sym != NULL);
#endif

	    sym->st_name = ebl_strtaboffset (symstrent[cnt]);

	    (void) xelf_update_sym (dynsymdata, dynidx, sym);
	  }

      free (hashcodes);

      
      if (ld_state.verneedscnidx != 0)
	{
	  Elf_Scn *verneedscn;
	  Elf_Data *verneeddata;
	  struct usedfiles *runp;
	  size_t verneed_size = xelf_fsize (ld_state.outelf, ELF_T_VNEED, 1);
	  size_t vernaux_size = xelf_fsize (ld_state.outelf, ELF_T_VNAUX, 1);
	  size_t offset;
	  int ntotal;

	  verneedscn = elf_getscn (ld_state.outelf, ld_state.verneedscnidx);
	  xelf_getshdr (verneedscn, shdr);
	  verneeddata = elf_newdata (verneedscn);
	  if (shdr == NULL || verneeddata == NULL)
	    error (EXIT_FAILURE, 0,
		   gettext ("cannot create versioning data: %s"),
		   elf_errmsg (-1));

	  verneeddata->d_size = (ld_state.nverdeffile * verneed_size
				 + ld_state.nverdefused * vernaux_size);
	  verneeddata->d_buf = xmalloc (verneeddata->d_size);
	  verneeddata->d_type = ELF_T_VNEED;
	  verneeddata->d_align = xelf_fsize (ld_state.outelf, ELF_T_WORD, 1);
	  verneeddata->d_off = 0;

	  offset = 0;
	  ntotal = ld_state.nverdeffile;
	  runp = ld_state.dsofiles->next;
	  do
	    {
	      offset = create_verneed_data (offset, verneeddata, runp,
					    &ntotal);
	      runp = runp->next;
	    }
	  while (ntotal > 0 && runp != ld_state.dsofiles->next);

	  if (ntotal > 0)
	    {
	      runp = ld_state.needed->next;
	      do
		{
		  offset = create_verneed_data (offset, verneeddata, runp,
						&ntotal);
		  runp = runp->next;
		}
	      while (ntotal > 0 && runp != ld_state.needed->next);
	    }

	  assert (offset == verneeddata->d_size);

	  
	  shdr->sh_link = ld_state.dynstrscnidx;
	  shdr->sh_info = ld_state.nverdeffile;
	  (void) xelf_update_shdr (verneedscn, shdr);
	}

      
      dynsymdata->d_size = xelf_fsize (ld_state.outelf, ELF_T_SYM, nsym_dyn);
      if (versymdata != NULL)
	versymdata->d_size = xelf_fsize (ld_state.outelf, ELF_T_HALF,
					 nsym_dyn);

      
      xelf_getshdr (dynsymscn, shdr);
      
      shdr->sh_info = 1;
      
      shdr->sh_link = ld_state.dynstrscnidx;
      
      (void) xelf_update_shdr (dynsymscn, shdr);
    }

  
  free (symstrent);

  
  ld_state.ndynsym = nsym_dyn;

  
  symscn = elf_getscn (ld_state.outelf, ld_state.symscnidx);
  xelf_getshdr (symscn, shdr);
  if (shdr == NULL)
    error (EXIT_FAILURE, 0,
	   gettext ("cannot create symbol table for output file: %s"),
	   elf_errmsg (-1));

  shdr->sh_type = SHT_SYMTAB;
  shdr->sh_link = ld_state.strscnidx;
  shdr->sh_info = nsym_local;
  shdr->sh_entsize = xelf_fsize (ld_state.outelf, ELF_T_SYM, 1);

  (void) xelf_update_shdr (symscn, shdr);


  
  if (ld_state.symscnidx != 0)
      symtab_ent = ebl_strtabadd (ld_state.shstrtab, ".symtab", 8);
  if (ld_state.xndxscnidx != 0)
    xndx_ent = ebl_strtabadd (ld_state.shstrtab, ".symtab_shndx", 14);
  if (ld_state.strscnidx != 0)
    strtab_ent = ebl_strtabadd (ld_state.shstrtab, ".strtab", 8);


  
  shstrtab_scn = elf_newscn (ld_state.outelf);
  shstrtab_ndx = elf_ndxscn (shstrtab_scn);
  if (unlikely (shstrtab_ndx == SHN_UNDEF))
    error (EXIT_FAILURE, 0,
	   gettext ("cannot create section header string section: %s"),
	   elf_errmsg (-1));

  
  shstrtab_ent = ebl_strtabadd (ld_state.shstrtab, ".shstrtab", 10);
  if (unlikely (shstrtab_ent == NULL))
    error (EXIT_FAILURE, errno,
	   gettext ("cannot create section header string section"));

  
  data = elf_newdata (shstrtab_scn);
  if (data == NULL)
    error (EXIT_FAILURE, 0,
	   gettext ("cannot create section header string section: %s"),
	   elf_errmsg (-1));
  ebl_strtabfinalize (ld_state.shstrtab, data);

  
  for (cnt = 0; cnt < ld_state.nallsections; ++cnt)
    if (ld_state.allsections[cnt]->scnidx != 0)
      {
	Elf_Scn *scn;

	scn = elf_getscn (ld_state.outelf, ld_state.allsections[cnt]->scnidx);

	xelf_getshdr (scn, shdr);
	assert (shdr != NULL);

	shdr->sh_name = ebl_strtaboffset (ld_state.allsections[cnt]->nameent);

	if (xelf_update_shdr (scn, shdr) == 0)
	  assert (0);
      }

  if (symtab_ent != NULL)
    {
      Elf_Scn *scn = elf_getscn (ld_state.outelf, ld_state.symscnidx);

      xelf_getshdr (scn, shdr);
      
      assert (shdr != NULL);

      shdr->sh_name = ebl_strtaboffset (symtab_ent);

      (void) xelf_update_shdr (scn, shdr);
    }
  if (xndx_ent != NULL)
    {
      Elf_Scn *scn = elf_getscn (ld_state.outelf, ld_state.xndxscnidx);

      xelf_getshdr (scn, shdr);
      
      assert (shdr != NULL);

      shdr->sh_name = ebl_strtaboffset (xndx_ent);

      (void) xelf_update_shdr (scn, shdr);
    }
  if (strtab_ent != NULL)
    {
      Elf_Scn *scn = elf_getscn (ld_state.outelf, ld_state.strscnidx);

      xelf_getshdr (scn, shdr);
      
      assert (shdr != NULL);

      shdr->sh_name = ebl_strtaboffset (strtab_ent);

      (void) xelf_update_shdr (scn, shdr);
    }

  
  xelf_getshdr (shstrtab_scn, shdr);
  if (shdr == NULL)
    error (EXIT_FAILURE, 0,
	   gettext ("cannot create section header string section: %s"),
	   elf_errmsg (-1));

  shdr->sh_name = ebl_strtaboffset (shstrtab_ent);
  shdr->sh_type = SHT_STRTAB;

  if (unlikely (xelf_update_shdr (shstrtab_scn, shdr) == 0))
    error (EXIT_FAILURE, 0,
	   gettext ("cannot create section header string section: %s"),
	   elf_errmsg (-1));


  
  groups = ld_state.groups;
  while (groups != NULL)
    {
      Elf_Scn *scn = elf_getscn (ld_state.outelf, groups->outscnidx);
      xelf_getshdr (scn, shdr);
      assert (shdr != NULL);

      shdr->sh_name = ebl_strtaboffset (groups->nameent);
      shdr->sh_type = SHT_GROUP;
      shdr->sh_flags = 0;
      shdr->sh_link = ld_state.symscnidx;
      shdr->sh_entsize = sizeof (Elf32_Word);

      
      Elf32_Word si
	= groups->symbol->file->symindirect[groups->symbol->symidx];
      if (si == 0)
	{
	  assert (groups->symbol->file->symref[groups->symbol->symidx]
		  != NULL);
	  si = groups->symbol->file->symref[groups->symbol->symidx]->outsymidx;
	  assert (si != 0);
	}
      shdr->sh_info = ld_state.dblindirect[si];

      (void) xelf_update_shdr (scn, shdr);

      struct scngroup *oldp = groups;
      groups = groups->next;
      free (oldp);
    }


  if (ld_state.file_type != relocatable_file_type)
    {
      size_t nphdr = 0;

      
      ++nphdr;

      struct output_segment *segment = ld_state.output_segments;
      while (segment != NULL)
	{
	  ++nphdr;
	  segment = segment->next;
	}

      
      nphdr += ld_state.nnotesections;

      if (dynamically_linked_p ())
	{
	  ++nphdr;

	  if (ld_state.interp != NULL || ld_state.file_type != dso_file_type)
	    nphdr += 2;
	}

      
      if (ld_state.need_tls)
	++nphdr;

      
      XElf_Phdr_vardef (phdr);
      if (xelf_newphdr (ld_state.outelf, nphdr) == 0)
	error (EXIT_FAILURE, 0, gettext ("cannot create program header: %s"),
	       elf_errmsg (-1));


      if (elf_update (ld_state.outelf, ELF_C_NULL) == -1)
	error (EXIT_FAILURE, 0, gettext ("while determining file layout: %s"),
	       elf_errmsg (-1));


      Elf32_Word nsec = 0;
      Elf_Scn *scn = elf_getscn (ld_state.outelf,
				 ld_state.allsections[nsec]->scnidx);
      xelf_getshdr (scn, shdr);
      assert (shdr != NULL);

      XElf_Addr addr = shdr->sh_offset;
      XElf_Addr tls_offset = 0;
      XElf_Addr tls_start = ~((XElf_Addr) 0);
      XElf_Addr tls_end = 0;
      XElf_Off tls_filesize = 0;
      XElf_Addr tls_align = 0;

      
      nphdr = 0;
      if (dynamically_linked_p ())
	{
	  ++nphdr;
	  if (ld_state.interp != NULL
	      || ld_state.file_type != dso_file_type)
	    nphdr += 2;
	}

      segment = ld_state.output_segments;
      while (segment != NULL)
	{
	  struct output_rule *orule;
	  bool first_section = true;
	  XElf_Off nobits_size = 0;
	  XElf_Off memsize = 0;

	  
	  segment->align = ld_state.pagesize;

	  for (orule = segment->output_rules; orule != NULL;
	       orule = orule->next)
	    if (orule->tag == output_section)
	      {
		if (ld_state.allsections[nsec]->name
		    != orule->val.section.name)
		  
		  continue;

		if (segment->mode != 0)
		  {
		    
		    struct scninfo *isect;
		    struct scninfo *first;

		    isect = first = ld_state.allsections[nsec]->last;
		    if (isect != NULL)
		      do
			isect->offset += addr;
		      while ((isect = isect->next) != first);

		    
		    shdr->sh_addr = addr;

		    
		    (void) xelf_update_shdr (scn, shdr);

		    
		    ld_state.allsections[nsec]->addr = addr;

		    
		    if (unlikely (shdr->sh_flags & SHF_TLS))
		      {
			if (tls_start > addr)
			  {
			    tls_start = addr;
			    tls_offset = shdr->sh_offset;
			  }
			if (tls_end < addr + shdr->sh_size)
			  tls_end = addr + shdr->sh_size;
			if (shdr->sh_type != SHT_NOBITS)
			  tls_filesize += shdr->sh_size;
			if (shdr->sh_addralign > tls_align)
			  tls_align = shdr->sh_addralign;
		      }
		  }

		if (first_section)
		  {
		    
		    if (segment == ld_state.output_segments)
		      {
			segment->offset = 0;
			segment->addr = addr - shdr->sh_offset;
		      }
		    else
		      {
			segment->offset = shdr->sh_offset;
			segment->addr = addr;
		      }

		    
		    segment->align = MAX (segment->align, shdr->sh_addralign);

		    first_section = false;
		  }

		if (shdr->sh_type != SHT_NOBITS
		    || (shdr->sh_flags & SHF_TLS) == 0)
		  {
		    memsize = (shdr->sh_offset - segment->offset
			       + shdr->sh_size);
		    if (nobits_size != 0 && shdr->sh_type != SHT_NOTE)
		      error (EXIT_FAILURE, 0, gettext ("\
internal error: non-nobits section follows nobits section"));
		    if (shdr->sh_type == SHT_NOBITS)
		      nobits_size += shdr->sh_size;
		  }

		XElf_Off oldoff = shdr->sh_offset;

		if (++nsec >= ld_state.nallsections)
		  break;

		scn = elf_getscn (ld_state.outelf,
				  ld_state.allsections[nsec]->scnidx);
		xelf_getshdr (scn, shdr);
		assert (shdr != NULL);

		assert (oldoff <= shdr->sh_offset);
		addr += shdr->sh_offset - oldoff;
	      }
	    else
	      {
		assert (orule->tag == output_assignment);

		if (strcmp (orule->val.assignment->variable, ".") == 0)
		  
		  addr = eval_expression (orule->val.assignment->expression,
					  addr);
		else if (orule->val.assignment->sym != NULL)
		  {
		    XElf_Sym_vardef (sym);
		    size_t idx;

		    idx = dblindirect[orule->val.assignment->sym->outsymidx];
		    xelf_getsym (symdata, idx, sym);
		    sym->st_value = addr;
		    (void) xelf_update_sym (symdata, idx, sym);

		    idx = orule->val.assignment->sym->outdynsymidx;
		    if (idx != 0)
		      {
			assert (dynsymdata != NULL);
			xelf_getsym (dynsymdata, idx, sym);
			sym->st_value = addr;
			(void) xelf_update_sym (dynsymdata, idx, sym);
		      }
		  }
	      }

	  
	  if (segment->mode != 0)
	    {
	      xelf_getphdr_ptr (ld_state.outelf, nphdr, phdr);

	      phdr->p_type = PT_LOAD;
	      phdr->p_offset = segment->offset;
	      phdr->p_vaddr = segment->addr;
	      phdr->p_paddr = phdr->p_vaddr;
	      phdr->p_filesz = memsize - nobits_size;
	      phdr->p_memsz = memsize;
	      phdr->p_flags = segment->mode;
	      phdr->p_align = segment->align;

	      (void) xelf_update_phdr (ld_state.outelf, nphdr, phdr);
	      ++nphdr;
	    }

	  segment = segment->next;
	}

      
      xelf_getehdr (ld_state.outelf, ehdr);
      assert (ehdr != NULL);

      
      if (ld_state.need_tls)
	{
	  xelf_getphdr_ptr (ld_state.outelf, nphdr, phdr);
	  phdr->p_type = PT_TLS;
	  phdr->p_offset = tls_offset;
	  phdr->p_vaddr = tls_start;
	  phdr->p_paddr = tls_start;
	  phdr->p_filesz = tls_filesize;
	  phdr->p_memsz = tls_end - tls_start;
	  phdr->p_flags = PF_R;
	  phdr->p_align = tls_align;
	  ld_state.tls_tcb = tls_end;
	  ld_state.tls_start = tls_start;

	  (void) xelf_update_phdr (ld_state.outelf, nphdr, phdr);
	  ++nphdr;
	}

      
      xelf_getphdr_ptr (ld_state.outelf, nphdr, phdr);
      phdr->p_type = PT_GNU_STACK;
      phdr->p_offset = 0;
      phdr->p_vaddr = 0;
      phdr->p_paddr = 0;
      phdr->p_filesz = 0;
      phdr->p_memsz = 0;
      phdr->p_flags = (PF_R | PF_W
		       | (ld_state.execstack == execstack_true ? PF_X : 0));
      phdr->p_align = xelf_fsize (ld_state.outelf, ELF_T_ADDR, 1);

      (void) xelf_update_phdr (ld_state.outelf, nphdr, phdr);
      ++nphdr;


      if (ld_state.need_symtab)
	for (cnt = 1; cnt < nsym; ++cnt)
	  {
	    XElf_Sym_vardef (sym);
	    Elf32_Word shndx;

	    xelf_getsymshndx (symdata, xndxdata, cnt, sym, shndx);
	    assert (sym != NULL);

	    if (sym->st_shndx != SHN_XINDEX)
	      shndx = sym->st_shndx;

	    if ((shndx > SHN_UNDEF && shndx < SHN_LORESERVE)
		|| shndx > SHN_HIRESERVE)
	      {
		sym->st_value += ld_state.allsections[shndx - 1]->addr;

		(void) xelf_update_sym (symdata, cnt, sym);
	      }
	  }

      if (ld_state.need_dynsym)
	for (cnt = 1; cnt < nsym_dyn; ++cnt)
	  {
	    XElf_Sym_vardef (sym);

	    xelf_getsym (dynsymdata, cnt, sym);
	    assert (sym != NULL);

	    if (sym->st_shndx > SHN_UNDEF && sym->st_shndx < SHN_LORESERVE)
	      {
		sym->st_value += ld_state.allsections[sym->st_shndx - 1]->addr;

		(void) xelf_update_sym (dynsymdata, cnt, sym);
	      }
	  }

      
      
      struct symbol *se;
      void *p = NULL;
      while ((se = ld_symbol_tab_iterate (&ld_state.symbol_tab, &p)) != NULL)
	if (! se->in_dso)
	  {
	    XElf_Sym_vardef (sym);

	    addr = 0;

	    if (se->outdynsymidx != 0)
	      {
		xelf_getsym (dynsymdata, se->outdynsymidx, sym);
		assert (sym != NULL);
		addr = sym->st_value;
	      }
	    else if (se->outsymidx != 0)
	      {
		assert (dblindirect[se->outsymidx] != 0);
		xelf_getsym (symdata, dblindirect[se->outsymidx], sym);
		assert (sym != NULL);
		addr = sym->st_value;
	      }
	    else
	      abort ();

	    se->merge.value = addr;
	  }

      if (ld_state.reldynscnidx != 0)
	{
	  assert (ld_state.dynsymscnidx != 0);
	  scn = elf_getscn (ld_state.outelf, ld_state.reldynscnidx);
	  xelf_getshdr (scn, shdr);
	  assert (shdr != NULL);

	  shdr->sh_link = ld_state.dynsymscnidx;

	  (void) xelf_update_shdr (scn, shdr);
	}

      
      if (dynamically_linked_p ())
	{
	  Elf_Scn *outscn;

	  int idx = 0;
	  if (ld_state.interp != NULL || ld_state.file_type != dso_file_type)
	    {
	      assert (ld_state.interpscnidx != 0);
	      xelf_getshdr (elf_getscn (ld_state.outelf,
					ld_state.interpscnidx), shdr);
	      assert (shdr != NULL);

	      xelf_getphdr_ptr (ld_state.outelf, idx, phdr);
	      phdr->p_type = PT_PHDR;
	      phdr->p_offset = ehdr->e_phoff;
	      phdr->p_vaddr = ld_state.output_segments->addr + phdr->p_offset;
	      phdr->p_paddr = phdr->p_vaddr;
	      phdr->p_filesz = ehdr->e_phnum * ehdr->e_phentsize;
	      phdr->p_memsz = phdr->p_filesz;
	      phdr->p_flags = 0;	
	      phdr->p_align = xelf_fsize (ld_state.outelf, ELF_T_ADDR, 1);

	      (void) xelf_update_phdr (ld_state.outelf, idx, phdr);
	      ++idx;

	      
	      xelf_getphdr_ptr (ld_state.outelf, idx, phdr);
	      phdr->p_type = PT_INTERP;
	      phdr->p_offset = shdr->sh_offset;
	      phdr->p_vaddr = shdr->sh_addr;
	      phdr->p_paddr = phdr->p_vaddr;
	      phdr->p_filesz = shdr->sh_size;
	      phdr->p_memsz = phdr->p_filesz;
	      phdr->p_flags = 0;	
	      phdr->p_align = 1;	

	      (void) xelf_update_phdr (ld_state.outelf, idx, phdr);
	      ++idx;
	    }

	  assert (ld_state.dynamicscnidx);
	  outscn = elf_getscn (ld_state.outelf, ld_state.dynamicscnidx);
	  xelf_getshdr (outscn, shdr);
	  assert (shdr != NULL);

	  xelf_getphdr_ptr (ld_state.outelf, idx, phdr);
	  phdr->p_type = PT_DYNAMIC;
	  phdr->p_offset = shdr->sh_offset;
	  phdr->p_vaddr = shdr->sh_addr;
	  phdr->p_paddr = phdr->p_vaddr;
	  phdr->p_filesz = shdr->sh_size;
	  phdr->p_memsz = phdr->p_filesz;
	  phdr->p_flags = 0;		
	  phdr->p_align = shdr->sh_addralign;

	  (void) xelf_update_phdr (ld_state.outelf, idx, phdr);

	  
	  assert (ld_state.dynstrscnidx != 0);
	  shdr->sh_link = ld_state.dynstrscnidx;
	  (void) xelf_update_shdr (outscn, shdr);

	  
	  Elf_Data *dyndata = elf_getdata (outscn, NULL);
	  assert (dyndata != NULL);

	  
	  if (ld_state.ndsofiles > 0)
	    {
	      struct usedfiles *runp = ld_state.dsofiles->next;

	      do
		if (runp->used || !runp->as_needed)
		  {
		    
		    if (runp->lazyload)
		      new_dynamic_entry (dyndata, ld_state.ndynamic_filled++,
					 DT_POSFLAG_1, DF_P1_LAZYLOAD);

		    new_dynamic_entry (dyndata, ld_state.ndynamic_filled++,
				       DT_NEEDED,
				       ebl_strtaboffset (runp->sonameent));
		  }
	      while ((runp = runp->next) != ld_state.dsofiles->next);
	    }

	  
	  if (ld_state.rxxpath_strent != NULL)
	    new_dynamic_entry (dyndata, ld_state.ndynamic_filled++,
			       ld_state.rxxpath_tag,
			       ebl_strtaboffset (ld_state.rxxpath_strent));

	  
	  
	  if (ld_state.init_symbol != NULL)
	    {
	      XElf_Sym_vardef (sym);

	      if (ld_state.need_symtab)
		xelf_getsym (symdata,
			     dblindirect[ld_state.init_symbol->outsymidx],
			     sym);
	      else
		xelf_getsym (dynsymdata, ld_state.init_symbol->outdynsymidx,
			     sym);
	      assert (sym != NULL);

	      new_dynamic_entry (dyndata, ld_state.ndynamic_filled++,
				 DT_INIT, sym->st_value);
	    }
	  if (ld_state.fini_symbol != NULL)
	    {
	      XElf_Sym_vardef (sym);

	      if (ld_state.need_symtab)
		xelf_getsym (symdata,
			     dblindirect[ld_state.fini_symbol->outsymidx],
			     sym);
	      else
		xelf_getsym (dynsymdata, ld_state.fini_symbol->outdynsymidx,
			     sym);
	      assert (sym != NULL);

	      new_dynamic_entry (dyndata, ld_state.ndynamic_filled++,
				 DT_FINI, sym->st_value);
	    }
	  

	  
	  xelf_getshdr (elf_getscn (ld_state.outelf, ld_state.hashscnidx),
			shdr);
	  assert (shdr != NULL);
	  new_dynamic_entry (dyndata, ld_state.ndynamic_filled++, DT_HASH,
			     shdr->sh_addr);

	  
	  assert (ld_state.dynsymscnidx != 0);
	  xelf_getshdr (elf_getscn (ld_state.outelf, ld_state.dynsymscnidx),
			shdr);
	  assert (shdr != NULL);
	  new_dynamic_entry (dyndata, ld_state.ndynamic_filled++, DT_SYMTAB,
			     shdr->sh_addr);

	  new_dynamic_entry (dyndata, ld_state.ndynamic_filled++, DT_SYMENT,
			     xelf_fsize (ld_state.outelf, ELF_T_SYM, 1));

	  
	  xelf_getshdr (elf_getscn (ld_state.outelf, ld_state.dynstrscnidx),
			shdr);
	  assert (shdr != NULL);
	  new_dynamic_entry (dyndata, ld_state.ndynamic_filled++, DT_STRTAB,
			     shdr->sh_addr);

	  new_dynamic_entry (dyndata, ld_state.ndynamic_filled++, DT_STRSZ,
			     shdr->sh_size);

	  
	  if (ld_state.nplt > 0)
	    {
	      
	      xelf_getshdr (elf_getscn (ld_state.outelf,
					ld_state.gotpltscnidx), shdr);
	      assert (shdr != NULL);
	      new_dynamic_entry (dyndata, ld_state.ndynamic_filled++,
				 
				 
				 DT_PLTGOT, shdr->sh_addr);

	      xelf_getshdr (elf_getscn (ld_state.outelf,
					ld_state.pltrelscnidx), shdr);
	      assert (shdr != NULL);
	      new_dynamic_entry (dyndata, ld_state.ndynamic_filled++,
				 DT_PLTRELSZ, shdr->sh_size);

	      new_dynamic_entry (dyndata, ld_state.ndynamic_filled++,
				 DT_JMPREL, shdr->sh_addr);

	      new_dynamic_entry (dyndata, ld_state.ndynamic_filled++,
				 DT_PLTREL, REL_TYPE (statep));
	    }

	  if (ld_state.relsize_total > 0)
	    {
	      int rel = REL_TYPE (statep);
	      xelf_getshdr (elf_getscn (ld_state.outelf,
					ld_state.reldynscnidx), shdr);
	      assert (shdr != NULL);
	      new_dynamic_entry (dyndata, ld_state.ndynamic_filled++,
				 rel, shdr->sh_addr);

	      assert (DT_RELASZ - DT_RELA == 1);
	      assert (DT_RELSZ - DT_REL == 1);
	      new_dynamic_entry (dyndata, ld_state.ndynamic_filled++,
				 rel + 1, shdr->sh_size);

	      
	      assert (DT_RELAENT - DT_RELA == 2);
	      assert (DT_RELENT - DT_REL == 2);
	      new_dynamic_entry (dyndata, ld_state.ndynamic_filled++,
				 rel + 2,
				 rel == DT_REL
				 ? xelf_fsize (ld_state.outelf, ELF_T_REL, 1)
				 : xelf_fsize (ld_state.outelf, ELF_T_RELA,
					       1));
	    }

	  if (ld_state.verneedscnidx != 0)
	    {
	      xelf_getshdr (elf_getscn (ld_state.outelf,
					ld_state.verneedscnidx), shdr);
	      assert (shdr != NULL);
	      new_dynamic_entry (dyndata, ld_state.ndynamic_filled++,
				 DT_VERNEED, shdr->sh_addr);

	      new_dynamic_entry (dyndata, ld_state.ndynamic_filled++,
				 DT_VERNEEDNUM, ld_state.nverdeffile);
	    }

	  if (ld_state.versymscnidx != 0)
	    {
	      xelf_getshdr (elf_getscn (ld_state.outelf,
					ld_state.versymscnidx), shdr);
	      assert (shdr != NULL);
	      new_dynamic_entry (dyndata, ld_state.ndynamic_filled++,
				 DT_VERSYM, shdr->sh_addr);
	    }

	  
	  new_dynamic_entry (dyndata, ld_state.ndynamic_filled++, DT_DEBUG, 0);
	  assert (ld_state.ndynamic_filled < ld_state.ndynamic);

	  
	  if (ld_state.dt_flags != 0)
	    new_dynamic_entry (dyndata, ld_state.ndynamic_filled++, DT_FLAGS,
			       ld_state.dt_flags);

	  
	  if (ld_state.dt_flags_1 != 0)
	    new_dynamic_entry (dyndata, ld_state.ndynamic_filled++,
			       DT_FLAGS_1, ld_state.dt_flags_1);

	  
	  if (ld_state.dt_feature_1 != 0)
	    new_dynamic_entry (dyndata, ld_state.ndynamic_filled++,
			       DT_FEATURE_1, ld_state.dt_feature_1);

	  assert (ld_state.ndynamic_filled <= ld_state.ndynamic);
	}
    }


  
  
  
  

  while (rellist != NULL)
    {
      assert (ld_state.file_type == relocatable_file_type);
      Elf_Scn *outscn;

      outscn = elf_getscn (ld_state.outelf, rellist->scnidx);
      xelf_getshdr (outscn, shdr);
      
      assert (shdr != NULL);

      
      shdr->sh_link = ld_state.symscnidx;

      shdr->sh_info =
	rellist->scninfo->fileinfo->scninfo[SCNINFO_SHDR (rellist->scninfo->shdr).sh_info].outscnndx;


      RELOCATE_SECTION (statep, outscn, rellist->scninfo, dblindirect);

      
      (void) xelf_update_shdr (outscn, shdr);

      
      rellist = rellist->next;
    }

  if (ld_state.rellist != NULL)
    {
      assert (ld_state.file_type != relocatable_file_type);
      
      CREATE_RELOCATIONS (statep, dblindirect);
    }


  
  xelf_getehdr (ld_state.outelf, ehdr);
  assert (ehdr != NULL);

  
  if (likely (shstrtab_ndx < SHN_HIRESERVE)
      && likely (shstrtab_ndx != SHN_XINDEX))
    ehdr->e_shstrndx = shstrtab_ndx;
  else
    {
      Elf_Scn *scn = elf_getscn (ld_state.outelf, 0);

      xelf_getshdr (scn, shdr);
      if (unlikely (shdr == NULL))
	error (EXIT_FAILURE, 0,
	       gettext ("cannot get header of 0th section: %s"),
	       elf_errmsg (-1));

      shdr->sh_link = shstrtab_ndx;

      (void) xelf_update_shdr (scn, shdr);

      ehdr->e_shstrndx = SHN_XINDEX;
    }

  if (ld_state.file_type != relocatable_file_type)
    
    ehdr->e_entry = find_entry_point ();

  if (unlikely (xelf_update_ehdr (ld_state.outelf, ehdr) == 0))
    error (EXIT_FAILURE, 0,
	   gettext ("cannot update ELF header: %s"),
	   elf_errmsg (-1));


  
  free (ld_state.dblindirect);


  
  FINALIZE_PLT (statep, nsym, nsym_local, ndxtosym);


  
  if (ld_state.build_id != NULL)
    compute_build_id ();


  free (ndxtosym);

  return 0;
}


static void
ld_generic_relocate_section (struct ld_state *statep, Elf_Scn *outscn,
			     struct scninfo *firstp,
			     const Elf32_Word *dblindirect)
{
  error (EXIT_FAILURE, 0, gettext ("\
linker backend didn't specify function to relocate section"));
  
}


static int
ld_generic_finalize (struct ld_state *statep)
{
  
  if (elf_update (ld_state.outelf, ELF_C_WRITE) == -1)
      error (EXIT_FAILURE, 0, gettext ("while writing output file: %s"),
	     elf_errmsg (-1));

  
  if (elf_end (ld_state.outelf) != 0)
    error (EXIT_FAILURE, 0, gettext ("while finishing output file: %s"),
	   elf_errmsg (-1));

  
  struct stat temp_st;
  if (fstat (ld_state.outfd, &temp_st) != 0)
    error (EXIT_FAILURE, errno, gettext ("cannot stat output file"));

  if (rename (ld_state.tempfname, ld_state.outfname) != 0)
    
    error (EXIT_FAILURE, errno, gettext ("cannot rename output file"));

  
  struct stat new_st;
  if (stat (ld_state.outfname, &new_st) != 0
      || new_st.st_ino != temp_st.st_ino
      || new_st.st_dev != temp_st.st_dev)
    {
      
      unlink (ld_state.outfname);
      error (EXIT_FAILURE, 0, gettext ("\
WARNING: temporary output file overwritten before linking finished"));
    }

  
  (void) close (ld_state.outfd);

  
  ld_state.tempfname = NULL;

  return 0;
}


static bool
ld_generic_special_section_number_p (struct ld_state *statep, size_t number)
{
  
  return false;
}


static bool
ld_generic_section_type_p (struct ld_state *statep, GElf_Word type)
{
  if (type < SHT_NUM
      
      
      
      || (type >= SHT_GNU_verdef && type <= SHT_GNU_versym))
    return true;

  return false;
}


static XElf_Xword
ld_generic_dynamic_section_flags (struct ld_state *statep)
{
  return SHF_ALLOC | SHF_WRITE;
}


static void
ld_generic_initialize_plt (struct ld_state *statep, Elf_Scn *scn)
{
  error (EXIT_FAILURE, 0, gettext ("no machine specific '%s' implementation"),
	 "initialize_plt");
}


static void
ld_generic_initialize_pltrel (struct ld_state *statep, Elf_Scn *scn)
{
  error (EXIT_FAILURE, 0, gettext ("no machine specific '%s' implementation"),
	 "initialize_pltrel");
}


static void
ld_generic_initialize_got (struct ld_state *statep, Elf_Scn *scn)
{
  error (EXIT_FAILURE, 0, gettext ("no machine specific '%s' implementation"),
	 "initialize_got");
}


static void
ld_generic_initialize_gotplt (struct ld_state *statep, Elf_Scn *scn)
{
  error (EXIT_FAILURE, 0, gettext ("no machine specific '%s' implementation"),
	 "initialize_gotplt");
}


static void
ld_generic_finalize_plt (struct ld_state *statep, size_t nsym, size_t nsym_dyn,
			 struct symbol **ndxtosymp)
{
  
}


static int
ld_generic_rel_type (struct ld_state *statep)
{
  error (EXIT_FAILURE, 0, gettext ("no machine specific '%s' implementation"),
	 "rel_type");
  
  return 0;
}


static void
ld_generic_count_relocations (struct ld_state *statep, struct scninfo *scninfo)
{
  error (EXIT_FAILURE, 0, gettext ("no machine specific '%s' implementation"),
	 "count_relocations");
}


static void
ld_generic_create_relocations (struct ld_state *statep,
			       const Elf32_Word *dblindirect)
{
  error (EXIT_FAILURE, 0, gettext ("no machine specific '%s' implementation"),
	 "create_relocations");
}
