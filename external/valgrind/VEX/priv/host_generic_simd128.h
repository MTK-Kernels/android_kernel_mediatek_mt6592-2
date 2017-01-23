

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2010-2013 OpenWorks GbR
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
*/


#ifndef __VEX_HOST_GENERIC_SIMD128_H
#define __VEX_HOST_GENERIC_SIMD128_H

#include "libvex_basictypes.h"

extern VEX_REGPARM(3)
       void h_generic_calc_Mul32x4    ( V128*, V128*, V128* );
extern VEX_REGPARM(3)
       void h_generic_calc_Max32Sx4   ( V128*, V128*, V128* );
extern VEX_REGPARM(3)
       void h_generic_calc_Min32Sx4   ( V128*, V128*, V128* );
extern VEX_REGPARM(3)
       void h_generic_calc_Max32Ux4   ( V128*, V128*, V128* );
extern VEX_REGPARM(3)
       void h_generic_calc_Min32Ux4   ( V128*, V128*, V128* );
extern VEX_REGPARM(3)
       void h_generic_calc_Max16Ux8   ( V128*, V128*, V128* );
extern VEX_REGPARM(3)
       void h_generic_calc_Min16Ux8   ( V128*, V128*, V128* );
extern VEX_REGPARM(3)
       void h_generic_calc_Max8Sx16   ( V128*, V128*, V128* );
extern VEX_REGPARM(3)
       void h_generic_calc_Min8Sx16   ( V128*, V128*, V128* );
extern VEX_REGPARM(3)
       void h_generic_calc_CmpEQ64x2  ( V128*, V128*, V128* );
extern VEX_REGPARM(3)
       void h_generic_calc_CmpGT64Sx2 ( V128*, V128*, V128* );

extern 
       void h_generic_calc_SarN64x2   ( V128*, V128*, UInt );
extern 
       void h_generic_calc_SarN8x16   ( V128*, V128*, UInt );

extern VEX_REGPARM(3)
       void h_generic_calc_QNarrowBin32Sto16Ux8
                                      ( V128*, V128*, V128* );
extern VEX_REGPARM(3)
       void h_generic_calc_NarrowBin16to8x16
                                      ( V128*, V128*, V128* );
extern VEX_REGPARM(3)
       void h_generic_calc_NarrowBin32to16x8
                                      ( V128*, V128*, V128* );

extern VEX_REGPARM(3)
       void h_generic_calc_Perm32x4   ( V128*, V128*, V128* );

extern 
       UInt  h_generic_calc_GetMSBs8x16 ( ULong w64hi, ULong w64lo );

#endif 

