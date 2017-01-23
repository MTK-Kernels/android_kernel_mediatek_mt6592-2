

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright IBM Corp. 2010-2013

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


#include "libvex_basictypes.h"
#include "libvex_emnote.h"
#include "libvex_guest_s390x.h"
#include "libvex_ir.h"
#include "libvex.h"
#include "libvex_s390x_common.h"

#include "main_util.h"
#include "main_globals.h"
#include "guest_generic_bb_to_IR.h"
#include "guest_s390_defs.h"
#include "s390_defs.h"               

void
LibVEX_GuestS390X_initialise(VexGuestS390XState *state)
{

   state->guest_a0 = 0;
   state->guest_a1 = 0;
   state->guest_a2 = 0;
   state->guest_a3 = 0;
   state->guest_a4 = 0;
   state->guest_a5 = 0;
   state->guest_a6 = 0;
   state->guest_a7 = 0;
   state->guest_a8 = 0;
   state->guest_a9 = 0;
   state->guest_a10 = 0;
   state->guest_a11 = 0;
   state->guest_a12 = 0;
   state->guest_a13 = 0;
   state->guest_a14 = 0;
   state->guest_a15 = 0;


   state->guest_f0 = 0;
   state->guest_f1 = 0;
   state->guest_f2 = 0;
   state->guest_f3 = 0;
   state->guest_f4 = 0;
   state->guest_f5 = 0;
   state->guest_f6 = 0;
   state->guest_f7 = 0;
   state->guest_f8 = 0;
   state->guest_f9 = 0;
   state->guest_f10 = 0;
   state->guest_f11 = 0;
   state->guest_f12 = 0;
   state->guest_f13 = 0;
   state->guest_f14 = 0;
   state->guest_f15 = 0;


   state->guest_r0 = 0;
   state->guest_r1 = 0;
   state->guest_r2 = 0;
   state->guest_r3 = 0;
   state->guest_r4 = 0;
   state->guest_r5 = 0;
   state->guest_r6 = 0;
   state->guest_r7 = 0;
   state->guest_r8 = 0;
   state->guest_r9 = 0;
   state->guest_r10 = 0;
   state->guest_r11 = 0;
   state->guest_r12 = 0;
   state->guest_r13 = 0;
   state->guest_r14 = 0;
   state->guest_r15 = 0;


   state->guest_counter = 0;
   state->guest_fpc = 0;
   state->guest_IA = 0;


   state->guest_SYSNO = 0;


   state->guest_NRADDR = 0;
   state->guest_CMSTART = 0;
   state->guest_CMLEN = 0;
   state->guest_IP_AT_SYSCALL = 0;
   state->guest_EMNOTE = EmNote_NONE;
   state->host_EvC_COUNTER = 0;
   state->host_EvC_FAILADDR = 0;


   state->guest_CC_OP = 0;
   state->guest_CC_DEP1 = 0;
   state->guest_CC_DEP2 = 0;
   state->guest_CC_NDEP = 0;

   __builtin_memset(state->padding, 0x0, sizeof(state->padding));
}


Bool
guest_s390x_state_requires_precise_mem_exns (
   Int minoff, Int maxoff, VexRegisterUpdates pxControl
)
{
   Int lr_min = S390X_GUEST_OFFSET(guest_LR);
   Int lr_max = lr_min + 8 - 1;
   Int sp_min = S390X_GUEST_OFFSET(guest_SP);
   Int sp_max = sp_min + 8 - 1;
   Int fp_min = S390X_GUEST_OFFSET(guest_FP);
   Int fp_max = fp_min + 8 - 1;
   Int ia_min = S390X_GUEST_OFFSET(guest_IA);
   Int ia_max = ia_min + 8 - 1;

   if (maxoff < sp_min || minoff > sp_max) {
      
      if (pxControl == VexRegUpdSpAtMemAccess)
         return False; 
   } else {
      return True;
   }

   if (maxoff < lr_min || minoff > lr_max) {
      
   } else {
      return True;
   }

   if (maxoff < fp_min || minoff > fp_max) {
      
   } else {
      return True;
   }

   if (maxoff < ia_min || minoff > ia_max) {
      
   } else {
      return True;
   }

   return False;
}


#define ALWAYSDEFD(field)                             \
    { S390X_GUEST_OFFSET(field),            \
      (sizeof ((VexGuestS390XState*)0)->field) }

VexGuestLayout s390xGuest_layout = {

   
   .total_sizeB = sizeof(VexGuestS390XState),

   
   .offset_SP = S390X_GUEST_OFFSET(guest_SP),
   .sizeof_SP = 8,

   
   .offset_FP = S390X_GUEST_OFFSET(guest_FP),
   .sizeof_FP = 8,

   
   .offset_IP = S390X_GUEST_OFFSET(guest_IA),
   .sizeof_IP = 8,

   .n_alwaysDefd = 9,

   .alwaysDefd = {
       ALWAYSDEFD(guest_CC_OP),     
       ALWAYSDEFD(guest_CC_NDEP),   
       ALWAYSDEFD(guest_EMNOTE),    
       ALWAYSDEFD(guest_CMSTART),   
       ALWAYSDEFD(guest_CMLEN),     
       ALWAYSDEFD(guest_IP_AT_SYSCALL), 
       ALWAYSDEFD(guest_IA),        
       ALWAYSDEFD(guest_fpc),       
       ALWAYSDEFD(guest_counter),   
   }
};

void
s390x_dirtyhelper_EX(ULong torun)
{
   last_execute_target = torun;
}


#if defined(VGA_s390x)
ULong
s390x_dirtyhelper_STCK(ULong *addr)
{
   UInt cc;

   asm volatile("stck %0\n"
                "ipm %1\n"
                "srl %1,28\n"
                : "+Q" (*addr), "=d" (cc) : : "cc");
   return cc;
}

ULong
s390x_dirtyhelper_STCKE(ULong *addr)
{
   UInt cc;

   asm volatile("stcke %0\n"
                "ipm %1\n"
                "srl %1,28\n"
                : "+Q" (*addr), "=d" (cc) : : "cc");
   return cc;
}

ULong s390x_dirtyhelper_STCKF(ULong *addr)
{
   UInt cc;

   asm volatile(".insn s,0xb27c0000,%0\n"
                "ipm %1\n"
                "srl %1,28\n"
                : "+Q" (*addr), "=d" (cc) : : "cc");
   return cc;
}
#else
ULong s390x_dirtyhelper_STCK(ULong *addr)  {return 3;}
ULong s390x_dirtyhelper_STCKF(ULong *addr) {return 3;}
ULong s390x_dirtyhelper_STCKE(ULong *addr) {return 3;}
#endif 

#if defined(VGA_s390x)
static void
s390_set_facility_bit(ULong *addr, UInt bitno, UInt value)
{
   addr  += bitno / 64;
   bitno  = bitno % 64;

   ULong mask = 1;
   mask <<= (63 - bitno);

   if (value == 1) {
      *addr |= mask;   
   } else {
      *addr &= ~mask;  
   }
}

ULong
s390x_dirtyhelper_STFLE(VexGuestS390XState *guest_state, ULong *addr)
{
   ULong hoststfle[S390_NUM_FACILITY_DW], cc, num_dw, i;
   register ULong reg0 asm("0") = guest_state->guest_r0 & 0xF;  

   if (reg0 > S390_NUM_FACILITY_DW - 1)
      reg0 = S390_NUM_FACILITY_DW - 1;

   num_dw = reg0 + 1;  /* number of double words written */

   asm volatile(" .insn s,0xb2b00000,%0\n"   
                "ipm    %2\n"
                "srl    %2,28\n"
                : "=m" (hoststfle), "+d"(reg0), "=d"(cc) : : "cc", "memory");

   
   guest_state->guest_r0 = reg0;

   
   for (i = 0; i < num_dw; ++i)
      addr[i] = hoststfle[i];

   
   s390_set_facility_bit(addr, S390_FAC_LDISP,  1);
   s390_set_facility_bit(addr, S390_FAC_EIMM,   1);
   s390_set_facility_bit(addr, S390_FAC_ETF2,   1);
   s390_set_facility_bit(addr, S390_FAC_ETF3,   1);
   s390_set_facility_bit(addr, S390_FAC_GIE,    1);
   s390_set_facility_bit(addr, S390_FAC_EXEXT,  1);
   s390_set_facility_bit(addr, S390_FAC_HIGHW,  1);

   s390_set_facility_bit(addr, S390_FAC_HFPMAS, 0);
   s390_set_facility_bit(addr, S390_FAC_HFPUNX, 0);
   s390_set_facility_bit(addr, S390_FAC_XCPUT,  0);
   s390_set_facility_bit(addr, S390_FAC_MSA,    0);
   s390_set_facility_bit(addr, S390_FAC_PENH,   0);
   s390_set_facility_bit(addr, S390_FAC_DFP,    0);
   s390_set_facility_bit(addr, S390_FAC_PFPO,   0);
   s390_set_facility_bit(addr, S390_FAC_DFPZC,  0);
   s390_set_facility_bit(addr, S390_FAC_MISC,   0);
   s390_set_facility_bit(addr, S390_FAC_CTREXE, 0);
   s390_set_facility_bit(addr, S390_FAC_TREXE,  0);
   s390_set_facility_bit(addr, S390_FAC_MSA4,   0);

   return cc;
}

#else

ULong
s390x_dirtyhelper_STFLE(VexGuestS390XState *guest_state, ULong *addr)
{
   return 3;
}
#endif 

void
s390x_dirtyhelper_CUxy(UChar *address, ULong data, ULong num_bytes)
{
   UInt i;

   vassert(num_bytes >= 1 && num_bytes <= 4);

   for (i = 1; i <= num_bytes; ++i) {
      address[num_bytes - i] = data & 0xff;
      data >>= 8;
   }
}



