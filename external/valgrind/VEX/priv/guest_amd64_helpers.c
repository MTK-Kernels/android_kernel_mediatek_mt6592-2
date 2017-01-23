

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

#include "libvex_basictypes.h"
#include "libvex_emnote.h"
#include "libvex_guest_amd64.h"
#include "libvex_ir.h"
#include "libvex.h"

#include "main_util.h"
#include "main_globals.h"
#include "guest_generic_bb_to_IR.h"
#include "guest_amd64_defs.h"
#include "guest_generic_x87.h"




#define PROFILE_RFLAGS 0




static void mullS64 ( Long u, Long v, Long* rHi, Long* rLo )
{
   ULong u0, v0, w0;
    Long u1, v1, w1, w2, t;
   u0   = u & 0xFFFFFFFFULL; 
   u1   = u >> 32;
   v0   = v & 0xFFFFFFFFULL;
   v1   = v >> 32;
   w0   = u0 * v0;
   t    = u1 * v0 + (w0 >> 32);
   w1   = t & 0xFFFFFFFFULL;
   w2   = t >> 32;
   w1   = u0 * v1 + w1;
   *rHi = u1 * v1 + w2 + (w1 >> 32);
   *rLo = u * v;
}

static void mullU64 ( ULong u, ULong v, ULong* rHi, ULong* rLo )
{
   ULong u0, v0, w0;
   ULong u1, v1, w1,w2,t;
   u0   = u & 0xFFFFFFFFULL;
   u1   = u >> 32;
   v0   = v & 0xFFFFFFFFULL;
   v1   = v >> 32;
   w0   = u0 * v0;
   t    = u1 * v0 + (w0 >> 32);
   w1   = t & 0xFFFFFFFFULL;
   w2   = t >> 32;
   w1   = u0 * v1 + w1;
   *rHi = u1 * v1 + w2 + (w1 >> 32);
   *rLo = u * v;
}


static const UChar parity_table[256] = {
    AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0,
    0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P,
    0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P,
    AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0,
    0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P,
    AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0,
    AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0,
    0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P,
    0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P,
    AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0,
    AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0,
    0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P,
    AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0,
    0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P,
    0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P,
    AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0,
    0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P,
    AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0,
    AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0,
    0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P,
    AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0,
    0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P,
    0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P,
    AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0,
    AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0,
    0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P,
    0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P,
    AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0,
    0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P,
    AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0,
    AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0,
    0, AMD64G_CC_MASK_P, AMD64G_CC_MASK_P, 0, AMD64G_CC_MASK_P, 0, 0, AMD64G_CC_MASK_P,
};

static inline Long lshift ( Long x, Int n )
{
   if (n >= 0)
      return (ULong)x << n;
   else
      return x >> (-n);
}

static inline ULong idULong ( ULong x )
{
   return x;
}


#define PREAMBLE(__data_bits)					\
    ULong DATA_MASK 					\
      = __data_bits==8                                          \
           ? 0xFFULL 					        \
           : (__data_bits==16                                   \
                ? 0xFFFFULL 		                        \
                : (__data_bits==32                              \
                     ? 0xFFFFFFFFULL                            \
                     : 0xFFFFFFFFFFFFFFFFULL));                 \
    ULong SIGN_MASK = 1ULL << (__data_bits - 1);     \
    ULong CC_DEP1 = cc_dep1_formal;			\
    ULong CC_DEP2 = cc_dep2_formal;			\
    ULong CC_NDEP = cc_ndep_formal;			\
   	\
   	\
   	\
   SIGN_MASK = SIGN_MASK;					\
   DATA_MASK = DATA_MASK;					\
   CC_DEP2 = CC_DEP2;						\
   CC_NDEP = CC_NDEP;



#define ACTIONS_ADD(DATA_BITS,DATA_UTYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   { ULong cf, pf, af, zf, sf, of;				\
     ULong argL, argR, res;					\
     argL = CC_DEP1;						\
     argR = CC_DEP2;						\
     res  = argL + argR;					\
     cf = (DATA_UTYPE)res < (DATA_UTYPE)argL;			\
     pf = parity_table[(UChar)res];				\
     af = (res ^ argL ^ argR) & 0x10;				\
     zf = ((DATA_UTYPE)res == 0) << 6;				\
     sf = lshift(res, 8 - DATA_BITS) & 0x80;			\
     of = lshift((argL ^ argR ^ -1) & (argL ^ res), 		\
                 12 - DATA_BITS) & AMD64G_CC_MASK_O;		\
     return cf | pf | af | zf | sf | of;			\
   }								\
}


#define ACTIONS_SUB(DATA_BITS,DATA_UTYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   { ULong cf, pf, af, zf, sf, of;				\
     ULong argL, argR, res;					\
     argL = CC_DEP1;						\
     argR = CC_DEP2;						\
     res  = argL - argR;					\
     cf = (DATA_UTYPE)argL < (DATA_UTYPE)argR;			\
     pf = parity_table[(UChar)res];				\
     af = (res ^ argL ^ argR) & 0x10;				\
     zf = ((DATA_UTYPE)res == 0) << 6;				\
     sf = lshift(res, 8 - DATA_BITS) & 0x80;			\
     of = lshift((argL ^ argR) & (argL ^ res),	 		\
                 12 - DATA_BITS) & AMD64G_CC_MASK_O; 		\
     return cf | pf | af | zf | sf | of;			\
   }								\
}


#define ACTIONS_ADC(DATA_BITS,DATA_UTYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   { ULong cf, pf, af, zf, sf, of;				\
     ULong argL, argR, oldC, res;		 		\
     oldC = CC_NDEP & AMD64G_CC_MASK_C;				\
     argL = CC_DEP1;						\
     argR = CC_DEP2 ^ oldC;	       				\
     res  = (argL + argR) + oldC;				\
     if (oldC)							\
        cf = (DATA_UTYPE)res <= (DATA_UTYPE)argL;		\
     else							\
        cf = (DATA_UTYPE)res < (DATA_UTYPE)argL;		\
     pf = parity_table[(UChar)res];				\
     af = (res ^ argL ^ argR) & 0x10;				\
     zf = ((DATA_UTYPE)res == 0) << 6;				\
     sf = lshift(res, 8 - DATA_BITS) & 0x80;			\
     of = lshift((argL ^ argR ^ -1) & (argL ^ res), 		\
                  12 - DATA_BITS) & AMD64G_CC_MASK_O;		\
     return cf | pf | af | zf | sf | of;			\
   }								\
}


#define ACTIONS_SBB(DATA_BITS,DATA_UTYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   { ULong cf, pf, af, zf, sf, of;				\
     ULong argL, argR, oldC, res;	       			\
     oldC = CC_NDEP & AMD64G_CC_MASK_C;				\
     argL = CC_DEP1;						\
     argR = CC_DEP2 ^ oldC;	       				\
     res  = (argL - argR) - oldC;				\
     if (oldC)							\
        cf = (DATA_UTYPE)argL <= (DATA_UTYPE)argR;		\
     else							\
        cf = (DATA_UTYPE)argL < (DATA_UTYPE)argR;		\
     pf = parity_table[(UChar)res];				\
     af = (res ^ argL ^ argR) & 0x10;				\
     zf = ((DATA_UTYPE)res == 0) << 6;				\
     sf = lshift(res, 8 - DATA_BITS) & 0x80;			\
     of = lshift((argL ^ argR) & (argL ^ res), 			\
                 12 - DATA_BITS) & AMD64G_CC_MASK_O;		\
     return cf | pf | af | zf | sf | of;			\
   }								\
}


#define ACTIONS_LOGIC(DATA_BITS,DATA_UTYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   { ULong cf, pf, af, zf, sf, of;				\
     cf = 0;							\
     pf = parity_table[(UChar)CC_DEP1];				\
     af = 0;							\
     zf = ((DATA_UTYPE)CC_DEP1 == 0) << 6;			\
     sf = lshift(CC_DEP1, 8 - DATA_BITS) & 0x80;		\
     of = 0;							\
     return cf | pf | af | zf | sf | of;			\
   }								\
}


#define ACTIONS_INC(DATA_BITS,DATA_UTYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   { ULong cf, pf, af, zf, sf, of;				\
     ULong argL, argR, res;					\
     res  = CC_DEP1;						\
     argL = res - 1;						\
     argR = 1;							\
     cf = CC_NDEP & AMD64G_CC_MASK_C;				\
     pf = parity_table[(UChar)res];				\
     af = (res ^ argL ^ argR) & 0x10;				\
     zf = ((DATA_UTYPE)res == 0) << 6;				\
     sf = lshift(res, 8 - DATA_BITS) & 0x80;			\
     of = ((res & DATA_MASK) == SIGN_MASK) << 11;		\
     return cf | pf | af | zf | sf | of;			\
   }								\
}


#define ACTIONS_DEC(DATA_BITS,DATA_UTYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   { ULong cf, pf, af, zf, sf, of;				\
     ULong argL, argR, res;					\
     res  = CC_DEP1;						\
     argL = res + 1;						\
     argR = 1;							\
     cf = CC_NDEP & AMD64G_CC_MASK_C;				\
     pf = parity_table[(UChar)res];				\
     af = (res ^ argL ^ argR) & 0x10;				\
     zf = ((DATA_UTYPE)res == 0) << 6;				\
     sf = lshift(res, 8 - DATA_BITS) & 0x80;			\
     of = ((res & DATA_MASK) 					\
          == ((ULong)SIGN_MASK - 1)) << 11;			\
     return cf | pf | af | zf | sf | of;			\
   }								\
}


#define ACTIONS_SHL(DATA_BITS,DATA_UTYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   { ULong cf, pf, af, zf, sf, of;				\
     cf = (CC_DEP2 >> (DATA_BITS - 1)) & AMD64G_CC_MASK_C;	\
     pf = parity_table[(UChar)CC_DEP1];				\
     af = 0; 					\
     zf = ((DATA_UTYPE)CC_DEP1 == 0) << 6;			\
     sf = lshift(CC_DEP1, 8 - DATA_BITS) & 0x80;		\
     			\
     of = lshift(CC_DEP2 ^ CC_DEP1, 12 - DATA_BITS) 		\
          & AMD64G_CC_MASK_O;					\
     return cf | pf | af | zf | sf | of;			\
   }								\
}


#define ACTIONS_SHR(DATA_BITS,DATA_UTYPE)			\
{								\
   PREAMBLE(DATA_BITS);  					\
   { ULong cf, pf, af, zf, sf, of;				\
     cf = CC_DEP2 & 1;						\
     pf = parity_table[(UChar)CC_DEP1];				\
     af = 0; 					\
     zf = ((DATA_UTYPE)CC_DEP1 == 0) << 6;			\
     sf = lshift(CC_DEP1, 8 - DATA_BITS) & 0x80;		\
     			\
     of = lshift(CC_DEP2 ^ CC_DEP1, 12 - DATA_BITS)		\
          & AMD64G_CC_MASK_O;					\
     return cf | pf | af | zf | sf | of;			\
   }								\
}


#define ACTIONS_ROL(DATA_BITS,DATA_UTYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   { ULong fl 							\
        = (CC_NDEP & ~(AMD64G_CC_MASK_O | AMD64G_CC_MASK_C))	\
          | (AMD64G_CC_MASK_C & CC_DEP1)			\
          | (AMD64G_CC_MASK_O & (lshift(CC_DEP1,  		\
                                      11-(DATA_BITS-1)) 	\
                     ^ lshift(CC_DEP1, 11)));			\
     return fl;							\
   }								\
}


#define ACTIONS_ROR(DATA_BITS,DATA_UTYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   { ULong fl 							\
        = (CC_NDEP & ~(AMD64G_CC_MASK_O | AMD64G_CC_MASK_C))	\
          | (AMD64G_CC_MASK_C & (CC_DEP1 >> (DATA_BITS-1)))	\
          | (AMD64G_CC_MASK_O & (lshift(CC_DEP1, 		\
                                      11-(DATA_BITS-1)) 	\
                     ^ lshift(CC_DEP1, 11-(DATA_BITS-1)+1)));	\
     return fl;							\
   }								\
}


#define ACTIONS_UMUL(DATA_BITS, DATA_UTYPE,  NARROWtoU,         \
                                DATA_U2TYPE, NARROWto2U)        \
{                                                               \
   PREAMBLE(DATA_BITS);                                         \
   { ULong cf, pf, af, zf, sf, of;                              \
     DATA_UTYPE  hi;                                            \
     DATA_UTYPE  lo                                             \
        = NARROWtoU( ((DATA_UTYPE)CC_DEP1)                      \
                     * ((DATA_UTYPE)CC_DEP2) );                 \
     DATA_U2TYPE rr                                             \
        = NARROWto2U(                                           \
             ((DATA_U2TYPE)((DATA_UTYPE)CC_DEP1))               \
             * ((DATA_U2TYPE)((DATA_UTYPE)CC_DEP2)) );          \
     hi = NARROWtoU(rr >> DATA_BITS);                      \
     cf = (hi != 0);                                            \
     pf = parity_table[(UChar)lo];                              \
     af = 0;                                     \
     zf = (lo == 0) << 6;                                       \
     sf = lshift(lo, 8 - DATA_BITS) & 0x80;                     \
     of = cf << 11;                                             \
     return cf | pf | af | zf | sf | of;                        \
   }								\
}


#define ACTIONS_SMUL(DATA_BITS, DATA_STYPE,  NARROWtoS,         \
                                DATA_S2TYPE, NARROWto2S)        \
{                                                               \
   PREAMBLE(DATA_BITS);                                         \
   { ULong cf, pf, af, zf, sf, of;                              \
     DATA_STYPE  hi;                                            \
     DATA_STYPE  lo                                             \
        = NARROWtoS( ((DATA_S2TYPE)(DATA_STYPE)CC_DEP1)         \
                     * ((DATA_S2TYPE)(DATA_STYPE)CC_DEP2) );    \
     DATA_S2TYPE rr                                             \
        = NARROWto2S(                                           \
             ((DATA_S2TYPE)((DATA_STYPE)CC_DEP1))               \
             * ((DATA_S2TYPE)((DATA_STYPE)CC_DEP2)) );          \
     hi = NARROWtoS(rr >> DATA_BITS);                      \
     cf = (hi != (lo >> (DATA_BITS-1)));                   \
     pf = parity_table[(UChar)lo];                              \
     af = 0;                                     \
     zf = (lo == 0) << 6;                                       \
     sf = lshift(lo, 8 - DATA_BITS) & 0x80;                     \
     of = cf << 11;                                             \
     return cf | pf | af | zf | sf | of;                        \
   }								\
}


#define ACTIONS_UMULQ                                           \
{                                                               \
   PREAMBLE(64);                                                \
   { ULong cf, pf, af, zf, sf, of;                              \
     ULong lo, hi;                                              \
     mullU64( (ULong)CC_DEP1, (ULong)CC_DEP2, &hi, &lo );       \
     cf = (hi != 0);                                            \
     pf = parity_table[(UChar)lo];                              \
     af = 0;                                     \
     zf = (lo == 0) << 6;                                       \
     sf = lshift(lo, 8 - 64) & 0x80;                            \
     of = cf << 11;                                             \
     return cf | pf | af | zf | sf | of;                        \
   }								\
}


#define ACTIONS_SMULQ                                           \
{                                                               \
   PREAMBLE(64);                                                \
   { ULong cf, pf, af, zf, sf, of;                              \
     Long lo, hi;                                               \
     mullS64( (Long)CC_DEP1, (Long)CC_DEP2, &hi, &lo );         \
     cf = (hi != (lo >> (64-1)));                          \
     pf = parity_table[(UChar)lo];                              \
     af = 0;                                     \
     zf = (lo == 0) << 6;                                       \
     sf = lshift(lo, 8 - 64) & 0x80;                            \
     of = cf << 11;                                             \
     return cf | pf | af | zf | sf | of;                        \
   }								\
}


#define ACTIONS_ANDN(DATA_BITS,DATA_UTYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   { ULong cf, pf, af, zf, sf, of;				\
     cf = 0;							\
     pf = 0;							\
     af = 0;							\
     zf = ((DATA_UTYPE)CC_DEP1 == 0) << 6;			\
     sf = lshift(CC_DEP1, 8 - DATA_BITS) & 0x80;		\
     of = 0;							\
     return cf | pf | af | zf | sf | of;			\
   }								\
}


