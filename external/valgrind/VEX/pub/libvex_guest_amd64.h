

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

#ifndef __LIBVEX_PUB_GUEST_AMD64_H
#define __LIBVEX_PUB_GUEST_AMD64_H

#include "libvex_basictypes.h"





typedef
   struct {
       ULong  host_EvC_FAILADDR;
       UInt   host_EvC_COUNTER;
       UInt   pad0;
       ULong  guest_RAX;
       ULong  guest_RCX;
       ULong  guest_RDX;
       ULong  guest_RBX;
       ULong  guest_RSP;
       ULong  guest_RBP;
       ULong  guest_RSI;
       ULong  guest_RDI;
       ULong  guest_R8;
       ULong  guest_R9;
       ULong  guest_R10;
       ULong  guest_R11;
       ULong  guest_R12;
       ULong  guest_R13;
       ULong  guest_R14;
       ULong  guest_R15;
      
       ULong  guest_CC_OP;
       ULong  guest_CC_DEP1;
       ULong  guest_CC_DEP2;
       ULong  guest_CC_NDEP;
      
       ULong  guest_DFLAG;
       ULong  guest_RIP;
      
       ULong  guest_ACFLAG;
      
       ULong guest_IDFLAG;

       ULong guest_FS_CONST;

      ULong guest_SSEROUND;
      U256  guest_YMM0;
      U256  guest_YMM1;
      U256  guest_YMM2;
      U256  guest_YMM3;
      U256  guest_YMM4;
      U256  guest_YMM5;
      U256  guest_YMM6;
      U256  guest_YMM7;
      U256  guest_YMM8;
      U256  guest_YMM9;
      U256  guest_YMM10;
      U256  guest_YMM11;
      U256  guest_YMM12;
      U256  guest_YMM13;
      U256  guest_YMM14;
      U256  guest_YMM15;
      U256  guest_YMM16;

      
      UInt  guest_FTOP;
      UInt  pad1;
      ULong guest_FPREG[8];
      UChar guest_FPTAG[8];
      ULong guest_FPROUND;
      ULong guest_FC3210;

      
      UInt  guest_EMNOTE;
      UInt  pad2;

      ULong guest_CMSTART;
      ULong guest_CMLEN;

      ULong guest_NRADDR;

      
      ULong guest_SC_CLASS;

      ULong guest_GS_CONST;

      ULong guest_IP_AT_SYSCALL;

      
      ULong pad3;
   }
   VexGuestAMD64State;





extern
void LibVEX_GuestAMD64_initialise ( VexGuestAMD64State* vex_state );


extern 
ULong LibVEX_GuestAMD64_get_rflags ( const VexGuestAMD64State* vex_state );

extern
void
LibVEX_GuestAMD64_put_rflag_c ( ULong new_carry_flag,
                                VexGuestAMD64State* vex_state );


#endif 

