

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

#include "main_util.h"
#include "guest_generic_x87.h"




static inline UInt read_bit_array ( UChar* arr, UInt n )
{
   UChar c = arr[n >> 3];
   c >>= (n&7);
   return c & 1;
}

static inline void write_bit_array ( UChar* arr, UInt n, UInt b )
{
   UChar c = arr[n >> 3];
   c = toUChar( c & ~(1 << (n&7)) );
   c = toUChar( c | ((b&1) << (n&7)) );
   arr[n >> 3] = c;
}

void convert_f64le_to_f80le ( UChar* f64, UChar* f80 )
{
   Bool  mantissaIsZero;
   Int   bexp, i, j, shift;
   UChar sign;

   sign = toUChar( (f64[7] >> 7) & 1 );
   bexp = (f64[7] << 4) | ((f64[6] >> 4) & 0x0F);
   bexp &= 0x7FF;

   mantissaIsZero = False;
   if (bexp == 0 || bexp == 0x7FF) {
      mantissaIsZero
         = toBool( 
              (f64[6] & 0x0F) == 0 
              && f64[5] == 0 && f64[4] == 0 && f64[3] == 0 
              && f64[2] == 0 && f64[1] == 0 && f64[0] == 0
           );
   }

   if (bexp == 0) {
      f80[9] = toUChar( sign << 7 );
      f80[8] = f80[7] = f80[6] = f80[5] = f80[4]
             = f80[3] = f80[2] = f80[1] = f80[0] = 0;

      if (mantissaIsZero)
         
         return;

      shift = 0;
      for (i = 51; i >= 0; i--) {
        if (read_bit_array(f64, i))
           break;
        shift++;
      }

      
      j = 63;
      for (i = 51 - shift; i >= 0; i--) {
         write_bit_array( f80, j,
     	 read_bit_array( f64, i ) );
         j--;
      }

      
      bexp -= shift;
      bexp += (16383 - 1023);
      f80[9] = toUChar( (sign << 7) | ((bexp >> 8) & 0xFF) );
      f80[8] = toUChar( bexp & 0xFF );
      return;
   }

   if (bexp == 0x7FF) {
      if (mantissaIsZero) {
         f80[9] = toUChar( (sign << 7) | 0x7F );
         f80[8] = 0xFF;
         f80[7] = 0x80;
         f80[6] = f80[5] = f80[4] = f80[3] 
                = f80[2] = f80[1] = f80[0] = 0;
         return;
      }
      if (f64[6] & 8) {
         f80[9] = toUChar( (sign << 7) | 0x7F );
         f80[8] = 0xFF;
         f80[7] = 0xC0;
         f80[6] = f80[5] = f80[4] = f80[3] 
                = f80[2] = f80[1] = f80[0] = 0x00;
      } else {
         f80[9] = toUChar( (sign << 7) | 0x7F );
         f80[8] = 0xFF;
         f80[7] = 0xBF;
         f80[6] = f80[5] = f80[4] = f80[3] 
                = f80[2] = f80[1] = f80[0] = 0xFF;
      }
      return;
   }

   bexp += (16383 - 1023);

   f80[9] = toUChar( (sign << 7) | ((bexp >> 8) & 0xFF) );
   f80[8] = toUChar( bexp & 0xFF );
   f80[7] = toUChar( (1 << 7) | ((f64[6] << 3) & 0x78) 
                              | ((f64[5] >> 5) & 7) );
   f80[6] = toUChar( ((f64[5] << 3) & 0xF8) | ((f64[4] >> 5) & 7) );
   f80[5] = toUChar( ((f64[4] << 3) & 0xF8) | ((f64[3] >> 5) & 7) );
   f80[4] = toUChar( ((f64[3] << 3) & 0xF8) | ((f64[2] >> 5) & 7) );
   f80[3] = toUChar( ((f64[2] << 3) & 0xF8) | ((f64[1] >> 5) & 7) );
   f80[2] = toUChar( ((f64[1] << 3) & 0xF8) | ((f64[0] >> 5) & 7) );
   f80[1] = toUChar( ((f64[0] << 3) & 0xF8) );
   f80[0] = toUChar( 0 );
}


