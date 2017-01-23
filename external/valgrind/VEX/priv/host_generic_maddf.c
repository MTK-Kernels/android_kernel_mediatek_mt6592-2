

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


#include "libvex_basictypes.h"
#include "host_generic_maddf.h"
#include "main_util.h"


#define FORCE_EVAL(X) __asm __volatile__ ("" : : "m" (X))

#if defined(__x86_64__) && defined(__SSE2_MATH__)
# define ENV_TYPE unsigned int
# define ROUNDTOZERO(env) \
   do {							\
      unsigned int mxcsr;				\
      __asm __volatile__ ("stmxcsr %0" : "=m" (mxcsr));	\
      (env) = mxcsr;					\
      mxcsr = (mxcsr | 0x7f80) & ~0x3f;			\
      __asm __volatile__ ("ldmxcsr %0" : : "m" (mxcsr));\
   } while (0)
# define RESET_TESTINEXACT(env) \
   ({							\
      unsigned int mxcsr, ret;				\
      __asm __volatile__ ("stmxcsr %0" : "=m" (mxcsr));	\
      ret = (mxcsr >> 5) & 1;				\
      mxcsr = (mxcsr & 0x3d) | (env);			\
      __asm __volatile__ ("ldmxcsr %0" : : "m" (mxcsr));\
      ret;						\
   })
# define TESTINEXACT() \
   ({							\
      unsigned int mxcsr;				\
      __asm __volatile__ ("stmxcsr %0" : "=m" (mxcsr));	\
      (mxcsr >> 5) & 1;					\
   })
#endif

#define DBL_MANT_DIG 53
#define IEEE754_DOUBLE_BIAS 0x3ff

union vg_ieee754_double {
   Double d;

   
   struct {
#ifdef VKI_BIG_ENDIAN
      unsigned int negative:1;
      unsigned int exponent:11;
      unsigned int mantissa0:20;
      unsigned int mantissa1:32;
#else
      unsigned int mantissa1:32;
      unsigned int mantissa0:20;
      unsigned int exponent:11;
      unsigned int negative:1;
#endif
   } ieee;
};

void VEX_REGPARM(3)
     h_generic_calc_MAddF32 ( Float* res,
                               Float* argX, Float* argY, Float* argZ )
{
#ifndef ENV_TYPE
   
   *res = *argX * *argY + *argZ;
#else
   ENV_TYPE env;
   
   Double temp = (Double) *argX * (Double) *argY;
   union vg_ieee754_double u;

   ROUNDTOZERO (env);

   
   u.d = temp + (Double) *argZ;
   
   FORCE_EVAL (u.d);

   
   int j = RESET_TESTINEXACT (env);

   if ((u.ieee.mantissa1 & 1) == 0 && u.ieee.exponent != 0x7ff)
      u.ieee.mantissa1 |= j;

   
   *res = (Float) u.d;
#endif
}