ULong
s390_do_cu21(UInt srcval, UInt low_surrogate)
{
   ULong retval = 0;   
   UInt b1, b2, b3, b4, num_bytes, invalid_low_surrogate = 0;

   srcval &= 0xffff;

   
   if (srcval <= 0x007f)
      num_bytes = 1;
   else if (srcval >= 0x0080 && srcval <= 0x07ff)
      num_bytes = 2;
   else if ((srcval >= 0x0800 && srcval <= 0xd7ff) ||
            (srcval >= 0xdc00 && srcval <= 0xffff))
      num_bytes = 3;
   else
      num_bytes = 4;

   
   switch (num_bytes){
   case 1:
      retval = srcval;
      break;

   case 2:
      
      b1  = 0xc0;
      b1 |= srcval >> 6;

      b2  = 0x80;
      b2 |= srcval & 0x3f;

      retval = (b1 << 8) | b2;
      break;

   case 3:
      
      b1  = 0xe0;
      b1 |= srcval >> 12;

      b2  = 0x80;
      b2 |= (srcval >> 6) & 0x3f;

      b3  = 0x80;
      b3 |= srcval & 0x3f;

      retval = (b1 << 16) | (b2 << 8) | b3;
      break;

   case 4: {
      
      UInt high_surrogate = srcval;
      UInt uvwxy = ((high_surrogate >> 6) & 0xf) + 1;   

      b1  = 0xf0;
      b1 |= uvwxy >> 2;     

      b2  = 0x80;
      b2 |= (uvwxy & 0x3) << 4;           
      b2 |= (high_surrogate >> 2) & 0xf;  

      b3  = 0x80;
      b3 |= (high_surrogate & 0x3) << 4;   
      b3 |= (low_surrogate >> 6) & 0xf;    

      b4  = 0x80;
      b4 |= low_surrogate & 0x3f;

      retval = (b1 << 24) | (b2 << 16) | (b3 << 8) | b4;

      invalid_low_surrogate = (low_surrogate & 0xfc00) != 0xdc00;
      break;
   }
   }

   return (retval << 16) | (num_bytes << 8) | invalid_low_surrogate;
}



ULong
s390_do_cu24(UInt srcval, UInt low_surrogate)
{
   ULong retval;
   UInt invalid_low_surrogate = 0;

   srcval &= 0xffff;

   if ((srcval >= 0x0000 && srcval <= 0xd7ff) ||
       (srcval >= 0xdc00 && srcval <= 0xffff)) {
      retval = srcval;
   } else {
      
      UInt high_surrogate = srcval;
      UInt uvwxy  = ((high_surrogate >> 6) & 0xf) + 1;   
      UInt efghij = high_surrogate & 0x3f;
      UInt klmnoprst = low_surrogate & 0x3ff;

      retval = (uvwxy << 16) | (efghij << 10) | klmnoprst;

      invalid_low_surrogate = (low_surrogate & 0xfc00) != 0xdc00;
   }

   return (retval << 8) | invalid_low_surrogate;
}



ULong
s390_do_cu42(UInt srcval)
{
   ULong retval;
   UInt num_bytes, invalid_character = 0;

   if ((srcval >= 0x0000 && srcval <= 0xd7ff) ||
       (srcval >= 0xdc00 && srcval <= 0xffff)) {
      retval = srcval;
      num_bytes = 2;
   } else if (srcval >= 0x00010000 && srcval <= 0x0010FFFF) {
      UInt uvwxy  = srcval >> 16;
      UInt abcd   = (uvwxy - 1) & 0xf;
      UInt efghij = (srcval >> 10) & 0x3f;

      UInt high_surrogate = (0xd8 << 8) | (abcd << 6) | efghij;
      UInt low_surrogate  = (0xdc << 8) | (srcval & 0x3ff);

      retval = (high_surrogate << 16) | low_surrogate;
      num_bytes = 4;
   } else {
      
      invalid_character = 1;
      retval = num_bytes = 0;   
   }

   return (retval << 16) | (num_bytes << 8) | invalid_character;
}



ULong
s390_do_cu41(UInt srcval)
{
   ULong retval;
   UInt num_bytes, invalid_character = 0;

   if (srcval <= 0x7f) {
      retval = srcval;
      num_bytes = 1;
   } else if (srcval >= 0x80 && srcval <= 0x7ff) {
      UInt fghij  = srcval >> 6;
      UInt klmnop = srcval & 0x3f;
      UInt byte1  = (0xc0 | fghij);
      UInt byte2  = (0x80 | klmnop);

      retval = (byte1 << 8) | byte2;
      num_bytes = 2;
   } else if ((srcval >= 0x800  && srcval <= 0xd7ff) ||
              (srcval >= 0xdc00 && srcval <= 0xffff)) {
      UInt abcd   = srcval >> 12;
      UInt efghij = (srcval >> 6) & 0x3f;
      UInt klmnop = srcval & 0x3f;
      UInt byte1  = 0xe0 | abcd;
      UInt byte2  = 0x80 | efghij;
      UInt byte3  = 0x80 | klmnop;

      retval = (byte1 << 16) | (byte2 << 8) | byte3;
      num_bytes = 3;
   } else if (srcval >= 0x10000 && srcval <= 0x10ffff) {
      UInt uvw    = (srcval >> 18) & 0x7;
      UInt xy     = (srcval >> 16) & 0x3;
      UInt efgh   = (srcval >> 12) & 0xf;
      UInt ijklmn = (srcval >>  6) & 0x3f;
      UInt opqrst = srcval & 0x3f;
      UInt byte1  = 0xf0 | uvw;
      UInt byte2  = 0x80 | (xy << 4) | efgh;
      UInt byte3  = 0x80 | ijklmn;
      UInt byte4  = 0x80 | opqrst;

      retval = (byte1 << 24) | (byte2 << 16) | (byte3 << 8) | byte4;
      num_bytes = 4;
   } else {
      
      invalid_character = 1;

      retval = 0;
      num_bytes = 0;
   }

   return (retval << 16) | (num_bytes << 8) | invalid_character;
}



ULong
s390_do_cu12_cu14_helper1(UInt byte, UInt etf3_and_m3_is_1)
{
   vassert(byte <= 0xff);

   
   if (byte >= 0x80 && byte <= 0xbf) return 1;
   if (byte >= 0xf8) return 1;

   if (etf3_and_m3_is_1) {
      if (byte == 0xc0 || byte == 0xc1) return 1;
      if (byte >= 0xf5 && byte <= 0xf7) return 1;
   }

   
   if (byte <= 0x7f) return 1 << 8;   
   if (byte <= 0xdf) return 2 << 8;   
   if (byte <= 0xef) return 3 << 8;   

   return 4 << 8;  
}

static ULong
s390_do_cu12_cu14_helper2(UInt byte1, UInt byte2, UInt byte3, UInt byte4,
                          ULong stuff, Bool is_cu12)
{
   UInt num_src_bytes = stuff >> 1, etf3_and_m3_is_1 = stuff & 0x1;
   UInt num_bytes = 0, invalid_character = 0;
   ULong retval = 0;

   vassert(num_src_bytes <= 4);

   switch (num_src_bytes) {
   case 1:
      num_bytes = 2;
      retval = byte1;
      break;

   case 2: {
      
      if (etf3_and_m3_is_1) {
         if (byte2 < 0x80 || byte2 > 0xbf) {
            invalid_character = 1;
            break;
         }
      }

      
      UInt fghij  = byte1 & 0x1f;
      UInt klmnop = byte2 & 0x3f;

      num_bytes = 2;
      retval = (fghij << 6) | klmnop;
      break;
   }

   case 3: {
      
      if (etf3_and_m3_is_1) {
         if (byte1 == 0xe0) {
            if ((byte2 < 0xa0 || byte2 > 0xbf) ||
                (byte3 < 0x80 || byte3 > 0xbf)) {
               invalid_character = 1;
               break;
            }
         }
         if ((byte1 >= 0xe1 && byte1 <= 0xec) ||
             byte1 == 0xee || byte1 == 0xef) {
            if ((byte2 < 0x80 || byte2 > 0xbf) ||
                (byte3 < 0x80 || byte3 > 0xbf)) {
               invalid_character = 1;
               break;
            }
         }
         if (byte1 == 0xed) {
            if ((byte2 < 0x80 || byte2 > 0x9f) ||
                (byte3 < 0x80 || byte3 > 0xbf)) {
               invalid_character = 1;
               break;
            }
         }
      }

      
      UInt abcd   = byte1 & 0xf;
      UInt efghij = byte2 & 0x3f;
      UInt klmnop = byte3 & 0x3f;

      num_bytes = 2;
      retval = (abcd << 12) | (efghij << 6) | klmnop;
      break;
   }

   case 4: {
      
      if (etf3_and_m3_is_1) {
         if (byte1 == 0xf0) {
            if ((byte2 < 0x90 || byte2 > 0xbf) ||
                (byte3 < 0x80 || byte3 > 0xbf) ||
                (byte4 < 0x80 || byte4 > 0xbf)) {
               invalid_character = 1;
               break;
            }
         }
         if (byte1 == 0xf1 || byte1 == 0xf2 || byte1 == 0xf3) {
            if ((byte2 < 0x80 || byte2 > 0xbf) ||
                (byte3 < 0x80 || byte3 > 0xbf) ||
                (byte4 < 0x80 || byte4 > 0xbf)) {
               invalid_character = 1;
               break;
            }
         }
         if (byte1 == 0xf4) {
            if ((byte2 < 0x80 || byte2 > 0x8f) ||
                (byte3 < 0x80 || byte3 > 0xbf) ||
                (byte4 < 0x80 || byte4 > 0xbf)) {
               invalid_character = 1;
               break;
            }
         }
      }

      
      UInt uvw    = byte1 & 0x7;
      UInt xy     = (byte2 >> 4) & 0x3;
      UInt uvwxy  = (uvw << 2) | xy;
      UInt efgh   = byte2 & 0xf;
      UInt ij     = (byte3 >> 4) & 0x3;
      UInt klmn   = byte3 & 0xf;
      UInt opqrst = byte4 & 0x3f;
      
      if (is_cu12) {
         UInt abcd = (uvwxy - 1) & 0xf;
         UInt high_surrogate = (0xd8 << 8) | (abcd << 6) | (efgh << 2) | ij;
         UInt low_surrogate  = (0xdc << 8) | (klmn << 6) | opqrst;

         num_bytes = 4;
         retval = (high_surrogate << 16) | low_surrogate;
      } else {
         num_bytes = 4;
         retval =
            (uvwxy << 16) | (efgh << 12) | (ij << 10) | (klmn << 6) | opqrst;
      }
      break;
   }
   }

   if (! is_cu12) num_bytes = 4;   

   return (retval << 16) | (num_bytes << 8) | invalid_character;
}

