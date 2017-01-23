/* Create descriptor for processing file.
   Copyright (C) 1998-2010, 2012, 2014 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 1998.

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
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <system.h>
#include "libelfP.h"
#include "common.h"


static inline Elf *
file_read_ar (int fildes, void *map_address, off_t offset, size_t maxsize,
	      Elf_Cmd cmd, Elf *parent)
{
  Elf *elf;

  
  elf = allocate_elf (fildes, map_address, offset, maxsize, cmd, parent,
                      ELF_K_AR, 0);
  if (elf != NULL)
    {
      elf->state.ar.offset = offset + SARMAG;

      elf->state.ar.elf_ar_hdr.ar_rawname = elf->state.ar.raw_name;
    }

  return elf;
}


static size_t
get_shnum (void *map_address, unsigned char *e_ident, int fildes, off_t offset,
	   size_t maxsize)
{
  size_t result;
  union
  {
    Elf32_Ehdr *e32;
    Elf64_Ehdr *e64;
    void *p;
  } ehdr;
  union
  {
    Elf32_Ehdr e32;
    Elf64_Ehdr e64;
  } ehdr_mem;
  bool is32 = e_ident[EI_CLASS] == ELFCLASS32;

  
  if (e_ident[EI_DATA] == MY_ELFDATA
      && (ALLOW_UNALIGNED
	  || (((size_t) e_ident
	       & ((is32 ? __alignof__ (Elf32_Ehdr) : __alignof__ (Elf64_Ehdr))
		  - 1)) == 0)))
    ehdr.p = e_ident;
  else
    {
      ehdr.p = &ehdr_mem;

      if (is32)
	{
	  if (ALLOW_UNALIGNED)
	    {
	      ehdr_mem.e32.e_shnum = ((Elf32_Ehdr *) e_ident)->e_shnum;
	      ehdr_mem.e32.e_shoff = ((Elf32_Ehdr *) e_ident)->e_shoff;
	    }
	  else
	    memcpy (&ehdr_mem, e_ident, sizeof (Elf32_Ehdr));

	  if (e_ident[EI_DATA] != MY_ELFDATA)
	    {
	      CONVERT (ehdr_mem.e32.e_shnum);
	      CONVERT (ehdr_mem.e32.e_shoff);
	    }
	}
      else
	{
	  if (ALLOW_UNALIGNED)
	    {
	      ehdr_mem.e64.e_shnum = ((Elf64_Ehdr *) e_ident)->e_shnum;
	      ehdr_mem.e64.e_shoff = ((Elf64_Ehdr *) e_ident)->e_shoff;
	    }
	  else
	    memcpy (&ehdr_mem, e_ident, sizeof (Elf64_Ehdr));

	  if (e_ident[EI_DATA] != MY_ELFDATA)
	    {
	      CONVERT (ehdr_mem.e64.e_shnum);
	      CONVERT (ehdr_mem.e64.e_shoff);
	    }
	}
    }

  if (is32)
    {
      
      result = ehdr.e32->e_shnum;

      if (unlikely (result == 0) && ehdr.e32->e_shoff != 0)
	{
	  if (unlikely (ehdr.e32->e_shoff >= maxsize)
	      || unlikely (maxsize - ehdr.e32->e_shoff < sizeof (Elf32_Shdr)))
	    
	    return 0;

	  if (likely (map_address != NULL) && e_ident[EI_DATA] == MY_ELFDATA
	      && (ALLOW_UNALIGNED
		  || (((size_t) ((char *) map_address + offset))
		      & (__alignof__ (Elf32_Ehdr) - 1)) == 0))
	    
	    result = ((Elf32_Shdr *) ((char *) map_address + ehdr.e32->e_shoff
				      + offset))->sh_size;
	  else
	    {
	      Elf32_Word size;

	      if (likely (map_address != NULL))
		memcpy (&size, &((Elf32_Shdr *) ((char *) map_address
						 + ehdr.e32->e_shoff
						 + offset))->sh_size,
			sizeof (Elf32_Word));
	      else
		if (unlikely (pread_retry (fildes, &size, sizeof (Elf32_Word),
					   offset + ehdr.e32->e_shoff
					   + offsetof (Elf32_Shdr, sh_size))
			      != sizeof (Elf32_Word)))
		  return (size_t) -1l;

	      if (e_ident[EI_DATA] != MY_ELFDATA)
		CONVERT (size);

	      result = size;
	    }
	}

      
      if (ehdr.e32->e_shoff > maxsize
	  || maxsize - ehdr.e32->e_shoff < sizeof (Elf32_Shdr) * result)
	result = 0;
    }
  else
    {
      
      result = ehdr.e64->e_shnum;

      if (unlikely (result == 0) && ehdr.e64->e_shoff != 0)
	{
	  if (unlikely (ehdr.e64->e_shoff >= maxsize)
	      || unlikely (ehdr.e64->e_shoff + sizeof (Elf64_Shdr) > maxsize))
	    
	    return 0;

	  Elf64_Xword size;
	  if (likely (map_address != NULL) && e_ident[EI_DATA] == MY_ELFDATA
	      && (ALLOW_UNALIGNED
		  || (((size_t) ((char *) map_address + offset))
		      & (__alignof__ (Elf64_Ehdr) - 1)) == 0))
	    
	    size = ((Elf64_Shdr *) ((char *) map_address + ehdr.e64->e_shoff
				    + offset))->sh_size;
	  else
	    {
	      if (likely (map_address != NULL))
		memcpy (&size, &((Elf64_Shdr *) ((char *) map_address
						 + ehdr.e64->e_shoff
						 + offset))->sh_size,
			sizeof (Elf64_Xword));
	      else
		if (unlikely (pread_retry (fildes, &size, sizeof (Elf64_Word),
					   offset + ehdr.e64->e_shoff
					   + offsetof (Elf64_Shdr, sh_size))
			      != sizeof (Elf64_Xword)))
		  return (size_t) -1l;

	      if (e_ident[EI_DATA] != MY_ELFDATA)
		CONVERT (size);
	    }

	  if (size > ~((GElf_Word) 0))
	    
	    return (size_t) -1l;

	  result = size;
	}

      
      if (ehdr.e64->e_shoff > maxsize
	  || maxsize - ehdr.e64->e_shoff < sizeof (Elf64_Shdr) * result)
	result = 0;
    }

  return result;
}


static Elf *
file_read_elf (int fildes, void *map_address, unsigned char *e_ident,
	       off_t offset, size_t maxsize, Elf_Cmd cmd, Elf *parent)
{
  
  if (unlikely ((e_ident[EI_CLASS] != ELFCLASS32
		 && e_ident[EI_CLASS] != ELFCLASS64)
		
		|| (e_ident[EI_DATA] != ELFDATA2LSB
		    && e_ident[EI_DATA] != ELFDATA2MSB)))
    {
      
      __libelf_seterrno (ELF_E_INVALID_FILE);
      return NULL;
    }

  
  size_t scncnt = get_shnum (map_address, e_ident, fildes, offset, maxsize);
  if (scncnt == (size_t) -1l)
    
    return NULL;

  
  if (e_ident[EI_CLASS] == ELFCLASS32)
    {
      if (scncnt > SIZE_MAX / (sizeof (Elf_Scn) + sizeof (Elf32_Shdr)))
	return NULL;
    }
  else if (scncnt > SIZE_MAX / (sizeof (Elf_Scn) + sizeof (Elf64_Shdr)))
    return NULL;

  const size_t scnmax = (scncnt ?: (cmd == ELF_C_RDWR || cmd == ELF_C_RDWR_MMAP)
			 ? 1 : 0);
  Elf *elf = allocate_elf (fildes, map_address, offset, maxsize, cmd, parent,
			   ELF_K_ELF, scnmax * sizeof (Elf_Scn));
  if (elf == NULL)
    
    return NULL;

  assert ((unsigned int) scncnt == scncnt);
  assert (offsetof (struct Elf, state.elf32.scns)
	  == offsetof (struct Elf, state.elf64.scns));
  elf->state.elf32.scns.cnt = scncnt;
  elf->state.elf32.scns.max = scnmax;

  
  elf->state.elf.scnincr = 10;

  
  elf->class = e_ident[EI_CLASS];

  if (e_ident[EI_CLASS] == ELFCLASS32)
    {
      Elf32_Ehdr *ehdr = (Elf32_Ehdr *) ((char *) map_address + offset);

      
      if (map_address != NULL && e_ident[EI_DATA] == MY_ELFDATA
	  && (ALLOW_UNALIGNED
	      || ((((uintptr_t) ehdr) & (__alignof__ (Elf32_Ehdr) - 1)) == 0
		  && ((uintptr_t) ((char *) ehdr + ehdr->e_shoff)
		      & (__alignof__ (Elf32_Shdr) - 1)) == 0
		  && ((uintptr_t) ((char *) ehdr + ehdr->e_phoff)
		      & (__alignof__ (Elf32_Phdr) - 1)) == 0)))
	{
	  
	  elf->state.elf32.ehdr = ehdr;

	  if (unlikely (ehdr->e_shoff >= maxsize)
	      || unlikely (maxsize - ehdr->e_shoff
			   < scncnt * sizeof (Elf32_Shdr)))
	    {
	    free_and_out:
	      free (elf);
	      __libelf_seterrno (ELF_E_INVALID_FILE);
	      return NULL;
	    }
	  elf->state.elf32.shdr
	    = (Elf32_Shdr *) ((char *) ehdr + ehdr->e_shoff);


	  for (size_t cnt = 0; cnt < scncnt; ++cnt)
	    {
	      elf->state.elf32.scns.data[cnt].index = cnt;
	      elf->state.elf32.scns.data[cnt].elf = elf;
	      elf->state.elf32.scns.data[cnt].shdr.e32 =
		&elf->state.elf32.shdr[cnt];
	      if (likely (elf->state.elf32.shdr[cnt].sh_offset < maxsize)
		  && likely (elf->state.elf32.shdr[cnt].sh_size
			     <= maxsize - elf->state.elf32.shdr[cnt].sh_offset))
		elf->state.elf32.scns.data[cnt].rawdata_base =
		  elf->state.elf32.scns.data[cnt].data_base =
		  ((char *) map_address + offset
		   + elf->state.elf32.shdr[cnt].sh_offset);
	      elf->state.elf32.scns.data[cnt].list = &elf->state.elf32.scns;

	      if (elf->state.elf32.shdr[cnt].sh_type == SHT_SYMTAB_SHNDX
		  && elf->state.elf32.shdr[cnt].sh_link < scncnt)
		elf->state.elf32.scns.data[elf->state.elf32.shdr[cnt].sh_link].shndx_index
		  = cnt;

	      if (elf->state.elf32.scns.data[cnt].shndx_index == 0)
		elf->state.elf32.scns.data[cnt].shndx_index = -1;
	    }
	}
      else
	{
	  
	  elf->state.elf32.ehdr = memcpy (&elf->state.elf32.ehdr_mem, e_ident,
					  sizeof (Elf32_Ehdr));

	  if (e_ident[EI_DATA] != MY_ELFDATA)
	    {
	      CONVERT (elf->state.elf32.ehdr_mem.e_type);
	      CONVERT (elf->state.elf32.ehdr_mem.e_machine);
	      CONVERT (elf->state.elf32.ehdr_mem.e_version);
	      CONVERT (elf->state.elf32.ehdr_mem.e_entry);
	      CONVERT (elf->state.elf32.ehdr_mem.e_phoff);
	      CONVERT (elf->state.elf32.ehdr_mem.e_shoff);
	      CONVERT (elf->state.elf32.ehdr_mem.e_flags);
	      CONVERT (elf->state.elf32.ehdr_mem.e_ehsize);
	      CONVERT (elf->state.elf32.ehdr_mem.e_phentsize);
	      CONVERT (elf->state.elf32.ehdr_mem.e_phnum);
	      CONVERT (elf->state.elf32.ehdr_mem.e_shentsize);
	      CONVERT (elf->state.elf32.ehdr_mem.e_shnum);
	      CONVERT (elf->state.elf32.ehdr_mem.e_shstrndx);
	    }

	  for (size_t cnt = 0; cnt < scncnt; ++cnt)
	    {
	      elf->state.elf32.scns.data[cnt].index = cnt;
	      elf->state.elf32.scns.data[cnt].elf = elf;
	      elf->state.elf32.scns.data[cnt].list = &elf->state.elf32.scns;
	    }
	}

      
      elf->state.elf32.scns_last = &elf->state.elf32.scns;
    }
  else
    {
      Elf64_Ehdr *ehdr = (Elf64_Ehdr *) ((char *) map_address + offset);

      
      if (map_address != NULL && e_ident[EI_DATA] == MY_ELFDATA
	  && (ALLOW_UNALIGNED
	      || ((((uintptr_t) ehdr) & (__alignof__ (Elf64_Ehdr) - 1)) == 0
		  && ((uintptr_t) ((char *) ehdr + ehdr->e_shoff)
		      & (__alignof__ (Elf64_Shdr) - 1)) == 0
		  && ((uintptr_t) ((char *) ehdr + ehdr->e_phoff)
		      & (__alignof__ (Elf64_Phdr) - 1)) == 0)))
	{
	  
	  elf->state.elf64.ehdr = ehdr;

	  if (unlikely (ehdr->e_shoff >= maxsize)
	      || unlikely (maxsize - ehdr->e_shoff
			   < scncnt * sizeof (Elf64_Shdr)))
	    goto free_and_out;
	  elf->state.elf64.shdr
	    = (Elf64_Shdr *) ((char *) ehdr + ehdr->e_shoff);


	  for (size_t cnt = 0; cnt < scncnt; ++cnt)
	    {
	      elf->state.elf64.scns.data[cnt].index = cnt;
	      elf->state.elf64.scns.data[cnt].elf = elf;
	      elf->state.elf64.scns.data[cnt].shdr.e64 =
		&elf->state.elf64.shdr[cnt];
	      if (likely (elf->state.elf64.shdr[cnt].sh_offset < maxsize)
		  && likely (elf->state.elf64.shdr[cnt].sh_size
			     <= maxsize - elf->state.elf64.shdr[cnt].sh_offset))
		elf->state.elf64.scns.data[cnt].rawdata_base =
		  elf->state.elf64.scns.data[cnt].data_base =
		  ((char *) map_address + offset
		   + elf->state.elf64.shdr[cnt].sh_offset);
	      elf->state.elf64.scns.data[cnt].list = &elf->state.elf64.scns;

	      if (elf->state.elf64.shdr[cnt].sh_type == SHT_SYMTAB_SHNDX
		  && elf->state.elf64.shdr[cnt].sh_link < scncnt)
		elf->state.elf64.scns.data[elf->state.elf64.shdr[cnt].sh_link].shndx_index
		  = cnt;

	      if (elf->state.elf64.scns.data[cnt].shndx_index == 0)
		elf->state.elf64.scns.data[cnt].shndx_index = -1;
	    }
	}
      else
	{
	  
	  elf->state.elf64.ehdr = memcpy (&elf->state.elf64.ehdr_mem, e_ident,
					  sizeof (Elf64_Ehdr));

	  if (e_ident[EI_DATA] != MY_ELFDATA)
	    {
	      CONVERT (elf->state.elf64.ehdr_mem.e_type);
	      CONVERT (elf->state.elf64.ehdr_mem.e_machine);
	      CONVERT (elf->state.elf64.ehdr_mem.e_version);
	      CONVERT (elf->state.elf64.ehdr_mem.e_entry);
	      CONVERT (elf->state.elf64.ehdr_mem.e_phoff);
	      CONVERT (elf->state.elf64.ehdr_mem.e_shoff);
	      CONVERT (elf->state.elf64.ehdr_mem.e_flags);
	      CONVERT (elf->state.elf64.ehdr_mem.e_ehsize);
	      CONVERT (elf->state.elf64.ehdr_mem.e_phentsize);
	      CONVERT (elf->state.elf64.ehdr_mem.e_phnum);
	      CONVERT (elf->state.elf64.ehdr_mem.e_shentsize);
	      CONVERT (elf->state.elf64.ehdr_mem.e_shnum);
	      CONVERT (elf->state.elf64.ehdr_mem.e_shstrndx);
	    }

	  for (size_t cnt = 0; cnt < scncnt; ++cnt)
	    {
	      elf->state.elf64.scns.data[cnt].index = cnt;
	      elf->state.elf64.scns.data[cnt].elf = elf;
	      elf->state.elf64.scns.data[cnt].list = &elf->state.elf64.scns;
	    }
	}

      
      elf->state.elf64.scns_last = &elf->state.elf64.scns;
    }

  return elf;
}


Elf *
internal_function
__libelf_read_mmaped_file (int fildes, void *map_address,  off_t offset,
			   size_t maxsize, Elf_Cmd cmd, Elf *parent)
{
  unsigned char *e_ident = (unsigned char *) map_address + offset;

  
  Elf_Kind kind = determine_kind (e_ident, maxsize);

  switch (kind)
    {
    case ELF_K_ELF:
      return file_read_elf (fildes, map_address, e_ident, offset, maxsize,
			    cmd, parent);

    case ELF_K_AR:
      return file_read_ar (fildes, map_address, offset, maxsize, cmd, parent);

    default:
      break;
    }

  return allocate_elf (fildes, map_address, offset, maxsize, cmd, parent,
		       ELF_K_NONE, 0);
}


static Elf *
read_unmmaped_file (int fildes, off_t offset, size_t maxsize, Elf_Cmd cmd,
		    Elf *parent)
{
  union
  {
    Elf64_Ehdr ehdr;
    unsigned char header[MAX (sizeof (Elf64_Ehdr), SARMAG)];
  } mem;

  
  ssize_t nread = pread_retry (fildes, mem.header,
			       MIN (MAX (sizeof (Elf64_Ehdr), SARMAG),
				    maxsize),
			       offset);
  if (unlikely (nread == -1))
    {
      __libelf_seterrno (ELF_E_INVALID_FILE);
      return NULL;
    }

  
  Elf_Kind kind = determine_kind (mem.header, nread);

  switch (kind)
    {
    case ELF_K_AR:
      return file_read_ar (fildes, NULL, offset, maxsize, cmd, parent);

    case ELF_K_ELF:
      
      if ((size_t) nread >= (mem.header[EI_CLASS] == ELFCLASS32
			     ? sizeof (Elf32_Ehdr) : sizeof (Elf64_Ehdr)))
	return file_read_elf (fildes, NULL, mem.header, offset, maxsize, cmd,
			      parent);
      

    default:
      break;
    }

  return allocate_elf (fildes, NULL, offset, maxsize, cmd, parent,
		       ELF_K_NONE, 0);
}


static struct Elf *
read_file (int fildes, off_t offset, size_t maxsize,
	   Elf_Cmd cmd, Elf *parent)
{
  void *map_address = NULL;
  int use_mmap = (cmd == ELF_C_READ_MMAP || cmd == ELF_C_RDWR_MMAP
		  || cmd == ELF_C_WRITE_MMAP
		  || cmd == ELF_C_READ_MMAP_PRIVATE);

  if (use_mmap)
    {
      if (parent == NULL)
	{
	  if (maxsize == ~((size_t) 0))
	    {
	      struct stat st;

	      if (fstat (fildes, &st) == 0
		  && (sizeof (size_t) >= sizeof (st.st_size)
		      || st.st_size <= ~((size_t) 0)))
		maxsize = (size_t) st.st_size;
	    }

	  
	  map_address = mmap (NULL, maxsize, (cmd == ELF_C_READ_MMAP
					      ? PROT_READ
					      : PROT_READ|PROT_WRITE),
			      cmd == ELF_C_READ_MMAP_PRIVATE
			      || cmd == ELF_C_READ_MMAP
			      ? MAP_PRIVATE : MAP_SHARED,
			      fildes, offset);

	  if (map_address == MAP_FAILED)
	    map_address = NULL;
	}
      else
	{
	  
	  assert (maxsize != ~((size_t) 0));

	  map_address = parent->map_address;
	}
    }

  
  if (map_address != NULL)
    {
      assert (map_address != MAP_FAILED);

      struct Elf *result = __libelf_read_mmaped_file (fildes, map_address,
						      offset, maxsize, cmd,
						      parent);

      if (result == NULL
	  && (parent == NULL
	      || parent->map_address != map_address))
	munmap (map_address, maxsize);
      else if (parent == NULL)
	
	result->flags |= ELF_F_MMAPPED;

      return result;
    }

  return read_unmmaped_file (fildes, offset, maxsize, cmd, parent);
}


static const char *
read_long_names (Elf *elf)
{
  off_t offset = SARMAG;	
  struct ar_hdr hdrm;
  struct ar_hdr *hdr;
  char *newp;
  size_t len;

  while (1)
    {
      if (elf->map_address != NULL)
	{
	  if ((size_t) offset > elf->maximum_size
	      || elf->maximum_size - offset < sizeof (struct ar_hdr))
	    return NULL;

	  
	  hdr = (struct ar_hdr *) (elf->map_address + offset);
	}
      else
	{
	  
	  if (unlikely (pread_retry (elf->fildes, &hdrm, sizeof (hdrm),
				     elf->start_offset + offset)
			!= sizeof (hdrm)))
	    return NULL;

	  hdr = &hdrm;
	}

      len = atol (hdr->ar_size);

      if (memcmp (hdr->ar_name, "//              ", 16) == 0)
	break;

      offset += sizeof (struct ar_hdr) + ((len + 1) & ~1l);
    }

  newp = (char *) malloc (len);
  if (newp != NULL)
    {
      char *runp;

      if (elf->map_address != NULL)
	{
	  if (len > elf->maximum_size - offset - sizeof (struct ar_hdr))
	    goto too_much;
	  
	  elf->state.ar.long_names = (char *) memcpy (newp,
						      elf->map_address + offset
						      + sizeof (struct ar_hdr),
						      len);
	}
      else
	{
	  if (unlikely ((size_t) pread_retry (elf->fildes, newp, len,
					      elf->start_offset + offset
					      + sizeof (struct ar_hdr))
			!= len))
	    {
	    too_much:
	      
	      free (newp);
	      elf->state.ar.long_names = NULL;
	      return NULL;
	    }
	  elf->state.ar.long_names = newp;
	}

      elf->state.ar.long_names_len = len;

      
      runp = newp;
      while (1)
        {
	  char *startp = runp;
	  runp = (char *) memchr (runp, '/', newp + len - runp);
	  if (runp == NULL)
	    {
	      
	      memset (startp, '\0', newp + len - startp);
	      break;
	    }

	  
	  *runp = '\0';

	  
	  runp += 2;

	  if (runp >= newp + len)
	    break;
	}
    }

  return newp;
}


int
internal_function
__libelf_next_arhdr_wrlock (elf)
     Elf *elf;
{
  struct ar_hdr *ar_hdr;
  Elf_Arhdr *elf_ar_hdr;

  if (elf->map_address != NULL)
    {
      
      if (unlikely ((size_t) elf->state.ar.offset
		    > elf->start_offset + elf->maximum_size
		    || (elf->start_offset + elf->maximum_size
			- elf->state.ar.offset) < sizeof (struct ar_hdr)))
	{
	  
	  __libelf_seterrno (ELF_E_RANGE);
	  return -1;
	}
      ar_hdr = (struct ar_hdr *) (elf->map_address + elf->state.ar.offset);
    }
  else
    {
      ar_hdr = &elf->state.ar.ar_hdr;

      if (unlikely (pread_retry (elf->fildes, ar_hdr, sizeof (struct ar_hdr),
				 elf->state.ar.offset)
		    != sizeof (struct ar_hdr)))
	{
	  
	  __libelf_seterrno (ELF_E_RANGE);
	  return -1;
	}
    }

  
  if (unlikely (memcmp (ar_hdr->ar_fmag, ARFMAG, 2) != 0))
    {
      
      __libelf_seterrno (ELF_E_ARCHIVE_FMAG);
      return -1;
    }

  
  *((char *) mempcpy (elf->state.ar.raw_name, ar_hdr->ar_name, 16)) = '\0';

  elf_ar_hdr = &elf->state.ar.elf_ar_hdr;

  if (ar_hdr->ar_name[0] == '/')
    {
      if (ar_hdr->ar_name[1] == ' '
	  && memcmp (ar_hdr->ar_name, "/               ", 16) == 0)
	
	elf_ar_hdr->ar_name = memcpy (elf->state.ar.ar_name, "/", 2);
      else if (ar_hdr->ar_name[1] == 'S'
	       && memcmp (ar_hdr->ar_name, "/SYM64/         ", 16) == 0)
	
	elf_ar_hdr->ar_name = memcpy (elf->state.ar.ar_name, "/SYM64/", 8);
      else if (ar_hdr->ar_name[1] == '/'
	       && memcmp (ar_hdr->ar_name, "//              ", 16) == 0)
	
	elf_ar_hdr->ar_name = memcpy (elf->state.ar.ar_name, "//", 3);
      else if (likely  (isdigit (ar_hdr->ar_name[1])))
	{
	  size_t offset;

	  if (unlikely (elf->state.ar.long_names == NULL
			&& read_long_names (elf) == NULL))
	    {
	      __libelf_seterrno (ELF_E_INVALID_ARCHIVE);
	      return -1;
	    }

	  offset = atol (ar_hdr->ar_name + 1);
	  if (unlikely (offset >= elf->state.ar.long_names_len))
	    {
	      
	      __libelf_seterrno (ELF_E_INVALID_ARCHIVE);
	      return -1;
	    }
	  elf_ar_hdr->ar_name = elf->state.ar.long_names + offset;
	}
      else
	{
	  
	  __libelf_seterrno (ELF_E_INVALID_ARCHIVE);
	  return -1;
	}
    }
  else
    {
      char *endp;

      
      endp = (char *) memccpy (elf->state.ar.ar_name, ar_hdr->ar_name,
			       '/', 16);
      if (endp != NULL)
	endp[-1] = '\0';
      else
	{
	  size_t i = 15;
	  do
	    elf->state.ar.ar_name[i] = '\0';
	  while (i > 0 && elf->state.ar.ar_name[--i] == ' ');
	}

      elf_ar_hdr->ar_name = elf->state.ar.ar_name;
    }

  if (unlikely (ar_hdr->ar_size[0] == ' '))
    {
      __libelf_seterrno (ELF_E_INVALID_ARCHIVE);
      return -1;
    }


#define INT_FIELD(FIELD)						      \
  do									      \
    {									      \
      char buf[sizeof (ar_hdr->FIELD) + 1];				      \
      const char *string = ar_hdr->FIELD;				      \
      if (ar_hdr->FIELD[sizeof (ar_hdr->FIELD) - 1] != ' ')		      \
	{								      \
	  *((char *) mempcpy (buf, ar_hdr->FIELD, sizeof (ar_hdr->FIELD)))  \
	    = '\0';							      \
	  string = buf;							      \
	}								      \
      if (sizeof (elf_ar_hdr->FIELD) <= sizeof (long int))		      \
	elf_ar_hdr->FIELD = (__typeof (elf_ar_hdr->FIELD)) atol (string);     \
      else								      \
	elf_ar_hdr->FIELD = (__typeof (elf_ar_hdr->FIELD)) atoll (string);    \
    }									      \
  while (0)

  INT_FIELD (ar_date);
  INT_FIELD (ar_uid);
  INT_FIELD (ar_gid);
  INT_FIELD (ar_mode);
  INT_FIELD (ar_size);

  
  size_t maxsize;
  maxsize = elf->maximum_size - elf->state.ar.offset - sizeof (struct ar_hdr);
  if ((size_t) elf_ar_hdr->ar_size > maxsize)
    elf_ar_hdr->ar_size = maxsize;

  return 0;
}


static Elf *
dup_elf (int fildes, Elf_Cmd cmd, Elf *ref)
{
  struct Elf *result;

  if (fildes == -1)
    
    fildes = ref->fildes;
  else if (unlikely (ref->fildes != -1 && fildes != ref->fildes))
    {
      __libelf_seterrno (ELF_E_FD_MISMATCH);
      return NULL;
    }

  if (unlikely (ref->cmd != ELF_C_READ && ref->cmd != ELF_C_READ_MMAP
		&& ref->cmd != ELF_C_WRITE && ref->cmd != ELF_C_WRITE_MMAP
		&& ref->cmd != ELF_C_RDWR && ref->cmd != ELF_C_RDWR_MMAP
		&& ref->cmd != ELF_C_READ_MMAP_PRIVATE))
    {
      __libelf_seterrno (ELF_E_INVALID_OP);
      return NULL;
    }

  if (ref->kind != ELF_K_AR)
    {
      ++ref->ref_count;
      return ref;
    }

  if (ref->state.ar.elf_ar_hdr.ar_name == NULL
      && __libelf_next_arhdr_wrlock (ref) != 0)
    
    return NULL;

  result = read_file (fildes, ref->state.ar.offset + sizeof (struct ar_hdr),
		      ref->state.ar.elf_ar_hdr.ar_size, cmd, ref);

  
  if (result != NULL)
    {
      result->next = ref->state.ar.children;
      ref->state.ar.children = result;
    }

  return result;
}


static struct Elf *
write_file (int fd, Elf_Cmd cmd)
{
  
#define NSCNSALLOC	10
  Elf *result = allocate_elf (fd, NULL, 0, 0, cmd, NULL, ELF_K_ELF,
			      NSCNSALLOC * sizeof (Elf_Scn));

  if (result != NULL)
    {
      
      result->flags = ELF_F_DIRTY;

      
      result->state.elf.scnincr = NSCNSALLOC;

      
      assert (offsetof (struct Elf, state.elf32.scns)
	      == offsetof (struct Elf, state.elf64.scns));
      result->state.elf.scns_last = &result->state.elf32.scns;
      result->state.elf32.scns.max = NSCNSALLOC;
    }

  return result;
}


Elf *
elf_begin (fildes, cmd, ref)
     int fildes;
     Elf_Cmd cmd;
     Elf *ref;
{
  Elf *retval;

  if (unlikely (! __libelf_version_initialized))
    {
      
      __libelf_seterrno (ELF_E_NO_VERSION);
      return NULL;
    }

  if (ref != NULL)
    
    rwlock_rdlock (ref->lock);
  else if (unlikely (fcntl (fildes, F_GETFL) == -1 && errno == EBADF))
    {
      
      __libelf_seterrno (ELF_E_INVALID_FILE);
      return NULL;
    }

  Elf *lock_dup_elf ()
  {
    
    if (ref->kind == ELF_K_AR)
      {
	rwlock_unlock (ref->lock);
	rwlock_wrlock (ref->lock);
      }

    
    return dup_elf (fildes, cmd, ref);
  }

  switch (cmd)
    {
    case ELF_C_NULL:
      
      retval = NULL;
      break;

    case ELF_C_READ_MMAP_PRIVATE:
      
      if (unlikely (ref != NULL && ref->cmd != ELF_C_READ_MMAP_PRIVATE))
	{
	  __libelf_seterrno (ELF_E_INVALID_CMD);
	  retval = NULL;
	  break;
	}
      

    case ELF_C_READ:
    case ELF_C_READ_MMAP:
      if (ref != NULL)
	retval = lock_dup_elf ();
      else
	
	retval = read_file (fildes, 0, ~((size_t) 0), cmd, NULL);
      break;

    case ELF_C_RDWR:
    case ELF_C_RDWR_MMAP:
      if (ref != NULL)
	{
	  if (unlikely (ref->cmd != ELF_C_RDWR && ref->cmd != ELF_C_RDWR_MMAP
			&& ref->cmd != ELF_C_WRITE
			&& ref->cmd != ELF_C_WRITE_MMAP))
	    {
	      
	      __libelf_seterrno (ELF_E_INVALID_CMD);
	      retval = NULL;
	    }
	  else
	    retval = lock_dup_elf ();
	}
      else
	
	retval = read_file (fildes, 0, ~((size_t) 0), cmd, NULL);
      break;

    case ELF_C_WRITE:
    case ELF_C_WRITE_MMAP:
      
      retval = write_file (fildes, cmd);
      break;

    default:
      __libelf_seterrno (ELF_E_INVALID_CMD);
      retval = NULL;
      break;
    }

  
  if (ref != NULL)
    rwlock_unlock (ref->lock);

  return retval;
}
INTDEF(elf_begin)
