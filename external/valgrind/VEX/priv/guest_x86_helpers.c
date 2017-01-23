

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
#include "libvex_guest_x86.h"
#include "libvex_ir.h"
#include "libvex.h"

#include "main_util.h"
#include "main_globals.h"
#include "guest_generic_bb_to_IR.h"
#include "guest_x86_defs.h"
#include "guest_generic_x87.h"




#define PROFILE_EFLAGS 0



static const UChar parity_table[256] = {
    X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0,
    0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P,
    0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P,
    X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0,
    0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P,
    X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0,
    X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0,
    0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P,
    0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P,
    X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0,
    X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0,
    0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P,
    X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0,
    0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P,
    0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P,
    X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0,
    0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P,
    X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0,
    X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0,
    0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P,
    X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0,
    0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P,
    0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P,
    X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0,
    X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0,
    0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P,
    0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P,
    X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0,
    0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P,
    X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0,
    X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0,
    0, X86G_CC_MASK_P, X86G_CC_MASK_P, 0, X86G_CC_MASK_P, 0, 0, X86G_CC_MASK_P,
};

inline static Int lshift ( Int x, Int n )
{
   if (n >= 0)
      return (UInt)x << n;
   else
      return x >> (-n);
}

static inline ULong idULong ( ULong x )
{
   return x;
}


#define PREAMBLE(__data_bits)					\
    UInt DATA_MASK 					\
      = __data_bits==8 ? 0xFF 					\
                       : (__data_bits==16 ? 0xFFFF 		\
                                          : 0xFFFFFFFF); 	\
    UInt SIGN_MASK = 1u << (__data_bits - 1);	\
    UInt CC_DEP1 = cc_dep1_formal;			\
    UInt CC_DEP2 = cc_dep2_formal;			\
    UInt CC_NDEP = cc_ndep_formal;			\
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
   { UInt cf, pf, af, zf, sf, of;				\
     UInt argL, argR, res;					\
     argL = CC_DEP1;						\
     argR = CC_DEP2;						\
     res  = argL + argR;					\
     cf = (DATA_UTYPE)res < (DATA_UTYPE)argL;			\
     pf = parity_table[(UChar)res];				\
     af = (res ^ argL ^ argR) & 0x10;				\
     zf = ((DATA_UTYPE)res == 0) << 6;				\
     sf = lshift(res, 8 - DATA_BITS) & 0x80;			\
     of = lshift((argL ^ argR ^ -1) & (argL ^ res), 		\
                 12 - DATA_BITS) & X86G_CC_MASK_O;		\
     return cf | pf | af | zf | sf | of;			\
   }								\
}


#define ACTIONS_SUB(DATA_BITS,DATA_UTYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   { UInt cf, pf, af, zf, sf, of;				\
     UInt argL, argR, res;					\
     argL = CC_DEP1;						\
     argR = CC_DEP2;						\
     res  = argL - argR;					\
     cf = (DATA_UTYPE)argL < (DATA_UTYPE)argR;			\
     pf = parity_table[(UChar)res];				\
     af = (res ^ argL ^ argR) & 0x10;				\
     zf = ((DATA_UTYPE)res == 0) << 6;				\
     sf = lshift(res, 8 - DATA_BITS) & 0x80;			\
     of = lshift((argL ^ argR) & (argL ^ res),	 		\
                 12 - DATA_BITS) & X86G_CC_MASK_O; 		\
     return cf | pf | af | zf | sf | of;			\
   }								\
}


#define ACTIONS_ADC(DATA_BITS,DATA_UTYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   { UInt cf, pf, af, zf, sf, of;				\
     UInt argL, argR, oldC, res;		       		\
     oldC = CC_NDEP & X86G_CC_MASK_C;				\
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
                  12 - DATA_BITS) & X86G_CC_MASK_O;		\
     return cf | pf | af | zf | sf | of;			\
   }								\
}


#define ACTIONS_SBB(DATA_BITS,DATA_UTYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   { UInt cf, pf, af, zf, sf, of;				\
     UInt argL, argR, oldC, res;		       		\
     oldC = CC_NDEP & X86G_CC_MASK_C;				\
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
                 12 - DATA_BITS) & X86G_CC_MASK_O;		\
     return cf | pf | af | zf | sf | of;			\
   }								\
}


#define ACTIONS_LOGIC(DATA_BITS,DATA_UTYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   { UInt cf, pf, af, zf, sf, of;				\
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
   { UInt cf, pf, af, zf, sf, of;				\
     UInt argL, argR, res;					\
     res  = CC_DEP1;						\
     argL = res - 1;						\
     argR = 1;							\
     cf = CC_NDEP & X86G_CC_MASK_C;				\
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
   { UInt cf, pf, af, zf, sf, of;				\
     UInt argL, argR, res;					\
     res  = CC_DEP1;						\
     argL = res + 1;						\
     argR = 1;							\
     cf = CC_NDEP & X86G_CC_MASK_C;				\
     pf = parity_table[(UChar)res];				\
     af = (res ^ argL ^ argR) & 0x10;				\
     zf = ((DATA_UTYPE)res == 0) << 6;				\
     sf = lshift(res, 8 - DATA_BITS) & 0x80;			\
     of = ((res & DATA_MASK) 					\
          == ((UInt)SIGN_MASK - 1)) << 11;			\
     return cf | pf | af | zf | sf | of;			\
   }								\
}


#define ACTIONS_SHL(DATA_BITS,DATA_UTYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   { UInt cf, pf, af, zf, sf, of;				\
     cf = (CC_DEP2 >> (DATA_BITS - 1)) & X86G_CC_MASK_C;	\
     pf = parity_table[(UChar)CC_DEP1];				\
     af = 0; 					\
     zf = ((DATA_UTYPE)CC_DEP1 == 0) << 6;			\
     sf = lshift(CC_DEP1, 8 - DATA_BITS) & 0x80;		\
     			\
     of = lshift(CC_DEP2 ^ CC_DEP1, 12 - DATA_BITS) 		\
          & X86G_CC_MASK_O;					\
     return cf | pf | af | zf | sf | of;			\
   }								\
}


#define ACTIONS_SHR(DATA_BITS,DATA_UTYPE)			\
{								\
   PREAMBLE(DATA_BITS);  					\
   { UInt cf, pf, af, zf, sf, of;				\
     cf = CC_DEP2 & 1;						\
     pf = parity_table[(UChar)CC_DEP1];				\
     af = 0; 					\
     zf = ((DATA_UTYPE)CC_DEP1 == 0) << 6;			\
     sf = lshift(CC_DEP1, 8 - DATA_BITS) & 0x80;		\
     			\
     of = lshift(CC_DEP2 ^ CC_DEP1, 12 - DATA_BITS)		\
          & X86G_CC_MASK_O;					\
     return cf | pf | af | zf | sf | of;			\
   }								\
}


#define ACTIONS_ROL(DATA_BITS,DATA_UTYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   { UInt fl 							\
        = (CC_NDEP & ~(X86G_CC_MASK_O | X86G_CC_MASK_C))	\
          | (X86G_CC_MASK_C & CC_DEP1)				\
          | (X86G_CC_MASK_O & (lshift(CC_DEP1,  		\
                                      11-(DATA_BITS-1)) 	\
                     ^ lshift(CC_DEP1, 11)));			\
     return fl;							\
   }								\
}


#define ACTIONS_ROR(DATA_BITS,DATA_UTYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   { UInt fl 							\
        = (CC_NDEP & ~(X86G_CC_MASK_O | X86G_CC_MASK_C))	\
          | (X86G_CC_MASK_C & (CC_DEP1 >> (DATA_BITS-1)))	\
          | (X86G_CC_MASK_O & (lshift(CC_DEP1, 			\
                                      11-(DATA_BITS-1)) 	\
                     ^ lshift(CC_DEP1, 11-(DATA_BITS-1)+1)));	\
     return fl;							\
   }								\
}


