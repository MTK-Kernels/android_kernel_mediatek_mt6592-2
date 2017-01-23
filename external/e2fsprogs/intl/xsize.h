/* xsize.h -- Checked size_t computations.

   Copyright (C) 2003 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU Library General Public License as published
   by the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
   USA.  */

#ifndef _XSIZE_H
#define _XSIZE_H

#include <stddef.h>

#include <limits.h>
#if HAVE_STDINT_H
# include <stdint.h>
#endif


#define xcast_size_t(N) \
  ((N) <= SIZE_MAX ? (size_t) (N) : SIZE_MAX)

static inline size_t
#if __GNUC__ >= 3
__attribute__ ((__pure__))
#endif
xsum (size_t size1, size_t size2)
{
  size_t sum = size1 + size2;
  return (sum >= size1 ? sum : SIZE_MAX);
}

static inline size_t
#if __GNUC__ >= 3
__attribute__ ((__pure__))
#endif
xsum3 (size_t size1, size_t size2, size_t size3)
{
  return xsum (xsum (size1, size2), size3);
}

static inline size_t
#if __GNUC__ >= 3
__attribute__ ((__pure__))
#endif
xsum4 (size_t size1, size_t size2, size_t size3, size_t size4)
{
  return xsum (xsum (xsum (size1, size2), size3), size4);
}

static inline size_t
#if __GNUC__ >= 3
__attribute__ ((__pure__))
#endif
xmax (size_t size1, size_t size2)
{
  return (size1 >= size2 ? size1 : size2);
}

#define xtimes(N, ELSIZE) \
  ((N) <= SIZE_MAX / (ELSIZE) ? (size_t) (N) * (ELSIZE) : SIZE_MAX)

#define size_overflow_p(SIZE) \
  ((SIZE) == SIZE_MAX)
#define size_in_bounds_p(SIZE) \
  ((SIZE) != SIZE_MAX)

#endif 
