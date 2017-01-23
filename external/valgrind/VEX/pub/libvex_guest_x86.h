

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

#ifndef __LIBVEX_PUB_GUEST_X86_H
#define __LIBVEX_PUB_GUEST_X86_H

#include "libvex_basictypes.h"





typedef
   struct {
      
      UInt  host_EvC_FAILADDR; 
      UInt  host_EvC_COUNTER;  
      UInt  guest_EAX;         
      UInt  guest_ECX;
      UInt  guest_EDX;
      UInt  guest_EBX;
      UInt  guest_ESP;
      UInt  guest_EBP;
      UInt  guest_ESI;
      UInt  guest_EDI;         

      
      UInt  guest_CC_OP;       
      UInt  guest_CC_DEP1;
      UInt  guest_CC_DEP2;
      UInt  guest_CC_NDEP;     
      
      UInt  guest_DFLAG;       
      
      UInt  guest_IDFLAG;      
      
      UInt  guest_ACFLAG;      

      
      UInt  guest_EIP;         

      
      ULong guest_FPREG[8];    
      UChar guest_FPTAG[8];   
      UInt  guest_FPROUND;    
      UInt  guest_FC3210;     
      UInt  guest_FTOP;       

      
      UInt  guest_SSEROUND;   
      U128  guest_XMM0;       
      U128  guest_XMM1;
      U128  guest_XMM2;
      U128  guest_XMM3;
      U128  guest_XMM4;
      U128  guest_XMM5;
      U128  guest_XMM6;
      U128  guest_XMM7;

      
      UShort guest_CS;
      UShort guest_DS;
      UShort guest_ES;
      UShort guest_FS;
      UShort guest_GS;
      UShort guest_SS;
      
      HWord  guest_LDT; 
      HWord  guest_GDT; 

      
      UInt   guest_EMNOTE;

      
      UInt guest_CMSTART;
      UInt guest_CMLEN;

      UInt guest_NRADDR;

      
      UInt guest_SC_CLASS;

      UInt guest_IP_AT_SYSCALL;

      
      UInt padding1;
   }
   VexGuestX86State;

#define VEX_GUEST_X86_LDT_NENT  8192 
#define VEX_GUEST_X86_GDT_NENT  8192 





typedef struct {
    union {
       struct {
          UShort  LimitLow;
          UShort  BaseLow;
          UInt    BaseMid         : 8;
          UInt    Type            : 5;
          UInt    Dpl             : 2;
          UInt    Pres            : 1;
          UInt    LimitHi         : 4;
          UInt    Sys             : 1;
          UInt    Reserved_0      : 1;
          UInt    Default_Big     : 1;
          UInt    Granularity     : 1;
          UInt    BaseHi          : 8;
       } Bits;
       struct {
          UInt word1;
          UInt word2;
       } Words;
    }
    LdtEnt;
} VexGuestX86SegDescr;




extern
void LibVEX_GuestX86_initialise ( VexGuestX86State* vex_state );


extern 
UInt LibVEX_GuestX86_get_eflags ( const VexGuestX86State* vex_state );

extern
void
LibVEX_GuestX86_put_eflag_c ( UInt new_carry_flag,
                              VexGuestX86State* vex_state );

#endif 

