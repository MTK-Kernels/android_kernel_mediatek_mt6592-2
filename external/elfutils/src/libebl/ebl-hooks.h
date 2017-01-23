/* Backend hook signatures internal interface for libebl.
   Copyright (C) 2000-2011, 2013, 2014 Red Hat, Inc.
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

const char *EBLHOOK(object_type_name) (int, char *, size_t);

const char *EBLHOOK(reloc_type_name) (int, char *, size_t);

bool EBLHOOK(reloc_type_check) (int);

Elf_Type EBLHOOK(reloc_simple_type) (Ebl *, int);

bool EBLHOOK(reloc_valid_use) (Elf *, int);

bool EBLHOOK(gotpc_reloc_check) (Elf *, int);

const char *EBLHOOK(segment_type_name) (int, char *, size_t);

const char *EBLHOOK(section_type_name) (int, char *, size_t);

const char *EBLHOOK(section_name) (int, int, char *, size_t);

const char *EBLHOOK(machine_flag_name) (GElf_Word *);

bool EBLHOOK(machine_flag_check) (GElf_Word);

bool EBLHOOK(machine_section_flag_check) (GElf_Xword);

bool EBLHOOK(check_special_section) (Ebl *, int,
				     const GElf_Shdr *, const char *);

const char *EBLHOOK(symbol_type_name) (int, char *, size_t);

const char *EBLHOOK(symbol_binding_name) (int, char *, size_t);

const char *EBLHOOK(dynamic_tag_name) (int64_t, char *, size_t);

bool EBLHOOK(dynamic_tag_check) (int64_t);

GElf_Word EBLHOOK(sh_flags_combine) (GElf_Word, GElf_Word);

const char *EBLHOOK(osabi_name) (int, char *, size_t);

const char *EBLHOOK(core_note_type_name) (uint32_t, char *, size_t);

const char *EBLHOOK(object_note_type_name) (const char *, uint32_t,
					    char *, size_t);

int EBLHOOK(core_note) (const GElf_Nhdr *, const char *,
			GElf_Word *, size_t *, const Ebl_Register_Location **,
			size_t *, const Ebl_Core_Item **);

bool EBLHOOK(object_note) (const char *, uint32_t, uint32_t, const char *);

bool EBLHOOK(check_object_attribute) (Ebl *, const char *, int, uint64_t,
				      const char **, const char **);

bool EBLHOOK(check_reloc_target_type) (Ebl *, Elf64_Word);

int EBLHOOK(auxv_info) (GElf_Xword, const char **, const char **);

bool EBLHOOK(debugscn_p) (const char *);

bool EBLHOOK(copy_reloc_p) (int);

bool EBLHOOK(none_reloc_p) (int);

bool EBLHOOK(relative_reloc_p) (int);

bool EBLHOOK(check_special_symbol) (Elf *, GElf_Ehdr *, const GElf_Sym *,
			      const char *, const GElf_Shdr *);

bool EBLHOOK(check_st_other_bits) (unsigned char st_other);

bool EBLHOOK(bss_plt_p) (Elf *);

int EBLHOOK(return_value_location) (Dwarf_Die *functypedie,
				    const Dwarf_Op **locp);

ssize_t EBLHOOK(register_info) (Ebl *ebl,
				int regno, char *name, size_t namelen,
				const char **prefix, const char **setname,
				int *bits, int *type);

int EBLHOOK(syscall_abi) (Ebl *ebl, int *sp, int *pc,
			  int *callno, int args[6]);

int EBLHOOK(disasm) (const uint8_t **startp, const uint8_t *end,
		     GElf_Addr addr, const char *fmt, DisasmOutputCB_t outcb,
		     DisasmGetSymCB_t symcb, void *outcbarg, void *symcbarg);

int EBLHOOK(abi_cfi) (Ebl *ebl, Dwarf_CIE *abi_info);

bool EBLHOOK(set_initial_registers_tid) (pid_t tid,
					 ebl_tid_registers_t *setfunc,
					 void *arg);

bool EBLHOOK(dwarf_to_regno) (Ebl *ebl, unsigned *regno);

void EBLHOOK(normalize_pc) (Ebl *ebl, Dwarf_Addr *pc);

bool EBLHOOK(unwind) (Ebl *ebl, Dwarf_Addr pc, ebl_tid_registers_t *setfunc,
		      ebl_tid_registers_get_t *getfunc,
		      ebl_pid_memory_read_t *readfunc, void *arg,
		      bool *signal_framep);

bool EBLHOOK(resolve_sym_value) (Ebl *ebl, GElf_Addr *addr);

void EBLHOOK(destr) (struct ebl *);