#define ACTIONS_BLSI(DATA_BITS,DATA_UTYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   { ULong cf, pf, af, zf, sf, of;				\
     cf = ((DATA_UTYPE)CC_DEP2 != 0);				\
     pf = 0;							\
     af = 0;							\
     zf = ((DATA_UTYPE)CC_DEP1 == 0) << 6;			\
     sf = lshift(CC_DEP1, 8 - DATA_BITS) & 0x80;		\
     of = 0;							\
     return cf | pf | af | zf | sf | of;			\
   }								\
}


#define ACTIONS_BLSMSK(DATA_BITS,DATA_UTYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   { Long cf, pf, af, zf, sf, of;				\
     cf = ((DATA_UTYPE)CC_DEP2 == 0);				\
     pf = 0;							\
     af = 0;							\
     zf = 0;							\
     sf = lshift(CC_DEP1, 8 - DATA_BITS) & 0x80;		\
     of = 0;							\
     return cf | pf | af | zf | sf | of;			\
   }								\
}


#define ACTIONS_BLSR(DATA_BITS,DATA_UTYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   { ULong cf, pf, af, zf, sf, of;				\
     cf = ((DATA_UTYPE)CC_DEP2 == 0);				\
     pf = 0;							\
     af = 0;							\
     zf = ((DATA_UTYPE)CC_DEP1 == 0) << 6;			\
     sf = lshift(CC_DEP1, 8 - DATA_BITS) & 0x80;		\
     of = 0;							\
     return cf | pf | af | zf | sf | of;			\
   }								\
}



#if PROFILE_RFLAGS

static Bool initted     = False;

static UInt tabc_fast[AMD64G_CC_OP_NUMBER];
static UInt tabc_slow[AMD64G_CC_OP_NUMBER];
static UInt tab_cond[AMD64G_CC_OP_NUMBER][16];
static UInt n_calc_all  = 0;
static UInt n_calc_c    = 0;
static UInt n_calc_cond = 0;

#define SHOW_COUNTS_NOW (0 == (0x3FFFFF & (n_calc_all+n_calc_c+n_calc_cond)))


static void showCounts ( void )
{
   Int op, co;
   HChar ch;
   vex_printf("\nTotal calls: calc_all=%u   calc_cond=%u   calc_c=%u\n",
              n_calc_all, n_calc_cond, n_calc_c);

   vex_printf("      cSLOW  cFAST    O   NO    B   NB    Z   NZ   BE  NBE"
              "    S   NS    P   NP    L   NL   LE  NLE\n");
   vex_printf("     -----------------------------------------------------"
              "----------------------------------------\n");
   for (op = 0; op < AMD64G_CC_OP_NUMBER; op++) {

      ch = ' ';
      if (op > 0 && (op-1) % 4 == 0) 
         ch = 'B';
      if (op > 0 && (op-1) % 4 == 1) 
         ch = 'W';
      if (op > 0 && (op-1) % 4 == 2) 
         ch = 'L';
      if (op > 0 && (op-1) % 4 == 3) 
         ch = 'Q';

      vex_printf("%2d%c: ", op, ch);
      vex_printf("%6u ", tabc_slow[op]);
      vex_printf("%6u ", tabc_fast[op]);
      for (co = 0; co < 16; co++) {
         Int n = tab_cond[op][co];
         if (n >= 1000) {
            vex_printf(" %3dK", n / 1000);
         } else 
         if (n >= 0) {
            vex_printf(" %3d ", n );
         } else {
            vex_printf("     ");
         }
      }
      vex_printf("\n");
   }
   vex_printf("\n");
}

static void initCounts ( void )
{
   Int op, co;
   initted = True;
   for (op = 0; op < AMD64G_CC_OP_NUMBER; op++) {
      tabc_fast[op] = tabc_slow[op] = 0;
      for (co = 0; co < 16; co++)
         tab_cond[op][co] = 0;
   }
}

#endif 


static
ULong amd64g_calculate_rflags_all_WRK ( ULong cc_op, 
                                        ULong cc_dep1_formal, 
                                        ULong cc_dep2_formal,
                                        ULong cc_ndep_formal )
{
   switch (cc_op) {
      case AMD64G_CC_OP_COPY:
         return cc_dep1_formal
                & (AMD64G_CC_MASK_O | AMD64G_CC_MASK_S | AMD64G_CC_MASK_Z 
                   | AMD64G_CC_MASK_A | AMD64G_CC_MASK_C | AMD64G_CC_MASK_P);

      case AMD64G_CC_OP_ADDB:   ACTIONS_ADD( 8,  UChar  );
      case AMD64G_CC_OP_ADDW:   ACTIONS_ADD( 16, UShort );
      case AMD64G_CC_OP_ADDL:   ACTIONS_ADD( 32, UInt   );
      case AMD64G_CC_OP_ADDQ:   ACTIONS_ADD( 64, ULong  );

      case AMD64G_CC_OP_ADCB:   ACTIONS_ADC( 8,  UChar  );
      case AMD64G_CC_OP_ADCW:   ACTIONS_ADC( 16, UShort );
      case AMD64G_CC_OP_ADCL:   ACTIONS_ADC( 32, UInt   );
      case AMD64G_CC_OP_ADCQ:   ACTIONS_ADC( 64, ULong  );

      case AMD64G_CC_OP_SUBB:   ACTIONS_SUB(  8, UChar  );
      case AMD64G_CC_OP_SUBW:   ACTIONS_SUB( 16, UShort );
      case AMD64G_CC_OP_SUBL:   ACTIONS_SUB( 32, UInt   );
      case AMD64G_CC_OP_SUBQ:   ACTIONS_SUB( 64, ULong  );

      case AMD64G_CC_OP_SBBB:   ACTIONS_SBB(  8, UChar  );
      case AMD64G_CC_OP_SBBW:   ACTIONS_SBB( 16, UShort );
      case AMD64G_CC_OP_SBBL:   ACTIONS_SBB( 32, UInt   );
      case AMD64G_CC_OP_SBBQ:   ACTIONS_SBB( 64, ULong  );

      case AMD64G_CC_OP_LOGICB: ACTIONS_LOGIC(  8, UChar  );
      case AMD64G_CC_OP_LOGICW: ACTIONS_LOGIC( 16, UShort );
      case AMD64G_CC_OP_LOGICL: ACTIONS_LOGIC( 32, UInt   );
      case AMD64G_CC_OP_LOGICQ: ACTIONS_LOGIC( 64, ULong  );

      case AMD64G_CC_OP_INCB:   ACTIONS_INC(  8, UChar  );
      case AMD64G_CC_OP_INCW:   ACTIONS_INC( 16, UShort );
      case AMD64G_CC_OP_INCL:   ACTIONS_INC( 32, UInt   );
      case AMD64G_CC_OP_INCQ:   ACTIONS_INC( 64, ULong  );

      case AMD64G_CC_OP_DECB:   ACTIONS_DEC(  8, UChar  );
      case AMD64G_CC_OP_DECW:   ACTIONS_DEC( 16, UShort );
      case AMD64G_CC_OP_DECL:   ACTIONS_DEC( 32, UInt   );
      case AMD64G_CC_OP_DECQ:   ACTIONS_DEC( 64, ULong  );

      case AMD64G_CC_OP_SHLB:   ACTIONS_SHL(  8, UChar  );
      case AMD64G_CC_OP_SHLW:   ACTIONS_SHL( 16, UShort );
      case AMD64G_CC_OP_SHLL:   ACTIONS_SHL( 32, UInt   );
      case AMD64G_CC_OP_SHLQ:   ACTIONS_SHL( 64, ULong  );

      case AMD64G_CC_OP_SHRB:   ACTIONS_SHR(  8, UChar  );
      case AMD64G_CC_OP_SHRW:   ACTIONS_SHR( 16, UShort );
      case AMD64G_CC_OP_SHRL:   ACTIONS_SHR( 32, UInt   );
      case AMD64G_CC_OP_SHRQ:   ACTIONS_SHR( 64, ULong  );

      case AMD64G_CC_OP_ROLB:   ACTIONS_ROL(  8, UChar  );
      case AMD64G_CC_OP_ROLW:   ACTIONS_ROL( 16, UShort );
      case AMD64G_CC_OP_ROLL:   ACTIONS_ROL( 32, UInt   );
      case AMD64G_CC_OP_ROLQ:   ACTIONS_ROL( 64, ULong  );

      case AMD64G_CC_OP_RORB:   ACTIONS_ROR(  8, UChar  );
      case AMD64G_CC_OP_RORW:   ACTIONS_ROR( 16, UShort );
      case AMD64G_CC_OP_RORL:   ACTIONS_ROR( 32, UInt   );
      case AMD64G_CC_OP_RORQ:   ACTIONS_ROR( 64, ULong  );

      case AMD64G_CC_OP_UMULB:  ACTIONS_UMUL(  8, UChar,  toUChar,
                                                  UShort, toUShort );
      case AMD64G_CC_OP_UMULW:  ACTIONS_UMUL( 16, UShort, toUShort,
                                                  UInt,   toUInt );
      case AMD64G_CC_OP_UMULL:  ACTIONS_UMUL( 32, UInt,   toUInt,
                                                  ULong,  idULong );

      case AMD64G_CC_OP_UMULQ:  ACTIONS_UMULQ;

      case AMD64G_CC_OP_SMULB:  ACTIONS_SMUL(  8, Char,   toUChar,
                                                  Short,  toUShort );
      case AMD64G_CC_OP_SMULW:  ACTIONS_SMUL( 16, Short,  toUShort, 
                                                  Int,    toUInt   );
      case AMD64G_CC_OP_SMULL:  ACTIONS_SMUL( 32, Int,    toUInt,
                                                  Long,   idULong );

      case AMD64G_CC_OP_SMULQ:  ACTIONS_SMULQ;

      case AMD64G_CC_OP_ANDN32: ACTIONS_ANDN( 32, UInt   );
      case AMD64G_CC_OP_ANDN64: ACTIONS_ANDN( 64, ULong  );

      case AMD64G_CC_OP_BLSI32: ACTIONS_BLSI( 32, UInt   );
      case AMD64G_CC_OP_BLSI64: ACTIONS_BLSI( 64, ULong  );

      case AMD64G_CC_OP_BLSMSK32: ACTIONS_BLSMSK( 32, UInt   );
      case AMD64G_CC_OP_BLSMSK64: ACTIONS_BLSMSK( 64, ULong  );

      case AMD64G_CC_OP_BLSR32: ACTIONS_BLSR( 32, UInt   );
      case AMD64G_CC_OP_BLSR64: ACTIONS_BLSR( 64, ULong  );

      default:
         
         vex_printf("amd64g_calculate_rflags_all_WRK(AMD64)"
                    "( %llu, 0x%llx, 0x%llx, 0x%llx )\n",
                    cc_op, cc_dep1_formal, cc_dep2_formal, cc_ndep_formal );
         vpanic("amd64g_calculate_rflags_all_WRK(AMD64)");
   }
}


ULong amd64g_calculate_rflags_all ( ULong cc_op, 
                                    ULong cc_dep1, 
                                    ULong cc_dep2,
                                    ULong cc_ndep )
{
#  if PROFILE_RFLAGS
   if (!initted) initCounts();
   n_calc_all++;
   if (SHOW_COUNTS_NOW) showCounts();
#  endif
   return
      amd64g_calculate_rflags_all_WRK ( cc_op, cc_dep1, cc_dep2, cc_ndep );
}


ULong amd64g_calculate_rflags_c ( ULong cc_op, 
                                  ULong cc_dep1, 
                                  ULong cc_dep2,
                                  ULong cc_ndep )
{
#  if PROFILE_RFLAGS
   if (!initted) initCounts();
   n_calc_c++;
   tabc_fast[cc_op]++;
   if (SHOW_COUNTS_NOW) showCounts();
#  endif

   
   switch (cc_op) {
      case AMD64G_CC_OP_COPY:
         return (cc_dep1 >> AMD64G_CC_SHIFT_C) & 1;
      case AMD64G_CC_OP_LOGICQ: 
      case AMD64G_CC_OP_LOGICL: 
      case AMD64G_CC_OP_LOGICW: 
      case AMD64G_CC_OP_LOGICB:
         return 0;
	 
	 
	 
	 
	 
	 
	 
	 
	 
	 
	 
	 
      default: 
         break;
   }

#  if PROFILE_RFLAGS
   tabc_fast[cc_op]--;
   tabc_slow[cc_op]++;
#  endif

   return amd64g_calculate_rflags_all_WRK(cc_op,cc_dep1,cc_dep2,cc_ndep) 
          & AMD64G_CC_MASK_C;
}


ULong amd64g_calculate_condition ( ULong cond, 
                                   ULong cc_op, 
                                   ULong cc_dep1, 
                                   ULong cc_dep2,
                                   ULong cc_ndep )
{
   ULong rflags = amd64g_calculate_rflags_all_WRK(cc_op, cc_dep1, 
                                                  cc_dep2, cc_ndep);
   ULong of,sf,zf,cf,pf;
   ULong inv = cond & 1;

#  if PROFILE_RFLAGS
   if (!initted) initCounts();
   tab_cond[cc_op][cond]++;
   n_calc_cond++;
   if (SHOW_COUNTS_NOW) showCounts();
#  endif

   switch (cond) {
      case AMD64CondNO:
      case AMD64CondO: 
         of = rflags >> AMD64G_CC_SHIFT_O;
         return 1 & (inv ^ of);

      case AMD64CondNZ:
      case AMD64CondZ: 
         zf = rflags >> AMD64G_CC_SHIFT_Z;
         return 1 & (inv ^ zf);

      case AMD64CondNB:
      case AMD64CondB: 
         cf = rflags >> AMD64G_CC_SHIFT_C;
         return 1 & (inv ^ cf);
         break;

      case AMD64CondNBE:
      case AMD64CondBE: 
         cf = rflags >> AMD64G_CC_SHIFT_C;
         zf = rflags >> AMD64G_CC_SHIFT_Z;
         return 1 & (inv ^ (cf | zf));
         break;

      case AMD64CondNS:
      case AMD64CondS: 
         sf = rflags >> AMD64G_CC_SHIFT_S;
         return 1 & (inv ^ sf);

      case AMD64CondNP:
      case AMD64CondP: 
         pf = rflags >> AMD64G_CC_SHIFT_P;
         return 1 & (inv ^ pf);

      case AMD64CondNL:
      case AMD64CondL: 
         sf = rflags >> AMD64G_CC_SHIFT_S;
         of = rflags >> AMD64G_CC_SHIFT_O;
         return 1 & (inv ^ (sf ^ of));
         break;

      case AMD64CondNLE:
      case AMD64CondLE: 
         sf = rflags >> AMD64G_CC_SHIFT_S;
         of = rflags >> AMD64G_CC_SHIFT_O;
         zf = rflags >> AMD64G_CC_SHIFT_Z;
         return 1 & (inv ^ ((sf ^ of) | zf));
         break;

      default:
         
         vex_printf("amd64g_calculate_condition"
                    "( %llu, %llu, 0x%llx, 0x%llx, 0x%llx )\n",
                    cond, cc_op, cc_dep1, cc_dep2, cc_ndep );
         vpanic("amd64g_calculate_condition");
   }
}


ULong LibVEX_GuestAMD64_get_rflags ( const VexGuestAMD64State* vex_state )
{
   ULong rflags = amd64g_calculate_rflags_all_WRK(
                     vex_state->guest_CC_OP,
                     vex_state->guest_CC_DEP1,
                     vex_state->guest_CC_DEP2,
                     vex_state->guest_CC_NDEP
                  );
   Long dflag = vex_state->guest_DFLAG;
   vassert(dflag == 1 || dflag == -1);
   if (dflag == -1)
      rflags |= (1<<10);
   if (vex_state->guest_IDFLAG == 1)
      rflags |= (1<<21);
   if (vex_state->guest_ACFLAG == 1)
      rflags |= (1<<18);

   return rflags;
}

void
LibVEX_GuestAMD64_put_rflag_c ( ULong new_carry_flag,
                               VexGuestAMD64State* vex_state )
{
   ULong oszacp = amd64g_calculate_rflags_all_WRK(
                     vex_state->guest_CC_OP,
                     vex_state->guest_CC_DEP1,
                     vex_state->guest_CC_DEP2,
                     vex_state->guest_CC_NDEP
                  );
   if (new_carry_flag & 1) {
      oszacp |= AMD64G_CC_MASK_C;
   } else {
      oszacp &= ~AMD64G_CC_MASK_C;
   }
   vex_state->guest_CC_OP   = AMD64G_CC_OP_COPY;
   vex_state->guest_CC_DEP1 = oszacp;
   vex_state->guest_CC_DEP2 = 0;
   vex_state->guest_CC_NDEP = 0;
}




