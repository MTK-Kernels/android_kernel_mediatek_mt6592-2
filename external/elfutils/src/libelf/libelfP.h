/* Internal interfaces for libelf.
   Copyright (C) 1998-2010 Red Hat, Inc.
   This file is part of elfutils.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 1998.

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

#ifndef _LIBELFP_H
#define _LIBELFP_H 1

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <ar.h>
#include <gelf.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define _(Str) dgettext ("elfutils", Str)


#define __elfw2_(Bits, Name) __elf##Bits##_##Name
#define elfw2_(Bits, Name) elf##Bits##_##Name
#define ElfW2_(Bits, Name) Elf##Bits##_##Name
#define ELFW2_(Bits, Name) ELF##Bits##_##Name
#define ELFW_(Name, Bits) Name##Bits
#define __elfw2(Bits, Name) __elfw2_(Bits, Name)
#define elfw2(Bits, Name) elfw2_(Bits, Name)
#define ElfW2(Bits, Name) ElfW2_(Bits, Name)
#define ELFW2(Bits, Name) ELFW2_(Bits, Name)
#define ELFW(Name, Bits)  ELFW_(Name, Bits)


#define ELF32_FSZ_ADDR   4
#define ELF32_FSZ_OFF    4
#define ELF32_FSZ_HALF   2
#define ELF32_FSZ_WORD   4
#define ELF32_FSZ_SWORD  4
#define ELF32_FSZ_XWORD  8
#define ELF32_FSZ_SXWORD 8

#define ELF64_FSZ_ADDR   8
#define ELF64_FSZ_OFF    8
#define ELF64_FSZ_HALF   2
#define ELF64_FSZ_WORD   4
#define ELF64_FSZ_SWORD  4
#define ELF64_FSZ_XWORD  8
#define ELF64_FSZ_SXWORD 8


enum
{
  ELF_F_MMAPPED = 0x40,
  ELF_F_MALLOCED = 0x80,
  ELF_F_FILEDATA = 0x100
};


#include "exttypes.h"


enum
{
  ELF_E_NOERROR = 0,
  ELF_E_UNKNOWN_ERROR,
  ELF_E_UNKNOWN_VERSION,
  ELF_E_UNKNOWN_TYPE,
  ELF_E_INVALID_HANDLE,
  ELF_E_SOURCE_SIZE,
  ELF_E_DEST_SIZE,
  ELF_E_INVALID_ENCODING,
  ELF_E_NOMEM,
  ELF_E_INVALID_FILE,
  ELF_E_INVALID_OP,
  ELF_E_NO_VERSION,
  ELF_E_INVALID_CMD,
  ELF_E_RANGE,
  ELF_E_ARCHIVE_FMAG,
  ELF_E_INVALID_ARCHIVE,
  ELF_E_NO_ARCHIVE,
  ELF_E_NO_INDEX,
  ELF_E_READ_ERROR,
  ELF_E_WRITE_ERROR,
  ELF_E_INVALID_CLASS,
  ELF_E_INVALID_INDEX,
  ELF_E_INVALID_OPERAND,
  ELF_E_INVALID_SECTION,
  ELF_E_INVALID_COMMAND,
  ELF_E_WRONG_ORDER_EHDR,
  ELF_E_FD_DISABLED,
  ELF_E_FD_MISMATCH,
  ELF_E_OFFSET_RANGE,
  ELF_E_NOT_NUL_SECTION,
  ELF_E_DATA_MISMATCH,
  ELF_E_INVALID_SECTION_HEADER,
  ELF_E_INVALID_DATA,
  ELF_E_DATA_ENCODING,
  ELF_E_SECTION_TOO_SMALL,
  ELF_E_INVALID_ALIGN,
  ELF_E_INVALID_SHENTSIZE,
  ELF_E_UPDATE_RO,
  ELF_E_NOFILE,
  ELF_E_GROUP_NOT_REL,
  ELF_E_INVALID_PHDR,
  ELF_E_NO_PHDR,
  ELF_E_INVALID_OFFSET,
  
  ELF_E_NUM
};


typedef struct
{
  Elf_Data d;
  Elf_Scn *s;
} Elf_Data_Scn;


typedef struct Elf_Data_List
{
  
  Elf_Data_Scn data;
  struct Elf_Data_List *next;
  int flags;
} Elf_Data_List;


struct Elf_Scn
{
  Elf_Data_List data_list;	
  Elf_Data_List *data_list_rear; 

  Elf_Data_Scn rawdata;		

  int data_read;		
  int shndx_index;		

  size_t index;			
  struct Elf *elf;		

  union
  {
    Elf32_Shdr *e32;		
    Elf64_Shdr *e64;		
  } shdr;

  unsigned int shdr_flags;	
  unsigned int flags;		

  char *rawdata_base;		
  char *data_base;		

  struct Elf_ScnList *list;	
};


typedef struct Elf_ScnList
{
  unsigned int cnt;		
  unsigned int max;		
  struct Elf_ScnList *next;	
  struct Elf_Scn data[0];	
} Elf_ScnList;


typedef struct Elf_Data_Chunk
{
  Elf_Data_Scn data;
  union
  {
    Elf_Scn dummy_scn;
    struct Elf_Data_Chunk *next;
  };
} Elf_Data_Chunk;


struct Elf
{
  
  void *map_address;

  Elf *parent;
  Elf *next;             

  
  Elf_Kind kind;

  
  Elf_Cmd cmd;

  
  unsigned int class;

  
  int fildes;

  
  off_t start_offset;

  size_t maximum_size;

  /* Describes the way the memory was allocated and if the dirty bit is
     signalled it means that the whole file has to be rewritten since
     the layout changed.  */
  int flags;

  
  int ref_count;

  
  rwlock_define (,lock);

  union
  {
    struct
    {
      
      void *ehdr;
      void *shdr;
      void *phdr;

      Elf_ScnList *scns_last;	
      Elf_Data_Chunk *rawchunks; 
      unsigned int scnincr;	
      int ehdr_flags;		
      int phdr_flags;		
      int shdr_malloced;	
      off64_t sizestr_offset;	
    } elf;

    struct
    {
      Elf32_Ehdr *ehdr;		
      Elf32_Shdr *shdr;		
      Elf32_Phdr *phdr;		
      Elf_ScnList *scns_last;	
      Elf_Data_Chunk *rawchunks; 
      unsigned int scnincr;	
      int ehdr_flags;		
      int phdr_flags;		
      int shdr_malloced;	
      off64_t sizestr_offset;	
      Elf32_Ehdr ehdr_mem;	
      char __e32scnspad[sizeof (Elf64_Ehdr) - sizeof (Elf32_Ehdr)];

      
      Elf_ScnList scns;
    } elf32;

    struct
    {
      Elf64_Ehdr *ehdr;		
      Elf64_Shdr *shdr;		
      Elf64_Phdr *phdr;		
      Elf_ScnList *scns_last;	
      Elf_Data_Chunk *rawchunks; 
      unsigned int scnincr;	
      int ehdr_flags;		
      int phdr_flags;		
      int shdr_malloced;	
      off64_t sizestr_offset;	
      Elf64_Ehdr ehdr_mem;	

      
      Elf_ScnList scns;
    } elf64;

    struct
    {
      Elf *children;		
      Elf_Arsym *ar_sym;	
      size_t ar_sym_num;	
      char *long_names;		
      size_t long_names_len;	
      off_t offset;		
      Elf_Arhdr elf_ar_hdr;	
      struct ar_hdr ar_hdr;	
      char ar_name[16];		
      char raw_name[17];	
    } ar;
  } state;

  
};

