/* Copyright (C) 1998, 1999, 2000, 2002 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 1998.

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

#include <config.h>

#include <error.h>
#include <fcntl.h>
#include <gelf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>


static const char *machines[] =
{
#define MACHINE(name) [name] = #name
  MACHINE (EM_NONE),
  MACHINE (EM_M32),
  MACHINE (EM_SPARC),
  MACHINE (EM_386),
  MACHINE (EM_68K),
  MACHINE (EM_88K),
  MACHINE (EM_860),
  MACHINE (EM_MIPS),
  MACHINE (EM_MIPS_RS3_LE),
  MACHINE (EM_PARISC),
  MACHINE (EM_VPP500),
  MACHINE (EM_SPARC32PLUS),
  MACHINE (EM_960),
  MACHINE (EM_PPC),
  MACHINE (EM_PPC64),
  MACHINE (EM_V800),
  MACHINE (EM_FR20),
  MACHINE (EM_RH32),
  MACHINE (EM_RCE),
  MACHINE (EM_ARM),
  MACHINE (EM_FAKE_ALPHA),
  MACHINE (EM_SH),
  MACHINE (EM_SPARCV9),
  MACHINE (EM_TRICORE),
  MACHINE (EM_ARC),
  MACHINE (EM_H8_300),
  MACHINE (EM_H8_300H),
  MACHINE (EM_H8S),
  MACHINE (EM_H8_500),
  MACHINE (EM_IA_64),
  MACHINE (EM_MIPS_X),
  MACHINE (EM_COLDFIRE),
  MACHINE (EM_68HC12),
  MACHINE (EM_MMA),
  MACHINE (EM_PCP),
  MACHINE (EM_NCPU),
  MACHINE (EM_NDR1),
  MACHINE (EM_STARCORE),
  MACHINE (EM_ME16),
  MACHINE (EM_ST100),
  MACHINE (EM_TINYJ),
  MACHINE (EM_FX66),
  MACHINE (EM_ST9PLUS),
  MACHINE (EM_ST7),
  MACHINE (EM_68HC16),
  MACHINE (EM_68HC11),
  MACHINE (EM_68HC08),
  MACHINE (EM_68HC05),
  MACHINE (EM_SVX),
  MACHINE (EM_ST19),
  MACHINE (EM_VAX)
};


int
main (int argc, char *argv[])
{
  int fd;
  Elf *elf;
  Elf_Cmd cmd;
  size_t n;
  int arg = 1;
  int verbose = 0;

  
  if (arg < argc && strcmp (argv[arg], "-v") == 0)
    {
      verbose = 1;
      ++arg;
    }

  
  if (arg >= argc)
    error (EXIT_FAILURE, 0, "No input file given");

  
  fd = open (argv[arg], O_RDONLY);
  if (fd == -1)
    {
      perror ("cannot open input file");
      exit (1);
    }

  
  if (elf_version (EV_CURRENT) == EV_NONE)
    {
      puts ("ELF library too old");
      exit (1);
    }

  
  cmd = ELF_C_READ;
  elf = elf_begin (fd, cmd, NULL);
  if (elf == NULL)
    {
      printf ("elf_begin: %s\n", elf_errmsg (-1));
      exit (1);
    }

  
  if (elf_kind (elf) != ELF_K_AR)
    {
      printf ("%s is not an archive\n", argv[1]);
      exit (1);
    }

  if (verbose)
    {
      
      Elf *subelf;
      char buf[100];
      time_t t;

      
      while ((subelf = elf_begin (fd, cmd, elf)) != NULL)
	{
	  
	  Elf_Arhdr *arhdr = elf_getarhdr (subelf);

	  if (arhdr == NULL)
	    {
	      printf ("cannot get arhdr: %s\n", elf_errmsg (-1));
	      break;
	    }

	  switch (elf_kind (subelf))
	    {
	    case ELF_K_ELF:
	      fputs ("ELF file:\n", stdout);
	      break;

	    case ELF_K_AR:
	      fputs ("archive:\n", stdout);
	      break;

	    default:
	      fputs ("unknown file:\n", stdout);
	      break;
	    }

	  
	  t = arhdr->ar_date;
	  strftime (buf, sizeof buf, "%Y-%m-%dT%H:%M:%S%z", gmtime (&t));
	  printf ("  name         : \"%s\"\n"
		  "  time         : %s\n"
		  "  uid          : %ld\n"
		  "  gid          : %ld\n"
		  "  mode         : %o\n"
		  "  size         : %ld\n"
		  "  rawname      : \"%s\"\n",
		  arhdr->ar_name,
		  buf,
		  (long int) arhdr->ar_uid,
		  (long int) arhdr->ar_gid,
		  arhdr->ar_mode,
		  (long int) arhdr->ar_size,
		  arhdr->ar_rawname);

	  
	  if (elf_kind (subelf) == ELF_K_ELF)
	    {
	      GElf_Ehdr ehdr;

	      
	      if (gelf_getehdr (subelf, &ehdr) == NULL)
		printf ("  *** cannot get ELF header: %s\n", elf_errmsg (-1));
	      else
		{
		  printf ("  binary class : %s\n",
			  ehdr.e_ident[EI_CLASS] == ELFCLASS32
			  ? "ELFCLASS32" : "ELFCLASS64");
		  printf ("  data encoding: %s\n",
			  ehdr.e_ident[EI_DATA] == ELFDATA2LSB
			  ? "ELFDATA2LSB" : "ELFDATA2MSB");
		  printf ("  binary type  : %s\n",
			  ehdr.e_type == ET_REL
			  ? "relocatable"
			  : (ehdr.e_type == ET_EXEC
			     ? "executable"
			     : (ehdr.e_type == ET_DYN
				? "dynamic"
				: "core file")));
		  printf ("  machine      : %s\n",
			  (ehdr.e_machine >= (sizeof (machines)
					      / sizeof (machines[0]))
			   || machines[ehdr.e_machine] == NULL)
			  ? "???"
			  : machines[ehdr.e_machine]);
		}
	    }

	  
	  cmd = elf_next (subelf);
	  if (elf_end (subelf) != 0)
	    printf ("error while freeing sub-ELF descriptor: %s\n",
		    elf_errmsg (-1));
	}
    }
  else
    {
      
      Elf_Arsym *arsym = elf_getarsym (elf, &n);

      if (n == 0)
	printf ("no symbol table in archive: %s\n", elf_errmsg (-1));
      else
	{
	  --n;

	  while (n-- > 0)
	    printf ("name = \"%s\", offset = %ld, hash = %lx\n",
		    arsym[n].as_name, (long int) arsym[n].as_off,
		    arsym[n].as_hash);
	}
    }

  
  if (elf_end (elf) != 0)
    printf ("error while freeing ELF descriptor: %s\n", elf_errmsg (-1));

  
  close (fd);

  return 0;
}
