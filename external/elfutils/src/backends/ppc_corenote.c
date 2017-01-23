/* PowerPC specific core note handling.
   Copyright (C) 2007, 2008 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <elf.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/time.h>

#ifndef BITS
# define BITS 		32
# define BACKEND	ppc_
#else
# define BITS 		64
# define BACKEND	ppc64_
#endif
#include "libebl_CPU.h"

static const Ebl_Register_Location prstatus_regs[] =
  {
#define GR(at, n, dwreg)						\
    { .offset = at * BITS/8, .regno = dwreg, .count = n, .bits = BITS }

    GR (0, 32, 0),		
    
    GR (33, 1, 66),		
    
    GR (35, 1, 109),		
    GR (36, 1, 108),		
    GR (37, 1, 101),		
    GR (38, 1, 64),		
    GR (39, 1, 100),		
    
    GR (41, 1, 119),		
    GR (42, 1, 118),		

#undef	GR
  };
#define PRSTATUS_REGS_SIZE	(BITS / 8 * 48)

static const Ebl_Register_Location fpregset_regs[] =
  {
    { .offset = 0, .regno = 32, .count = 32, .bits = 64 }, 
    { .offset = 32 * 8 + 4, .regno = 65, .count = 1, .bits = 32 } 
  };
#define FPREGSET_SIZE		(33 * 8)

static const Ebl_Register_Location altivec_regs[] =
  {
    
    { .offset = 0, .regno = 1124, .count = 32, .bits = 128 },
    
    { .offset = 32 * 16, .regno = 67, .count = 1, .bits = 32, .pad = 12 },
    
    { .offset = 33 * 16, .regno = 356, .count = 1, .bits = 32, .pad = 12 }
  };

static const Ebl_Register_Location spe_regs[] =
  {
    
    { .offset = 34 * 4, .regno = 612, .count = 1, .bits = 32 }
  };

#define EXTRA_NOTES \
  EXTRA_REGSET (NT_PPC_VMX, 34 * 16, altivec_regs) \
  EXTRA_REGSET (NT_PPC_SPE, 35 * 4, spe_regs)

#if BITS == 32
# define ULONG			uint32_t
# define ALIGN_ULONG		4
# define TYPE_ULONG		ELF_T_WORD
# define TYPE_LONG		ELF_T_SWORD
#else
# define ULONG			uint64_t
# define ALIGN_ULONG		8
# define TYPE_ULONG		ELF_T_XWORD
# define TYPE_LONG		ELF_T_SXWORD
#endif
#define PID_T			int32_t
#define	UID_T			uint32_t
#define	GID_T			uint32_t
#define ALIGN_PID_T		4
#define ALIGN_UID_T		4
#define ALIGN_GID_T		4
#define TYPE_PID_T		ELF_T_SWORD
#define TYPE_UID_T		ELF_T_WORD
#define TYPE_GID_T		ELF_T_WORD

#define PRSTATUS_REGSET_ITEMS						      \
  {									      \
    .name = "nip", .type = ELF_T_ADDR, .format = 'x',			      \
    .offset = offsetof (struct EBLHOOK(prstatus), pr_reg[32]),		      \
    .group = "register", .pc_register = true				      \
  },								      	      \
  {									      \
    .name = "orig_gpr3", .type = TYPE_LONG, .format = 'd',		      \
    .offset = offsetof (struct EBLHOOK(prstatus), pr_reg[34]),		      \
    .group = "register"	       			  	       	 	      \
  }

#include "linux-core-note.c"