void convert_f80le_to_f64le ( UChar* f80, UChar* f64 )
{
   Bool  isInf;
   Int   bexp, i, j;
   UChar sign;

   sign = toUChar((f80[9] >> 7) & 1);
   bexp = (((UInt)f80[9]) << 8) | (UInt)f80[8];
   bexp &= 0x7FFF;

   if (bexp == 0) {
      f64[7] = toUChar(sign << 7);
      f64[6] = f64[5] = f64[4] = f64[3] = f64[2] = f64[1] = f64[0] = 0;
      return;
   }
   
   if (bexp == 0x7FFF) {
      isInf = toBool(
                 (f80[7] & 0x7F) == 0 
                 && f80[6] == 0 && f80[5] == 0 && f80[4] == 0 
                 && f80[3] == 0 && f80[2] == 0 && f80[1] == 0 
                 && f80[0] == 0
              );
      if (isInf) {
         if (0 == (f80[7] & 0x80))
            goto wierd_NaN;
         f64[7] = toUChar((sign << 7) | 0x7F);
         f64[6] = 0xF0;
         f64[5] = f64[4] = f64[3] = f64[2] = f64[1] = f64[0] = 0;
         return;
      }
      if (f80[7] & 0x40) {
         f64[7] = toUChar((sign << 7) | 0x7F);
         f64[6] = 0xF8;
         f64[5] = f64[4] = f64[3] = f64[2] = f64[1] = f64[0] = 0x00;
      } else {
         f64[7] = toUChar((sign << 7) | 0x7F);
         f64[6] = 0xF7;
         f64[5] = f64[4] = f64[3] = f64[2] = f64[1] = f64[0] = 0xFF;
      }
      return;
   }

   if (0 == (f80[7] & 0x80)) {
      wierd_NaN:
      f64[7] = (1  << 7) | 0x7F;
      f64[6] = 0xF8;
      f64[5] = f64[4] = f64[3] = f64[2] = f64[1] = f64[0] = 0;
      return;
   }

   bexp -= (16383 - 1023);
   if (bexp >= 0x7FF) {
      
      f64[7] = toUChar((sign << 7) | 0x7F);
      f64[6] = 0xF0;
      f64[5] = f64[4] = f64[3] = f64[2] = f64[1] = f64[0] = 0;
      return;
   }

   if (bexp <= 0) {
      f64[7] = toUChar(sign << 7);
      f64[6] = f64[5] = f64[4] = f64[3] = f64[2] = f64[1] = f64[0] = 0;

      if (bexp < -52)
         
         return;

      
      
      for (i = 63; i >= 0; i--) {
         j = i - 12 + bexp;
         if (j < 0) break;
         
         vassert(j >= 0 && j < 52);
         write_bit_array ( f64,
                           j,
                           read_bit_array ( f80, i ) );
      }
      
      if (read_bit_array(f80, 10+1 - bexp) == 1) 
         goto do_rounding;

      return;
   }

   f64[0] = toUChar( (f80[1] >> 3) | (f80[2] << 5) );
   f64[1] = toUChar( (f80[2] >> 3) | (f80[3] << 5) );
   f64[2] = toUChar( (f80[3] >> 3) | (f80[4] << 5) );
   f64[3] = toUChar( (f80[4] >> 3) | (f80[5] << 5) );
   f64[4] = toUChar( (f80[5] >> 3) | (f80[6] << 5) );
   f64[5] = toUChar( (f80[6] >> 3) | (f80[7] << 5) );

   f64[6] = toUChar( ((bexp << 4) & 0xF0) | ((f80[7] >> 3) & 0x0F) );

   f64[7] = toUChar( (sign << 7) | ((bexp >> 4) & 0x7F) );

   if (f80[1] & 4)  {

      if ((f80[1] & 0xF) == 4 && f80[0] == 0)
         return;

      do_rounding:
      if (f64[0] != 0xFF) { 
         f64[0]++; 
      }
      else 
      if (f64[0] == 0xFF && f64[1] != 0xFF) {
         f64[0] = 0;
         f64[1]++;
      }
      else      
      if (f64[0] == 0xFF && f64[1] == 0xFF && f64[2] != 0xFF) {
         f64[0] = 0;
         f64[1] = 0;
         f64[2]++;
      }
      
   }
}


