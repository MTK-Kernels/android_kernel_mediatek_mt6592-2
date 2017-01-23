

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2000-2013 Julian Seward
      jseward@acm.org

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
*/

#ifndef __PUB_CORE_DISPATCH_ASM_H
#define __PUB_CORE_DISPATCH_ASM_H

#include "libvex_trc_values.h"

#define VG_TRC_BORING              29 
#define VG_TRC_INNER_FASTMISS      37 
#define VG_TRC_INNER_COUNTERZERO   41 
#define VG_TRC_FAULT_SIGNAL        43 
#define VG_TRC_INVARIANT_FAILED    47 
#define VG_TRC_CHAIN_ME_TO_SLOW_EP 49 
#define VG_TRC_CHAIN_ME_TO_FAST_EP 51 

#endif   

