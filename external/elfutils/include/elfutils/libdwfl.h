/* Interfaces for libdwfl.
   Copyright (C) 2005-2010, 2013 Red Hat, Inc.
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

#ifndef _LIBDWFL_H
#define _LIBDWFL_H	1

#include "libdw.h"
#include <stdio.h>

typedef struct Dwfl Dwfl;

typedef struct Dwfl_Module Dwfl_Module;

typedef struct Dwfl_Line Dwfl_Line;

typedef struct Dwfl_Thread Dwfl_Thread;

typedef struct Dwfl_Frame Dwfl_Frame;

typedef struct
{
  int (*find_elf) (Dwfl_Module *mod, void **userdata,
		   const char *modname, Dwarf_Addr base,
		   char **file_name, Elf **elfp);

  int (*find_debuginfo) (Dwfl_Module *mod, void **userdata,
			 const char *modname, Dwarf_Addr base,
			 const char *file_name,
			 const char *debuglink_file, GElf_Word debuglink_crc,
			 char **debuginfo_file_name);

  int (*section_address) (Dwfl_Module *mod, void **userdata,
			  const char *modname, Dwarf_Addr base,
			  const char *secname,
			  GElf_Word shndx, const GElf_Shdr *shdr,
			  Dwarf_Addr *addr);

  char **debuginfo_path;	
} Dwfl_Callbacks;


#ifdef __cplusplus
extern "C" {
#endif

extern Dwfl *dwfl_begin (const Dwfl_Callbacks *callbacks)
  __nonnull_attribute__ (1);


extern void dwfl_end (Dwfl *);

extern const char *dwfl_version (Dwfl *);

extern int dwfl_errno (void);

extern const char *dwfl_errmsg (int err);


extern void dwfl_report_begin (Dwfl *dwfl);

extern int dwfl_report_segment (Dwfl *dwfl, int ndx,
				const GElf_Phdr *phdr, GElf_Addr bias,
				const void *ident);

extern Dwfl_Module *dwfl_report_module (Dwfl *dwfl, const char *name,
					Dwarf_Addr start, Dwarf_Addr end);

extern Dwfl_Module *dwfl_report_elf (Dwfl *dwfl, const char *name,
				     const char *file_name, int fd,
				     GElf_Addr base, bool add_p_vaddr);

extern Dwfl_Module *dwfl_report_offline (Dwfl *dwfl, const char *name,
					 const char *file_name, int fd);


extern int dwfl_report_end (Dwfl *dwfl,
			    int (*removed) (Dwfl_Module *, void *,
					    const char *, Dwarf_Addr,
					    void *arg),
			    void *arg);

extern void dwfl_report_begin_add (Dwfl *dwfl);


extern const char *dwfl_module_info (Dwfl_Module *mod, void ***userdata,
				     Dwarf_Addr *start, Dwarf_Addr *end,
				     Dwarf_Addr *dwbias, Dwarf_Addr *symbias,
				     const char **mainfile,
				     const char **debugfile);

extern ptrdiff_t dwfl_getmodules (Dwfl *dwfl,
				  int (*callback) (Dwfl_Module *, void **,
						   const char *, Dwarf_Addr,
						   void *arg),
				  void *arg,
				  ptrdiff_t offset);

extern Dwfl_Module *dwfl_addrmodule (Dwfl *dwfl, Dwarf_Addr address);

extern int dwfl_addrsegment (Dwfl *dwfl, Dwarf_Addr address, Dwfl_Module **mod);



extern int dwfl_module_report_build_id (Dwfl_Module *mod,
					const unsigned char *bits, size_t len,
					GElf_Addr vaddr)
  __nonnull_attribute__ (2);

extern int dwfl_module_build_id (Dwfl_Module *mod,
				 const unsigned char **bits, GElf_Addr *vaddr)
  __nonnull_attribute__ (2, 3);




extern int dwfl_build_id_find_elf (Dwfl_Module *, void **,
				   const char *, Dwarf_Addr,
				   char **, Elf **);

extern int dwfl_build_id_find_debuginfo (Dwfl_Module *, void **,
					 const char *, Dwarf_Addr,
					 const char *, const char *,
					 GElf_Word, char **);

extern int dwfl_standard_find_debuginfo (Dwfl_Module *, void **,
					 const char *, Dwarf_Addr,
					 const char *, const char *,
					 GElf_Word, char **);


extern int dwfl_offline_section_address (Dwfl_Module *, void **,
					 const char *, Dwarf_Addr,
					 const char *, GElf_Word,
					 const GElf_Shdr *,
					 Dwarf_Addr *addr);


extern int dwfl_linux_kernel_find_elf (Dwfl_Module *, void **,
				       const char *, Dwarf_Addr,
				       char **, Elf **);
extern int dwfl_linux_kernel_module_section_address (Dwfl_Module *, void **,
						     const char *, Dwarf_Addr,
						     const char *, GElf_Word,
						     const GElf_Shdr *,
						     Dwarf_Addr *addr);

extern int dwfl_linux_kernel_report_kernel (Dwfl *dwfl);

extern int dwfl_linux_kernel_report_modules (Dwfl *dwfl);

extern int dwfl_linux_kernel_report_offline (Dwfl *dwfl, const char *release,
					     int (*predicate) (const char *,
							       const char *));

extern int dwfl_core_file_report (Dwfl *dwfl, Elf *elf, const char *executable);

extern int dwfl_linux_proc_report (Dwfl *dwfl, pid_t pid);

extern int dwfl_linux_proc_maps_report (Dwfl *dwfl, FILE *);

extern int dwfl_linux_proc_find_elf (Dwfl_Module *mod, void **userdata,
				     const char *module_name, Dwarf_Addr base,
				     char **file_name, Elf **);

struct argp;
extern const struct argp *dwfl_standard_argp (void) __attribute__ ((const));



extern int dwfl_module_relocations (Dwfl_Module *mod);

extern int dwfl_module_relocate_address (Dwfl_Module *mod,
					 Dwarf_Addr *address);

extern const char *dwfl_module_relocation_info (Dwfl_Module *mod,
						unsigned int idx,
						GElf_Word *shndxp);

extern int dwfl_validate_address (Dwfl *dwfl,
				  Dwarf_Addr address, Dwarf_Sword offset);



extern Elf *dwfl_module_getelf (Dwfl_Module *, GElf_Addr *bias)
  __nonnull_attribute__ (1, 2);

extern int dwfl_module_getsymtab (Dwfl_Module *mod);

extern int dwfl_module_getsymtab_first_global (Dwfl_Module *mod);

extern const char *dwfl_module_getsym (Dwfl_Module *mod, int ndx,
				       GElf_Sym *sym, GElf_Word *shndxp)
  __nonnull_attribute__ (3);

extern const char *dwfl_module_getsym_info (Dwfl_Module *mod, int ndx,
					    GElf_Sym *sym, GElf_Addr *addr,
					    GElf_Word *shndxp,
					    Elf **elfp, Dwarf_Addr *bias)
  __nonnull_attribute__ (3, 4);

extern const char *dwfl_module_addrname (Dwfl_Module *mod, GElf_Addr address);

extern const char *dwfl_module_addrinfo (Dwfl_Module *mod, GElf_Addr address,
					 GElf_Off *offset, GElf_Sym *sym,
					 GElf_Word *shndxp, Elf **elfp,
					 Dwarf_Addr *bias)
  __nonnull_attribute__ (3);

extern const char *dwfl_module_addrsym (Dwfl_Module *mod, GElf_Addr address,
					GElf_Sym *sym, GElf_Word *shndxp)
  __nonnull_attribute__ (3);

extern Elf_Scn *dwfl_module_address_section (Dwfl_Module *mod,
					     Dwarf_Addr *address,
					     Dwarf_Addr *bias)
  __nonnull_attribute__ (2, 3);



extern Dwarf *dwfl_module_getdwarf (Dwfl_Module *, Dwarf_Addr *bias)
     __nonnull_attribute__ (2);

extern ptrdiff_t dwfl_getdwarf (Dwfl *,
				int (*callback) (Dwfl_Module *, void **,
						 const char *, Dwarf_Addr,
						 Dwarf *, Dwarf_Addr, void *),
				void *arg, ptrdiff_t offset);

extern Dwarf *dwfl_addrdwarf (Dwfl *dwfl, Dwarf_Addr addr, Dwarf_Addr *bias)
     __nonnull_attribute__ (3);


extern Dwarf_Die *dwfl_addrdie (Dwfl *dwfl, Dwarf_Addr addr, Dwarf_Addr *bias)
     __nonnull_attribute__ (3);
extern Dwarf_Die *dwfl_module_addrdie (Dwfl_Module *mod,
				       Dwarf_Addr addr, Dwarf_Addr *bias)
     __nonnull_attribute__ (3);

extern Dwarf_Die *dwfl_nextcu (Dwfl *dwfl, Dwarf_Die *lastcu, Dwarf_Addr *bias)
     __nonnull_attribute__ (3);
extern Dwarf_Die *dwfl_module_nextcu (Dwfl_Module *mod,
				      Dwarf_Die *lastcu, Dwarf_Addr *bias)
     __nonnull_attribute__ (3);

extern Dwfl_Module *dwfl_cumodule (Dwarf_Die *cudie);


extern int dwfl_getsrclines (Dwarf_Die *cudie, size_t *nlines);

extern Dwfl_Line *dwfl_onesrcline (Dwarf_Die *cudie, size_t idx);

extern Dwfl_Line *dwfl_module_getsrc (Dwfl_Module *mod, Dwarf_Addr addr);
extern Dwfl_Line *dwfl_getsrc (Dwfl *dwfl, Dwarf_Addr addr);

extern int dwfl_module_getsrc_file (Dwfl_Module *mod,
				    const char *fname, int lineno, int column,
				    Dwfl_Line ***srcsp, size_t *nsrcs);

extern Dwfl_Module *dwfl_linemodule (Dwfl_Line *line);

extern Dwarf_Die *dwfl_linecu (Dwfl_Line *line);

extern const char *dwfl_lineinfo (Dwfl_Line *line, Dwarf_Addr *addr,
				  int *linep, int *colp,
				  Dwarf_Word *mtime, Dwarf_Word *length);

  
extern Dwarf_Line *dwfl_dwarf_line (Dwfl_Line *line, Dwarf_Addr *bias);

extern const char *dwfl_line_comp_dir (Dwfl_Line *line);



extern int dwfl_module_return_value_location (Dwfl_Module *mod,
					      Dwarf_Die *functypedie,
					      const Dwarf_Op **locops);

extern int dwfl_module_register_names (Dwfl_Module *mod,
				       int (*callback) (void *arg,
							int regno,
							const char *setname,
							const char *prefix,
							const char *regname,
							int bits, int type),
				       void *arg);


extern Dwarf_CFI *dwfl_module_dwarf_cfi (Dwfl_Module *mod, Dwarf_Addr *bias);
extern Dwarf_CFI *dwfl_module_eh_cfi (Dwfl_Module *mod, Dwarf_Addr *bias);


typedef struct
{
  pid_t (*next_thread) (Dwfl *dwfl, void *dwfl_arg, void **thread_argp)
    __nonnull_attribute__ (1);

  bool (*get_thread) (Dwfl *dwfl, pid_t tid, void *dwfl_arg,
		      void **thread_argp)
    __nonnull_attribute__ (1);

  bool (*memory_read) (Dwfl *dwfl, Dwarf_Addr addr, Dwarf_Word *result,
                       void *dwfl_arg)
    __nonnull_attribute__ (1, 3);

  bool (*set_initial_registers) (Dwfl_Thread *thread, void *thread_arg)
    __nonnull_attribute__ (1);

  void (*detach) (Dwfl *dwfl, void *dwfl_arg)
    __nonnull_attribute__ (1);

  void (*thread_detach) (Dwfl_Thread *thread, void *thread_arg)
    __nonnull_attribute__ (1);
} Dwfl_Thread_Callbacks;

bool dwfl_attach_state (Dwfl *dwfl, Elf *elf, pid_t pid,
                        const Dwfl_Thread_Callbacks *thread_callbacks,
			void *dwfl_arg)
  __nonnull_attribute__ (1, 4);

extern int dwfl_core_file_attach (Dwfl *dwfl, Elf *elf);

extern int dwfl_linux_proc_attach (Dwfl *dwfl, pid_t pid,
				   bool assume_ptrace_stopped);

pid_t dwfl_pid (Dwfl *dwfl)
  __nonnull_attribute__ (1);

Dwfl *dwfl_thread_dwfl (Dwfl_Thread *thread)
  __nonnull_attribute__ (1);

pid_t dwfl_thread_tid (Dwfl_Thread *thread)
  __nonnull_attribute__ (1);

Dwfl_Thread *dwfl_frame_thread (Dwfl_Frame *state)
  __nonnull_attribute__ (1);

bool dwfl_thread_state_registers (Dwfl_Thread *thread, int firstreg,
                                  unsigned nregs, const Dwarf_Word *regs)
  __nonnull_attribute__ (1, 4);

void dwfl_thread_state_register_pc (Dwfl_Thread *thread, Dwarf_Word pc)
  __nonnull_attribute__ (1);

int dwfl_getthreads (Dwfl *dwfl,
		     int (*callback) (Dwfl_Thread *thread, void *arg),
		     void *arg)
  __nonnull_attribute__ (1, 2);

int dwfl_thread_getframes (Dwfl_Thread *thread,
			   int (*callback) (Dwfl_Frame *state, void *arg),
			   void *arg)
  __nonnull_attribute__ (1, 2);

int dwfl_getthread_frames (Dwfl *dwfl, pid_t tid,
			   int (*callback) (Dwfl_Frame *thread, void *arg),
			   void *arg)
  __nonnull_attribute__ (1, 3);

bool dwfl_frame_pc (Dwfl_Frame *state, Dwarf_Addr *pc, bool *isactivation)
  __nonnull_attribute__ (1, 2);

#ifdef __cplusplus
}
#endif

#endif	
