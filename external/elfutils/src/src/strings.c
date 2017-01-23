/* Print the strings of printable characters in files.
   Copyright (C) 2005-2010, 2012, 2014 Red Hat, Inc.
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

#include <argp.h>
#include <assert.h>
#include <ctype.h>
#include <endian.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <gelf.h>
#include <inttypes.h>
#include <libintl.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <system.h>


static int read_fd (int fd, const char *fname, off64_t fdlen);
static int read_elf (Elf *elf, int fd, const char *fname, off64_t fdlen);


static void print_version (FILE *stream, struct argp_state *state);
ARGP_PROGRAM_VERSION_HOOK_DEF = print_version;

ARGP_PROGRAM_BUG_ADDRESS_DEF = PACKAGE_BUGREPORT;

static const struct argp_option options[] =
{
  { NULL, 0, NULL, 0, N_("Output Selection:"), 0 },
  { "all", 'a', NULL, 0, N_("Scan entire file, not only loaded sections"), 0 },
  { "bytes", 'n', "MIN-LEN", 0,
    N_("Only NUL-terminated sequences of MIN-LEN characters or more are printed"), 0 },
  { "encoding", 'e', "SELECTOR", 0, N_("\
Select character size and endianess: s = 7-bit, S = 8-bit, {b,l} = 16-bit, {B,L} = 32-bit"),
    0},
  { "print-file-name", 'f', NULL, 0,
    N_("Print name of the file before each string."), 0 },
  { "radix", 't', "{o,d,x}", 0,
    N_("Print location of the string in base 8, 10, or 16 respectively."), 0 },
  { NULL, 'o', NULL, 0, N_("Alias for --radix=o"), 0 },

  { NULL, 0, NULL, 0, N_("Miscellaneous:"), 0 },
  { NULL, 0, NULL, 0, NULL, 0 }
};

static const char doc[] = N_("\
Print the strings of printable characters in files.");

static const char args_doc[] = N_("[FILE...]");

static error_t parse_opt (int key, char *arg, struct argp_state *state);

static struct argp argp =
{
  options, parse_opt, args_doc, doc, NULL, NULL, NULL
};



static bool entire_file;

static size_t min_len = 4;

static size_t bytes_per_char = 1;

static size_t min_len_bytes;

static bool big_endian;

static bool char_7bit;

static bool print_file_name;

static enum
{
  radix_none = 0,
  radix_decimal,
  radix_hex,
  radix_octal
} radix = radix_none;


static size_t ps;


static unsigned char *elfmap;
static unsigned char *elfmap_base;
static size_t elfmap_size;
static off64_t elfmap_off;


int
main (int argc, char *argv[])
{
  
  __fsetlocking (stdin, FSETLOCKING_BYCALLER);
  __fsetlocking (stdout, FSETLOCKING_BYCALLER);

  
  (void) setlocale (LC_ALL, "");

  
  (void) bindtextdomain (PACKAGE_TARNAME, LOCALEDIR);

  
  (void) textdomain (PACKAGE_TARNAME);

  
  int remaining;
  (void) argp_parse (&argp, argc, argv, 0, &remaining, NULL);

  
  elf_version (EV_CURRENT);

  
  ps = sysconf (_SC_PAGESIZE);

  struct stat64 st;
  int result = 0;
  if (remaining == argc)
    result = read_fd (STDIN_FILENO,
		      print_file_name ? "{standard input}" : NULL,
		      (fstat64 (STDIN_FILENO, &st) == 0 && S_ISREG (st.st_mode))
		      ? st.st_size : INT64_C (0x7fffffffffffffff));
  else
    do
      {
	int fd = (strcmp (argv[remaining], "-") == 0
		  ? STDIN_FILENO : open (argv[remaining], O_RDONLY));
	if (unlikely (fd == -1))
	  {
	    error (0, errno, gettext ("cannot open '%s'"), argv[remaining]);
	    result = 1;
	  }
	else
	  {
	    const char *fname = print_file_name ? argv[remaining] : NULL;
	    int fstat_fail = fstat64 (fd, &st);
	    off64_t fdlen = (fstat_fail
			     ? INT64_C (0x7fffffffffffffff) : st.st_size);
	    if (fdlen > (off64_t) min_len_bytes)
	      {
		Elf *elf = NULL;
		if (entire_file
		    || fstat_fail
		    || !S_ISREG (st.st_mode)
		    || (elf = elf_begin (fd, ELF_C_READ, NULL)) == NULL
		    || elf_kind (elf) != ELF_K_ELF)
		  result |= read_fd (fd, fname, fdlen);
		else
		  result |= read_elf (elf, fd, fname, fdlen);

		
		elf_end (elf);
	      }

	    if (strcmp (argv[remaining], "-") != 0)
	      close (fd);
	  }

	if (elfmap != NULL && elfmap != MAP_FAILED)
	  munmap (elfmap, elfmap_size);
	elfmap = NULL;
      }
    while (++remaining < argc);

  return result;
}


static void
print_version (FILE *stream, struct argp_state *state __attribute__ ((unused)))
{
  fprintf (stream, "strings (%s) %s\n", PACKAGE_NAME, PACKAGE_VERSION);
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
    case 'a':
      entire_file = true;
      break;

    case 'e':
      
      switch (arg[1] != '\0' ? '\0' : arg[0])
	{
	case 's':
	case 'S':
	  char_7bit = arg[0] == 's';
	  bytes_per_char = 1;
	  break;

	case 'b':
	case 'B':
	  big_endian = true;
	  

	case 'l':
	case 'L':
	  bytes_per_char = isupper (arg[0]) ? 4 : 2;
	  break;

	default:
	  error (0, 0, gettext ("invalid value '%s' for %s parameter"),
		 arg, "-e");
	  argp_help (&argp, stderr, ARGP_HELP_SEE, "strings");
	  return ARGP_ERR_UNKNOWN;
	}
      break;

    case 'f':
      print_file_name = true;
      break;

    case 'n':
      min_len = atoi (arg);
      break;

    case 'o':
      goto octfmt;

    case 't':
      switch (arg[0])
	{
	case 'd':
	  radix = radix_decimal;
	  break;

	case 'o':
	octfmt:
	  radix = radix_octal;
	  break;

	case 'x':
	  radix = radix_hex;
	  break;

	default:
	  error (0, 0, gettext ("invalid value '%s' for %s parameter"),
		 arg, "-t");
	  argp_help (&argp, stderr, ARGP_HELP_SEE, "strings");
	  return ARGP_ERR_UNKNOWN;
	}
      break;

    case ARGP_KEY_FINI:
      
      if (min_len <= 0 || min_len > INT_MAX / bytes_per_char)
	error (EXIT_FAILURE, 0,
	       gettext ("invalid minimum length of matched string size"));
      min_len_bytes = min_len * bytes_per_char;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}


static void
process_chunk_mb (const char *fname, const unsigned char *buf, off64_t to,
		  size_t len, char **unprinted)
{
  size_t curlen = *unprinted == NULL ? 0 : strlen (*unprinted);
  const unsigned char *start = buf;
  while (len >= bytes_per_char)
    {
      uint32_t ch;

      if (bytes_per_char == 2)
	{
	  if (big_endian)
	    ch = buf[0] << 8 | buf[1];
	  else
	    ch = buf[1] << 8 | buf[0];
	}
      else
	{
	  if (big_endian)
	    ch = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
	  else
	    ch = buf[3] << 24 | buf[2] << 16 | buf[1] << 8 | buf[0];
	}

      if (ch <= 255 && (isprint (ch) || ch == '\t'))
	{
	  ++buf;
	  ++curlen;
	}
      else
	{
	  if (curlen >= min_len)
	    {
	      
	      if (unlikely (fname != NULL))
		{
		  fputs_unlocked (fname, stdout);
		  fputs_unlocked (": ", stdout);
		}

	      if (unlikely (radix != radix_none))
		printf ((radix == radix_octal ? "%7" PRIo64 " "
			 : (radix == radix_decimal ? "%7" PRId64 " "
			    : "%7" PRIx64 " ")),
			(int64_t) to - len - (buf - start));

	      if (unlikely (*unprinted != NULL))
		{
		  fputs_unlocked (*unprinted, stdout);
		  free (*unprinted);
		  *unprinted = NULL;
		}

	      fwrite_unlocked (start, 1, buf - start, stdout);
	      putc_unlocked ('\n', stdout);
	    }

	  start = ++buf;
	  curlen =  0;

	  if (len <= min_len)
	    break;
	}

      --len;
    }

  if (curlen != 0)
    *unprinted = xstrndup ((const char *) start, curlen);
}


static void
process_chunk (const char *fname, const unsigned char *buf, off64_t to,
	       size_t len, char **unprinted)
{
  if (unlikely (bytes_per_char != 1))
    {
      process_chunk_mb (fname, buf, to, len, unprinted);
      return;
    }

  size_t curlen = *unprinted == NULL ? 0 : strlen (*unprinted);
  const unsigned char *start = buf;
  while (len > 0)
    {
      if ((isprint (*buf) || *buf == '\t') && (! char_7bit || *buf <= 127))
	{
	  ++buf;
	  ++curlen;
	}
      else
	{
	  if (curlen >= min_len)
	    {
	      
	      if (likely (fname != NULL))
		{
		  fputs_unlocked (fname, stdout);
		  fputs_unlocked (": ", stdout);
		}

	      if (likely (radix != radix_none))
		printf ((radix == radix_octal ? "%7" PRIo64 " "
			 : (radix == radix_decimal ? "%7" PRId64 " "
			    : "%7" PRIx64 " ")),
			(int64_t) to - len - (buf - start));

	      if (unlikely (*unprinted != NULL))
		{
		  fputs_unlocked (*unprinted, stdout);
		  free (*unprinted);
		  *unprinted = NULL;
		}
	      fwrite_unlocked (start, 1, buf - start, stdout);
	      putc_unlocked ('\n', stdout);
	    }

	  start = ++buf;
	  curlen =  0;

	  if (len <= min_len)
	    break;
	}

      --len;
    }

  if (curlen != 0)
    *unprinted = xstrndup ((const char *) start, curlen);
}


static void *
map_file (int fd, off64_t start_off, off64_t fdlen, size_t *map_sizep)
{
# if SIZE_MAX > 0xffffffff
  const size_t mmap_max = 0x4000000000lu;
# else
  const size_t mmap_max = 0x40000000lu;
# endif

  
  size_t map_size = MIN ((off64_t) mmap_max, fdlen);
  const size_t map_size_min = MAX (MAX (SIZE_MAX / 16, 2 * ps),
				   roundup (2 * min_len_bytes + 1, ps));
  void *mem;
  while (1)
    {
      mem = mmap64 (NULL, map_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE,
		    fd, start_off);
      if (mem != MAP_FAILED)
	{
	  
	  (void) posix_madvise (mem, map_size, POSIX_MADV_SEQUENTIAL);
	  break;
	}
      if (errno != EINVAL && errno != ENOMEM)
	
	break;

      
      map_size /= 2;
      if (map_size < map_size_min)
	
	break;
    }

  *map_sizep = map_size;
  return mem;
}


static int
read_block_no_mmap (int fd, const char *fname, off64_t from, off64_t fdlen)
{
  char *unprinted = NULL;
#define CHUNKSIZE 65536
  unsigned char *buf = xmalloc (CHUNKSIZE + min_len_bytes
				+ bytes_per_char - 1);
  size_t ntrailer = 0;
  int result = 0;
  while (fdlen > 0)
    {
      ssize_t n = TEMP_FAILURE_RETRY (read (fd, buf + ntrailer,
					    MIN (fdlen, CHUNKSIZE)));
      if (n == 0)
	{
	  assert (unprinted == NULL || ntrailer == 0);
	  break;
	}
      if (unlikely (n < 0))
	{
	  
	  result = 1;
	  break;
	}

      
      fdlen -= n;

      size_t nb = (size_t) n + ntrailer;
      if (nb >= min_len_bytes)
	{
	  
	  nb &= ~(bytes_per_char - 1);

	  process_chunk (fname, buf, from + nb, nb, &unprinted);

	  size_t to_keep = unprinted != NULL ? 0 : min_len_bytes;

	  memmove (buf, buf + nb - to_keep, to_keep);
	  ntrailer = to_keep;
	  from += nb;
	}
      else
	ntrailer = nb;
    }

  free (buf);

  free (unprinted);

  return result;
}


static int
read_block (int fd, const char *fname, off64_t fdlen, off64_t from, off64_t to)
{
  if (elfmap == NULL)
    {
      
      elfmap_off = from & ~(ps - 1);
      elfmap_base = elfmap = map_file (fd, elfmap_off, fdlen, &elfmap_size);

      if (unlikely (elfmap == MAP_FAILED))
	
	(void) posix_fadvise (fd, 0, 0, POSIX_FADV_SEQUENTIAL);
    }

  if (unlikely (elfmap == MAP_FAILED))
    {
      
      
      if (from != 0 && lseek64 (fd, from, SEEK_SET) != from)
	error (EXIT_FAILURE, errno, gettext ("lseek64 failed"));

      return read_block_no_mmap (fd, fname, from, to - from);
    }

  assert ((off64_t) min_len_bytes < fdlen);

  if (to < (off64_t) elfmap_off || from > (off64_t) (elfmap_off + elfmap_size))
    {
      elfmap_off = from & ~(ps - 1);
      if (mmap64 (elfmap, elfmap_size, PROT_READ,
		  MAP_PRIVATE | MAP_POPULATE | MAP_FIXED, fd, from)
	  == MAP_FAILED)
	error (EXIT_FAILURE, errno, gettext ("re-mmap failed"));
      elfmap_base = elfmap;
    }

  char *unprinted = NULL;

  if (from >= (off64_t) elfmap_off
      && from < (off64_t) (elfmap_off + elfmap_size))
    process_chunk (fname, elfmap_base + (from - elfmap_off),
		   MIN (to, (off64_t) (elfmap_off + elfmap_size)),
		   MIN (to, (off64_t) (elfmap_off + elfmap_size)) - from,
		   &unprinted);

  if (to > (off64_t) (elfmap_off + elfmap_size))
    {
      unsigned char *remap_base = elfmap_base;
      size_t read_now = elfmap_size - (elfmap_base - elfmap);

      assert (from >= (off64_t) elfmap_off
	      && from < (off64_t) (elfmap_off + elfmap_size));
      off64_t handled_to = elfmap_off + elfmap_size;
      assert (elfmap == elfmap_base
	      || (elfmap_base - elfmap
		  == (ptrdiff_t) ((min_len_bytes + ps - 1) & ~(ps - 1))));
      if (elfmap == elfmap_base)
	{
	  size_t keep_area = (min_len_bytes + ps - 1) & ~(ps - 1);
	  assert (elfmap_size >= keep_area + ps);
	  if (unlikely (mprotect (elfmap, keep_area, PROT_READ | PROT_WRITE)
			!= 0))
	    error (EXIT_FAILURE, errno, gettext ("mprotect failed"));

	  elfmap_base = elfmap + keep_area;
	}

      while (1)
	{
	  size_t to_keep = unprinted != NULL ? 0 : min_len_bytes;

	  assert (read_now >= to_keep);
	  memmove (elfmap_base - to_keep,
		   remap_base + read_now - to_keep, to_keep);
	  remap_base = elfmap_base;

	  assert ((elfmap_size - (elfmap_base - elfmap)) % bytes_per_char
		  == 0);
	  read_now = MIN (to - handled_to,
			  (ptrdiff_t) elfmap_size - (elfmap_base - elfmap));

	  assert (handled_to % ps == 0);
	  assert (handled_to % bytes_per_char == 0);
	  if (mmap64 (remap_base, read_now, PROT_READ,
		      MAP_PRIVATE | MAP_POPULATE | MAP_FIXED, fd, handled_to)
	      == MAP_FAILED)
	    error (EXIT_FAILURE, errno, gettext ("re-mmap failed"));
	  elfmap_off = handled_to;

	  process_chunk (fname, remap_base - to_keep,
			 elfmap_off + (read_now & ~(bytes_per_char - 1)),
			 to_keep + (read_now & ~(bytes_per_char - 1)),
			 &unprinted);
	  handled_to += read_now;
	  if (handled_to >= to)
	    break;
	}
    }

  free (unprinted);

  return 0;
}


static int
read_fd (int fd, const char *fname, off64_t fdlen)
{
  return read_block (fd, fname, fdlen, 0, fdlen);
}


static int
read_elf (Elf *elf, int fd, const char *fname, off64_t fdlen)
{
  assert (fdlen >= 0);

  Elf_Scn *scn = elf_nextscn (elf, NULL);
  if (scn == NULL)
    return read_fd (fd, fname, fdlen);

  int result = 0;
  do
    {
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);

      if (shdr != NULL && shdr->sh_type != SHT_NOBITS
	  && (shdr->sh_flags & SHF_ALLOC) != 0)
	result |= read_block (fd, fname, fdlen, shdr->sh_offset,
			      shdr->sh_offset + shdr->sh_size);
    }
  while ((scn = elf_nextscn (elf, scn)) != NULL);

  if (elfmap != NULL && elfmap != MAP_FAILED)
    munmap (elfmap, elfmap_size);
  elfmap = NULL;

  return result;
}


#include "debugpred.h"
