

/* 
   Compute x * y + z as ternary operation.
   Copyright (C) 2010-2013 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Jakub Jelinek <jakub@redhat.com>, 2010.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.
*/


#ifndef __VEX_HOST_GENERIC_MADDF_H
#define __VEX_HOST_GENERIC_MADDF_H

#include "libvex_basictypes.h"

extern VEX_REGPARM(3)
       void h_generic_calc_MAddF32 ( Float*, Float*, Float*, Float* );

extern VEX_REGPARM(3)
       void h_generic_calc_MAddF64 ( Double*, Double*, Double*,
                                     Double* );

#endif 