typedef void (*xfct_t) (void *, const void *, size_t, int);

extern const xfct_t __elf_xfctstom[EV_NUM - 1][EV_NUM - 1][ELFCLASSNUM - 1][ELF_T_NUM] attribute_hidden;
extern const xfct_t __elf_xfctstof[EV_NUM - 1][EV_NUM - 1][ELFCLASSNUM - 1][ELF_T_NUM] attribute_hidden;


extern const size_t __libelf_type_sizes[EV_NUM - 1][ELFCLASSNUM - 1][ELF_T_NUM] attribute_hidden;
#if EV_NUM != 2
# define elf_typesize(class,type,n) \
  elfw2(class,fsize) (type, n, __libelf_version)
#else
# define elf_typesize(class,type,n) \
  (__libelf_type_sizes[EV_CURRENT - 1][ELFW(ELFCLASS,class) - 1][type] * n)
#endif

extern unsigned int __libelf_version attribute_hidden;

extern int __libelf_fill_byte attribute_hidden;

extern int __libelf_version_initialized attribute_hidden;

#if EV_NUM == 2
# define LIBELF_EV_IDX	0
#else
# define LIBELF_EV_IDX	(__libelf_version - 1)
#endif

#if !ALLOW_UNALIGNED
extern const uint_fast8_t __libelf_type_aligns[EV_NUM - 1][ELFCLASSNUM - 1][ELF_T_NUM] attribute_hidden;
# define __libelf_type_align(class, type)	\
    (__libelf_type_aligns[LIBELF_EV_IDX][class - 1][type] ?: 1)
