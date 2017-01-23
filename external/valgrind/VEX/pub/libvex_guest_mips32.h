

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2010-2013 RT-RK
      mips-valgrind@rt-rk.com

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
*/

#ifndef __LIBVEX_PUB_GUEST_MIPS32_H
#define __LIBVEX_PUB_GUEST_MIPS32_H

#include "libvex_basictypes.h"



typedef
   struct {
      
       UInt guest_r0;   
       UInt guest_r1;   
       UInt guest_r2;   
       UInt guest_r3;  
       UInt guest_r4;  
       UInt guest_r5;
       UInt guest_r6;
       UInt guest_r7;
       UInt guest_r8;  
       UInt guest_r9;
       UInt guest_r10;
       UInt guest_r11;
       UInt guest_r12;
       UInt guest_r13;
       UInt guest_r14;
       UInt guest_r15;
       UInt guest_r16;  
       UInt guest_r17;
       UInt guest_r18;
       UInt guest_r19;
       UInt guest_r20;
       UInt guest_r21;
       UInt guest_r22;
       UInt guest_r23;
       UInt guest_r24;  
       UInt guest_r25;
       UInt guest_r26;  
       UInt guest_r27;
       UInt guest_r28;  
       UInt guest_r29;  
       UInt guest_r30;  
       UInt guest_r31;  
       UInt guest_PC;  
       UInt guest_HI;  
       UInt guest_LO;  

      
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
       ULong guest_f16;
       ULong guest_f17;
       ULong guest_f18;
       ULong guest_f19;
       ULong guest_f20;
       ULong guest_f21;
       ULong guest_f22;
       ULong guest_f23;
       ULong guest_f24;
       ULong guest_f25;
       ULong guest_f26;
       ULong guest_f27;
       ULong guest_f28;
       ULong guest_f29;
       ULong guest_f30;
       ULong guest_f31;

       UInt guest_FIR;
       UInt guest_FCCR;
       UInt guest_FEXR;
       UInt guest_FENR;
       UInt guest_FCSR;

       UInt guest_ULR;

      
       UInt guest_EMNOTE;

      
       UInt guest_CMSTART;
       UInt guest_CMLEN;
       UInt guest_NRADDR;

       UInt host_EvC_FAILADDR;
       UInt host_EvC_COUNTER;
       UInt guest_COND;

      
       UInt guest_DSPControl;
       ULong guest_ac0;
       ULong guest_ac1;
       ULong guest_ac2;
       ULong guest_ac3;

        UInt padding;
} VexGuestMIPS32State;



extern
void LibVEX_GuestMIPS32_initialise ( VexGuestMIPS32State* vex_state );


#endif 


