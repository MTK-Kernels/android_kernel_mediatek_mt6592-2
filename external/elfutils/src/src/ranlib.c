/* Generate an index to speed access to archives.
   Copyright (C) 2005-2012 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2005.

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

#include <ar.h>
#include <argp.h>
#include <assert.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <gelf.h>
#include <libintl.h>
#include <locale.h>
#include <mcheck.h>
#include <obstack.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <system.h>

#include "arlib.h"


static int handle_file (const char *fname);


static void print_version (FILE *stream, struct argp_state *state);
ARGP_PROGRAM_VERSION_HOOK_DEF = print_version;

ARGP_PROGRAM_BUG_ADDRESS_DEF = PACKAGE_BUGREPORT;


static const struct argp_option options[] =
{
  { NULL, 0, NULL, 0, NULL, 0 }
};

static const char doc[] = N_("Generate an index to speed access to archives.");

static const char args_doc[] = N_("ARCHIVE");

static const struct argp argp =
{
  options, NULL, args_doc, doc, arlib_argp_children, NULL, NULL
};


int
main (int argc, char *argv[])
{
  
  mtrace ();

  
  (void) __fsetlocking (stdin, FSETLOCKING_BYCALLER);
  (void) __fsetlocking (stdout, FSETLOCKING_BYCALLER);
  (void) __fsetlocking (stderr, FSETLOCKING_BYCALLER);

  
  (void) setlocale (LC_ALL, "");

  
  (void) bindtextdomain (PACKAGE_TARNAME, LOCALEDIR);

  
  (void) textdomain (PACKAGE_TARNAME);

  
  int remaining;
  (void) argp_parse (&argp, argc, argv, ARGP_IN_ORDER, &remaining, NULL);

  
  (void) elf_version (EV_CURRENT);

  
  if (remaining == argc)
    {
      error (0, 0, gettext ("Archive name required"));
      argp_help (&argp, stderr, ARGP_HELP_SEE, "ranlib");
      exit (EXIT_FAILURE);
    }

  
  int status = 0;
  do
    status |= handle_file (argv[remaining]);
  while (++remaining < argc);

  return status;
}


static void
print_version (FILE *stream, struct argp_state *state __attribute__ ((unused)))
{
  fprintf (stream, "ranlib (%s) %s\n", PACKAGE_NAME, PACKAGE_VERSION);
  fprintf (stream, gettext ("\
Copyright (C) %s Red Hat, Inc.\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
"), "2012");
  fprintf (stream, gettext ("Written by %s.\n"), "Ulrich Drepper");
}


static int
copy_content (Elf *elf, int newfd, off_t off, size_t n)
{
  size_t len;
  char *rawfile = elf_rawfile (elf, &len);

  assert (off + n <= len);

  
  size_t ps = sysconf (_SC_PAGESIZE);
  if (n > 2 * ps)
    posix_madvise (rawfile + (off & ~(ps - 1)), n, POSIX_MADV_SEQUENTIAL);

  return write_retry (newfd, rawfile + off, n) != (ssize_t) n;
}


static int
handle_file (const char *fname)
{
  int fd = open (fname, O_RDONLY);
  if (fd == -1)
    {
      error (0, errno, gettext ("cannot open '%s'"), fname);
      return 1;
    }

  struct stat st;
  if (fstat (fd, &st) != 0)
    {
      error (0, errno, gettext ("cannot stat '%s'"), fname);
      close (fd);
      return 1;
    }

  Elf *arelf = elf_begin (fd, ELF_C_READ_MMAP, NULL);
  if (arelf == NULL)
    {
      error (0, 0, gettext ("cannot create ELF descriptor for '%s': %s"),
	     fname, elf_errmsg (-1));
      close (fd);
      return 1;
    }

  if (elf_kind (arelf) != ELF_K_AR)
    {
      error (0, 0, gettext ("'%s' is no archive"), fname);
      elf_end (arelf);
      close (fd);
      return 1;
    }

  arlib_init ();

  
  off_t index_off = -1;
  size_t index_size = 0;
  off_t cur_off = SARMAG;
  Elf *elf;
  Elf_Cmd cmd = ELF_C_READ_MMAP;
  while ((elf = elf_begin (fd, cmd, arelf)) != NULL)
    {
      Elf_Arhdr *arhdr = elf_getarhdr (elf);
      assert (arhdr != NULL);

      
      if (strcmp (arhdr->ar_name, "/") == 0)
	{
	  index_off = elf_getaroff (elf);
	  index_size = arhdr->ar_size;
	}
      else
	{
	  arlib_add_symbols (elf, fname, arhdr->ar_name, cur_off);
	  cur_off += (((arhdr->ar_size + 1) & ~((off_t) 1))
		      + sizeof (struct ar_hdr));
	}

      
      cmd = elf_next (elf);
      if (elf_end (elf) != 0)
	error (0, 0, gettext ("error while freeing sub-ELF descriptor: %s"),
	       elf_errmsg (-1));
    }

  arlib_finalize ();

  
  int status = 0;
  if (symtab.symsnamelen != 0
      || (symtab.symsnamelen == 0 && index_size != 0))
    {
      char tmpfname[strlen (fname) + 7];
      strcpy (stpcpy (tmpfname, fname), "XXXXXX");
      int newfd = mkstemp (tmpfname);
      if (unlikely (newfd == -1))
	{
	nonew:
	  error (0, errno, gettext ("cannot create new file"));
	  status = 1;
	}
      else
	{
	  
	  if (unlikely (write_retry (newfd, ARMAG, SARMAG) != SARMAG))
	    {
	      
	    nonew_unlink:
	      unlink (tmpfname);
	      if (newfd != -1)
		close (newfd);
	      goto nonew;
	    }

	  off_t rest_off;
	  if (index_off != -1)
	    rest_off = (index_off + sizeof (struct ar_hdr)
			+ ((index_size + 1) & ~1ul));
	  else
	    rest_off = SARMAG;

	  if ((symtab.symsnamelen != 0
	       && ((write_retry (newfd, symtab.symsoff,
				 symtab.symsofflen)
		    != (ssize_t) symtab.symsofflen)
		   || (write_retry (newfd, symtab.symsname,
				    symtab.symsnamelen)
		       != (ssize_t) symtab.symsnamelen)))
	      || (index_off > SARMAG
		  && copy_content (arelf, newfd, SARMAG, index_off - SARMAG))
	      || copy_content (arelf, newfd, rest_off, st.st_size - rest_off)
	      || fchmod (newfd, st.st_mode & ALLPERMS) != 0
	      
	      || (({asm ("" :: "r" (fchown (newfd, st.st_uid, st.st_gid))); }),
		  close (newfd) != 0)
	      || (newfd = -1, rename (tmpfname, fname) != 0))
	    goto nonew_unlink;
	}
    }

  elf_end (arelf);

  arlib_fini ();

  close (fd);

  return status;
}


#include "debugpred.h"
