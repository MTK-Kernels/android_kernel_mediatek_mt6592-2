/* Accumulation of various pieces of knowledge about ELF.
   Copyright (C) 2000-2012, 2014 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

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

#ifndef _ELF_KNOWLEDGE_H
#define _ELF_KNOWLEDGE_H	1

#include <stdbool.h>


#define SECTION_STRIP_P(shdr, name, remove_comment) \
  			      \
  (((shdr)->sh_flags & SHF_ALLOC) == 0					      \
   				      \
   && (shdr)->sh_type != SHT_NOTE					      \
   && (((shdr)->sh_type) != SHT_PROGBITS				      \
       			      \
       || (name != NULL							      \
	   && strncmp (name, ".gnu.warning.", sizeof ".gnu.warning." - 1) != 0\
	   \
	   && (remove_comment						      \
	       || strcmp (name, ".comment") != 0))))


#define SH_INFO_LINK_P(Shdr) \
  ((Shdr)->sh_type == SHT_REL || (Shdr)->sh_type == SHT_RELA		      \
   || ((Shdr)->sh_flags & SHF_INFO_LINK) != 0)


#define SH_FLAGS_COMBINE(Flags1, Flags2) \
  (((Flags1 | Flags2)							      \
    & (SHF_WRITE | SHF_ALLOC | SHF_EXECINSTR | SHF_LINK_ORDER		      \
       | SHF_OS_NONCONFORMING | SHF_GROUP))				      \
   | (Flags1 & Flags2 & (SHF_MERGE | SHF_STRINGS | SHF_INFO_LINK)))

#define SH_FLAGS_IMPORTANT(Flags) \
  ((Flags) & ~((GElf_Xword) 0 | SHF_LINK_ORDER | SHF_OS_NONCONFORMING))


#define SH_ENTSIZE_HASH(Ehdr) \
  ((Ehdr)->e_machine == EM_ALPHA					      \
   || ((Ehdr)->e_machine == EM_S390					      \
       && (Ehdr)->e_ident[EI_CLASS] == ELFCLASS64) ? 8 : 4)

#endif	
