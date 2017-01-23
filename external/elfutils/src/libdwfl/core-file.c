/* Core file handling.
   Copyright (C) 2008-2010, 2013 Red Hat, Inc.
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

#include <config.h>
#include "../libelf/libelfP.h"	
#undef	_
#include "libdwflP.h"
#include <gelf.h>

#include <sys/param.h>
#include <unistd.h>
#include <endian.h>
#include <byteswap.h>
#include "system.h"


static Elf *
elf_begin_rand (Elf *parent, loff_t offset, loff_t size, loff_t *next)
{
  if (parent == NULL)
    return NULL;

  
  inline Elf *fail (int error)
  {
    if (next != NULL)
      *next = offset;
    
    __libdwfl_seterrno (DWFL_E (LIBELF, error));
    return NULL;
  }

  loff_t min = (parent->kind == ELF_K_ELF ?
		(parent->class == ELFCLASS32
		 ? sizeof (Elf32_Ehdr) : sizeof (Elf64_Ehdr))
		: parent->kind == ELF_K_AR ? SARMAG
		: 0);

  if (unlikely (offset < min)
      || unlikely (offset >= (loff_t) parent->maximum_size))
    return fail (ELF_E_RANGE);

  if (parent->kind == ELF_K_AR)
    {
      struct ar_hdr h = { .ar_size = "" };

      if (unlikely (parent->maximum_size - offset < sizeof h))
	return fail (ELF_E_RANGE);

      if (parent->map_address != NULL)
	memcpy (h.ar_size, parent->map_address + parent->start_offset + offset,
		sizeof h.ar_size);
      else if (unlikely (pread_retry (parent->fildes,
				      h.ar_size, sizeof (h.ar_size),
				      parent->start_offset + offset
				      + offsetof (struct ar_hdr, ar_size))
			 != sizeof (h.ar_size)))
	return fail (ELF_E_READ_ERROR);

      offset += sizeof h;

      char *endp;
      size = strtoll (h.ar_size, &endp, 10);
      if (unlikely (endp == h.ar_size)
	  || unlikely ((loff_t) parent->maximum_size - offset < size))
	return fail (ELF_E_INVALID_ARCHIVE);
    }

  if (unlikely ((loff_t) parent->maximum_size - offset < size))
    return fail (ELF_E_RANGE);

  
  if (next != NULL)
    *next = offset + size;

  if (unlikely (offset == 0)
      && unlikely (size == (loff_t) parent->maximum_size))
    return elf_clone (parent, parent->cmd);

  Elf_Data *data = elf_getdata_rawchunk (parent, offset, size, ELF_T_BYTE);
  if (data == NULL)
    return NULL;
  assert ((loff_t) data->d_size == size);
  return elf_memory (data->d_buf, size);
}


int
dwfl_report_core_segments (Dwfl *dwfl, Elf *elf, size_t phnum, GElf_Phdr *notes)
{
  if (unlikely (dwfl == NULL))
    return -1;

  int result = 0;

  if (notes != NULL)
    notes->p_type = PT_NULL;

  for (size_t ndx = 0; result >= 0 && ndx < phnum; ++ndx)
    {
      GElf_Phdr phdr_mem;
      GElf_Phdr *phdr = gelf_getphdr (elf, ndx, &phdr_mem);
      if (unlikely (phdr == NULL))
	{
	  __libdwfl_seterrno (DWFL_E_LIBELF);
	  return -1;
	}
      switch (phdr->p_type)
	{
	case PT_LOAD:
	  result = dwfl_report_segment (dwfl, ndx, phdr, 0, NULL);
	  break;

	case PT_NOTE:
	  if (notes != NULL)
	    {
	      *notes = *phdr;
	      notes = NULL;
	    }
	  break;
	}
    }

  return result;
}

#define MAX_EAGER_COST	8192

static bool
core_file_read_eagerly (Dwfl_Module *mod,
			void **userdata __attribute__ ((unused)),
			const char *name __attribute__ ((unused)),
			Dwarf_Addr start __attribute__ ((unused)),
			void **buffer, size_t *buffer_available,
			GElf_Off cost, GElf_Off worthwhile,
			GElf_Off whole,
			GElf_Off contiguous __attribute__ ((unused)),
			void *arg, Elf **elfp)
{
  Elf *core = arg;

  if (whole <= *buffer_available)
    {
      

      if (core->map_address == NULL)
	{
	  
	  *elfp = elf_memory (*buffer, whole);
	  if (unlikely (*elfp == NULL))
	    return false;

	  (*elfp)->flags |= ELF_F_MALLOCED;
	  *buffer = NULL;
	  *buffer_available = 0;
	  return true;
	}

      
      *elfp = elf_begin_rand (core, *buffer - core->map_address, whole, NULL);
      *buffer = NULL;
      *buffer_available = 0;
      return *elfp != NULL;
    }


  if (worthwhile == 0)
    
    return false;


  if (mod->build_id_len > 0)
    return false;

  if (core->map_address != NULL)
    
    return true;

  
  return cost <= MAX_EAGER_COST;
}

bool
dwfl_elf_phdr_memory_callback (Dwfl *dwfl, int ndx,
			       void **buffer, size_t *buffer_available,
			       GElf_Addr vaddr,
			       size_t minread,
			       void *arg)
{
  Elf *elf = arg;

  if (ndx == -1)
    {
      
      if (elf->map_address == NULL)
	free (*buffer);
      *buffer = NULL;
      *buffer_available = 0;
      return false;
    }

  const GElf_Off align = dwfl->segment_align ?: 1;
  GElf_Phdr phdr;

  do
    if (unlikely (gelf_getphdr (elf, ndx++, &phdr) == NULL))
      return false;
  while (phdr.p_type != PT_LOAD
	 || ((phdr.p_vaddr + phdr.p_memsz + align - 1) & -align) <= vaddr);

  GElf_Off start = vaddr - phdr.p_vaddr + phdr.p_offset;
  GElf_Off end;
  GElf_Addr end_vaddr;

  inline void update_end ()
  {
    end = (phdr.p_offset + phdr.p_filesz + align - 1) & -align;
    end_vaddr = (phdr.p_vaddr + phdr.p_memsz + align - 1) & -align;
  }

  update_end ();

  
  inline bool more (size_t size)
  {
    while (end <= start || end - start < size)
      {
	if (phdr.p_filesz < phdr.p_memsz)
	  
	  return false;

	if (unlikely (gelf_getphdr (elf, ndx++, &phdr) == NULL))
	  return false;

	if (phdr.p_type == PT_LOAD)
	  {
	    if (phdr.p_offset > end
		|| phdr.p_vaddr > end_vaddr)
	      
	      return false;

	    update_end ();
	  }
      }
    return true;
  }

  
  if (! more (minread))
    return false;

  
  (void) more (*buffer_available);

  
  if (elf->map_address != NULL)
    (void) more (elf->maximum_size - start);

  if (unlikely (end > elf->maximum_size))
    end = elf->maximum_size;

  
  if (unlikely (start >= end))
    return false;

  if (elf->map_address != NULL)
    {
      void *contents = elf->map_address + elf->start_offset + start;
      size_t size = end - start;

      if (minread == 0)		
	{
	  const void *eos = memchr (contents, '\0', size);
	  if (unlikely (eos == NULL) || unlikely (eos == contents))
	    return false;
	  size = eos + 1 - contents;
	}

      if (*buffer == NULL)
	{
	  *buffer = contents;
	  *buffer_available = size;
	}
      else
	{
	  *buffer_available = MIN (size, *buffer_available);
	  memcpy (*buffer, contents, *buffer_available);
	}
    }
  else
    {
      void *into = *buffer;
      if (*buffer == NULL)
	{
	  *buffer_available = MIN (minread ?: 512,
				   MAX (4096, MIN (end - start,
						   *buffer_available)));
	  into = malloc (*buffer_available);
	  if (unlikely (into == NULL))
	    {
	      __libdwfl_seterrno (DWFL_E_NOMEM);
	      return false;
	    }
	}

      ssize_t nread = pread_retry (elf->fildes, into, *buffer_available, start);
      if (nread < (ssize_t) minread)
	{
	  if (into != *buffer)
	    free (into);
	  if (nread < 0)
	    __libdwfl_seterrno (DWFL_E_ERRNO);
	  return false;
	}

      if (minread == 0)		
	{
	  const void *eos = memchr (into, '\0', nread);
	  if (unlikely (eos == NULL) || unlikely (eos == into))
	    {
	      if (*buffer == NULL)
		free (into);
	      return false;
	    }
	  nread = eos + 1 - into;
	}

      if (*buffer == NULL)
	*buffer = into;
      *buffer_available = nread;
    }

  return true;
}


static void
clear_r_debug_info (struct r_debug_info *r_debug_info)
{
  while (r_debug_info->module != NULL)
    {
      struct r_debug_info_module *module = r_debug_info->module;
      r_debug_info->module = module->next;
      elf_end (module->elf);
      if (module->fd != -1)
	close (module->fd);
      free (module);
    }
}

bool
internal_function
__libdwfl_dynamic_vaddr_get (Elf *elf, GElf_Addr *vaddrp)
{
  size_t phnum;
  if (unlikely (elf_getphdrnum (elf, &phnum) != 0))
    return false;
  for (size_t i = 0; i < phnum; ++i)
    {
      GElf_Phdr phdr_mem;
      GElf_Phdr *phdr = gelf_getphdr (elf, i, &phdr_mem);
      if (unlikely (phdr == NULL))
	return false;
      if (phdr->p_type == PT_DYNAMIC)
	{
	  *vaddrp = phdr->p_vaddr;
	  return true;
	}
    }
  return false;
}

int
dwfl_core_file_report (Dwfl *dwfl, Elf *elf, const char *executable)
{
  size_t phnum;
  if (unlikely (elf_getphdrnum (elf, &phnum) != 0))
    {
      __libdwfl_seterrno (DWFL_E_LIBELF);
      return -1;
    }

  free (dwfl->executable_for_core);
  if (executable == NULL)
    dwfl->executable_for_core = NULL;
  else
    {
      dwfl->executable_for_core = strdup (executable);
      if (dwfl->executable_for_core == NULL)
	{
	  __libdwfl_seterrno (DWFL_E_NOMEM);
	  return -1;
	}
    }

  
  GElf_Phdr notes_phdr;
  int ndx = dwfl_report_core_segments (dwfl, elf, phnum, &notes_phdr);
  if (unlikely (ndx <= 0))
    return ndx;

  

  const void *auxv = NULL;
  const void *note_file = NULL;
  size_t auxv_size = 0;
  size_t note_file_size = 0;
  if (likely (notes_phdr.p_type == PT_NOTE))
    {
      

      Elf_Data *notes = elf_getdata_rawchunk (elf,
					      notes_phdr.p_offset,
					      notes_phdr.p_filesz,
					      ELF_T_NHDR);
      if (likely (notes != NULL))
	{
	  size_t pos = 0;
	  GElf_Nhdr nhdr;
	  size_t name_pos;
	  size_t desc_pos;
	  while ((pos = gelf_getnote (notes, pos, &nhdr,
				      &name_pos, &desc_pos)) > 0)
	    if (nhdr.n_namesz == sizeof "CORE"
		&& !memcmp (notes->d_buf + name_pos, "CORE", sizeof "CORE"))
	      {
		if (nhdr.n_type == NT_AUXV)
		  {
		    auxv = notes->d_buf + desc_pos;
		    auxv_size = nhdr.n_descsz;
		  }
		if (nhdr.n_type == NT_FILE)
		  {
		    note_file = notes->d_buf + desc_pos;
		    note_file_size = nhdr.n_descsz;
		  }
	      }
	}
    }


  struct r_debug_info r_debug_info;
  memset (&r_debug_info, 0, sizeof r_debug_info);
  int retval = dwfl_link_map_report (dwfl, auxv, auxv_size,
				     dwfl_elf_phdr_memory_callback, elf,
				     &r_debug_info);
  int listed = retval > 0 ? retval : 0;


  ndx = 0;
  do
    {
      int seg = dwfl_segment_report_module (dwfl, ndx, NULL,
					    &dwfl_elf_phdr_memory_callback, elf,
					    core_file_read_eagerly, elf,
					    note_file, note_file_size,
					    &r_debug_info);
      if (unlikely (seg < 0))
	{
	  clear_r_debug_info (&r_debug_info);
	  return seg;
	}
      if (seg > ndx)
	{
	  ndx = seg;
	  ++listed;
	}
      else
	++ndx;
    }
  while (ndx < (int) phnum);


  Dwfl_Module **lastmodp = &dwfl->modulelist;
  while (*lastmodp != NULL)
    lastmodp = &(*lastmodp)->next;
  for (struct r_debug_info_module *module = r_debug_info.module;
       module != NULL; module = module->next)
    {
      if (module->elf == NULL)
	continue;
      GElf_Addr file_dynamic_vaddr;
      if (! __libdwfl_dynamic_vaddr_get (module->elf, &file_dynamic_vaddr))
	continue;
      Dwfl_Module *mod;
      mod = __libdwfl_report_elf (dwfl, basename (module->name), module->name,
				  module->fd, module->elf,
				  module->l_ld - file_dynamic_vaddr,
				  true, true);
      if (mod == NULL)
	continue;
      ++listed;
      module->elf = NULL;
      module->fd = -1;
      if (mod->next != NULL)
	{
	  if (*lastmodp != mod)
	    {
	      lastmodp = &dwfl->modulelist;
	      while (*lastmodp != mod)
		lastmodp = &(*lastmodp)->next;
	    }
	  *lastmodp = mod->next;
	  mod->next = NULL;
	  while (*lastmodp != NULL)
	    lastmodp = &(*lastmodp)->next;
	  *lastmodp = mod;
	}
      lastmodp = &mod->next;
    }

  clear_r_debug_info (&r_debug_info);

  return listed > 0 ? listed : retval;
}
INTDEF (dwfl_core_file_report)
NEW_VERSION (dwfl_core_file_report, ELFUTILS_0.158)

#ifdef SHARED
int _compat_without_executable_dwfl_core_file_report (Dwfl *dwfl, Elf *elf);
COMPAT_VERSION_NEWPROTO (dwfl_core_file_report, ELFUTILS_0.146,
			 without_executable)

int
_compat_without_executable_dwfl_core_file_report (Dwfl *dwfl, Elf *elf)
{
  return dwfl_core_file_report (dwfl, elf, NULL);
}
#endif