static Bool isU64 ( IRExpr* e, ULong n )
{
   return toBool( e->tag == Iex_Const
                  && e->Iex.Const.con->tag == Ico_U64
                  && e->Iex.Const.con->Ico.U64 == n );
}

IRExpr* guest_amd64_spechelper ( const HChar* function_name,
                                 IRExpr** args,
                                 IRStmt** precedingStmts,
                                 Int      n_precedingStmts )
{
#  define unop(_op,_a1) IRExpr_Unop((_op),(_a1))
#  define binop(_op,_a1,_a2) IRExpr_Binop((_op),(_a1),(_a2))
#  define mkU64(_n) IRExpr_Const(IRConst_U64(_n))
#  define mkU32(_n) IRExpr_Const(IRConst_U32(_n))
#  define mkU8(_n)  IRExpr_Const(IRConst_U8(_n))

   Int i, arity = 0;
   for (i = 0; args[i]; i++)
      arity++;
#  if 0
   vex_printf("spec request:\n");
   vex_printf("   %s  ", function_name);
   for (i = 0; i < arity; i++) {
      vex_printf("  ");
      ppIRExpr(args[i]);
   }
   vex_printf("\n");
#  endif

   

   if (vex_streq(function_name, "amd64g_calculate_condition")) {
      
      IRExpr *cond, *cc_op, *cc_dep1, *cc_dep2;
      vassert(arity == 5);
      cond    = args[0];
      cc_op   = args[1];
      cc_dep1 = args[2];
      cc_dep2 = args[3];

      

      if (isU64(cc_op, AMD64G_CC_OP_ADDQ) && isU64(cond, AMD64CondZ)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpEQ64, 
                           binop(Iop_Add64, cc_dep1, cc_dep2),
                           mkU64(0)));
      }

      

      if (isU64(cc_op, AMD64G_CC_OP_ADDL) && isU64(cond, AMD64CondO)) {
         vassert(isIRAtom(cc_dep1));
         vassert(isIRAtom(cc_dep2));
         return
            binop(Iop_And64,
                  binop(Iop_Shr64,
                        binop(Iop_And64,
                              unop(Iop_Not64,
                                   binop(Iop_Xor64, cc_dep1, cc_dep2)),
                              binop(Iop_Xor64,
                                    cc_dep1,
                                    binop(Iop_Add64, cc_dep1, cc_dep2))),
                        mkU8(31)),
                  mkU64(1));

      }

      

      
      if (isU64(cc_op, AMD64G_CC_OP_SUBQ) && isU64(cond, AMD64CondO)) {
         vassert(isIRAtom(cc_dep1));
         vassert(isIRAtom(cc_dep2));
         return binop(Iop_Shr64,
                      binop(Iop_And64,
                            binop(Iop_Xor64, cc_dep1, cc_dep2),
                            binop(Iop_Xor64,
                                  cc_dep1,
                                  binop(Iop_Sub64, cc_dep1, cc_dep2))),
                      mkU8(64));
      }
      if (isU64(cc_op, AMD64G_CC_OP_SUBQ) && isU64(cond, AMD64CondNO)) {
         
      }

      
      if (isU64(cc_op, AMD64G_CC_OP_SUBQ) && isU64(cond, AMD64CondB)) {
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLT64U, cc_dep1, cc_dep2));
      }
      if (isU64(cc_op, AMD64G_CC_OP_SUBQ) && isU64(cond, AMD64CondNB)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLE64U, cc_dep2, cc_dep1));
      }

      
      if (isU64(cc_op, AMD64G_CC_OP_SUBQ) && isU64(cond, AMD64CondZ)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpEQ64,cc_dep1,cc_dep2));
      }
      if (isU64(cc_op, AMD64G_CC_OP_SUBQ) && isU64(cond, AMD64CondNZ)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpNE64,cc_dep1,cc_dep2));
      }

      
      if (isU64(cc_op, AMD64G_CC_OP_SUBQ) && isU64(cond, AMD64CondBE)) {
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLE64U, cc_dep1, cc_dep2));
      }
      if (isU64(cc_op, AMD64G_CC_OP_SUBQ) && isU64(cond, AMD64CondNBE)) {
         return binop(Iop_Xor64,
                      unop(Iop_1Uto64,
                           binop(Iop_CmpLE64U, cc_dep1, cc_dep2)),
                      mkU64(1));
      }

      
      if (isU64(cc_op, AMD64G_CC_OP_SUBQ) && isU64(cond, AMD64CondS)) {
         return binop(Iop_Shr64,
                      binop(Iop_Sub64, cc_dep1, cc_dep2),
                      mkU8(63));
      }
      if (isU64(cc_op, AMD64G_CC_OP_SUBQ) && isU64(cond, AMD64CondNS)) {
         return binop(Iop_Xor64,
                      binop(Iop_Shr64,
                            binop(Iop_Sub64, cc_dep1, cc_dep2),
                            mkU8(63)),
                      mkU64(1));
      }

      
      if (isU64(cc_op, AMD64G_CC_OP_SUBQ) && isU64(cond, AMD64CondL)) {
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLT64S, cc_dep1, cc_dep2));
      }
      if (isU64(cc_op, AMD64G_CC_OP_SUBQ) && isU64(cond, AMD64CondNL)) {
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLE64S, cc_dep2, cc_dep1));
      }

      
      if (isU64(cc_op, AMD64G_CC_OP_SUBQ) && isU64(cond, AMD64CondLE)) {
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLE64S, cc_dep1, cc_dep2));
      }
      if (isU64(cc_op, AMD64G_CC_OP_SUBQ) && isU64(cond, AMD64CondNLE)) {
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLT64S, cc_dep2, cc_dep1));

      }

      

      
      if (isU64(cc_op, AMD64G_CC_OP_SUBL) && isU64(cond, AMD64CondO)) {
         vassert(isIRAtom(cc_dep1));
         vassert(isIRAtom(cc_dep2));
         return
            binop(Iop_And64,
                  binop(Iop_Shr64,
                        binop(Iop_And64,
                              binop(Iop_Xor64, cc_dep1, cc_dep2),
                              binop(Iop_Xor64,
                                    cc_dep1,
                                    binop(Iop_Sub64, cc_dep1, cc_dep2))),
                        mkU8(31)),
                  mkU64(1));
      }
      if (isU64(cc_op, AMD64G_CC_OP_SUBL) && isU64(cond, AMD64CondNO)) {
         
      }

      
      if (isU64(cc_op, AMD64G_CC_OP_SUBL) && isU64(cond, AMD64CondB)) {
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLT32U,
                           unop(Iop_64to32, cc_dep1),
                           unop(Iop_64to32, cc_dep2)));
      }
      if (isU64(cc_op, AMD64G_CC_OP_SUBL) && isU64(cond, AMD64CondNB)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLE32U,
                           unop(Iop_64to32, cc_dep2),
                           unop(Iop_64to32, cc_dep1)));
      }

      
      if (isU64(cc_op, AMD64G_CC_OP_SUBL) && isU64(cond, AMD64CondZ)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpEQ32,
                           unop(Iop_64to32, cc_dep1),
                           unop(Iop_64to32, cc_dep2)));
      }
      if (isU64(cc_op, AMD64G_CC_OP_SUBL) && isU64(cond, AMD64CondNZ)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpNE32,
                           unop(Iop_64to32, cc_dep1),
                           unop(Iop_64to32, cc_dep2)));
      }

      
      if (isU64(cc_op, AMD64G_CC_OP_SUBL) && isU64(cond, AMD64CondBE)) {
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLE32U, 
                           unop(Iop_64to32, cc_dep1),
                           unop(Iop_64to32, cc_dep2)));
      }
      if (isU64(cc_op, AMD64G_CC_OP_SUBL) && isU64(cond, AMD64CondNBE)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLT32U, 
                           unop(Iop_64to32, cc_dep2),
                           unop(Iop_64to32, cc_dep1)));
      }

      
      if (isU64(cc_op, AMD64G_CC_OP_SUBL) && isU64(cond, AMD64CondS)) {
         return binop(Iop_And64,
                      binop(Iop_Shr64,
                            binop(Iop_Sub64, cc_dep1, cc_dep2),
                            mkU8(31)),
                      mkU64(1));
      }
      if (isU64(cc_op, AMD64G_CC_OP_SUBL) && isU64(cond, AMD64CondNS)) {
         return binop(Iop_Xor64,
                      binop(Iop_And64,
                            binop(Iop_Shr64,
                                  binop(Iop_Sub64, cc_dep1, cc_dep2),
                                  mkU8(31)),
                            mkU64(1)),
                      mkU64(1));
      }

      
      if (isU64(cc_op, AMD64G_CC_OP_SUBL) && isU64(cond, AMD64CondL)) {
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLT32S,
                           unop(Iop_64to32, cc_dep1),
                           unop(Iop_64to32, cc_dep2)));
      }
      if (isU64(cc_op, AMD64G_CC_OP_SUBL) && isU64(cond, AMD64CondNL)) {
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLE32S,
                           unop(Iop_64to32, cc_dep2),
                           unop(Iop_64to32, cc_dep1)));
      }

      
      if (isU64(cc_op, AMD64G_CC_OP_SUBL) && isU64(cond, AMD64CondLE)) {
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLE32S,
                           unop(Iop_64to32, cc_dep1),
                           unop(Iop_64to32, cc_dep2)));

      }
      if (isU64(cc_op, AMD64G_CC_OP_SUBL) && isU64(cond, AMD64CondNLE)) {
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLT32S,
                           unop(Iop_64to32, cc_dep2),
                           unop(Iop_64to32, cc_dep1)));

      }

      

      
      if (isU64(cc_op, AMD64G_CC_OP_SUBW) && isU64(cond, AMD64CondZ)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpEQ16, 
                           unop(Iop_64to16,cc_dep1),
                           unop(Iop_64to16,cc_dep2)));
      }
      if (isU64(cc_op, AMD64G_CC_OP_SUBW) && isU64(cond, AMD64CondNZ)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpNE16, 
                           unop(Iop_64to16,cc_dep1),
                           unop(Iop_64to16,cc_dep2)));
      }

      
      if (isU64(cc_op, AMD64G_CC_OP_SUBW) && isU64(cond, AMD64CondBE)) {
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLE64U,
                           binop(Iop_Shl64, cc_dep1, mkU8(48)),
                           binop(Iop_Shl64, cc_dep2, mkU8(48))));
      }

      
      if (isU64(cc_op, AMD64G_CC_OP_SUBW) && isU64(cond, AMD64CondLE)) {
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLE64S, 
                           binop(Iop_Shl64,cc_dep1,mkU8(48)),
                           binop(Iop_Shl64,cc_dep2,mkU8(48))));

      }

      

      
      if (isU64(cc_op, AMD64G_CC_OP_SUBB) && isU64(cond, AMD64CondB)) {
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLT64U,
                           binop(Iop_And64, cc_dep1, mkU64(0xFF)),
                           binop(Iop_And64, cc_dep2, mkU64(0xFF))));
      }
      if (isU64(cc_op, AMD64G_CC_OP_SUBB) && isU64(cond, AMD64CondNB)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLE64U,
                           binop(Iop_And64, cc_dep2, mkU64(0xFF)),
                           binop(Iop_And64, cc_dep1, mkU64(0xFF))));
      }

      
      if (isU64(cc_op, AMD64G_CC_OP_SUBB) && isU64(cond, AMD64CondZ)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpEQ8, 
                           unop(Iop_64to8,cc_dep1),
                           unop(Iop_64to8,cc_dep2)));
      }
      if (isU64(cc_op, AMD64G_CC_OP_SUBB) && isU64(cond, AMD64CondNZ)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpNE8, 
                           unop(Iop_64to8,cc_dep1),
                           unop(Iop_64to8,cc_dep2)));
      }

      
      if (isU64(cc_op, AMD64G_CC_OP_SUBB) && isU64(cond, AMD64CondBE)) {
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLE64U, 
                           binop(Iop_And64, cc_dep1, mkU64(0xFF)),
                           binop(Iop_And64, cc_dep2, mkU64(0xFF))));
      }

      
      if (isU64(cc_op, AMD64G_CC_OP_SUBB) && isU64(cond, AMD64CondS)
                                          && isU64(cc_dep2, 0)) {
         return binop(Iop_And64,
                      binop(Iop_Shr64,cc_dep1,mkU8(7)),
                      mkU64(1));
      }
      if (isU64(cc_op, AMD64G_CC_OP_SUBB) && isU64(cond, AMD64CondNS)
                                          && isU64(cc_dep2, 0)) {
         return binop(Iop_Xor64,
                      binop(Iop_And64,
                            binop(Iop_Shr64,cc_dep1,mkU8(7)),
                            mkU64(1)),
                      mkU64(1));
      }

      

      if (isU64(cc_op, AMD64G_CC_OP_LOGICQ) && isU64(cond, AMD64CondZ)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpEQ64, cc_dep1, mkU64(0)));
      }
      if (isU64(cc_op, AMD64G_CC_OP_LOGICQ) && isU64(cond, AMD64CondNZ)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpNE64, cc_dep1, mkU64(0)));
      }

      if (isU64(cc_op, AMD64G_CC_OP_LOGICQ) && isU64(cond, AMD64CondL)) {
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLT64S, 
                           cc_dep1, 
                           mkU64(0)));
      }

      

      if (isU64(cc_op, AMD64G_CC_OP_LOGICL) && isU64(cond, AMD64CondZ)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpEQ32,
                           unop(Iop_64to32, cc_dep1), 
                           mkU32(0)));
      }
      if (isU64(cc_op, AMD64G_CC_OP_LOGICL) && isU64(cond, AMD64CondNZ)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpNE32,
                           unop(Iop_64to32, cc_dep1), 
                           mkU32(0)));
      }

      if (isU64(cc_op, AMD64G_CC_OP_LOGICL) && isU64(cond, AMD64CondLE)) {
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLE32S,
                           unop(Iop_64to32, cc_dep1), 
                           mkU32(0)));
      }

      if (isU64(cc_op, AMD64G_CC_OP_LOGICL) && isU64(cond, AMD64CondS)) {
         
         return binop(Iop_And64,
                      binop(Iop_Shr64, cc_dep1, mkU8(31)),
                      mkU64(1));
      }
      if (isU64(cc_op, AMD64G_CC_OP_LOGICL) && isU64(cond, AMD64CondNS)) {
         
         return binop(Iop_Xor64,
                binop(Iop_And64,
                      binop(Iop_Shr64, cc_dep1, mkU8(31)),
                      mkU64(1)),
                mkU64(1));
      }

      

      if (isU64(cc_op, AMD64G_CC_OP_LOGICW) && isU64(cond, AMD64CondZ)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpEQ64,
                           binop(Iop_And64, cc_dep1, mkU64(0xFFFF)),
                           mkU64(0)));
      }
      if (isU64(cc_op, AMD64G_CC_OP_LOGICW) && isU64(cond, AMD64CondNZ)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpNE64,
                           binop(Iop_And64, cc_dep1, mkU64(0xFFFF)),
                           mkU64(0)));
      }

      

      if (isU64(cc_op, AMD64G_CC_OP_LOGICB) && isU64(cond, AMD64CondZ)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpEQ64, binop(Iop_And64,cc_dep1,mkU64(255)), 
                                        mkU64(0)));
      }
      if (isU64(cc_op, AMD64G_CC_OP_LOGICB) && isU64(cond, AMD64CondNZ)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpNE64, binop(Iop_And64,cc_dep1,mkU64(255)), 
                                        mkU64(0)));
      }

      if (isU64(cc_op, AMD64G_CC_OP_LOGICB) && isU64(cond, AMD64CondS)) {
         
         return binop(Iop_And64,
                      binop(Iop_Shr64,cc_dep1,mkU8(7)),
                      mkU64(1));
      }
      if (isU64(cc_op, AMD64G_CC_OP_LOGICB) && isU64(cond, AMD64CondNS)) {
         
         return binop(Iop_Xor64,
                      binop(Iop_And64,
                            binop(Iop_Shr64,cc_dep1,mkU8(7)),
                            mkU64(1)),
                      mkU64(1));
      }

      

      if (isU64(cc_op, AMD64G_CC_OP_INCB) && isU64(cond, AMD64CondLE)) {
         
         return binop(Iop_And64,
                      binop(Iop_Shr64,
                            binop(Iop_Sub64, cc_dep1, mkU64(1)),
                            mkU8(7)),
                      mkU64(1));
      }

      

      if (isU64(cc_op, AMD64G_CC_OP_INCW) && isU64(cond, AMD64CondZ)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpEQ64, 
                           binop(Iop_Shl64,cc_dep1,mkU8(48)), 
                           mkU64(0)));
      }

      

      if (isU64(cc_op, AMD64G_CC_OP_DECL) && isU64(cond, AMD64CondZ)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpEQ32,
                           unop(Iop_64to32, cc_dep1),
                           mkU32(0)));
      }

      

      if (isU64(cc_op, AMD64G_CC_OP_DECW) && isU64(cond, AMD64CondNZ)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpNE64, 
                           binop(Iop_Shl64,cc_dep1,mkU8(48)), 
                           mkU64(0)));
      }

      

      if (isU64(cc_op, AMD64G_CC_OP_COPY) && 
          (isU64(cond, AMD64CondBE) || isU64(cond, AMD64CondNBE))) {
         ULong nnn = isU64(cond, AMD64CondBE) ? 1 : 0;
         return
            unop(
               Iop_1Uto64,
               binop(
                  Iop_CmpEQ64,
                  binop(
                     Iop_And64,
                     binop(
                        Iop_Or64,
                        binop(Iop_Shr64, cc_dep1, mkU8(AMD64G_CC_SHIFT_C)),
                        binop(Iop_Shr64, cc_dep1, mkU8(AMD64G_CC_SHIFT_Z))
                     ),
                     mkU64(1)
                  ),
                  mkU64(nnn)
               )
            );
      }
      
      if (isU64(cc_op, AMD64G_CC_OP_COPY) && isU64(cond, AMD64CondB)) {
         
         return
            unop(
               Iop_1Uto64,
               binop(
                  Iop_CmpNE64,
                  binop(
                     Iop_And64,
                     binop(Iop_Shr64, cc_dep1, mkU8(AMD64G_CC_SHIFT_C)),
                     mkU64(1)
                  ),
                  mkU64(0)
               )
            );
      }

      if (isU64(cc_op, AMD64G_CC_OP_COPY) 
          && (isU64(cond, AMD64CondZ) || isU64(cond, AMD64CondNZ))) {
         
         
         UInt nnn = isU64(cond, AMD64CondZ) ? 1 : 0;
         return
            unop(
               Iop_1Uto64,
               binop(
                  Iop_CmpEQ64,
                  binop(
                     Iop_And64,
                     binop(Iop_Shr64, cc_dep1, mkU8(AMD64G_CC_SHIFT_Z)),
                     mkU64(1)
                  ),
                  mkU64(nnn)
               )
            );
      }

      if (isU64(cc_op, AMD64G_CC_OP_COPY) && isU64(cond, AMD64CondP)) {
         
         return
            unop(
               Iop_1Uto64,
               binop(
                  Iop_CmpNE64,
                  binop(
                     Iop_And64,
                     binop(Iop_Shr64, cc_dep1, mkU8(AMD64G_CC_SHIFT_P)),
                     mkU64(1)
                  ),
                  mkU64(0)
               )
            );
      }

      return NULL;
   }

   

   if (vex_streq(function_name, "amd64g_calculate_rflags_c")) {
      
      IRExpr *cc_op, *cc_dep1, *cc_dep2, *cc_ndep;
      vassert(arity == 4);
      cc_op   = args[0];
      cc_dep1 = args[1];
      cc_dep2 = args[2];
      cc_ndep = args[3];

      if (isU64(cc_op, AMD64G_CC_OP_SUBQ)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLT64U, 
                           cc_dep1,
                           cc_dep2));
      }
      if (isU64(cc_op, AMD64G_CC_OP_SUBL)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLT32U,
                           unop(Iop_64to32, cc_dep1), 
                           unop(Iop_64to32, cc_dep2)));
      }
      if (isU64(cc_op, AMD64G_CC_OP_SUBB)) {
         
         return unop(Iop_1Uto64,
                     binop(Iop_CmpLT64U, 
                           binop(Iop_And64,cc_dep1,mkU64(0xFF)),
                           binop(Iop_And64,cc_dep2,mkU64(0xFF))));
      }
      if (isU64(cc_op, AMD64G_CC_OP_LOGICQ)
          || isU64(cc_op, AMD64G_CC_OP_LOGICL)
          || isU64(cc_op, AMD64G_CC_OP_LOGICW)
          || isU64(cc_op, AMD64G_CC_OP_LOGICB)) {
         
         return mkU64(0);
      }
      if (isU64(cc_op, AMD64G_CC_OP_DECL) || isU64(cc_op, AMD64G_CC_OP_INCL)
          || isU64(cc_op, AMD64G_CC_OP_DECQ) || isU64(cc_op, AMD64G_CC_OP_INCQ)) {
         
         return cc_ndep;
      }

