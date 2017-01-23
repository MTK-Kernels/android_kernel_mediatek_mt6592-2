/* Print size information from ELF file.
   Copyright (C) 2000-2007,2009,2012,2014 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

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
#include <error.h>
#include <fcntl.h>
#include <gelf.h>
#include <inttypes.h>
#include <libelf.h>
#include <libintl.h>
#include <locale.h>
#include <mcheck.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>

#include <system.h>


static void print_version (FILE *stream, struct argp_state *state);
ARGP_PROGRAM_VERSION_HOOK_DEF = print_version;

ARGP_PROGRAM_BUG_ADDRESS_DEF = PACKAGE_BUGREPORT;


#define OPT_FORMAT	0x100
#define OPT_RADIX	0x101

static const struct argp_option options[] =
{
  { NULL, 0, NULL, 0, N_("Output format:"), 0 },
  { "format", OPT_FORMAT, "FORMAT", 0,
    N_("Use the output format FORMAT.  FORMAT can be `bsd' or `sysv'.  "
       "The default is `bsd'"), 0 },
  { NULL, 'A', NULL, 0, N_("Same as `--format=sysv'"), 0 },
  { NULL, 'B', NULL, 0, N_("Same as `--format=bsd'"), 0 },
  { "radix", OPT_RADIX, "RADIX", 0, N_("Use RADIX for printing symbol values"),
    0},
  { NULL, 'd', NULL, 0, N_("Same as `--radix=10'"), 0 },
  { NULL, 'o', NULL, 0, N_("Same as `--radix=8'"), 0 },
  { NULL, 'x', NULL, 0, N_("Same as `--radix=16'"), 0 },
  { NULL, 'f', NULL, 0,
    N_("Similar to `--format=sysv' output but in one line"), 0 },

  { NULL, 0, NULL, 0, N_("Output options:"), 0 },
  { NULL, 'F', NULL, 0,
    N_("Print size and permission flags for loadable segments"), 0 },
  { "totals", 't', NULL, 0, N_("Display the total sizes (bsd only)"), 0 },
  { NULL, 0, NULL, 0, NULL, 0 }
};

static const char doc[] = N_("\
List section sizes of FILEs (a.out by default).");

static const char args_doc[] = N_("[FILE...]");

static error_t parse_opt (int key, char *arg, struct argp_state *state);

static struct argp argp =
{
  options, parse_opt, args_doc, doc, NULL, NULL, NULL
};


static int process_file (const char *fname);

static int handle_ar (int fd, Elf *elf, const char *prefix, const char *fname);

static void handle_elf (Elf *elf, const char *fullname, const char *fname);

static void show_bsd_totals (void);

#define INTERNAL_ERROR(fname) \
  error (EXIT_FAILURE, 0, gettext ("%s: INTERNAL ERROR %d (%s-%s): %s"),      \
	 fname, __LINE__, PACKAGE_VERSION, __DATE__, elf_errmsg (-1))



static enum
{
  format_bsd = 0,
  format_sysv,
  format_sysv_one_line,
  format_segments
} format;

static enum
{
  radix_decimal = 0,
  radix_hex,
  radix_octal
} radix;


static const int length_map[2][3] =
{
  [ELFCLASS32 - 1] =
  {
    [radix_hex] = 8,
    [radix_decimal] = 10,
    [radix_octal] = 11
  },
  [ELFCLASS64 - 1] =
  {
    [radix_hex] = 16,
    [radix_decimal] = 20,
    [radix_octal] = 22
  }
};

static bool totals;
static int totals_class;


int
main (int argc, char *argv[])
{
  int remaining;
  int result = 0;

  
  mtrace ();

  
  __fsetlocking (stdin, FSETLOCKING_BYCALLER);
  __fsetlocking (stdout, FSETLOCKING_BYCALLER);
  __fsetlocking (stderr, FSETLOCKING_BYCALLER);

  
  setlocale (LC_ALL, "");

  
  bindtextdomain (PACKAGE_TARNAME, LOCALEDIR);

  
  textdomain (PACKAGE_TARNAME);

  
  argp_parse (&argp, argc, argv, 0, &remaining, NULL);


  
  elf_version (EV_CURRENT);

  if (remaining == argc)
    
    result = process_file ("a.out");
  else
    
    do
      result |= process_file (argv[remaining]);
    while (++remaining < argc);

  if (totals && format == format_bsd && totals_class != 0)
    show_bsd_totals ();

  return result;
}


static void
print_version (FILE *stream, struct argp_state *state __attribute__ ((unused)))
{
  fprintf (stream, "size (%s) %s\n", PACKAGE_NAME, PACKAGE_VERSION);
  fprintf (stream, gettext ("\
Copyright (C) %s Red Hat, Inc.\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
"), "2012");
  fprintf (stream, gettext ("Written by %s.\n"), "Ulrich Drepper");
}


static error_t
parse_opt (int key, char *arg,
	   struct argp_state *state __attribute__ ((unused)))
{
  switch (key)
    {
    case 'd':
      radix = radix_decimal;
      break;

    case 'f':
      format = format_sysv_one_line;
      break;

    case 'o':
      radix = radix_octal;
      break;

    case 'x':
      radix = radix_hex;
      break;

    case 'A':
      format = format_sysv;
      break;

    case 'B':
      format = format_bsd;
      break;

    case 'F':
      format = format_segments;
      break;

    case OPT_FORMAT:
      if (strcmp (arg, "bsd") == 0 || strcmp (arg, "berkeley") == 0)
	format = format_bsd;
      else if (likely (strcmp (arg, "sysv") == 0))
	format = format_sysv;
      else
	error (EXIT_FAILURE, 0, gettext ("Invalid format: %s"), arg);
      break;

    case OPT_RADIX:
      if (strcmp (arg, "x") == 0 || strcmp (arg, "16") == 0)
	radix = radix_hex;
      else if (strcmp (arg, "d") == 0 || strcmp (arg, "10") == 0)
	radix = radix_decimal;
      else if (strcmp (arg, "o") == 0 || strcmp (arg, "8") == 0)
	radix = radix_octal;
      else
	error (EXIT_FAILURE, 0, gettext ("Invalid radix: %s"), arg);
      break;

    case 't':
      totals = true;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}


static int
process_file (const char *fname)
{
  int fd = open (fname, O_RDONLY);
  if (unlikely (fd == -1))
    {
      error (0, errno, gettext ("cannot open '%s'"), fname);
      return 1;
    }

  
  Elf *elf = elf_begin (fd, ELF_C_READ_MMAP, NULL);
  if (likely (elf != NULL))
    {
      if (elf_kind (elf) == ELF_K_ELF)
	{
	  handle_elf (elf, NULL, fname);

	  if (unlikely (elf_end (elf) != 0))
	    INTERNAL_ERROR (fname);

	  if (unlikely (close (fd) != 0))
	    error (EXIT_FAILURE, errno, gettext ("while closing '%s'"), fname);

	  return 0;
	}
      else if (likely (elf_kind (elf) == ELF_K_AR))
	{
	  int result = handle_ar (fd, elf, NULL, fname);

	  if (unlikely  (close (fd) != 0))
	    error (EXIT_FAILURE, errno, gettext ("while closing '%s'"), fname);

	  return result;
	}

      
      if (unlikely (elf_end (elf) != 0))
	INTERNAL_ERROR (fname);
    }

  if (unlikely (close (fd) != 0))
    error (EXIT_FAILURE, errno, gettext ("while closing '%s'"), fname);

  error (0, 0, gettext ("%s: file format not recognized"), fname);

  return 1;
}


static void
print_header (Elf *elf)
{
  static int done;

  if (! done)
    {
      int ddigits = length_map[gelf_getclass (elf) - 1][radix_decimal];
      int xdigits = length_map[gelf_getclass (elf) - 1][radix_hex];

      printf ("%*s %*s %*s %*s %*s %s\n",
	      ddigits - 2, sgettext ("bsd|text"),
	      ddigits - 2, sgettext ("bsd|data"),
	      ddigits - 2, sgettext ("bsd|bss"),
	      ddigits - 2, sgettext ("bsd|dec"),
	      xdigits - 2, sgettext ("bsd|hex"),
	      sgettext ("bsd|filename"));

      done = 1;
    }
}


static int
handle_ar (int fd, Elf *elf, const char *prefix, const char *fname)
{
  size_t prefix_len = prefix == NULL ? 0 : strlen (prefix);
  size_t fname_len = strlen (fname) + 1;
  char new_prefix[prefix_len + 1 + fname_len];
  char *cp = new_prefix;

  
  if (prefix != NULL)
    {
      cp = mempcpy (cp, prefix, prefix_len);
      *cp++ = ':';
    }
  memcpy (cp, fname, fname_len);

  
  int result = 0;
  Elf *subelf;
  Elf_Cmd cmd = ELF_C_READ_MMAP;
  while ((subelf = elf_begin (fd, cmd, elf)) != NULL)
    {
      
      Elf_Arhdr *arhdr = elf_getarhdr (subelf);

      if (elf_kind (subelf) == ELF_K_ELF)
	handle_elf (subelf, new_prefix, arhdr->ar_name);
      else if (likely (elf_kind (subelf) == ELF_K_AR))
	result |= handle_ar (fd, subelf, new_prefix, arhdr->ar_name);
      

      
      cmd = elf_next (subelf);
      if (unlikely (elf_end (subelf) != 0))
	INTERNAL_ERROR (fname);
    }

  if (unlikely (elf_end (elf) != 0))
    INTERNAL_ERROR (fname);

  return result;
}


static void
show_sysv (Elf *elf, const char *prefix, const char *fname,
	   const char *fullname)
{
  int maxlen = 10;
  const int digits = length_map[gelf_getclass (elf) - 1][radix];

  
  size_t shstrndx;
  if (unlikely (elf_getshdrstrndx (elf, &shstrndx) < 0))
    error (EXIT_FAILURE, 0,
	   gettext ("cannot get section header string table index"));

  
  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);

      if (shdr == NULL)
	INTERNAL_ERROR (fullname);

      
      const char *name = elf_strptr (elf, shstrndx, shdr->sh_name);
      if (name != NULL && (shdr->sh_flags & SHF_ALLOC) != 0)
	maxlen = MAX (maxlen, (int) strlen (name));
    }

  fputs_unlocked (fname, stdout);
  if (prefix != NULL)
    printf (gettext (" (ex %s)"), prefix);
  printf (":\n%-*s %*s %*s\n",
	  maxlen, sgettext ("sysv|section"),
	  digits - 2, sgettext ("sysv|size"),
	  digits, sgettext ("sysv|addr"));

  
  GElf_Off total = 0;
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);

      
      if ((shdr->sh_flags & SHF_ALLOC) != 0)
	{
	  printf ((radix == radix_hex
		   ? "%-*s %*" PRIx64 " %*" PRIx64 "\n"
		   : (radix == radix_decimal
		      ? "%-*s %*" PRId64 " %*" PRId64 "\n"
		      : "%-*s %*" PRIo64 " %*" PRIo64 "\n")),
		  maxlen, elf_strptr (elf, shstrndx, shdr->sh_name),
		  digits - 2, shdr->sh_size,
		  digits, shdr->sh_addr);

	  total += shdr->sh_size;
	}
    }

  if (radix == radix_hex)
    printf ("%-*s %*" PRIx64 "\n\n\n", maxlen, sgettext ("sysv|Total"),
	    digits - 2, total);
  else if (radix == radix_decimal)
    printf ("%-*s %*" PRId64 "\n\n\n", maxlen, sgettext ("sysv|Total"),
	    digits - 2, total);
  else
    printf ("%-*s %*" PRIo64 "\n\n\n", maxlen, sgettext ("sysv|Total"),
	    digits - 2, total);
}


static void
show_sysv_one_line (Elf *elf)
{
  
  size_t shstrndx;
  if (unlikely (elf_getshdrstrndx (elf, &shstrndx) < 0))
    error (EXIT_FAILURE, 0,
	   gettext ("cannot get section header string table index"));

  
  GElf_Off total = 0;
  bool first = true;
  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);

      
      if ((shdr->sh_flags & SHF_ALLOC) == 0)
	continue;

      if (! first)
	fputs_unlocked (" + ", stdout);
      first = false;

      printf ((radix == radix_hex ? "%" PRIx64 "(%s)"
	       : (radix == radix_decimal ? "%" PRId64 "(%s)"
		  : "%" PRIo64 "(%s)")),
	      shdr->sh_size, elf_strptr (elf, shstrndx, shdr->sh_name));

      total += shdr->sh_size;
    }

  if (radix == radix_hex)
    printf (" = %#" PRIx64 "\n", total);
  else if (radix == radix_decimal)
    printf (" = %" PRId64 "\n", total);
  else
    printf (" = %" PRIo64 "\n", total);
}


static uintmax_t total_textsize;
static uintmax_t total_datasize;
static uintmax_t total_bsssize;


static void
show_bsd (Elf *elf, const char *prefix, const char *fname,
	  const char *fullname)
{
  GElf_Off textsize = 0;
  GElf_Off datasize = 0;
  GElf_Off bsssize = 0;
  const int ddigits = length_map[gelf_getclass (elf) - 1][radix_decimal];
  const int xdigits = length_map[gelf_getclass (elf) - 1][radix_hex];

  
  Elf_Scn *scn = NULL;
  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);

      if (shdr == NULL)
	INTERNAL_ERROR (fullname);

      
      if ((shdr->sh_flags & SHF_ALLOC) == 0)
	continue;

      if ((shdr->sh_flags & SHF_WRITE) == 0)
	textsize += shdr->sh_size;
      else if (shdr->sh_type == SHT_NOBITS)
	bsssize += shdr->sh_size;
      else
	datasize += shdr->sh_size;
    }

  printf ("%*" PRId64 " %*" PRId64 " %*" PRId64 " %*" PRId64 " %*"
	  PRIx64 " %s",
	  ddigits - 2, textsize,
	  ddigits - 2, datasize,
	  ddigits - 2, bsssize,
	  ddigits - 2, textsize + datasize + bsssize,
	  xdigits - 2, textsize + datasize + bsssize,
	  fname);
  if (prefix != NULL)
    printf (gettext (" (ex %s)"), prefix);
  fputs_unlocked ("\n", stdout);

  total_textsize += textsize;
  total_datasize += datasize;
  total_bsssize += bsssize;

  totals_class = MAX (totals_class, gelf_getclass (elf));
}


static void
show_bsd_totals (void)
{
  int ddigits = length_map[totals_class - 1][radix_decimal];
  int xdigits = length_map[totals_class - 1][radix_hex];

  printf ("%*" PRIuMAX " %*" PRIuMAX " %*" PRIuMAX " %*" PRIuMAX " %*"
	  PRIxMAX " %s",
	  ddigits - 2, total_textsize,
	  ddigits - 2, total_datasize,
	  ddigits - 2, total_bsssize,
	  ddigits - 2, total_textsize + total_datasize + total_bsssize,
	  xdigits - 2, total_textsize + total_datasize + total_bsssize,
	  gettext ("(TOTALS)\n"));
}


static void
show_segments (Elf *elf, const char *fullname)
{
  size_t phnum;
  if (elf_getphdrnum (elf, &phnum) != 0)
    INTERNAL_ERROR (fullname);

  GElf_Off total = 0;
  bool first = true;
  for (size_t cnt = 0; cnt < phnum; ++cnt)
    {
      GElf_Phdr phdr_mem;
      GElf_Phdr *phdr;

      phdr = gelf_getphdr (elf, cnt, &phdr_mem);
      if (phdr == NULL)
	INTERNAL_ERROR (fullname);

      if (phdr->p_type != PT_LOAD)
	
	continue;

      if (! first)
	fputs_unlocked (" + ", stdout);
      first = false;

      printf (radix == radix_hex ? "%" PRIx64 "(%c%c%c)"
	      : (radix == radix_decimal ? "%" PRId64 "(%c%c%c)"
		 : "%" PRIo64 "(%c%c%c)"),
	      phdr->p_memsz,
	      (phdr->p_flags & PF_R) == 0 ? '-' : 'r',
	      (phdr->p_flags & PF_W) == 0 ? '-' : 'w',
	      (phdr->p_flags & PF_X) == 0 ? '-' : 'x');

      total += phdr->p_memsz;
    }

  if (radix == radix_hex)
    printf (" = %#" PRIx64 "\n", total);
  else if (radix == radix_decimal)
    printf (" = %" PRId64 "\n", total);
  else
    printf (" = %" PRIo64 "\n", total);
}


static void
handle_elf (Elf *elf, const char *prefix, const char *fname)
{
  size_t prefix_len = prefix == NULL ? 0 : strlen (prefix);
  size_t fname_len = strlen (fname) + 1;
  char fullname[prefix_len + 1 + fname_len];
  char *cp = fullname;

  
  if (prefix != NULL)
    {
      cp = mempcpy (cp, prefix, prefix_len);
      *cp++ = ':';
    }
  memcpy (cp, fname, fname_len);

  if (format == format_sysv)
    show_sysv (elf, prefix, fname, fullname);
  else if (format == format_sysv_one_line)
    show_sysv_one_line (elf);
  else if (format == format_segments)
    show_segments (elf, fullname);
  else
    {
      print_header (elf);

      show_bsd (elf, prefix, fname, fullname);
    }
}


#include "debugpred.h"