ULong x86amd64g_calculate_FXTRACT ( ULong arg, HWord getExp )
{
   ULong  uSig, uExp;
   
   Int    sExp, i;
   UInt   sign, expExp;

   const ULong posInf  = 0x7FF0000000000000ULL;
   const ULong negInf  = 0xFFF0000000000000ULL;
   const ULong nanMask = 0x7FF0000000000000ULL;
   const ULong qNan    = 0x7FF8000000000000ULL;
   const ULong posZero = 0x0000000000000000ULL;
   const ULong negZero = 0x8000000000000000ULL;
   const ULong bit51   = 1ULL << 51;
   const ULong bit52   = 1ULL << 52;
   const ULong sigMask = bit52 - 1;

   
   if (arg == posInf)
      return getExp ? posInf : posInf;
   if (arg == negInf)
      return getExp ? posInf : negInf;
   if ((arg & nanMask) == nanMask)
      return qNan | (arg & (1ULL << 63));
   if (arg == posZero)
      return getExp ? negInf : posZero;
   if (arg == negZero)
      return getExp ? negInf : negZero;

   
   sign = ((UInt)(arg >> 63)) & 1;

   
   uSig = arg & sigMask;

   
   sExp = ((Int)(arg >> 52)) & 0x7FF;

   if (sExp == 0) {
      for (i = 0; i < 52; i++) {
         if (uSig & bit51)
            break;
         uSig <<= 1;
         sExp--;
      }
      uSig <<= 1;
   } else {
      
      uSig |= bit52;
   }

   
   
   

   
    
    
   uSig &= sigMask;
   uSig |= 0x3FF0000000000000ULL;
   if (sign)
      uSig ^= negZero;

   
   
   sExp -= 1023;
   if (sExp == 0) {
      uExp = 0;
   } else {
      uExp   = sExp < 0 ? -sExp : sExp;
      expExp = 0x3FF +52;
      
      uExp <<= 42;
      expExp -= 42;
      for (i = 0; i < 52-42; i++) {
         if (uExp & bit52)
            break;
         uExp <<= 1;
         expExp--;
      }
      uExp &= sigMask;
      uExp |= ((ULong)expExp) << 52;
      if (sExp < 0) uExp ^= negZero;
   }

   return getExp ? uExp : uSig;
}





#define SHIFT_O   11
#define SHIFT_S   7
#define SHIFT_Z   6
#define SHIFT_A   4
#define SHIFT_C   0
#define SHIFT_P   2

#define MASK_O    (1 << SHIFT_O)
#define MASK_S    (1 << SHIFT_S)
#define MASK_Z    (1 << SHIFT_Z)
#define MASK_A    (1 << SHIFT_A)
#define MASK_C    (1 << SHIFT_C)
#define MASK_P    (1 << SHIFT_P)


static UInt clz32 ( UInt x )
{
   Int y, m, n;
   y = -(x >> 16);
   m = (y >> 16) & 16;
   n = 16 - m;
   x = x >> m;
   y = x - 0x100;
   m = (y >> 16) & 8;
   n = n + m;
   x = x << m;
   y = x - 0x1000;
   m = (y >> 16) & 4;
   n = n + m;
   x = x << m;
   y = x - 0x4000;
   m = (y >> 16) & 2;
   n = n + m;
   x = x << m;
   y = x >> 14;
   m = y & ~(y >> 1);
   return n + 2 - m;
}

static UInt ctz32 ( UInt x )
{
   return 32 - clz32((~x) & (x-1));
}

static UInt bits4_to_bytes4 ( UInt bits4 )
{
   UInt r = 0;
   r |= (bits4 & 1) ? 0x000000FF : 0;
   r |= (bits4 & 2) ? 0x0000FF00 : 0;
   r |= (bits4 & 4) ? 0x00FF0000 : 0;
   r |= (bits4 & 8) ? 0xFF000000 : 0;
   return r;
}


static UInt bits2_to_bytes4 ( UInt bits2 )
{
   UInt r = 0;
   r |= (bits2 & 1) ? 0x0000FFFF : 0;
   r |= (bits2 & 2) ? 0xFFFF0000 : 0;
   return r;
}


