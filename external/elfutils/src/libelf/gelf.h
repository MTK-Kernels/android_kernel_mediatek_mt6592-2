/* This file defines generic ELF types, structures, and macros.
   Copyright (C) 1999, 2000, 2001, 2002, 2004, 2005, 2007 Red Hat, Inc.
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

#ifndef _GELF_H
#define	_GELF_H 1

#include <libelf.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef Elf64_Half GElf_Half;

typedef Elf64_Word GElf_Word;
typedef	Elf64_Sword GElf_Sword;

typedef Elf64_Xword GElf_Xword;
typedef	Elf64_Sxword GElf_Sxword;

typedef Elf64_Addr GElf_Addr;

typedef Elf64_Off GElf_Off;


typedef Elf64_Ehdr GElf_Ehdr;

typedef Elf64_Shdr GElf_Shdr;

typedef Elf64_Section GElf_Section;

typedef Elf64_Sym GElf_Sym;

typedef Elf64_Syminfo GElf_Syminfo;

typedef Elf64_Rel GElf_Rel;

typedef Elf64_Rela GElf_Rela;

typedef Elf64_Phdr GElf_Phdr;

typedef Elf64_Dyn GElf_Dyn;


typedef Elf64_Verdef GElf_Verdef;

typedef Elf64_Verdaux GElf_Verdaux;

typedef Elf64_Verneed GElf_Verneed;

typedef Elf64_Vernaux GElf_Vernaux;


typedef Elf64_Versym GElf_Versym;


typedef Elf64_auxv_t GElf_auxv_t;


typedef Elf64_Nhdr GElf_Nhdr;


typedef Elf64_Move GElf_Move;


typedef Elf64_Lib GElf_Lib;



#define GELF_ST_BIND(val)		ELF64_ST_BIND (val)
#define GELF_ST_TYPE(val)		ELF64_ST_TYPE (val)
#define GELF_ST_INFO(bind, type)	ELF64_ST_INFO (bind, type)


#define GELF_ST_VISIBILITY(val)		ELF64_ST_VISIBILITY (val)



#define GELF_R_SYM(info)		ELF64_R_SYM (info)
#define GELF_R_TYPE(info)		ELF64_R_TYPE (info)
#define GELF_R_INFO(sym, type)		ELF64_R_INFO (sym, type)


#define GELF_M_SYM(info)		ELF64_M_SYM (info)
#define GELF_M_SIZE(info)		ELF64_M_SIZE (info)
#define GELF_M_INFO(sym, size)		ELF64_M_INFO (sym, size)


extern int gelf_getclass (Elf *__elf);


extern size_t gelf_fsize (Elf *__elf, Elf_Type __type, size_t __count,
			  unsigned int __version);

extern GElf_Ehdr *gelf_getehdr (Elf *__elf, GElf_Ehdr *__dest);

extern int gelf_update_ehdr (Elf *__elf, GElf_Ehdr *__src);

extern unsigned long int gelf_newehdr (Elf *__elf, int __class);

extern Elf_Scn *gelf_offscn (Elf *__elf, GElf_Off __offset);

extern GElf_Shdr *gelf_getshdr (Elf_Scn *__scn, GElf_Shdr *__dst);

extern int gelf_update_shdr (Elf_Scn *__scn, GElf_Shdr *__src);

extern GElf_Phdr *gelf_getphdr (Elf *__elf, int __ndx, GElf_Phdr *__dst);

extern int gelf_update_phdr (Elf *__elf, int __ndx, GElf_Phdr *__src);

extern unsigned long int gelf_newphdr (Elf *__elf, size_t __phnum);


extern Elf_Data *gelf_xlatetom (Elf *__elf, Elf_Data *__dest,
				const Elf_Data *__src, unsigned int __encode);

extern Elf_Data *gelf_xlatetof (Elf *__elf, Elf_Data *__dest,
				const Elf_Data *__src, unsigned int __encode);


extern GElf_Rel *gelf_getrel (Elf_Data *__data, int __ndx, GElf_Rel *__dst);

extern GElf_Rela *gelf_getrela (Elf_Data *__data, int __ndx, GElf_Rela *__dst);

extern int gelf_update_rel (Elf_Data *__dst, int __ndx, GElf_Rel *__src);

extern int gelf_update_rela (Elf_Data *__dst, int __ndx, GElf_Rela *__src);


extern GElf_Sym *gelf_getsym (Elf_Data *__data, int __ndx, GElf_Sym *__dst);

extern int gelf_update_sym (Elf_Data *__data, int __ndx, GElf_Sym *__src);


extern GElf_Sym *gelf_getsymshndx (Elf_Data *__symdata, Elf_Data *__shndxdata,
				   int __ndx, GElf_Sym *__sym,
				   Elf32_Word *__xshndx);

extern int gelf_update_symshndx (Elf_Data *__symdata, Elf_Data *__shndxdata,
				 int __ndx, GElf_Sym *__sym,
				 Elf32_Word __xshndx);


extern GElf_Syminfo *gelf_getsyminfo (Elf_Data *__data, int __ndx,
				      GElf_Syminfo *__dst);

extern int gelf_update_syminfo (Elf_Data *__data, int __ndx,
				GElf_Syminfo *__src);


extern GElf_Dyn *gelf_getdyn (Elf_Data *__data, int __ndx, GElf_Dyn *__dst);

extern int gelf_update_dyn (Elf_Data *__dst, int __ndx, GElf_Dyn *__src);


extern GElf_Move *gelf_getmove (Elf_Data *__data, int __ndx, GElf_Move *__dst);

extern int gelf_update_move (Elf_Data *__data, int __ndx,
			     GElf_Move *__src);


extern GElf_Lib *gelf_getlib (Elf_Data *__data, int __ndx, GElf_Lib *__dst);

extern int gelf_update_lib (Elf_Data *__data, int __ndx, GElf_Lib *__src);



extern GElf_Versym *gelf_getversym (Elf_Data *__data, int __ndx,
				    GElf_Versym *__dst);

extern int gelf_update_versym (Elf_Data *__data, int __ndx,
			       GElf_Versym *__src);


extern GElf_Verneed *gelf_getverneed (Elf_Data *__data, int __offset,
				      GElf_Verneed *__dst);

extern int gelf_update_verneed (Elf_Data *__data, int __offset,
				GElf_Verneed *__src);

extern GElf_Vernaux *gelf_getvernaux (Elf_Data *__data, int __offset,
				      GElf_Vernaux *__dst);

extern int gelf_update_vernaux (Elf_Data *__data, int __offset,
				GElf_Vernaux *__src);


extern GElf_Verdef *gelf_getverdef (Elf_Data *__data, int __offset,
				    GElf_Verdef *__dst);

extern int gelf_update_verdef (Elf_Data *__data, int __offset,
			       GElf_Verdef *__src);

extern GElf_Verdaux *gelf_getverdaux (Elf_Data *__data, int __offset,
				      GElf_Verdaux *__dst);

extern int gelf_update_verdaux (Elf_Data *__data, int __offset,
				GElf_Verdaux *__src);


extern GElf_auxv_t *gelf_getauxv (Elf_Data *__data, int __ndx,
				  GElf_auxv_t *__dst);

extern int gelf_update_auxv (Elf_Data *__data, int __ndx, GElf_auxv_t *__src);


extern size_t gelf_getnote (Elf_Data *__data, size_t __offset,
			    GElf_Nhdr *__result,
			    size_t *__name_offset, size_t *__desc_offset);


extern long int gelf_checksum (Elf *__elf);

#ifdef __cplusplus
}
#endif

#endif	
