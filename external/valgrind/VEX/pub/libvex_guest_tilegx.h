

/*
  This file is part of Valgrind, a dynamic binary instrumentation
  framework.

  Copyright (C) 2010-2013 Tilera Corp.

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


#ifndef __LIBVEX_PUB_GUEST_TILEGX_H
#define __LIBVEX_PUB_GUEST_TILEGX_H

#include "libvex_basictypes.h"
#include "libvex_emnote.h"

#undef   TILEGX_DEBUG


typedef ULong ULONG;

typedef
struct {
  
   ULONG guest_r0;
   ULONG guest_r1;
   ULONG guest_r2;
   ULONG guest_r3;
   ULONG guest_r4;
   ULONG guest_r5;
   ULONG guest_r6;
   ULONG guest_r7;
   ULONG guest_r8;
   ULONG guest_r9;
   ULONG guest_r10;
   ULONG guest_r11;
   ULONG guest_r12;
   ULONG guest_r13;
   ULONG guest_r14;
   ULONG guest_r15;
   ULONG guest_r16;
   ULONG guest_r17;
   ULONG guest_r18;
   ULONG guest_r19;
   ULONG guest_r20;
   ULONG guest_r21;
   ULONG guest_r22;
   ULONG guest_r23;
   ULONG guest_r24;
   ULONG guest_r25;
   ULONG guest_r26;
   ULONG guest_r27;
   ULONG guest_r28;
   ULONG guest_r29;
   ULONG guest_r30;
   ULONG guest_r31;
   ULONG guest_r32;
   ULONG guest_r33;
   ULONG guest_r34;
   ULONG guest_r35;
   ULONG guest_r36;
   ULONG guest_r37;
   ULONG guest_r38;
   ULONG guest_r39;
   ULONG guest_r40;
   ULONG guest_r41;
   ULONG guest_r42;
   ULONG guest_r43;
   ULONG guest_r44;
   ULONG guest_r45;
   ULONG guest_r46;
   ULONG guest_r47;
   ULONG guest_r48;
   ULONG guest_r49;
   ULONG guest_r50;
   ULONG guest_r51;
   ULONG guest_r52; 
   ULONG guest_r53;
   ULONG guest_r54; 
   ULONG guest_r55; 
   ULONG guest_r56; 
   ULONG guest_r57; 
   ULONG guest_r58; 
   ULONG guest_r59; 
   ULONG guest_r60; 
   ULONG guest_r61; 
   ULONG guest_r62; 
   ULONG guest_r63; 
   ULONG guest_pc;
   ULONG guest_spare; 
   ULONG guest_EMNOTE;
   ULONG guest_CMSTART;
   ULONG guest_CMLEN;
   ULONG guest_NRADDR;
   ULong guest_cmpexch;
   ULong guest_zero;
   ULong guest_ex_context_0;
   ULong guest_ex_context_1;
   ULong host_EvC_FAILADDR;
   ULong host_EvC_COUNTER;
   ULong guest_COND;
   ULong PAD;

} VexGuestTILEGXState;

#define OFFSET_tilegx_r(_N)  (8 * (_N))




extern
void LibVEX_GuestTILEGX_initialise ( VexGuestTILEGXState* vex_state );


#endif 


