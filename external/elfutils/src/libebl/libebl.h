/* Interface for libebl.
   Copyright (C) 2000-2010, 2013, 2014 Red Hat, Inc.
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

#ifndef _LIBEBL_H
#define _LIBEBL_H 1

#include <gelf.h>
#include "libdw.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "elf-knowledge.h"


typedef struct ebl Ebl;


#ifdef __cplusplus
extern "C" {
#endif

extern Ebl *ebl_openbackend (Elf *elf);
extern Ebl *ebl_openbackend_machine (GElf_Half machine);
extern Ebl *ebl_openbackend_emulation (const char *emulation);

extern void ebl_closebackend (Ebl *bh);



extern int ebl_get_elfmachine (Ebl *ebl) __attribute__ ((__pure__));

extern int ebl_get_elfclass (Ebl *ebl) __attribute__ ((__pure__));

extern int ebl_get_elfdata (Ebl *ebl) __attribute__ ((__pure__));



extern const char *ebl_backend_name (Ebl *ebl);

extern const char *ebl_object_type_name (Ebl *ebl, int object,
					 char *buf, size_t len);

extern const char *ebl_reloc_type_name (Ebl *ebl, int reloc,
					char *buf, size_t len);

extern bool ebl_reloc_type_check (Ebl *ebl, int reloc);

extern bool ebl_reloc_valid_use (Ebl *ebl, int reloc);

extern Elf_Type ebl_reloc_simple_type (Ebl *ebl, int reloc);

extern bool ebl_gotpc_reloc_check (Ebl *ebl, int reloc);

extern const char *ebl_segment_type_name (Ebl *ebl, int segment,
					  char *buf, size_t len);

extern const char *ebl_section_type_name (Ebl *ebl, int section,
					  char *buf, size_t len);

extern const char *ebl_section_name (Ebl *ebl, int section, int xsection,
				     char *buf, size_t len,
				     const char *scnnames[], size_t shnum);

extern const char *ebl_machine_flag_name (Ebl *ebl, GElf_Word flags,
					  char *buf, size_t len);

extern bool ebl_machine_flag_check (Ebl *ebl, GElf_Word flags);

extern bool ebl_machine_section_flag_check (Ebl *ebl, GElf_Xword flags);

extern bool ebl_check_special_section (Ebl *ebl, int ndx,
				       const GElf_Shdr *shdr, const char *name);

extern const char *ebl_symbol_type_name (Ebl *ebl, int symbol,
					 char *buf, size_t len);

extern const char *ebl_symbol_binding_name (Ebl *ebl, int binding,
					    char *buf, size_t len);

extern const char *ebl_dynamic_tag_name (Ebl *ebl, int64_t tag,
					 char *buf, size_t len);

extern bool ebl_dynamic_tag_check (Ebl *ebl, int64_t tag);

extern bool ebl_check_special_symbol (Ebl *ebl, GElf_Ehdr *ehdr,
				      const GElf_Sym *sym, const char *name,
				      const GElf_Shdr *destshdr);

extern bool ebl_check_st_other_bits (Ebl *ebl, unsigned char st_other);

extern GElf_Word ebl_sh_flags_combine (Ebl *ebl, GElf_Word flags1,
				       GElf_Word flags2);

extern const char *ebl_osabi_name (Ebl *ebl, int osabi, char *buf, size_t len);


extern const char *ebl_core_note_type_name (Ebl *ebl, uint32_t type, char *buf,
					    size_t len);

extern const char *ebl_object_note_type_name (Ebl *ebl, const char *name,
					      uint32_t type, char *buf,
					      size_t len);

extern void ebl_object_note (Ebl *ebl, const char *name, uint32_t type,
			     uint32_t descsz, const char *desc);

extern bool ebl_check_object_attribute (Ebl *ebl, const char *vendor,
					int tag, uint64_t value,
					const char **tag_name,
					const char **value_name);

extern bool ebl_check_reloc_target_type (Ebl *ebl, Elf64_Word sh_type);


extern bool ebl_debugscn_p (Ebl *ebl, const char *name);

extern bool ebl_copy_reloc_p (Ebl *ebl, int reloc);

extern bool ebl_none_reloc_p (Ebl *ebl, int reloc);

extern bool ebl_relative_reloc_p (Ebl *ebl, int reloc);

extern bool ebl_section_strip_p (Ebl *ebl, const GElf_Ehdr *ehdr,
				 const GElf_Shdr *shdr, const char *name,
				 bool remove_comment, bool only_remove_debug);

extern bool ebl_bss_plt_p (Ebl *ebl);

extern int ebl_sysvhash_entrysize (Ebl *ebl);

extern int ebl_return_value_location (Ebl *ebl,
				      Dwarf_Die *functypedie,
				      const Dwarf_Op **locops);

/* Fill in register information given DWARF register numbers.
   If NAME is null, return the maximum REGNO + 1 that has a name.
   Otherwise, store in NAME the name for DWARF register number REGNO
   and return the number of bytes written (including '\0' terminator).
   Return -1 if NAMELEN is too short or REGNO is negative or too large.
   Return 0 if REGNO is unused (a gap in the DWARF number assignment).
   On success, set *SETNAME to a description like "integer" or "FPU"
   fit for "%s registers" title display, and *PREFIX to the string
   that precedes NAME in canonical assembler syntax (e.g. "%" or "$").
   The NAME string contains identifier characters only (maybe just digits).  */
