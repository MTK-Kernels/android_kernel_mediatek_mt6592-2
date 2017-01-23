/* Get previous frame state for an existing frame state.
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

#include <stdlib.h>
#include <assert.h>

#define BACKEND s390_
#include "libebl_CPU.h"


bool
s390_unwind (Ebl *ebl, Dwarf_Addr pc, ebl_tid_registers_t *setfunc,
	     ebl_tid_registers_get_t *getfunc, ebl_pid_memory_read_t *readfunc,
	     void *arg, bool *signal_framep)
{
  if ((pc & 0x3) != 0x3)
    return false;
  pc++;
  
  Dwarf_Word instr;
  if (! readfunc (pc, &instr, arg))
    return false;
  
  instr = (instr >> (ebl->class == ELFCLASS64 ? 48 : 16)) & 0xffff;
  
  
  if (((instr >> 8) & 0xff) != 0x0a)
    return false;
  
  if ((instr & 0xff) != 119 && (instr & 0xff) != 173)
    return false;
  
  Dwarf_Word this_sp;
  if (! getfunc (0 + 15, 1, &this_sp, arg))
    return false;
  unsigned word_size = ebl->class == ELFCLASS64 ? 8 : 4;
  Dwarf_Addr next_cfa = this_sp + 16 * word_size + 32;
  Dwarf_Word sigreg_ptr;
  if (! readfunc (next_cfa + 8, &sigreg_ptr, arg))
    return false;
  
  sigreg_ptr += word_size;
  
  Dwarf_Word val;
  if (! readfunc (sigreg_ptr, &val, arg))
    return false;
  if (! setfunc (-1, 1, &val, arg))
    return false;
  sigreg_ptr += word_size;
  
  Dwarf_Word gprs[16];
  for (int i = 0; i < 16; i++)
    {
      if (! readfunc (sigreg_ptr, &gprs[i], arg))
	return false;
      sigreg_ptr += word_size;
    }
  
  for (int i = 0; i < 16; i++)
    sigreg_ptr += 4;
  
  sigreg_ptr += 8;
  
  Dwarf_Word fprs[16];
  for (int i = 0; i < 16; i++)
    {
      if (! readfunc (sigreg_ptr, &val, arg))
	return false;
      if (ebl->class == ELFCLASS32)
	{
	  Dwarf_Addr val_low;
	  if (! readfunc (sigreg_ptr + 4, &val_low, arg))
	    return false;
	  val = (val << 32) | val_low;
	}
      fprs[i] = val;
      sigreg_ptr += 8;
    }
  
  if (ebl->class == ELFCLASS32)
    {
      
      sigreg_ptr += 4;
      for (int i = 0; i < 16; i++)
	{
	  if (! readfunc (sigreg_ptr, &val, arg))
	    return false;
	  Dwarf_Word val_low = gprs[i];
	  val = (val << 32) | val_low;
	  gprs[i] = val;
	  sigreg_ptr += 4;
	}
    }
  if (! setfunc (0, 16, gprs, arg))
    return false;
  if (! setfunc (16, 16, fprs, arg))
    return false;
  *signal_framep = true;
  return true;
}