ULong
s390_do_cu12_helper2(UInt byte1, UInt byte2, UInt byte3, UInt byte4,
                     ULong stuff)
{
   return s390_do_cu12_cu14_helper2(byte1, byte2, byte3, byte4, stuff,
                                     1);
}

ULong
s390_do_cu14_helper2(UInt byte1, UInt byte2, UInt byte3, UInt byte4,
                     ULong stuff)
{
   return s390_do_cu12_cu14_helper2(byte1, byte2, byte3, byte4, stuff,
                                     0);
}


#if defined(VGA_s390x)
UInt
s390_do_cvb(ULong decimal)
{
   UInt binary;

   __asm__ volatile (
        "cvb %[result],%[input]\n\t"
          : [result] "=d"(binary)
          : [input] "m"(decimal)
   );

   return binary;
}

#else
UInt s390_do_cvb(ULong decimal) { return 0; }
#endif


#if defined(VGA_s390x)
ULong
s390_do_cvd(ULong binary_in)
{
   UInt binary = binary_in & 0xffffffffULL;
   ULong decimal;

   __asm__ volatile (
        "cvd %[input],%[result]\n\t"
          : [result] "=m"(decimal)
          : [input] "d"(binary)
   );

   return decimal;
}

#else
ULong s390_do_cvd(ULong binary) { return 0; }
#endif

#if defined(VGA_s390x)
ULong
s390_do_ecag(ULong op2addr)
{
   ULong result;

   __asm__ volatile(".insn rsy,0xEB000000004C,%[out],0,0(%[in])\n\t"
                    : [out] "=d"(result)
                    : [in] "d"(op2addr));
   return result;
}

#else
ULong s390_do_ecag(ULong op2addr) { return 0; }
#endif

#if defined(VGA_s390x)
UInt
s390_do_pfpo(UInt gpr0)
{
   UChar rm;
   UChar op1_ty, op2_ty;

   rm  = gpr0 & 0xf;
   if (rm > 1 && rm < 8)
      return EmFail_S390X_invalid_PFPO_rounding_mode;

   op1_ty = (gpr0 >> 16) & 0xff; 
   op2_ty = (gpr0 >> 8)  & 0xff; 
   if ((op1_ty == op2_ty) ||
       (op1_ty < 0x5 || op1_ty > 0xa) ||
       (op2_ty < 0x5 || op2_ty > 0xa))
      return EmFail_S390X_invalid_PFPO_function;

   return EmNote_NONE;
}
#else
UInt s390_do_pfpo(UInt gpr0) { return 0; }
#endif


#if defined(VGA_s390x)
static s390_bfp_round_t
decode_bfp_rounding_mode(UInt irrm)
{
   switch (irrm) {
   case Irrm_NEAREST: return S390_BFP_ROUND_NEAREST_EVEN;
   case Irrm_NegINF:  return S390_BFP_ROUND_NEGINF;
   case Irrm_PosINF:  return S390_BFP_ROUND_POSINF;
   case Irrm_ZERO:    return S390_BFP_ROUND_ZERO;
   }
   vpanic("decode_bfp_rounding_mode");
}
#endif


#define S390_CC_FOR_BINARY(opcode,cc_dep1,cc_dep2) \
({ \
   __asm__ volatile ( \
        opcode " %[op1],%[op2]\n\t" \
        "ipm %[psw]\n\t"           : [psw] "=d"(psw), [op1] "+d"(cc_dep1) \
                                   : [op2] "d"(cc_dep2) \
                                   : "cc");\
   psw >> 28;    \
})

#define S390_CC_FOR_TERNARY_SUBB(opcode,cc_dep1,cc_dep2,cc_ndep) \
({ \
 \
   cc_dep2 = cc_dep2 ^ cc_ndep; \
   __asm__ volatile ( \
	"lghi 0,1\n\t" \
	"sr 0,%[op3]\n\t"  \
        opcode " %[op1],%[op2]\n\t" \
        "ipm %[psw]\n\t"           : [psw] "=d"(psw), [op1] "+&d"(cc_dep1) \
                                   : [op2] "d"(cc_dep2), [op3] "d"(cc_ndep) \
                                   : "0", "cc");\
   psw >> 28;    \
})

#define S390_CC_FOR_TERNARY_ADDC(opcode,cc_dep1,cc_dep2,cc_ndep) \
({ \
 \
   cc_dep2 = cc_dep2 ^ cc_ndep; \
   __asm__ volatile ( \
	"lgfr 0,%[op3]\n\t"  \
	"aghi 0,0\n\t"  \
        opcode " %[op1],%[op2]\n\t" \
        "ipm %[psw]\n\t"           : [psw] "=d"(psw), [op1] "+&d"(cc_dep1) \
                                   : [op2] "d"(cc_dep2), [op3] "d"(cc_ndep) \
                                   : "0", "cc");\
   psw >> 28;    \
})


#define S390_CC_FOR_BFP_RESULT(opcode,cc_dep1) \
({ \
   __asm__ volatile ( \
        opcode " 0,%[op]\n\t" \
        "ipm %[psw]\n\t"           : [psw] "=d"(psw) \
                                   : [op]  "f"(cc_dep1) \
                                   : "cc", "f0");\
   psw >> 28;    \
})

#define S390_CC_FOR_BFP128_RESULT(hi,lo) \
({ \
   __asm__ volatile ( \
        "ldr   4,%[high]\n\t" \
        "ldr   6,%[low]\n\t" \
        "ltxbr 0,4\n\t" \
        "ipm %[psw]\n\t"           : [psw] "=d"(psw) \
                                   : [high] "f"(hi), [low] "f"(lo) \
                                   : "cc", "f0", "f2", "f4", "f6");\
   psw >> 28;    \
})

#define S390_CC_FOR_BFP_CONVERT_AUX(opcode,cc_dep1,rounding_mode) \
({ \
   __asm__ volatile ( \
        opcode " 0," #rounding_mode ",%[op]\n\t" \
        "ipm %[psw]\n\t"           : [psw] "=d"(psw) \
                                   : [op]  "f"(cc_dep1) \
                                   : "cc", "r0");\
   psw >> 28;    \
})

#define S390_CC_FOR_BFP_CONVERT(opcode,cc_dep1,cc_dep2)   \
({                                                        \
   UInt cc;                                               \
   switch (decode_bfp_rounding_mode(cc_dep2)) {           \
   case S390_BFP_ROUND_NEAREST_EVEN:                      \
      cc = S390_CC_FOR_BFP_CONVERT_AUX(opcode,cc_dep1,4); \
      break;                                              \
   case S390_BFP_ROUND_ZERO:                              \
      cc = S390_CC_FOR_BFP_CONVERT_AUX(opcode,cc_dep1,5); \
      break;                                              \
   case S390_BFP_ROUND_POSINF:                            \
      cc = S390_CC_FOR_BFP_CONVERT_AUX(opcode,cc_dep1,6); \
      break;                                              \
   case S390_BFP_ROUND_NEGINF:                            \
      cc = S390_CC_FOR_BFP_CONVERT_AUX(opcode,cc_dep1,7); \
      break;                                              \
   default:                                               \
      vpanic("unexpected bfp rounding mode");             \
   }                                                      \
   cc;                                                    \
})

#define S390_CC_FOR_BFP_UCONVERT_AUX(opcode,cc_dep1,rounding_mode) \
({ \
   __asm__ volatile ( \
        opcode ",0,%[op]," #rounding_mode ",0\n\t" \
        "ipm %[psw]\n\t"           : [psw] "=d"(psw) \
                                   : [op]  "f"(cc_dep1) \
                                   : "cc", "r0");\
   psw >> 28;    \
})

#define S390_CC_FOR_BFP_UCONVERT(opcode,cc_dep1,cc_dep2)   \
({                                                         \
   UInt cc;                                                \
   switch (decode_bfp_rounding_mode(cc_dep2)) {            \
   case S390_BFP_ROUND_NEAREST_EVEN:                       \
      cc = S390_CC_FOR_BFP_UCONVERT_AUX(opcode,cc_dep1,4); \
      break;                                               \
   case S390_BFP_ROUND_ZERO:                               \
      cc = S390_CC_FOR_BFP_UCONVERT_AUX(opcode,cc_dep1,5); \
      break;                                               \
   case S390_BFP_ROUND_POSINF:                             \
      cc = S390_CC_FOR_BFP_UCONVERT_AUX(opcode,cc_dep1,6); \
      break;                                               \
   case S390_BFP_ROUND_NEGINF:                             \
      cc = S390_CC_FOR_BFP_UCONVERT_AUX(opcode,cc_dep1,7); \
      break;                                               \
   default:                                                \
      vpanic("unexpected bfp rounding mode");              \
   }                                                       \
   cc;                                                     \
})

#define S390_CC_FOR_BFP128_CONVERT_AUX(opcode,hi,lo,rounding_mode) \
({ \
   __asm__ volatile ( \
        "ldr   4,%[high]\n\t" \
        "ldr   6,%[low]\n\t" \
        opcode " 0," #rounding_mode ",4\n\t" \
        "ipm %[psw]\n\t"           : [psw] "=d"(psw) \
                                   : [high] "f"(hi), [low] "f"(lo) \
                                   : "cc", "r0", "f4", "f6");\
   psw >> 28;    \
})

#define S390_CC_FOR_BFP128_CONVERT(opcode,cc_dep1,cc_dep2,cc_ndep)   \
({                                                                   \
   UInt cc;                                                          \
                           \
   cc_dep2 = cc_dep2 ^ cc_ndep;                                      \
   switch (decode_bfp_rounding_mode(cc_ndep)) {                      \
   case S390_BFP_ROUND_NEAREST_EVEN:                                 \
      cc = S390_CC_FOR_BFP128_CONVERT_AUX(opcode,cc_dep1,cc_dep2,4); \
      break;                                                         \
   case S390_BFP_ROUND_ZERO:                                         \
      cc = S390_CC_FOR_BFP128_CONVERT_AUX(opcode,cc_dep1,cc_dep2,5); \
      break;                                                         \
   case S390_BFP_ROUND_POSINF:                                       \
      cc = S390_CC_FOR_BFP128_CONVERT_AUX(opcode,cc_dep1,cc_dep2,6); \
      break;                                                         \
   case S390_BFP_ROUND_NEGINF:                                       \
      cc = S390_CC_FOR_BFP128_CONVERT_AUX(opcode,cc_dep1,cc_dep2,7); \
      break;                                                         \
   default:                                                          \
      vpanic("unexpected bfp rounding mode");                        \
   }                                                                 \
   cc;                                                               \
})

