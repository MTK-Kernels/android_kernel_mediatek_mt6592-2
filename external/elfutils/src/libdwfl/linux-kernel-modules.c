/* Standard libdwfl callbacks for debugging the running Linux kernel.
   Copyright (C) 2005-2011, 2013, 2014 Red Hat, Inc.
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


#undef _FILE_OFFSET_BITS  
#include <fts.h>

#include <config.h>

#include "libdwflP.h"
#include <inttypes.h>
#include <errno.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <string.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <unistd.h>


#define KERNEL_MODNAME	"kernel"

#define MODULEDIRFMT	"/lib/modules/%s"

#define KNOTESFILE	"/sys/kernel/notes"
#define	MODNOTESFMT	"/sys/module/%s/notes"
#define KSYMSFILE	"/proc/kallsyms"
#define MODULELIST	"/proc/modules"
#define	SECADDRDIRFMT	"/sys/module/%s/sections/"
#define MODULE_SECT_NAME_LEN 32	


#if defined (USE_ZLIB) || defined (USE_BZLIB) || defined (USE_LZMA)
static const char *vmlinux_suffixes[] =
  {
#ifdef USE_ZLIB
    ".gz",
#endif
#ifdef USE_BZLIB
    ".bz2",
#endif
#ifdef USE_LZMA
    ".xz",
#endif
  };
#endif

static int
try_kernel_name (Dwfl *dwfl, char **fname, bool try_debug)
{
  if (*fname == NULL)
    return -1;

  int fd = ((((dwfl->callbacks->debuginfo_path
	       ? *dwfl->callbacks->debuginfo_path : NULL)
	      ?: DEFAULT_DEBUGINFO_PATH)[0] == ':') ? -1
	    : TEMP_FAILURE_RETRY (open64 (*fname, O_RDONLY)));

  if (fd < 0)
    {
      Dwfl_Module fakemod = { .dwfl = dwfl };
      fd = INTUSE(dwfl_standard_find_debuginfo) (&fakemod, NULL, NULL, 0,
						 *fname, basename (*fname), 0,
						 &fakemod.debug.name);
      if (fd < 0 && try_debug)
	fd = INTUSE(dwfl_standard_find_debuginfo) (&fakemod, NULL, NULL, 0,
						   *fname, NULL, 0,
						   &fakemod.debug.name);
      if (fakemod.debug.name != NULL)
	{
	  free (*fname);
	  *fname = fakemod.debug.name;
	}
    }

#if defined (USE_ZLIB) || defined (USE_BZLIB) || defined (USE_LZMA)
  if (fd < 0)
    for (size_t i = 0;
	 i < sizeof vmlinux_suffixes / sizeof vmlinux_suffixes[0];
	 ++i)
      {
	char *zname;
	if (asprintf (&zname, "%s%s", *fname, vmlinux_suffixes[i]) > 0)
	  {
	    fd = TEMP_FAILURE_RETRY (open64 (zname, O_RDONLY));
	    if (fd < 0)
	      free (zname);
	    else
	      {
		free (*fname);
		*fname = zname;
	      }
	  }
      }
#endif

  if (fd < 0)
    {
      free (*fname);
      *fname = NULL;
    }

  return fd;
}

static inline const char *
kernel_release (void)
{
  
  static struct utsname utsname;
  if (utsname.release[0] == '\0' && uname (&utsname) != 0)
    return NULL;
  return utsname.release;
}

static int
find_kernel_elf (Dwfl *dwfl, const char *release, char **fname)
{
  if ((release[0] == '/'
       ? asprintf (fname, "%s/vmlinux", release)
       : asprintf (fname, "/boot/vmlinux-%s", release)) < 0)
    return -1;

  int fd = try_kernel_name (dwfl, fname, true);
  if (fd < 0 && release[0] != '/')
    {
      free (*fname);
      if (asprintf (fname, MODULEDIRFMT "/vmlinux", release) < 0)
	return -1;
      fd = try_kernel_name (dwfl, fname, true);
    }

  return fd;
}

static int
get_release (Dwfl *dwfl, const char **release)
{
  if (dwfl == NULL)
    return -1;

  const char *release_string = release == NULL ? NULL : *release;
  if (release_string == NULL)
    {
      release_string = kernel_release ();
      if (release_string == NULL)
	return errno;
      if (release != NULL)
	*release = release_string;
    }

  return 0;
}

static int
report_kernel (Dwfl *dwfl, const char **release,
	       int (*predicate) (const char *module, const char *file))
{
  int result = get_release (dwfl, release);
  if (unlikely (result != 0))
    return result;

  char *fname;
  int fd = find_kernel_elf (dwfl, *release, &fname);

  if (fd < 0)
    result = ((predicate != NULL && !(*predicate) (KERNEL_MODNAME, NULL))
	      ? 0 : errno ?: ENOENT);
  else
    {
      bool report = true;

      if (predicate != NULL)
	{
	  
	  int want = (*predicate) (KERNEL_MODNAME, fname);
	  if (want < 0)
	    result = errno;
	  report = want > 0;
	}

      if (report)
	{
	  Dwfl_Module *mod = INTUSE(dwfl_report_elf) (dwfl, KERNEL_MODNAME,
						      fname, fd, 0, true);
	  if (mod == NULL)
	    result = -1;
	  else
	    
	    mod->e_type = ET_DYN;
	}

      free (fname);

      if (!report || result < 0)
	close (fd);
    }

  return result;
}

static int
report_kernel_archive (Dwfl *dwfl, const char **release,
		       int (*predicate) (const char *module, const char *file))
{
  int result = get_release (dwfl, release);
  if (unlikely (result != 0))
    return result;

  char *archive;
  int res = (((*release)[0] == '/')
	     ? asprintf (&archive, "%s/debug.a", *release)
	     : asprintf (&archive, MODULEDIRFMT "/debug.a", *release));
  if (unlikely (res < 0))
    return ENOMEM;

  int fd = try_kernel_name (dwfl, &archive, false);
  if (fd < 0)
    result = errno ?: ENOENT;
  else
    {
      
      Dwfl_Module *last = __libdwfl_report_offline (dwfl, NULL, archive, fd,
						    true, predicate);
      if (unlikely (last == NULL))
	result = -1;
      else
	{
	  
	  Dwfl_Module **tailp = &dwfl->modulelist, **prevp = tailp;
	  for (Dwfl_Module *m = *prevp; m != NULL; m = *(prevp = &m->next))
	    if (!m->gc && m->e_type != ET_REL && !strcmp (m->name, "kernel"))
	      {
		*prevp = m->next;
		m->next = *tailp;
		*tailp = m;
		break;
	      }
	}
    }

  free (archive);
  return result;
}

static size_t
check_suffix (const FTSENT *f, size_t namelen)
{
#define TRY(sfx)							\
  if ((namelen ? f->fts_namelen == namelen + sizeof sfx - 1		\
       : f->fts_namelen >= sizeof sfx)					\
      && !memcmp (f->fts_name + f->fts_namelen - (sizeof sfx - 1),	\
		  sfx, sizeof sfx))					\
    return sizeof sfx - 1

  TRY (".ko");
#if USE_ZLIB
  TRY (".ko.gz");
#endif
#if USE_BZLIB
  TRY (".ko.bz2");
#endif
#if USE_LZMA
  TRY (".ko.xz");
#endif

  return 0;

#undef	TRY
}

int
dwfl_linux_kernel_report_offline (Dwfl *dwfl, const char *release,
				  int (*predicate) (const char *module,
						    const char *file))
{
  int result = report_kernel_archive (dwfl, &release, predicate);
  if (result != ENOENT)
    return result;

  
  result = report_kernel (dwfl, &release, predicate);
  if (result == 0)
    {
      

      char *modulesdir[] = { NULL, NULL };
      if (release[0] == '/')
	modulesdir[0] = (char *) release;
      else
	{
	  if (asprintf (&modulesdir[0], MODULEDIRFMT, release) < 0)
	    return errno;
	}

      FTS *fts = fts_open (modulesdir, FTS_NOSTAT | FTS_LOGICAL, NULL);
      if (modulesdir[0] == (char *) release)
	modulesdir[0] = NULL;
      if (fts == NULL)
	{
	  free (modulesdir[0]);
	  return errno;
	}

      FTSENT *f;
      while ((f = fts_read (fts)) != NULL)
	{
	  if (f->fts_namelen == sizeof "source" - 1
	      && !strcmp (f->fts_name, "source"))
	    {
	      fts_set (fts, f, FTS_SKIP);
	      continue;
	    }

	  switch (f->fts_info)
	    {
	    case FTS_F:
	    case FTS_SL:
	    case FTS_NSOK:;
	      
	      const size_t suffix = check_suffix (f, 0);
	      if (suffix)
		{

		  char name[f->fts_namelen - suffix + 1];
		  for (size_t i = 0; i < f->fts_namelen - 3U; ++i)
		    if (f->fts_name[i] == '-' || f->fts_name[i] == ',')
		      name[i] = '_';
		    else
		      name[i] = f->fts_name[i];
		  name[f->fts_namelen - suffix] = '\0';

		  if (predicate != NULL)
		    {
		      
		      int want = (*predicate) (name, f->fts_path);
		      if (want < 0)
			{
			  result = -1;
			  break;
			}
		      if (!want)
			continue;
		    }

		  if (dwfl_report_offline (dwfl, name, f->fts_path, -1) == NULL)
		    {
		      result = -1;
		      break;
		    }
		}
	      continue;

	    case FTS_ERR:
	    case FTS_DNR:
	    case FTS_NS:
	      result = f->fts_errno;
	      break;

	    case FTS_SLNONE:
	    default:
	      continue;
	    }

	  
	  break;
	}
      fts_close (fts);
      free (modulesdir[0]);
    }

  return result;
}
INTDEF (dwfl_linux_kernel_report_offline)


static int
intuit_kernel_bounds (Dwarf_Addr *start, Dwarf_Addr *end, Dwarf_Addr *notes)
{
  FILE *f = fopen (KSYMSFILE, "r");
  if (f == NULL)
    return errno;

  (void) __fsetlocking (f, FSETLOCKING_BYCALLER);

  *notes = 0;

  char *line = NULL;
  size_t linesz = 0;
  size_t n;
  char *p = NULL;
  const char *type;

  inline bool read_address (Dwarf_Addr *addr)
  {
    if ((n = getline (&line, &linesz, f)) < 1 || line[n - 2] == ']')
      return false;
    *addr = strtoull (line, &p, 16);
    p += strspn (p, " \t");
    type = strsep (&p, " \t\n");
    if (type == NULL)
      return false;
    return p != NULL && p != line;
  }

  int result;
  do
    result = read_address (start) ? 0 : -1;
  while (result == 0 && strchr ("TtRr", *type) == NULL);

  if (result == 0)
    {
      *end = *start;
      while (read_address (end))
	if (*notes == 0 && !strcmp (p, "__start_notes\n"))
	  *notes = *end;

      Dwarf_Addr round_kernel = sysconf (_SC_PAGE_SIZE);
      *start &= -(Dwarf_Addr) round_kernel;
      *end += round_kernel - 1;
      *end &= -(Dwarf_Addr) round_kernel;
      if (*start >= *end || *end - *start < round_kernel)
	result = -1;
    }
  free (line);

  if (result == -1)
    result = ferror_unlocked (f) ? errno : ENOEXEC;

  fclose (f);

  return result;
}


static int
check_notes (Dwfl_Module *mod, const char *notesfile,
	     Dwarf_Addr vaddr, const char *secname)
{
  int fd = open64 (notesfile, O_RDONLY);
  if (fd < 0)
    return 1;

  assert (sizeof (Elf32_Nhdr) == sizeof (GElf_Nhdr));
  assert (sizeof (Elf64_Nhdr) == sizeof (GElf_Nhdr));
  union
  {
    GElf_Nhdr nhdr;
    unsigned char data[8192];
  } buf;

  ssize_t n = read (fd, buf.data, sizeof buf);
  close (fd);

  if (n <= 0)
    return 1;

  unsigned char *p = buf.data;
  while (p < &buf.data[n])
    {
      
      GElf_Nhdr *nhdr = (void *) p;
      p += sizeof *nhdr;
      unsigned char *name = p;
      p += (nhdr->n_namesz + 3) & -4U;
      unsigned char *bits = p;
      p += (nhdr->n_descsz + 3) & -4U;

      if (p <= &buf.data[n]
	  && nhdr->n_type == NT_GNU_BUILD_ID
	  && nhdr->n_namesz == sizeof "GNU"
	  && !memcmp (name, "GNU", sizeof "GNU"))
	{
	  

	  if (secname != NULL
	      && (INTUSE(dwfl_linux_kernel_module_section_address)
		  (mod, NULL, mod->name, 0, secname, 0, NULL, &vaddr) != 0
		  || vaddr == (GElf_Addr) -1l))
	    vaddr = 0;

	  if (vaddr != 0)
	    vaddr += bits - buf.data;
	  return INTUSE(dwfl_module_report_build_id) (mod, bits,
						      nhdr->n_descsz, vaddr);
	}
    }

  return 0;
}

static int
check_kernel_notes (Dwfl_Module *kernelmod, GElf_Addr vaddr)
{
  return check_notes (kernelmod, KNOTESFILE, vaddr, NULL) < 0 ? -1 : 0;
}

static int
check_module_notes (Dwfl_Module *mod)
{
  char *dirs[2] = { NULL, NULL };
  if (asprintf (&dirs[0], MODNOTESFMT, mod->name) < 0)
    return ENOMEM;

  FTS *fts = fts_open (dirs, FTS_NOSTAT | FTS_LOGICAL, NULL);
  if (fts == NULL)
    {
      free (dirs[0]);
      return 0;
    }

  int result = 0;
  FTSENT *f;
  while ((f = fts_read (fts)) != NULL)
    {
      switch (f->fts_info)
	{
	case FTS_F:
	case FTS_SL:
	case FTS_NSOK:
	  result = check_notes (mod, f->fts_accpath, 0, f->fts_name);
	  if (result > 0)	
	    {
	      result = 0;
	      continue;
	    }
	  break;

	case FTS_ERR:
	case FTS_DNR:
	  result = f->fts_errno;
	  break;

	case FTS_NS:
	case FTS_SLNONE:
	default:
	  continue;
	}

      
      break;
    }
  fts_close (fts);
  free (dirs[0]);

  return result;
}

int
dwfl_linux_kernel_report_kernel (Dwfl *dwfl)
{
  Dwarf_Addr start;
  Dwarf_Addr end;
  inline Dwfl_Module *report (void)
    {
      return INTUSE(dwfl_report_module) (dwfl, KERNEL_MODNAME, start, end);
    }

  for (Dwfl_Module *m = dwfl->modulelist; m != NULL; m = m->next)
    if (!strcmp (m->name, KERNEL_MODNAME))
      {
	start = m->low_addr;
	end = m->high_addr;
	return report () == NULL ? -1 : 0;
      }

  Dwarf_Addr notes;
  asm ("" : "=m" (notes));
  int result = intuit_kernel_bounds (&start, &end, &notes);
  if (result == 0)
    {
      Dwfl_Module *mod = report ();
      return unlikely (mod == NULL) ? -1 : check_kernel_notes (mod, notes);
    }
  if (result != ENOENT)
    return result;

  
  return report_kernel (dwfl, NULL, NULL);
}
INTDEF (dwfl_linux_kernel_report_kernel)



int
dwfl_linux_kernel_find_elf (Dwfl_Module *mod,
			    void **userdata __attribute__ ((unused)),
			    const char *module_name,
			    Dwarf_Addr base __attribute__ ((unused)),
			    char **file_name, Elf **elfp)
{
  if (mod->build_id_len > 0)
    {
      int fd = INTUSE(dwfl_build_id_find_elf) (mod, NULL, NULL, 0,
					       file_name, elfp);
      if (fd >= 0 || mod->main.elf != NULL || errno != 0)
	return fd;
    }

  const char *release = kernel_release ();
  if (release == NULL)
    return errno;

  if (!strcmp (module_name, KERNEL_MODNAME))
    return find_kernel_elf (mod->dwfl, release, file_name);

  

  char *modulesdir[] = { NULL, NULL };
  if (asprintf (&modulesdir[0], MODULEDIRFMT, release) < 0)
    return -1;

  FTS *fts = fts_open (modulesdir, FTS_NOSTAT | FTS_LOGICAL, NULL);
  if (fts == NULL)
    {
      free (modulesdir[0]);
      return -1;
    }

  size_t namelen = strlen (module_name);


  char alternate_name[namelen + 1];
  inline bool subst_name (char from, char to)
    {
      const char *n = memchr (module_name, from, namelen);
      if (n == NULL)
	return false;
      char *a = mempcpy (alternate_name, module_name, n - module_name);
      *a++ = to;
      ++n;
      const char *p;
      while ((p = memchr (n, from, namelen - (n - module_name))) != NULL)
	{
	  a = mempcpy (a, n, p - n);
	  *a++ = to;
	  n = p + 1;
	}
      memcpy (a, n, namelen - (n - module_name) + 1);
      return true;
    }
  if (!subst_name ('-', '_') && !subst_name ('_', '-'))
    alternate_name[0] = '\0';

  FTSENT *f;
  int error = ENOENT;
  while ((f = fts_read (fts)) != NULL)
    {
      if (f->fts_namelen == sizeof "source" - 1
	  && !strcmp (f->fts_name, "source"))
	{
	  fts_set (fts, f, FTS_SKIP);
	  continue;
	}

      error = ENOENT;
      switch (f->fts_info)
	{
	case FTS_F:
	case FTS_SL:
	case FTS_NSOK:
	  
	  if (check_suffix (f, namelen)
	      && (!memcmp (f->fts_name, module_name, namelen)
		  || !memcmp (f->fts_name, alternate_name, namelen)))
	    {
	      int fd = open64 (f->fts_accpath, O_RDONLY);
	      *file_name = strdup (f->fts_path);
	      fts_close (fts);
	      free (modulesdir[0]);
	      if (fd < 0)
		free (*file_name);
	      else if (*file_name == NULL)
		{
		  close (fd);
		  fd = -1;
		}
	      return fd;
	    }
	  break;

	case FTS_ERR:
	case FTS_DNR:
	case FTS_NS:
	  error = f->fts_errno;
	  break;

	case FTS_SLNONE:
	default:
	  break;
	}
    }

  fts_close (fts);
  free (modulesdir[0]);
  errno = error;
  return -1;
}
INTDEF (dwfl_linux_kernel_find_elf)



int
dwfl_linux_kernel_module_section_address
(Dwfl_Module *mod __attribute__ ((unused)),
 void **userdata __attribute__ ((unused)),
 const char *modname, Dwarf_Addr base __attribute__ ((unused)),
 const char *secname, Elf32_Word shndx __attribute__ ((unused)),
 const GElf_Shdr *shdr __attribute__ ((unused)),
 Dwarf_Addr *addr)
{
  char *sysfile;
  if (asprintf (&sysfile, SECADDRDIRFMT "%s", modname, secname) < 0)
    return DWARF_CB_ABORT;

  FILE *f = fopen (sysfile, "r");
  free (sysfile);

  if (f == NULL)
    {
      if (errno == ENOENT)
	{

	  if (!strcmp (secname, ".modinfo")
	      || !strcmp (secname, ".data.percpu")
	      || !strncmp (secname, ".exit", 5))
	    {
	      *addr = (Dwarf_Addr) -1l;
	      return DWARF_CB_OK;
	    }


	  const bool is_init = !strncmp (secname, ".init", 5);
	  if (is_init)
	    {
	      if (asprintf (&sysfile, SECADDRDIRFMT "_%s",
			    modname, &secname[1]) < 0)
		return ENOMEM;
	      f = fopen (sysfile, "r");
	      free (sysfile);
	      if (f != NULL)
		goto ok;
	    }

	  size_t namelen = strlen (secname);
	  if (namelen >= MODULE_SECT_NAME_LEN)
	    {
	      int len = asprintf (&sysfile, SECADDRDIRFMT "%s",
				  modname, secname);
	      if (len < 0)
		return DWARF_CB_ABORT;
	      char *end = sysfile + len;
	      do
		{
		  *--end = '\0';
		  f = fopen (sysfile, "r");
		  if (is_init && f == NULL && errno == ENOENT)
		    {
		      sysfile[len - namelen] = '_';
		      f = fopen (sysfile, "r");
		      sysfile[len - namelen] = '.';
		    }
		}
	      while (f == NULL && errno == ENOENT
		     && end - &sysfile[len - namelen] >= MODULE_SECT_NAME_LEN);
	      free (sysfile);

	      if (f != NULL)
		goto ok;
	    }
	}

      return DWARF_CB_ABORT;
    }

 ok:
  (void) __fsetlocking (f, FSETLOCKING_BYCALLER);

  int result = (fscanf (f, "%" PRIx64 "\n", addr) == 1 ? 0
		: ferror_unlocked (f) ? errno : ENOEXEC);
  fclose (f);

  if (result == 0)
    return DWARF_CB_OK;

  errno = result;
  return DWARF_CB_ABORT;
}
INTDEF (dwfl_linux_kernel_module_section_address)

int
dwfl_linux_kernel_report_modules (Dwfl *dwfl)
{
  FILE *f = fopen (MODULELIST, "r");
  if (f == NULL)
    return errno;

  (void) __fsetlocking (f, FSETLOCKING_BYCALLER);

  int result = 0;
  Dwarf_Addr modaddr;
  unsigned long int modsz;
  char modname[128];
  char *line = NULL;
  size_t linesz = 0;
  while (getline (&line, &linesz, f) > 0
	 && sscanf (line, "%128s %lu %*s %*s %*s %" PRIx64 " %*s\n",
		    modname, &modsz, &modaddr) == 3)
    {
      Dwfl_Module *mod = INTUSE(dwfl_report_module) (dwfl, modname,
						     modaddr, modaddr + modsz);
      if (mod == NULL)
	{
	  result = -1;
	  break;
	}

      result = check_module_notes (mod);
    }
  free (line);

  if (result == 0)
    result = ferror_unlocked (f) ? errno : feof_unlocked (f) ? 0 : ENOEXEC;

  fclose (f);

  return result;
}
INTDEF (dwfl_linux_kernel_report_modules)