#     if 0
      if (cc_op->tag == Iex_Const) {
         vex_printf("CFLAG "); ppIRExpr(cc_op); vex_printf("\n");
      }
#     endif

      return NULL;
   }

#  undef unop
#  undef binop
#  undef mkU64
#  undef mkU32
#  undef mkU8

   return NULL;
}



static inline Bool host_is_little_endian ( void )
{
   UInt x = 0x76543210;
   UChar* p = (UChar*)(&x);
   return toBool(*p == 0x10);
}

ULong amd64g_calculate_FXAM ( ULong tag, ULong dbl ) 
{
   Bool   mantissaIsZero;
   Int    bexp;
   UChar  sign;
   UChar* f64;

   vassert(host_is_little_endian());

   

   f64  = (UChar*)(&dbl);
   sign = toUChar( (f64[7] >> 7) & 1 );

   if (tag == 0) {
      
      return AMD64G_FC_MASK_C3 | 0 | (sign << AMD64G_FC_SHIFT_C1) 
                                   | AMD64G_FC_MASK_C0;
   }

   bexp = (f64[7] << 4) | ((f64[6] >> 4) & 0x0F);
   bexp &= 0x7FF;

   mantissaIsZero
      = toBool(
           (f64[6] & 0x0F) == 0 
           && (f64[5] | f64[4] | f64[3] | f64[2] | f64[1] | f64[0]) == 0
        );

   if (bexp == 0 && mantissaIsZero) {
      
      return AMD64G_FC_MASK_C3 | 0 
                               | (sign << AMD64G_FC_SHIFT_C1) | 0;
   }
   
   if (bexp == 0 && !mantissaIsZero) {
      
      return AMD64G_FC_MASK_C3 | AMD64G_FC_MASK_C2 
                               | (sign << AMD64G_FC_SHIFT_C1) | 0;
   }

   if (bexp == 0x7FF && mantissaIsZero) {
      
      return 0 | AMD64G_FC_MASK_C2 | (sign << AMD64G_FC_SHIFT_C1) 
                                   | AMD64G_FC_MASK_C0;
   }

   if (bexp == 0x7FF && !mantissaIsZero) {
      
      return 0 | 0 | (sign << AMD64G_FC_SHIFT_C1) | AMD64G_FC_MASK_C0;
   }

   
   return 0 | AMD64G_FC_MASK_C2 | (sign << AMD64G_FC_SHIFT_C1) | 0;
}


static
VexEmNote do_put_x87 ( Bool moveRegs,
                       UChar* x87_state,
                       VexGuestAMD64State* vex_state )
{
   Int        stno, preg;
   UInt       tag;
   ULong*     vexRegs = (ULong*)(&vex_state->guest_FPREG[0]);
   UChar*     vexTags = (UChar*)(&vex_state->guest_FPTAG[0]);
   Fpu_State* x87     = (Fpu_State*)x87_state;
   UInt       ftop    = (x87->env[FP_ENV_STAT] >> 11) & 7;
   UInt       tagw    = x87->env[FP_ENV_TAG];
   UInt       fpucw   = x87->env[FP_ENV_CTRL];
   UInt       c3210   = x87->env[FP_ENV_STAT] & 0x4700;
   VexEmNote  ew;
   UInt       fpround;
   ULong      pair;

   
   for (stno = 0; stno < 8; stno++) {
      preg = (stno + ftop) & 7;
      tag = (tagw >> (2*preg)) & 3;
      if (tag == 3) {
         
         /* hmm, if it's empty, does it still get written?  Probably
            safer to say it does.  If we don't, memcheck could get out
            of sync, in that it thinks all FP registers are defined by
            this helper, but in reality some have not been updated. */
         if (moveRegs)
            vexRegs[preg] = 0; 
         vexTags[preg] = 0;
      } else {
         
         if (moveRegs)
            convert_f80le_to_f64le( &x87->reg[10*stno], 
                                    (UChar*)&vexRegs[preg] );
         vexTags[preg] = 1;
      }
   }

   
   vex_state->guest_FTOP = ftop;

   
   vex_state->guest_FC3210 = c3210;

   pair    = amd64g_check_fldcw ( (ULong)fpucw );
   fpround = (UInt)pair & 0xFFFFFFFFULL;
   ew      = (VexEmNote)(pair >> 32);
   
   vex_state->guest_FPROUND = fpround & 3;

   
   return ew;
}


static
void do_get_x87 ( VexGuestAMD64State* vex_state,
                  UChar* x87_state )
{
   Int        i, stno, preg;
   UInt       tagw;
   ULong*     vexRegs = (ULong*)(&vex_state->guest_FPREG[0]);
   UChar*     vexTags = (UChar*)(&vex_state->guest_FPTAG[0]);
   Fpu_State* x87     = (Fpu_State*)x87_state;
   UInt       ftop    = vex_state->guest_FTOP;
   UInt       c3210   = vex_state->guest_FC3210;

   for (i = 0; i < 14; i++)
      x87->env[i] = 0;

   x87->env[1] = x87->env[3] = x87->env[5] = x87->env[13] = 0xFFFF;
   x87->env[FP_ENV_STAT] 
      = toUShort(((ftop & 7) << 11) | (c3210 & 0x4700));
   x87->env[FP_ENV_CTRL] 
      = toUShort(amd64g_create_fpucw( vex_state->guest_FPROUND ));

   
   tagw = 0;
   for (stno = 0; stno < 8; stno++) {
      preg = (stno + ftop) & 7;
      if (vexTags[preg] == 0) {
         
         tagw |= (3 << (2*preg));
         convert_f64le_to_f80le( (UChar*)&vexRegs[preg], 
                                 &x87->reg[10*stno] );
      } else {
         
         tagw |= (0 << (2*preg));
         convert_f64le_to_f80le( (UChar*)&vexRegs[preg], 
                                 &x87->reg[10*stno] );
      }
   }
   x87->env[FP_ENV_TAG] = toUShort(tagw);
}


void amd64g_dirtyhelper_FXSAVE_ALL_EXCEPT_XMM ( VexGuestAMD64State* gst,
                                                HWord addr )
{
   
   Fpu_State tmp;
   UShort*   addrS = (UShort*)addr;
   UChar*    addrC = (UChar*)addr;
   UInt      mxcsr;
   UShort    fp_tags;
   UInt      summary_tags;
   Int       r, stno;
   UShort    *srcS, *dstS;

   do_get_x87( gst, (UChar*)&tmp );
   mxcsr = amd64g_create_mxcsr( gst->guest_SSEROUND );


   addrS[0]  = tmp.env[FP_ENV_CTRL]; 
   addrS[1]  = tmp.env[FP_ENV_STAT]; 

   
   summary_tags = 0;
   fp_tags = tmp.env[FP_ENV_TAG];
   for (r = 0; r < 8; r++) {
      if ( ((fp_tags >> (2*r)) & 3) != 3 )
         summary_tags |= (1 << r);
   }
   addrC[4]  = toUChar(summary_tags); 
   addrC[5]  = 0; 

   addrS[3]  = 0; 

   addrS[4]  = 0; 
   addrS[5]  = 0; 
   addrS[6]  = 0; 
   addrS[7]  = 0; 

   addrS[8]  = 0; 
   addrS[9]  = 0; 
   addrS[10] = 0; 
   addrS[11] = 0; 

   addrS[12] = toUShort(mxcsr);  
   addrS[13] = toUShort(mxcsr >> 16);

   addrS[14] = 0xFFFF; 
   addrS[15] = 0x0000; 

   
   for (stno = 0; stno < 8; stno++) {
      srcS = (UShort*)(&tmp.reg[10*stno]);
      dstS = (UShort*)(&addrS[16 + 8*stno]);
      dstS[0] = srcS[0];
      dstS[1] = srcS[1];
      dstS[2] = srcS[2];
      dstS[3] = srcS[3];
      dstS[4] = srcS[4];
      dstS[5] = 0;
      dstS[6] = 0;
      dstS[7] = 0;
   }

}


VexEmNote amd64g_dirtyhelper_FXRSTOR_ALL_EXCEPT_XMM ( VexGuestAMD64State* gst,
                                                      HWord addr )
{
   Fpu_State tmp;
   VexEmNote warnX87 = EmNote_NONE;
   VexEmNote warnXMM = EmNote_NONE;
   UShort*   addrS   = (UShort*)addr;
   UChar*    addrC   = (UChar*)addr;
   UShort    fp_tags;
   Int       r, stno, i;


   for (i = 0; i < 14; i++) tmp.env[i] = 0;
   for (i = 0; i < 80; i++) tmp.reg[i] = 0;
   
   for (stno = 0; stno < 8; stno++) {
      UShort* dstS = (UShort*)(&tmp.reg[10*stno]);
      UShort* srcS = (UShort*)(&addrS[16 + 8*stno]);
      dstS[0] = srcS[0];
      dstS[1] = srcS[1];
      dstS[2] = srcS[2];
      dstS[3] = srcS[3];
      dstS[4] = srcS[4];
   }
   
   tmp.env[FP_ENV_CTRL] = addrS[0]; 
   tmp.env[FP_ENV_STAT] = addrS[1]; 

   fp_tags = 0;
   for (r = 0; r < 8; r++) {
      if (addrC[4] & (1<<r))
         fp_tags |= (0 << (2*r)); 
      else 
         fp_tags |= (3 << (2*r)); 
   }
   tmp.env[FP_ENV_TAG] = fp_tags;

   
   warnX87 = do_put_x87( True, (UChar*)&tmp, gst );

   { UInt w32 = (((UInt)addrS[12]) & 0xFFFF)
                | ((((UInt)addrS[13]) & 0xFFFF) << 16);
     ULong w64 = amd64g_check_ldmxcsr( (ULong)w32 );

     warnXMM = (VexEmNote)(w64 >> 32);

     gst->guest_SSEROUND = w64 & 0xFFFFFFFFULL;
   }

   
   if (warnX87 != EmNote_NONE)
      return warnX87;
   else
      return warnXMM;
}


void amd64g_dirtyhelper_FINIT ( VexGuestAMD64State* gst )
{
   Int i;
   gst->guest_FTOP = 0;
   for (i = 0; i < 8; i++) {
      gst->guest_FPTAG[i] = 0; 
      gst->guest_FPREG[i] = 0; 
   }
   gst->guest_FPROUND = (ULong)Irrm_NEAREST;
   gst->guest_FC3210  = 0;
}


ULong amd64g_dirtyhelper_loadF80le ( Addr addrU )
{
   ULong f64;
   convert_f80le_to_f64le ( (UChar*)addrU, (UChar*)&f64 );
   return f64;
}

void amd64g_dirtyhelper_storeF80le ( Addr addrU, ULong f64 )
{
   convert_f64le_to_f80le( (UChar*)&f64, (UChar*)addrU );
}


ULong amd64g_check_ldmxcsr ( ULong mxcsr )
{
   
   
   ULong rmode = (mxcsr >> 13) & 3;

   
   VexEmNote ew = EmNote_NONE;

   if ((mxcsr & 0x1F80) != 0x1F80) {
      
      ew = EmWarn_X86_sseExns;
   }
   else 
   if (mxcsr & (1<<15)) {
      
      ew = EmWarn_X86_fz;
   } 
   else
   if (mxcsr & (1<<6)) {
      
      ew = EmWarn_X86_daz;
   }

   return (((ULong)ew) << 32) | ((ULong)rmode);
}


ULong amd64g_create_mxcsr ( ULong sseround )
{
   sseround &= 3;
   return 0x1F80 | (sseround << 13);
}


