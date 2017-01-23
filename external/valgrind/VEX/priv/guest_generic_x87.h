

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2004-2013 OpenWorks LLP
      info@open-works.net

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   The GNU General Public License is contained in the file COPYING.

   Neither the names of the U.S. Department of Energy nor the
   University of California nor the names of its contributors may be
   used to endorse or promote products derived from this software
   without prior written permission.
*/


#ifndef __VEX_GUEST_GENERIC_X87_H
#define __VEX_GUEST_GENERIC_X87_H

#include "libvex_basictypes.h"


extern
void convert_f64le_to_f80le ( UChar* f64, UChar* f80 );


extern
void convert_f80le_to_f64le ( UChar* f80, UChar* f64 );


typedef
   struct {
      UShort env[14];
      UChar  reg[80];
   }
   Fpu_State;

#define FP_ENV_CTRL   0
#define FP_ENV_STAT   2
#define FP_ENV_TAG    4
#define FP_ENV_IP     6 
#define FP_ENV_CS     8
#define FP_ENV_LSTOP  9
#define FP_ENV_OPOFF  10 
#define FP_ENV_OPSEL  12
#define FP_REG(ii)    (10*(7-(ii)))


typedef
   struct {
      UShort env[7];
      UChar  reg[80];
   }
   Fpu_State_16;

#define FPS_ENV_CTRL   0
#define FPS_ENV_STAT   1
#define FPS_ENV_TAG    2
#define FPS_ENV_IP     3
#define FPS_ENV_CS     4
#define FPS_ENV_OPOFF  5
#define FPS_ENV_OPSEL  6


extern ULong x86amd64g_calculate_FXTRACT ( ULong arg, HWord getExp );

extern Bool compute_PCMPxSTRx ( V128* resV,
                                UInt* resOSZACP,
                                V128* argLV,  V128* argRV,
                                UInt zmaskL, UInt zmaskR,
                                UInt imm8,   Bool isxSTRM );

extern Bool compute_PCMPxSTRx_wide ( V128* resV,
                                     UInt* resOSZACP,
                                     V128* argLV,  V128* argRV,
                                     UInt zmaskL, UInt zmaskR,
                                     UInt imm8,   Bool isxSTRM );

#endif 