void VEX_REGPARM(3)
     h_generic_calc_MAddF64 ( Double* res,
                               Double* argX, Double* argY, Double* argZ )
{
#ifndef ENV_TYPE
   
   *res = *argX * *argY + *argZ;
#else
   Double x = *argX, y = *argY, z = *argZ;
   union vg_ieee754_double u, v, w;
   int adjust = 0;
   u.d = x;
   v.d = y;
   w.d = z;
   if (UNLIKELY (u.ieee.exponent + v.ieee.exponent
                 >= 0x7ff + IEEE754_DOUBLE_BIAS - DBL_MANT_DIG)
       || UNLIKELY (u.ieee.exponent >= 0x7ff - DBL_MANT_DIG)
       || UNLIKELY (v.ieee.exponent >= 0x7ff - DBL_MANT_DIG)
       || UNLIKELY (w.ieee.exponent >= 0x7ff - DBL_MANT_DIG)
       || UNLIKELY (u.ieee.exponent + v.ieee.exponent
                    <= IEEE754_DOUBLE_BIAS + DBL_MANT_DIG)) {
      if (w.ieee.exponent == 0x7ff
          && u.ieee.exponent != 0x7ff
          && v.ieee.exponent != 0x7ff) {
         *res = (z + x) + y;
         return;
      }
      if (u.ieee.exponent == 0x7ff
          || v.ieee.exponent == 0x7ff
          || w.ieee.exponent == 0x7ff
          || u.ieee.exponent + v.ieee.exponent > 0x7ff + IEEE754_DOUBLE_BIAS
          || u.ieee.exponent + v.ieee.exponent
             < IEEE754_DOUBLE_BIAS - DBL_MANT_DIG - 2) {
         *res = x * y + z;
         return;
      }
      if (u.ieee.exponent + v.ieee.exponent
          >= 0x7ff + IEEE754_DOUBLE_BIAS - DBL_MANT_DIG) {
         if (u.ieee.exponent > v.ieee.exponent)
            u.ieee.exponent -= DBL_MANT_DIG;
         else
            v.ieee.exponent -= DBL_MANT_DIG;
         if (w.ieee.exponent > DBL_MANT_DIG)
            w.ieee.exponent -= DBL_MANT_DIG;
         adjust = 1;
      } else if (w.ieee.exponent >= 0x7ff - DBL_MANT_DIG) {
         if (u.ieee.exponent > v.ieee.exponent) {
            if (u.ieee.exponent > DBL_MANT_DIG)
               u.ieee.exponent -= DBL_MANT_DIG;
         } else if (v.ieee.exponent > DBL_MANT_DIG)
            v.ieee.exponent -= DBL_MANT_DIG;
         w.ieee.exponent -= DBL_MANT_DIG;
         adjust = 1;
      } else if (u.ieee.exponent >= 0x7ff - DBL_MANT_DIG) {
         u.ieee.exponent -= DBL_MANT_DIG;
         if (v.ieee.exponent)
            v.ieee.exponent += DBL_MANT_DIG;
         else
            v.d *= 0x1p53;
      } else if (v.ieee.exponent >= 0x7ff - DBL_MANT_DIG) {
         v.ieee.exponent -= DBL_MANT_DIG;
         if (u.ieee.exponent)
            u.ieee.exponent += DBL_MANT_DIG;
         else
            u.d *= 0x1p53;
      } else 
 {
         if (u.ieee.exponent > v.ieee.exponent)
            u.ieee.exponent += 2 * DBL_MANT_DIG;
         else
            v.ieee.exponent += 2 * DBL_MANT_DIG;
         if (w.ieee.exponent <= 4 * DBL_MANT_DIG + 4) {
            if (w.ieee.exponent)
               w.ieee.exponent += 2 * DBL_MANT_DIG;
            else
               w.d *= 0x1p106;
            adjust = -1;
         }
      }
      x = u.d;
      y = v.d;
      z = w.d;
   }
   
#  define C ((1 << (DBL_MANT_DIG + 1) / 2) + 1)
   Double x1 = x * C;
   Double y1 = y * C;
   Double m1 = x * y;
   x1 = (x - x1) + x1;
   y1 = (y - y1) + y1;
   Double x2 = x - x1;
   Double y2 = y - y1;
   Double m2 = (((x1 * y1 - m1) + x1 * y2) + x2 * y1) + x2 * y2;
#  undef C

   
   Double a1 = z + m1;
   Double t1 = a1 - z;
   Double t2 = a1 - t1;
   t1 = m1 - t1;
   t2 = z - t2;
   Double a2 = t1 + t2;

   ENV_TYPE env;
   ROUNDTOZERO (env);

   
   u.d = a2 + m2;

   if (UNLIKELY (adjust < 0)) {
      if ((u.ieee.mantissa1 & 1) == 0)
         u.ieee.mantissa1 |= TESTINEXACT ();
      v.d = a1 + u.d;
      
      FORCE_EVAL (v.d);
   }

   
   int j = RESET_TESTINEXACT (env) != 0;

   if (LIKELY (adjust == 0)) {
      if ((u.ieee.mantissa1 & 1) == 0 && u.ieee.exponent != 0x7ff)
         u.ieee.mantissa1 |= j;
      
      *res = a1 + u.d;
   } else if (LIKELY (adjust > 0)) {
      if ((u.ieee.mantissa1 & 1) == 0 && u.ieee.exponent != 0x7ff)
         u.ieee.mantissa1 |= j;
      
      *res = (a1 + u.d) * 0x1p53;
   } else {
      if (j == 0) {
         *res = v.d * 0x1p-106;
         return;
      }
      if (v.ieee.exponent > 106) {
         *res = (a1 + u.d) * 0x1p-106;
         return;
      }
      if (v.ieee.exponent == 106) {
         if ((v.ieee.mantissa1 & 3) == 1) {
            v.d *= 0x1p-106;
            if (v.ieee.negative)
               *res = v.d - 0x1p-1074;
            else
               *res = v.d + 0x1p-1074;
         } else
            *res = v.d * 0x1p-106;
         return;
      }
      v.ieee.mantissa1 |= j;
      *res = v.d * 0x1p-106;
      return;
    }
#endif
}