#else
# define __libelf_type_align(class, type)	1
#endif

extern size_t __elf32_msize (Elf_Type __type, size_t __count,
			     unsigned int __version);
extern size_t __elf64_msize (Elf_Type __type, size_t __count,
			     unsigned int __version);


extern Elf *__libelf_read_mmaped_file (int fildes, void *map_address,
				       off_t offset, size_t maxsize,
				       Elf_Cmd cmd, Elf *parent)
     internal_function;

extern void __libelf_seterrno (int value) internal_function;

extern int __libelf_next_arhdr_wrlock (Elf *elf) internal_function;

extern char *__libelf_readall (Elf *elf) internal_function;

extern int __libelf_readsections (Elf *elf) internal_function;

extern int __libelf_set_rawdata (Elf_Scn *scn) internal_function;
extern int __libelf_set_rawdata_wrlock (Elf_Scn *scn) internal_function;


extern off_t __elf32_updatenull_wrlock (Elf *elf, int *change_bop,
					size_t shnum) internal_function;
extern off_t __elf64_updatenull_wrlock (Elf *elf, int *change_bop,
					size_t shnum) internal_function;

extern int __elf32_updatemmap (Elf *elf, int change_bo, size_t shnum)
     internal_function;
extern int __elf64_updatemmap (Elf *elf, int change_bo, size_t shnum)
     internal_function;
extern int __elf32_updatefile (Elf *elf, int change_bo, size_t shnum)
     internal_function;
extern int __elf64_updatefile (Elf *elf, int change_bo, size_t shnum)
     internal_function;


extern int __elf_end_internal (Elf *__elf) attribute_hidden;
extern Elf *__elf_begin_internal (int __fildes, Elf_Cmd __cmd, Elf *__ref)
     attribute_hidden;
extern Elf32_Ehdr *__elf32_getehdr_wrlock (Elf *__elf) internal_function;
extern Elf64_Ehdr *__elf64_getehdr_wrlock (Elf *__elf) internal_function;
extern Elf32_Ehdr *__elf32_newehdr_internal (Elf *__elf) attribute_hidden;
extern Elf64_Ehdr *__elf64_newehdr_internal (Elf *__elf) attribute_hidden;
extern Elf32_Phdr *__elf32_getphdr_internal (Elf *__elf) attribute_hidden;
extern Elf64_Phdr *__elf64_getphdr_internal (Elf *__elf) attribute_hidden;
extern Elf32_Phdr *__elf32_getphdr_wrlock (Elf *__elf) attribute_hidden;
extern Elf64_Phdr *__elf64_getphdr_wrlock (Elf *__elf) attribute_hidden;
extern Elf32_Phdr *__elf32_newphdr_internal (Elf *__elf, size_t __cnt)
     attribute_hidden;
extern Elf64_Phdr *__elf64_newphdr_internal (Elf *__elf, size_t __cnt)
     attribute_hidden;
extern Elf_Scn *__elf32_offscn_internal (Elf *__elf, Elf32_Off __offset)
     attribute_hidden;
extern Elf_Scn *__elf64_offscn_internal (Elf *__elf, Elf64_Off __offset)
     attribute_hidden;
