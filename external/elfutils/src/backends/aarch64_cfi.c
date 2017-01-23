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

#define BACKEND aarch64_
#include "libebl_CPU.h"



int
aarch64_abi_cfi (Ebl *ebl __attribute__ ((unused)), Dwarf_CIE *abi_info)
{
  static const uint8_t abi_cfi[] =
    {
      DW_CFA_def_cfa, ULEB128_7 (30), ULEB128_7 (0),

#define SV(n) DW_CFA_same_value, ULEB128_7 (n)
      
      SV (19), SV (20), SV (21), SV (22), SV (23),
      SV (24), SV (25), SV (26), SV (27), SV (28),

      
      SV (29), SV (30),

      
      SV (72), SV (73), SV (74), SV (75),
      SV (76), SV (77), SV (78), SV (79),
#undef SV

    };

  abi_info->initial_instructions = abi_cfi;
  abi_info->initial_instructions_end = &abi_cfi[sizeof abi_cfi];
  abi_info->data_alignment_factor = -4;

  abi_info->return_address_register = 30; 

  return 0;
}
