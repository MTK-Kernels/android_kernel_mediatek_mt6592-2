/* Create descriptor from file descriptor for processing file.
   Copyright (C) 2002, 2003, 2004, 2005 Red Hat, Inc.
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

#include <errno.h>
#include <stddef.h>
#include <sys/stat.h>

#include <libdwP.h>


Dwarf *
dwarf_begin (fd, cmd)
     int fd;
     Dwarf_Cmd cmd;
{
  Elf *elf;
  Elf_Cmd elfcmd;
  Dwarf *result = NULL;

  switch (cmd)
    {
    case DWARF_C_READ:
      elfcmd = ELF_C_READ_MMAP;
      break;
    case DWARF_C_WRITE:
      elfcmd = ELF_C_WRITE;
      break;
    case DWARF_C_RDWR:
      elfcmd = ELF_C_RDWR;
      break;
    default:
      
      __libdw_seterrno (DWARF_E_INVALID_CMD);
      return NULL;
    }

  elf_version (EV_CURRENT);

  
  elf = elf_begin (fd, elfcmd, NULL);
  if (elf == NULL)
    {
      
      struct stat64 st;

      if (fstat64 (fd, &st) == 0 && ! S_ISREG (st.st_mode))
	__libdw_seterrno (DWARF_E_NO_REGFILE);
      else if (errno == EBADF)
	__libdw_seterrno (DWARF_E_INVALID_FILE);
      else
	__libdw_seterrno (DWARF_E_IO_ERROR);
    }
  else
    {
      
      result = INTUSE(dwarf_begin_elf) (elf, cmd, NULL);

      
      if (result == NULL)
	elf_end (elf);
      else
	result->free_elf = true;
    }

  return result;
}
INTDEF(dwarf_begin)