extern int __elf_getphdrnum_rdlock (Elf *__elf, size_t *__dst)
     internal_function;
extern int __elf_getshdrnum_rdlock (Elf *__elf, size_t *__dst)
     internal_function;
extern int __elf_getshdrstrndx_internal (Elf *__elf, size_t *__dst)
     attribute_hidden;
extern Elf32_Shdr *__elf32_getshdr_rdlock (Elf_Scn *__scn) internal_function;
extern Elf64_Shdr *__elf64_getshdr_rdlock (Elf_Scn *__scn) internal_function;
extern Elf32_Shdr *__elf32_getshdr_wrlock (Elf_Scn *__scn) internal_function;
extern Elf64_Shdr *__elf64_getshdr_wrlock (Elf_Scn *__scn) internal_function;
extern Elf_Scn *__elf_getscn_internal (Elf *__elf, size_t __index)
     attribute_hidden;
extern Elf_Scn *__elf_nextscn_internal (Elf *__elf, Elf_Scn *__scn)
     attribute_hidden;
extern int __elf_scnshndx_internal (Elf_Scn *__scn) attribute_hidden;
extern Elf_Data *__elf_getdata_internal (Elf_Scn *__scn, Elf_Data *__data)
     attribute_hidden;
extern Elf_Data *__elf_getdata_rdlock (Elf_Scn *__scn, Elf_Data *__data)
     internal_function;
extern Elf_Data *__elf_rawdata_internal (Elf_Scn *__scn, Elf_Data *__data)
     attribute_hidden;
extern char *__elf_strptr_internal (Elf *__elf, size_t __index,
				    size_t __offset) attribute_hidden;
extern Elf_Data *__elf32_xlatetom_internal (Elf_Data *__dest,
					    const Elf_Data *__src,
					    unsigned int __encode)
     attribute_hidden;
extern Elf_Data *__elf64_xlatetom_internal (Elf_Data *__dest,
					    const Elf_Data *__src,
					    unsigned int __encode)
     attribute_hidden;
extern Elf_Data *__elf32_xlatetof_internal (Elf_Data *__dest,
					    const Elf_Data *__src,
					    unsigned int __encode)
     attribute_hidden;
extern Elf_Data *__elf64_xlatetof_internal (Elf_Data *__dest,
					    const Elf_Data *__src,
					    unsigned int __encode)
     attribute_hidden;
extern unsigned int __elf_version_internal (unsigned int __version)
     attribute_hidden;
extern unsigned long int __elf_hash_internal (const char *__string)
       __attribute__ ((__pure__, visibility ("hidden")));
extern long int __elf32_checksum_internal (Elf *__elf) attribute_hidden;
extern long int __elf64_checksum_internal (Elf *__elf) attribute_hidden;


extern GElf_Ehdr *__gelf_getehdr_rdlock (Elf *__elf, GElf_Ehdr *__dest)
     internal_function;
extern size_t __gelf_fsize_internal (Elf *__elf, Elf_Type __type,
				     size_t __count, unsigned int __version)
     attribute_hidden;
extern GElf_Shdr *__gelf_getshdr_internal (Elf_Scn *__scn, GElf_Shdr *__dst)
     attribute_hidden;
extern GElf_Sym *__gelf_getsym_internal (Elf_Data *__data, int __ndx,
					 GElf_Sym *__dst) attribute_hidden;


extern uint32_t __libelf_crc32 (uint32_t crc, unsigned char *buf, size_t len)
     attribute_hidden;


#define update_if_changed(var, exp, flag) \
  do {									      \
    __typeof__ (var) *_var = &(var);					      \
    __typeof__ (exp) _exp = (exp);					      \
    if (*_var != _exp)							      \
      {									      \
	*_var = _exp;							      \
	(flag) |= ELF_F_DIRTY;						      \
      }									      \
  } while (0)

#define NOTE_ALIGN(n)	(((n) + 3) & -4U)

#define INVALID_NDX(ndx, type, data) \
  unlikely ((data)->d_size / sizeof (type) <= (unsigned int) (ndx))

#endif  