static
void compute_PCMPxSTRx_gen_output (V128* resV,
                                   UInt* resOSZACP,
                                   UInt intRes1,
                                   UInt zmaskL, UInt zmaskR,
                                   UInt validL,
                                   UInt pol, UInt idx,
                                   Bool isxSTRM )
{
   vassert((pol >> 2) == 0);
   vassert((idx >> 1) == 0);

   UInt intRes2 = 0;
   switch (pol) {
      case 0: intRes2 = intRes1;          break; 
      case 1: intRes2 = ~intRes1;         break; 
      case 2: intRes2 = intRes1;          break; 
      case 3: intRes2 = intRes1 ^ validL; break; 
   }
   intRes2 &= 0xFFFF;

   if (isxSTRM) {
 
      
      if (idx) {
         resV->w32[0] = bits4_to_bytes4( (intRes2 >>  0) & 0xF );
         resV->w32[1] = bits4_to_bytes4( (intRes2 >>  4) & 0xF );
         resV->w32[2] = bits4_to_bytes4( (intRes2 >>  8) & 0xF );
         resV->w32[3] = bits4_to_bytes4( (intRes2 >> 12) & 0xF );
      } else {
         resV->w32[0] = intRes2 & 0xFFFF;
         resV->w32[1] = 0;
         resV->w32[2] = 0;
         resV->w32[3] = 0;
      }

   } else {

      
      
      UInt newECX = 0;
      if (idx) {
         
         newECX = intRes2 == 0 ? 16 : (31 - clz32(intRes2));
      } else {
         
         newECX = intRes2 == 0 ? 16 : ctz32(intRes2);
      }

      resV->w32[0] = newECX;
      resV->w32[1] = 0;
      resV->w32[2] = 0;
      resV->w32[3] = 0;

   }

   
   *resOSZACP    
     = ((intRes2 == 0) ? 0 : MASK_C) 
     | ((zmaskL == 0)  ? 0 : MASK_Z) 
     | ((zmaskR == 0)  ? 0 : MASK_S) 
     | ((intRes2 & 1) << SHIFT_O);   
}


static
void compute_PCMPxSTRx_gen_output_wide (V128* resV,
                                        UInt* resOSZACP,
                                        UInt intRes1,
                                        UInt zmaskL, UInt zmaskR,
                                        UInt validL,
                                        UInt pol, UInt idx,
                                        Bool isxSTRM )
{
   vassert((pol >> 2) == 0);
   vassert((idx >> 1) == 0);

   UInt intRes2 = 0;
   switch (pol) {
      case 0: intRes2 = intRes1;          break; 
      case 1: intRes2 = ~intRes1;         break; 
      case 2: intRes2 = intRes1;          break; 
      case 3: intRes2 = intRes1 ^ validL; break; 
   }
   intRes2 &= 0xFF;

   if (isxSTRM) {
 
      
      if (idx) {
         resV->w32[0] = bits2_to_bytes4( (intRes2 >> 0) & 0x3 );
         resV->w32[1] = bits2_to_bytes4( (intRes2 >> 2) & 0x3 );
         resV->w32[2] = bits2_to_bytes4( (intRes2 >> 4) & 0x3 );
         resV->w32[3] = bits2_to_bytes4( (intRes2 >> 6) & 0x3 );
      } else {
         resV->w32[0] = intRes2 & 0xFF;
         resV->w32[1] = 0;
         resV->w32[2] = 0;
         resV->w32[3] = 0;
      }

   } else {

      
      
      UInt newECX = 0;
      if (idx) {
         
         newECX = intRes2 == 0 ? 8 : (31 - clz32(intRes2));
      } else {
         
         newECX = intRes2 == 0 ? 8 : ctz32(intRes2);
      }

      resV->w32[0] = newECX;
      resV->w32[1] = 0;
      resV->w32[2] = 0;
      resV->w32[3] = 0;

   }

   
   *resOSZACP    
     = ((intRes2 == 0) ? 0 : MASK_C) 
     | ((zmaskL == 0)  ? 0 : MASK_Z) 
     | ((zmaskR == 0)  ? 0 : MASK_S) 
     | ((intRes2 & 1) << SHIFT_O);   
}


