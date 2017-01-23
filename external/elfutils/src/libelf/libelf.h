/* Interface for libelf.
   Copyright (C) 1998-2010 Red Hat, Inc.
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

#ifndef _LIBELF_H
#define _LIBELF_H 1

#include <sys/types.h>

#include <elf.h>


typedef enum
{
  ELF_T_BYTE,                   
  ELF_T_ADDR,                   
  ELF_T_DYN,                    
  ELF_T_EHDR,                   
  ELF_T_HALF,                   
  ELF_T_OFF,                    
  ELF_T_PHDR,                   
  ELF_T_RELA,                   
  ELF_T_REL,                    
  ELF_T_SHDR,                   
  ELF_T_SWORD,                  
  ELF_T_SYM,                    
  ELF_T_WORD,                   
  ELF_T_XWORD,                  
  ELF_T_SXWORD,                 
  ELF_T_VDEF,                   
  ELF_T_VDAUX,                  
  ELF_T_VNEED,                  
  ELF_T_VNAUX,                  
  ELF_T_NHDR,                   
  ELF_T_SYMINFO,		
  ELF_T_MOVE,			
  ELF_T_LIB,			
  ELF_T_GNUHASH,		
  ELF_T_AUXV,			
  
  ELF_T_NUM
} Elf_Type;

typedef struct
{
  void *d_buf;			
  Elf_Type d_type;		
  unsigned int d_version;	
  size_t d_size;		
  loff_t d_off;			
  size_t d_align;		
} Elf_Data;


typedef enum
{
  ELF_C_NULL,			
  ELF_C_READ,			
  ELF_C_RDWR,			
  ELF_C_WRITE,			
  ELF_C_CLR,			
  ELF_C_SET,			
  ELF_C_FDDONE,			
  ELF_C_FDREAD,			
  
  ELF_C_READ_MMAP,		
  ELF_C_RDWR_MMAP,		
  ELF_C_WRITE_MMAP,		
  ELF_C_READ_MMAP_PRIVATE,	/* Read, but memory is writable, results are
				   not written to the file.  */
  ELF_C_EMPTY,			
  
  ELF_C_NUM
} Elf_Cmd;


enum
{
  ELF_F_DIRTY = 0x1,
#define ELF_F_DIRTY		ELF_F_DIRTY
  ELF_F_LAYOUT = 0x4,
#define ELF_F_LAYOUT		ELF_F_LAYOUT
  ELF_F_PERMISSIVE = 0x8
#define ELF_F_PERMISSIVE	ELF_F_PERMISSIVE
};


typedef enum
{
  ELF_K_NONE,			
  ELF_K_AR,			
  ELF_K_COFF,			
  ELF_K_ELF,			
  
  ELF_K_NUM
} Elf_Kind;


typedef struct
{
  char *ar_name;		
  time_t ar_date;		
  uid_t ar_uid;			
  gid_t ar_gid;			
  mode_t ar_mode;		
  loff_t ar_size;		
  char *ar_rawname;		
} Elf_Arhdr;


typedef struct
{
  char *as_name;		
  size_t as_off;		
  unsigned long int as_hash;	
} Elf_Arsym;


typedef struct Elf Elf;

typedef struct Elf_Scn Elf_Scn;


