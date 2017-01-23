

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

#ifndef __LIBVEX_PUB_S390X_H
#define __LIBVEX_PUB_S390X_H




#define S390_REGNO_RETURN_VALUE         2
#define S390_REGNO_TCHAIN_SCRATCH      12
#define S390_REGNO_GUEST_STATE_POINTER 13
#define S390_REGNO_LINK_REGISTER       14
#define S390_REGNO_STACK_POINTER       15




#define S390_OFFSET_SAVED_R2 160+80

#define S390_OFFSET_SAVED_FPC_C 160+72

#define S390_OFFSET_SAVED_FPC_V 160+64

#define S390_INNERLOOP_FRAME_SIZE ((8+1+2)*8 + 160)



#define S390_FAC_MSA     17  
#define S390_FAC_LDISP   18  
#define S390_FAC_HFPMAS  20  
#define S390_FAC_EIMM    21  
#define S390_FAC_HFPUNX  23  
#define S390_FAC_ETF2    24  
#define S390_FAC_STCKF   25  
#define S390_FAC_PENH    26  
#define S390_FAC_ETF3    30  
#define S390_FAC_XCPUT   31  
#define S390_FAC_GIE     34  
#define S390_FAC_EXEXT   35  
#define S390_FAC_FPEXT   37  
#define S390_FAC_FPSE    41  
#define S390_FAC_DFP     42  
#define S390_FAC_PFPO    44  
#define S390_FAC_HIGHW   45  
#define S390_FAC_LSC     45  
#define S390_FAC_DFPZC   48  
#define S390_FAC_MISC    49  
#define S390_FAC_CTREXE  50  
#define S390_FAC_TREXE   73  
#define S390_FAC_MSA4    77  



#define S390_NUM_GPRPARMS 5

#define S390_NUM_FACILITY_DW 2

#endif 

