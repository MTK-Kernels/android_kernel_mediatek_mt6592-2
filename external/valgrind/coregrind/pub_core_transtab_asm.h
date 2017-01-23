

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

#ifndef __PUB_CORE_TRANSTAB_ASM_H
#define __PUB_CORE_TRANSTAB_ASM_H


#define VG_TT_FAST_BITS 15
#define VG_TT_FAST_SIZE (1 << VG_TT_FAST_BITS)
#define VG_TT_FAST_MASK ((VG_TT_FAST_SIZE) - 1)


#if defined(VGA_x86) || defined(VGA_amd64)
#  define VG_TT_FAST_HASH(_addr)  ((((UWord)(_addr))     ) & VG_TT_FAST_MASK)

#elif defined(VGA_s390x) || defined(VGA_arm)
#  define VG_TT_FAST_HASH(_addr)  ((((UWord)(_addr)) >> 1) & VG_TT_FAST_MASK)

#elif defined(VGA_ppc32) || defined(VGA_ppc64be) || defined(VGA_ppc64le) \
      || defined(VGA_mips32) || defined(VGA_mips64) || defined(VGA_arm64)
#  define VG_TT_FAST_HASH(_addr)  ((((UWord)(_addr)) >> 2) & VG_TT_FAST_MASK)

#elif defined(VGA_tilegx)
#  define VG_TT_FAST_HASH(_addr)  ((((UWord)(_addr)) >> 3) & VG_TT_FAST_MASK)

#else
#  error "VG_TT_FAST_HASH: unknown platform"
#endif

#endif   