#ifdef __cplusplus
extern "C" {
#endif

extern Elf *elf_begin (int __fildes, Elf_Cmd __cmd, Elf *__ref);

  extern Elf *elf_clone (Elf *__elf, Elf_Cmd __cmd);

extern Elf *elf_memory (char *__image, size_t __size);

extern Elf_Cmd elf_next (Elf *__elf);

extern int elf_end (Elf *__elf);

extern loff_t elf_update (Elf *__elf, Elf_Cmd __cmd);

extern Elf_Kind elf_kind (Elf *__elf) __attribute__ ((__pure__));

extern loff_t elf_getbase (Elf *__elf);


extern char *elf_getident (Elf *__elf, size_t *__nbytes);

extern Elf32_Ehdr *elf32_getehdr (Elf *__elf);
extern Elf64_Ehdr *elf64_getehdr (Elf *__elf);

extern Elf32_Ehdr *elf32_newehdr (Elf *__elf);
extern Elf64_Ehdr *elf64_newehdr (Elf *__elf);

extern int elf_getphdrnum (Elf *__elf, size_t *__dst);

extern Elf32_Phdr *elf32_getphdr (Elf *__elf);
extern Elf64_Phdr *elf64_getphdr (Elf *__elf);

extern Elf32_Phdr *elf32_newphdr (Elf *__elf, size_t __cnt);
extern Elf64_Phdr *elf64_newphdr (Elf *__elf, size_t __cnt);


extern Elf_Scn *elf_getscn (Elf *__elf, size_t __index);

extern Elf_Scn *elf32_offscn (Elf *__elf, Elf32_Off __offset);
extern Elf_Scn *elf64_offscn (Elf *__elf, Elf64_Off __offset);

extern size_t elf_ndxscn (Elf_Scn *__scn);

extern Elf_Scn *elf_nextscn (Elf *__elf, Elf_Scn *__scn);

extern Elf_Scn *elf_newscn (Elf *__elf);

extern int elf_scnshndx (Elf_Scn *__scn);

extern int elf_getshdrnum (Elf *__elf, size_t *__dst);
extern int elf_getshnum (Elf *__elf, size_t *__dst)
     __attribute__ ((__deprecated__));


extern int elf_getshdrstrndx (Elf *__elf, size_t *__dst);
extern int elf_getshstrndx (Elf *__elf, size_t *__dst)
     __attribute__ ((__deprecated__));


extern Elf32_Shdr *elf32_getshdr (Elf_Scn *__scn);
extern Elf64_Shdr *elf64_getshdr (Elf_Scn *__scn);


extern unsigned int elf_flagelf (Elf *__elf, Elf_Cmd __cmd,
				 unsigned int __flags);
extern unsigned int elf_flagehdr (Elf *__elf, Elf_Cmd __cmd,
				  unsigned int __flags);
extern unsigned int elf_flagphdr (Elf *__elf, Elf_Cmd __cmd,
				  unsigned int __flags);
extern unsigned int elf_flagscn (Elf_Scn *__scn, Elf_Cmd __cmd,
				 unsigned int __flags);
extern unsigned int elf_flagdata (Elf_Data *__data, Elf_Cmd __cmd,
				  unsigned int __flags);
extern unsigned int elf_flagshdr (Elf_Scn *__scn, Elf_Cmd __cmd,
				  unsigned int __flags);


extern Elf_Data *elf_getdata (Elf_Scn *__scn, Elf_Data *__data);

extern Elf_Data *elf_rawdata (Elf_Scn *__scn, Elf_Data *__data);

extern Elf_Data *elf_newdata (Elf_Scn *__scn);

extern Elf_Data *elf_getdata_rawchunk (Elf *__elf,
				       loff_t __offset, size_t __size,
				       Elf_Type __type);


extern char *elf_strptr (Elf *__elf, size_t __index, size_t __offset);


extern Elf_Arhdr *elf_getarhdr (Elf *__elf);

extern loff_t elf_getaroff (Elf *__elf);

extern size_t elf_rand (Elf *__elf, size_t __offset);

extern Elf_Arsym *elf_getarsym (Elf *__elf, size_t *__narsyms);


extern int elf_cntl (Elf *__elf, Elf_Cmd __cmd);

extern char *elf_rawfile (Elf *__elf, size_t *__nbytes);


extern size_t elf32_fsize (Elf_Type __type, size_t __count,
			   unsigned int __version)
       __attribute__ ((__const__));
extern size_t elf64_fsize (Elf_Type __type, size_t __count,
			   unsigned int __version)
       __attribute__ ((__const__));


extern Elf_Data *elf32_xlatetom (Elf_Data *__dest, const Elf_Data *__src,
				 unsigned int __encode);
extern Elf_Data *elf64_xlatetom (Elf_Data *__dest, const Elf_Data *__src,
				 unsigned int __encode);

extern Elf_Data *elf32_xlatetof (Elf_Data *__dest, const Elf_Data *__src,
				 unsigned int __encode);
extern Elf_Data *elf64_xlatetof (Elf_Data *__dest, const Elf_Data *__src,
				 unsigned int __encode);


extern int elf_errno (void);

extern const char *elf_errmsg (int __error);


extern unsigned int elf_version (unsigned int __version);

extern void elf_fill (int __fill);

extern unsigned long int elf_hash (const char *__string)
       __attribute__ ((__pure__));

extern unsigned long int elf_gnu_hash (const char *__string)
       __attribute__ ((__pure__));


extern long int elf32_checksum (Elf *__elf);
extern long int elf64_checksum (Elf *__elf);

#ifdef __cplusplus
}
#endif

#endif  
