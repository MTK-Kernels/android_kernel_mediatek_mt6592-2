

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2013-2013 OpenWorks
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

#ifndef __LIBVEX_PUB_GUEST_ARM64_H
#define __LIBVEX_PUB_GUEST_ARM64_H

#include "libvex_basictypes.h"



typedef
   struct {
      
        ULong host_EvC_FAILADDR;
        UInt  host_EvC_COUNTER;
       UInt  pad0;
      
      ULong guest_X0;
      ULong guest_X1;
      ULong guest_X2;
      ULong guest_X3;
      ULong guest_X4;
      ULong guest_X5;
      ULong guest_X6;
      ULong guest_X7;
      ULong guest_X8;
      ULong guest_X9;
      ULong guest_X10;
      ULong guest_X11;
      ULong guest_X12;
      ULong guest_X13;
      ULong guest_X14;
      ULong guest_X15;
      ULong guest_X16;
      ULong guest_X17;
      ULong guest_X18;
      ULong guest_X19;
      ULong guest_X20;
      ULong guest_X21;
      ULong guest_X22;
      ULong guest_X23;
      ULong guest_X24;
      ULong guest_X25;
      ULong guest_X26;
      ULong guest_X27;
      ULong guest_X28;
      ULong guest_X29;
      ULong guest_X30;     
      ULong guest_XSP;
      ULong guest_PC;

      ULong guest_CC_OP;
      ULong guest_CC_DEP1;
      ULong guest_CC_DEP2;
      ULong guest_CC_NDEP;

      
      ULong guest_TPIDR_EL0;

      
      U128 guest_Q0;
      U128 guest_Q1;
      U128 guest_Q2;
      U128 guest_Q3;
      U128 guest_Q4;
      U128 guest_Q5;
      U128 guest_Q6;
      U128 guest_Q7;
      U128 guest_Q8;
      U128 guest_Q9;
      U128 guest_Q10;
      U128 guest_Q11;
      U128 guest_Q12;
      U128 guest_Q13;
      U128 guest_Q14;
      U128 guest_Q15;
      U128 guest_Q16;
      U128 guest_Q17;
      U128 guest_Q18;
      U128 guest_Q19;
      U128 guest_Q20;
      U128 guest_Q21;
      U128 guest_Q22;
      U128 guest_Q23;
      U128 guest_Q24;
      U128 guest_Q25;
      U128 guest_Q26;
      U128 guest_Q27;
      U128 guest_Q28;
      U128 guest_Q29;
      U128 guest_Q30;
      U128 guest_Q31;

      U128 guest_QCFLAG;

      
      
      UInt guest_EMNOTE;

      
      ULong guest_CMSTART;
      ULong guest_CMLEN;

      ULong guest_NRADDR;

      ULong guest_IP_AT_SYSCALL;

      UInt  guest_FPCR;

      
      
      
   }
   VexGuestARM64State;





extern
void LibVEX_GuestARM64_initialise ( VexGuestARM64State* vex_state );

extern
ULong LibVEX_GuestARM64_get_nzcv ( 
                                   const VexGuestARM64State* vex_state );

extern
ULong LibVEX_GuestARM64_get_fpsr ( 
                                   const VexGuestARM64State* vex_state );

extern
void LibVEX_GuestARM64_set_fpsr ( VexGuestARM64State* vex_state,
                                  ULong fpsr );
                                  

#endif 