ULong amd64g_check_fldcw ( ULong fpucw )
{
   
   
   ULong rmode = (fpucw >> 10) & 3;

   
   VexEmNote ew = EmNote_NONE;

   if ((fpucw & 0x3F) != 0x3F) {
      
      ew = EmWarn_X86_x87exns;
   }
   else
   if (((fpucw >> 8) & 3) != 3) {
      
      ew = EmWarn_X86_x87precision;
   }

   return (((ULong)ew) << 32) | ((ULong)rmode);
}


ULong amd64g_create_fpucw ( ULong fpround )
{
   fpround &= 3;
   return 0x037F | (fpround << 10);
}


VexEmNote amd64g_dirtyhelper_FLDENV ( VexGuestAMD64State* vex_state,
                                      HWord x87_state)
{
   return do_put_x87( False, (UChar*)x87_state, vex_state );
}


void amd64g_dirtyhelper_FSTENV ( VexGuestAMD64State* vex_state,
                                 HWord x87_state )
{
   Int        i, stno, preg;
   UInt       tagw;
   UChar*     vexTags = (UChar*)(&vex_state->guest_FPTAG[0]);
   Fpu_State* x87     = (Fpu_State*)x87_state;
   UInt       ftop    = vex_state->guest_FTOP;
   ULong      c3210   = vex_state->guest_FC3210;

   for (i = 0; i < 14; i++)
      x87->env[i] = 0;

   x87->env[1] = x87->env[3] = x87->env[5] = x87->env[13] = 0xFFFF;
   x87->env[FP_ENV_STAT] 
      = toUShort(toUInt( ((ftop & 7) << 11) | (c3210 & 0x4700) ));
   x87->env[FP_ENV_CTRL] 
      = toUShort(toUInt( amd64g_create_fpucw( vex_state->guest_FPROUND ) ));

   
   tagw = 0;
   for (stno = 0; stno < 8; stno++) {
      preg = (stno + ftop) & 7;
      if (vexTags[preg] == 0) {
         
         tagw |= (3 << (2*preg));
      } else {
         
         tagw |= (0 << (2*preg));
      }
   }
   x87->env[FP_ENV_TAG] = toUShort(tagw);

   
}


void amd64g_dirtyhelper_FNSAVE ( VexGuestAMD64State* vex_state,
                                 HWord x87_state)
{
   do_get_x87( vex_state, (UChar*)x87_state );
}


void amd64g_dirtyhelper_FNSAVES ( VexGuestAMD64State* vex_state,
                                  HWord x87_state)
{
   Int           i, stno, preg;
   UInt          tagw;
   ULong*        vexRegs = (ULong*)(&vex_state->guest_FPREG[0]);
   UChar*        vexTags = (UChar*)(&vex_state->guest_FPTAG[0]);
   Fpu_State_16* x87     = (Fpu_State_16*)x87_state;
   UInt          ftop    = vex_state->guest_FTOP;
   UInt          c3210   = vex_state->guest_FC3210;

   for (i = 0; i < 7; i++)
      x87->env[i] = 0;

   x87->env[FPS_ENV_STAT] 
      = toUShort(((ftop & 7) << 11) | (c3210 & 0x4700));
   x87->env[FPS_ENV_CTRL] 
      = toUShort(amd64g_create_fpucw( vex_state->guest_FPROUND ));

   
   tagw = 0;
   for (stno = 0; stno < 8; stno++) {
      preg = (stno + ftop) & 7;
      if (vexTags[preg] == 0) {
         
         tagw |= (3 << (2*preg));
         convert_f64le_to_f80le( (UChar*)&vexRegs[preg], 
                                 &x87->reg[10*stno] );
      } else {
         
         tagw |= (0 << (2*preg));
         convert_f64le_to_f80le( (UChar*)&vexRegs[preg], 
                                 &x87->reg[10*stno] );
      }
   }
   x87->env[FPS_ENV_TAG] = toUShort(tagw);
}


VexEmNote amd64g_dirtyhelper_FRSTOR ( VexGuestAMD64State* vex_state,
                                      HWord x87_state)
{
   return do_put_x87( True, (UChar*)x87_state, vex_state );
}


VexEmNote amd64g_dirtyhelper_FRSTORS ( VexGuestAMD64State* vex_state,
                                       HWord x87_state)
{
   Int           stno, preg;
   UInt          tag;
   ULong*        vexRegs = (ULong*)(&vex_state->guest_FPREG[0]);
   UChar*        vexTags = (UChar*)(&vex_state->guest_FPTAG[0]);
   Fpu_State_16* x87     = (Fpu_State_16*)x87_state;
   UInt          ftop    = (x87->env[FPS_ENV_STAT] >> 11) & 7;
   UInt          tagw    = x87->env[FPS_ENV_TAG];
   UInt          fpucw   = x87->env[FPS_ENV_CTRL];
   UInt          c3210   = x87->env[FPS_ENV_STAT] & 0x4700;
   VexEmNote     ew;
   UInt          fpround;
   ULong         pair;

   
   for (stno = 0; stno < 8; stno++) {
      preg = (stno + ftop) & 7;
      tag = (tagw >> (2*preg)) & 3;
      if (tag == 3) {
         
         /* hmm, if it's empty, does it still get written?  Probably
            safer to say it does.  If we don't, memcheck could get out
            of sync, in that it thinks all FP registers are defined by
            this helper, but in reality some have not been updated. */
         vexRegs[preg] = 0; 
         vexTags[preg] = 0;
      } else {
         
         convert_f80le_to_f64le( &x87->reg[10*stno], 
                                 (UChar*)&vexRegs[preg] );
         vexTags[preg] = 1;
      }
   }

   
   vex_state->guest_FTOP = ftop;

   
   vex_state->guest_FC3210 = c3210;

   pair    = amd64g_check_fldcw ( (ULong)fpucw );
   fpround = (UInt)pair & 0xFFFFFFFFULL;
   ew      = (VexEmNote)(pair >> 32);
   
   vex_state->guest_FPROUND = fpround & 3;

   
   return ew;
}



void amd64g_dirtyhelper_CPUID_baseline ( VexGuestAMD64State* st )
{
#  define SET_ABCD(_a,_b,_c,_d)                \
      do { st->guest_RAX = (ULong)(_a);        \
           st->guest_RBX = (ULong)(_b);        \
           st->guest_RCX = (ULong)(_c);        \
           st->guest_RDX = (ULong)(_d);        \
      } while (0)

   switch (0xFFFFFFFF & st->guest_RAX) {
      case 0x00000000:
         SET_ABCD(0x00000001, 0x68747541, 0x444d4163, 0x69746e65);
         break;
      case 0x00000001:
         SET_ABCD(0x00000f5a, 0x01000800, 0x00000000, 0x078bfbff);
         break;
      case 0x80000000:
         SET_ABCD(0x80000018, 0x68747541, 0x444d4163, 0x69746e65);
         break;
      case 0x80000001:
         SET_ABCD(0x00000f5a, 0x00000505, 0x00000000, 
                                                      0x21d3fbff);
         break;
      case 0x80000002:
         SET_ABCD(0x20444d41, 0x6574704f, 0x206e6f72, 0x296d7428);
         break;
      case 0x80000003:
         SET_ABCD(0x6f725020, 0x73736563, 0x3820726f, 0x00003834);
         break;
      case 0x80000004:
         SET_ABCD(0x00000000, 0x00000000, 0x00000000, 0x00000000);
         break;
      case 0x80000005:
         SET_ABCD(0xff08ff08, 0xff20ff20, 0x40020140, 0x40020140);
         break;
      case 0x80000006:
         SET_ABCD(0x00000000, 0x42004200, 0x04008140, 0x00000000);
         break;
      case 0x80000007:
         SET_ABCD(0x00000000, 0x00000000, 0x00000000, 0x0000000f);
         break;
      case 0x80000008:
         SET_ABCD(0x00003028, 0x00000000, 0x00000000, 0x00000000);
         break;
      default:
         SET_ABCD(0x00000000, 0x00000000, 0x00000000, 0x00000000);
         break;
   }
#  undef SET_ABCD
}


void amd64g_dirtyhelper_CPUID_sse3_and_cx16 ( VexGuestAMD64State* st )
{
#  define SET_ABCD(_a,_b,_c,_d)                \
      do { st->guest_RAX = (ULong)(_a);        \
           st->guest_RBX = (ULong)(_b);        \
           st->guest_RCX = (ULong)(_c);        \
           st->guest_RDX = (ULong)(_d);        \
      } while (0)

   switch (0xFFFFFFFF & st->guest_RAX) {
      case 0x00000000:
         SET_ABCD(0x0000000a, 0x756e6547, 0x6c65746e, 0x49656e69);
         break;
      case 0x00000001:
         SET_ABCD(0x000006f6, 0x00020800, 0x0000e3bd, 0xbfebfbff);
         break;
      case 0x00000002:
         SET_ABCD(0x05b0b101, 0x005657f0, 0x00000000, 0x2cb43049);
         break;
      case 0x00000003:
         SET_ABCD(0x00000000, 0x00000000, 0x00000000, 0x00000000);
         break;
      case 0x00000004: {
         switch (0xFFFFFFFF & st->guest_RCX) {
            case 0x00000000: SET_ABCD(0x04000121, 0x01c0003f,
                                      0x0000003f, 0x00000001); break;
            case 0x00000001: SET_ABCD(0x04000122, 0x01c0003f,
                                      0x0000003f, 0x00000001); break;
            case 0x00000002: SET_ABCD(0x04004143, 0x03c0003f,
                                      0x00000fff, 0x00000001); break;
            default:         SET_ABCD(0x00000000, 0x00000000,
                                      0x00000000, 0x00000000); break;
         }
         break;
      }
      case 0x00000005:
         SET_ABCD(0x00000040, 0x00000040, 0x00000003, 0x00000020);
         break;
      case 0x00000006:
         SET_ABCD(0x00000001, 0x00000002, 0x00000001, 0x00000000);
         break;
      case 0x00000007:
         SET_ABCD(0x00000000, 0x00000000, 0x00000000, 0x00000000);
         break;
      case 0x00000008:
         SET_ABCD(0x00000400, 0x00000000, 0x00000000, 0x00000000);
         break;
      case 0x00000009:
         SET_ABCD(0x00000000, 0x00000000, 0x00000000, 0x00000000);
         break;
      case 0x0000000a:
      unhandled_eax_value:
         SET_ABCD(0x07280202, 0x00000000, 0x00000000, 0x00000000);
         break;
      case 0x80000000:
         SET_ABCD(0x80000008, 0x00000000, 0x00000000, 0x00000000);
         break;
      case 0x80000001:
         SET_ABCD(0x00000000, 0x00000000, 0x00000001, 0x20100800);
         break;
      case 0x80000002:
         SET_ABCD(0x65746e49, 0x2952286c, 0x726f4320, 0x4d542865);
         break;
      case 0x80000003:
         SET_ABCD(0x43203229, 0x20205550, 0x20202020, 0x20202020);
         break;
      case 0x80000004:
         SET_ABCD(0x30303636, 0x20402020, 0x30342e32, 0x007a4847);
         break;
      case 0x80000005:
         SET_ABCD(0x00000000, 0x00000000, 0x00000000, 0x00000000);
         break;
      case 0x80000006:
         SET_ABCD(0x00000000, 0x00000000, 0x10008040, 0x00000000);
         break;
      case 0x80000007:
         SET_ABCD(0x00000000, 0x00000000, 0x00000000, 0x00000000);
         break;
      case 0x80000008:
         SET_ABCD(0x00003024, 0x00000000, 0x00000000, 0x00000000);
         break;
      default:         
         goto unhandled_eax_value;
   }
#  undef SET_ABCD
}


void amd64g_dirtyhelper_CPUID_sse42_and_cx16 ( VexGuestAMD64State* st )
{
#  define SET_ABCD(_a,_b,_c,_d)                \
      do { st->guest_RAX = (ULong)(_a);        \
           st->guest_RBX = (ULong)(_b);        \
           st->guest_RCX = (ULong)(_c);        \
           st->guest_RDX = (ULong)(_d);        \
      } while (0)

   UInt old_eax = (UInt)st->guest_RAX;
   UInt old_ecx = (UInt)st->guest_RCX;

   switch (old_eax) {
      case 0x00000000:
         SET_ABCD(0x0000000b, 0x756e6547, 0x6c65746e, 0x49656e69);
         break;
      case 0x00000001:
         SET_ABCD(0x00020652, 0x00100800, 0x0298e3ff, 0xbfebfbff);
         break;
      case 0x00000002:
         SET_ABCD(0x55035a01, 0x00f0b2e3, 0x00000000, 0x09ca212c);
         break;
      case 0x00000003:
         SET_ABCD(0x00000000, 0x00000000, 0x00000000, 0x00000000);
         break;
      case 0x00000004:
         switch (old_ecx) {
            case 0x00000000: SET_ABCD(0x1c004121, 0x01c0003f,
                                      0x0000003f, 0x00000000); break;
            case 0x00000001: SET_ABCD(0x1c004122, 0x00c0003f,
                                      0x0000007f, 0x00000000); break;
            case 0x00000002: SET_ABCD(0x1c004143, 0x01c0003f,
                                      0x000001ff, 0x00000000); break;
            case 0x00000003: SET_ABCD(0x1c03c163, 0x03c0003f,
                                      0x00000fff, 0x00000002); break;
            default:         SET_ABCD(0x00000000, 0x00000000,
                                      0x00000000, 0x00000000); break;
         }
         break;
      case 0x00000005:
         SET_ABCD(0x00000040, 0x00000040, 0x00000003, 0x00001120);
         break;
      case 0x00000006:
         SET_ABCD(0x00000007, 0x00000002, 0x00000001, 0x00000000);
         break;
      case 0x00000007:
         SET_ABCD(0x00000000, 0x00000000, 0x00000000, 0x00000000);
         break;
      case 0x00000008:
         SET_ABCD(0x00000000, 0x00000000, 0x00000000, 0x00000000);
         break;
      case 0x00000009:
         SET_ABCD(0x00000000, 0x00000000, 0x00000000, 0x00000000);
         break;
      case 0x0000000a:
         SET_ABCD(0x07300403, 0x00000004, 0x00000000, 0x00000603);
         break;
      case 0x0000000b:
         switch (old_ecx) {
            case 0x00000000:
               SET_ABCD(0x00000001, 0x00000002,
                        0x00000100, 0x00000000); break;
            case 0x00000001:
               SET_ABCD(0x00000004, 0x00000004,
                        0x00000201, 0x00000000); break;
            default:
               SET_ABCD(0x00000000, 0x00000000,
                        old_ecx,    0x00000000); break;
         }
         break;
      case 0x0000000c:
         SET_ABCD(0x00000001, 0x00000002, 0x00000100, 0x00000000);
         break;
      case 0x0000000d:
         switch (old_ecx) {
            case 0x00000000: SET_ABCD(0x00000001, 0x00000002,
                                      0x00000100, 0x00000000); break;
            case 0x00000001: SET_ABCD(0x00000004, 0x00000004,
                                      0x00000201, 0x00000000); break;
            default:         SET_ABCD(0x00000000, 0x00000000,
                                      old_ecx,    0x00000000); break;
         }
         break;
      case 0x80000000:
         SET_ABCD(0x80000008, 0x00000000, 0x00000000, 0x00000000);
         break;
      case 0x80000001:
         SET_ABCD(0x00000000, 0x00000000, 0x00000001, 0x28100800);
         break;
      case 0x80000002:
         SET_ABCD(0x65746e49, 0x2952286c, 0x726f4320, 0x4d542865);
         break;
      case 0x80000003:
         SET_ABCD(0x35692029, 0x55504320, 0x20202020, 0x20202020);
         break;
      case 0x80000004:
         SET_ABCD(0x30373620, 0x20402020, 0x37342e33, 0x007a4847);
         break;
      case 0x80000005:
         SET_ABCD(0x00000000, 0x00000000, 0x00000000, 0x00000000);
         break;
      case 0x80000006:
         SET_ABCD(0x00000000, 0x00000000, 0x01006040, 0x00000000);
         break;
      case 0x80000007:
         SET_ABCD(0x00000000, 0x00000000, 0x00000000, 0x00000100);
         break;
      case 0x80000008:
         SET_ABCD(0x00003024, 0x00000000, 0x00000000, 0x00000000);
         break;
      default:
         SET_ABCD(0x00000001, 0x00000002, 0x00000100, 0x00000000);
         break;
   }
#  undef SET_ABCD
}