#define S390_CC_FOR_BFP128_UCONVERT_AUX(opcode,hi,lo,rounding_mode) \
({ \
   __asm__ volatile ( \
        "ldr   4,%[high]\n\t" \
        "ldr   6,%[low]\n\t" \
        opcode ",0,4," #rounding_mode ",0\n\t" \
        "ipm %[psw]\n\t"           : [psw] "=d"(psw) \
                                   : [high] "f"(hi), [low] "f"(lo) \
                                   : "cc", "r0", "f4", "f6");\
   psw >> 28;    \
})

#define S390_CC_FOR_BFP128_UCONVERT(opcode,cc_dep1,cc_dep2,cc_ndep)   \
({                                                                    \
   UInt cc;                                                           \
                            \
   cc_dep2 = cc_dep2 ^ cc_ndep;                                       \
   switch (decode_bfp_rounding_mode(cc_ndep)) {                       \
   case S390_BFP_ROUND_NEAREST_EVEN:                                  \
      cc = S390_CC_FOR_BFP128_UCONVERT_AUX(opcode,cc_dep1,cc_dep2,4); \
      break;                                                          \
   case S390_BFP_ROUND_ZERO:                                          \
      cc = S390_CC_FOR_BFP128_UCONVERT_AUX(opcode,cc_dep1,cc_dep2,5); \
      break;                                                          \
   case S390_BFP_ROUND_POSINF:                                        \
      cc = S390_CC_FOR_BFP128_UCONVERT_AUX(opcode,cc_dep1,cc_dep2,6); \
      break;                                                          \
   case S390_BFP_ROUND_NEGINF:                                        \
      cc = S390_CC_FOR_BFP128_UCONVERT_AUX(opcode,cc_dep1,cc_dep2,7); \
      break;                                                          \
   default:                                                           \
      vpanic("unexpected bfp rounding mode");                         \
   }                                                                  \
   cc;                                                                \
})

#define S390_CC_FOR_BFP_TDC(opcode,cc_dep1,cc_dep2) \
({ \
   __asm__ volatile ( \
        opcode " %[value],0(%[class])\n\t" \
        "ipm %[psw]\n\t"           : [psw] "=d"(psw) \
                                   : [value] "f"(cc_dep1), \
                                     [class] "a"(cc_dep2)  \
                                   : "cc");\
   psw >> 28;    \
})

#define S390_CC_FOR_BFP128_TDC(cc_dep1,cc_dep2,cc_ndep) \
({ \
 \
   cc_dep2 = cc_dep2 ^ cc_ndep; \
   __asm__ volatile ( \
        "ldr  4,%[high]\n\t" \
        "ldr  6,%[low]\n\t" \
        "tcxb 4,0(%[class])\n\t" \
        "ipm  %[psw]\n\t"          : [psw] "=d"(psw) \
                                   : [high] "f"(cc_dep1), [low] "f"(cc_dep2), \
                                     [class] "a"(cc_ndep)  \
                                   : "cc", "f4", "f6");\
   psw >> 28;    \
})

#if defined(VGA_s390x)
static s390_dfp_round_t
decode_dfp_rounding_mode(UInt irrm)
{
   switch (irrm) {
   case Irrm_NEAREST:
      return S390_DFP_ROUND_NEAREST_EVEN_4;
   case Irrm_NegINF:
      return S390_DFP_ROUND_NEGINF_7;
   case Irrm_PosINF:
      return S390_DFP_ROUND_POSINF_6;
   case Irrm_ZERO:
      return S390_DFP_ROUND_ZERO_5;
   case Irrm_NEAREST_TIE_AWAY_0:
      return S390_DFP_ROUND_NEAREST_TIE_AWAY_0_1;
   case Irrm_PREPARE_SHORTER:
      return S390_DFP_ROUND_PREPARE_SHORT_3;
   case Irrm_AWAY_FROM_ZERO:
      return S390_DFP_ROUND_AWAY_0;
   case Irrm_NEAREST_TIE_TOWARD_0:
      return S390_DFP_ROUND_NEAREST_TIE_TOWARD_0;
   }
   vpanic("decode_dfp_rounding_mode");
}
#endif

#define S390_CC_FOR_DFP_RESULT(cc_dep1) \
({ \
   __asm__ volatile ( \
        ".insn rre, 0xb3d60000,0,%[op]\n\t"               \
        "ipm %[psw]\n\t"           : [psw] "=d"(psw) \
                                   : [op]  "f"(cc_dep1) \
                                   : "cc", "f0"); \
   psw >> 28;    \
})

#define S390_CC_FOR_DFP128_RESULT(hi,lo) \
({ \
   __asm__ volatile ( \
        "ldr   4,%[high]\n\t"                                           \
        "ldr   6,%[low]\n\t"                                            \
        ".insn rre, 0xb3de0000,0,4\n\t"                      \
        "ipm %[psw]\n\t"           : [psw] "=d"(psw)                    \
                                   : [high] "f"(hi), [low] "f"(lo)      \
                                   : "cc", "f0", "f2", "f4", "f6");     \
   psw >> 28;                                                   \
})

#define S390_CC_FOR_DFP_TD(opcode,cc_dep1,cc_dep2)                      \
({                                                                      \
   __asm__ volatile (                                                   \
        opcode ",%[value],0(%[class])\n\t"                              \
        "ipm %[psw]\n\t"           : [psw] "=d"(psw)                    \
                                   : [value] "f"(cc_dep1),              \
                                     [class] "a"(cc_dep2)               \
                                   : "cc");                             \
   psw >> 28;                                                   \
})

#define S390_CC_FOR_DFP128_TD(opcode,cc_dep1,cc_dep2,cc_ndep)           \
({                                                                      \
                         \
   cc_dep2 = cc_dep2 ^ cc_ndep;                                         \
   __asm__ volatile (                                                   \
        "ldr  4,%[high]\n\t"                                            \
        "ldr  6,%[low]\n\t"                                             \
        opcode ",4,0(%[class])\n\t"                                     \
        "ipm  %[psw]\n\t"          : [psw] "=d"(psw)                    \
                                   : [high] "f"(cc_dep1), [low] "f"(cc_dep2), \
                                     [class] "a"(cc_ndep)               \
                                   : "cc", "f4", "f6");                 \
   psw >> 28;                                                   \
})

#define S390_CC_FOR_DFP_CONVERT_AUX(opcode,cc_dep1,rounding_mode)       \
   ({                                                                   \
      __asm__ volatile (                                                \
                        opcode ",0,%[op]," #rounding_mode ",0\n\t"      \
                        "ipm %[psw]\n\t"           : [psw] "=d"(psw)    \
                        : [op] "f"(cc_dep1)                             \
                        : "cc", "r0");                                  \
      psw >> 28;                                                \
   })

#define S390_CC_FOR_DFP_CONVERT(opcode,cc_dep1,cc_dep2)                 \
   ({                                                                   \
      UInt cc;                                                          \
      switch (decode_dfp_rounding_mode(cc_dep2)) {                      \
      case S390_DFP_ROUND_NEAREST_TIE_AWAY_0_1:                         \
      case S390_DFP_ROUND_NEAREST_TIE_AWAY_0_12:                        \
         cc = S390_CC_FOR_DFP_CONVERT_AUX(opcode,cc_dep1,1);            \
         break;                                                         \
      case S390_DFP_ROUND_PREPARE_SHORT_3:                              \
      case S390_DFP_ROUND_PREPARE_SHORT_15:                             \
         cc = S390_CC_FOR_DFP_CONVERT_AUX(opcode,cc_dep1,3);            \
         break;                                                         \
      case S390_DFP_ROUND_NEAREST_EVEN_4:                               \
      case S390_DFP_ROUND_NEAREST_EVEN_8:                               \
         cc = S390_CC_FOR_DFP_CONVERT_AUX(opcode,cc_dep1,4);            \
         break;                                                         \
      case S390_DFP_ROUND_ZERO_5:                                       \
      case S390_DFP_ROUND_ZERO_9:                                       \
         cc = S390_CC_FOR_DFP_CONVERT_AUX(opcode,cc_dep1,5);            \
         break;                                                         \
      case S390_DFP_ROUND_POSINF_6:                                     \
      case S390_DFP_ROUND_POSINF_10:                                    \
         cc = S390_CC_FOR_DFP_CONVERT_AUX(opcode,cc_dep1,6);            \
         break;                                                         \
      case S390_DFP_ROUND_NEGINF_7:                                     \
      case S390_DFP_ROUND_NEGINF_11:                                    \
         cc = S390_CC_FOR_DFP_CONVERT_AUX(opcode,cc_dep1,7);            \
         break;                                                         \
      case S390_DFP_ROUND_NEAREST_TIE_TOWARD_0:                         \
         cc = S390_CC_FOR_DFP_CONVERT_AUX(opcode,cc_dep1,13);           \
         break;                                                         \
      case S390_DFP_ROUND_AWAY_0:                                       \
         cc = S390_CC_FOR_DFP_CONVERT_AUX(opcode,cc_dep1,14);           \
         break;                                                         \
      default:                                                          \
         vpanic("unexpected dfp rounding mode");                        \
      }                                                                 \
      cc;                                                               \
   })

#define S390_CC_FOR_DFP_UCONVERT_AUX(opcode,cc_dep1,rounding_mode)      \
   ({                                                                   \
      __asm__ volatile (                                                \
                        opcode ",0,%[op]," #rounding_mode ",0\n\t"      \
                        "ipm %[psw]\n\t"           : [psw] "=d"(psw)    \
                        : [op] "f"(cc_dep1)                             \
                        : "cc", "r0");                                  \
      psw >> 28;                                                \
   })

