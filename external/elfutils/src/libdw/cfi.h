/* Internal definitions for libdw CFI interpreter.
   Copyright (C) 2009-2010, 2013 Red Hat, Inc.
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

#ifndef _UNWINDP_H
#define _UNWINDP_H 1

#include "libdwP.h"
#include "libelfP.h"
struct ebl;

struct dwarf_cie
{
  Dwarf_Off offset;	 

  Dwarf_Word code_alignment_factor;
  Dwarf_Sword data_alignment_factor;
  Dwarf_Word return_address_register;

  size_t fde_augmentation_data_size;

  
  const uint8_t *initial_instructions;
  const uint8_t *initial_instructions_end;

  const Dwarf_Frame *initial_state;

  uint8_t fde_encoding;		
  uint8_t lsda_encoding;    

  bool sized_augmentation_data;	
  bool signal_frame;		
};

struct dwarf_fde
{
  struct dwarf_cie *cie;

  
  Dwarf_Addr start;
  Dwarf_Addr end;

  const uint8_t *instructions;
  const uint8_t *instructions_end;
};

struct Dwarf_CFI_s
{
  
  Dwarf *dbg;
#define CFI_IS_EH(cfi)	((cfi)->dbg == NULL)

  
  Elf_Data_Scn *data;
  const unsigned char *e_ident;	

  Dwarf_Addr frame_vaddr;  
  Dwarf_Addr textrel;		
  Dwarf_Addr datarel;		

  
  Dwarf_Off next_offset;

  
  void *cie_tree;

  
  void *fde_tree;

  
  void *expr_tree;

  
  struct ebl *ebl;

  
  const uint8_t *search_table;
  Dwarf_Addr search_table_vaddr;
  size_t search_table_entries;
  uint8_t search_table_encoding;

  
  bool other_byte_order;

  bool default_same_value;
};


enum dwarf_frame_rule
  {
    reg_unspecified,		
    reg_undefined,		
    reg_same_value,		
    reg_offset,			
    reg_val_offset,		
    reg_register,		
    reg_expression,		
    reg_val_expression,		
  };

struct dwarf_frame_register
{
  enum dwarf_frame_rule rule:3;

  Dwarf_Sword value:(sizeof (Dwarf_Sword) * 8 - 3);
};

struct Dwarf_Frame_s
{
  
  Dwarf_Addr start;
  Dwarf_Addr end;

  Dwarf_CFI *cache;

  Dwarf_Frame *prev;

  struct dwarf_fde *fde;

  enum { cfa_undefined, cfa_offset, cfa_expr, cfa_invalid } cfa_rule;
  union
  {
    Dwarf_Op offset;
    Dwarf_Block expr;
  } cfa_data;
  
#define cfa_val_reg	cfa_data.offset.number
#define cfa_val_offset	cfa_data.offset.number2

  size_t nregs;
  struct dwarf_frame_register regs[];
};


extern void __libdw_destroy_frame_cache (Dwarf_CFI *cache)
  __nonnull_attribute__ (1) internal_function;

extern void __libdw_intern_cie (Dwarf_CFI *cache, Dwarf_Off offset,
				const Dwarf_CIE *info)
  __nonnull_attribute__ (1, 3) internal_function;

extern struct dwarf_cie *__libdw_find_cie (Dwarf_CFI *cache, Dwarf_Off offset)
  __nonnull_attribute__ (1) internal_function;


extern struct dwarf_fde *__libdw_find_fde (Dwarf_CFI *cache,
					   Dwarf_Addr address)
  __nonnull_attribute__ (1) internal_function;

extern struct dwarf_fde *__libdw_fde_by_offset (Dwarf_CFI *cache,
						Dwarf_Off offset)
  __nonnull_attribute__ (1) internal_function;

extern int __libdw_frame_at_address (Dwarf_CFI *cache, struct dwarf_fde *fde,
				     Dwarf_Addr address, Dwarf_Frame **frame)
  __nonnull_attribute__ (1, 2, 4) internal_function;


#define BYTE_ORDER_DUMMY(var, e_ident)					      \
  const struct { bool other_byte_order; } var =				      \
    { ((BYTE_ORDER == LITTLE_ENDIAN && e_ident[EI_DATA] == ELFDATA2MSB)       \
       || (BYTE_ORDER == BIG_ENDIAN && e_ident[EI_DATA] == ELFDATA2LSB)) }


INTDECL (dwarf_next_cfi)
INTDECL (dwarf_getcfi)
INTDECL (dwarf_getcfi_elf)
INTDECL (dwarf_cfi_end)
INTDECL (dwarf_cfi_addrframe)

#endif	