void amd64g_dirtyhelper_CPUID_avx_and_cx16 ( VexGuestAMD64State* st )
{
#  define SET_ABCD(_a,_b,_c,_d)                \
      do { st->guest_RAX = (ULong)(_a);        \
           st->guest_RBX = (ULong)(_b);        \
           st->guest_RCX = (ULong)(_c);        \
           st->guest_RDX = (ULong)(_d);        \
      } while (0)

   UInt old_eax = (UInt)st->guest_RAX;
   UInt old_ecx = (UInt)st->guest_RCX;

   switch (old_eax) {
      case 0x00000000:
         SET_ABCD(0x0000000d, 0x756e6547, 0x6c65746e, 0x49656e69);
         break;
      case 0x00000001:
         SET_ABCD(0x000206a7, 0x00100800, 0x1f9ae3bf, 0xbfebfbff);
         break;
      case 0x00000002:
         SET_ABCD(0x76035a01, 0x00f0b0ff, 0x00000000, 0x00ca0000);
         break;
      case 0x00000003:
         SET_ABCD(0x00000000, 0x00000000, 0x00000000, 0x00000000);
         break;
      case 0x00000004:
         switch (old_ecx) {
            case 0x00000000: SET_ABCD(0x1c004121, 0x01c0003f,
                                      0x0000003f, 0x00000000); break;
            case 0x00000001: SET_ABCD(0x1c004122, 0x01c0003f,
                                      0x0000003f, 0x00000000); break;
            case 0x00000002: SET_ABCD(0x1c004143, 0x01c0003f,
                                      0x000001ff, 0x00000000); break;
            case 0x00000003: SET_ABCD(0x1c03c163, 0x02c0003f,
                                      0x00001fff, 0x00000006); break;
            default:         SET_ABCD(0x00000000, 0x00000000,
                                      0x00000000, 0x00000000); break;
         }
         break;
      case 0x00000005:
         SET_ABCD(0x00000040, 0x00000040, 0x00000003, 0x00001120);
         break;
      case 0x00000006:
         SET_ABCD(0x00000077, 0x00000002, 0x00000009, 0x00000000);
         break;
      case 0x00000007:
         SET_ABCD(0x00000000, 0x00000800, 0x00000000, 0x00000000);
         break;
      case 0x00000008:
         SET_ABCD(0x00000000, 0x00000000, 0x00000000, 0x00000000);
         break;
      case 0x00000009:
         SET_ABCD(0x00000000, 0x00000000, 0x00000000, 0x00000000);
         break;
      case 0x0000000a:
         SET_ABCD(0x07300803, 0x00000000, 0x00000000, 0x00000603);
         break;
      case 0x0000000b:
         switch (old_ecx) {
            case 0x00000000:
               SET_ABCD(0x00000001, 0x00000001,
                        0x00000100, 0x00000000); break;
            case 0x00000001:
               SET_ABCD(0x00000004, 0x00000004,
                        0x00000201, 0x00000000); break;
            default:
               SET_ABCD(0x00000000, 0x00000000,
                        old_ecx,    0x00000000); break;
         }
         break;
      case 0x0000000c:
         SET_ABCD(0x00000000, 0x00000000, 0x00000000, 0x00000000);
         break;
      case 0x0000000d:
         switch (old_ecx) {
            case 0x00000000: SET_ABCD(0x00000007, 0x00000340,
                                      0x00000340, 0x00000000); break;
            case 0x00000001: SET_ABCD(0x00000001, 0x00000000,
                                      0x00000000, 0x00000000); break;
            case 0x00000002: SET_ABCD(0x00000100, 0x00000240,
                                      0x00000000, 0x00000000); break;
            default:         SET_ABCD(0x00000000, 0x00000000,
                                      0x00000000, 0x00000000); break;
         }
         break;
      case 0x0000000e:
         SET_ABCD(0x00000007, 0x00000340, 0x00000340, 0x00000000);
         break;
      case 0x0000000f:
         SET_ABCD(0x00000007, 0x00000340, 0x00000340, 0x00000000);
         break;
      case 0x80000000:
         SET_ABCD(0x80000008, 0x00000000, 0x00000000, 0x00000000);
         break;
      case 0x80000001:
         SET_ABCD(0x00000000, 0x00000000, 0x00000001, 0x28100800);
         break;
      case 0x80000002:
         SET_ABCD(0x20202020, 0x20202020, 0x65746e49, 0x2952286c);
         break;
      case 0x80000003:
         SET_ABCD(0x726f4320, 0x4d542865, 0x35692029, 0x3033322d);
         break;
      case 0x80000004:
         SET_ABCD(0x50432030, 0x20402055, 0x30382e32, 0x007a4847);
         break;
      case 0x80000005:
         SET_ABCD(0x00000000, 0x00000000, 0x00000000, 0x00000000);
         break;
      case 0x80000006:
         SET_ABCD(0x00000000, 0x00000000, 0x01006040, 0x00000000);
         break;
      case 0x80000007:
         SET_ABCD(0x00000000, 0x00000000, 0x00000000, 0x00000100);
         break;
      case 0x80000008:
         SET_ABCD(0x00003024, 0x00000000, 0x00000000, 0x00000000);
         break;
      default:
         SET_ABCD(0x00000007, 0x00000340, 0x00000340, 0x00000000);
         break;
   }
#  undef SET_ABCD
}


ULong amd64g_calculate_RCR ( ULong arg, 
                             ULong rot_amt, 
                             ULong rflags_in, 
                             Long  szIN )
{
   Bool  wantRflags = toBool(szIN < 0);
   ULong sz         = wantRflags ? (-szIN) : szIN;
   ULong tempCOUNT  = rot_amt & (sz == 8 ? 0x3F : 0x1F);
   ULong cf=0, of=0, tempcf;

   switch (sz) {
      case 8:
         cf        = (rflags_in >> AMD64G_CC_SHIFT_C) & 1;
         of        = ((arg >> 63) ^ cf) & 1;
         while (tempCOUNT > 0) {
            tempcf = arg & 1;
            arg    = (arg >> 1) | (cf << 63);
            cf     = tempcf;
            tempCOUNT--;
         }
         break;
      case 4:
         while (tempCOUNT >= 33) tempCOUNT -= 33;
         cf        = (rflags_in >> AMD64G_CC_SHIFT_C) & 1;
         of        = ((arg >> 31) ^ cf) & 1;
         while (tempCOUNT > 0) {
            tempcf = arg & 1;
            arg    = ((arg >> 1) & 0x7FFFFFFFULL) | (cf << 31);
            cf     = tempcf;
            tempCOUNT--;
         }
         break;
      case 2:
         while (tempCOUNT >= 17) tempCOUNT -= 17;
         cf        = (rflags_in >> AMD64G_CC_SHIFT_C) & 1;
         of        = ((arg >> 15) ^ cf) & 1;
         while (tempCOUNT > 0) {
            tempcf = arg & 1;
            arg    = ((arg >> 1) & 0x7FFFULL) | (cf << 15);
            cf     = tempcf;
            tempCOUNT--;
         }
         break;
      case 1:
         while (tempCOUNT >= 9) tempCOUNT -= 9;
         cf        = (rflags_in >> AMD64G_CC_SHIFT_C) & 1;
         of        = ((arg >> 7) ^ cf) & 1;
         while (tempCOUNT > 0) {
            tempcf = arg & 1;
            arg    = ((arg >> 1) & 0x7FULL) | (cf << 7);
            cf     = tempcf;
            tempCOUNT--;
         }
         break;
      default:
         vpanic("calculate_RCR(amd64g): invalid size");
   }

   cf &= 1;
   of &= 1;
   rflags_in &= ~(AMD64G_CC_MASK_C | AMD64G_CC_MASK_O);
   rflags_in |= (cf << AMD64G_CC_SHIFT_C) | (of << AMD64G_CC_SHIFT_O);

   return wantRflags ? rflags_in : arg;
}

ULong amd64g_calculate_RCL ( ULong arg, 
                             ULong rot_amt, 
                             ULong rflags_in, 
                             Long  szIN )
{
   Bool  wantRflags = toBool(szIN < 0);
   ULong sz         = wantRflags ? (-szIN) : szIN;
   ULong tempCOUNT  = rot_amt & (sz == 8 ? 0x3F : 0x1F);
   ULong cf=0, of=0, tempcf;

   switch (sz) {
      case 8:
         cf = (rflags_in >> AMD64G_CC_SHIFT_C) & 1;
         while (tempCOUNT > 0) {
            tempcf = (arg >> 63) & 1;
            arg    = (arg << 1) | (cf & 1);
            cf     = tempcf;
            tempCOUNT--;
         }
         of = ((arg >> 63) ^ cf) & 1;
         break;
      case 4:
         while (tempCOUNT >= 33) tempCOUNT -= 33;
         cf = (rflags_in >> AMD64G_CC_SHIFT_C) & 1;
         while (tempCOUNT > 0) {
            tempcf = (arg >> 31) & 1;
            arg    = 0xFFFFFFFFULL & ((arg << 1) | (cf & 1));
            cf     = tempcf;
            tempCOUNT--;
         }
         of = ((arg >> 31) ^ cf) & 1;
         break;
      case 2:
         while (tempCOUNT >= 17) tempCOUNT -= 17;
         cf = (rflags_in >> AMD64G_CC_SHIFT_C) & 1;
         while (tempCOUNT > 0) {
            tempcf = (arg >> 15) & 1;
            arg    = 0xFFFFULL & ((arg << 1) | (cf & 1));
            cf     = tempcf;
            tempCOUNT--;
         }
         of = ((arg >> 15) ^ cf) & 1;
         break;
      case 1:
         while (tempCOUNT >= 9) tempCOUNT -= 9;
         cf = (rflags_in >> AMD64G_CC_SHIFT_C) & 1;
         while (tempCOUNT > 0) {
            tempcf = (arg >> 7) & 1;
            arg    = 0xFFULL & ((arg << 1) | (cf & 1));
            cf     = tempcf;
            tempCOUNT--;
         }
         of = ((arg >> 7) ^ cf) & 1;
         break;
      default: 
         vpanic("calculate_RCL(amd64g): invalid size");
   }

   cf &= 1;
   of &= 1;
   rflags_in &= ~(AMD64G_CC_MASK_C | AMD64G_CC_MASK_O);
   rflags_in |= (cf << AMD64G_CC_SHIFT_C) | (of << AMD64G_CC_SHIFT_O);

   return wantRflags ? rflags_in : arg;
}

/* Taken from gf2x-0.9.5, released under GPLv2+ (later versions LGPLv2+)
 * svn://scm.gforge.inria.fr/svn/gf2x/trunk/hardware/opteron/gf2x_mul1.h@25
 */
ULong amd64g_calculate_pclmul(ULong a, ULong b, ULong which)
{
    ULong hi, lo, tmp, A[16];

   A[0] = 0;            A[1] = a;
   A[2] = A[1] << 1;    A[3] = A[2] ^ a;
   A[4] = A[2] << 1;    A[5] = A[4] ^ a;
   A[6] = A[3] << 1;    A[7] = A[6] ^ a;
   A[8] = A[4] << 1;    A[9] = A[8] ^ a;
   A[10] = A[5] << 1;   A[11] = A[10] ^ a;
   A[12] = A[6] << 1;   A[13] = A[12] ^ a;
   A[14] = A[7] << 1;   A[15] = A[14] ^ a;

   lo = (A[b >> 60] << 4) ^ A[(b >> 56) & 15];
   hi = lo >> 56;
   lo = (lo << 8) ^ (A[(b >> 52) & 15] << 4) ^ A[(b >> 48) & 15];
   hi = (hi << 8) | (lo >> 56);
   lo = (lo << 8) ^ (A[(b >> 44) & 15] << 4) ^ A[(b >> 40) & 15];
   hi = (hi << 8) | (lo >> 56);
   lo = (lo << 8) ^ (A[(b >> 36) & 15] << 4) ^ A[(b >> 32) & 15];
   hi = (hi << 8) | (lo >> 56);
   lo = (lo << 8) ^ (A[(b >> 28) & 15] << 4) ^ A[(b >> 24) & 15];
   hi = (hi << 8) | (lo >> 56);
   lo = (lo << 8) ^ (A[(b >> 20) & 15] << 4) ^ A[(b >> 16) & 15];
   hi = (hi << 8) | (lo >> 56);
   lo = (lo << 8) ^ (A[(b >> 12) & 15] << 4) ^ A[(b >> 8) & 15];
   hi = (hi << 8) | (lo >> 56);
   lo = (lo << 8) ^ (A[(b >> 4) & 15] << 4) ^ A[b & 15];

   ULong m0 = -1;
   m0 /= 255;
   tmp = -((a >> 63) & 1); tmp &= ((b & (m0 * 0xfe)) >> 1); hi = hi ^ tmp;
   tmp = -((a >> 62) & 1); tmp &= ((b & (m0 * 0xfc)) >> 2); hi = hi ^ tmp;
   tmp = -((a >> 61) & 1); tmp &= ((b & (m0 * 0xf8)) >> 3); hi = hi ^ tmp;
   tmp = -((a >> 60) & 1); tmp &= ((b & (m0 * 0xf0)) >> 4); hi = hi ^ tmp;
   tmp = -((a >> 59) & 1); tmp &= ((b & (m0 * 0xe0)) >> 5); hi = hi ^ tmp;
   tmp = -((a >> 58) & 1); tmp &= ((b & (m0 * 0xc0)) >> 6); hi = hi ^ tmp;
   tmp = -((a >> 57) & 1); tmp &= ((b & (m0 * 0x80)) >> 7); hi = hi ^ tmp;

   return which ? hi : lo;
}


ULong amd64g_dirtyhelper_RDTSC ( void )
{
#  if defined(__x86_64__)
   UInt  eax, edx;
   __asm__ __volatile__("rdtsc" : "=a" (eax), "=d" (edx));
   return (((ULong)edx) << 32) | ((ULong)eax);
#  else
   return 1ULL;
#  endif
}

void amd64g_dirtyhelper_RDTSCP ( VexGuestAMD64State* st )
{
#  if defined(__x86_64__)
   UInt eax, ecx, edx;
   __asm__ __volatile__("rdtscp" : "=a" (eax), "=d" (edx), "=c" (ecx));
   st->guest_RAX = (ULong)eax;
   st->guest_RCX = (ULong)ecx;
   st->guest_RDX = (ULong)edx;
#  else
   
#  endif
}

ULong amd64g_dirtyhelper_IN ( ULong portno, ULong sz )
{
#  if defined(__x86_64__)
   ULong r = 0;
   portno &= 0xFFFF;
   switch (sz) {
      case 4: 
         __asm__ __volatile__("movq $0,%%rax; inl %w1,%%eax; movq %%rax,%0" 
                              : "=a" (r) : "Nd" (portno));
	 break;
      case 2: 
         __asm__ __volatile__("movq $0,%%rax; inw %w1,%w0" 
                              : "=a" (r) : "Nd" (portno));
	 break;
      case 1: 
         __asm__ __volatile__("movq $0,%%rax; inb %w1,%b0" 
                              : "=a" (r) : "Nd" (portno));
	 break;
      default:
         break; 
   }
   return r;
#  else
   return 0;
#  endif
}


void amd64g_dirtyhelper_OUT ( ULong portno, ULong data, ULong sz )
{
#  if defined(__x86_64__)
   portno &= 0xFFFF;
   switch (sz) {
      case 4: 
         __asm__ __volatile__("movq %0,%%rax; outl %%eax, %w1" 
                              : : "a" (data), "Nd" (portno));
	 break;
      case 2: 
         __asm__ __volatile__("outw %w0, %w1" 
                              : : "a" (data), "Nd" (portno));
	 break;
      case 1: 
         __asm__ __volatile__("outb %b0, %w1" 
                              : : "a" (data), "Nd" (portno));
	 break;
      default:
         break; 
   }
#  else
   
#  endif
}

void amd64g_dirtyhelper_SxDT ( void *address, ULong op ) {
#  if defined(__x86_64__)
   switch (op) {
      case 0:
         __asm__ __volatile__("sgdt (%0)" : : "r" (address) : "memory");
         break;
      case 1:
         __asm__ __volatile__("sidt (%0)" : : "r" (address) : "memory");
         break;
      default:
         vpanic("amd64g_dirtyhelper_SxDT");
   }
#  else
   
   UChar* p = (UChar*)address;
   p[0] = p[1] = p[2] = p[3] = p[4] = p[5] = 0;
   p[6] = p[7] = p[8] = p[9] = 0;
#  endif
}