#define S390_CC_FOR_DFP_UCONVERT(opcode,cc_dep1,cc_dep2)                \
   ({                                                                   \
      UInt cc;                                                          \
      switch (decode_dfp_rounding_mode(cc_dep2)) {                      \
      case S390_DFP_ROUND_NEAREST_TIE_AWAY_0_1:                         \
      case S390_DFP_ROUND_NEAREST_TIE_AWAY_0_12:                        \
         cc = S390_CC_FOR_DFP_UCONVERT_AUX(opcode,cc_dep1,1);           \
         break;                                                         \
      case S390_DFP_ROUND_PREPARE_SHORT_3:                              \
      case S390_DFP_ROUND_PREPARE_SHORT_15:                             \
         cc = S390_CC_FOR_DFP_UCONVERT_AUX(opcode,cc_dep1,3);           \
         break;                                                         \
      case S390_DFP_ROUND_NEAREST_EVEN_4:                               \
      case S390_DFP_ROUND_NEAREST_EVEN_8:                               \
         cc = S390_CC_FOR_DFP_UCONVERT_AUX(opcode,cc_dep1,4);           \
         break;                                                         \
      case S390_DFP_ROUND_ZERO_5:                                       \
      case S390_DFP_ROUND_ZERO_9:                                       \
         cc = S390_CC_FOR_DFP_UCONVERT_AUX(opcode,cc_dep1,5);           \
         break;                                                         \
      case S390_DFP_ROUND_POSINF_6:                                     \
      case S390_DFP_ROUND_POSINF_10:                                    \
         cc = S390_CC_FOR_DFP_UCONVERT_AUX(opcode,cc_dep1,6);           \
         break;                                                         \
      case S390_DFP_ROUND_NEGINF_7:                                     \
      case S390_DFP_ROUND_NEGINF_11:                                    \
         cc = S390_CC_FOR_DFP_UCONVERT_AUX(opcode,cc_dep1,7);           \
         break;                                                         \
      case S390_DFP_ROUND_NEAREST_TIE_TOWARD_0:                         \
         cc = S390_CC_FOR_DFP_UCONVERT_AUX(opcode,cc_dep1,13);          \
         break;                                                         \
      case S390_DFP_ROUND_AWAY_0:                                       \
         cc = S390_CC_FOR_DFP_UCONVERT_AUX(opcode,cc_dep1,14);          \
         break;                                                         \
      default:                                                          \
         vpanic("unexpected dfp rounding mode");                        \
      }                                                                 \
      cc;                                                               \
   })

#define S390_CC_FOR_DFP128_CONVERT_AUX(opcode,hi,lo,rounding_mode)      \
   ({                                                                   \
      __asm__ volatile (                                                \
                        "ldr   4,%[high]\n\t"                           \
                        "ldr   6,%[low]\n\t"                            \
                        opcode ",0,4," #rounding_mode ",0\n\t"          \
                        "ipm %[psw]\n\t"           : [psw] "=d"(psw)    \
                        : [high] "f"(hi), [low] "f"(lo)                 \
                        : "cc", "r0", "f4", "f6");                      \
      psw >> 28;                                                \
   })

#define S390_CC_FOR_DFP128_CONVERT(opcode,cc_dep1,cc_dep2,cc_ndep)       \
   ({                                                                    \
      UInt cc;                                                           \
                            \
      cc_dep2 = cc_dep2 ^ cc_ndep;                                       \
      switch (decode_dfp_rounding_mode(cc_ndep)) {                       \
      case S390_DFP_ROUND_NEAREST_TIE_AWAY_0_1:                          \
      case S390_DFP_ROUND_NEAREST_TIE_AWAY_0_12:                         \
         cc = S390_CC_FOR_DFP128_CONVERT_AUX(opcode,cc_dep1,cc_dep2,1);  \
         break;                                                          \
      case S390_DFP_ROUND_PREPARE_SHORT_3:                               \
      case S390_DFP_ROUND_PREPARE_SHORT_15:                              \
         cc = S390_CC_FOR_DFP128_CONVERT_AUX(opcode,cc_dep1,cc_dep2,3);  \
         break;                                                          \
      case S390_DFP_ROUND_NEAREST_EVEN_4:                                \
      case S390_DFP_ROUND_NEAREST_EVEN_8:                                \
         cc = S390_CC_FOR_DFP128_CONVERT_AUX(opcode,cc_dep1,cc_dep2,4);  \
         break;                                                          \
      case S390_DFP_ROUND_ZERO_5:                                        \
      case S390_DFP_ROUND_ZERO_9:                                        \
         cc = S390_CC_FOR_DFP128_CONVERT_AUX(opcode,cc_dep1,cc_dep2,5);  \
         break;                                                          \
      case S390_DFP_ROUND_POSINF_6:                                      \
      case S390_DFP_ROUND_POSINF_10:                                     \
         cc = S390_CC_FOR_DFP128_CONVERT_AUX(opcode,cc_dep1,cc_dep2,6);  \
         break;                                                          \
      case S390_DFP_ROUND_NEGINF_7:                                      \
      case S390_DFP_ROUND_NEGINF_11:                                     \
         cc = S390_CC_FOR_DFP128_CONVERT_AUX(opcode,cc_dep1,cc_dep2,7);  \
         break;                                                          \
      case S390_DFP_ROUND_NEAREST_TIE_TOWARD_0:                          \
         cc = S390_CC_FOR_DFP128_CONVERT_AUX(opcode,cc_dep1,cc_dep2,13); \
         break;                                                          \
      case S390_DFP_ROUND_AWAY_0:                                        \
         cc = S390_CC_FOR_DFP128_CONVERT_AUX(opcode,cc_dep1,cc_dep2,14); \
         break;                                                          \
      default:                                                           \
         vpanic("unexpected dfp rounding mode");                         \
      }                                                                  \
      cc;                                                                \
   })

#define S390_CC_FOR_DFP128_UCONVERT_AUX(opcode,hi,lo,rounding_mode)      \
   ({                                                                    \
      __asm__ volatile (                                                 \
                        "ldr   4,%[high]\n\t"                            \
                        "ldr   6,%[low]\n\t"                             \
                        opcode ",0,4," #rounding_mode ",0\n\t"           \
                        "ipm %[psw]\n\t"           : [psw] "=d"(psw)     \
                        : [high] "f"(hi), [low] "f"(lo)                  \
                        : "cc", "r0", "f4", "f6");                       \
      psw >> 28;                                                 \
   })

#define S390_CC_FOR_DFP128_UCONVERT(opcode,cc_dep1,cc_dep2,cc_ndep)       \
   ({                                                                     \
      UInt cc;                                                            \
                             \
      cc_dep2 = cc_dep2 ^ cc_ndep;                                        \
      switch (decode_dfp_rounding_mode(cc_ndep)) {                        \
      case S390_DFP_ROUND_NEAREST_TIE_AWAY_0_1:                           \
      case S390_DFP_ROUND_NEAREST_TIE_AWAY_0_12:                          \
         cc = S390_CC_FOR_DFP128_UCONVERT_AUX(opcode,cc_dep1,cc_dep2,1);  \
         break;                                                           \
      case S390_DFP_ROUND_PREPARE_SHORT_3:                                \
      case S390_DFP_ROUND_PREPARE_SHORT_15:                               \
         cc = S390_CC_FOR_DFP128_UCONVERT_AUX(opcode,cc_dep1,cc_dep2,3);  \
         break;                                                           \
      case S390_DFP_ROUND_NEAREST_EVEN_4:                                 \
      case S390_DFP_ROUND_NEAREST_EVEN_8:                                 \
         cc = S390_CC_FOR_DFP128_UCONVERT_AUX(opcode,cc_dep1,cc_dep2,4);  \
         break;                                                           \
      case S390_DFP_ROUND_ZERO_5:                                         \
      case S390_DFP_ROUND_ZERO_9:                                         \
         cc = S390_CC_FOR_DFP128_UCONVERT_AUX(opcode,cc_dep1,cc_dep2,5);  \
         break;                                                           \
      case S390_DFP_ROUND_POSINF_6:                                       \
      case S390_DFP_ROUND_POSINF_10:                                      \
         cc = S390_CC_FOR_DFP128_UCONVERT_AUX(opcode,cc_dep1,cc_dep2,6);  \
         break;                                                           \
      case S390_DFP_ROUND_NEGINF_7:                                       \
      case S390_DFP_ROUND_NEGINF_11:                                      \
         cc = S390_CC_FOR_DFP128_UCONVERT_AUX(opcode,cc_dep1,cc_dep2,7);  \
         break;                                                           \
      case S390_DFP_ROUND_NEAREST_TIE_TOWARD_0:                           \
         cc = S390_CC_FOR_DFP128_UCONVERT_AUX(opcode,cc_dep1,cc_dep2,13); \
         break;                                                           \
      case S390_DFP_ROUND_AWAY_0:                                         \
         cc = S390_CC_FOR_DFP128_UCONVERT_AUX(opcode,cc_dep1,cc_dep2,14); \
         break;                                                           \
      default:                                                            \
         vpanic("unexpected dfp rounding mode");                          \
      }                                                                   \
      cc;                                                                 \
   })


