

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

#ifndef __LIBVEX_PUB_GUEST_S390X_H
#define __LIBVEX_PUB_GUEST_S390X_H

#include "libvex_basictypes.h"


typedef struct {


     UInt guest_a0;
     UInt guest_a1;
     UInt guest_a2;
     UInt guest_a3;
     UInt guest_a4;
     UInt guest_a5;
     UInt guest_a6;
     UInt guest_a7;
     UInt guest_a8;
     UInt guest_a9;
     UInt guest_a10;
     UInt guest_a11;
     UInt guest_a12;
     UInt guest_a13;
     UInt guest_a14;
     UInt guest_a15;


     ULong guest_f0;
     ULong guest_f1;
     ULong guest_f2;
     ULong guest_f3;
     ULong guest_f4;
     ULong guest_f5;
     ULong guest_f6;
     ULong guest_f7;
     ULong guest_f8;
     ULong guest_f9;
     ULong guest_f10;
     ULong guest_f11;
     ULong guest_f12;
     ULong guest_f13;
     ULong guest_f14;
     ULong guest_f15;


     ULong guest_r0;
     ULong guest_r1;
     ULong guest_r2;
     ULong guest_r3;
     ULong guest_r4;
     ULong guest_r5;
     ULong guest_r6;
     ULong guest_r7;
     ULong guest_r8;
     ULong guest_r9;
     ULong guest_r10;
     ULong guest_r11;
     ULong guest_r12;
     ULong guest_r13;
     ULong guest_r14;
     ULong guest_r15;


     ULong guest_counter;
     UInt guest_fpc;
     UChar unused[4]; 
     ULong guest_IA;


     ULong guest_SYSNO;


     ULong guest_CC_OP;
     ULong guest_CC_DEP1;
     ULong guest_CC_DEP2;
     ULong guest_CC_NDEP;


   
     ULong guest_NRADDR;
     ULong guest_CMSTART;
     ULong guest_CMLEN;

     ULong guest_IP_AT_SYSCALL;

   
     UInt guest_EMNOTE;

   
     UInt  host_EvC_COUNTER;
     ULong host_EvC_FAILADDR;

     UChar padding[0];

     
} VexGuestS390XState;



void LibVEX_GuestS390X_initialise(VexGuestS390XState *);


#define guest_LR guest_r14  
#define guest_SP guest_r15  
#define guest_FP guest_r11  


#endif 