/* Compute result and new OSZACP flags for all PCMP{E,I}STR{I,M}
   variants on 8-bit data.

   For xSTRI variants, the new ECX value is placed in the 32 bits
   pointed to by *resV, and the top 96 bits are zeroed.  For xSTRM
   variants, the result is a 128 bit value and is placed at *resV in
   the obvious way.

   For all variants, the new OSZACP value is placed at *resOSZACP.

   argLV and argRV are the vector args.  The caller must prepare a
   16-bit mask for each, zmaskL and zmaskR.  For ISTRx variants this
   must be 1 for each zero byte of of the respective arg.  For ESTRx
   variants this is derived from the explicit length indication, and
   must be 0 in all places except at the bit index corresponding to
   the valid length (0 .. 16).  If the valid length is 16 then the
   mask must be all zeroes.  In all cases, bits 31:16 must be zero.

   imm8 is the original immediate from the instruction.  isSTRM
   indicates whether this is a xSTRM or xSTRI variant, which controls
   how much of *res is written.

   If the given imm8 case can be handled, the return value is True.
   If not, False is returned, and neither *res not *resOSZACP are
   altered.
*/

Bool compute_PCMPxSTRx ( V128* resV,
                         UInt* resOSZACP,
                         V128* argLV,  V128* argRV,
                         UInt zmaskL, UInt zmaskR,
                         UInt imm8,   Bool isxSTRM )
{
   vassert(imm8 < 0x80);
   vassert((zmaskL >> 16) == 0);
   vassert((zmaskR >> 16) == 0);

   switch (imm8) {
      case 0x00: case 0x02: case 0x08: case 0x0A: case 0x0C: case 0x0E:
      case 0x12: case 0x14: case 0x1A:
      case 0x30: case 0x34: case 0x38: case 0x3A:
      case 0x40: case 0x44: case 0x46: case 0x4A:
         break;
      default:
         return False;
   }

   UInt fmt = (imm8 >> 0) & 3; 
   UInt agg = (imm8 >> 2) & 3; 
   UInt pol = (imm8 >> 4) & 3; 
   UInt idx = (imm8 >> 6) & 1; 

   
   
   

   if (agg == 2
       && (fmt == 0 || fmt == 2)) {
      Int    i;
      UChar* argL = (UChar*)argLV;
      UChar* argR = (UChar*)argRV;
      UInt boolResII = 0;
      for (i = 15; i >= 0; i--) {
         UChar cL  = argL[i];
         UChar cR  = argR[i];
         boolResII = (boolResII << 1) | (cL == cR ? 1 : 0);
      }
      UInt validL = ~(zmaskL | -zmaskL);  
      UInt validR = ~(zmaskR | -zmaskR);  

      
      UInt intRes1
         = (boolResII & validL & validR)  
           | (~ (validL | validR));       
                                          
      intRes1 &= 0xFFFF;

      
      compute_PCMPxSTRx_gen_output(
         resV, resOSZACP,
         intRes1, zmaskL, zmaskR, validL, pol, idx, isxSTRM
      );

      return True;
   }

   
   
   

   if (agg == 0
       && (fmt == 0 || fmt == 2)) {
      
      UInt   si, ci;
      UChar* argL    = (UChar*)argLV;
      UChar* argR    = (UChar*)argRV;
      UInt   boolRes = 0;
      UInt   validL  = ~(zmaskL | -zmaskL);  
      UInt   validR  = ~(zmaskR | -zmaskR);  

      for (si = 0; si < 16; si++) {
         if ((validL & (1 << si)) == 0)
            
            break;
         UInt m = 0;
         for (ci = 0; ci < 16; ci++) {
            if ((validR & (1 << ci)) == 0) break;
            if (argR[ci] == argL[si]) { m = 1; break; }
         }
         boolRes |= (m << si);
      }

      
      UInt intRes1 = boolRes & 0xFFFF;
   
      
      compute_PCMPxSTRx_gen_output(
         resV, resOSZACP,
         intRes1, zmaskL, zmaskR, validL, pol, idx, isxSTRM
      );

      return True;
   }

   
   
   

   if (agg == 3
       && (fmt == 0 || fmt == 2)) {

      
      UInt   ni, hi;
      UChar* argL    = (UChar*)argLV;
      UChar* argR    = (UChar*)argRV;
      UInt   boolRes = 0;
      UInt   validL  = ~(zmaskL | -zmaskL);  
      UInt   validR  = ~(zmaskR | -zmaskR);  
      for (hi = 0; hi < 16; hi++) {
         UInt m = 1;
         for (ni = 0; ni < 16; ni++) {
            if ((validR & (1 << ni)) == 0) break;
            UInt i = ni + hi;
            if (i >= 16) break;
            if (argL[i] != argR[ni]) { m = 0; break; }
         }
         boolRes |= (m << hi);
         if ((validL & (1 << hi)) == 0)
            
            break;
      }

      
      UInt intRes1 = boolRes & 0xFFFF;

      
      compute_PCMPxSTRx_gen_output(
         resV, resOSZACP,
         intRes1, zmaskL, zmaskR, validL, pol, idx, isxSTRM
      );

      return True;
   }

   
   
   

   if (agg == 1
       && fmt == 0) {

      
      UInt   ri, si;
      UChar* argL    = (UChar*)argLV;
      UChar* argR    = (UChar*)argRV;
      UInt   boolRes = 0;
      UInt   validL  = ~(zmaskL | -zmaskL);  
      UInt   validR  = ~(zmaskR | -zmaskR);  
      for (si = 0; si < 16; si++) {
         if ((validL & (1 << si)) == 0)
            
            break;
         UInt m = 0;
         for (ri = 0; ri < 16; ri += 2) {
            if ((validR & (3 << ri)) != (3 << ri)) break;
            if (argR[ri] <= argL[si] && argL[si] <= argR[ri+1]) { 
               m = 1; break;
            }
         }
         boolRes |= (m << si);
      }

      
      UInt intRes1 = boolRes & 0xFFFF;

      
      compute_PCMPxSTRx_gen_output(
         resV, resOSZACP,
         intRes1, zmaskL, zmaskR, validL, pol, idx, isxSTRM
      );

      return True;
   }

   
   
   

   if (agg == 1
       && fmt == 2) {

      
      UInt   ri, si;
      Char*  argL    = (Char*)argLV;
      Char*  argR    = (Char*)argRV;
      UInt   boolRes = 0;
      UInt   validL  = ~(zmaskL | -zmaskL);  
      UInt   validR  = ~(zmaskR | -zmaskR);  
      for (si = 0; si < 16; si++) {
         if ((validL & (1 << si)) == 0)
            
            break;
         UInt m = 0;
         for (ri = 0; ri < 16; ri += 2) {
            if ((validR & (3 << ri)) != (3 << ri)) break;
            if (argR[ri] <= argL[si] && argL[si] <= argR[ri+1]) { 
               m = 1; break;
            }
         }
         boolRes |= (m << si);
      }

      
      UInt intRes1 = boolRes & 0xFFFF;

      
      compute_PCMPxSTRx_gen_output(
         resV, resOSZACP,
         intRes1, zmaskL, zmaskR, validL, pol, idx, isxSTRM
      );

      return True;
   }

   return False;
}


