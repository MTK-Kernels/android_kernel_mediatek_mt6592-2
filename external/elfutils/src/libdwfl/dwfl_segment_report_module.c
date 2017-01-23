/* Sniff out modules from ELF headers visible in memory segments.
   Copyright (C) 2008-2012, 2014 Red Hat, Inc.
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
#include "common.h"

#include <elf.h>
#include <gelf.h>
#include <inttypes.h>
#include <sys/param.h>
#include <alloca.h>
#include <endian.h>
#include <unistd.h>
#include <fcntl.h>



#define INITIAL_READ	1024

#if __BYTE_ORDER == __LITTLE_ENDIAN
# define MY_ELFDATA	ELFDATA2LSB
#else
# define MY_ELFDATA	ELFDATA2MSB
#endif


static int
addr_segndx (Dwfl *dwfl, size_t segment, GElf_Addr addr, bool next)
{
  int ndx = -1;
  do
    {
      if (dwfl->lookup_segndx[segment] >= 0)
	ndx = dwfl->lookup_segndx[segment];
      if (++segment >= dwfl->lookup_elts - 1)
	return next ? ndx + 1 : ndx;
    }
  while (dwfl->lookup_addr[segment] < addr);

  if (next)
    {
      while (dwfl->lookup_segndx[segment] < 0)
	if (++segment >= dwfl->lookup_elts - 1)
	  return ndx + 1;
      ndx = dwfl->lookup_segndx[segment];
    }

  return ndx;
}


static bool
buf_has_data (const void *ptr, const void *end, size_t sz)
{
  return ptr < end && (size_t) (end - ptr) >= sz;
}


static bool
buf_read_ulong (unsigned char ei_data, size_t sz,
		const void **ptrp, const void *end, uint64_t *retp)
{
  if (! buf_has_data (*ptrp, end, sz))
    return false;

  union
  {
    uint64_t u64;
    uint32_t u32;
  } u;

  memcpy (&u, *ptrp, sz);
  (*ptrp) += sz;

  if (retp == NULL)
    return true;

  if (MY_ELFDATA != ei_data)
    {
      if (sz == 4)
	CONVERT (u.u32);
      else
	CONVERT (u.u64);
    }
  if (sz == 4)
    *retp = u.u32;
  else
    *retp = u.u64;
  return true;
}


static const char *
handle_file_note (GElf_Addr module_start, GElf_Addr module_end,
		  unsigned char ei_class, unsigned char ei_data,
		  const void *note_file, size_t note_file_size)
{
  if (note_file == NULL)
    return NULL;

  size_t sz;
  switch (ei_class)
    {
    case ELFCLASS32:
      sz = 4;
      break;
    case ELFCLASS64:
      sz = 8;
      break;
    default:
      return NULL;
    }

  const void *ptr = note_file;
  const void *end = note_file + note_file_size;
  uint64_t count;
  if (! buf_read_ulong (ei_data, sz, &ptr, end, &count))
    return NULL;
  if (! buf_read_ulong (ei_data, sz, &ptr, end, NULL)) 
    return NULL;

  uint64_t maxcount = (size_t) (end - ptr) / (3 * sz);
  if (count > maxcount)
    return NULL;

  
  const char *fptr = ptr + 3 * count * sz;

  ssize_t firstix = -1;
  ssize_t lastix = -1;
  for (size_t mix = 0; mix < count; mix++)
    {
      uint64_t mstart, mend, moffset;
      if (! buf_read_ulong (ei_data, sz, &ptr, fptr, &mstart)
	  || ! buf_read_ulong (ei_data, sz, &ptr, fptr, &mend)
	  || ! buf_read_ulong (ei_data, sz, &ptr, fptr, &moffset))
	return NULL;
      if (mstart == module_start && moffset == 0)
	firstix = lastix = mix;
      if (firstix != -1 && mstart < module_end)
	lastix = mix;
      if (mend >= module_end)
	break;
    }
  if (firstix == -1)
    return NULL;

  const char *retval = NULL;
  for (ssize_t mix = 0; mix <= lastix; mix++)
    {
      const char *fnext = memchr (fptr, 0, (const char *) end - fptr);
      if (fnext == NULL)
	return NULL;
      if (mix == firstix)
	retval = fptr;
      if (firstix < mix && mix <= lastix && strcmp (fptr, retval) != 0)
	return NULL;
      fptr = fnext + 1;
    }
  return retval;
}


static bool
invalid_elf (Elf *elf, bool disk_file_has_build_id,
	     const void *build_id, size_t build_id_len)
{
  if (! disk_file_has_build_id && build_id_len > 0)
    {
      return true;
    }
  if (disk_file_has_build_id && build_id_len > 0)
    {
      const void *elf_build_id;
      ssize_t elf_build_id_len;

      
      elf_build_id_len = INTUSE(dwelf_elf_gnu_build_id) (elf, &elf_build_id);
      if (elf_build_id_len > 0)
	{
	  if (build_id_len != (size_t) elf_build_id_len
	      || memcmp (build_id, elf_build_id, build_id_len) != 0)
	    return true;
	}
    }
  return false;
}

int
dwfl_segment_report_module (Dwfl *dwfl, int ndx, const char *name,
			    Dwfl_Memory_Callback *memory_callback,
			    void *memory_callback_arg,
			    Dwfl_Module_Callback *read_eagerly,
			    void *read_eagerly_arg,
			    const void *note_file, size_t note_file_size,
			    const struct r_debug_info *r_debug_info)
{
  size_t segment = ndx;

  if (segment >= dwfl->lookup_elts)
    segment = dwfl->lookup_elts - 1;

  while (segment > 0
	 && (dwfl->lookup_segndx[segment] > ndx
	     || dwfl->lookup_segndx[segment] == -1))
    --segment;

  while (dwfl->lookup_segndx[segment] < ndx)
    if (++segment == dwfl->lookup_elts)
      return 0;

  GElf_Addr start = dwfl->lookup_addr[segment];

  inline bool segment_read (int segndx,
			    void **buffer, size_t *buffer_available,
			    GElf_Addr addr, size_t minread)
  {
    return ! (*memory_callback) (dwfl, segndx, buffer, buffer_available,
				 addr, minread, memory_callback_arg);
  }

  inline void release_buffer (void **buffer, size_t *buffer_available)
  {
    if (*buffer != NULL)
      (void) segment_read (-1, buffer, buffer_available, 0, 0);
  }

  

  void *buffer = NULL;
  size_t buffer_available = INITIAL_READ;
  Elf *elf = NULL;
  int fd = -1;

  inline int finish (void)
  {
    release_buffer (&buffer, &buffer_available);
    if (elf != NULL)
      elf_end (elf);
    if (fd != -1)
      close (fd);
    return ndx;
  }

  if (segment_read (ndx, &buffer, &buffer_available,
		    start, sizeof (Elf64_Ehdr))
      || memcmp (buffer, ELFMAG, SELFMAG) != 0)
    return finish ();

  inline bool read_portion (void **data, size_t *data_size,
			    GElf_Addr vaddr, size_t filesz)
  {
    if (vaddr - start + filesz > buffer_available
	|| (filesz == 0 && memchr (vaddr - start + buffer, '\0',
				   buffer_available - (vaddr - start)) == NULL))
      {
	*data = NULL;
	*data_size = filesz;
	return segment_read (addr_segndx (dwfl, segment, vaddr, false),
			     data, data_size, vaddr, filesz);
      }

    
    *data = vaddr - start + buffer;
    *data_size = 0;
    return false;
  }

  inline void finish_portion (void **data, size_t *data_size)
  {
    if (*data_size != 0)
      release_buffer (data, data_size);
  }

  
  const unsigned char *e_ident;
  unsigned char ei_class;
  unsigned char ei_data;
  uint16_t e_type;
  union
  {
    Elf32_Ehdr e32;
    Elf64_Ehdr e64;
  } ehdr;
  GElf_Off phoff;
  uint_fast16_t phnum;
  uint_fast16_t phentsize;
  GElf_Off shdrs_end;
  Elf_Data xlatefrom =
    {
      .d_type = ELF_T_EHDR,
      .d_buf = (void *) buffer,
      .d_version = EV_CURRENT,
    };
  Elf_Data xlateto =
    {
      .d_type = ELF_T_EHDR,
      .d_buf = &ehdr,
      .d_size = sizeof ehdr,
      .d_version = EV_CURRENT,
    };
  e_ident = ((const unsigned char *) buffer);
  ei_class = e_ident[EI_CLASS];
  ei_data = e_ident[EI_DATA];
  switch (ei_class)
    {
    case ELFCLASS32:
      xlatefrom.d_size = sizeof (Elf32_Ehdr);
      if (elf32_xlatetom (&xlateto, &xlatefrom, ei_data) == NULL)
	return finish ();
      e_type = ehdr.e32.e_type;
      phoff = ehdr.e32.e_phoff;
      phnum = ehdr.e32.e_phnum;
      phentsize = ehdr.e32.e_phentsize;
      if (phentsize != sizeof (Elf32_Phdr))
	return finish ();
      shdrs_end = ehdr.e32.e_shoff + ehdr.e32.e_shnum * ehdr.e32.e_shentsize;
      break;

    case ELFCLASS64:
      xlatefrom.d_size = sizeof (Elf64_Ehdr);
      if (elf64_xlatetom (&xlateto, &xlatefrom, ei_data) == NULL)
	return finish ();
      e_type = ehdr.e64.e_type;
      phoff = ehdr.e64.e_phoff;
      phnum = ehdr.e64.e_phnum;
      phentsize = ehdr.e64.e_phentsize;
      if (phentsize != sizeof (Elf64_Phdr))
	return finish ();
      shdrs_end = ehdr.e64.e_shoff + ehdr.e64.e_shnum * ehdr.e64.e_shentsize;
      break;

    default:
      return finish ();
    }


  if (phnum == 0)
    return finish ();

  xlatefrom.d_type = xlateto.d_type = ELF_T_PHDR;
  xlatefrom.d_size = phnum * phentsize;

  void *ph_buffer = NULL;
  size_t ph_buffer_size = 0;
  if (read_portion (&ph_buffer, &ph_buffer_size,
		    start + phoff, xlatefrom.d_size))
    return finish ();

  xlatefrom.d_buf = ph_buffer;

  union
  {
    Elf32_Phdr p32[phnum];
    Elf64_Phdr p64[phnum];
  } phdrs;

  xlateto.d_buf = &phdrs;
  xlateto.d_size = sizeof phdrs;

  
  GElf_Off file_trimmed_end = 0; 
  GElf_Off file_end = 0;	 
  GElf_Off contiguous = 0;	 
  GElf_Off total_filesz = 0;	 

  
  GElf_Addr bias = 0;
  bool found_bias = false;

  
  GElf_Addr module_start = -1l;
  GElf_Addr module_end = 0;
  GElf_Addr module_address_sync = 0;

  
  GElf_Addr dyn_vaddr = 0;
  GElf_Xword dyn_filesz = 0;

  
  void *build_id = NULL;
  size_t build_id_len = 0;
  GElf_Addr build_id_vaddr = 0;

  
  inline void consider_notes (GElf_Addr vaddr, GElf_Xword filesz)
  {
    
    if (build_id != NULL || filesz == 0)
      return;

    void *data;
    size_t data_size;
    if (read_portion (&data, &data_size, vaddr, filesz))
      return;

    assert (sizeof (Elf32_Nhdr) == sizeof (Elf64_Nhdr));

    void *notes;
    if (ei_data == MY_ELFDATA)
      notes = data;
    else
      {
	notes = malloc (filesz);
	if (unlikely (notes == NULL))
	  return;
	xlatefrom.d_type = xlateto.d_type = ELF_T_NHDR;
	xlatefrom.d_buf = (void *) data;
	xlatefrom.d_size = filesz;
	xlateto.d_buf = notes;
	xlateto.d_size = filesz;
	if (elf32_xlatetom (&xlateto, &xlatefrom,
			    ehdr.e32.e_ident[EI_DATA]) == NULL)
	  goto done;
      }

    const GElf_Nhdr *nh = notes;
    while ((const void *) nh < (const void *) notes + filesz)
     {
	const void *note_name = nh + 1;
	const void *note_desc = note_name + NOTE_ALIGN (nh->n_namesz);
	if (unlikely ((size_t) ((const void *) notes + filesz
				- note_desc) < nh->n_descsz))
	  break;

	if (nh->n_type == NT_GNU_BUILD_ID
	    && nh->n_descsz > 0
	    && nh->n_namesz == sizeof "GNU"
	    && !memcmp (note_name, "GNU", sizeof "GNU"))
	  {
	    build_id_vaddr = note_desc - (const void *) notes + vaddr;
	    build_id_len = nh->n_descsz;
	    build_id = malloc (nh->n_descsz);
	    if (likely (build_id != NULL))
	      memcpy (build_id, note_desc, build_id_len);
	    break;
	  }

	nh = note_desc + NOTE_ALIGN (nh->n_descsz);
      }

  done:
    if (notes != data)
      free (notes);
    finish_portion (&data, &data_size);
  }

  
  inline void consider_phdr (GElf_Word type,
			     GElf_Addr vaddr, GElf_Xword memsz,
			     GElf_Off offset, GElf_Xword filesz,
			     GElf_Xword align)
  {
    switch (type)
      {
      case PT_DYNAMIC:
	dyn_vaddr = vaddr;
	dyn_filesz = filesz;
	break;

      case PT_NOTE:
	consider_notes (start + offset, filesz);
	break;

      case PT_LOAD:
	align = dwfl->segment_align > 1 ? dwfl->segment_align : align ?: 1;

	GElf_Addr vaddr_end = (vaddr + memsz + align - 1) & -align;
	GElf_Addr filesz_vaddr = filesz < memsz ? vaddr + filesz : vaddr_end;
	GElf_Off filesz_offset = filesz_vaddr - vaddr + offset;

	if (file_trimmed_end < offset + filesz)
	  {
	    file_trimmed_end = offset + filesz;

	    if (shdrs_end <= filesz_offset && shdrs_end > file_trimmed_end)
	      {
		filesz += shdrs_end - file_trimmed_end;
		file_trimmed_end = shdrs_end;
	      }
	  }

	total_filesz += filesz;

	if (file_end < filesz_offset)
	  {
	    file_end = filesz_offset;
	    if (filesz_vaddr - start == filesz_offset)
	      contiguous = file_end;
	  }

	if (!found_bias && (offset & -align) == 0
	    && likely (filesz_offset >= phoff + phnum * phentsize))
	  {
	    bias = start - vaddr;
	    found_bias = true;
	  }

	if ((vaddr & -align) < module_start)
	  {
	    module_start = vaddr & -align;
	    module_address_sync = vaddr + memsz;
	  }

	if (module_end < vaddr_end)
	  module_end = vaddr_end;
	break;
      }
  }
  if (ei_class == ELFCLASS32)
    {
      if (elf32_xlatetom (&xlateto, &xlatefrom, ei_data) == NULL)
	found_bias = false;	
      else
	for (uint_fast16_t i = 0; i < phnum; ++i)
	  consider_phdr (phdrs.p32[i].p_type,
			 phdrs.p32[i].p_vaddr, phdrs.p32[i].p_memsz,
			 phdrs.p32[i].p_offset, phdrs.p32[i].p_filesz,
			 phdrs.p32[i].p_align);
    }
  else
    {
      if (elf64_xlatetom (&xlateto, &xlatefrom, ei_data) == NULL)
	found_bias = false;	
      else
	for (uint_fast16_t i = 0; i < phnum; ++i)
	  consider_phdr (phdrs.p64[i].p_type,
			 phdrs.p64[i].p_vaddr, phdrs.p64[i].p_memsz,
			 phdrs.p64[i].p_offset, phdrs.p64[i].p_filesz,
			 phdrs.p64[i].p_align);
    }

  finish_portion (&ph_buffer, &ph_buffer_size);

  if (unlikely (!found_bias))
    {
      free (build_id);
      return finish ();
    }

  
  module_start += bias;
  module_end += bias;

  dyn_vaddr += bias;

  bool name_is_final = false;

  if (r_debug_info != NULL)
    for (const struct r_debug_info_module *module = r_debug_info->module;
	 module != NULL; module = module->next)
      if (module_start <= module->l_ld && module->l_ld < module_end)
	{
	  GElf_Addr fixup = module->l_ld - dyn_vaddr;
	  if ((fixup & (dwfl->segment_align - 1)) == 0
	      && module_start + fixup <= module->l_ld
	      && module->l_ld < module_end + fixup)
	    {
	      module_start += fixup;
	      module_end += fixup;
	      dyn_vaddr += fixup;
	      bias += fixup;
	      if (module->name[0] != '\0')
		{
		  name = basename (module->name);
		  name_is_final = true;
		}
	      break;
	    }
	}

  if (r_debug_info != NULL)
    {
      bool skip_this_module = false;
      for (struct r_debug_info_module *module = r_debug_info->module;
	   module != NULL; module = module->next)
	if ((module_end > module->start && module_start < module->end)
	    || dyn_vaddr == module->l_ld)
	  {
	    if (module->elf != NULL
	        && invalid_elf (module->elf, module->disk_file_has_build_id,
				build_id, build_id_len))
	      {
		elf_end (module->elf);
		close (module->fd);
		module->elf = NULL;
		module->fd = -1;
	      }
	    if (module->elf != NULL)
	      {
		skip_this_module = true;
	      }
	  }
      if (skip_this_module)
	{
	  free (build_id);
	  return finish ();
	}
    }

  const char *file_note_name = handle_file_note (module_start, module_end,
						 ei_class, ei_data,
						 note_file, note_file_size);
  if (file_note_name)
    {
      name = file_note_name;
      name_is_final = true;
      bool invalid = false;
      fd = open64 (name, O_RDONLY);
      if (fd >= 0)
	{
	  Dwfl_Error error = __libdw_open_file (&fd, &elf, true, false);
	  if (error == DWFL_E_NOERROR)
	    invalid = invalid_elf (elf, true ,
				   build_id, build_id_len);
	}
      if (invalid)
	{
	  free (build_id);
	  return finish ();
	}
    }

  ndx = addr_segndx (dwfl, segment, module_end, true);

  GElf_Addr soname_stroff = 0;
  GElf_Addr dynstr_vaddr = 0;
  GElf_Xword dynstrsz = 0;
  bool execlike = false;
  inline bool consider_dyn (GElf_Sxword tag, GElf_Xword val)
  {
    switch (tag)
      {
      default:
	return false;

      case DT_DEBUG:
	execlike = true;
	break;

      case DT_SONAME:
	soname_stroff = val;
	break;

      case DT_STRTAB:
	dynstr_vaddr = val;
	break;

      case DT_STRSZ:
	dynstrsz = val;
	break;
      }

    return soname_stroff != 0 && dynstr_vaddr != 0 && dynstrsz != 0;
  }

  const size_t dyn_entsize = (ei_class == ELFCLASS32
			      ? sizeof (Elf32_Dyn) : sizeof (Elf64_Dyn));
  void *dyn_data = NULL;
  size_t dyn_data_size = 0;
  if (dyn_filesz != 0 && dyn_filesz % dyn_entsize == 0
      && ! read_portion (&dyn_data, &dyn_data_size, dyn_vaddr, dyn_filesz))
    {
      union
      {
	Elf32_Dyn d32[dyn_filesz / sizeof (Elf32_Dyn)];
	Elf64_Dyn d64[dyn_filesz / sizeof (Elf64_Dyn)];
      } dyn;

      xlatefrom.d_type = xlateto.d_type = ELF_T_DYN;
      xlatefrom.d_buf = (void *) dyn_data;
      xlatefrom.d_size = dyn_filesz;
      xlateto.d_buf = &dyn;
      xlateto.d_size = sizeof dyn;

      if (ei_class == ELFCLASS32)
	{
	  if (elf32_xlatetom (&xlateto, &xlatefrom, ei_data) != NULL)
	    for (size_t i = 0; i < dyn_filesz / sizeof dyn.d32[0]; ++i)
	      if (consider_dyn (dyn.d32[i].d_tag, dyn.d32[i].d_un.d_val))
		break;
	}
      else
	{
	  if (elf64_xlatetom (&xlateto, &xlatefrom, ei_data) != NULL)
	    for (size_t i = 0; i < dyn_filesz / sizeof dyn.d64[0]; ++i)
	      if (consider_dyn (dyn.d64[i].d_tag, dyn.d64[i].d_un.d_val))
		break;
	}
    }
  finish_portion (&dyn_data, &dyn_data_size);

  
  if (name == NULL)
    name = e_type == ET_EXEC ? "[exe]" : execlike ? "[pie]" : "[dso]";

  void *soname = NULL;
  size_t soname_size = 0;
  if (! name_is_final && dynstrsz != 0 && dynstr_vaddr != 0)
    {

      if ((dynstr_vaddr < module_start || dynstr_vaddr >= module_end)
	  && dynstr_vaddr + bias >= module_start
	  && dynstr_vaddr + bias < module_end)
	dynstr_vaddr += bias;

      if (unlikely (dynstr_vaddr + dynstrsz > module_end))
	dynstrsz = 0;

      
      if (soname_stroff != 0 && soname_stroff + 1 < dynstrsz
	  && ! read_portion (&soname, &soname_size,
			     dynstr_vaddr + soname_stroff, 0))
	name = soname;
    }


  Dwfl_Module *mod = INTUSE(dwfl_report_module) (dwfl, name,
						 module_start, module_end);

  
  
  if (mod != NULL && (execlike || ehdr.e32.e_type == ET_EXEC))
    mod->is_executable = true;

  if (likely (mod != NULL) && build_id != NULL
      && unlikely (INTUSE(dwfl_module_report_build_id) (mod,
							build_id,
							build_id_len,
							build_id_vaddr)))
    {
      mod->gc = true;
      mod = NULL;
    }

  free (build_id);
  finish_portion (&soname, &soname_size);

  if (unlikely (mod == NULL))
    {
      ndx = -1;
      return finish ();
    }


  const GElf_Off cost = (contiguous < file_trimmed_end ? total_filesz
			 : buffer_available >= contiguous ? 0
			 : contiguous - buffer_available);
  const GElf_Off worthwhile = ((dynstr_vaddr == 0 || dynstrsz == 0) ? 0
			       : dynstr_vaddr + dynstrsz - start);
  const GElf_Off whole = MAX (file_trimmed_end, shdrs_end);

  if (elf == NULL
      && (*read_eagerly) (MODCB_ARGS (mod), &buffer, &buffer_available,
			  cost, worthwhile, whole, contiguous,
			  read_eagerly_arg, &elf)
      && elf == NULL)
    {

      void *contents = calloc (1, file_trimmed_end);
      if (unlikely (contents == NULL))
	return finish ();

      inline void final_read (size_t offset, GElf_Addr vaddr, size_t size)
      {
	void *into = contents + offset;
	size_t read_size = size;
	(void) segment_read (addr_segndx (dwfl, segment, vaddr, false),
			     &into, &read_size, vaddr, size);
      }

      if (contiguous < file_trimmed_end)
	{

	  inline void read_phdr (GElf_Word type, GElf_Addr vaddr,
				 GElf_Off offset, GElf_Xword filesz)
	  {
	    if (type == PT_LOAD)
	      final_read (offset, vaddr + bias, filesz);
	  }

	  if (ei_class == ELFCLASS32)
	    for (uint_fast16_t i = 0; i < phnum; ++i)
	      read_phdr (phdrs.p32[i].p_type, phdrs.p32[i].p_vaddr,
			 phdrs.p32[i].p_offset, phdrs.p32[i].p_filesz);
	  else
	    for (uint_fast16_t i = 0; i < phnum; ++i)
	      read_phdr (phdrs.p64[i].p_type, phdrs.p64[i].p_vaddr,
			 phdrs.p64[i].p_offset, phdrs.p64[i].p_filesz);
	}
      else
	{

	  const size_t have = MIN (buffer_available, file_trimmed_end);
	  memcpy (contents, buffer, have);

	  if (have < file_trimmed_end)
	    final_read (have, start + have, file_trimmed_end - have);
	}

      elf = elf_memory (contents, file_trimmed_end);
      if (unlikely (elf == NULL))
	free (contents);
      else
	elf->flags |= ELF_F_MALLOCED;
    }

  if (elf != NULL)
    {
      
      mod->main.elf = elf;
      elf = NULL;
      fd = -1;
      mod->main.vaddr = module_start - bias;
      mod->main.address_sync = module_address_sync;
      mod->main_bias = bias;
    }

  return finish ();
}