static inline UChar abdU8 ( UChar xx, UChar yy ) {
   return toUChar(xx>yy ? xx-yy : yy-xx);
}

static inline ULong mk32x2 ( UInt w1, UInt w0 ) {
   return (((ULong)w1) << 32) | ((ULong)w0);
}

static inline UShort sel16x4_3 ( ULong w64 ) {
   UInt hi32 = toUInt(w64 >> 32);
   return toUShort(hi32 >> 16);
}
static inline UShort sel16x4_2 ( ULong w64 ) {
   UInt hi32 = toUInt(w64 >> 32);
   return toUShort(hi32);
}
static inline UShort sel16x4_1 ( ULong w64 ) {
   UInt lo32 = toUInt(w64);
   return toUShort(lo32 >> 16);
}
static inline UShort sel16x4_0 ( ULong w64 ) {
   UInt lo32 = toUInt(w64);
   return toUShort(lo32);
}

static inline UChar sel8x8_7 ( ULong w64 ) {
   UInt hi32 = toUInt(w64 >> 32);
   return toUChar(hi32 >> 24);
}
static inline UChar sel8x8_6 ( ULong w64 ) {
   UInt hi32 = toUInt(w64 >> 32);
   return toUChar(hi32 >> 16);
}
static inline UChar sel8x8_5 ( ULong w64 ) {
   UInt hi32 = toUInt(w64 >> 32);
   return toUChar(hi32 >> 8);
}
static inline UChar sel8x8_4 ( ULong w64 ) {
   UInt hi32 = toUInt(w64 >> 32);
   return toUChar(hi32 >> 0);
}
static inline UChar sel8x8_3 ( ULong w64 ) {
   UInt lo32 = toUInt(w64);
   return toUChar(lo32 >> 24);
}
static inline UChar sel8x8_2 ( ULong w64 ) {
   UInt lo32 = toUInt(w64);
   return toUChar(lo32 >> 16);
}
static inline UChar sel8x8_1 ( ULong w64 ) {
   UInt lo32 = toUInt(w64);
   return toUChar(lo32 >> 8);
}
static inline UChar sel8x8_0 ( ULong w64 ) {
   UInt lo32 = toUInt(w64);
   return toUChar(lo32 >> 0);
}

ULong amd64g_calculate_mmx_pmaddwd ( ULong xx, ULong yy )
{
   return
      mk32x2(
         (((Int)(Short)sel16x4_3(xx)) * ((Int)(Short)sel16x4_3(yy)))
            + (((Int)(Short)sel16x4_2(xx)) * ((Int)(Short)sel16x4_2(yy))),
         (((Int)(Short)sel16x4_1(xx)) * ((Int)(Short)sel16x4_1(yy)))
            + (((Int)(Short)sel16x4_0(xx)) * ((Int)(Short)sel16x4_0(yy)))
      );
}

ULong amd64g_calculate_mmx_psadbw ( ULong xx, ULong yy )
{
   UInt t = 0;
   t += (UInt)abdU8( sel8x8_7(xx), sel8x8_7(yy) );
   t += (UInt)abdU8( sel8x8_6(xx), sel8x8_6(yy) );
   t += (UInt)abdU8( sel8x8_5(xx), sel8x8_5(yy) );
   t += (UInt)abdU8( sel8x8_4(xx), sel8x8_4(yy) );
   t += (UInt)abdU8( sel8x8_3(xx), sel8x8_3(yy) );
   t += (UInt)abdU8( sel8x8_2(xx), sel8x8_2(yy) );
   t += (UInt)abdU8( sel8x8_1(xx), sel8x8_1(yy) );
   t += (UInt)abdU8( sel8x8_0(xx), sel8x8_0(yy) );
   t &= 0xFFFF;
   return (ULong)t;
}

ULong amd64g_calculate_sse_phminposuw ( ULong sLo, ULong sHi )
{
   UShort t, min;
   UInt   idx;
   t = sel16x4_0(sLo); if (True)    { min = t; idx = 0; }
   t = sel16x4_1(sLo); if (t < min) { min = t; idx = 1; }
   t = sel16x4_2(sLo); if (t < min) { min = t; idx = 2; }
   t = sel16x4_3(sLo); if (t < min) { min = t; idx = 3; }
   t = sel16x4_0(sHi); if (t < min) { min = t; idx = 4; }
   t = sel16x4_1(sHi); if (t < min) { min = t; idx = 5; }
   t = sel16x4_2(sHi); if (t < min) { min = t; idx = 6; }
   t = sel16x4_3(sHi); if (t < min) { min = t; idx = 7; }
   return ((ULong)(idx << 16)) | ((ULong)min);
}

ULong amd64g_calc_crc32b ( ULong crcIn, ULong b )
{
   UInt  i;
   ULong crc = (b & 0xFFULL) ^ crcIn;
   for (i = 0; i < 8; i++)
      crc = (crc >> 1) ^ ((crc & 1) ? 0x82f63b78ULL : 0);
   return crc;
}

ULong amd64g_calc_crc32w ( ULong crcIn, ULong w )
{
   UInt  i;
   ULong crc = (w & 0xFFFFULL) ^ crcIn;
   for (i = 0; i < 16; i++)
      crc = (crc >> 1) ^ ((crc & 1) ? 0x82f63b78ULL : 0);
   return crc;
}

ULong amd64g_calc_crc32l ( ULong crcIn, ULong l )
{
   UInt i;
   ULong crc = (l & 0xFFFFFFFFULL) ^ crcIn;
   for (i = 0; i < 32; i++)
      crc = (crc >> 1) ^ ((crc & 1) ? 0x82f63b78ULL : 0);
   return crc;
}

ULong amd64g_calc_crc32q ( ULong crcIn, ULong q )
{
   ULong crc = amd64g_calc_crc32l(crcIn, q);
   return amd64g_calc_crc32l(crc, q >> 32);
}


static inline ULong sad_8x4 ( ULong xx, ULong yy )
{
   UInt t = 0;
   t += (UInt)abdU8( sel8x8_3(xx), sel8x8_3(yy) );
   t += (UInt)abdU8( sel8x8_2(xx), sel8x8_2(yy) );
   t += (UInt)abdU8( sel8x8_1(xx), sel8x8_1(yy) );
   t += (UInt)abdU8( sel8x8_0(xx), sel8x8_0(yy) );
   return (ULong)t;
}

ULong amd64g_calc_mpsadbw ( ULong sHi, ULong sLo,
                            ULong dHi, ULong dLo,
                            ULong imm_and_return_control_bit )
{
   UInt imm8     = imm_and_return_control_bit & 7;
   Bool calcHi   = (imm_and_return_control_bit >> 7) & 1;
   UInt srcOffsL = imm8 & 3; 
   UInt dstOffsL = (imm8 >> 2) & 1; 
   ULong src = ((srcOffsL & 2) ? sHi : sLo) >> (32 * (srcOffsL & 1));
   ULong dst;
   if (calcHi && dstOffsL) {
      
      dst = dHi & 0x00FFFFFFFFFFFFFFULL;
   }
   else if (!calcHi && !dstOffsL) {
      
      dst = dLo & 0x00FFFFFFFFFFFFFFULL;
   } 
   else {
      
      dst = (dLo >> 32) | ((dHi & 0x00FFFFFFULL) << 32);
   }
   ULong r0  = sad_8x4( dst >>  0, src );
   ULong r1  = sad_8x4( dst >>  8, src );
   ULong r2  = sad_8x4( dst >> 16, src );
   ULong r3  = sad_8x4( dst >> 24, src );
   ULong res = (r3 << 48) | (r2 << 32) | (r1 << 16) | r0;
   return res;
}

ULong amd64g_calculate_pext ( ULong src_masked, ULong mask )
{
   ULong dst = 0;
   ULong src_bit;
   ULong dst_bit = 1;
   for (src_bit = 1; src_bit; src_bit <<= 1) {
      if (mask & src_bit) {
         if (src_masked & src_bit) dst |= dst_bit;
         dst_bit <<= 1;
      }
   }
   return dst;
}

ULong amd64g_calculate_pdep ( ULong src, ULong mask )
{
   ULong dst = 0;
   ULong dst_bit;
   ULong src_bit = 1;
   for (dst_bit = 1; dst_bit; dst_bit <<= 1) {
      if (mask & dst_bit) {
         if (src & src_bit) dst |= dst_bit;
         src_bit <<= 1;
      }
   }
   return dst;
}


static UInt zmask_from_V128 ( V128* arg )
{
   UInt i, res = 0;
   for (i = 0; i < 16; i++) {
      res |=  ((arg->w8[i] == 0) ? 1 : 0) << i;
   }
   return res;
}

static UInt zmask_from_V128_wide ( V128* arg )
{
   UInt i, res = 0;
   for (i = 0; i < 8; i++) {
      res |=  ((arg->w16[i] == 0) ? 1 : 0) << i;
   }
   return res;
}

ULong amd64g_dirtyhelper_PCMPxSTRx ( 
          VexGuestAMD64State* gst,
          HWord opc4_and_imm,
          HWord gstOffL, HWord gstOffR,
          HWord edxIN, HWord eaxIN
       )
{
   HWord opc4 = (opc4_and_imm >> 8) & 0xFF;
   HWord imm8 = opc4_and_imm & 0xFF;
   HWord isISTRx = opc4 & 2;
   HWord isxSTRM = (opc4 & 1) ^ 1;
   vassert((opc4 & 0xFC) == 0x60); 
   HWord wide = (imm8 & 1);

   
   V128* argL = (V128*)( ((UChar*)gst) + gstOffL );
   V128* argR = (V128*)( ((UChar*)gst) + gstOffR );

   
   
   UInt zmaskL, zmaskR;

   
   V128 resV;
   UInt resOSZACP;

   
   Bool ok = False;

   if (wide) {
      if (isISTRx) {
         zmaskL = zmask_from_V128_wide(argL);
         zmaskR = zmask_from_V128_wide(argR);
      } else {
         Int tmp;
         tmp = edxIN & 0xFFFFFFFF;
         if (tmp < -8) tmp = -8;
         if (tmp > 8)  tmp = 8;
         if (tmp < 0)  tmp = -tmp;
         vassert(tmp >= 0 && tmp <= 8);
         zmaskL = (1 << tmp) & 0xFF;
         tmp = eaxIN & 0xFFFFFFFF;
         if (tmp < -8) tmp = -8;
         if (tmp > 8)  tmp = 8;
         if (tmp < 0)  tmp = -tmp;
         vassert(tmp >= 0 && tmp <= 8);
         zmaskR = (1 << tmp) & 0xFF;
      }
      
      ok = compute_PCMPxSTRx_wide ( 
              &resV, &resOSZACP, argL, argR, 
              zmaskL, zmaskR, imm8, (Bool)isxSTRM
           );
   } else {
      if (isISTRx) {
         zmaskL = zmask_from_V128(argL);
         zmaskR = zmask_from_V128(argR);
      } else {
         Int tmp;
         tmp = edxIN & 0xFFFFFFFF;
         if (tmp < -16) tmp = -16;
         if (tmp > 16)  tmp = 16;
         if (tmp < 0)   tmp = -tmp;
         vassert(tmp >= 0 && tmp <= 16);
         zmaskL = (1 << tmp) & 0xFFFF;
         tmp = eaxIN & 0xFFFFFFFF;
         if (tmp < -16) tmp = -16;
         if (tmp > 16)  tmp = 16;
         if (tmp < 0)   tmp = -tmp;
         vassert(tmp >= 0 && tmp <= 16);
         zmaskR = (1 << tmp) & 0xFFFF;
      }
      
      ok = compute_PCMPxSTRx ( 
              &resV, &resOSZACP, argL, argR, 
              zmaskL, zmaskR, imm8, (Bool)isxSTRM
           );
   }

   
   
   vassert(ok);

   
   
   
   if (isxSTRM) {
      gst->guest_YMM0[0] = resV.w32[0];
      gst->guest_YMM0[1] = resV.w32[1];
      gst->guest_YMM0[2] = resV.w32[2];
      gst->guest_YMM0[3] = resV.w32[3];
      return resOSZACP & 0x8D5;
   } else {
      UInt newECX = resV.w32[0] & 0xFFFF;
      return (newECX << 16) | (resOSZACP & 0x8D5);
   }
}

static const UChar sbox[256] = {                   
   0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 
   0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
   0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 
   0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
   0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 
   0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
   0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 
   0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
   0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 
   0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
   0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 
   0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
   0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 
   0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
   0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 
   0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
   0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 
   0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
   0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 
   0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
   0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 
   0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
   0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 
   0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
   0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 
   0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
   0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 
   0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
   0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 
   0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
   0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 
   0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};
static void SubBytes (V128* v)
{
   V128 r;
   UInt i;
   for (i = 0; i < 16; i++)
      r.w8[i] = sbox[v->w8[i]];
   *v = r;
}

static const UChar invsbox[256] = {                
   0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 
   0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,     
   0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 
   0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,     
   0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 
   0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,     
   0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 
   0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,     
   0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 
   0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,     
   0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 
   0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,     
   0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 
   0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,     
   0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 
   0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,     
   0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 
   0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,     
   0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 
   0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,     
   0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 
   0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,     
   0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 
   0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,     
   0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 
   0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,     
   0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 
   0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,     
   0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 
   0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,     
   0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 
   0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
};
static void InvSubBytes (V128* v)
{
   V128 r;
   UInt i;
   for (i = 0; i < 16; i++)
      r.w8[i] = invsbox[v->w8[i]];
   *v = r;
}

static const UChar ShiftRows_op[16] =
   {11, 6, 1, 12, 7, 2, 13, 8, 3, 14, 9, 4, 15, 10, 5, 0};
static void ShiftRows (V128* v)
{
   V128 r;
   UInt i;
   for (i = 0; i < 16; i++)
      r.w8[i] = v->w8[ShiftRows_op[15-i]];
   *v = r;
}

static const UChar InvShiftRows_op[16] = 
   {3, 6, 9, 12, 15, 2, 5, 8, 11, 14, 1, 4, 7, 10, 13, 0};
static void InvShiftRows (V128* v)
{
   V128 r;
   UInt i;
   for (i = 0; i < 16; i++)
      r.w8[i] = v->w8[InvShiftRows_op[15-i]];
   *v = r;
}

static const UChar Nxy[256] = {                    
   0xff, 0x00, 0x19, 0x01, 0x32, 0x02, 0x1a, 0xc6, 
   0x4b, 0xc7, 0x1b, 0x68, 0x33, 0xee, 0xdf, 0x03,     
   0x64, 0x04, 0xe0, 0x0e, 0x34, 0x8d, 0x81, 0xef, 
   0x4c, 0x71, 0x08, 0xc8, 0xf8, 0x69, 0x1c, 0xc1,     
   0x7d, 0xc2, 0x1d, 0xb5, 0xf9, 0xb9, 0x27, 0x6a, 
   0x4d, 0xe4, 0xa6, 0x72, 0x9a, 0xc9, 0x09, 0x78,     
   0x65, 0x2f, 0x8a, 0x05, 0x21, 0x0f, 0xe1, 0x24, 
   0x12, 0xf0, 0x82, 0x45, 0x35, 0x93, 0xda, 0x8e,     
   0x96, 0x8f, 0xdb, 0xbd, 0x36, 0xd0, 0xce, 0x94, 
   0x13, 0x5c, 0xd2, 0xf1, 0x40, 0x46, 0x83, 0x38,     
   0x66, 0xdd, 0xfd, 0x30, 0xbf, 0x06, 0x8b, 0x62, 
   0xb3, 0x25, 0xe2, 0x98, 0x22, 0x88, 0x91, 0x10,     
   0x7e, 0x6e, 0x48, 0xc3, 0xa3, 0xb6, 0x1e, 0x42, 
   0x3a, 0x6b, 0x28, 0x54, 0xfa, 0x85, 0x3d, 0xba,     
   0x2b, 0x79, 0x0a, 0x15, 0x9b, 0x9f, 0x5e, 0xca, 
   0x4e, 0xd4, 0xac, 0xe5, 0xf3, 0x73, 0xa7, 0x57,     
   0xaf, 0x58, 0xa8, 0x50, 0xf4, 0xea, 0xd6, 0x74, 
   0x4f, 0xae, 0xe9, 0xd5, 0xe7, 0xe6, 0xad, 0xe8,     
   0x2c, 0xd7, 0x75, 0x7a, 0xeb, 0x16, 0x0b, 0xf5, 
   0x59, 0xcb, 0x5f, 0xb0, 0x9c, 0xa9, 0x51, 0xa0,     
   0x7f, 0x0c, 0xf6, 0x6f, 0x17, 0xc4, 0x49, 0xec, 
   0xd8, 0x43, 0x1f, 0x2d, 0xa4, 0x76, 0x7b, 0xb7,     
   0xcc, 0xbb, 0x3e, 0x5a, 0xfb, 0x60, 0xb1, 0x86, 
   0x3b, 0x52, 0xa1, 0x6c, 0xaa, 0x55, 0x29, 0x9d,     
   0x97, 0xb2, 0x87, 0x90, 0x61, 0xbe, 0xdc, 0xfc, 
   0xbc, 0x95, 0xcf, 0xcd, 0x37, 0x3f, 0x5b, 0xd1,     
   0x53, 0x39, 0x84, 0x3c, 0x41, 0xa2, 0x6d, 0x47, 
   0x14, 0x2a, 0x9e, 0x5d, 0x56, 0xf2, 0xd3, 0xab,     
   0x44, 0x11, 0x92, 0xd9, 0x23, 0x20, 0x2e, 0x89, 
   0xb4, 0x7c, 0xb8, 0x26, 0x77, 0x99, 0xe3, 0xa5,     
   0x67, 0x4a, 0xed, 0xde, 0xc5, 0x31, 0xfe, 0x18, 
   0x0d, 0x63, 0x8c, 0x80, 0xc0, 0xf7, 0x70, 0x07
};

