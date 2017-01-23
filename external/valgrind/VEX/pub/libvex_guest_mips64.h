

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   The GNU General Public License is contained in the file COPYING.

   Neither the names of the U.S. Department of Energy nor the
   University of California nor the names of its contributors may be
   used to endorse or promote products derived from this software
   without prior written permission.
*/

#ifndef __LIBVEX_PUB_GUEST_MIPS64_H
#define __LIBVEX_PUB_GUEST_MIPS64_H

#include "libvex_basictypes.h"
#include "libvex_emnote.h"



typedef
   struct {
      
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
       ULong guest_r16;  
       ULong guest_r17;
       ULong guest_r18;
       ULong guest_r19;
       ULong guest_r20;
       ULong guest_r21;
       ULong guest_r22;
       ULong guest_r23;
       ULong guest_r24;  
       ULong guest_r25;
       ULong guest_r26;  
       ULong guest_r27;
       ULong guest_r28;  
       ULong guest_r29;  
       ULong guest_r30;  
       ULong guest_r31;  
       ULong guest_PC;   
       ULong guest_HI;   
       ULong guest_LO;   

      
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

        ULong guest_ULR;         

      
        UInt guest_EMNOTE;       

      
        ULong guest_CMSTART;     
        ULong guest_CMLEN;       

        ULong guest_NRADDR;      

        ULong host_EvC_FAILADDR; 
        UInt host_EvC_COUNTER;   
        UInt guest_COND;         
        UInt padding[2];
} VexGuestMIPS64State;




extern
void LibVEX_GuestMIPS64_initialise ( VexGuestMIPS64State* vex_state );

#endif 