#define ACTIONS_UMUL(DATA_BITS, DATA_UTYPE,  NARROWtoU,         \
                                DATA_U2TYPE, NARROWto2U)        \
{                                                               \
   PREAMBLE(DATA_BITS);                                         \
   { UInt cf, pf, af, zf, sf, of;                               \
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
   { UInt cf, pf, af, zf, sf, of;                               \
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


#if PROFILE_EFLAGS

static Bool initted     = False;

static UInt tabc_fast[X86G_CC_OP_NUMBER];
static UInt tabc_slow[X86G_CC_OP_NUMBER];
static UInt tab_cond[X86G_CC_OP_NUMBER][16];
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
   for (op = 0; op < X86G_CC_OP_NUMBER; op++) {

      ch = ' ';
      if (op > 0 && (op-1) % 3 == 0) 
         ch = 'B';
      if (op > 0 && (op-1) % 3 == 1) 
         ch = 'W';
      if (op > 0 && (op-1) % 3 == 2) 
         ch = 'L';

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
   for (op = 0; op < X86G_CC_OP_NUMBER; op++) {
      tabc_fast[op] = tabc_slow[op] = 0;
      for (co = 0; co < 16; co++)
         tab_cond[op][co] = 0;
   }
}

#endif 


static
UInt x86g_calculate_eflags_all_WRK ( UInt cc_op, 
                                     UInt cc_dep1_formal, 
                                     UInt cc_dep2_formal,
                                     UInt cc_ndep_formal )
{
   switch (cc_op) {
      case X86G_CC_OP_COPY:
         return cc_dep1_formal
                & (X86G_CC_MASK_O | X86G_CC_MASK_S | X86G_CC_MASK_Z 
                   | X86G_CC_MASK_A | X86G_CC_MASK_C | X86G_CC_MASK_P);

      case X86G_CC_OP_ADDB:   ACTIONS_ADD( 8,  UChar  );
      case X86G_CC_OP_ADDW:   ACTIONS_ADD( 16, UShort );
      case X86G_CC_OP_ADDL:   ACTIONS_ADD( 32, UInt   );

      case X86G_CC_OP_ADCB:   ACTIONS_ADC( 8,  UChar  );
      case X86G_CC_OP_ADCW:   ACTIONS_ADC( 16, UShort );
      case X86G_CC_OP_ADCL:   ACTIONS_ADC( 32, UInt   );

      case X86G_CC_OP_SUBB:   ACTIONS_SUB(  8, UChar  );
      case X86G_CC_OP_SUBW:   ACTIONS_SUB( 16, UShort );
      case X86G_CC_OP_SUBL:   ACTIONS_SUB( 32, UInt   );

      case X86G_CC_OP_SBBB:   ACTIONS_SBB(  8, UChar  );
      case X86G_CC_OP_SBBW:   ACTIONS_SBB( 16, UShort );
      case X86G_CC_OP_SBBL:   ACTIONS_SBB( 32, UInt   );

      case X86G_CC_OP_LOGICB: ACTIONS_LOGIC(  8, UChar  );
      case X86G_CC_OP_LOGICW: ACTIONS_LOGIC( 16, UShort );
      case X86G_CC_OP_LOGICL: ACTIONS_LOGIC( 32, UInt   );

      case X86G_CC_OP_INCB:   ACTIONS_INC(  8, UChar  );
      case X86G_CC_OP_INCW:   ACTIONS_INC( 16, UShort );
      case X86G_CC_OP_INCL:   ACTIONS_INC( 32, UInt   );

      case X86G_CC_OP_DECB:   ACTIONS_DEC(  8, UChar  );
      case X86G_CC_OP_DECW:   ACTIONS_DEC( 16, UShort );
      case X86G_CC_OP_DECL:   ACTIONS_DEC( 32, UInt   );

      case X86G_CC_OP_SHLB:   ACTIONS_SHL(  8, UChar  );
      case X86G_CC_OP_SHLW:   ACTIONS_SHL( 16, UShort );
      case X86G_CC_OP_SHLL:   ACTIONS_SHL( 32, UInt   );

      case X86G_CC_OP_SHRB:   ACTIONS_SHR(  8, UChar  );
      case X86G_CC_OP_SHRW:   ACTIONS_SHR( 16, UShort );
      case X86G_CC_OP_SHRL:   ACTIONS_SHR( 32, UInt   );

      case X86G_CC_OP_ROLB:   ACTIONS_ROL(  8, UChar  );
      case X86G_CC_OP_ROLW:   ACTIONS_ROL( 16, UShort );
      case X86G_CC_OP_ROLL:   ACTIONS_ROL( 32, UInt   );

      case X86G_CC_OP_RORB:   ACTIONS_ROR(  8, UChar  );
      case X86G_CC_OP_RORW:   ACTIONS_ROR( 16, UShort );
      case X86G_CC_OP_RORL:   ACTIONS_ROR( 32, UInt   );

      case X86G_CC_OP_UMULB:  ACTIONS_UMUL(  8, UChar,  toUChar,
                                                UShort, toUShort );
      case X86G_CC_OP_UMULW:  ACTIONS_UMUL( 16, UShort, toUShort,
                                                UInt,   toUInt );
      case X86G_CC_OP_UMULL:  ACTIONS_UMUL( 32, UInt,   toUInt,
                                                ULong,  idULong );

      case X86G_CC_OP_SMULB:  ACTIONS_SMUL(  8, Char,   toUChar,
                                                Short,  toUShort );
      case X86G_CC_OP_SMULW:  ACTIONS_SMUL( 16, Short,  toUShort, 
                                                Int,    toUInt   );
      case X86G_CC_OP_SMULL:  ACTIONS_SMUL( 32, Int,    toUInt,
                                                Long,   idULong );

      default:
         
         vex_printf("x86g_calculate_eflags_all_WRK(X86)"
                    "( %u, 0x%x, 0x%x, 0x%x )\n",
                    cc_op, cc_dep1_formal, cc_dep2_formal, cc_ndep_formal );
         vpanic("x86g_calculate_eflags_all_WRK(X86)");
   }
}


UInt x86g_calculate_eflags_all ( UInt cc_op, 
                                 UInt cc_dep1, 
                                 UInt cc_dep2,
                                 UInt cc_ndep )
{
#  if PROFILE_EFLAGS
   if (!initted) initCounts();
   n_calc_all++;
   if (SHOW_COUNTS_NOW) showCounts();
#  endif
   return
      x86g_calculate_eflags_all_WRK ( cc_op, cc_dep1, cc_dep2, cc_ndep );
}


VEX_REGPARM(3)
UInt x86g_calculate_eflags_c ( UInt cc_op, 
                               UInt cc_dep1, 
                               UInt cc_dep2,
                               UInt cc_ndep )
{
#  if PROFILE_EFLAGS
   if (!initted) initCounts();
   n_calc_c++;
   tabc_fast[cc_op]++;
   if (SHOW_COUNTS_NOW) showCounts();
#  endif

   
   switch (cc_op) {
      case X86G_CC_OP_LOGICL: 
      case X86G_CC_OP_LOGICW: 
      case X86G_CC_OP_LOGICB:
         return 0;
      case X86G_CC_OP_SUBL:
         return ((UInt)cc_dep1) < ((UInt)cc_dep2)
                   ? X86G_CC_MASK_C : 0;
      case X86G_CC_OP_SUBW:
         return ((UInt)(cc_dep1 & 0xFFFF)) < ((UInt)(cc_dep2 & 0xFFFF))
                   ? X86G_CC_MASK_C : 0;
      case X86G_CC_OP_SUBB:
         return ((UInt)(cc_dep1 & 0xFF)) < ((UInt)(cc_dep2 & 0xFF))
                   ? X86G_CC_MASK_C : 0;
      case X86G_CC_OP_INCL:
      case X86G_CC_OP_DECL:
         return cc_ndep & X86G_CC_MASK_C;
      default: 
         break;
   }

#  if PROFILE_EFLAGS
   tabc_fast[cc_op]--;
   tabc_slow[cc_op]++;
#  endif

   return x86g_calculate_eflags_all_WRK(cc_op,cc_dep1,cc_dep2,cc_ndep) 
          & X86G_CC_MASK_C;
}


UInt x86g_calculate_condition ( UInt cond, 
                                UInt cc_op, 
                                UInt cc_dep1, 
                                UInt cc_dep2,
                                UInt cc_ndep )
{
   UInt eflags = x86g_calculate_eflags_all_WRK(cc_op, cc_dep1, 
                                               cc_dep2, cc_ndep);
   UInt of,sf,zf,cf,pf;
   UInt inv = cond & 1;

#  if PROFILE_EFLAGS
   if (!initted) initCounts();
   tab_cond[cc_op][cond]++;
   n_calc_cond++;
   if (SHOW_COUNTS_NOW) showCounts();
#  endif

   switch (cond) {
      case X86CondNO:
      case X86CondO: 
         of = eflags >> X86G_CC_SHIFT_O;
         return 1 & (inv ^ of);

      case X86CondNZ:
      case X86CondZ: 
         zf = eflags >> X86G_CC_SHIFT_Z;
         return 1 & (inv ^ zf);

      case X86CondNB:
      case X86CondB: 
         cf = eflags >> X86G_CC_SHIFT_C;
         return 1 & (inv ^ cf);
         break;

      case X86CondNBE:
      case X86CondBE: 
         cf = eflags >> X86G_CC_SHIFT_C;
         zf = eflags >> X86G_CC_SHIFT_Z;
         return 1 & (inv ^ (cf | zf));
         break;

      case X86CondNS:
      case X86CondS: 
         sf = eflags >> X86G_CC_SHIFT_S;
         return 1 & (inv ^ sf);

      case X86CondNP:
      case X86CondP: 
         pf = eflags >> X86G_CC_SHIFT_P;
         return 1 & (inv ^ pf);

      case X86CondNL:
      case X86CondL: 
         sf = eflags >> X86G_CC_SHIFT_S;
         of = eflags >> X86G_CC_SHIFT_O;
         return 1 & (inv ^ (sf ^ of));
         break;

      case X86CondNLE:
      case X86CondLE: 
         sf = eflags >> X86G_CC_SHIFT_S;
         of = eflags >> X86G_CC_SHIFT_O;
         zf = eflags >> X86G_CC_SHIFT_Z;
         return 1 & (inv ^ ((sf ^ of) | zf));
         break;

      default:
         
         vex_printf("x86g_calculate_condition( %u, %u, 0x%x, 0x%x, 0x%x )\n",
                    cond, cc_op, cc_dep1, cc_dep2, cc_ndep );
         vpanic("x86g_calculate_condition");
   }
}


UInt LibVEX_GuestX86_get_eflags ( const VexGuestX86State* vex_state )
{
   UInt eflags = x86g_calculate_eflags_all_WRK(
                    vex_state->guest_CC_OP,
                    vex_state->guest_CC_DEP1,
                    vex_state->guest_CC_DEP2,
                    vex_state->guest_CC_NDEP
                 );
   UInt dflag = vex_state->guest_DFLAG;
   vassert(dflag == 1 || dflag == 0xFFFFFFFF);
   if (dflag == 0xFFFFFFFF)
      eflags |= (1<<10);
   if (vex_state->guest_IDFLAG == 1)
      eflags |= (1<<21);
   if (vex_state->guest_ACFLAG == 1)
      eflags |= (1<<18);
					     
   return eflags;
}

void
LibVEX_GuestX86_put_eflag_c ( UInt new_carry_flag,
                              VexGuestX86State* vex_state )
{
   UInt oszacp = x86g_calculate_eflags_all_WRK(
                    vex_state->guest_CC_OP,
                    vex_state->guest_CC_DEP1,
                    vex_state->guest_CC_DEP2,
                    vex_state->guest_CC_NDEP
                 );
   if (new_carry_flag & 1) {
      oszacp |= X86G_CC_MASK_C;
   } else {
      oszacp &= ~X86G_CC_MASK_C;
   }
   vex_state->guest_CC_OP   = X86G_CC_OP_COPY;
   vex_state->guest_CC_DEP1 = oszacp;
   vex_state->guest_CC_DEP2 = 0;
   vex_state->guest_CC_NDEP = 0;
}




static inline Bool isU32 ( IRExpr* e, UInt n )
{
   return 
      toBool( e->tag == Iex_Const
              && e->Iex.Const.con->tag == Ico_U32
              && e->Iex.Const.con->Ico.U32 == n );
}

IRExpr* guest_x86_spechelper ( const HChar* function_name,
                               IRExpr** args,
                               IRStmt** precedingStmts,
                               Int      n_precedingStmts )
{
#  define unop(_op,_a1) IRExpr_Unop((_op),(_a1))
#  define binop(_op,_a1,_a2) IRExpr_Binop((_op),(_a1),(_a2))
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

   

   if (vex_streq(function_name, "x86g_calculate_condition")) {
      
      IRExpr *cond, *cc_op, *cc_dep1, *cc_dep2;
      vassert(arity == 5);
      cond    = args[0];
      cc_op   = args[1];
      cc_dep1 = args[2];
      cc_dep2 = args[3];

      

      if (isU32(cc_op, X86G_CC_OP_ADDL) && isU32(cond, X86CondZ)) {
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpEQ32, 
                           binop(Iop_Add32, cc_dep1, cc_dep2),
                           mkU32(0)));
      }

      

      if (isU32(cc_op, X86G_CC_OP_SUBL) && isU32(cond, X86CondZ)) {
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpEQ32, cc_dep1, cc_dep2));
      }
      if (isU32(cc_op, X86G_CC_OP_SUBL) && isU32(cond, X86CondNZ)) {
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpNE32, cc_dep1, cc_dep2));
      }

      if (isU32(cc_op, X86G_CC_OP_SUBL) && isU32(cond, X86CondL)) {
         return unop(Iop_1Uto32,
                     binop(Iop_CmpLT32S, cc_dep1, cc_dep2));
      }
      if (isU32(cc_op, X86G_CC_OP_SUBL) && isU32(cond, X86CondNL)) {
         return binop(Iop_Xor32,
                      unop(Iop_1Uto32,
                           binop(Iop_CmpLT32S, cc_dep1, cc_dep2)),
                      mkU32(1));
      }

      if (isU32(cc_op, X86G_CC_OP_SUBL) && isU32(cond, X86CondLE)) {
         return unop(Iop_1Uto32,
                     binop(Iop_CmpLE32S, cc_dep1, cc_dep2));
      }
      if (isU32(cc_op, X86G_CC_OP_SUBL) && isU32(cond, X86CondNLE)) {
         return binop(Iop_Xor32,
                      unop(Iop_1Uto32,
                           binop(Iop_CmpLE32S, cc_dep1, cc_dep2)),
                      mkU32(1));
      }

      if (isU32(cc_op, X86G_CC_OP_SUBL) && isU32(cond, X86CondBE)) {
         return unop(Iop_1Uto32,
                     binop(Iop_CmpLE32U, cc_dep1, cc_dep2));
      }
      if (isU32(cc_op, X86G_CC_OP_SUBL) && isU32(cond, X86CondNBE)) {
         return binop(Iop_Xor32,
                      unop(Iop_1Uto32,
                           binop(Iop_CmpLE32U, cc_dep1, cc_dep2)),
                      mkU32(1));
      }

      if (isU32(cc_op, X86G_CC_OP_SUBL) && isU32(cond, X86CondB)) {
         return unop(Iop_1Uto32,
                     binop(Iop_CmpLT32U, cc_dep1, cc_dep2));
      }
      if (isU32(cc_op, X86G_CC_OP_SUBL) && isU32(cond, X86CondNB)) {
         return binop(Iop_Xor32,
                      unop(Iop_1Uto32,
                           binop(Iop_CmpLT32U, cc_dep1, cc_dep2)),
                      mkU32(1));
      }

      if (isU32(cc_op, X86G_CC_OP_SUBL) && isU32(cond, X86CondS)) {
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpLT32S, 
                           binop(Iop_Sub32, cc_dep1, cc_dep2),
                           mkU32(0)));
      }
      if (isU32(cc_op, X86G_CC_OP_SUBL) && isU32(cond, X86CondNS)) {
         
         return binop(Iop_Xor32,
                      unop(Iop_1Uto32,
                           binop(Iop_CmpLT32S, 
                                 binop(Iop_Sub32, cc_dep1, cc_dep2),
                                 mkU32(0))),
                      mkU32(1));
      }

      

      if (isU32(cc_op, X86G_CC_OP_SUBW) && isU32(cond, X86CondZ)) {
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpEQ16, 
                           unop(Iop_32to16,cc_dep1), 
                           unop(Iop_32to16,cc_dep2)));
      }
      if (isU32(cc_op, X86G_CC_OP_SUBW) && isU32(cond, X86CondNZ)) {
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpNE16, 
                           unop(Iop_32to16,cc_dep1), 
                           unop(Iop_32to16,cc_dep2)));
      }

      

      if (isU32(cc_op, X86G_CC_OP_SUBB) && isU32(cond, X86CondZ)) {
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpEQ8, 
                           unop(Iop_32to8,cc_dep1), 
                           unop(Iop_32to8,cc_dep2)));
      }
      if (isU32(cc_op, X86G_CC_OP_SUBB) && isU32(cond, X86CondNZ)) {
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpNE8, 
                           unop(Iop_32to8,cc_dep1), 
                           unop(Iop_32to8,cc_dep2)));
      }

      if (isU32(cc_op, X86G_CC_OP_SUBB) && isU32(cond, X86CondNBE)) {
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpLT32U, 
                           binop(Iop_And32,cc_dep2,mkU32(0xFF)),
			   binop(Iop_And32,cc_dep1,mkU32(0xFF))));
      }

      if (isU32(cc_op, X86G_CC_OP_SUBB) && isU32(cond, X86CondS)
                                        && isU32(cc_dep2, 0)) {
         return binop(Iop_And32,
                      binop(Iop_Shr32,cc_dep1,mkU8(7)),
                      mkU32(1));
      }
      if (isU32(cc_op, X86G_CC_OP_SUBB) && isU32(cond, X86CondNS)
                                        && isU32(cc_dep2, 0)) {
         return binop(Iop_Xor32,
                      binop(Iop_And32,
                            binop(Iop_Shr32,cc_dep1,mkU8(7)),
                            mkU32(1)),
                mkU32(1));
      }

      

      if (isU32(cc_op, X86G_CC_OP_LOGICL) && isU32(cond, X86CondZ)) {
         
         return unop(Iop_1Uto32,binop(Iop_CmpEQ32, cc_dep1, mkU32(0)));
      }
      if (isU32(cc_op, X86G_CC_OP_LOGICL) && isU32(cond, X86CondNZ)) {
         
         return unop(Iop_1Uto32,binop(Iop_CmpNE32, cc_dep1, mkU32(0)));
      }

      if (isU32(cc_op, X86G_CC_OP_LOGICL) && isU32(cond, X86CondLE)) {
         return unop(Iop_1Uto32,binop(Iop_CmpLE32S, cc_dep1, mkU32(0)));
      }

      if (isU32(cc_op, X86G_CC_OP_LOGICL) && isU32(cond, X86CondBE)) {
         return unop(Iop_1Uto32,binop(Iop_CmpEQ32, cc_dep1, mkU32(0)));
      }

      if (isU32(cc_op, X86G_CC_OP_LOGICL) && isU32(cond, X86CondS)) {
         
         
         return binop(Iop_And32,
                      binop(Iop_Shr32,cc_dep1,mkU8(31)),
                      mkU32(1));
      }
      if (isU32(cc_op, X86G_CC_OP_LOGICL) && isU32(cond, X86CondNS)) {
         
         
         return binop(Iop_Xor32,
                binop(Iop_And32,
                      binop(Iop_Shr32,cc_dep1,mkU8(31)),
                      mkU32(1)),
                mkU32(1));
      }

      

      if (isU32(cc_op, X86G_CC_OP_LOGICW) && isU32(cond, X86CondZ)) {
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpEQ32, binop(Iop_And32,cc_dep1,mkU32(0xFFFF)), 
                                        mkU32(0)));
      }

      if (isU32(cc_op, X86G_CC_OP_LOGICW) && isU32(cond, X86CondS)) {
         
         
         return binop(Iop_And32,
                      binop(Iop_Shr32,cc_dep1,mkU8(15)),
                      mkU32(1));
      }

      

      if (isU32(cc_op, X86G_CC_OP_LOGICB) && isU32(cond, X86CondZ)) {
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpEQ32, binop(Iop_And32,cc_dep1,mkU32(255)), 
                                        mkU32(0)));
      }
      if (isU32(cc_op, X86G_CC_OP_LOGICB) && isU32(cond, X86CondNZ)) {
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpNE32, binop(Iop_And32,cc_dep1,mkU32(255)), 
                                        mkU32(0)));
      }

      if (isU32(cc_op, X86G_CC_OP_LOGICB) && isU32(cond, X86CondS)) {
         
         return binop(Iop_And32,
                      binop(Iop_Shr32,cc_dep1,mkU8(7)),
                      mkU32(1));
      }
      if (isU32(cc_op, X86G_CC_OP_LOGICB) && isU32(cond, X86CondNS)) {
         
         
         return binop(Iop_Xor32,
                binop(Iop_And32,
                      binop(Iop_Shr32,cc_dep1,mkU8(7)),
                      mkU32(1)),
                mkU32(1));
      }

      

      if (isU32(cc_op, X86G_CC_OP_DECL) && isU32(cond, X86CondZ)) {
         
         return unop(Iop_1Uto32,binop(Iop_CmpEQ32, cc_dep1, mkU32(0)));
      }

      if (isU32(cc_op, X86G_CC_OP_DECL) && isU32(cond, X86CondS)) {
         
         return unop(Iop_1Uto32,binop(Iop_CmpLT32S, cc_dep1, mkU32(0)));
      }

      

      if (isU32(cc_op, X86G_CC_OP_DECW) && isU32(cond, X86CondZ)) {
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpEQ32, 
                           binop(Iop_Shl32,cc_dep1,mkU8(16)), 
                           mkU32(0)));
      }

      

      if (isU32(cc_op, X86G_CC_OP_INCW) && isU32(cond, X86CondZ)) {
         
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpEQ32, 
                           binop(Iop_Shl32,cc_dep1,mkU8(16)),
                           mkU32(0)));
      }

      

      if (isU32(cc_op, X86G_CC_OP_SHRL) && isU32(cond, X86CondZ)) {
         
         return unop(Iop_1Uto32,binop(Iop_CmpEQ32, cc_dep1, mkU32(0)));
      }

      

      if (isU32(cc_op, X86G_CC_OP_COPY) && 
          (isU32(cond, X86CondBE) || isU32(cond, X86CondNBE))) {
         UInt nnn = isU32(cond, X86CondBE) ? 1 : 0;
         return
            unop(
               Iop_1Uto32,
               binop(
                  Iop_CmpEQ32,
                  binop(
                     Iop_And32,
                     binop(
                        Iop_Or32,
                        binop(Iop_Shr32, cc_dep1, mkU8(X86G_CC_SHIFT_C)),
                        binop(Iop_Shr32, cc_dep1, mkU8(X86G_CC_SHIFT_Z))
                     ),
                     mkU32(1)
                  ),
                  mkU32(nnn)
               )
            );
      }
      
      if (isU32(cc_op, X86G_CC_OP_COPY) 
          && (isU32(cond, X86CondB) || isU32(cond, X86CondNB))) {
         
         
         UInt nnn = isU32(cond, X86CondB) ? 1 : 0;
         return
            unop(
               Iop_1Uto32,
               binop(
                  Iop_CmpEQ32,
                  binop(
                     Iop_And32,
                     binop(Iop_Shr32, cc_dep1, mkU8(X86G_CC_SHIFT_C)),
                     mkU32(1)
                  ),
                  mkU32(nnn)
               )
            );
      }

      if (isU32(cc_op, X86G_CC_OP_COPY) 
          && (isU32(cond, X86CondZ) || isU32(cond, X86CondNZ))) {
         
         
         UInt nnn = isU32(cond, X86CondZ) ? 1 : 0;
         return
            unop(
               Iop_1Uto32,
               binop(
                  Iop_CmpEQ32,
                  binop(
                     Iop_And32,
                     binop(Iop_Shr32, cc_dep1, mkU8(X86G_CC_SHIFT_Z)),
                     mkU32(1)
                  ),
                  mkU32(nnn)
               )
            );
      }

      if (isU32(cc_op, X86G_CC_OP_COPY) 
          && (isU32(cond, X86CondP) || isU32(cond, X86CondNP))) {
         
         
         UInt nnn = isU32(cond, X86CondP) ? 1 : 0;
         return
            unop(
               Iop_1Uto32,
               binop(
                  Iop_CmpEQ32,
                  binop(
                     Iop_And32,
                     binop(Iop_Shr32, cc_dep1, mkU8(X86G_CC_SHIFT_P)),
                     mkU32(1)
                  ),
                  mkU32(nnn)
               )
            );
      }

      return NULL;
   }

   

   if (vex_streq(function_name, "x86g_calculate_eflags_c")) {
      
      IRExpr *cc_op, *cc_dep1, *cc_dep2, *cc_ndep;
      vassert(arity == 4);
      cc_op   = args[0];
      cc_dep1 = args[1];
      cc_dep2 = args[2];
      cc_ndep = args[3];

      if (isU32(cc_op, X86G_CC_OP_SUBL)) {
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpLT32U, cc_dep1, cc_dep2));
      }
      if (isU32(cc_op, X86G_CC_OP_SUBB)) {
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpLT32U, 
                           binop(Iop_And32,cc_dep1,mkU32(0xFF)),
                           binop(Iop_And32,cc_dep2,mkU32(0xFF))));
      }
      if (isU32(cc_op, X86G_CC_OP_LOGICL)
          || isU32(cc_op, X86G_CC_OP_LOGICW)
          || isU32(cc_op, X86G_CC_OP_LOGICB)) {
         
         return mkU32(0);
      }
      if (isU32(cc_op, X86G_CC_OP_DECL) || isU32(cc_op, X86G_CC_OP_INCL)) {
         
         return cc_ndep;
      }
      if (isU32(cc_op, X86G_CC_OP_COPY)) {
         
         return
            binop(
               Iop_And32,
               binop(Iop_Shr32, cc_dep1, mkU8(X86G_CC_SHIFT_C)),
               mkU32(1)
            );
      }
      if (isU32(cc_op, X86G_CC_OP_ADDL)) {
         
         return unop(Iop_1Uto32,
                     binop(Iop_CmpLT32U, 
                           binop(Iop_Add32, cc_dep1, cc_dep2), 
                           cc_dep1));
      }
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
#     if 0
      if (cc_op->tag == Iex_Const) {
         vex_printf("CFLAG "); ppIRExpr(cc_op); vex_printf("\n");
      }
