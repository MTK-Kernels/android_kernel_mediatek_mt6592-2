

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

#ifndef __PUB_CORE_MACHINE_H
#define __PUB_CORE_MACHINE_H


#include "pub_tool_machine.h"
#include "pub_core_basics.h"      

#if defined(VGP_x86_linux)
#  define VG_ELF_DATA2XXX     ELFDATA2LSB
#  define VG_ELF_MACHINE      EM_386
#  define VG_ELF_CLASS        ELFCLASS32
#  undef  VG_PLAT_USES_PPCTOC
#elif defined(VGP_amd64_linux)
#  define VG_ELF_DATA2XXX     ELFDATA2LSB
#  define VG_ELF_MACHINE      EM_X86_64
#  define VG_ELF_CLASS        ELFCLASS64
#  undef  VG_PLAT_USES_PPCTOC
#elif defined(VGP_ppc32_linux)
#  define VG_ELF_DATA2XXX     ELFDATA2MSB
#  define VG_ELF_MACHINE      EM_PPC
#  define VG_ELF_CLASS        ELFCLASS32
#  undef  VG_PLAT_USES_PPCTOC
#elif defined(VGP_ppc64be_linux)
#  define VG_ELF_DATA2XXX     ELFDATA2MSB
#  define VG_ELF_MACHINE      EM_PPC64
#  define VG_ELF_CLASS        ELFCLASS64
#  define VG_PLAT_USES_PPCTOC 1
#elif defined(VGP_ppc64le_linux)
#  define VG_ELF_DATA2XXX     ELFDATA2LSB
#  define VG_ELF_MACHINE      EM_PPC64
#  define VG_ELF_CLASS        ELFCLASS64
#  undef VG_PLAT_USES_PPCTOC
#elif defined(VGP_arm_linux)
#  define VG_ELF_DATA2XXX     ELFDATA2LSB
#  define VG_ELF_MACHINE      EM_ARM
#  define VG_ELF_CLASS        ELFCLASS32
#  undef  VG_PLAT_USES_PPCTOC
#elif defined(VGP_arm64_linux)
#  define VG_ELF_DATA2XXX     ELFDATA2LSB
#  define VG_ELF_MACHINE      EM_AARCH64
#  define VG_ELF_CLASS        ELFCLASS64
#  undef  VG_PLAT_USES_PPCTOC
#elif defined(VGO_darwin)
#  undef  VG_ELF_DATA2XXX
#  undef  VG_ELF_MACHINE
#  undef  VG_ELF_CLASS
#  undef  VG_PLAT_USES_PPCTOC
#elif defined(VGP_s390x_linux)
#  define VG_ELF_DATA2XXX     ELFDATA2MSB
#  define VG_ELF_MACHINE      EM_S390
#  define VG_ELF_CLASS        ELFCLASS64
#  undef  VG_PLAT_USES_PPCTOC
#elif defined(VGP_mips32_linux)
#  if defined (VG_LITTLEENDIAN)
#    define VG_ELF_DATA2XXX   ELFDATA2LSB
#  elif defined (VG_BIGENDIAN)
#    define VG_ELF_DATA2XXX   ELFDATA2MSB
#  else
#    error "Unknown endianness"
#  endif
#  define VG_ELF_MACHINE      EM_MIPS
#  define VG_ELF_CLASS        ELFCLASS32
#  undef  VG_PLAT_USES_PPCTOC
#elif defined(VGP_mips64_linux)
#  if defined (VG_LITTLEENDIAN)
#    define VG_ELF_DATA2XXX     ELFDATA2LSB
#  elif defined (VG_BIGENDIAN)
#    define VG_ELF_DATA2XXX     ELFDATA2MSB
#  else
#    error "Unknown endianness"
#  endif
#  define VG_ELF_MACHINE      EM_MIPS
#  define VG_ELF_CLASS        ELFCLASS64
#  undef  VG_PLAT_USES_PPCTOC
#elif defined(VGP_tilegx_linux)
#  define VG_ELF_DATA2XXX     ELFDATA2LSB
   #ifndef EM_TILEGX
   #define EM_TILEGX 191
   #endif
