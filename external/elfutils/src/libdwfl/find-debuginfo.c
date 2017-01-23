/* Standard find_debuginfo callback for libdwfl.
   Copyright (C) 2005-2010, 2014 Red Hat, Inc.
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

#include "libdwflP.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "system.h"


static int
try_open (const struct stat64 *main_stat,
	  const char *dir, const char *subdir, const char *debuglink,
	  char **debuginfo_file_name)
{
  char *fname;
  if (dir == NULL && subdir == NULL)
    {
      fname = strdup (debuglink);
      if (fname == NULL)
	return -1;
    }
  else if ((subdir == NULL ? asprintf (&fname, "%s/%s", dir, debuglink)
	    : dir == NULL ? asprintf (&fname, "%s/%s", subdir, debuglink)
	    : asprintf (&fname, "%s/%s/%s", dir, subdir, debuglink)) < 0)
    return -1;

  struct stat64 st;
  int fd = TEMP_FAILURE_RETRY (open64 (fname, O_RDONLY));
  if (fd < 0)
    free (fname);
  else if (fstat64 (fd, &st) == 0
	   && st.st_ino == main_stat->st_ino
	   && st.st_dev == main_stat->st_dev)
    {
      
      close (fd);
      errno = ENOENT;
      fd = -1;
    }
  else
    *debuginfo_file_name = fname;

  return fd;
}

static inline bool
check_crc (int fd, GElf_Word debuglink_crc)
{
  uint32_t file_crc;
  return (__libdwfl_crc32_file (fd, &file_crc) == 0
	  && file_crc == debuglink_crc);
}

static bool
validate (Dwfl_Module *mod, int fd, bool check, GElf_Word debuglink_crc)
{
  
  if (mod->dw != NULL)
    {
      bool valid = false;
      const void *build_id;
      const char *altname;
      ssize_t build_id_len = INTUSE(dwelf_dwarf_gnu_debugaltlink) (mod->dw,
								   &altname,
								   &build_id);
      if (build_id_len > 0)
	{
	  Dwfl_Error error = __libdw_open_file (&fd, &mod->alt_elf,
						false, false);
	  if (error != DWFL_E_NOERROR)
	    __libdwfl_seterrno (error);
	  else
	    {
	      const void *alt_build_id;
	      ssize_t alt_len = INTUSE(dwelf_elf_gnu_build_id) (mod->alt_elf,
								&alt_build_id);
	      if (alt_len > 0 && alt_len == build_id_len
		  && memcmp (build_id, alt_build_id, alt_len) == 0)
		valid = true;
	      else
		{
		  
		  elf_end (mod->alt_elf);
		  mod->alt_elf = NULL;
		  close (fd);
		  fd = -1;
		}
	    }
	}
      return valid;
    }

  
  if (mod->build_id_len > 0)
    {

      mod->debug.valid = false;
      Dwfl_Error error = __libdw_open_file (&fd, &mod->debug.elf, false, false);
      if (error != DWFL_E_NOERROR)
	__libdwfl_seterrno (error);
      else if (likely (__libdwfl_find_build_id (mod, false,
						mod->debug.elf) == 2))
	
	mod->debug.valid = true;
      else
	{
	  
	  elf_end (mod->debug.elf);
	  mod->debug.elf = NULL;
	  close (fd);
	  fd = -1;
	}

      return mod->debug.valid;
    }

  return !check || check_crc (fd, debuglink_crc);
}

static int
find_debuginfo_in_path (Dwfl_Module *mod, const char *file_name,
			const char *debuglink_file, GElf_Word debuglink_crc,
			char **debuginfo_file_name)
{
  bool cancheck = debuglink_crc != (GElf_Word) 0;

  const char *file_basename = file_name == NULL ? NULL : basename (file_name);
  if (debuglink_file == NULL)
    {
      if (file_basename == NULL || mod->dw != NULL)
	{
	  errno = 0;
	  return -1;
	}

      size_t len = strlen (file_basename);
      char *localname = alloca (len + sizeof ".debug");
      memcpy (localname, file_basename, len);
      memcpy (&localname[len], ".debug", sizeof ".debug");
      debuglink_file = localname;
      cancheck = false;
    }


  const Dwfl_Callbacks *const cb = mod->dwfl->callbacks;
  char *path = strdupa ((cb->debuginfo_path ? *cb->debuginfo_path : NULL)
			?: DEFAULT_DEBUGINFO_PATH);

  
  bool defcheck = true;
  if (path[0] == '-' || path[0] == '+')
    {
      defcheck = path[0] == '+';
      ++path;
    }

  
  struct stat64 main_stat;
  if (unlikely ((mod->main.fd != -1 ? fstat64 (mod->main.fd, &main_stat)
		 : file_name != NULL ? stat64 (file_name, &main_stat)
		 : -1) < 0))
    {
      main_stat.st_dev = 0;
      main_stat.st_ino = 0;
    }

  char *file_dirname = (file_basename == file_name ? NULL
			: strndupa (file_name, file_basename - 1 - file_name));
  char *p;
  while ((p = strsep (&path, ":")) != NULL)
    {
      
      bool check = defcheck;
      if (*p == '+' || *p == '-')
	check = *p++ == '+';
      check = check && cancheck;

      const char *dir, *subdir, *file;
      switch (p[0])
	{
	case '\0':
	  
	  dir = file_dirname;
	  subdir = NULL;
	  file = debuglink_file;
	  break;
	case '/':
	  if (mod->dw == NULL
	      && (file_dirname == NULL || file_dirname[0] != '/'))
	    continue;
	  dir = p;
	  if (mod->dw == NULL)
	    {
	      subdir = file_dirname + 1;
	      file = debuglink_file;
	    }
	  else
	    {
	      subdir = NULL;
	      file = basename (debuglink_file);
	    }
	  break;
	default:
	  dir = file_dirname;
	  subdir = p;
	  file = debuglink_file;
	  break;
	}

      char *fname = NULL;
      int fd = try_open (&main_stat, dir, subdir, file, &fname);
      if (fd < 0)
	switch (errno)
	  {
	  case ENOENT:
	  case ENOTDIR:
	    if (mod->dw != NULL && (p[0] == '\0' || p[0] == '/'))
	      {
		fd = try_open (&main_stat, dir, ".dwz",
			       basename (file), &fname);
		if (fd < 0)
		  {
		    if (errno != ENOENT && errno != ENOTDIR)
		      return -1;
		    else
		      continue;
		  }
		break;
	      }
	    continue;
	  default:
	    return -1;
	  }
      if (validate (mod, fd, check, debuglink_crc))
	{
	  *debuginfo_file_name = fname;
	  return fd;
	}
      free (fname);
      close (fd);
    }

  
  errno = 0;
  return -1;
}

int
dwfl_standard_find_debuginfo (Dwfl_Module *mod,
			      void **userdata __attribute__ ((unused)),
			      const char *modname __attribute__ ((unused)),
			      GElf_Addr base __attribute__ ((unused)),
			      const char *file_name,
			      const char *debuglink_file,
			      GElf_Word debuglink_crc,
			      char **debuginfo_file_name)
{
  const unsigned char *bits;
  GElf_Addr vaddr;
  if (INTUSE(dwfl_module_build_id) (mod, &bits, &vaddr) > 0)
    {
      int fd = INTUSE(dwfl_build_id_find_debuginfo) (mod,
						     NULL, NULL, 0,
						     NULL, NULL, 0,
						     debuginfo_file_name);

      if (fd >= 0
	  || (mod->dw == NULL && mod->debug.elf != NULL)
	  || (mod->dw != NULL && mod->alt_elf != NULL)
	  || errno != 0)
	return fd;
    }

  
  int fd = find_debuginfo_in_path (mod, file_name,
				   debuglink_file, debuglink_crc,
				   debuginfo_file_name);

  if (fd < 0 && errno == 0)
    {

      char *canon = canonicalize_file_name (file_name);
      if (canon != NULL && strcmp (file_name, canon))
	fd = find_debuginfo_in_path (mod, canon,
				     debuglink_file, debuglink_crc,
				     debuginfo_file_name);
      free (canon);
    }

  return fd;
}
INTDEF (dwfl_standard_find_debuginfo)