UInt
s390_calculate_cc(ULong cc_op, ULong cc_dep1, ULong cc_dep2, ULong cc_ndep)
{
#if defined(VGA_s390x)
   UInt psw;

   switch (cc_op) {

   case S390_CC_OP_BITWISE:
      return S390_CC_FOR_BINARY("ogr", cc_dep1, (ULong)0);

   case S390_CC_OP_SIGNED_COMPARE:
      return S390_CC_FOR_BINARY("cgr", cc_dep1, cc_dep2);

   case S390_CC_OP_UNSIGNED_COMPARE:
      return S390_CC_FOR_BINARY("clgr", cc_dep1, cc_dep2);

   case S390_CC_OP_SIGNED_ADD_64:
      return S390_CC_FOR_BINARY("agr", cc_dep1, cc_dep2);

   case S390_CC_OP_SIGNED_ADD_32:
      return S390_CC_FOR_BINARY("ar", cc_dep1, cc_dep2);

   case S390_CC_OP_SIGNED_SUB_64:
      return S390_CC_FOR_BINARY("sgr", cc_dep1, cc_dep2);

   case S390_CC_OP_SIGNED_SUB_32:
      return S390_CC_FOR_BINARY("sr", cc_dep1, cc_dep2);

   case S390_CC_OP_UNSIGNED_ADD_64:
      return S390_CC_FOR_BINARY("algr", cc_dep1, cc_dep2);

   case S390_CC_OP_UNSIGNED_ADD_32:
      return S390_CC_FOR_BINARY("alr", cc_dep1, cc_dep2);

   case S390_CC_OP_UNSIGNED_ADDC_64:
      return S390_CC_FOR_TERNARY_ADDC("alcgr", cc_dep1, cc_dep2, cc_ndep);

   case S390_CC_OP_UNSIGNED_ADDC_32:
      return S390_CC_FOR_TERNARY_ADDC("alcr", cc_dep1, cc_dep2, cc_ndep);

   case S390_CC_OP_UNSIGNED_SUB_64:
      return S390_CC_FOR_BINARY("slgr", cc_dep1, cc_dep2);

   case S390_CC_OP_UNSIGNED_SUB_32:
      return S390_CC_FOR_BINARY("slr", cc_dep1, cc_dep2);

   case S390_CC_OP_UNSIGNED_SUBB_64:
      return S390_CC_FOR_TERNARY_SUBB("slbgr", cc_dep1, cc_dep2, cc_ndep);

   case S390_CC_OP_UNSIGNED_SUBB_32:
      return S390_CC_FOR_TERNARY_SUBB("slbr", cc_dep1, cc_dep2, cc_ndep);

   case S390_CC_OP_LOAD_AND_TEST:
      
      return S390_CC_FOR_BINARY("cgr", cc_dep1, (Long)0);

   case S390_CC_OP_LOAD_POSITIVE_32:
      __asm__ volatile (
           "lpr  %[result],%[op]\n\t"
           "ipm  %[psw]\n\t"         : [psw] "=d"(psw), [result] "=d"(cc_dep1)
                                     : [op] "d"(cc_dep1)
                                     : "cc");
      return psw >> 28;   

   case S390_CC_OP_LOAD_POSITIVE_64:
      __asm__ volatile (
           "lpgr %[result],%[op]\n\t"
           "ipm  %[psw]\n\t"         : [psw] "=d"(psw), [result] "=d"(cc_dep1)
                                     : [op] "d"(cc_dep1)
                                     : "cc");
      return psw >> 28;   

   case S390_CC_OP_TEST_UNDER_MASK_8: {
      UChar value  = cc_dep1;
      UChar mask   = cc_dep2;

      __asm__ volatile (
           "bras %%r2,1f\n\t"             
           "tm %[value],0\n\t"            
           "1: ex %[mask],0(%%r2)\n\t"    
           "ipm %[psw]\n\t"             : [psw] "=d"(psw)
                                        : [value] "m"(value), [mask] "a"(mask)
                                        : "r2", "cc");
      return psw >> 28;   
   }

   case S390_CC_OP_TEST_UNDER_MASK_16: {
      
      UInt insn  = (0xA701u << 16) | cc_dep2;
      UInt value = cc_dep1;

      __asm__ volatile (
           "lr   1,%[value]\n\t"
           "lhi  2,0x10\n\t"
           "ex   2,%[insn]\n\t"
           "ipm  %[psw]\n\t"       : [psw] "=d"(psw)
                                   : [value] "d"(value), [insn] "m"(insn)
                                   : "r1", "r2", "cc");
      return psw >> 28;   
   }

   case S390_CC_OP_SHIFT_LEFT_32:
      __asm__ volatile (
           "sla  %[op],0(%[amount])\n\t"
           "ipm  %[psw]\n\t"            : [psw] "=d"(psw), [op] "+d"(cc_dep1)
                                        : [amount] "a"(cc_dep2)
                                        : "cc");
      return psw >> 28;   

   case S390_CC_OP_SHIFT_LEFT_64: {
      Int high = (Int)(cc_dep1 >> 32);
      Int low  = (Int)(cc_dep1 & 0xFFFFFFFF);

      __asm__ volatile (
           "lr   2,%[high]\n\t"
           "lr   3,%[low]\n\t"
           "slda 2,0(%[amount])\n\t"
           "ipm %[psw]\n\t"             : [psw] "=d"(psw), [high] "+d"(high),
                                          [low] "+d"(low)
                                        : [amount] "a"(cc_dep2)
                                        : "cc", "r2", "r3");
      return psw >> 28;   
   }

   case S390_CC_OP_INSERT_CHAR_MASK_32: {
      Int inserted = 0;
      Int msb = 0;

      if (cc_dep2 & 1) {
         inserted |= cc_dep1 & 0xff;
         msb = 0x80;
      }
      if (cc_dep2 & 2) {
         inserted |= cc_dep1 & 0xff00;
         msb = 0x8000;
      }
      if (cc_dep2 & 4) {
         inserted |= cc_dep1 & 0xff0000;
         msb = 0x800000;
      }
      if (cc_dep2 & 8) {
         inserted |= cc_dep1 & 0xff000000;
         msb = 0x80000000;
      }

      if (inserted & msb)  
         return 1;
      if (inserted > 0)
         return 2;
      return 0;
   }

   case S390_CC_OP_BFP_RESULT_32:
      return S390_CC_FOR_BFP_RESULT("ltebr", cc_dep1);

   case S390_CC_OP_BFP_RESULT_64:
      return S390_CC_FOR_BFP_RESULT("ltdbr", cc_dep1);

   case S390_CC_OP_BFP_RESULT_128:
      return S390_CC_FOR_BFP128_RESULT(cc_dep1, cc_dep2);

   case S390_CC_OP_BFP_32_TO_INT_32:
      return S390_CC_FOR_BFP_CONVERT("cfebr", cc_dep1, cc_dep2);

   case S390_CC_OP_BFP_64_TO_INT_32:
      return S390_CC_FOR_BFP_CONVERT("cfdbr", cc_dep1, cc_dep2);

   case S390_CC_OP_BFP_128_TO_INT_32:
      return S390_CC_FOR_BFP128_CONVERT("cfxbr", cc_dep1, cc_dep2, cc_ndep);

   case S390_CC_OP_BFP_32_TO_INT_64:
      return S390_CC_FOR_BFP_CONVERT("cgebr", cc_dep1, cc_dep2);

   case S390_CC_OP_BFP_64_TO_INT_64:
      return S390_CC_FOR_BFP_CONVERT("cgdbr", cc_dep1, cc_dep2);

   case S390_CC_OP_BFP_128_TO_INT_64:
      return S390_CC_FOR_BFP128_CONVERT("cgxbr", cc_dep1, cc_dep2, cc_ndep);

   case S390_CC_OP_BFP_TDC_32:
      return S390_CC_FOR_BFP_TDC("tceb", cc_dep1, cc_dep2);

   case S390_CC_OP_BFP_TDC_64:
      return S390_CC_FOR_BFP_TDC("tcdb", cc_dep1, cc_dep2);

   case S390_CC_OP_BFP_TDC_128:
      return S390_CC_FOR_BFP128_TDC(cc_dep1, cc_dep2, cc_ndep);

   case S390_CC_OP_SET:
      return cc_dep1;

   case S390_CC_OP_BFP_32_TO_UINT_32:
      return S390_CC_FOR_BFP_UCONVERT(".insn rrf,0xb39c0000", cc_dep1, cc_dep2);

   case S390_CC_OP_BFP_64_TO_UINT_32:
      return S390_CC_FOR_BFP_UCONVERT(".insn rrf,0xb39d0000", cc_dep1, cc_dep2);

   case S390_CC_OP_BFP_128_TO_UINT_32:
      return S390_CC_FOR_BFP128_UCONVERT(".insn rrf,0xb39e0000", cc_dep1,
                                         cc_dep2, cc_ndep);

   case S390_CC_OP_BFP_32_TO_UINT_64:
      return S390_CC_FOR_BFP_UCONVERT(".insn rrf,0xb3ac0000", cc_dep1, cc_dep2);

   case S390_CC_OP_BFP_64_TO_UINT_64:
      return S390_CC_FOR_BFP_UCONVERT(".insn rrf,0xb3ad0000", cc_dep1, cc_dep2);

   case S390_CC_OP_BFP_128_TO_UINT_64:
      return S390_CC_FOR_BFP128_UCONVERT(".insn rrf,0xb3ae0000", cc_dep1,
                                         cc_dep2, cc_ndep);

   case S390_CC_OP_DFP_RESULT_64:
      return S390_CC_FOR_DFP_RESULT(cc_dep1);

   case S390_CC_OP_DFP_RESULT_128:
      return S390_CC_FOR_DFP128_RESULT(cc_dep1, cc_dep2);

   case S390_CC_OP_DFP_TDC_32:  
      return S390_CC_FOR_DFP_TD(".insn rxe, 0xed0000000050", cc_dep1, cc_dep2);

   case S390_CC_OP_DFP_TDC_64:  
      return S390_CC_FOR_DFP_TD(".insn rxe, 0xed0000000054", cc_dep1, cc_dep2);

   case S390_CC_OP_DFP_TDC_128: 
      return S390_CC_FOR_DFP128_TD(".insn rxe, 0xed0000000058", cc_dep1,
                                   cc_dep2, cc_ndep);

   case S390_CC_OP_DFP_TDG_32:  
      return S390_CC_FOR_DFP_TD(".insn rxe, 0xed0000000051", cc_dep1, cc_dep2);

   case S390_CC_OP_DFP_TDG_64:  
      return S390_CC_FOR_DFP_TD(".insn rxe, 0xed0000000055", cc_dep1, cc_dep2);

   case S390_CC_OP_DFP_TDG_128: 
      return S390_CC_FOR_DFP128_TD(".insn rxe, 0xed0000000059", cc_dep1,
                                   cc_dep2, cc_ndep);

   case S390_CC_OP_DFP_64_TO_INT_32: 
      return S390_CC_FOR_DFP_CONVERT(".insn rrf,0xb9410000", cc_dep1, cc_dep2);

   case S390_CC_OP_DFP_128_TO_INT_32: 
      return S390_CC_FOR_DFP128_CONVERT(".insn rrf,0xb9490000", cc_dep1,
                                        cc_dep2, cc_ndep);

   case S390_CC_OP_DFP_64_TO_INT_64: 
      return S390_CC_FOR_DFP_CONVERT(".insn rrf,0xb3e10000", cc_dep1, cc_dep2);

   case S390_CC_OP_DFP_128_TO_INT_64: 
      return S390_CC_FOR_DFP128_CONVERT(".insn rrf,0xb3e90000", cc_dep1,
                                        cc_dep2, cc_ndep);

   case S390_CC_OP_DFP_64_TO_UINT_32: 
      return S390_CC_FOR_DFP_UCONVERT(".insn rrf,0xb9430000", cc_dep1, cc_dep2);

   case S390_CC_OP_DFP_128_TO_UINT_32: 
      return S390_CC_FOR_DFP128_UCONVERT(".insn rrf,0xb94b0000", cc_dep1,
                                         cc_dep2, cc_ndep);

   case S390_CC_OP_DFP_64_TO_UINT_64: 
      return S390_CC_FOR_DFP_UCONVERT(".insn rrf,0xb9420000", cc_dep1, cc_dep2);

   case S390_CC_OP_DFP_128_TO_UINT_64: 
      return S390_CC_FOR_DFP128_UCONVERT(".insn rrf,0xb94a0000", cc_dep1,
                                         cc_dep2, cc_ndep);

   case S390_CC_OP_PFPO_32: {
      __asm__ volatile(
           "ler 4, %[cc_dep1]\n\t"      
           "lr  0, %[cc_dep2]\n\t"      
           ".short 0x010a\n\t"          
           "ipm %[psw]\n\t"             : [psw] "=d"(psw)
                                        : [cc_dep1] "f"(cc_dep1),
                                          [cc_dep2] "d"(cc_dep2)
                                        : "r0", "r1", "f4");
      return psw >> 28;  
   }

   case S390_CC_OP_PFPO_64: {
      __asm__ volatile(
           "ldr 4, %[cc_dep1]\n\t"
           "lr  0, %[cc_dep2]\n\t"      
           ".short 0x010a\n\t"          
           "ipm %[psw]\n\t"             : [psw] "=d"(psw)
                                        : [cc_dep1] "f"(cc_dep1),
                                          [cc_dep2] "d"(cc_dep2)
                                        : "r0", "r1", "f4");
      return psw >> 28;  
   }

   case S390_CC_OP_PFPO_128: {
      __asm__ volatile(
           "ldr 4,%[cc_dep1]\n\t"
           "ldr 6,%[cc_dep2]\n\t"
           "lr  0,%[cc_ndep]\n\t"       
           ".short 0x010a\n\t"          
           "ipm %[psw]\n\t"             : [psw] "=d"(psw)
                                        : [cc_dep1] "f"(cc_dep1),
                                          [cc_dep2] "f"(cc_dep2),
                                          [cc_ndep] "d"(cc_ndep)
                                        : "r0", "r1", "f0", "f2", "f4", "f6");
      return psw >> 28;  
   }

   default:
      break;
   }
#endif
   vpanic("s390_calculate_cc");
}