/* Compute result and new OSZACP flags for all PCMP{E,I}STR{I,M}
   variants on 16-bit characters.

   For xSTRI variants, the new ECX value is placed in the 32 bits
   pointed to by *resV, and the top 96 bits are zeroed.  For xSTRM
   variants, the result is a 128 bit value and is placed at *resV in
   the obvious way.

   For all variants, the new OSZACP value is placed at *resOSZACP.

   argLV and argRV are the vector args.  The caller must prepare a
   8-bit mask for each, zmaskL and zmaskR.  For ISTRx variants this
   must be 1 for each zero byte of of the respective arg.  For ESTRx
   variants this is derived from the explicit length indication, and
   must be 0 in all places except at the bit index corresponding to
   the valid length (0 .. 8).  If the valid length is 8 then the
   mask must be all zeroes.  In all cases, bits 31:8 must be zero.

   imm8 is the original immediate from the instruction.  isSTRM
   indicates whether this is a xSTRM or xSTRI variant, which controls
   how much of *res is written.

   If the given imm8 case can be handled, the return value is True.
   If not, False is returned, and neither *res not *resOSZACP are
   altered.
*/

Bool compute_PCMPxSTRx_wide ( V128* resV,
                              UInt* resOSZACP,
                              V128* argLV,  V128* argRV,
                              UInt zmaskL, UInt zmaskR,
                              UInt imm8,   Bool isxSTRM )
{
   vassert(imm8 < 0x80);
   vassert((zmaskL >> 8) == 0);
   vassert((zmaskR >> 8) == 0);

   switch (imm8) {
      case 0x01: case 0x03: case 0x09: case 0x0B: case 0x0D:
      case 0x13:            case 0x1B:
                            case 0x39: case 0x3B:
                 case 0x45:            case 0x4B:
         break;
      default:
         return False;
   }

   UInt fmt = (imm8 >> 0) & 3; 
   UInt agg = (imm8 >> 2) & 3; 
   UInt pol = (imm8 >> 4) & 3; 
   UInt idx = (imm8 >> 6) & 1; 

   
   
   

   if (agg == 2
       && (fmt == 1 || fmt == 3)) {
      Int     i;
      UShort* argL = (UShort*)argLV;
      UShort* argR = (UShort*)argRV;
      UInt boolResII = 0;
      for (i = 7; i >= 0; i--) {
         UShort cL  = argL[i];
         UShort cR  = argR[i];
         boolResII = (boolResII << 1) | (cL == cR ? 1 : 0);
      }
      UInt validL = ~(zmaskL | -zmaskL);  
      UInt validR = ~(zmaskR | -zmaskR);  

      
      UInt intRes1
         = (boolResII & validL & validR)  
           | (~ (validL | validR));       
                                          
      intRes1 &= 0xFF;

      
      compute_PCMPxSTRx_gen_output_wide(
         resV, resOSZACP,
         intRes1, zmaskL, zmaskR, validL, pol, idx, isxSTRM
      );

      return True;
   }

   
   
   

   if (agg == 0
       && (fmt == 1 || fmt == 3)) {
      
      UInt    si, ci;
      UShort* argL    = (UShort*)argLV;
      UShort* argR    = (UShort*)argRV;
      UInt    boolRes = 0;
      UInt    validL  = ~(zmaskL | -zmaskL);  
      UInt    validR  = ~(zmaskR | -zmaskR);  

      for (si = 0; si < 8; si++) {
         if ((validL & (1 << si)) == 0)
            
            break;
         UInt m = 0;
         for (ci = 0; ci < 8; ci++) {
            if ((validR & (1 << ci)) == 0) break;
            if (argR[ci] == argL[si]) { m = 1; break; }
         }
         boolRes |= (m << si);
      }

      
      UInt intRes1 = boolRes & 0xFF;
   
      
      compute_PCMPxSTRx_gen_output_wide(
         resV, resOSZACP,
         intRes1, zmaskL, zmaskR, validL, pol, idx, isxSTRM
      );

      return True;
   }

   
   
   

   if (agg == 3
       && (fmt == 1 || fmt == 3)) {

      
      UInt    ni, hi;
      UShort* argL    = (UShort*)argLV;
      UShort* argR    = (UShort*)argRV;
      UInt    boolRes = 0;
      UInt    validL  = ~(zmaskL | -zmaskL);  
      UInt    validR  = ~(zmaskR | -zmaskR);  
      for (hi = 0; hi < 8; hi++) {
         UInt m = 1;
         for (ni = 0; ni < 8; ni++) {
            if ((validR & (1 << ni)) == 0) break;
            UInt i = ni + hi;
            if (i >= 8) break;
            if (argL[i] != argR[ni]) { m = 0; break; }
         }
         boolRes |= (m << hi);
         if ((validL & (1 << hi)) == 0)
            
            break;
      }

      
      UInt intRes1 = boolRes & 0xFF;

      
      compute_PCMPxSTRx_gen_output_wide(
         resV, resOSZACP,
         intRes1, zmaskL, zmaskR, validL, pol, idx, isxSTRM
      );

      return True;
   }

   
   
   

   if (agg == 1
       && fmt == 1) {

      
      UInt    ri, si;
      UShort* argL    = (UShort*)argLV;
      UShort* argR    = (UShort*)argRV;
      UInt    boolRes = 0;
      UInt    validL  = ~(zmaskL | -zmaskL);  
      UInt    validR  = ~(zmaskR | -zmaskR);  
      for (si = 0; si < 8; si++) {
         if ((validL & (1 << si)) == 0)
            
            break;
         UInt m = 0;
         for (ri = 0; ri < 8; ri += 2) {
            if ((validR & (3 << ri)) != (3 << ri)) break;
            if (argR[ri] <= argL[si] && argL[si] <= argR[ri+1]) { 
               m = 1; break;
            }
         }
         boolRes |= (m << si);
      }

      
      UInt intRes1 = boolRes & 0xFF;

      
      compute_PCMPxSTRx_gen_output_wide(
         resV, resOSZACP,
         intRes1, zmaskL, zmaskR, validL, pol, idx, isxSTRM
      );

      return True;
   }

   return False;
}