static const UChar Exy[256] = {                    
   0x01, 0x03, 0x05, 0x0f, 0x11, 0x33, 0x55, 0xff, 
   0x1a, 0x2e, 0x72, 0x96, 0xa1, 0xf8, 0x13, 0x35,     
   0x5f, 0xe1, 0x38, 0x48, 0xd8, 0x73, 0x95, 0xa4, 
   0xf7, 0x02, 0x06, 0x0a, 0x1e, 0x22, 0x66, 0xaa,     
   0xe5, 0x34, 0x5c, 0xe4, 0x37, 0x59, 0xeb, 0x26, 
   0x6a, 0xbe, 0xd9, 0x70, 0x90, 0xab, 0xe6, 0x31,     
   0x53, 0xf5, 0x04, 0x0c, 0x14, 0x3c, 0x44, 0xcc, 
   0x4f, 0xd1, 0x68, 0xb8, 0xd3, 0x6e, 0xb2, 0xcd,     
   0x4c, 0xd4, 0x67, 0xa9, 0xe0, 0x3b, 0x4d, 0xd7, 
   0x62, 0xa6, 0xf1, 0x08, 0x18, 0x28, 0x78, 0x88,     
   0x83, 0x9e, 0xb9, 0xd0, 0x6b, 0xbd, 0xdc, 0x7f, 
   0x81, 0x98, 0xb3, 0xce, 0x49, 0xdb, 0x76, 0x9a,     
   0xb5, 0xc4, 0x57, 0xf9, 0x10, 0x30, 0x50, 0xf0, 
   0x0b, 0x1d, 0x27, 0x69, 0xbb, 0xd6, 0x61, 0xa3,     
   0xfe, 0x19, 0x2b, 0x7d, 0x87, 0x92, 0xad, 0xec, 
   0x2f, 0x71, 0x93, 0xae, 0xe9, 0x20, 0x60, 0xa0,     
   0xfb, 0x16, 0x3a, 0x4e, 0xd2, 0x6d, 0xb7, 0xc2, 
   0x5d, 0xe7, 0x32, 0x56, 0xfa, 0x15, 0x3f, 0x41,     
   0xc3, 0x5e, 0xe2, 0x3d, 0x47, 0xc9, 0x40, 0xc0, 
   0x5b, 0xed, 0x2c, 0x74, 0x9c, 0xbf, 0xda, 0x75,     
   0x9f, 0xba, 0xd5, 0x64, 0xac, 0xef, 0x2a, 0x7e, 
   0x82, 0x9d, 0xbc, 0xdf, 0x7a, 0x8e, 0x89, 0x80,     
   0x9b, 0xb6, 0xc1, 0x58, 0xe8, 0x23, 0x65, 0xaf, 
   0xea, 0x25, 0x6f, 0xb1, 0xc8, 0x43, 0xc5, 0x54,     
   0xfc, 0x1f, 0x21, 0x63, 0xa5, 0xf4, 0x07, 0x09, 
   0x1b, 0x2d, 0x77, 0x99, 0xb0, 0xcb, 0x46, 0xca,     
   0x45, 0xcf, 0x4a, 0xde, 0x79, 0x8b, 0x86, 0x91, 
   0xa8, 0xe3, 0x3e, 0x42, 0xc6, 0x51, 0xf3, 0x0e,     
   0x12, 0x36, 0x5a, 0xee, 0x29, 0x7b, 0x8d, 0x8c, 
   0x8f, 0x8a, 0x85, 0x94, 0xa7, 0xf2, 0x0d, 0x17,     
   0x39, 0x4b, 0xdd, 0x7c, 0x84, 0x97, 0xa2, 0xfd, 
   0x1c, 0x24, 0x6c, 0xb4, 0xc7, 0x52, 0xf6, 0x01};

static inline UChar ff_mul(UChar u1, UChar u2)
{
   if ((u1 > 0) && (u2 > 0)) {
      UInt ui = Nxy[u1] + Nxy[u2];
      if (ui >= 255)
         ui = ui - 255;
      return Exy[ui];
   } else {
      return 0;
   };
}

static void MixColumns (V128* v)
{
   V128 r;
   Int j;
#define P(x,row,col) (x)->w8[((row)*4+(col))]
   for (j = 0; j < 4; j++) {
      P(&r,j,0) = ff_mul(0x02, P(v,j,0)) ^ ff_mul(0x03, P(v,j,1)) 
         ^ P(v,j,2) ^ P(v,j,3);
      P(&r,j,1) = P(v,j,0) ^ ff_mul( 0x02, P(v,j,1) ) 
         ^ ff_mul(0x03, P(v,j,2) ) ^ P(v,j,3);
      P(&r,j,2) = P(v,j,0) ^ P(v,j,1) ^ ff_mul( 0x02, P(v,j,2) )
         ^ ff_mul(0x03, P(v,j,3) );
      P(&r,j,3) = ff_mul(0x03, P(v,j,0) ) ^ P(v,j,1) ^ P(v,j,2)
         ^ ff_mul( 0x02, P(v,j,3) );
   }
   *v = r;
#undef P
}

static void InvMixColumns (V128* v)
{
   V128 r;
   Int j;
#define P(x,row,col) (x)->w8[((row)*4+(col))]
   for (j = 0; j < 4; j++) {
      P(&r,j,0) = ff_mul(0x0e, P(v,j,0) ) ^ ff_mul(0x0b, P(v,j,1) )
         ^ ff_mul(0x0d,P(v,j,2) ) ^ ff_mul(0x09, P(v,j,3) );
      P(&r,j,1) = ff_mul(0x09, P(v,j,0) ) ^ ff_mul(0x0e, P(v,j,1) )
         ^ ff_mul(0x0b,P(v,j,2) ) ^ ff_mul(0x0d, P(v,j,3) );
      P(&r,j,2) = ff_mul(0x0d, P(v,j,0) ) ^ ff_mul(0x09, P(v,j,1) )
         ^ ff_mul(0x0e,P(v,j,2) ) ^ ff_mul(0x0b, P(v,j,3) );
      P(&r,j,3) = ff_mul(0x0b, P(v,j,0) ) ^ ff_mul(0x0d, P(v,j,1) )
         ^ ff_mul(0x09,P(v,j,2) ) ^ ff_mul(0x0e, P(v,j,3) );
   }
   *v = r;
#undef P

}

void amd64g_dirtyhelper_AES ( 
          VexGuestAMD64State* gst,
          HWord opc4, HWord gstOffD,
          HWord gstOffL, HWord gstOffR
       )
{
   
   V128* argD = (V128*)( ((UChar*)gst) + gstOffD );
   V128* argL = (V128*)( ((UChar*)gst) + gstOffL );
   V128* argR = (V128*)( ((UChar*)gst) + gstOffR );
   V128  r;

   switch (opc4) {
      case 0xDC: 
      case 0xDD: 
         r = *argR;
         ShiftRows (&r);
         SubBytes  (&r);
         if (opc4 == 0xDC)
            MixColumns (&r);
         argD->w64[0] = r.w64[0] ^ argL->w64[0];
         argD->w64[1] = r.w64[1] ^ argL->w64[1];
         break;

      case 0xDE: 
      case 0xDF: 
         r = *argR;
         InvShiftRows (&r);
         InvSubBytes (&r);
         if (opc4 == 0xDE)
            InvMixColumns (&r);
         argD->w64[0] = r.w64[0] ^ argL->w64[0];
         argD->w64[1] = r.w64[1] ^ argL->w64[1];
         break;

      case 0xDB: 
         *argD = *argL;
         InvMixColumns (argD);
         break;
      default: vassert(0);
   }
}

static inline UInt RotWord (UInt   w32)
{
   return ((w32 >> 8) | (w32 << 24));
}

static inline UInt SubWord (UInt   w32)
{
   UChar *w8;
   UChar *r8;
   UInt res;
   w8 = (UChar*) &w32;
   r8 = (UChar*) &res;
   r8[0] = sbox[w8[0]];
   r8[1] = sbox[w8[1]];
   r8[2] = sbox[w8[2]];
   r8[3] = sbox[w8[3]];
   return res;
}

extern void amd64g_dirtyhelper_AESKEYGENASSIST ( 
          VexGuestAMD64State* gst,
          HWord imm8,
          HWord gstOffL, HWord gstOffR
       )
{
   
   V128* argL = (V128*)( ((UChar*)gst) + gstOffL );
   V128* argR = (V128*)( ((UChar*)gst) + gstOffR );

   
   
   V128 tmp;

   tmp.w32[3] = RotWord (SubWord (argL->w32[3])) ^ imm8;
   tmp.w32[2] = SubWord (argL->w32[3]);
   tmp.w32[1] = RotWord (SubWord (argL->w32[1])) ^ imm8;
   tmp.w32[0] = SubWord (argL->w32[1]);

   argR->w32[3] = tmp.w32[3];
   argR->w32[2] = tmp.w32[2];
   argR->w32[1] = tmp.w32[1];
   argR->w32[0] = tmp.w32[0];
}




void LibVEX_GuestAMD64_initialise ( VexGuestAMD64State* vex_state )
{
   vex_state->host_EvC_FAILADDR = 0;
   vex_state->host_EvC_COUNTER = 0;
   vex_state->pad0 = 0;

   vex_state->guest_RAX = 0;
   vex_state->guest_RCX = 0;
   vex_state->guest_RDX = 0;
   vex_state->guest_RBX = 0;
   vex_state->guest_RSP = 0;
   vex_state->guest_RBP = 0;
   vex_state->guest_RSI = 0;
   vex_state->guest_RDI = 0;
   vex_state->guest_R8  = 0;
   vex_state->guest_R9  = 0;
   vex_state->guest_R10 = 0;
   vex_state->guest_R11 = 0;
   vex_state->guest_R12 = 0;
   vex_state->guest_R13 = 0;
   vex_state->guest_R14 = 0;
   vex_state->guest_R15 = 0;

   vex_state->guest_CC_OP   = AMD64G_CC_OP_COPY;
   vex_state->guest_CC_DEP1 = 0;
   vex_state->guest_CC_DEP2 = 0;
   vex_state->guest_CC_NDEP = 0;

   vex_state->guest_DFLAG   = 1; 
   vex_state->guest_IDFLAG  = 0;
   vex_state->guest_ACFLAG  = 0;

   vex_state->guest_FS_CONST = 0;

   vex_state->guest_RIP = 0;

   
   amd64g_dirtyhelper_FINIT( vex_state );

   
#  define AVXZERO(_ymm) \
      do { _ymm[0]=_ymm[1]=_ymm[2]=_ymm[3] = 0; \
           _ymm[4]=_ymm[5]=_ymm[6]=_ymm[7] = 0; \
      } while (0)
   vex_state->guest_SSEROUND = (ULong)Irrm_NEAREST;
   AVXZERO(vex_state->guest_YMM0);
   AVXZERO(vex_state->guest_YMM1);
   AVXZERO(vex_state->guest_YMM2);
   AVXZERO(vex_state->guest_YMM3);
   AVXZERO(vex_state->guest_YMM4);
   AVXZERO(vex_state->guest_YMM5);
   AVXZERO(vex_state->guest_YMM6);
   AVXZERO(vex_state->guest_YMM7);
   AVXZERO(vex_state->guest_YMM8);
   AVXZERO(vex_state->guest_YMM9);
   AVXZERO(vex_state->guest_YMM10);
   AVXZERO(vex_state->guest_YMM11);
   AVXZERO(vex_state->guest_YMM12);
   AVXZERO(vex_state->guest_YMM13);
   AVXZERO(vex_state->guest_YMM14);
   AVXZERO(vex_state->guest_YMM15);
   AVXZERO(vex_state->guest_YMM16);

#  undef AVXZERO

   vex_state->guest_EMNOTE = EmNote_NONE;

   /* These should not ever be either read or written, but we
      initialise them anyway. */
   vex_state->guest_CMSTART = 0;
   vex_state->guest_CMLEN   = 0;

   vex_state->guest_NRADDR   = 0;
   vex_state->guest_SC_CLASS = 0;
   vex_state->guest_GS_CONST = 0;

   vex_state->guest_IP_AT_SYSCALL = 0;
   vex_state->pad1 = 0;
}


Bool guest_amd64_state_requires_precise_mem_exns (
        Int minoff, Int maxoff, VexRegisterUpdates pxControl
     )
{
   Int rbp_min = offsetof(VexGuestAMD64State, guest_RBP);
   Int rbp_max = rbp_min + 8 - 1;
   Int rsp_min = offsetof(VexGuestAMD64State, guest_RSP);
   Int rsp_max = rsp_min + 8 - 1;
   Int rip_min = offsetof(VexGuestAMD64State, guest_RIP);
   Int rip_max = rip_min + 8 - 1;

   if (maxoff < rsp_min || minoff > rsp_max) {
      
      if (pxControl == VexRegUpdSpAtMemAccess)
         return False; 
   } else {
      return True;
   }

   if (maxoff < rbp_min || minoff > rbp_max) {
      
   } else {
      return True;
   }

   if (maxoff < rip_min || minoff > rip_max) {
      
   } else {
      return True;
   }

   return False;
}


#define ALWAYSDEFD(field)                             \
    { offsetof(VexGuestAMD64State, field),            \
      (sizeof ((VexGuestAMD64State*)0)->field) }

VexGuestLayout
   amd64guest_layout
      = {
          
          .total_sizeB = sizeof(VexGuestAMD64State),

          
          .offset_SP = offsetof(VexGuestAMD64State,guest_RSP),
          .sizeof_SP = 8,

          
          .offset_FP = offsetof(VexGuestAMD64State,guest_RBP),
          .sizeof_FP = 8,

          
          .offset_IP = offsetof(VexGuestAMD64State,guest_RIP),
          .sizeof_IP = 8,

          .n_alwaysDefd = 16,

          .alwaysDefd
             = {  ALWAYSDEFD(guest_CC_OP),
                  ALWAYSDEFD(guest_CC_NDEP),
		  ALWAYSDEFD(guest_DFLAG),
                  ALWAYSDEFD(guest_IDFLAG),
                  ALWAYSDEFD(guest_RIP),
                  ALWAYSDEFD(guest_FS_CONST),
                  ALWAYSDEFD(guest_FTOP),
                  ALWAYSDEFD(guest_FPTAG),
                  ALWAYSDEFD(guest_FPROUND),
                  ALWAYSDEFD(guest_FC3210),
                 
                 
                 
                 
                 
                 
                 
                 
                  ALWAYSDEFD(guest_EMNOTE),
                  ALWAYSDEFD(guest_SSEROUND),
                  ALWAYSDEFD(guest_CMSTART),
                  ALWAYSDEFD(guest_CMLEN),
                  ALWAYSDEFD(guest_SC_CLASS),
                  ALWAYSDEFD(guest_IP_AT_SYSCALL)
               }
        };


