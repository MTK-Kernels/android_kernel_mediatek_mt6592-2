/* Definitions of interface to the "low" (arch specific) functions
   needed for interfacing the Valgrind gdbserver with the Valgrind
   guest.

   Copyright (C) 2011, 2012
   Free Software Foundation, Inc.

   This file has been inspired from a file that is part of GDB.
   It has been modified to integrate it in valgrind

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

#ifndef VALGRIND_LOW_H
#define VALGRIND_LOW_H

#include "pub_core_basics.h"    
#include "server.h"             

struct valgrind_target_ops
{
   int num_regs;
   struct reg *reg_defs;

   int stack_pointer_regno;
   
   
   void (*transfer_register) (ThreadId tid, int regno, void * buf, 
                              transfer_direction dir, int size, Bool *mod);
   

   CORE_ADDR (*get_pc) (void);
   void (*set_pc) (CORE_ADDR newpc);

   const char *arch_string;
   
   const char* (*target_xml) (Bool shadow_mode);

   CORE_ADDR** (*target_get_dtv)(ThreadState *tst);

};

extern void x86_init_architecture (struct valgrind_target_ops *target);
extern void amd64_init_architecture (struct valgrind_target_ops *target);
extern void arm_init_architecture (struct valgrind_target_ops *target);
extern void arm64_init_architecture (struct valgrind_target_ops *target);
extern void ppc32_init_architecture (struct valgrind_target_ops *target);
extern void ppc64_init_architecture (struct valgrind_target_ops *target);
extern void s390x_init_architecture (struct valgrind_target_ops *target);
extern void mips32_init_architecture (struct valgrind_target_ops *target);
extern void mips64_init_architecture (struct valgrind_target_ops *target);
extern void tilegx_init_architecture (struct valgrind_target_ops *target);

#endif