#     endif

      return NULL;
   }

   

   if (vex_streq(function_name, "x86g_calculate_eflags_all")) {
      
      IRExpr *cc_op, *cc_dep1; 
      vassert(arity == 4);
      cc_op   = args[0];
      cc_dep1 = args[1];
      
      

      if (isU32(cc_op, X86G_CC_OP_COPY)) {
         
         return
            binop(
               Iop_And32,
               cc_dep1,
               mkU32(X86G_CC_MASK_O | X86G_CC_MASK_S | X86G_CC_MASK_Z 
                     | X86G_CC_MASK_A | X86G_CC_MASK_C | X86G_CC_MASK_P)
            );
      }
      return NULL;
   }

#  undef unop
#  undef binop
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


UInt x86g_calculate_FXAM ( UInt tag, ULong dbl ) 
{
   Bool   mantissaIsZero;
   Int    bexp;
   UChar  sign;
   UChar* f64;

   vassert(host_is_little_endian());

   

   f64  = (UChar*)(&dbl);
   sign = toUChar( (f64[7] >> 7) & 1 );

   if (tag == 0) {
      
      return X86G_FC_MASK_C3 | 0 | (sign << X86G_FC_SHIFT_C1) 
                                 | X86G_FC_MASK_C0;
   }

   bexp = (f64[7] << 4) | ((f64[6] >> 4) & 0x0F);
   bexp &= 0x7FF;

   mantissaIsZero
      = toBool(
           (f64[6] & 0x0F) == 0 
           && (f64[5] | f64[4] | f64[3] | f64[2] | f64[1] | f64[0]) == 0
        );

   if (bexp == 0 && mantissaIsZero) {
      
      return X86G_FC_MASK_C3 | 0 
                             | (sign << X86G_FC_SHIFT_C1) | 0;
   }
   
   if (bexp == 0 && !mantissaIsZero) {
      
      return X86G_FC_MASK_C3 | X86G_FC_MASK_C2 
                             | (sign << X86G_FC_SHIFT_C1) | 0;
   }

   if (bexp == 0x7FF && mantissaIsZero) {
      
      return 0 | X86G_FC_MASK_C2 | (sign << X86G_FC_SHIFT_C1) 
                                 | X86G_FC_MASK_C0;
   }

   if (bexp == 0x7FF && !mantissaIsZero) {
      
      return 0 | 0 | (sign << X86G_FC_SHIFT_C1) | X86G_FC_MASK_C0;
   }

   
   return 0 | X86G_FC_MASK_C2 | (sign << X86G_FC_SHIFT_C1) | 0;
}