#  define VG_ELF_MACHINE      EM_TILEGX
#  define VG_ELF_CLASS        ELFCLASS64
#  undef  VG_PLAT_USES_PPCTOC
#else
#  error Unknown platform
#endif

#if defined(VGA_x86)
#  define VG_INSTR_PTR        guest_EIP
#  define VG_STACK_PTR        guest_ESP
#  define VG_FRAME_PTR        guest_EBP
#elif defined(VGA_amd64)
#  define VG_INSTR_PTR        guest_RIP
#  define VG_STACK_PTR        guest_RSP
#  define VG_FRAME_PTR        guest_RBP
#elif defined(VGA_ppc32)
#  define VG_INSTR_PTR        guest_CIA
#  define VG_STACK_PTR        guest_GPR1
#  define VG_FRAME_PTR        guest_GPR1   
#elif defined(VGA_ppc64be) || defined(VGA_ppc64le)
#  define VG_INSTR_PTR        guest_CIA
#  define VG_STACK_PTR        guest_GPR1
#  define VG_FRAME_PTR        guest_GPR1   
#elif defined(VGA_arm)
#  define VG_INSTR_PTR        guest_R15T
#  define VG_STACK_PTR        guest_R13
#  define VG_FRAME_PTR        guest_R11
#elif defined(VGA_arm64)
#  define VG_INSTR_PTR        guest_PC
#  define VG_STACK_PTR        guest_XSP
#  define VG_FRAME_PTR        guest_X29   
#elif defined(VGA_s390x)
#  define VG_INSTR_PTR        guest_IA
#  define VG_STACK_PTR        guest_SP
#  define VG_FRAME_PTR        guest_FP
#  define VG_FPC_REG          guest_fpc
#elif defined(VGA_mips32)
#  define VG_INSTR_PTR        guest_PC
#  define VG_STACK_PTR        guest_r29
#  define VG_FRAME_PTR        guest_r30
#elif defined(VGA_mips64)
#  define VG_INSTR_PTR        guest_PC
#  define VG_STACK_PTR        guest_r29
#  define VG_FRAME_PTR        guest_r30
#elif defined(VGA_tilegx)
#  define VG_INSTR_PTR        guest_pc
#  define VG_STACK_PTR        guest_r54
#  define VG_FRAME_PTR        guest_r52
#else
#  error Unknown arch
#endif


#define VG_O_STACK_PTR        (offsetof(VexGuestArchState, VG_STACK_PTR))
#define VG_O_INSTR_PTR        (offsetof(VexGuestArchState, VG_INSTR_PTR))
#define VG_O_FPC_REG          (offsetof(VexGuestArchState, VG_FPC_REG))



Addr VG_(get_FP) ( ThreadId tid );

void VG_(set_IP) ( ThreadId tid, Addr encip );
void VG_(set_SP) ( ThreadId tid, Addr sp );


void VG_(get_UnwindStartRegs) ( UnwindStartRegs* regs,
                                ThreadId tid );



extern Bool VG_(machine_get_hwcaps)( void );

extern Bool VG_(machine_get_cache_info)( VexArchInfo * );

#if defined(VGA_ppc32)
extern void VG_(machine_ppc32_set_clszB)( Int );
#endif

#if defined(VGA_ppc64be) || defined(VGA_ppc64le)
extern void VG_(machine_ppc64_set_clszB)( Int );
#endif

#if defined(VGA_arm)
extern void VG_(machine_arm_set_has_NEON)( Bool );
#endif

#if defined(VGA_x86)
extern UInt VG_(machine_x86_have_mxcsr);
#endif

#if defined(VGA_ppc32)
extern UInt VG_(machine_ppc32_has_FP);
#endif

#if defined(VGA_ppc32)
extern UInt VG_(machine_ppc32_has_VMX);
#endif

#if defined(VGA_ppc64be) || defined(VGA_ppc64le)
extern ULong VG_(machine_ppc64_has_VMX);
#endif

#if defined(VGA_arm)
extern Int VG_(machine_arm_archlevel);
#endif

#endif   

