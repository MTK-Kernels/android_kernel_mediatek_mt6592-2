

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2000-2013 Julian Seward
      jseward@acm.org

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

#ifndef __PUB_CORE_BASICS_H
#define __PUB_CORE_BASICS_H


#include "pub_tool_basics.h"




typedef
   struct {
      ULong r_pc; 
      ULong r_sp; 
      union {
         struct {
            UInt r_ebp;
         } X86;
         struct {
            ULong r_rbp;
         } AMD64;
         struct {
            UInt r_lr;
         } PPC32;
         struct {
            ULong r_lr;
         } PPC64;
         struct {
            UInt r14;
            UInt r12;
            UInt r11;
            UInt r7;
         } ARM;
         struct {
            ULong x29; 
            ULong x30; 
         } ARM64;
         struct {
            ULong r_fp;
            ULong r_lr;
         } S390X;
         struct {
            UInt r30;  
            UInt r31;  
            UInt r28;
         } MIPS32;
         struct {
            ULong r30;  
            ULong r31;  
            ULong r28;
         } MIPS64;
         struct {
            ULong r52;
            ULong r55;
         } TILEGX;
      } misc;
   }
   UnwindStartRegs;


#endif   

