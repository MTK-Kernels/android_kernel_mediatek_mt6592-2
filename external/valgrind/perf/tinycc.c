/*
 *  TCC - Tiny C Compiler
 * 
 *  Copyright (c) 2001-2004 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#define _GNU_SOURCE

#define CONFIG_TCCDIR "tinycc-extras"
#define GCC_MAJOR 3
#define HOST_I386 1
#define TCC_VERSION "0.9.23"


#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <setjmp.h>
#include <time.h>
#ifdef WIN32
#include <sys/timeb.h>
#endif
#ifndef WIN32
#include <sys/time.h>
#include <sys/ucontext.h>
#endif


char* dummy_char_star;
size_t dummy_size_t;

/* This file defines standard ELF types, structures, and macros.
   Copyright (C) 1995, 1996, 1997, 1998, 1999 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Ian Lance Taylor <ian@cygnus.com>.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#ifndef _ELF_H
#define	_ELF_H 1

#ifndef WIN32
#include <inttypes.h>
#else
#ifndef __int8_t_defined
#define __int8_t_defined
typedef signed char int8_t;
typedef	short int int16_t;
typedef	int int32_t;
typedef long long int int64_t;
#endif

typedef unsigned char		uint8_t;
typedef unsigned short int	uint16_t;
typedef unsigned int		uint32_t;
typedef unsigned long long int	uint64_t;
#endif


typedef uint16_t Elf32_Half;
typedef uint16_t Elf64_Half;

typedef uint32_t Elf32_Word;
typedef	int32_t  Elf32_Sword;
typedef uint32_t Elf64_Word;
typedef	int32_t  Elf64_Sword;

typedef uint64_t Elf32_Xword;
typedef	int64_t  Elf32_Sxword;
typedef uint64_t Elf64_Xword;
typedef	int64_t  Elf64_Sxword;

typedef uint32_t Elf32_Addr;
typedef uint64_t Elf64_Addr;

typedef uint32_t Elf32_Off;
typedef uint64_t Elf64_Off;

typedef uint16_t Elf32_Section;
typedef uint16_t Elf64_Section;

typedef uint32_t Elf32_Symndx;
typedef uint64_t Elf64_Symndx;



#define EI_NIDENT (16)

typedef struct
{
  unsigned char	e_ident[EI_NIDENT];	
  Elf32_Half	e_type;			
  Elf32_Half	e_machine;		
  Elf32_Word	e_version;		
  Elf32_Addr	e_entry;		
  Elf32_Off	e_phoff;		
  Elf32_Off	e_shoff;		
  Elf32_Word	e_flags;		
  Elf32_Half	e_ehsize;		
  Elf32_Half	e_phentsize;		
  Elf32_Half	e_phnum;		
  Elf32_Half	e_shentsize;		
  Elf32_Half	e_shnum;		
  Elf32_Half	e_shstrndx;		
} Elf32_Ehdr;

typedef struct
{
  unsigned char	e_ident[EI_NIDENT];	
  Elf64_Half	e_type;			
  Elf64_Half	e_machine;		
  Elf64_Word	e_version;		
  Elf64_Addr	e_entry;		
  Elf64_Off	e_phoff;		
  Elf64_Off	e_shoff;		
  Elf64_Word	e_flags;		
  Elf64_Half	e_ehsize;		
  Elf64_Half	e_phentsize;		
  Elf64_Half	e_phnum;		
  Elf64_Half	e_shentsize;		
  Elf64_Half	e_shnum;		
  Elf64_Half	e_shstrndx;		
} Elf64_Ehdr;


#define EI_MAG0		0		
#define ELFMAG0		0x7f		

#define EI_MAG1		1		
#define ELFMAG1		'E'		

#define EI_MAG2		2		
#define ELFMAG2		'L'		

#define EI_MAG3		3		
#define ELFMAG3		'F'		

#define	ELFMAG		"\177ELF"
#define	SELFMAG		4

#define EI_CLASS	4		
#define ELFCLASSNONE	0		
#define ELFCLASS32	1		
#define ELFCLASS64	2		
#define ELFCLASSNUM	3

#define EI_DATA		5		
#define ELFDATANONE	0		
#define ELFDATA2LSB	1		
#define ELFDATA2MSB	2		
#define ELFDATANUM	3

#define EI_VERSION	6		
					

#define EI_OSABI	7		
#define ELFOSABI_SYSV		0	
#define ELFOSABI_HPUX		1	
#define ELFOSABI_FREEBSD        9       
#define ELFOSABI_ARM		97	
#define ELFOSABI_STANDALONE	255	

#define EI_ABIVERSION	8		

#define EI_PAD		9		


#define ET_NONE		0		
#define ET_REL		1		
#define ET_EXEC		2		
#define ET_DYN		3		
#define ET_CORE		4		
#define	ET_NUM		5		
#define ET_LOPROC	0xff00		
#define ET_HIPROC	0xffff		


#define EM_NONE		 0		
#define EM_M32		 1		
#define EM_SPARC	 2		
#define EM_386		 3		
#define EM_68K		 4		
#define EM_88K		 5		
#define EM_486		 6		
#define EM_860		 7		
#define EM_MIPS		 8		
#define EM_S370		 9		
#define EM_MIPS_RS4_BE	10		
#define EM_RS6000	11		

#define EM_PARISC	15		
#define EM_nCUBE	16		
#define EM_VPP500	17		
#define EM_SPARC32PLUS	18		
#define EM_960		19		
#define EM_PPC		20		

#define EM_V800		36		
#define EM_FR20		37		
#define EM_RH32		38		
#define EM_MMA		39		
#define EM_ARM		40		
#define EM_FAKE_ALPHA	41		
#define EM_SH		42		
#define EM_SPARCV9	43		
#define EM_TRICORE	44		
#define EM_ARC		45		
#define EM_H8_300	46		
#define EM_H8_300H	47		
#define EM_H8S		48		
#define EM_H8_500	49		
#define EM_IA_64	50		
#define EM_MIPS_X	51		
#define EM_COLDFIRE	52		
#define EM_68HC12	53		
#define EM_NUM		54


#define EM_ALPHA	0x9026
#define EM_C60          0x9c60


#define EV_NONE		0		
#define EV_CURRENT	1		
#define EV_NUM		2


typedef struct
{
  Elf32_Word	sh_name;		
  Elf32_Word	sh_type;		
  Elf32_Word	sh_flags;		
  Elf32_Addr	sh_addr;		
  Elf32_Off	sh_offset;		
  Elf32_Word	sh_size;		
  Elf32_Word	sh_link;		
  Elf32_Word	sh_info;		
  Elf32_Word	sh_addralign;		
  Elf32_Word	sh_entsize;		
} Elf32_Shdr;

typedef struct
{
  Elf64_Word	sh_name;		
  Elf64_Word	sh_type;		
  Elf64_Xword	sh_flags;		
  Elf64_Addr	sh_addr;		
  Elf64_Off	sh_offset;		
  Elf64_Xword	sh_size;		
  Elf64_Word	sh_link;		
  Elf64_Word	sh_info;		
  Elf64_Xword	sh_addralign;		
  Elf64_Xword	sh_entsize;		
} Elf64_Shdr;


#define SHN_UNDEF	0		
#define SHN_LORESERVE	0xff00		
#define SHN_LOPROC	0xff00		
#define SHN_HIPROC	0xff1f		
#define SHN_ABS		0xfff1		
#define SHN_COMMON	0xfff2		
#define SHN_HIRESERVE	0xffff		


#define SHT_NULL	 0		
#define SHT_PROGBITS	 1		
#define SHT_SYMTAB	 2		
#define SHT_STRTAB	 3		
#define SHT_RELA	 4		
#define SHT_HASH	 5		
#define SHT_DYNAMIC	 6		
#define SHT_NOTE	 7		
#define SHT_NOBITS	 8		
#define SHT_REL		 9		
#define SHT_SHLIB	 10		
#define SHT_DYNSYM	 11		
#define	SHT_NUM		 12		
#define SHT_LOOS	 0x60000000	
#define SHT_LOSUNW	 0x6ffffffb	
#define SHT_SUNW_COMDAT  0x6ffffffb
#define SHT_SUNW_syminfo 0x6ffffffc
#define SHT_GNU_verdef	 0x6ffffffd	
#define SHT_GNU_verneed	 0x6ffffffe	
#define SHT_GNU_versym	 0x6fffffff	
#define SHT_HISUNW	 0x6fffffff	
#define SHT_HIOS	 0x6fffffff	
#define SHT_LOPROC	 0x70000000	
#define SHT_HIPROC	 0x7fffffff	
#define SHT_LOUSER	 0x80000000	
#define SHT_HIUSER	 0x8fffffff	


#define SHF_WRITE	(1 << 0)	
#define SHF_ALLOC	(1 << 1)	
#define SHF_EXECINSTR	(1 << 2)	
#define SHF_MASKPROC	0xf0000000	


typedef struct
{
  Elf32_Word	st_name;		
  Elf32_Addr	st_value;		
  Elf32_Word	st_size;		
  unsigned char	st_info;		
  unsigned char	st_other;		
  Elf32_Section	st_shndx;		
} Elf32_Sym;

typedef struct
{
  Elf64_Word	st_name;		
  unsigned char	st_info;		
  unsigned char st_other;		
  Elf64_Section	st_shndx;		
  Elf64_Addr	st_value;		
  Elf64_Xword	st_size;		
} Elf64_Sym;


typedef struct
{
  Elf32_Half si_boundto;		
  Elf32_Half si_flags;			
} Elf32_Syminfo;

typedef struct
{
  Elf64_Half si_boundto;		
  Elf64_Half si_flags;			
} Elf64_Syminfo;

#define SYMINFO_BT_SELF		0xffff	
#define SYMINFO_BT_PARENT	0xfffe	
#define SYMINFO_BT_LOWRESERVE	0xff00	

#define SYMINFO_FLG_DIRECT	0x0001	
#define SYMINFO_FLG_PASSTHRU	0x0002	
#define SYMINFO_FLG_COPY	0x0004	
#define SYMINFO_FLG_LAZYLOAD	0x0008	
#define SYMINFO_NONE		0
#define SYMINFO_CURRENT		1
#define SYMINFO_NUM		2



#define SHN_UNDEF	0		


#define ELF32_ST_BIND(val)		(((unsigned char) (val)) >> 4)
#define ELF32_ST_TYPE(val)		((val) & 0xf)
#define ELF32_ST_INFO(bind, type)	(((bind) << 4) + ((type) & 0xf))

#define ELF64_ST_BIND(val)		ELF32_ST_BIND (val)
#define ELF64_ST_TYPE(val)		ELF32_ST_TYPE (val)
#define ELF64_ST_INFO(bind, type)	ELF32_ST_INFO ((bind), (type))


#define STB_LOCAL	0		
#define STB_GLOBAL	1		
#define STB_WEAK	2		
#define	STB_NUM		3		
#define STB_LOOS	10		
#define STB_HIOS	12		
#define STB_LOPROC	13		
#define STB_HIPROC	15		


#define STT_NOTYPE	0		
#define STT_OBJECT	1		
#define STT_FUNC	2		
#define STT_SECTION	3		
#define STT_FILE	4		
#define	STT_NUM		5		
#define STT_LOOS	11		
#define STT_HIOS	12		
#define STT_LOPROC	13		
#define STT_HIPROC	15		



#define STN_UNDEF	0		



#define ELF32_ST_VISIBILITY(o)	((o) & 0x03)

#define ELF64_ST_VISIBILITY(o)	ELF32_ST_VISIBILITY (o)

#define STV_DEFAULT	0		
#define STV_INTERNAL	1		
#define STV_HIDDEN	2		
#define STV_PROTECTED	3		



typedef struct
{
  Elf32_Addr	r_offset;		
  Elf32_Word	r_info;			
} Elf32_Rel;


typedef struct
{
  Elf64_Addr	r_offset;		
  Elf64_Xword	r_info;			
} Elf64_Rel;


typedef struct
{
  Elf32_Addr	r_offset;		
  Elf32_Word	r_info;			
  Elf32_Sword	r_addend;		
} Elf32_Rela;

typedef struct
{
  Elf64_Addr	r_offset;		
  Elf64_Xword	r_info;			
  Elf64_Sxword	r_addend;		
} Elf64_Rela;


#define ELF32_R_SYM(val)		((val) >> 8)
#define ELF32_R_TYPE(val)		((val) & 0xff)
#define ELF32_R_INFO(sym, type)		(((sym) << 8) + ((type) & 0xff))

#define ELF64_R_SYM(i)			((i) >> 32)
#define ELF64_R_TYPE(i)			((i) & 0xffffffff)
#define ELF64_R_INFO(sym,type)		(((sym) << 32) + (type))


typedef struct
{
  Elf32_Word	p_type;			
  Elf32_Off	p_offset;		
  Elf32_Addr	p_vaddr;		
  Elf32_Addr	p_paddr;		
  Elf32_Word	p_filesz;		
  Elf32_Word	p_memsz;		
  Elf32_Word	p_flags;		
  Elf32_Word	p_align;		
} Elf32_Phdr;

typedef struct
{
  Elf64_Word	p_type;			
  Elf64_Word	p_flags;		
  Elf64_Off	p_offset;		
  Elf64_Addr	p_vaddr;		
  Elf64_Addr	p_paddr;		
  Elf64_Xword	p_filesz;		
  Elf64_Xword	p_memsz;		
  Elf64_Xword	p_align;		
} Elf64_Phdr;


#define	PT_NULL		0		
#define PT_LOAD		1		
#define PT_DYNAMIC	2		
#define PT_INTERP	3		
#define PT_NOTE		4		
#define PT_SHLIB	5		
#define PT_PHDR		6		
#define	PT_NUM		7		
#define PT_LOOS		0x60000000	
#define PT_HIOS		0x6fffffff	
#define PT_LOPROC	0x70000000	
#define PT_HIPROC	0x7fffffff	


#define PF_X		(1 << 0)	
#define PF_W		(1 << 1)	
#define PF_R		(1 << 2)	
#define PF_MASKPROC	0xf0000000	


#define NT_PRSTATUS	1		
#define NT_FPREGSET	2		
#define NT_PRPSINFO	3		
#define NT_PRXREG	4		
#define NT_PLATFORM	5		
#define NT_AUXV		6		
#define NT_GWINDOWS	7		
#define NT_PSTATUS	10		
#define NT_PSINFO	13		
#define NT_PRCRED	14		
#define NT_UTSNAME	15		
#define NT_LWPSTATUS	16		
#define NT_LWPSINFO	17		


#define NT_VERSION	1		



typedef struct
{
  Elf32_Sword	d_tag;			
  union
    {
      Elf32_Word d_val;			
      Elf32_Addr d_ptr;			
    } d_un;
} Elf32_Dyn;

typedef struct
{
  Elf64_Sxword	d_tag;			
  union
    {
      Elf64_Xword d_val;		
      Elf64_Addr d_ptr;			
    } d_un;
} Elf64_Dyn;


#define DT_NULL		0		
#define DT_NEEDED	1		
#define DT_PLTRELSZ	2		
#define DT_PLTGOT	3		
#define DT_HASH		4		
#define DT_STRTAB	5		
#define DT_SYMTAB	6		
#define DT_RELA		7		
#define DT_RELASZ	8		
#define DT_RELAENT	9		
#define DT_STRSZ	10		
#define DT_SYMENT	11		
#define DT_INIT		12		
#define DT_FINI		13		
#define DT_SONAME	14		
#define DT_RPATH	15		
#define DT_SYMBOLIC	16		
#define DT_REL		17		
#define DT_RELSZ	18		
#define DT_RELENT	19		
#define DT_PLTREL	20		
#define DT_DEBUG	21		
#define DT_TEXTREL	22		
#define DT_JMPREL	23		
#define	DT_BIND_NOW	24		
#define	DT_INIT_ARRAY	25		
#define	DT_FINI_ARRAY	26		
#define	DT_INIT_ARRAYSZ	27		
#define	DT_FINI_ARRAYSZ	28		
#define	DT_NUM		29		
#define DT_LOOS		0x60000000	
#define DT_HIOS		0x6fffffff	
#define DT_LOPROC	0x70000000	
#define DT_HIPROC	0x7fffffff	
#define	DT_PROCNUM	DT_MIPS_NUM	

#define DT_VALRNGLO	0x6ffffd00
#define DT_POSFLAG_1	0x6ffffdfd	
#define DT_SYMINSZ	0x6ffffdfe	
#define DT_SYMINENT	0x6ffffdff	
#define DT_VALRNGHI	0x6ffffdff

#define DT_ADDRRNGLO	0x6ffffe00
#define DT_SYMINFO	0x6ffffeff	
#define DT_ADDRRNGHI	0x6ffffeff

#define DT_VERSYM	0x6ffffff0

#define DT_FLAGS_1	0x6ffffffb	
#define	DT_VERDEF	0x6ffffffc	
#define	DT_VERDEFNUM	0x6ffffffd	
#define	DT_VERNEED	0x6ffffffe	
#define	DT_VERNEEDNUM	0x6fffffff	
#define DT_VERSIONTAGIDX(tag)	(DT_VERNEEDNUM - (tag))	
#define DT_VERSIONTAGNUM 16

#define DT_AUXILIARY    0x7ffffffd      
#define DT_FILTER       0x7fffffff      
#define DT_EXTRATAGIDX(tag)	((Elf32_Word)-((Elf32_Sword) (tag) <<1>>1)-1)
#define DT_EXTRANUM	3

#define DF_1_NOW	0x00000001	
#define DF_1_GLOBAL	0x00000002	
#define DF_1_GROUP	0x00000004	
#define DF_1_NODELETE	0x00000008	
#define DF_1_LOADFLTR	0x00000010	
#define DF_1_INITFIRST	0x00000020	
#define DF_1_NOOPEN	0x00000040	


typedef struct
{
  Elf32_Half	vd_version;		
  Elf32_Half	vd_flags;		
  Elf32_Half	vd_ndx;			
  Elf32_Half	vd_cnt;			
  Elf32_Word	vd_hash;		
  Elf32_Word	vd_aux;			
  Elf32_Word	vd_next;		
} Elf32_Verdef;

typedef struct
{
  Elf64_Half	vd_version;		
  Elf64_Half	vd_flags;		
  Elf64_Half	vd_ndx;			
  Elf64_Half	vd_cnt;			
  Elf64_Word	vd_hash;		
  Elf64_Word	vd_aux;			
  Elf64_Word	vd_next;		
} Elf64_Verdef;


#define VER_DEF_NONE	0		
#define VER_DEF_CURRENT	1		
#define VER_DEF_NUM	2		

#define VER_FLG_BASE	0x1		
#define VER_FLG_WEAK	0x2		


typedef struct
{
  Elf32_Word	vda_name;		
  Elf32_Word	vda_next;		
} Elf32_Verdaux;

typedef struct
{
  Elf64_Word	vda_name;		
  Elf64_Word	vda_next;		
} Elf64_Verdaux;



typedef struct
{
  Elf32_Half	vn_version;		
  Elf32_Half	vn_cnt;			
  Elf32_Word	vn_file;		
  Elf32_Word	vn_aux;			
  Elf32_Word	vn_next;		
} Elf32_Verneed;

typedef struct
{
  Elf64_Half	vn_version;		
  Elf64_Half	vn_cnt;			
  Elf64_Word	vn_file;		
  Elf64_Word	vn_aux;			
  Elf64_Word	vn_next;		
} Elf64_Verneed;


#define VER_NEED_NONE	 0		
#define VER_NEED_CURRENT 1		
#define VER_NEED_NUM	 2		


typedef struct
{
  Elf32_Word	vna_hash;		
  Elf32_Half	vna_flags;		
  Elf32_Half	vna_other;		
  Elf32_Word	vna_name;		
  Elf32_Word	vna_next;		
} Elf32_Vernaux;

typedef struct
{
  Elf64_Word	vna_hash;		
  Elf64_Half	vna_flags;		
  Elf64_Half	vna_other;		
  Elf64_Word	vna_name;		
  Elf64_Word	vna_next;		
} Elf64_Vernaux;


#define VER_FLG_WEAK	0x2		




typedef struct
{
  int a_type;			
  union
    {
      long int a_val;		
      void *a_ptr;		
      void (*a_fcn) (void);	
    } a_un;
} Elf32_auxv_t;

typedef struct
{
  long int a_type;		
  union
    {
      long int a_val;		
      void *a_ptr;		
      void (*a_fcn) (void);	
    } a_un;
} Elf64_auxv_t;


#define AT_NULL		0		
#define AT_IGNORE	1		
#define AT_EXECFD	2		
#define AT_PHDR		3		
#define AT_PHENT	4		
#define AT_PHNUM	5		
#define AT_PAGESZ	6		
#define AT_BASE		7		
#define AT_FLAGS	8		
#define AT_ENTRY	9		
#define AT_NOTELF	10		
#define AT_UID		11		
#define AT_EUID		12		
#define AT_GID		13		
#define AT_EGID		14		

#define AT_PLATFORM	15		
#define AT_HWCAP	16		

#define AT_FPUCW	17		



typedef struct
{
  Elf32_Word n_namesz;			
  Elf32_Word n_descsz;			
  Elf32_Word n_type;			
} Elf32_Nhdr;

typedef struct
{
  Elf64_Word n_namesz;			
  Elf64_Word n_descsz;			
  Elf64_Word n_type;			
} Elf64_Nhdr;


#define ELF_NOTE_SOLARIS	"SUNW Solaris"

#define ELF_NOTE_GNU		"GNU"



#define ELF_NOTE_PAGESIZE_HINT	1



#define ELF_NOTE_ABI		1

#define ELF_NOTE_OS_LINUX	0
#define ELF_NOTE_OS_GNU		1
#define ELF_NOTE_OS_SOLARIS2	2




#define R_68K_NONE	0		
#define R_68K_32	1		
#define R_68K_16	2		
#define R_68K_8		3		
#define R_68K_PC32	4		
#define R_68K_PC16	5		
#define R_68K_PC8	6		
#define R_68K_GOT32	7		
#define R_68K_GOT16	8		
#define R_68K_GOT8	9		
#define R_68K_GOT32O	10		
#define R_68K_GOT16O	11		
#define R_68K_GOT8O	12		
#define R_68K_PLT32	13		
#define R_68K_PLT16	14		
#define R_68K_PLT8	15		
#define R_68K_PLT32O	16		
#define R_68K_PLT16O	17		
#define R_68K_PLT8O	18		
#define R_68K_COPY	19		
#define R_68K_GLOB_DAT	20		
#define R_68K_JMP_SLOT	21		
#define R_68K_RELATIVE	22		
#define R_68K_NUM	23



#define R_386_NONE	0		
#define R_386_32	1		
#define R_386_PC32	2		
#define R_386_GOT32	3		
#define R_386_PLT32	4		
#define R_386_COPY	5		
#define R_386_GLOB_DAT	6		
#define R_386_JMP_SLOT	7		
#define R_386_RELATIVE	8		
#define R_386_GOTOFF	9		
#define R_386_GOTPC	10		
#define R_386_NUM	11



#define EF_SPARCV9_MM		3
#define EF_SPARCV9_TSO		0
#define EF_SPARCV9_PSO		1
#define EF_SPARCV9_RMO		2
#define EF_SPARC_EXT_MASK	0xFFFF00
#define EF_SPARC_SUN_US1	0x000200
#define EF_SPARC_HAL_R1		0x000400


#define R_SPARC_NONE	0		
#define R_SPARC_8	1		
#define R_SPARC_16	2		
#define R_SPARC_32	3		
#define R_SPARC_DISP8	4		
#define R_SPARC_DISP16	5		
#define R_SPARC_DISP32	6		
#define R_SPARC_WDISP30	7		
#define R_SPARC_WDISP22	8		
#define R_SPARC_HI22	9		
#define R_SPARC_22	10		
#define R_SPARC_13	11		
#define R_SPARC_LO10	12		
#define R_SPARC_GOT10	13		
#define R_SPARC_GOT13	14		
#define R_SPARC_GOT22	15		
#define R_SPARC_PC10	16		
#define R_SPARC_PC22	17		
#define R_SPARC_WPLT30	18		
#define R_SPARC_COPY	19		
#define R_SPARC_GLOB_DAT 20		
#define R_SPARC_JMP_SLOT 21		
#define R_SPARC_RELATIVE 22		
#define R_SPARC_UA32	23		


#define R_SPARC_PLT32	24		
#define R_SPARC_HIPLT22	25		
#define R_SPARC_LOPLT10	26		
#define R_SPARC_PCPLT32	27		
#define R_SPARC_PCPLT22	28		
#define R_SPARC_PCPLT10	29		
#define R_SPARC_10	30		
#define R_SPARC_11	31		
#define R_SPARC_64	32		
#define R_SPARC_OLO10	33		
#define R_SPARC_HH22	34		
#define R_SPARC_HM10	35		
#define R_SPARC_LM22	36		
#define R_SPARC_PC_HH22	37		
#define R_SPARC_PC_HM10	38		
#define R_SPARC_PC_LM22	39		
#define R_SPARC_WDISP16	40		
#define R_SPARC_WDISP19	41		
#define R_SPARC_7	43		
#define R_SPARC_5	44		
#define R_SPARC_6	45		
#define R_SPARC_DISP64	46		
#define R_SPARC_PLT64	47		
#define R_SPARC_HIX22	48		
#define R_SPARC_LOX10	49		
#define R_SPARC_H44	50		
#define R_SPARC_M44	51		
#define R_SPARC_L44	52		
#define R_SPARC_REGISTER 53		
#define R_SPARC_UA64	54		
#define R_SPARC_UA16	55		
#define R_SPARC_NUM	56


#define DT_SPARC_REGISTER 0x70000001
#define DT_SPARC_NUM	2


#define HWCAP_SPARC_FLUSH	1	
#define HWCAP_SPARC_STBAR	2
#define HWCAP_SPARC_SWAP	4
#define HWCAP_SPARC_MULDIV	8
#define HWCAP_SPARC_V9		16	



#define EF_MIPS_NOREORDER   1		
#define EF_MIPS_PIC	    2		
#define EF_MIPS_CPIC	    4		
#define EF_MIPS_XGOT	    8
#define EF_MIPS_64BIT_WHIRL 16
#define EF_MIPS_ABI2	    32
#define EF_MIPS_ABI_ON32    64
#define EF_MIPS_ARCH	    0xf0000000	


#define EF_MIPS_ARCH_1	    0x00000000	
#define EF_MIPS_ARCH_2	    0x10000000	
#define EF_MIPS_ARCH_3	    0x20000000	
#define EF_MIPS_ARCH_4	    0x30000000	
#define EF_MIPS_ARCH_5	    0x40000000	


#define E_MIPS_ARCH_1	  0x00000000	
#define E_MIPS_ARCH_2	  0x10000000	
#define E_MIPS_ARCH_3	  0x20000000	
#define E_MIPS_ARCH_4	  0x30000000	
#define E_MIPS_ARCH_5	  0x40000000	


#define SHN_MIPS_ACOMMON 0xff00		
#define SHN_MIPS_TEXT	 0xff01		
#define SHN_MIPS_DATA	 0xff02		
#define SHN_MIPS_SCOMMON 0xff03		
#define SHN_MIPS_SUNDEFINED 0xff04	


#define SHT_MIPS_LIBLIST       0x70000000 
#define SHT_MIPS_MSYM	       0x70000001
#define SHT_MIPS_CONFLICT      0x70000002 
#define SHT_MIPS_GPTAB	       0x70000003 
#define SHT_MIPS_UCODE	       0x70000004 
#define SHT_MIPS_DEBUG	       0x70000005 
#define SHT_MIPS_REGINFO       0x70000006 
#define SHT_MIPS_PACKAGE       0x70000007
#define SHT_MIPS_PACKSYM       0x70000008
#define SHT_MIPS_RELD	       0x70000009
#define SHT_MIPS_IFACE         0x7000000b
#define SHT_MIPS_CONTENT       0x7000000c
#define SHT_MIPS_OPTIONS       0x7000000d 
#define SHT_MIPS_SHDR	       0x70000010
#define SHT_MIPS_FDESC	       0x70000011
#define SHT_MIPS_EXTSYM	       0x70000012
#define SHT_MIPS_DENSE	       0x70000013
#define SHT_MIPS_PDESC	       0x70000014
#define SHT_MIPS_LOCSYM	       0x70000015
#define SHT_MIPS_AUXSYM	       0x70000016
#define SHT_MIPS_OPTSYM	       0x70000017
#define SHT_MIPS_LOCSTR	       0x70000018
#define SHT_MIPS_LINE	       0x70000019
#define SHT_MIPS_RFDESC	       0x7000001a
#define SHT_MIPS_DELTASYM      0x7000001b
#define SHT_MIPS_DELTAINST     0x7000001c
#define SHT_MIPS_DELTACLASS    0x7000001d
#define SHT_MIPS_DWARF         0x7000001e 
#define SHT_MIPS_DELTADECL     0x7000001f
#define SHT_MIPS_SYMBOL_LIB    0x70000020
#define SHT_MIPS_EVENTS	       0x70000021 
#define SHT_MIPS_TRANSLATE     0x70000022
#define SHT_MIPS_PIXIE	       0x70000023
#define SHT_MIPS_XLATE	       0x70000024
#define SHT_MIPS_XLATE_DEBUG   0x70000025
#define SHT_MIPS_WHIRL	       0x70000026
#define SHT_MIPS_EH_REGION     0x70000027
#define SHT_MIPS_XLATE_OLD     0x70000028
#define SHT_MIPS_PDR_EXCEPTION 0x70000029


#define SHF_MIPS_GPREL	 0x10000000	
#define SHF_MIPS_MERGE	 0x20000000
#define SHF_MIPS_ADDR	 0x40000000
#define SHF_MIPS_STRINGS 0x80000000
#define SHF_MIPS_NOSTRIP 0x08000000
#define SHF_MIPS_LOCAL	 0x04000000
#define SHF_MIPS_NAMES	 0x02000000
#define SHF_MIPS_NODUPE	 0x01000000



#define STO_MIPS_DEFAULT		0x0
#define STO_MIPS_INTERNAL		0x1
#define STO_MIPS_HIDDEN			0x2
#define STO_MIPS_PROTECTED		0x3
#define STO_MIPS_SC_ALIGN_UNUSED	0xff

#define STB_MIPS_SPLIT_COMMON		13


typedef union
{
  struct
    {
      Elf32_Word gt_current_g_value;	
      Elf32_Word gt_unused;		
    } gt_header;			
  struct
    {
      Elf32_Word gt_g_value;		
      Elf32_Word gt_bytes;		
    } gt_entry;				
} Elf32_gptab;


typedef struct
{
  Elf32_Word	ri_gprmask;		
  Elf32_Word	ri_cprmask[4];		
  Elf32_Sword	ri_gp_value;		
} Elf32_RegInfo;


typedef struct
{
  unsigned char kind;		
  unsigned char size;		
  Elf32_Section section;	
  Elf32_Word info;		
} Elf_Options;


#define ODK_NULL	0	
#define ODK_REGINFO	1	
#define ODK_EXCEPTIONS	2	
#define ODK_PAD		3	
#define ODK_HWPATCH	4	
#define ODK_FILL	5	
#define ODK_TAGS	6	
#define ODK_HWAND	7	
#define ODK_HWOR	8	


#define OEX_FPU_MIN	0x1f	
#define OEX_FPU_MAX	0x1f00	
#define OEX_PAGE0	0x10000	
#define OEX_SMM		0x20000	
#define OEX_FPDBUG	0x40000	
#define OEX_PRECISEFP	OEX_FPDBUG
#define OEX_DISMISS	0x80000	

#define OEX_FPU_INVAL	0x10
#define OEX_FPU_DIV0	0x08
#define OEX_FPU_OFLO	0x04
#define OEX_FPU_UFLO	0x02
#define OEX_FPU_INEX	0x01


#define OHW_R4KEOP	0x1	
#define OHW_R8KPFETCH	0x2	
#define OHW_R5KEOP	0x4	
#define OHW_R5KCVTL	0x8	

#define OPAD_PREFIX	0x1
#define OPAD_POSTFIX	0x2
#define OPAD_SYMBOL	0x4


typedef struct
{
  Elf32_Word hwp_flags1;	
  Elf32_Word hwp_flags2;	
} Elf_Options_Hw;


#define OHWA0_R4KEOP_CHECKED	0x00000001
#define OHWA1_R4KEOP_CLEAN	0x00000002


#define R_MIPS_NONE		0	
#define R_MIPS_16		1	
#define R_MIPS_32		2	
#define R_MIPS_REL32		3	
#define R_MIPS_26		4	
#define R_MIPS_HI16		5	
#define R_MIPS_LO16		6	
#define R_MIPS_GPREL16		7	
#define R_MIPS_LITERAL		8	
#define R_MIPS_GOT16		9	
#define R_MIPS_PC16		10	
#define R_MIPS_CALL16		11	
#define R_MIPS_GPREL32		12	

#define R_MIPS_SHIFT5		16
#define R_MIPS_SHIFT6		17
#define R_MIPS_64		18
#define R_MIPS_GOT_DISP		19
#define R_MIPS_GOT_PAGE		20
#define R_MIPS_GOT_OFST		21
#define R_MIPS_GOT_HI16		22
#define R_MIPS_GOT_LO16		23
#define R_MIPS_SUB		24
#define R_MIPS_INSERT_A		25
#define R_MIPS_INSERT_B		26
#define R_MIPS_DELETE		27
#define R_MIPS_HIGHER		28
#define R_MIPS_HIGHEST		29
#define R_MIPS_CALL_HI16	30
#define R_MIPS_CALL_LO16	31
#define R_MIPS_SCN_DISP		32
#define R_MIPS_REL16		33
#define R_MIPS_ADD_IMMEDIATE	34
#define R_MIPS_PJUMP		35
#define R_MIPS_RELGOT		36
#define R_MIPS_JALR		37
#define R_MIPS_NUM		38


#define PT_MIPS_REGINFO	0x70000000	
#define PT_MIPS_RTPROC  0x70000001	
#define PT_MIPS_OPTIONS 0x70000002


#define PF_MIPS_LOCAL	0x10000000


#define DT_MIPS_RLD_VERSION  0x70000001	
#define DT_MIPS_TIME_STAMP   0x70000002	
#define DT_MIPS_ICHECKSUM    0x70000003	
#define DT_MIPS_IVERSION     0x70000004	
#define DT_MIPS_FLAGS	     0x70000005	
#define DT_MIPS_BASE_ADDRESS 0x70000006	
#define DT_MIPS_MSYM	     0x70000007
#define DT_MIPS_CONFLICT     0x70000008	
#define DT_MIPS_LIBLIST	     0x70000009	
#define DT_MIPS_LOCAL_GOTNO  0x7000000a	
#define DT_MIPS_CONFLICTNO   0x7000000b	
#define DT_MIPS_LIBLISTNO    0x70000010	
#define DT_MIPS_SYMTABNO     0x70000011	
#define DT_MIPS_UNREFEXTNO   0x70000012	
#define DT_MIPS_GOTSYM	     0x70000013	
#define DT_MIPS_HIPAGENO     0x70000014	
#define DT_MIPS_RLD_MAP	     0x70000016	
#define DT_MIPS_DELTA_CLASS  0x70000017	
#define DT_MIPS_DELTA_CLASS_NO    0x70000018 
#define DT_MIPS_DELTA_INSTANCE    0x70000019 
#define DT_MIPS_DELTA_INSTANCE_NO 0x7000001a 
#define DT_MIPS_DELTA_RELOC  0x7000001b 
#define DT_MIPS_DELTA_RELOC_NO 0x7000001c 
#define DT_MIPS_DELTA_SYM    0x7000001d 
#define DT_MIPS_DELTA_SYM_NO 0x7000001e 
#define DT_MIPS_DELTA_CLASSSYM 0x70000020 
#define DT_MIPS_DELTA_CLASSSYM_NO 0x70000021 
#define DT_MIPS_CXX_FLAGS    0x70000022 
#define DT_MIPS_PIXIE_INIT   0x70000023
#define DT_MIPS_SYMBOL_LIB   0x70000024
#define DT_MIPS_LOCALPAGE_GOTIDX 0x70000025
#define DT_MIPS_LOCAL_GOTIDX 0x70000026
#define DT_MIPS_HIDDEN_GOTIDX 0x70000027
#define DT_MIPS_PROTECTED_GOTIDX 0x70000028
#define DT_MIPS_OPTIONS	     0x70000029 
#define DT_MIPS_INTERFACE    0x7000002a 
#define DT_MIPS_DYNSTR_ALIGN 0x7000002b
#define DT_MIPS_INTERFACE_SIZE 0x7000002c 
#define DT_MIPS_RLD_TEXT_RESOLVE_ADDR 0x7000002d 
#define DT_MIPS_PERF_SUFFIX  0x7000002e 
#define DT_MIPS_COMPACT_SIZE 0x7000002f 
#define DT_MIPS_GP_VALUE     0x70000030 
#define DT_MIPS_AUX_DYNAMIC  0x70000031 
#define DT_MIPS_NUM	     0x32


#define RHF_NONE		   0		
#define RHF_QUICKSTART		   (1 << 0)	
#define RHF_NOTPOT		   (1 << 1)	
#define RHF_NO_LIBRARY_REPLACEMENT (1 << 2)	
#define RHF_NO_MOVE		   (1 << 3)
#define RHF_SGI_ONLY		   (1 << 4)
#define RHF_GUARANTEE_INIT	   (1 << 5)
#define RHF_DELTA_C_PLUS_PLUS	   (1 << 6)
#define RHF_GUARANTEE_START_INIT   (1 << 7)
#define RHF_PIXIE		   (1 << 8)
#define RHF_DEFAULT_DELAY_LOAD	   (1 << 9)
#define RHF_REQUICKSTART	   (1 << 10)
#define RHF_REQUICKSTARTED	   (1 << 11)
#define RHF_CORD		   (1 << 12)
#define RHF_NO_UNRES_UNDEF	   (1 << 13)
#define RHF_RLD_ORDER_SAFE	   (1 << 14)


typedef struct
{
  Elf32_Word l_name;		
  Elf32_Word l_time_stamp;	
  Elf32_Word l_checksum;	
  Elf32_Word l_version;		
  Elf32_Word l_flags;		
} Elf32_Lib;

typedef struct
{
  Elf64_Word l_name;		
  Elf64_Word l_time_stamp;	
  Elf64_Word l_checksum;	
  Elf64_Word l_version;		
  Elf64_Word l_flags;		
} Elf64_Lib;



#define LL_NONE		  0
#define LL_EXACT_MATCH	  (1 << 0)	
#define LL_IGNORE_INT_VER (1 << 1)	
#define LL_REQUIRE_MINOR  (1 << 2)
#define LL_EXPORTS	  (1 << 3)
#define LL_DELAY_LOAD	  (1 << 4)
#define LL_DELTA	  (1 << 5)


typedef Elf32_Addr Elf32_Conflict;




#define EF_PARISC_TRAPNL	1	
#define EF_PARISC_EXT		2	
#define EF_PARISC_ARCH		0xffff0000 


#define SHT_PARISC_GOT		0x70000000 
#define SHT_PARISC_ARCH		0x70000001 
#define SHT_PARISC_GLOBAL	0x70000002 
#define SHT_PARISC_MILLI	0x70000003 
#define SHT_PARISC_UNWIND	0x70000004 
#define SHT_PARISC_PLT		0x70000005 
#define SHT_PARISC_SDATA	0x70000006 
#define SHT_PARISC_SBSS		0x70000007 
#define SHT_PARISC_SYMEXTN	0x70000008 
#define SHT_PARISC_STUBS	0x70000009 


#define SHF_PARISC_GLOBAL	0x10000000 
#define SHF_PARISC_SHORT	0x20000000 


#define STT_PARISC_MILLICODE	13	


#define R_PARISC_NONE		0	
#define R_PARISC_DIR32		1	
#define R_PARISC_DIR21L		2	
#define R_PARISC_DIR17R		3	
#define R_PARISC_DIR14R		4	
#define R_PARISC_PCREL21L	5	
#define R_PARISC_PCREL14R	6	
#define R_PARISC_PCREL17C	7	
#define R_PARISC_PCREL17F	8	
#define R_PARISC_DPREL21L	9	
#define R_PARISC_DPREL14R	10	
#define R_PARISC_DPREL14F	11	
#define R_PARISC_DLTREL21L	12	
#define R_PARISC_DLTREL14R	13	
#define R_PARISC_DLTREL14F	14	
#define R_PARISC_DLTIND21L	15	
#define R_PARISC_DLTIND14R	16	
#define R_PARISC_DLTIND14F	17	
#define R_PARISC_PLABEL32	18	



#define EF_ALPHA_32BIT		1	
#define EF_ALPHA_CANRELAX	2	


#define SHT_ALPHA_DEBUG		0x70000001
#define SHT_ALPHA_REGINFO	0x70000002


#define SHF_ALPHA_GPREL		0x10000000

#define STO_ALPHA_NOPV		0x80	
#define STO_ALPHA_STD_GPLOAD	0x88	


#define R_ALPHA_NONE		0	
#define R_ALPHA_REFLONG		1	
#define R_ALPHA_REFQUAD		2	
#define R_ALPHA_GPREL32		3	
#define R_ALPHA_LITERAL		4	
#define R_ALPHA_LITUSE		5	
#define R_ALPHA_GPDISP		6	
#define R_ALPHA_BRADDR		7	
#define R_ALPHA_HINT		8	
#define R_ALPHA_SREL16		9	
#define R_ALPHA_SREL32		10	
#define R_ALPHA_SREL64		11	
#define R_ALPHA_OP_PUSH		12	
#define R_ALPHA_OP_STORE	13	
#define R_ALPHA_OP_PSUB		14	
#define R_ALPHA_OP_PRSHIFT	15	
#define R_ALPHA_GPVALUE		16
#define R_ALPHA_GPRELHIGH	17
#define R_ALPHA_GPRELLOW	18
#define R_ALPHA_IMMED_GP_16	19
#define R_ALPHA_IMMED_GP_HI32	20
#define R_ALPHA_IMMED_SCN_HI32	21
#define R_ALPHA_IMMED_BR_HI32	22
#define R_ALPHA_IMMED_LO32	23
#define R_ALPHA_COPY		24	
#define R_ALPHA_GLOB_DAT	25	
#define R_ALPHA_JMP_SLOT	26	
#define R_ALPHA_RELATIVE	27	
#define R_ALPHA_NUM		28



#define R_PPC_NONE		0
#define R_PPC_ADDR32		1	
#define R_PPC_ADDR24		2	
#define R_PPC_ADDR16		3	
#define R_PPC_ADDR16_LO		4	
#define R_PPC_ADDR16_HI		5	
#define R_PPC_ADDR16_HA		6	
#define R_PPC_ADDR14		7	
#define R_PPC_ADDR14_BRTAKEN	8
#define R_PPC_ADDR14_BRNTAKEN	9
#define R_PPC_REL24		10	
#define R_PPC_REL14		11	
#define R_PPC_REL14_BRTAKEN	12
#define R_PPC_REL14_BRNTAKEN	13
#define R_PPC_GOT16		14
#define R_PPC_GOT16_LO		15
#define R_PPC_GOT16_HI		16
#define R_PPC_GOT16_HA		17
#define R_PPC_PLTREL24		18
#define R_PPC_COPY		19
#define R_PPC_GLOB_DAT		20
#define R_PPC_JMP_SLOT		21
#define R_PPC_RELATIVE		22
#define R_PPC_LOCAL24PC		23
#define R_PPC_UADDR32		24
#define R_PPC_UADDR16		25
#define R_PPC_REL32		26
#define R_PPC_PLT32		27
#define R_PPC_PLTREL32		28
#define R_PPC_PLT16_LO		29
#define R_PPC_PLT16_HI		30
#define R_PPC_PLT16_HA		31
#define R_PPC_SDAREL16		32
#define R_PPC_SECTOFF		33
#define R_PPC_SECTOFF_LO	34
#define R_PPC_SECTOFF_HI	35
#define R_PPC_SECTOFF_HA	36
#define R_PPC_NUMm		37

#define R_PPC_EMB_NADDR32	101
#define R_PPC_EMB_NADDR16	102
#define R_PPC_EMB_NADDR16_LO	103
#define R_PPC_EMB_NADDR16_HI	104
#define R_PPC_EMB_NADDR16_HA	105
#define R_PPC_EMB_SDAI16	106
#define R_PPC_EMB_SDA2I16	107
#define R_PPC_EMB_SDA2REL	108
#define R_PPC_EMB_SDA21		109	
#define R_PPC_EMB_MRKREF	110
#define R_PPC_EMB_RELSEC16	111
#define R_PPC_EMB_RELST_LO	112
#define R_PPC_EMB_RELST_HI	113
#define R_PPC_EMB_RELST_HA	114
#define R_PPC_EMB_BIT_FLD	115
#define R_PPC_EMB_RELSDA	116	

#define R_PPC_DIAB_SDA21_LO	180	
#define R_PPC_DIAB_SDA21_HI	181	
#define R_PPC_DIAB_SDA21_HA	182	
#define R_PPC_DIAB_RELSDA_LO	183	
#define R_PPC_DIAB_RELSDA_HI	184	
#define R_PPC_DIAB_RELSDA_HA	185	

#define R_PPC_TOC16		255



#define EF_ARM_RELEXEC     0x01
#define EF_ARM_HASENTRY    0x02
#define EF_ARM_INTERWORK   0x04
#define EF_ARM_APCS_26     0x08
#define EF_ARM_APCS_FLOAT  0x10
#define EF_ARM_PIC         0x20
#define EF_ALIGN8          0x40		
#define EF_NEW_ABI         0x80
#define EF_OLD_ABI         0x100

#define STT_ARM_TFUNC      0xd

#define SHF_ARM_ENTRYSECT  0x10000000   
#define SHF_ARM_COMDEF     0x80000000   

#define PF_ARM_SB          0x10000000   

#define R_ARM_NONE		0	
#define R_ARM_PC24		1	
#define R_ARM_ABS32		2	
#define R_ARM_REL32		3	
#define R_ARM_PC13		4
#define R_ARM_ABS16		5	
#define R_ARM_ABS12		6	
#define R_ARM_THM_ABS5		7
#define R_ARM_ABS8		8	
#define R_ARM_SBREL32		9
#define R_ARM_THM_PC22		10
#define R_ARM_THM_PC8		11
#define R_ARM_AMP_VCALL9	12
#define R_ARM_SWI24		13
#define R_ARM_THM_SWI8		14
#define R_ARM_XPC25		15
#define R_ARM_THM_XPC22		16
#define R_ARM_COPY		20	
#define R_ARM_GLOB_DAT		21	
#define R_ARM_JUMP_SLOT		22	
#define R_ARM_RELATIVE		23	
#define R_ARM_GOTOFF		24	
#define R_ARM_GOTPC		25	
#define R_ARM_GOT32		26	
#define R_ARM_PLT32		27	
#define R_ARM_GNU_VTENTRY	100
#define R_ARM_GNU_VTINHERIT	101
#define R_ARM_THM_PC11		102	
#define R_ARM_THM_PC9		103	
#define R_ARM_RXPC25		249
#define R_ARM_RSBREL32		250
#define R_ARM_THM_RPC22		251
#define R_ARM_RREL32		252
#define R_ARM_RABS22		253
#define R_ARM_RPC24		254
#define R_ARM_RBASE		255
#define R_ARM_NUM		256


#define R_C60_32       1
#define R_C60_GOT32	3		
#define R_C60_PLT32	4		
#define R_C60_COPY	5		
#define R_C60_GLOB_DAT	6		
#define R_C60_JMP_SLOT	7		
#define R_C60_RELATIVE	8		
#define R_C60_GOTOFF	9		
#define R_C60_GOTPC	10		

#define R_C60HI16      0x55       
#define R_C60LO16      0x54       

#endif	


#ifndef __GNU_STAB__


#define __GNU_STAB__

#define __define_stab(NAME, CODE, STRING) NAME=CODE,

enum __stab_debug_code
{
/* Table of DBX symbol codes for the GNU system.
   Copyright (C) 1988, 1997 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */


__define_stab (N_GSYM, 0x20, "GSYM")

__define_stab (N_FNAME, 0x22, "FNAME")

__define_stab (N_FUN, 0x24, "FUN")

__define_stab (N_STSYM, 0x26, "STSYM")

__define_stab (N_LCSYM, 0x28, "LCSYM")

__define_stab (N_MAIN, 0x2a, "MAIN")

__define_stab (N_PC, 0x30, "PC")

__define_stab (N_NSYMS, 0x32, "NSYMS")

__define_stab (N_NOMAP, 0x34, "NOMAP")

__define_stab (N_OBJ, 0x38, "OBJ")

__define_stab (N_OPT, 0x3c, "OPT")

__define_stab (N_RSYM, 0x40, "RSYM")

__define_stab (N_M2C, 0x42, "M2C")

__define_stab (N_SLINE, 0x44, "SLINE")

__define_stab (N_DSLINE, 0x46, "DSLINE")

__define_stab (N_BSLINE, 0x48, "BSLINE")

__define_stab (N_BROWS, 0x48, "BROWS")

__define_stab(N_DEFD, 0x4a, "DEFD")

__define_stab (N_EHDECL, 0x50, "EHDECL")
__define_stab (N_MOD2, 0x50, "MOD2")

__define_stab (N_CATCH, 0x54, "CATCH")

__define_stab (N_SSYM, 0x60, "SSYM")

__define_stab (N_SO, 0x64, "SO")

__define_stab (N_LSYM, 0x80, "LSYM")

__define_stab (N_BINCL, 0x82, "BINCL")

__define_stab (N_SOL, 0x84, "SOL")

__define_stab (N_PSYM, 0xa0, "PSYM")

__define_stab (N_EINCL, 0xa2, "EINCL")

__define_stab (N_ENTRY, 0xa4, "ENTRY")

__define_stab (N_LBRAC, 0xc0, "LBRAC")

__define_stab (N_EXCL, 0xc2, "EXCL")

__define_stab (N_SCOPE, 0xc4, "SCOPE")

__define_stab (N_RBRAC, 0xe0, "RBRAC")

__define_stab (N_BCOMM, 0xe2, "BCOMM")

__define_stab (N_ECOMM, 0xe4, "ECOMM")

__define_stab (N_ECOML, 0xe8, "ECOML")

__define_stab (N_NBTEXT, 0xF0, "NBTEXT")
__define_stab (N_NBDATA, 0xF2, "NBDATA")
__define_stab (N_NBBSS,  0xF4, "NBBSS")
__define_stab (N_NBSTS,  0xF6, "NBSTS")
__define_stab (N_NBLCS,  0xF8, "NBLCS")

__define_stab (N_LENG, 0xfe, "LENG")

LAST_UNUSED_STAB_CODE
};

#undef __define_stab

#endif 

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef LIBTCC_H
#define LIBTCC_H

#ifdef __cplusplus
extern "C" {
#endif

struct TCCState;

typedef struct TCCState TCCState;

TCCState *tcc_new(void);

void tcc_delete(TCCState *s);

void tcc_enable_debug(TCCState *s);

void tcc_set_error_func(TCCState *s, void *error_opaque,
                        void (*error_func)(void *opaque, const char *msg));

int tcc_set_warning(TCCState *s, const char *warning_name, int value);


int tcc_add_include_path(TCCState *s, const char *pathname);

int tcc_add_sysinclude_path(TCCState *s, const char *pathname);

void tcc_define_symbol(TCCState *s, const char *sym, const char *value);

void tcc_undefine_symbol(TCCState *s, const char *sym);


int tcc_add_file(TCCState *s, const char *filename);

int tcc_compile_string(TCCState *s, const char *buf);


#define TCC_OUTPUT_MEMORY   0 
#define TCC_OUTPUT_EXE      1 
#define TCC_OUTPUT_DLL      2 
#define TCC_OUTPUT_OBJ      3 
int tcc_set_output_type(TCCState *s, int output_type);

#define TCC_OUTPUT_FORMAT_ELF    0 
#define TCC_OUTPUT_FORMAT_BINARY 1 
#define TCC_OUTPUT_FORMAT_COFF   2 

int tcc_add_library_path(TCCState *s, const char *pathname);

int tcc_add_library(TCCState *s, const char *libraryname);

int tcc_add_symbol(TCCState *s, const char *name, unsigned long val);

int tcc_output_file(TCCState *s, const char *filename);

int tcc_run(TCCState *s, int argc, char **argv);

int tcc_relocate(TCCState *s);

int tcc_get_symbol(TCCState *s, unsigned long *pval, const char *name);

#ifdef __cplusplus
}
#endif

#endif





#if !defined(TCC_TARGET_I386) && !defined(TCC_TARGET_ARM) && \
    !defined(TCC_TARGET_C67)
#define TCC_TARGET_I386
#endif

#if !defined(WIN32) && !defined(TCC_UCLIBC) && !defined(TCC_TARGET_ARM) && \
    !defined(TCC_TARGET_C67)
#define CONFIG_TCC_BCHECK 
#endif

#if defined(WIN32) && !defined(TCC_TARGET_PE)
#define CONFIG_TCC_STATIC
#endif

#if !defined(TCC_TARGET_ARM) && !defined(TCC_TARGET_C67)
#define CONFIG_TCC_ASM
#endif

#if defined(TCC_TARGET_C67)
#define TCC_TARGET_COFF
#endif

#define FALSE 0
#define false 0
#define TRUE 1
#define true 1
typedef int BOOL;

#define CONFIG_TCC_CRT_PREFIX "/usr/lib"

#define INCLUDE_STACK_SIZE  32
#define IFDEF_STACK_SIZE    64
#define VSTACK_SIZE         256
#define STRING_MAX_SIZE     1024
#define PACK_STACK_SIZE     8

#define TOK_HASH_SIZE       8192 
#define TOK_ALLOC_INCR      512  
#define TOK_MAX_SIZE        4 

typedef struct TokenSym {
    struct TokenSym *hash_next;
    struct Sym *sym_define; 
    struct Sym *sym_label; 
    struct Sym *sym_struct; 
    struct Sym *sym_identifier; 
    int tok; 
    int len;
    char str[1];
} TokenSym;

typedef struct CString {
    int size; 
    void *data; 
    int size_allocated;
    void *data_allocated; 
} CString;

typedef struct CType {
    int t;
    struct Sym *ref;
} CType;

typedef union CValue {
    long double ld;
    double d;
    float f;
    int i;
    unsigned int ui;
    unsigned int ul; 
    long long ll;
    unsigned long long ull;
    struct CString *cstr;
    void *ptr;
    int tab[sizeof(long double) / sizeof(int)];
} CValue;

typedef struct SValue {
    CType type;      
    unsigned short r;      
    unsigned short r2;     
    CValue c;              
    struct Sym *sym;       
} SValue;

typedef struct Sym {
    long v;    
    long r;    
    long c;    
    CType type;    
    struct Sym *next; 
    struct Sym *prev; 
    struct Sym *prev_tok; 
} Sym;

#define SHF_PRIVATE 0x80000000

typedef struct Section {
    unsigned long data_offset; 
    unsigned char *data;       
    unsigned long data_allocated; 
    int sh_name;             
    int sh_num;              
    int sh_type;             
    int sh_flags;            
    int sh_info;             
    int sh_addralign;        
    int sh_entsize;          
    unsigned long sh_size;   
    unsigned long sh_addr;      
    unsigned long sh_offset;      
    int nb_hashed_syms;      
    struct Section *link;    
    struct Section *reloc;   
    struct Section *hash;     
    struct Section *next;
    char name[1];           
} Section;

typedef struct DLLReference {
    int level;
    char name[1];
} DLLReference;

typedef struct AttributeDef {
    int aligned;
    int packed; 
    Section *section;
    unsigned char func_call; 
    unsigned char dllexport; 
} AttributeDef;

#define SYM_STRUCT     0x40000000 
#define SYM_FIELD      0x20000000 
#define SYM_FIRST_ANOM 0x10000000 

#define FUNC_NEW       1 
#define FUNC_OLD       2 
#define FUNC_ELLIPSIS  3 

#define FUNC_CDECL     0 
#define FUNC_STDCALL   1 
#define FUNC_FASTCALL1 2 
#define FUNC_FASTCALL2 3 
#define FUNC_FASTCALL3 4 

#define MACRO_OBJ      0 
#define MACRO_FUNC     1 

#define LABEL_DEFINED  0 
#define LABEL_FORWARD  1 
#define LABEL_DECLARED 2 

#define TYPE_ABSTRACT  1 
#define TYPE_DIRECT    2 

#define IO_BUF_SIZE 8192

typedef struct BufferedFile {
    uint8_t *buf_ptr;
    uint8_t *buf_end;
    int fd;
    int line_num;    
    int ifndef_macro;  
    int ifndef_macro_saved; 
    int *ifdef_stack_ptr; 
    char inc_type;          
    char inc_filename[512]; 
    char filename[1024];    
    unsigned char buffer[IO_BUF_SIZE + 1]; 
} BufferedFile;

#define CH_EOB   '\\'       
#define CH_EOF   (-1)   

typedef struct ParseState {
    int *macro_ptr;
    int line_num;
    int tok;
    CValue tokc;
} ParseState;

typedef struct TokenString {
    int *str;
    int len;
    int allocated_len;
    int last_line_num;
} TokenString;

typedef struct CachedInclude {
    int ifndef_macro;
    int hash_next; 
    char type; 
    char filename[1]; 
} CachedInclude;

#define CACHED_INCLUDES_HASH_SIZE 512

static struct BufferedFile *file;
static int ch, tok;
static CValue tokc;
static CString tokcstr; 
static int tok_flags;
#define TOK_FLAG_BOL   0x0001 
#define TOK_FLAG_BOF   0x0002 
#define TOK_FLAG_ENDIF 0x0004 

static int *macro_ptr, *macro_ptr_allocated;
static int *unget_saved_macro_ptr;
static int unget_saved_buffer[TOK_MAX_SIZE + 1];
static int unget_buffer_enabled;
static int parse_flags;
#define PARSE_FLAG_PREPROCESS 0x0001 
#define PARSE_FLAG_TOK_NUM    0x0002 
#define PARSE_FLAG_LINEFEED   0x0004 
#define PARSE_FLAG_ASM_COMMENTS 0x0008 
 
static Section *text_section, *data_section, *bss_section; 
static Section *cur_text_section; 
#ifdef CONFIG_TCC_ASM
static Section *last_text_section; 
#endif
static Section *bounds_section; 
static Section *lbounds_section; 
static Section *symtab_section, *strtab_section;

static Section *stab_section, *stabstr_section;

static long rsym, anon_sym, ind, loc;
static int const_wanted; 
static int nocode_wanted; 
static int global_expr;  
static CType func_vt; 
static int func_vc;
static long last_line_num, last_ind, func_ind; 
static int tok_ident;
static TokenSym **table_ident;
static TokenSym *hash_ident[TOK_HASH_SIZE];
static char token_buf[STRING_MAX_SIZE + 1];
static char *funcname;
static Sym *global_stack, *local_stack;
static Sym *define_stack;
static Sym *global_label_stack, *local_label_stack;
#define SYM_POOL_NB (8192 / sizeof(Sym))
static Sym *sym_free_first;

static SValue vstack[VSTACK_SIZE], *vtop;
static CType char_pointer_type, func_old_type, int_type;
static unsigned char isidnum_table[256];

static int do_debug = 0;

static int do_bounds_check = 0;

#if !defined(LIBTCC)
static int do_bench = 0;
#endif
static int total_lines;
static int total_bytes;

static int gnu_ext = 1;

static int tcc_ext = 1;

static int num_callers = 6;
static const char **rt_bound_error_msg;

static struct TCCState *tcc_state;

static const char *tcc_lib_path = CONFIG_TCCDIR;

struct TCCState {
    int output_type;
 
    BufferedFile **include_stack_ptr;
    int *ifdef_stack_ptr;

    
    char **include_paths;
    int nb_include_paths;
    char **sysinclude_paths;
    int nb_sysinclude_paths;
    CachedInclude **cached_includes;
    int nb_cached_includes;

    char **library_paths;
    int nb_library_paths;

    DLLReference **loaded_dlls;
    int nb_loaded_dlls;

    
    Section **sections;
    int nb_sections; 

    
    Section *got;
    Section *plt;
    unsigned long *got_offsets;
    int nb_got_offsets;
    
    int *symtab_to_dynsym;

    
    Section *dynsymtab_section;
    
    Section *dynsym;

    int nostdinc; 
    int nostdlib; 

    int nocommon; 

    
    int static_link;

    
    int rdynamic;

    
    int alacarte_link;

    
    unsigned long text_addr;
    int has_text_addr;
    
    
    int output_format;

    
    int char_is_unsigned;
    int leading_underscore;
    
    
    int warn_write_strings;
    int warn_unsupported;
    int warn_error;
    int warn_none;
    int warn_implicit_function_declaration;

    
    void *error_opaque;
    void (*error_func)(void *opaque, const char *msg);
    int error_set_jmp_enabled;
    jmp_buf error_jmp_buf;
    int nb_errors;

    
    Sym *asm_labels;

    
    BufferedFile *include_stack[INCLUDE_STACK_SIZE];

    
    int ifdef_stack[IFDEF_STACK_SIZE];

    
    int cached_includes_hash[CACHED_INCLUDES_HASH_SIZE];

    
    int pack_stack[PACK_STACK_SIZE];
    int *pack_stack_ptr;
};

#define VT_VALMASK   0x00ff
#define VT_CONST     0x00f0  
#define VT_LLOCAL    0x00f1  
#define VT_LOCAL     0x00f2  
#define VT_CMP       0x00f3  
#define VT_JMP       0x00f4  
#define VT_JMPI      0x00f5  
#define VT_LVAL      0x0100  
#define VT_SYM       0x0200  
#define VT_MUSTCAST  0x0400  
#define VT_MUSTBOUND 0x0800  
#define VT_BOUNDED   0x8000  
#define VT_LVAL_BYTE     0x1000  
#define VT_LVAL_SHORT    0x2000  
#define VT_LVAL_UNSIGNED 0x4000  
#define VT_LVAL_TYPE     (VT_LVAL_BYTE | VT_LVAL_SHORT | VT_LVAL_UNSIGNED)

#define VT_INT        0  
#define VT_BYTE       1  
#define VT_SHORT      2  
#define VT_VOID       3  
#define VT_PTR        4  
#define VT_ENUM       5  
#define VT_FUNC       6  
#define VT_STRUCT     7  
#define VT_FLOAT      8  
#define VT_DOUBLE     9  
#define VT_LDOUBLE   10  
#define VT_BOOL      11  
#define VT_LLONG     12  
#define VT_LONG      13  
#define VT_BTYPE      0x000f 
#define VT_UNSIGNED   0x0010  
#define VT_ARRAY      0x0020  
#define VT_BITFIELD   0x0040  
#define VT_CONSTANT   0x0800  
#define VT_VOLATILE   0x1000  
#define VT_SIGNED     0x2000  

#define VT_EXTERN  0x00000080  
#define VT_STATIC  0x00000100  
#define VT_TYPEDEF 0x00000200  
#define VT_INLINE  0x00000400  

#define VT_STRUCT_SHIFT 16   

#define VT_STORAGE (VT_EXTERN | VT_STATIC | VT_TYPEDEF | VT_INLINE)
#define VT_TYPE    (~(VT_STORAGE))


#define TOK_ULT 0x92
#define TOK_UGE 0x93
#define TOK_EQ  0x94
#define TOK_NE  0x95
#define TOK_ULE 0x96
#define TOK_UGT 0x97
#define TOK_LT  0x9c
#define TOK_GE  0x9d
#define TOK_LE  0x9e
#define TOK_GT  0x9f

#define TOK_LAND  0xa0
#define TOK_LOR   0xa1

#define TOK_DEC   0xa2
#define TOK_MID   0xa3 
#define TOK_INC   0xa4
#define TOK_UDIV  0xb0 
#define TOK_UMOD  0xb1 
#define TOK_PDIV  0xb2 
#define TOK_CINT   0xb3 
#define TOK_CCHAR 0xb4 
#define TOK_STR   0xb5 
#define TOK_TWOSHARPS 0xb6 
#define TOK_LCHAR    0xb7
#define TOK_LSTR     0xb8
#define TOK_CFLOAT   0xb9 
#define TOK_LINENUM  0xba 
#define TOK_CDOUBLE  0xc0 
#define TOK_CLDOUBLE 0xc1 
#define TOK_UMULL    0xc2 
#define TOK_ADDC1    0xc3 
#define TOK_ADDC2    0xc4 
#define TOK_SUBC1    0xc5 
#define TOK_SUBC2    0xc6 
#define TOK_CUINT    0xc8 
#define TOK_CLLONG   0xc9 
#define TOK_CULLONG  0xca 
#define TOK_ARROW    0xcb
#define TOK_DOTS     0xcc 
#define TOK_SHR      0xcd 
#define TOK_PPNUM    0xce 

#define TOK_SHL   0x01 
#define TOK_SAR   0x02 
  
#define TOK_A_MOD 0xa5
#define TOK_A_AND 0xa6
#define TOK_A_MUL 0xaa
#define TOK_A_ADD 0xab
#define TOK_A_SUB 0xad
#define TOK_A_DIV 0xaf
#define TOK_A_XOR 0xde
#define TOK_A_OR  0xfc
#define TOK_A_SHL 0x81
#define TOK_A_SAR 0x82

#ifndef offsetof
#define offsetof(type, field) ((size_t) &((type *)0)->field)
#endif

#ifndef countof
#define countof(tab) (sizeof(tab) / sizeof((tab)[0]))
#endif

static char tok_two_chars[] = "<=\236>=\235!=\225&&\240||\241++\244--\242==\224<<\1>>\2+=\253-=\255*=\252/=\257%=\245&=\246^=\336|=\374->\313..\250##\266";

#define TOK_EOF       (-1)  
#define TOK_LINEFEED  10    

#define TOK_IDENT 256

#define DEF_ASM(x) DEF(TOK_ASM_ ## x, #x)

#define DEF_BWL(x) \
 DEF(TOK_ASM_ ## x ## b, #x "b") \
 DEF(TOK_ASM_ ## x ## w, #x "w") \
 DEF(TOK_ASM_ ## x ## l, #x "l") \
 DEF(TOK_ASM_ ## x, #x)

#define DEF_WL(x) \
 DEF(TOK_ASM_ ## x ## w, #x "w") \
 DEF(TOK_ASM_ ## x ## l, #x "l") \
 DEF(TOK_ASM_ ## x, #x)

#define DEF_FP1(x) \
 DEF(TOK_ASM_ ## f ## x ## s, "f" #x "s") \
 DEF(TOK_ASM_ ## fi ## x ## l, "fi" #x "l") \
 DEF(TOK_ASM_ ## f ## x ## l, "f" #x "l") \
 DEF(TOK_ASM_ ## fi ## x ## s, "fi" #x "s")

#define DEF_FP(x) \
 DEF(TOK_ASM_ ## f ## x, "f" #x ) \
 DEF(TOK_ASM_ ## f ## x ## p, "f" #x "p") \
 DEF_FP1(x)

#define DEF_ASMTEST(x) \
 DEF_ASM(x ## o) \
 DEF_ASM(x ## no) \
 DEF_ASM(x ## b) \
 DEF_ASM(x ## c) \
 DEF_ASM(x ## nae) \
 DEF_ASM(x ## nb) \
 DEF_ASM(x ## nc) \
 DEF_ASM(x ## ae) \
 DEF_ASM(x ## e) \
 DEF_ASM(x ## z) \
 DEF_ASM(x ## ne) \
 DEF_ASM(x ## nz) \
 DEF_ASM(x ## be) \
 DEF_ASM(x ## na) \
 DEF_ASM(x ## nbe) \
 DEF_ASM(x ## a) \
 DEF_ASM(x ## s) \
 DEF_ASM(x ## ns) \
 DEF_ASM(x ## p) \
 DEF_ASM(x ## pe) \
 DEF_ASM(x ## np) \
 DEF_ASM(x ## po) \
 DEF_ASM(x ## l) \
 DEF_ASM(x ## nge) \
 DEF_ASM(x ## nl) \
 DEF_ASM(x ## ge) \
 DEF_ASM(x ## le) \
 DEF_ASM(x ## ng) \
 DEF_ASM(x ## nle) \
 DEF_ASM(x ## g)

#define TOK_ASM_int TOK_INT

enum tcc_token {
    TOK_LAST = TOK_IDENT - 1,
#define DEF(id, str) id,
     DEF(TOK_INT, "int")
     DEF(TOK_VOID, "void")
     DEF(TOK_CHAR, "char")
     DEF(TOK_IF, "if")
     DEF(TOK_ELSE, "else")
     DEF(TOK_WHILE, "while")
     DEF(TOK_BREAK, "break")
     DEF(TOK_RETURN, "return")
     DEF(TOK_FOR, "for")
     DEF(TOK_EXTERN, "extern")
     DEF(TOK_STATIC, "static")
     DEF(TOK_UNSIGNED, "unsigned")
     DEF(TOK_GOTO, "goto")
     DEF(TOK_DO, "do")
     DEF(TOK_CONTINUE, "continue")
     DEF(TOK_SWITCH, "switch")
     DEF(TOK_CASE, "case")

     DEF(TOK_CONST1, "const")
     DEF(TOK_CONST2, "__const") 
     DEF(TOK_CONST3, "__const__") 
     DEF(TOK_VOLATILE1, "volatile")
     DEF(TOK_VOLATILE2, "__volatile") 
     DEF(TOK_VOLATILE3, "__volatile__") 
     DEF(TOK_LONG, "long")
     DEF(TOK_REGISTER, "register")
     DEF(TOK_SIGNED1, "signed")
     DEF(TOK_SIGNED2, "__signed") 
     DEF(TOK_SIGNED3, "__signed__") 
     DEF(TOK_AUTO, "auto")
     DEF(TOK_INLINE1, "inline")
     DEF(TOK_INLINE2, "__inline") 
     DEF(TOK_INLINE3, "__inline__") 
     DEF(TOK_RESTRICT1, "restrict")
     DEF(TOK_RESTRICT2, "__restrict")
     DEF(TOK_RESTRICT3, "__restrict__")
     DEF(TOK_EXTENSION, "__extension__") 
     
     DEF(TOK_FLOAT, "float")
     DEF(TOK_DOUBLE, "double")
     DEF(TOK_BOOL, "_Bool")
     DEF(TOK_SHORT, "short")
     DEF(TOK_STRUCT, "struct")
     DEF(TOK_UNION, "union")
     DEF(TOK_TYPEDEF, "typedef")
     DEF(TOK_DEFAULT, "default")
     DEF(TOK_ENUM, "enum")
     DEF(TOK_SIZEOF, "sizeof")
     DEF(TOK_ATTRIBUTE1, "__attribute")
     DEF(TOK_ATTRIBUTE2, "__attribute__")
     DEF(TOK_ALIGNOF1, "__alignof")
     DEF(TOK_ALIGNOF2, "__alignof__")
     DEF(TOK_TYPEOF1, "typeof")
     DEF(TOK_TYPEOF2, "__typeof")
     DEF(TOK_TYPEOF3, "__typeof__")
     DEF(TOK_LABEL, "__label__")
     DEF(TOK_ASM1, "asm")
     DEF(TOK_ASM2, "__asm")
     DEF(TOK_ASM3, "__asm__")

     DEF(TOK_DEFINE, "define")
     DEF(TOK_INCLUDE, "include")
     DEF(TOK_INCLUDE_NEXT, "include_next")
     DEF(TOK_IFDEF, "ifdef")
     DEF(TOK_IFNDEF, "ifndef")
     DEF(TOK_ELIF, "elif")
     DEF(TOK_ENDIF, "endif")
     DEF(TOK_DEFINED, "defined")
     DEF(TOK_UNDEF, "undef")
     DEF(TOK_ERROR, "error")
     DEF(TOK_WARNING, "warning")
     DEF(TOK_LINE, "line")
     DEF(TOK_PRAGMA, "pragma")
     DEF(TOK___LINE__, "__LINE__")
     DEF(TOK___FILE__, "__FILE__")
     DEF(TOK___DATE__, "__DATE__")
     DEF(TOK___TIME__, "__TIME__")
     DEF(TOK___FUNCTION__, "__FUNCTION__")
     DEF(TOK___VA_ARGS__, "__VA_ARGS__")
     
     DEF(TOK___FUNC__, "__func__")
     
     DEF(TOK_SECTION1, "section")
     DEF(TOK_SECTION2, "__section__")
     DEF(TOK_ALIGNED1, "aligned")
     DEF(TOK_ALIGNED2, "__aligned__")
     DEF(TOK_PACKED1, "packed")
     DEF(TOK_PACKED2, "__packed__")
     DEF(TOK_UNUSED1, "unused")
     DEF(TOK_UNUSED2, "__unused__")
     DEF(TOK_CDECL1, "cdecl")
     DEF(TOK_CDECL2, "__cdecl")
     DEF(TOK_CDECL3, "__cdecl__")
     DEF(TOK_STDCALL1, "stdcall")
     DEF(TOK_STDCALL2, "__stdcall")
     DEF(TOK_STDCALL3, "__stdcall__")
     DEF(TOK_DLLEXPORT, "dllexport")
     DEF(TOK_NORETURN1, "noreturn")
     DEF(TOK_NORETURN2, "__noreturn__")
     DEF(TOK_builtin_types_compatible_p, "__builtin_types_compatible_p")
     DEF(TOK_builtin_constant_p, "__builtin_constant_p")
     DEF(TOK_REGPARM1, "regparm")
     DEF(TOK_REGPARM2, "__regparm__")

     DEF(TOK_pack, "pack")
#if !defined(TCC_TARGET_I386)
     
     DEF(TOK_ASM_push, "push")
     DEF(TOK_ASM_pop, "pop")
#endif

     DEF(TOK_memcpy, "memcpy")
     DEF(TOK_memset, "memset")
     DEF(TOK_alloca, "alloca")
     DEF(TOK___divdi3, "__divdi3")
     DEF(TOK___moddi3, "__moddi3")
     DEF(TOK___udivdi3, "__udivdi3")
     DEF(TOK___umoddi3, "__umoddi3")
#if defined(TCC_TARGET_ARM)
     DEF(TOK___divsi3, "__divsi3")
     DEF(TOK___modsi3, "__modsi3")
     DEF(TOK___udivsi3, "__udivsi3")
     DEF(TOK___umodsi3, "__umodsi3")
     DEF(TOK___sardi3, "__ashrdi3")
     DEF(TOK___shrdi3, "__lshrdi3")
     DEF(TOK___shldi3, "__ashldi3")
     DEF(TOK___slltold, "__slltold")
     DEF(TOK___fixunssfsi, "__fixunssfsi")
     DEF(TOK___fixunsdfsi, "__fixunsdfsi")
     DEF(TOK___fixunsxfsi, "__fixunsxfsi")
     DEF(TOK___fixsfdi, "__fixsfdi")
     DEF(TOK___fixdfdi, "__fixdfdi")
     DEF(TOK___fixxfdi, "__fixxfdi")
#elif defined(TCC_TARGET_C67)
     DEF(TOK__divi, "_divi")
     DEF(TOK__divu, "_divu")
     DEF(TOK__divf, "_divf")
     DEF(TOK__divd, "_divd")
     DEF(TOK__remi, "_remi")
     DEF(TOK__remu, "_remu")
     DEF(TOK___sardi3, "__sardi3")
     DEF(TOK___shrdi3, "__shrdi3")
     DEF(TOK___shldi3, "__shldi3")
#else
     
     DEF(TOK___sardi3, "__sardi3")
     DEF(TOK___shrdi3, "__shrdi3")
     DEF(TOK___shldi3, "__shldi3")
#endif
     DEF(TOK___tcc_int_fpu_control, "__tcc_int_fpu_control")
     DEF(TOK___tcc_fpu_control, "__tcc_fpu_control")
     DEF(TOK___ulltof, "__ulltof")
     DEF(TOK___ulltod, "__ulltod")
     DEF(TOK___ulltold, "__ulltold")
     DEF(TOK___fixunssfdi, "__fixunssfdi")
     DEF(TOK___fixunsdfdi, "__fixunsdfdi")
     DEF(TOK___fixunsxfdi, "__fixunsxfdi")
     DEF(TOK___chkstk, "__chkstk")

#ifdef CONFIG_TCC_BCHECK
     DEF(TOK___bound_ptr_add, "__bound_ptr_add")
     DEF(TOK___bound_ptr_indir1, "__bound_ptr_indir1")
     DEF(TOK___bound_ptr_indir2, "__bound_ptr_indir2")
     DEF(TOK___bound_ptr_indir4, "__bound_ptr_indir4")
     DEF(TOK___bound_ptr_indir8, "__bound_ptr_indir8")
     DEF(TOK___bound_ptr_indir12, "__bound_ptr_indir12")
     DEF(TOK___bound_ptr_indir16, "__bound_ptr_indir16")
     DEF(TOK___bound_local_new, "__bound_local_new")
     DEF(TOK___bound_local_delete, "__bound_local_delete")
     DEF(TOK_malloc, "malloc")
     DEF(TOK_free, "free")
     DEF(TOK_realloc, "realloc")
     DEF(TOK_memalign, "memalign")
     DEF(TOK_calloc, "calloc")
     DEF(TOK_memmove, "memmove")
     DEF(TOK_strlen, "strlen")
     DEF(TOK_strcpy, "strcpy")
#endif


 DEF_ASM(byte)
 DEF_ASM(align)
 DEF_ASM(skip)
 DEF_ASM(space)
 DEF_ASM(string)
 DEF_ASM(asciz)
 DEF_ASM(ascii)
 DEF_ASM(globl)
 DEF_ASM(global)
 DEF_ASM(text)
 DEF_ASM(data)
 DEF_ASM(bss)
 DEF_ASM(previous)
 DEF_ASM(fill)
 DEF_ASM(org)
 DEF_ASM(quad)

#ifdef TCC_TARGET_I386

 DEF_ASM(al)
 DEF_ASM(cl)
 DEF_ASM(dl)
 DEF_ASM(bl)
 DEF_ASM(ah)
 DEF_ASM(ch)
 DEF_ASM(dh)
 DEF_ASM(bh)
 DEF_ASM(ax)
 DEF_ASM(cx)
 DEF_ASM(dx)
 DEF_ASM(bx)
 DEF_ASM(sp)
 DEF_ASM(bp)
 DEF_ASM(si)
 DEF_ASM(di)
 DEF_ASM(eax)
 DEF_ASM(ecx)
 DEF_ASM(edx)
 DEF_ASM(ebx)
 DEF_ASM(esp)
 DEF_ASM(ebp)
 DEF_ASM(esi)
 DEF_ASM(edi)
 DEF_ASM(mm0)
 DEF_ASM(mm1)
 DEF_ASM(mm2)
 DEF_ASM(mm3)
 DEF_ASM(mm4)
 DEF_ASM(mm5)
 DEF_ASM(mm6)
 DEF_ASM(mm7)
 DEF_ASM(xmm0)
 DEF_ASM(xmm1)
 DEF_ASM(xmm2)
 DEF_ASM(xmm3)
 DEF_ASM(xmm4)
 DEF_ASM(xmm5)
 DEF_ASM(xmm6)
 DEF_ASM(xmm7)
 DEF_ASM(cr0)
 DEF_ASM(cr1)
 DEF_ASM(cr2)
 DEF_ASM(cr3)
 DEF_ASM(cr4)
 DEF_ASM(cr5)
 DEF_ASM(cr6)
 DEF_ASM(cr7)
 DEF_ASM(tr0)
 DEF_ASM(tr1)
 DEF_ASM(tr2)
 DEF_ASM(tr3)
 DEF_ASM(tr4)
 DEF_ASM(tr5)
 DEF_ASM(tr6)
 DEF_ASM(tr7)
 DEF_ASM(db0)
 DEF_ASM(db1)
 DEF_ASM(db2)
 DEF_ASM(db3)
 DEF_ASM(db4)
 DEF_ASM(db5)
 DEF_ASM(db6)
 DEF_ASM(db7)
 DEF_ASM(dr0)
 DEF_ASM(dr1)
 DEF_ASM(dr2)
 DEF_ASM(dr3)
 DEF_ASM(dr4)
 DEF_ASM(dr5)
 DEF_ASM(dr6)
 DEF_ASM(dr7)
 DEF_ASM(es)
 DEF_ASM(cs)
 DEF_ASM(ss)
 DEF_ASM(ds)
 DEF_ASM(fs)
 DEF_ASM(gs)
 DEF_ASM(st)

 DEF_BWL(mov)

 
 DEF_BWL(add)
 DEF_BWL(or)
 DEF_BWL(adc)
 DEF_BWL(sbb)
 DEF_BWL(and)
 DEF_BWL(sub)
 DEF_BWL(xor)
 DEF_BWL(cmp)

 
 DEF_BWL(inc)
 DEF_BWL(dec)
 DEF_BWL(not)
 DEF_BWL(neg)
 DEF_BWL(mul)
 DEF_BWL(imul)
 DEF_BWL(div)
 DEF_BWL(idiv)

 DEF_BWL(xchg)
 DEF_BWL(test)

 
 DEF_BWL(rol)
 DEF_BWL(ror)
 DEF_BWL(rcl)
 DEF_BWL(rcr)
 DEF_BWL(shl)
 DEF_BWL(shr)
 DEF_BWL(sar)

 DEF_ASM(shldw)
 DEF_ASM(shldl)
 DEF_ASM(shld)
 DEF_ASM(shrdw)
 DEF_ASM(shrdl)
 DEF_ASM(shrd)

 DEF_ASM(pushw)
 DEF_ASM(pushl)
 DEF_ASM(push)
 DEF_ASM(popw)
 DEF_ASM(popl)
 DEF_ASM(pop)
 DEF_BWL(in)
 DEF_BWL(out)

 DEF_WL(movzb)

 DEF_ASM(movzwl)
 DEF_ASM(movsbw)
 DEF_ASM(movsbl)
 DEF_ASM(movswl)

 DEF_WL(lea) 

 DEF_ASM(les) 
 DEF_ASM(lds) 
 DEF_ASM(lss) 
 DEF_ASM(lfs) 
 DEF_ASM(lgs) 

 DEF_ASM(call)
 DEF_ASM(jmp)
 DEF_ASM(lcall)
 DEF_ASM(ljmp)
 
 DEF_ASMTEST(j)

 DEF_ASMTEST(set)
 DEF_ASMTEST(cmov)

 DEF_WL(bsf)
 DEF_WL(bsr)
 DEF_WL(bt)
 DEF_WL(bts)
 DEF_WL(btr)
 DEF_WL(btc)

 DEF_WL(lsl)

 
 DEF_FP(add)
 DEF_FP(mul)

 DEF_ASM(fcom)
 DEF_ASM(fcom_1) 
 DEF_FP1(com)

 DEF_FP(comp)
 DEF_FP(sub)
 DEF_FP(subr)
 DEF_FP(div)
 DEF_FP(divr)

 DEF_BWL(xadd)
 DEF_BWL(cmpxchg)

 
 DEF_BWL(cmps)
 DEF_BWL(scmp)
 DEF_BWL(ins)
 DEF_BWL(outs)
 DEF_BWL(lods)
 DEF_BWL(slod)
 DEF_BWL(movs)
 DEF_BWL(smov)
 DEF_BWL(scas)
 DEF_BWL(ssca)
 DEF_BWL(stos)
 DEF_BWL(ssto)

 

#define ALT(x)
#define DEF_ASM_OP0(name, opcode) DEF_ASM(name)
#define DEF_ASM_OP0L(name, opcode, group, instr_type)
#define DEF_ASM_OP1(name, opcode, group, instr_type, op0)
#define DEF_ASM_OP2(name, opcode, group, instr_type, op0, op1)
#define DEF_ASM_OP3(name, opcode, group, instr_type, op0, op1, op2)
     DEF_ASM_OP0(pusha, 0x60) 
     DEF_ASM_OP0(popa, 0x61)
     DEF_ASM_OP0(clc, 0xf8)
     DEF_ASM_OP0(cld, 0xfc)
     DEF_ASM_OP0(cli, 0xfa)
     DEF_ASM_OP0(clts, 0x0f06)
     DEF_ASM_OP0(cmc, 0xf5)
     DEF_ASM_OP0(lahf, 0x9f)
     DEF_ASM_OP0(sahf, 0x9e)
     DEF_ASM_OP0(pushfl, 0x9c)
     DEF_ASM_OP0(popfl, 0x9d)
     DEF_ASM_OP0(pushf, 0x9c)
     DEF_ASM_OP0(popf, 0x9d)
     DEF_ASM_OP0(stc, 0xf9)
     DEF_ASM_OP0(std, 0xfd)
     DEF_ASM_OP0(sti, 0xfb)
     DEF_ASM_OP0(aaa, 0x37)
     DEF_ASM_OP0(aas, 0x3f)
     DEF_ASM_OP0(daa, 0x27)
     DEF_ASM_OP0(das, 0x2f)
     DEF_ASM_OP0(aad, 0xd50a)
     DEF_ASM_OP0(aam, 0xd40a)
     DEF_ASM_OP0(cbw, 0x6698)
     DEF_ASM_OP0(cwd, 0x6699)
     DEF_ASM_OP0(cwde, 0x98)
     DEF_ASM_OP0(cdq, 0x99)
     DEF_ASM_OP0(cbtw, 0x6698)
     DEF_ASM_OP0(cwtl, 0x98)
     DEF_ASM_OP0(cwtd, 0x6699)
     DEF_ASM_OP0(cltd, 0x99)
     DEF_ASM_OP0(int3, 0xcc)
     DEF_ASM_OP0(into, 0xce)
     DEF_ASM_OP0(iret, 0xcf)
     DEF_ASM_OP0(rsm, 0x0faa)
     DEF_ASM_OP0(hlt, 0xf4)
     DEF_ASM_OP0(wait, 0x9b)
     DEF_ASM_OP0(nop, 0x90)
     DEF_ASM_OP0(xlat, 0xd7)

     
ALT(DEF_ASM_OP0L(cmpsb, 0xa6, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(scmpb, 0xa6, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(insb, 0x6c, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(outsb, 0x6e, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(lodsb, 0xac, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(slodb, 0xac, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(movsb, 0xa4, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(smovb, 0xa4, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(scasb, 0xae, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(sscab, 0xae, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(stosb, 0xaa, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(sstob, 0xaa, 0, OPC_BWL))

     
     
ALT(DEF_ASM_OP2(bsfw, 0x0fbc, 0, OPC_MODRM | OPC_WL, OPT_REGW | OPT_EA, OPT_REGW))
ALT(DEF_ASM_OP2(bsrw, 0x0fbd, 0, OPC_MODRM | OPC_WL, OPT_REGW | OPT_EA, OPT_REGW))

ALT(DEF_ASM_OP2(btw, 0x0fa3, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP2(btw, 0x0fba, 4, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW | OPT_EA))

ALT(DEF_ASM_OP2(btsw, 0x0fab, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP2(btsw, 0x0fba, 5, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW | OPT_EA))

ALT(DEF_ASM_OP2(btrw, 0x0fb3, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP2(btrw, 0x0fba, 6, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW | OPT_EA))

ALT(DEF_ASM_OP2(btcw, 0x0fbb, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP2(btcw, 0x0fba, 7, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW | OPT_EA))

     
     DEF_ASM_OP0(aword, 0x67)
     DEF_ASM_OP0(addr16, 0x67)
     DEF_ASM_OP0(word, 0x66)
     DEF_ASM_OP0(data16, 0x66)
     DEF_ASM_OP0(lock, 0xf0)
     DEF_ASM_OP0(rep, 0xf3)
     DEF_ASM_OP0(repe, 0xf3)
     DEF_ASM_OP0(repz, 0xf3)
     DEF_ASM_OP0(repne, 0xf2)
     DEF_ASM_OP0(repnz, 0xf2)
             
     DEF_ASM_OP0(invd, 0x0f08)
     DEF_ASM_OP0(wbinvd, 0x0f09)
     DEF_ASM_OP0(cpuid, 0x0fa2)
     DEF_ASM_OP0(wrmsr, 0x0f30)
     DEF_ASM_OP0(rdtsc, 0x0f31)
     DEF_ASM_OP0(rdmsr, 0x0f32)
     DEF_ASM_OP0(rdpmc, 0x0f33)
     DEF_ASM_OP0(ud2, 0x0f0b)

     
ALT(DEF_ASM_OP2(movb, 0xa0, 0, OPC_BWL, OPT_ADDR, OPT_EAX))
ALT(DEF_ASM_OP2(movb, 0xa2, 0, OPC_BWL, OPT_EAX, OPT_ADDR))
ALT(DEF_ASM_OP2(movb, 0x88, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(movb, 0x8a, 0, OPC_MODRM | OPC_BWL, OPT_EA | OPT_REG, OPT_REG))
ALT(DEF_ASM_OP2(movb, 0xb0, 0, OPC_REG | OPC_BWL, OPT_IM, OPT_REG))
ALT(DEF_ASM_OP2(movb, 0xc6, 0, OPC_MODRM | OPC_BWL, OPT_IM, OPT_REG | OPT_EA))

ALT(DEF_ASM_OP2(movw, 0x8c, 0, OPC_MODRM | OPC_WL, OPT_SEG, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(movw, 0x8e, 0, OPC_MODRM | OPC_WL, OPT_EA | OPT_REG, OPT_SEG))

ALT(DEF_ASM_OP2(movw, 0x0f20, 0, OPC_MODRM | OPC_WL, OPT_CR, OPT_REG32))
ALT(DEF_ASM_OP2(movw, 0x0f21, 0, OPC_MODRM | OPC_WL, OPT_DB, OPT_REG32))
ALT(DEF_ASM_OP2(movw, 0x0f24, 0, OPC_MODRM | OPC_WL, OPT_TR, OPT_REG32))
ALT(DEF_ASM_OP2(movw, 0x0f22, 0, OPC_MODRM | OPC_WL, OPT_REG32, OPT_CR))
ALT(DEF_ASM_OP2(movw, 0x0f23, 0, OPC_MODRM | OPC_WL, OPT_REG32, OPT_DB))
ALT(DEF_ASM_OP2(movw, 0x0f26, 0, OPC_MODRM | OPC_WL, OPT_REG32, OPT_TR))

ALT(DEF_ASM_OP2(movsbl, 0x0fbe, 0, OPC_MODRM, OPT_REG8 | OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(movsbw, 0x0fbe, 0, OPC_MODRM | OPC_D16, OPT_REG8 | OPT_EA, OPT_REG16))
ALT(DEF_ASM_OP2(movswl, 0x0fbf, 0, OPC_MODRM, OPT_REG16 | OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(movzbw, 0x0fb6, 0, OPC_MODRM | OPC_WL, OPT_REG8 | OPT_EA, OPT_REGW))
ALT(DEF_ASM_OP2(movzwl, 0x0fb7, 0, OPC_MODRM, OPT_REG16 | OPT_EA, OPT_REG32))

ALT(DEF_ASM_OP1(pushw, 0x50, 0, OPC_REG | OPC_WL, OPT_REGW))
ALT(DEF_ASM_OP1(pushw, 0xff, 6, OPC_MODRM | OPC_WL, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP1(pushw, 0x6a, 0, OPC_WL, OPT_IM8S))
ALT(DEF_ASM_OP1(pushw, 0x68, 0, OPC_WL, OPT_IM32))
ALT(DEF_ASM_OP1(pushw, 0x06, 0, OPC_WL, OPT_SEG))

ALT(DEF_ASM_OP1(popw, 0x58, 0, OPC_REG | OPC_WL, OPT_REGW))
ALT(DEF_ASM_OP1(popw, 0x8f, 0, OPC_MODRM | OPC_WL, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP1(popw, 0x07, 0, OPC_WL, OPT_SEG))

ALT(DEF_ASM_OP2(xchgw, 0x90, 0, OPC_REG | OPC_WL, OPT_REG, OPT_EAX))
ALT(DEF_ASM_OP2(xchgw, 0x90, 0, OPC_REG | OPC_WL, OPT_EAX, OPT_REG))
ALT(DEF_ASM_OP2(xchgb, 0x86, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(xchgb, 0x86, 0, OPC_MODRM | OPC_BWL, OPT_EA | OPT_REG, OPT_REG))

ALT(DEF_ASM_OP2(inb, 0xe4, 0, OPC_BWL, OPT_IM8, OPT_EAX))
ALT(DEF_ASM_OP1(inb, 0xe4, 0, OPC_BWL, OPT_IM8))
ALT(DEF_ASM_OP2(inb, 0xec, 0, OPC_BWL, OPT_DX, OPT_EAX))
ALT(DEF_ASM_OP1(inb, 0xec, 0, OPC_BWL, OPT_DX))

ALT(DEF_ASM_OP2(outb, 0xe6, 0, OPC_BWL, OPT_EAX, OPT_IM8))
ALT(DEF_ASM_OP1(outb, 0xe6, 0, OPC_BWL, OPT_IM8))
ALT(DEF_ASM_OP2(outb, 0xee, 0, OPC_BWL, OPT_EAX, OPT_DX))
ALT(DEF_ASM_OP1(outb, 0xee, 0, OPC_BWL, OPT_DX))

ALT(DEF_ASM_OP2(leaw, 0x8d, 0, OPC_MODRM | OPC_WL, OPT_EA, OPT_REG))

ALT(DEF_ASM_OP2(les, 0xc4, 0, OPC_MODRM, OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(lds, 0xc5, 0, OPC_MODRM, OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(lss, 0x0fb2, 0, OPC_MODRM, OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(lfs, 0x0fb4, 0, OPC_MODRM, OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(lgs, 0x0fb5, 0, OPC_MODRM, OPT_EA, OPT_REG32))

     
ALT(DEF_ASM_OP2(addb, 0x00, 0, OPC_ARITH | OPC_MODRM | OPC_BWL, OPT_REG, OPT_EA | OPT_REG)) 
ALT(DEF_ASM_OP2(addb, 0x02, 0, OPC_ARITH | OPC_MODRM | OPC_BWL, OPT_EA | OPT_REG, OPT_REG))
ALT(DEF_ASM_OP2(addb, 0x04, 0, OPC_ARITH | OPC_BWL, OPT_IM, OPT_EAX))
ALT(DEF_ASM_OP2(addb, 0x80, 0, OPC_ARITH | OPC_MODRM | OPC_BWL, OPT_IM, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(addw, 0x83, 0, OPC_ARITH | OPC_MODRM | OPC_WL, OPT_IM8S, OPT_EA | OPT_REG))

ALT(DEF_ASM_OP2(testb, 0x84, 0, OPC_MODRM | OPC_BWL, OPT_EA | OPT_REG, OPT_REG))
ALT(DEF_ASM_OP2(testb, 0x84, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(testb, 0xa8, 0, OPC_BWL, OPT_IM, OPT_EAX))
ALT(DEF_ASM_OP2(testb, 0xf6, 0, OPC_MODRM | OPC_BWL, OPT_IM, OPT_EA | OPT_REG))

ALT(DEF_ASM_OP1(incw, 0x40, 0, OPC_REG | OPC_WL, OPT_REGW))
ALT(DEF_ASM_OP1(incb, 0xfe, 0, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP1(decw, 0x48, 0, OPC_REG | OPC_WL, OPT_REGW))
ALT(DEF_ASM_OP1(decb, 0xfe, 1, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))

ALT(DEF_ASM_OP1(notb, 0xf6, 2, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP1(negb, 0xf6, 3, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))

ALT(DEF_ASM_OP1(mulb, 0xf6, 4, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP1(imulb, 0xf6, 5, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))

ALT(DEF_ASM_OP2(imulw, 0x0faf, 0, OPC_MODRM | OPC_WL, OPT_REG | OPT_EA, OPT_REG))
ALT(DEF_ASM_OP3(imulw, 0x6b, 0, OPC_MODRM | OPC_WL, OPT_IM8S, OPT_REGW | OPT_EA, OPT_REGW))
ALT(DEF_ASM_OP2(imulw, 0x6b, 0, OPC_MODRM | OPC_WL, OPT_IM8S, OPT_REGW))
ALT(DEF_ASM_OP3(imulw, 0x69, 0, OPC_MODRM | OPC_WL, OPT_IMW, OPT_REGW | OPT_EA, OPT_REGW))
ALT(DEF_ASM_OP2(imulw, 0x69, 0, OPC_MODRM | OPC_WL, OPT_IMW, OPT_REGW))

ALT(DEF_ASM_OP1(divb, 0xf6, 6, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP2(divb, 0xf6, 6, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA, OPT_EAX))
ALT(DEF_ASM_OP1(idivb, 0xf6, 7, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP2(idivb, 0xf6, 7, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA, OPT_EAX))

     
ALT(DEF_ASM_OP2(rolb, 0xc0, 0, OPC_MODRM | OPC_BWL | OPC_SHIFT, OPT_IM8, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(rolb, 0xd2, 0, OPC_MODRM | OPC_BWL | OPC_SHIFT, OPT_CL, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP1(rolb, 0xd0, 0, OPC_MODRM | OPC_BWL | OPC_SHIFT, OPT_EA | OPT_REG))

ALT(DEF_ASM_OP3(shldw, 0x0fa4, 0, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP3(shldw, 0x0fa5, 0, OPC_MODRM | OPC_WL, OPT_CL, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP2(shldw, 0x0fa5, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP3(shrdw, 0x0fac, 0, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP3(shrdw, 0x0fad, 0, OPC_MODRM | OPC_WL, OPT_CL, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP2(shrdw, 0x0fad, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_EA | OPT_REGW))

ALT(DEF_ASM_OP1(call, 0xff, 2, OPC_MODRM, OPT_INDIR))
ALT(DEF_ASM_OP1(call, 0xe8, 0, OPC_JMP, OPT_ADDR))
ALT(DEF_ASM_OP1(jmp, 0xff, 4, OPC_MODRM, OPT_INDIR))
ALT(DEF_ASM_OP1(jmp, 0xeb, 0, OPC_SHORTJMP | OPC_JMP, OPT_ADDR))

ALT(DEF_ASM_OP2(lcall, 0x9a, 0, 0, OPT_IM16, OPT_IM32))
ALT(DEF_ASM_OP1(lcall, 0xff, 3, 0, OPT_EA))
ALT(DEF_ASM_OP2(ljmp, 0xea, 0, 0, OPT_IM16, OPT_IM32))
ALT(DEF_ASM_OP1(ljmp, 0xff, 5, 0, OPT_EA))

ALT(DEF_ASM_OP1(int, 0xcd, 0, 0, OPT_IM8))
ALT(DEF_ASM_OP1(seto, 0x0f90, 0, OPC_MODRM | OPC_TEST, OPT_REG8 | OPT_EA))
    DEF_ASM_OP2(enter, 0xc8, 0, 0, OPT_IM16, OPT_IM8)
    DEF_ASM_OP0(leave, 0xc9)
    DEF_ASM_OP0(ret, 0xc3)
ALT(DEF_ASM_OP1(ret, 0xc2, 0, 0, OPT_IM16))
    DEF_ASM_OP0(lret, 0xcb)
ALT(DEF_ASM_OP1(lret, 0xca, 0, 0, OPT_IM16))

ALT(DEF_ASM_OP1(jo, 0x70, 0, OPC_SHORTJMP | OPC_JMP | OPC_TEST, OPT_ADDR))
    DEF_ASM_OP1(loopne, 0xe0, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(loopnz, 0xe0, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(loope, 0xe1, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(loopz, 0xe1, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(loop, 0xe2, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(jecxz, 0xe3, 0, OPC_SHORTJMP, OPT_ADDR)
     
     
     
ALT(DEF_ASM_OP0L(fcomp, 0xd8d9, 0, 0))

ALT(DEF_ASM_OP1(fadd, 0xd8c0, 0, OPC_FARITH | OPC_REG, OPT_ST))
ALT(DEF_ASM_OP2(fadd, 0xd8c0, 0, OPC_FARITH | OPC_REG, OPT_ST, OPT_ST0))
ALT(DEF_ASM_OP0L(fadd, 0xdec1, 0, OPC_FARITH))
ALT(DEF_ASM_OP1(faddp, 0xdec0, 0, OPC_FARITH | OPC_REG, OPT_ST))
ALT(DEF_ASM_OP2(faddp, 0xdec0, 0, OPC_FARITH | OPC_REG, OPT_ST, OPT_ST0))
ALT(DEF_ASM_OP2(faddp, 0xdec0, 0, OPC_FARITH | OPC_REG, OPT_ST0, OPT_ST))
ALT(DEF_ASM_OP0L(faddp, 0xdec1, 0, OPC_FARITH))
ALT(DEF_ASM_OP1(fadds, 0xd8, 0, OPC_FARITH | OPC_MODRM, OPT_EA))
ALT(DEF_ASM_OP1(fiaddl, 0xda, 0, OPC_FARITH | OPC_MODRM, OPT_EA))
ALT(DEF_ASM_OP1(faddl, 0xdc, 0, OPC_FARITH | OPC_MODRM, OPT_EA))
ALT(DEF_ASM_OP1(fiadds, 0xde, 0, OPC_FARITH | OPC_MODRM, OPT_EA))

     DEF_ASM_OP0(fucompp, 0xdae9)
     DEF_ASM_OP0(ftst, 0xd9e4)
     DEF_ASM_OP0(fxam, 0xd9e5)
     DEF_ASM_OP0(fld1, 0xd9e8)
     DEF_ASM_OP0(fldl2t, 0xd9e9)
     DEF_ASM_OP0(fldl2e, 0xd9ea)
     DEF_ASM_OP0(fldpi, 0xd9eb)
     DEF_ASM_OP0(fldlg2, 0xd9ec)
     DEF_ASM_OP0(fldln2, 0xd9ed)
     DEF_ASM_OP0(fldz, 0xd9ee)

     DEF_ASM_OP0(f2xm1, 0xd9f0)
     DEF_ASM_OP0(fyl2x, 0xd9f1)
     DEF_ASM_OP0(fptan, 0xd9f2)
     DEF_ASM_OP0(fpatan, 0xd9f3)
     DEF_ASM_OP0(fxtract, 0xd9f4)
     DEF_ASM_OP0(fprem1, 0xd9f5)
     DEF_ASM_OP0(fdecstp, 0xd9f6)
     DEF_ASM_OP0(fincstp, 0xd9f7)
     DEF_ASM_OP0(fprem, 0xd9f8)
     DEF_ASM_OP0(fyl2xp1, 0xd9f9)
     DEF_ASM_OP0(fsqrt, 0xd9fa)
     DEF_ASM_OP0(fsincos, 0xd9fb)
     DEF_ASM_OP0(frndint, 0xd9fc)
     DEF_ASM_OP0(fscale, 0xd9fd)
     DEF_ASM_OP0(fsin, 0xd9fe)
     DEF_ASM_OP0(fcos, 0xd9ff)
     DEF_ASM_OP0(fchs, 0xd9e0)
     DEF_ASM_OP0(fabs, 0xd9e1)
     DEF_ASM_OP0(fninit, 0xdbe3)
     DEF_ASM_OP0(fnclex, 0xdbe2)
     DEF_ASM_OP0(fnop, 0xd9d0)
     DEF_ASM_OP0(fwait, 0x9b)

    
    DEF_ASM_OP1(fld, 0xd9c0, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(fldl, 0xd9c0, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(flds, 0xd9, 0, OPC_MODRM, OPT_EA)
ALT(DEF_ASM_OP1(fldl, 0xdd, 0, OPC_MODRM, OPT_EA))
    DEF_ASM_OP1(fildl, 0xdb, 0, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fildq, 0xdf, 5, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fildll, 0xdf, 5, OPC_MODRM,OPT_EA)
    DEF_ASM_OP1(fldt, 0xdb, 5, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fbld, 0xdf, 4, OPC_MODRM, OPT_EA)
    
    
    DEF_ASM_OP1(fst, 0xddd0, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(fstl, 0xddd0, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(fsts, 0xd9, 2, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fstps, 0xd9, 3, OPC_MODRM, OPT_EA)
ALT(DEF_ASM_OP1(fstl, 0xdd, 2, OPC_MODRM, OPT_EA))
    DEF_ASM_OP1(fstpl, 0xdd, 3, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fist, 0xdf, 2, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fistp, 0xdf, 3, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fistl, 0xdb, 2, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fistpl, 0xdb, 3, OPC_MODRM, OPT_EA)

    DEF_ASM_OP1(fstp, 0xddd8, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(fistpq, 0xdf, 7, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fistpll, 0xdf, 7, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fstpt, 0xdb, 7, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fbstp, 0xdf, 6, OPC_MODRM, OPT_EA)

    
    DEF_ASM_OP0(fxch, 0xd9c9)
ALT(DEF_ASM_OP1(fxch, 0xd9c8, 0, OPC_REG, OPT_ST))

    
    DEF_ASM_OP1(fucom, 0xdde0, 0, OPC_REG, OPT_ST )
    DEF_ASM_OP1(fucomp, 0xdde8, 0, OPC_REG, OPT_ST )

    DEF_ASM_OP0L(finit, 0xdbe3, 0, OPC_FWAIT)
    DEF_ASM_OP1(fldcw, 0xd9, 5, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fnstcw, 0xd9, 7, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fstcw, 0xd9, 7, OPC_MODRM | OPC_FWAIT, OPT_EA )
    DEF_ASM_OP0(fnstsw, 0xdfe0)
ALT(DEF_ASM_OP1(fnstsw, 0xdfe0, 0, 0, OPT_EAX ))
ALT(DEF_ASM_OP1(fnstsw, 0xdd, 7, OPC_MODRM, OPT_EA ))
    DEF_ASM_OP1(fstsw, 0xdfe0, 0, OPC_FWAIT, OPT_EAX )
ALT(DEF_ASM_OP0L(fstsw, 0xdfe0, 0, OPC_FWAIT))
ALT(DEF_ASM_OP1(fstsw, 0xdd, 7, OPC_MODRM | OPC_FWAIT, OPT_EA ))
    DEF_ASM_OP0L(fclex, 0xdbe2, 0, OPC_FWAIT)
    DEF_ASM_OP1(fnstenv, 0xd9, 6, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fstenv, 0xd9, 6, OPC_MODRM | OPC_FWAIT, OPT_EA )
    DEF_ASM_OP1(fldenv, 0xd9, 4, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fnsave, 0xdd, 6, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fsave, 0xdd, 6, OPC_MODRM | OPC_FWAIT, OPT_EA )
    DEF_ASM_OP1(frstor, 0xdd, 4, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(ffree, 0xddc0, 4, OPC_REG, OPT_ST )
    DEF_ASM_OP1(ffreep, 0xdfc0, 4, OPC_REG, OPT_ST )
    DEF_ASM_OP1(fxsave, 0x0fae, 0, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fxrstor, 0x0fae, 1, OPC_MODRM, OPT_EA )

    
    DEF_ASM_OP2(arpl, 0x63, 0, OPC_MODRM, OPT_REG16, OPT_REG16 | OPT_EA)
    DEF_ASM_OP2(lar, 0x0f02, 0, OPC_MODRM, OPT_REG32 | OPT_EA, OPT_REG32)
    DEF_ASM_OP1(lgdt, 0x0f01, 2, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(lidt, 0x0f01, 3, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(lldt, 0x0f00, 2, OPC_MODRM, OPT_EA | OPT_REG)
    DEF_ASM_OP1(lmsw, 0x0f01, 6, OPC_MODRM, OPT_EA | OPT_REG)
ALT(DEF_ASM_OP2(lslw, 0x0f03, 0, OPC_MODRM | OPC_WL, OPT_EA | OPT_REG, OPT_REG))
    DEF_ASM_OP1(ltr, 0x0f00, 3, OPC_MODRM, OPT_EA | OPT_REG)
    DEF_ASM_OP1(sgdt, 0x0f01, 0, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(sidt, 0x0f01, 1, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(sldt, 0x0f00, 0, OPC_MODRM, OPT_REG | OPT_EA)
    DEF_ASM_OP1(smsw, 0x0f01, 4, OPC_MODRM, OPT_REG | OPT_EA)
    DEF_ASM_OP1(str, 0x0f00, 1, OPC_MODRM, OPT_REG16| OPT_EA)
    DEF_ASM_OP1(verr, 0x0f00, 4, OPC_MODRM, OPT_REG | OPT_EA)
    DEF_ASM_OP1(verw, 0x0f00, 5, OPC_MODRM, OPT_REG | OPT_EA)

    
    DEF_ASM_OP1(bswap, 0x0fc8, 0, OPC_REG, OPT_REG32 )
ALT(DEF_ASM_OP2(xaddb, 0x0fc0, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_REG | OPT_EA ))
ALT(DEF_ASM_OP2(cmpxchgb, 0x0fb0, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_REG | OPT_EA ))
    DEF_ASM_OP1(invlpg, 0x0f01, 7, OPC_MODRM, OPT_EA )

    DEF_ASM_OP2(boundl, 0x62, 0, OPC_MODRM, OPT_REG32, OPT_EA)
    DEF_ASM_OP2(boundw, 0x62, 0, OPC_MODRM | OPC_D16, OPT_REG16, OPT_EA)

    
    DEF_ASM_OP1(cmpxchg8b, 0x0fc7, 1, OPC_MODRM, OPT_EA )
    
    
    ALT(DEF_ASM_OP2(cmovo, 0x0f40, 0, OPC_MODRM | OPC_TEST, OPT_REG32 | OPT_EA, OPT_REG32))

    DEF_ASM_OP2(fcmovb, 0xdac0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmove, 0xdac8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovbe, 0xdad0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovu, 0xdad8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovnb, 0xdbc0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovne, 0xdbc8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovnbe, 0xdbd0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovnu, 0xdbd8, 0, OPC_REG, OPT_ST, OPT_ST0 )

    DEF_ASM_OP2(fucomi, 0xdbe8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcomi, 0xdbf0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fucomip, 0xdfe8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcomip, 0xdff0, 0, OPC_REG, OPT_ST, OPT_ST0 )

    
    DEF_ASM_OP0(emms, 0x0f77) 
    DEF_ASM_OP2(movd, 0x0f6e, 0, OPC_MODRM, OPT_EA | OPT_REG32, OPT_MMX )
ALT(DEF_ASM_OP2(movd, 0x0f7e, 0, OPC_MODRM, OPT_MMX, OPT_EA | OPT_REG32 ))
    DEF_ASM_OP2(movq, 0x0f6f, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(movq, 0x0f7f, 0, OPC_MODRM, OPT_MMX, OPT_EA | OPT_MMX ))
    DEF_ASM_OP2(packssdw, 0x0f6b, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(packsswb, 0x0f63, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(packuswb, 0x0f67, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddb, 0x0ffc, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddw, 0x0ffd, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddd, 0x0ffe, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddsb, 0x0fec, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddsw, 0x0fed, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddusb, 0x0fdc, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddusw, 0x0fdd, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pand, 0x0fdb, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pandn, 0x0fdf, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpeqb, 0x0f74, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpeqw, 0x0f75, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpeqd, 0x0f76, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpgtb, 0x0f64, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpgtw, 0x0f65, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpgtd, 0x0f66, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pmaddwd, 0x0ff5, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pmulhw, 0x0fe5, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pmullw, 0x0fd5, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(por, 0x0feb, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psllw, 0x0ff1, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psllw, 0x0f71, 6, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(pslld, 0x0ff2, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(pslld, 0x0f72, 6, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psllq, 0x0ff3, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psllq, 0x0f73, 6, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psraw, 0x0fe1, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psraw, 0x0f71, 4, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psrad, 0x0fe2, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psrad, 0x0f72, 4, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psrlw, 0x0fd1, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psrlw, 0x0f71, 2, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psrld, 0x0fd2, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psrld, 0x0f72, 2, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psrlq, 0x0fd3, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psrlq, 0x0f73, 2, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psubb, 0x0ff8, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubw, 0x0ff9, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubd, 0x0ffa, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubsb, 0x0fe8, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubsw, 0x0fe9, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubusb, 0x0fd8, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubusw, 0x0fd9, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpckhbw, 0x0f68, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpckhwd, 0x0f69, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpckhdq, 0x0f6a, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpcklbw, 0x0f60, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpcklwd, 0x0f61, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpckldq, 0x0f62, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pxor, 0x0fef, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )

#undef ALT
#undef DEF_ASM_OP0
#undef DEF_ASM_OP0L
#undef DEF_ASM_OP1
#undef DEF_ASM_OP2
#undef DEF_ASM_OP3

#define ALT(x)
#define DEF_ASM_OP0(name, opcode)
#define DEF_ASM_OP0L(name, opcode, group, instr_type) DEF_ASM(name)
#define DEF_ASM_OP1(name, opcode, group, instr_type, op0) DEF_ASM(name)
#define DEF_ASM_OP2(name, opcode, group, instr_type, op0, op1) DEF_ASM(name)
#define DEF_ASM_OP3(name, opcode, group, instr_type, op0, op1, op2) DEF_ASM(name)
     DEF_ASM_OP0(pusha, 0x60) 
     DEF_ASM_OP0(popa, 0x61)
     DEF_ASM_OP0(clc, 0xf8)
     DEF_ASM_OP0(cld, 0xfc)
     DEF_ASM_OP0(cli, 0xfa)
     DEF_ASM_OP0(clts, 0x0f06)
     DEF_ASM_OP0(cmc, 0xf5)
     DEF_ASM_OP0(lahf, 0x9f)
     DEF_ASM_OP0(sahf, 0x9e)
     DEF_ASM_OP0(pushfl, 0x9c)
     DEF_ASM_OP0(popfl, 0x9d)
     DEF_ASM_OP0(pushf, 0x9c)
     DEF_ASM_OP0(popf, 0x9d)
     DEF_ASM_OP0(stc, 0xf9)
     DEF_ASM_OP0(std, 0xfd)
     DEF_ASM_OP0(sti, 0xfb)
     DEF_ASM_OP0(aaa, 0x37)
     DEF_ASM_OP0(aas, 0x3f)
     DEF_ASM_OP0(daa, 0x27)
     DEF_ASM_OP0(das, 0x2f)
     DEF_ASM_OP0(aad, 0xd50a)
     DEF_ASM_OP0(aam, 0xd40a)
     DEF_ASM_OP0(cbw, 0x6698)
     DEF_ASM_OP0(cwd, 0x6699)
     DEF_ASM_OP0(cwde, 0x98)
     DEF_ASM_OP0(cdq, 0x99)
     DEF_ASM_OP0(cbtw, 0x6698)
     DEF_ASM_OP0(cwtl, 0x98)
     DEF_ASM_OP0(cwtd, 0x6699)
     DEF_ASM_OP0(cltd, 0x99)
     DEF_ASM_OP0(int3, 0xcc)
     DEF_ASM_OP0(into, 0xce)
     DEF_ASM_OP0(iret, 0xcf)
     DEF_ASM_OP0(rsm, 0x0faa)
     DEF_ASM_OP0(hlt, 0xf4)
     DEF_ASM_OP0(wait, 0x9b)
     DEF_ASM_OP0(nop, 0x90)
     DEF_ASM_OP0(xlat, 0xd7)

     
ALT(DEF_ASM_OP0L(cmpsb, 0xa6, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(scmpb, 0xa6, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(insb, 0x6c, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(outsb, 0x6e, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(lodsb, 0xac, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(slodb, 0xac, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(movsb, 0xa4, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(smovb, 0xa4, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(scasb, 0xae, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(sscab, 0xae, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(stosb, 0xaa, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(sstob, 0xaa, 0, OPC_BWL))

     
     
ALT(DEF_ASM_OP2(bsfw, 0x0fbc, 0, OPC_MODRM | OPC_WL, OPT_REGW | OPT_EA, OPT_REGW))
ALT(DEF_ASM_OP2(bsrw, 0x0fbd, 0, OPC_MODRM | OPC_WL, OPT_REGW | OPT_EA, OPT_REGW))

ALT(DEF_ASM_OP2(btw, 0x0fa3, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP2(btw, 0x0fba, 4, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW | OPT_EA))

ALT(DEF_ASM_OP2(btsw, 0x0fab, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP2(btsw, 0x0fba, 5, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW | OPT_EA))

ALT(DEF_ASM_OP2(btrw, 0x0fb3, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP2(btrw, 0x0fba, 6, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW | OPT_EA))

ALT(DEF_ASM_OP2(btcw, 0x0fbb, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP2(btcw, 0x0fba, 7, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW | OPT_EA))

     
     DEF_ASM_OP0(aword, 0x67)
     DEF_ASM_OP0(addr16, 0x67)
     DEF_ASM_OP0(word, 0x66)
     DEF_ASM_OP0(data16, 0x66)
     DEF_ASM_OP0(lock, 0xf0)
     DEF_ASM_OP0(rep, 0xf3)
     DEF_ASM_OP0(repe, 0xf3)
     DEF_ASM_OP0(repz, 0xf3)
     DEF_ASM_OP0(repne, 0xf2)
     DEF_ASM_OP0(repnz, 0xf2)
             
     DEF_ASM_OP0(invd, 0x0f08)
     DEF_ASM_OP0(wbinvd, 0x0f09)
     DEF_ASM_OP0(cpuid, 0x0fa2)
     DEF_ASM_OP0(wrmsr, 0x0f30)
     DEF_ASM_OP0(rdtsc, 0x0f31)
     DEF_ASM_OP0(rdmsr, 0x0f32)
     DEF_ASM_OP0(rdpmc, 0x0f33)
     DEF_ASM_OP0(ud2, 0x0f0b)

     
ALT(DEF_ASM_OP2(movb, 0xa0, 0, OPC_BWL, OPT_ADDR, OPT_EAX))
ALT(DEF_ASM_OP2(movb, 0xa2, 0, OPC_BWL, OPT_EAX, OPT_ADDR))
ALT(DEF_ASM_OP2(movb, 0x88, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(movb, 0x8a, 0, OPC_MODRM | OPC_BWL, OPT_EA | OPT_REG, OPT_REG))
ALT(DEF_ASM_OP2(movb, 0xb0, 0, OPC_REG | OPC_BWL, OPT_IM, OPT_REG))
ALT(DEF_ASM_OP2(movb, 0xc6, 0, OPC_MODRM | OPC_BWL, OPT_IM, OPT_REG | OPT_EA))

ALT(DEF_ASM_OP2(movw, 0x8c, 0, OPC_MODRM | OPC_WL, OPT_SEG, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(movw, 0x8e, 0, OPC_MODRM | OPC_WL, OPT_EA | OPT_REG, OPT_SEG))

ALT(DEF_ASM_OP2(movw, 0x0f20, 0, OPC_MODRM | OPC_WL, OPT_CR, OPT_REG32))
ALT(DEF_ASM_OP2(movw, 0x0f21, 0, OPC_MODRM | OPC_WL, OPT_DB, OPT_REG32))
ALT(DEF_ASM_OP2(movw, 0x0f24, 0, OPC_MODRM | OPC_WL, OPT_TR, OPT_REG32))
ALT(DEF_ASM_OP2(movw, 0x0f22, 0, OPC_MODRM | OPC_WL, OPT_REG32, OPT_CR))
ALT(DEF_ASM_OP2(movw, 0x0f23, 0, OPC_MODRM | OPC_WL, OPT_REG32, OPT_DB))
ALT(DEF_ASM_OP2(movw, 0x0f26, 0, OPC_MODRM | OPC_WL, OPT_REG32, OPT_TR))

ALT(DEF_ASM_OP2(movsbl, 0x0fbe, 0, OPC_MODRM, OPT_REG8 | OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(movsbw, 0x0fbe, 0, OPC_MODRM | OPC_D16, OPT_REG8 | OPT_EA, OPT_REG16))
ALT(DEF_ASM_OP2(movswl, 0x0fbf, 0, OPC_MODRM, OPT_REG16 | OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(movzbw, 0x0fb6, 0, OPC_MODRM | OPC_WL, OPT_REG8 | OPT_EA, OPT_REGW))
ALT(DEF_ASM_OP2(movzwl, 0x0fb7, 0, OPC_MODRM, OPT_REG16 | OPT_EA, OPT_REG32))

ALT(DEF_ASM_OP1(pushw, 0x50, 0, OPC_REG | OPC_WL, OPT_REGW))
ALT(DEF_ASM_OP1(pushw, 0xff, 6, OPC_MODRM | OPC_WL, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP1(pushw, 0x6a, 0, OPC_WL, OPT_IM8S))
ALT(DEF_ASM_OP1(pushw, 0x68, 0, OPC_WL, OPT_IM32))
ALT(DEF_ASM_OP1(pushw, 0x06, 0, OPC_WL, OPT_SEG))

ALT(DEF_ASM_OP1(popw, 0x58, 0, OPC_REG | OPC_WL, OPT_REGW))
ALT(DEF_ASM_OP1(popw, 0x8f, 0, OPC_MODRM | OPC_WL, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP1(popw, 0x07, 0, OPC_WL, OPT_SEG))

ALT(DEF_ASM_OP2(xchgw, 0x90, 0, OPC_REG | OPC_WL, OPT_REG, OPT_EAX))
ALT(DEF_ASM_OP2(xchgw, 0x90, 0, OPC_REG | OPC_WL, OPT_EAX, OPT_REG))
ALT(DEF_ASM_OP2(xchgb, 0x86, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(xchgb, 0x86, 0, OPC_MODRM | OPC_BWL, OPT_EA | OPT_REG, OPT_REG))

ALT(DEF_ASM_OP2(inb, 0xe4, 0, OPC_BWL, OPT_IM8, OPT_EAX))
ALT(DEF_ASM_OP1(inb, 0xe4, 0, OPC_BWL, OPT_IM8))
ALT(DEF_ASM_OP2(inb, 0xec, 0, OPC_BWL, OPT_DX, OPT_EAX))
ALT(DEF_ASM_OP1(inb, 0xec, 0, OPC_BWL, OPT_DX))

ALT(DEF_ASM_OP2(outb, 0xe6, 0, OPC_BWL, OPT_EAX, OPT_IM8))
ALT(DEF_ASM_OP1(outb, 0xe6, 0, OPC_BWL, OPT_IM8))
ALT(DEF_ASM_OP2(outb, 0xee, 0, OPC_BWL, OPT_EAX, OPT_DX))
ALT(DEF_ASM_OP1(outb, 0xee, 0, OPC_BWL, OPT_DX))

ALT(DEF_ASM_OP2(leaw, 0x8d, 0, OPC_MODRM | OPC_WL, OPT_EA, OPT_REG))

ALT(DEF_ASM_OP2(les, 0xc4, 0, OPC_MODRM, OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(lds, 0xc5, 0, OPC_MODRM, OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(lss, 0x0fb2, 0, OPC_MODRM, OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(lfs, 0x0fb4, 0, OPC_MODRM, OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(lgs, 0x0fb5, 0, OPC_MODRM, OPT_EA, OPT_REG32))

     
ALT(DEF_ASM_OP2(addb, 0x00, 0, OPC_ARITH | OPC_MODRM | OPC_BWL, OPT_REG, OPT_EA | OPT_REG)) 
ALT(DEF_ASM_OP2(addb, 0x02, 0, OPC_ARITH | OPC_MODRM | OPC_BWL, OPT_EA | OPT_REG, OPT_REG))
ALT(DEF_ASM_OP2(addb, 0x04, 0, OPC_ARITH | OPC_BWL, OPT_IM, OPT_EAX))
ALT(DEF_ASM_OP2(addb, 0x80, 0, OPC_ARITH | OPC_MODRM | OPC_BWL, OPT_IM, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(addw, 0x83, 0, OPC_ARITH | OPC_MODRM | OPC_WL, OPT_IM8S, OPT_EA | OPT_REG))

ALT(DEF_ASM_OP2(testb, 0x84, 0, OPC_MODRM | OPC_BWL, OPT_EA | OPT_REG, OPT_REG))
ALT(DEF_ASM_OP2(testb, 0x84, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(testb, 0xa8, 0, OPC_BWL, OPT_IM, OPT_EAX))
ALT(DEF_ASM_OP2(testb, 0xf6, 0, OPC_MODRM | OPC_BWL, OPT_IM, OPT_EA | OPT_REG))

ALT(DEF_ASM_OP1(incw, 0x40, 0, OPC_REG | OPC_WL, OPT_REGW))
ALT(DEF_ASM_OP1(incb, 0xfe, 0, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP1(decw, 0x48, 0, OPC_REG | OPC_WL, OPT_REGW))
ALT(DEF_ASM_OP1(decb, 0xfe, 1, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))

ALT(DEF_ASM_OP1(notb, 0xf6, 2, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP1(negb, 0xf6, 3, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))

ALT(DEF_ASM_OP1(mulb, 0xf6, 4, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP1(imulb, 0xf6, 5, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))

ALT(DEF_ASM_OP2(imulw, 0x0faf, 0, OPC_MODRM | OPC_WL, OPT_REG | OPT_EA, OPT_REG))
ALT(DEF_ASM_OP3(imulw, 0x6b, 0, OPC_MODRM | OPC_WL, OPT_IM8S, OPT_REGW | OPT_EA, OPT_REGW))
ALT(DEF_ASM_OP2(imulw, 0x6b, 0, OPC_MODRM | OPC_WL, OPT_IM8S, OPT_REGW))
ALT(DEF_ASM_OP3(imulw, 0x69, 0, OPC_MODRM | OPC_WL, OPT_IMW, OPT_REGW | OPT_EA, OPT_REGW))
ALT(DEF_ASM_OP2(imulw, 0x69, 0, OPC_MODRM | OPC_WL, OPT_IMW, OPT_REGW))

ALT(DEF_ASM_OP1(divb, 0xf6, 6, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP2(divb, 0xf6, 6, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA, OPT_EAX))
ALT(DEF_ASM_OP1(idivb, 0xf6, 7, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP2(idivb, 0xf6, 7, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA, OPT_EAX))

     
ALT(DEF_ASM_OP2(rolb, 0xc0, 0, OPC_MODRM | OPC_BWL | OPC_SHIFT, OPT_IM8, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(rolb, 0xd2, 0, OPC_MODRM | OPC_BWL | OPC_SHIFT, OPT_CL, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP1(rolb, 0xd0, 0, OPC_MODRM | OPC_BWL | OPC_SHIFT, OPT_EA | OPT_REG))

ALT(DEF_ASM_OP3(shldw, 0x0fa4, 0, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP3(shldw, 0x0fa5, 0, OPC_MODRM | OPC_WL, OPT_CL, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP2(shldw, 0x0fa5, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP3(shrdw, 0x0fac, 0, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP3(shrdw, 0x0fad, 0, OPC_MODRM | OPC_WL, OPT_CL, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP2(shrdw, 0x0fad, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_EA | OPT_REGW))

ALT(DEF_ASM_OP1(call, 0xff, 2, OPC_MODRM, OPT_INDIR))
ALT(DEF_ASM_OP1(call, 0xe8, 0, OPC_JMP, OPT_ADDR))
ALT(DEF_ASM_OP1(jmp, 0xff, 4, OPC_MODRM, OPT_INDIR))
ALT(DEF_ASM_OP1(jmp, 0xeb, 0, OPC_SHORTJMP | OPC_JMP, OPT_ADDR))

ALT(DEF_ASM_OP2(lcall, 0x9a, 0, 0, OPT_IM16, OPT_IM32))
ALT(DEF_ASM_OP1(lcall, 0xff, 3, 0, OPT_EA))
ALT(DEF_ASM_OP2(ljmp, 0xea, 0, 0, OPT_IM16, OPT_IM32))
ALT(DEF_ASM_OP1(ljmp, 0xff, 5, 0, OPT_EA))

ALT(DEF_ASM_OP1(int, 0xcd, 0, 0, OPT_IM8))
ALT(DEF_ASM_OP1(seto, 0x0f90, 0, OPC_MODRM | OPC_TEST, OPT_REG8 | OPT_EA))
    DEF_ASM_OP2(enter, 0xc8, 0, 0, OPT_IM16, OPT_IM8)
    DEF_ASM_OP0(leave, 0xc9)
    DEF_ASM_OP0(ret, 0xc3)
ALT(DEF_ASM_OP1(ret, 0xc2, 0, 0, OPT_IM16))
    DEF_ASM_OP0(lret, 0xcb)
ALT(DEF_ASM_OP1(lret, 0xca, 0, 0, OPT_IM16))

ALT(DEF_ASM_OP1(jo, 0x70, 0, OPC_SHORTJMP | OPC_JMP | OPC_TEST, OPT_ADDR))
    DEF_ASM_OP1(loopne, 0xe0, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(loopnz, 0xe0, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(loope, 0xe1, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(loopz, 0xe1, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(loop, 0xe2, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(jecxz, 0xe3, 0, OPC_SHORTJMP, OPT_ADDR)
     
     
     
ALT(DEF_ASM_OP0L(fcomp, 0xd8d9, 0, 0))

ALT(DEF_ASM_OP1(fadd, 0xd8c0, 0, OPC_FARITH | OPC_REG, OPT_ST))
ALT(DEF_ASM_OP2(fadd, 0xd8c0, 0, OPC_FARITH | OPC_REG, OPT_ST, OPT_ST0))
ALT(DEF_ASM_OP0L(fadd, 0xdec1, 0, OPC_FARITH))
ALT(DEF_ASM_OP1(faddp, 0xdec0, 0, OPC_FARITH | OPC_REG, OPT_ST))
ALT(DEF_ASM_OP2(faddp, 0xdec0, 0, OPC_FARITH | OPC_REG, OPT_ST, OPT_ST0))
ALT(DEF_ASM_OP2(faddp, 0xdec0, 0, OPC_FARITH | OPC_REG, OPT_ST0, OPT_ST))
ALT(DEF_ASM_OP0L(faddp, 0xdec1, 0, OPC_FARITH))
ALT(DEF_ASM_OP1(fadds, 0xd8, 0, OPC_FARITH | OPC_MODRM, OPT_EA))
ALT(DEF_ASM_OP1(fiaddl, 0xda, 0, OPC_FARITH | OPC_MODRM, OPT_EA))
ALT(DEF_ASM_OP1(faddl, 0xdc, 0, OPC_FARITH | OPC_MODRM, OPT_EA))
ALT(DEF_ASM_OP1(fiadds, 0xde, 0, OPC_FARITH | OPC_MODRM, OPT_EA))

     DEF_ASM_OP0(fucompp, 0xdae9)
     DEF_ASM_OP0(ftst, 0xd9e4)
     DEF_ASM_OP0(fxam, 0xd9e5)
     DEF_ASM_OP0(fld1, 0xd9e8)
     DEF_ASM_OP0(fldl2t, 0xd9e9)
     DEF_ASM_OP0(fldl2e, 0xd9ea)
     DEF_ASM_OP0(fldpi, 0xd9eb)
     DEF_ASM_OP0(fldlg2, 0xd9ec)
     DEF_ASM_OP0(fldln2, 0xd9ed)
     DEF_ASM_OP0(fldz, 0xd9ee)

     DEF_ASM_OP0(f2xm1, 0xd9f0)
     DEF_ASM_OP0(fyl2x, 0xd9f1)
     DEF_ASM_OP0(fptan, 0xd9f2)
     DEF_ASM_OP0(fpatan, 0xd9f3)
     DEF_ASM_OP0(fxtract, 0xd9f4)
     DEF_ASM_OP0(fprem1, 0xd9f5)
     DEF_ASM_OP0(fdecstp, 0xd9f6)
     DEF_ASM_OP0(fincstp, 0xd9f7)
     DEF_ASM_OP0(fprem, 0xd9f8)
     DEF_ASM_OP0(fyl2xp1, 0xd9f9)
     DEF_ASM_OP0(fsqrt, 0xd9fa)
     DEF_ASM_OP0(fsincos, 0xd9fb)
     DEF_ASM_OP0(frndint, 0xd9fc)
     DEF_ASM_OP0(fscale, 0xd9fd)
     DEF_ASM_OP0(fsin, 0xd9fe)
     DEF_ASM_OP0(fcos, 0xd9ff)
     DEF_ASM_OP0(fchs, 0xd9e0)
     DEF_ASM_OP0(fabs, 0xd9e1)
     DEF_ASM_OP0(fninit, 0xdbe3)
     DEF_ASM_OP0(fnclex, 0xdbe2)
     DEF_ASM_OP0(fnop, 0xd9d0)
     DEF_ASM_OP0(fwait, 0x9b)

    
    DEF_ASM_OP1(fld, 0xd9c0, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(fldl, 0xd9c0, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(flds, 0xd9, 0, OPC_MODRM, OPT_EA)
ALT(DEF_ASM_OP1(fldl, 0xdd, 0, OPC_MODRM, OPT_EA))
    DEF_ASM_OP1(fildl, 0xdb, 0, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fildq, 0xdf, 5, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fildll, 0xdf, 5, OPC_MODRM,OPT_EA)
    DEF_ASM_OP1(fldt, 0xdb, 5, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fbld, 0xdf, 4, OPC_MODRM, OPT_EA)
    
    
    DEF_ASM_OP1(fst, 0xddd0, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(fstl, 0xddd0, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(fsts, 0xd9, 2, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fstps, 0xd9, 3, OPC_MODRM, OPT_EA)
ALT(DEF_ASM_OP1(fstl, 0xdd, 2, OPC_MODRM, OPT_EA))
    DEF_ASM_OP1(fstpl, 0xdd, 3, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fist, 0xdf, 2, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fistp, 0xdf, 3, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fistl, 0xdb, 2, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fistpl, 0xdb, 3, OPC_MODRM, OPT_EA)

    DEF_ASM_OP1(fstp, 0xddd8, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(fistpq, 0xdf, 7, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fistpll, 0xdf, 7, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fstpt, 0xdb, 7, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fbstp, 0xdf, 6, OPC_MODRM, OPT_EA)

    
    DEF_ASM_OP0(fxch, 0xd9c9)
ALT(DEF_ASM_OP1(fxch, 0xd9c8, 0, OPC_REG, OPT_ST))

    
    DEF_ASM_OP1(fucom, 0xdde0, 0, OPC_REG, OPT_ST )
    DEF_ASM_OP1(fucomp, 0xdde8, 0, OPC_REG, OPT_ST )

    DEF_ASM_OP0L(finit, 0xdbe3, 0, OPC_FWAIT)
    DEF_ASM_OP1(fldcw, 0xd9, 5, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fnstcw, 0xd9, 7, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fstcw, 0xd9, 7, OPC_MODRM | OPC_FWAIT, OPT_EA )
    DEF_ASM_OP0(fnstsw, 0xdfe0)
ALT(DEF_ASM_OP1(fnstsw, 0xdfe0, 0, 0, OPT_EAX ))
ALT(DEF_ASM_OP1(fnstsw, 0xdd, 7, OPC_MODRM, OPT_EA ))
    DEF_ASM_OP1(fstsw, 0xdfe0, 0, OPC_FWAIT, OPT_EAX )
ALT(DEF_ASM_OP0L(fstsw, 0xdfe0, 0, OPC_FWAIT))
ALT(DEF_ASM_OP1(fstsw, 0xdd, 7, OPC_MODRM | OPC_FWAIT, OPT_EA ))
    DEF_ASM_OP0L(fclex, 0xdbe2, 0, OPC_FWAIT)
    DEF_ASM_OP1(fnstenv, 0xd9, 6, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fstenv, 0xd9, 6, OPC_MODRM | OPC_FWAIT, OPT_EA )
    DEF_ASM_OP1(fldenv, 0xd9, 4, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fnsave, 0xdd, 6, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fsave, 0xdd, 6, OPC_MODRM | OPC_FWAIT, OPT_EA )
    DEF_ASM_OP1(frstor, 0xdd, 4, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(ffree, 0xddc0, 4, OPC_REG, OPT_ST )
    DEF_ASM_OP1(ffreep, 0xdfc0, 4, OPC_REG, OPT_ST )
    DEF_ASM_OP1(fxsave, 0x0fae, 0, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fxrstor, 0x0fae, 1, OPC_MODRM, OPT_EA )

    
    DEF_ASM_OP2(arpl, 0x63, 0, OPC_MODRM, OPT_REG16, OPT_REG16 | OPT_EA)
    DEF_ASM_OP2(lar, 0x0f02, 0, OPC_MODRM, OPT_REG32 | OPT_EA, OPT_REG32)
    DEF_ASM_OP1(lgdt, 0x0f01, 2, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(lidt, 0x0f01, 3, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(lldt, 0x0f00, 2, OPC_MODRM, OPT_EA | OPT_REG)
    DEF_ASM_OP1(lmsw, 0x0f01, 6, OPC_MODRM, OPT_EA | OPT_REG)
ALT(DEF_ASM_OP2(lslw, 0x0f03, 0, OPC_MODRM | OPC_WL, OPT_EA | OPT_REG, OPT_REG))
    DEF_ASM_OP1(ltr, 0x0f00, 3, OPC_MODRM, OPT_EA | OPT_REG)
    DEF_ASM_OP1(sgdt, 0x0f01, 0, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(sidt, 0x0f01, 1, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(sldt, 0x0f00, 0, OPC_MODRM, OPT_REG | OPT_EA)
    DEF_ASM_OP1(smsw, 0x0f01, 4, OPC_MODRM, OPT_REG | OPT_EA)
    DEF_ASM_OP1(str, 0x0f00, 1, OPC_MODRM, OPT_REG16| OPT_EA)
    DEF_ASM_OP1(verr, 0x0f00, 4, OPC_MODRM, OPT_REG | OPT_EA)
    DEF_ASM_OP1(verw, 0x0f00, 5, OPC_MODRM, OPT_REG | OPT_EA)

    
    DEF_ASM_OP1(bswap, 0x0fc8, 0, OPC_REG, OPT_REG32 )
ALT(DEF_ASM_OP2(xaddb, 0x0fc0, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_REG | OPT_EA ))
ALT(DEF_ASM_OP2(cmpxchgb, 0x0fb0, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_REG | OPT_EA ))
    DEF_ASM_OP1(invlpg, 0x0f01, 7, OPC_MODRM, OPT_EA )

    DEF_ASM_OP2(boundl, 0x62, 0, OPC_MODRM, OPT_REG32, OPT_EA)
    DEF_ASM_OP2(boundw, 0x62, 0, OPC_MODRM | OPC_D16, OPT_REG16, OPT_EA)

    
    DEF_ASM_OP1(cmpxchg8b, 0x0fc7, 1, OPC_MODRM, OPT_EA )
    
    
    ALT(DEF_ASM_OP2(cmovo, 0x0f40, 0, OPC_MODRM | OPC_TEST, OPT_REG32 | OPT_EA, OPT_REG32))

    DEF_ASM_OP2(fcmovb, 0xdac0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmove, 0xdac8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovbe, 0xdad0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovu, 0xdad8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovnb, 0xdbc0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovne, 0xdbc8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovnbe, 0xdbd0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovnu, 0xdbd8, 0, OPC_REG, OPT_ST, OPT_ST0 )

    DEF_ASM_OP2(fucomi, 0xdbe8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcomi, 0xdbf0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fucomip, 0xdfe8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcomip, 0xdff0, 0, OPC_REG, OPT_ST, OPT_ST0 )

    
    DEF_ASM_OP0(emms, 0x0f77) 
    DEF_ASM_OP2(movd, 0x0f6e, 0, OPC_MODRM, OPT_EA | OPT_REG32, OPT_MMX )
ALT(DEF_ASM_OP2(movd, 0x0f7e, 0, OPC_MODRM, OPT_MMX, OPT_EA | OPT_REG32 ))
    DEF_ASM_OP2(movq, 0x0f6f, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(movq, 0x0f7f, 0, OPC_MODRM, OPT_MMX, OPT_EA | OPT_MMX ))
    DEF_ASM_OP2(packssdw, 0x0f6b, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(packsswb, 0x0f63, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(packuswb, 0x0f67, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddb, 0x0ffc, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddw, 0x0ffd, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddd, 0x0ffe, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddsb, 0x0fec, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddsw, 0x0fed, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddusb, 0x0fdc, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddusw, 0x0fdd, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pand, 0x0fdb, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pandn, 0x0fdf, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpeqb, 0x0f74, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpeqw, 0x0f75, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpeqd, 0x0f76, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpgtb, 0x0f64, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpgtw, 0x0f65, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpgtd, 0x0f66, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pmaddwd, 0x0ff5, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pmulhw, 0x0fe5, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pmullw, 0x0fd5, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(por, 0x0feb, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psllw, 0x0ff1, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psllw, 0x0f71, 6, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(pslld, 0x0ff2, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(pslld, 0x0f72, 6, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psllq, 0x0ff3, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psllq, 0x0f73, 6, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psraw, 0x0fe1, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psraw, 0x0f71, 4, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psrad, 0x0fe2, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psrad, 0x0f72, 4, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psrlw, 0x0fd1, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psrlw, 0x0f71, 2, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psrld, 0x0fd2, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psrld, 0x0f72, 2, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psrlq, 0x0fd3, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psrlq, 0x0f73, 2, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psubb, 0x0ff8, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubw, 0x0ff9, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubd, 0x0ffa, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubsb, 0x0fe8, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubsw, 0x0fe9, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubusb, 0x0fd8, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubusw, 0x0fd9, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpckhbw, 0x0f68, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpckhwd, 0x0f69, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpckhdq, 0x0f6a, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpcklbw, 0x0f60, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpcklwd, 0x0f61, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpckldq, 0x0f62, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pxor, 0x0fef, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )

#undef ALT
#undef DEF_ASM_OP0
#undef DEF_ASM_OP0L
#undef DEF_ASM_OP1
#undef DEF_ASM_OP2
#undef DEF_ASM_OP3

#endif
#undef DEF
};

static const char tcc_keywords[] = 
#define DEF(id, str) str "\0"
     DEF(TOK_INT, "int")
     DEF(TOK_VOID, "void")
     DEF(TOK_CHAR, "char")
     DEF(TOK_IF, "if")
     DEF(TOK_ELSE, "else")
     DEF(TOK_WHILE, "while")
     DEF(TOK_BREAK, "break")
     DEF(TOK_RETURN, "return")
     DEF(TOK_FOR, "for")
     DEF(TOK_EXTERN, "extern")
     DEF(TOK_STATIC, "static")
     DEF(TOK_UNSIGNED, "unsigned")
     DEF(TOK_GOTO, "goto")
     DEF(TOK_DO, "do")
     DEF(TOK_CONTINUE, "continue")
     DEF(TOK_SWITCH, "switch")
     DEF(TOK_CASE, "case")

     DEF(TOK_CONST1, "const")
     DEF(TOK_CONST2, "__const") 
     DEF(TOK_CONST3, "__const__") 
     DEF(TOK_VOLATILE1, "volatile")
     DEF(TOK_VOLATILE2, "__volatile") 
     DEF(TOK_VOLATILE3, "__volatile__") 
     DEF(TOK_LONG, "long")
     DEF(TOK_REGISTER, "register")
     DEF(TOK_SIGNED1, "signed")
     DEF(TOK_SIGNED2, "__signed") 
     DEF(TOK_SIGNED3, "__signed__") 
     DEF(TOK_AUTO, "auto")
     DEF(TOK_INLINE1, "inline")
     DEF(TOK_INLINE2, "__inline") 
     DEF(TOK_INLINE3, "__inline__") 
     DEF(TOK_RESTRICT1, "restrict")
     DEF(TOK_RESTRICT2, "__restrict")
     DEF(TOK_RESTRICT3, "__restrict__")
     DEF(TOK_EXTENSION, "__extension__") 
     
     DEF(TOK_FLOAT, "float")
     DEF(TOK_DOUBLE, "double")
     DEF(TOK_BOOL, "_Bool")
     DEF(TOK_SHORT, "short")
     DEF(TOK_STRUCT, "struct")
     DEF(TOK_UNION, "union")
     DEF(TOK_TYPEDEF, "typedef")
     DEF(TOK_DEFAULT, "default")
     DEF(TOK_ENUM, "enum")
     DEF(TOK_SIZEOF, "sizeof")
     DEF(TOK_ATTRIBUTE1, "__attribute")
     DEF(TOK_ATTRIBUTE2, "__attribute__")
     DEF(TOK_ALIGNOF1, "__alignof")
     DEF(TOK_ALIGNOF2, "__alignof__")
     DEF(TOK_TYPEOF1, "typeof")
     DEF(TOK_TYPEOF2, "__typeof")
     DEF(TOK_TYPEOF3, "__typeof__")
     DEF(TOK_LABEL, "__label__")
     DEF(TOK_ASM1, "asm")
     DEF(TOK_ASM2, "__asm")
     DEF(TOK_ASM3, "__asm__")

     DEF(TOK_DEFINE, "define")
     DEF(TOK_INCLUDE, "include")
     DEF(TOK_INCLUDE_NEXT, "include_next")
     DEF(TOK_IFDEF, "ifdef")
     DEF(TOK_IFNDEF, "ifndef")
     DEF(TOK_ELIF, "elif")
     DEF(TOK_ENDIF, "endif")
     DEF(TOK_DEFINED, "defined")
     DEF(TOK_UNDEF, "undef")
     DEF(TOK_ERROR, "error")
     DEF(TOK_WARNING, "warning")
     DEF(TOK_LINE, "line")
     DEF(TOK_PRAGMA, "pragma")
     DEF(TOK___LINE__, "__LINE__")
     DEF(TOK___FILE__, "__FILE__")
     DEF(TOK___DATE__, "__DATE__")
     DEF(TOK___TIME__, "__TIME__")
     DEF(TOK___FUNCTION__, "__FUNCTION__")
     DEF(TOK___VA_ARGS__, "__VA_ARGS__")
     
     DEF(TOK___FUNC__, "__func__")
     
     DEF(TOK_SECTION1, "section")
     DEF(TOK_SECTION2, "__section__")
     DEF(TOK_ALIGNED1, "aligned")
     DEF(TOK_ALIGNED2, "__aligned__")
     DEF(TOK_PACKED1, "packed")
     DEF(TOK_PACKED2, "__packed__")
     DEF(TOK_UNUSED1, "unused")
     DEF(TOK_UNUSED2, "__unused__")
     DEF(TOK_CDECL1, "cdecl")
     DEF(TOK_CDECL2, "__cdecl")
     DEF(TOK_CDECL3, "__cdecl__")
     DEF(TOK_STDCALL1, "stdcall")
     DEF(TOK_STDCALL2, "__stdcall")
     DEF(TOK_STDCALL3, "__stdcall__")
     DEF(TOK_DLLEXPORT, "dllexport")
     DEF(TOK_NORETURN1, "noreturn")
     DEF(TOK_NORETURN2, "__noreturn__")
     DEF(TOK_builtin_types_compatible_p, "__builtin_types_compatible_p")
     DEF(TOK_builtin_constant_p, "__builtin_constant_p")
     DEF(TOK_REGPARM1, "regparm")
     DEF(TOK_REGPARM2, "__regparm__")

     DEF(TOK_pack, "pack")
#if !defined(TCC_TARGET_I386)
     
     DEF(TOK_ASM_push, "push")
     DEF(TOK_ASM_pop, "pop")
#endif

     DEF(TOK_memcpy, "memcpy")
     DEF(TOK_memset, "memset")
     DEF(TOK_alloca, "alloca")
     DEF(TOK___divdi3, "__divdi3")
     DEF(TOK___moddi3, "__moddi3")
     DEF(TOK___udivdi3, "__udivdi3")
     DEF(TOK___umoddi3, "__umoddi3")
#if defined(TCC_TARGET_ARM)
     DEF(TOK___divsi3, "__divsi3")
     DEF(TOK___modsi3, "__modsi3")
     DEF(TOK___udivsi3, "__udivsi3")
     DEF(TOK___umodsi3, "__umodsi3")
     DEF(TOK___sardi3, "__ashrdi3")
     DEF(TOK___shrdi3, "__lshrdi3")
     DEF(TOK___shldi3, "__ashldi3")
     DEF(TOK___slltold, "__slltold")
     DEF(TOK___fixunssfsi, "__fixunssfsi")
     DEF(TOK___fixunsdfsi, "__fixunsdfsi")
     DEF(TOK___fixunsxfsi, "__fixunsxfsi")
     DEF(TOK___fixsfdi, "__fixsfdi")
     DEF(TOK___fixdfdi, "__fixdfdi")
     DEF(TOK___fixxfdi, "__fixxfdi")
#elif defined(TCC_TARGET_C67)
     DEF(TOK__divi, "_divi")
     DEF(TOK__divu, "_divu")
     DEF(TOK__divf, "_divf")
     DEF(TOK__divd, "_divd")
     DEF(TOK__remi, "_remi")
     DEF(TOK__remu, "_remu")
     DEF(TOK___sardi3, "__sardi3")
     DEF(TOK___shrdi3, "__shrdi3")
     DEF(TOK___shldi3, "__shldi3")
#else
     
     DEF(TOK___sardi3, "__sardi3")
     DEF(TOK___shrdi3, "__shrdi3")
     DEF(TOK___shldi3, "__shldi3")
#endif
     DEF(TOK___tcc_int_fpu_control, "__tcc_int_fpu_control")
     DEF(TOK___tcc_fpu_control, "__tcc_fpu_control")
     DEF(TOK___ulltof, "__ulltof")
     DEF(TOK___ulltod, "__ulltod")
     DEF(TOK___ulltold, "__ulltold")
     DEF(TOK___fixunssfdi, "__fixunssfdi")
     DEF(TOK___fixunsdfdi, "__fixunsdfdi")
     DEF(TOK___fixunsxfdi, "__fixunsxfdi")
     DEF(TOK___chkstk, "__chkstk")

#ifdef CONFIG_TCC_BCHECK
     DEF(TOK___bound_ptr_add, "__bound_ptr_add")
     DEF(TOK___bound_ptr_indir1, "__bound_ptr_indir1")
     DEF(TOK___bound_ptr_indir2, "__bound_ptr_indir2")
     DEF(TOK___bound_ptr_indir4, "__bound_ptr_indir4")
     DEF(TOK___bound_ptr_indir8, "__bound_ptr_indir8")
     DEF(TOK___bound_ptr_indir12, "__bound_ptr_indir12")
     DEF(TOK___bound_ptr_indir16, "__bound_ptr_indir16")
     DEF(TOK___bound_local_new, "__bound_local_new")
     DEF(TOK___bound_local_delete, "__bound_local_delete")
     DEF(TOK_malloc, "malloc")
     DEF(TOK_free, "free")
     DEF(TOK_realloc, "realloc")
     DEF(TOK_memalign, "memalign")
     DEF(TOK_calloc, "calloc")
     DEF(TOK_memmove, "memmove")
     DEF(TOK_strlen, "strlen")
     DEF(TOK_strcpy, "strcpy")
#endif


 DEF_ASM(byte)
 DEF_ASM(align)
 DEF_ASM(skip)
 DEF_ASM(space)
 DEF_ASM(string)
 DEF_ASM(asciz)
 DEF_ASM(ascii)
 DEF_ASM(globl)
 DEF_ASM(global)
 DEF_ASM(text)
 DEF_ASM(data)
 DEF_ASM(bss)
 DEF_ASM(previous)
 DEF_ASM(fill)
 DEF_ASM(org)
 DEF_ASM(quad)

#ifdef TCC_TARGET_I386

 DEF_ASM(al)
 DEF_ASM(cl)
 DEF_ASM(dl)
 DEF_ASM(bl)
 DEF_ASM(ah)
 DEF_ASM(ch)
 DEF_ASM(dh)
 DEF_ASM(bh)
 DEF_ASM(ax)
 DEF_ASM(cx)
 DEF_ASM(dx)
 DEF_ASM(bx)
 DEF_ASM(sp)
 DEF_ASM(bp)
 DEF_ASM(si)
 DEF_ASM(di)
 DEF_ASM(eax)
 DEF_ASM(ecx)
 DEF_ASM(edx)
 DEF_ASM(ebx)
 DEF_ASM(esp)
 DEF_ASM(ebp)
 DEF_ASM(esi)
 DEF_ASM(edi)
 DEF_ASM(mm0)
 DEF_ASM(mm1)
 DEF_ASM(mm2)
 DEF_ASM(mm3)
 DEF_ASM(mm4)
 DEF_ASM(mm5)
 DEF_ASM(mm6)
 DEF_ASM(mm7)
 DEF_ASM(xmm0)
 DEF_ASM(xmm1)
 DEF_ASM(xmm2)
 DEF_ASM(xmm3)
 DEF_ASM(xmm4)
 DEF_ASM(xmm5)
 DEF_ASM(xmm6)
 DEF_ASM(xmm7)
 DEF_ASM(cr0)
 DEF_ASM(cr1)
 DEF_ASM(cr2)
 DEF_ASM(cr3)
 DEF_ASM(cr4)
 DEF_ASM(cr5)
 DEF_ASM(cr6)
 DEF_ASM(cr7)
 DEF_ASM(tr0)
 DEF_ASM(tr1)
 DEF_ASM(tr2)
 DEF_ASM(tr3)
 DEF_ASM(tr4)
 DEF_ASM(tr5)
 DEF_ASM(tr6)
 DEF_ASM(tr7)
 DEF_ASM(db0)
 DEF_ASM(db1)
 DEF_ASM(db2)
 DEF_ASM(db3)
 DEF_ASM(db4)
 DEF_ASM(db5)
 DEF_ASM(db6)
 DEF_ASM(db7)
 DEF_ASM(dr0)
 DEF_ASM(dr1)
 DEF_ASM(dr2)
 DEF_ASM(dr3)
 DEF_ASM(dr4)
 DEF_ASM(dr5)
 DEF_ASM(dr6)
 DEF_ASM(dr7)
 DEF_ASM(es)
 DEF_ASM(cs)
 DEF_ASM(ss)
 DEF_ASM(ds)
 DEF_ASM(fs)
 DEF_ASM(gs)
 DEF_ASM(st)

 DEF_BWL(mov)

 
 DEF_BWL(add)
 DEF_BWL(or)
 DEF_BWL(adc)
 DEF_BWL(sbb)
 DEF_BWL(and)
 DEF_BWL(sub)
 DEF_BWL(xor)
 DEF_BWL(cmp)

 
 DEF_BWL(inc)
 DEF_BWL(dec)
 DEF_BWL(not)
 DEF_BWL(neg)
 DEF_BWL(mul)
 DEF_BWL(imul)
 DEF_BWL(div)
 DEF_BWL(idiv)

 DEF_BWL(xchg)
 DEF_BWL(test)

 
 DEF_BWL(rol)
 DEF_BWL(ror)
 DEF_BWL(rcl)
 DEF_BWL(rcr)
 DEF_BWL(shl)
 DEF_BWL(shr)
 DEF_BWL(sar)

 DEF_ASM(shldw)
 DEF_ASM(shldl)
 DEF_ASM(shld)
 DEF_ASM(shrdw)
 DEF_ASM(shrdl)
 DEF_ASM(shrd)

 DEF_ASM(pushw)
 DEF_ASM(pushl)
 DEF_ASM(push)
 DEF_ASM(popw)
 DEF_ASM(popl)
 DEF_ASM(pop)
 DEF_BWL(in)
 DEF_BWL(out)

 DEF_WL(movzb)

 DEF_ASM(movzwl)
 DEF_ASM(movsbw)
 DEF_ASM(movsbl)
 DEF_ASM(movswl)

 DEF_WL(lea) 

 DEF_ASM(les) 
 DEF_ASM(lds) 
 DEF_ASM(lss) 
 DEF_ASM(lfs) 
 DEF_ASM(lgs) 

 DEF_ASM(call)
 DEF_ASM(jmp)
 DEF_ASM(lcall)
 DEF_ASM(ljmp)
 
 DEF_ASMTEST(j)

 DEF_ASMTEST(set)
 DEF_ASMTEST(cmov)

 DEF_WL(bsf)
 DEF_WL(bsr)
 DEF_WL(bt)
 DEF_WL(bts)
 DEF_WL(btr)
 DEF_WL(btc)

 DEF_WL(lsl)

 
 DEF_FP(add)
 DEF_FP(mul)

 DEF_ASM(fcom)
 DEF_ASM(fcom_1) 
 DEF_FP1(com)

 DEF_FP(comp)
 DEF_FP(sub)
 DEF_FP(subr)
 DEF_FP(div)
 DEF_FP(divr)

 DEF_BWL(xadd)
 DEF_BWL(cmpxchg)

 
 DEF_BWL(cmps)
 DEF_BWL(scmp)
 DEF_BWL(ins)
 DEF_BWL(outs)
 DEF_BWL(lods)
 DEF_BWL(slod)
 DEF_BWL(movs)
 DEF_BWL(smov)
 DEF_BWL(scas)
 DEF_BWL(ssca)
 DEF_BWL(stos)
 DEF_BWL(ssto)

 

#define ALT(x)
#define DEF_ASM_OP0(name, opcode) DEF_ASM(name)
#define DEF_ASM_OP0L(name, opcode, group, instr_type)
#define DEF_ASM_OP1(name, opcode, group, instr_type, op0)
#define DEF_ASM_OP2(name, opcode, group, instr_type, op0, op1)
#define DEF_ASM_OP3(name, opcode, group, instr_type, op0, op1, op2)
     DEF_ASM_OP0(pusha, 0x60) 
     DEF_ASM_OP0(popa, 0x61)
     DEF_ASM_OP0(clc, 0xf8)
     DEF_ASM_OP0(cld, 0xfc)
     DEF_ASM_OP0(cli, 0xfa)
     DEF_ASM_OP0(clts, 0x0f06)
     DEF_ASM_OP0(cmc, 0xf5)
     DEF_ASM_OP0(lahf, 0x9f)
     DEF_ASM_OP0(sahf, 0x9e)
     DEF_ASM_OP0(pushfl, 0x9c)
     DEF_ASM_OP0(popfl, 0x9d)
     DEF_ASM_OP0(pushf, 0x9c)
     DEF_ASM_OP0(popf, 0x9d)
     DEF_ASM_OP0(stc, 0xf9)
     DEF_ASM_OP0(std, 0xfd)
     DEF_ASM_OP0(sti, 0xfb)
     DEF_ASM_OP0(aaa, 0x37)
     DEF_ASM_OP0(aas, 0x3f)
     DEF_ASM_OP0(daa, 0x27)
     DEF_ASM_OP0(das, 0x2f)
     DEF_ASM_OP0(aad, 0xd50a)
     DEF_ASM_OP0(aam, 0xd40a)
     DEF_ASM_OP0(cbw, 0x6698)
     DEF_ASM_OP0(cwd, 0x6699)
     DEF_ASM_OP0(cwde, 0x98)
     DEF_ASM_OP0(cdq, 0x99)
     DEF_ASM_OP0(cbtw, 0x6698)
     DEF_ASM_OP0(cwtl, 0x98)
     DEF_ASM_OP0(cwtd, 0x6699)
     DEF_ASM_OP0(cltd, 0x99)
     DEF_ASM_OP0(int3, 0xcc)
     DEF_ASM_OP0(into, 0xce)
     DEF_ASM_OP0(iret, 0xcf)
     DEF_ASM_OP0(rsm, 0x0faa)
     DEF_ASM_OP0(hlt, 0xf4)
     DEF_ASM_OP0(wait, 0x9b)
     DEF_ASM_OP0(nop, 0x90)
     DEF_ASM_OP0(xlat, 0xd7)

     
ALT(DEF_ASM_OP0L(cmpsb, 0xa6, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(scmpb, 0xa6, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(insb, 0x6c, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(outsb, 0x6e, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(lodsb, 0xac, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(slodb, 0xac, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(movsb, 0xa4, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(smovb, 0xa4, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(scasb, 0xae, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(sscab, 0xae, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(stosb, 0xaa, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(sstob, 0xaa, 0, OPC_BWL))

     
     
ALT(DEF_ASM_OP2(bsfw, 0x0fbc, 0, OPC_MODRM | OPC_WL, OPT_REGW | OPT_EA, OPT_REGW))
ALT(DEF_ASM_OP2(bsrw, 0x0fbd, 0, OPC_MODRM | OPC_WL, OPT_REGW | OPT_EA, OPT_REGW))

ALT(DEF_ASM_OP2(btw, 0x0fa3, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP2(btw, 0x0fba, 4, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW | OPT_EA))

ALT(DEF_ASM_OP2(btsw, 0x0fab, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP2(btsw, 0x0fba, 5, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW | OPT_EA))

ALT(DEF_ASM_OP2(btrw, 0x0fb3, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP2(btrw, 0x0fba, 6, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW | OPT_EA))

ALT(DEF_ASM_OP2(btcw, 0x0fbb, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP2(btcw, 0x0fba, 7, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW | OPT_EA))

     
     DEF_ASM_OP0(aword, 0x67)
     DEF_ASM_OP0(addr16, 0x67)
     DEF_ASM_OP0(word, 0x66)
     DEF_ASM_OP0(data16, 0x66)
     DEF_ASM_OP0(lock, 0xf0)
     DEF_ASM_OP0(rep, 0xf3)
     DEF_ASM_OP0(repe, 0xf3)
     DEF_ASM_OP0(repz, 0xf3)
     DEF_ASM_OP0(repne, 0xf2)
     DEF_ASM_OP0(repnz, 0xf2)
             
     DEF_ASM_OP0(invd, 0x0f08)
     DEF_ASM_OP0(wbinvd, 0x0f09)
     DEF_ASM_OP0(cpuid, 0x0fa2)
     DEF_ASM_OP0(wrmsr, 0x0f30)
     DEF_ASM_OP0(rdtsc, 0x0f31)
     DEF_ASM_OP0(rdmsr, 0x0f32)
     DEF_ASM_OP0(rdpmc, 0x0f33)
     DEF_ASM_OP0(ud2, 0x0f0b)

     
ALT(DEF_ASM_OP2(movb, 0xa0, 0, OPC_BWL, OPT_ADDR, OPT_EAX))
ALT(DEF_ASM_OP2(movb, 0xa2, 0, OPC_BWL, OPT_EAX, OPT_ADDR))
ALT(DEF_ASM_OP2(movb, 0x88, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(movb, 0x8a, 0, OPC_MODRM | OPC_BWL, OPT_EA | OPT_REG, OPT_REG))
ALT(DEF_ASM_OP2(movb, 0xb0, 0, OPC_REG | OPC_BWL, OPT_IM, OPT_REG))
ALT(DEF_ASM_OP2(movb, 0xc6, 0, OPC_MODRM | OPC_BWL, OPT_IM, OPT_REG | OPT_EA))

ALT(DEF_ASM_OP2(movw, 0x8c, 0, OPC_MODRM | OPC_WL, OPT_SEG, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(movw, 0x8e, 0, OPC_MODRM | OPC_WL, OPT_EA | OPT_REG, OPT_SEG))

ALT(DEF_ASM_OP2(movw, 0x0f20, 0, OPC_MODRM | OPC_WL, OPT_CR, OPT_REG32))
ALT(DEF_ASM_OP2(movw, 0x0f21, 0, OPC_MODRM | OPC_WL, OPT_DB, OPT_REG32))
ALT(DEF_ASM_OP2(movw, 0x0f24, 0, OPC_MODRM | OPC_WL, OPT_TR, OPT_REG32))
ALT(DEF_ASM_OP2(movw, 0x0f22, 0, OPC_MODRM | OPC_WL, OPT_REG32, OPT_CR))
ALT(DEF_ASM_OP2(movw, 0x0f23, 0, OPC_MODRM | OPC_WL, OPT_REG32, OPT_DB))
ALT(DEF_ASM_OP2(movw, 0x0f26, 0, OPC_MODRM | OPC_WL, OPT_REG32, OPT_TR))

ALT(DEF_ASM_OP2(movsbl, 0x0fbe, 0, OPC_MODRM, OPT_REG8 | OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(movsbw, 0x0fbe, 0, OPC_MODRM | OPC_D16, OPT_REG8 | OPT_EA, OPT_REG16))
ALT(DEF_ASM_OP2(movswl, 0x0fbf, 0, OPC_MODRM, OPT_REG16 | OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(movzbw, 0x0fb6, 0, OPC_MODRM | OPC_WL, OPT_REG8 | OPT_EA, OPT_REGW))
ALT(DEF_ASM_OP2(movzwl, 0x0fb7, 0, OPC_MODRM, OPT_REG16 | OPT_EA, OPT_REG32))

ALT(DEF_ASM_OP1(pushw, 0x50, 0, OPC_REG | OPC_WL, OPT_REGW))
ALT(DEF_ASM_OP1(pushw, 0xff, 6, OPC_MODRM | OPC_WL, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP1(pushw, 0x6a, 0, OPC_WL, OPT_IM8S))
ALT(DEF_ASM_OP1(pushw, 0x68, 0, OPC_WL, OPT_IM32))
ALT(DEF_ASM_OP1(pushw, 0x06, 0, OPC_WL, OPT_SEG))

ALT(DEF_ASM_OP1(popw, 0x58, 0, OPC_REG | OPC_WL, OPT_REGW))
ALT(DEF_ASM_OP1(popw, 0x8f, 0, OPC_MODRM | OPC_WL, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP1(popw, 0x07, 0, OPC_WL, OPT_SEG))

ALT(DEF_ASM_OP2(xchgw, 0x90, 0, OPC_REG | OPC_WL, OPT_REG, OPT_EAX))
ALT(DEF_ASM_OP2(xchgw, 0x90, 0, OPC_REG | OPC_WL, OPT_EAX, OPT_REG))
ALT(DEF_ASM_OP2(xchgb, 0x86, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(xchgb, 0x86, 0, OPC_MODRM | OPC_BWL, OPT_EA | OPT_REG, OPT_REG))

ALT(DEF_ASM_OP2(inb, 0xe4, 0, OPC_BWL, OPT_IM8, OPT_EAX))
ALT(DEF_ASM_OP1(inb, 0xe4, 0, OPC_BWL, OPT_IM8))
ALT(DEF_ASM_OP2(inb, 0xec, 0, OPC_BWL, OPT_DX, OPT_EAX))
ALT(DEF_ASM_OP1(inb, 0xec, 0, OPC_BWL, OPT_DX))

ALT(DEF_ASM_OP2(outb, 0xe6, 0, OPC_BWL, OPT_EAX, OPT_IM8))
ALT(DEF_ASM_OP1(outb, 0xe6, 0, OPC_BWL, OPT_IM8))
ALT(DEF_ASM_OP2(outb, 0xee, 0, OPC_BWL, OPT_EAX, OPT_DX))
ALT(DEF_ASM_OP1(outb, 0xee, 0, OPC_BWL, OPT_DX))

ALT(DEF_ASM_OP2(leaw, 0x8d, 0, OPC_MODRM | OPC_WL, OPT_EA, OPT_REG))

ALT(DEF_ASM_OP2(les, 0xc4, 0, OPC_MODRM, OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(lds, 0xc5, 0, OPC_MODRM, OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(lss, 0x0fb2, 0, OPC_MODRM, OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(lfs, 0x0fb4, 0, OPC_MODRM, OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(lgs, 0x0fb5, 0, OPC_MODRM, OPT_EA, OPT_REG32))

     
ALT(DEF_ASM_OP2(addb, 0x00, 0, OPC_ARITH | OPC_MODRM | OPC_BWL, OPT_REG, OPT_EA | OPT_REG)) 
ALT(DEF_ASM_OP2(addb, 0x02, 0, OPC_ARITH | OPC_MODRM | OPC_BWL, OPT_EA | OPT_REG, OPT_REG))
ALT(DEF_ASM_OP2(addb, 0x04, 0, OPC_ARITH | OPC_BWL, OPT_IM, OPT_EAX))
ALT(DEF_ASM_OP2(addb, 0x80, 0, OPC_ARITH | OPC_MODRM | OPC_BWL, OPT_IM, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(addw, 0x83, 0, OPC_ARITH | OPC_MODRM | OPC_WL, OPT_IM8S, OPT_EA | OPT_REG))

ALT(DEF_ASM_OP2(testb, 0x84, 0, OPC_MODRM | OPC_BWL, OPT_EA | OPT_REG, OPT_REG))
ALT(DEF_ASM_OP2(testb, 0x84, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(testb, 0xa8, 0, OPC_BWL, OPT_IM, OPT_EAX))
ALT(DEF_ASM_OP2(testb, 0xf6, 0, OPC_MODRM | OPC_BWL, OPT_IM, OPT_EA | OPT_REG))

ALT(DEF_ASM_OP1(incw, 0x40, 0, OPC_REG | OPC_WL, OPT_REGW))
ALT(DEF_ASM_OP1(incb, 0xfe, 0, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP1(decw, 0x48, 0, OPC_REG | OPC_WL, OPT_REGW))
ALT(DEF_ASM_OP1(decb, 0xfe, 1, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))

ALT(DEF_ASM_OP1(notb, 0xf6, 2, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP1(negb, 0xf6, 3, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))

ALT(DEF_ASM_OP1(mulb, 0xf6, 4, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP1(imulb, 0xf6, 5, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))

ALT(DEF_ASM_OP2(imulw, 0x0faf, 0, OPC_MODRM | OPC_WL, OPT_REG | OPT_EA, OPT_REG))
ALT(DEF_ASM_OP3(imulw, 0x6b, 0, OPC_MODRM | OPC_WL, OPT_IM8S, OPT_REGW | OPT_EA, OPT_REGW))
ALT(DEF_ASM_OP2(imulw, 0x6b, 0, OPC_MODRM | OPC_WL, OPT_IM8S, OPT_REGW))
ALT(DEF_ASM_OP3(imulw, 0x69, 0, OPC_MODRM | OPC_WL, OPT_IMW, OPT_REGW | OPT_EA, OPT_REGW))
ALT(DEF_ASM_OP2(imulw, 0x69, 0, OPC_MODRM | OPC_WL, OPT_IMW, OPT_REGW))

ALT(DEF_ASM_OP1(divb, 0xf6, 6, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP2(divb, 0xf6, 6, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA, OPT_EAX))
ALT(DEF_ASM_OP1(idivb, 0xf6, 7, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP2(idivb, 0xf6, 7, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA, OPT_EAX))

     
ALT(DEF_ASM_OP2(rolb, 0xc0, 0, OPC_MODRM | OPC_BWL | OPC_SHIFT, OPT_IM8, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(rolb, 0xd2, 0, OPC_MODRM | OPC_BWL | OPC_SHIFT, OPT_CL, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP1(rolb, 0xd0, 0, OPC_MODRM | OPC_BWL | OPC_SHIFT, OPT_EA | OPT_REG))

ALT(DEF_ASM_OP3(shldw, 0x0fa4, 0, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP3(shldw, 0x0fa5, 0, OPC_MODRM | OPC_WL, OPT_CL, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP2(shldw, 0x0fa5, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP3(shrdw, 0x0fac, 0, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP3(shrdw, 0x0fad, 0, OPC_MODRM | OPC_WL, OPT_CL, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP2(shrdw, 0x0fad, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_EA | OPT_REGW))

ALT(DEF_ASM_OP1(call, 0xff, 2, OPC_MODRM, OPT_INDIR))
ALT(DEF_ASM_OP1(call, 0xe8, 0, OPC_JMP, OPT_ADDR))
ALT(DEF_ASM_OP1(jmp, 0xff, 4, OPC_MODRM, OPT_INDIR))
ALT(DEF_ASM_OP1(jmp, 0xeb, 0, OPC_SHORTJMP | OPC_JMP, OPT_ADDR))

ALT(DEF_ASM_OP2(lcall, 0x9a, 0, 0, OPT_IM16, OPT_IM32))
ALT(DEF_ASM_OP1(lcall, 0xff, 3, 0, OPT_EA))
ALT(DEF_ASM_OP2(ljmp, 0xea, 0, 0, OPT_IM16, OPT_IM32))
ALT(DEF_ASM_OP1(ljmp, 0xff, 5, 0, OPT_EA))

ALT(DEF_ASM_OP1(int, 0xcd, 0, 0, OPT_IM8))
ALT(DEF_ASM_OP1(seto, 0x0f90, 0, OPC_MODRM | OPC_TEST, OPT_REG8 | OPT_EA))
    DEF_ASM_OP2(enter, 0xc8, 0, 0, OPT_IM16, OPT_IM8)
    DEF_ASM_OP0(leave, 0xc9)
    DEF_ASM_OP0(ret, 0xc3)
ALT(DEF_ASM_OP1(ret, 0xc2, 0, 0, OPT_IM16))
    DEF_ASM_OP0(lret, 0xcb)
ALT(DEF_ASM_OP1(lret, 0xca, 0, 0, OPT_IM16))

ALT(DEF_ASM_OP1(jo, 0x70, 0, OPC_SHORTJMP | OPC_JMP | OPC_TEST, OPT_ADDR))
    DEF_ASM_OP1(loopne, 0xe0, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(loopnz, 0xe0, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(loope, 0xe1, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(loopz, 0xe1, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(loop, 0xe2, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(jecxz, 0xe3, 0, OPC_SHORTJMP, OPT_ADDR)
     
     
     
ALT(DEF_ASM_OP0L(fcomp, 0xd8d9, 0, 0))

ALT(DEF_ASM_OP1(fadd, 0xd8c0, 0, OPC_FARITH | OPC_REG, OPT_ST))
ALT(DEF_ASM_OP2(fadd, 0xd8c0, 0, OPC_FARITH | OPC_REG, OPT_ST, OPT_ST0))
ALT(DEF_ASM_OP0L(fadd, 0xdec1, 0, OPC_FARITH))
ALT(DEF_ASM_OP1(faddp, 0xdec0, 0, OPC_FARITH | OPC_REG, OPT_ST))
ALT(DEF_ASM_OP2(faddp, 0xdec0, 0, OPC_FARITH | OPC_REG, OPT_ST, OPT_ST0))
ALT(DEF_ASM_OP2(faddp, 0xdec0, 0, OPC_FARITH | OPC_REG, OPT_ST0, OPT_ST))
ALT(DEF_ASM_OP0L(faddp, 0xdec1, 0, OPC_FARITH))
ALT(DEF_ASM_OP1(fadds, 0xd8, 0, OPC_FARITH | OPC_MODRM, OPT_EA))
ALT(DEF_ASM_OP1(fiaddl, 0xda, 0, OPC_FARITH | OPC_MODRM, OPT_EA))
ALT(DEF_ASM_OP1(faddl, 0xdc, 0, OPC_FARITH | OPC_MODRM, OPT_EA))
ALT(DEF_ASM_OP1(fiadds, 0xde, 0, OPC_FARITH | OPC_MODRM, OPT_EA))

     DEF_ASM_OP0(fucompp, 0xdae9)
     DEF_ASM_OP0(ftst, 0xd9e4)
     DEF_ASM_OP0(fxam, 0xd9e5)
     DEF_ASM_OP0(fld1, 0xd9e8)
     DEF_ASM_OP0(fldl2t, 0xd9e9)
     DEF_ASM_OP0(fldl2e, 0xd9ea)
     DEF_ASM_OP0(fldpi, 0xd9eb)
     DEF_ASM_OP0(fldlg2, 0xd9ec)
     DEF_ASM_OP0(fldln2, 0xd9ed)
     DEF_ASM_OP0(fldz, 0xd9ee)

     DEF_ASM_OP0(f2xm1, 0xd9f0)
     DEF_ASM_OP0(fyl2x, 0xd9f1)
     DEF_ASM_OP0(fptan, 0xd9f2)
     DEF_ASM_OP0(fpatan, 0xd9f3)
     DEF_ASM_OP0(fxtract, 0xd9f4)
     DEF_ASM_OP0(fprem1, 0xd9f5)
     DEF_ASM_OP0(fdecstp, 0xd9f6)
     DEF_ASM_OP0(fincstp, 0xd9f7)
     DEF_ASM_OP0(fprem, 0xd9f8)
     DEF_ASM_OP0(fyl2xp1, 0xd9f9)
     DEF_ASM_OP0(fsqrt, 0xd9fa)
     DEF_ASM_OP0(fsincos, 0xd9fb)
     DEF_ASM_OP0(frndint, 0xd9fc)
     DEF_ASM_OP0(fscale, 0xd9fd)
     DEF_ASM_OP0(fsin, 0xd9fe)
     DEF_ASM_OP0(fcos, 0xd9ff)
     DEF_ASM_OP0(fchs, 0xd9e0)
     DEF_ASM_OP0(fabs, 0xd9e1)
     DEF_ASM_OP0(fninit, 0xdbe3)
     DEF_ASM_OP0(fnclex, 0xdbe2)
     DEF_ASM_OP0(fnop, 0xd9d0)
     DEF_ASM_OP0(fwait, 0x9b)

    
    DEF_ASM_OP1(fld, 0xd9c0, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(fldl, 0xd9c0, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(flds, 0xd9, 0, OPC_MODRM, OPT_EA)
ALT(DEF_ASM_OP1(fldl, 0xdd, 0, OPC_MODRM, OPT_EA))
    DEF_ASM_OP1(fildl, 0xdb, 0, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fildq, 0xdf, 5, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fildll, 0xdf, 5, OPC_MODRM,OPT_EA)
    DEF_ASM_OP1(fldt, 0xdb, 5, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fbld, 0xdf, 4, OPC_MODRM, OPT_EA)
    
    
    DEF_ASM_OP1(fst, 0xddd0, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(fstl, 0xddd0, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(fsts, 0xd9, 2, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fstps, 0xd9, 3, OPC_MODRM, OPT_EA)
ALT(DEF_ASM_OP1(fstl, 0xdd, 2, OPC_MODRM, OPT_EA))
    DEF_ASM_OP1(fstpl, 0xdd, 3, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fist, 0xdf, 2, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fistp, 0xdf, 3, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fistl, 0xdb, 2, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fistpl, 0xdb, 3, OPC_MODRM, OPT_EA)

    DEF_ASM_OP1(fstp, 0xddd8, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(fistpq, 0xdf, 7, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fistpll, 0xdf, 7, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fstpt, 0xdb, 7, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fbstp, 0xdf, 6, OPC_MODRM, OPT_EA)

    
    DEF_ASM_OP0(fxch, 0xd9c9)
ALT(DEF_ASM_OP1(fxch, 0xd9c8, 0, OPC_REG, OPT_ST))

    
    DEF_ASM_OP1(fucom, 0xdde0, 0, OPC_REG, OPT_ST )
    DEF_ASM_OP1(fucomp, 0xdde8, 0, OPC_REG, OPT_ST )

    DEF_ASM_OP0L(finit, 0xdbe3, 0, OPC_FWAIT)
    DEF_ASM_OP1(fldcw, 0xd9, 5, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fnstcw, 0xd9, 7, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fstcw, 0xd9, 7, OPC_MODRM | OPC_FWAIT, OPT_EA )
    DEF_ASM_OP0(fnstsw, 0xdfe0)
ALT(DEF_ASM_OP1(fnstsw, 0xdfe0, 0, 0, OPT_EAX ))
ALT(DEF_ASM_OP1(fnstsw, 0xdd, 7, OPC_MODRM, OPT_EA ))
    DEF_ASM_OP1(fstsw, 0xdfe0, 0, OPC_FWAIT, OPT_EAX )
ALT(DEF_ASM_OP0L(fstsw, 0xdfe0, 0, OPC_FWAIT))
ALT(DEF_ASM_OP1(fstsw, 0xdd, 7, OPC_MODRM | OPC_FWAIT, OPT_EA ))
    DEF_ASM_OP0L(fclex, 0xdbe2, 0, OPC_FWAIT)
    DEF_ASM_OP1(fnstenv, 0xd9, 6, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fstenv, 0xd9, 6, OPC_MODRM | OPC_FWAIT, OPT_EA )
    DEF_ASM_OP1(fldenv, 0xd9, 4, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fnsave, 0xdd, 6, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fsave, 0xdd, 6, OPC_MODRM | OPC_FWAIT, OPT_EA )
    DEF_ASM_OP1(frstor, 0xdd, 4, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(ffree, 0xddc0, 4, OPC_REG, OPT_ST )
    DEF_ASM_OP1(ffreep, 0xdfc0, 4, OPC_REG, OPT_ST )
    DEF_ASM_OP1(fxsave, 0x0fae, 0, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fxrstor, 0x0fae, 1, OPC_MODRM, OPT_EA )

    
    DEF_ASM_OP2(arpl, 0x63, 0, OPC_MODRM, OPT_REG16, OPT_REG16 | OPT_EA)
    DEF_ASM_OP2(lar, 0x0f02, 0, OPC_MODRM, OPT_REG32 | OPT_EA, OPT_REG32)
    DEF_ASM_OP1(lgdt, 0x0f01, 2, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(lidt, 0x0f01, 3, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(lldt, 0x0f00, 2, OPC_MODRM, OPT_EA | OPT_REG)
    DEF_ASM_OP1(lmsw, 0x0f01, 6, OPC_MODRM, OPT_EA | OPT_REG)
ALT(DEF_ASM_OP2(lslw, 0x0f03, 0, OPC_MODRM | OPC_WL, OPT_EA | OPT_REG, OPT_REG))
    DEF_ASM_OP1(ltr, 0x0f00, 3, OPC_MODRM, OPT_EA | OPT_REG)
    DEF_ASM_OP1(sgdt, 0x0f01, 0, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(sidt, 0x0f01, 1, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(sldt, 0x0f00, 0, OPC_MODRM, OPT_REG | OPT_EA)
    DEF_ASM_OP1(smsw, 0x0f01, 4, OPC_MODRM, OPT_REG | OPT_EA)
    DEF_ASM_OP1(str, 0x0f00, 1, OPC_MODRM, OPT_REG16| OPT_EA)
    DEF_ASM_OP1(verr, 0x0f00, 4, OPC_MODRM, OPT_REG | OPT_EA)
    DEF_ASM_OP1(verw, 0x0f00, 5, OPC_MODRM, OPT_REG | OPT_EA)

    
    DEF_ASM_OP1(bswap, 0x0fc8, 0, OPC_REG, OPT_REG32 )
ALT(DEF_ASM_OP2(xaddb, 0x0fc0, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_REG | OPT_EA ))
ALT(DEF_ASM_OP2(cmpxchgb, 0x0fb0, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_REG | OPT_EA ))
    DEF_ASM_OP1(invlpg, 0x0f01, 7, OPC_MODRM, OPT_EA )

    DEF_ASM_OP2(boundl, 0x62, 0, OPC_MODRM, OPT_REG32, OPT_EA)
    DEF_ASM_OP2(boundw, 0x62, 0, OPC_MODRM | OPC_D16, OPT_REG16, OPT_EA)

    
    DEF_ASM_OP1(cmpxchg8b, 0x0fc7, 1, OPC_MODRM, OPT_EA )
    
    
    ALT(DEF_ASM_OP2(cmovo, 0x0f40, 0, OPC_MODRM | OPC_TEST, OPT_REG32 | OPT_EA, OPT_REG32))

    DEF_ASM_OP2(fcmovb, 0xdac0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmove, 0xdac8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovbe, 0xdad0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovu, 0xdad8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovnb, 0xdbc0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovne, 0xdbc8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovnbe, 0xdbd0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovnu, 0xdbd8, 0, OPC_REG, OPT_ST, OPT_ST0 )

    DEF_ASM_OP2(fucomi, 0xdbe8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcomi, 0xdbf0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fucomip, 0xdfe8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcomip, 0xdff0, 0, OPC_REG, OPT_ST, OPT_ST0 )

    
    DEF_ASM_OP0(emms, 0x0f77) 
    DEF_ASM_OP2(movd, 0x0f6e, 0, OPC_MODRM, OPT_EA | OPT_REG32, OPT_MMX )
ALT(DEF_ASM_OP2(movd, 0x0f7e, 0, OPC_MODRM, OPT_MMX, OPT_EA | OPT_REG32 ))
    DEF_ASM_OP2(movq, 0x0f6f, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(movq, 0x0f7f, 0, OPC_MODRM, OPT_MMX, OPT_EA | OPT_MMX ))
    DEF_ASM_OP2(packssdw, 0x0f6b, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(packsswb, 0x0f63, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(packuswb, 0x0f67, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddb, 0x0ffc, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddw, 0x0ffd, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddd, 0x0ffe, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddsb, 0x0fec, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddsw, 0x0fed, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddusb, 0x0fdc, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddusw, 0x0fdd, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pand, 0x0fdb, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pandn, 0x0fdf, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpeqb, 0x0f74, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpeqw, 0x0f75, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpeqd, 0x0f76, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpgtb, 0x0f64, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpgtw, 0x0f65, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpgtd, 0x0f66, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pmaddwd, 0x0ff5, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pmulhw, 0x0fe5, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pmullw, 0x0fd5, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(por, 0x0feb, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psllw, 0x0ff1, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psllw, 0x0f71, 6, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(pslld, 0x0ff2, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(pslld, 0x0f72, 6, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psllq, 0x0ff3, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psllq, 0x0f73, 6, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psraw, 0x0fe1, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psraw, 0x0f71, 4, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psrad, 0x0fe2, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psrad, 0x0f72, 4, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psrlw, 0x0fd1, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psrlw, 0x0f71, 2, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psrld, 0x0fd2, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psrld, 0x0f72, 2, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psrlq, 0x0fd3, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psrlq, 0x0f73, 2, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psubb, 0x0ff8, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubw, 0x0ff9, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubd, 0x0ffa, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubsb, 0x0fe8, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubsw, 0x0fe9, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubusb, 0x0fd8, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubusw, 0x0fd9, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpckhbw, 0x0f68, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpckhwd, 0x0f69, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpckhdq, 0x0f6a, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpcklbw, 0x0f60, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpcklwd, 0x0f61, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpckldq, 0x0f62, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pxor, 0x0fef, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )

#undef ALT
#undef DEF_ASM_OP0
#undef DEF_ASM_OP0L
#undef DEF_ASM_OP1
#undef DEF_ASM_OP2
#undef DEF_ASM_OP3

#define ALT(x)
#define DEF_ASM_OP0(name, opcode)
#define DEF_ASM_OP0L(name, opcode, group, instr_type) DEF_ASM(name)
#define DEF_ASM_OP1(name, opcode, group, instr_type, op0) DEF_ASM(name)
#define DEF_ASM_OP2(name, opcode, group, instr_type, op0, op1) DEF_ASM(name)
#define DEF_ASM_OP3(name, opcode, group, instr_type, op0, op1, op2) DEF_ASM(name)
     DEF_ASM_OP0(pusha, 0x60) 
     DEF_ASM_OP0(popa, 0x61)
     DEF_ASM_OP0(clc, 0xf8)
     DEF_ASM_OP0(cld, 0xfc)
     DEF_ASM_OP0(cli, 0xfa)
     DEF_ASM_OP0(clts, 0x0f06)
     DEF_ASM_OP0(cmc, 0xf5)
     DEF_ASM_OP0(lahf, 0x9f)
     DEF_ASM_OP0(sahf, 0x9e)
     DEF_ASM_OP0(pushfl, 0x9c)
     DEF_ASM_OP0(popfl, 0x9d)
     DEF_ASM_OP0(pushf, 0x9c)
     DEF_ASM_OP0(popf, 0x9d)
     DEF_ASM_OP0(stc, 0xf9)
     DEF_ASM_OP0(std, 0xfd)
     DEF_ASM_OP0(sti, 0xfb)
     DEF_ASM_OP0(aaa, 0x37)
     DEF_ASM_OP0(aas, 0x3f)
     DEF_ASM_OP0(daa, 0x27)
     DEF_ASM_OP0(das, 0x2f)
     DEF_ASM_OP0(aad, 0xd50a)
     DEF_ASM_OP0(aam, 0xd40a)
     DEF_ASM_OP0(cbw, 0x6698)
     DEF_ASM_OP0(cwd, 0x6699)
     DEF_ASM_OP0(cwde, 0x98)
     DEF_ASM_OP0(cdq, 0x99)
     DEF_ASM_OP0(cbtw, 0x6698)
     DEF_ASM_OP0(cwtl, 0x98)
     DEF_ASM_OP0(cwtd, 0x6699)
     DEF_ASM_OP0(cltd, 0x99)
     DEF_ASM_OP0(int3, 0xcc)
     DEF_ASM_OP0(into, 0xce)
     DEF_ASM_OP0(iret, 0xcf)
     DEF_ASM_OP0(rsm, 0x0faa)
     DEF_ASM_OP0(hlt, 0xf4)
     DEF_ASM_OP0(wait, 0x9b)
     DEF_ASM_OP0(nop, 0x90)
     DEF_ASM_OP0(xlat, 0xd7)

     
ALT(DEF_ASM_OP0L(cmpsb, 0xa6, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(scmpb, 0xa6, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(insb, 0x6c, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(outsb, 0x6e, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(lodsb, 0xac, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(slodb, 0xac, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(movsb, 0xa4, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(smovb, 0xa4, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(scasb, 0xae, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(sscab, 0xae, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(stosb, 0xaa, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(sstob, 0xaa, 0, OPC_BWL))

     
     
ALT(DEF_ASM_OP2(bsfw, 0x0fbc, 0, OPC_MODRM | OPC_WL, OPT_REGW | OPT_EA, OPT_REGW))
ALT(DEF_ASM_OP2(bsrw, 0x0fbd, 0, OPC_MODRM | OPC_WL, OPT_REGW | OPT_EA, OPT_REGW))

ALT(DEF_ASM_OP2(btw, 0x0fa3, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP2(btw, 0x0fba, 4, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW | OPT_EA))

ALT(DEF_ASM_OP2(btsw, 0x0fab, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP2(btsw, 0x0fba, 5, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW | OPT_EA))

ALT(DEF_ASM_OP2(btrw, 0x0fb3, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP2(btrw, 0x0fba, 6, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW | OPT_EA))

ALT(DEF_ASM_OP2(btcw, 0x0fbb, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP2(btcw, 0x0fba, 7, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW | OPT_EA))

     
     DEF_ASM_OP0(aword, 0x67)
     DEF_ASM_OP0(addr16, 0x67)
     DEF_ASM_OP0(word, 0x66)
     DEF_ASM_OP0(data16, 0x66)
     DEF_ASM_OP0(lock, 0xf0)
     DEF_ASM_OP0(rep, 0xf3)
     DEF_ASM_OP0(repe, 0xf3)
     DEF_ASM_OP0(repz, 0xf3)
     DEF_ASM_OP0(repne, 0xf2)
     DEF_ASM_OP0(repnz, 0xf2)
             
     DEF_ASM_OP0(invd, 0x0f08)
     DEF_ASM_OP0(wbinvd, 0x0f09)
     DEF_ASM_OP0(cpuid, 0x0fa2)
     DEF_ASM_OP0(wrmsr, 0x0f30)
     DEF_ASM_OP0(rdtsc, 0x0f31)
     DEF_ASM_OP0(rdmsr, 0x0f32)
     DEF_ASM_OP0(rdpmc, 0x0f33)
     DEF_ASM_OP0(ud2, 0x0f0b)

     
ALT(DEF_ASM_OP2(movb, 0xa0, 0, OPC_BWL, OPT_ADDR, OPT_EAX))
ALT(DEF_ASM_OP2(movb, 0xa2, 0, OPC_BWL, OPT_EAX, OPT_ADDR))
ALT(DEF_ASM_OP2(movb, 0x88, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(movb, 0x8a, 0, OPC_MODRM | OPC_BWL, OPT_EA | OPT_REG, OPT_REG))
ALT(DEF_ASM_OP2(movb, 0xb0, 0, OPC_REG | OPC_BWL, OPT_IM, OPT_REG))
ALT(DEF_ASM_OP2(movb, 0xc6, 0, OPC_MODRM | OPC_BWL, OPT_IM, OPT_REG | OPT_EA))

ALT(DEF_ASM_OP2(movw, 0x8c, 0, OPC_MODRM | OPC_WL, OPT_SEG, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(movw, 0x8e, 0, OPC_MODRM | OPC_WL, OPT_EA | OPT_REG, OPT_SEG))

ALT(DEF_ASM_OP2(movw, 0x0f20, 0, OPC_MODRM | OPC_WL, OPT_CR, OPT_REG32))
ALT(DEF_ASM_OP2(movw, 0x0f21, 0, OPC_MODRM | OPC_WL, OPT_DB, OPT_REG32))
ALT(DEF_ASM_OP2(movw, 0x0f24, 0, OPC_MODRM | OPC_WL, OPT_TR, OPT_REG32))
ALT(DEF_ASM_OP2(movw, 0x0f22, 0, OPC_MODRM | OPC_WL, OPT_REG32, OPT_CR))
ALT(DEF_ASM_OP2(movw, 0x0f23, 0, OPC_MODRM | OPC_WL, OPT_REG32, OPT_DB))
ALT(DEF_ASM_OP2(movw, 0x0f26, 0, OPC_MODRM | OPC_WL, OPT_REG32, OPT_TR))

ALT(DEF_ASM_OP2(movsbl, 0x0fbe, 0, OPC_MODRM, OPT_REG8 | OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(movsbw, 0x0fbe, 0, OPC_MODRM | OPC_D16, OPT_REG8 | OPT_EA, OPT_REG16))
ALT(DEF_ASM_OP2(movswl, 0x0fbf, 0, OPC_MODRM, OPT_REG16 | OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(movzbw, 0x0fb6, 0, OPC_MODRM | OPC_WL, OPT_REG8 | OPT_EA, OPT_REGW))
ALT(DEF_ASM_OP2(movzwl, 0x0fb7, 0, OPC_MODRM, OPT_REG16 | OPT_EA, OPT_REG32))

ALT(DEF_ASM_OP1(pushw, 0x50, 0, OPC_REG | OPC_WL, OPT_REGW))
ALT(DEF_ASM_OP1(pushw, 0xff, 6, OPC_MODRM | OPC_WL, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP1(pushw, 0x6a, 0, OPC_WL, OPT_IM8S))
ALT(DEF_ASM_OP1(pushw, 0x68, 0, OPC_WL, OPT_IM32))
ALT(DEF_ASM_OP1(pushw, 0x06, 0, OPC_WL, OPT_SEG))

ALT(DEF_ASM_OP1(popw, 0x58, 0, OPC_REG | OPC_WL, OPT_REGW))
ALT(DEF_ASM_OP1(popw, 0x8f, 0, OPC_MODRM | OPC_WL, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP1(popw, 0x07, 0, OPC_WL, OPT_SEG))

ALT(DEF_ASM_OP2(xchgw, 0x90, 0, OPC_REG | OPC_WL, OPT_REG, OPT_EAX))
ALT(DEF_ASM_OP2(xchgw, 0x90, 0, OPC_REG | OPC_WL, OPT_EAX, OPT_REG))
ALT(DEF_ASM_OP2(xchgb, 0x86, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(xchgb, 0x86, 0, OPC_MODRM | OPC_BWL, OPT_EA | OPT_REG, OPT_REG))

ALT(DEF_ASM_OP2(inb, 0xe4, 0, OPC_BWL, OPT_IM8, OPT_EAX))
ALT(DEF_ASM_OP1(inb, 0xe4, 0, OPC_BWL, OPT_IM8))
ALT(DEF_ASM_OP2(inb, 0xec, 0, OPC_BWL, OPT_DX, OPT_EAX))
ALT(DEF_ASM_OP1(inb, 0xec, 0, OPC_BWL, OPT_DX))

ALT(DEF_ASM_OP2(outb, 0xe6, 0, OPC_BWL, OPT_EAX, OPT_IM8))
ALT(DEF_ASM_OP1(outb, 0xe6, 0, OPC_BWL, OPT_IM8))
ALT(DEF_ASM_OP2(outb, 0xee, 0, OPC_BWL, OPT_EAX, OPT_DX))
ALT(DEF_ASM_OP1(outb, 0xee, 0, OPC_BWL, OPT_DX))

ALT(DEF_ASM_OP2(leaw, 0x8d, 0, OPC_MODRM | OPC_WL, OPT_EA, OPT_REG))

ALT(DEF_ASM_OP2(les, 0xc4, 0, OPC_MODRM, OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(lds, 0xc5, 0, OPC_MODRM, OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(lss, 0x0fb2, 0, OPC_MODRM, OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(lfs, 0x0fb4, 0, OPC_MODRM, OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(lgs, 0x0fb5, 0, OPC_MODRM, OPT_EA, OPT_REG32))

     
ALT(DEF_ASM_OP2(addb, 0x00, 0, OPC_ARITH | OPC_MODRM | OPC_BWL, OPT_REG, OPT_EA | OPT_REG)) 
ALT(DEF_ASM_OP2(addb, 0x02, 0, OPC_ARITH | OPC_MODRM | OPC_BWL, OPT_EA | OPT_REG, OPT_REG))
ALT(DEF_ASM_OP2(addb, 0x04, 0, OPC_ARITH | OPC_BWL, OPT_IM, OPT_EAX))
ALT(DEF_ASM_OP2(addb, 0x80, 0, OPC_ARITH | OPC_MODRM | OPC_BWL, OPT_IM, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(addw, 0x83, 0, OPC_ARITH | OPC_MODRM | OPC_WL, OPT_IM8S, OPT_EA | OPT_REG))

ALT(DEF_ASM_OP2(testb, 0x84, 0, OPC_MODRM | OPC_BWL, OPT_EA | OPT_REG, OPT_REG))
ALT(DEF_ASM_OP2(testb, 0x84, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(testb, 0xa8, 0, OPC_BWL, OPT_IM, OPT_EAX))
ALT(DEF_ASM_OP2(testb, 0xf6, 0, OPC_MODRM | OPC_BWL, OPT_IM, OPT_EA | OPT_REG))

ALT(DEF_ASM_OP1(incw, 0x40, 0, OPC_REG | OPC_WL, OPT_REGW))
ALT(DEF_ASM_OP1(incb, 0xfe, 0, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP1(decw, 0x48, 0, OPC_REG | OPC_WL, OPT_REGW))
ALT(DEF_ASM_OP1(decb, 0xfe, 1, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))

ALT(DEF_ASM_OP1(notb, 0xf6, 2, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP1(negb, 0xf6, 3, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))

ALT(DEF_ASM_OP1(mulb, 0xf6, 4, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP1(imulb, 0xf6, 5, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))

ALT(DEF_ASM_OP2(imulw, 0x0faf, 0, OPC_MODRM | OPC_WL, OPT_REG | OPT_EA, OPT_REG))
ALT(DEF_ASM_OP3(imulw, 0x6b, 0, OPC_MODRM | OPC_WL, OPT_IM8S, OPT_REGW | OPT_EA, OPT_REGW))
ALT(DEF_ASM_OP2(imulw, 0x6b, 0, OPC_MODRM | OPC_WL, OPT_IM8S, OPT_REGW))
ALT(DEF_ASM_OP3(imulw, 0x69, 0, OPC_MODRM | OPC_WL, OPT_IMW, OPT_REGW | OPT_EA, OPT_REGW))
ALT(DEF_ASM_OP2(imulw, 0x69, 0, OPC_MODRM | OPC_WL, OPT_IMW, OPT_REGW))

ALT(DEF_ASM_OP1(divb, 0xf6, 6, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP2(divb, 0xf6, 6, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA, OPT_EAX))
ALT(DEF_ASM_OP1(idivb, 0xf6, 7, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP2(idivb, 0xf6, 7, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA, OPT_EAX))

     
ALT(DEF_ASM_OP2(rolb, 0xc0, 0, OPC_MODRM | OPC_BWL | OPC_SHIFT, OPT_IM8, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(rolb, 0xd2, 0, OPC_MODRM | OPC_BWL | OPC_SHIFT, OPT_CL, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP1(rolb, 0xd0, 0, OPC_MODRM | OPC_BWL | OPC_SHIFT, OPT_EA | OPT_REG))

ALT(DEF_ASM_OP3(shldw, 0x0fa4, 0, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP3(shldw, 0x0fa5, 0, OPC_MODRM | OPC_WL, OPT_CL, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP2(shldw, 0x0fa5, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP3(shrdw, 0x0fac, 0, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP3(shrdw, 0x0fad, 0, OPC_MODRM | OPC_WL, OPT_CL, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP2(shrdw, 0x0fad, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_EA | OPT_REGW))

ALT(DEF_ASM_OP1(call, 0xff, 2, OPC_MODRM, OPT_INDIR))
ALT(DEF_ASM_OP1(call, 0xe8, 0, OPC_JMP, OPT_ADDR))
ALT(DEF_ASM_OP1(jmp, 0xff, 4, OPC_MODRM, OPT_INDIR))
ALT(DEF_ASM_OP1(jmp, 0xeb, 0, OPC_SHORTJMP | OPC_JMP, OPT_ADDR))

ALT(DEF_ASM_OP2(lcall, 0x9a, 0, 0, OPT_IM16, OPT_IM32))
ALT(DEF_ASM_OP1(lcall, 0xff, 3, 0, OPT_EA))
ALT(DEF_ASM_OP2(ljmp, 0xea, 0, 0, OPT_IM16, OPT_IM32))
ALT(DEF_ASM_OP1(ljmp, 0xff, 5, 0, OPT_EA))

ALT(DEF_ASM_OP1(int, 0xcd, 0, 0, OPT_IM8))
ALT(DEF_ASM_OP1(seto, 0x0f90, 0, OPC_MODRM | OPC_TEST, OPT_REG8 | OPT_EA))
    DEF_ASM_OP2(enter, 0xc8, 0, 0, OPT_IM16, OPT_IM8)
    DEF_ASM_OP0(leave, 0xc9)
    DEF_ASM_OP0(ret, 0xc3)
ALT(DEF_ASM_OP1(ret, 0xc2, 0, 0, OPT_IM16))
    DEF_ASM_OP0(lret, 0xcb)
ALT(DEF_ASM_OP1(lret, 0xca, 0, 0, OPT_IM16))

ALT(DEF_ASM_OP1(jo, 0x70, 0, OPC_SHORTJMP | OPC_JMP | OPC_TEST, OPT_ADDR))
    DEF_ASM_OP1(loopne, 0xe0, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(loopnz, 0xe0, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(loope, 0xe1, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(loopz, 0xe1, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(loop, 0xe2, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(jecxz, 0xe3, 0, OPC_SHORTJMP, OPT_ADDR)
     
     
     
ALT(DEF_ASM_OP0L(fcomp, 0xd8d9, 0, 0))

ALT(DEF_ASM_OP1(fadd, 0xd8c0, 0, OPC_FARITH | OPC_REG, OPT_ST))
ALT(DEF_ASM_OP2(fadd, 0xd8c0, 0, OPC_FARITH | OPC_REG, OPT_ST, OPT_ST0))
ALT(DEF_ASM_OP0L(fadd, 0xdec1, 0, OPC_FARITH))
ALT(DEF_ASM_OP1(faddp, 0xdec0, 0, OPC_FARITH | OPC_REG, OPT_ST))
ALT(DEF_ASM_OP2(faddp, 0xdec0, 0, OPC_FARITH | OPC_REG, OPT_ST, OPT_ST0))
ALT(DEF_ASM_OP2(faddp, 0xdec0, 0, OPC_FARITH | OPC_REG, OPT_ST0, OPT_ST))
ALT(DEF_ASM_OP0L(faddp, 0xdec1, 0, OPC_FARITH))
ALT(DEF_ASM_OP1(fadds, 0xd8, 0, OPC_FARITH | OPC_MODRM, OPT_EA))
ALT(DEF_ASM_OP1(fiaddl, 0xda, 0, OPC_FARITH | OPC_MODRM, OPT_EA))
ALT(DEF_ASM_OP1(faddl, 0xdc, 0, OPC_FARITH | OPC_MODRM, OPT_EA))
ALT(DEF_ASM_OP1(fiadds, 0xde, 0, OPC_FARITH | OPC_MODRM, OPT_EA))

     DEF_ASM_OP0(fucompp, 0xdae9)
     DEF_ASM_OP0(ftst, 0xd9e4)
     DEF_ASM_OP0(fxam, 0xd9e5)
     DEF_ASM_OP0(fld1, 0xd9e8)
     DEF_ASM_OP0(fldl2t, 0xd9e9)
     DEF_ASM_OP0(fldl2e, 0xd9ea)
     DEF_ASM_OP0(fldpi, 0xd9eb)
     DEF_ASM_OP0(fldlg2, 0xd9ec)
     DEF_ASM_OP0(fldln2, 0xd9ed)
     DEF_ASM_OP0(fldz, 0xd9ee)

     DEF_ASM_OP0(f2xm1, 0xd9f0)
     DEF_ASM_OP0(fyl2x, 0xd9f1)
     DEF_ASM_OP0(fptan, 0xd9f2)
     DEF_ASM_OP0(fpatan, 0xd9f3)
     DEF_ASM_OP0(fxtract, 0xd9f4)
     DEF_ASM_OP0(fprem1, 0xd9f5)
     DEF_ASM_OP0(fdecstp, 0xd9f6)
     DEF_ASM_OP0(fincstp, 0xd9f7)
     DEF_ASM_OP0(fprem, 0xd9f8)
     DEF_ASM_OP0(fyl2xp1, 0xd9f9)
     DEF_ASM_OP0(fsqrt, 0xd9fa)
     DEF_ASM_OP0(fsincos, 0xd9fb)
     DEF_ASM_OP0(frndint, 0xd9fc)
     DEF_ASM_OP0(fscale, 0xd9fd)
     DEF_ASM_OP0(fsin, 0xd9fe)
     DEF_ASM_OP0(fcos, 0xd9ff)
     DEF_ASM_OP0(fchs, 0xd9e0)
     DEF_ASM_OP0(fabs, 0xd9e1)
     DEF_ASM_OP0(fninit, 0xdbe3)
     DEF_ASM_OP0(fnclex, 0xdbe2)
     DEF_ASM_OP0(fnop, 0xd9d0)
     DEF_ASM_OP0(fwait, 0x9b)

    
    DEF_ASM_OP1(fld, 0xd9c0, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(fldl, 0xd9c0, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(flds, 0xd9, 0, OPC_MODRM, OPT_EA)
ALT(DEF_ASM_OP1(fldl, 0xdd, 0, OPC_MODRM, OPT_EA))
    DEF_ASM_OP1(fildl, 0xdb, 0, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fildq, 0xdf, 5, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fildll, 0xdf, 5, OPC_MODRM,OPT_EA)
    DEF_ASM_OP1(fldt, 0xdb, 5, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fbld, 0xdf, 4, OPC_MODRM, OPT_EA)
    
    
    DEF_ASM_OP1(fst, 0xddd0, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(fstl, 0xddd0, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(fsts, 0xd9, 2, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fstps, 0xd9, 3, OPC_MODRM, OPT_EA)
ALT(DEF_ASM_OP1(fstl, 0xdd, 2, OPC_MODRM, OPT_EA))
    DEF_ASM_OP1(fstpl, 0xdd, 3, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fist, 0xdf, 2, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fistp, 0xdf, 3, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fistl, 0xdb, 2, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fistpl, 0xdb, 3, OPC_MODRM, OPT_EA)

    DEF_ASM_OP1(fstp, 0xddd8, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(fistpq, 0xdf, 7, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fistpll, 0xdf, 7, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fstpt, 0xdb, 7, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fbstp, 0xdf, 6, OPC_MODRM, OPT_EA)

    
    DEF_ASM_OP0(fxch, 0xd9c9)
ALT(DEF_ASM_OP1(fxch, 0xd9c8, 0, OPC_REG, OPT_ST))

    
    DEF_ASM_OP1(fucom, 0xdde0, 0, OPC_REG, OPT_ST )
    DEF_ASM_OP1(fucomp, 0xdde8, 0, OPC_REG, OPT_ST )

    DEF_ASM_OP0L(finit, 0xdbe3, 0, OPC_FWAIT)
    DEF_ASM_OP1(fldcw, 0xd9, 5, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fnstcw, 0xd9, 7, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fstcw, 0xd9, 7, OPC_MODRM | OPC_FWAIT, OPT_EA )
    DEF_ASM_OP0(fnstsw, 0xdfe0)
ALT(DEF_ASM_OP1(fnstsw, 0xdfe0, 0, 0, OPT_EAX ))
ALT(DEF_ASM_OP1(fnstsw, 0xdd, 7, OPC_MODRM, OPT_EA ))
    DEF_ASM_OP1(fstsw, 0xdfe0, 0, OPC_FWAIT, OPT_EAX )
ALT(DEF_ASM_OP0L(fstsw, 0xdfe0, 0, OPC_FWAIT))
ALT(DEF_ASM_OP1(fstsw, 0xdd, 7, OPC_MODRM | OPC_FWAIT, OPT_EA ))
    DEF_ASM_OP0L(fclex, 0xdbe2, 0, OPC_FWAIT)
    DEF_ASM_OP1(fnstenv, 0xd9, 6, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fstenv, 0xd9, 6, OPC_MODRM | OPC_FWAIT, OPT_EA )
    DEF_ASM_OP1(fldenv, 0xd9, 4, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fnsave, 0xdd, 6, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fsave, 0xdd, 6, OPC_MODRM | OPC_FWAIT, OPT_EA )
    DEF_ASM_OP1(frstor, 0xdd, 4, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(ffree, 0xddc0, 4, OPC_REG, OPT_ST )
    DEF_ASM_OP1(ffreep, 0xdfc0, 4, OPC_REG, OPT_ST )
    DEF_ASM_OP1(fxsave, 0x0fae, 0, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fxrstor, 0x0fae, 1, OPC_MODRM, OPT_EA )

    
    DEF_ASM_OP2(arpl, 0x63, 0, OPC_MODRM, OPT_REG16, OPT_REG16 | OPT_EA)
    DEF_ASM_OP2(lar, 0x0f02, 0, OPC_MODRM, OPT_REG32 | OPT_EA, OPT_REG32)
    DEF_ASM_OP1(lgdt, 0x0f01, 2, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(lidt, 0x0f01, 3, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(lldt, 0x0f00, 2, OPC_MODRM, OPT_EA | OPT_REG)
    DEF_ASM_OP1(lmsw, 0x0f01, 6, OPC_MODRM, OPT_EA | OPT_REG)
ALT(DEF_ASM_OP2(lslw, 0x0f03, 0, OPC_MODRM | OPC_WL, OPT_EA | OPT_REG, OPT_REG))
    DEF_ASM_OP1(ltr, 0x0f00, 3, OPC_MODRM, OPT_EA | OPT_REG)
    DEF_ASM_OP1(sgdt, 0x0f01, 0, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(sidt, 0x0f01, 1, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(sldt, 0x0f00, 0, OPC_MODRM, OPT_REG | OPT_EA)
    DEF_ASM_OP1(smsw, 0x0f01, 4, OPC_MODRM, OPT_REG | OPT_EA)
    DEF_ASM_OP1(str, 0x0f00, 1, OPC_MODRM, OPT_REG16| OPT_EA)
    DEF_ASM_OP1(verr, 0x0f00, 4, OPC_MODRM, OPT_REG | OPT_EA)
    DEF_ASM_OP1(verw, 0x0f00, 5, OPC_MODRM, OPT_REG | OPT_EA)

    
    DEF_ASM_OP1(bswap, 0x0fc8, 0, OPC_REG, OPT_REG32 )
ALT(DEF_ASM_OP2(xaddb, 0x0fc0, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_REG | OPT_EA ))
ALT(DEF_ASM_OP2(cmpxchgb, 0x0fb0, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_REG | OPT_EA ))
    DEF_ASM_OP1(invlpg, 0x0f01, 7, OPC_MODRM, OPT_EA )

    DEF_ASM_OP2(boundl, 0x62, 0, OPC_MODRM, OPT_REG32, OPT_EA)
    DEF_ASM_OP2(boundw, 0x62, 0, OPC_MODRM | OPC_D16, OPT_REG16, OPT_EA)

    
    DEF_ASM_OP1(cmpxchg8b, 0x0fc7, 1, OPC_MODRM, OPT_EA )
    
    
    ALT(DEF_ASM_OP2(cmovo, 0x0f40, 0, OPC_MODRM | OPC_TEST, OPT_REG32 | OPT_EA, OPT_REG32))

    DEF_ASM_OP2(fcmovb, 0xdac0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmove, 0xdac8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovbe, 0xdad0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovu, 0xdad8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovnb, 0xdbc0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovne, 0xdbc8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovnbe, 0xdbd0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovnu, 0xdbd8, 0, OPC_REG, OPT_ST, OPT_ST0 )

    DEF_ASM_OP2(fucomi, 0xdbe8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcomi, 0xdbf0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fucomip, 0xdfe8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcomip, 0xdff0, 0, OPC_REG, OPT_ST, OPT_ST0 )

    
    DEF_ASM_OP0(emms, 0x0f77) 
    DEF_ASM_OP2(movd, 0x0f6e, 0, OPC_MODRM, OPT_EA | OPT_REG32, OPT_MMX )
ALT(DEF_ASM_OP2(movd, 0x0f7e, 0, OPC_MODRM, OPT_MMX, OPT_EA | OPT_REG32 ))
    DEF_ASM_OP2(movq, 0x0f6f, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(movq, 0x0f7f, 0, OPC_MODRM, OPT_MMX, OPT_EA | OPT_MMX ))
    DEF_ASM_OP2(packssdw, 0x0f6b, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(packsswb, 0x0f63, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(packuswb, 0x0f67, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddb, 0x0ffc, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddw, 0x0ffd, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddd, 0x0ffe, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddsb, 0x0fec, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddsw, 0x0fed, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddusb, 0x0fdc, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddusw, 0x0fdd, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pand, 0x0fdb, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pandn, 0x0fdf, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpeqb, 0x0f74, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpeqw, 0x0f75, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpeqd, 0x0f76, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpgtb, 0x0f64, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpgtw, 0x0f65, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpgtd, 0x0f66, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pmaddwd, 0x0ff5, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pmulhw, 0x0fe5, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pmullw, 0x0fd5, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(por, 0x0feb, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psllw, 0x0ff1, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psllw, 0x0f71, 6, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(pslld, 0x0ff2, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(pslld, 0x0f72, 6, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psllq, 0x0ff3, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psllq, 0x0f73, 6, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psraw, 0x0fe1, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psraw, 0x0f71, 4, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psrad, 0x0fe2, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psrad, 0x0f72, 4, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psrlw, 0x0fd1, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psrlw, 0x0f71, 2, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psrld, 0x0fd2, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psrld, 0x0f72, 2, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psrlq, 0x0fd3, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psrlq, 0x0f73, 2, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psubb, 0x0ff8, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubw, 0x0ff9, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubd, 0x0ffa, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubsb, 0x0fe8, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubsw, 0x0fe9, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubusb, 0x0fd8, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubusw, 0x0fd9, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpckhbw, 0x0f68, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpckhwd, 0x0f69, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpckhdq, 0x0f6a, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpcklbw, 0x0f60, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpcklwd, 0x0f61, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpckldq, 0x0f62, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pxor, 0x0fef, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )

#undef ALT
#undef DEF_ASM_OP0
#undef DEF_ASM_OP0L
#undef DEF_ASM_OP1
#undef DEF_ASM_OP2
#undef DEF_ASM_OP3

#endif
#undef DEF
;

#define TOK_UIDENT TOK_DEFINE

#ifdef WIN32
int __stdcall GetModuleFileNameA(void *, char *, int);
void *__stdcall GetProcAddress(void *, const char *);
void *__stdcall GetModuleHandleA(const char *);
void *__stdcall LoadLibraryA(const char *);
int __stdcall FreeConsole(void);

#define snprintf _snprintf
#define vsnprintf _vsnprintf
#ifndef __GNUC__
  #define strtold (long double)strtod
  #define strtof (float)strtod
  #define strtoll (long long)strtol
#endif
#elif defined(TCC_UCLIBC) || defined(__FreeBSD__)
long double strtold(const char *nptr, char **endptr)
{
    return (long double)strtod(nptr, endptr);
}
float strtof(const char *nptr, char **endptr)
{
    return (float)strtod(nptr, endptr);
}
#else
extern float strtof (const char *__nptr, char **__endptr);
extern long double strtold (const char *__nptr, char **__endptr);
#endif

static char *pstrcpy(char *buf, int buf_size, const char *s);
static char *pstrcat(char *buf, int buf_size, const char *s);
static const char *tcc_basename(const char *name);

static void next(void);
static void next_nomacro(void);
static void parse_expr_type(CType *type);
static void expr_type(CType *type);
static void unary_type(CType *type);
static void block(int *bsym, int *csym, int *case_sym, int *def_sym, 
                  int case_reg, int is_expr);
static int expr_const(void);
static void expr_eq(void);
static void gexpr(void);
static void gen_inline_functions(void);
static void decl(int l);
static void decl_initializer(CType *type, Section *sec, unsigned long c, 
                             int first, int size_only);
static void decl_initializer_alloc(CType *type, AttributeDef *ad, int r, 
                                   int has_init, int v, int scope);
int gv(int rc);
void gv2(int rc1, int rc2);
void move_reg(int r, int s);
void save_regs(int n);
void save_reg(int r);
void vpop(void);
void vswap(void);
void vdup(void);
int get_reg(int rc);
int get_reg_ex(int rc,int rc2);

struct macro_level {
    struct macro_level *prev;
    int *p;
};

static void macro_subst(TokenString *tok_str, Sym **nested_list, 
                        const int *macro_str, struct macro_level **can_read_stream);
void gen_op(int op);
void force_charshort_cast(int t);
static void gen_cast(CType *type);
void vstore(void);
static Sym *sym_find(int v);
static Sym *sym_push(int v, CType *type, int r, int c);

static int type_size(CType *type, int *a);
static inline CType *pointed_type(CType *type);
static int pointed_size(CType *type);
static int lvalue_type(int t);
static int parse_btype(CType *type, AttributeDef *ad);
static void type_decl(CType *type, AttributeDef *ad, int *v, int td);
static int is_compatible_types(CType *type1, CType *type2);

int ieee_finite(double d);
void error(const char *fmt, ...);
void vpushi(int v);
void vrott(int n);
void vnrott(int n);
void lexpand_nr(void);
static void vpush_global_sym(CType *type, int v);
void vset(CType *type, int r, int v);
void type_to_str(char *buf, int buf_size, 
                 CType *type, const char *varstr);
char *get_tok_str(int v, CValue *cv);
static Sym *get_sym_ref(CType *type, Section *sec, 
                        unsigned long offset, unsigned long size);
static Sym *external_global_sym(int v, CType *type, int r);

static void section_realloc(Section *sec, unsigned long new_size);
static void *section_ptr_add(Section *sec, unsigned long size);
static void put_extern_sym(Sym *sym, Section *section, 
                           unsigned long value, unsigned long size);
static void greloc(Section *s, Sym *sym, unsigned long addr, int type);
static int put_elf_str(Section *s, const char *sym);
static int put_elf_sym(Section *s, 
                       unsigned long value, unsigned long size,
                       int info, int other, int shndx, const char *name);
static int add_elf_sym(Section *s, unsigned long value, unsigned long size,
                       int info, int other, int sh_num, const char *name);
static void put_elf_reloc(Section *symtab, Section *s, unsigned long offset,
                          int type, int symbol);
static void put_stabs(const char *str, int type, int other, int desc, 
                      unsigned long value);
static void put_stabs_r(const char *str, int type, int other, int desc, 
                        unsigned long value, Section *sec, int sym_index);
static void put_stabn(int type, int other, int desc, int value);
static void put_stabd(int type, int other, int desc);
static int tcc_add_dll(TCCState *s, const char *filename, int flags);

#define AFF_PRINT_ERROR     0x0001 
#define AFF_REFERENCED_DLL  0x0002 
static int tcc_add_file_internal(TCCState *s, const char *filename, int flags);

int tcc_output_coff(TCCState *s1, FILE *f);

void *resolve_sym(TCCState *s1, const char *sym, int type);
int pe_load_def_file(struct TCCState *s1, FILE *fp);
void pe_setup_paths(struct TCCState *s1, int *p_output_type, const char **p_outfile, char *first_file);
unsigned long pe_add_runtime(struct TCCState *s1);
int tcc_output_pe(struct TCCState *s1, const char *filename);


#ifdef CONFIG_TCC_ASM

typedef struct ExprValue {
    uint32_t v;
    Sym *sym;
} ExprValue;

#define MAX_ASM_OPERANDS 30

typedef struct ASMOperand {
    int id; 
    char *constraint;
    char asm_str[16]; 
    SValue *vt; 
    int ref_index; 
    int input_index; 
    int priority; 
    int reg; 
    int is_llong; 
    int is_memory; 
    int is_rw;     
} ASMOperand;

static void asm_expr(TCCState *s1, ExprValue *pe);
static int asm_int_expr(TCCState *s1);
static int find_constraint(ASMOperand *operands, int nb_operands, 
                           const char *name, const char **pp);

static int tcc_assemble(TCCState *s1, int do_preprocess);

#endif

static void asm_instr(void);
static void asm_global_instr(void);

static inline int is_float(int t)
{
    int bt;
    bt = t & VT_BTYPE;
    return bt == VT_LDOUBLE || bt == VT_DOUBLE || bt == VT_FLOAT;
}

#ifdef TCC_TARGET_I386
/*
 *  X86 code generator for TCC
 * 
 *  Copyright (c) 2001-2004 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define NB_REGS             4

#define RC_INT     0x0001 
#define RC_FLOAT   0x0002 
#define RC_EAX     0x0004
#define RC_ST0     0x0008 
#define RC_ECX     0x0010
#define RC_EDX     0x0020
#define RC_IRET    RC_EAX 
#define RC_LRET    RC_EDX 
#define RC_FRET    RC_ST0 

enum {
    TREG_EAX = 0,
    TREG_ECX,
    TREG_EDX,
    TREG_ST0,
};

int reg_classes[NB_REGS] = {
     RC_INT | RC_EAX,
     RC_INT | RC_ECX,
     RC_INT | RC_EDX,
     RC_FLOAT | RC_ST0,
};

#define REG_IRET TREG_EAX 
#define REG_LRET TREG_EDX 
#define REG_FRET TREG_ST0 

#define INVERT_FUNC_PARAMS


#define PTR_SIZE 4

#define LDOUBLE_SIZE  12
#define LDOUBLE_ALIGN 4
#define MAX_ALIGN     8


#define EM_TCC_TARGET EM_386

#define R_DATA_32   R_386_32
#define R_JMP_SLOT  R_386_JMP_SLOT
#define R_COPY      R_386_COPY

#define ELF_START_ADDR 0x08048000
#define ELF_PAGE_SIZE  0x1000


static unsigned long func_sub_sp_offset;
static unsigned long func_bound_offset;
static int func_ret_sub;

void g(int c)
{
    int ind1;
    ind1 = ind + 1;
    if (ind1 > cur_text_section->data_allocated)
        section_realloc(cur_text_section, ind1);
    cur_text_section->data[ind] = c;
    ind = ind1;
}

void o(unsigned int c)
{
    while (c) {
        g(c);
        c = c >> 8;
    }
}

void gen_le32(int c)
{
    g(c);
    g(c >> 8);
    g(c >> 16);
    g(c >> 24);
}

void gsym_addr(int t, int a)
{
    int n, *ptr;
    while (t) {
        ptr = (int *)(cur_text_section->data + t);
        n = *ptr; 
        *ptr = a - t - 4;
        t = n;
    }
}

void gsym(int t)
{
    gsym_addr(t, ind);
}

#define psym oad

static int oad(int c, int s)
{
    int ind1;

    o(c);
    ind1 = ind + 4;
    if (ind1 > cur_text_section->data_allocated)
        section_realloc(cur_text_section, ind1);
    *(int *)(cur_text_section->data + ind) = s;
    s = ind;
    ind = ind1;
    return s;
}

static void gen_addr32(int r, Sym *sym, int c)
{
    if (r & VT_SYM)
        greloc(cur_text_section, sym, ind, R_386_32);
    gen_le32(c);
}

static void gen_modrm(int op_reg, int r, Sym *sym, int c)
{
    op_reg = op_reg << 3;
    if ((r & VT_VALMASK) == VT_CONST) {
        
        o(0x05 | op_reg);
        gen_addr32(r, sym, c);
    } else if ((r & VT_VALMASK) == VT_LOCAL) {
        
        if (c == (char)c) {
            
            o(0x45 | op_reg);
            g(c);
        } else {
            oad(0x85 | op_reg, c);
        }
    } else {
        g(0x00 | op_reg | (r & VT_VALMASK));
    }
}


void load(int r, SValue *sv)
{
    int v, t, ft, fc, fr;
    SValue v1;

    fr = sv->r;
    ft = sv->type.t;
    fc = sv->c.ul;

    v = fr & VT_VALMASK;
    if (fr & VT_LVAL) {
        if (v == VT_LLOCAL) {
            v1.type.t = VT_INT;
            v1.r = VT_LOCAL | VT_LVAL;
            v1.c.ul = fc;
            load(r, &v1);
            fr = r;
        }
        if ((ft & VT_BTYPE) == VT_FLOAT) {
            o(0xd9); 
            r = 0;
        } else if ((ft & VT_BTYPE) == VT_DOUBLE) {
            o(0xdd); 
            r = 0;
        } else if ((ft & VT_BTYPE) == VT_LDOUBLE) {
            o(0xdb); 
            r = 5;
        } else if ((ft & VT_TYPE) == VT_BYTE) {
            o(0xbe0f);   
        } else if ((ft & VT_TYPE) == (VT_BYTE | VT_UNSIGNED)) {
            o(0xb60f);   
        } else if ((ft & VT_TYPE) == VT_SHORT) {
            o(0xbf0f);   
        } else if ((ft & VT_TYPE) == (VT_SHORT | VT_UNSIGNED)) {
            o(0xb70f);   
        } else {
            o(0x8b);     
        }
        gen_modrm(r, fr, sv->sym, fc);
    } else {
        if (v == VT_CONST) {
            o(0xb8 + r); 
            gen_addr32(fr, sv->sym, fc);
        } else if (v == VT_LOCAL) {
            o(0x8d); 
            gen_modrm(r, VT_LOCAL, sv->sym, fc);
        } else if (v == VT_CMP) {
            oad(0xb8 + r, 0); 
            o(0x0f); 
            o(fc);
            o(0xc0 + r);
        } else if (v == VT_JMP || v == VT_JMPI) {
            t = v & 1;
            oad(0xb8 + r, t); 
            o(0x05eb); 
            gsym(fc);
            oad(0xb8 + r, t ^ 1); 
        } else if (v != r) {
            o(0x89);
            o(0xc0 + r + v * 8); 
        }
    }
}

void store(int r, SValue *v)
{
    int fr, bt, ft, fc;

    ft = v->type.t;
    fc = v->c.ul;
    fr = v->r & VT_VALMASK;
    bt = ft & VT_BTYPE;
    
    if (bt == VT_FLOAT) {
        o(0xd9); 
        r = 2;
    } else if (bt == VT_DOUBLE) {
        o(0xdd); 
        r = 2;
    } else if (bt == VT_LDOUBLE) {
        o(0xc0d9); 
        o(0xdb); 
        r = 7;
    } else {
        if (bt == VT_SHORT)
            o(0x66);
        if (bt == VT_BYTE || bt == VT_BOOL)
            o(0x88);
        else
            o(0x89);
    }
    if (fr == VT_CONST ||
        fr == VT_LOCAL ||
        (v->r & VT_LVAL)) {
        gen_modrm(r, v->r, v->sym, fc);
    } else if (fr != r) {
        o(0xc0 + fr + r * 8); 
    }
}

static void gadd_sp(int val)
{
    if (val == (char)val) {
        o(0xc483);
        g(val);
    } else {
        oad(0xc481, val); 
    }
}

static void gcall_or_jmp(int is_jmp)
{
    int r;
    if ((vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST) {
        
        if (vtop->r & VT_SYM) {
            
            greloc(cur_text_section, vtop->sym, 
                   ind + 1, R_386_PC32);
        } else {
            
            put_elf_reloc(symtab_section, cur_text_section, 
                          ind + 1, R_386_PC32, 0);
        }
        oad(0xe8 + is_jmp, vtop->c.ul - 4); 
    } else {
        
        r = gv(RC_INT);
        o(0xff); 
        o(0xd0 + r + (is_jmp << 4));
    }
}

static uint8_t fastcall_regs[3] = { TREG_EAX, TREG_EDX, TREG_ECX };

void gfunc_call(int nb_args)
{
    int size, align, r, args_size, i, func_call;
    Sym *func_sym;
    
    args_size = 0;
    for(i = 0;i < nb_args; i++) {
        if ((vtop->type.t & VT_BTYPE) == VT_STRUCT) {
            size = type_size(&vtop->type, &align);
            
            size = (size + 3) & ~3;
            
            oad(0xec81, size); 
            
            r = get_reg(RC_INT);
            o(0x89); 
            o(0xe0 + r);
            vset(&vtop->type, r | VT_LVAL, 0);
            vswap();
            vstore();
            args_size += size;
        } else if (is_float(vtop->type.t)) {
            gv(RC_FLOAT); 
            if ((vtop->type.t & VT_BTYPE) == VT_FLOAT)
                size = 4;
            else if ((vtop->type.t & VT_BTYPE) == VT_DOUBLE)
                size = 8;
            else
                size = 12;
            oad(0xec81, size); 
            if (size == 12)
                o(0x7cdb);
            else
                o(0x5cd9 + size - 4); 
            g(0x24);
            g(0x00);
            args_size += size;
        } else {
            
            
            r = gv(RC_INT);
            if ((vtop->type.t & VT_BTYPE) == VT_LLONG) {
                size = 8;
                o(0x50 + vtop->r2); 
            } else {
                size = 4;
            }
            o(0x50 + r); 
            args_size += size;
        }
        vtop--;
    }
    save_regs(0); 
    func_sym = vtop->type.ref;
    func_call = func_sym->r;
    
    if (func_call >= FUNC_FASTCALL1 && func_call <= FUNC_FASTCALL3) {
        int fastcall_nb_regs;
        fastcall_nb_regs = func_call - FUNC_FASTCALL1 + 1;
        for(i = 0;i < fastcall_nb_regs; i++) {
            if (args_size <= 0)
                break;
            o(0x58 + fastcall_regs[i]); 
            
            args_size -= 4;
        }
    }
    gcall_or_jmp(0);
    if (args_size && func_sym->r != FUNC_STDCALL)
        gadd_sp(args_size);
    vtop--;
}

#ifdef TCC_TARGET_PE
#define FUNC_PROLOG_SIZE 10
#else
#define FUNC_PROLOG_SIZE 9
#endif

void gfunc_prolog(CType *func_type)
{
    int addr, align, size, func_call, fastcall_nb_regs;
    int param_index, param_addr;
    Sym *sym;
    CType *type;

    sym = func_type->ref;
    func_call = sym->r;
    addr = 8;
    loc = 0;
    if (func_call >= FUNC_FASTCALL1 && func_call <= FUNC_FASTCALL3) {
        fastcall_nb_regs = func_call - FUNC_FASTCALL1 + 1;
    } else {
        fastcall_nb_regs = 0;
    }
    param_index = 0;

    ind += FUNC_PROLOG_SIZE;
    func_sub_sp_offset = ind;
    func_vt = sym->type;
    if ((func_vt.t & VT_BTYPE) == VT_STRUCT) {
        
        func_vc = addr;
        addr += 4;
        param_index++;
    }
    
    while ((sym = sym->next) != NULL) {
        type = &sym->type;
        size = type_size(type, &align);
        size = (size + 3) & ~3;
#ifdef FUNC_STRUCT_PARAM_AS_PTR
        
        if ((type->t & VT_BTYPE) == VT_STRUCT) {
            size = 4;
        }
#endif
        if (param_index < fastcall_nb_regs) {
            
            loc -= 4;
            o(0x89);     
            gen_modrm(fastcall_regs[param_index], VT_LOCAL, NULL, loc);
            param_addr = loc;
        } else {
            param_addr = addr;
            addr += size;
        }
        sym_push(sym->v & ~SYM_FIELD, type,
                 VT_LOCAL | VT_LVAL, param_addr);
        param_index++;
    }
    func_ret_sub = 0;
    
    if (func_call == FUNC_STDCALL)
        func_ret_sub = addr - 8;

    
    if (do_bounds_check) {
        oad(0xb8, 0); 
        oad(0xb8, 0); 
        func_bound_offset = lbounds_section->data_offset;
    }
}

void gfunc_epilog(void)
{
    int v, saved_ind;

#ifdef CONFIG_TCC_BCHECK
    if (do_bounds_check && func_bound_offset != lbounds_section->data_offset) {
        int saved_ind;
        int *bounds_ptr;
        Sym *sym, *sym_data;
        
        bounds_ptr = section_ptr_add(lbounds_section, sizeof(int));
        *bounds_ptr = 0;
        
        saved_ind = ind;
        ind = func_sub_sp_offset;
        sym_data = get_sym_ref(&char_pointer_type, lbounds_section, 
                               func_bound_offset, lbounds_section->data_offset);
        greloc(cur_text_section, sym_data,
               ind + 1, R_386_32);
        oad(0xb8, 0); 
        sym = external_global_sym(TOK___bound_local_new, &func_old_type, 0);
        greloc(cur_text_section, sym, 
               ind + 1, R_386_PC32);
        oad(0xe8, -4);
        ind = saved_ind;
        
        o(0x5250); 
        greloc(cur_text_section, sym_data,
               ind + 1, R_386_32);
        oad(0xb8, 0); 
        sym = external_global_sym(TOK___bound_local_delete, &func_old_type, 0);
        greloc(cur_text_section, sym, 
               ind + 1, R_386_PC32);
        oad(0xe8, -4);
        o(0x585a); 
    }
#endif
    o(0xc9); 
    if (func_ret_sub == 0) {
        o(0xc3); 
    } else {
        o(0xc2); 
        g(func_ret_sub);
        g(func_ret_sub >> 8);
    }
    
    
    v = (-loc + 3) & -4; 
    saved_ind = ind;
    ind = func_sub_sp_offset - FUNC_PROLOG_SIZE;
#ifdef TCC_TARGET_PE
    if (v >= 4096) {
        Sym *sym = external_global_sym(TOK___chkstk, &func_old_type, 0);
        oad(0xb8, v); 
        oad(0xe8, -4); 
        greloc(cur_text_section, sym, ind-4, R_386_PC32);
    } else
#endif
    {
        o(0xe58955);  
        o(0xec81);  
        gen_le32(v);
#if FUNC_PROLOG_SIZE == 10
        o(0x90);  
#endif
    }
    ind = saved_ind;
}

long gjmp(int t)
{
    return psym(0xe9, t);
}

void gjmp_addr(int a)
{
    int r;
    r = a - ind - 2;
    if (r == (char)r) {
        g(0xeb);
        g(r);
    } else {
        oad(0xe9, a - ind - 5);
    }
}

int gtst(int inv, int t)
{
    int v, *p;

    v = vtop->r & VT_VALMASK;
    if (v == VT_CMP) {
        
        g(0x0f);
        t = psym((vtop->c.i - 16) ^ inv, t);
    } else if (v == VT_JMP || v == VT_JMPI) {
        
        if ((v & 1) == inv) {
            
            p = &vtop->c.i;
            while (*p != 0)
                p = (int *)(cur_text_section->data + *p);
            *p = t;
            t = vtop->c.i;
        } else {
            t = gjmp(t);
            gsym(vtop->c.i);
        }
    } else {
        if (is_float(vtop->type.t)) {
            vpushi(0);
            gen_op(TOK_NE);
        }
        if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
            
            if ((vtop->c.i != 0) != inv) 
                t = gjmp(t);
        } else {
            v = gv(RC_INT);
            o(0x85);
            o(0xc0 + v * 9);
            g(0x0f);
            t = psym(0x85 ^ inv, t);
        }
    }
    vtop--;
    return t;
}

void gen_opi(int op)
{
    int r, fr, opc, c;

    switch(op) {
    case '+':
    case TOK_ADDC1: 
        opc = 0;
    gen_op8:
        if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
            
            vswap();
            r = gv(RC_INT);
            vswap();
            c = vtop->c.i;
            if (c == (char)c) {
                
                o(0x83);
                o(0xc0 | (opc << 3) | r);
                g(c);
            } else {
                o(0x81);
                oad(0xc0 | (opc << 3) | r, c);
            }
        } else {
            gv2(RC_INT, RC_INT);
            r = vtop[-1].r;
            fr = vtop[0].r;
            o((opc << 3) | 0x01);
            o(0xc0 + r + fr * 8); 
        }
        vtop--;
        if (op >= TOK_ULT && op <= TOK_GT) {
            vtop->r = VT_CMP;
            vtop->c.i = op;
        }
        break;
    case '-':
    case TOK_SUBC1: 
        opc = 5;
        goto gen_op8;
    case TOK_ADDC2: 
        opc = 2;
        goto gen_op8;
    case TOK_SUBC2: 
        opc = 3;
        goto gen_op8;
    case '&':
        opc = 4;
        goto gen_op8;
    case '^':
        opc = 6;
        goto gen_op8;
    case '|':
        opc = 1;
        goto gen_op8;
    case '*':
        gv2(RC_INT, RC_INT);
        r = vtop[-1].r;
        fr = vtop[0].r;
        vtop--;
        o(0xaf0f); 
        o(0xc0 + fr + r * 8);
        break;
    case TOK_SHL:
        opc = 4;
        goto gen_shift;
    case TOK_SHR:
        opc = 5;
        goto gen_shift;
    case TOK_SAR:
        opc = 7;
    gen_shift:
        opc = 0xc0 | (opc << 3);
        if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
            
            vswap();
            r = gv(RC_INT);
            vswap();
            c = vtop->c.i & 0x1f;
            o(0xc1); 
            o(opc | r);
            g(c);
        } else {
            
            gv2(RC_INT, RC_ECX);
            r = vtop[-1].r;
            o(0xd3); 
            o(opc | r);
        }
        vtop--;
        break;
    case '/':
    case TOK_UDIV:
    case TOK_PDIV:
    case '%':
    case TOK_UMOD:
    case TOK_UMULL:
        
        
        gv2(RC_EAX, RC_ECX);
        r = vtop[-1].r;
        fr = vtop[0].r;
        vtop--;
        save_reg(TREG_EDX);
        if (op == TOK_UMULL) {
            o(0xf7); 
            o(0xe0 + fr);
            vtop->r2 = TREG_EDX;
            r = TREG_EAX;
        } else {
            if (op == TOK_UDIV || op == TOK_UMOD) {
                o(0xf7d231); 
                o(0xf0 + fr);
            } else {
                o(0xf799); 
                o(0xf8 + fr);
            }
            if (op == '%' || op == TOK_UMOD)
                r = TREG_EDX;
            else
                r = TREG_EAX;
        }
        vtop->r = r;
        break;
    default:
        opc = 7;
        goto gen_op8;
    }
}

void gen_opf(int op)
{
    int a, ft, fc, swapped, r;

    
    if ((vtop[-1].r & (VT_VALMASK | VT_LVAL)) == VT_CONST) {
        vswap();
        gv(RC_FLOAT);
        vswap();
    }
    if ((vtop[0].r & (VT_VALMASK | VT_LVAL)) == VT_CONST)
        gv(RC_FLOAT);

    
    if ((vtop[-1].r & VT_LVAL) &&
        (vtop[0].r & VT_LVAL)) {
        vswap();
        gv(RC_FLOAT);
        vswap();
    }
    swapped = 0;
    if (vtop[-1].r & VT_LVAL) {
        vswap();
        swapped = 1;
    }
    if (op >= TOK_ULT && op <= TOK_GT) {
        
        load(TREG_ST0, vtop);
        save_reg(TREG_EAX); 
        if (op == TOK_GE || op == TOK_GT)
            swapped = !swapped;
        else if (op == TOK_EQ || op == TOK_NE)
            swapped = 0;
        if (swapped)
            o(0xc9d9); 
        o(0xe9da); 
        o(0xe0df); 
        if (op == TOK_EQ) {
            o(0x45e480); 
            o(0x40fC80); 
        } else if (op == TOK_NE) {
            o(0x45e480); 
            o(0x40f480); 
            op = TOK_NE;
        } else if (op == TOK_GE || op == TOK_LE) {
            o(0x05c4f6); 
            op = TOK_EQ;
        } else {
            o(0x45c4f6); 
            op = TOK_EQ;
        }
        vtop--;
        vtop->r = VT_CMP;
        vtop->c.i = op;
    } else {
        
        if ((vtop->type.t & VT_BTYPE) == VT_LDOUBLE) {
            load(TREG_ST0, vtop);
            swapped = !swapped;
        }
        
        switch(op) {
        default:
        case '+':
            a = 0;
            break;
        case '-':
            a = 4;
            if (swapped)
                a++;
            break;
        case '*':
            a = 1;
            break;
        case '/':
            a = 6;
            if (swapped)
                a++;
            break;
        }
        ft = vtop->type.t;
        fc = vtop->c.ul;
        if ((ft & VT_BTYPE) == VT_LDOUBLE) {
            o(0xde); 
            o(0xc1 + (a << 3));
        } else {
            
            r = vtop->r;
            if ((r & VT_VALMASK) == VT_LLOCAL) {
                SValue v1;
                r = get_reg(RC_INT);
                v1.type.t = VT_INT;
                v1.r = VT_LOCAL | VT_LVAL;
                v1.c.ul = fc;
                load(r, &v1);
                fc = 0;
            }

            if ((ft & VT_BTYPE) == VT_DOUBLE)
                o(0xdc);
            else
                o(0xd8);
            gen_modrm(a, r, vtop->sym, fc);
        }
        vtop--;
    }
}

void gen_cvt_itof(int t)
{
    save_reg(TREG_ST0);
    gv(RC_INT);
    if ((vtop->type.t & VT_BTYPE) == VT_LLONG) {
        o(0x50 + vtop->r2); 
        o(0x50 + (vtop->r & VT_VALMASK)); 
        o(0x242cdf); 
        o(0x08c483); 
    } else if ((vtop->type.t & (VT_BTYPE | VT_UNSIGNED)) == 
               (VT_INT | VT_UNSIGNED)) {
        
        o(0x6a); 
        g(0x00);
        o(0x50 + (vtop->r & VT_VALMASK)); 
        o(0x242cdf); 
        o(0x08c483); 
    } else {
        
        o(0x50 + (vtop->r & VT_VALMASK)); 
        o(0x2404db); 
        o(0x04c483); 
    }
    vtop->r = TREG_ST0;
}

void gen_cvt_ftoi(int t)
{
    int r, r2, size;
    Sym *sym;
    CType ushort_type;

    ushort_type.t = VT_SHORT | VT_UNSIGNED;

    gv(RC_FLOAT);
    if (t != VT_INT)
        size = 8;
    else 
        size = 4;
    
    o(0x2dd9); 
    sym = external_global_sym(TOK___tcc_int_fpu_control, 
                              &ushort_type, VT_LVAL);
    greloc(cur_text_section, sym, 
           ind, R_386_32);
    gen_le32(0);
    
    oad(0xec81, size); 
    if (size == 4)
        o(0x1cdb); 
    else
        o(0x3cdf); 
    o(0x24);
    o(0x2dd9); 
    sym = external_global_sym(TOK___tcc_fpu_control, 
                              &ushort_type, VT_LVAL);
    greloc(cur_text_section, sym, 
           ind, R_386_32);
    gen_le32(0);

    r = get_reg(RC_INT);
    o(0x58 + r); 
    if (size == 8) {
        if (t == VT_LLONG) {
            vtop->r = r; 
            r2 = get_reg(RC_INT);
            o(0x58 + r2); 
            vtop->r2 = r2;
        } else {
            o(0x04c483); 
        }
    }
    vtop->r = r;
}

void gen_cvt_ftof(int t)
{
    
    gv(RC_FLOAT);
}

void ggoto(void)
{
    gcall_or_jmp(1);
    vtop--;
}

#ifdef CONFIG_TCC_BCHECK

void gen_bounded_ptr_add(void)
{
    Sym *sym;

    
    gv2(RC_EAX, RC_EDX);
    
    vtop -= 2;
    save_regs(0);
    
    sym = external_global_sym(TOK___bound_ptr_add, &func_old_type, 0);
    greloc(cur_text_section, sym, 
           ind + 1, R_386_PC32);
    oad(0xe8, -4);
    
    vtop++;
    vtop->r = TREG_EAX | VT_BOUNDED;
    
    vtop->c.ul = (cur_text_section->reloc->data_offset - sizeof(Elf32_Rel)); 
}

void gen_bounded_ptr_deref(void)
{
    int func;
    int size, align;
    Elf32_Rel *rel;
    Sym *sym;

    size = 0;
    
    if (!is_float(vtop->type.t)) {
        if (vtop->r & VT_LVAL_BYTE)
            size = 1;
        else if (vtop->r & VT_LVAL_SHORT)
            size = 2;
    }
    if (!size)
        size = type_size(&vtop->type, &align);
    switch(size) {
    case  1: func = TOK___bound_ptr_indir1; break;
    case  2: func = TOK___bound_ptr_indir2; break;
    case  4: func = TOK___bound_ptr_indir4; break;
    case  8: func = TOK___bound_ptr_indir8; break;
    case 12: func = TOK___bound_ptr_indir12; break;
    case 16: func = TOK___bound_ptr_indir16; break;
    default:
        error("unhandled size when derefencing bounded pointer");
        func = 0;
        break;
    }

    
    
    rel = (Elf32_Rel *)(cur_text_section->reloc->data + vtop->c.ul);
    sym = external_global_sym(func, &func_old_type, 0);
    if (!sym->c)
        put_extern_sym(sym, NULL, 0, 0);
    rel->r_info = ELF32_R_INFO(sym->c, ELF32_R_TYPE(rel->r_info));
}
#endif


#endif


#ifdef CONFIG_TCC_STATIC

#define RTLD_LAZY       0x001
#define RTLD_NOW        0x002
#define RTLD_GLOBAL     0x100
#define RTLD_DEFAULT    NULL

void *dlopen(const char *filename, int flag)
{
    return NULL;
}

const char *dlerror(void)
{
    return "error";
}

typedef struct TCCSyms {
    char *str;
    void *ptr;
} TCCSyms;

#define TCCSYM(a) { #a, &a, },

static TCCSyms tcc_syms[] = {
#if !defined(CONFIG_TCCBOOT)
    TCCSYM(printf)
    TCCSYM(fprintf)
    TCCSYM(fopen)
    TCCSYM(fclose)
#endif
    { NULL, NULL },
};

void *resolve_sym(TCCState *s1, const char *symbol, int type)
{
    TCCSyms *p;
    p = tcc_syms;
    while (p->str != NULL) {
        if (!strcmp(p->str, symbol))
            return p->ptr;
        p++;
    }
    return NULL;
}

#elif !defined(WIN32)

#include <dlfcn.h>

void *resolve_sym(TCCState *s1, const char *sym, int type)
{
  assert(0);
  return 0; 
  
}

#endif


int ieee_finite(double d)
{
    int *p = (int *)&d;
    return ((unsigned)((p[1] | 0x800fffff) + 1)) >> 31;
}

static char *pstrcpy(char *buf, int buf_size, const char *s)
{
    char *q, *q_end;
    int c;

    if (buf_size > 0) {
        q = buf;
        q_end = buf + buf_size - 1;
        while (q < q_end) {
            c = *s++;
            if (c == '\0')
                break;
            *q++ = c;
        }
        *q = '\0';
    }
    return buf;
}

static char *pstrcat(char *buf, int buf_size, const char *s)
{
    int len;
    len = strlen(buf);
    if (len < buf_size) 
        pstrcpy(buf + len, buf_size - len, s);
    return buf;
}

static int strstart(const char *str, const char *val, const char **ptr)
{
    const char *p, *q;
    p = str;
    q = val;
    while (*q != '\0') {
        if (*p != *q)
            return 0;
        p++;
        q++;
    }
    if (ptr)
        *ptr = p;
    return 1;
}

#ifdef MEM_DEBUG
int mem_cur_size;
int mem_max_size;
#endif

static inline void tcc_free(void *ptr)
{
#ifdef MEM_DEBUG
    mem_cur_size -= malloc_usable_size(ptr);
#endif
    free(ptr);
}

static void *tcc_malloc(unsigned long size)
{
    void *ptr;
    ptr = malloc(size);
    if (!ptr && size)
        error("memory full");
#ifdef MEM_DEBUG
    mem_cur_size += malloc_usable_size(ptr);
    if (mem_cur_size > mem_max_size)
        mem_max_size = mem_cur_size;
#endif
    return ptr;
}

static void *tcc_mallocz(unsigned long size)
{
    void *ptr;
    ptr = tcc_malloc(size);
    memset(ptr, 0, size);
    return ptr;
}

static inline void *tcc_realloc(void *ptr, unsigned long size)
{
    void *ptr1;
#ifdef MEM_DEBUG
    mem_cur_size -= malloc_usable_size(ptr);
#endif
    ptr1 = realloc(ptr, size);
#ifdef MEM_DEBUG
    
    mem_cur_size += malloc_usable_size(ptr1);
    if (mem_cur_size > mem_max_size)
        mem_max_size = mem_cur_size;
#endif
    return ptr1;
}

static char *tcc_strdup(const char *str)
{
    char *ptr;
    ptr = tcc_malloc(strlen(str) + 1);
    strcpy(ptr, str);
    return ptr;
}

#define free(p) use_tcc_free(p)
#define malloc(s) use_tcc_malloc(s)
#define realloc(p, s) use_tcc_realloc(p, s)

static void dynarray_add(void ***ptab, int *nb_ptr, void *data)
{
    int nb, nb_alloc;
    void **pp;
    
    nb = *nb_ptr;
    pp = *ptab;
    
    if ((nb & (nb - 1)) == 0) {
        if (!nb)
            nb_alloc = 1;
        else
            nb_alloc = nb * 2;
        pp = tcc_realloc(pp, nb_alloc * sizeof(void *));
        if (!pp)
            error("memory full");
        *ptab = pp;
    }
    pp[nb++] = data;
    *nb_ptr = nb;
}

static Sym *__sym_malloc(void)
{
    Sym *sym_pool, *sym, *last_sym;
    int i;

    sym_pool = tcc_malloc(SYM_POOL_NB * sizeof(Sym));

    last_sym = sym_free_first;
    sym = sym_pool;
    for(i = 0; i < SYM_POOL_NB; i++) {
        sym->next = last_sym;
        last_sym = sym;
        sym++;
    }
    sym_free_first = last_sym;
    return last_sym;
}

static inline Sym *sym_malloc(void)
{
    Sym *sym;
    sym = sym_free_first;
    if (!sym)
        sym = __sym_malloc();
    sym_free_first = sym->next;
    return sym;
}

static inline void sym_free(Sym *sym)
{
    sym->next = sym_free_first;
    sym_free_first = sym;
}

Section *new_section(TCCState *s1, const char *name, int sh_type, int sh_flags)
{
    Section *sec;

    sec = tcc_mallocz(sizeof(Section) + strlen(name));
    strcpy(sec->name, name);
    sec->sh_type = sh_type;
    sec->sh_flags = sh_flags;
    switch(sh_type) {
    case SHT_HASH:
    case SHT_REL:
    case SHT_DYNSYM:
    case SHT_SYMTAB:
    case SHT_DYNAMIC:
        sec->sh_addralign = 4;
        break;
    case SHT_STRTAB:
        sec->sh_addralign = 1;
        break;
    default:
        sec->sh_addralign = 32; 
        break;
    }

    
    if (!(sh_flags & SHF_PRIVATE)) {
        sec->sh_num = s1->nb_sections;
        dynarray_add((void ***)&s1->sections, &s1->nb_sections, sec);
    }
    return sec;
}

static void free_section(Section *s)
{
    tcc_free(s->data);
    tcc_free(s);
}

static void section_realloc(Section *sec, unsigned long new_size)
{
    unsigned long size;
    unsigned char *data;
    
    size = sec->data_allocated;
    if (size == 0)
        size = 1;
    while (size < new_size)
        size = size * 2;
    data = tcc_realloc(sec->data, size);
    if (!data)
        error("memory full");
    memset(data + sec->data_allocated, 0, size - sec->data_allocated);
    sec->data = data;
    sec->data_allocated = size;
}

static void *section_ptr_add(Section *sec, unsigned long size)
{
    unsigned long offset, offset1;

    offset = sec->data_offset;
    offset1 = offset + size;
    if (offset1 > sec->data_allocated)
        section_realloc(sec, offset1);
    sec->data_offset = offset1;
    return sec->data + offset;
}

Section *find_section(TCCState *s1, const char *name)
{
    Section *sec;
    int i;
    for(i = 1; i < s1->nb_sections; i++) {
        sec = s1->sections[i];
        if (!strcmp(name, sec->name)) 
            return sec;
    }
    
    return new_section(s1, name, SHT_PROGBITS, SHF_ALLOC);
}

#define SECTION_ABS ((void *)1)

static void put_extern_sym2(Sym *sym, Section *section, 
                            unsigned long value, unsigned long size,
                            int can_add_underscore)
{
    int sym_type, sym_bind, sh_num, info;
    Elf32_Sym *esym;
    const char *name;
    char buf1[256];

    if (section == NULL)
        sh_num = SHN_UNDEF;
    else if (section == SECTION_ABS) 
        sh_num = SHN_ABS;
    else
        sh_num = section->sh_num;
    if (!sym->c) {
        if ((sym->type.t & VT_BTYPE) == VT_FUNC)
            sym_type = STT_FUNC;
        else
            sym_type = STT_OBJECT;
        if (sym->type.t & VT_STATIC)
            sym_bind = STB_LOCAL;
        else
            sym_bind = STB_GLOBAL;
        
        name = get_tok_str(sym->v, NULL);
#ifdef CONFIG_TCC_BCHECK
        if (do_bounds_check) {
            char buf[32];

            
            switch(sym->v) {
#if 0
            
            case TOK_malloc: 
            case TOK_free: 
            case TOK_realloc: 
            case TOK_memalign: 
            case TOK_calloc: 
#endif
            case TOK_memcpy: 
            case TOK_memmove:
            case TOK_memset:
            case TOK_strlen:
            case TOK_strcpy:
                strcpy(buf, "__bound_");
                strcat(buf, name);
                name = buf;
                break;
            }
        }
#endif
        if (tcc_state->leading_underscore && can_add_underscore) {
            buf1[0] = '_';
            pstrcpy(buf1 + 1, sizeof(buf1) - 1, name);
            name = buf1;
        }
        info = ELF32_ST_INFO(sym_bind, sym_type);
        sym->c = add_elf_sym(symtab_section, value, size, info, 0, sh_num, name);
    } else {
        esym = &((Elf32_Sym *)symtab_section->data)[sym->c];
        esym->st_value = value;
        esym->st_size = size;
        esym->st_shndx = sh_num;
    }
}

static void put_extern_sym(Sym *sym, Section *section, 
                           unsigned long value, unsigned long size)
{
    put_extern_sym2(sym, section, value, size, 1);
}

static void greloc(Section *s, Sym *sym, unsigned long offset, int type)
{
    if (!sym->c) 
        put_extern_sym(sym, NULL, 0, 0);
    
    put_elf_reloc(symtab_section, s, offset, type, sym->c);
}

static inline int isid(int c)
{
    return (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        c == '_';
}

static inline int isnum(int c)
{
    return c >= '0' && c <= '9';
}

static inline int isoct(int c)
{
    return c >= '0' && c <= '7';
}

static inline int toup(int c)
{
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 'A';
    else
        return c;
}

static void strcat_vprintf(char *buf, int buf_size, const char *fmt, va_list ap)
{
    int len;
    len = strlen(buf);
    vsnprintf(buf + len, buf_size - len, fmt, ap);
}

static void strcat_printf(char *buf, int buf_size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    strcat_vprintf(buf, buf_size, fmt, ap);
    va_end(ap);
}

void error1(TCCState *s1, int is_warning, const char *fmt, va_list ap)
{
    char buf[2048];
    BufferedFile **f;
    
    buf[0] = '\0';
    if (file) {
        for(f = s1->include_stack; f < s1->include_stack_ptr; f++)
            strcat_printf(buf, sizeof(buf), "In file included from %s:%d:\n", 
                          (*f)->filename, (*f)->line_num);
        if (file->line_num > 0) {
            strcat_printf(buf, sizeof(buf), 
                          "%s:%d: ", file->filename, file->line_num);
        } else {
            strcat_printf(buf, sizeof(buf),
                          "%s: ", file->filename);
        }
    } else {
        strcat_printf(buf, sizeof(buf),
                      "tcc: ");
    }
    if (is_warning)
        strcat_printf(buf, sizeof(buf), "warning: ");
    strcat_vprintf(buf, sizeof(buf), fmt, ap);

    if (!s1->error_func) {
        
        fprintf(stderr, "%s\n", buf);
    } else {
        s1->error_func(s1->error_opaque, buf);
    }
    if (!is_warning || s1->warn_error)
        s1->nb_errors++;
}

#ifdef LIBTCC
void tcc_set_error_func(TCCState *s, void *error_opaque,
                        void (*error_func)(void *opaque, const char *msg))
{
    s->error_opaque = error_opaque;
    s->error_func = error_func;
}
#endif

void error_noabort(const char *fmt, ...)
{
    TCCState *s1 = tcc_state;
    va_list ap;

    va_start(ap, fmt);
    error1(s1, 0, fmt, ap);
    va_end(ap);
}

void error(const char *fmt, ...)
{
    TCCState *s1 = tcc_state;
    va_list ap;

    va_start(ap, fmt);
    error1(s1, 0, fmt, ap);
    va_end(ap);
    
    if (s1->error_set_jmp_enabled) {
        longjmp(s1->error_jmp_buf, 1);
    } else {
        
        exit(1);
    }
}

void expect(const char *msg)
{
    error("%s expected", msg);
}

void warning(const char *fmt, ...)
{
    TCCState *s1 = tcc_state;
    va_list ap;

    if (s1->warn_none)
        return;

    va_start(ap, fmt);
    error1(s1, 1, fmt, ap);
    va_end(ap);
}

void skip(int c)
{
    if (tok != c)
        error("'%c' expected", c);
    next();
}

static void test_lvalue(void)
{
    if (!(vtop->r & VT_LVAL))
        expect("lvalue");
}

static TokenSym *tok_alloc_new(TokenSym **pts, const char *str, int len)
{
    TokenSym *ts, **ptable;
    int i;

    if (tok_ident >= SYM_FIRST_ANOM) 
        error("memory full");

    
    i = tok_ident - TOK_IDENT;
    if ((i % TOK_ALLOC_INCR) == 0) {
        ptable = tcc_realloc(table_ident, (i + TOK_ALLOC_INCR) * sizeof(TokenSym *));
        if (!ptable)
            error("memory full");
        table_ident = ptable;
    }

    ts = tcc_malloc(sizeof(TokenSym) + len);
    table_ident[i] = ts;
    ts->tok = tok_ident++;
    ts->sym_define = NULL;
    ts->sym_label = NULL;
    ts->sym_struct = NULL;
    ts->sym_identifier = NULL;
    ts->len = len;
    ts->hash_next = NULL;
    memcpy(ts->str, str, len);
    ts->str[len] = '\0';
    *pts = ts;
    return ts;
}

#define TOK_HASH_INIT 1
#define TOK_HASH_FUNC(h, c) ((h) * 263 + (c))

static TokenSym *tok_alloc(const char *str, int len)
{
    TokenSym *ts, **pts;
    int i;
    unsigned int h;
    
    h = TOK_HASH_INIT;
    for(i=0;i<len;i++)
        h = TOK_HASH_FUNC(h, ((unsigned char *)str)[i]);
    h &= (TOK_HASH_SIZE - 1);

    pts = &hash_ident[h];
    for(;;) {
        ts = *pts;
        if (!ts)
            break;
        if (ts->len == len && !memcmp(ts->str, str, len))
            return ts;
        pts = &(ts->hash_next);
    }
    return tok_alloc_new(pts, str, len);
}


static void cstr_realloc(CString *cstr, int new_size)
{
    int size;
    void *data;

    size = cstr->size_allocated;
    if (size == 0)
        size = 8; 
    while (size < new_size)
        size = size * 2;
    data = tcc_realloc(cstr->data_allocated, size);
    if (!data)
        error("memory full");
    cstr->data_allocated = data;
    cstr->size_allocated = size;
    cstr->data = data;
}

static inline void cstr_ccat(CString *cstr, int ch)
{
    int size;
    size = cstr->size + 1;
    if (size > cstr->size_allocated)
        cstr_realloc(cstr, size);
    ((unsigned char *)cstr->data)[size - 1] = ch;
    cstr->size = size;
}

static void cstr_cat(CString *cstr, const char *str)
{
    int c;
    for(;;) {
        c = *str;
        if (c == '\0')
            break;
        cstr_ccat(cstr, c);
        str++;
    }
}

static void cstr_wccat(CString *cstr, int ch)
{
    int size;
    size = cstr->size + sizeof(int);
    if (size > cstr->size_allocated)
        cstr_realloc(cstr, size);
    *(int *)(((unsigned char *)cstr->data) + size - sizeof(int)) = ch;
    cstr->size = size;
}

static void cstr_new(CString *cstr)
{
    memset(cstr, 0, sizeof(CString));
}

static void cstr_free(CString *cstr)
{
    tcc_free(cstr->data_allocated);
    cstr_new(cstr);
}

#define cstr_reset(cstr) cstr_free(cstr)

static void add_char(CString *cstr, int c)
{
    if (c == '\'' || c == '\"' || c == '\\') {
        
        cstr_ccat(cstr, '\\');
    }
    if (c >= 32 && c <= 126) {
        cstr_ccat(cstr, c);
    } else {
        cstr_ccat(cstr, '\\');
        if (c == '\n') {
            cstr_ccat(cstr, 'n');
        } else {
            cstr_ccat(cstr, '0' + ((c >> 6) & 7));
            cstr_ccat(cstr, '0' + ((c >> 3) & 7));
            cstr_ccat(cstr, '0' + (c & 7));
        }
    }
}

char *get_tok_str(int v, CValue *cv)
{
    static char buf[STRING_MAX_SIZE + 1];
    static CString cstr_buf;
    CString *cstr;
    unsigned char *q;
    char *p;
    int i, len;

    
    cstr_reset(&cstr_buf);
    cstr_buf.data = buf;
    cstr_buf.size_allocated = sizeof(buf);
    p = buf;

    switch(v) {
    case TOK_CINT:
    case TOK_CUINT:
        
        sprintf(p, "%u", cv->ui);
        break;
    case TOK_CLLONG:
    case TOK_CULLONG:
        
        sprintf(p, "%llu", cv->ull);
        break;
    case TOK_CCHAR:
    case TOK_LCHAR:
        cstr_ccat(&cstr_buf, '\'');
        add_char(&cstr_buf, cv->i);
        cstr_ccat(&cstr_buf, '\'');
        cstr_ccat(&cstr_buf, '\0');
        break;
    case TOK_PPNUM:
        cstr = cv->cstr;
        len = cstr->size - 1;
        for(i=0;i<len;i++)
            add_char(&cstr_buf, ((unsigned char *)cstr->data)[i]);
        cstr_ccat(&cstr_buf, '\0');
        break;
    case TOK_STR:
    case TOK_LSTR:
        cstr = cv->cstr;
        cstr_ccat(&cstr_buf, '\"');
        if (v == TOK_STR) {
            len = cstr->size - 1;
            for(i=0;i<len;i++)
                add_char(&cstr_buf, ((unsigned char *)cstr->data)[i]);
        } else {
            len = (cstr->size / sizeof(int)) - 1;
            for(i=0;i<len;i++)
                add_char(&cstr_buf, ((int *)cstr->data)[i]);
        }
        cstr_ccat(&cstr_buf, '\"');
        cstr_ccat(&cstr_buf, '\0');
        break;
    case TOK_LT:
        v = '<';
        goto addv;
    case TOK_GT:
        v = '>';
        goto addv;
    case TOK_A_SHL:
        return strcpy(p, "<<=");
    case TOK_A_SAR:
        return strcpy(p, ">>=");
    default:
        if (v < TOK_IDENT) {
            
            q = tok_two_chars;
            while (*q) {
                if (q[2] == v) {
                    *p++ = q[0];
                    *p++ = q[1];
                    *p = '\0';
                    return buf;
                }
                q += 3;
            }
        addv:
            *p++ = v;
            *p = '\0';
        } else if (v < tok_ident) {
            return table_ident[v - TOK_IDENT]->str;
        } else if (v >= SYM_FIRST_ANOM) {
            
            sprintf(p, "L.%u", v - SYM_FIRST_ANOM);
        } else {
            
            return NULL;
        }
        break;
    }
    return cstr_buf.data;
}

static Sym *sym_push2(Sym **ps, long v, long t, long c)
{
    Sym *s;
    s = sym_malloc();
    s->v = v;
    s->type.t = t;
    s->c = c;
    s->next = NULL;
    
    s->prev = *ps;
    *ps = s;
    return s;
}

static Sym *sym_find2(Sym *s, int v)
{
    while (s) {
        if (s->v == v)
            return s;
        s = s->prev;
    }
    return NULL;
}

static inline Sym *struct_find(int v)
{
    v -= TOK_IDENT;
    if ((unsigned)v >= (unsigned)(tok_ident - TOK_IDENT))
        return NULL;
    return table_ident[v]->sym_struct;
}

static inline Sym *sym_find(int v)
{
    v -= TOK_IDENT;
    if ((unsigned)v >= (unsigned)(tok_ident - TOK_IDENT))
        return NULL;
    return table_ident[v]->sym_identifier;
}

static Sym *sym_push(int v, CType *type, int r, int c)
{
    Sym *s, **ps;
    TokenSym *ts;

    if (local_stack)
        ps = &local_stack;
    else
        ps = &global_stack;
    s = sym_push2(ps, v, type->t, c);
    s->type.ref = type->ref;
    s->r = r;
    
    
    if (!(v & SYM_FIELD) && (v & ~SYM_STRUCT) < SYM_FIRST_ANOM) {
        
        ts = table_ident[(v & ~SYM_STRUCT) - TOK_IDENT];
        if (v & SYM_STRUCT)
            ps = &ts->sym_struct;
        else
            ps = &ts->sym_identifier;
        s->prev_tok = *ps;
        *ps = s;
    }
    return s;
}

static Sym *global_identifier_push(int v, int t, int c)
{
    Sym *s, **ps;
    s = sym_push2(&global_stack, v, t, c);
    
    if (v < SYM_FIRST_ANOM) {
        ps = &table_ident[v - TOK_IDENT]->sym_identifier;
        while (*ps != NULL)
            ps = &(*ps)->prev_tok;
        s->prev_tok = NULL;
        *ps = s;
    }
    return s;
}

static void sym_pop(Sym **ptop, Sym *b)
{
    Sym *s, *ss, **ps;
    TokenSym *ts;
    int v;

    s = *ptop;
    while(s != b) {
        ss = s->prev;
        v = s->v;
        
        
        if (!(v & SYM_FIELD) && (v & ~SYM_STRUCT) < SYM_FIRST_ANOM) {
            ts = table_ident[(v & ~SYM_STRUCT) - TOK_IDENT];
            if (v & SYM_STRUCT)
                ps = &ts->sym_struct;
            else
                ps = &ts->sym_identifier;
            *ps = s->prev_tok;
        }
        sym_free(s);
        s = ss;
    }
    *ptop = b;
}


BufferedFile *tcc_open(TCCState *s1, const char *filename)
{
    int fd;
    BufferedFile *bf;

    fd = open(filename, O_RDONLY | O_BINARY);
    if (fd < 0)
        return NULL;
    bf = tcc_malloc(sizeof(BufferedFile));
    if (!bf) {
        close(fd);
        return NULL;
    }
    bf->fd = fd;
    bf->buf_ptr = bf->buffer;
    bf->buf_end = bf->buffer;
    bf->buffer[0] = CH_EOB; 
    pstrcpy(bf->filename, sizeof(bf->filename), filename);
    bf->line_num = 1;
    bf->ifndef_macro = 0;
    bf->ifdef_stack_ptr = s1->ifdef_stack_ptr;
    
    return bf;
}

void tcc_close(BufferedFile *bf)
{
    total_lines += bf->line_num;
    close(bf->fd);
    tcc_free(bf);
}

static int tcc_peekc_slow(BufferedFile *bf)
{
    int len;
    
    if (bf->buf_ptr >= bf->buf_end) {
        if (bf->fd != -1) {
#if defined(PARSE_DEBUG)
            len = 8;
#else
            len = IO_BUF_SIZE;
#endif
            len = read(bf->fd, bf->buffer, len);
            if (len < 0)
                len = 0;
        } else {
            len = 0;
        }
        total_bytes += len;
        bf->buf_ptr = bf->buffer;
        bf->buf_end = bf->buffer + len;
        *bf->buf_end = CH_EOB;
    }
    if (bf->buf_ptr < bf->buf_end) {
        return bf->buf_ptr[0];
    } else {
        bf->buf_ptr = bf->buf_end;
        return CH_EOF;
    }
}

static int handle_eob(void)
{
    return tcc_peekc_slow(file);
}

static inline void inp(void)
{
    ch = *(++(file->buf_ptr));
    
    if (ch == CH_EOB)
        ch = handle_eob();
}

static void handle_stray(void)
{
    while (ch == '\\') {
        inp();
        if (ch == '\n') {
            file->line_num++;
            inp();
        } else if (ch == '\r') {
            inp();
            if (ch != '\n')
                goto fail;
            file->line_num++;
            inp();
        } else {
        fail:
            error("stray '\\' in program");
        }
    }
}

static int handle_stray1(uint8_t *p)
{
    int c;

    if (p >= file->buf_end) {
        file->buf_ptr = p;
        c = handle_eob();
        p = file->buf_ptr;
        if (c == '\\')
            goto parse_stray;
    } else {
    parse_stray:
        file->buf_ptr = p;
        ch = *p;
        handle_stray();
        p = file->buf_ptr;
        c = *p;
    }
    return c;
}

#define PEEKC_EOB(c, p)\
{\
    p++;\
    c = *p;\
    if (c == '\\') {\
        file->buf_ptr = p;\
        c = handle_eob();\
        p = file->buf_ptr;\
    }\
}

#define PEEKC(c, p)\
{\
    p++;\
    c = *p;\
    if (c == '\\') {\
        c = handle_stray1(p);\
        p = file->buf_ptr;\
    }\
}

static void minp(void)
{
    inp();
    if (ch == '\\') 
        handle_stray();
}


static uint8_t *parse_line_comment(uint8_t *p)
{
    int c;

    p++;
    for(;;) {
        c = *p;
    redo:
        if (c == '\n' || c == CH_EOF) {
            break;
        } else if (c == '\\') {
            file->buf_ptr = p;
            c = handle_eob();
            p = file->buf_ptr;
            if (c == '\\') {
                PEEKC_EOB(c, p);
                if (c == '\n') {
                    file->line_num++;
                    PEEKC_EOB(c, p);
                } else if (c == '\r') {
                    PEEKC_EOB(c, p);
                    if (c == '\n') {
                        file->line_num++;
                        PEEKC_EOB(c, p);
                    }
                }
            } else {
                goto redo;
            }
        } else {
            p++;
        }
    }
    return p;
}

static uint8_t *parse_comment(uint8_t *p)
{
    int c;
    
    p++;
    for(;;) {
        
        for(;;) {
            c = *p;
            if (c == '\n' || c == '*' || c == '\\')
                break;
            p++;
            c = *p;
            if (c == '\n' || c == '*' || c == '\\')
                break;
            p++;
        }
        
        if (c == '\n') {
            file->line_num++;
            p++;
        } else if (c == '*') {
            p++;
            for(;;) {
                c = *p;
                if (c == '*') {
                    p++;
                } else if (c == '/') {
                    goto end_of_comment;
                } else if (c == '\\') {
                    file->buf_ptr = p;
                    c = handle_eob();
                    p = file->buf_ptr;
                    if (c == '\\') {
                        
                        while (c == '\\') {
                            PEEKC_EOB(c, p);
                            if (c == '\n') {
                                file->line_num++;
                                PEEKC_EOB(c, p);
                            } else if (c == '\r') {
                                PEEKC_EOB(c, p);
                                if (c == '\n') {
                                    file->line_num++;
                                    PEEKC_EOB(c, p);
                                }
                            } else {
                                goto after_star;
                            }
                        }
                    }
                } else {
                    break;
                }
            }
        after_star: ;
        } else {
            
            file->buf_ptr = p;
            c = handle_eob();
            p = file->buf_ptr;
            if (c == CH_EOF) {
                error("unexpected end of file in comment");
            } else if (c == '\\') {
                p++;
            }
        }
    }
 end_of_comment:
    p++;
    return p;
}

#define cinp minp

static inline int is_space(int ch)
{
    return ch == ' ' || ch == '\t' || ch == '\v' || ch == '\f' || ch == '\r';
}

static inline void skip_spaces(void)
{
    while (is_space(ch))
        cinp();
}

static uint8_t *parse_pp_string(uint8_t *p,
                                int sep, CString *str)
{
    int c;
    p++;
    for(;;) {
        c = *p;
        if (c == sep) {
            break;
        } else if (c == '\\') {
            file->buf_ptr = p;
            c = handle_eob();
            p = file->buf_ptr;
            if (c == CH_EOF) {
            unterminated_string:
                
                error("missing terminating %c character", sep);
            } else if (c == '\\') {
                
                PEEKC_EOB(c, p);
                if (c == '\n') {
                    file->line_num++;
                    p++;
                } else if (c == '\r') {
                    PEEKC_EOB(c, p);
                    if (c != '\n')
                        expect("'\n' after '\r'");
                    file->line_num++;
                    p++;
                } else if (c == CH_EOF) {
                    goto unterminated_string;
                } else {
                    if (str) {
                        cstr_ccat(str, '\\');
                        cstr_ccat(str, c);
                    }
                    p++;
                }
            }
        } else if (c == '\n') {
            file->line_num++;
            goto add_char;
        } else if (c == '\r') {
            PEEKC_EOB(c, p);
            if (c != '\n') {
                if (str)
                    cstr_ccat(str, '\r');
            } else {
                file->line_num++;
                goto add_char;
            }
        } else {
        add_char:
            if (str)
                cstr_ccat(str, c);
            p++;
        }
    }
    p++;
    return p;
}

void preprocess_skip(void)
{
    int a, start_of_line, c;
    uint8_t *p;

    p = file->buf_ptr;
    start_of_line = 1;
    a = 0;
    for(;;) {
    redo_no_start:
        c = *p;
        switch(c) {
        case ' ':
        case '\t':
        case '\f':
        case '\v':
        case '\r':
            p++;
            goto redo_no_start;
        case '\n':
            start_of_line = 1;
            file->line_num++;
            p++;
            goto redo_no_start;
        case '\\':
            file->buf_ptr = p;
            c = handle_eob();
            if (c == CH_EOF) {
                expect("#endif");
            } else if (c == '\\') {
                
                ch = file->buf_ptr[0];
                handle_stray();
            }
            p = file->buf_ptr;
            goto redo_no_start;
            
        case '\"':
        case '\'':
            p = parse_pp_string(p, c, NULL);
            break;
            
        case '/':
            file->buf_ptr = p;
            ch = *p;
            minp();
            p = file->buf_ptr;
            if (ch == '*') {
                p = parse_comment(p);
            } else if (ch == '/') {
                p = parse_line_comment(p);
            }
            break;

        case '#':
            p++;
            if (start_of_line) {
                file->buf_ptr = p;
                next_nomacro();
                p = file->buf_ptr;
                if (a == 0 && 
                    (tok == TOK_ELSE || tok == TOK_ELIF || tok == TOK_ENDIF))
                    goto the_end;
                if (tok == TOK_IF || tok == TOK_IFDEF || tok == TOK_IFNDEF)
                    a++;
                else if (tok == TOK_ENDIF)
                    a--;
            }
            break;
        default:
            p++;
            break;
        }
        start_of_line = 0;
    }
 the_end: ;
    file->buf_ptr = p;
}



void save_parse_state(ParseState *s)
{
    s->line_num = file->line_num;
    s->macro_ptr = macro_ptr;
    s->tok = tok;
    s->tokc = tokc;
}

void restore_parse_state(ParseState *s)
{
    file->line_num = s->line_num;
    macro_ptr = s->macro_ptr;
    tok = s->tok;
    tokc = s->tokc;
}

static inline int tok_ext_size(int t)
{
    switch(t) {
        
    case TOK_CINT:
    case TOK_CUINT:
    case TOK_CCHAR:
    case TOK_LCHAR:
    case TOK_CFLOAT:
    case TOK_LINENUM:
        return 1;
    case TOK_STR:
    case TOK_LSTR:
    case TOK_PPNUM:
        error("unsupported token");
        return 1;
    case TOK_CDOUBLE:
    case TOK_CLLONG:
    case TOK_CULLONG:
        return 2;
    case TOK_CLDOUBLE:
        return LDOUBLE_SIZE / 4;
    default:
        return 0;
    }
}


static inline void tok_str_new(TokenString *s)
{
    s->str = NULL;
    s->len = 0;
    s->allocated_len = 0;
    s->last_line_num = -1;
}

static void tok_str_free(int *str)
{
    tcc_free(str);
}

static int *tok_str_realloc(TokenString *s)
{
    int *str, len;

    if (s->allocated_len == 0) {
        len = 8;
    } else {
        len = s->allocated_len * 2;
    }
    str = tcc_realloc(s->str, len * sizeof(int));
    if (!str)
        error("memory full");
    s->allocated_len = len;
    s->str = str;
    return str;
}

static void tok_str_add(TokenString *s, int t)
{
    int len, *str;

    len = s->len;
    str = s->str;
    if (len >= s->allocated_len)
        str = tok_str_realloc(s);
    str[len++] = t;
    s->len = len;
}

static void tok_str_add2(TokenString *s, int t, CValue *cv)
{
    int len, *str;

    len = s->len;
    str = s->str;

    
    if (len + TOK_MAX_SIZE > s->allocated_len)
        str = tok_str_realloc(s);
    str[len++] = t;
    switch(t) {
    case TOK_CINT:
    case TOK_CUINT:
    case TOK_CCHAR:
    case TOK_LCHAR:
    case TOK_CFLOAT:
    case TOK_LINENUM:
        str[len++] = cv->tab[0];
        break;
    case TOK_PPNUM:
    case TOK_STR:
    case TOK_LSTR:
        {
            int nb_words;
            CString *cstr;

            nb_words = (sizeof(CString) + cv->cstr->size + 3) >> 2;
            while ((len + nb_words) > s->allocated_len)
                str = tok_str_realloc(s);
            cstr = (CString *)(str + len);
            cstr->data = NULL;
            cstr->size = cv->cstr->size;
            cstr->data_allocated = NULL;
            cstr->size_allocated = cstr->size;
            memcpy((char *)cstr + sizeof(CString), 
                   cv->cstr->data, cstr->size);
            len += nb_words;
        }
        break;
    case TOK_CDOUBLE:
    case TOK_CLLONG:
    case TOK_CULLONG:
#if LDOUBLE_SIZE == 8
    case TOK_CLDOUBLE:
#endif
        str[len++] = cv->tab[0];
        str[len++] = cv->tab[1];
        break;
#if LDOUBLE_SIZE == 12
    case TOK_CLDOUBLE:
        str[len++] = cv->tab[0];
        str[len++] = cv->tab[1];
        str[len++] = cv->tab[2];
#elif LDOUBLE_SIZE != 8
#error add long double size support
#endif
        break;
    default:
        break;
    }
    s->len = len;
}

static void tok_str_add_tok(TokenString *s)
{
    CValue cval;

    
    if (file->line_num != s->last_line_num) {
        s->last_line_num = file->line_num;
        cval.i = s->last_line_num;
        tok_str_add2(s, TOK_LINENUM, &cval);
    }
    tok_str_add2(s, tok, &tokc);
}

#if LDOUBLE_SIZE == 12
#define LDOUBLE_GET(p, cv)                      \
        cv.tab[0] = p[0];                       \
        cv.tab[1] = p[1];                       \
        cv.tab[2] = p[2];
#elif LDOUBLE_SIZE == 8
#define LDOUBLE_GET(p, cv)                      \
        cv.tab[0] = p[0];                       \
        cv.tab[1] = p[1];
#else
#error add long double size support
#endif


#define TOK_GET(t, p, cv)                       \
{                                               \
    t = *p++;                                   \
    switch(t) {                                 \
    case TOK_CINT:                              \
    case TOK_CUINT:                             \
    case TOK_CCHAR:                             \
    case TOK_LCHAR:                             \
    case TOK_CFLOAT:                            \
    case TOK_LINENUM:                           \
        cv.tab[0] = *p++;                       \
        break;                                  \
    case TOK_STR:                               \
    case TOK_LSTR:                              \
    case TOK_PPNUM:                             \
        cv.cstr = (CString *)p;                 \
        cv.cstr->data = (char *)p + sizeof(CString);\
        p += (sizeof(CString) + cv.cstr->size + 3) >> 2;\
        break;                                  \
    case TOK_CDOUBLE:                           \
    case TOK_CLLONG:                            \
    case TOK_CULLONG:                           \
        cv.tab[0] = p[0];                       \
        cv.tab[1] = p[1];                       \
        p += 2;                                 \
        break;                                  \
    case TOK_CLDOUBLE:                          \
        LDOUBLE_GET(p, cv);                     \
        p += LDOUBLE_SIZE / 4;                  \
        break;                                  \
    default:                                    \
        break;                                  \
    }                                           \
}

static inline void define_push(int v, int macro_type, int *str, Sym *first_arg)
{
    Sym *s;

    s = sym_push2(&define_stack, v, macro_type, (long)str);
    s->next = first_arg;
    table_ident[v - TOK_IDENT]->sym_define = s;
}

static void define_undef(Sym *s)
{
    int v;
    v = s->v;
    if (v >= TOK_IDENT && v < tok_ident)
        table_ident[v - TOK_IDENT]->sym_define = NULL;
    s->v = 0;
}

static inline Sym *define_find(int v)
{
    v -= TOK_IDENT;
    if ((unsigned)v >= (unsigned)(tok_ident - TOK_IDENT))
        return NULL;
    return table_ident[v]->sym_define;
}

static void free_defines(Sym *b)
{
    Sym *top, *top1;
    int v;

    top = define_stack;
    while (top != b) {
        top1 = top->prev;
        
        if (top->c)
            tok_str_free((int *)top->c);
        v = top->v;
        if (v >= TOK_IDENT && v < tok_ident)
            table_ident[v - TOK_IDENT]->sym_define = NULL;
        sym_free(top);
        top = top1;
    }
    define_stack = b;
}

static Sym *label_find(int v)
{
    v -= TOK_IDENT;
    if ((unsigned)v >= (unsigned)(tok_ident - TOK_IDENT))
        return NULL;
    return table_ident[v]->sym_label;
}

static Sym *label_push(Sym **ptop, int v, int flags)
{
    Sym *s, **ps;
    s = sym_push2(ptop, v, 0, 0);
    s->r = flags;
    ps = &table_ident[v - TOK_IDENT]->sym_label;
    if (ptop == &global_label_stack) {
        while (*ps != NULL)
            ps = &(*ps)->prev_tok;
    }
    s->prev_tok = *ps;
    *ps = s;
    return s;
}

static void label_pop(Sym **ptop, Sym *slast)
{
    Sym *s, *s1;
    for(s = *ptop; s != slast; s = s1) {
        s1 = s->prev;
        if (s->r == LABEL_DECLARED) {
            warning("label '%s' declared but not used", get_tok_str(s->v, NULL));
        } else if (s->r == LABEL_FORWARD) {
                error("label '%s' used but not defined",
                      get_tok_str(s->v, NULL));
        } else {
            if (s->c) {
                put_extern_sym(s, cur_text_section, (long)s->next, 1);
            }
        }
        
        table_ident[s->v - TOK_IDENT]->sym_label = s->prev_tok;
        sym_free(s);
    }
    *ptop = slast;
}

static int expr_preprocess(void)
{
    int c, t;
    TokenString str;
    
    tok_str_new(&str);
    while (tok != TOK_LINEFEED && tok != TOK_EOF) {
        next(); 
        if (tok == TOK_DEFINED) {
            next_nomacro();
            t = tok;
            if (t == '(') 
                next_nomacro();
            c = define_find(tok) != 0;
            if (t == '(')
                next_nomacro();
            tok = TOK_CINT;
            tokc.i = c;
        } else if (tok >= TOK_IDENT) {
            
            tok = TOK_CINT;
            tokc.i = 0;
        }
        tok_str_add_tok(&str);
    }
    tok_str_add(&str, -1); 
    tok_str_add(&str, 0);
    
    macro_ptr = str.str;
    next();
    c = expr_const();
    macro_ptr = NULL;
    tok_str_free(str.str);
    return c != 0;
}

#if defined(PARSE_DEBUG) || defined(PP_DEBUG)
static void tok_print(int *str)
{
    int t;
    CValue cval;

    while (1) {
        TOK_GET(t, str, cval);
        if (!t)
            break;
        printf(" %s", get_tok_str(t, &cval));
    }
    printf("\n");
}
#endif

static void parse_define(void)
{
    Sym *s, *first, **ps;
    int v, t, varg, is_vaargs, c;
    TokenString str;
    
    v = tok;
    if (v < TOK_IDENT)
        error("invalid macro name '%s'", get_tok_str(tok, &tokc));
    
    first = NULL;
    t = MACRO_OBJ;
    
    c = file->buf_ptr[0];
    if (c == '\\')
        c = handle_stray1(file->buf_ptr);
    if (c == '(') {
        next_nomacro();
        next_nomacro();
        ps = &first;
        while (tok != ')') {
            varg = tok;
            next_nomacro();
            is_vaargs = 0;
            if (varg == TOK_DOTS) {
                varg = TOK___VA_ARGS__;
                is_vaargs = 1;
            } else if (tok == TOK_DOTS && gnu_ext) {
                is_vaargs = 1;
                next_nomacro();
            }
            if (varg < TOK_IDENT)
                error("badly punctuated parameter list");
            s = sym_push2(&define_stack, varg | SYM_FIELD, is_vaargs, 0);
            *ps = s;
            ps = &s->next;
            if (tok != ',')
                break;
            next_nomacro();
        }
        t = MACRO_FUNC;
    }
    tok_str_new(&str);
    next_nomacro();
    
    while (tok != TOK_LINEFEED && tok != TOK_EOF) {
        tok_str_add2(&str, tok, &tokc);
        next_nomacro();
    }
    tok_str_add(&str, 0);
#ifdef PP_DEBUG
    printf("define %s %d: ", get_tok_str(v, NULL), t);
    tok_print(str.str);
#endif
    define_push(v, t, str.str, first);
}

static inline int hash_cached_include(int type, const char *filename)
{
    const unsigned char *s;
    unsigned int h;

    h = TOK_HASH_INIT;
    h = TOK_HASH_FUNC(h, type);
    s = filename;
    while (*s) {
        h = TOK_HASH_FUNC(h, *s);
        s++;
    }
    h &= (CACHED_INCLUDES_HASH_SIZE - 1);
    return h;
}

static CachedInclude *search_cached_include(TCCState *s1,
                                            int type, const char *filename)
{
    CachedInclude *e;
    int i, h;
    h = hash_cached_include(type, filename);
    i = s1->cached_includes_hash[h];
    for(;;) {
        if (i == 0)
            break;
        e = s1->cached_includes[i - 1];
        if (e->type == type && !strcmp(e->filename, filename))
            return e;
        i = e->hash_next;
    }
    return NULL;
}

static inline void add_cached_include(TCCState *s1, int type, 
                                      const char *filename, int ifndef_macro)
{
    CachedInclude *e;
    int h;

    if (search_cached_include(s1, type, filename))
        return;
#ifdef INC_DEBUG
    printf("adding cached '%s' %s\n", filename, get_tok_str(ifndef_macro, NULL));
#endif
    e = tcc_malloc(sizeof(CachedInclude) + strlen(filename));
    if (!e)
        return;
    e->type = type;
    strcpy(e->filename, filename);
    e->ifndef_macro = ifndef_macro;
    dynarray_add((void ***)&s1->cached_includes, &s1->nb_cached_includes, e);
    
    h = hash_cached_include(type, filename);
    e->hash_next = s1->cached_includes_hash[h];
    s1->cached_includes_hash[h] = s1->nb_cached_includes;
}

static void pragma_parse(TCCState *s1)
{
    int val;

    next();
    if (tok == TOK_pack) {
        next();
        skip('(');
        if (tok == TOK_ASM_pop) {
            next();
            if (s1->pack_stack_ptr <= s1->pack_stack) {
            stk_error:
                error("out of pack stack");
            }
            s1->pack_stack_ptr--;
        } else {
            val = 0;
            if (tok != ')') {
                if (tok == TOK_ASM_push) {
                    next();
                    if (s1->pack_stack_ptr >= s1->pack_stack + PACK_STACK_SIZE - 1)
                        goto stk_error;
                    s1->pack_stack_ptr++;
                    skip(',');
                }
                if (tok != TOK_CINT) {
                pack_error:
                    error("invalid pack pragma");
                }
                val = tokc.i;
                if (val < 1 || val > 16 || (val & (val - 1)) != 0)
                    goto pack_error;
                next();
            }
            *s1->pack_stack_ptr = val;
            skip(')');
        }
    }
}

static void preprocess(int is_bof)
{
    TCCState *s1 = tcc_state;
    int size, i, c, n, saved_parse_flags;
    char buf[1024], *q, *p;
    char buf1[1024];
    BufferedFile *f;
    Sym *s;
    CachedInclude *e;
    
    saved_parse_flags = parse_flags;
    parse_flags = PARSE_FLAG_PREPROCESS | PARSE_FLAG_TOK_NUM | 
        PARSE_FLAG_LINEFEED;
    next_nomacro();
 redo:
    switch(tok) {
    case TOK_DEFINE:
        next_nomacro();
        parse_define();
        break;
    case TOK_UNDEF:
        next_nomacro();
        s = define_find(tok);
        
        if (s)
            define_undef(s);
        break;
    case TOK_INCLUDE:
    case TOK_INCLUDE_NEXT:
        ch = file->buf_ptr[0];
        
        skip_spaces();
        if (ch == '<') {
            c = '>';
            goto read_name;
        } else if (ch == '\"') {
            c = ch;
        read_name:
            
            minp();
            q = buf;
            while (ch != c && ch != '\n' && ch != CH_EOF) {
                if ((q - buf) < sizeof(buf) - 1)
                    *q++ = ch;
                minp();
            }
            *q = '\0';
            minp();
#if 0
            
            
            while (ch1 != '\n' && ch1 != CH_EOF)
                inp();
#endif
        } else {
            next();
            buf[0] = '\0';
            if (tok == TOK_STR) {
                while (tok != TOK_LINEFEED) {
                    if (tok != TOK_STR) {
                    include_syntax:
                        error("'#include' expects \"FILENAME\" or <FILENAME>");
                    }
                    pstrcat(buf, sizeof(buf), (char *)tokc.cstr->data);
                    next();
                }
                c = '\"';
            } else {
                int len;
                while (tok != TOK_LINEFEED) {
                    pstrcat(buf, sizeof(buf), get_tok_str(tok, &tokc));
                    next();
                }
                len = strlen(buf);
                
                if (len < 2 || buf[0] != '<' || buf[len - 1] != '>')
                    goto include_syntax;
                memmove(buf, buf + 1, len - 2);
                buf[len - 2] = '\0';
                c = '>';
            }
        }

        e = search_cached_include(s1, c, buf);
        if (e && define_find(e->ifndef_macro)) {
#ifdef INC_DEBUG
            printf("%s: skipping %s\n", file->filename, buf);
#endif
        } else {
            if (c == '\"') {
                
                size = 0;
                p = strrchr(file->filename, '/');
                if (p) 
                    size = p + 1 - file->filename;
                if (size > sizeof(buf1) - 1)
                    size = sizeof(buf1) - 1;
                memcpy(buf1, file->filename, size);
                buf1[size] = '\0';
                pstrcat(buf1, sizeof(buf1), buf);
                f = tcc_open(s1, buf1);
                if (f) {
                    if (tok == TOK_INCLUDE_NEXT)
                        tok = TOK_INCLUDE;
                    else
                        goto found;
                }
            }
            if (s1->include_stack_ptr >= s1->include_stack + INCLUDE_STACK_SIZE)
                error("#include recursion too deep");
            
            n = s1->nb_include_paths + s1->nb_sysinclude_paths;
            for(i = 0; i < n; i++) {
                const char *path;
                if (i < s1->nb_include_paths)
                    path = s1->include_paths[i];
                else
                    path = s1->sysinclude_paths[i - s1->nb_include_paths];
                pstrcpy(buf1, sizeof(buf1), path);
                pstrcat(buf1, sizeof(buf1), "/");
                pstrcat(buf1, sizeof(buf1), buf);
                f = tcc_open(s1, buf1);
                if (f) {
                    if (tok == TOK_INCLUDE_NEXT)
                        tok = TOK_INCLUDE;
                    else
                        goto found;
                }
            }
            error("include file '%s' not found", buf);
            f = NULL;
        found:
#ifdef INC_DEBUG
            printf("%s: including %s\n", file->filename, buf1);
#endif
            f->inc_type = c;
            pstrcpy(f->inc_filename, sizeof(f->inc_filename), buf);
            
            
            *s1->include_stack_ptr++ = file;
            file = f;
            
            if (do_debug) {
                put_stabs(file->filename, N_BINCL, 0, 0, 0);
            }
            tok_flags |= TOK_FLAG_BOF | TOK_FLAG_BOL;
            ch = file->buf_ptr[0];
            goto the_end;
        }
        break;
    case TOK_IFNDEF:
        c = 1;
        goto do_ifdef;
    case TOK_IF:
        c = expr_preprocess();
        goto do_if;
    case TOK_IFDEF:
        c = 0;
    do_ifdef:
        next_nomacro();
        if (tok < TOK_IDENT)
            error("invalid argument for '#if%sdef'", c ? "n" : "");
        if (is_bof) {
            if (c) {
#ifdef INC_DEBUG
                printf("#ifndef %s\n", get_tok_str(tok, NULL));
#endif
                file->ifndef_macro = tok;
            }
        }
        c = (define_find(tok) != 0) ^ c;
    do_if:
        if (s1->ifdef_stack_ptr >= s1->ifdef_stack + IFDEF_STACK_SIZE)
            error("memory full");
        *s1->ifdef_stack_ptr++ = c;
        goto test_skip;
    case TOK_ELSE:
        if (s1->ifdef_stack_ptr == s1->ifdef_stack)
            error("#else without matching #if");
        if (s1->ifdef_stack_ptr[-1] & 2)
            error("#else after #else");
        c = (s1->ifdef_stack_ptr[-1] ^= 3);
        goto test_skip;
    case TOK_ELIF:
        if (s1->ifdef_stack_ptr == s1->ifdef_stack)
            error("#elif without matching #if");
        c = s1->ifdef_stack_ptr[-1];
        if (c > 1)
            error("#elif after #else");
        
        if (c == 1)
            goto skip;
        c = expr_preprocess();
        s1->ifdef_stack_ptr[-1] = c;
    test_skip:
        if (!(c & 1)) {
        skip:
            preprocess_skip();
            is_bof = 0;
            goto redo;
        }
        break;
    case TOK_ENDIF:
        if (s1->ifdef_stack_ptr <= file->ifdef_stack_ptr)
            error("#endif without matching #if");
        s1->ifdef_stack_ptr--;
        if (file->ifndef_macro &&
            s1->ifdef_stack_ptr == file->ifdef_stack_ptr) {
            file->ifndef_macro_saved = file->ifndef_macro;
            file->ifndef_macro = 0;
            while (tok != TOK_LINEFEED)
                next_nomacro();
            tok_flags |= TOK_FLAG_ENDIF;
            goto the_end;
        }
        break;
    case TOK_LINE:
        next();
        if (tok != TOK_CINT)
            error("#line");
        file->line_num = tokc.i - 1; 
        next();
        if (tok != TOK_LINEFEED) {
            if (tok != TOK_STR)
                error("#line");
            pstrcpy(file->filename, sizeof(file->filename), 
                    (char *)tokc.cstr->data);
        }
        break;
    case TOK_ERROR:
    case TOK_WARNING:
        c = tok;
        ch = file->buf_ptr[0];
        skip_spaces();
        q = buf;
        while (ch != '\n' && ch != CH_EOF) {
            if ((q - buf) < sizeof(buf) - 1)
                *q++ = ch;
            minp();
        }
        *q = '\0';
        if (c == TOK_ERROR)
            error("#error %s", buf);
        else
            warning("#warning %s", buf);
        break;
    case TOK_PRAGMA:
        pragma_parse(s1);
        break;
    default:
        if (tok == TOK_LINEFEED || tok == '!' || tok == TOK_CINT) {
        } else {
            if (!(saved_parse_flags & PARSE_FLAG_ASM_COMMENTS))
                error("invalid preprocessing directive #%s", get_tok_str(tok, &tokc));
        }
        break;
    }
    
    while (tok != TOK_LINEFEED)
        next_nomacro();
 the_end:
    parse_flags = saved_parse_flags;
}

static void parse_escape_string(CString *outstr, const uint8_t *buf, int is_long)
{
    int c, n;
    const uint8_t *p;

    p = buf;
    for(;;) {
        c = *p;
        if (c == '\0')
            break;
        if (c == '\\') {
            p++;
            
            c = *p;
            switch(c) {
            case '0': case '1': case '2': case '3':
            case '4': case '5': case '6': case '7':
                
                n = c - '0';
                p++;
                c = *p;
                if (isoct(c)) {
                    n = n * 8 + c - '0';
                    p++;
                    c = *p;
                    if (isoct(c)) {
                        n = n * 8 + c - '0';
                        p++;
                    }
                }
                c = n;
                goto add_char_nonext;
            case 'x':
                p++;
                n = 0;
                for(;;) {
                    c = *p;
                    if (c >= 'a' && c <= 'f')
                        c = c - 'a' + 10;
                    else if (c >= 'A' && c <= 'F')
                        c = c - 'A' + 10;
                    else if (isnum(c))
                        c = c - '0';
                    else
                        break;
                    n = n * 16 + c;
                    p++;
                }
                c = n;
                goto add_char_nonext;
            case 'a':
                c = '\a';
                break;
            case 'b':
                c = '\b';
                break;
            case 'f':
                c = '\f';
                break;
            case 'n':
                c = '\n';
                break;
            case 'r':
                c = '\r';
                break;
            case 't':
                c = '\t';
                break;
            case 'v':
                c = '\v';
                break;
            case 'e':
                if (!gnu_ext)
                    goto invalid_escape;
                c = 27;
                break;
            case '\'':
            case '\"':
            case '\\': 
            case '?':
                break;
            default:
            invalid_escape:
                if (c >= '!' && c <= '~')
                    warning("unknown escape sequence: \'\\%c\'", c);
                else
                    warning("unknown escape sequence: \'\\x%x\'", c);
                break;
            }
        }
        p++;
    add_char_nonext:
        if (!is_long)
            cstr_ccat(outstr, c);
        else
            cstr_wccat(outstr, c);
    }
    
    if (!is_long)
        cstr_ccat(outstr, '\0');
    else
        cstr_wccat(outstr, '\0');
}

#define BN_SIZE 2

void bn_lshift(unsigned int *bn, int shift, int or_val)
{
    int i;
    unsigned int v;
    for(i=0;i<BN_SIZE;i++) {
        v = bn[i];
        bn[i] = (v << shift) | or_val;
        or_val = v >> (32 - shift);
    }
}

void bn_zero(unsigned int *bn)
{
    int i;
    for(i=0;i<BN_SIZE;i++) {
        bn[i] = 0;
    }
}

void parse_number(const char *p)
{
    int b, t, shift, frac_bits, s, exp_val, ch;
    char *q;
    unsigned int bn[BN_SIZE];
    double d;

    
    q = token_buf;
    ch = *p++;
    t = ch;
    ch = *p++;
    *q++ = t;
    b = 10;
    if (t == '.') {
        goto float_frac_parse;
    } else if (t == '0') {
        if (ch == 'x' || ch == 'X') {
            q--;
            ch = *p++;
            b = 16;
        } else if (tcc_ext && (ch == 'b' || ch == 'B')) {
            q--;
            ch = *p++;
            b = 2;
        }
    }
    while (1) {
        if (ch >= 'a' && ch <= 'f')
            t = ch - 'a' + 10;
        else if (ch >= 'A' && ch <= 'F')
            t = ch - 'A' + 10;
        else if (isnum(ch))
            t = ch - '0';
        else
            break;
        if (t >= b)
            break;
        if (q >= token_buf + STRING_MAX_SIZE) {
        num_too_long:
            error("number too long");
        }
        *q++ = ch;
        ch = *p++;
    }
    if (ch == '.' ||
        ((ch == 'e' || ch == 'E') && b == 10) ||
        ((ch == 'p' || ch == 'P') && (b == 16 || b == 2))) {
        if (b != 10) {
            
            
            *q = '\0';
            if (b == 16)
                shift = 4;
            else 
                shift = 2;
            bn_zero(bn);
            q = token_buf;
            while (1) {
                t = *q++;
                if (t == '\0') {
                    break;
                } else if (t >= 'a') {
                    t = t - 'a' + 10;
                } else if (t >= 'A') {
                    t = t - 'A' + 10;
                } else {
                    t = t - '0';
                }
                bn_lshift(bn, shift, t);
            }
            frac_bits = 0;
            if (ch == '.') {
                ch = *p++;
                while (1) {
                    t = ch;
                    if (t >= 'a' && t <= 'f') {
                        t = t - 'a' + 10;
                    } else if (t >= 'A' && t <= 'F') {
                        t = t - 'A' + 10;
                    } else if (t >= '0' && t <= '9') {
                        t = t - '0';
                    } else {
                        break;
                    }
                    if (t >= b)
                        error("invalid digit");
                    bn_lshift(bn, shift, t);
                    frac_bits += shift;
                    ch = *p++;
                }
            }
            if (ch != 'p' && ch != 'P')
                expect("exponent");
            ch = *p++;
            s = 1;
            exp_val = 0;
            if (ch == '+') {
                ch = *p++;
            } else if (ch == '-') {
                s = -1;
                ch = *p++;
            }
            if (ch < '0' || ch > '9')
                expect("exponent digits");
            while (ch >= '0' && ch <= '9') {
                exp_val = exp_val * 10 + ch - '0';
                ch = *p++;
            }
            exp_val = exp_val * s;
            
            
            
            d = (double)bn[1] * 4294967296.0 + (double)bn[0];
            d = ldexp(d, exp_val - frac_bits);
            t = toup(ch);
            if (t == 'F') {
                ch = *p++;
                tok = TOK_CFLOAT;
                
                tokc.f = (float)d;
            } else if (t == 'L') {
                ch = *p++;
                tok = TOK_CLDOUBLE;
                
                tokc.ld = (long double)d;
            } else {
                tok = TOK_CDOUBLE;
                tokc.d = d;
            }
        } else {
            
            if (ch == '.') {
                if (q >= token_buf + STRING_MAX_SIZE)
                    goto num_too_long;
                *q++ = ch;
                ch = *p++;
            float_frac_parse:
                while (ch >= '0' && ch <= '9') {
                    if (q >= token_buf + STRING_MAX_SIZE)
                        goto num_too_long;
                    *q++ = ch;
                    ch = *p++;
                }
            }
            if (ch == 'e' || ch == 'E') {
                if (q >= token_buf + STRING_MAX_SIZE)
                    goto num_too_long;
                *q++ = ch;
                ch = *p++;
                if (ch == '-' || ch == '+') {
                    if (q >= token_buf + STRING_MAX_SIZE)
                        goto num_too_long;
                    *q++ = ch;
                    ch = *p++;
                }
                if (ch < '0' || ch > '9')
                    expect("exponent digits");
                while (ch >= '0' && ch <= '9') {
                    if (q >= token_buf + STRING_MAX_SIZE)
                        goto num_too_long;
                    *q++ = ch;
                    ch = *p++;
                }
            }
            *q = '\0';
            t = toup(ch);
            errno = 0;
            if (t == 'F') {
                ch = *p++;
                tok = TOK_CFLOAT;
                tokc.f = strtof(token_buf, NULL);
            } else if (t == 'L') {
                ch = *p++;
                tok = TOK_CLDOUBLE;
                tokc.ld = strtold(token_buf, NULL);
            } else {
                tok = TOK_CDOUBLE;
                tokc.d = strtod(token_buf, NULL);
            }
        }
    } else {
        unsigned long long n, n1;
        int lcount, ucount;

        
        *q = '\0';
        q = token_buf;
        if (b == 10 && *q == '0') {
            b = 8;
            q++;
        }
        n = 0;
        while(1) {
            t = *q++;
            
            if (t == '\0') {
                break;
            } else if (t >= 'a') {
                t = t - 'a' + 10;
            } else if (t >= 'A') {
                t = t - 'A' + 10;
            } else {
                t = t - '0';
                if (t >= b)
                    error("invalid digit");
            }
            n1 = n;
            n = n * b + t;
            
            
            if (n < n1)
                error("integer constant overflow");
        }
        
        
        if ((n & 0xffffffff00000000LL) != 0) {
            if ((n >> 63) != 0)
                tok = TOK_CULLONG;
            else
                tok = TOK_CLLONG;
        } else if (n > 0x7fffffff) {
            tok = TOK_CUINT;
        } else {
            tok = TOK_CINT;
        }
        lcount = 0;
        ucount = 0;
        for(;;) {
            t = toup(ch);
            if (t == 'L') {
                if (lcount >= 2)
                    error("three 'l's in integer constant");
                lcount++;
                if (lcount == 2) {
                    if (tok == TOK_CINT)
                        tok = TOK_CLLONG;
                    else if (tok == TOK_CUINT)
                        tok = TOK_CULLONG;
                }
                ch = *p++;
            } else if (t == 'U') {
                if (ucount >= 1)
                    error("two 'u's in integer constant");
                ucount++;
                if (tok == TOK_CINT)
                    tok = TOK_CUINT;
                else if (tok == TOK_CLLONG)
                    tok = TOK_CULLONG;
                ch = *p++;
            } else {
                break;
            }
        }
        if (tok == TOK_CINT || tok == TOK_CUINT)
            tokc.ui = n;
        else
            tokc.ull = n;
    }
}


#define PARSE2(c1, tok1, c2, tok2)              \
    case c1:                                    \
        PEEKC(c, p);                            \
        if (c == c2) {                          \
            p++;                                \
            tok = tok2;                         \
        } else {                                \
            tok = tok1;                         \
        }                                       \
        break;

static  void next_nomacro1(void)
{
    int t, c, is_long;
    TokenSym *ts;
    uint8_t *p, *p1;
    unsigned int h;

    p = file->buf_ptr;
 redo_no_start:
    c = *p;
    switch(c) {
    case ' ':
    case '\t':
    case '\f':
    case '\v':
    case '\r':
        p++;
        goto redo_no_start;
        
    case '\\':
        
        if (p >= file->buf_end) {
            file->buf_ptr = p;
            handle_eob();
            p = file->buf_ptr;
            if (p >= file->buf_end)
                goto parse_eof;
            else
                goto redo_no_start;
        } else {
            file->buf_ptr = p;
            ch = *p;
            handle_stray();
            p = file->buf_ptr;
            goto redo_no_start;
        }
    parse_eof:
        {
            TCCState *s1 = tcc_state;
            if (parse_flags & PARSE_FLAG_LINEFEED) {
                tok = TOK_LINEFEED;
            } else if (s1->include_stack_ptr == s1->include_stack ||
                       !(parse_flags & PARSE_FLAG_PREPROCESS)) {
                
                tok = TOK_EOF;
            } else {
                
                
                if (tok_flags & TOK_FLAG_ENDIF) {
#ifdef INC_DEBUG
                    printf("#endif %s\n", get_tok_str(file->ifndef_macro_saved, NULL));
#endif
                    add_cached_include(s1, file->inc_type, file->inc_filename,
                                       file->ifndef_macro_saved);
                }

                
                if (do_debug) {
                    put_stabd(N_EINCL, 0, 0);
                }
                
                tcc_close(file);
                s1->include_stack_ptr--;
                file = *s1->include_stack_ptr;
                p = file->buf_ptr;
                goto redo_no_start;
            }
        }
        break;

    case '\n':
        if (parse_flags & PARSE_FLAG_LINEFEED) {
            tok = TOK_LINEFEED;
        } else {
            file->line_num++;
            tok_flags |= TOK_FLAG_BOL;
            p++;
            goto redo_no_start;
        }
        break;

    case '#':
        
        PEEKC(c, p);
        if ((tok_flags & TOK_FLAG_BOL) && 
            (parse_flags & PARSE_FLAG_PREPROCESS)) {
            file->buf_ptr = p;
            preprocess(tok_flags & TOK_FLAG_BOF);
            p = file->buf_ptr;
            goto redo_no_start;
        } else {
            if (c == '#') {
                p++;
                tok = TOK_TWOSHARPS;
            } else {
                if (parse_flags & PARSE_FLAG_ASM_COMMENTS) {
                    p = parse_line_comment(p - 1);
                    goto redo_no_start;
                } else {
                    tok = '#';
                }
            }
        }
        break;

    case 'a': case 'b': case 'c': case 'd':
    case 'e': case 'f': case 'g': case 'h':
    case 'i': case 'j': case 'k': case 'l':
    case 'm': case 'n': case 'o': case 'p':
    case 'q': case 'r': case 's': case 't':
    case 'u': case 'v': case 'w': case 'x':
    case 'y': case 'z': 
    case 'A': case 'B': case 'C': case 'D':
    case 'E': case 'F': case 'G': case 'H':
    case 'I': case 'J': case 'K': 
    case 'M': case 'N': case 'O': case 'P':
    case 'Q': case 'R': case 'S': case 'T':
    case 'U': case 'V': case 'W': case 'X':
    case 'Y': case 'Z': 
    case '_':
    parse_ident_fast:
        p1 = p;
        h = TOK_HASH_INIT;
        h = TOK_HASH_FUNC(h, c);
        p++;
        for(;;) {
            c = *p;
            if (!isidnum_table[c])
                break;
            h = TOK_HASH_FUNC(h, c);
            p++;
        }
        if (c != '\\') {
            TokenSym **pts;
            int len;

            len = p - p1;
            h &= (TOK_HASH_SIZE - 1);
            pts = &hash_ident[h];
            for(;;) {
                ts = *pts;
                if (!ts)
                    break;
                if (ts->len == len && !memcmp(ts->str, p1, len))
                    goto token_found;
                pts = &(ts->hash_next);
            }
            ts = tok_alloc_new(pts, p1, len);
        token_found: ;
        } else {
            
            cstr_reset(&tokcstr);

            while (p1 < p) {
                cstr_ccat(&tokcstr, *p1);
                p1++;
            }
            p--;
            PEEKC(c, p);
        parse_ident_slow:
            while (isidnum_table[c]) {
                cstr_ccat(&tokcstr, c);
                PEEKC(c, p);
            }
            ts = tok_alloc(tokcstr.data, tokcstr.size);
        }
        tok = ts->tok;
        break;
    case 'L':
        t = p[1];
        if (t != '\\' && t != '\'' && t != '\"') {
            
            goto parse_ident_fast;
        } else {
            PEEKC(c, p);
            if (c == '\'' || c == '\"') {
                is_long = 1;
                goto str_const;
            } else {
                cstr_reset(&tokcstr);
                cstr_ccat(&tokcstr, 'L');
                goto parse_ident_slow;
            }
        }
        break;
    case '0': case '1': case '2': case '3':
    case '4': case '5': case '6': case '7':
    case '8': case '9':

        cstr_reset(&tokcstr);
    parse_num:
        for(;;) {
            t = c;
            cstr_ccat(&tokcstr, c);
            PEEKC(c, p);
            if (!(isnum(c) || isid(c) || c == '.' ||
                  ((c == '+' || c == '-') && 
                   (t == 'e' || t == 'E' || t == 'p' || t == 'P'))))
                break;
        }
        
        cstr_ccat(&tokcstr, '\0');
        tokc.cstr = &tokcstr;
        tok = TOK_PPNUM;
        break;
    case '.':
        
        PEEKC(c, p);
        if (isnum(c)) {
            cstr_reset(&tokcstr);
            cstr_ccat(&tokcstr, '.');
            goto parse_num;
        } else if (c == '.') {
            PEEKC(c, p);
            if (c != '.')
                expect("'.'");
            PEEKC(c, p);
            tok = TOK_DOTS;
        } else {
            tok = '.';
        }
        break;
    case '\'':
    case '\"':
        is_long = 0;
    str_const:
        {
            CString str;
            int sep;

            sep = c;

            
            cstr_new(&str);
            p = parse_pp_string(p, sep, &str);
            cstr_ccat(&str, '\0');
            
            
            cstr_reset(&tokcstr);
            parse_escape_string(&tokcstr, str.data, is_long);
            cstr_free(&str);

            if (sep == '\'') {
                int char_size;
                
                if (!is_long)
                    char_size = 1;
                else
                    char_size = sizeof(int);
                if (tokcstr.size <= char_size)
                    error("empty character constant");
                if (tokcstr.size > 2 * char_size)
                    warning("multi-character character constant");
                if (!is_long) {
                    tokc.i = *(int8_t *)tokcstr.data;
                    tok = TOK_CCHAR;
                } else {
                    tokc.i = *(int *)tokcstr.data;
                    tok = TOK_LCHAR;
                }
            } else {
                tokc.cstr = &tokcstr;
                if (!is_long)
                    tok = TOK_STR;
                else
                    tok = TOK_LSTR;
            }
        }
        break;

    case '<':
        PEEKC(c, p);
        if (c == '=') {
            p++;
            tok = TOK_LE;
        } else if (c == '<') {
            PEEKC(c, p);
            if (c == '=') {
                p++;
                tok = TOK_A_SHL;
            } else {
                tok = TOK_SHL;
            }
        } else {
            tok = TOK_LT;
        }
        break;
        
    case '>':
        PEEKC(c, p);
        if (c == '=') {
            p++;
            tok = TOK_GE;
        } else if (c == '>') {
            PEEKC(c, p);
            if (c == '=') {
                p++;
                tok = TOK_A_SAR;
            } else {
                tok = TOK_SAR;
            }
        } else {
            tok = TOK_GT;
        }
        break;
        
    case '&':
        PEEKC(c, p);
        if (c == '&') {
            p++;
            tok = TOK_LAND;
        } else if (c == '=') {
            p++;
            tok = TOK_A_AND;
        } else {
            tok = '&';
        }
        break;
        
    case '|':
        PEEKC(c, p);
        if (c == '|') {
            p++;
            tok = TOK_LOR;
        } else if (c == '=') {
            p++;
            tok = TOK_A_OR;
        } else {
            tok = '|';
        }
        break;

    case '+':
        PEEKC(c, p);
        if (c == '+') {
            p++;
            tok = TOK_INC;
        } else if (c == '=') {
            p++;
            tok = TOK_A_ADD;
        } else {
            tok = '+';
        }
        break;
        
    case '-':
        PEEKC(c, p);
        if (c == '-') {
            p++;
            tok = TOK_DEC;
        } else if (c == '=') {
            p++;
            tok = TOK_A_SUB;
        } else if (c == '>') {
            p++;
            tok = TOK_ARROW;
        } else {
            tok = '-';
        }
        break;

    PARSE2('!', '!', '=', TOK_NE)
    PARSE2('=', '=', '=', TOK_EQ)
    PARSE2('*', '*', '=', TOK_A_MUL)
    PARSE2('%', '%', '=', TOK_A_MOD)
    PARSE2('^', '^', '=', TOK_A_XOR)
        
        
    case '/':
        PEEKC(c, p);
        if (c == '*') {
            p = parse_comment(p);
            goto redo_no_start;
        } else if (c == '/') {
            p = parse_line_comment(p);
            goto redo_no_start;
        } else if (c == '=') {
            p++;
            tok = TOK_A_DIV;
        } else {
            tok = '/';
        }
        break;
        
        
    case '(':
    case ')':
    case '[':
    case ']':
    case '{':
    case '}':
    case ',':
    case ';':
    case ':':
    case '?':
    case '~':
    case '$': 
    case '@': 
        tok = c;
        p++;
        break;
    default:
        error("unrecognized character \\x%02x", c);
        break;
    }
    file->buf_ptr = p;
    tok_flags = 0;
#if defined(PARSE_DEBUG)
    printf("token = %s\n", get_tok_str(tok, &tokc));
#endif
}

static void next_nomacro(void)
{
    if (macro_ptr) {
    redo:
        tok = *macro_ptr;
        if (tok) {
            TOK_GET(tok, macro_ptr, tokc);
            if (tok == TOK_LINENUM) {
                file->line_num = tokc.i;
                goto redo;
            }
        }
    } else {
        next_nomacro1();
    }
}

static int *macro_arg_subst(Sym **nested_list, int *macro_str, Sym *args)
{
    int *st, last_tok, t, notfirst;
    Sym *s;
    CValue cval;
    TokenString str;
    CString cstr;

    tok_str_new(&str);
    last_tok = 0;
    while(1) {
        TOK_GET(t, macro_str, cval);
        if (!t)
            break;
        if (t == '#') {
            
            TOK_GET(t, macro_str, cval);
            if (!t)
                break;
            s = sym_find2(args, t);
            if (s) {
                cstr_new(&cstr);
                st = (int *)s->c;
                notfirst = 0;
                while (*st) {
                    if (notfirst)
                        cstr_ccat(&cstr, ' ');
                    TOK_GET(t, st, cval);
                    cstr_cat(&cstr, get_tok_str(t, &cval));
                    notfirst = 1;
                }
                cstr_ccat(&cstr, '\0');
#ifdef PP_DEBUG
                printf("stringize: %s\n", (char *)cstr.data);
#endif
                
                cval.cstr = &cstr;
                tok_str_add2(&str, TOK_STR, &cval);
                cstr_free(&cstr);
            } else {
                tok_str_add2(&str, t, &cval);
            }
        } else if (t >= TOK_IDENT) {
            s = sym_find2(args, t);
            if (s) {
                st = (int *)s->c;
                
                if (*macro_str == TOK_TWOSHARPS || last_tok == TOK_TWOSHARPS) {
                    if (gnu_ext && s->type.t &&
                        last_tok == TOK_TWOSHARPS && 
                        str.len >= 2 && str.str[str.len - 2] == ',') {
                        if (*st == 0) {
                            
                            str.len -= 2;
                        } else {
                            
                            str.len--;
                            goto add_var;
                        }
                    } else {
                        int t1;
                    add_var:
                        for(;;) {
                            TOK_GET(t1, st, cval);
                            if (!t1)
                                break;
                            tok_str_add2(&str, t1, &cval);
                        }
                    }
                } else {
                    macro_subst(&str, nested_list, st, NULL);
                }
            } else {
                tok_str_add(&str, t);
            }
        } else {
            tok_str_add2(&str, t, &cval);
        }
        last_tok = t;
    }
    tok_str_add(&str, 0);
    return str.str;
}

static char const ab_month_name[12][4] =
{
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static int macro_subst_tok(TokenString *tok_str,
                           Sym **nested_list, Sym *s, struct macro_level **can_read_stream)
{
    Sym *args, *sa, *sa1;
    int mstr_allocated, parlevel, *mstr, t, t1;
    TokenString str;
    char *cstrval;
    CValue cval;
    CString cstr;
    char buf[32];
    
    
    
    if (tok == TOK___LINE__) {
        snprintf(buf, sizeof(buf), "%d", file->line_num);
        cstrval = buf;
        t1 = TOK_PPNUM;
        goto add_cstr1;
    } else if (tok == TOK___FILE__) {
        cstrval = file->filename;
        goto add_cstr;
    } else if (tok == TOK___DATE__ || tok == TOK___TIME__) {
        time_t ti;
        struct tm *tm;

        time(&ti);
        tm = localtime(&ti);
        if (tok == TOK___DATE__) {
            snprintf(buf, sizeof(buf), "%s %2d %d", 
                     ab_month_name[tm->tm_mon], tm->tm_mday, tm->tm_year + 1900);
        } else {
            snprintf(buf, sizeof(buf), "%02d:%02d:%02d", 
                     tm->tm_hour, tm->tm_min, tm->tm_sec);
        }
        cstrval = buf;
    add_cstr:
        t1 = TOK_STR;
    add_cstr1:
        cstr_new(&cstr);
        cstr_cat(&cstr, cstrval);
        cstr_ccat(&cstr, '\0');
        cval.cstr = &cstr;
        tok_str_add2(tok_str, t1, &cval);
        cstr_free(&cstr);
    } else {
        mstr = (int *)s->c;
        mstr_allocated = 0;
        if (s->type.t == MACRO_FUNC) {
        redo:
            if (macro_ptr) {
                t = *macro_ptr;
                if (t == 0 && can_read_stream) {
                    struct macro_level *ml = *can_read_stream;
                    macro_ptr = NULL;
                    if (ml)
                    {
                        macro_ptr = ml->p;
                        ml->p = NULL;
                        *can_read_stream = ml -> prev;
                    }
                    goto redo;
                }
            } else {
                
                ch = file->buf_ptr[0];
                while (is_space(ch) || ch == '\n')
                    cinp();
                t = ch;
            }
            if (t != '(') 
                return -1;
                    
            
            next_nomacro();
            next_nomacro();
            args = NULL;
            sa = s->next;
            
            for(;;) {
                
                if (!args && !sa && tok == ')')
                    break;
                if (!sa)
                    error("macro '%s' used with too many args",
                          get_tok_str(s->v, 0));
                tok_str_new(&str);
                parlevel = 0;
                
                while ((parlevel > 0 || 
                        (tok != ')' && 
                         (tok != ',' || sa->type.t))) && 
                       tok != -1) {
                    if (tok == '(')
                        parlevel++;
                    else if (tok == ')')
                        parlevel--;
                    tok_str_add2(&str, tok, &tokc);
                    next_nomacro();
                }
                tok_str_add(&str, 0);
                sym_push2(&args, sa->v & ~SYM_FIELD, sa->type.t, (long)str.str);
                sa = sa->next;
                if (tok == ')') {
                    if (sa && sa->type.t && gnu_ext)
                        continue;
                    else
                        break;
                }
                if (tok != ',')
                    expect(",");
                next_nomacro();
            }
            if (sa) {
                error("macro '%s' used with too few args",
                      get_tok_str(s->v, 0));
            }

            
            mstr = macro_arg_subst(nested_list, mstr, args);
            
            sa = args;
            while (sa) {
                sa1 = sa->prev;
                tok_str_free((int *)sa->c);
                sym_free(sa);
                sa = sa1;
            }
            mstr_allocated = 1;
        }
        sym_push2(nested_list, s->v, 0, 0);
        macro_subst(tok_str, nested_list, mstr, can_read_stream);
        
        sa1 = *nested_list;
        *nested_list = sa1->prev;
        sym_free(sa1);
        if (mstr_allocated)
            tok_str_free(mstr);
    }
    return 0;
}

static  int *macro_twosharps(const int *macro_str)
{
    TokenSym *ts;
    const int *macro_ptr1, *start_macro_ptr, *ptr, *saved_macro_ptr;
    int t;
    const char *p1, *p2;
    CValue cval;
    TokenString macro_str1;
    CString cstr;
    memset(&cval, 0, sizeof(cval));
    start_macro_ptr = macro_str;
    
    for(;;) {
        macro_ptr1 = macro_str;
        TOK_GET(t, macro_str, cval);
        
        if (t == 0)
            return NULL;
        if (*macro_str == TOK_TWOSHARPS)
            break;
    }

    
    cstr_new(&cstr);
    tok_str_new(&macro_str1);
    tok = t;
    tokc = cval;

    
    for(ptr = start_macro_ptr; ptr < macro_ptr1;) {
        TOK_GET(t, ptr, cval);
        tok_str_add2(&macro_str1, t, &cval);
    }
    saved_macro_ptr = macro_ptr;
    
    macro_ptr = (int *)macro_str;
    for(;;) {
        while (*macro_ptr == TOK_TWOSHARPS) {
            macro_ptr++;
            macro_ptr1 = macro_ptr;
            t = *macro_ptr;
            if (t) {
                TOK_GET(t, macro_ptr, cval);
                cstr_reset(&cstr);
                p1 = get_tok_str(tok, &tokc);
                cstr_cat(&cstr, p1);
                p2 = get_tok_str(t, &cval);
                cstr_cat(&cstr, p2);
                cstr_ccat(&cstr, '\0');
                
                if ((tok >= TOK_IDENT || tok == TOK_PPNUM) && 
                    (t >= TOK_IDENT || t == TOK_PPNUM)) {
                    if (tok == TOK_PPNUM) {
                        
                        tokc.cstr = &cstr;
                    } else {
                        if (t == TOK_PPNUM) {
                            const char *p;
                            int c;

                            p = p2;
                            for(;;) {
                                c = *p;
                                if (c == '\0')
                                    break;
                                p++;
                                if (!isnum(c) && !isid(c))
                                    goto error_pasting;
                            }
                        }
                        ts = tok_alloc(cstr.data, strlen(cstr.data));
                        tok = ts->tok; 
                    }
                } else {
                    const char *str = cstr.data;
                    const unsigned char *q;

                    
                    
                    if (!strcmp(str, ">>=")) {
                        tok = TOK_A_SAR;
                    } else if (!strcmp(str, "<<=")) {
                        tok = TOK_A_SHL;
                    } else if (strlen(str) == 2) {
                        
                        q = tok_two_chars;
                        for(;;) {
                            if (!*q)
                                goto error_pasting;
                            if (q[0] == str[0] && q[1] == str[1])
                                break;
                            q += 3;
                        }
                        tok = q[2];
                    } else {
                    error_pasting:
                        cstr_reset(&cstr);
                        p1 = get_tok_str(tok, &tokc);
                        cstr_cat(&cstr, p1);
                        cstr_ccat(&cstr, '\0');
                        p2 = get_tok_str(t, &cval);
                        warning("pasting \"%s\" and \"%s\" does not give a valid preprocessing token", cstr.data, p2);
                        
                        tok_str_add2(&macro_str1, tok, &tokc);
                        
                        tok = t;
                        tokc = cval;
                    }
                }
            }
        }
        tok_str_add2(&macro_str1, tok, &tokc);
        next_nomacro();
        if (tok == 0)
            break;
    }
    macro_ptr = (int *)saved_macro_ptr;
    cstr_free(&cstr);
    tok_str_add(&macro_str1, 0);
    return macro_str1.str;
}


static void macro_subst(TokenString *tok_str, Sym **nested_list, 
                        const int *macro_str, struct macro_level ** can_read_stream)
{
    Sym *s;
    int *macro_str1;
    const int *ptr;
    int t, ret;
    CValue cval;
    struct macro_level ml;
    
    
    ptr = macro_str;
    macro_str1 = macro_twosharps(ptr);
    if (macro_str1) 
        ptr = macro_str1;
    while (1) {
        if (ptr == NULL)
            break;
        TOK_GET(t, ptr, cval);
        if (t == 0)
            break;
        s = define_find(t);
        if (s != NULL) {
            
            if (sym_find2(*nested_list, t))
                goto no_subst;
            ml.p = macro_ptr;
            if (can_read_stream)
                ml.prev = *can_read_stream, *can_read_stream = &ml;
            macro_ptr = (int *)ptr;
            tok = t;
            ret = macro_subst_tok(tok_str, nested_list, s, can_read_stream);
            ptr = (int *)macro_ptr;
            macro_ptr = ml.p;
            if (can_read_stream && *can_read_stream == &ml)
                    *can_read_stream = ml.prev;
            if (ret != 0)
                goto no_subst;
        } else {
        no_subst:
            tok_str_add2(tok_str, t, &cval);
        }
    }
    if (macro_str1)
        tok_str_free(macro_str1);
}

static void next(void)
{
    Sym *nested_list, *s;
    TokenString str;
    struct macro_level *ml;

 redo:
    next_nomacro();
    if (!macro_ptr) {
        if (tok >= TOK_IDENT &&
            (parse_flags & PARSE_FLAG_PREPROCESS)) {
            s = define_find(tok);
            if (s) {
                
                tok_str_new(&str);
                nested_list = NULL;
                ml = NULL;
                if (macro_subst_tok(&str, &nested_list, s, &ml) == 0) {
                    
                    tok_str_add(&str, 0);
                    macro_ptr = str.str;
                    macro_ptr_allocated = str.str;
                    goto redo;
                }
            }
        }
    } else {
        if (tok == 0) {
            
            if (unget_buffer_enabled) {
                macro_ptr = unget_saved_macro_ptr;
                unget_buffer_enabled = 0;
            } else {
                
                tok_str_free(macro_ptr_allocated);
                macro_ptr = NULL;
            }
            goto redo;
        }
    }
    
    
    if (tok == TOK_PPNUM &&
        (parse_flags & PARSE_FLAG_TOK_NUM)) {
        parse_number((char *)tokc.cstr->data);
    }
}

static inline void unget_tok(int last_tok)
{
    int i, n;
    int *q;
    unget_saved_macro_ptr = macro_ptr;
    unget_buffer_enabled = 1;
    q = unget_saved_buffer;
    macro_ptr = q;
    *q++ = tok;
    n = tok_ext_size(tok) - 1;
    for(i=0;i<n;i++)
        *q++ = tokc.tab[i];
    *q = 0; 
    tok = last_tok;
}


void swap(int *p, int *q)
{
    int t;
    t = *p;
    *p = *q;
    *q = t;
}

void vsetc(CType *type, int r, CValue *vc)
{
    int v;

    if (vtop >= vstack + (VSTACK_SIZE - 1))
        error("memory full");
    if (vtop >= vstack) {
        v = vtop->r & VT_VALMASK;
        if (v == VT_CMP || (v & ~1) == VT_JMP)
            gv(RC_INT);
    }
    vtop++;
    vtop->type = *type;
    vtop->r = r;
    vtop->r2 = VT_CONST;
    vtop->c = *vc;
}

void vpushi(int v)
{
    CValue cval;
    cval.i = v;
    vsetc(&int_type, VT_CONST, &cval);
}

static Sym *get_sym_ref(CType *type, Section *sec, 
                        unsigned long offset, unsigned long size)
{
    int v;
    Sym *sym;

    v = anon_sym++;
    sym = global_identifier_push(v, type->t | VT_STATIC, 0);
    sym->type.ref = type->ref;
    sym->r = VT_CONST | VT_SYM;
    put_extern_sym(sym, sec, offset, size);
    return sym;
}

static void vpush_ref(CType *type, Section *sec, unsigned long offset, unsigned long size)
{
    CValue cval;

    cval.ul = 0;
    vsetc(type, VT_CONST | VT_SYM, &cval);
    vtop->sym = get_sym_ref(type, sec, offset, size);
}

static Sym *external_global_sym(int v, CType *type, int r)
{
    Sym *s;

    s = sym_find(v);
    if (!s) {
        
        s = global_identifier_push(v, type->t | VT_EXTERN, 0);
        s->type.ref = type->ref;
        s->r = r | VT_CONST | VT_SYM;
    }
    return s;
}

static Sym *external_sym(int v, CType *type, int r)
{
    Sym *s;

    s = sym_find(v);
    if (!s) {
        
        s = sym_push(v, type, r | VT_CONST | VT_SYM, 0);
        s->type.t |= VT_EXTERN;
    } else {
        if (!is_compatible_types(&s->type, type))
            error("incompatible types for redefinition of '%s'", 
                  get_tok_str(v, NULL));
    }
    return s;
}

static void vpush_global_sym(CType *type, int v)
{
    Sym *sym;
    CValue cval;

    sym = external_global_sym(v, type, 0);
    cval.ul = 0;
    vsetc(type, VT_CONST | VT_SYM, &cval);
    vtop->sym = sym;
}

void vset(CType *type, int r, int v)
{
    CValue cval;

    cval.i = v;
    vsetc(type, r, &cval);
}

void vseti(int r, int v)
{
    CType type;
    type.t = VT_INT;
    vset(&type, r, v);
}

void vswap(void)
{
    SValue tmp;

    tmp = vtop[0];
    vtop[0] = vtop[-1];
    vtop[-1] = tmp;
}

void vpushv(SValue *v)
{
    if (vtop >= vstack + (VSTACK_SIZE - 1))
        error("memory full");
    vtop++;
    *vtop = *v;
}

void vdup(void)
{
    vpushv(vtop);
}

void save_reg(int r)
{
    int l, saved, size, align;
    SValue *p, sv;
    CType *type;

    
    saved = 0;
    l = 0;
    for(p=vstack;p<=vtop;p++) {
        if ((p->r & VT_VALMASK) == r ||
            (p->r2 & VT_VALMASK) == r) {
            
            if (!saved) {
                
                r = p->r & VT_VALMASK;
                
                type = &p->type;
                if ((p->r & VT_LVAL) || 
                    (!is_float(type->t) && (type->t & VT_BTYPE) != VT_LLONG))
                    type = &int_type;
                size = type_size(type, &align);
                loc = (loc - size) & -align;
                sv.type.t = type->t;
                sv.r = VT_LOCAL | VT_LVAL;
                sv.c.ul = loc;
                store(r, &sv);
#ifdef TCC_TARGET_I386
                
                if (r == TREG_ST0) {
                    o(0xd9dd); 
                }
#endif
                
                if ((type->t & VT_BTYPE) == VT_LLONG) {
                    sv.c.ul += 4;
                    store(p->r2, &sv);
                }
                l = loc;
                saved = 1;
            }
            
            if (p->r & VT_LVAL) {
                p->r = (p->r & ~(VT_VALMASK | VT_BOUNDED)) | VT_LLOCAL;
            } else {
                p->r = lvalue_type(p->type.t) | VT_LOCAL;
            }
            p->r2 = VT_CONST;
            p->c.ul = l;
        }
    }
}

int get_reg_ex(int rc, int rc2) 
{
    int r;
    SValue *p;
    
    for(r=0;r<NB_REGS;r++) {
        if (reg_classes[r] & rc2) {
            int n;
            n=0;
            for(p = vstack; p <= vtop; p++) {
                if ((p->r & VT_VALMASK) == r ||
                    (p->r2 & VT_VALMASK) == r)
                    n++;
            }
            if (n <= 1)
                return r;
        }
    }
    return get_reg(rc);
}

int get_reg(int rc)
{
    int r;
    SValue *p;

    
    for(r=0;r<NB_REGS;r++) {
        if (reg_classes[r] & rc) {
            for(p=vstack;p<=vtop;p++) {
                if ((p->r & VT_VALMASK) == r ||
                    (p->r2 & VT_VALMASK) == r)
                    goto notfound;
            }
            return r;
        }
    notfound: ;
    }
    
    for(p=vstack;p<=vtop;p++) {
        r = p->r & VT_VALMASK;
        if (r < VT_CONST && (reg_classes[r] & rc))
            goto save_found;
        
        r = p->r2 & VT_VALMASK;
        if (r < VT_CONST && (reg_classes[r] & rc)) {
        save_found:
            save_reg(r);
            return r;
        }
    }
    
    return -1;
}

void save_regs(int n)
{
    int r;
    SValue *p, *p1;
    p1 = vtop - n;
    for(p = vstack;p <= p1; p++) {
        r = p->r & VT_VALMASK;
        if (r < VT_CONST) {
            save_reg(r);
        }
    }
}

void move_reg(int r, int s)
{
    SValue sv;

    if (r != s) {
        save_reg(r);
        sv.type.t = VT_INT;
        sv.r = s;
        sv.c.ul = 0;
        load(r, &sv);
    }
}

void gaddrof(void)
{
    vtop->r &= ~VT_LVAL;
    
    if ((vtop->r & VT_VALMASK) == VT_LLOCAL)
        vtop->r = (vtop->r & ~(VT_VALMASK | VT_LVAL_TYPE)) | VT_LOCAL | VT_LVAL;
}

#ifdef CONFIG_TCC_BCHECK
void gbound(void)
{
    int lval_type;
    CType type1;

    vtop->r &= ~VT_MUSTBOUND;
    
    if (vtop->r & VT_LVAL) {
        
        if (!(vtop->r & VT_BOUNDED)) {
            lval_type = vtop->r & (VT_LVAL_TYPE | VT_LVAL);
            
            type1 = vtop->type;
            vtop->type.t = VT_INT;
            gaddrof();
            vpushi(0);
            gen_bounded_ptr_add();
            vtop->r |= lval_type;
            vtop->type = type1;
        }
        
        gen_bounded_ptr_deref();
    }
}
#endif

int gv(int rc)
{
    int r, r2, rc2, bit_pos, bit_size, size, align, i;
    unsigned long long ll;

    
    if (vtop->type.t & VT_BITFIELD) {
        bit_pos = (vtop->type.t >> VT_STRUCT_SHIFT) & 0x3f;
        bit_size = (vtop->type.t >> (VT_STRUCT_SHIFT + 6)) & 0x3f;
        
        vtop->type.t &= ~(VT_BITFIELD | (-1 << VT_STRUCT_SHIFT));
        
        vpushi(32 - (bit_pos + bit_size));
        gen_op(TOK_SHL);
        vpushi(32 - bit_size);
        
        gen_op(TOK_SAR);
        r = gv(rc);
    } else {
        if (is_float(vtop->type.t) && 
            (vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST) {
            Sym *sym;
            int *ptr;
            unsigned long offset;
            
            
            size = type_size(&vtop->type, &align);
            offset = (data_section->data_offset + align - 1) & -align;
            data_section->data_offset = offset;
            
            ptr = section_ptr_add(data_section, size);
            size = size >> 2;
            for(i=0;i<size;i++)
                ptr[i] = vtop->c.tab[i];
            sym = get_sym_ref(&vtop->type, data_section, offset, size << 2);
            vtop->r |= VT_LVAL | VT_SYM;
            vtop->sym = sym;
            vtop->c.ul = 0;
        }
#ifdef CONFIG_TCC_BCHECK
        if (vtop->r & VT_MUSTBOUND) 
            gbound();
#endif

        r = vtop->r & VT_VALMASK;
        if (r >= VT_CONST || 
            (vtop->r & VT_LVAL) ||
            !(reg_classes[r] & rc) ||
            ((vtop->type.t & VT_BTYPE) == VT_LLONG && 
             !(reg_classes[vtop->r2] & rc))) {
            r = get_reg(rc);
            if ((vtop->type.t & VT_BTYPE) == VT_LLONG) {
                if ((vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST) {
                    
                    ll = vtop->c.ull;
                    vtop->c.ui = ll; 
                    load(r, vtop);
                    vtop->r = r; 
                    vpushi(ll >> 32); 
                } else if (r >= VT_CONST || 
                           (vtop->r & VT_LVAL)) {
                    save_regs(1);
                    
                    load(r, vtop);
                    vdup();
                    vtop[-1].r = r; 
                    
                    vtop->type.t = VT_INT;
                    gaddrof();
                    vpushi(4);
                    gen_op('+');
                    vtop->r |= VT_LVAL;
                } else {
                    
                    load(r, vtop);
                    vdup();
                    vtop[-1].r = r; 
                    vtop->r = vtop[-1].r2;
                }
                
                rc2 = RC_INT;
                if (rc == RC_IRET)
                    rc2 = RC_LRET;
                r2 = get_reg(rc2);
                load(r2, vtop);
                vpop();
                
                vtop->r2 = r2;
            } else if ((vtop->r & VT_LVAL) && !is_float(vtop->type.t)) {
                int t1, t;
                t = vtop->type.t;
                t1 = t;
                
                if (vtop->r & VT_LVAL_BYTE)
                    t = VT_BYTE;
                else if (vtop->r & VT_LVAL_SHORT)
                    t = VT_SHORT;
                if (vtop->r & VT_LVAL_UNSIGNED)
                    t |= VT_UNSIGNED;
                vtop->type.t = t;
                load(r, vtop);
                
                vtop->type.t = t1;
            } else {
                
                load(r, vtop);
            }
        }
        vtop->r = r;
#ifdef TCC_TARGET_C67
        
        if ((vtop->type.t & VT_BTYPE) == VT_DOUBLE) 
            vtop->r2 = r+1;
#endif
    }
    return r;
}

void gv2(int rc1, int rc2)
{
    int v;

    v = vtop[0].r & VT_VALMASK;
    if (v != VT_CMP && (v & ~1) != VT_JMP && rc1 <= rc2) {
        vswap();
        gv(rc1);
        vswap();
        gv(rc2);
        
        if ((vtop[-1].r & VT_VALMASK) >= VT_CONST) {
            vswap();
            gv(rc1);
            vswap();
        }
    } else {
        gv(rc2);
        vswap();
        gv(rc1);
        vswap();
        
        if ((vtop[0].r & VT_VALMASK) >= VT_CONST) {
            gv(rc2);
        }
    }
}

void lexpand(void)
{
    int u;

    u = vtop->type.t & VT_UNSIGNED;
    gv(RC_INT);
    vdup();
    vtop[0].r = vtop[-1].r2;
    vtop[0].r2 = VT_CONST;
    vtop[-1].r2 = VT_CONST;
    vtop[0].type.t = VT_INT | u;
    vtop[-1].type.t = VT_INT | u;
}

#ifdef TCC_TARGET_ARM
void lexpand_nr(void)
{
    int u,v;

    u = vtop->type.t & VT_UNSIGNED;
    vdup();
    vtop->r2 = VT_CONST;
    vtop->type.t = VT_INT | u;
    v=vtop[-1].r & (VT_VALMASK | VT_LVAL);
    if (v == VT_CONST) {
      vtop[-1].c.ui = vtop->c.ull;
      vtop->c.ui = vtop->c.ull >> 32;
      vtop->r = VT_CONST;
    } else if (v == (VT_LVAL|VT_CONST) || v == (VT_LVAL|VT_LOCAL)) {
      vtop->c.ui += 4;
      vtop->r = vtop[-1].r;
    } else if (v > VT_CONST) {
      vtop--;
      lexpand();
    } else
      vtop->r = vtop[-1].r2;
    vtop[-1].r2 = VT_CONST;
    vtop[-1].type.t = VT_INT | u;
}
#endif

void lbuild(int t)
{
    gv2(RC_INT, RC_INT);
    vtop[-1].r2 = vtop[0].r;
    vtop[-1].type.t = t;
    vpop();
}

void vrotb(int n)
{
    int i;
    SValue tmp;

    tmp = vtop[-n + 1];
    for(i=-n+1;i!=0;i++)
        vtop[i] = vtop[i+1];
    vtop[0] = tmp;
}

void vrott(int n)
{
    int i;
    SValue tmp;

    tmp = vtop[0];
    for(i = 0;i < n - 1; i++)
        vtop[-i] = vtop[-i - 1];
    vtop[-n + 1] = tmp;
}

#ifdef TCC_TARGET_ARM
void vnrott(int n)
{
    int i;
    SValue tmp;

    tmp = vtop[-n + 1];
    for(i = n - 1; i > 0; i--)
        vtop[-i] = vtop[-i + 1];
    vtop[0] = tmp;
}
#endif

void vpop(void)
{
    int v;
    v = vtop->r & VT_VALMASK;
#ifdef TCC_TARGET_I386
    
    if (v == TREG_ST0 && !nocode_wanted) {
        o(0xd9dd); 
    } else
#endif
    if (v == VT_JMP || v == VT_JMPI) {
        
        gsym(vtop->c.ul);
    }
    vtop--;
}

void gv_dup(void)
{
    int rc, t, r, r1;
    SValue sv;

    t = vtop->type.t;
    if ((t & VT_BTYPE) == VT_LLONG) {
        lexpand();
        gv_dup();
        vswap();
        vrotb(3);
        gv_dup();
        vrotb(4);
        
        lbuild(t);
        vrotb(3);
        vrotb(3);
        vswap();
        lbuild(t);
        vswap();
    } else {
        
        rc = RC_INT;
        sv.type.t = VT_INT;
        if (is_float(t)) {
            rc = RC_FLOAT;
            sv.type.t = t;
        }
        r = gv(rc);
        r1 = get_reg(rc);
        sv.r = r;
        sv.c.ul = 0;
        load(r1, &sv); 
        vdup();
        
        vtop->r = r1;
    }
}

void gen_opl(int op)
{
    int t, a, b, op1, c, i;
    int func;
    SValue tmp;

    switch(op) {
    case '/':
    case TOK_PDIV:
        func = TOK___divdi3;
        goto gen_func;
    case TOK_UDIV:
        func = TOK___udivdi3;
        goto gen_func;
    case '%':
        func = TOK___moddi3;
        goto gen_func;
    case TOK_UMOD:
        func = TOK___umoddi3;
    gen_func:
        
        vpush_global_sym(&func_old_type, func);
        vrott(3);
        gfunc_call(2);
        vpushi(0);
        vtop->r = REG_IRET;
        vtop->r2 = REG_LRET;
        break;
    case '^':
    case '&':
    case '|':
    case '*':
    case '+':
    case '-':
        t = vtop->type.t;
        vswap();
        lexpand();
        vrotb(3);
        lexpand();
        
        tmp = vtop[0];
        vtop[0] = vtop[-3];
        vtop[-3] = tmp;
        tmp = vtop[-2];
        vtop[-2] = vtop[-3];
        vtop[-3] = tmp;
        vswap();
        
        if (op == '*') {
            vpushv(vtop - 1);
            vpushv(vtop - 1);
            gen_op(TOK_UMULL);
            lexpand();
            
            for(i=0;i<4;i++)
                vrotb(6);
            
            tmp = vtop[0];
            vtop[0] = vtop[-2];
            vtop[-2] = tmp;
            
            gen_op('*');
            vrotb(3);
            vrotb(3);
            gen_op('*');
            
            gen_op('+');
            gen_op('+');
        } else if (op == '+' || op == '-') {
            
            if (op == '+')
                op1 = TOK_ADDC1;
            else
                op1 = TOK_SUBC1;
            gen_op(op1);
            
            vrotb(3);
            vrotb(3);
            gen_op(op1 + 1); 
        } else {
            gen_op(op);
            
            vrotb(3);
            vrotb(3);
            
            gen_op(op);
            
        }
        
        lbuild(t);
        break;
    case TOK_SAR:
    case TOK_SHR:
    case TOK_SHL:
        if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
            t = vtop[-1].type.t;
            vswap();
            lexpand();
            vrotb(3);
            
            c = (int)vtop->c.i;
            
            vpop();
            if (op != TOK_SHL)
                vswap();
            if (c >= 32) {
                
                vpop();
                if (c > 32) {
                    vpushi(c - 32);
                    gen_op(op);
                }
                if (op != TOK_SAR) {
                    vpushi(0);
                } else {
                    gv_dup();
                    vpushi(31);
                    gen_op(TOK_SAR);
                }
                vswap();
            } else {
                vswap();
                gv_dup();
                
                vpushi(c);
                gen_op(op);
                vswap();
                vpushi(32 - c);
                if (op == TOK_SHL)
                    gen_op(TOK_SHR);
                else
                    gen_op(TOK_SHL);
                vrotb(3);
                
                vpushi(c);
                if (op == TOK_SHL)
                    gen_op(TOK_SHL);
                else
                    gen_op(TOK_SHR);
                gen_op('|');
            }
            if (op != TOK_SHL)
                vswap();
            lbuild(t);
        } else {
            
            switch(op) {
            case TOK_SAR:
                func = TOK___sardi3;
                goto gen_func;
            case TOK_SHR:
                func = TOK___shrdi3;
                goto gen_func;
            case TOK_SHL:
                func = TOK___shldi3;
                goto gen_func;
            }
        }
        break;
    default:
        
        t = vtop->type.t;
        vswap();
        lexpand();
        vrotb(3);
        lexpand();
        
        tmp = vtop[-1];
        vtop[-1] = vtop[-2];
        vtop[-2] = tmp;
        
        
        op1 = op;
        if (op1 == TOK_LT)
            op1 = TOK_LE;
        else if (op1 == TOK_GT)
            op1 = TOK_GE;
        else if (op1 == TOK_ULT)
            op1 = TOK_ULE;
        else if (op1 == TOK_UGT)
            op1 = TOK_UGE;
        a = 0;
        b = 0;
        gen_op(op1);
        if (op1 != TOK_NE) {
            a = gtst(1, 0);
        }
        if (op != TOK_EQ) {
            
            
            if (a == 0) {
                b = gtst(0, 0);
            } else {
#if defined(TCC_TARGET_I386)
                b = psym(0x850f, 0);
#elif defined(TCC_TARGET_ARM)
		b = ind;
		o(0x1A000000 | encbranch(ind, 0, 1));
#elif defined(TCC_TARGET_C67)
                error("not implemented");
#else
#error not supported
#endif
            }
        }
        
        op1 = op;
        if (op1 == TOK_LT)
            op1 = TOK_ULT;
        else if (op1 == TOK_LE)
            op1 = TOK_ULE;
        else if (op1 == TOK_GT)
            op1 = TOK_UGT;
        else if (op1 == TOK_GE)
            op1 = TOK_UGE;
        gen_op(op1);
        a = gtst(1, a);
        gsym(b);
        vseti(VT_JMPI, a);
        break;
    }
}

void gen_opic(int op)
{
    int fc, c1, c2, n;
    SValue *v1, *v2;

    v1 = vtop - 1;
    v2 = vtop;
    
    c1 = (v1->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
    c2 = (v2->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
    if (c1 && c2) {
        fc = v2->c.i;
        switch(op) {
        case '+': v1->c.i += fc; break;
        case '-': v1->c.i -= fc; break;
        case '&': v1->c.i &= fc; break;
        case '^': v1->c.i ^= fc; break;
        case '|': v1->c.i |= fc; break;
        case '*': v1->c.i *= fc; break;

        case TOK_PDIV:
        case '/':
        case '%':
        case TOK_UDIV:
        case TOK_UMOD:
            
            if (fc == 0) {
                if (const_wanted)
                    error("division by zero in constant");
                goto general_case;
            }
            switch(op) {
            default: v1->c.i /= fc; break;
            case '%': v1->c.i %= fc; break;
            case TOK_UDIV: v1->c.i = (unsigned)v1->c.i / fc; break;
            case TOK_UMOD: v1->c.i = (unsigned)v1->c.i % fc; break;
            }
            break;
        case TOK_SHL: v1->c.i <<= fc; break;
        case TOK_SHR: v1->c.i = (unsigned)v1->c.i >> fc; break;
        case TOK_SAR: v1->c.i >>= fc; break;
            
        case TOK_ULT: v1->c.i = (unsigned)v1->c.i < (unsigned)fc; break;
        case TOK_UGE: v1->c.i = (unsigned)v1->c.i >= (unsigned)fc; break;
        case TOK_EQ: v1->c.i = v1->c.i == fc; break;
        case TOK_NE: v1->c.i = v1->c.i != fc; break;
        case TOK_ULE: v1->c.i = (unsigned)v1->c.i <= (unsigned)fc; break;
        case TOK_UGT: v1->c.i = (unsigned)v1->c.i > (unsigned)fc; break;
        case TOK_LT: v1->c.i = v1->c.i < fc; break;
        case TOK_GE: v1->c.i = v1->c.i >= fc; break;
        case TOK_LE: v1->c.i = v1->c.i <= fc; break;
        case TOK_GT: v1->c.i = v1->c.i > fc; break;
            
        case TOK_LAND: v1->c.i = v1->c.i && fc; break;
        case TOK_LOR: v1->c.i = v1->c.i || fc; break;
        default:
            goto general_case;
        }
        vtop--;
    } else {
        
        if (c1 && (op == '+' || op == '&' || op == '^' || 
                   op == '|' || op == '*')) {
            vswap();
            swap(&c1, &c2);
        }
        fc = vtop->c.i;
        if (c2 && (((op == '*' || op == '/' || op == TOK_UDIV || 
                     op == TOK_PDIV) && 
                    fc == 1) ||
                   ((op == '+' || op == '-' || op == '|' || op == '^' || 
                     op == TOK_SHL || op == TOK_SHR || op == TOK_SAR) && 
                    fc == 0) ||
                   (op == '&' && 
                    fc == -1))) {
            
            vtop--;
        } else if (c2 && (op == '*' || op == TOK_PDIV || op == TOK_UDIV)) {
            
            if (fc > 0 && (fc & (fc - 1)) == 0) {
                n = -1;
                while (fc) {
                    fc >>= 1;
                    n++;
                }
                vtop->c.i = n;
                if (op == '*')
                    op = TOK_SHL;
                else if (op == TOK_PDIV)
                    op = TOK_SAR;
                else
                    op = TOK_SHR;
            }
            goto general_case;
        } else if (c2 && (op == '+' || op == '-') &&
                   (vtop[-1].r & (VT_VALMASK | VT_LVAL | VT_SYM)) == 
                   (VT_CONST | VT_SYM)) {
            
            if (op == '-')
                fc = -fc;
            vtop--;
            vtop->c.i += fc;
        } else {
        general_case:
            if (!nocode_wanted) {
                
                gen_opi(op);
            } else {
                vtop--;
            }
        }
    }
}

void gen_opif(int op)
{
    int c1, c2;
    SValue *v1, *v2;
    long double f1, f2;

    v1 = vtop - 1;
    v2 = vtop;
    
    c1 = (v1->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
    c2 = (v2->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
    if (c1 && c2) {
        if (v1->type.t == VT_FLOAT) {
            f1 = v1->c.f;
            f2 = v2->c.f;
        } else if (v1->type.t == VT_DOUBLE) {
            f1 = v1->c.d;
            f2 = v2->c.d;
        } else {
            f1 = v1->c.ld;
            f2 = v2->c.ld;
        }

        if (!ieee_finite(f1) || !ieee_finite(f2))
            goto general_case;

        switch(op) {
        case '+': f1 += f2; break;
        case '-': f1 -= f2; break;
        case '*': f1 *= f2; break;
        case '/': 
            if (f2 == 0.0) {
                if (const_wanted)
                    error("division by zero in constant");
                goto general_case;
            }
            f1 /= f2; 
            break;
            
        default:
            goto general_case;
        }
        
        if (v1->type.t == VT_FLOAT) {
            v1->c.f = f1;
        } else if (v1->type.t == VT_DOUBLE) {
            v1->c.d = f1;
        } else {
            v1->c.ld = f1;
        }
        vtop--;
    } else {
    general_case:
        if (!nocode_wanted) {
            gen_opf(op);
        } else {
            vtop--;
        }
    }
}

static inline CType *pointed_type(CType *type)
{
    return &type->ref->type;
}

static int pointed_size(CType *type)
{
    int align;
    return type_size(pointed_type(type), &align);
}

static inline int is_null_pointer(SValue *p)
{
    if ((p->r & (VT_VALMASK | VT_LVAL | VT_SYM)) != VT_CONST)
        return 0;
    return ((p->type.t & VT_BTYPE) == VT_INT && p->c.i == 0) ||
        ((p->type.t & VT_BTYPE) == VT_LLONG && p->c.ll == 0);
}

static inline int is_integer_btype(int bt)
{
    return (bt == VT_BYTE || bt == VT_SHORT || 
            bt == VT_INT || bt == VT_LLONG);
}

static void check_comparison_pointer_types(SValue *p1, SValue *p2, int op)
{
    CType *type1, *type2, tmp_type1, tmp_type2;
    int bt1, bt2;
    
    
    if (is_null_pointer(p1) || is_null_pointer(p2))
        return;
    type1 = &p1->type;
    type2 = &p2->type;
    bt1 = type1->t & VT_BTYPE;
    bt2 = type2->t & VT_BTYPE;
    
    if ((is_integer_btype(bt1) || is_integer_btype(bt2)) && op != '-') {
        warning("comparison between pointer and integer");
        return;
    }

    
    if (bt1 == VT_PTR) {
        type1 = pointed_type(type1);
    } else if (bt1 != VT_FUNC) 
        goto invalid_operands;

    if (bt2 == VT_PTR) {
        type2 = pointed_type(type2);
    } else if (bt2 != VT_FUNC) { 
    invalid_operands:
        error("invalid operands to binary %s", get_tok_str(op, NULL));
    }
    if ((type1->t & VT_BTYPE) == VT_VOID || 
        (type2->t & VT_BTYPE) == VT_VOID)
        return;
    tmp_type1 = *type1;
    tmp_type2 = *type2;
    tmp_type1.t &= ~(VT_UNSIGNED | VT_CONSTANT | VT_VOLATILE);
    tmp_type2.t &= ~(VT_UNSIGNED | VT_CONSTANT | VT_VOLATILE);
    if (!is_compatible_types(&tmp_type1, &tmp_type2)) {
        
        if (op == '-')
            goto invalid_operands;
        else
            warning("comparison of distinct pointer types lacks a cast");
    }
}

void gen_op(int op)
{
    int u, t1, t2, bt1, bt2, t;
    CType type1;

    t1 = vtop[-1].type.t;
    t2 = vtop[0].type.t;
    bt1 = t1 & VT_BTYPE;
    bt2 = t2 & VT_BTYPE;
        
    if (bt1 == VT_PTR || bt2 == VT_PTR) {
        
        
        if (op >= TOK_ULT && op <= TOK_GT) {
            check_comparison_pointer_types(vtop - 1, vtop, op);
            
            t = VT_INT | VT_UNSIGNED;
            goto std_op;
        }
        
        if (bt1 == VT_PTR && bt2 == VT_PTR) {
            if (op != '-')
                error("cannot use pointers here");
            check_comparison_pointer_types(vtop - 1, vtop, op);
            
            u = pointed_size(&vtop[-1].type);
            gen_opic(op);
            
            vtop->type.t = VT_INT; 
            vpushi(u);
            gen_op(TOK_PDIV);
        } else {
            
            if (op != '-' && op != '+')
                error("cannot use pointers here");
            
            if (bt2 == VT_PTR) {
                vswap();
                swap(&t1, &t2);
            }
            type1 = vtop[-1].type;
            
            vpushi(pointed_size(&vtop[-1].type));
            gen_op('*');
#ifdef CONFIG_TCC_BCHECK
            if (do_bounds_check && !const_wanted) {
                if (op == '-') {
                    vpushi(0);
                    vswap();
                    gen_op('-');
                }
                gen_bounded_ptr_add();
            } else
#endif
            {
                gen_opic(op);
            }
            
            vtop->type = type1;
        }
    } else if (is_float(bt1) || is_float(bt2)) {
        
        if (bt1 == VT_LDOUBLE || bt2 == VT_LDOUBLE) {
            t = VT_LDOUBLE;
        } else if (bt1 == VT_DOUBLE || bt2 == VT_DOUBLE) {
            t = VT_DOUBLE;
        } else {
            t = VT_FLOAT;
        }
        
        if (op != '+' && op != '-' && op != '*' && op != '/' &&
            (op < TOK_ULT || op > TOK_GT))
            error("invalid operands for binary operation");
        goto std_op;
    } else if (bt1 == VT_LLONG || bt2 == VT_LLONG) {
        
        t = VT_LLONG;
        
        if ((t1 & (VT_BTYPE | VT_UNSIGNED)) == (VT_LLONG | VT_UNSIGNED) ||
            (t2 & (VT_BTYPE | VT_UNSIGNED)) == (VT_LLONG | VT_UNSIGNED))
            t |= VT_UNSIGNED;
        goto std_op;
    } else {
        
        t = VT_INT;
        
        if ((t1 & (VT_BTYPE | VT_UNSIGNED)) == (VT_INT | VT_UNSIGNED) ||
            (t2 & (VT_BTYPE | VT_UNSIGNED)) == (VT_INT | VT_UNSIGNED))
            t |= VT_UNSIGNED;
    std_op:
        if (t & VT_UNSIGNED) {
            if (op == TOK_SAR)
                op = TOK_SHR;
            else if (op == '/')
                op = TOK_UDIV;
            else if (op == '%')
                op = TOK_UMOD;
            else if (op == TOK_LT)
                op = TOK_ULT;
            else if (op == TOK_GT)
                op = TOK_UGT;
            else if (op == TOK_LE)
                op = TOK_ULE;
            else if (op == TOK_GE)
                op = TOK_UGE;
        }
        vswap();
        type1.t = t;
        gen_cast(&type1);
        vswap();
        if (op == TOK_SHR || op == TOK_SAR || op == TOK_SHL)
            type1.t = VT_INT;
        gen_cast(&type1);
        if (is_float(t))
            gen_opif(op);
        else if ((t & VT_BTYPE) == VT_LLONG)
            gen_opl(op);
        else
            gen_opic(op);
        if (op >= TOK_ULT && op <= TOK_GT) {
            
            vtop->type.t = VT_INT;
        } else {
            vtop->type.t = t;
        }
    }
}

void gen_cvt_itof1(int t)
{
    if ((vtop->type.t & (VT_BTYPE | VT_UNSIGNED)) == 
        (VT_LLONG | VT_UNSIGNED)) {

        if (t == VT_FLOAT)
            vpush_global_sym(&func_old_type, TOK___ulltof);
        else if (t == VT_DOUBLE)
            vpush_global_sym(&func_old_type, TOK___ulltod);
        else
            vpush_global_sym(&func_old_type, TOK___ulltold);
        vrott(2);
        gfunc_call(1);
        vpushi(0);
        vtop->r = REG_FRET;
    } else {
        gen_cvt_itof(t);
    }
}

void gen_cvt_ftoi1(int t)
{
    int st;

    if (t == (VT_LLONG | VT_UNSIGNED)) {
        
        st = vtop->type.t & VT_BTYPE;
        if (st == VT_FLOAT)
            vpush_global_sym(&func_old_type, TOK___fixunssfdi);
        else if (st == VT_DOUBLE)
            vpush_global_sym(&func_old_type, TOK___fixunsdfdi);
        else
            vpush_global_sym(&func_old_type, TOK___fixunsxfdi);
        vrott(2);
        gfunc_call(1);
        vpushi(0);
        vtop->r = REG_IRET;
        vtop->r2 = REG_LRET;
    } else {
        gen_cvt_ftoi(t);
    }
}

void force_charshort_cast(int t)
{
    int bits, dbt;
    dbt = t & VT_BTYPE;
    
    if (dbt == VT_BYTE)
        bits = 8;
    else
        bits = 16;
    if (t & VT_UNSIGNED) {
        vpushi((1 << bits) - 1);
        gen_op('&');
    } else {
        bits = 32 - bits;
        vpushi(bits);
        gen_op(TOK_SHL);
        vpushi(bits);
        gen_op(TOK_SAR);
    }
}

static void gen_cast(CType *type)
{
    int sbt, dbt, sf, df, c;

    
    if (vtop->r & VT_MUSTCAST) {
        vtop->r &= ~VT_MUSTCAST;
        force_charshort_cast(vtop->type.t);
    }

    
    if (vtop->type.t & VT_BITFIELD) {
        gv(RC_INT);
    }

    dbt = type->t & (VT_BTYPE | VT_UNSIGNED);
    sbt = vtop->type.t & (VT_BTYPE | VT_UNSIGNED);

    if (sbt != dbt && !nocode_wanted) {
        sf = is_float(sbt);
        df = is_float(dbt);
        c = (vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
        if (sf && df) {
            
            if (c) {
                
                
                if (dbt == VT_FLOAT && sbt == VT_DOUBLE) 
                    vtop->c.f = (float)vtop->c.d;
                else if (dbt == VT_FLOAT && sbt == VT_LDOUBLE) 
                    vtop->c.f = (float)vtop->c.ld;
                else if (dbt == VT_DOUBLE && sbt == VT_FLOAT) 
                    vtop->c.d = (double)vtop->c.f;
                else if (dbt == VT_DOUBLE && sbt == VT_LDOUBLE) 
                    vtop->c.d = (double)vtop->c.ld;
                else if (dbt == VT_LDOUBLE && sbt == VT_FLOAT) 
                    vtop->c.ld = (long double)vtop->c.f;
                else if (dbt == VT_LDOUBLE && sbt == VT_DOUBLE) 
                    vtop->c.ld = (long double)vtop->c.d;
            } else {
                
                gen_cvt_ftof(dbt);
            }
        } else if (df) {
            
            if (c) {
                switch(sbt) {
                case VT_LLONG | VT_UNSIGNED:
                case VT_LLONG:
                    
                    goto do_itof;
                case VT_INT | VT_UNSIGNED:
                    switch(dbt) {
                    case VT_FLOAT: vtop->c.f = (float)vtop->c.ui; break;
                    case VT_DOUBLE: vtop->c.d = (double)vtop->c.ui; break;
                    case VT_LDOUBLE: vtop->c.ld = (long double)vtop->c.ui; break;
                    }
                    break;
                default:
                    switch(dbt) {
                    case VT_FLOAT: vtop->c.f = (float)vtop->c.i; break;
                    case VT_DOUBLE: vtop->c.d = (double)vtop->c.i; break;
                    case VT_LDOUBLE: vtop->c.ld = (long double)vtop->c.i; break;
                    }
                    break;
                }
            } else {
            do_itof:
#if !defined(TCC_TARGET_ARM)
                gen_cvt_itof1(dbt);
#else
                gen_cvt_itof(dbt);
#endif
            }
        } else if (sf) {
            
            
            if (dbt != (VT_INT | VT_UNSIGNED) &&
                dbt != (VT_LLONG | VT_UNSIGNED) &&
                dbt != VT_LLONG)
                dbt = VT_INT;
            if (c) {
                switch(dbt) {
                case VT_LLONG | VT_UNSIGNED:
                case VT_LLONG:
                    
                    goto do_ftoi;
                case VT_INT | VT_UNSIGNED:
                    switch(sbt) {
                    case VT_FLOAT: vtop->c.ui = (unsigned int)vtop->c.d; break;
                    case VT_DOUBLE: vtop->c.ui = (unsigned int)vtop->c.d; break;
                    case VT_LDOUBLE: vtop->c.ui = (unsigned int)vtop->c.d; break;
                    }
                    break;
                default:
                    
                    switch(sbt) {
                    case VT_FLOAT: vtop->c.i = (int)vtop->c.d; break;
                    case VT_DOUBLE: vtop->c.i = (int)vtop->c.d; break;
                    case VT_LDOUBLE: vtop->c.i = (int)vtop->c.d; break;
                    }
                    break;
                }
            } else {
            do_ftoi:
                gen_cvt_ftoi1(dbt);
            }
            if (dbt == VT_INT && (type->t & (VT_BTYPE | VT_UNSIGNED)) != dbt) {
                
                vtop->type.t = dbt;
                gen_cast(type);
            }
        } else if ((dbt & VT_BTYPE) == VT_LLONG) {
            if ((sbt & VT_BTYPE) != VT_LLONG) {
                
                if (c) {
                    if (sbt == (VT_INT | VT_UNSIGNED))
                        vtop->c.ll = vtop->c.ui;
                    else
                        vtop->c.ll = vtop->c.i;
                } else {
                    
                    gv(RC_INT);
                    
                    if (sbt == (VT_INT | VT_UNSIGNED)) {
                        vpushi(0);
                        gv(RC_INT);
                    } else {
                        gv_dup();
                        vpushi(31);
                        gen_op(TOK_SAR);
                    }
                    
                    vtop[-1].r2 = vtop->r;
                    vpop();
                }
            }
        } else if (dbt == VT_BOOL) {
            
            vpushi(0);
            gen_op(TOK_NE);
        } else if ((dbt & VT_BTYPE) == VT_BYTE || 
                   (dbt & VT_BTYPE) == VT_SHORT) {
            force_charshort_cast(dbt);
        } else if ((dbt & VT_BTYPE) == VT_INT) {
            
            if (sbt == VT_LLONG) {
                
                lexpand();
                vpop();
            } 
        }
    }
    vtop->type = *type;
}

static int type_size(CType *type, int *a)
{
    Sym *s;
    int bt;

    bt = type->t & VT_BTYPE;
    if (bt == VT_STRUCT) {
        
        s = type->ref;
        *a = s->r;
        return s->c;
    } else if (bt == VT_PTR) {
        if (type->t & VT_ARRAY) {
            s = type->ref;
            return type_size(&s->type, a) * s->c;
        } else {
            *a = PTR_SIZE;
            return PTR_SIZE;
        }
    } else if (bt == VT_LDOUBLE) {
        *a = LDOUBLE_ALIGN;
        return LDOUBLE_SIZE;
    } else if (bt == VT_DOUBLE || bt == VT_LLONG) {
#ifdef TCC_TARGET_I386
        *a = 4;
#else
        *a = 8;
#endif
        return 8;
    } else if (bt == VT_INT || bt == VT_ENUM || bt == VT_FLOAT) {
        *a = 4;
        return 4;
    } else if (bt == VT_SHORT) {
        *a = 2;
        return 2;
    } else {
        
        *a = 1;
        return 1;
    }
}

static void mk_pointer(CType *type)
{
    Sym *s;
    s = sym_push(SYM_FIELD, type, 0, -1);
    type->t = VT_PTR | (type->t & ~VT_TYPE);
    type->ref = s;
}

static int is_compatible_func(CType *type1, CType *type2)
{
    Sym *s1, *s2;

    s1 = type1->ref;
    s2 = type2->ref;
    if (!is_compatible_types(&s1->type, &s2->type))
        return 0;
    
    if (s1->r != s2->r)
        return 0;
    
    if (s1->c == FUNC_OLD || s2->c == FUNC_OLD)
        return 1;
    if (s1->c != s2->c)
        return 0;
    while (s1 != NULL) {
        if (s2 == NULL)
            return 0;
        if (!is_compatible_types(&s1->type, &s2->type))
            return 0;
        s1 = s1->next;
        s2 = s2->next;
    }
    if (s2)
        return 0;
    return 1;
}

static int is_compatible_types(CType *type1, CType *type2)
{
    int bt1, t1, t2;

    t1 = type1->t & VT_TYPE;
    t2 = type2->t & VT_TYPE;
    
    if (t1 != t2)
        return 0;
    
    bt1 = t1 & VT_BTYPE;
    if (bt1 == VT_PTR) {
        type1 = pointed_type(type1);
        type2 = pointed_type(type2);
        return is_compatible_types(type1, type2);
    } else if (bt1 == VT_STRUCT) {
        return (type1->ref == type2->ref);
    } else if (bt1 == VT_FUNC) {
        return is_compatible_func(type1, type2);
    } else {
        return 1;
    }
}

void type_to_str(char *buf, int buf_size, 
                 CType *type, const char *varstr)
{
    int bt, v, t;
    Sym *s, *sa;
    char buf1[256];
    const char *tstr;

    t = type->t & VT_TYPE;
    bt = t & VT_BTYPE;
    buf[0] = '\0';
    if (t & VT_CONSTANT)
        pstrcat(buf, buf_size, "const ");
    if (t & VT_VOLATILE)
        pstrcat(buf, buf_size, "volatile ");
    if (t & VT_UNSIGNED)
        pstrcat(buf, buf_size, "unsigned ");
    switch(bt) {
    case VT_VOID:
        tstr = "void";
        goto add_tstr;
    case VT_BOOL:
        tstr = "_Bool";
        goto add_tstr;
    case VT_BYTE:
        tstr = "char";
        goto add_tstr;
    case VT_SHORT:
        tstr = "short";
        goto add_tstr;
    case VT_INT:
        tstr = "int";
        goto add_tstr;
    case VT_LONG:
        tstr = "long";
        goto add_tstr;
    case VT_LLONG:
        tstr = "long long";
        goto add_tstr;
    case VT_FLOAT:
        tstr = "float";
        goto add_tstr;
    case VT_DOUBLE:
        tstr = "double";
        goto add_tstr;
    case VT_LDOUBLE:
        tstr = "long double";
    add_tstr:
        pstrcat(buf, buf_size, tstr);
        break;
    case VT_ENUM:
    case VT_STRUCT:
        if (bt == VT_STRUCT)
            tstr = "struct ";
        else
            tstr = "enum ";
        pstrcat(buf, buf_size, tstr);
        v = type->ref->v & ~SYM_STRUCT;
        if (v >= SYM_FIRST_ANOM)
            pstrcat(buf, buf_size, "<anonymous>");
        else
            pstrcat(buf, buf_size, get_tok_str(v, NULL));
        break;
    case VT_FUNC:
        s = type->ref;
        type_to_str(buf, buf_size, &s->type, varstr);
        pstrcat(buf, buf_size, "(");
        sa = s->next;
        while (sa != NULL) {
            type_to_str(buf1, sizeof(buf1), &sa->type, NULL);
            pstrcat(buf, buf_size, buf1);
            sa = sa->next;
            if (sa)
                pstrcat(buf, buf_size, ", ");
        }
        pstrcat(buf, buf_size, ")");
        goto no_var;
    case VT_PTR:
        s = type->ref;
        pstrcpy(buf1, sizeof(buf1), "*");
        if (varstr)
            pstrcat(buf1, sizeof(buf1), varstr);
        type_to_str(buf, buf_size, &s->type, buf1);
        goto no_var;
    }
    if (varstr) {
        pstrcat(buf, buf_size, " ");
        pstrcat(buf, buf_size, varstr);
    }
 no_var: ;
}

static void gen_assign_cast(CType *dt)
{
    CType *st, *type1, *type2, tmp_type1, tmp_type2;
    char buf1[256], buf2[256];
    int dbt, sbt;

    st = &vtop->type; 
    dbt = dt->t & VT_BTYPE;
    sbt = st->t & VT_BTYPE;
    if (dt->t & VT_CONSTANT)
        warning("assignment of read-only location");
    switch(dbt) {
    case VT_PTR:
        
        
        if (is_null_pointer(vtop))
            goto type_ok;
        
        if (is_integer_btype(sbt)) {
            warning("assignment makes pointer from integer without a cast");
            goto type_ok;
        }
        type1 = pointed_type(dt);
        
        if (sbt == VT_FUNC) {
            if ((type1->t & VT_BTYPE) != VT_VOID &&
                !is_compatible_types(pointed_type(dt), st))
                goto error;
            else
                goto type_ok;
        }
        if (sbt != VT_PTR)
            goto error;
        type2 = pointed_type(st);
        if ((type1->t & VT_BTYPE) == VT_VOID || 
            (type2->t & VT_BTYPE) == VT_VOID) {
            
        } else {
            
            tmp_type1 = *type1;
            tmp_type2 = *type2;
            tmp_type1.t &= ~(VT_UNSIGNED | VT_CONSTANT | VT_VOLATILE);
            tmp_type2.t &= ~(VT_UNSIGNED | VT_CONSTANT | VT_VOLATILE);
            if (!is_compatible_types(&tmp_type1, &tmp_type2))
                goto error;
        }
        
        if ((!(type1->t & VT_CONSTANT) && (type2->t & VT_CONSTANT)) ||
            (!(type1->t & VT_VOLATILE) && (type2->t & VT_VOLATILE)))
            warning("assignment discards qualifiers from pointer target type");
        break;
    case VT_BYTE:
    case VT_SHORT:
    case VT_INT:
    case VT_LLONG:
        if (sbt == VT_PTR || sbt == VT_FUNC) {
            warning("assignment makes integer from pointer without a cast");
        }
        
        break;
    case VT_STRUCT:
        tmp_type1 = *dt;
        tmp_type2 = *st;
        tmp_type1.t &= ~(VT_CONSTANT | VT_VOLATILE);
        tmp_type2.t &= ~(VT_CONSTANT | VT_VOLATILE);
        if (!is_compatible_types(&tmp_type1, &tmp_type2)) {
        error:
            type_to_str(buf1, sizeof(buf1), st, NULL);
            type_to_str(buf2, sizeof(buf2), dt, NULL);
            error("cannot cast '%s' to '%s'", buf1, buf2);
        }
        break;
    }
 type_ok:
    gen_cast(dt);
}

void vstore(void)
{
    int sbt, dbt, ft, r, t, size, align, bit_size, bit_pos, rc, delayed_cast;

    ft = vtop[-1].type.t;
    sbt = vtop->type.t & VT_BTYPE;
    dbt = ft & VT_BTYPE;
    if (((sbt == VT_INT || sbt == VT_SHORT) && dbt == VT_BYTE) ||
        (sbt == VT_INT && dbt == VT_SHORT)) {
        
        delayed_cast = VT_MUSTCAST;
        vtop->type.t = ft & VT_TYPE;
        
        if (ft & VT_CONSTANT)
            warning("assignment of read-only location");
    } else {
        delayed_cast = 0;
        if (!(ft & VT_BITFIELD))
            gen_assign_cast(&vtop[-1].type);
    }

    if (sbt == VT_STRUCT) {
        
        
        
        if (!nocode_wanted) {
            size = type_size(&vtop->type, &align);

            vpush_global_sym(&func_old_type, TOK_memcpy);

            
            vpushv(vtop - 2);
            vtop->type.t = VT_INT;
            gaddrof();
            
            vpushv(vtop - 2);
            vtop->type.t = VT_INT;
            gaddrof();
            
            vpushi(size);
            gfunc_call(3);
            
            vswap();
            vpop();
        } else {
            vswap();
            vpop();
        }
        
    } else if (ft & VT_BITFIELD) {
        
        bit_pos = (ft >> VT_STRUCT_SHIFT) & 0x3f;
        bit_size = (ft >> (VT_STRUCT_SHIFT + 6)) & 0x3f;
        
        vtop[-1].type.t = ft & ~(VT_BITFIELD | (-1 << VT_STRUCT_SHIFT));

        
        vdup();
        vtop[-1] = vtop[-2];

        
        vpushi((1 << bit_size) - 1);
        gen_op('&');
        vpushi(bit_pos);
        gen_op(TOK_SHL);
        
        vswap();
        vpushi(~(((1 << bit_size) - 1) << bit_pos));
        gen_op('&');
        gen_op('|');
        
        vstore();
    } else {
#ifdef CONFIG_TCC_BCHECK
        
        if (vtop[-1].r & VT_MUSTBOUND) {
            vswap();
            gbound();
            vswap();
        }
#endif
        if (!nocode_wanted) {
            rc = RC_INT;
            if (is_float(ft))
                rc = RC_FLOAT;
            r = gv(rc);  
            
            if ((vtop[-1].r & VT_VALMASK) == VT_LLOCAL) {
                SValue sv;
                t = get_reg(RC_INT);
                sv.type.t = VT_INT;
                sv.r = VT_LOCAL | VT_LVAL;
                sv.c.ul = vtop[-1].c.ul;
                load(t, &sv);
                vtop[-1].r = t | VT_LVAL;
            }
            store(r, vtop - 1);
            
            if ((ft & VT_BTYPE) == VT_LLONG) {
                vswap();
                
                vtop->type.t = VT_INT;
                gaddrof();
                vpushi(4);
                gen_op('+');
                vtop->r |= VT_LVAL;
                vswap();
                
                store(vtop->r2, vtop - 1);
            }
        }
        vswap();
        vtop--; 
        vtop->r |= delayed_cast;
    }
}

void inc(int post, int c)
{
    test_lvalue();
    vdup(); 
    if (post) {
        gv_dup(); 
        vrotb(3);
        vrotb(3);
    }
    
    vpushi(c - TOK_MID); 
    gen_op('+');
    vstore(); 
    if (post)
        vpop(); 
}

static void parse_attribute(AttributeDef *ad)
{
    int t, n;
    
    while (tok == TOK_ATTRIBUTE1 || tok == TOK_ATTRIBUTE2) {
    next();
    skip('(');
    skip('(');
    while (tok != ')') {
        if (tok < TOK_IDENT)
            expect("attribute name");
        t = tok;
        next();
        switch(t) {
        case TOK_SECTION1:
        case TOK_SECTION2:
            skip('(');
            if (tok != TOK_STR)
                expect("section name");
            ad->section = find_section(tcc_state, (char *)tokc.cstr->data);
            next();
            skip(')');
            break;
        case TOK_ALIGNED1:
        case TOK_ALIGNED2:
            if (tok == '(') {
                next();
                n = expr_const();
                if (n <= 0 || (n & (n - 1)) != 0) 
                    error("alignment must be a positive power of two");
                skip(')');
            } else {
                n = MAX_ALIGN;
            }
            ad->aligned = n;
            break;
        case TOK_PACKED1:
        case TOK_PACKED2:
            ad->packed = 1;
            break;
        case TOK_UNUSED1:
        case TOK_UNUSED2:
            break;
        case TOK_NORETURN1:
        case TOK_NORETURN2:
            break;
        case TOK_CDECL1:
        case TOK_CDECL2:
        case TOK_CDECL3:
            ad->func_call = FUNC_CDECL;
            break;
        case TOK_STDCALL1:
        case TOK_STDCALL2:
        case TOK_STDCALL3:
            ad->func_call = FUNC_STDCALL;
            break;
#ifdef TCC_TARGET_I386
        case TOK_REGPARM1:
        case TOK_REGPARM2:
            skip('(');
            n = expr_const();
            if (n > 3) 
                n = 3;
            else if (n < 0)
                n = 0;
            if (n > 0)
                ad->func_call = FUNC_FASTCALL1 + n - 1;
            skip(')');
            break;
#endif
        case TOK_DLLEXPORT:
            ad->dllexport = 1;
            break;
        default:
            if (tcc_state->warn_unsupported)
                warning("'%s' attribute ignored", get_tok_str(t, NULL));
            
            
            if (tok == '(') {
                next();
                while (tok != ')' && tok != -1)
                    next();
                next();
            }
            break;
        }
        if (tok != ',')
            break;
        next();
    }
    skip(')');
    skip(')');
    }
}

static void struct_decl(CType *type, int u)
{
    int a, v, size, align, maxalign, c, offset;
    int bit_size, bit_pos, bsize, bt, lbit_pos;
    Sym *s, *ss, **ps;
    AttributeDef ad;
    CType type1, btype;

    a = tok; 
    next();
    if (tok != '{') {
        v = tok;
        next();
        
        if (v < TOK_IDENT)
            expect("struct/union/enum name");
        s = struct_find(v);
        if (s) {
            if (s->type.t != a)
                error("invalid type");
            goto do_decl;
        }
    } else {
        v = anon_sym++;
    }
    type1.t = a;
    
    s = sym_push(v | SYM_STRUCT, &type1, 0, -1);
    s->r = 0; 
    
 do_decl:
    type->t = u;
    type->ref = s;
    
    if (tok == '{') {
        next();
        if (s->c != -1)
            error("struct/union/enum already defined");
        
        c = 0;
        
        if (a == TOK_ENUM) {
            for(;;) {
                v = tok;
                if (v < TOK_UIDENT)
                    expect("identifier");
                next();
                if (tok == '=') {
                    next();
                    c = expr_const();
                }
                
                ss = sym_push(v, &int_type, VT_CONST, c);
                ss->type.t |= VT_STATIC;
                if (tok != ',')
                    break;
                next();
                c++;
                
                if (tok == '}')
                    break;
            }
            skip('}');
        } else {
            maxalign = 1;
            ps = &s->next;
            bit_pos = 0;
            offset = 0;
            while (tok != '}') {
                parse_btype(&btype, &ad);
                while (1) {
                    bit_size = -1;
                    v = 0;
                    type1 = btype;
                    if (tok != ':') {
                        type_decl(&type1, &ad, &v, TYPE_DIRECT);
                        if ((type1.t & VT_BTYPE) == VT_FUNC ||
                            (type1.t & (VT_TYPEDEF | VT_STATIC | VT_EXTERN | VT_INLINE)))
                            error("invalid type for '%s'", 
                                  get_tok_str(v, NULL));
                    }
                    if (tok == ':') {
                        next();
                        bit_size = expr_const();
                        
                        if (bit_size < 0)
                            error("negative width in bit-field '%s'", 
                                  get_tok_str(v, NULL));
                        if (v && bit_size == 0)
                            error("zero width for bit-field '%s'", 
                                  get_tok_str(v, NULL));
                    }
                    size = type_size(&type1, &align);
                    if (ad.aligned) {
                        if (align < ad.aligned)
                            align = ad.aligned;
                    } else if (ad.packed) {
                        align = 1;
                    } else if (*tcc_state->pack_stack_ptr) {
                        if (align > *tcc_state->pack_stack_ptr)
                            align = *tcc_state->pack_stack_ptr;
                    }
                    lbit_pos = 0;
                    if (bit_size >= 0) {
                        bt = type1.t & VT_BTYPE;
                        if (bt != VT_INT && 
                            bt != VT_BYTE && 
                            bt != VT_SHORT &&
                            bt != VT_BOOL &&
                            bt != VT_ENUM)
                            error("bitfields must have scalar type");
                        bsize = size * 8;
                        if (bit_size > bsize) {
                            error("width of '%s' exceeds its type",
                                  get_tok_str(v, NULL));
                        } else if (bit_size == bsize) {
                            
                            bit_pos = 0;
                        } else if (bit_size == 0) {
                            
                            if (bit_pos > 0)
                                bit_pos = bsize;
                        } else {
                            
                            if ((bit_pos + bit_size) > bsize)
                                bit_pos = 0;
                            lbit_pos = bit_pos;
                            
                            type1.t |= VT_BITFIELD | 
                                (bit_pos << VT_STRUCT_SHIFT) |
                                (bit_size << (VT_STRUCT_SHIFT + 6));
                            bit_pos += bit_size;
                        }
                    } else {
                        bit_pos = 0;
                    }
                    if (v) {
                        if (lbit_pos == 0) {
                            if (a == TOK_STRUCT) {
                                c = (c + align - 1) & -align;
                                offset = c;
                                c += size;
                            } else {
                                offset = 0;
                                if (size > c)
                                    c = size;
                            }
                            if (align > maxalign)
                                maxalign = align;
                        }
#if 0
                        printf("add field %s offset=%d", 
                               get_tok_str(v, NULL), offset);
                        if (type1.t & VT_BITFIELD) {
                            printf(" pos=%d size=%d", 
                                   (type1.t >> VT_STRUCT_SHIFT) & 0x3f,
                                   (type1.t >> (VT_STRUCT_SHIFT + 6)) & 0x3f);
                        }
                        printf("\n");
#endif
                        ss = sym_push(v | SYM_FIELD, &type1, 0, offset);
                        *ps = ss;
                        ps = &ss->next;
                    }
                    if (tok == ';' || tok == TOK_EOF)
                        break;
                    skip(',');
                }
                skip(';');
            }
            skip('}');
            
            s->c = (c + maxalign - 1) & -maxalign; 
            s->r = maxalign;
        }
    }
}

static int parse_btype(CType *type, AttributeDef *ad)
{
    int t, u, type_found, typespec_found;
    Sym *s;
    CType type1;

    memset(ad, 0, sizeof(AttributeDef));
    type_found = 0;
    typespec_found = 0;
    t = 0;
    while(1) {
        switch(tok) {
        case TOK_EXTENSION:
            
            next();
            continue;

            
        case TOK_CHAR:
            u = VT_BYTE;
        basic_type:
            next();
        basic_type1:
            if ((t & VT_BTYPE) != 0)
                error("too many basic types");
            t |= u;
            typespec_found = 1;
            break;
        case TOK_VOID:
            u = VT_VOID;
            goto basic_type;
        case TOK_SHORT:
            u = VT_SHORT;
            goto basic_type;
        case TOK_INT:
            next();
            typespec_found = 1;
            break;
        case TOK_LONG:
            next();
            if ((t & VT_BTYPE) == VT_DOUBLE) {
                t = (t & ~VT_BTYPE) | VT_LDOUBLE;
            } else if ((t & VT_BTYPE) == VT_LONG) {
                t = (t & ~VT_BTYPE) | VT_LLONG;
            } else {
                u = VT_LONG;
                goto basic_type1;
            }
            break;
        case TOK_BOOL:
            u = VT_BOOL;
            goto basic_type;
        case TOK_FLOAT:
            u = VT_FLOAT;
            goto basic_type;
        case TOK_DOUBLE:
            next();
            if ((t & VT_BTYPE) == VT_LONG) {
                t = (t & ~VT_BTYPE) | VT_LDOUBLE;
            } else {
                u = VT_DOUBLE;
                goto basic_type1;
            }
            break;
        case TOK_ENUM:
            struct_decl(&type1, VT_ENUM);
        basic_type2:
            u = type1.t;
            type->ref = type1.ref;
            goto basic_type1;
        case TOK_STRUCT:
        case TOK_UNION:
            struct_decl(&type1, VT_STRUCT);
            goto basic_type2;

            
        case TOK_CONST1:
        case TOK_CONST2:
        case TOK_CONST3:
            t |= VT_CONSTANT;
            next();
            break;
        case TOK_VOLATILE1:
        case TOK_VOLATILE2:
        case TOK_VOLATILE3:
            t |= VT_VOLATILE;
            next();
            break;
        case TOK_SIGNED1:
        case TOK_SIGNED2:
        case TOK_SIGNED3:
            typespec_found = 1;
	    t |= VT_SIGNED;
	    next();
	    break;
        case TOK_REGISTER:
        case TOK_AUTO:
        case TOK_RESTRICT1:
        case TOK_RESTRICT2:
        case TOK_RESTRICT3:
            next();
            break;
        case TOK_UNSIGNED:
            t |= VT_UNSIGNED;
            next();
            typespec_found = 1;
            break;

            
        case TOK_EXTERN:
            t |= VT_EXTERN;
            next();
            break;
        case TOK_STATIC:
            t |= VT_STATIC;
            next();
            break;
        case TOK_TYPEDEF:
            t |= VT_TYPEDEF;
            next();
            break;
        case TOK_INLINE1:
        case TOK_INLINE2:
        case TOK_INLINE3:
            t |= VT_INLINE;
            next();
            break;

            
        case TOK_ATTRIBUTE1:
        case TOK_ATTRIBUTE2:
            parse_attribute(ad);
            break;
            
        case TOK_TYPEOF1:
        case TOK_TYPEOF2:
        case TOK_TYPEOF3:
            next();
            parse_expr_type(&type1);
            goto basic_type2;
        default:
            if (typespec_found)
                goto the_end;
            s = sym_find(tok);
            if (!s || !(s->type.t & VT_TYPEDEF))
                goto the_end;
            t |= (s->type.t & ~VT_TYPEDEF);
            type->ref = s->type.ref;
            next();
            break;
        }
        type_found = 1;
    }
the_end:
    if ((t & (VT_SIGNED|VT_UNSIGNED)) == (VT_SIGNED|VT_UNSIGNED))
      error("signed and unsigned modifier");
    if (tcc_state->char_is_unsigned) {
        if ((t & (VT_SIGNED|VT_UNSIGNED|VT_BTYPE)) == VT_BYTE)
            t |= VT_UNSIGNED;
    }
    t &= ~VT_SIGNED;

    
    if ((t & VT_BTYPE) == VT_LONG)
        t = (t & ~VT_BTYPE) | VT_INT;
    type->t = t;
    return type_found;
}

static inline void convert_parameter_type(CType *pt)
{
    pt->t &= ~(VT_CONSTANT | VT_VOLATILE);
    
    pt->t &= ~VT_ARRAY;
    if ((pt->t & VT_BTYPE) == VT_FUNC) {
        mk_pointer(pt);
    }
}

static void post_type(CType *type, AttributeDef *ad)
{
    int n, l, t1;
    Sym **plast, *s, *first;
    AttributeDef ad1;
    CType pt;

    if (tok == '(') {
        
        next();
        l = 0;
        first = NULL;
        plast = &first;
        while (tok != ')') {
            
            if (l != FUNC_OLD) {
                if (!parse_btype(&pt, &ad1)) {
                    if (l) {
                        error("invalid type");
                    } else {
                        l = FUNC_OLD;
                        goto old_proto;
                    }
                }
                l = FUNC_NEW;
                if ((pt.t & VT_BTYPE) == VT_VOID && tok == ')')
                    break;
                type_decl(&pt, &ad1, &n, TYPE_DIRECT | TYPE_ABSTRACT);
                if ((pt.t & VT_BTYPE) == VT_VOID)
                    error("parameter declared as void");
            } else {
            old_proto:
                n = tok;
                pt.t = VT_INT;
                next();
            }
            convert_parameter_type(&pt);
            s = sym_push(n | SYM_FIELD, &pt, 0, 0);
            *plast = s;
            plast = &s->next;
            if (tok == ',') {
                next();
                if (l == FUNC_NEW && tok == TOK_DOTS) {
                    l = FUNC_ELLIPSIS;
                    next();
                    break;
                }
            }
        }
        
        if (l == 0)
            l = FUNC_OLD;
        skip(')');
        t1 = type->t & VT_STORAGE;
        type->t &= ~(VT_STORAGE | VT_CONSTANT); 
        post_type(type, ad);
        
        s = sym_push(SYM_FIELD, type, ad->func_call, l);
        s->next = first;
        type->t = t1 | VT_FUNC;
        type->ref = s;
    } else if (tok == '[') {
        
        next();
        n = -1;
        if (tok != ']') {
            n = expr_const();
            if (n < 0)
                error("invalid array size");    
        }
        skip(']');
        
        t1 = type->t & VT_STORAGE;
        type->t &= ~VT_STORAGE;
        post_type(type, ad);
        
        s = sym_push(SYM_FIELD, type, 0, n);
        type->t = t1 | VT_ARRAY | VT_PTR;
        type->ref = s;
    }
}

static void type_decl(CType *type, AttributeDef *ad, int *v, int td)
{
    Sym *s;
    CType type1, *type2;
    int qualifiers;
    
    while (tok == '*') {
        qualifiers = 0;
    redo:
        next();
        switch(tok) {
        case TOK_CONST1:
        case TOK_CONST2:
        case TOK_CONST3:
            qualifiers |= VT_CONSTANT;
            goto redo;
        case TOK_VOLATILE1:
        case TOK_VOLATILE2:
        case TOK_VOLATILE3:
            qualifiers |= VT_VOLATILE;
            goto redo;
        case TOK_RESTRICT1:
        case TOK_RESTRICT2:
        case TOK_RESTRICT3:
            goto redo;
        }
        mk_pointer(type);
        type->t |= qualifiers;
    }
    
    
    if (tok == TOK_ATTRIBUTE1 || tok == TOK_ATTRIBUTE2)
        parse_attribute(ad);

    
    
    type1.t = 0; 
    if (tok == '(') {
        next();
        if (tok == TOK_ATTRIBUTE1 || tok == TOK_ATTRIBUTE2)
            parse_attribute(ad);
        type_decl(&type1, ad, v, td);
        skip(')');
    } else {
        
        if (tok >= TOK_IDENT && (td & TYPE_DIRECT)) {
            *v = tok;
            next();
        } else {
            if (!(td & TYPE_ABSTRACT))
                expect("identifier");
            *v = 0;
        }
    }
    post_type(type, ad);
    if (tok == TOK_ATTRIBUTE1 || tok == TOK_ATTRIBUTE2)
        parse_attribute(ad);
    if (!type1.t)
        return;
    
    type2 = &type1;
    for(;;) {
        s = type2->ref;
        type2 = &s->type;
        if (!type2->t) {
            *type2 = *type;
            break;
        }
    }
    *type = type1;
}

static int lvalue_type(int t)
{
    int bt, r;
    r = VT_LVAL;
    bt = t & VT_BTYPE;
    if (bt == VT_BYTE || bt == VT_BOOL)
        r |= VT_LVAL_BYTE;
    else if (bt == VT_SHORT)
        r |= VT_LVAL_SHORT;
    else
        return r;
    if (t & VT_UNSIGNED)
        r |= VT_LVAL_UNSIGNED;
    return r;
}

static void indir(void)
{
    if ((vtop->type.t & VT_BTYPE) != VT_PTR)
        expect("pointer");
    if ((vtop->r & VT_LVAL) && !nocode_wanted)
        gv(RC_INT);
    vtop->type = *pointed_type(&vtop->type);
    
    if (!(vtop->type.t & VT_ARRAY)) {
        vtop->r |= lvalue_type(vtop->type.t);
        
        if (do_bounds_check) 
            vtop->r |= VT_MUSTBOUND;
    }
}

static void gfunc_param_typed(Sym *func, Sym *arg)
{
    int func_type;
    CType type;

    func_type = func->c;
    if (func_type == FUNC_OLD ||
        (func_type == FUNC_ELLIPSIS && arg == NULL)) {
        
        if ((vtop->type.t & VT_BTYPE) == VT_FLOAT) {
            type.t = VT_DOUBLE;
            gen_cast(&type);
        }
    } else if (arg == NULL) {
        error("too many arguments to function");
    } else {
        type = arg->type;
        type.t &= ~VT_CONSTANT; 
        gen_assign_cast(&type);
    }
}

static void parse_expr_type(CType *type)
{
    int n;
    AttributeDef ad;

    skip('(');
    if (parse_btype(type, &ad)) {
        type_decl(type, &ad, &n, TYPE_ABSTRACT);
    } else {
        expr_type(type);
    }
    skip(')');
}

static void parse_type(CType *type)
{
    AttributeDef ad;
    int n;

    if (!parse_btype(type, &ad)) {
        expect("type");
    }
    type_decl(type, &ad, &n, TYPE_ABSTRACT);
}

static void vpush_tokc(int t)
{
    CType type;
    type.t = t;
    vsetc(&type, VT_CONST, &tokc);
}

static void unary(void)
{
    int n, t, align, size, r;
    CType type;
    Sym *s;
    AttributeDef ad;

 tok_next:
    switch(tok) {
    case TOK_EXTENSION:
        next();
        goto tok_next;
    case TOK_CINT:
    case TOK_CCHAR: 
    case TOK_LCHAR:
        vpushi(tokc.i);
        next();
        break;
    case TOK_CUINT:
        vpush_tokc(VT_INT | VT_UNSIGNED);
        next();
        break;
    case TOK_CLLONG:
        vpush_tokc(VT_LLONG);
        next();
        break;
    case TOK_CULLONG:
        vpush_tokc(VT_LLONG | VT_UNSIGNED);
        next();
        break;
    case TOK_CFLOAT:
        vpush_tokc(VT_FLOAT);
        next();
        break;
    case TOK_CDOUBLE:
        vpush_tokc(VT_DOUBLE);
        next();
        break;
    case TOK_CLDOUBLE:
        vpush_tokc(VT_LDOUBLE);
        next();
        break;
    case TOK___FUNCTION__:
        if (!gnu_ext)
            goto tok_identifier;
        
    case TOK___FUNC__:
        {
            void *ptr;
            int len;
            
            len = strlen(funcname) + 1;
            
            type.t = VT_BYTE;
            mk_pointer(&type);
            type.t |= VT_ARRAY;
            type.ref->c = len;
            vpush_ref(&type, data_section, data_section->data_offset, len);
            ptr = section_ptr_add(data_section, len);
            memcpy(ptr, funcname, len);
            next();
        }
        break;
    case TOK_LSTR:
        t = VT_INT;
        goto str_init;
    case TOK_STR:
        
        t = VT_BYTE;
    str_init:
        if (tcc_state->warn_write_strings)
            t |= VT_CONSTANT;
        type.t = t;
        mk_pointer(&type);
        type.t |= VT_ARRAY;
        memset(&ad, 0, sizeof(AttributeDef));
        decl_initializer_alloc(&type, &ad, VT_CONST, 2, 0, 0);
        break;
    case '(':
        next();
        
        if (parse_btype(&type, &ad)) {
            type_decl(&type, &ad, &n, TYPE_ABSTRACT);
            skip(')');
            
            if (tok == '{') {
                    
                if (global_expr)
                    r = VT_CONST;
                else
                    r = VT_LOCAL;
                
                if (!(type.t & VT_ARRAY))
                    r |= lvalue_type(type.t);
                memset(&ad, 0, sizeof(AttributeDef));
                decl_initializer_alloc(&type, &ad, r, 1, 0, 0);
            } else {
                unary();
                gen_cast(&type);
            }
        } else if (tok == '{') {
            
            save_regs(0); 
            block(NULL, NULL, NULL, NULL, 0, 1);
            skip(')');
        } else {
            gexpr();
            skip(')');
        }
        break;
    case '*':
        next();
        unary();
        indir();
        break;
    case '&':
        next();
        unary();
        
        if ((vtop->type.t & VT_BTYPE) != VT_FUNC &&
            !(vtop->type.t & VT_ARRAY))
            test_lvalue();
        mk_pointer(&vtop->type);
        gaddrof();
        break;
    case '!':
        next();
        unary();
        if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST)
            vtop->c.i = !vtop->c.i;
        else if ((vtop->r & VT_VALMASK) == VT_CMP)
            vtop->c.i = vtop->c.i ^ 1;
        else
            vseti(VT_JMP, gtst(1, 0));
        break;
    case '~':
        next();
        unary();
        vpushi(-1);
        gen_op('^');
        break;
    case '+':
        next();
        
        unary();
        if ((vtop->type.t & VT_BTYPE) == VT_PTR)
            error("pointer not accepted for unary plus");
        vpushi(0);
        gen_op('+');
        break;
    case TOK_SIZEOF:
    case TOK_ALIGNOF1:
    case TOK_ALIGNOF2:
        t = tok;
        next();
        if (tok == '(') {
            parse_expr_type(&type);
        } else {
            unary_type(&type);
        }
        size = type_size(&type, &align);
        if (t == TOK_SIZEOF) {
            if (size < 0)
                error("sizeof applied to an incomplete type");
            vpushi(size);
        } else {
            vpushi(align);
        }
        break;

    case TOK_builtin_types_compatible_p:
        {
            CType type1, type2;
            next();
            skip('(');
            parse_type(&type1);
            skip(',');
            parse_type(&type2);
            skip(')');
            type1.t &= ~(VT_CONSTANT | VT_VOLATILE);
            type2.t &= ~(VT_CONSTANT | VT_VOLATILE);
            vpushi(is_compatible_types(&type1, &type2));
        }
        break;
    case TOK_builtin_constant_p:
        {
            int saved_nocode_wanted, res;
            next();
            skip('(');
            saved_nocode_wanted = nocode_wanted;
            nocode_wanted = 1;
            gexpr();
            res = (vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
            vpop();
            nocode_wanted = saved_nocode_wanted;
            skip(')');
            vpushi(res);
        }
        break;
    case TOK_INC:
    case TOK_DEC:
        t = tok;
        next();
        unary();
        inc(0, t);
        break;
    case '-':
        next();
        vpushi(0);
        unary();
        gen_op('-');
        break;
    case TOK_LAND:
        if (!gnu_ext)
            goto tok_identifier;
        next();
        
        if (tok < TOK_UIDENT)
            expect("label identifier");
        s = label_find(tok);
        if (!s) {
            s = label_push(&global_label_stack, tok, LABEL_FORWARD);
        } else {
            if (s->r == LABEL_DECLARED)
                s->r = LABEL_FORWARD;
        }
        if (!s->type.t) {
            s->type.t = VT_VOID;
            mk_pointer(&s->type);
            s->type.t |= VT_STATIC;
        }
        vset(&s->type, VT_CONST | VT_SYM, 0);
        vtop->sym = s;
        next();
        break;
    default:
    tok_identifier:
        t = tok;
        next();
        if (t < TOK_UIDENT)
            expect("identifier");
        s = sym_find(t);
        if (!s) {
            if (tok != '(')
                error("'%s' undeclared", get_tok_str(t, NULL));
            if (tcc_state->warn_implicit_function_declaration)
                warning("implicit declaration of function '%s'",
                        get_tok_str(t, NULL));
            s = external_global_sym(t, &func_old_type, 0); 
        }
        if ((s->type.t & (VT_STATIC | VT_INLINE | VT_BTYPE)) ==
            (VT_STATIC | VT_INLINE | VT_FUNC)) {
            if (!s->c)
                put_extern_sym(s, text_section, 0, 0);
            r = VT_SYM | VT_CONST;
        } else {
            r = s->r;
        }
        vset(&s->type, r, s->c);
        
        if (vtop->r & VT_SYM) {
            vtop->sym = s;
            vtop->c.ul = 0;
        }
        break;
    }
    
    
    while (1) {
        if (tok == TOK_INC || tok == TOK_DEC) {
            inc(1, tok);
            next();
        } else if (tok == '.' || tok == TOK_ARROW) {
             
            if (tok == TOK_ARROW) 
                indir();
            test_lvalue();
            gaddrof();
            next();
            
            if ((vtop->type.t & VT_BTYPE) != VT_STRUCT)
                expect("struct or union");
            s = vtop->type.ref;
            
            tok |= SYM_FIELD;
            while ((s = s->next) != NULL) {
                if (s->v == tok)
                    break;
            }
            if (!s)
                error("field not found");
            
            vtop->type = char_pointer_type; 
            vpushi(s->c);
            gen_op('+');
            
            vtop->type = s->type;
            
            if (!(vtop->type.t & VT_ARRAY)) {
                vtop->r |= lvalue_type(vtop->type.t);
                
                if (do_bounds_check) 
                    vtop->r |= VT_MUSTBOUND;
            }
            next();
        } else if (tok == '[') {
            next();
            gexpr();
            gen_op('+');
            indir();
            skip(']');
        } else if (tok == '(') {
            SValue ret;
            Sym *sa;
            int nb_args;

            
            if ((vtop->type.t & VT_BTYPE) != VT_FUNC) {
                
                if ((vtop->type.t & (VT_BTYPE | VT_ARRAY)) == VT_PTR) {
                    vtop->type = *pointed_type(&vtop->type);
                    if ((vtop->type.t & VT_BTYPE) != VT_FUNC)
                        goto error_func;
                } else {
                error_func:
                    expect("function pointer");
                }
            } else {
                vtop->r &= ~VT_LVAL; 
            }
            
            s = vtop->type.ref;
            next();
            sa = s->next; 
            nb_args = 0;
            
            if ((s->type.t & VT_BTYPE) == VT_STRUCT) {
                
                size = type_size(&s->type, &align);
                loc = (loc - size) & -align;
                ret.type = s->type;
                ret.r = VT_LOCAL | VT_LVAL;
                vseti(VT_LOCAL, loc);
                ret.c = vtop->c;
                nb_args++;
            } else {
                ret.type = s->type; 
                ret.r2 = VT_CONST;
                
                if (is_float(ret.type.t)) {
                    ret.r = REG_FRET; 
                } else {
                    if ((ret.type.t & VT_BTYPE) == VT_LLONG)
                        ret.r2 = REG_LRET;
                    ret.r = REG_IRET;
                }
                ret.c.i = 0;
            }
            if (tok != ')') {
                for(;;) {
                    expr_eq();
                    gfunc_param_typed(s, sa);
                    nb_args++;
                    if (sa)
                        sa = sa->next;
                    if (tok == ')')
                        break;
                    skip(',');
                }
            }
            if (sa)
                error("too few arguments to function");
            skip(')');
            if (!nocode_wanted) {
                gfunc_call(nb_args);
            } else {
                vtop -= (nb_args + 1);
            }
            
            vsetc(&ret.type, ret.r, &ret.c);
            vtop->r2 = ret.r2;
        } else {
            break;
        }
    }
}

static void uneq(void)
{
    int t;
    
    unary();
    if (tok == '=' ||
        (tok >= TOK_A_MOD && tok <= TOK_A_DIV) ||
        tok == TOK_A_XOR || tok == TOK_A_OR ||
        tok == TOK_A_SHL || tok == TOK_A_SAR) {
        test_lvalue();
        t = tok;
        next();
        if (t == '=') {
            expr_eq();
        } else {
            vdup();
            expr_eq();
            gen_op(t & 0x7f);
        }
        vstore();
    }
}

static void expr_prod(void)
{
    int t;

    uneq();
    while (tok == '*' || tok == '/' || tok == '%') {
        t = tok;
        next();
        uneq();
        gen_op(t);
    }
}

static void expr_sum(void)
{
    int t;

    expr_prod();
    while (tok == '+' || tok == '-') {
        t = tok;
        next();
        expr_prod();
        gen_op(t);
    }
}

static void expr_shift(void)
{
    int t;

    expr_sum();
    while (tok == TOK_SHL || tok == TOK_SAR) {
        t = tok;
        next();
        expr_sum();
        gen_op(t);
    }
}

static void expr_cmp(void)
{
    int t;

    expr_shift();
    while ((tok >= TOK_ULE && tok <= TOK_GT) ||
           tok == TOK_ULT || tok == TOK_UGE) {
        t = tok;
        next();
        expr_shift();
        gen_op(t);
    }
}

static void expr_cmpeq(void)
{
    int t;

    expr_cmp();
    while (tok == TOK_EQ || tok == TOK_NE) {
        t = tok;
        next();
        expr_cmp();
        gen_op(t);
    }
}

static void expr_and(void)
{
    expr_cmpeq();
    while (tok == '&') {
        next();
        expr_cmpeq();
        gen_op('&');
    }
}

static void expr_xor(void)
{
    expr_and();
    while (tok == '^') {
        next();
        expr_and();
        gen_op('^');
    }
}

static void expr_or(void)
{
    expr_xor();
    while (tok == '|') {
        next();
        expr_xor();
        gen_op('|');
    }
}

static void expr_land_const(void)
{
    expr_or();
    while (tok == TOK_LAND) {
        next();
        expr_or();
        gen_op(TOK_LAND);
    }
}

static void expr_lor_const(void)
{
    expr_land_const();
    while (tok == TOK_LOR) {
        next();
        expr_land_const();
        gen_op(TOK_LOR);
    }
}

static void expr_land(void)
{
    int t;

    expr_or();
    if (tok == TOK_LAND) {
        t = 0;
        for(;;) {
            t = gtst(1, t);
            if (tok != TOK_LAND) {
                vseti(VT_JMPI, t);
                break;
            }
            next();
            expr_or();
        }
    }
}

static void expr_lor(void)
{
    int t;

    expr_land();
    if (tok == TOK_LOR) {
        t = 0;
        for(;;) {
            t = gtst(0, t);
            if (tok != TOK_LOR) {
                vseti(VT_JMP, t);
                break;
            }
            next();
            expr_land();
        }
    }
}

static void expr_eq(void)
{
    int tt, u, r1, r2, rc, t1, t2, bt1, bt2;
    SValue sv;
    CType type, type1, type2;

    if (const_wanted) {
        int c1, c;
        expr_lor_const();
        if (tok == '?') {
            c = vtop->c.i;
            vpop();
            next();
            if (tok == ':' && gnu_ext) {
                c1 = c;
            } else {
                gexpr();
                c1 = vtop->c.i;
                vpop();
            }
            skip(':');
            expr_eq();
            if (c)
                vtop->c.i = c1;
        }
    } else {
        expr_lor();
        if (tok == '?') {
            next();
            if (vtop != vstack) {
                if (is_float(vtop->type.t))
                    rc = RC_FLOAT;
                else
                    rc = RC_INT;
                    gv(rc);
                    save_regs(1);
            }
            if (tok == ':' && gnu_ext) {
                gv_dup();
                tt = gtst(1, 0);
            } else {
                tt = gtst(1, 0);
                gexpr();
            }
            type1 = vtop->type;
            sv = *vtop; 
            vtop--; 
            skip(':');
            u = gjmp(0);
            gsym(tt);
            expr_eq();
            type2 = vtop->type;

            t1 = type1.t;
            bt1 = t1 & VT_BTYPE;
            t2 = type2.t;
            bt2 = t2 & VT_BTYPE;
            
            if (is_float(bt1) || is_float(bt2)) {
                if (bt1 == VT_LDOUBLE || bt2 == VT_LDOUBLE) {
                    type.t = VT_LDOUBLE;
                } else if (bt1 == VT_DOUBLE || bt2 == VT_DOUBLE) {
                    type.t = VT_DOUBLE;
                } else {
                    type.t = VT_FLOAT;
                }
            } else if (bt1 == VT_LLONG || bt2 == VT_LLONG) {
                
                type.t = VT_LLONG;
                
                if ((t1 & (VT_BTYPE | VT_UNSIGNED)) == (VT_LLONG | VT_UNSIGNED) ||
                    (t2 & (VT_BTYPE | VT_UNSIGNED)) == (VT_LLONG | VT_UNSIGNED))
                    type.t |= VT_UNSIGNED;
            } else if (bt1 == VT_PTR || bt2 == VT_PTR) {
                
                type = type1;
            } else if (bt1 == VT_STRUCT || bt2 == VT_STRUCT) {
                
                type = type1;
            } else if (bt1 == VT_VOID || bt2 == VT_VOID) {
                
                type.t = VT_VOID;
            } else {
                
                type.t = VT_INT;
                
                if ((t1 & (VT_BTYPE | VT_UNSIGNED)) == (VT_INT | VT_UNSIGNED) ||
                    (t2 & (VT_BTYPE | VT_UNSIGNED)) == (VT_INT | VT_UNSIGNED))
                    type.t |= VT_UNSIGNED;
            }
                
            
            gen_cast(&type);
            rc = RC_INT;
            if (is_float(type.t)) {
                rc = RC_FLOAT;
            } else if ((type.t & VT_BTYPE) == VT_LLONG) {
                rc = RC_IRET; 
            }
            
            r2 = gv(rc);
            tt = gjmp(0);
            gsym(u);
            
            *vtop = sv;
            gen_cast(&type);
            r1 = gv(rc);
            move_reg(r2, r1);
            vtop->r = r2;
            gsym(tt);
        }
    }
}

static void gexpr(void)
{
    while (1) {
        expr_eq();
        if (tok != ',')
            break;
        vpop();
        next();
    }
}

static void expr_type(CType *type)
{
    int saved_nocode_wanted;

    saved_nocode_wanted = nocode_wanted;
    nocode_wanted = 1;
    gexpr();
    *type = vtop->type;
    vpop();
    nocode_wanted = saved_nocode_wanted;
}

static void unary_type(CType *type)
{
    int a;

    a = nocode_wanted;
    nocode_wanted = 1;
    unary();
    *type = vtop->type;
    vpop();
    nocode_wanted = a;
}

static void expr_const1(void)
{
    int a;
    a = const_wanted;
    const_wanted = 1;
    expr_eq();
    const_wanted = a;
}

static int expr_const(void)
{
    int c;
    expr_const1();
    if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) != VT_CONST)
        expect("constant expression");
    c = vtop->c.i;
    vpop();
    return c;
}

static int is_label(void)
{
    int last_tok;

    
    if (tok < TOK_UIDENT)
        return 0;
    
    last_tok = tok;
    next();
    if (tok == ':') {
        next();
        return last_tok;
    } else {
        unget_tok(last_tok);
        return 0;
    }
}

static void block(int *bsym, int *csym, int *case_sym, int *def_sym, 
                  int case_reg, int is_expr)
{
    int a, b, c, d;
    Sym *s;

    
    if (do_debug && 
        (last_line_num != file->line_num || last_ind != ind)) {
        put_stabn(N_SLINE, 0, file->line_num, ind - func_ind);
        last_ind = ind;
        last_line_num = file->line_num;
    }

    if (is_expr) {
        
        vpushi(0);
        vtop->type.t = VT_VOID;
    }

    if (tok == TOK_IF) {
        
        next();
        skip('(');
        gexpr();
        skip(')');
        a = gtst(1, 0);
        block(bsym, csym, case_sym, def_sym, case_reg, 0);
        c = tok;
        if (c == TOK_ELSE) {
            next();
            d = gjmp(0);
            gsym(a);
            block(bsym, csym, case_sym, def_sym, case_reg, 0);
            gsym(d); 
        } else
            gsym(a);
    } else if (tok == TOK_WHILE) {
        next();
        d = ind;
        skip('(');
        gexpr();
        skip(')');
        a = gtst(1, 0);
        b = 0;
        block(&a, &b, case_sym, def_sym, case_reg, 0);
        gjmp_addr(d);
        gsym(a);
        gsym_addr(b, d);
    } else if (tok == '{') {
        Sym *llabel;
        
        next();
        
        s = local_stack;
        llabel = local_label_stack;
        
        if (tok == TOK_LABEL) {
            next();
            for(;;) {
                if (tok < TOK_UIDENT)
                    expect("label identifier");
                label_push(&local_label_stack, tok, LABEL_DECLARED);
                next();
                if (tok == ',') {
                    next();
                } else {
                    skip(';');
                    break;
                }
            }
        }
        while (tok != '}') {
            decl(VT_LOCAL);
            if (tok != '}') {
                if (is_expr)
                    vpop();
                block(bsym, csym, case_sym, def_sym, case_reg, is_expr);
            }
        }
        
        label_pop(&local_label_stack, llabel);
        
        sym_pop(&local_stack, s);
        next();
    } else if (tok == TOK_RETURN) {
        next();
        if (tok != ';') {
            gexpr();
            gen_assign_cast(&func_vt);
            if ((func_vt.t & VT_BTYPE) == VT_STRUCT) {
                CType type;
                type = func_vt;
                mk_pointer(&type);
                vset(&type, VT_LOCAL | VT_LVAL, func_vc);
                indir();
                vswap();
                
                vstore();
            } else if (is_float(func_vt.t)) {
                gv(RC_FRET);
            } else {
                gv(RC_IRET);
            }
            vtop--; 
        }
        skip(';');
        rsym = gjmp(rsym); 
    } else if (tok == TOK_BREAK) {
        
        if (!bsym)
            error("cannot break");
        *bsym = gjmp(*bsym);
        next();
        skip(';');
    } else if (tok == TOK_CONTINUE) {
        
        if (!csym)
            error("cannot continue");
        *csym = gjmp(*csym);
        next();
        skip(';');
    } else if (tok == TOK_FOR) {
        int e;
        next();
        skip('(');
        if (tok != ';') {
            gexpr();
            vpop();
        }
        skip(';');
        d = ind;
        c = ind;
        a = 0;
        b = 0;
        if (tok != ';') {
            gexpr();
            a = gtst(1, 0);
        }
        skip(';');
        if (tok != ')') {
            e = gjmp(0);
            c = ind;
            gexpr();
            vpop();
            gjmp_addr(d);
            gsym(e);
        }
        skip(')');
        block(&a, &b, case_sym, def_sym, case_reg, 0);
        gjmp_addr(c);
        gsym(a);
        gsym_addr(b, c);
    } else 
    if (tok == TOK_DO) {
        next();
        a = 0;
        b = 0;
        d = ind;
        block(&a, &b, case_sym, def_sym, case_reg, 0);
        skip(TOK_WHILE);
        skip('(');
        gsym(b);
        gexpr();
        c = gtst(0, 0);
        gsym_addr(c, d);
        skip(')');
        gsym(a);
        skip(';');
    } else
    if (tok == TOK_SWITCH) {
        next();
        skip('(');
        gexpr();
        
        case_reg = gv(RC_INT);
        vpop();
        skip(')');
        a = 0;
        b = gjmp(0); 
        c = 0;
        block(&a, csym, &b, &c, case_reg, 0);
        
        if (c == 0)
            c = ind;
        
        gsym_addr(b, c);
        
        gsym(a);
    } else
    if (tok == TOK_CASE) {
        int v1, v2;
        if (!case_sym)
            expect("switch");
        next();
        v1 = expr_const();
        v2 = v1;
        if (gnu_ext && tok == TOK_DOTS) {
            next();
            v2 = expr_const();
            if (v2 < v1)
                warning("empty case range");
        }
        
        b = gjmp(0);
        gsym(*case_sym);
        vseti(case_reg, 0);
        vpushi(v1);
        if (v1 == v2) {
            gen_op(TOK_EQ);
            *case_sym = gtst(1, 0);
        } else {
            gen_op(TOK_GE);
            *case_sym = gtst(1, 0);
            vseti(case_reg, 0);
            vpushi(v2);
            gen_op(TOK_LE);
            *case_sym = gtst(1, *case_sym);
        }
        gsym(b);
        skip(':');
        is_expr = 0;
        goto block_after_label;
    } else 
    if (tok == TOK_DEFAULT) {
        next();
        skip(':');
        if (!def_sym)
            expect("switch");
        if (*def_sym)
            error("too many 'default'");
        *def_sym = ind;
        is_expr = 0;
        goto block_after_label;
    } else
    if (tok == TOK_GOTO) {
        next();
        if (tok == '*' && gnu_ext) {
            
            next();
            gexpr();
            if ((vtop->type.t & VT_BTYPE) != VT_PTR)
                expect("pointer");
            ggoto();
        } else if (tok >= TOK_UIDENT) {
            s = label_find(tok);
            
            if (!s) {
                s = label_push(&global_label_stack, tok, LABEL_FORWARD);
            } else {
                if (s->r == LABEL_DECLARED)
                    s->r = LABEL_FORWARD;
            }
            
            if (s->r & LABEL_FORWARD) 
                s->next = (void *)gjmp((long)s->next);
            else
                gjmp_addr((long)s->next);
            next();
        } else {
            expect("label identifier");
        }
        skip(';');
    } else if (tok == TOK_ASM1 || tok == TOK_ASM2 || tok == TOK_ASM3) {
        asm_instr();
    } else {
        b = is_label();
        if (b) {
            
            s = label_find(b);
            if (s) {
                if (s->r == LABEL_DEFINED)
                    error("duplicate label '%s'", get_tok_str(s->v, NULL));
                gsym((long)s->next);
                s->r = LABEL_DEFINED;
            } else {
                s = label_push(&global_label_stack, b, LABEL_DEFINED);
            }
            s->next = (void *)ind;
            
        block_after_label:
            if (tok == '}') {
                warning("deprecated use of label at end of compound statement");
            } else {
                if (is_expr)
                    vpop();
                block(bsym, csym, case_sym, def_sym, case_reg, is_expr);
            }
        } else {
            
            if (tok != ';') {
                if (is_expr) {
                    vpop();
                    gexpr();
                } else {
                    gexpr();
                    vpop();
                }
            }
            skip(';');
        }
    }
}

static void decl_designator(CType *type, Section *sec, unsigned long c, 
                            int *cur_index, Sym **cur_field, 
                            int size_only)
{
    Sym *s, *f;
    int notfirst, index, index_last, align, l, nb_elems, elem_size;
    CType type1;

    notfirst = 0;
    elem_size = 0;
    nb_elems = 1;
    if (gnu_ext && (l = is_label()) != 0)
        goto struct_field;
    while (tok == '[' || tok == '.') {
        if (tok == '[') {
            if (!(type->t & VT_ARRAY))
                expect("array type");
            s = type->ref;
            next();
            index = expr_const();
            if (index < 0 || (s->c >= 0 && index >= s->c))
                expect("invalid index");
            if (tok == TOK_DOTS && gnu_ext) {
                next();
                index_last = expr_const();
                if (index_last < 0 || 
                    (s->c >= 0 && index_last >= s->c) ||
                    index_last < index)
                    expect("invalid index");
            } else {
                index_last = index;
            }
            skip(']');
            if (!notfirst)
                *cur_index = index_last;
            type = pointed_type(type);
            elem_size = type_size(type, &align);
            c += index * elem_size;
            
            nb_elems = index_last - index + 1;
            if (nb_elems != 1) {
                notfirst = 1;
                break;
            }
        } else {
            next();
            l = tok;
            next();
        struct_field:
            if ((type->t & VT_BTYPE) != VT_STRUCT)
                expect("struct/union type");
            s = type->ref;
            l |= SYM_FIELD;
            f = s->next;
            while (f) {
                if (f->v == l)
                    break;
                f = f->next;
            }
            if (!f)
                expect("field");
            if (!notfirst)
                *cur_field = f;
            
            type1 = f->type;
            type1.t |= (type->t & ~VT_TYPE);
            type = &type1;
            c += f->c;
        }
        notfirst = 1;
    }
    if (notfirst) {
        if (tok == '=') {
            next();
        } else {
            if (!gnu_ext)
                expect("=");
        }
    } else {
        if (type->t & VT_ARRAY) {
            index = *cur_index;
            type = pointed_type(type);
            c += index * type_size(type, &align);
        } else {
            f = *cur_field;
            if (!f)
                error("too many field init");
            
            type1 = f->type;
            type1.t |= (type->t & ~VT_TYPE);
            type = &type1;
            c += f->c;
        }
    }
    decl_initializer(type, sec, c, 0, size_only);

    
    if (!size_only && nb_elems > 1) {
        unsigned long c_end;
        uint8_t *src, *dst;
        int i;

        if (!sec)
            error("range init not supported yet for dynamic storage");
        c_end = c + nb_elems * elem_size;
        if (c_end > sec->data_allocated)
            section_realloc(sec, c_end);
        src = sec->data + c;
        dst = src;
        for(i = 1; i < nb_elems; i++) {
            dst += elem_size;
            memcpy(dst, src, elem_size);
        }
    }
}

#define EXPR_VAL   0
#define EXPR_CONST 1
#define EXPR_ANY   2

static void init_putv(CType *type, Section *sec, unsigned long c, 
                      int v, int expr_type)
{
    int saved_global_expr, bt, bit_pos, bit_size;
    void *ptr;
    unsigned long long bit_mask;
    CType dtype;

    switch(expr_type) {
    case EXPR_VAL:
        vpushi(v);
        break;
    case EXPR_CONST:
        
        saved_global_expr = global_expr;
        global_expr = 1;
        expr_const1();
        global_expr = saved_global_expr;
        
        if ((vtop->r & (VT_VALMASK | VT_LVAL)) != VT_CONST)
            error("initializer element is not constant");
        break;
    case EXPR_ANY:
        expr_eq();
        break;
    }
    
    dtype = *type;
    dtype.t &= ~VT_CONSTANT; 

    if (sec) {
        
        
        gen_assign_cast(&dtype);
        bt = type->t & VT_BTYPE;
        ptr = sec->data + c;
        
        if (!(type->t & VT_BITFIELD)) {
            bit_pos = 0;
            bit_size = 32;
            bit_mask = -1LL;
        } else {
            bit_pos = (vtop->type.t >> VT_STRUCT_SHIFT) & 0x3f;
            bit_size = (vtop->type.t >> (VT_STRUCT_SHIFT + 6)) & 0x3f;
            bit_mask = (1LL << bit_size) - 1;
        }
        if ((vtop->r & VT_SYM) &&
            (bt == VT_BYTE ||
             bt == VT_SHORT ||
             bt == VT_DOUBLE ||
             bt == VT_LDOUBLE ||
             bt == VT_LLONG ||
             (bt == VT_INT && bit_size != 32)))
            error("initializer element is not computable at load time");
        switch(bt) {
        case VT_BYTE:
            *(char *)ptr |= (vtop->c.i & bit_mask) << bit_pos;
            break;
        case VT_SHORT:
            *(short *)ptr |= (vtop->c.i & bit_mask) << bit_pos;
            break;
        case VT_DOUBLE:
            *(double *)ptr = vtop->c.d;
            break;
        case VT_LDOUBLE:
            *(long double *)ptr = vtop->c.ld;
            break;
        case VT_LLONG:
            *(long long *)ptr |= (vtop->c.ll & bit_mask) << bit_pos;
            break;
        default:
            if (vtop->r & VT_SYM) {
                greloc(sec, vtop->sym, c, R_DATA_32);
            }
            *(int *)ptr |= (vtop->c.i & bit_mask) << bit_pos;
            break;
        }
        vtop--;
    } else {
        vset(&dtype, VT_LOCAL, c);
        vswap();
        vstore();
        vpop();
    }
}

static void init_putz(CType *t, Section *sec, unsigned long c, int size)
{
    if (sec) {
        
    } else {
        vpush_global_sym(&func_old_type, TOK_memset);
        vseti(VT_LOCAL, c);
        vpushi(0);
        vpushi(size);
        gfunc_call(3);
    }
}

static void decl_initializer(CType *type, Section *sec, unsigned long c, 
                             int first, int size_only)
{
    int index, array_length, n, no_oblock, nb, parlevel, i;
    int size1, align1, expr_type;
    Sym *s, *f;
    CType *t1;

    if (type->t & VT_ARRAY) {
        s = type->ref;
        n = s->c;
        array_length = 0;
        t1 = pointed_type(type);
        size1 = type_size(t1, &align1);

        no_oblock = 1;
        if ((first && tok != TOK_LSTR && tok != TOK_STR) || 
            tok == '{') {
            skip('{');
            no_oblock = 0;
        }

        if ((tok == TOK_LSTR && 
             (t1->t & VT_BTYPE) == VT_INT) ||
            (tok == TOK_STR &&
             (t1->t & VT_BTYPE) == VT_BYTE)) {
            while (tok == TOK_STR || tok == TOK_LSTR) {
                int cstr_len, ch;
                CString *cstr;

                cstr = tokc.cstr;
                
                if (tok == TOK_STR)
                    cstr_len = cstr->size;
                else
                    cstr_len = cstr->size / sizeof(int);
                cstr_len--;
                nb = cstr_len;
                if (n >= 0 && nb > (n - array_length))
                    nb = n - array_length;
                if (!size_only) {
                    if (cstr_len > nb)
                        warning("initializer-string for array is too long");
                    if (sec && tok == TOK_STR && size1 == 1) {
                        memcpy(sec->data + c + array_length, cstr->data, nb);
                    } else {
                        for(i=0;i<nb;i++) {
                            if (tok == TOK_STR)
                                ch = ((unsigned char *)cstr->data)[i];
                            else
                                ch = ((int *)cstr->data)[i];
                            init_putv(t1, sec, c + (array_length + i) * size1,
                                      ch, EXPR_VAL);
                        }
                    }
                }
                array_length += nb;
                next();
            }
            if (n < 0 || array_length < n) {
                if (!size_only) {
                    init_putv(t1, sec, c + (array_length * size1), 0, EXPR_VAL);
                }
                array_length++;
            }
        } else {
            index = 0;
            while (tok != '}') {
                decl_designator(type, sec, c, &index, NULL, size_only);
                if (n >= 0 && index >= n)
                    error("index too large");
                if (!size_only && array_length < index) {
                    init_putz(t1, sec, c + array_length * size1, 
                              (index - array_length) * size1);
                }
                index++;
                if (index > array_length)
                    array_length = index;
                if (index >= n && no_oblock)
                    break;
                if (tok == '}')
                    break;
                skip(',');
            }
        }
        if (!no_oblock)
            skip('}');
        
        if (!size_only && n >= 0 && array_length < n) {
            init_putz(t1, sec, c + array_length * size1, 
                      (n - array_length) * size1);
        }
        
        if (n < 0)
            s->c = array_length;
    } else if ((type->t & VT_BTYPE) == VT_STRUCT &&
               (sec || !first || tok == '{')) {
        int par_count;

        

        par_count = 0;
        if (tok == '(') {
            AttributeDef ad1;
            CType type1;
            next();
            while (tok == '(') {
                par_count++;
                next();
            }
            if (!parse_btype(&type1, &ad1))
                expect("cast");
            type_decl(&type1, &ad1, &n, TYPE_ABSTRACT);
#if 0
            if (!is_assignable_types(type, &type1))
                error("invalid type for cast");
#endif
            skip(')');
        }
        no_oblock = 1;
        if (first || tok == '{') {
            skip('{');
            no_oblock = 0;
        }
        s = type->ref;
        f = s->next;
        array_length = 0;
        index = 0;
        n = s->c;
        while (tok != '}') {
            decl_designator(type, sec, c, NULL, &f, size_only);
            index = f->c;
            if (!size_only && array_length < index) {
                init_putz(type, sec, c + array_length, 
                          index - array_length);
            }
            index = index + type_size(&f->type, &align1);
            if (index > array_length)
                array_length = index;
            f = f->next;
            if (no_oblock && f == NULL)
                break;
            if (tok == '}')
                break;
            skip(',');
        }
        
        if (!size_only && array_length < n) {
            init_putz(type, sec, c + array_length, 
                      n - array_length);
        }
        if (!no_oblock)
            skip('}');
        while (par_count) {
            skip(')');
            par_count--;
        }
    } else if (tok == '{') {
        next();
        decl_initializer(type, sec, c, first, size_only);
        skip('}');
    } else if (size_only) {
        
        parlevel = 0;
        while ((parlevel > 0 || (tok != '}' && tok != ',')) && 
               tok != -1) {
            if (tok == '(')
                parlevel++;
            else if (tok == ')')
                parlevel--;
            next();
        }
    } else {
        expr_type = EXPR_CONST;
        if (!sec)
            expr_type = EXPR_ANY;
        init_putv(type, sec, c, 0, expr_type);
    }
}

static void decl_initializer_alloc(CType *type, AttributeDef *ad, int r, 
                                   int has_init, int v, int scope)
{
    int size, align, addr, data_offset;
    int level;
    ParseState saved_parse_state;
    TokenString init_str;
    Section *sec;

    size = type_size(type, &align);
    tok_str_new(&init_str);
    if (size < 0) {
        if (!has_init) 
            error("unknown type size");
        
        if (has_init == 2) {
            
            while (tok == TOK_STR || tok == TOK_LSTR) {
                tok_str_add_tok(&init_str);
                next();
            }
        } else {
            level = 0;
            while (level > 0 || (tok != ',' && tok != ';')) {
                if (tok < 0)
                    error("unexpected end of file in initializer");
                tok_str_add_tok(&init_str);
                if (tok == '{')
                    level++;
                else if (tok == '}') {
                    if (level == 0)
                        break;
                    level--;
                }
                next();
            }
        }
        tok_str_add(&init_str, -1);
        tok_str_add(&init_str, 0);
        
        
        save_parse_state(&saved_parse_state);

        macro_ptr = init_str.str;
        next();
        decl_initializer(type, NULL, 0, 1, 1);
        
        macro_ptr = init_str.str;
        next();
        
        
        size = type_size(type, &align);
        if (size < 0) 
            error("unknown type size");
    }
    
    if (ad->aligned) {
        if (ad->aligned > align)
            align = ad->aligned;
    } else if (ad->packed) {
        align = 1;
    }
    if ((r & VT_VALMASK) == VT_LOCAL) {
        sec = NULL;
        if (do_bounds_check && (type->t & VT_ARRAY)) 
            loc--;
        loc = (loc - size) & -align;
        addr = loc;
        
        if (do_bounds_check && (type->t & VT_ARRAY)) {
            unsigned long *bounds_ptr;
            
            loc--;
            
            bounds_ptr = section_ptr_add(lbounds_section, 2 * sizeof(unsigned long));
            bounds_ptr[0] = addr;
            bounds_ptr[1] = size;
        }
        if (v) {
            
            sym_push(v, type, r, addr);
        } else {
            
            vset(type, r, addr);
        }
    } else {
        Sym *sym;

        sym = NULL;
        if (v && scope == VT_CONST) {
            
            sym = sym_find(v);
            if (sym) {
                if (!is_compatible_types(&sym->type, type))
                    error("incompatible types for redefinition of '%s'", 
                          get_tok_str(v, NULL));
                if (sym->type.t & VT_EXTERN) {
                    
                    sym->type.t &= ~VT_EXTERN;
                    if ((sym->type.t & VT_ARRAY) && 
                        sym->type.ref->c < 0 &&
                        type->ref->c >= 0)
                        sym->type.ref->c = type->ref->c;
                } else {
                    
                    if (!has_init)
                        goto no_alloc;
                }
            }
        }

        
        sec = ad->section;
        if (!sec) {
            if (has_init)
                sec = data_section;
            else if (tcc_state->nocommon)
                sec = bss_section;
        }
        if (sec) {
            data_offset = sec->data_offset;
            data_offset = (data_offset + align - 1) & -align;
            addr = data_offset;
            data_offset += size;
            
            if (do_bounds_check)
                data_offset++;
            sec->data_offset = data_offset;
            
            if (sec->sh_type != SHT_NOBITS && 
                data_offset > sec->data_allocated)
                section_realloc(sec, data_offset);
            
            if (align > sec->sh_addralign)
                sec->sh_addralign = align;
        } else {
            addr = 0; 
        }

        if (v) {
            if (scope == VT_CONST) {
                if (!sym)
                    goto do_def;
            } else {
            do_def:
                sym = sym_push(v, type, r | VT_SYM, 0);
            }
            
            if (sec) {
                put_extern_sym(sym, sec, addr, size);
            } else {
                Elf32_Sym *esym;
                
                put_extern_sym(sym, NULL, align, size);
                
                esym = &((Elf32_Sym *)symtab_section->data)[sym->c];
                esym->st_shndx = SHN_COMMON;
            }
        } else {
            CValue cval;

            
            sym = get_sym_ref(type, sec, addr, size);
            cval.ul = 0;
            vsetc(type, VT_CONST | VT_SYM, &cval);
            vtop->sym = sym;
        }

        if (do_bounds_check) {
            unsigned long *bounds_ptr;

            greloc(bounds_section, sym, bounds_section->data_offset, R_DATA_32);
            
            bounds_ptr = section_ptr_add(bounds_section, 2 * sizeof(long));
            bounds_ptr[0] = 0; 
            bounds_ptr[1] = size;
        }
    }
    if (has_init) {
        decl_initializer(type, sec, addr, 1, 0);
        
        if (init_str.str) {
            tok_str_free(init_str.str);
            restore_parse_state(&saved_parse_state);
        }
    }
 no_alloc: ;
}

void put_func_debug(Sym *sym)
{
    char buf[512];

    
    
    snprintf(buf, sizeof(buf), "%s:%c1", 
             funcname, sym->type.t & VT_STATIC ? 'f' : 'F');
    put_stabs_r(buf, N_FUN, 0, file->line_num, 0,
                cur_text_section, sym->c);
    last_ind = 0;
    last_line_num = 0;
}

static void func_decl_list(Sym *func_sym)
{
    AttributeDef ad;
    int v;
    Sym *s;
    CType btype, type;

    
    while (tok != '{' && tok != ';' && tok != ',' && tok != TOK_EOF) {
        if (!parse_btype(&btype, &ad)) 
            expect("declaration list");
        if (((btype.t & VT_BTYPE) == VT_ENUM ||
             (btype.t & VT_BTYPE) == VT_STRUCT) && 
            tok == ';') {
            
        } else {
            for(;;) {
                type = btype;
                type_decl(&type, &ad, &v, TYPE_DIRECT);
                
                s = func_sym->next;
                while (s != NULL) {
                    if ((s->v & ~SYM_FIELD) == v)
                        goto found;
                    s = s->next;
                }
                error("declaration for parameter '%s' but no such parameter",
                      get_tok_str(v, NULL));
            found:
                
                if (type.t & VT_STORAGE)
                    error("storage class specified for '%s'", get_tok_str(v, NULL)); 
                convert_parameter_type(&type);
                
                s->type = type;
                
                if (tok == ',')
                    next();
                else
                    break;
            }
        }
        skip(';');
    }
}

static void gen_function(Sym *sym)
{
    ind = cur_text_section->data_offset;
    
    put_extern_sym(sym, cur_text_section, ind, 0);
    funcname = get_tok_str(sym->v, NULL);
    func_ind = ind;
    
    if (do_debug)
        put_func_debug(sym);
    
    sym_push2(&local_stack, SYM_FIELD, 0, 0);
    gfunc_prolog(&sym->type);
    rsym = 0;
    block(NULL, NULL, NULL, NULL, 0, 0);
    gsym(rsym);
    gfunc_epilog();
    cur_text_section->data_offset = ind;
    label_pop(&global_label_stack, NULL);
    sym_pop(&local_stack, NULL); 
    
    
    ((Elf32_Sym *)symtab_section->data)[sym->c].st_size = 
        ind - func_ind;
    if (do_debug) {
        put_stabn(N_FUN, 0, 0, ind - func_ind);
    }
    funcname = ""; 
    func_vt.t = VT_VOID; 
    ind = 0; 
}

static void gen_inline_functions(void)
{
    Sym *sym;
    CType *type;
    int *str, inline_generated;

    
    for(;;) {
        inline_generated = 0;
        for(sym = global_stack; sym != NULL; sym = sym->prev) {
            type = &sym->type;
            if (((type->t & VT_BTYPE) == VT_FUNC) &&
                (type->t & (VT_STATIC | VT_INLINE)) == 
                (VT_STATIC | VT_INLINE) &&
                sym->c != 0) {
                str = (int *)sym->r;
                sym->r = VT_SYM | VT_CONST;
                type->t &= ~VT_INLINE;

                macro_ptr = str;
                next();
                cur_text_section = text_section;
                gen_function(sym);
                macro_ptr = NULL; 

                tok_str_free(str);
                inline_generated = 1;
            }
        }
        if (!inline_generated)
            break;
    }

    
    for(sym = global_stack; sym != NULL; sym = sym->prev) {
        type = &sym->type;
        if (((type->t & VT_BTYPE) == VT_FUNC) &&
            (type->t & (VT_STATIC | VT_INLINE)) == 
            (VT_STATIC | VT_INLINE)) {
            str = (int *)sym->r;
            tok_str_free(str);
            sym->r = 0; 
        }
    }
}

static void decl(int l)
{
    int v, has_init, r;
    CType type, btype;
    Sym *sym;
    AttributeDef ad;
    
    while (1) {
        if (!parse_btype(&btype, &ad)) {
            
            
            if (tok == ';') {
                next();
                continue;
            }
            if (l == VT_CONST &&
                (tok == TOK_ASM1 || tok == TOK_ASM2 || tok == TOK_ASM3)) {
                
                asm_global_instr();
                continue;
            }
            if (l == VT_LOCAL || tok < TOK_DEFINE)
                break;
            btype.t = VT_INT;
        }
        if (((btype.t & VT_BTYPE) == VT_ENUM ||
             (btype.t & VT_BTYPE) == VT_STRUCT) && 
            tok == ';') {
            
            next();
            continue;
        }
        while (1) { 
            type = btype;
            type_decl(&type, &ad, &v, TYPE_DIRECT);
#if 0
            {
                char buf[500];
                type_to_str(buf, sizeof(buf), t, get_tok_str(v, NULL));
                printf("type = '%s'\n", buf);
            }
#endif
            if ((type.t & VT_BTYPE) == VT_FUNC) {
                sym = type.ref;
                if (sym->c == FUNC_OLD)
                    func_decl_list(sym);
            }

            if (tok == '{') {
                if (l == VT_LOCAL)
                    error("cannot use local functions");
                if (!(type.t & VT_FUNC))
                    expect("function definition");

                
                sym = type.ref;
                while ((sym = sym->next) != NULL)
                    if (!(sym->v & ~SYM_FIELD))
                       expect("identifier");
                
                
                if ((type.t & (VT_EXTERN | VT_INLINE)) == (VT_EXTERN | VT_INLINE))
                    type.t = (type.t & ~VT_EXTERN) | VT_STATIC;
                
                sym = sym_find(v);
                if (sym) {
                    if ((sym->type.t & VT_BTYPE) != VT_FUNC)
                        goto func_error1;
                    
                    if (sym->type.ref->r != FUNC_CDECL &&
                        type.ref->r == FUNC_CDECL)
                        type.ref->r = sym->type.ref->r;
                    if (!is_compatible_types(&sym->type, &type)) {
                    func_error1:
                        error("incompatible types for redefinition of '%s'", 
                              get_tok_str(v, NULL));
                    }
                    
                    sym->type = type;
                } else {
                    
                    sym = global_identifier_push(v, type.t, 0);
                    sym->type.ref = type.ref;
                }

                if ((type.t & (VT_INLINE | VT_STATIC)) == 
                    (VT_INLINE | VT_STATIC)) {
                    TokenString func_str;
                    int block_level;
                           
                    tok_str_new(&func_str);
                    
                    block_level = 0;
                    for(;;) {
                        int t;
                        if (tok == TOK_EOF)
                            error("unexpected end of file");
                        tok_str_add_tok(&func_str);
                        t = tok;
                        next();
                        if (t == '{') {
                            block_level++;
                        } else if (t == '}') {
                            block_level--;
                            if (block_level == 0)
                                break;
                        }
                    }
                    tok_str_add(&func_str, -1);
                    tok_str_add(&func_str, 0);
                    sym->r = (long)func_str.str;
                } else {
                    
                    cur_text_section = ad.section;
                    if (!cur_text_section)
                        cur_text_section = text_section;
                    sym->r = VT_SYM | VT_CONST;
                    gen_function(sym);
#ifdef TCC_TARGET_PE
                    if (ad.dllexport) {
                        ((Elf32_Sym *)symtab_section->data)[sym->c].st_other |= 1;
                    }
#endif
                }
                break;
            } else {
                if (btype.t & VT_TYPEDEF) {
                    
                    
                    sym = sym_push(v, &type, 0, 0);
                    sym->type.t |= VT_TYPEDEF;
                } else if ((type.t & VT_BTYPE) == VT_FUNC) {
                    
                    
                    if (ad.func_call)
                        type.ref->r = ad.func_call;
                    external_sym(v, &type, 0);
                } else {
                    
                    r = 0;
                    if (!(type.t & VT_ARRAY))
                        r |= lvalue_type(type.t);
                    has_init = (tok == '=');
                    if ((btype.t & VT_EXTERN) || 
                        ((type.t & VT_ARRAY) && (type.t & VT_STATIC) &&
                         !has_init && l == VT_CONST && type.ref->c < 0)) {
                        
                        external_sym(v, &type, r);
                    } else {
                        if (type.t & VT_STATIC)
                            r |= VT_CONST;
                        else
                            r |= l;
                        if (has_init)
                            next();
                        decl_initializer_alloc(&type, &ad, r, 
                                               has_init, v, l);
                    }
                }
                if (tok != ',') {
                    skip(';');
                    break;
                }
                next();
            }
        }
    }
}

static void preprocess_init(TCCState *s1)
{
    s1->include_stack_ptr = s1->include_stack;
    s1->ifdef_stack_ptr = s1->ifdef_stack;
    file->ifdef_stack_ptr = s1->ifdef_stack_ptr;

    
    vtop = vstack - 1;
    s1->pack_stack[0] = 0;
    s1->pack_stack_ptr = s1->pack_stack;
}

static int tcc_compile(TCCState *s1)
{
    Sym *define_start;
    char buf[512];
    volatile int section_sym;

#ifdef INC_DEBUG
    printf("%s: **** new file\n", file->filename);
#endif
    preprocess_init(s1);

    funcname = "";
    anon_sym = SYM_FIRST_ANOM; 

    
    section_sym = 0; 
    if (do_debug) {
        section_sym = put_elf_sym(symtab_section, 0, 0, 
                                  ELF32_ST_INFO(STB_LOCAL, STT_SECTION), 0, 
                                  text_section->sh_num, NULL);
        dummy_char_star = getcwd(buf, sizeof(buf));
        pstrcat(buf, sizeof(buf), "/");
        put_stabs_r(buf, N_SO, 0, 0, 
                    text_section->data_offset, text_section, section_sym);
        put_stabs_r(file->filename, N_SO, 0, 0, 
                    text_section->data_offset, text_section, section_sym);
    }
    put_elf_sym(symtab_section, 0, 0, 
                ELF32_ST_INFO(STB_LOCAL, STT_FILE), 0, 
                SHN_ABS, file->filename);

    
    int_type.t = VT_INT;

    char_pointer_type.t = VT_BYTE;
    mk_pointer(&char_pointer_type);

    func_old_type.t = VT_FUNC;
    func_old_type.ref = sym_push(SYM_FIELD, &int_type, FUNC_CDECL, FUNC_OLD);

#if 0
    
    {
        Sym *s1;

        p = anon_sym++;
        sym = sym_push(p, mk_pointer(VT_VOID), FUNC_CDECL, FUNC_NEW);
        s1 = sym_push(SYM_FIELD, VT_UNSIGNED | VT_INT, 0, 0);
        s1->next = NULL;
        sym->next = s1;
        sym_push(TOK_alloca, VT_FUNC | (p << VT_STRUCT_SHIFT), VT_CONST, 0);
    }
#endif

    define_start = define_stack;

    if (setjmp(s1->error_jmp_buf) == 0) {
        s1->nb_errors = 0;
        s1->error_set_jmp_enabled = 1;

        ch = file->buf_ptr[0];
        tok_flags = TOK_FLAG_BOL | TOK_FLAG_BOF;
        parse_flags = PARSE_FLAG_PREPROCESS | PARSE_FLAG_TOK_NUM;
        next();
        decl(VT_CONST);
        if (tok != TOK_EOF)
            expect("declaration");

        
        if (do_debug) {
            put_stabs_r(NULL, N_SO, 0, 0, 
                        text_section->data_offset, text_section, section_sym);
        }
    }
    s1->error_set_jmp_enabled = 0;

    free_defines(define_start); 

    gen_inline_functions();

    sym_pop(&global_stack, NULL);

    return s1->nb_errors != 0 ? -1 : 0;
}

#ifdef LIBTCC
int tcc_compile_string(TCCState *s, const char *str)
{
    BufferedFile bf1, *bf = &bf1;
    int ret, len;
    char *buf;

    
    bf->fd = -1;
    
    len = strlen(str);
    buf = tcc_malloc(len + 1);
    if (!buf)
        return -1;
    memcpy(buf, str, len);
    buf[len] = CH_EOB;
    bf->buf_ptr = buf;
    bf->buf_end = buf + len;
    pstrcpy(bf->filename, sizeof(bf->filename), "<string>");
    bf->line_num = 1;
    file = bf;
    
    ret = tcc_compile(s);
    
    tcc_free(buf);

    
    return ret;
}
#endif

void tcc_define_symbol(TCCState *s1, const char *sym, const char *value)
{
    BufferedFile bf1, *bf = &bf1;

    pstrcpy(bf->buffer, IO_BUF_SIZE, sym);
    pstrcat(bf->buffer, IO_BUF_SIZE, " ");
    
    if (!value) 
        value = "1";
    pstrcat(bf->buffer, IO_BUF_SIZE, value);
    
    
    bf->fd = -1;
    bf->buf_ptr = bf->buffer;
    bf->buf_end = bf->buffer + strlen(bf->buffer);
    *bf->buf_end = CH_EOB;
    bf->filename[0] = '\0';
    bf->line_num = 1;
    file = bf;
    
    s1->include_stack_ptr = s1->include_stack;

    
    ch = file->buf_ptr[0];
    next_nomacro();
    parse_define();
    file = NULL;
}

void tcc_undefine_symbol(TCCState *s1, const char *sym)
{
    TokenSym *ts;
    Sym *s;
    ts = tok_alloc(sym, strlen(sym));
    s = define_find(ts->tok);
    
    if (s)
        define_undef(s);
}

#ifdef CONFIG_TCC_ASM

#ifdef TCC_TARGET_I386
/*
 *  i386 specific functions for TCC assembler
 * 
 *  Copyright (c) 2001, 2002 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define MAX_OPERANDS 3

typedef struct ASMInstr {
    uint16_t sym;
    uint16_t opcode;
    uint16_t instr_type;
#define OPC_JMP       0x01  
#define OPC_B         0x02  
#define OPC_WL        0x04  
#define OPC_BWL       (OPC_B | OPC_WL) 
#define OPC_REG       0x08 
#define OPC_MODRM     0x10 
#define OPC_FWAIT     0x20 
#define OPC_TEST      0x40 
#define OPC_SHIFT     0x80 
#define OPC_D16      0x0100 
#define OPC_ARITH    0x0200 
#define OPC_SHORTJMP 0x0400 
#define OPC_FARITH   0x0800 
#define OPC_GROUP_SHIFT 13

 
#define OPT_REG8  0 
#define OPT_REG16 1 
#define OPT_REG32 2 
#define OPT_MMX   3 
#define OPT_SSE   4 
#define OPT_CR    5 
#define OPT_TR    6 
#define OPT_DB    7 
#define OPT_SEG   8
#define OPT_ST    9
#define OPT_IM8   10
#define OPT_IM8S  11
#define OPT_IM16  12
#define OPT_IM32  13
#define OPT_EAX   14 
#define OPT_ST0   15 
#define OPT_CL    16 
#define OPT_DX    17 
#define OPT_ADDR  18 
#define OPT_INDIR 19 

 
#define OPT_COMPOSITE_FIRST   20
#define OPT_IM       20 
#define OPT_REG      21  
#define OPT_REGW     22 
#define OPT_IMW      23  

#define OPT_EA    0x80

    uint8_t nb_ops;
    uint8_t op_type[MAX_OPERANDS]; 
} ASMInstr;

typedef struct Operand {
    uint32_t type;
#define OP_REG8   (1 << OPT_REG8)
#define OP_REG16  (1 << OPT_REG16)
#define OP_REG32  (1 << OPT_REG32)
#define OP_MMX    (1 << OPT_MMX)
#define OP_SSE    (1 << OPT_SSE)
#define OP_CR     (1 << OPT_CR)
#define OP_TR     (1 << OPT_TR)
#define OP_DB     (1 << OPT_DB)
#define OP_SEG    (1 << OPT_SEG)
#define OP_ST     (1 << OPT_ST)
#define OP_IM8    (1 << OPT_IM8)
#define OP_IM8S   (1 << OPT_IM8S)
#define OP_IM16   (1 << OPT_IM16)
#define OP_IM32   (1 << OPT_IM32)
#define OP_EAX    (1 << OPT_EAX)
#define OP_ST0    (1 << OPT_ST0)
#define OP_CL     (1 << OPT_CL)
#define OP_DX     (1 << OPT_DX)
#define OP_ADDR   (1 << OPT_ADDR)
#define OP_INDIR  (1 << OPT_INDIR)

#define OP_EA     0x40000000
#define OP_REG    (OP_REG8 | OP_REG16 | OP_REG32)
#define OP_IM     OP_IM32
    int8_t  reg; 
    int8_t  reg2; 
    uint8_t shift;
    ExprValue e;
} Operand;

static const uint8_t reg_to_size[5] = {
    [OP_REG8] = 0,
    [OP_REG16] = 1,
    [OP_REG32] = 2,
};
    
#define WORD_PREFIX_OPCODE 0x66

#define NB_TEST_OPCODES 30

static const uint8_t test_bits[NB_TEST_OPCODES] = {
 0x00, 
 0x01, 
 0x02, 
 0x02, 
 0x02, 
 0x03, 
 0x03, 
 0x03, 
 0x04, 
 0x04, 
 0x05, 
 0x05, 
 0x06, 
 0x06, 
 0x07, 
 0x07, 
 0x08, 
 0x09, 
 0x0a, 
 0x0a, 
 0x0b, 
 0x0b, 
 0x0c, 
 0x0c, 
 0x0d, 
 0x0d, 
 0x0e, 
 0x0e, 
 0x0f, 
 0x0f, 
};

static const ASMInstr asm_instrs[] = {
#define ALT(x) x
#define DEF_ASM_OP0(name, opcode)
#define DEF_ASM_OP0L(name, opcode, group, instr_type) { TOK_ASM_ ## name, opcode, (instr_type | group << OPC_GROUP_SHIFT), 0 },
#define DEF_ASM_OP1(name, opcode, group, instr_type, op0) { TOK_ASM_ ## name, opcode, (instr_type | group << OPC_GROUP_SHIFT), 1, { op0 }},
#define DEF_ASM_OP2(name, opcode, group, instr_type, op0, op1) { TOK_ASM_ ## name, opcode, (instr_type | group << OPC_GROUP_SHIFT), 2, { op0, op1 }},
#define DEF_ASM_OP3(name, opcode, group, instr_type, op0, op1, op2) { TOK_ASM_ ## name, opcode, (instr_type | group << OPC_GROUP_SHIFT), 3, { op0, op1, op2 }},
     DEF_ASM_OP0(pusha, 0x60) 
     DEF_ASM_OP0(popa, 0x61)
     DEF_ASM_OP0(clc, 0xf8)
     DEF_ASM_OP0(cld, 0xfc)
     DEF_ASM_OP0(cli, 0xfa)
     DEF_ASM_OP0(clts, 0x0f06)
     DEF_ASM_OP0(cmc, 0xf5)
     DEF_ASM_OP0(lahf, 0x9f)
     DEF_ASM_OP0(sahf, 0x9e)
     DEF_ASM_OP0(pushfl, 0x9c)
     DEF_ASM_OP0(popfl, 0x9d)
     DEF_ASM_OP0(pushf, 0x9c)
     DEF_ASM_OP0(popf, 0x9d)
     DEF_ASM_OP0(stc, 0xf9)
     DEF_ASM_OP0(std, 0xfd)
     DEF_ASM_OP0(sti, 0xfb)
     DEF_ASM_OP0(aaa, 0x37)
     DEF_ASM_OP0(aas, 0x3f)
     DEF_ASM_OP0(daa, 0x27)
     DEF_ASM_OP0(das, 0x2f)
     DEF_ASM_OP0(aad, 0xd50a)
     DEF_ASM_OP0(aam, 0xd40a)
     DEF_ASM_OP0(cbw, 0x6698)
     DEF_ASM_OP0(cwd, 0x6699)
     DEF_ASM_OP0(cwde, 0x98)
     DEF_ASM_OP0(cdq, 0x99)
     DEF_ASM_OP0(cbtw, 0x6698)
     DEF_ASM_OP0(cwtl, 0x98)
     DEF_ASM_OP0(cwtd, 0x6699)
     DEF_ASM_OP0(cltd, 0x99)
     DEF_ASM_OP0(int3, 0xcc)
     DEF_ASM_OP0(into, 0xce)
     DEF_ASM_OP0(iret, 0xcf)
     DEF_ASM_OP0(rsm, 0x0faa)
     DEF_ASM_OP0(hlt, 0xf4)
     DEF_ASM_OP0(wait, 0x9b)
     DEF_ASM_OP0(nop, 0x90)
     DEF_ASM_OP0(xlat, 0xd7)

     
ALT(DEF_ASM_OP0L(cmpsb, 0xa6, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(scmpb, 0xa6, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(insb, 0x6c, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(outsb, 0x6e, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(lodsb, 0xac, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(slodb, 0xac, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(movsb, 0xa4, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(smovb, 0xa4, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(scasb, 0xae, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(sscab, 0xae, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(stosb, 0xaa, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(sstob, 0xaa, 0, OPC_BWL))

     
     
ALT(DEF_ASM_OP2(bsfw, 0x0fbc, 0, OPC_MODRM | OPC_WL, OPT_REGW | OPT_EA, OPT_REGW))
ALT(DEF_ASM_OP2(bsrw, 0x0fbd, 0, OPC_MODRM | OPC_WL, OPT_REGW | OPT_EA, OPT_REGW))

ALT(DEF_ASM_OP2(btw, 0x0fa3, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP2(btw, 0x0fba, 4, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW | OPT_EA))

ALT(DEF_ASM_OP2(btsw, 0x0fab, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP2(btsw, 0x0fba, 5, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW | OPT_EA))

ALT(DEF_ASM_OP2(btrw, 0x0fb3, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP2(btrw, 0x0fba, 6, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW | OPT_EA))

ALT(DEF_ASM_OP2(btcw, 0x0fbb, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP2(btcw, 0x0fba, 7, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW | OPT_EA))

     
     DEF_ASM_OP0(aword, 0x67)
     DEF_ASM_OP0(addr16, 0x67)
     DEF_ASM_OP0(word, 0x66)
     DEF_ASM_OP0(data16, 0x66)
     DEF_ASM_OP0(lock, 0xf0)
     DEF_ASM_OP0(rep, 0xf3)
     DEF_ASM_OP0(repe, 0xf3)
     DEF_ASM_OP0(repz, 0xf3)
     DEF_ASM_OP0(repne, 0xf2)
     DEF_ASM_OP0(repnz, 0xf2)
             
     DEF_ASM_OP0(invd, 0x0f08)
     DEF_ASM_OP0(wbinvd, 0x0f09)
     DEF_ASM_OP0(cpuid, 0x0fa2)
     DEF_ASM_OP0(wrmsr, 0x0f30)
     DEF_ASM_OP0(rdtsc, 0x0f31)
     DEF_ASM_OP0(rdmsr, 0x0f32)
     DEF_ASM_OP0(rdpmc, 0x0f33)
     DEF_ASM_OP0(ud2, 0x0f0b)

     
ALT(DEF_ASM_OP2(movb, 0xa0, 0, OPC_BWL, OPT_ADDR, OPT_EAX))
ALT(DEF_ASM_OP2(movb, 0xa2, 0, OPC_BWL, OPT_EAX, OPT_ADDR))
ALT(DEF_ASM_OP2(movb, 0x88, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(movb, 0x8a, 0, OPC_MODRM | OPC_BWL, OPT_EA | OPT_REG, OPT_REG))
ALT(DEF_ASM_OP2(movb, 0xb0, 0, OPC_REG | OPC_BWL, OPT_IM, OPT_REG))
ALT(DEF_ASM_OP2(movb, 0xc6, 0, OPC_MODRM | OPC_BWL, OPT_IM, OPT_REG | OPT_EA))

ALT(DEF_ASM_OP2(movw, 0x8c, 0, OPC_MODRM | OPC_WL, OPT_SEG, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(movw, 0x8e, 0, OPC_MODRM | OPC_WL, OPT_EA | OPT_REG, OPT_SEG))

ALT(DEF_ASM_OP2(movw, 0x0f20, 0, OPC_MODRM | OPC_WL, OPT_CR, OPT_REG32))
ALT(DEF_ASM_OP2(movw, 0x0f21, 0, OPC_MODRM | OPC_WL, OPT_DB, OPT_REG32))
ALT(DEF_ASM_OP2(movw, 0x0f24, 0, OPC_MODRM | OPC_WL, OPT_TR, OPT_REG32))
ALT(DEF_ASM_OP2(movw, 0x0f22, 0, OPC_MODRM | OPC_WL, OPT_REG32, OPT_CR))
ALT(DEF_ASM_OP2(movw, 0x0f23, 0, OPC_MODRM | OPC_WL, OPT_REG32, OPT_DB))
ALT(DEF_ASM_OP2(movw, 0x0f26, 0, OPC_MODRM | OPC_WL, OPT_REG32, OPT_TR))

ALT(DEF_ASM_OP2(movsbl, 0x0fbe, 0, OPC_MODRM, OPT_REG8 | OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(movsbw, 0x0fbe, 0, OPC_MODRM | OPC_D16, OPT_REG8 | OPT_EA, OPT_REG16))
ALT(DEF_ASM_OP2(movswl, 0x0fbf, 0, OPC_MODRM, OPT_REG16 | OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(movzbw, 0x0fb6, 0, OPC_MODRM | OPC_WL, OPT_REG8 | OPT_EA, OPT_REGW))
ALT(DEF_ASM_OP2(movzwl, 0x0fb7, 0, OPC_MODRM, OPT_REG16 | OPT_EA, OPT_REG32))

ALT(DEF_ASM_OP1(pushw, 0x50, 0, OPC_REG | OPC_WL, OPT_REGW))
ALT(DEF_ASM_OP1(pushw, 0xff, 6, OPC_MODRM | OPC_WL, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP1(pushw, 0x6a, 0, OPC_WL, OPT_IM8S))
ALT(DEF_ASM_OP1(pushw, 0x68, 0, OPC_WL, OPT_IM32))
ALT(DEF_ASM_OP1(pushw, 0x06, 0, OPC_WL, OPT_SEG))

ALT(DEF_ASM_OP1(popw, 0x58, 0, OPC_REG | OPC_WL, OPT_REGW))
ALT(DEF_ASM_OP1(popw, 0x8f, 0, OPC_MODRM | OPC_WL, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP1(popw, 0x07, 0, OPC_WL, OPT_SEG))

ALT(DEF_ASM_OP2(xchgw, 0x90, 0, OPC_REG | OPC_WL, OPT_REG, OPT_EAX))
ALT(DEF_ASM_OP2(xchgw, 0x90, 0, OPC_REG | OPC_WL, OPT_EAX, OPT_REG))
ALT(DEF_ASM_OP2(xchgb, 0x86, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(xchgb, 0x86, 0, OPC_MODRM | OPC_BWL, OPT_EA | OPT_REG, OPT_REG))

ALT(DEF_ASM_OP2(inb, 0xe4, 0, OPC_BWL, OPT_IM8, OPT_EAX))
ALT(DEF_ASM_OP1(inb, 0xe4, 0, OPC_BWL, OPT_IM8))
ALT(DEF_ASM_OP2(inb, 0xec, 0, OPC_BWL, OPT_DX, OPT_EAX))
ALT(DEF_ASM_OP1(inb, 0xec, 0, OPC_BWL, OPT_DX))

ALT(DEF_ASM_OP2(outb, 0xe6, 0, OPC_BWL, OPT_EAX, OPT_IM8))
ALT(DEF_ASM_OP1(outb, 0xe6, 0, OPC_BWL, OPT_IM8))
ALT(DEF_ASM_OP2(outb, 0xee, 0, OPC_BWL, OPT_EAX, OPT_DX))
ALT(DEF_ASM_OP1(outb, 0xee, 0, OPC_BWL, OPT_DX))

ALT(DEF_ASM_OP2(leaw, 0x8d, 0, OPC_MODRM | OPC_WL, OPT_EA, OPT_REG))

ALT(DEF_ASM_OP2(les, 0xc4, 0, OPC_MODRM, OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(lds, 0xc5, 0, OPC_MODRM, OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(lss, 0x0fb2, 0, OPC_MODRM, OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(lfs, 0x0fb4, 0, OPC_MODRM, OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(lgs, 0x0fb5, 0, OPC_MODRM, OPT_EA, OPT_REG32))

     
ALT(DEF_ASM_OP2(addb, 0x00, 0, OPC_ARITH | OPC_MODRM | OPC_BWL, OPT_REG, OPT_EA | OPT_REG)) 
ALT(DEF_ASM_OP2(addb, 0x02, 0, OPC_ARITH | OPC_MODRM | OPC_BWL, OPT_EA | OPT_REG, OPT_REG))
ALT(DEF_ASM_OP2(addb, 0x04, 0, OPC_ARITH | OPC_BWL, OPT_IM, OPT_EAX))
ALT(DEF_ASM_OP2(addb, 0x80, 0, OPC_ARITH | OPC_MODRM | OPC_BWL, OPT_IM, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(addw, 0x83, 0, OPC_ARITH | OPC_MODRM | OPC_WL, OPT_IM8S, OPT_EA | OPT_REG))

ALT(DEF_ASM_OP2(testb, 0x84, 0, OPC_MODRM | OPC_BWL, OPT_EA | OPT_REG, OPT_REG))
ALT(DEF_ASM_OP2(testb, 0x84, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(testb, 0xa8, 0, OPC_BWL, OPT_IM, OPT_EAX))
ALT(DEF_ASM_OP2(testb, 0xf6, 0, OPC_MODRM | OPC_BWL, OPT_IM, OPT_EA | OPT_REG))

ALT(DEF_ASM_OP1(incw, 0x40, 0, OPC_REG | OPC_WL, OPT_REGW))
ALT(DEF_ASM_OP1(incb, 0xfe, 0, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP1(decw, 0x48, 0, OPC_REG | OPC_WL, OPT_REGW))
ALT(DEF_ASM_OP1(decb, 0xfe, 1, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))

ALT(DEF_ASM_OP1(notb, 0xf6, 2, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP1(negb, 0xf6, 3, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))

ALT(DEF_ASM_OP1(mulb, 0xf6, 4, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP1(imulb, 0xf6, 5, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))

ALT(DEF_ASM_OP2(imulw, 0x0faf, 0, OPC_MODRM | OPC_WL, OPT_REG | OPT_EA, OPT_REG))
ALT(DEF_ASM_OP3(imulw, 0x6b, 0, OPC_MODRM | OPC_WL, OPT_IM8S, OPT_REGW | OPT_EA, OPT_REGW))
ALT(DEF_ASM_OP2(imulw, 0x6b, 0, OPC_MODRM | OPC_WL, OPT_IM8S, OPT_REGW))
ALT(DEF_ASM_OP3(imulw, 0x69, 0, OPC_MODRM | OPC_WL, OPT_IMW, OPT_REGW | OPT_EA, OPT_REGW))
ALT(DEF_ASM_OP2(imulw, 0x69, 0, OPC_MODRM | OPC_WL, OPT_IMW, OPT_REGW))

ALT(DEF_ASM_OP1(divb, 0xf6, 6, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP2(divb, 0xf6, 6, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA, OPT_EAX))
ALT(DEF_ASM_OP1(idivb, 0xf6, 7, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP2(idivb, 0xf6, 7, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA, OPT_EAX))

     
ALT(DEF_ASM_OP2(rolb, 0xc0, 0, OPC_MODRM | OPC_BWL | OPC_SHIFT, OPT_IM8, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(rolb, 0xd2, 0, OPC_MODRM | OPC_BWL | OPC_SHIFT, OPT_CL, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP1(rolb, 0xd0, 0, OPC_MODRM | OPC_BWL | OPC_SHIFT, OPT_EA | OPT_REG))

ALT(DEF_ASM_OP3(shldw, 0x0fa4, 0, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP3(shldw, 0x0fa5, 0, OPC_MODRM | OPC_WL, OPT_CL, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP2(shldw, 0x0fa5, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP3(shrdw, 0x0fac, 0, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP3(shrdw, 0x0fad, 0, OPC_MODRM | OPC_WL, OPT_CL, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP2(shrdw, 0x0fad, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_EA | OPT_REGW))

ALT(DEF_ASM_OP1(call, 0xff, 2, OPC_MODRM, OPT_INDIR))
ALT(DEF_ASM_OP1(call, 0xe8, 0, OPC_JMP, OPT_ADDR))
ALT(DEF_ASM_OP1(jmp, 0xff, 4, OPC_MODRM, OPT_INDIR))
ALT(DEF_ASM_OP1(jmp, 0xeb, 0, OPC_SHORTJMP | OPC_JMP, OPT_ADDR))

ALT(DEF_ASM_OP2(lcall, 0x9a, 0, 0, OPT_IM16, OPT_IM32))
ALT(DEF_ASM_OP1(lcall, 0xff, 3, 0, OPT_EA))
ALT(DEF_ASM_OP2(ljmp, 0xea, 0, 0, OPT_IM16, OPT_IM32))
ALT(DEF_ASM_OP1(ljmp, 0xff, 5, 0, OPT_EA))

ALT(DEF_ASM_OP1(int, 0xcd, 0, 0, OPT_IM8))
ALT(DEF_ASM_OP1(seto, 0x0f90, 0, OPC_MODRM | OPC_TEST, OPT_REG8 | OPT_EA))
    DEF_ASM_OP2(enter, 0xc8, 0, 0, OPT_IM16, OPT_IM8)
    DEF_ASM_OP0(leave, 0xc9)
    DEF_ASM_OP0(ret, 0xc3)
ALT(DEF_ASM_OP1(ret, 0xc2, 0, 0, OPT_IM16))
    DEF_ASM_OP0(lret, 0xcb)
ALT(DEF_ASM_OP1(lret, 0xca, 0, 0, OPT_IM16))

ALT(DEF_ASM_OP1(jo, 0x70, 0, OPC_SHORTJMP | OPC_JMP | OPC_TEST, OPT_ADDR))
    DEF_ASM_OP1(loopne, 0xe0, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(loopnz, 0xe0, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(loope, 0xe1, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(loopz, 0xe1, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(loop, 0xe2, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(jecxz, 0xe3, 0, OPC_SHORTJMP, OPT_ADDR)
     
     
     
ALT(DEF_ASM_OP0L(fcomp, 0xd8d9, 0, 0))

ALT(DEF_ASM_OP1(fadd, 0xd8c0, 0, OPC_FARITH | OPC_REG, OPT_ST))
ALT(DEF_ASM_OP2(fadd, 0xd8c0, 0, OPC_FARITH | OPC_REG, OPT_ST, OPT_ST0))
ALT(DEF_ASM_OP0L(fadd, 0xdec1, 0, OPC_FARITH))
ALT(DEF_ASM_OP1(faddp, 0xdec0, 0, OPC_FARITH | OPC_REG, OPT_ST))
ALT(DEF_ASM_OP2(faddp, 0xdec0, 0, OPC_FARITH | OPC_REG, OPT_ST, OPT_ST0))
ALT(DEF_ASM_OP2(faddp, 0xdec0, 0, OPC_FARITH | OPC_REG, OPT_ST0, OPT_ST))
ALT(DEF_ASM_OP0L(faddp, 0xdec1, 0, OPC_FARITH))
ALT(DEF_ASM_OP1(fadds, 0xd8, 0, OPC_FARITH | OPC_MODRM, OPT_EA))
ALT(DEF_ASM_OP1(fiaddl, 0xda, 0, OPC_FARITH | OPC_MODRM, OPT_EA))
ALT(DEF_ASM_OP1(faddl, 0xdc, 0, OPC_FARITH | OPC_MODRM, OPT_EA))
ALT(DEF_ASM_OP1(fiadds, 0xde, 0, OPC_FARITH | OPC_MODRM, OPT_EA))

     DEF_ASM_OP0(fucompp, 0xdae9)
     DEF_ASM_OP0(ftst, 0xd9e4)
     DEF_ASM_OP0(fxam, 0xd9e5)
     DEF_ASM_OP0(fld1, 0xd9e8)
     DEF_ASM_OP0(fldl2t, 0xd9e9)
     DEF_ASM_OP0(fldl2e, 0xd9ea)
     DEF_ASM_OP0(fldpi, 0xd9eb)
     DEF_ASM_OP0(fldlg2, 0xd9ec)
     DEF_ASM_OP0(fldln2, 0xd9ed)
     DEF_ASM_OP0(fldz, 0xd9ee)

     DEF_ASM_OP0(f2xm1, 0xd9f0)
     DEF_ASM_OP0(fyl2x, 0xd9f1)
     DEF_ASM_OP0(fptan, 0xd9f2)
     DEF_ASM_OP0(fpatan, 0xd9f3)
     DEF_ASM_OP0(fxtract, 0xd9f4)
     DEF_ASM_OP0(fprem1, 0xd9f5)
     DEF_ASM_OP0(fdecstp, 0xd9f6)
     DEF_ASM_OP0(fincstp, 0xd9f7)
     DEF_ASM_OP0(fprem, 0xd9f8)
     DEF_ASM_OP0(fyl2xp1, 0xd9f9)
     DEF_ASM_OP0(fsqrt, 0xd9fa)
     DEF_ASM_OP0(fsincos, 0xd9fb)
     DEF_ASM_OP0(frndint, 0xd9fc)
     DEF_ASM_OP0(fscale, 0xd9fd)
     DEF_ASM_OP0(fsin, 0xd9fe)
     DEF_ASM_OP0(fcos, 0xd9ff)
     DEF_ASM_OP0(fchs, 0xd9e0)
     DEF_ASM_OP0(fabs, 0xd9e1)
     DEF_ASM_OP0(fninit, 0xdbe3)
     DEF_ASM_OP0(fnclex, 0xdbe2)
     DEF_ASM_OP0(fnop, 0xd9d0)
     DEF_ASM_OP0(fwait, 0x9b)

    
    DEF_ASM_OP1(fld, 0xd9c0, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(fldl, 0xd9c0, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(flds, 0xd9, 0, OPC_MODRM, OPT_EA)
ALT(DEF_ASM_OP1(fldl, 0xdd, 0, OPC_MODRM, OPT_EA))
    DEF_ASM_OP1(fildl, 0xdb, 0, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fildq, 0xdf, 5, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fildll, 0xdf, 5, OPC_MODRM,OPT_EA)
    DEF_ASM_OP1(fldt, 0xdb, 5, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fbld, 0xdf, 4, OPC_MODRM, OPT_EA)
    
    
    DEF_ASM_OP1(fst, 0xddd0, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(fstl, 0xddd0, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(fsts, 0xd9, 2, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fstps, 0xd9, 3, OPC_MODRM, OPT_EA)
ALT(DEF_ASM_OP1(fstl, 0xdd, 2, OPC_MODRM, OPT_EA))
    DEF_ASM_OP1(fstpl, 0xdd, 3, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fist, 0xdf, 2, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fistp, 0xdf, 3, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fistl, 0xdb, 2, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fistpl, 0xdb, 3, OPC_MODRM, OPT_EA)

    DEF_ASM_OP1(fstp, 0xddd8, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(fistpq, 0xdf, 7, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fistpll, 0xdf, 7, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fstpt, 0xdb, 7, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fbstp, 0xdf, 6, OPC_MODRM, OPT_EA)

    
    DEF_ASM_OP0(fxch, 0xd9c9)
ALT(DEF_ASM_OP1(fxch, 0xd9c8, 0, OPC_REG, OPT_ST))

    
    DEF_ASM_OP1(fucom, 0xdde0, 0, OPC_REG, OPT_ST )
    DEF_ASM_OP1(fucomp, 0xdde8, 0, OPC_REG, OPT_ST )

    DEF_ASM_OP0L(finit, 0xdbe3, 0, OPC_FWAIT)
    DEF_ASM_OP1(fldcw, 0xd9, 5, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fnstcw, 0xd9, 7, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fstcw, 0xd9, 7, OPC_MODRM | OPC_FWAIT, OPT_EA )
    DEF_ASM_OP0(fnstsw, 0xdfe0)
ALT(DEF_ASM_OP1(fnstsw, 0xdfe0, 0, 0, OPT_EAX ))
ALT(DEF_ASM_OP1(fnstsw, 0xdd, 7, OPC_MODRM, OPT_EA ))
    DEF_ASM_OP1(fstsw, 0xdfe0, 0, OPC_FWAIT, OPT_EAX )
ALT(DEF_ASM_OP0L(fstsw, 0xdfe0, 0, OPC_FWAIT))
ALT(DEF_ASM_OP1(fstsw, 0xdd, 7, OPC_MODRM | OPC_FWAIT, OPT_EA ))
    DEF_ASM_OP0L(fclex, 0xdbe2, 0, OPC_FWAIT)
    DEF_ASM_OP1(fnstenv, 0xd9, 6, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fstenv, 0xd9, 6, OPC_MODRM | OPC_FWAIT, OPT_EA )
    DEF_ASM_OP1(fldenv, 0xd9, 4, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fnsave, 0xdd, 6, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fsave, 0xdd, 6, OPC_MODRM | OPC_FWAIT, OPT_EA )
    DEF_ASM_OP1(frstor, 0xdd, 4, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(ffree, 0xddc0, 4, OPC_REG, OPT_ST )
    DEF_ASM_OP1(ffreep, 0xdfc0, 4, OPC_REG, OPT_ST )
    DEF_ASM_OP1(fxsave, 0x0fae, 0, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fxrstor, 0x0fae, 1, OPC_MODRM, OPT_EA )

    
    DEF_ASM_OP2(arpl, 0x63, 0, OPC_MODRM, OPT_REG16, OPT_REG16 | OPT_EA)
    DEF_ASM_OP2(lar, 0x0f02, 0, OPC_MODRM, OPT_REG32 | OPT_EA, OPT_REG32)
    DEF_ASM_OP1(lgdt, 0x0f01, 2, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(lidt, 0x0f01, 3, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(lldt, 0x0f00, 2, OPC_MODRM, OPT_EA | OPT_REG)
    DEF_ASM_OP1(lmsw, 0x0f01, 6, OPC_MODRM, OPT_EA | OPT_REG)
ALT(DEF_ASM_OP2(lslw, 0x0f03, 0, OPC_MODRM | OPC_WL, OPT_EA | OPT_REG, OPT_REG))
    DEF_ASM_OP1(ltr, 0x0f00, 3, OPC_MODRM, OPT_EA | OPT_REG)
    DEF_ASM_OP1(sgdt, 0x0f01, 0, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(sidt, 0x0f01, 1, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(sldt, 0x0f00, 0, OPC_MODRM, OPT_REG | OPT_EA)
    DEF_ASM_OP1(smsw, 0x0f01, 4, OPC_MODRM, OPT_REG | OPT_EA)
    DEF_ASM_OP1(str, 0x0f00, 1, OPC_MODRM, OPT_REG16| OPT_EA)
    DEF_ASM_OP1(verr, 0x0f00, 4, OPC_MODRM, OPT_REG | OPT_EA)
    DEF_ASM_OP1(verw, 0x0f00, 5, OPC_MODRM, OPT_REG | OPT_EA)

    
    DEF_ASM_OP1(bswap, 0x0fc8, 0, OPC_REG, OPT_REG32 )
ALT(DEF_ASM_OP2(xaddb, 0x0fc0, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_REG | OPT_EA ))
ALT(DEF_ASM_OP2(cmpxchgb, 0x0fb0, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_REG | OPT_EA ))
    DEF_ASM_OP1(invlpg, 0x0f01, 7, OPC_MODRM, OPT_EA )

    DEF_ASM_OP2(boundl, 0x62, 0, OPC_MODRM, OPT_REG32, OPT_EA)
    DEF_ASM_OP2(boundw, 0x62, 0, OPC_MODRM | OPC_D16, OPT_REG16, OPT_EA)

    
    DEF_ASM_OP1(cmpxchg8b, 0x0fc7, 1, OPC_MODRM, OPT_EA )
    
    
    ALT(DEF_ASM_OP2(cmovo, 0x0f40, 0, OPC_MODRM | OPC_TEST, OPT_REG32 | OPT_EA, OPT_REG32))

    DEF_ASM_OP2(fcmovb, 0xdac0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmove, 0xdac8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovbe, 0xdad0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovu, 0xdad8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovnb, 0xdbc0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovne, 0xdbc8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovnbe, 0xdbd0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovnu, 0xdbd8, 0, OPC_REG, OPT_ST, OPT_ST0 )

    DEF_ASM_OP2(fucomi, 0xdbe8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcomi, 0xdbf0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fucomip, 0xdfe8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcomip, 0xdff0, 0, OPC_REG, OPT_ST, OPT_ST0 )

    
    DEF_ASM_OP0(emms, 0x0f77) 
    DEF_ASM_OP2(movd, 0x0f6e, 0, OPC_MODRM, OPT_EA | OPT_REG32, OPT_MMX )
ALT(DEF_ASM_OP2(movd, 0x0f7e, 0, OPC_MODRM, OPT_MMX, OPT_EA | OPT_REG32 ))
    DEF_ASM_OP2(movq, 0x0f6f, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(movq, 0x0f7f, 0, OPC_MODRM, OPT_MMX, OPT_EA | OPT_MMX ))
    DEF_ASM_OP2(packssdw, 0x0f6b, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(packsswb, 0x0f63, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(packuswb, 0x0f67, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddb, 0x0ffc, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddw, 0x0ffd, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddd, 0x0ffe, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddsb, 0x0fec, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddsw, 0x0fed, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddusb, 0x0fdc, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddusw, 0x0fdd, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pand, 0x0fdb, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pandn, 0x0fdf, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpeqb, 0x0f74, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpeqw, 0x0f75, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpeqd, 0x0f76, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpgtb, 0x0f64, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpgtw, 0x0f65, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpgtd, 0x0f66, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pmaddwd, 0x0ff5, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pmulhw, 0x0fe5, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pmullw, 0x0fd5, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(por, 0x0feb, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psllw, 0x0ff1, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psllw, 0x0f71, 6, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(pslld, 0x0ff2, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(pslld, 0x0f72, 6, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psllq, 0x0ff3, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psllq, 0x0f73, 6, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psraw, 0x0fe1, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psraw, 0x0f71, 4, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psrad, 0x0fe2, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psrad, 0x0f72, 4, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psrlw, 0x0fd1, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psrlw, 0x0f71, 2, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psrld, 0x0fd2, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psrld, 0x0f72, 2, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psrlq, 0x0fd3, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psrlq, 0x0f73, 2, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psubb, 0x0ff8, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubw, 0x0ff9, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubd, 0x0ffa, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubsb, 0x0fe8, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubsw, 0x0fe9, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubusb, 0x0fd8, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubusw, 0x0fd9, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpckhbw, 0x0f68, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpckhwd, 0x0f69, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpckhdq, 0x0f6a, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpcklbw, 0x0f60, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpcklwd, 0x0f61, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpckldq, 0x0f62, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pxor, 0x0fef, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )

#undef ALT
#undef DEF_ASM_OP0
#undef DEF_ASM_OP0L
#undef DEF_ASM_OP1
#undef DEF_ASM_OP2
#undef DEF_ASM_OP3

    
    { 0, },
};

static const uint16_t op0_codes[] = {
#define ALT(x)
#define DEF_ASM_OP0(x, opcode) opcode,
#define DEF_ASM_OP0L(name, opcode, group, instr_type)
#define DEF_ASM_OP1(name, opcode, group, instr_type, op0)
#define DEF_ASM_OP2(name, opcode, group, instr_type, op0, op1)
#define DEF_ASM_OP3(name, opcode, group, instr_type, op0, op1, op2)
     DEF_ASM_OP0(pusha, 0x60) 
     DEF_ASM_OP0(popa, 0x61)
     DEF_ASM_OP0(clc, 0xf8)
     DEF_ASM_OP0(cld, 0xfc)
     DEF_ASM_OP0(cli, 0xfa)
     DEF_ASM_OP0(clts, 0x0f06)
     DEF_ASM_OP0(cmc, 0xf5)
     DEF_ASM_OP0(lahf, 0x9f)
     DEF_ASM_OP0(sahf, 0x9e)
     DEF_ASM_OP0(pushfl, 0x9c)
     DEF_ASM_OP0(popfl, 0x9d)
     DEF_ASM_OP0(pushf, 0x9c)
     DEF_ASM_OP0(popf, 0x9d)
     DEF_ASM_OP0(stc, 0xf9)
     DEF_ASM_OP0(std, 0xfd)
     DEF_ASM_OP0(sti, 0xfb)
     DEF_ASM_OP0(aaa, 0x37)
     DEF_ASM_OP0(aas, 0x3f)
     DEF_ASM_OP0(daa, 0x27)
     DEF_ASM_OP0(das, 0x2f)
     DEF_ASM_OP0(aad, 0xd50a)
     DEF_ASM_OP0(aam, 0xd40a)
     DEF_ASM_OP0(cbw, 0x6698)
     DEF_ASM_OP0(cwd, 0x6699)
     DEF_ASM_OP0(cwde, 0x98)
     DEF_ASM_OP0(cdq, 0x99)
     DEF_ASM_OP0(cbtw, 0x6698)
     DEF_ASM_OP0(cwtl, 0x98)
     DEF_ASM_OP0(cwtd, 0x6699)
     DEF_ASM_OP0(cltd, 0x99)
     DEF_ASM_OP0(int3, 0xcc)
     DEF_ASM_OP0(into, 0xce)
     DEF_ASM_OP0(iret, 0xcf)
     DEF_ASM_OP0(rsm, 0x0faa)
     DEF_ASM_OP0(hlt, 0xf4)
     DEF_ASM_OP0(wait, 0x9b)
     DEF_ASM_OP0(nop, 0x90)
     DEF_ASM_OP0(xlat, 0xd7)

     
ALT(DEF_ASM_OP0L(cmpsb, 0xa6, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(scmpb, 0xa6, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(insb, 0x6c, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(outsb, 0x6e, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(lodsb, 0xac, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(slodb, 0xac, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(movsb, 0xa4, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(smovb, 0xa4, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(scasb, 0xae, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(sscab, 0xae, 0, OPC_BWL))

ALT(DEF_ASM_OP0L(stosb, 0xaa, 0, OPC_BWL))
ALT(DEF_ASM_OP0L(sstob, 0xaa, 0, OPC_BWL))

     
     
ALT(DEF_ASM_OP2(bsfw, 0x0fbc, 0, OPC_MODRM | OPC_WL, OPT_REGW | OPT_EA, OPT_REGW))
ALT(DEF_ASM_OP2(bsrw, 0x0fbd, 0, OPC_MODRM | OPC_WL, OPT_REGW | OPT_EA, OPT_REGW))

ALT(DEF_ASM_OP2(btw, 0x0fa3, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP2(btw, 0x0fba, 4, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW | OPT_EA))

ALT(DEF_ASM_OP2(btsw, 0x0fab, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP2(btsw, 0x0fba, 5, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW | OPT_EA))

ALT(DEF_ASM_OP2(btrw, 0x0fb3, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP2(btrw, 0x0fba, 6, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW | OPT_EA))

ALT(DEF_ASM_OP2(btcw, 0x0fbb, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP2(btcw, 0x0fba, 7, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW | OPT_EA))

     
     DEF_ASM_OP0(aword, 0x67)
     DEF_ASM_OP0(addr16, 0x67)
     DEF_ASM_OP0(word, 0x66)
     DEF_ASM_OP0(data16, 0x66)
     DEF_ASM_OP0(lock, 0xf0)
     DEF_ASM_OP0(rep, 0xf3)
     DEF_ASM_OP0(repe, 0xf3)
     DEF_ASM_OP0(repz, 0xf3)
     DEF_ASM_OP0(repne, 0xf2)
     DEF_ASM_OP0(repnz, 0xf2)
             
     DEF_ASM_OP0(invd, 0x0f08)
     DEF_ASM_OP0(wbinvd, 0x0f09)
     DEF_ASM_OP0(cpuid, 0x0fa2)
     DEF_ASM_OP0(wrmsr, 0x0f30)
     DEF_ASM_OP0(rdtsc, 0x0f31)
     DEF_ASM_OP0(rdmsr, 0x0f32)
     DEF_ASM_OP0(rdpmc, 0x0f33)
     DEF_ASM_OP0(ud2, 0x0f0b)

     
ALT(DEF_ASM_OP2(movb, 0xa0, 0, OPC_BWL, OPT_ADDR, OPT_EAX))
ALT(DEF_ASM_OP2(movb, 0xa2, 0, OPC_BWL, OPT_EAX, OPT_ADDR))
ALT(DEF_ASM_OP2(movb, 0x88, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(movb, 0x8a, 0, OPC_MODRM | OPC_BWL, OPT_EA | OPT_REG, OPT_REG))
ALT(DEF_ASM_OP2(movb, 0xb0, 0, OPC_REG | OPC_BWL, OPT_IM, OPT_REG))
ALT(DEF_ASM_OP2(movb, 0xc6, 0, OPC_MODRM | OPC_BWL, OPT_IM, OPT_REG | OPT_EA))

ALT(DEF_ASM_OP2(movw, 0x8c, 0, OPC_MODRM | OPC_WL, OPT_SEG, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(movw, 0x8e, 0, OPC_MODRM | OPC_WL, OPT_EA | OPT_REG, OPT_SEG))

ALT(DEF_ASM_OP2(movw, 0x0f20, 0, OPC_MODRM | OPC_WL, OPT_CR, OPT_REG32))
ALT(DEF_ASM_OP2(movw, 0x0f21, 0, OPC_MODRM | OPC_WL, OPT_DB, OPT_REG32))
ALT(DEF_ASM_OP2(movw, 0x0f24, 0, OPC_MODRM | OPC_WL, OPT_TR, OPT_REG32))
ALT(DEF_ASM_OP2(movw, 0x0f22, 0, OPC_MODRM | OPC_WL, OPT_REG32, OPT_CR))
ALT(DEF_ASM_OP2(movw, 0x0f23, 0, OPC_MODRM | OPC_WL, OPT_REG32, OPT_DB))
ALT(DEF_ASM_OP2(movw, 0x0f26, 0, OPC_MODRM | OPC_WL, OPT_REG32, OPT_TR))

ALT(DEF_ASM_OP2(movsbl, 0x0fbe, 0, OPC_MODRM, OPT_REG8 | OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(movsbw, 0x0fbe, 0, OPC_MODRM | OPC_D16, OPT_REG8 | OPT_EA, OPT_REG16))
ALT(DEF_ASM_OP2(movswl, 0x0fbf, 0, OPC_MODRM, OPT_REG16 | OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(movzbw, 0x0fb6, 0, OPC_MODRM | OPC_WL, OPT_REG8 | OPT_EA, OPT_REGW))
ALT(DEF_ASM_OP2(movzwl, 0x0fb7, 0, OPC_MODRM, OPT_REG16 | OPT_EA, OPT_REG32))

ALT(DEF_ASM_OP1(pushw, 0x50, 0, OPC_REG | OPC_WL, OPT_REGW))
ALT(DEF_ASM_OP1(pushw, 0xff, 6, OPC_MODRM | OPC_WL, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP1(pushw, 0x6a, 0, OPC_WL, OPT_IM8S))
ALT(DEF_ASM_OP1(pushw, 0x68, 0, OPC_WL, OPT_IM32))
ALT(DEF_ASM_OP1(pushw, 0x06, 0, OPC_WL, OPT_SEG))

ALT(DEF_ASM_OP1(popw, 0x58, 0, OPC_REG | OPC_WL, OPT_REGW))
ALT(DEF_ASM_OP1(popw, 0x8f, 0, OPC_MODRM | OPC_WL, OPT_REGW | OPT_EA))
ALT(DEF_ASM_OP1(popw, 0x07, 0, OPC_WL, OPT_SEG))

ALT(DEF_ASM_OP2(xchgw, 0x90, 0, OPC_REG | OPC_WL, OPT_REG, OPT_EAX))
ALT(DEF_ASM_OP2(xchgw, 0x90, 0, OPC_REG | OPC_WL, OPT_EAX, OPT_REG))
ALT(DEF_ASM_OP2(xchgb, 0x86, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(xchgb, 0x86, 0, OPC_MODRM | OPC_BWL, OPT_EA | OPT_REG, OPT_REG))

ALT(DEF_ASM_OP2(inb, 0xe4, 0, OPC_BWL, OPT_IM8, OPT_EAX))
ALT(DEF_ASM_OP1(inb, 0xe4, 0, OPC_BWL, OPT_IM8))
ALT(DEF_ASM_OP2(inb, 0xec, 0, OPC_BWL, OPT_DX, OPT_EAX))
ALT(DEF_ASM_OP1(inb, 0xec, 0, OPC_BWL, OPT_DX))

ALT(DEF_ASM_OP2(outb, 0xe6, 0, OPC_BWL, OPT_EAX, OPT_IM8))
ALT(DEF_ASM_OP1(outb, 0xe6, 0, OPC_BWL, OPT_IM8))
ALT(DEF_ASM_OP2(outb, 0xee, 0, OPC_BWL, OPT_EAX, OPT_DX))
ALT(DEF_ASM_OP1(outb, 0xee, 0, OPC_BWL, OPT_DX))

ALT(DEF_ASM_OP2(leaw, 0x8d, 0, OPC_MODRM | OPC_WL, OPT_EA, OPT_REG))

ALT(DEF_ASM_OP2(les, 0xc4, 0, OPC_MODRM, OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(lds, 0xc5, 0, OPC_MODRM, OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(lss, 0x0fb2, 0, OPC_MODRM, OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(lfs, 0x0fb4, 0, OPC_MODRM, OPT_EA, OPT_REG32))
ALT(DEF_ASM_OP2(lgs, 0x0fb5, 0, OPC_MODRM, OPT_EA, OPT_REG32))

     
ALT(DEF_ASM_OP2(addb, 0x00, 0, OPC_ARITH | OPC_MODRM | OPC_BWL, OPT_REG, OPT_EA | OPT_REG)) 
ALT(DEF_ASM_OP2(addb, 0x02, 0, OPC_ARITH | OPC_MODRM | OPC_BWL, OPT_EA | OPT_REG, OPT_REG))
ALT(DEF_ASM_OP2(addb, 0x04, 0, OPC_ARITH | OPC_BWL, OPT_IM, OPT_EAX))
ALT(DEF_ASM_OP2(addb, 0x80, 0, OPC_ARITH | OPC_MODRM | OPC_BWL, OPT_IM, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(addw, 0x83, 0, OPC_ARITH | OPC_MODRM | OPC_WL, OPT_IM8S, OPT_EA | OPT_REG))

ALT(DEF_ASM_OP2(testb, 0x84, 0, OPC_MODRM | OPC_BWL, OPT_EA | OPT_REG, OPT_REG))
ALT(DEF_ASM_OP2(testb, 0x84, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(testb, 0xa8, 0, OPC_BWL, OPT_IM, OPT_EAX))
ALT(DEF_ASM_OP2(testb, 0xf6, 0, OPC_MODRM | OPC_BWL, OPT_IM, OPT_EA | OPT_REG))

ALT(DEF_ASM_OP1(incw, 0x40, 0, OPC_REG | OPC_WL, OPT_REGW))
ALT(DEF_ASM_OP1(incb, 0xfe, 0, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP1(decw, 0x48, 0, OPC_REG | OPC_WL, OPT_REGW))
ALT(DEF_ASM_OP1(decb, 0xfe, 1, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))

ALT(DEF_ASM_OP1(notb, 0xf6, 2, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP1(negb, 0xf6, 3, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))

ALT(DEF_ASM_OP1(mulb, 0xf6, 4, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP1(imulb, 0xf6, 5, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))

ALT(DEF_ASM_OP2(imulw, 0x0faf, 0, OPC_MODRM | OPC_WL, OPT_REG | OPT_EA, OPT_REG))
ALT(DEF_ASM_OP3(imulw, 0x6b, 0, OPC_MODRM | OPC_WL, OPT_IM8S, OPT_REGW | OPT_EA, OPT_REGW))
ALT(DEF_ASM_OP2(imulw, 0x6b, 0, OPC_MODRM | OPC_WL, OPT_IM8S, OPT_REGW))
ALT(DEF_ASM_OP3(imulw, 0x69, 0, OPC_MODRM | OPC_WL, OPT_IMW, OPT_REGW | OPT_EA, OPT_REGW))
ALT(DEF_ASM_OP2(imulw, 0x69, 0, OPC_MODRM | OPC_WL, OPT_IMW, OPT_REGW))

ALT(DEF_ASM_OP1(divb, 0xf6, 6, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP2(divb, 0xf6, 6, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA, OPT_EAX))
ALT(DEF_ASM_OP1(idivb, 0xf6, 7, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA))
ALT(DEF_ASM_OP2(idivb, 0xf6, 7, OPC_MODRM | OPC_BWL, OPT_REG | OPT_EA, OPT_EAX))

     
ALT(DEF_ASM_OP2(rolb, 0xc0, 0, OPC_MODRM | OPC_BWL | OPC_SHIFT, OPT_IM8, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP2(rolb, 0xd2, 0, OPC_MODRM | OPC_BWL | OPC_SHIFT, OPT_CL, OPT_EA | OPT_REG))
ALT(DEF_ASM_OP1(rolb, 0xd0, 0, OPC_MODRM | OPC_BWL | OPC_SHIFT, OPT_EA | OPT_REG))

ALT(DEF_ASM_OP3(shldw, 0x0fa4, 0, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP3(shldw, 0x0fa5, 0, OPC_MODRM | OPC_WL, OPT_CL, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP2(shldw, 0x0fa5, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP3(shrdw, 0x0fac, 0, OPC_MODRM | OPC_WL, OPT_IM8, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP3(shrdw, 0x0fad, 0, OPC_MODRM | OPC_WL, OPT_CL, OPT_REGW, OPT_EA | OPT_REGW))
ALT(DEF_ASM_OP2(shrdw, 0x0fad, 0, OPC_MODRM | OPC_WL, OPT_REGW, OPT_EA | OPT_REGW))

ALT(DEF_ASM_OP1(call, 0xff, 2, OPC_MODRM, OPT_INDIR))
ALT(DEF_ASM_OP1(call, 0xe8, 0, OPC_JMP, OPT_ADDR))
ALT(DEF_ASM_OP1(jmp, 0xff, 4, OPC_MODRM, OPT_INDIR))
ALT(DEF_ASM_OP1(jmp, 0xeb, 0, OPC_SHORTJMP | OPC_JMP, OPT_ADDR))

ALT(DEF_ASM_OP2(lcall, 0x9a, 0, 0, OPT_IM16, OPT_IM32))
ALT(DEF_ASM_OP1(lcall, 0xff, 3, 0, OPT_EA))
ALT(DEF_ASM_OP2(ljmp, 0xea, 0, 0, OPT_IM16, OPT_IM32))
ALT(DEF_ASM_OP1(ljmp, 0xff, 5, 0, OPT_EA))

ALT(DEF_ASM_OP1(int, 0xcd, 0, 0, OPT_IM8))
ALT(DEF_ASM_OP1(seto, 0x0f90, 0, OPC_MODRM | OPC_TEST, OPT_REG8 | OPT_EA))
    DEF_ASM_OP2(enter, 0xc8, 0, 0, OPT_IM16, OPT_IM8)
    DEF_ASM_OP0(leave, 0xc9)
    DEF_ASM_OP0(ret, 0xc3)
ALT(DEF_ASM_OP1(ret, 0xc2, 0, 0, OPT_IM16))
    DEF_ASM_OP0(lret, 0xcb)
ALT(DEF_ASM_OP1(lret, 0xca, 0, 0, OPT_IM16))

ALT(DEF_ASM_OP1(jo, 0x70, 0, OPC_SHORTJMP | OPC_JMP | OPC_TEST, OPT_ADDR))
    DEF_ASM_OP1(loopne, 0xe0, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(loopnz, 0xe0, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(loope, 0xe1, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(loopz, 0xe1, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(loop, 0xe2, 0, OPC_SHORTJMP, OPT_ADDR)
    DEF_ASM_OP1(jecxz, 0xe3, 0, OPC_SHORTJMP, OPT_ADDR)
     
     
     
ALT(DEF_ASM_OP0L(fcomp, 0xd8d9, 0, 0))

ALT(DEF_ASM_OP1(fadd, 0xd8c0, 0, OPC_FARITH | OPC_REG, OPT_ST))
ALT(DEF_ASM_OP2(fadd, 0xd8c0, 0, OPC_FARITH | OPC_REG, OPT_ST, OPT_ST0))
ALT(DEF_ASM_OP0L(fadd, 0xdec1, 0, OPC_FARITH))
ALT(DEF_ASM_OP1(faddp, 0xdec0, 0, OPC_FARITH | OPC_REG, OPT_ST))
ALT(DEF_ASM_OP2(faddp, 0xdec0, 0, OPC_FARITH | OPC_REG, OPT_ST, OPT_ST0))
ALT(DEF_ASM_OP2(faddp, 0xdec0, 0, OPC_FARITH | OPC_REG, OPT_ST0, OPT_ST))
ALT(DEF_ASM_OP0L(faddp, 0xdec1, 0, OPC_FARITH))
ALT(DEF_ASM_OP1(fadds, 0xd8, 0, OPC_FARITH | OPC_MODRM, OPT_EA))
ALT(DEF_ASM_OP1(fiaddl, 0xda, 0, OPC_FARITH | OPC_MODRM, OPT_EA))
ALT(DEF_ASM_OP1(faddl, 0xdc, 0, OPC_FARITH | OPC_MODRM, OPT_EA))
ALT(DEF_ASM_OP1(fiadds, 0xde, 0, OPC_FARITH | OPC_MODRM, OPT_EA))

     DEF_ASM_OP0(fucompp, 0xdae9)
     DEF_ASM_OP0(ftst, 0xd9e4)
     DEF_ASM_OP0(fxam, 0xd9e5)
     DEF_ASM_OP0(fld1, 0xd9e8)
     DEF_ASM_OP0(fldl2t, 0xd9e9)
     DEF_ASM_OP0(fldl2e, 0xd9ea)
     DEF_ASM_OP0(fldpi, 0xd9eb)
     DEF_ASM_OP0(fldlg2, 0xd9ec)
     DEF_ASM_OP0(fldln2, 0xd9ed)
     DEF_ASM_OP0(fldz, 0xd9ee)

     DEF_ASM_OP0(f2xm1, 0xd9f0)
     DEF_ASM_OP0(fyl2x, 0xd9f1)
     DEF_ASM_OP0(fptan, 0xd9f2)
     DEF_ASM_OP0(fpatan, 0xd9f3)
     DEF_ASM_OP0(fxtract, 0xd9f4)
     DEF_ASM_OP0(fprem1, 0xd9f5)
     DEF_ASM_OP0(fdecstp, 0xd9f6)
     DEF_ASM_OP0(fincstp, 0xd9f7)
     DEF_ASM_OP0(fprem, 0xd9f8)
     DEF_ASM_OP0(fyl2xp1, 0xd9f9)
     DEF_ASM_OP0(fsqrt, 0xd9fa)
     DEF_ASM_OP0(fsincos, 0xd9fb)
     DEF_ASM_OP0(frndint, 0xd9fc)
     DEF_ASM_OP0(fscale, 0xd9fd)
     DEF_ASM_OP0(fsin, 0xd9fe)
     DEF_ASM_OP0(fcos, 0xd9ff)
     DEF_ASM_OP0(fchs, 0xd9e0)
     DEF_ASM_OP0(fabs, 0xd9e1)
     DEF_ASM_OP0(fninit, 0xdbe3)
     DEF_ASM_OP0(fnclex, 0xdbe2)
     DEF_ASM_OP0(fnop, 0xd9d0)
     DEF_ASM_OP0(fwait, 0x9b)

    
    DEF_ASM_OP1(fld, 0xd9c0, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(fldl, 0xd9c0, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(flds, 0xd9, 0, OPC_MODRM, OPT_EA)
ALT(DEF_ASM_OP1(fldl, 0xdd, 0, OPC_MODRM, OPT_EA))
    DEF_ASM_OP1(fildl, 0xdb, 0, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fildq, 0xdf, 5, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fildll, 0xdf, 5, OPC_MODRM,OPT_EA)
    DEF_ASM_OP1(fldt, 0xdb, 5, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fbld, 0xdf, 4, OPC_MODRM, OPT_EA)
    
    
    DEF_ASM_OP1(fst, 0xddd0, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(fstl, 0xddd0, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(fsts, 0xd9, 2, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fstps, 0xd9, 3, OPC_MODRM, OPT_EA)
ALT(DEF_ASM_OP1(fstl, 0xdd, 2, OPC_MODRM, OPT_EA))
    DEF_ASM_OP1(fstpl, 0xdd, 3, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fist, 0xdf, 2, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fistp, 0xdf, 3, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fistl, 0xdb, 2, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fistpl, 0xdb, 3, OPC_MODRM, OPT_EA)

    DEF_ASM_OP1(fstp, 0xddd8, 0, OPC_REG, OPT_ST)
    DEF_ASM_OP1(fistpq, 0xdf, 7, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fistpll, 0xdf, 7, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fstpt, 0xdb, 7, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(fbstp, 0xdf, 6, OPC_MODRM, OPT_EA)

    
    DEF_ASM_OP0(fxch, 0xd9c9)
ALT(DEF_ASM_OP1(fxch, 0xd9c8, 0, OPC_REG, OPT_ST))

    
    DEF_ASM_OP1(fucom, 0xdde0, 0, OPC_REG, OPT_ST )
    DEF_ASM_OP1(fucomp, 0xdde8, 0, OPC_REG, OPT_ST )

    DEF_ASM_OP0L(finit, 0xdbe3, 0, OPC_FWAIT)
    DEF_ASM_OP1(fldcw, 0xd9, 5, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fnstcw, 0xd9, 7, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fstcw, 0xd9, 7, OPC_MODRM | OPC_FWAIT, OPT_EA )
    DEF_ASM_OP0(fnstsw, 0xdfe0)
ALT(DEF_ASM_OP1(fnstsw, 0xdfe0, 0, 0, OPT_EAX ))
ALT(DEF_ASM_OP1(fnstsw, 0xdd, 7, OPC_MODRM, OPT_EA ))
    DEF_ASM_OP1(fstsw, 0xdfe0, 0, OPC_FWAIT, OPT_EAX )
ALT(DEF_ASM_OP0L(fstsw, 0xdfe0, 0, OPC_FWAIT))
ALT(DEF_ASM_OP1(fstsw, 0xdd, 7, OPC_MODRM | OPC_FWAIT, OPT_EA ))
    DEF_ASM_OP0L(fclex, 0xdbe2, 0, OPC_FWAIT)
    DEF_ASM_OP1(fnstenv, 0xd9, 6, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fstenv, 0xd9, 6, OPC_MODRM | OPC_FWAIT, OPT_EA )
    DEF_ASM_OP1(fldenv, 0xd9, 4, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fnsave, 0xdd, 6, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fsave, 0xdd, 6, OPC_MODRM | OPC_FWAIT, OPT_EA )
    DEF_ASM_OP1(frstor, 0xdd, 4, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(ffree, 0xddc0, 4, OPC_REG, OPT_ST )
    DEF_ASM_OP1(ffreep, 0xdfc0, 4, OPC_REG, OPT_ST )
    DEF_ASM_OP1(fxsave, 0x0fae, 0, OPC_MODRM, OPT_EA )
    DEF_ASM_OP1(fxrstor, 0x0fae, 1, OPC_MODRM, OPT_EA )

    
    DEF_ASM_OP2(arpl, 0x63, 0, OPC_MODRM, OPT_REG16, OPT_REG16 | OPT_EA)
    DEF_ASM_OP2(lar, 0x0f02, 0, OPC_MODRM, OPT_REG32 | OPT_EA, OPT_REG32)
    DEF_ASM_OP1(lgdt, 0x0f01, 2, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(lidt, 0x0f01, 3, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(lldt, 0x0f00, 2, OPC_MODRM, OPT_EA | OPT_REG)
    DEF_ASM_OP1(lmsw, 0x0f01, 6, OPC_MODRM, OPT_EA | OPT_REG)
ALT(DEF_ASM_OP2(lslw, 0x0f03, 0, OPC_MODRM | OPC_WL, OPT_EA | OPT_REG, OPT_REG))
    DEF_ASM_OP1(ltr, 0x0f00, 3, OPC_MODRM, OPT_EA | OPT_REG)
    DEF_ASM_OP1(sgdt, 0x0f01, 0, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(sidt, 0x0f01, 1, OPC_MODRM, OPT_EA)
    DEF_ASM_OP1(sldt, 0x0f00, 0, OPC_MODRM, OPT_REG | OPT_EA)
    DEF_ASM_OP1(smsw, 0x0f01, 4, OPC_MODRM, OPT_REG | OPT_EA)
    DEF_ASM_OP1(str, 0x0f00, 1, OPC_MODRM, OPT_REG16| OPT_EA)
    DEF_ASM_OP1(verr, 0x0f00, 4, OPC_MODRM, OPT_REG | OPT_EA)
    DEF_ASM_OP1(verw, 0x0f00, 5, OPC_MODRM, OPT_REG | OPT_EA)

    
    DEF_ASM_OP1(bswap, 0x0fc8, 0, OPC_REG, OPT_REG32 )
ALT(DEF_ASM_OP2(xaddb, 0x0fc0, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_REG | OPT_EA ))
ALT(DEF_ASM_OP2(cmpxchgb, 0x0fb0, 0, OPC_MODRM | OPC_BWL, OPT_REG, OPT_REG | OPT_EA ))
    DEF_ASM_OP1(invlpg, 0x0f01, 7, OPC_MODRM, OPT_EA )

    DEF_ASM_OP2(boundl, 0x62, 0, OPC_MODRM, OPT_REG32, OPT_EA)
    DEF_ASM_OP2(boundw, 0x62, 0, OPC_MODRM | OPC_D16, OPT_REG16, OPT_EA)

    
    DEF_ASM_OP1(cmpxchg8b, 0x0fc7, 1, OPC_MODRM, OPT_EA )
    
    
    ALT(DEF_ASM_OP2(cmovo, 0x0f40, 0, OPC_MODRM | OPC_TEST, OPT_REG32 | OPT_EA, OPT_REG32))

    DEF_ASM_OP2(fcmovb, 0xdac0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmove, 0xdac8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovbe, 0xdad0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovu, 0xdad8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovnb, 0xdbc0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovne, 0xdbc8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovnbe, 0xdbd0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcmovnu, 0xdbd8, 0, OPC_REG, OPT_ST, OPT_ST0 )

    DEF_ASM_OP2(fucomi, 0xdbe8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcomi, 0xdbf0, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fucomip, 0xdfe8, 0, OPC_REG, OPT_ST, OPT_ST0 )
    DEF_ASM_OP2(fcomip, 0xdff0, 0, OPC_REG, OPT_ST, OPT_ST0 )

    
    DEF_ASM_OP0(emms, 0x0f77) 
    DEF_ASM_OP2(movd, 0x0f6e, 0, OPC_MODRM, OPT_EA | OPT_REG32, OPT_MMX )
ALT(DEF_ASM_OP2(movd, 0x0f7e, 0, OPC_MODRM, OPT_MMX, OPT_EA | OPT_REG32 ))
    DEF_ASM_OP2(movq, 0x0f6f, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(movq, 0x0f7f, 0, OPC_MODRM, OPT_MMX, OPT_EA | OPT_MMX ))
    DEF_ASM_OP2(packssdw, 0x0f6b, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(packsswb, 0x0f63, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(packuswb, 0x0f67, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddb, 0x0ffc, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddw, 0x0ffd, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddd, 0x0ffe, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddsb, 0x0fec, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddsw, 0x0fed, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddusb, 0x0fdc, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(paddusw, 0x0fdd, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pand, 0x0fdb, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pandn, 0x0fdf, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpeqb, 0x0f74, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpeqw, 0x0f75, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpeqd, 0x0f76, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpgtb, 0x0f64, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpgtw, 0x0f65, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pcmpgtd, 0x0f66, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pmaddwd, 0x0ff5, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pmulhw, 0x0fe5, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pmullw, 0x0fd5, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(por, 0x0feb, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psllw, 0x0ff1, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psllw, 0x0f71, 6, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(pslld, 0x0ff2, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(pslld, 0x0f72, 6, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psllq, 0x0ff3, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psllq, 0x0f73, 6, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psraw, 0x0fe1, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psraw, 0x0f71, 4, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psrad, 0x0fe2, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psrad, 0x0f72, 4, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psrlw, 0x0fd1, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psrlw, 0x0f71, 2, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psrld, 0x0fd2, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psrld, 0x0f72, 2, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psrlq, 0x0fd3, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
ALT(DEF_ASM_OP2(psrlq, 0x0f73, 2, OPC_MODRM, OPT_IM8, OPT_MMX ))
    DEF_ASM_OP2(psubb, 0x0ff8, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubw, 0x0ff9, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubd, 0x0ffa, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubsb, 0x0fe8, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubsw, 0x0fe9, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubusb, 0x0fd8, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(psubusw, 0x0fd9, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpckhbw, 0x0f68, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpckhwd, 0x0f69, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpckhdq, 0x0f6a, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpcklbw, 0x0f60, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpcklwd, 0x0f61, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(punpckldq, 0x0f62, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )
    DEF_ASM_OP2(pxor, 0x0fef, 0, OPC_MODRM, OPT_EA | OPT_MMX, OPT_MMX )

#undef ALT
#undef DEF_ASM_OP0
#undef DEF_ASM_OP0L
#undef DEF_ASM_OP1
#undef DEF_ASM_OP2
#undef DEF_ASM_OP3
};

static inline int get_reg_shift(TCCState *s1)
{
    int shift, v;

    v = asm_int_expr(s1);
    switch(v) {
    case 1:
        shift = 0;
        break;
    case 2:
        shift = 1;
        break;
    case 4:
        shift = 2;
        break;
    case 8:
        shift = 3;
        break;
    default:
        expect("1, 2, 4 or 8 constant");
        shift = 0;
        break;
    }
    return shift;
}

static int asm_parse_reg(void)
{
    int reg;
    if (tok != '%')
        goto error_32;
    next();
    if (tok >= TOK_ASM_eax && tok <= TOK_ASM_edi) {
        reg = tok - TOK_ASM_eax;
        next();
        return reg;
    } else {
    error_32:
        expect("32 bit register");
        return 0;
    }
}

static void parse_operand(TCCState *s1, Operand *op)
{
    ExprValue e;
    int reg, indir;
    const char *p;

    indir = 0;
    if (tok == '*') {
        next();
        indir = OP_INDIR;
    }

    if (tok == '%') {
        next();
        if (tok >= TOK_ASM_al && tok <= TOK_ASM_db7) {
            reg = tok - TOK_ASM_al;
            op->type = 1 << (reg >> 3); 
            op->reg = reg & 7;
            if ((op->type & OP_REG) && op->reg == TREG_EAX)
                op->type |= OP_EAX;
            else if (op->type == OP_REG8 && op->reg == TREG_ECX)
                op->type |= OP_CL;
            else if (op->type == OP_REG16 && op->reg == TREG_EDX)
                op->type |= OP_DX;
        } else if (tok >= TOK_ASM_dr0 && tok <= TOK_ASM_dr7) {
            op->type = OP_DB;
            op->reg = tok - TOK_ASM_dr0;
        } else if (tok >= TOK_ASM_es && tok <= TOK_ASM_gs) {
            op->type = OP_SEG;
            op->reg = tok - TOK_ASM_es;
        } else if (tok == TOK_ASM_st) {
            op->type = OP_ST;
            op->reg = 0;
            next();
            if (tok == '(') {
                next();
                if (tok != TOK_PPNUM)
                    goto reg_error;
                p = tokc.cstr->data;
                reg = p[0] - '0';
                if ((unsigned)reg >= 8 || p[1] != '\0')
                    goto reg_error;
                op->reg = reg;
                next();
                skip(')');
            }
            if (op->reg == 0)
                op->type |= OP_ST0;
            goto no_skip;
        } else {
        reg_error:
            error("unknown register");
        }
        next();
    no_skip: ;
    } else if (tok == '$') {
        
        next();
        asm_expr(s1, &e);
        op->type = OP_IM32;
        op->e.v = e.v;
        op->e.sym = e.sym;
        if (!op->e.sym) {
            if (op->e.v == (uint8_t)op->e.v)
                op->type |= OP_IM8;
            if (op->e.v == (int8_t)op->e.v)
                op->type |= OP_IM8S;
            if (op->e.v == (uint16_t)op->e.v)
                op->type |= OP_IM16;
        }
    } else {
        
        op->type = OP_EA;
        op->reg = -1;
        op->reg2 = -1;
        op->shift = 0;
        if (tok != '(') {
            asm_expr(s1, &e);
            op->e.v = e.v;
            op->e.sym = e.sym;
        } else {
            op->e.v = 0;
            op->e.sym = NULL;
        }
        if (tok == '(') {
            next();
            if (tok != ',') {
                op->reg = asm_parse_reg();
            }
            if (tok == ',') {
                next();
                if (tok != ',') {
                    op->reg2 = asm_parse_reg();
                } 
                skip(',');
                op->shift = get_reg_shift(s1);
            }
            skip(')');
        }
        if (op->reg == -1 && op->reg2 == -1)
            op->type |= OP_ADDR;
    }
    op->type |= indir;
}

static void gen_expr32(ExprValue *pe)
{
    if (pe->sym)
        greloc(cur_text_section, pe->sym, ind, R_386_32);
    gen_le32(pe->v);
}

static void gen_disp32(ExprValue *pe)
{
    Sym *sym;
    sym = pe->sym;
    if (sym) {
        if (sym->r == cur_text_section->sh_num) {
            gen_le32(pe->v + (long)sym->next - ind - 4);
        } else {
            greloc(cur_text_section, sym, ind, R_386_PC32);
            gen_le32(pe->v - 4);
        }
    } else {
        
        put_elf_reloc(symtab_section, cur_text_section, 
                      ind, R_386_PC32, 0);
        gen_le32(pe->v - 4);
    }
}


static void gen_le16(int v)
{
    g(v);
    g(v >> 8);
}

static inline void asm_modrm(int reg, Operand *op)
{
    int mod, reg1, reg2, sib_reg1;

    if (op->type & (OP_REG | OP_MMX | OP_SSE)) {
        g(0xc0 + (reg << 3) + op->reg);
    } else if (op->reg == -1 && op->reg2 == -1) {
        
        g(0x05 + (reg << 3));
        gen_expr32(&op->e);
    } else {
        sib_reg1 = op->reg;
        
        if (sib_reg1 == -1) {
            sib_reg1 = 5;
            mod = 0x00;
        } else if (op->e.v == 0 && !op->e.sym && op->reg != 5) {
            mod = 0x00;
        } else if (op->e.v == (int8_t)op->e.v && !op->e.sym) {
            mod = 0x40;
        } else {
            mod = 0x80;
        }
        
        reg1 = op->reg;
        if (op->reg2 != -1)
            reg1 = 4;
        g(mod + (reg << 3) + reg1);
        if (reg1 == 4) {
            
            reg2 = op->reg2;
            if (reg2 == -1)
                reg2 = 4; 
            g((op->shift << 6) + (reg2 << 3) + sib_reg1);
        }

        
        if (mod == 0x40) {
            g(op->e.v);
        } else if (mod == 0x80 || op->reg == -1) {
            gen_expr32(&op->e);
        }
    }
}

static void asm_opcode(TCCState *s1, int opcode)
{
    const ASMInstr *pa;
    int i, modrm_index, reg, v, op1, is_short_jmp;
    int nb_ops, s, ss;
    Operand ops[MAX_OPERANDS], *pop;
    int op_type[3]; 

    
    pop = ops;
    nb_ops = 0;
    for(;;) {
        if (tok == ';' || tok == TOK_LINEFEED)
            break;
        if (nb_ops >= MAX_OPERANDS) {
            error("incorrect number of operands");
        }
        parse_operand(s1, pop);
        pop++;
        nb_ops++;
        if (tok != ',')
            break;
        next();
    }

    is_short_jmp = 0;
    s = 0; 
    
    for(pa = asm_instrs; pa->sym != 0; pa++) {
        s = 0;
        if (pa->instr_type & OPC_FARITH) {
            v = opcode - pa->sym;
            if (!((unsigned)v < 8 * 6 && (v % 6) == 0))
                continue;
        } else if (pa->instr_type & OPC_ARITH) {
            if (!(opcode >= pa->sym && opcode < pa->sym + 8 * 4))
                continue;
            goto compute_size;
        } else if (pa->instr_type & OPC_SHIFT) {
            if (!(opcode >= pa->sym && opcode < pa->sym + 7 * 4))
                continue;
            goto compute_size;
        } else if (pa->instr_type & OPC_TEST) {
            if (!(opcode >= pa->sym && opcode < pa->sym + NB_TEST_OPCODES))
                continue;
        } else if (pa->instr_type & OPC_B) {
            if (!(opcode >= pa->sym && opcode <= pa->sym + 3))
                continue;
        compute_size:
            s = (opcode - pa->sym) & 3;
        } else if (pa->instr_type & OPC_WL) {
            if (!(opcode >= pa->sym && opcode <= pa->sym + 2))
                continue;
            s = opcode - pa->sym + 1;
        } else {
            if (pa->sym != opcode)
                continue;
        }
        if (pa->nb_ops != nb_ops)
            continue;
        
        for(i = 0; i < nb_ops; i++) {
            int op1, op2;
            op1 = pa->op_type[i];
            op2 = op1 & 0x1f;
            switch(op2) {
            case OPT_IM:
                v = OP_IM8 | OP_IM16 | OP_IM32;
                break;
            case OPT_REG:
                v = OP_REG8 | OP_REG16 | OP_REG32;
                break;
            case OPT_REGW:
                v = OP_REG16 | OP_REG32;
                break;
            case OPT_IMW:
                v = OP_IM16 | OP_IM32;
                break;
            default:
                v = 1 << op2;
                break;
            }
            if (op1 & OPT_EA)
                v |= OP_EA;
            op_type[i] = v;
            if ((ops[i].type & v) == 0)
                goto next;
        }
        
        break;
    next: ;
    }
    if (pa->sym == 0) {
        if (opcode >= TOK_ASM_pusha && opcode <= TOK_ASM_emms) {
            int b;
            b = op0_codes[opcode - TOK_ASM_pusha];
            if (b & 0xff00) 
                g(b >> 8);
            g(b);
            return;
        } else {
            error("unknown opcode '%s'", 
                  get_tok_str(opcode, NULL));
        }
    }
    
    if (s == 3) {
        for(i = 0; s == 3 && i < nb_ops; i++) {
            if ((ops[i].type & OP_REG) && !(op_type[i] & (OP_CL | OP_DX)))
                s = reg_to_size[ops[i].type & OP_REG];
        }
        if (s == 3) {
            if ((opcode == TOK_ASM_push || opcode == TOK_ASM_pop) && 
                (ops[0].type & (OP_SEG | OP_IM8S | OP_IM32)))
                s = 2;
            else
                error("cannot infer opcode suffix");
        }
    }

    
    ss = s;
    if (s == 1 || (pa->instr_type & OPC_D16))
        g(WORD_PREFIX_OPCODE);
    else if (s == 2)
        s = 1;
    
    if (pa->instr_type & OPC_FWAIT)
        g(0x9b);

    v = pa->opcode;
    if (v == 0x69 || v == 0x69) {
        
        nb_ops = 3;
        ops[2] = ops[1];
    } else if (v == 0xcd && ops[0].e.v == 3 && !ops[0].e.sym) {
        v--; 
        nb_ops = 0;
    } else if ((v == 0x06 || v == 0x07)) {
        if (ops[0].reg >= 4) {
            
            v = 0x0fa0 + (v - 0x06) + ((ops[0].reg - 4) << 3);
        } else {
            v += ops[0].reg << 3;
        }
        nb_ops = 0;
    } else if (v <= 0x05) {
        
        v += ((opcode - TOK_ASM_addb) >> 2) << 3;
    } else if ((pa->instr_type & (OPC_FARITH | OPC_MODRM)) == OPC_FARITH) {
        
        v += ((opcode - pa->sym) / 6) << 3;
    }
    if (pa->instr_type & OPC_REG) {
        for(i = 0; i < nb_ops; i++) {
            if (op_type[i] & (OP_REG | OP_ST)) {
                v += ops[i].reg;
                break;
            }
        }
        
        if (pa->opcode == 0xb0 && s >= 1)
            v += 7;
    }
    if (pa->instr_type & OPC_B)
        v += s;
    if (pa->instr_type & OPC_TEST)
        v += test_bits[opcode - pa->sym]; 
    if (pa->instr_type & OPC_SHORTJMP) {
        Sym *sym;
        int jmp_disp;

        
        sym = ops[0].e.sym;
        if (!sym)
            goto no_short_jump;
        if (sym->r != cur_text_section->sh_num)
            goto no_short_jump;
        jmp_disp = ops[0].e.v + (long)sym->next - ind - 2;
        if (jmp_disp == (int8_t)jmp_disp) {
            
            is_short_jmp = 1;
            ops[0].e.v = jmp_disp;
        } else {
        no_short_jump:
            if (pa->instr_type & OPC_JMP) {
                if (v == 0xeb)
                    v = 0xe9;
                else 
                    v += 0x0f10;
            } else {
                error("invalid displacement");
            }
        }
    }
    op1 = v >> 8;
    if (op1)
        g(op1);
    g(v);
        
    
    modrm_index = 0;
    if (pa->instr_type & OPC_SHIFT) {
        reg = (opcode - pa->sym) >> 2; 
        if (reg == 6)
            reg = 7;
    } else if (pa->instr_type & OPC_ARITH) {
        reg = (opcode - pa->sym) >> 2;
    } else if (pa->instr_type & OPC_FARITH) {
        reg = (opcode - pa->sym) / 6;
    } else {
        reg = (pa->instr_type >> OPC_GROUP_SHIFT) & 7;
    }
    if (pa->instr_type & OPC_MODRM) {
        
        for(i = 0;i < nb_ops; i++) {
            if (op_type[i] & OP_EA)
                goto modrm_found;
        }
        
        for(i = 0;i < nb_ops; i++) {
            if (op_type[i] & (OP_REG | OP_MMX | OP_SSE | OP_INDIR))
                goto modrm_found;
        }
#ifdef ASM_DEBUG
        error("bad op table");
#endif      
    modrm_found:
        modrm_index = i;
        for(i = 0;i < nb_ops; i++) {
            v = op_type[i];
            if (i != modrm_index && 
                (v & (OP_REG | OP_MMX | OP_SSE | OP_CR | OP_TR | OP_DB | OP_SEG))) {
                reg = ops[i].reg;
                break;
            }
        }

        asm_modrm(reg, &ops[modrm_index]);
    }

    
    if (pa->opcode == 0x9a || pa->opcode == 0xea) {
        
        gen_expr32(&ops[1].e);
        if (ops[0].e.sym)
            error("cannot relocate");
        gen_le16(ops[0].e.v);
    } else {
        for(i = 0;i < nb_ops; i++) {
            v = op_type[i];
            if (v & (OP_IM8 | OP_IM16 | OP_IM32 | OP_IM8S | OP_ADDR)) {
                if (v == (OP_IM8 | OP_IM16 | OP_IM32) ||
                    v == (OP_IM16 | OP_IM32)) {
                    if (ss == 0)
                        v = OP_IM8;
                    else if (ss == 1)
                        v = OP_IM16;
                    else
                        v = OP_IM32;
                }
                if (v & (OP_IM8 | OP_IM8S)) {
                    if (ops[i].e.sym)
                        goto error_relocate;
                    g(ops[i].e.v);
                } else if (v & OP_IM16) {
                    if (ops[i].e.sym) {
                    error_relocate:
                        error("cannot relocate");
                    }
                    gen_le16(ops[i].e.v);
                } else {
                    if (pa->instr_type & (OPC_JMP | OPC_SHORTJMP)) {
                        if (is_short_jmp)
                            g(ops[i].e.v);
                        else
                            gen_disp32(&ops[i].e);
                    } else {
                        gen_expr32(&ops[i].e);
                    }
                }
            }
        }
    }
}

#define NB_SAVED_REGS 3
#define NB_ASM_REGS 8

static inline int constraint_priority(const char *str)
{
    int priority, c, pr;

    
    priority = 0;
    for(;;) {
        c = *str;
        if (c == '\0')
            break;
        str++;
        switch(c) {
        case 'A':
            pr = 0;
            break;
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'S':
        case 'D':
            pr = 1;
            break;
        case 'q':
            pr = 2;
            break;
        case 'r':
            pr = 3;
            break;
        case 'N':
        case 'M':
        case 'I':
        case 'i':
        case 'm':
        case 'g':
            pr = 4;
            break;
        default:
            error("unknown constraint '%c'", c);
            pr = 0;
        }
        if (pr > priority)
            priority = pr;
    }
    return priority;
}

static const char *skip_constraint_modifiers(const char *p)
{
    while (*p == '=' || *p == '&' || *p == '+' || *p == '%')
        p++;
    return p;
}

#define REG_OUT_MASK 0x01
#define REG_IN_MASK  0x02

#define is_reg_allocated(reg) (regs_allocated[reg] & reg_mask)

static void asm_compute_constraints(ASMOperand *operands, 
                                    int nb_operands, int nb_outputs, 
                                    const uint8_t *clobber_regs,
                                    int *pout_reg)
{
    ASMOperand *op;
    int sorted_op[MAX_ASM_OPERANDS];
    int i, j, k, p1, p2, tmp, reg, c, reg_mask;
    const char *str;
    uint8_t regs_allocated[NB_ASM_REGS];
    
    
    for(i=0;i<nb_operands;i++) {
        op = &operands[i];
        op->input_index = -1;
        op->ref_index = -1;
        op->reg = -1;
        op->is_memory = 0;
        op->is_rw = 0;
    }
    for(i=0;i<nb_operands;i++) {
        op = &operands[i];
        str = op->constraint;
        str = skip_constraint_modifiers(str);
        if (isnum(*str) || *str == '[') {
            
            k = find_constraint(operands, nb_operands, str, NULL);
            if ((unsigned)k >= i || i < nb_outputs)
                error("invalid reference in constraint %d ('%s')",
                      i, str);
            op->ref_index = k;
            if (operands[k].input_index >= 0)
                error("cannot reference twice the same operand");
            operands[k].input_index = i;
            op->priority = 5;
        } else {
            op->priority = constraint_priority(str);
        }
    }
    
    
    for(i=0;i<nb_operands;i++)
        sorted_op[i] = i;
    for(i=0;i<nb_operands - 1;i++) {
        for(j=i+1;j<nb_operands;j++) {
            p1 = operands[sorted_op[i]].priority; 
            p2 = operands[sorted_op[j]].priority;
            if (p2 < p1) {
                tmp = sorted_op[i];
                sorted_op[i] = sorted_op[j];
                sorted_op[j] = tmp;
            }
        }
    }

    for(i = 0;i < NB_ASM_REGS; i++) {
        if (clobber_regs[i])
            regs_allocated[i] = REG_IN_MASK | REG_OUT_MASK;
        else
            regs_allocated[i] = 0;
    }
    
    regs_allocated[4] = REG_IN_MASK | REG_OUT_MASK; 
    
    regs_allocated[5] = REG_IN_MASK | REG_OUT_MASK; 

    
    for(i=0;i<nb_operands;i++) {
        j = sorted_op[i];
        op = &operands[j];
        str = op->constraint;
        
        if (op->ref_index >= 0)
            continue;
        
        if (op->input_index >= 0) {
            reg_mask = REG_IN_MASK | REG_OUT_MASK;
        } else if (j < nb_outputs) {
            reg_mask = REG_OUT_MASK;
        } else {
            reg_mask = REG_IN_MASK;
        }
    try_next:
        c = *str++;
        switch(c) {
        case '=':
            goto try_next;
        case '+':
            op->is_rw = 1;
            
        case '&':
            if (j >= nb_outputs)
                error("'%c' modifier can only be applied to outputs", c);
            reg_mask = REG_IN_MASK | REG_OUT_MASK;
            goto try_next;
        case 'A':
            
            if (is_reg_allocated(TREG_EAX) || 
                is_reg_allocated(TREG_EDX))
                goto try_next;
            op->is_llong = 1;
            op->reg = TREG_EAX;
            regs_allocated[TREG_EAX] |= reg_mask;
            regs_allocated[TREG_EDX] |= reg_mask;
            break;
        case 'a':
            reg = TREG_EAX;
            goto alloc_reg;
        case 'b':
            reg = 3;
            goto alloc_reg;
        case 'c':
            reg = TREG_ECX;
            goto alloc_reg;
        case 'd':
            reg = TREG_EDX;
            goto alloc_reg;
        case 'S':
            reg = 6;
            goto alloc_reg;
        case 'D':
            reg = 7;
        alloc_reg:
            if (is_reg_allocated(reg))
                goto try_next;
            goto reg_found;
        case 'q':
            
            for(reg = 0; reg < 4; reg++) {
                if (!is_reg_allocated(reg))
                    goto reg_found;
            }
            goto try_next;
        case 'r':
            
            for(reg = 0; reg < 8; reg++) {
                if (!is_reg_allocated(reg))
                    goto reg_found;
            }
            goto try_next;
        reg_found:
            
            op->is_llong = 0;
            op->reg = reg;
            regs_allocated[reg] |= reg_mask;
            break;
        case 'i':
            if (!((op->vt->r & (VT_VALMASK | VT_LVAL)) == VT_CONST))
                goto try_next;
            break;
        case 'I':
        case 'N':
        case 'M':
            if (!((op->vt->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST))
                goto try_next;
            break;
        case 'm':
        case 'g':
            
            if (j < nb_outputs || c == 'm') {
                if ((op->vt->r & VT_VALMASK) == VT_LLOCAL) {
                    
                    for(reg = 0; reg < 8; reg++) {
                        if (!(regs_allocated[reg] & REG_IN_MASK))
                            goto reg_found1;
                    }
                    goto try_next;
                reg_found1:
                    
                    regs_allocated[reg] |= REG_IN_MASK;
                    op->reg = reg;
                    op->is_memory = 1;
                }
            }
            break;
        default:
            error("asm constraint %d ('%s') could not be satisfied", 
                  j, op->constraint);
            break;
        }
        
        if (op->input_index >= 0) {
            operands[op->input_index].reg = op->reg;
            operands[op->input_index].is_llong = op->is_llong;
        }
    }
    
    *pout_reg = -1;
    for(i=0;i<nb_operands;i++) {
        op = &operands[i];
        if (op->reg >= 0 && 
            (op->vt->r & VT_VALMASK) == VT_LLOCAL  &&
            !op->is_memory) {
            for(reg = 0; reg < 8; reg++) {
                if (!(regs_allocated[reg] & REG_OUT_MASK))
                    goto reg_found2;
            }
            error("could not find free output register for reloading");
        reg_found2:
            *pout_reg = reg;
            break;
        }
    }
    
    
#ifdef ASM_DEBUG
    for(i=0;i<nb_operands;i++) {
        j = sorted_op[i];
        op = &operands[j];
        printf("%%%d [%s]: \"%s\" r=0x%04x reg=%d\n", 
               j,                
               op->id ? get_tok_str(op->id, NULL) : "", 
               op->constraint,
               op->vt->r,
               op->reg);
    }
    if (*pout_reg >= 0)
        printf("out_reg=%d\n", *pout_reg);
#endif
}

static void subst_asm_operand(CString *add_str, 
                              SValue *sv, int modifier)
{
    int r, reg, size, val;
    char buf[64];

    r = sv->r;
    if ((r & VT_VALMASK) == VT_CONST) {
        if (!(r & VT_LVAL) && modifier != 'c' && modifier != 'n')
            cstr_ccat(add_str, '$');
        if (r & VT_SYM) {
            cstr_cat(add_str, get_tok_str(sv->sym->v, NULL));
            if (sv->c.i != 0) {
                cstr_ccat(add_str, '+');
            } else {
                return;
            }
        }
        val = sv->c.i;
        if (modifier == 'n')
            val = -val;
        snprintf(buf, sizeof(buf), "%d", sv->c.i);
        cstr_cat(add_str, buf);
    } else if ((r & VT_VALMASK) == VT_LOCAL) {
        snprintf(buf, sizeof(buf), "%d(%%ebp)", sv->c.i);
        cstr_cat(add_str, buf);
    } else if (r & VT_LVAL) {
        reg = r & VT_VALMASK;
        if (reg >= VT_CONST)
            error("internal compiler error");
        snprintf(buf, sizeof(buf), "(%%%s)", 
                 get_tok_str(TOK_ASM_eax + reg, NULL));
        cstr_cat(add_str, buf);
    } else {
        
        reg = r & VT_VALMASK;
        if (reg >= VT_CONST)
            error("internal compiler error");

        
        if ((sv->type.t & VT_BTYPE) == VT_BYTE)
            size = 1;
        else if ((sv->type.t & VT_BTYPE) == VT_SHORT)
            size = 2;
        else
            size = 4;
        if (size == 1 && reg >= 4)
            size = 4;

        if (modifier == 'b') {
            if (reg >= 4)
                error("cannot use byte register");
            size = 1;
        } else if (modifier == 'h') {
            if (reg >= 4)
                error("cannot use byte register");
            size = -1;
        } else if (modifier == 'w') {
            size = 2;
        }

        switch(size) {
        case -1:
            reg = TOK_ASM_ah + reg;
            break;
        case 1:
            reg = TOK_ASM_al + reg;
            break;
        case 2:
            reg = TOK_ASM_ax + reg;
            break;
        default:
            reg = TOK_ASM_eax + reg;
            break;
        }
        snprintf(buf, sizeof(buf), "%%%s", get_tok_str(reg, NULL));
        cstr_cat(add_str, buf);
    }
}

static void asm_gen_code(ASMOperand *operands, int nb_operands, 
                         int nb_outputs, int is_output,
                         uint8_t *clobber_regs,
                         int out_reg)
{
    uint8_t regs_allocated[NB_ASM_REGS];
    ASMOperand *op;
    int i, reg;
    static uint8_t reg_saved[NB_SAVED_REGS] = { 3, 6, 7 };

    
    memcpy(regs_allocated, clobber_regs, sizeof(regs_allocated));
    for(i = 0; i < nb_operands;i++) {
        op = &operands[i];
        if (op->reg >= 0)
            regs_allocated[op->reg] = 1;
    }
    if (!is_output) {
        
        for(i = 0; i < NB_SAVED_REGS; i++) {
            reg = reg_saved[i];
            if (regs_allocated[reg]) 
                g(0x50 + reg);
        }

        
        for(i = 0; i < nb_operands; i++) {
            op = &operands[i];
            if (op->reg >= 0) {
                if ((op->vt->r & VT_VALMASK) == VT_LLOCAL &&
                    op->is_memory) {
                    SValue sv;
                    sv = *op->vt;
                    sv.r = (sv.r & ~VT_VALMASK) | VT_LOCAL;
                    load(op->reg, &sv);
                } else if (i >= nb_outputs || op->is_rw) {
                    
                    load(op->reg, op->vt);
                    if (op->is_llong) {
                        SValue sv;
                        sv = *op->vt;
                        sv.c.ul += 4;
                        load(TREG_EDX, &sv);
                    }
                }
            }
        }
    } else {
        
        for(i = 0 ; i < nb_outputs; i++) {
            op = &operands[i];
            if (op->reg >= 0) {
                if ((op->vt->r & VT_VALMASK) == VT_LLOCAL) {
                    if (!op->is_memory) {
                        SValue sv;
                        sv = *op->vt;
                        sv.r = (sv.r & ~VT_VALMASK) | VT_LOCAL;
                        load(out_reg, &sv);

                        sv.r = (sv.r & ~VT_VALMASK) | out_reg;
                        store(op->reg, &sv);
                    }
                } else {
                    store(op->reg, op->vt);
                    if (op->is_llong) {
                        SValue sv;
                        sv = *op->vt;
                        sv.c.ul += 4;
                        store(TREG_EDX, &sv);
                    }
                }
            }
        }
        
        for(i = NB_SAVED_REGS - 1; i >= 0; i--) {
            reg = reg_saved[i];
            if (regs_allocated[reg]) 
                g(0x58 + reg);
        }
    }
}

static void asm_clobber(uint8_t *clobber_regs, const char *str)
{
    int reg;
    TokenSym *ts;

    if (!strcmp(str, "memory") || 
        !strcmp(str, "cc"))
        return;
    ts = tok_alloc(str, strlen(str));
    reg = ts->tok;
    if (reg >= TOK_ASM_eax && reg <= TOK_ASM_edi) {
        reg -= TOK_ASM_eax;
    } else if (reg >= TOK_ASM_ax && reg <= TOK_ASM_di) {
        reg -= TOK_ASM_ax;
    } else {
        error("invalid clobber register '%s'", str);
    }
    clobber_regs[reg] = 1;
}
#endif
/*
 *  GAS like assembler for TCC
 * 
 *  Copyright (c) 2001-2004 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

static int asm_get_local_label_name(TCCState *s1, unsigned int n)
{
    char buf[64];
    TokenSym *ts;

    snprintf(buf, sizeof(buf), "L..%u", n);
    ts = tok_alloc(buf, strlen(buf));
    return ts->tok;
}

static void asm_expr(TCCState *s1, ExprValue *pe);


static void asm_expr_unary(TCCState *s1, ExprValue *pe)
{
    Sym *sym;
    int op, n, label;
    const char *p;

    switch(tok) {
    case TOK_PPNUM:
        p = tokc.cstr->data;
        n = strtoul(p, (char **)&p, 0);
        if (*p == 'b' || *p == 'f') {
            
            label = asm_get_local_label_name(s1, n);
            sym = label_find(label);
            if (*p == 'b') {
                
                if (sym && sym->r == 0)
                    sym = sym->prev_tok;
                if (!sym)
                    error("local label '%d' not found backward", n);
            } else {
                
                if (!sym || sym->r) {
                    
                    sym = label_push(&s1->asm_labels, label, 0);
                    sym->type.t = VT_STATIC | VT_VOID;
                }
            }
            pe->v = 0;
            pe->sym = sym;
        } else if (*p == '\0') {
            pe->v = n;
            pe->sym = NULL;
        } else {
            error("invalid number syntax");
        }
        next();
        break;
    case '+':
        next();
        asm_expr_unary(s1, pe);
        break;
    case '-':
    case '~':
        op = tok;
        next();
        asm_expr_unary(s1, pe);
        if (pe->sym)
            error("invalid operation with label");
        if (op == '-')
            pe->v = -pe->v;
        else
            pe->v = ~pe->v;
        break;
    case TOK_CCHAR:
    case TOK_LCHAR:
	pe->v = tokc.i;
	pe->sym = NULL;
	next();
	break;
    case '(':
        next();
        asm_expr(s1, pe);
        skip(')');
        break;
    default:
        if (tok >= TOK_IDENT) {
            
            sym = label_find(tok);
            if (!sym) {
                sym = label_push(&s1->asm_labels, tok, 0);
                
                sym->type.t = VT_VOID;
            }
            if (sym->r == SHN_ABS) {
                
                pe->v = (long)sym->next;
                pe->sym = NULL;
            } else {
                pe->v = 0;
                pe->sym = sym;
            }
            next();
        } else {
            error("bad expression syntax [%s]", get_tok_str(tok, &tokc));
        }
        break;
    }
}
    
static void asm_expr_prod(TCCState *s1, ExprValue *pe)
{
    int op;
    ExprValue e2;

    asm_expr_unary(s1, pe);
    for(;;) {
        op = tok;
        if (op != '*' && op != '/' && op != '%' && 
            op != TOK_SHL && op != TOK_SAR)
            break;
        next();
        asm_expr_unary(s1, &e2);
        if (pe->sym || e2.sym)
            error("invalid operation with label");
        switch(op) {
        case '*':
            pe->v *= e2.v;
            break;
        case '/':  
            if (e2.v == 0) {
            div_error:
                error("division by zero");
            }
            pe->v /= e2.v;
            break;
        case '%':  
            if (e2.v == 0)
                goto div_error;
            pe->v %= e2.v;
            break;
        case TOK_SHL:
            pe->v <<= e2.v;
            break;
        default:
        case TOK_SAR:
            pe->v >>= e2.v;
            break;
        }
    }
}

static void asm_expr_logic(TCCState *s1, ExprValue *pe)
{
    int op;
    ExprValue e2;

    asm_expr_prod(s1, pe);
    for(;;) {
        op = tok;
        if (op != '&' && op != '|' && op != '^')
            break;
        next();
        asm_expr_prod(s1, &e2);
        if (pe->sym || e2.sym)
            error("invalid operation with label");
        switch(op) {
        case '&':
            pe->v &= e2.v;
            break;
        case '|':  
            pe->v |= e2.v;
            break;
        default:
        case '^':
            pe->v ^= e2.v;
            break;
        }
    }
}

static inline void asm_expr_sum(TCCState *s1, ExprValue *pe)
{
    int op;
    ExprValue e2;

    asm_expr_logic(s1, pe);
    for(;;) {
        op = tok;
        if (op != '+' && op != '-')
            break;
        next();
        asm_expr_logic(s1, &e2);
        if (op == '+') {
            if (pe->sym != NULL && e2.sym != NULL)
                goto cannot_relocate;
            pe->v += e2.v;
            if (pe->sym == NULL && e2.sym != NULL)
                pe->sym = e2.sym;
        } else {
            pe->v -= e2.v;
            if (!pe->sym && !e2.sym) {
                
            } else if (pe->sym && !e2.sym) {
                
            } else if (pe->sym && e2.sym) {
                if (pe->sym == e2.sym) { 
                    
                } else if (pe->sym->r == e2.sym->r && pe->sym->r != 0) {
                    
                    pe->v += (long)pe->sym->next - (long)e2.sym->next;
                } else {
                    goto cannot_relocate;
                }
                pe->sym = NULL; 
            } else {
            cannot_relocate:
                error("invalid operation with label");
            }
        }
    }
}

static void asm_expr(TCCState *s1, ExprValue *pe)
{
    asm_expr_sum(s1, pe);
}

static int asm_int_expr(TCCState *s1)
{
    ExprValue e;
    asm_expr(s1, &e);
    if (e.sym)
        expect("constant");
    return e.v;
}

static void asm_new_label1(TCCState *s1, int label, int is_local,
                           int sh_num, long value)
{
    Sym *sym;

    sym = label_find(label);
    if (sym) {
        if (sym->r) {
            
            if (!is_local) {
                error("assembler label '%s' already defined", 
                      get_tok_str(label, NULL));
            } else {
                
                goto new_label;
            }
        }
    } else {
    new_label:
        sym = label_push(&s1->asm_labels, label, 0);
        sym->type.t = VT_STATIC | VT_VOID;
    }
    sym->r = sh_num;
    sym->next = (void *)value;
}

static void asm_new_label(TCCState *s1, int label, int is_local)
{
    asm_new_label1(s1, label, is_local, cur_text_section->sh_num, ind);
}

static void asm_free_labels(TCCState *st)
{
    Sym *s, *s1;
    Section *sec;
    
    for(s = st->asm_labels; s != NULL; s = s1) {
        s1 = s->prev;
        
        if (s->r) {
            if (s->r == SHN_ABS)
                sec = SECTION_ABS;
            else
                sec = st->sections[s->r];
            put_extern_sym2(s, sec, (long)s->next, 0, 0);
        }
        
        table_ident[s->v - TOK_IDENT]->sym_label = NULL;
        sym_free(s);
    }
    st->asm_labels = NULL;
}

static void use_section1(TCCState *s1, Section *sec)
{
    cur_text_section->data_offset = ind;
    cur_text_section = sec;
    ind = cur_text_section->data_offset;
}

static void use_section(TCCState *s1, const char *name)
{
    Section *sec;
    sec = find_section(s1, name);
    use_section1(s1, sec);
}

static void asm_parse_directive(TCCState *s1)
{
    int n, offset, v, size, tok1;
    Section *sec;
    uint8_t *ptr;

    
    next();
    sec = cur_text_section;
    switch(tok) {
    case TOK_ASM_align:
    case TOK_ASM_skip:
    case TOK_ASM_space:
        tok1 = tok;
        next();
        n = asm_int_expr(s1);
        if (tok1 == TOK_ASM_align) {
            if (n < 0 || (n & (n-1)) != 0)
                error("alignment must be a positive power of two");
            offset = (ind + n - 1) & -n;
            size = offset - ind;
            
            if (sec->sh_addralign < n)
                sec->sh_addralign = n;
        } else {
            size = n;
        }
        v = 0;
        if (tok == ',') {
            next();
            v = asm_int_expr(s1);
        }
    zero_pad:
        if (sec->sh_type != SHT_NOBITS) {
            sec->data_offset = ind;
            ptr = section_ptr_add(sec, size);
            memset(ptr, v, size);
        }
        ind += size;
        break;
    case TOK_ASM_quad:
        next();
        for(;;) {
            uint64_t vl;
            const char *p;

            p = tokc.cstr->data;
            if (tok != TOK_PPNUM) {
            error_constant:
                error("64 bit constant");
            }
            vl = strtoll(p, (char **)&p, 0);
            if (*p != '\0')
                goto error_constant;
            next();
            if (sec->sh_type != SHT_NOBITS) {
                
                gen_le32(vl);
                gen_le32(vl >> 32);
            } else {
                ind += 8;
            }
            if (tok != ',')
                break;
            next();
        }
        break;
    case TOK_ASM_byte:
        size = 1;
        goto asm_data;
    case TOK_ASM_word:
    case TOK_SHORT:
        size = 2;
        goto asm_data;
    case TOK_LONG:
    case TOK_INT:
        size = 4;
    asm_data:
        next();
        for(;;) {
            ExprValue e;
            asm_expr(s1, &e);
            if (sec->sh_type != SHT_NOBITS) {
                if (size == 4) {
                    gen_expr32(&e);
                } else {
                    if (e.sym)
                        expect("constant");
                    if (size == 1)
                        g(e.v);
                    else
                        gen_le16(e.v);
                }
            } else {
                ind += size;
            }
            if (tok != ',')
                break;
            next();
        }
        break;
    case TOK_ASM_fill:
        {
            int repeat, size, val, i, j;
            uint8_t repeat_buf[8];
            next();
            repeat = asm_int_expr(s1);
            if (repeat < 0) {
                error("repeat < 0; .fill ignored");
                break;
            }
            size = 1;
            val = 0;
            if (tok == ',') {
                next();
                size = asm_int_expr(s1);
                if (size < 0) {
                    error("size < 0; .fill ignored");
                    break;
                }
                if (size > 8)
                    size = 8;
                if (tok == ',') {
                    next();
                    val = asm_int_expr(s1);
                }
            }
            
            repeat_buf[0] = val;
            repeat_buf[1] = val >> 8;
            repeat_buf[2] = val >> 16;
            repeat_buf[3] = val >> 24;
            repeat_buf[4] = 0;
            repeat_buf[5] = 0;
            repeat_buf[6] = 0;
            repeat_buf[7] = 0;
            for(i = 0; i < repeat; i++) {
                for(j = 0; j < size; j++) {
                    g(repeat_buf[j]);
                }
            }
        }
        break;
    case TOK_ASM_org:
        {
            unsigned long n;
            next();
            
            n = asm_int_expr(s1);
            if (n < ind)
                error("attempt to .org backwards");
            v = 0;
            size = n - ind;
            goto zero_pad;
        }
        break;
    case TOK_ASM_globl:
    case TOK_ASM_global:
	{ 
            Sym *sym;

            next();
            sym = label_find(tok);
            if (!sym) {
                sym = label_push(&s1->asm_labels, tok, 0);
                sym->type.t = VT_VOID;
            }
            sym->type.t &= ~VT_STATIC;
            next();
	}
	break;
    case TOK_ASM_string:
    case TOK_ASM_ascii:
    case TOK_ASM_asciz:
        {
            const uint8_t *p;
            int i, size, t;

            t = tok;
            next();
            for(;;) {
                if (tok != TOK_STR)
                    expect("string constant");
                p = tokc.cstr->data;
                size = tokc.cstr->size;
                if (t == TOK_ASM_ascii && size > 0)
                    size--;
                for(i = 0; i < size; i++)
                    g(p[i]);
                next();
                if (tok == ',') {
                    next();
                } else if (tok != TOK_STR) {
                    break;
                }
            }
	}
	break;
    case TOK_ASM_text:
    case TOK_ASM_data:
    case TOK_ASM_bss:
	{ 
            char sname[64];
            tok1 = tok;
            n = 0;
            next();
            if (tok != ';' && tok != TOK_LINEFEED) {
		n = asm_int_expr(s1);
		next();
            }
            sprintf(sname, (n?".%s%d":".%s"), get_tok_str(tok1, NULL), n);
            use_section(s1, sname);
	}
	break;
    case TOK_SECTION1:
        {
            char sname[256];

            
            next();
            sname[0] = '\0';
            while (tok != ';' && tok != TOK_LINEFEED && tok != ',') {
                if (tok == TOK_STR)
                    pstrcat(sname, sizeof(sname), tokc.cstr->data);
                else
                    pstrcat(sname, sizeof(sname), get_tok_str(tok, NULL));
                next();
            }
            if (tok == ',') {
                
                next();
                if (tok != TOK_STR)
                    expect("string constant");
                next();
            }
            last_text_section = cur_text_section;
            use_section(s1, sname);
        }
        break;
    case TOK_ASM_previous:
        { 
            Section *sec;
            next();
            if (!last_text_section)
                error("no previous section referenced");
            sec = cur_text_section;
            use_section1(s1, last_text_section);
            last_text_section = sec;
        }
        break;
    default:
        error("unknown assembler directive '.%s'", get_tok_str(tok, NULL));
        break;
    }
}


static int tcc_assemble_internal(TCCState *s1, int do_preprocess)
{
    int opcode;

#if 0
    
    {
        const ASMInstr *pa;
        int freq[4];
        int op_vals[500];
        int nb_op_vals, i, j;

        nb_op_vals = 0;
        memset(freq, 0, sizeof(freq));
        for(pa = asm_instrs; pa->sym != 0; pa++) {
            freq[pa->nb_ops]++;
            for(i=0;i<pa->nb_ops;i++) {
                for(j=0;j<nb_op_vals;j++) {
                    if (pa->op_type[i] == op_vals[j])
                        goto found;
                }
                op_vals[nb_op_vals++] = pa->op_type[i];
            found: ;
            }
        }
        for(i=0;i<nb_op_vals;i++) {
            int v = op_vals[i];
            if ((v & (v - 1)) != 0)
                printf("%3d: %08x\n", i, v);
        }
        printf("size=%d nb=%d f0=%d f1=%d f2=%d f3=%d\n",
               sizeof(asm_instrs), sizeof(asm_instrs) / sizeof(ASMInstr),
               freq[0], freq[1], freq[2], freq[3]);
    }
#endif

    

    ch = file->buf_ptr[0];
    tok_flags = TOK_FLAG_BOL | TOK_FLAG_BOF;
    parse_flags = PARSE_FLAG_ASM_COMMENTS;
    if (do_preprocess)
        parse_flags |= PARSE_FLAG_PREPROCESS;
    next();
    for(;;) {
        if (tok == TOK_EOF)
            break;
        parse_flags |= PARSE_FLAG_LINEFEED; 
    redo:
        if (tok == '#') {
            
            while (tok != TOK_LINEFEED)
                next();
        } else if (tok == '.') {
            asm_parse_directive(s1);
        } else if (tok == TOK_PPNUM) {
            const char *p;
            int n;
            p = tokc.cstr->data;
            n = strtoul(p, (char **)&p, 10);
            if (*p != '\0')
                expect("':'");
            
            asm_new_label(s1, asm_get_local_label_name(s1, n), 1);
            next();
            skip(':');
            goto redo;
        } else if (tok >= TOK_IDENT) {
            
            opcode = tok;
            next();
            if (tok == ':') {
                
                asm_new_label(s1, opcode, 0);
                next();
                goto redo;
            } else if (tok == '=') {
                int n;
                next();
                n = asm_int_expr(s1);
                asm_new_label1(s1, opcode, 0, SHN_ABS, n);
                goto redo;
            } else {
                asm_opcode(s1, opcode);
            }
        }
        
        if (tok != ';' && tok != TOK_LINEFEED){
            expect("end of line");
        }
        parse_flags &= ~PARSE_FLAG_LINEFEED; 
        next();
    }

    asm_free_labels(s1);

    return 0;
}

static int tcc_assemble(TCCState *s1, int do_preprocess)
{
    Sym *define_start;
    int ret;

    preprocess_init(s1);

    
    cur_text_section = text_section;
    ind = cur_text_section->data_offset;

    define_start = define_stack;

    ret = tcc_assemble_internal(s1, do_preprocess);

    cur_text_section->data_offset = ind;

    free_defines(define_start); 

    return ret;
}


static void tcc_assemble_inline(TCCState *s1, char *str, int len)
{
    BufferedFile *bf, *saved_file;
    int saved_parse_flags, *saved_macro_ptr;

    bf = tcc_malloc(sizeof(BufferedFile));
    memset(bf, 0, sizeof(BufferedFile));
    bf->fd = -1;
    bf->buf_ptr = str;
    bf->buf_end = str + len;
    str[len] = CH_EOB;
    pstrcpy(bf->filename, sizeof(bf->filename), file->filename);
    bf->line_num = file->line_num;
    saved_file = file;
    file = bf;
    saved_parse_flags = parse_flags;
    saved_macro_ptr = macro_ptr;
    macro_ptr = NULL;
    
    tcc_assemble_internal(s1, 0);

    parse_flags = saved_parse_flags;
    macro_ptr = saved_macro_ptr;
    file = saved_file;
    tcc_free(bf);
}

static int find_constraint(ASMOperand *operands, int nb_operands, 
                           const char *name, const char **pp)
{
    int index;
    TokenSym *ts;
    const char *p;

    if (isnum(*name)) {
        index = 0;
        while (isnum(*name)) {
            index = (index * 10) + (*name) - '0';
            name++;
        }
        if ((unsigned)index >= nb_operands)
            index = -1;
    } else if (*name == '[') {
        name++;
        p = strchr(name, ']');
        if (p) {
            ts = tok_alloc(name, p - name);
            for(index = 0; index < nb_operands; index++) {
                if (operands[index].id == ts->tok)
                    goto found;
            }
            index = -1;
        found:
            name = p + 1;
        } else {
            index = -1;
        }
    } else {
        index = -1;
    }
    if (pp)
        *pp = name;
    return index;
}

static void subst_asm_operands(ASMOperand *operands, int nb_operands, 
                               int nb_outputs,
                               CString *out_str, CString *in_str)
{
    int c, index, modifier;
    const char *str;
    ASMOperand *op;
    SValue sv;

    cstr_new(out_str);
    str = in_str->data;
    for(;;) {
        c = *str++;
        if (c == '%') {
            if (*str == '%') {
                str++;
                goto add_char;
            }
            modifier = 0;
            if (*str == 'c' || *str == 'n' ||
                *str == 'b' || *str == 'w' || *str == 'h')
                modifier = *str++;
            index = find_constraint(operands, nb_operands, str, &str);
            if (index < 0)
                error("invalid operand reference after %%");
            op = &operands[index];
            sv = *op->vt;
            if (op->reg >= 0) {
                sv.r = op->reg;
                if ((op->vt->r & VT_VALMASK) == VT_LLOCAL)
                    sv.r |= VT_LVAL;
            }
            subst_asm_operand(out_str, &sv, modifier);
        } else {
        add_char:
            cstr_ccat(out_str, c);
            if (c == '\0')
                break;
        }
    }
}


static void parse_asm_operands(ASMOperand *operands, int *nb_operands_ptr,
                               int is_output)
{
    ASMOperand *op;
    int nb_operands;

    if (tok != ':') {
        nb_operands = *nb_operands_ptr;
        for(;;) {
            if (nb_operands >= MAX_ASM_OPERANDS)
                error("too many asm operands");
            op = &operands[nb_operands++];
            op->id = 0;
            if (tok == '[') {
                next();
                if (tok < TOK_IDENT)
                    expect("identifier");
                op->id = tok;
                next();
                skip(']');
            }
            if (tok != TOK_STR)
                expect("string constant");
            op->constraint = tcc_malloc(tokc.cstr->size);
            strcpy(op->constraint, tokc.cstr->data);
            next();
            skip('(');
            gexpr();
            if (is_output) {
                test_lvalue();
            } else {
                if ((vtop->r & VT_LVAL) &&
                    ((vtop->r & VT_VALMASK) == VT_LLOCAL ||
                     (vtop->r & VT_VALMASK) < VT_CONST) &&
                    !strchr(op->constraint, 'm')) {
                    gv(RC_INT);
                }
            }
            op->vt = vtop;
            skip(')');
            if (tok == ',') {
                next();
            } else {
                break;
            }
        }
        *nb_operands_ptr = nb_operands;
    }
}

static void parse_asm_str(CString *astr)
{
    skip('(');
    
    if (tok != TOK_STR)
        expect("string constant");
    cstr_new(astr);
    while (tok == TOK_STR) {
        
        cstr_cat(astr, tokc.cstr->data);
        next();
    }
    cstr_ccat(astr, '\0');
}

static void asm_instr(void)
{
    CString astr, astr1;
    ASMOperand operands[MAX_ASM_OPERANDS];
    int nb_inputs __attribute__((unused));
    int nb_outputs, nb_operands, i, must_subst, out_reg;
    uint8_t clobber_regs[NB_ASM_REGS];

    next();
    if (tok == TOK_VOLATILE1 || tok == TOK_VOLATILE2 || tok == TOK_VOLATILE3) {
        next();
    }
    parse_asm_str(&astr);
    nb_operands = 0;
    nb_outputs = 0;
    must_subst = 0;
    memset(clobber_regs, 0, sizeof(clobber_regs));
    if (tok == ':') {
        next();
        must_subst = 1;
        
        parse_asm_operands(operands, &nb_operands, 1);
        nb_outputs = nb_operands;
        if (tok == ':') {
            next();
            
            parse_asm_operands(operands, &nb_operands, 0);
            if (tok == ':') {
                
                
                next();
                for(;;) {
                    if (tok != TOK_STR)
                        expect("string constant");
                    asm_clobber(clobber_regs, tokc.cstr->data);
                    next();
                    if (tok == ',') {
                        next();
                    } else {
                        break;
                    }
                }
            }
        }
    }
    skip(')');
    if (tok != ';')
        expect("';'");
    nb_inputs = nb_operands - nb_outputs;
    
    
    save_regs(0);

    
    asm_compute_constraints(operands, nb_operands, nb_outputs, 
                            clobber_regs, &out_reg);

#ifdef ASM_DEBUG
    printf("asm: \"%s\"\n", (char *)astr.data);
#endif
    if (must_subst) {
        subst_asm_operands(operands, nb_operands, nb_outputs, &astr1, &astr);
        cstr_free(&astr);
    } else {
        astr1 = astr;
    }
#ifdef ASM_DEBUG
    printf("subst_asm: \"%s\"\n", (char *)astr1.data);
#endif

    
    asm_gen_code(operands, nb_operands, nb_outputs, 0, 
                 clobber_regs, out_reg);    

    
    tcc_assemble_inline(tcc_state, astr1.data, astr1.size - 1);

    
    next();

    
    asm_gen_code(operands, nb_operands, nb_outputs, 1, 
                 clobber_regs, out_reg);
    
    
    for(i=0;i<nb_operands;i++) {
        ASMOperand *op;
        op = &operands[i];
        tcc_free(op->constraint);
        vpop();
    }
    cstr_free(&astr1);
}

static void asm_global_instr(void)
{
    CString astr;

    next();
    parse_asm_str(&astr);
    skip(')');
    if (tok != ';')
        expect("';'");
    
#ifdef ASM_DEBUG
    printf("asm_global: \"%s\"\n", (char *)astr.data);
#endif
    cur_text_section = text_section;
    ind = cur_text_section->data_offset;

    
    tcc_assemble_inline(tcc_state, astr.data, astr.size - 1);
    
    cur_text_section->data_offset = ind;

    
    next();

    cstr_free(&astr);
}

#else
static void asm_instr(void)
{
    error("inline asm() not supported");
}
static void asm_global_instr(void)
{
    error("inline asm() not supported");
}
#endif

/*
 *  ELF file handling for TCC
 * 
 *  Copyright (c) 2001-2004 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

static int put_elf_str(Section *s, const char *sym)
{
    int offset, len;
    char *ptr;

    len = strlen(sym) + 1;
    offset = s->data_offset;
    ptr = section_ptr_add(s, len);
    memcpy(ptr, sym, len);
    return offset;
}

static unsigned long elf_hash(const unsigned char *name)
{
    unsigned long h = 0, g;
    
    while (*name) {
        h = (h << 4) + *name++;
        g = h & 0xf0000000;
        if (g)
            h ^= g >> 24;
        h &= ~g;
    }
    return h;
}

static void rebuild_hash(Section *s, unsigned int nb_buckets)
{
    Elf32_Sym *sym;
    int *ptr, *hash, nb_syms, sym_index, h;
    char *strtab;

    strtab = s->link->data;
    nb_syms = s->data_offset / sizeof(Elf32_Sym);

    s->hash->data_offset = 0;
    ptr = section_ptr_add(s->hash, (2 + nb_buckets + nb_syms) * sizeof(int));
    ptr[0] = nb_buckets;
    ptr[1] = nb_syms;
    ptr += 2;
    hash = ptr;
    memset(hash, 0, (nb_buckets + 1) * sizeof(int));
    ptr += nb_buckets + 1;

    sym = (Elf32_Sym *)s->data + 1;
    for(sym_index = 1; sym_index < nb_syms; sym_index++) {
        if (ELF32_ST_BIND(sym->st_info) != STB_LOCAL) {
            h = elf_hash(strtab + sym->st_name) % nb_buckets;
            *ptr = hash[h];
            hash[h] = sym_index;
        } else {
            *ptr = 0;
        }
        ptr++;
        sym++;
    }
}

static int put_elf_sym(Section *s, 
                       unsigned long value, unsigned long size,
                       int info, int other, int shndx, const char *name)
{
    int name_offset, sym_index;
    int nbuckets, h;
    Elf32_Sym *sym;
    Section *hs;
    
    sym = section_ptr_add(s, sizeof(Elf32_Sym));
    if (name)
        name_offset = put_elf_str(s->link, name);
    else
        name_offset = 0;
    
    sym->st_name = name_offset;
    sym->st_value = value;
    sym->st_size = size;
    sym->st_info = info;
    sym->st_other = other;
    sym->st_shndx = shndx;
    sym_index = sym - (Elf32_Sym *)s->data;
    hs = s->hash;
    if (hs) {
        int *ptr, *base;
        ptr = section_ptr_add(hs, sizeof(int));
        base = (int *)hs->data;
        
        if (ELF32_ST_BIND(info) != STB_LOCAL) {
            
            nbuckets = base[0];
            h = elf_hash(name) % nbuckets;
            *ptr = base[2 + h];
            base[2 + h] = sym_index;
            base[1]++;
            
            hs->nb_hashed_syms++;
            if (hs->nb_hashed_syms > 2 * nbuckets) {
                rebuild_hash(s, 2 * nbuckets);
            }
        } else {
            *ptr = 0;
            base[1]++;
        }
    }
    return sym_index;
}

static int find_elf_sym(Section *s, const char *name)
{
    Elf32_Sym *sym;
    Section *hs;
    int nbuckets, sym_index, h;
    const char *name1;
    
    hs = s->hash;
    if (!hs)
        return 0;
    nbuckets = ((int *)hs->data)[0];
    h = elf_hash(name) % nbuckets;
    sym_index = ((int *)hs->data)[2 + h];
    while (sym_index != 0) {
        sym = &((Elf32_Sym *)s->data)[sym_index];
        name1 = s->link->data + sym->st_name;
        if (!strcmp(name, name1))
            return sym_index;
        sym_index = ((int *)hs->data)[2 + nbuckets + sym_index];
    }
    return 0;
}

int tcc_get_symbol(TCCState *s, unsigned long *pval, const char *name)
{
    int sym_index;
    Elf32_Sym *sym;
    
    sym_index = find_elf_sym(symtab_section, name);
    if (!sym_index)
        return -1;
    sym = &((Elf32_Sym *)symtab_section->data)[sym_index];
    *pval = sym->st_value;
    return 0;
}

void *tcc_get_symbol_err(TCCState *s, const char *name)
{
    unsigned long val;
    if (tcc_get_symbol(s, &val, name) < 0)
        error("%s not defined", name);
    return (void *)val;
}

static int add_elf_sym(Section *s, unsigned long value, unsigned long size,
                       int info, int other, int sh_num, const char *name)
{
    Elf32_Sym *esym;
    int sym_bind, sym_index, sym_type, esym_bind;

    sym_bind = ELF32_ST_BIND(info);
    sym_type = ELF32_ST_TYPE(info);
        
    if (sym_bind != STB_LOCAL) {
        
        sym_index = find_elf_sym(s, name);
        if (!sym_index)
            goto do_def;
        esym = &((Elf32_Sym *)s->data)[sym_index];
        if (esym->st_shndx != SHN_UNDEF) {
            esym_bind = ELF32_ST_BIND(esym->st_info);
            if (sh_num == SHN_UNDEF) {
            } else if (sym_bind == STB_GLOBAL && esym_bind == STB_WEAK) {
                
                goto do_patch;
            } else if (sym_bind == STB_WEAK && esym_bind == STB_GLOBAL) {
                
            } else {
#if 0
                printf("new_bind=%d new_shndx=%d last_bind=%d old_shndx=%d\n",
                       sym_bind, sh_num, esym_bind, esym->st_shndx);
#endif
                
                if (s != tcc_state->dynsymtab_section)
                    error_noabort("'%s' defined twice", name);
            }
        } else {
        do_patch:
            esym->st_info = ELF32_ST_INFO(sym_bind, sym_type);
            esym->st_shndx = sh_num;
            esym->st_value = value;
            esym->st_size = size;
            esym->st_other = other;
        }
    } else {
    do_def:
        sym_index = put_elf_sym(s, value, size, 
                                ELF32_ST_INFO(sym_bind, sym_type), other, 
                                sh_num, name);
    }
    return sym_index;
}

static void put_elf_reloc(Section *symtab, Section *s, unsigned long offset,
                          int type, int symbol)
{
    char buf[256];
    Section *sr;
    Elf32_Rel *rel;

    sr = s->reloc;
    if (!sr) {
        
        snprintf(buf, sizeof(buf), ".rel%s", s->name);
        sr = new_section(tcc_state, buf, SHT_REL, symtab->sh_flags);
        sr->sh_entsize = sizeof(Elf32_Rel);
        sr->link = symtab;
        sr->sh_info = s->sh_num;
        s->reloc = sr;
    }
    rel = section_ptr_add(sr, sizeof(Elf32_Rel));
    rel->r_offset = offset;
    rel->r_info = ELF32_R_INFO(symbol, type);
}


typedef struct {
    unsigned long n_strx;         
    unsigned char n_type;         
    unsigned char n_other;        
    unsigned short n_desc;        
    unsigned long n_value;        
} Stab_Sym;

static void put_stabs(const char *str, int type, int other, int desc, 
                      unsigned long value)
{
    Stab_Sym *sym;

    sym = section_ptr_add(stab_section, sizeof(Stab_Sym));
    if (str) {
        sym->n_strx = put_elf_str(stabstr_section, str);
    } else {
        sym->n_strx = 0;
    }
    sym->n_type = type;
    sym->n_other = other;
    sym->n_desc = desc;
    sym->n_value = value;
}

static void put_stabs_r(const char *str, int type, int other, int desc, 
                        unsigned long value, Section *sec, int sym_index)
{
    put_stabs(str, type, other, desc, value);
    put_elf_reloc(symtab_section, stab_section, 
                  stab_section->data_offset - sizeof(unsigned long),
                  R_DATA_32, sym_index);
}

static void put_stabn(int type, int other, int desc, int value)
{
    put_stabs(NULL, type, other, desc, value);
}

static void put_stabd(int type, int other, int desc)
{
    put_stabs(NULL, type, other, desc, 0);
}

static void sort_syms(TCCState *s1, Section *s)
{
    int *old_to_new_syms;
    Elf32_Sym *new_syms;
    int nb_syms, i;
    Elf32_Sym *p, *q;
    Elf32_Rel *rel, *rel_end;
    Section *sr;
    int type, sym_index;

    nb_syms = s->data_offset / sizeof(Elf32_Sym);
    new_syms = tcc_malloc(nb_syms * sizeof(Elf32_Sym));
    old_to_new_syms = tcc_malloc(nb_syms * sizeof(int));

    
    p = (Elf32_Sym *)s->data;
    q = new_syms;
    for(i = 0; i < nb_syms; i++) {
        if (ELF32_ST_BIND(p->st_info) == STB_LOCAL) {
            old_to_new_syms[i] = q - new_syms;
            *q++ = *p;
        }
        p++;
    }
    
    s->sh_info = q - new_syms;

    
    p = (Elf32_Sym *)s->data;
    for(i = 0; i < nb_syms; i++) {
        if (ELF32_ST_BIND(p->st_info) != STB_LOCAL) {
            old_to_new_syms[i] = q - new_syms;
            *q++ = *p;
        }
        p++;
    }
    
    
    memcpy(s->data, new_syms, nb_syms * sizeof(Elf32_Sym));
    tcc_free(new_syms);

    
    for(i = 1; i < s1->nb_sections; i++) {
        sr = s1->sections[i];
        if (sr->sh_type == SHT_REL && sr->link == s) {
            rel_end = (Elf32_Rel *)(sr->data + sr->data_offset);
            for(rel = (Elf32_Rel *)sr->data;
                rel < rel_end;
                rel++) {
                sym_index = ELF32_R_SYM(rel->r_info);
                type = ELF32_R_TYPE(rel->r_info);
                sym_index = old_to_new_syms[sym_index];
                rel->r_info = ELF32_R_INFO(sym_index, type);
            }
        }
    }
    
    tcc_free(old_to_new_syms);
}

static void relocate_common_syms(void)
{
    Elf32_Sym *sym, *sym_end;
    unsigned long offset, align;
    
    sym_end = (Elf32_Sym *)(symtab_section->data + symtab_section->data_offset);
    for(sym = (Elf32_Sym *)symtab_section->data + 1; 
        sym < sym_end;
        sym++) {
        if (sym->st_shndx == SHN_COMMON) {
            
            align = sym->st_value;
            offset = bss_section->data_offset;
            offset = (offset + align - 1) & -align;
            sym->st_value = offset;
            sym->st_shndx = bss_section->sh_num;
            offset += sym->st_size;
            bss_section->data_offset = offset;
        }
    }
}

static void relocate_syms(TCCState *s1, int do_resolve)
{
    Elf32_Sym *sym, *esym, *sym_end;
    int sym_bind, sh_num, sym_index;
    const char *name;
    unsigned long addr;

    sym_end = (Elf32_Sym *)(symtab_section->data + symtab_section->data_offset);
    for(sym = (Elf32_Sym *)symtab_section->data + 1; 
        sym < sym_end;
        sym++) {
        sh_num = sym->st_shndx;
        if (sh_num == SHN_UNDEF) {
            name = strtab_section->data + sym->st_name;
            if (do_resolve) {
                name = symtab_section->link->data + sym->st_name;
                addr = (unsigned long)resolve_sym(s1, name, ELF32_ST_TYPE(sym->st_info));
                if (addr) {
                    sym->st_value = addr;
                    goto found;
                }
            } else if (s1->dynsym) {
                
                sym_index = find_elf_sym(s1->dynsym, name);
                if (sym_index) {
                    esym = &((Elf32_Sym *)s1->dynsym->data)[sym_index];
                    sym->st_value = esym->st_value;
                    goto found;
                }
            }
            if (!strcmp(name, "_fp_hw"))
                goto found;
            sym_bind = ELF32_ST_BIND(sym->st_info);
            if (sym_bind == STB_WEAK) {
                sym->st_value = 0;
            } else {
                error_noabort("undefined symbol '%s'", name);
            }
        } else if (sh_num < SHN_LORESERVE) {
            
            sym->st_value += s1->sections[sym->st_shndx]->sh_addr;
        }
    found: ;
    }
}

static void relocate_section(TCCState *s1, Section *s)
{
    Section *sr;
    Elf32_Rel *rel, *rel_end, *qrel;
    Elf32_Sym *sym;
    int type, sym_index;
    unsigned char *ptr;
    unsigned long val, addr;
#if defined(TCC_TARGET_I386)
    int esym_index;
#endif

    sr = s->reloc;
    rel_end = (Elf32_Rel *)(sr->data + sr->data_offset);
    qrel = (Elf32_Rel *)sr->data;
    for(rel = qrel;
        rel < rel_end;
        rel++) {
        ptr = s->data + rel->r_offset;

        sym_index = ELF32_R_SYM(rel->r_info);
        sym = &((Elf32_Sym *)symtab_section->data)[sym_index];
        val = sym->st_value;
        type = ELF32_R_TYPE(rel->r_info);
        addr = s->sh_addr + rel->r_offset;

        
        switch(type) {
#if defined(TCC_TARGET_I386)
        case R_386_32:
            if (s1->output_type == TCC_OUTPUT_DLL) {
                esym_index = s1->symtab_to_dynsym[sym_index];
                qrel->r_offset = rel->r_offset;
                if (esym_index) {
                    qrel->r_info = ELF32_R_INFO(esym_index, R_386_32);
                    qrel++;
                    break;
                } else {
                    qrel->r_info = ELF32_R_INFO(0, R_386_RELATIVE);
                    qrel++;
                }
            }
            *(int *)ptr += val;
            break;
        case R_386_PC32:
            if (s1->output_type == TCC_OUTPUT_DLL) {
                
                esym_index = s1->symtab_to_dynsym[sym_index];
                if (esym_index) {
                    qrel->r_offset = rel->r_offset;
                    qrel->r_info = ELF32_R_INFO(esym_index, R_386_PC32);
                    qrel++;
                    break;
                }
            }
            *(int *)ptr += val - addr;
            break;
        case R_386_PLT32:
            *(int *)ptr += val - addr;
            break;
        case R_386_GLOB_DAT:
        case R_386_JMP_SLOT:
            *(int *)ptr = val;
            break;
        case R_386_GOTPC:
            *(int *)ptr += s1->got->sh_addr - addr;
            break;
        case R_386_GOTOFF:
            *(int *)ptr += val - s1->got->sh_addr;
            break;
        case R_386_GOT32:
            
            *(int *)ptr += s1->got_offsets[sym_index];
            break;
#elif defined(TCC_TARGET_ARM)
	case R_ARM_PC24:
	case R_ARM_PLT32:
	    {
                int x;
                x = (*(int *)ptr)&0xffffff;
                (*(int *)ptr) &= 0xff000000;
                if (x & 0x800000)
                    x -= 0x1000000;
                x *= 4;
                x += val - addr;
                if((x & 3) != 0 || x >= 0x4000000 || x < -0x4000000)
                    error("can't relocate value at %x",addr);
                x >>= 2;
                x &= 0xffffff;
                (*(int *)ptr) |= x;
	    }
	    break;
	case R_ARM_ABS32:
	    *(int *)ptr += val;
	    break;
	case R_ARM_GOTPC:
	    *(int *)ptr += s1->got->sh_addr - addr;
	    break;
        case R_ARM_GOT32:
            
            *(int *)ptr += s1->got_offsets[sym_index];
            break;
	case R_ARM_COPY:
            break;
	default:
	    fprintf(stderr,"FIXME: handle reloc type %x at %lx [%.8x] to %lx\n",
                    type,addr,(unsigned int )ptr,val);
            break;
#elif defined(TCC_TARGET_C67)
	case R_C60_32:
	    *(int *)ptr += val;
	    break;
        case R_C60LO16:
            {
                uint32_t orig;
                
                
                
                
                orig  =   ((*(int *)(ptr  )) >> 7) & 0xffff;
                orig |=  (((*(int *)(ptr+4)) >> 7) & 0xffff) << 16;
                
                
                
                *(int *) ptr    = (*(int *) ptr    & (~(0xffff << 7)) ) |  (((val+orig)      & 0xffff) << 7);
                *(int *)(ptr+4) = (*(int *)(ptr+4) & (~(0xffff << 7)) ) | ((((val+orig)>>16) & 0xffff) << 7);
            }
            break;
        case R_C60HI16:
            break;
        default:
	    fprintf(stderr,"FIXME: handle reloc type %x at %lx [%.8x] to %lx\n",
                    type,addr,(unsigned int )ptr,val);
            break;
#else
#error unsupported processor
#endif
        }
    }
    
    if (sr->sh_flags & SHF_ALLOC)
        sr->link = s1->dynsym;
}

static void relocate_rel(TCCState *s1, Section *sr)
{
    Section *s;
    Elf32_Rel *rel, *rel_end;
    
    s = s1->sections[sr->sh_info];
    rel_end = (Elf32_Rel *)(sr->data + sr->data_offset);
    for(rel = (Elf32_Rel *)sr->data;
        rel < rel_end;
        rel++) {
        rel->r_offset += s->sh_addr;
    }
}

static int prepare_dynamic_rel(TCCState *s1, Section *sr)
{
    Elf32_Rel *rel, *rel_end;
    int sym_index, esym_index, type, count;

    count = 0;
    rel_end = (Elf32_Rel *)(sr->data + sr->data_offset);
    for(rel = (Elf32_Rel *)sr->data; rel < rel_end; rel++) {
        sym_index = ELF32_R_SYM(rel->r_info);
        type = ELF32_R_TYPE(rel->r_info);
        switch(type) {
        case R_386_32:
            count++;
            break;
        case R_386_PC32:
            esym_index = s1->symtab_to_dynsym[sym_index];
            if (esym_index)
                count++;
            break;
        default:
            break;
        }
    }
    if (count) {
        
        sr->sh_flags |= SHF_ALLOC;
        sr->sh_size = count * sizeof(Elf32_Rel);
    }
    return count;
}

static void put_got_offset(TCCState *s1, int index, unsigned long val)
{
    int n;
    unsigned long *tab;

    if (index >= s1->nb_got_offsets) {
        
        n = 1;
        while (index >= n)
            n *= 2;
        tab = tcc_realloc(s1->got_offsets, n * sizeof(unsigned long));
        if (!tab)
            error("memory full");
        s1->got_offsets = tab;
        memset(s1->got_offsets + s1->nb_got_offsets, 0,
               (n - s1->nb_got_offsets) * sizeof(unsigned long));
        s1->nb_got_offsets = n;
    }
    s1->got_offsets[index] = val;
}

static void put32(unsigned char *p, uint32_t val)
{
    p[0] = val;
    p[1] = val >> 8;
    p[2] = val >> 16;
    p[3] = val >> 24;
}

#if defined(TCC_TARGET_I386) || defined(TCC_TARGET_ARM)
static uint32_t get32(unsigned char *p)
{
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}
#endif

static void build_got(TCCState *s1)
{
    unsigned char *ptr;

    
    s1->got = new_section(s1, ".got", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE);
    s1->got->sh_entsize = 4;
    add_elf_sym(symtab_section, 0, 4, ELF32_ST_INFO(STB_GLOBAL, STT_OBJECT), 
                0, s1->got->sh_num, "_GLOBAL_OFFSET_TABLE_");
    ptr = section_ptr_add(s1->got, 3 * sizeof(int));
    
    put32(ptr, 0);
    
    put32(ptr + 4, 0);
    put32(ptr + 8, 0);
}

static void put_got_entry(TCCState *s1,
                          int reloc_type, unsigned long size, int info, 
                          int sym_index)
{
    int index;
    const char *name;
    Elf32_Sym *sym;
    unsigned long offset;
    int *ptr;

    if (!s1->got)
        build_got(s1);

    
    if (sym_index < s1->nb_got_offsets &&
        s1->got_offsets[sym_index] != 0)
        return;
    
    put_got_offset(s1, sym_index, s1->got->data_offset);

    if (s1->dynsym) {
        sym = &((Elf32_Sym *)symtab_section->data)[sym_index];
        name = symtab_section->link->data + sym->st_name;
        offset = sym->st_value;
#ifdef TCC_TARGET_I386
        if (reloc_type == R_386_JMP_SLOT) {
            Section *plt;
            uint8_t *p;
            int modrm;

            
            if (s1->output_type == TCC_OUTPUT_DLL)
                modrm = 0xa3;
            else
                modrm = 0x25;

            
            plt = s1->plt;
            if (plt->data_offset == 0) {
                
                p = section_ptr_add(plt, 16);
                p[0] = 0xff; 
                p[1] = modrm + 0x10;
                put32(p + 2, 4);
                p[6] = 0xff; 
                p[7] = modrm;
                put32(p + 8, 8);
            }

            p = section_ptr_add(plt, 16);
            p[0] = 0xff; 
            p[1] = modrm;
            put32(p + 2, s1->got->data_offset);
            p[6] = 0x68; 
            put32(p + 7, (plt->data_offset - 32) >> 1);
            p[11] = 0xe9; 
            put32(p + 12, -(plt->data_offset));

            if (s1->output_type == TCC_OUTPUT_EXE)
                offset = plt->data_offset - 16;
        }
#elif defined(TCC_TARGET_ARM)
	if (reloc_type == R_ARM_JUMP_SLOT) {
            Section *plt;
            uint8_t *p;
            
            
            if (s1->output_type == TCC_OUTPUT_DLL)
                error("DLLs unimplemented!");

            
            plt = s1->plt;
            if (plt->data_offset == 0) {
                
                p = section_ptr_add(plt, 16);
		put32(p     , 0xe52de004);
		put32(p +  4, 0xe59fe010);
		put32(p +  8, 0xe08fe00e);
		put32(p + 12, 0xe5bef008);
            }

            p = section_ptr_add(plt, 16);
	    put32(p  , 0xe59fc004);
	    put32(p+4, 0xe08fc00c);
	    put32(p+8, 0xe59cf000);
	    put32(p+12, s1->got->data_offset);

            if (s1->output_type == TCC_OUTPUT_EXE)
                offset = plt->data_offset - 16;
        }
#elif defined(TCC_TARGET_C67)
        error("C67 got not implemented");
#else
#error unsupported CPU
#endif
        index = put_elf_sym(s1->dynsym, offset, 
                            size, info, 0, sym->st_shndx, name);
        
        put_elf_reloc(s1->dynsym, s1->got, 
                      s1->got->data_offset, 
                      reloc_type, index);
    }
    ptr = section_ptr_add(s1->got, sizeof(int));
    *ptr = 0;
}

static void build_got_entries(TCCState *s1)
{
    Section *s, *symtab __attribute__((unused));
    Elf32_Rel *rel, *rel_end;
    Elf32_Sym *sym;
    int i, type, reloc_type, sym_index;

    for(i = 1; i < s1->nb_sections; i++) {
        s = s1->sections[i];
        if (s->sh_type != SHT_REL)
            continue;
        
        if (s->link != symtab_section)
            continue;
        symtab = s->link;
        rel_end = (Elf32_Rel *)(s->data + s->data_offset);
        for(rel = (Elf32_Rel *)s->data;
            rel < rel_end;
            rel++) {
            type = ELF32_R_TYPE(rel->r_info);
            switch(type) {
#if defined(TCC_TARGET_I386)
            case R_386_GOT32:
            case R_386_GOTOFF:
            case R_386_GOTPC:
            case R_386_PLT32:
                if (!s1->got)
                    build_got(s1);
                if (type == R_386_GOT32 || type == R_386_PLT32) {
                    sym_index = ELF32_R_SYM(rel->r_info);
                    sym = &((Elf32_Sym *)symtab_section->data)[sym_index];
                    
                    if (type == R_386_GOT32)
                        reloc_type = R_386_GLOB_DAT;
                    else
                        reloc_type = R_386_JMP_SLOT;
                    put_got_entry(s1, reloc_type, sym->st_size, sym->st_info, 
                                  sym_index);
                }
                break;
#elif defined(TCC_TARGET_ARM)
	    case R_ARM_GOT32:
            case R_ARM_GOTOFF:
            case R_ARM_GOTPC:
            case R_ARM_PLT32:
                if (!s1->got)
                    build_got(s1);
                if (type == R_ARM_GOT32 || type == R_ARM_PLT32) {
                    sym_index = ELF32_R_SYM(rel->r_info);
                    sym = &((Elf32_Sym *)symtab_section->data)[sym_index];
                    
                    if (type == R_ARM_GOT32)
                        reloc_type = R_ARM_GLOB_DAT;
                    else
                        reloc_type = R_ARM_JUMP_SLOT;
                    put_got_entry(s1, reloc_type, sym->st_size, sym->st_info, 
                                  sym_index);
                }
                break;
#elif defined(TCC_TARGET_C67)
	    case R_C60_GOT32:
            case R_C60_GOTOFF:
            case R_C60_GOTPC:
            case R_C60_PLT32:
                if (!s1->got)
                    build_got(s1);
                if (type == R_C60_GOT32 || type == R_C60_PLT32) {
                    sym_index = ELF32_R_SYM(rel->r_info);
                    sym = &((Elf32_Sym *)symtab_section->data)[sym_index];
                    
                    if (type == R_C60_GOT32)
                        reloc_type = R_C60_GLOB_DAT;
                    else
                        reloc_type = R_C60_JMP_SLOT;
                    put_got_entry(s1, reloc_type, sym->st_size, sym->st_info, 
                                  sym_index);
                }
                break;
#else
#error unsupported CPU
#endif
            default:
                break;
            }
        }
    }
}

static Section *new_symtab(TCCState *s1,
                           const char *symtab_name, int sh_type, int sh_flags,
                           const char *strtab_name, 
                           const char *hash_name, int hash_sh_flags)
{
    Section *symtab, *strtab, *hash;
    int *ptr, nb_buckets;

    symtab = new_section(s1, symtab_name, sh_type, sh_flags);
    symtab->sh_entsize = sizeof(Elf32_Sym);
    strtab = new_section(s1, strtab_name, SHT_STRTAB, sh_flags);
    put_elf_str(strtab, "");
    symtab->link = strtab;
    put_elf_sym(symtab, 0, 0, 0, 0, 0, NULL);
    
    nb_buckets = 1;

    hash = new_section(s1, hash_name, SHT_HASH, hash_sh_flags);
    hash->sh_entsize = sizeof(int);
    symtab->hash = hash;
    hash->link = symtab;

    ptr = section_ptr_add(hash, (2 + nb_buckets + 1) * sizeof(int));
    ptr[0] = nb_buckets;
    ptr[1] = 1;
    memset(ptr + 2, 0, (nb_buckets + 1) * sizeof(int));
    return symtab;
}

static void put_dt(Section *dynamic, int dt, unsigned long val)
{
    Elf32_Dyn *dyn;
    dyn = section_ptr_add(dynamic, sizeof(Elf32_Dyn));
    dyn->d_tag = dt;
    dyn->d_un.d_val = val;
}

static void add_init_array_defines(TCCState *s1, const char *section_name)
{
    Section *s;
    long end_offset;
    char sym_start[1024];
    char sym_end[1024];
    
    snprintf(sym_start, sizeof(sym_start), "__%s_start", section_name + 1);
    snprintf(sym_end, sizeof(sym_end), "__%s_end", section_name + 1);

    s = find_section(s1, section_name);
    if (!s) {
        end_offset = 0;
        s = data_section;
    } else {
        end_offset = s->data_offset;
    }

    add_elf_sym(symtab_section, 
                0, 0,
                ELF32_ST_INFO(STB_GLOBAL, STT_NOTYPE), 0,
                s->sh_num, sym_start);
    add_elf_sym(symtab_section, 
                end_offset, 0,
                ELF32_ST_INFO(STB_GLOBAL, STT_NOTYPE), 0,
                s->sh_num, sym_end);
}

static void tcc_add_runtime(TCCState *s1)
{
    char buf[1024];

#ifdef CONFIG_TCC_BCHECK
    if (do_bounds_check) {
        unsigned long *ptr;
        Section *init_section;
        unsigned char *pinit;
        int sym_index;

        
        ptr = section_ptr_add(bounds_section, sizeof(unsigned long));
        *ptr = 0;
        add_elf_sym(symtab_section, 0, 0, 
                    ELF32_ST_INFO(STB_GLOBAL, STT_NOTYPE), 0,
                    bounds_section->sh_num, "__bounds_start");
        
        snprintf(buf, sizeof(buf), "%s/%s", tcc_lib_path, "bcheck.o");
        tcc_add_file(s1, buf);
#ifdef TCC_TARGET_I386
        if (s1->output_type != TCC_OUTPUT_MEMORY) {
            
            init_section = find_section(s1, ".init");
            pinit = section_ptr_add(init_section, 5);
            pinit[0] = 0xe8;
            put32(pinit + 1, -4);
            sym_index = find_elf_sym(symtab_section, "__bound_init");
            put_elf_reloc(symtab_section, init_section, 
                          init_section->data_offset - 4, R_386_PC32, sym_index);
        }
#endif
    }
#endif
    
    if (!s1->nostdlib) {
        tcc_add_library(s1, "c");

        snprintf(buf, sizeof(buf), "%s/%s", tcc_lib_path, "libtcc1.a");
        tcc_add_file(s1, buf);
    }
    
    if (s1->output_type != TCC_OUTPUT_MEMORY && !s1->nostdlib) {
        tcc_add_file(s1, CONFIG_TCC_CRT_PREFIX "/crtn.o");
    }
}

static void tcc_add_linker_symbols(TCCState *s1)
{
    char buf[1024];
    int i;
    Section *s;

    add_elf_sym(symtab_section, 
                text_section->data_offset, 0,
                ELF32_ST_INFO(STB_GLOBAL, STT_NOTYPE), 0,
                text_section->sh_num, "_etext");
    add_elf_sym(symtab_section, 
                data_section->data_offset, 0,
                ELF32_ST_INFO(STB_GLOBAL, STT_NOTYPE), 0,
                data_section->sh_num, "_edata");
    add_elf_sym(symtab_section, 
                bss_section->data_offset, 0,
                ELF32_ST_INFO(STB_GLOBAL, STT_NOTYPE), 0,
                bss_section->sh_num, "_end");
    
    add_init_array_defines(s1, ".preinit_array");
    add_init_array_defines(s1, ".init_array");
    add_init_array_defines(s1, ".fini_array");
    
    for(i = 1; i < s1->nb_sections; i++) {
        s = s1->sections[i];
        if (s->sh_type == SHT_PROGBITS &&
            (s->sh_flags & SHF_ALLOC)) {
            const char *p;
            int ch;

            
            p = s->name;
            for(;;) {
                ch = *p;
                if (!ch)
                    break;
                if (!isid(ch) && !isnum(ch))
                    goto next_sec;
                p++;
            }
            snprintf(buf, sizeof(buf), "__start_%s", s->name);
            add_elf_sym(symtab_section, 
                        0, 0,
                        ELF32_ST_INFO(STB_GLOBAL, STT_NOTYPE), 0,
                        s->sh_num, buf);
            snprintf(buf, sizeof(buf), "__stop_%s", s->name);
            add_elf_sym(symtab_section,
                        s->data_offset, 0,
                        ELF32_ST_INFO(STB_GLOBAL, STT_NOTYPE), 0,
                        s->sh_num, buf);
        }
    next_sec: ;
    }
}

#ifdef __FreeBSD__
static char elf_interp[] = "/usr/libexec/ld-elf.so.1";
#else
static char elf_interp[] = "/lib/ld-linux.so.2";
#endif

static void tcc_output_binary(TCCState *s1, FILE *f,
                              const int *section_order)
{
    Section *s;
    int i, offset, size;

    offset = 0;
    for(i=1;i<s1->nb_sections;i++) {
        s = s1->sections[section_order[i]];
        if (s->sh_type != SHT_NOBITS &&
            (s->sh_flags & SHF_ALLOC)) {
            while (offset < s->sh_offset) {
                fputc(0, f);
                offset++;
            }
            size = s->sh_size;
            dummy_size_t = fwrite(s->data, 1, size, f);
            offset += size;
        }
    }
}

int tcc_output_file(TCCState *s1, const char *filename)
{
    Elf32_Ehdr ehdr;
    FILE *f;
    int fd, mode, ret;
    int *section_order;
    int shnum, i, phnum, file_offset, offset, size, j, tmp, sh_order_index, k;
    unsigned long addr;
    Section *strsec, *s;
    Elf32_Shdr shdr, *sh;
    Elf32_Phdr *phdr, *ph;
    Section *interp, *dynamic, *dynstr;
    unsigned long saved_dynamic_data_offset;
    Elf32_Sym *sym;
    int type, file_type;
    unsigned long rel_addr, rel_size;
    
    file_type = s1->output_type;
    s1->nb_errors = 0;

    if (file_type != TCC_OUTPUT_OBJ) {
        tcc_add_runtime(s1);
    }

    phdr = NULL;
    section_order = NULL;
    interp = NULL;
    dynamic = NULL;
    dynstr = NULL; 
    saved_dynamic_data_offset = 0; 
    
    if (file_type != TCC_OUTPUT_OBJ) {
        relocate_common_syms();

        tcc_add_linker_symbols(s1);

        if (!s1->static_link) {
            const char *name;
            int sym_index, index;
            Elf32_Sym *esym, *sym_end;
            
            if (file_type == TCC_OUTPUT_EXE) {
                char *ptr;
                
                interp = new_section(s1, ".interp", SHT_PROGBITS, SHF_ALLOC);
                interp->sh_addralign = 1;
                ptr = section_ptr_add(interp, sizeof(elf_interp));
                strcpy(ptr, elf_interp);
            }
        
            
            s1->dynsym = new_symtab(s1, ".dynsym", SHT_DYNSYM, SHF_ALLOC,
                                    ".dynstr", 
                                    ".hash", SHF_ALLOC);
            dynstr = s1->dynsym->link;
            
            
            dynamic = new_section(s1, ".dynamic", SHT_DYNAMIC, 
                                  SHF_ALLOC | SHF_WRITE);
            dynamic->link = dynstr;
            dynamic->sh_entsize = sizeof(Elf32_Dyn);
        
            
            s1->plt = new_section(s1, ".plt", SHT_PROGBITS, 
                                  SHF_ALLOC | SHF_EXECINSTR);
            s1->plt->sh_entsize = 4;

            build_got(s1);

            sym_end = (Elf32_Sym *)(symtab_section->data + 
                                    symtab_section->data_offset);
            if (file_type == TCC_OUTPUT_EXE) {
                for(sym = (Elf32_Sym *)symtab_section->data + 1; 
                    sym < sym_end;
                    sym++) {
                    if (sym->st_shndx == SHN_UNDEF) {
                        name = symtab_section->link->data + sym->st_name;
                        sym_index = find_elf_sym(s1->dynsymtab_section, name);
                        if (sym_index) {
                            esym = &((Elf32_Sym *)s1->dynsymtab_section->data)[sym_index];
                            type = ELF32_ST_TYPE(esym->st_info);
                            if (type == STT_FUNC) {
                                put_got_entry(s1, R_JMP_SLOT, esym->st_size, 
                                              esym->st_info, 
                                              sym - (Elf32_Sym *)symtab_section->data);
                            } else if (type == STT_OBJECT) {
                                unsigned long offset;
                                offset = bss_section->data_offset;
                                
                                offset = (offset + 16 - 1) & -16;
                                index = put_elf_sym(s1->dynsym, offset, esym->st_size, 
                                                    esym->st_info, 0, 
                                                    bss_section->sh_num, name);
                                put_elf_reloc(s1->dynsym, bss_section, 
                                              offset, R_COPY, index);
                                offset += esym->st_size;
                                bss_section->data_offset = offset;
                            }
                        } else {
                                
                            if (ELF32_ST_BIND(sym->st_info) == STB_WEAK ||
                                !strcmp(name, "_fp_hw")) {
                            } else {
                                error_noabort("undefined symbol '%s'", name);
                            }
                        }
                    } else if (s1->rdynamic && 
                               ELF32_ST_BIND(sym->st_info) != STB_LOCAL) {
                        name = symtab_section->link->data + sym->st_name;
                        put_elf_sym(s1->dynsym, sym->st_value, sym->st_size, 
                                    sym->st_info, 0, 
                                    sym->st_shndx, name);
                    }
                }
            
                if (s1->nb_errors)
                    goto fail;

                sym_end = (Elf32_Sym *)(s1->dynsymtab_section->data + 
                                        s1->dynsymtab_section->data_offset);
                for(esym = (Elf32_Sym *)s1->dynsymtab_section->data + 1; 
                    esym < sym_end;
                    esym++) {
                    if (esym->st_shndx == SHN_UNDEF) {
                        name = s1->dynsymtab_section->link->data + esym->st_name;
                        sym_index = find_elf_sym(symtab_section, name);
                        if (sym_index) {
                            sym = &((Elf32_Sym *)symtab_section->data)[sym_index];
                            put_elf_sym(s1->dynsym, sym->st_value, sym->st_size, 
                                        sym->st_info, 0, 
                                        sym->st_shndx, name);
                        } else {
                            if (ELF32_ST_BIND(esym->st_info) == STB_WEAK) {
                                
                            } else {
                                warning("undefined dynamic symbol '%s'", name);
                            }
                        }
                    }
                }
            } else {
                int nb_syms;
                
                nb_syms = symtab_section->data_offset / sizeof(Elf32_Sym);
                s1->symtab_to_dynsym = tcc_mallocz(sizeof(int) * nb_syms);
                for(sym = (Elf32_Sym *)symtab_section->data + 1; 
                    sym < sym_end;
                    sym++) {
                    if (ELF32_ST_BIND(sym->st_info) != STB_LOCAL) {
                        name = symtab_section->link->data + sym->st_name;
                        index = put_elf_sym(s1->dynsym, sym->st_value, sym->st_size, 
                                            sym->st_info, 0, 
                                            sym->st_shndx, name);
                        s1->symtab_to_dynsym[sym - 
                                            (Elf32_Sym *)symtab_section->data] = 
                            index;
                    }
                }
            }

            build_got_entries(s1);
        
            
            for(i = 0; i < s1->nb_loaded_dlls; i++) {
                DLLReference *dllref = s1->loaded_dlls[i];
                if (dllref->level == 0)
                    put_dt(dynamic, DT_NEEDED, put_elf_str(dynstr, dllref->name));
            }
            if (file_type == TCC_OUTPUT_DLL)
                put_dt(dynamic, DT_TEXTREL, 0);

            
            saved_dynamic_data_offset = dynamic->data_offset;
            dynamic->data_offset += 8 * 9;
        } else {
            
            build_got_entries(s1);
        }
    }

    memset(&ehdr, 0, sizeof(ehdr));

    
    strsec = new_section(s1, ".shstrtab", SHT_STRTAB, 0);
    put_elf_str(strsec, "");
    
    
    shnum = s1->nb_sections;

    
    section_order = tcc_malloc(sizeof(int) * shnum);
    section_order[0] = 0;
    sh_order_index = 1;
    
    
    switch(file_type) {
    default:
    case TCC_OUTPUT_OBJ:
        phnum = 0;
        break;
    case TCC_OUTPUT_EXE:
        if (!s1->static_link)
            phnum = 4;
        else
            phnum = 2;
        break;
    case TCC_OUTPUT_DLL:
        phnum = 3;
        break;
    }

    for(i = 1; i < s1->nb_sections; i++) {
        s = s1->sections[i];
        s->sh_name = put_elf_str(strsec, s->name);
        if (file_type == TCC_OUTPUT_DLL && 
            s->sh_type == SHT_REL && 
            !(s->sh_flags & SHF_ALLOC)) {
            prepare_dynamic_rel(s1, s);
        } else if (do_debug || 
            file_type == TCC_OUTPUT_OBJ || 
            (s->sh_flags & SHF_ALLOC) ||
            i == (s1->nb_sections - 1)) {
            
            s->sh_size = s->data_offset;
        }
    }

    
    phdr = tcc_mallocz(phnum * sizeof(Elf32_Phdr));
        
    if (s1->output_format == TCC_OUTPUT_FORMAT_ELF) {
        file_offset = sizeof(Elf32_Ehdr) + phnum * sizeof(Elf32_Phdr);
    } else {
        file_offset = 0;
    }
    if (phnum > 0) {
        
        if (s1->has_text_addr) { 
            int a_offset, p_offset;
            addr = s1->text_addr;
            a_offset = addr & (ELF_PAGE_SIZE - 1);
            p_offset = file_offset & (ELF_PAGE_SIZE - 1);
            if (a_offset < p_offset) 
                a_offset += ELF_PAGE_SIZE;
            file_offset += (a_offset - p_offset);
        } else {
            if (file_type == TCC_OUTPUT_DLL)
                addr = 0;
            else
                addr = ELF_START_ADDR;
            
            addr += (file_offset & (ELF_PAGE_SIZE - 1));
        }
        
        
        rel_size = 0;
        rel_addr = 0;

        
        ph = &phdr[0];
        if (interp)
            ph++;

        for(j = 0; j < 2; j++) {
            ph->p_type = PT_LOAD;
            if (j == 0)
                ph->p_flags = PF_R | PF_X;
            else
                ph->p_flags = PF_R | PF_W;
            ph->p_align = ELF_PAGE_SIZE;
            
            
            for(k = 0; k < 5; k++) {
                for(i = 1; i < s1->nb_sections; i++) {
                    s = s1->sections[i];
                    
                    if (j == 0) {
                        if ((s->sh_flags & (SHF_ALLOC | SHF_WRITE)) != 
                            SHF_ALLOC)
                            continue;
                    } else {
                        if ((s->sh_flags & (SHF_ALLOC | SHF_WRITE)) != 
                            (SHF_ALLOC | SHF_WRITE))
                            continue;
                    }
                    if (s == interp) {
                        if (k != 0)
                            continue;
                    } else if (s->sh_type == SHT_DYNSYM ||
                               s->sh_type == SHT_STRTAB ||
                               s->sh_type == SHT_HASH) {
                        if (k != 1)
                            continue;
                    } else if (s->sh_type == SHT_REL) {
                        if (k != 2)
                            continue;
                    } else if (s->sh_type == SHT_NOBITS) {
                        if (k != 4)
                            continue;
                    } else {
                        if (k != 3)
                            continue;
                    }
                    section_order[sh_order_index++] = i;

                    
                    tmp = addr;
                    addr = (addr + s->sh_addralign - 1) & 
                        ~(s->sh_addralign - 1);
                    file_offset += addr - tmp;
                    s->sh_offset = file_offset;
                    s->sh_addr = addr;
                    
                    
                    if (ph->p_offset == 0) {
                        ph->p_offset = file_offset;
                        ph->p_vaddr = addr;
                        ph->p_paddr = ph->p_vaddr;
                    }
                    
                    if (s->sh_type == SHT_REL) {
                        if (rel_size == 0)
                            rel_addr = addr;
                        rel_size += s->sh_size;
                    }
                    addr += s->sh_size;
                    if (s->sh_type != SHT_NOBITS)
                        file_offset += s->sh_size;
                }
            }
            ph->p_filesz = file_offset - ph->p_offset;
            ph->p_memsz = addr - ph->p_vaddr;
            ph++;
            if (j == 0) {
                if (s1->output_format == TCC_OUTPUT_FORMAT_ELF) {
                    if ((addr & (ELF_PAGE_SIZE - 1)) != 0)
                        addr += ELF_PAGE_SIZE;
                } else {
                    addr = (addr + ELF_PAGE_SIZE - 1) & ~(ELF_PAGE_SIZE - 1);
                    file_offset = (file_offset + ELF_PAGE_SIZE - 1) & 
                        ~(ELF_PAGE_SIZE - 1);
                }
            }
        }

        
        if (interp) {
            ph = &phdr[0];
            
            ph->p_type = PT_INTERP;
            ph->p_offset = interp->sh_offset;
            ph->p_vaddr = interp->sh_addr;
            ph->p_paddr = ph->p_vaddr;
            ph->p_filesz = interp->sh_size;
            ph->p_memsz = interp->sh_size;
            ph->p_flags = PF_R;
            ph->p_align = interp->sh_addralign;
        }
        
        
        if (dynamic) {
            Elf32_Sym *sym_end;

            ph = &phdr[phnum - 1];
            
            ph->p_type = PT_DYNAMIC;
            ph->p_offset = dynamic->sh_offset;
            ph->p_vaddr = dynamic->sh_addr;
            ph->p_paddr = ph->p_vaddr;
            ph->p_filesz = dynamic->sh_size;
            ph->p_memsz = dynamic->sh_size;
            ph->p_flags = PF_R | PF_W;
            ph->p_align = dynamic->sh_addralign;

            
            put32(s1->got->data, dynamic->sh_addr);

            
            if (file_type == TCC_OUTPUT_EXE) {
                uint8_t *p, *p_end;

                p = s1->plt->data;
                p_end = p + s1->plt->data_offset;
                if (p < p_end) {
#if defined(TCC_TARGET_I386)
                    put32(p + 2, get32(p + 2) + s1->got->sh_addr);
                    put32(p + 8, get32(p + 8) + s1->got->sh_addr);
                    p += 16;
                    while (p < p_end) {
                        put32(p + 2, get32(p + 2) + s1->got->sh_addr);
                        p += 16;
                    }
#elif defined(TCC_TARGET_ARM)
		    int x;
		    x=s1->got->sh_addr - s1->plt->sh_addr - 12;
		    p +=16;
		    while (p < p_end) {
		        put32(p + 12, x + get32(p + 12) + s1->plt->data - p);
			p += 16;
		    }
#elif defined(TCC_TARGET_C67)
                    
#else
#error unsupported CPU
#endif
                }
            }

            
            sym_end = (Elf32_Sym *)(s1->dynsym->data + s1->dynsym->data_offset);
            for(sym = (Elf32_Sym *)s1->dynsym->data + 1; 
                sym < sym_end;
                sym++) {
                if (sym->st_shndx == SHN_UNDEF) {
                    if (sym->st_value)
                        sym->st_value += s1->plt->sh_addr;
                } else if (sym->st_shndx < SHN_LORESERVE) {
                    
                    sym->st_value += s1->sections[sym->st_shndx]->sh_addr;
                }
            }

            
            dynamic->data_offset = saved_dynamic_data_offset;
            put_dt(dynamic, DT_HASH, s1->dynsym->hash->sh_addr);
            put_dt(dynamic, DT_STRTAB, dynstr->sh_addr);
            put_dt(dynamic, DT_SYMTAB, s1->dynsym->sh_addr);
            put_dt(dynamic, DT_STRSZ, dynstr->data_offset);
            put_dt(dynamic, DT_SYMENT, sizeof(Elf32_Sym));
            put_dt(dynamic, DT_REL, rel_addr);
            put_dt(dynamic, DT_RELSZ, rel_size);
            put_dt(dynamic, DT_RELENT, sizeof(Elf32_Rel));
            put_dt(dynamic, DT_NULL, 0);
        }

        ehdr.e_phentsize = sizeof(Elf32_Phdr);
        ehdr.e_phnum = phnum;
        ehdr.e_phoff = sizeof(Elf32_Ehdr);
    }

    
    for(i = 1; i < s1->nb_sections; i++) {
        s = s1->sections[i];
        if (phnum > 0 && (s->sh_flags & SHF_ALLOC))
            continue;
        section_order[sh_order_index++] = i;
        
        file_offset = (file_offset + s->sh_addralign - 1) & 
            ~(s->sh_addralign - 1);
        s->sh_offset = file_offset;
        if (s->sh_type != SHT_NOBITS)
            file_offset += s->sh_size;
    }
    
    if (file_type != TCC_OUTPUT_OBJ) {
        relocate_syms(s1, 0);

        if (s1->nb_errors != 0) {
        fail:
            ret = -1;
            goto the_end;
        }

        
        
        for(i = 1; i < s1->nb_sections; i++) {
            s = s1->sections[i];
            if (s->reloc && s != s1->got)
                relocate_section(s1, s);
        }

        for(i = 1; i < s1->nb_sections; i++) {
            s = s1->sections[i];
            if ((s->sh_flags & SHF_ALLOC) &&
                s->sh_type == SHT_REL) {
                relocate_rel(s1, s);
            }
        }

        
        if (file_type == TCC_OUTPUT_EXE)
            ehdr.e_entry = (unsigned long)tcc_get_symbol_err(s1, "_start");
        else
            ehdr.e_entry = text_section->sh_addr; 
    }

    
    if (file_type == TCC_OUTPUT_OBJ)
        mode = 0666;
    else
        mode = 0777;
    fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, mode); 
    if (fd < 0) {
        error_noabort("could not write '%s'", filename);
        goto fail;
    }
    f = fdopen(fd, "wb");

#ifdef TCC_TARGET_COFF
    if (s1->output_format == TCC_OUTPUT_FORMAT_COFF) {
        tcc_output_coff(s1, f);
    } else
#endif
    if (s1->output_format == TCC_OUTPUT_FORMAT_ELF) {
        sort_syms(s1, symtab_section);
        
        
        file_offset = (file_offset + 3) & -4;
    
        
        ehdr.e_ident[0] = ELFMAG0;
        ehdr.e_ident[1] = ELFMAG1;
        ehdr.e_ident[2] = ELFMAG2;
        ehdr.e_ident[3] = ELFMAG3;
        ehdr.e_ident[4] = ELFCLASS32;
        ehdr.e_ident[5] = ELFDATA2LSB;
        ehdr.e_ident[6] = EV_CURRENT;
#ifdef __FreeBSD__
        ehdr.e_ident[EI_OSABI] = ELFOSABI_FREEBSD;
#endif
#ifdef TCC_TARGET_ARM
        ehdr.e_ident[EI_OSABI] = ELFOSABI_ARM;
#endif
        switch(file_type) {
        default:
        case TCC_OUTPUT_EXE:
            ehdr.e_type = ET_EXEC;
            break;
        case TCC_OUTPUT_DLL:
            ehdr.e_type = ET_DYN;
            break;
        case TCC_OUTPUT_OBJ:
            ehdr.e_type = ET_REL;
            break;
        }
        ehdr.e_machine = EM_TCC_TARGET;
        ehdr.e_version = EV_CURRENT;
        ehdr.e_shoff = file_offset;
        ehdr.e_ehsize = sizeof(Elf32_Ehdr);
        ehdr.e_shentsize = sizeof(Elf32_Shdr);
        ehdr.e_shnum = shnum;
        ehdr.e_shstrndx = shnum - 1;
        
        dummy_size_t = fwrite(&ehdr, 1, sizeof(Elf32_Ehdr), f);
        dummy_size_t = fwrite(phdr, 1, phnum * sizeof(Elf32_Phdr), f);
        offset = sizeof(Elf32_Ehdr) + phnum * sizeof(Elf32_Phdr);

        for(i=1;i<s1->nb_sections;i++) {
            s = s1->sections[section_order[i]];
            if (s->sh_type != SHT_NOBITS) {
                while (offset < s->sh_offset) {
                    fputc(0, f);
                    offset++;
                }
                size = s->sh_size;
                dummy_size_t = fwrite(s->data, 1, size, f);
                offset += size;
            }
        }

        
        while (offset < ehdr.e_shoff) {
            fputc(0, f);
            offset++;
        }
    
        for(i=0;i<s1->nb_sections;i++) {
            sh = &shdr;
            memset(sh, 0, sizeof(Elf32_Shdr));
            s = s1->sections[i];
            if (s) {
                sh->sh_name = s->sh_name;
                sh->sh_type = s->sh_type;
                sh->sh_flags = s->sh_flags;
                sh->sh_entsize = s->sh_entsize;
                sh->sh_info = s->sh_info;
                if (s->link)
                    sh->sh_link = s->link->sh_num;
                sh->sh_addralign = s->sh_addralign;
                sh->sh_addr = s->sh_addr;
                sh->sh_offset = s->sh_offset;
                sh->sh_size = s->sh_size;
            }
            dummy_size_t = fwrite(sh, 1, sizeof(Elf32_Shdr), f);
        }
    } else {
        tcc_output_binary(s1, f, section_order);
    }
    fclose(f);

    ret = 0;
 the_end:
    tcc_free(s1->symtab_to_dynsym);
    tcc_free(section_order);
    tcc_free(phdr);
    tcc_free(s1->got_offsets);
    return ret;
}

static void *load_data(int fd, unsigned long file_offset, unsigned long size)
{
    void *data;

    data = tcc_malloc(size);
    lseek(fd, file_offset, SEEK_SET);
    dummy_size_t = read(fd, data, size);
    return data;
}

typedef struct SectionMergeInfo {
    Section *s;            
    unsigned long offset;  
    uint8_t new_section;       
    uint8_t link_once;         
} SectionMergeInfo;

static int tcc_load_object_file(TCCState *s1, 
                                int fd, unsigned long file_offset)
{ 
    Elf32_Ehdr ehdr;
    Elf32_Shdr *shdr, *sh;
    int size, i, j, offset, offseti, nb_syms, sym_index, ret;
    unsigned char *strsec, *strtab;
    int *old_to_new_syms;
    char *sh_name, *name;
    SectionMergeInfo *sm_table, *sm;
    Elf32_Sym *sym, *symtab;
    Elf32_Rel *rel, *rel_end;
    Section *s;

    if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr))
        goto fail1;
    if (ehdr.e_ident[0] != ELFMAG0 ||
        ehdr.e_ident[1] != ELFMAG1 ||
        ehdr.e_ident[2] != ELFMAG2 ||
        ehdr.e_ident[3] != ELFMAG3)
        goto fail1;
    
    if (ehdr.e_type != ET_REL)
        goto fail1;
    
    if (ehdr.e_ident[5] != ELFDATA2LSB ||
        ehdr.e_machine != EM_TCC_TARGET) {
    fail1:
        error_noabort("invalid object file");
        return -1;
    }
    
    shdr = load_data(fd, file_offset + ehdr.e_shoff, 
                     sizeof(Elf32_Shdr) * ehdr.e_shnum);
    sm_table = tcc_mallocz(sizeof(SectionMergeInfo) * ehdr.e_shnum);
    
    
    sh = &shdr[ehdr.e_shstrndx];
    strsec = load_data(fd, file_offset + sh->sh_offset, sh->sh_size);

    
    old_to_new_syms = NULL;
    symtab = NULL;
    strtab = NULL;
    nb_syms = 0;
    for(i = 1; i < ehdr.e_shnum; i++) {
        sh = &shdr[i];
        if (sh->sh_type == SHT_SYMTAB) {
            if (symtab) {
                error_noabort("object must contain only one symtab");
            fail:
                ret = -1;
                goto the_end;
            }
            nb_syms = sh->sh_size / sizeof(Elf32_Sym);
            symtab = load_data(fd, file_offset + sh->sh_offset, sh->sh_size);
            sm_table[i].s = symtab_section;

            
            sh = &shdr[sh->sh_link];
            strtab = load_data(fd, file_offset + sh->sh_offset, sh->sh_size);
        }
    }
        
    for(i = 1; i < ehdr.e_shnum; i++) {
        
        if (i == ehdr.e_shstrndx)
            continue;
        sh = &shdr[i];
        sh_name = strsec + sh->sh_name;
        
        if (sh->sh_type != SHT_PROGBITS &&
            sh->sh_type != SHT_REL && 
            sh->sh_type != SHT_NOBITS)
            continue;
        if (sh->sh_addralign < 1)
            sh->sh_addralign = 1;
        
        for(j = 1; j < s1->nb_sections;j++) {
            s = s1->sections[j];
            if (!strcmp(s->name, sh_name)) {
                if (!strncmp(sh_name, ".gnu.linkonce", 
                             sizeof(".gnu.linkonce") - 1)) {
                    sm_table[i].link_once = 1;
                    goto next;
                } else {
                    goto found;
                }
            }
        }
        
        s = new_section(s1, sh_name, sh->sh_type, sh->sh_flags);
        s->sh_addralign = sh->sh_addralign;
        s->sh_entsize = sh->sh_entsize;
        sm_table[i].new_section = 1;
    found:
        if (sh->sh_type != s->sh_type) {
            error_noabort("invalid section type");
            goto fail;
        }

        
        offset = s->data_offset;
        size = sh->sh_addralign - 1;
        offset = (offset + size) & ~size;
        if (sh->sh_addralign > s->sh_addralign)
            s->sh_addralign = sh->sh_addralign;
        s->data_offset = offset;
        sm_table[i].offset = offset;
        sm_table[i].s = s;
        
        size = sh->sh_size;
        if (sh->sh_type != SHT_NOBITS) {
            unsigned char *ptr;
            lseek(fd, file_offset + sh->sh_offset, SEEK_SET);
            ptr = section_ptr_add(s, size);
            dummy_size_t = read(fd, ptr, size);
        } else {
            s->data_offset += size;
        }
    next: ;
    }

    sm = sm_table;
    for(i = 1; i < ehdr.e_shnum; i++) {
        s = sm_table[i].s;
        if (!s || !sm_table[i].new_section)
            continue;
        sh = &shdr[i];
        if (sh->sh_link > 0)
            s->link = sm_table[sh->sh_link].s;
        if (sh->sh_type == SHT_REL) {
            s->sh_info = sm_table[sh->sh_info].s->sh_num;
            
            s1->sections[s->sh_info]->reloc = s;
        }
    }

    
    old_to_new_syms = tcc_mallocz(nb_syms * sizeof(int));

    sym = symtab + 1;
    for(i = 1; i < nb_syms; i++, sym++) {
        if (sym->st_shndx != SHN_UNDEF &&
            sym->st_shndx < SHN_LORESERVE) {
            sm = &sm_table[sym->st_shndx];
            if (sm->link_once) {
                if (ELF32_ST_BIND(sym->st_info) != STB_LOCAL) {
                    name = strtab + sym->st_name;
                    sym_index = find_elf_sym(symtab_section, name);
                    if (sym_index)
                        old_to_new_syms[i] = sym_index;
                }
                continue;
            }
            
            if (!sm->s)
                continue;
            
            sym->st_shndx = sm->s->sh_num;
            
            sym->st_value += sm->offset;
        }
        
        name = strtab + sym->st_name;
        sym_index = add_elf_sym(symtab_section, sym->st_value, sym->st_size, 
                                sym->st_info, sym->st_other, 
                                sym->st_shndx, name);
        old_to_new_syms[i] = sym_index;
    }

    
    for(i = 1; i < ehdr.e_shnum; i++) {
        s = sm_table[i].s;
        if (!s)
            continue;
        sh = &shdr[i];
        offset = sm_table[i].offset;
        switch(s->sh_type) {
        case SHT_REL:
            
            offseti = sm_table[sh->sh_info].offset;
            rel_end = (Elf32_Rel *)(s->data + s->data_offset);
            for(rel = (Elf32_Rel *)(s->data + offset);
                rel < rel_end;
                rel++) {
                int type;
                unsigned sym_index;
                
                type = ELF32_R_TYPE(rel->r_info);
                sym_index = ELF32_R_SYM(rel->r_info);
                
                if (sym_index >= nb_syms)
                    goto invalid_reloc;
                sym_index = old_to_new_syms[sym_index];
                if (!sym_index) {
                invalid_reloc:
                    error_noabort("Invalid relocation entry");
                    goto fail;
                }
                rel->r_info = ELF32_R_INFO(sym_index, type);
                
                rel->r_offset += offseti;
            }
            break;
        default:
            break;
        }
    }
    
    ret = 0;
 the_end:
    tcc_free(symtab);
    tcc_free(strtab);
    tcc_free(old_to_new_syms);
    tcc_free(sm_table);
    tcc_free(strsec);
    tcc_free(shdr);
    return ret;
}

#define ARMAG  "!<arch>\012"	

typedef struct ArchiveHeader {
    char ar_name[16];		
    char ar_date[12];		
    char ar_uid[6];		
    char ar_gid[6];		
    char ar_mode[8];		
    char ar_size[10];		
    char ar_fmag[2];		
} ArchiveHeader;

static int get_be32(const uint8_t *b)
{
    return b[3] | (b[2] << 8) | (b[1] << 16) | (b[0] << 24);
}

static int tcc_load_alacarte(TCCState *s1, int fd, int size)
{
    int i, bound, nsyms, sym_index, off, ret;
    uint8_t *data;
    const char *ar_names, *p;
    const uint8_t *ar_index;
    Elf32_Sym *sym;

    data = tcc_malloc(size);
    if (read(fd, data, size) != size)
        goto fail;
    nsyms = get_be32(data);
    ar_index = data + 4;
    ar_names = ar_index + nsyms * 4;

    do {
	bound = 0;
	for(p = ar_names, i = 0; i < nsyms; i++, p += strlen(p)+1) {
	    sym_index = find_elf_sym(symtab_section, p);
	    if(sym_index) {
		sym = &((Elf32_Sym *)symtab_section->data)[sym_index];
		if(sym->st_shndx == SHN_UNDEF) {
		    off = get_be32(ar_index + i * 4) + sizeof(ArchiveHeader);
#if 0
		    printf("%5d\t%s\t%08x\n", i, p, sym->st_shndx);
#endif
		    ++bound;
		    lseek(fd, off, SEEK_SET);
		    if(tcc_load_object_file(s1, fd, off) < 0) {
                    fail:
                        ret = -1;
                        goto the_end;
                    }
		}
	    }
	}
    } while(bound);
    ret = 0;
 the_end:
    tcc_free(data);
    return ret;
}

static int tcc_load_archive(TCCState *s1, int fd)
{
    ArchiveHeader hdr;
    char ar_size[11];
    char ar_name[17];
    char magic[8];
    int size, len, i;
    unsigned long file_offset;

    
    dummy_size_t = read(fd, magic, sizeof(magic));
    
    for(;;) {
        len = read(fd, &hdr, sizeof(hdr));
        if (len == 0)
            break;
        if (len != sizeof(hdr)) {
            error_noabort("invalid archive");
            return -1;
        }
        memcpy(ar_size, hdr.ar_size, sizeof(hdr.ar_size));
        ar_size[sizeof(hdr.ar_size)] = '\0';
        size = strtol(ar_size, NULL, 0);
        memcpy(ar_name, hdr.ar_name, sizeof(hdr.ar_name));
        for(i = sizeof(hdr.ar_name) - 1; i >= 0; i--) {
            if (ar_name[i] != ' ')
                break;
        }
        ar_name[i + 1] = '\0';
        
        file_offset = lseek(fd, 0, SEEK_CUR);
        
        size = (size + 1) & ~1;
        if (!strcmp(ar_name, "/")) {
            
	    if(s1->alacarte_link)
		return tcc_load_alacarte(s1, fd, size);
        } else if (!strcmp(ar_name, "//") ||
                   !strcmp(ar_name, "__.SYMDEF") ||
                   !strcmp(ar_name, "__.SYMDEF/") ||
                   !strcmp(ar_name, "ARFILENAMES/")) {
            
        } else {
            if (tcc_load_object_file(s1, fd, file_offset) < 0)
                return -1;
        }
        lseek(fd, file_offset + size, SEEK_SET);
    }
    return 0;
}

static int tcc_load_dll(TCCState *s1, int fd, const char *filename, int level)
{ 
    Elf32_Ehdr ehdr;
    Elf32_Shdr *shdr, *sh, *sh1;
    int i, nb_syms, nb_dts, sym_bind, ret;
    Elf32_Sym *sym, *dynsym;
    Elf32_Dyn *dt, *dynamic;
    unsigned char *dynstr;
    const char *name, *soname, *p;
    DLLReference *dllref;
    
    dummy_size_t = read(fd, &ehdr, sizeof(ehdr));

    
    if (ehdr.e_ident[5] != ELFDATA2LSB ||
        ehdr.e_machine != EM_TCC_TARGET) {
        error_noabort("bad architecture");
        return -1;
    }

    
    shdr = load_data(fd, ehdr.e_shoff, sizeof(Elf32_Shdr) * ehdr.e_shnum);

    
    nb_syms = 0;
    nb_dts = 0;
    dynamic = NULL;
    dynsym = NULL; 
    dynstr = NULL; 
    for(i = 0, sh = shdr; i < ehdr.e_shnum; i++, sh++) {
        switch(sh->sh_type) {
        case SHT_DYNAMIC:
            nb_dts = sh->sh_size / sizeof(Elf32_Dyn);
            dynamic = load_data(fd, sh->sh_offset, sh->sh_size);
            break;
        case SHT_DYNSYM:
            nb_syms = sh->sh_size / sizeof(Elf32_Sym);
            dynsym = load_data(fd, sh->sh_offset, sh->sh_size);
            sh1 = &shdr[sh->sh_link];
            dynstr = load_data(fd, sh1->sh_offset, sh1->sh_size);
            break;
        default:
            break;
        }
    }
    
    
    soname = filename;
    p = strrchr(soname, '/');
    if (p)
        soname = p + 1;
        
    for(i = 0, dt = dynamic; i < nb_dts; i++, dt++) {
        if (dt->d_tag == DT_SONAME) {
            soname = dynstr + dt->d_un.d_val;
        }
    }

    
    for(i = 0; i < s1->nb_loaded_dlls; i++) {
        dllref = s1->loaded_dlls[i];
        if (!strcmp(soname, dllref->name)) {
            
            if (level < dllref->level)
                dllref->level = level;
            ret = 0;
            goto the_end;
        }
    }
    
    

    
    dllref = tcc_malloc(sizeof(DLLReference) + strlen(soname));
    dllref->level = level;
    strcpy(dllref->name, soname);
    dynarray_add((void ***)&s1->loaded_dlls, &s1->nb_loaded_dlls, dllref);

    
    for(i = 1, sym = dynsym + 1; i < nb_syms; i++, sym++) {
        sym_bind = ELF32_ST_BIND(sym->st_info);
        if (sym_bind == STB_LOCAL)
            continue;
        name = dynstr + sym->st_name;
        add_elf_sym(s1->dynsymtab_section, sym->st_value, sym->st_size,
                    sym->st_info, sym->st_other, sym->st_shndx, name);
    }

    
    for(i = 0, dt = dynamic; i < nb_dts; i++, dt++) {
        switch(dt->d_tag) {
        case DT_NEEDED:
            name = dynstr + dt->d_un.d_val;
            for(i = 0; i < s1->nb_loaded_dlls; i++) {
                dllref = s1->loaded_dlls[i];
                if (!strcmp(name, dllref->name))
                    goto already_loaded;
            }
            if (tcc_add_dll(s1, name, AFF_REFERENCED_DLL) < 0) {
                error_noabort("referenced dll '%s' not found", name);
                ret = -1;
                goto the_end;
            }
        already_loaded:
            break;
        }
    }
    ret = 0;
 the_end:
    tcc_free(dynstr);
    tcc_free(dynsym);
    tcc_free(dynamic);
    tcc_free(shdr);
    return ret;
}

#define LD_TOK_NAME 256
#define LD_TOK_EOF  (-1)

static int ld_next(TCCState *s1, char *name, int name_size)
{
    int c;
    char *q;

 redo:
    switch(ch) {
    case ' ':
    case '\t':
    case '\f':
    case '\v':
    case '\r':
    case '\n':
        inp();
        goto redo;
    case '/':
        minp();
        if (ch == '*') {
            file->buf_ptr = parse_comment(file->buf_ptr);
            ch = file->buf_ptr[0];
            goto redo;
        } else {
            q = name;
            *q++ = '/';
            goto parse_name;
        }
        break;
    case 'a' ... 'z':
    case 'A' ... 'Z':
    case '_':
    case '\\':
    case '.':
    case '$':
    case '~':
        q = name;
    parse_name:
        for(;;) {
            if (!((ch >= 'a' && ch <= 'z') ||
                  (ch >= 'A' && ch <= 'Z') ||
                  (ch >= '0' && ch <= '9') ||
                  strchr("/.-_+=$:\\,~", ch)))
                break;
            if ((q - name) < name_size - 1) {
                *q++ = ch;
            }
            minp();
        }
        *q = '\0';
        c = LD_TOK_NAME;
        break;
    case CH_EOF:
        c = LD_TOK_EOF;
        break;
    default:
        c = ch;
        inp();
        break;
    }
#if 0
    printf("tok=%c %d\n", c, c);
    if (c == LD_TOK_NAME)
        printf("  name=%s\n", name);
#endif
    return c;
}

static int tcc_load_ldscript(TCCState *s1)
{
    char cmd[64];
    char filename[1024];
    int t;
    
    ch = file->buf_ptr[0];
    ch = handle_eob();
    for(;;) {
        t = ld_next(s1, cmd, sizeof(cmd));
        if (t == LD_TOK_EOF)
            return 0;
        else if (t != LD_TOK_NAME)
            return -1;
        if (!strcmp(cmd, "INPUT") ||
            !strcmp(cmd, "GROUP")) {
            t = ld_next(s1, cmd, sizeof(cmd));
            if (t != '(')
                expect("(");
            t = ld_next(s1, filename, sizeof(filename));
            for(;;) {
                if (t == LD_TOK_EOF) {
                    error_noabort("unexpected end of file");
                    return -1;
                } else if (t == ')') {
                    break;
                } else if (t != LD_TOK_NAME) {
                    error_noabort("filename expected");
                    return -1;
                } 
                tcc_add_file(s1, filename);
                t = ld_next(s1, filename, sizeof(filename));
                if (t == ',') {
                    t = ld_next(s1, filename, sizeof(filename));
                }
            }
        } else if (!strcmp(cmd, "OUTPUT_FORMAT") ||
                   !strcmp(cmd, "TARGET")) {
            
            t = ld_next(s1, cmd, sizeof(cmd));
            if (t != '(')
                expect("(");
            for(;;) {
                t = ld_next(s1, filename, sizeof(filename));
                if (t == LD_TOK_EOF) {
                    error_noabort("unexpected end of file");
                    return -1;
                } else if (t == ')') {
                    break;
                }
            }
        } else {
            return -1;
        }
    }
    return 0;
}

#ifdef TCC_TARGET_COFF
#include "tcccoff.c"
#endif

#ifdef TCC_TARGET_PE
#include "tccpe.c"
#endif

static void rt_printline(unsigned long wanted_pc)
{
    Stab_Sym *sym, *sym_end;
    char func_name[128], last_func_name[128];
    unsigned long func_addr, last_pc, pc;
    const char *incl_files[INCLUDE_STACK_SIZE];
    int incl_index, len, last_line_num, i;
    const char *str, *p;

    fprintf(stderr, "0x%08lx:", wanted_pc);

    func_name[0] = '\0';
    func_addr = 0;
    incl_index = 0;
    last_func_name[0] = '\0';
    last_pc = 0xffffffff;
    last_line_num = 1;
    sym = (Stab_Sym *)stab_section->data + 1;
    sym_end = (Stab_Sym *)(stab_section->data + stab_section->data_offset);
    while (sym < sym_end) {
        switch(sym->n_type) {
            
        case N_FUN:
            if (sym->n_strx == 0) {
                
                pc = sym->n_value + func_addr;
                if (wanted_pc >= last_pc && wanted_pc < pc)
                    goto found;
                func_name[0] = '\0';
                func_addr = 0;
            } else {
                str = stabstr_section->data + sym->n_strx;
                p = strchr(str, ':');
                if (!p) {
                    pstrcpy(func_name, sizeof(func_name), str);
                } else {
                    len = p - str;
                    if (len > sizeof(func_name) - 1)
                        len = sizeof(func_name) - 1;
                    memcpy(func_name, str, len);
                    func_name[len] = '\0';
                }
                func_addr = sym->n_value;
            }
            break;
            
        case N_SLINE:
            pc = sym->n_value + func_addr;
            if (wanted_pc >= last_pc && wanted_pc < pc)
                goto found;
            last_pc = pc;
            last_line_num = sym->n_desc;
            
            strcpy(last_func_name, func_name);
            break;
            
        case N_BINCL:
            str = stabstr_section->data + sym->n_strx;
        add_incl:
            if (incl_index < INCLUDE_STACK_SIZE) {
                incl_files[incl_index++] = str;
            }
            break;
        case N_EINCL:
            if (incl_index > 1)
                incl_index--;
            break;
        case N_SO:
            if (sym->n_strx == 0) {
                incl_index = 0; 
            } else {
                str = stabstr_section->data + sym->n_strx;
                
                len = strlen(str);
                if (len > 0 && str[len - 1] != '/')
                    goto add_incl;
            }
            break;
        }
        sym++;
    }

    
    incl_index = 0;
    {
        Elf32_Sym *sym, *sym_end;
        int type;

        sym_end = (Elf32_Sym *)(symtab_section->data + symtab_section->data_offset);
        for(sym = (Elf32_Sym *)symtab_section->data + 1; 
            sym < sym_end;
            sym++) {
            type = ELF32_ST_TYPE(sym->st_info);
            if (type == STT_FUNC) {
                if (wanted_pc >= sym->st_value &&
                    wanted_pc < sym->st_value + sym->st_size) {
                    pstrcpy(last_func_name, sizeof(last_func_name),
                            strtab_section->data + sym->st_name);
                    goto found;
                }
            }
        }
    }
    
    fprintf(stderr, " ???\n");
    return;
 found:
    if (last_func_name[0] != '\0') {
        fprintf(stderr, " %s()", last_func_name);
    }
    if (incl_index > 0) {
        fprintf(stderr, " (%s:%d", 
                incl_files[incl_index - 1], last_line_num);
        for(i = incl_index - 2; i >= 0; i--)
            fprintf(stderr, ", included from %s", incl_files[i]);
        fprintf(stderr, ")");
    }
    fprintf(stderr, "\n");
}

#if !defined(WIN32) && !defined(CONFIG_TCCBOOT)

static int rt_get_caller_pc(unsigned long *paddr, 
                            ucontext_t *uc, int level)
{
    unsigned long fp __attribute__((unused));
    

    if (level == 0) {
        *paddr = 12345; 
        return 0;
    } else {
        fp = 23456; 
        return 0;
    }
}

void rt_error(ucontext_t *uc, const char *fmt, ...)
{
    va_list ap;
    unsigned long pc = 0;  
    int i;

    va_start(ap, fmt);
    fprintf(stderr, "Runtime error: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    for(i=0;i<num_callers;i++) {
        if (rt_get_caller_pc(&pc, uc, i) < 0)
            break;
        if (i == 0)
            fprintf(stderr, "at ");
        else
            fprintf(stderr, "by ");
        rt_printline(pc);
    }
    exit(255);
    va_end(ap);
}

static void sig_error(int signum, siginfo_t *siginf, void *puc)
{
    ucontext_t *uc = puc;

    switch(signum) {
    case SIGFPE:
        switch(siginf->si_code) {
        case FPE_INTDIV:
        case FPE_FLTDIV:
            rt_error(uc, "division by zero");
            break;
        default:
            rt_error(uc, "floating point exception");
            break;
        }
        break;
    case SIGBUS:
    case SIGSEGV:
        if (rt_bound_error_msg && *rt_bound_error_msg)
            rt_error(uc, *rt_bound_error_msg);
        else
            rt_error(uc, "dereferencing invalid pointer");
        break;
    case SIGILL:
        rt_error(uc, "illegal instruction");
        break;
    case SIGABRT:
        rt_error(uc, "abort() called");
        break;
    default:
        rt_error(uc, "caught signal %d", signum);
        break;
    }
    exit(255);
}
#endif

int tcc_relocate(TCCState *s1)
{
    Section *s;
    int i;

    s1->nb_errors = 0;
    
#ifdef TCC_TARGET_PE
    pe_add_runtime(s1);
#else
    tcc_add_runtime(s1);
#endif

    relocate_common_syms();

    tcc_add_linker_symbols(s1);

    build_got_entries(s1);
    
    for(i = 1; i < s1->nb_sections; i++) {
        s = s1->sections[i];
        if (s->sh_flags & SHF_ALLOC) {
            if (s->sh_type == SHT_NOBITS)
                s->data = tcc_mallocz(s->data_offset);
            s->sh_addr = (unsigned long)s->data;
        }
    }

    relocate_syms(s1, 1);

    if (s1->nb_errors != 0)
        return -1;

    
    for(i = 1; i < s1->nb_sections; i++) {
        s = s1->sections[i];
        if (s->reloc)
            relocate_section(s1, s);
    }
    return 0;
}

int tcc_run(TCCState *s1, int argc, char **argv)
{
    int (*prog_main)(int, char **);

    if (tcc_relocate(s1) < 0)
        return -1;

    prog_main = tcc_get_symbol_err(s1, "main");
    
    if (do_debug) {
#if defined(WIN32) || defined(CONFIG_TCCBOOT)
        error("debug mode currently not available for Windows");
#else        
        struct sigaction sigact;
        sigact.sa_flags = SA_SIGINFO | SA_RESETHAND;
        sigact.sa_sigaction = sig_error;
        sigemptyset(&sigact.sa_mask);
        sigaction(SIGFPE, &sigact, NULL);
        sigaction(SIGILL, &sigact, NULL);
        sigaction(SIGSEGV, &sigact, NULL);
        sigaction(SIGBUS, &sigact, NULL);
        sigaction(SIGABRT, &sigact, NULL);
#endif
    }

#ifdef CONFIG_TCC_BCHECK
    if (do_bounds_check) {
        void (*bound_init)(void);

        
        rt_bound_error_msg = (void *)tcc_get_symbol_err(s1, 
                                                        "__bound_error_msg");

        
        bound_init = (void *)tcc_get_symbol_err(s1, "__bound_init");
        bound_init();
    }
#endif
    return (*prog_main)(argc, argv);
}

TCCState *tcc_new(void)
{
    const char *p, *r;
    TCCState *s;
    TokenSym *ts __attribute__((unused));
    int i, c;

    s = tcc_mallocz(sizeof(TCCState));
    if (!s)
        return NULL;
    tcc_state = s;
    s->output_type = TCC_OUTPUT_MEMORY;

    
    for(i=0;i<256;i++)
        isidnum_table[i] = isid(i) || isnum(i);

    
    table_ident = NULL;
    memset(hash_ident, 0, TOK_HASH_SIZE * sizeof(TokenSym *));
    
    tok_ident = TOK_IDENT;
    p = tcc_keywords;
    while (*p) {
        r = p;
        for(;;) {
            c = *r++;
            if (c == '\0')
                break;
        }
        ts = tok_alloc(p, r - p - 1);
        p = r;
    }

    define_push(TOK___LINE__, MACRO_OBJ, NULL, NULL);
    define_push(TOK___FILE__, MACRO_OBJ, NULL, NULL);
    define_push(TOK___DATE__, MACRO_OBJ, NULL, NULL);
    define_push(TOK___TIME__, MACRO_OBJ, NULL, NULL);

    
    tcc_define_symbol(s, "__STDC__", NULL);
#if defined(TCC_TARGET_I386)
    tcc_define_symbol(s, "__i386__", NULL);
#endif
#if defined(TCC_TARGET_ARM)
    tcc_define_symbol(s, "__ARM_ARCH_4__", NULL);
    tcc_define_symbol(s, "__arm_elf__", NULL);
    tcc_define_symbol(s, "__arm_elf", NULL);
    tcc_define_symbol(s, "arm_elf", NULL);
    tcc_define_symbol(s, "__arm__", NULL);
    tcc_define_symbol(s, "__arm", NULL);
    tcc_define_symbol(s, "arm", NULL);
    tcc_define_symbol(s, "__APCS_32__", NULL);
#endif
#if defined(linux)
    tcc_define_symbol(s, "__linux__", NULL);
    tcc_define_symbol(s, "linux", NULL);
#endif
    
    tcc_define_symbol(s, "__TINYC__", NULL);

    
    tcc_define_symbol(s, "__SIZE_TYPE__", "unsigned int");
    tcc_define_symbol(s, "__PTRDIFF_TYPE__", "int");
    tcc_define_symbol(s, "__WCHAR_TYPE__", "int");
    
    
#ifdef TCC_TARGET_PE
    {
        char buf[1024];
        snprintf(buf, sizeof(buf), "%s/lib", tcc_lib_path);
        tcc_add_library_path(s, buf);
    }
#else
    tcc_add_library_path(s, "/usr/local/lib");
    tcc_add_library_path(s, "/usr/lib");
    tcc_add_library_path(s, "/lib");
#endif

    
    dynarray_add((void ***)&s->sections, &s->nb_sections, NULL);

    
    text_section = new_section(s, ".text", SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR);
    data_section = new_section(s, ".data", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE);
    bss_section = new_section(s, ".bss", SHT_NOBITS, SHF_ALLOC | SHF_WRITE);

    
    symtab_section = new_symtab(s, ".symtab", SHT_SYMTAB, 0,
                                ".strtab",
                                ".hashtab", SHF_PRIVATE); 
    strtab_section = symtab_section->link;
    
    
    s->dynsymtab_section = new_symtab(s, ".dynsymtab", SHT_SYMTAB, SHF_PRIVATE,
                                      ".dynstrtab", 
                                      ".dynhashtab", SHF_PRIVATE);
    s->alacarte_link = 1;

#ifdef CHAR_IS_UNSIGNED
    s->char_is_unsigned = 1;
#endif
#if defined(TCC_TARGET_PE) && 0
    
    s->leading_underscore = 1;
#endif
    return s;
}

void tcc_delete(TCCState *s1)
{
    int i, n;

    
    free_defines(NULL);

    
    n = tok_ident - TOK_IDENT;
    for(i = 0; i < n; i++)
        tcc_free(table_ident[i]);
    tcc_free(table_ident);

    

    free_section(symtab_section->hash);

    free_section(s1->dynsymtab_section->hash);
    free_section(s1->dynsymtab_section->link);
    free_section(s1->dynsymtab_section);

    for(i = 1; i < s1->nb_sections; i++)
        free_section(s1->sections[i]);
    tcc_free(s1->sections);
    
    
    for(i = 0; i < s1->nb_loaded_dlls; i++)
        tcc_free(s1->loaded_dlls[i]);
    tcc_free(s1->loaded_dlls);

    
    for(i = 0; i < s1->nb_library_paths; i++)
        tcc_free(s1->library_paths[i]);
    tcc_free(s1->library_paths);

    
    for(i = 0; i < s1->nb_cached_includes; i++)
        tcc_free(s1->cached_includes[i]);
    tcc_free(s1->cached_includes);

    for(i = 0; i < s1->nb_include_paths; i++)
        tcc_free(s1->include_paths[i]);
    tcc_free(s1->include_paths);

    for(i = 0; i < s1->nb_sysinclude_paths; i++)
        tcc_free(s1->sysinclude_paths[i]);
    tcc_free(s1->sysinclude_paths);

    tcc_free(s1);
}

int tcc_add_include_path(TCCState *s1, const char *pathname)
{
    char *pathname1;
    
    pathname1 = tcc_strdup(pathname);
    dynarray_add((void ***)&s1->include_paths, &s1->nb_include_paths, pathname1);
    return 0;
}

int tcc_add_sysinclude_path(TCCState *s1, const char *pathname)
{
    char *pathname1;
    
    pathname1 = tcc_strdup(pathname);
    dynarray_add((void ***)&s1->sysinclude_paths, &s1->nb_sysinclude_paths, pathname1);
    return 0;
}

static int tcc_add_file_internal(TCCState *s1, const char *filename, int flags)
{
    const char *ext, *filename1;
    Elf32_Ehdr ehdr;
    int fd, ret;
    BufferedFile *saved_file;

    
    filename1 = strrchr(filename, '/');
    if (filename1)
        filename1++;
    else
        filename1 = filename;
    ext = strrchr(filename1, '.');
    if (ext)
        ext++;

    
    saved_file = file;
    file = tcc_open(s1, filename);
    if (!file) {
        if (flags & AFF_PRINT_ERROR) {
            error_noabort("file '%s' not found", filename);
        }
        ret = -1;
        goto fail1;
    }

    if (!ext || !strcmp(ext, "c")) {
        
        ret = tcc_compile(s1);
    } else 
#ifdef CONFIG_TCC_ASM
    if (!strcmp(ext, "S")) {
        
        ret = tcc_assemble(s1, 1);
    } else if (!strcmp(ext, "s")) {
        
        ret = tcc_assemble(s1, 0);
    } else 
#endif
#ifdef TCC_TARGET_PE
    if (!strcmp(ext, "def")) {
        ret = pe_load_def_file(s1, fdopen(file->fd, "rb"));
    } else
#endif
    {
        fd = file->fd;
        
        ret = read(fd, &ehdr, sizeof(ehdr));
        lseek(fd, 0, SEEK_SET);
        if (ret <= 0) {
            error_noabort("could not read header");
            goto fail;
        } else if (ret != sizeof(ehdr)) {
            goto try_load_script;
        }

        if (ehdr.e_ident[0] == ELFMAG0 &&
            ehdr.e_ident[1] == ELFMAG1 &&
            ehdr.e_ident[2] == ELFMAG2 &&
            ehdr.e_ident[3] == ELFMAG3) {
            file->line_num = 0; 
            if (ehdr.e_type == ET_REL) {
                ret = tcc_load_object_file(s1, fd, 0);
            } else if (ehdr.e_type == ET_DYN) {
                if (s1->output_type == TCC_OUTPUT_MEMORY) {
#ifdef TCC_TARGET_PE
                    ret = -1;
#else
                    void *h;
		    assert(0);
		    h = 0;
                    
		    
                    if (h)
                        ret = 0;
                    else
                        ret = -1;
#endif
                } else {
                    ret = tcc_load_dll(s1, fd, filename, 
                                       (flags & AFF_REFERENCED_DLL) != 0);
                }
            } else {
                error_noabort("unrecognized ELF file");
                goto fail;
            }
        } else if (memcmp((char *)&ehdr, ARMAG, 8) == 0) {
            file->line_num = 0; 
            ret = tcc_load_archive(s1, fd);
        } else 
#ifdef TCC_TARGET_COFF
        if (*(uint16_t *)(&ehdr) == COFF_C67_MAGIC) {
            ret = tcc_load_coff(s1, fd);
        } else
#endif
        {
            
        try_load_script:
            ret = tcc_load_ldscript(s1);
            if (ret < 0) {
                error_noabort("unrecognized file type");
                goto fail;
            }
        }
    }
 the_end:
    tcc_close(file);
 fail1:
    file = saved_file;
    return ret;
 fail:
    ret = -1;
    goto the_end;
}

int tcc_add_file(TCCState *s, const char *filename)
{
    return tcc_add_file_internal(s, filename, AFF_PRINT_ERROR);
}

int tcc_add_library_path(TCCState *s, const char *pathname)
{
    char *pathname1;
    
    pathname1 = tcc_strdup(pathname);
    dynarray_add((void ***)&s->library_paths, &s->nb_library_paths, pathname1);
    return 0;
}

static int tcc_add_dll(TCCState *s, const char *filename, int flags)
{
    char buf[1024];
    int i;

    for(i = 0; i < s->nb_library_paths; i++) {
        snprintf(buf, sizeof(buf), "%s/%s", 
                 s->library_paths[i], filename);
        if (tcc_add_file_internal(s, buf, flags) == 0)
            return 0;
    }
    return -1;
}

int tcc_add_library(TCCState *s, const char *libraryname)
{
    char buf[1024];
    int i;
    
    
    if (!s->static_link) {
#ifdef TCC_TARGET_PE
        snprintf(buf, sizeof(buf), "%s.def", libraryname);
#else
        snprintf(buf, sizeof(buf), "lib%s.so", libraryname);
#endif
        if (tcc_add_dll(s, buf, 0) == 0)
            return 0;
    }

    
    for(i = 0; i < s->nb_library_paths; i++) {
        snprintf(buf, sizeof(buf), "%s/lib%s.a", 
                 s->library_paths[i], libraryname);
        if (tcc_add_file_internal(s, buf, 0) == 0)
            return 0;
    }
    return -1;
}

int tcc_add_symbol(TCCState *s, const char *name, unsigned long val)
{
    add_elf_sym(symtab_section, val, 0, 
                ELF32_ST_INFO(STB_GLOBAL, STT_NOTYPE), 0,
                SHN_ABS, name);
    return 0;
}

int tcc_set_output_type(TCCState *s, int output_type)
{
    s->output_type = output_type;

    if (!s->nostdinc) {
        char buf[1024];

        
        
#ifndef TCC_TARGET_PE
        tcc_add_sysinclude_path(s, "/usr/local/include");
        tcc_add_sysinclude_path(s, "/usr/include");
#endif
        snprintf(buf, sizeof(buf), "%s/include", tcc_lib_path);
        tcc_add_sysinclude_path(s, buf);
#ifdef TCC_TARGET_PE
        snprintf(buf, sizeof(buf), "%s/include/winapi", tcc_lib_path);
        tcc_add_sysinclude_path(s, buf);
#endif
    }

    
#ifdef CONFIG_TCC_BCHECK
    if (do_bounds_check) {
        
        tcc_define_symbol(s, "__BOUNDS_CHECKING_ON", NULL);
        
        bounds_section = new_section(s, ".bounds", 
                                     SHT_PROGBITS, SHF_ALLOC);
        lbounds_section = new_section(s, ".lbounds", 
                                      SHT_PROGBITS, SHF_ALLOC);
    }
#endif

    if (s->char_is_unsigned) {
        tcc_define_symbol(s, "__CHAR_UNSIGNED__", NULL);
    }

    
    if (do_debug) {
        
        stab_section = new_section(s, ".stab", SHT_PROGBITS, 0);
        stab_section->sh_entsize = sizeof(Stab_Sym);
        stabstr_section = new_section(s, ".stabstr", SHT_STRTAB, 0);
        put_elf_str(stabstr_section, "");
        stab_section->link = stabstr_section;
        
        put_stabs("", 0, 0, 0, 0);
    }

    
#ifndef TCC_TARGET_PE
    if ((output_type == TCC_OUTPUT_EXE || output_type == TCC_OUTPUT_DLL) &&
        !s->nostdlib) {
        if (output_type != TCC_OUTPUT_DLL)
            tcc_add_file(s, CONFIG_TCC_CRT_PREFIX "/crt1.o");
        tcc_add_file(s, CONFIG_TCC_CRT_PREFIX "/crti.o");
    }
#endif
    return 0;
}

#define WD_ALL    0x0001 
#define FD_INVERT 0x0002 

typedef struct FlagDef {
    uint16_t offset;
    uint16_t flags;
    const char *name;
} FlagDef;

static const FlagDef warning_defs[] = {
    { offsetof(TCCState, warn_unsupported), 0, "unsupported" },
    { offsetof(TCCState, warn_write_strings), 0, "write-strings" },
    { offsetof(TCCState, warn_error), 0, "error" },
    { offsetof(TCCState, warn_implicit_function_declaration), WD_ALL,
      "implicit-function-declaration" },
};

static int set_flag(TCCState *s, const FlagDef *flags, int nb_flags,
                    const char *name, int value)
{
    int i;
    const FlagDef *p;
    const char *r;

    r = name;
    if (r[0] == 'n' && r[1] == 'o' && r[2] == '-') {
        r += 3;
        value = !value;
    }
    for(i = 0, p = flags; i < nb_flags; i++, p++) {
        if (!strcmp(r, p->name))
            goto found;
    }
    return -1;
 found:
    if (p->flags & FD_INVERT)
        value = !value;
    *(int *)((uint8_t *)s + p->offset) = value;
    return 0;
}


int tcc_set_warning(TCCState *s, const char *warning_name, int value)
{
    int i;
    const FlagDef *p;

    if (!strcmp(warning_name, "all")) {
        for(i = 0, p = warning_defs; i < countof(warning_defs); i++, p++) {
            if (p->flags & WD_ALL)
                *(int *)((uint8_t *)s + p->offset) = 1;
        }
        return 0;
    } else {
        return set_flag(s, warning_defs, countof(warning_defs),
                        warning_name, value);
    }
}

static const FlagDef flag_defs[] = {
    { offsetof(TCCState, char_is_unsigned), 0, "unsigned-char" },
    { offsetof(TCCState, char_is_unsigned), FD_INVERT, "signed-char" },
    { offsetof(TCCState, nocommon), FD_INVERT, "common" },
    { offsetof(TCCState, leading_underscore), 0, "leading-underscore" },
};

int tcc_set_flag(TCCState *s, const char *flag_name, int value)
{
    return set_flag(s, flag_defs, countof(flag_defs),
                    flag_name, value);
}

#if !defined(LIBTCC)

static const char *tcc_basename(const char *name)
{
    const char *p;
    p = strrchr(name, '/');
#ifdef WIN32
    if (!p)
        p = strrchr(name, '\\');
#endif    
    if (!p)
        p = name;
    else 
        p++;
    return p;
}

static int64_t getclock_us(void)
{
#ifdef WIN32
    struct _timeb tb;
    _ftime(&tb);
    return (tb.time * 1000LL + tb.millitm) * 1000LL;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
#endif
}

void help(void)
{
    printf("tcc version " TCC_VERSION " - Tiny C Compiler - Copyright (C) 2001-2013 Fabrice Bellard\n"
           "usage: tcc [-v] [-c] [-o outfile] [-Bdir] [-bench] [-Idir] [-Dsym[=val]] [-Usym]\n"
           "           [-Wwarn] [-g] [-b] [-bt N] [-Ldir] [-llib] [-shared] [-static]\n"
           "           [infile1 infile2...] [-run infile args...]\n"
           "\n"
           "General options:\n"
           "  -v          display current version\n"
           "  -c          compile only - generate an object file\n"
           "  -o outfile  set output filename\n"
           "  -Bdir       set tcc internal library path\n"
           "  -bench      output compilation statistics\n"
 	   "  -run        run compiled source\n"
           "  -fflag      set or reset (with 'no-' prefix) 'flag' (see man page)\n"
           "  -Wwarning   set or reset (with 'no-' prefix) 'warning' (see man page)\n"
           "  -w          disable all warnings\n"
           "Preprocessor options:\n"
           "  -Idir       add include path 'dir'\n"
           "  -Dsym[=val] define 'sym' with value 'val'\n"
           "  -Usym       undefine 'sym'\n"
           "Linker options:\n"
           "  -Ldir       add library path 'dir'\n"
           "  -llib       link with dynamic or static library 'lib'\n"
           "  -shared     generate a shared library\n"
           "  -static     static linking\n"
           "  -rdynamic   export all global symbols to dynamic linker\n"
           "  -r          relocatable output\n"
           "Debugger options:\n"
           "  -g          generate runtime debug info\n"
#ifdef CONFIG_TCC_BCHECK
           "  -b          compile with built-in memory and bounds checker (implies -g)\n"
#endif
           "  -bt N       show N callers in stack traces\n"
           );
}

#define TCC_OPTION_HAS_ARG 0x0001
#define TCC_OPTION_NOSEP   0x0002 

typedef struct TCCOption {
    const char *name;
    uint16_t index;
    uint16_t flags;
} TCCOption;

enum {
    TCC_OPTION_HELP,
    TCC_OPTION_I,
    TCC_OPTION_D,
    TCC_OPTION_U,
    TCC_OPTION_L,
    TCC_OPTION_B,
    TCC_OPTION_l,
    TCC_OPTION_bench,
    TCC_OPTION_bt,
    TCC_OPTION_b,
    TCC_OPTION_g,
    TCC_OPTION_c,
    TCC_OPTION_static,
    TCC_OPTION_shared,
    TCC_OPTION_o,
    TCC_OPTION_r,
    TCC_OPTION_Wl,
    TCC_OPTION_W,
    TCC_OPTION_O,
    TCC_OPTION_m,
    TCC_OPTION_f,
    TCC_OPTION_nostdinc,
    TCC_OPTION_nostdlib,
    TCC_OPTION_print_search_dirs,
    TCC_OPTION_rdynamic,
    TCC_OPTION_run,
    TCC_OPTION_v,
    TCC_OPTION_w,
    TCC_OPTION_pipe,
};

static const TCCOption tcc_options[] = {
    { "h", TCC_OPTION_HELP, 0 },
    { "?", TCC_OPTION_HELP, 0 },
    { "I", TCC_OPTION_I, TCC_OPTION_HAS_ARG },
    { "D", TCC_OPTION_D, TCC_OPTION_HAS_ARG },
    { "U", TCC_OPTION_U, TCC_OPTION_HAS_ARG },
    { "L", TCC_OPTION_L, TCC_OPTION_HAS_ARG },
    { "B", TCC_OPTION_B, TCC_OPTION_HAS_ARG },
    { "l", TCC_OPTION_l, TCC_OPTION_HAS_ARG | TCC_OPTION_NOSEP },
    { "bench", TCC_OPTION_bench, 0 },
    { "bt", TCC_OPTION_bt, TCC_OPTION_HAS_ARG },
#ifdef CONFIG_TCC_BCHECK
    { "b", TCC_OPTION_b, 0 },
#endif
    { "g", TCC_OPTION_g, TCC_OPTION_HAS_ARG | TCC_OPTION_NOSEP },
    { "c", TCC_OPTION_c, 0 },
    { "static", TCC_OPTION_static, 0 },
    { "shared", TCC_OPTION_shared, 0 },
    { "o", TCC_OPTION_o, TCC_OPTION_HAS_ARG },
    { "run", TCC_OPTION_run, TCC_OPTION_HAS_ARG | TCC_OPTION_NOSEP },
    { "rdynamic", TCC_OPTION_rdynamic, 0 },
    { "r", TCC_OPTION_r, 0 },
    { "Wl,", TCC_OPTION_Wl, TCC_OPTION_HAS_ARG | TCC_OPTION_NOSEP },
    { "W", TCC_OPTION_W, TCC_OPTION_HAS_ARG | TCC_OPTION_NOSEP },
    { "O", TCC_OPTION_O, TCC_OPTION_HAS_ARG | TCC_OPTION_NOSEP },
    { "m", TCC_OPTION_m, TCC_OPTION_HAS_ARG },
    { "f", TCC_OPTION_f, TCC_OPTION_HAS_ARG | TCC_OPTION_NOSEP },
    { "nostdinc", TCC_OPTION_nostdinc, 0 },
    { "nostdlib", TCC_OPTION_nostdlib, 0 },
    { "print-search-dirs", TCC_OPTION_print_search_dirs, 0 }, 
    { "v", TCC_OPTION_v, 0 },
    { "w", TCC_OPTION_w, 0 },
    { "pipe", TCC_OPTION_pipe, 0},
    { NULL },
};

static int expand_args(char ***pargv, const char *str)
{
    const char *s1;
    char **argv, *arg;
    int argc, len;

    argc = 0;
    argv = NULL;
    for(;;) {
        while (is_space(*str))
            str++;
        if (*str == '\0')
            break;
        s1 = str;
        while (*str != '\0' && !is_space(*str))
            str++;
        len = str - s1;
        arg = tcc_malloc(len + 1);
        memcpy(arg, s1, len);
        arg[len] = '\0';
        dynarray_add((void ***)&argv, &argc, arg);
    }
    *pargv = argv;
    return argc;
}

static char **files;
static int nb_files, nb_libraries;
static int multiple_files;
static int print_search_dirs;
static int output_type;
static int reloc_output;
static const char *outfile;

int parse_args(TCCState *s, int argc, char **argv)
{
    int optind;
    const TCCOption *popt;
    const char *optarg, *p1, *r1;
    char *r;

    optind = 0;
    while (1) {
        if (optind >= argc) {
            if (nb_files == 0 && !print_search_dirs)
                goto show_help;
            else
                break;
        }
        r = argv[optind++];
        if (r[0] != '-') {
            
            dynarray_add((void ***)&files, &nb_files, r);
            if (!multiple_files) {
                optind--;
                
                break;
            }
        } else {
            
            popt = tcc_options;
            for(;;) {
                p1 = popt->name;
                if (p1 == NULL)
                    error("invalid option -- '%s'", r);
                r1 = r + 1;
                for(;;) {
                    if (*p1 == '\0')
                        goto option_found;
                    if (*r1 != *p1)
                        break;
                    p1++;
                    r1++;
                }
                popt++;
            }
        option_found:
            if (popt->flags & TCC_OPTION_HAS_ARG) {
                if (*r1 != '\0' || (popt->flags & TCC_OPTION_NOSEP)) {
                    optarg = r1;
                } else {
                    if (optind >= argc)
                        error("argument to '%s' is missing", r);
                    optarg = argv[optind++];
                }
            } else {
                if (*r1 != '\0')
                    goto show_help;
                optarg = NULL;
            }
                
            switch(popt->index) {
            case TCC_OPTION_HELP:
            show_help:
                help();
                exit(1);
            case TCC_OPTION_I:
                if (tcc_add_include_path(s, optarg) < 0)
                    error("too many include paths");
                break;
            case TCC_OPTION_D:
                {
                    char *sym, *value;
                    sym = (char *)optarg;
                    value = strchr(sym, '=');
                    if (value) {
                        *value = '\0';
                        value++;
                    }
                    tcc_define_symbol(s, sym, value);
                }
                break;
            case TCC_OPTION_U:
                tcc_undefine_symbol(s, optarg);
                break;
            case TCC_OPTION_L:
                tcc_add_library_path(s, optarg);
                break;
            case TCC_OPTION_B:
                
                tcc_lib_path = optarg;
                break;
            case TCC_OPTION_l:
                dynarray_add((void ***)&files, &nb_files, r);
                nb_libraries++;
                break;
            case TCC_OPTION_bench:
                do_bench = 1;
                break;
            case TCC_OPTION_bt:
                num_callers = atoi(optarg);
                break;
#ifdef CONFIG_TCC_BCHECK
            case TCC_OPTION_b:
                do_bounds_check = 1;
                do_debug = 1;
                break;
#endif
            case TCC_OPTION_g:
                do_debug = 1;
                break;
            case TCC_OPTION_c:
                multiple_files = 1;
                output_type = TCC_OUTPUT_OBJ;
                break;
            case TCC_OPTION_static:
                s->static_link = 1;
                break;
            case TCC_OPTION_shared:
                output_type = TCC_OUTPUT_DLL;
                break;
            case TCC_OPTION_o:
                multiple_files = 1;
                outfile = optarg;
                break;
            case TCC_OPTION_r:
                
                reloc_output = 1;
                output_type = TCC_OUTPUT_OBJ;
                break;
            case TCC_OPTION_nostdinc:
                s->nostdinc = 1;
                break;
            case TCC_OPTION_nostdlib:
                s->nostdlib = 1;
                break;
            case TCC_OPTION_print_search_dirs:
                print_search_dirs = 1;
                break;
            case TCC_OPTION_run:
                {
                    int argc1;
                    char **argv1;
                    argc1 = expand_args(&argv1, optarg);
                    if (argc1 > 0) {
                        parse_args(s, argc1, argv1);
                    }
                    multiple_files = 0;
                    output_type = TCC_OUTPUT_MEMORY;
                }
                break;
            case TCC_OPTION_v:
                printf("tcc version %s\n", TCC_VERSION);
                exit(0);
            case TCC_OPTION_f:
                if (tcc_set_flag(s, optarg, 1) < 0 && s->warn_unsupported)
                    goto unsupported_option;
                break;
            case TCC_OPTION_W:
                if (tcc_set_warning(s, optarg, 1) < 0 && 
                    s->warn_unsupported)
                    goto unsupported_option;
                break;
            case TCC_OPTION_w:
                s->warn_none = 1;
                break;
            case TCC_OPTION_rdynamic:
                s->rdynamic = 1;
                break;
            case TCC_OPTION_Wl:
                {
                    const char *p;
                    if (strstart(optarg, "-Ttext,", &p)) {
                        s->text_addr = strtoul(p, NULL, 16);
                        s->has_text_addr = 1;
                    } else if (strstart(optarg, "--oformat,", &p)) {
                        if (strstart(p, "elf32-", NULL)) {
                            s->output_format = TCC_OUTPUT_FORMAT_ELF;
                        } else if (!strcmp(p, "binary")) {
                            s->output_format = TCC_OUTPUT_FORMAT_BINARY;
                        } else
#ifdef TCC_TARGET_COFF
                        if (!strcmp(p, "coff")) {
                            s->output_format = TCC_OUTPUT_FORMAT_COFF;
                        } else
#endif
                        {
                            error("target %s not found", p);
                        }
                    } else {
                        error("unsupported linker option '%s'", optarg);
                    }
                }
                break;
            default:
                if (s->warn_unsupported) {
                unsupported_option:
                    warning("unsupported option '%s'", r);
                }
                break;
            }
        }
    }
    return optind;
}

int main2(int argc, char **argv)
{
    int i;
    TCCState *s;
    int nb_objfiles, ret, optind;
    char objfilename[1024];
    int64_t start_time = 0;

#ifdef WIN32
    {
        static char path[1024];
        char *p, *d;
        
        GetModuleFileNameA(NULL, path, sizeof path);
        p = d = strlwr(path);
        while (*d)
        {
            if (*d == '\\') *d = '/', p = d;
            ++d;
        }
        *p = '\0';
        tcc_lib_path = path;
    }
#endif

    s = tcc_new();
    output_type = TCC_OUTPUT_EXE;
    outfile = NULL;
    multiple_files = 1;
    files = NULL;
    nb_files = 0;
    nb_libraries = 0;
    reloc_output = 0;
    print_search_dirs = 0;

    optind = parse_args(s, argc - 1, argv + 1) + 1;

    if (print_search_dirs) {
        
        printf("install: %s/\n", tcc_lib_path);
        return 0;
    }

    nb_objfiles = nb_files - nb_libraries;

    if (outfile && output_type == TCC_OUTPUT_MEMORY)
        output_type = TCC_OUTPUT_EXE;

    
    if (output_type == TCC_OUTPUT_OBJ && !reloc_output) {
        
        if (nb_objfiles != 1)
            error("cannot specify multiple files with -c");
        if (nb_libraries != 0)
            error("cannot specify libraries with -c");
    }
    
    if (output_type != TCC_OUTPUT_MEMORY) {
        if (!outfile) {
    
            pstrcpy(objfilename, sizeof(objfilename) - 1, 
                    
                    tcc_basename(files[0]));
#ifdef TCC_TARGET_PE
            pe_guess_outfile(objfilename, output_type);
#else
            if (output_type == TCC_OUTPUT_OBJ && !reloc_output) {
                char *ext = strrchr(objfilename, '.');
            if (!ext)
                goto default_outfile;
                
            strcpy(ext + 1, "o");
        } else {
        default_outfile:
            pstrcpy(objfilename, sizeof(objfilename), "a.out");
        }
#endif
        outfile = objfilename;
        }
    }

    if (do_bench) {
        start_time = getclock_us();
    }

    tcc_set_output_type(s, output_type);

    
    for(i = 0;i < nb_files; i++) {
        const char *filename;

        filename = files[i];
        if (filename[0] == '-') {
            if (tcc_add_library(s, filename + 2) < 0)
                error("cannot find %s", filename);
        } else {
            if (tcc_add_file(s, filename) < 0) {
                ret = 1;
                goto the_end;
            }
        }
    }

    
    tcc_free(files);

    if (do_bench) {
        double total_time;
        total_time = (double)(getclock_us() - start_time) / 1000000.0;
        if (total_time < 0.001)
            total_time = 0.001;
        if (total_bytes < 1)
            total_bytes = 1;
        printf("%d idents, %d lines, %d bytes, %0.3f s, %d lines/s, %0.1f MB/s\n", 
               tok_ident - TOK_IDENT, total_lines, total_bytes,
               total_time, (int)(total_lines / total_time), 
               total_bytes / total_time / 1000000.0); 
    }

    if (s->output_type == TCC_OUTPUT_MEMORY) {
        ret = tcc_run(s, argc - optind, argv + optind);
    } else
#ifdef TCC_TARGET_PE
    if (s->output_type != TCC_OUTPUT_OBJ) {
        ret = tcc_output_pe(s, outfile);
    } else
#endif
    {
        tcc_output_file(s, outfile);
        ret = 0;
    }
 the_end:
    
    if (!do_bounds_check)
        tcc_delete(s);

#ifdef MEM_DEBUG
    if (do_bench) {
        printf("memory: %d bytes, max = %d bytes\n", mem_cur_size, mem_max_size);
    }
#endif
    return ret;
}

int main(int argc, char **argv)
{
   #define REPS   30
   int i;
   for (i = 0; i < REPS; i++) {
      main2(argc, argv);
   }
   return 0;
}

#endif

unsigned short __tcc_fpu_control = 0x137f;
unsigned short __tcc_int_fpu_control = 0x137f | 0x0c00;

#if 0
long long __shldi3(long long a, int b)
{
#ifdef __TINYC__ 
    DWunion u;
    u.ll = a;
    if (b >= 32) {
        u.s.high = (unsigned)u.s.low << (b - 32);
        u.s.low = 0;
    } else if (b != 0) {
        u.s.high = ((unsigned)u.s.high << b) | (u.s.low >> (32 - b));
        u.s.low = (unsigned)u.s.low << b;
    }
    return u.ll;
#else
    return a << b;
#endif
}
#endif