UInt
s390_calculate_cond(ULong mask, ULong op, ULong dep1, ULong dep2, ULong ndep)
{
   UInt cc = s390_calculate_cc(op, dep1, dep2, ndep);

   return ((mask << cc) & 0x8);
}



#define unop(op,a1) IRExpr_Unop((op),(a1))
#define binop(op,a1,a2) IRExpr_Binop((op),(a1),(a2))
#define mkU64(v) IRExpr_Const(IRConst_U64(v))
#define mkU32(v) IRExpr_Const(IRConst_U32(v))
#define mkU8(v)  IRExpr_Const(IRConst_U8(v))


static inline Bool
isC64(const IRExpr *expr)
{
   return expr->tag == Iex_Const && expr->Iex.Const.con->tag == Ico_U64;
}


IRExpr *
guest_s390x_spechelper(const HChar *function_name, IRExpr **args,
                       IRStmt **precedingStmts, Int n_precedingStmts)
{
   UInt i, arity = 0;

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

   

   if (vex_streq(function_name, "s390_calculate_cond")) {
      IRExpr *cond_expr, *cc_op_expr, *cc_dep1, *cc_dep2;
      ULong cond, cc_op;

      vassert(arity == 5);

      cond_expr  = args[0];
      cc_op_expr = args[1];

      if (! isC64(cond_expr))  return NULL;
      if (! isC64(cc_op_expr)) return NULL;

      cond    = cond_expr->Iex.Const.con->Ico.U64;
      cc_op   = cc_op_expr->Iex.Const.con->Ico.U64;

      vassert(cond <= 15);

      cc_dep1 = args[2];
      cc_dep2 = args[3];

      
      if (cc_op == S390_CC_OP_SIGNED_COMPARE) {
         if (cond == 8 || cond == 8 + 1) {
            return unop(Iop_1Uto32, binop(Iop_CmpEQ64, cc_dep1, cc_dep2));
         }
         if (cond == 4 + 2 || cond == 4 + 2 + 1) {
            return unop(Iop_1Uto32, binop(Iop_CmpNE64, cc_dep1, cc_dep2));
         }
         if (cond == 4 || cond == 4 + 1) {
            return unop(Iop_1Uto32, binop(Iop_CmpLT64S, cc_dep1, cc_dep2));
         }
         if (cond == 8 + 4 || cond == 8 + 4 + 1) {
            return unop(Iop_1Uto32, binop(Iop_CmpLE64S, cc_dep1, cc_dep2));
         }
         
         if (cond == 2 || cond == 2 + 1) {
            return unop(Iop_1Uto32, binop(Iop_CmpLT64S, cc_dep2, cc_dep1));
         }
         if (cond == 8 + 2 || cond == 8 + 2 + 1) {
            return unop(Iop_1Uto32, binop(Iop_CmpLE64S, cc_dep2, cc_dep1));
         }
         if (cond == 8 + 4 + 2 || cond == 8 + 4 + 2 + 1) {
            return mkU32(1);
         }
         
         return mkU32(0);
      }

      
      if (cc_op == S390_CC_OP_UNSIGNED_COMPARE) {
         if (cond == 8 || cond == 8 + 1) {
            return unop(Iop_1Uto32, binop(Iop_CmpEQ64, cc_dep1, cc_dep2));
         }
         if (cond == 4 + 2 || cond == 4 + 2 + 1) {
            return unop(Iop_1Uto32, binop(Iop_CmpNE64, cc_dep1, cc_dep2));
         }
         if (cond == 4 || cond == 4 + 1) {
            return unop(Iop_1Uto32, binop(Iop_CmpLT64U, cc_dep1, cc_dep2));
         }
         if (cond == 8 + 4 || cond == 8 + 4 + 1) {
            return unop(Iop_1Uto32, binop(Iop_CmpLE64U, cc_dep1, cc_dep2));
         }
         
         if (cond == 2 || cond == 2 + 1) {
            return unop(Iop_1Uto32, binop(Iop_CmpLT64U, cc_dep2, cc_dep1));
         }
         if (cond == 8 + 2 || cond == 8 + 2 + 1) {
            return unop(Iop_1Uto32, binop(Iop_CmpLE64U, cc_dep2, cc_dep1));
         }
         if (cond == 8 + 4 + 2 || cond == 8 + 4 + 2 + 1) {
            return mkU32(1);
         }
         
         return mkU32(0);
      }

      
      if (cc_op == S390_CC_OP_LOAD_AND_TEST) {
         if (cond == 8 || cond == 8 + 1) {
            return unop(Iop_1Uto32, binop(Iop_CmpEQ64, cc_dep1, mkU64(0)));
         }
         if (cond == 4 + 2 || cond == 4 + 2 + 1) {
            return unop(Iop_1Uto32, binop(Iop_CmpNE64, cc_dep1, mkU64(0)));
         }
         if (cond == 4 || cond == 4 + 1) {
            return unop(Iop_64to32, binop(Iop_Shr64, cc_dep1, mkU8(63)));
         }
         if (cond == 8 + 4 || cond == 8 + 4 + 1) {
            return unop(Iop_1Uto32, binop(Iop_CmpLE64S, cc_dep1, mkU64(0)));
         }
         
         if (cond == 2 || cond == 2 + 1) {
            return unop(Iop_1Uto32, binop(Iop_CmpLT64S, mkU64(0), cc_dep1));
         }
         if (cond == 8 + 2 || cond == 8 + 2 + 1) {
            return unop(Iop_64to32, binop(Iop_Xor64,
                                          binop(Iop_Shr64, cc_dep1, mkU8(63)),
                                          mkU64(1)));
         }
         if (cond == 8 + 4 + 2 || cond == 8 + 4 + 2 + 1) {
            return mkU32(1);
         }
         
         return mkU32(0);
      }

      
      if (cc_op == S390_CC_OP_BITWISE) {
         if ((cond & (8 + 4)) == 8 + 4) {
            return mkU32(1);
         }
         if (cond & 8) {
            return unop(Iop_1Uto32, binop(Iop_CmpEQ64, cc_dep1, mkU64(0)));
         }
         if (cond & 4) {
            return unop(Iop_1Uto32, binop(Iop_CmpNE64, cc_dep1, mkU64(0)));
         }
         
         return mkU32(0);
      }

      if (cc_op == S390_CC_OP_INSERT_CHAR_MASK_32) {
         ULong mask;
         UInt imask = 0, shift = 0;
         IRExpr *word;

         if (! isC64(cc_dep2)) goto missed;

         mask = cc_dep2->Iex.Const.con->Ico.U64;

         

         word = unop(Iop_64to32, cc_dep1);

         switch (mask) {
         case 0:  shift =  0; imask = 0x00000000; break;
         case 1:  shift = 24; imask = 0x000000FF; break;
         case 2:  shift = 16; imask = 0x0000FF00; break;
         case 3:  shift = 16; imask = 0x0000FFFF; break;
         case 4:  shift =  8; imask = 0x00FF0000; break;
         case 5:  shift =  8; imask = 0x00FF00FF; break;
         case 6:  shift =  8; imask = 0x00FFFF00; break;
         case 7:  shift =  8; imask = 0x00FFFFFF; break;
         case 8:  shift =  0; imask = 0xFF000000; break;
         case 9:  shift =  0; imask = 0xFF0000FF; break;
         case 10: shift =  0; imask = 0xFF00FF00; break;
         case 11: shift =  0; imask = 0xFF00FFFF; break;
         case 12: shift =  0; imask = 0xFFFF0000; break;
         case 13: shift =  0; imask = 0xFFFF00FF; break;
         case 14: shift =  0; imask = 0xFFFFFF00; break;
         case 15: shift =  0; imask = 0xFFFFFFFF; break;
         }

         
         word = binop(Iop_And32, word, mkU32(imask));

         if (cond == 8 || cond == 8 + 1) {
            return unop(Iop_1Uto32, binop(Iop_CmpEQ32, word, mkU32(0)));
         }
         if (cond == 4 + 2 || cond == 4 + 2 + 1) {
            return unop(Iop_1Uto32, binop(Iop_CmpNE32, word, mkU32(0)));
         }

         
         if (shift != 0) {
            word = binop(Iop_Sar32, binop(Iop_Shl32, word, mkU8(shift)),
                         mkU8(shift));
         }

         if (cond == 4 || cond == 4 + 1) {  
            return unop(Iop_1Uto32, binop(Iop_CmpLT32S, word, mkU32(0)));
         }
         if (cond == 2 || cond == 2 + 1) {  
            return unop(Iop_1Uto32, binop(Iop_CmpLT32S, mkU32(0), word));
         }
         if (cond == 8 + 4 || cond == 8 + 4 + 1) {
            return unop(Iop_1Uto32, binop(Iop_CmpLE32S, word, mkU32(0)));
         }
         if (cond == 8 + 2 || cond == 8 + 2 + 1) {
            return unop(Iop_1Uto32, binop(Iop_CmpLE32S, mkU32(0), word));
         }
         if (cond == 8 + 4 + 2 || cond == 8 + 4 + 2 + 1) {
            return mkU32(1);
         }
         
         return mkU32(0);
      }

      if (cc_op == S390_CC_OP_TEST_UNDER_MASK_8) {
         ULong mask16;

         if (! isC64(cc_dep2)) goto missed;

         mask16 = cc_dep2->Iex.Const.con->Ico.U64;

         if (mask16 == 0) {   
            if (cond & 0x8) return mkU32(1);
            return mkU32(0);
         }

         
         if (cond == 8 || cond == 8 + 2) {
            return unop(Iop_1Uto32, binop(Iop_CmpEQ64,
                                          binop(Iop_And64, cc_dep1, cc_dep2),
                                          mkU64(0)));
         }
         if (cond == 7 || cond == 7 - 2) {
            return unop(Iop_1Uto32, binop(Iop_CmpNE64,
                                          binop(Iop_And64, cc_dep1, cc_dep2),
                                          mkU64(0)));
         }
         if (cond == 1 || cond == 1 + 2) {
            return unop(Iop_1Uto32, binop(Iop_CmpEQ64,
                                          binop(Iop_And64, cc_dep1, cc_dep2),
                                          cc_dep2));
         }
         if (cond == 14 || cond == 14 - 2) {  
            return unop(Iop_1Uto32, binop(Iop_CmpNE64,
                                          binop(Iop_And64, cc_dep1, cc_dep2),
                                          cc_dep2));
         }
         goto missed;
      }

      if (cc_op == S390_CC_OP_TEST_UNDER_MASK_16) {
         ULong mask16;
         UInt msb;

         if (! isC64(cc_dep2)) goto missed;

         mask16 = cc_dep2->Iex.Const.con->Ico.U64;

         if (mask16 == 0) {   
            if (cond & 0x8) return mkU32(1);
            return mkU32(0);
         }

         if (cond == 8) {
            return unop(Iop_1Uto32, binop(Iop_CmpEQ64,
                                          binop(Iop_And64, cc_dep1, cc_dep2),
                                          mkU64(0)));
         }
         if (cond == 7) {
            return unop(Iop_1Uto32, binop(Iop_CmpNE64,
                                          binop(Iop_And64, cc_dep1, cc_dep2),
                                          mkU64(0)));
         }
         if (cond == 1) {
            return unop(Iop_1Uto32, binop(Iop_CmpEQ64,
                                          binop(Iop_And64, cc_dep1, cc_dep2),
                                          mkU64(mask16)));
         }
         if (cond == 14) {  
            return unop(Iop_1Uto32, binop(Iop_CmpNE64,
                                          binop(Iop_And64, cc_dep1, cc_dep2),
                                          mkU64(mask16)));
         }

         
         msb = 0x8000;
         while (msb > mask16)
            msb >>= 1;

         if (cond == 2) {  
            IRExpr *c1, *c2;

            
            c1 = binop(Iop_CmpNE64,
                       binop(Iop_And64, cc_dep1, mkU64(msb)), mkU64(0));
            c2 = binop(Iop_CmpNE64,
                       binop(Iop_And64, cc_dep1, cc_dep2),
                       mkU64(mask16));
            return binop(Iop_And32, unop(Iop_1Uto32, c1),
                         unop(Iop_1Uto32, c2));
         }

         if (cond == 4) {  
            IRExpr *c1, *c2;

            
            c1 = binop(Iop_CmpEQ64,
                       binop(Iop_And64, cc_dep1, mkU64(msb)), mkU64(0));
            c2 = binop(Iop_CmpNE64,
                       binop(Iop_And64, cc_dep1, cc_dep2),
                       mkU64(0));
            return binop(Iop_And32, unop(Iop_1Uto32, c1),
                         unop(Iop_1Uto32, c2));
         }

         if (cond == 11) {  
            IRExpr *c1, *c2;

            c1 = binop(Iop_CmpNE64,
                       binop(Iop_And64, cc_dep1, mkU64(msb)), mkU64(0));
            c2 = binop(Iop_CmpEQ64,
                       binop(Iop_And64, cc_dep1, cc_dep2),
                       mkU64(0));
            return binop(Iop_Or32, unop(Iop_1Uto32, c1),
                         unop(Iop_1Uto32, c2));
         }

         if (cond == 3) {  
            return unop(Iop_1Uto32,
                        binop(Iop_CmpNE64,
                              binop(Iop_And64, cc_dep1, mkU64(msb)),
                              mkU64(0)));
         }
         if (cond == 12) { 
            return unop(Iop_1Uto32,
                        binop(Iop_CmpEQ64,
                              binop(Iop_And64, cc_dep1, mkU64(msb)),
                              mkU64(0)));
         }
         
         goto missed;
      }

      
      if (cc_op == S390_CC_OP_UNSIGNED_SUB_64 ||
          cc_op == S390_CC_OP_UNSIGNED_SUB_32) {
         if (cond == 1 || cond == 1 + 8) {  
            return unop(Iop_1Uto32, binop(Iop_CmpLT64U, cc_dep2, cc_dep1));
         }
         if (cond == 2 || cond == 2 + 8) {  
            return unop(Iop_1Uto32, binop(Iop_CmpEQ64, cc_dep1, cc_dep2));
         }
         if (cond == 4 || cond == 4 + 8) {  
            return unop(Iop_1Uto32, binop(Iop_CmpLT64U, cc_dep1, cc_dep2));
         }
         if (cond == 3 || cond == 3 + 8) {  
            return unop(Iop_1Uto32, binop(Iop_CmpLE64U, cc_dep2, cc_dep1));
         }
         if (cond == 6 || cond == 6 + 8) {  
            return unop(Iop_1Uto32, binop(Iop_CmpLE64U, cc_dep1, cc_dep2));
         }

         if (cond == 5 || cond == 5 + 8) {  
            return unop(Iop_1Uto32, binop(Iop_CmpNE64, cc_dep1, cc_dep2));
         }
         if (cond == 7 || cond == 7 + 8) {
            return mkU32(1);
         }
         
         return mkU32(0);
      }

      
      if (cc_op == S390_CC_OP_UNSIGNED_ADD_64) {
         if (cond == 8) { 
            
            return unop(Iop_1Uto32, binop(Iop_CmpEQ64,
                                          binop(Iop_Or64, cc_dep1, cc_dep2),
                                          mkU64(0)));
         }
         if (cond == 7) { 
            
            return unop(Iop_1Uto32, binop(Iop_CmpNE64,
                                          binop(Iop_Or64, cc_dep1, cc_dep2),
                                          mkU64(0)));
         }
         if (cond == 8 + 2) {  
            return unop(Iop_1Uto32, binop(Iop_CmpEQ64,
                                          binop(Iop_Add64, cc_dep1, cc_dep2),
                                          mkU64(0)));
         }
         if (cond == 4 + 1) {  
            return unop(Iop_1Uto32, binop(Iop_CmpNE64,
                                          binop(Iop_Add64, cc_dep1, cc_dep2),
                                          mkU64(0)));
         }
         goto missed;
      }

      
      if (cc_op == S390_CC_OP_UNSIGNED_ADD_32) {
         if (cond == 8) { 
            
            return unop(Iop_1Uto32, binop(Iop_CmpEQ64,
                                          binop(Iop_Or64, cc_dep1, cc_dep2),
                                          mkU64(0)));
         }
         if (cond == 7) { 
            
            return unop(Iop_1Uto32, binop(Iop_CmpNE64,
                                          binop(Iop_Or64, cc_dep1, cc_dep2),
                                          mkU64(0)));
         }
         if (cond == 8 + 2) {  
            return unop(Iop_1Uto32, binop(Iop_CmpEQ32,
                                          binop(Iop_Add32,
                                                unop(Iop_64to32, cc_dep1),
                                                unop(Iop_64to32, cc_dep2)),
                                          mkU32(0)));
         }
         if (cond == 4 + 1) {  
            return unop(Iop_1Uto32, binop(Iop_CmpNE32,
                                          binop(Iop_Add32,
                                                unop(Iop_64to32, cc_dep1),
                                                unop(Iop_64to32, cc_dep2)),
                                          mkU32(0)));
         }
         goto missed;
      }

      
      if (cc_op == S390_CC_OP_SET) {

        return unop(Iop_1Uto32,
                    binop(Iop_CmpNE64,
                          binop(Iop_And64,
                                binop(Iop_Shl64, cond_expr,
                                      unop(Iop_64to8, cc_dep1)),
                                mkU64(8)),
                          mkU64(0)));
      }

      goto missed;
   }

   

   if (vex_streq(function_name, "s390_calculate_cc")) {
      IRExpr *cc_op_expr, *cc_dep1;
      ULong cc_op;

      vassert(arity == 4);

      cc_op_expr = args[0];

      if (! isC64(cc_op_expr)) return NULL;

      cc_op   = cc_op_expr->Iex.Const.con->Ico.U64;
      cc_dep1 = args[1];

      if (cc_op == S390_CC_OP_BITWISE) {
         return unop(Iop_1Uto32,
                     binop(Iop_CmpNE64, cc_dep1, mkU64(0)));
      }

      if (cc_op == S390_CC_OP_SET) {
         return unop(Iop_64to32, cc_dep1);
      }

      goto missed;
   }

missed:
   return NULL;
}

