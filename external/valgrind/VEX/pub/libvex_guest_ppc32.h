

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

#ifndef __LIBVEX_PUB_GUEST_PPC32_H
#define __LIBVEX_PUB_GUEST_PPC32_H

#include "libvex_basictypes.h"



#define VEX_GUEST_PPC32_REDIR_STACK_SIZE (16 * 2)

typedef
   struct {
      
       UInt host_EvC_FAILADDR;
       UInt host_EvC_COUNTER;
       UInt pad3;
       UInt pad4; 
      
      
       UInt guest_GPR0;
       UInt guest_GPR1;
       UInt guest_GPR2;
       UInt guest_GPR3;
       UInt guest_GPR4;
       UInt guest_GPR5;
       UInt guest_GPR6;
       UInt guest_GPR7;
       UInt guest_GPR8;
       UInt guest_GPR9;
       UInt guest_GPR10;
       UInt guest_GPR11;
       UInt guest_GPR12;
       UInt guest_GPR13;
       UInt guest_GPR14;
       UInt guest_GPR15;
       UInt guest_GPR16;
       UInt guest_GPR17;
       UInt guest_GPR18;
       UInt guest_GPR19;
       UInt guest_GPR20;
       UInt guest_GPR21;
       UInt guest_GPR22;
       UInt guest_GPR23;
       UInt guest_GPR24;
       UInt guest_GPR25;
       UInt guest_GPR26;
       UInt guest_GPR27;
       UInt guest_GPR28;
       UInt guest_GPR29;
       UInt guest_GPR30;
       UInt guest_GPR31;

      
      
      
      
      
      
      
      
      

      
      
      

       U128 guest_VSR0;
       U128 guest_VSR1;
       U128 guest_VSR2;
       U128 guest_VSR3;
       U128 guest_VSR4;
       U128 guest_VSR5;
       U128 guest_VSR6;
       U128 guest_VSR7;
       U128 guest_VSR8;
       U128 guest_VSR9;
       U128 guest_VSR10;
       U128 guest_VSR11;
       U128 guest_VSR12;
       U128 guest_VSR13;
       U128 guest_VSR14;
       U128 guest_VSR15;
       U128 guest_VSR16;
       U128 guest_VSR17;
       U128 guest_VSR18;
       U128 guest_VSR19;
       U128 guest_VSR20;
       U128 guest_VSR21;
       U128 guest_VSR22;
       U128 guest_VSR23;
       U128 guest_VSR24;
       U128 guest_VSR25;
       U128 guest_VSR26;
       U128 guest_VSR27;
       U128 guest_VSR28;
       U128 guest_VSR29;
       U128 guest_VSR30;
       U128 guest_VSR31;
       U128 guest_VSR32;
       U128 guest_VSR33;
       U128 guest_VSR34;
       U128 guest_VSR35;
       U128 guest_VSR36;
       U128 guest_VSR37;
       U128 guest_VSR38;
       U128 guest_VSR39;
       U128 guest_VSR40;
       U128 guest_VSR41;
       U128 guest_VSR42;
       U128 guest_VSR43;
       U128 guest_VSR44;
       U128 guest_VSR45;
       U128 guest_VSR46;
       U128 guest_VSR47;
       U128 guest_VSR48;
       U128 guest_VSR49;
       U128 guest_VSR50;
       U128 guest_VSR51;
       U128 guest_VSR52;
       U128 guest_VSR53;
       U128 guest_VSR54;
       U128 guest_VSR55;
       U128 guest_VSR56;
       U128 guest_VSR57;
       U128 guest_VSR58;
       U128 guest_VSR59;
       U128 guest_VSR60;
       U128 guest_VSR61;
       U128 guest_VSR62;
       U128 guest_VSR63;

       UInt guest_CIA;    
       UInt guest_LR;     
       UInt guest_CTR;    

      
       UChar guest_XER_SO; 
       UChar guest_XER_OV; 
       UChar guest_XER_CA; 
       UChar guest_XER_BC; 

      
       UChar guest_CR0_321; 
       UChar guest_CR0_0;   
       UChar guest_CR1_321; 
       UChar guest_CR1_0;   
       UChar guest_CR2_321; 
       UChar guest_CR2_0;   
       UChar guest_CR3_321; 
       UChar guest_CR3_0;   
       UChar guest_CR4_321; 
       UChar guest_CR4_0;   
       UChar guest_CR5_321; 
       UChar guest_CR5_0;   
       UChar guest_CR6_321; 
       UChar guest_CR6_0;   
       UChar guest_CR7_321; 
       UChar guest_CR7_0;   

      
       UChar guest_FPROUND; 
       UChar guest_DFPROUND; 
       UChar pad1;
       UChar pad2;

      
       UInt guest_VRSAVE;

      
       UInt guest_VSCR;

      
       UInt guest_EMNOTE;

      
       UInt guest_CMSTART;
       UInt guest_CMLEN;

       UInt guest_NRADDR;
       UInt guest_NRADDR_GPR2; 

       UInt guest_REDIR_SP;
       UInt guest_REDIR_STACK[VEX_GUEST_PPC32_REDIR_STACK_SIZE];

       UInt guest_IP_AT_SYSCALL;

       UInt guest_SPRG3_RO;
       UInt  padding1;
       ULong guest_TFHAR;     
       ULong guest_TEXASR;    
       ULong guest_TFIAR;     
       UInt  guest_TEXASRU;   

      
       UInt  padding2;

   }
   VexGuestPPC32State;





extern
void LibVEX_GuestPPC32_initialise ( VexGuestPPC32State* vex_state );


extern
void LibVEX_GuestPPC32_put_CR ( UInt cr_native,
                                VexGuestPPC32State* vex_state );

extern
UInt LibVEX_GuestPPC32_get_CR ( const VexGuestPPC32State* vex_state );


extern
void LibVEX_GuestPPC32_put_XER ( UInt xer_native,
                                 VexGuestPPC32State* vex_state );

extern
UInt LibVEX_GuestPPC32_get_XER ( const VexGuestPPC32State* vex_state );

#endif 


