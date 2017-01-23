

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
*/

#ifndef __LIBVEX_PUB_GUEST_ARM_H
#define __LIBVEX_PUB_GUEST_ARM_H

#include "libvex_basictypes.h"



typedef
   struct {
      
      
      UInt host_EvC_FAILADDR; 
      UInt host_EvC_COUNTER;  
      UInt guest_R0;
      UInt guest_R1;
      UInt guest_R2;
      UInt guest_R3;
      UInt guest_R4;
      UInt guest_R5;
      UInt guest_R6;
      UInt guest_R7;
      UInt guest_R8;
      UInt guest_R9;
      UInt guest_R10;
      UInt guest_R11;
      UInt guest_R12;
      UInt guest_R13;     
      UInt guest_R14;     
      UInt guest_R15T;

      
      UInt guest_CC_OP;
      UInt guest_CC_DEP1;
      UInt guest_CC_DEP2;
      UInt guest_CC_NDEP;

      UInt guest_QFLAG32;

      UInt guest_GEFLAG0;
      UInt guest_GEFLAG1;
      UInt guest_GEFLAG2;
      UInt guest_GEFLAG3;

      
      
      UInt guest_EMNOTE;

      
      UInt guest_CMSTART;
      UInt guest_CMLEN;

      UInt guest_NRADDR;

      
      UInt guest_IP_AT_SYSCALL;

      
      
      ULong guest_D0;
      ULong guest_D1;
      ULong guest_D2;
      ULong guest_D3;
      ULong guest_D4;
      ULong guest_D5;
      ULong guest_D6;
      ULong guest_D7;
      ULong guest_D8;
      ULong guest_D9;
      ULong guest_D10;
      ULong guest_D11;
      ULong guest_D12;
      ULong guest_D13;
      ULong guest_D14;
      ULong guest_D15;
      ULong guest_D16;
      ULong guest_D17;
      ULong guest_D18;
      ULong guest_D19;
      ULong guest_D20;
      ULong guest_D21;
      ULong guest_D22;
      ULong guest_D23;
      ULong guest_D24;
      ULong guest_D25;
      ULong guest_D26;
      ULong guest_D27;
      ULong guest_D28;
      ULong guest_D29;
      ULong guest_D30;
      ULong guest_D31;
      UInt  guest_FPSCR;

      UInt guest_TPIDRURO;

      UInt guest_ITSTATE;

      
      UInt padding1;
   }
   VexGuestARMState;





extern
void LibVEX_GuestARM_initialise ( VexGuestARMState* vex_state );


extern
UInt LibVEX_GuestARM_get_cpsr ( const VexGuestARMState* vex_state );


#endif 