extern ssize_t ebl_register_info (Ebl *ebl,
				  int regno, char *name, size_t namelen,
				  const char **prefix, const char **setname,
				  int *bits, int *type);

extern int ebl_syscall_abi (Ebl *ebl, int *sp, int *pc,
			    int *callno, int args[6]);

extern int ebl_abi_cfi (Ebl *ebl, Dwarf_CIE *abi_info)
  __nonnull_attribute__ (2);

struct Ebl_Strtab;
struct Ebl_Strent;

extern struct Ebl_Strtab *ebl_strtabinit (bool nullstr);

extern void ebl_strtabfree (struct Ebl_Strtab *st);

extern struct Ebl_Strent *ebl_strtabadd (struct Ebl_Strtab *st,
					 const char *str, size_t len);

extern void ebl_strtabfinalize (struct Ebl_Strtab *st, Elf_Data *data);

extern size_t ebl_strtaboffset (struct Ebl_Strent *se);

extern const char *ebl_string (struct Ebl_Strent *se);


struct Ebl_WStrtab;
struct Ebl_WStrent;

extern struct Ebl_WStrtab *ebl_wstrtabinit (bool nullstr);

extern void ebl_wstrtabfree (struct Ebl_WStrtab *st);

extern struct Ebl_WStrent *ebl_wstrtabadd (struct Ebl_WStrtab *st,
					   const wchar_t *str, size_t len);

extern void ebl_wstrtabfinalize (struct Ebl_WStrtab *st, Elf_Data *data);

extern size_t ebl_wstrtaboffset (struct Ebl_WStrent *se);


struct Ebl_GStrtab;
struct Ebl_GStrent;

extern struct Ebl_GStrtab *ebl_gstrtabinit (unsigned int width, bool nullstr);

extern void ebl_gstrtabfree (struct Ebl_GStrtab *st);

extern struct Ebl_GStrent *ebl_gstrtabadd (struct Ebl_GStrtab *st,
					   const char *str, size_t len);

extern void ebl_gstrtabfinalize (struct Ebl_GStrtab *st, Elf_Data *data);

extern size_t ebl_gstrtaboffset (struct Ebl_GStrent *se);


typedef struct
{
  Dwarf_Half offset;		
  Dwarf_Half regno;		
  uint8_t bits;			
  uint8_t pad;			
  Dwarf_Half count;		
  bool pc_register;
} Ebl_Register_Location;

typedef struct
{
  const char *name;		
  const char *group;		
  Dwarf_Half offset;		
  Dwarf_Half count;
  Elf_Type type;
  char format;
  bool thread_identifier;
  bool pc_register;
} Ebl_Core_Item;

extern int ebl_core_note (Ebl *ebl, const GElf_Nhdr *nhdr, const char *name,
			  GElf_Word *regs_offset, size_t *nregloc,
			  const Ebl_Register_Location **reglocs,
			  size_t *nitems, const Ebl_Core_Item **items)
  __nonnull_attribute__ (1, 2, 3, 4, 5, 6, 7, 8);

extern int ebl_auxv_info (Ebl *ebl, GElf_Xword a_type,
			  const char **name, const char **format)
  __nonnull_attribute__ (1, 3, 4);

typedef bool (ebl_tid_registers_t) (int firstreg, unsigned nregs,
				    const Dwarf_Word *regs, void *arg)
  __nonnull_attribute__ (3);

extern bool ebl_set_initial_registers_tid (Ebl *ebl,
					   pid_t tid,
					   ebl_tid_registers_t *setfunc,
					   void *arg)
  __nonnull_attribute__ (1, 3);

extern size_t ebl_frame_nregs (Ebl *ebl)
  __nonnull_attribute__ (1);

extern GElf_Addr ebl_func_addr_mask (Ebl *ebl);

extern bool ebl_dwarf_to_regno (Ebl *ebl, unsigned *regno)
  __nonnull_attribute__ (1, 2);

extern void ebl_normalize_pc (Ebl *ebl, Dwarf_Addr *pc)
  __nonnull_attribute__ (1, 2);

typedef bool (ebl_tid_registers_get_t) (int firstreg, unsigned nregs,
					Dwarf_Word *regs, void *arg)
  __nonnull_attribute__ (3);

typedef bool (ebl_pid_memory_read_t) (Dwarf_Addr addr, Dwarf_Word *data,
				      void *arg)
  __nonnull_attribute__ (3);

extern bool ebl_unwind (Ebl *ebl, Dwarf_Addr pc, ebl_tid_registers_t *setfunc,
			ebl_tid_registers_get_t *getfunc,
			ebl_pid_memory_read_t *readfunc, void *arg,
			bool *signal_framep)
  __nonnull_attribute__ (1, 3, 4, 5, 7);

extern bool ebl_resolve_sym_value (Ebl *ebl, GElf_Addr *addr)
   __nonnull_attribute__ (2);

#ifdef __cplusplus
}
#endif

#endif	
