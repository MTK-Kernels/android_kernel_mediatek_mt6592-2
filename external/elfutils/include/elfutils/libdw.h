/* Interfaces for libdw.
   Copyright (C) 2002-2010, 2013, 2014 Red Hat, Inc.
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

#ifndef _LIBDW_H
#define _LIBDW_H	1

#include <gelf.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3)
# define __nonnull_attribute__(...) __attribute__ ((__nonnull__ (__VA_ARGS__)))
# define __deprecated_attribute__ __attribute__ ((__deprecated__))
#else
# define __nonnull_attribute__(args...)
# define __deprecated_attribute__
#endif


#ifdef __GNUC_STDC_INLINE__
# define __libdw_extern_inline extern __inline __attribute__ ((__gnu_inline__))
#else
# define __libdw_extern_inline extern __inline
#endif


typedef enum
  {
    DWARF_C_READ,		
    DWARF_C_RDWR,		
    DWARF_C_WRITE,		
  }
Dwarf_Cmd;


enum
{
  DWARF_CB_OK = 0,
  DWARF_CB_ABORT
};


enum
  {
    DW_TAG_invalid = 0
#define DW_TAG_invalid	DW_TAG_invalid
  };


typedef GElf_Off Dwarf_Off;

typedef GElf_Addr Dwarf_Addr;

typedef GElf_Xword Dwarf_Word;
typedef GElf_Sxword Dwarf_Sword;
typedef GElf_Half Dwarf_Half;


typedef struct Dwarf_Abbrev Dwarf_Abbrev;

#define DWARF_END_ABBREV ((Dwarf_Abbrev *) -1l)

typedef struct Dwarf_Lines_s Dwarf_Lines;

typedef struct Dwarf_Line_s Dwarf_Line;

typedef struct Dwarf_Files_s Dwarf_Files;

typedef struct Dwarf_Arange_s Dwarf_Arange;

typedef struct Dwarf_Aranges_s Dwarf_Aranges;

struct Dwarf_CU;
typedef struct Dwarf_CU Dwarf_CU;

typedef struct Dwarf_Macro_s Dwarf_Macro;

typedef struct
{
  unsigned int code;
  unsigned int form;
  unsigned char *valp;
  struct Dwarf_CU *cu;
} Dwarf_Attribute;


typedef struct
{
  Dwarf_Word length;
  unsigned char *data;
} Dwarf_Block;


typedef struct
{
  
  void *addr;
  struct Dwarf_CU *cu;
  Dwarf_Abbrev *abbrev;
  
  long int padding__;
} Dwarf_Die;

#define DWARF_END_DIE ((Dwarf_Die *) -1l)


typedef struct
{
  Dwarf_Off cu_offset;
  Dwarf_Off die_offset;
  const char *name;
} Dwarf_Global;


typedef struct
{
  uint8_t atom;			
  Dwarf_Word number;		
  Dwarf_Word number2;		
  Dwarf_Word offset;		
} Dwarf_Op;


typedef struct
{
  Dwarf_Off CIE_id;	 

  const uint8_t *initial_instructions;
  const uint8_t *initial_instructions_end;

  Dwarf_Word code_alignment_factor;
  Dwarf_Sword data_alignment_factor;
  Dwarf_Word return_address_register;

  const char *augmentation;	

  const uint8_t *augmentation_data;
  size_t augmentation_data_size;
  size_t fde_augmentation_data_size;
} Dwarf_CIE;

typedef struct
{
  Dwarf_Off CIE_pointer;

  const uint8_t *start;
  const uint8_t *end;
} Dwarf_FDE;

typedef union
{
  Dwarf_Off CIE_id;	 
  Dwarf_CIE cie;
  Dwarf_FDE fde;
} Dwarf_CFI_Entry;

#define dwarf_cfi_cie_p(entry)	((entry)->cie.CIE_id == DW_CIE_ID_64)

typedef struct Dwarf_Frame_s Dwarf_Frame;

typedef struct Dwarf_CFI_s Dwarf_CFI;


typedef struct Dwarf Dwarf;


#if __GNUC__ < 4
typedef void (*Dwarf_OOM) (void);
#else
typedef void (*__attribute__ ((noreturn)) Dwarf_OOM) (void);
#endif


#ifdef __cplusplus
extern "C" {
#endif

extern Dwarf *dwarf_begin (int fildes, Dwarf_Cmd cmd);

extern Dwarf *dwarf_begin_elf (Elf *elf, Dwarf_Cmd cmd, Elf_Scn *scngrp);

extern Elf *dwarf_getelf (Dwarf *dwarf);

extern Dwarf *dwarf_cu_getdwarf (Dwarf_CU *cu);

extern Dwarf *dwarf_getalt (Dwarf *main);

extern void dwarf_setalt (Dwarf *main, Dwarf *alt);

extern int dwarf_end (Dwarf *dwarf);


extern Elf_Data *dwarf_getscn_info (Dwarf *dwarf);

extern int dwarf_nextcu (Dwarf *dwarf, Dwarf_Off off, Dwarf_Off *next_off,
			 size_t *header_sizep, Dwarf_Off *abbrev_offsetp,
			 uint8_t *address_sizep, uint8_t *offset_sizep)
     __nonnull_attribute__ (3);

extern int dwarf_next_unit (Dwarf *dwarf, Dwarf_Off off, Dwarf_Off *next_off,
			    size_t *header_sizep, Dwarf_Half *versionp,
			    Dwarf_Off *abbrev_offsetp,
			    uint8_t *address_sizep, uint8_t *offset_sizep,
			    uint64_t *type_signaturep, Dwarf_Off *type_offsetp)
     __nonnull_attribute__ (3);


extern int dwarf_next_cfi (const unsigned char e_ident[],
			   Elf_Data *data, bool eh_frame_p,
			   Dwarf_Off offset, Dwarf_Off *next_offset,
			   Dwarf_CFI_Entry *entry)
  __nonnull_attribute__ (1, 2, 5, 6);

extern Dwarf_CFI *dwarf_getcfi (Dwarf *dwarf);

extern Dwarf_CFI *dwarf_getcfi_elf (Elf *elf);

extern int dwarf_cfi_end (Dwarf_CFI *cache);


extern Dwarf_Die *dwarf_offdie (Dwarf *dbg, Dwarf_Off offset,
				Dwarf_Die *result) __nonnull_attribute__ (3);

extern Dwarf_Die *dwarf_offdie_types (Dwarf *dbg, Dwarf_Off offset,
				      Dwarf_Die *result)
     __nonnull_attribute__ (3);

extern Dwarf_Off dwarf_dieoffset (Dwarf_Die *die);

extern Dwarf_Off dwarf_cuoffset (Dwarf_Die *die);

extern Dwarf_Die *dwarf_diecu (Dwarf_Die *die, Dwarf_Die *result,
			       uint8_t *address_sizep, uint8_t *offset_sizep)
     __nonnull_attribute__ (2);

extern Dwarf_Die *dwarf_cu_die (Dwarf_CU *cu, Dwarf_Die *result,
				Dwarf_Half *versionp,
				Dwarf_Off *abbrev_offsetp,
				uint8_t *address_sizep,
				uint8_t *offset_sizep,
				uint64_t *type_signaturep,
				Dwarf_Off *type_offsetp)
     __nonnull_attribute__ (2);

extern Dwarf_Die *dwarf_addrdie (Dwarf *dbg, Dwarf_Addr addr,
				 Dwarf_Die *result) __nonnull_attribute__ (3);

extern int dwarf_child (Dwarf_Die *die, Dwarf_Die *result)
     __nonnull_attribute__ (2);

extern int dwarf_siblingof (Dwarf_Die *die, Dwarf_Die *result)
     __nonnull_attribute__ (2);

extern int dwarf_peel_type (Dwarf_Die *die, Dwarf_Die *result)
    __nonnull_attribute__ (2);

extern int dwarf_haschildren (Dwarf_Die *die) __nonnull_attribute__ (1);

extern ptrdiff_t dwarf_getattrs (Dwarf_Die *die,
				 int (*callback) (Dwarf_Attribute *, void *),
				 void *arg, ptrdiff_t offset)
     __nonnull_attribute__ (2);

extern int dwarf_tag (Dwarf_Die *die) __nonnull_attribute__ (1);


extern Dwarf_Attribute *dwarf_attr (Dwarf_Die *die, unsigned int search_name,
				    Dwarf_Attribute *result)
     __nonnull_attribute__ (3);

extern int dwarf_hasattr (Dwarf_Die *die, unsigned int search_name);

extern Dwarf_Attribute *dwarf_attr_integrate (Dwarf_Die *die,
					      unsigned int search_name,
					      Dwarf_Attribute *result)
     __nonnull_attribute__ (3);
extern int dwarf_hasattr_integrate (Dwarf_Die *die, unsigned int search_name);




extern int dwarf_hasform (Dwarf_Attribute *attr, unsigned int search_form);

extern unsigned int dwarf_whatattr (Dwarf_Attribute *attr);

extern unsigned int dwarf_whatform (Dwarf_Attribute *attr);


extern const char *dwarf_formstring (Dwarf_Attribute *attrp);

extern int dwarf_formudata (Dwarf_Attribute *attr, Dwarf_Word *return_uval)
     __nonnull_attribute__ (2);

extern int dwarf_formsdata (Dwarf_Attribute *attr, Dwarf_Sword *return_uval)
     __nonnull_attribute__ (2);

extern int dwarf_formaddr (Dwarf_Attribute *attr, Dwarf_Addr *return_addr)
     __nonnull_attribute__ (2);

extern int dwarf_formref (Dwarf_Attribute *attr, Dwarf_Off *return_offset)
     __nonnull_attribute__ (2) __deprecated_attribute__;

extern Dwarf_Die *dwarf_formref_die (Dwarf_Attribute *attr, Dwarf_Die *die_mem)
     __nonnull_attribute__ (2);

extern int dwarf_formblock (Dwarf_Attribute *attr, Dwarf_Block *return_block)
     __nonnull_attribute__ (2);

extern int dwarf_formflag (Dwarf_Attribute *attr, bool *return_bool)
     __nonnull_attribute__ (2);



extern const char *dwarf_diename (Dwarf_Die *die);

extern int dwarf_highpc (Dwarf_Die *die, Dwarf_Addr *return_addr)
     __nonnull_attribute__ (2);

extern int dwarf_lowpc (Dwarf_Die *die, Dwarf_Addr *return_addr)
     __nonnull_attribute__ (2);

extern int dwarf_entrypc (Dwarf_Die *die, Dwarf_Addr *return_addr)
     __nonnull_attribute__ (2);

extern int dwarf_haspc (Dwarf_Die *die, Dwarf_Addr pc);

extern ptrdiff_t dwarf_ranges (Dwarf_Die *die,
			       ptrdiff_t offset, Dwarf_Addr *basep,
			       Dwarf_Addr *startp, Dwarf_Addr *endp);


extern int dwarf_bytesize (Dwarf_Die *die);

extern int dwarf_bitsize (Dwarf_Die *die);

extern int dwarf_bitoffset (Dwarf_Die *die);

extern int dwarf_arrayorder (Dwarf_Die *die);

extern int dwarf_srclang (Dwarf_Die *die);


extern Dwarf_Abbrev *dwarf_getabbrev (Dwarf_Die *die, Dwarf_Off offset,
				      size_t *lengthp);

extern int dwarf_offabbrev (Dwarf *dbg, Dwarf_Off offset, size_t *lengthp,
			    Dwarf_Abbrev *abbrevp)
     __nonnull_attribute__ (4);

extern unsigned int dwarf_getabbrevcode (Dwarf_Abbrev *abbrev);

extern unsigned int dwarf_getabbrevtag (Dwarf_Abbrev *abbrev);

extern int dwarf_abbrevhaschildren (Dwarf_Abbrev *abbrev);

extern int dwarf_getattrcnt (Dwarf_Abbrev *abbrev, size_t *attrcntp)
     __nonnull_attribute__ (2);

extern int dwarf_getabbrevattr (Dwarf_Abbrev *abbrev, size_t idx,
				unsigned int *namep, unsigned int *formp,
				Dwarf_Off *offset);


extern const char *dwarf_getstring (Dwarf *dbg, Dwarf_Off offset,
				    size_t *lenp);


extern ptrdiff_t dwarf_getpubnames (Dwarf *dbg,
				    int (*callback) (Dwarf *, Dwarf_Global *,
						     void *),
				    void *arg, ptrdiff_t offset)
     __nonnull_attribute__ (2);


extern int dwarf_getsrclines (Dwarf_Die *cudie, Dwarf_Lines **lines,
			      size_t *nlines) __nonnull_attribute__ (2, 3);

extern Dwarf_Line *dwarf_onesrcline (Dwarf_Lines *lines, size_t idx);

extern int dwarf_getsrcfiles (Dwarf_Die *cudie, Dwarf_Files **files,
			      size_t *nfiles)
     __nonnull_attribute__ (2);


extern Dwarf_Line *dwarf_getsrc_die (Dwarf_Die *cudie, Dwarf_Addr addr);

extern int dwarf_getsrc_file (Dwarf *dbg, const char *fname, int line, int col,
			      Dwarf_Line ***srcsp, size_t *nsrcs)
     __nonnull_attribute__ (2, 5, 6);


extern int dwarf_lineaddr (Dwarf_Line *line, Dwarf_Addr *addrp);

extern int dwarf_lineop_index (Dwarf_Line *line, unsigned int *op_indexp);

extern int dwarf_lineno (Dwarf_Line *line, int *linep)
     __nonnull_attribute__ (2);

extern int dwarf_linecol (Dwarf_Line *line, int *colp)
     __nonnull_attribute__ (2);

extern int dwarf_linebeginstatement (Dwarf_Line *line, bool *flagp)
     __nonnull_attribute__ (2);

extern int dwarf_lineendsequence (Dwarf_Line *line, bool *flagp)
     __nonnull_attribute__ (2);

extern int dwarf_lineblock (Dwarf_Line *line, bool *flagp)
     __nonnull_attribute__ (2);

extern int dwarf_lineprologueend (Dwarf_Line *line, bool *flagp)
     __nonnull_attribute__ (2);

extern int dwarf_lineepiloguebegin (Dwarf_Line *line, bool *flagp)
     __nonnull_attribute__ (2);

extern int dwarf_lineisa (Dwarf_Line *line, unsigned int *isap)
     __nonnull_attribute__ (2);

extern int dwarf_linediscriminator (Dwarf_Line *line, unsigned int *discp)
     __nonnull_attribute__ (2);


extern const char *dwarf_linesrc (Dwarf_Line *line,
				  Dwarf_Word *mtime, Dwarf_Word *length);

extern const char *dwarf_filesrc (Dwarf_Files *file, size_t idx,
				  Dwarf_Word *mtime, Dwarf_Word *length);

extern int dwarf_getsrcdirs (Dwarf_Files *files,
			     const char *const **result, size_t *ndirs)
  __nonnull_attribute__ (2, 3);


extern int dwarf_getlocation (Dwarf_Attribute *attr, Dwarf_Op **expr,
			      size_t *exprlen) __nonnull_attribute__ (2, 3);

extern int dwarf_getlocation_addr (Dwarf_Attribute *attr, Dwarf_Addr address,
				   Dwarf_Op **exprs, size_t *exprlens,
				   size_t nlocs);

extern ptrdiff_t dwarf_getlocations (Dwarf_Attribute *attr,
				     ptrdiff_t offset, Dwarf_Addr *basep,
				     Dwarf_Addr *startp, Dwarf_Addr *endp,
				     Dwarf_Op **expr, size_t *exprlen);

extern int dwarf_getlocation_implicit_value (Dwarf_Attribute *attr,
					     const Dwarf_Op *op,
					     Dwarf_Block *return_block)
  __nonnull_attribute__ (2, 3);

extern int dwarf_getlocation_implicit_pointer (Dwarf_Attribute *attr,
					       const Dwarf_Op *op,
					       Dwarf_Attribute *result)
  __nonnull_attribute__ (2, 3);

extern int dwarf_getlocation_die (Dwarf_Attribute *attr,
				  const Dwarf_Op *op,
				  Dwarf_Die *result)
  __nonnull_attribute__ (2, 3);

extern int dwarf_getlocation_attr (Dwarf_Attribute *attr,
				   const Dwarf_Op *op,
				   Dwarf_Attribute *result)
  __nonnull_attribute__ (2, 3);


extern int dwarf_aggregate_size (Dwarf_Die *die, Dwarf_Word *size);


extern int dwarf_getscopes (Dwarf_Die *cudie, Dwarf_Addr pc,
			    Dwarf_Die **scopes);

extern int dwarf_getscopes_die (Dwarf_Die *die, Dwarf_Die **scopes);


extern int dwarf_getscopevar (Dwarf_Die *scopes, int nscopes,
			      const char *name, int skip_shadows,
			      const char *match_file,
			      int match_lineno, int match_linecol,
			      Dwarf_Die *result);



extern int dwarf_getaranges (Dwarf *dbg, Dwarf_Aranges **aranges,
			     size_t *naranges)
     __nonnull_attribute__ (2);

extern Dwarf_Arange *dwarf_onearange (Dwarf_Aranges *aranges, size_t idx);

extern int dwarf_getarangeinfo (Dwarf_Arange *arange, Dwarf_Addr *addrp,
				Dwarf_Word *lengthp, Dwarf_Off *offsetp);

extern Dwarf_Arange *dwarf_getarange_addr (Dwarf_Aranges *aranges,
					   Dwarf_Addr addr);



extern ptrdiff_t dwarf_getfuncs (Dwarf_Die *cudie,
				 int (*callback) (Dwarf_Die *, void *),
				 void *arg, ptrdiff_t offset);


extern const char *dwarf_decl_file (Dwarf_Die *decl);

extern int dwarf_decl_line (Dwarf_Die *decl, int *linep)
     __nonnull_attribute__ (2);

extern int dwarf_decl_column (Dwarf_Die *decl, int *colp)
     __nonnull_attribute__ (2);


extern int dwarf_func_inline (Dwarf_Die *func);

extern int dwarf_func_inline_instances (Dwarf_Die *func,
					int (*callback) (Dwarf_Die *, void *),
					void *arg);


extern int dwarf_entry_breakpoints (Dwarf_Die *die, Dwarf_Addr **bkpts);


#define DWARF_GETMACROS_START PTRDIFF_MIN
extern ptrdiff_t dwarf_getmacros (Dwarf_Die *cudie,
				  int (*callback) (Dwarf_Macro *, void *),
				  void *arg, ptrdiff_t token)
     __nonnull_attribute__ (2);

extern ptrdiff_t dwarf_getmacros_off (Dwarf *dbg, Dwarf_Off macoff,
				      int (*callback) (Dwarf_Macro *, void *),
				      void *arg, ptrdiff_t token)
  __nonnull_attribute__ (3);

extern int dwarf_macro_getsrcfiles (Dwarf *dbg, Dwarf_Macro *macro,
				    Dwarf_Files **files, size_t *nfiles)
  __nonnull_attribute__ (2, 3, 4);

extern int dwarf_macro_opcode (Dwarf_Macro *macro, unsigned int *opcodep)
     __nonnull_attribute__ (2);

extern int dwarf_macro_getparamcnt (Dwarf_Macro *macro, size_t *paramcntp);

extern int dwarf_macro_param (Dwarf_Macro *macro, size_t idx,
			      Dwarf_Attribute *attribute);

extern int dwarf_macro_param1 (Dwarf_Macro *macro, Dwarf_Word *paramp)
     __nonnull_attribute__ (2);

extern int dwarf_macro_param2 (Dwarf_Macro *macro, Dwarf_Word *paramp,
			       const char **strp);

extern int dwarf_cfi_addrframe (Dwarf_CFI *cache,
				Dwarf_Addr address, Dwarf_Frame **frame)
  __nonnull_attribute__ (3);

extern int dwarf_frame_info (Dwarf_Frame *frame,
			     Dwarf_Addr *start, Dwarf_Addr *end, bool *signalp);

extern int dwarf_frame_cfa (Dwarf_Frame *frame, Dwarf_Op **ops, size_t *nops)
  __nonnull_attribute__ (2);

extern int dwarf_frame_register (Dwarf_Frame *frame, int regno,
				 Dwarf_Op ops_mem[3],
				 Dwarf_Op **ops, size_t *nops)
  __nonnull_attribute__ (3, 4, 5);


extern int dwarf_errno (void);

extern const char *dwarf_errmsg (int err);


extern Dwarf_OOM dwarf_new_oom_handler (Dwarf *dbg, Dwarf_OOM handler);


#ifdef __OPTIMIZE__
__libdw_extern_inline unsigned int
dwarf_whatattr (Dwarf_Attribute *attr)
{
  return attr == NULL ? 0 : attr->code;
}

__libdw_extern_inline unsigned int
dwarf_whatform (Dwarf_Attribute *attr)
{
  return attr == NULL ? 0 : attr->form;
}
#endif	

#ifdef __cplusplus
}
#endif

#endif	
