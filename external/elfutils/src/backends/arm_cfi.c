/* arm ABI-specified defaults for DWARF CFI.
   Copyright (C) 2013 Red Hat, Inc.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <dwarf.h>

#define BACKEND arm_
#include "libebl_CPU.h"



int
arm_abi_cfi (Ebl *ebl __attribute__ ((unused)), Dwarf_CIE *abi_info)
{
  static const uint8_t abi_cfi[] =
    {
      DW_CFA_def_cfa, ULEB128_7 (13), ULEB128_7 (0),

      
      DW_CFA_val_offset, ULEB128_7 (13), ULEB128_7 (0),

#define SV(n) DW_CFA_same_value, ULEB128_7 (n)
      
      SV (4), SV (5), SV (6), SV (7), SV (8), SV (10), SV (11),

      
      SV (14),
      DW_CFA_register, ULEB128_7 (15), ULEB128_7 (14), 
#undef SV

#define ULEB128_8_2(x) ((x & 0x7f) | 0x80), 0x02
#define SV(n) DW_CFA_same_value, ULEB128_8_2 (n)
      SV (264), SV (265), SV (266), SV (267),
      SV (268), SV (269), SV (270), SV (271),

    };
#undef ULEB128_8_2
#undef SV

  abi_info->initial_instructions = abi_cfi;
  abi_info->initial_instructions_end = &abi_cfi[sizeof abi_cfi];
  abi_info->data_alignment_factor = 4;

  abi_info->return_address_register = 15; 

  return 0;
}