ULong x86g_dirtyhelper_loadF80le ( Addr addrU )
{
   ULong f64;
   convert_f80le_to_f64le ( (UChar*)addrU, (UChar*)&f64 );
   return f64;
}

void x86g_dirtyhelper_storeF80le ( Addr addrU, ULong f64 )
{
   convert_f64le_to_f80le( (UChar*)&f64, (UChar*)addrU );
}





ULong x86g_check_fldcw ( UInt fpucw )
{
   
   
   UInt rmode = (fpucw >> 10) & 3;

   
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

UInt x86g_create_fpucw ( UInt fpround )
{
   fpround &= 3;
   return 0x037F | (fpround << 10);
}


ULong x86g_check_ldmxcsr ( UInt mxcsr )
{
   
   
   UInt rmode = (mxcsr >> 13) & 3;

   
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


UInt x86g_create_mxcsr ( UInt sseround )
{
   sseround &= 3;
   return 0x1F80 | (sseround << 13);
}


void x86g_dirtyhelper_FINIT ( VexGuestX86State* gst )
{
   Int i;
   gst->guest_FTOP = 0;
   for (i = 0; i < 8; i++) {
      gst->guest_FPTAG[i] = 0; 
      gst->guest_FPREG[i] = 0; 
   }
   gst->guest_FPROUND = (UInt)Irrm_NEAREST;
   gst->guest_FC3210  = 0;
}


static
VexEmNote do_put_x87 ( Bool moveRegs,
                       UChar* x87_state,
                       VexGuestX86State* vex_state )
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

   pair    = x86g_check_fldcw ( (UInt)fpucw );
   fpround = (UInt)pair;
   ew      = (VexEmNote)(pair >> 32);
   
   vex_state->guest_FPROUND = fpround & 3;

   
   return ew;
}


static
void do_get_x87 ( VexGuestX86State* vex_state,
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
      = toUShort(x86g_create_fpucw( vex_state->guest_FPROUND ));

   
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


void x86g_dirtyhelper_FXSAVE ( VexGuestX86State* gst, HWord addr )
{
   
   Fpu_State tmp;
   UShort*   addrS = (UShort*)addr;
   UChar*    addrC = (UChar*)addr;
   U128*     xmm   = (U128*)(addr + 160);
   UInt      mxcsr;
   UShort    fp_tags;
   UInt      summary_tags;
   Int       r, stno;
   UShort    *srcS, *dstS;

   do_get_x87( gst, (UChar*)&tmp );
   mxcsr = x86g_create_mxcsr( gst->guest_SSEROUND );


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
   addrS[15] = 0xFFFF; 

   
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

   vassert(host_is_little_endian());

#  define COPY_U128(_dst,_src)                       \
      do { _dst[0] = _src[0]; _dst[1] = _src[1];     \
           _dst[2] = _src[2]; _dst[3] = _src[3]; }   \
      while (0)

   COPY_U128( xmm[0], gst->guest_XMM0 );
   COPY_U128( xmm[1], gst->guest_XMM1 );
   COPY_U128( xmm[2], gst->guest_XMM2 );
   COPY_U128( xmm[3], gst->guest_XMM3 );
   COPY_U128( xmm[4], gst->guest_XMM4 );
   COPY_U128( xmm[5], gst->guest_XMM5 );
   COPY_U128( xmm[6], gst->guest_XMM6 );
   COPY_U128( xmm[7], gst->guest_XMM7 );

#  undef COPY_U128
}


VexEmNote x86g_dirtyhelper_FXRSTOR ( VexGuestX86State* gst, HWord addr )
{
   Fpu_State tmp;
   VexEmNote warnX87 = EmNote_NONE;
   VexEmNote warnXMM = EmNote_NONE;
   UShort*   addrS   = (UShort*)addr;
   UChar*    addrC   = (UChar*)addr;
   U128*     xmm     = (U128*)(addr + 160);
   UShort    fp_tags;
   Int       r, stno, i;

   vassert(host_is_little_endian());

#  define COPY_U128(_dst,_src)                       \
      do { _dst[0] = _src[0]; _dst[1] = _src[1];     \
           _dst[2] = _src[2]; _dst[3] = _src[3]; }   \
      while (0)

   COPY_U128( gst->guest_XMM0, xmm[0] );
   COPY_U128( gst->guest_XMM1, xmm[1] );
   COPY_U128( gst->guest_XMM2, xmm[2] );
   COPY_U128( gst->guest_XMM3, xmm[3] );
   COPY_U128( gst->guest_XMM4, xmm[4] );
   COPY_U128( gst->guest_XMM5, xmm[5] );
   COPY_U128( gst->guest_XMM6, xmm[6] );
   COPY_U128( gst->guest_XMM7, xmm[7] );

#  undef COPY_U128


   for (i = 0; i < 7; i++) tmp.env[i+0] = 0;
   for (i = 0; i < 7; i++) tmp.env[i+7] = 0;
   
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
     ULong w64 = x86g_check_ldmxcsr( w32 );

     warnXMM = (VexEmNote)(w64 >> 32);

     gst->guest_SSEROUND = (UInt)w64;
   }

   
   if (warnX87 != EmNote_NONE)
      return warnX87;
   else
      return warnXMM;
}


void x86g_dirtyhelper_FSAVE ( VexGuestX86State* gst, HWord addr )
{
   do_get_x87( gst, (UChar*)addr );
}

VexEmNote x86g_dirtyhelper_FRSTOR ( VexGuestX86State* gst, HWord addr )
{
   return do_put_x87( True, (UChar*)addr, gst );
}

void x86g_dirtyhelper_FSTENV ( VexGuestX86State* gst, HWord addr )
{
   
   Int       i;
   UShort*   addrP = (UShort*)addr;
   Fpu_State tmp;
   do_get_x87( gst, (UChar*)&tmp );
   for (i = 0; i < 14; i++)
      addrP[i] = tmp.env[i];
}

VexEmNote x86g_dirtyhelper_FLDENV ( VexGuestX86State* gst, HWord addr )
{
   return do_put_x87( False, (UChar*)addr, gst);
}



ULong x86g_calculate_RCR ( UInt arg, UInt rot_amt, UInt eflags_in, UInt sz )
{
   UInt tempCOUNT = rot_amt & 0x1F, cf=0, of=0, tempcf;

   switch (sz) {
      case 4:
         cf        = (eflags_in >> X86G_CC_SHIFT_C) & 1;
         of        = ((arg >> 31) ^ cf) & 1;
         while (tempCOUNT > 0) {
            tempcf = arg & 1;
            arg    = (arg >> 1) | (cf << 31);
            cf     = tempcf;
            tempCOUNT--;
         }
         break;
      case 2:
         while (tempCOUNT >= 17) tempCOUNT -= 17;
         cf        = (eflags_in >> X86G_CC_SHIFT_C) & 1;
         of        = ((arg >> 15) ^ cf) & 1;
         while (tempCOUNT > 0) {
            tempcf = arg & 1;
            arg    = ((arg >> 1) & 0x7FFF) | (cf << 15);
            cf     = tempcf;
            tempCOUNT--;
         }
         break;
      case 1:
         while (tempCOUNT >= 9) tempCOUNT -= 9;
         cf        = (eflags_in >> X86G_CC_SHIFT_C) & 1;
         of        = ((arg >> 7) ^ cf) & 1;
         while (tempCOUNT > 0) {
            tempcf = arg & 1;
            arg    = ((arg >> 1) & 0x7F) | (cf << 7);
            cf     = tempcf;
            tempCOUNT--;
         }
         break;
      default: 
         vpanic("calculate_RCR: invalid size");
   }

   cf &= 1;
   of &= 1;
   eflags_in &= ~(X86G_CC_MASK_C | X86G_CC_MASK_O);
   eflags_in |= (cf << X86G_CC_SHIFT_C) | (of << X86G_CC_SHIFT_O);

   return (((ULong)eflags_in) << 32) | ((ULong)arg);
}


ULong x86g_calculate_RCL ( UInt arg, UInt rot_amt, UInt eflags_in, UInt sz )
{
   UInt tempCOUNT = rot_amt & 0x1F, cf=0, of=0, tempcf;

   switch (sz) {
      case 4:
         cf = (eflags_in >> X86G_CC_SHIFT_C) & 1;
         while (tempCOUNT > 0) {
            tempcf = (arg >> 31) & 1;
            arg    = (arg << 1) | (cf & 1);
            cf     = tempcf;
            tempCOUNT--;
         }
         of = ((arg >> 31) ^ cf) & 1;
         break;
      case 2:
         while (tempCOUNT >= 17) tempCOUNT -= 17;
         cf = (eflags_in >> X86G_CC_SHIFT_C) & 1;
         while (tempCOUNT > 0) {
            tempcf = (arg >> 15) & 1;
            arg    = 0xFFFF & ((arg << 1) | (cf & 1));
            cf     = tempcf;
            tempCOUNT--;
         }
         of = ((arg >> 15) ^ cf) & 1;
         break;
      case 1:
         while (tempCOUNT >= 9) tempCOUNT -= 9;
         cf = (eflags_in >> X86G_CC_SHIFT_C) & 1;
         while (tempCOUNT > 0) {
            tempcf = (arg >> 7) & 1;
            arg    = 0xFF & ((arg << 1) | (cf & 1));
            cf     = tempcf;
            tempCOUNT--;
         }
         of = ((arg >> 7) ^ cf) & 1;
         break;
      default: 
         vpanic("calculate_RCL: invalid size");
   }

   cf &= 1;
   of &= 1;
   eflags_in &= ~(X86G_CC_MASK_C | X86G_CC_MASK_O);
   eflags_in |= (cf << X86G_CC_SHIFT_C) | (of << X86G_CC_SHIFT_O);

   return (((ULong)eflags_in) << 32) | ((ULong)arg);
}


static UInt calc_parity_8bit ( UInt w32 ) {
   UInt i;
   UInt p = 1;
   for (i = 0; i < 8; i++)
      p ^= (1 & (w32 >> i));
   return p;
}
UInt x86g_calculate_daa_das_aaa_aas ( UInt flags_and_AX, UInt opcode )
{
   UInt r_AL = (flags_and_AX >> 0) & 0xFF;
   UInt r_AH = (flags_and_AX >> 8) & 0xFF;
   UInt r_O  = (flags_and_AX >> (16 + X86G_CC_SHIFT_O)) & 1;
   UInt r_S  = (flags_and_AX >> (16 + X86G_CC_SHIFT_S)) & 1;
   UInt r_Z  = (flags_and_AX >> (16 + X86G_CC_SHIFT_Z)) & 1;
   UInt r_A  = (flags_and_AX >> (16 + X86G_CC_SHIFT_A)) & 1;
   UInt r_C  = (flags_and_AX >> (16 + X86G_CC_SHIFT_C)) & 1;
   UInt r_P  = (flags_and_AX >> (16 + X86G_CC_SHIFT_P)) & 1;
   UInt result = 0;

   switch (opcode) {
      case 0x27: { 
         UInt old_AL = r_AL;
         UInt old_C  = r_C;
         r_C = 0;
         if ((r_AL & 0xF) > 9 || r_A == 1) {
            r_AL = r_AL + 6;
            r_C  = old_C;
            if (r_AL >= 0x100) r_C = 1;
            r_A = 1;
         } else {
            r_A = 0;
         }
         if (old_AL > 0x99 || old_C == 1) {
            r_AL = r_AL + 0x60;
            r_C  = 1;
         } else {
            r_C = 0;
         }
         r_AL &= 0xFF;
         r_O = 0; 
         r_S = (r_AL & 0x80) ? 1 : 0;
         r_Z = (r_AL == 0) ? 1 : 0;
         r_P = calc_parity_8bit( r_AL );
         break;
      }
      case 0x2F: { 
         UInt old_AL = r_AL;
         UInt old_C  = r_C;
         r_C = 0;
         if ((r_AL & 0xF) > 9 || r_A == 1) {
            Bool borrow = r_AL < 6;
            r_AL = r_AL - 6;
            r_C  = old_C;
            if (borrow) r_C = 1;
            r_A = 1;
         } else {
            r_A = 0;
         }
         if (old_AL > 0x99 || old_C == 1) {
            r_AL = r_AL - 0x60;
            r_C  = 1;
         } else {
            
         }
         r_AL &= 0xFF;
         r_O = 0; 
         r_S = (r_AL & 0x80) ? 1 : 0;
         r_Z = (r_AL == 0) ? 1 : 0;
         r_P = calc_parity_8bit( r_AL );
         break;
      }
      case 0x37: { 
         Bool nudge = r_AL > 0xF9;
         if ((r_AL & 0xF) > 9 || r_A == 1) {
            r_AL = r_AL + 6;
            r_AH = r_AH + 1 + (nudge ? 1 : 0);
            r_A  = 1;
            r_C  = 1;
            r_AL = r_AL & 0xF;
         } else {
            r_A  = 0;
            r_C  = 0;
            r_AL = r_AL & 0xF;
         }
         
         r_O = r_S = r_Z = r_P = 0; 
         break;
      }
      case 0x3F: { 
         Bool nudge = r_AL < 0x06;
         if ((r_AL & 0xF) > 9 || r_A == 1) {
            r_AL = r_AL - 6;
            r_AH = r_AH - 1 - (nudge ? 1 : 0);
            r_A  = 1;
            r_C  = 1;
            r_AL = r_AL & 0xF;
         } else {
            r_A  = 0;
            r_C  = 0;
            r_AL = r_AL & 0xF;
         }
         
         r_O = r_S = r_Z = r_P = 0; 
         break;
      }
      default:
         vassert(0);
   }
   result =   ( (r_O & 1) << (16 + X86G_CC_SHIFT_O) )
            | ( (r_S & 1) << (16 + X86G_CC_SHIFT_S) )
            | ( (r_Z & 1) << (16 + X86G_CC_SHIFT_Z) )
            | ( (r_A & 1) << (16 + X86G_CC_SHIFT_A) )
            | ( (r_C & 1) << (16 + X86G_CC_SHIFT_C) )
            | ( (r_P & 1) << (16 + X86G_CC_SHIFT_P) )
            | ( (r_AH & 0xFF) << 8 )
            | ( (r_AL & 0xFF) << 0 );
   return result;
}

UInt x86g_calculate_aad_aam ( UInt flags_and_AX, UInt opcode )
{
   UInt r_AL = (flags_and_AX >> 0) & 0xFF;
   UInt r_AH = (flags_and_AX >> 8) & 0xFF;
   UInt r_O  = (flags_and_AX >> (16 + X86G_CC_SHIFT_O)) & 1;
   UInt r_S  = (flags_and_AX >> (16 + X86G_CC_SHIFT_S)) & 1;
   UInt r_Z  = (flags_and_AX >> (16 + X86G_CC_SHIFT_Z)) & 1;
   UInt r_A  = (flags_and_AX >> (16 + X86G_CC_SHIFT_A)) & 1;
   UInt r_C  = (flags_and_AX >> (16 + X86G_CC_SHIFT_C)) & 1;
   UInt r_P  = (flags_and_AX >> (16 + X86G_CC_SHIFT_P)) & 1;
   UInt result = 0;

   switch (opcode) {
      case 0xD4: { 
         r_AH = r_AL / 10;
         r_AL = r_AL % 10;
         break;
      }
      case 0xD5: { 
         r_AL = ((r_AH * 10) + r_AL) & 0xff;
         r_AH = 0;
         break;
      }
      default:
         vassert(0);
   }

   r_O = 0; 
   r_C = 0; 
   r_A = 0; 
   r_S = (r_AL & 0x80) ? 1 : 0;
   r_Z = (r_AL == 0) ? 1 : 0;
   r_P = calc_parity_8bit( r_AL );

   result =   ( (r_O & 1) << (16 + X86G_CC_SHIFT_O) )
            | ( (r_S & 1) << (16 + X86G_CC_SHIFT_S) )
            | ( (r_Z & 1) << (16 + X86G_CC_SHIFT_Z) )
            | ( (r_A & 1) << (16 + X86G_CC_SHIFT_A) )
            | ( (r_C & 1) << (16 + X86G_CC_SHIFT_C) )
            | ( (r_P & 1) << (16 + X86G_CC_SHIFT_P) )
            | ( (r_AH & 0xFF) << 8 )
            | ( (r_AL & 0xFF) << 0 );
   return result;
}


ULong x86g_dirtyhelper_RDTSC ( void )
{
#  if defined(__i386__)
   ULong res;
   __asm__ __volatile__("rdtsc" : "=A" (res));
   return res;
#  else
   return 1ULL;
#  endif
}


void x86g_dirtyhelper_CPUID_sse0 ( VexGuestX86State* st )
{
   switch (st->guest_EAX) {
      case 0: 
         st->guest_EAX = 0x1;
         st->guest_EBX = 0x756e6547;
         st->guest_ECX = 0x6c65746e;
         st->guest_EDX = 0x49656e69;
         break;
      default:
         st->guest_EAX = 0x543;
         st->guest_EBX = 0x0;
         st->guest_ECX = 0x0;
         st->guest_EDX = 0x8001bf;
         break;
   }
}

void x86g_dirtyhelper_CPUID_mmxext ( VexGuestX86State* st )
{
   switch (st->guest_EAX) {
      
      case 0:
         st->guest_EAX = 0x1;
         st->guest_EBX = 0x68747541;
         st->guest_ECX = 0x444d4163;
         st->guest_EDX = 0x69746e65;
         break;
      
      case 1:
         st->guest_EAX = 0x621;
         st->guest_EBX = 0x0;
         st->guest_ECX = 0x0;
         st->guest_EDX = 0x183f9ff;
         break;
      
      case 0x80000000:
         st->guest_EAX = 0x80000004;
         st->guest_EBX = 0x68747541;
         st->guest_ECX = 0x444d4163;
         st->guest_EDX = 0x69746e65;
         break;
      
      case 0x80000001:
         st->guest_EAX = 0x721;
         st->guest_EBX = 0x0;
         st->guest_ECX = 0x0;
         st->guest_EDX = 0x1c3f9ff; 
         break;
      
      case 0x80000002:
         st->guest_EAX = 0x20444d41;
         st->guest_EBX = 0x6c687441;
         st->guest_ECX = 0x74286e6f;
         st->guest_EDX = 0x5020296d;
         break;
      case 0x80000003:
         st->guest_EAX = 0x65636f72;
         st->guest_EBX = 0x726f7373;
         st->guest_ECX = 0x0;
         st->guest_EDX = 0x0;
         break;
      default:
         st->guest_EAX = 0x0;
         st->guest_EBX = 0x0;
         st->guest_ECX = 0x0;
         st->guest_EDX = 0x0;
         break;
   }
}

void x86g_dirtyhelper_CPUID_sse1 ( VexGuestX86State* st )
{
   switch (st->guest_EAX) {
      case 0: 
         st->guest_EAX = 0x00000002;
         st->guest_EBX = 0x756e6547;
         st->guest_ECX = 0x6c65746e;
         st->guest_EDX = 0x49656e69;
         break;
      case 1: 
         st->guest_EAX = 0x000006b1;
         st->guest_EBX = 0x00000004;
         st->guest_ECX = 0x00000000;
         st->guest_EDX = 0x0383fbff;
         break;
      default:
         st->guest_EAX = 0x03020101;
         st->guest_EBX = 0x00000000;
         st->guest_ECX = 0x00000000;
         st->guest_EDX = 0x0c040883;
         break;
   }
}

void x86g_dirtyhelper_CPUID_sse2 ( VexGuestX86State* st )
{
#  define SET_ABCD(_a,_b,_c,_d)               \
      do { st->guest_EAX = (UInt)(_a);        \
           st->guest_EBX = (UInt)(_b);        \
           st->guest_ECX = (UInt)(_c);        \
           st->guest_EDX = (UInt)(_d);        \
      } while (0)

   switch (st->guest_EAX) {
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
         switch (st->guest_ECX) {
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
         SET_ABCD(0x00000000, 0x00000000, 0x00000001, 0x20100000);
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


UInt x86g_dirtyhelper_IN ( UInt portno, UInt sz )
{
#  if defined(__i386__)
   UInt r = 0;
   portno &= 0xFFFF;
   switch (sz) {
      case 4: 
         __asm__ __volatile__("movl $0,%%eax; inl %w1,%0" 
                              : "=a" (r) : "Nd" (portno));
	 break;
      case 2: 
         __asm__ __volatile__("movl $0,%%eax; inw %w1,%w0" 
                              : "=a" (r) : "Nd" (portno));
	 break;
      case 1: 
         __asm__ __volatile__("movl $0,%%eax; inb %w1,%b0" 
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


void x86g_dirtyhelper_OUT ( UInt portno, UInt data, UInt sz )
{
#  if defined(__i386__)
   portno &= 0xFFFF;
   switch (sz) {
      case 4: 
         __asm__ __volatile__("outl %0, %w1" 
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

void x86g_dirtyhelper_SxDT ( void *address, UInt op ) {
#  if defined(__i386__)
   switch (op) {
      case 0:
         __asm__ __volatile__("sgdt (%0)" : : "r" (address) : "memory");
         break;
      case 1:
         __asm__ __volatile__("sidt (%0)" : : "r" (address) : "memory");
         break;
      default:
         vpanic("x86g_dirtyhelper_SxDT");
   }
#  else
   
   UChar* p = (UChar*)address;
   p[0] = p[1] = p[2] = p[3] = p[4] = p[5] = 0;
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

ULong x86g_calculate_mmx_pmaddwd ( ULong xx, ULong yy )
{
   return
      mk32x2( 
         (((Int)(Short)sel16x4_3(xx)) * ((Int)(Short)sel16x4_3(yy)))
            + (((Int)(Short)sel16x4_2(xx)) * ((Int)(Short)sel16x4_2(yy))),
         (((Int)(Short)sel16x4_1(xx)) * ((Int)(Short)sel16x4_1(yy)))
            + (((Int)(Short)sel16x4_0(xx)) * ((Int)(Short)sel16x4_0(yy)))
      );
}

ULong x86g_calculate_mmx_psadbw ( ULong xx, ULong yy )
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



static inline 
UInt get_segdescr_base ( VexGuestX86SegDescr* ent )
{
   UInt lo  = 0xFFFF & (UInt)ent->LdtEnt.Bits.BaseLow;
   UInt mid =   0xFF & (UInt)ent->LdtEnt.Bits.BaseMid;
   UInt hi  =   0xFF & (UInt)ent->LdtEnt.Bits.BaseHi;
   return (hi << 24) | (mid << 16) | lo;
}

static inline
UInt get_segdescr_limit ( VexGuestX86SegDescr* ent )
{
    UInt lo    = 0xFFFF & (UInt)ent->LdtEnt.Bits.LimitLow;
    UInt hi    =    0xF & (UInt)ent->LdtEnt.Bits.LimitHi;
    UInt limit = (hi << 16) | lo;
    if (ent->LdtEnt.Bits.Granularity) 
       limit = (limit << 12) | 0xFFF;
    return limit;
}

ULong x86g_use_seg_selector ( HWord ldt, HWord gdt,
                              UInt seg_selector, UInt virtual_addr )
{
   UInt tiBit, base, limit;
   VexGuestX86SegDescr* the_descrs;

   Bool verboze = False;

   
   vassert(8 == sizeof(VexGuestX86SegDescr));

   if (verboze) 
      vex_printf("x86h_use_seg_selector: "
                 "seg_selector = 0x%x, vaddr = 0x%x\n", 
                 seg_selector, virtual_addr);

   
   if (seg_selector & ~0xFFFF)
      goto bad;

   seg_selector &= 0x0000FFFF;
  
   if ((seg_selector & 3) != 3)
      goto bad;

   
   tiBit = (seg_selector >> 2) & 1;

   
   seg_selector >>= 3;
   vassert(seg_selector >= 0 && seg_selector < 8192);

   if (tiBit == 0) {

      
      
      if (gdt == 0)
         goto bad;

      
      if (seg_selector >= VEX_GUEST_X86_GDT_NENT)
         goto bad;

      the_descrs = (VexGuestX86SegDescr*)gdt;
      base  = get_segdescr_base (&the_descrs[seg_selector]);
      limit = get_segdescr_limit(&the_descrs[seg_selector]);

   } else {

      
      if (ldt == 0)
         goto bad;

      if (seg_selector >= VEX_GUEST_X86_LDT_NENT)
         goto bad;

      the_descrs = (VexGuestX86SegDescr*)ldt;
      base  = get_segdescr_base (&the_descrs[seg_selector]);
      limit = get_segdescr_limit(&the_descrs[seg_selector]);

   }

   if (virtual_addr >= limit)
      goto bad;

   if (verboze) 
      vex_printf("x86h_use_seg_selector: "
                 "base = 0x%x, addr = 0x%x\n", 
                 base, base + virtual_addr);

   
   return (ULong)( ((UInt)virtual_addr) + base );

 bad:
   return 1ULL << 32;
}



void LibVEX_GuestX86_initialise ( VexGuestX86State* vex_state )
{
   vex_state->host_EvC_FAILADDR = 0;
   vex_state->host_EvC_COUNTER = 0;

   vex_state->guest_EAX = 0;
   vex_state->guest_ECX = 0;
   vex_state->guest_EDX = 0;
   vex_state->guest_EBX = 0;
   vex_state->guest_ESP = 0;
   vex_state->guest_EBP = 0;
   vex_state->guest_ESI = 0;
   vex_state->guest_EDI = 0;

   vex_state->guest_CC_OP   = X86G_CC_OP_COPY;
   vex_state->guest_CC_DEP1 = 0;
   vex_state->guest_CC_DEP2 = 0;
   vex_state->guest_CC_NDEP = 0;
   vex_state->guest_DFLAG   = 1; 
   vex_state->guest_IDFLAG  = 0;
   vex_state->guest_ACFLAG  = 0;

   vex_state->guest_EIP = 0;

   
   x86g_dirtyhelper_FINIT( vex_state );

   
#  define SSEZERO(_xmm) _xmm[0]=_xmm[1]=_xmm[2]=_xmm[3] = 0;

   vex_state->guest_SSEROUND = (UInt)Irrm_NEAREST;
   SSEZERO(vex_state->guest_XMM0);
   SSEZERO(vex_state->guest_XMM1);
   SSEZERO(vex_state->guest_XMM2);
   SSEZERO(vex_state->guest_XMM3);
   SSEZERO(vex_state->guest_XMM4);
   SSEZERO(vex_state->guest_XMM5);
   SSEZERO(vex_state->guest_XMM6);
   SSEZERO(vex_state->guest_XMM7);

#  undef SSEZERO

   vex_state->guest_CS  = 0;
   vex_state->guest_DS  = 0;
   vex_state->guest_ES  = 0;
   vex_state->guest_FS  = 0;
   vex_state->guest_GS  = 0;
   vex_state->guest_SS  = 0;
   vex_state->guest_LDT = 0;
   vex_state->guest_GDT = 0;

   vex_state->guest_EMNOTE = EmNote_NONE;

   
   vex_state->guest_CMSTART = 0;
   vex_state->guest_CMLEN   = 0;

   vex_state->guest_NRADDR   = 0;
   vex_state->guest_SC_CLASS = 0;
   vex_state->guest_IP_AT_SYSCALL = 0;

   vex_state->padding1 = 0;
}


Bool guest_x86_state_requires_precise_mem_exns (
        Int minoff, Int maxoff, VexRegisterUpdates pxControl
     )
{
   Int ebp_min = offsetof(VexGuestX86State, guest_EBP);
   Int ebp_max = ebp_min + 4 - 1;
   Int esp_min = offsetof(VexGuestX86State, guest_ESP);
   Int esp_max = esp_min + 4 - 1;
   Int eip_min = offsetof(VexGuestX86State, guest_EIP);
   Int eip_max = eip_min + 4 - 1;

   if (maxoff < esp_min || minoff > esp_max) {
      
      if (pxControl == VexRegUpdSpAtMemAccess)
         return False; 
   } else {
      return True;
   }

   if (maxoff < ebp_min || minoff > ebp_max) {
      
   } else {
      return True;
   }

   if (maxoff < eip_min || minoff > eip_max) {
      
   } else {
      return True;
   }

   return False;
}


#define ALWAYSDEFD(field)                           \
    { offsetof(VexGuestX86State, field),            \
      (sizeof ((VexGuestX86State*)0)->field) }

VexGuestLayout
   x86guest_layout 
      = { 
          
          .total_sizeB = sizeof(VexGuestX86State),

          
          .offset_SP = offsetof(VexGuestX86State,guest_ESP),
          .sizeof_SP = 4,

          
          .offset_FP = offsetof(VexGuestX86State,guest_EBP),
          .sizeof_FP = 4,

          
          .offset_IP = offsetof(VexGuestX86State,guest_EIP),
          .sizeof_IP = 4,

          .n_alwaysDefd = 24,

          .alwaysDefd 
             = {  ALWAYSDEFD(guest_CC_OP),
                  ALWAYSDEFD(guest_CC_NDEP),
                  ALWAYSDEFD(guest_DFLAG),
                  ALWAYSDEFD(guest_IDFLAG),
                  ALWAYSDEFD(guest_ACFLAG),
                  ALWAYSDEFD(guest_EIP),
                  ALWAYSDEFD(guest_FTOP),
                  ALWAYSDEFD(guest_FPTAG),
                  ALWAYSDEFD(guest_FPROUND),
                  ALWAYSDEFD(guest_FC3210),
                  ALWAYSDEFD(guest_CS),
                  ALWAYSDEFD(guest_DS),
                  ALWAYSDEFD(guest_ES),
                  ALWAYSDEFD(guest_FS),
                  ALWAYSDEFD(guest_GS),
                  ALWAYSDEFD(guest_SS),
                  ALWAYSDEFD(guest_LDT),
                  ALWAYSDEFD(guest_GDT),
                  ALWAYSDEFD(guest_EMNOTE),
                  ALWAYSDEFD(guest_SSEROUND),
                  ALWAYSDEFD(guest_CMSTART),
                  ALWAYSDEFD(guest_CMLEN),
                  ALWAYSDEFD(guest_SC_CLASS),
                  ALWAYSDEFD(guest_IP_AT_SYSCALL)
               }
        };


