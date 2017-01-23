
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

#include "pub_core_basics.h"
#include "pub_core_vki.h"
#include "pub_core_threadstate.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcbase.h"
#include "pub_core_libcfile.h"
#include "pub_core_libcprint.h"
#include "pub_core_mallocfree.h"
#include "pub_core_machine.h"
#include "pub_core_cpuid.h"
#include "pub_core_libcsignal.h"   
#include "pub_core_debuglog.h"


#define INSTR_PTR(regs)    ((regs).vex.VG_INSTR_PTR)
#define STACK_PTR(regs)    ((regs).vex.VG_STACK_PTR)
#define FRAME_PTR(regs)    ((regs).vex.VG_FRAME_PTR)

Addr VG_(get_IP) ( ThreadId tid ) {
   return INSTR_PTR( VG_(threads)[tid].arch );
}
Addr VG_(get_SP) ( ThreadId tid ) {
   return STACK_PTR( VG_(threads)[tid].arch );
}
Addr VG_(get_FP) ( ThreadId tid ) {
   return FRAME_PTR( VG_(threads)[tid].arch );
}

void VG_(set_IP) ( ThreadId tid, Addr ip ) {
   INSTR_PTR( VG_(threads)[tid].arch ) = ip;
}
void VG_(set_SP) ( ThreadId tid, Addr sp ) {
   STACK_PTR( VG_(threads)[tid].arch ) = sp;
}

void VG_(get_UnwindStartRegs) ( UnwindStartRegs* regs,
                                ThreadId tid )
{
#  if defined(VGA_x86)
   regs->r_pc = (ULong)VG_(threads)[tid].arch.vex.guest_EIP;
   regs->r_sp = (ULong)VG_(threads)[tid].arch.vex.guest_ESP;
   regs->misc.X86.r_ebp
      = VG_(threads)[tid].arch.vex.guest_EBP;
#  elif defined(VGA_amd64)
   regs->r_pc = VG_(threads)[tid].arch.vex.guest_RIP;
   regs->r_sp = VG_(threads)[tid].arch.vex.guest_RSP;
   regs->misc.AMD64.r_rbp
      = VG_(threads)[tid].arch.vex.guest_RBP;
#  elif defined(VGA_ppc32)
   regs->r_pc = (ULong)VG_(threads)[tid].arch.vex.guest_CIA;
   regs->r_sp = (ULong)VG_(threads)[tid].arch.vex.guest_GPR1;
   regs->misc.PPC32.r_lr
      = VG_(threads)[tid].arch.vex.guest_LR;
#  elif defined(VGA_ppc64be) || defined(VGA_ppc64le)
   regs->r_pc = VG_(threads)[tid].arch.vex.guest_CIA;
   regs->r_sp = VG_(threads)[tid].arch.vex.guest_GPR1;
   regs->misc.PPC64.r_lr
      = VG_(threads)[tid].arch.vex.guest_LR;
#  elif defined(VGA_arm)
   regs->r_pc = (ULong)VG_(threads)[tid].arch.vex.guest_R15T;
   regs->r_sp = (ULong)VG_(threads)[tid].arch.vex.guest_R13;
   regs->misc.ARM.r14
      = VG_(threads)[tid].arch.vex.guest_R14;
   regs->misc.ARM.r12
      = VG_(threads)[tid].arch.vex.guest_R12;
   regs->misc.ARM.r11
      = VG_(threads)[tid].arch.vex.guest_R11;
   regs->misc.ARM.r7
      = VG_(threads)[tid].arch.vex.guest_R7;
#  elif defined(VGA_arm64)
   regs->r_pc = VG_(threads)[tid].arch.vex.guest_PC;
   regs->r_sp = VG_(threads)[tid].arch.vex.guest_XSP;
   regs->misc.ARM64.x29 = VG_(threads)[tid].arch.vex.guest_X29;
   regs->misc.ARM64.x30 = VG_(threads)[tid].arch.vex.guest_X30;
#  elif defined(VGA_s390x)
   regs->r_pc = (ULong)VG_(threads)[tid].arch.vex.guest_IA;
   regs->r_sp = (ULong)VG_(threads)[tid].arch.vex.guest_SP;
   regs->misc.S390X.r_fp
      = VG_(threads)[tid].arch.vex.guest_FP;
   regs->misc.S390X.r_lr
      = VG_(threads)[tid].arch.vex.guest_LR;
#  elif defined(VGA_mips32)
   regs->r_pc = VG_(threads)[tid].arch.vex.guest_PC;
   regs->r_sp = VG_(threads)[tid].arch.vex.guest_r29;
   regs->misc.MIPS32.r30
      = VG_(threads)[tid].arch.vex.guest_r30;
   regs->misc.MIPS32.r31
      = VG_(threads)[tid].arch.vex.guest_r31;
   regs->misc.MIPS32.r28
      = VG_(threads)[tid].arch.vex.guest_r28;
#  elif defined(VGA_mips64)
   regs->r_pc = VG_(threads)[tid].arch.vex.guest_PC;
   regs->r_sp = VG_(threads)[tid].arch.vex.guest_r29;
   regs->misc.MIPS64.r30
      = VG_(threads)[tid].arch.vex.guest_r30;
   regs->misc.MIPS64.r31
      = VG_(threads)[tid].arch.vex.guest_r31;
   regs->misc.MIPS64.r28
      = VG_(threads)[tid].arch.vex.guest_r28;
#  elif defined(VGA_tilegx)
   regs->r_pc = VG_(threads)[tid].arch.vex.guest_pc;
   regs->r_sp = VG_(threads)[tid].arch.vex.guest_r54;
   regs->misc.TILEGX.r52
      = VG_(threads)[tid].arch.vex.guest_r52;
   regs->misc.TILEGX.r55
      = VG_(threads)[tid].arch.vex.guest_r55;
#  else
#    error "Unknown arch"
#  endif
}

void
VG_(get_shadow_regs_area) ( ThreadId tid, 
                            UChar* dst,
                            Int shadowNo, PtrdiffT offset, SizeT size )
{
   void*        src;
   ThreadState* tst;
   vg_assert(shadowNo == 0 || shadowNo == 1 || shadowNo == 2);
   vg_assert(VG_(is_valid_tid)(tid));
   
   vg_assert(0 <= offset && offset < sizeof(VexGuestArchState));
   vg_assert(offset + size <= sizeof(VexGuestArchState));
   
   tst = & VG_(threads)[tid];
   src = NULL;
   switch (shadowNo) {
      case 0: src = (void*)(((Addr)&(tst->arch.vex)) + offset); break;
      case 1: src = (void*)(((Addr)&(tst->arch.vex_shadow1)) + offset); break;
      case 2: src = (void*)(((Addr)&(tst->arch.vex_shadow2)) + offset); break;
   }
   vg_assert(src != NULL);
   VG_(memcpy)( dst, src, size);
}

void
VG_(set_shadow_regs_area) ( ThreadId tid, 
                            Int shadowNo, PtrdiffT offset, SizeT size,
                            const UChar* src )
{
   void*        dst;
   ThreadState* tst;
   vg_assert(shadowNo == 0 || shadowNo == 1 || shadowNo == 2);
   vg_assert(VG_(is_valid_tid)(tid));
   
   vg_assert(0 <= offset && offset < sizeof(VexGuestArchState));
   vg_assert(offset + size <= sizeof(VexGuestArchState));
   
   tst = & VG_(threads)[tid];
   dst = NULL;
   switch (shadowNo) {
      case 0: dst = (void*)(((Addr)&(tst->arch.vex)) + offset); break;
      case 1: dst = (void*)(((Addr)&(tst->arch.vex_shadow1)) + offset); break;
      case 2: dst = (void*)(((Addr)&(tst->arch.vex_shadow2)) + offset); break;
   }
   vg_assert(dst != NULL);
   VG_(memcpy)( dst, src, size);
}


static void apply_to_GPs_of_tid(ThreadId tid, void (*f)(ThreadId,
                                                        const HChar*, Addr))
{
   VexGuestArchState* vex = &(VG_(get_ThreadState)(tid)->arch.vex);
   VG_(debugLog)(2, "machine", "apply_to_GPs_of_tid %d\n", tid);
#if defined(VGA_x86)
   (*f)(tid, "EAX", vex->guest_EAX);
   (*f)(tid, "ECX", vex->guest_ECX);
   (*f)(tid, "EDX", vex->guest_EDX);
   (*f)(tid, "EBX", vex->guest_EBX);
   (*f)(tid, "ESI", vex->guest_ESI);
   (*f)(tid, "EDI", vex->guest_EDI);
   (*f)(tid, "ESP", vex->guest_ESP);
   (*f)(tid, "EBP", vex->guest_EBP);
#elif defined(VGA_amd64)
   (*f)(tid, "RAX", vex->guest_RAX);
   (*f)(tid, "RCX", vex->guest_RCX);
   (*f)(tid, "RDX", vex->guest_RDX);
   (*f)(tid, "RBX", vex->guest_RBX);
   (*f)(tid, "RSI", vex->guest_RSI);
   (*f)(tid, "RDI", vex->guest_RDI);
   (*f)(tid, "RSP", vex->guest_RSP);
   (*f)(tid, "RBP", vex->guest_RBP);
   (*f)(tid, "R8" , vex->guest_R8 );
   (*f)(tid, "R9" , vex->guest_R9 );
   (*f)(tid, "R10", vex->guest_R10);
   (*f)(tid, "R11", vex->guest_R11);
   (*f)(tid, "R12", vex->guest_R12);
   (*f)(tid, "R13", vex->guest_R13);
   (*f)(tid, "R14", vex->guest_R14);
   (*f)(tid, "R15", vex->guest_R15);
#elif defined(VGA_ppc32) || defined(VGA_ppc64be) || defined(VGA_ppc64le)
   (*f)(tid, "GPR0" , vex->guest_GPR0 );
   (*f)(tid, "GPR1" , vex->guest_GPR1 );
   (*f)(tid, "GPR2" , vex->guest_GPR2 );
   (*f)(tid, "GPR3" , vex->guest_GPR3 );
   (*f)(tid, "GPR4" , vex->guest_GPR4 );
   (*f)(tid, "GPR5" , vex->guest_GPR5 );
   (*f)(tid, "GPR6" , vex->guest_GPR6 );
   (*f)(tid, "GPR7" , vex->guest_GPR7 );
   (*f)(tid, "GPR8" , vex->guest_GPR8 );
   (*f)(tid, "GPR9" , vex->guest_GPR9 );
   (*f)(tid, "GPR10", vex->guest_GPR10);
   (*f)(tid, "GPR11", vex->guest_GPR11);
   (*f)(tid, "GPR12", vex->guest_GPR12);
   (*f)(tid, "GPR13", vex->guest_GPR13);
   (*f)(tid, "GPR14", vex->guest_GPR14);
   (*f)(tid, "GPR15", vex->guest_GPR15);
   (*f)(tid, "GPR16", vex->guest_GPR16);
   (*f)(tid, "GPR17", vex->guest_GPR17);
   (*f)(tid, "GPR18", vex->guest_GPR18);
   (*f)(tid, "GPR19", vex->guest_GPR19);
   (*f)(tid, "GPR20", vex->guest_GPR20);
   (*f)(tid, "GPR21", vex->guest_GPR21);
   (*f)(tid, "GPR22", vex->guest_GPR22);
   (*f)(tid, "GPR23", vex->guest_GPR23);
   (*f)(tid, "GPR24", vex->guest_GPR24);
   (*f)(tid, "GPR25", vex->guest_GPR25);
   (*f)(tid, "GPR26", vex->guest_GPR26);
   (*f)(tid, "GPR27", vex->guest_GPR27);
   (*f)(tid, "GPR28", vex->guest_GPR28);
   (*f)(tid, "GPR29", vex->guest_GPR29);
   (*f)(tid, "GPR30", vex->guest_GPR30);
   (*f)(tid, "GPR31", vex->guest_GPR31);
   (*f)(tid, "CTR"  , vex->guest_CTR  );
   (*f)(tid, "LR"   , vex->guest_LR   );
#elif defined(VGA_arm)
   (*f)(tid, "R0" , vex->guest_R0 );
   (*f)(tid, "R1" , vex->guest_R1 );
   (*f)(tid, "R2" , vex->guest_R2 );
   (*f)(tid, "R3" , vex->guest_R3 );
   (*f)(tid, "R4" , vex->guest_R4 );
   (*f)(tid, "R5" , vex->guest_R5 );
   (*f)(tid, "R6" , vex->guest_R6 );
   (*f)(tid, "R8" , vex->guest_R8 );
   (*f)(tid, "R9" , vex->guest_R9 );
   (*f)(tid, "R10", vex->guest_R10);
   (*f)(tid, "R11", vex->guest_R11);
   (*f)(tid, "R12", vex->guest_R12);
   (*f)(tid, "R13", vex->guest_R13);
   (*f)(tid, "R14", vex->guest_R14);
#elif defined(VGA_s390x)
   (*f)(tid, "r0" , vex->guest_r0 );
   (*f)(tid, "r1" , vex->guest_r1 );
   (*f)(tid, "r2" , vex->guest_r2 );
   (*f)(tid, "r3" , vex->guest_r3 );
   (*f)(tid, "r4" , vex->guest_r4 );
   (*f)(tid, "r5" , vex->guest_r5 );
   (*f)(tid, "r6" , vex->guest_r6 );
   (*f)(tid, "r7" , vex->guest_r7 );
   (*f)(tid, "r8" , vex->guest_r8 );
   (*f)(tid, "r9" , vex->guest_r9 );
   (*f)(tid, "r10", vex->guest_r10);
   (*f)(tid, "r11", vex->guest_r11);
   (*f)(tid, "r12", vex->guest_r12);
   (*f)(tid, "r13", vex->guest_r13);
   (*f)(tid, "r14", vex->guest_r14);
   (*f)(tid, "r15", vex->guest_r15);
#elif defined(VGA_mips32) || defined(VGA_mips64)
   (*f)(tid, "r0" , vex->guest_r0 );
   (*f)(tid, "r1" , vex->guest_r1 );
   (*f)(tid, "r2" , vex->guest_r2 );
   (*f)(tid, "r3" , vex->guest_r3 );
   (*f)(tid, "r4" , vex->guest_r4 );
   (*f)(tid, "r5" , vex->guest_r5 );
   (*f)(tid, "r6" , vex->guest_r6 );
   (*f)(tid, "r7" , vex->guest_r7 );
   (*f)(tid, "r8" , vex->guest_r8 );
   (*f)(tid, "r9" , vex->guest_r9 );
   (*f)(tid, "r10", vex->guest_r10);
   (*f)(tid, "r11", vex->guest_r11);
   (*f)(tid, "r12", vex->guest_r12);
   (*f)(tid, "r13", vex->guest_r13);
   (*f)(tid, "r14", vex->guest_r14);
   (*f)(tid, "r15", vex->guest_r15);
   (*f)(tid, "r16", vex->guest_r16);
   (*f)(tid, "r17", vex->guest_r17);
   (*f)(tid, "r18", vex->guest_r18);
   (*f)(tid, "r19", vex->guest_r19);
   (*f)(tid, "r20", vex->guest_r20);
   (*f)(tid, "r21", vex->guest_r21);
   (*f)(tid, "r22", vex->guest_r22);
   (*f)(tid, "r23", vex->guest_r23);
   (*f)(tid, "r24", vex->guest_r24);
   (*f)(tid, "r25", vex->guest_r25);
   (*f)(tid, "r26", vex->guest_r26);
   (*f)(tid, "r27", vex->guest_r27);
   (*f)(tid, "r28", vex->guest_r28);
   (*f)(tid, "r29", vex->guest_r29);
   (*f)(tid, "r30", vex->guest_r30);
   (*f)(tid, "r31", vex->guest_r31);
#elif defined(VGA_arm64)
   (*f)(tid, "x0" , vex->guest_X0 );
   (*f)(tid, "x1" , vex->guest_X1 );
   (*f)(tid, "x2" , vex->guest_X2 );
   (*f)(tid, "x3" , vex->guest_X3 );
   (*f)(tid, "x4" , vex->guest_X4 );
   (*f)(tid, "x5" , vex->guest_X5 );
   (*f)(tid, "x6" , vex->guest_X6 );
   (*f)(tid, "x7" , vex->guest_X7 );
   (*f)(tid, "x8" , vex->guest_X8 );
   (*f)(tid, "x9" , vex->guest_X9 );
   (*f)(tid, "x10", vex->guest_X10);
   (*f)(tid, "x11", vex->guest_X11);
   (*f)(tid, "x12", vex->guest_X12);
   (*f)(tid, "x13", vex->guest_X13);
   (*f)(tid, "x14", vex->guest_X14);
   (*f)(tid, "x15", vex->guest_X15);
   (*f)(tid, "x16", vex->guest_X16);
   (*f)(tid, "x17", vex->guest_X17);
   (*f)(tid, "x18", vex->guest_X18);
   (*f)(tid, "x19", vex->guest_X19);
   (*f)(tid, "x20", vex->guest_X20);
   (*f)(tid, "x21", vex->guest_X21);
   (*f)(tid, "x22", vex->guest_X22);
   (*f)(tid, "x23", vex->guest_X23);
   (*f)(tid, "x24", vex->guest_X24);
   (*f)(tid, "x25", vex->guest_X25);
   (*f)(tid, "x26", vex->guest_X26);
   (*f)(tid, "x27", vex->guest_X27);
   (*f)(tid, "x28", vex->guest_X28);
   (*f)(tid, "x29", vex->guest_X29);
   (*f)(tid, "x30", vex->guest_X30);
#elif defined(VGA_tilegx)
   (*f)(tid, "r0",  vex->guest_r0 );
   (*f)(tid, "r1",  vex->guest_r1 );
   (*f)(tid, "r2",  vex->guest_r2 );
   (*f)(tid, "r3",  vex->guest_r3 );
   (*f)(tid, "r4",  vex->guest_r4 );
   (*f)(tid, "r5",  vex->guest_r5 );
   (*f)(tid, "r6",  vex->guest_r6 );
   (*f)(tid, "r7",  vex->guest_r7 );
   (*f)(tid, "r8",  vex->guest_r8 );
   (*f)(tid, "r9",  vex->guest_r9 );
   (*f)(tid, "r10", vex->guest_r10);
   (*f)(tid, "r11", vex->guest_r11);
   (*f)(tid, "r12", vex->guest_r12);
   (*f)(tid, "r13", vex->guest_r13);
   (*f)(tid, "r14", vex->guest_r14);
   (*f)(tid, "r15", vex->guest_r15);
   (*f)(tid, "r16", vex->guest_r16);
   (*f)(tid, "r17", vex->guest_r17);
   (*f)(tid, "r18", vex->guest_r18);
   (*f)(tid, "r19", vex->guest_r19);
   (*f)(tid, "r20", vex->guest_r20);
   (*f)(tid, "r21", vex->guest_r21);
   (*f)(tid, "r22", vex->guest_r22);
   (*f)(tid, "r23", vex->guest_r23);
   (*f)(tid, "r24", vex->guest_r24);
   (*f)(tid, "r25", vex->guest_r25);
   (*f)(tid, "r26", vex->guest_r26);
   (*f)(tid, "r27", vex->guest_r27);
   (*f)(tid, "r28", vex->guest_r28);
   (*f)(tid, "r29", vex->guest_r29);
   (*f)(tid, "r30", vex->guest_r30);
   (*f)(tid, "r31", vex->guest_r31);
   (*f)(tid, "r32", vex->guest_r32);
   (*f)(tid, "r33", vex->guest_r33);
   (*f)(tid, "r34", vex->guest_r34);
   (*f)(tid, "r35", vex->guest_r35);
   (*f)(tid, "r36", vex->guest_r36);
   (*f)(tid, "r37", vex->guest_r37);
   (*f)(tid, "r38", vex->guest_r38);
   (*f)(tid, "r39", vex->guest_r39);
   (*f)(tid, "r40", vex->guest_r40);
   (*f)(tid, "r41", vex->guest_r41);
   (*f)(tid, "r42", vex->guest_r42);
   (*f)(tid, "r43", vex->guest_r43);
   (*f)(tid, "r44", vex->guest_r44);
   (*f)(tid, "r45", vex->guest_r45);
   (*f)(tid, "r46", vex->guest_r46);
   (*f)(tid, "r47", vex->guest_r47);
   (*f)(tid, "r48", vex->guest_r48);
   (*f)(tid, "r49", vex->guest_r49);
   (*f)(tid, "r50", vex->guest_r50);
   (*f)(tid, "r51", vex->guest_r51);
   (*f)(tid, "r52", vex->guest_r52);
   (*f)(tid, "r53", vex->guest_r53);
   (*f)(tid, "r54", vex->guest_r54);
   (*f)(tid, "r55", vex->guest_r55);
#else
#  error Unknown arch
#endif
}


void VG_(apply_to_GP_regs)(void (*f)(ThreadId, const HChar*, UWord))
{
   ThreadId tid;

   for (tid = 1; tid < VG_N_THREADS; tid++) {
      if (VG_(is_valid_tid)(tid)
          || VG_(threads)[tid].exitreason == VgSrc_ExitProcess) {
         
         
         apply_to_GPs_of_tid(tid, f);
      }
   }
}

void VG_(thread_stack_reset_iter)(ThreadId* tid)
{
   *tid = (ThreadId)(-1);
}

Bool VG_(thread_stack_next)(ThreadId* tid,
                            Addr* stack_min, 
                            Addr* stack_max)
{
   ThreadId i;
   for (i = (*tid)+1; i < VG_N_THREADS; i++) {
      if (i == VG_INVALID_THREADID)
         continue;
      if (VG_(threads)[i].status != VgTs_Empty) {
         *tid       = i;
         *stack_min = VG_(get_SP)(i);
         *stack_max = VG_(threads)[i].client_stack_highest_byte;
         return True;
      }
   }
   return False;
}

Addr VG_(thread_get_stack_max)(ThreadId tid)
{
   vg_assert(0 <= tid && tid < VG_N_THREADS && tid != VG_INVALID_THREADID);
   vg_assert(VG_(threads)[tid].status != VgTs_Empty);
   return VG_(threads)[tid].client_stack_highest_byte;
}

SizeT VG_(thread_get_stack_size)(ThreadId tid)
{
   vg_assert(0 <= tid && tid < VG_N_THREADS && tid != VG_INVALID_THREADID);
   vg_assert(VG_(threads)[tid].status != VgTs_Empty);
   return VG_(threads)[tid].client_stack_szB;
}

Addr VG_(thread_get_altstack_min)(ThreadId tid)
{
   vg_assert(0 <= tid && tid < VG_N_THREADS && tid != VG_INVALID_THREADID);
   vg_assert(VG_(threads)[tid].status != VgTs_Empty);
   return (Addr)VG_(threads)[tid].altstack.ss_sp;
}

SizeT VG_(thread_get_altstack_size)(ThreadId tid)
{
   vg_assert(0 <= tid && tid < VG_N_THREADS && tid != VG_INVALID_THREADID);
   vg_assert(VG_(threads)[tid].status != VgTs_Empty);
   return VG_(threads)[tid].altstack.ss_size;
}


static Bool hwcaps_done = False;

static VexArch     va = VexArch_INVALID;
static VexArchInfo vai;

#if defined(VGA_x86)
UInt VG_(machine_x86_have_mxcsr) = 0;
#endif
#if defined(VGA_ppc32)
UInt VG_(machine_ppc32_has_FP)  = 0;
UInt VG_(machine_ppc32_has_VMX) = 0;
#endif
#if defined(VGA_ppc64be) || defined(VGA_ppc64le)
ULong VG_(machine_ppc64_has_VMX) = 0;
#endif
#if defined(VGA_arm)
Int VG_(machine_arm_archlevel) = 4;
#endif


#if defined(VGA_ppc32) || defined(VGA_ppc64be) || defined(VGA_ppc64le) \
    || defined(VGA_arm) || defined(VGA_s390x) || defined(VGA_mips32)
#include "pub_core_libcsetjmp.h"
static VG_MINIMAL_JMP_BUF(env_unsup_insn);
static void handler_unsup_insn ( Int x ) {
   VG_MINIMAL_LONGJMP(env_unsup_insn);
}
#endif


#if defined(VGA_ppc32) || defined(VGA_ppc64be) || defined(VGA_ppc64le)
static void find_ppc_dcbz_sz(VexArchInfo *arch_info)
{
   Int dcbz_szB = 0;
   Int dcbzl_szB;
#  define MAX_DCBZL_SZB (128) 
   char test_block[4*MAX_DCBZL_SZB];
   char *aligned = test_block;
   Int i;

   
   aligned = (char *)(((HWord)aligned + MAX_DCBZL_SZB) & ~(MAX_DCBZL_SZB - 1));
   vg_assert((aligned + MAX_DCBZL_SZB) <= &test_block[sizeof(test_block)]);

   VG_(memset)(test_block, 0xff, sizeof(test_block));
   __asm__ __volatile__("dcbz 0,%0"
                        : 
                        : "r" (aligned) 
                        : "memory" );
   for (dcbz_szB = 0, i = 0; i < sizeof(test_block); ++i) {
      if (!test_block[i])
         ++dcbz_szB;
   }
   vg_assert(dcbz_szB == 16 || dcbz_szB == 32 || dcbz_szB == 64 || dcbz_szB == 128);

   
   if (VG_MINIMAL_SETJMP(env_unsup_insn)) {
      dcbzl_szB = 0; 
   }
   else {
      VG_(memset)(test_block, 0xff, sizeof(test_block));
      __asm__ __volatile__("mr 9, %0 ; .long 0x7C204FEC" 
                           : 
                           : "r" (aligned) 
                           : "memory", "r9" );
      for (dcbzl_szB = 0, i = 0; i < sizeof(test_block); ++i) {
         if (!test_block[i])
            ++dcbzl_szB;
      }
      vg_assert(dcbzl_szB == 16 || dcbzl_szB == 32 || dcbzl_szB == 64 || dcbzl_szB == 128);
   }

   arch_info->ppc_dcbz_szB  = dcbz_szB;
   arch_info->ppc_dcbzl_szB = dcbzl_szB;

   VG_(debugLog)(1, "machine", "dcbz_szB=%d dcbzl_szB=%d\n",
                 dcbz_szB, dcbzl_szB);
#  undef MAX_DCBZL_SZB
}
#endif 

#ifdef VGA_s390x


static UInt VG_(get_machine_model)(void)
{
   static struct model_map {
      const HChar name[5];
      UInt  id;
   } model_map[] = {
      { "2064", VEX_S390X_MODEL_Z900 },
      { "2066", VEX_S390X_MODEL_Z800 },
      { "2084", VEX_S390X_MODEL_Z990 },
      { "2086", VEX_S390X_MODEL_Z890 },
      { "2094", VEX_S390X_MODEL_Z9_EC },
      { "2096", VEX_S390X_MODEL_Z9_BC },
      { "2097", VEX_S390X_MODEL_Z10_EC },
      { "2098", VEX_S390X_MODEL_Z10_BC },
      { "2817", VEX_S390X_MODEL_Z196 },
      { "2818", VEX_S390X_MODEL_Z114 },
      { "2827", VEX_S390X_MODEL_ZEC12 },
      { "2828", VEX_S390X_MODEL_ZBC12 },
      { "2964", VEX_S390X_MODEL_Z13 },
   };

   Int    model, n, fh;
   SysRes fd;
   SizeT  num_bytes, file_buf_size;
   HChar *p, *m, *model_name, *file_buf;

   
   fd = VG_(open)( "/proc/cpuinfo", 0, VKI_S_IRUSR );
   if ( sr_isError(fd) ) return VEX_S390X_MODEL_UNKNOWN;

   fh  = sr_Res(fd);

   num_bytes = 0;
   file_buf_size = 1000;
   file_buf = VG_(malloc)("cpuinfo", file_buf_size + 1);
   while (42) {
      n = VG_(read)(fh, file_buf, file_buf_size);
      if (n < 0) break;

      num_bytes += n;
      if (n < file_buf_size) break;  
   }

   if (n < 0) num_bytes = 0;   

   if (num_bytes > file_buf_size) {
      VG_(free)( file_buf );
      VG_(lseek)( fh, 0, VKI_SEEK_SET );
      file_buf = VG_(malloc)( "cpuinfo", num_bytes + 1 );
      n = VG_(read)( fh, file_buf, num_bytes );
      if (n < 0) num_bytes = 0;
   }

   file_buf[num_bytes] = '\0';
   VG_(close)(fh);

   
   model = VEX_S390X_MODEL_UNKNOWN;
   for (p = file_buf; *p; ++p) {
      
     if (VG_(strncmp)( p, "processor", sizeof "processor" - 1 ) != 0) continue;

     m = VG_(strstr)( p, "machine" );
     if (m == NULL) continue;

     p = m + sizeof "machine" - 1;
     while ( VG_(isspace)( *p ) || *p == '=') {
       if (*p == '\n') goto next_line;
       ++p;
     }

     model_name = p;
     for (n = 0; n < sizeof model_map / sizeof model_map[0]; ++n) {
       struct model_map *mm = model_map + n;
       SizeT len = VG_(strlen)( mm->name );
       if ( VG_(strncmp)( mm->name, model_name, len ) == 0 &&
            VG_(isspace)( model_name[len] )) {
         if (mm->id < model) model = mm->id;
         p = model_name + len;
         break;
       }
     }
     
     while (*p != '\n')
       ++p;
   next_line: ;
   }

   VG_(free)( file_buf );
   VG_(debugLog)(1, "machine", "model = %s\n",
                 model == VEX_S390X_MODEL_UNKNOWN ? "UNKNOWN"
                                                  : model_map[model].name);
   return model;
}

#endif 

#if defined(VGA_mips32) || defined(VGA_mips64)

static UInt VG_(get_machine_model)(void)
{
   const char *search_MIPS_str = "MIPS";
   const char *search_Broadcom_str = "Broadcom";
   const char *search_Netlogic_str = "Netlogic";
   const char *search_Cavium_str= "Cavium";
   Int    n, fh;
   SysRes fd;
   SizeT  num_bytes, file_buf_size;
   HChar  *file_buf;

   
   fd = VG_(open)( "/proc/cpuinfo", 0, VKI_S_IRUSR );
   if ( sr_isError(fd) ) return -1;

   fh  = sr_Res(fd);

   num_bytes = 0;
   file_buf_size = 1000;
   file_buf = VG_(malloc)("cpuinfo", file_buf_size + 1);
   while (42) {
      n = VG_(read)(fh, file_buf, file_buf_size);
      if (n < 0) break;

      num_bytes += n;
      if (n < file_buf_size) break;  
   }

   if (n < 0) num_bytes = 0;   

   if (num_bytes > file_buf_size) {
      VG_(free)( file_buf );
      VG_(lseek)( fh, 0, VKI_SEEK_SET );
      file_buf = VG_(malloc)( "cpuinfo", num_bytes + 1 );
      n = VG_(read)( fh, file_buf, num_bytes );
      if (n < 0) num_bytes = 0;
   }

   file_buf[num_bytes] = '\0';
   VG_(close)(fh);

   
   if (VG_(strstr) (file_buf, search_Broadcom_str) != NULL)
       return VEX_PRID_COMP_BROADCOM;
   if (VG_(strstr) (file_buf, search_Netlogic_str) != NULL)
       return VEX_PRID_COMP_NETLOGIC;
   if (VG_(strstr)(file_buf, search_Cavium_str) != NULL)
       return VEX_PRID_COMP_CAVIUM;
   if (VG_(strstr) (file_buf, search_MIPS_str) != NULL)
       return VEX_PRID_COMP_MIPS;

   
   return -1;
}

#endif


Bool VG_(machine_get_hwcaps)( void )
{
   vg_assert(hwcaps_done == False);
   hwcaps_done = True;

   
   
   LibVEX_default_VexArchInfo(&vai);

#if defined(VGA_x86)
   { Bool have_sse1, have_sse2, have_sse3, have_cx8, have_lzcnt, have_mmxext;
     UInt eax, ebx, ecx, edx, max_extended;
     HChar vstr[13];
     vstr[0] = 0;

     if (!VG_(has_cpuid)())
        
        return False;

     VG_(cpuid)(0, 0, &eax, &ebx, &ecx, &edx);
     if (eax < 1)
        
        return False;

     VG_(memcpy)(&vstr[0], &ebx, 4);
     VG_(memcpy)(&vstr[4], &edx, 4);
     VG_(memcpy)(&vstr[8], &ecx, 4);
     vstr[12] = 0;

     VG_(cpuid)(0x80000000, 0, &eax, &ebx, &ecx, &edx);
     max_extended = eax;

     
     VG_(cpuid)(1, 0, &eax, &ebx, &ecx, &edx);

     have_sse1 = (edx & (1<<25)) != 0; 
     have_sse2 = (edx & (1<<26)) != 0; 
     have_sse3 = (ecx & (1<<0)) != 0;  

     have_cx8 = (edx & (1<<8)) != 0; 
     if (!have_cx8)
        return False;

     
     have_mmxext = False;
     if (0 == VG_(strcmp)(vstr, "AuthenticAMD")
         && max_extended >= 0x80000001) {
        VG_(cpuid)(0x80000001, 0, &eax, &ebx, &ecx, &edx);
        
        have_mmxext = !have_sse1 && ((edx & (1<<22)) != 0);
     }

     
     have_lzcnt = False;
     if ((0 == VG_(strcmp)(vstr, "AuthenticAMD")
          || 0 == VG_(strcmp)(vstr, "GenuineIntel"))
         && max_extended >= 0x80000001) {
        VG_(cpuid)(0x80000001, 0, &eax, &ebx, &ecx, &edx);
        have_lzcnt = (ecx & (1<<5)) != 0; 
     }

     if (have_sse1)
        have_mmxext = True;

     va = VexArchX86;
     vai.endness = VexEndnessLE;

     if (have_sse3 && have_sse2 && have_sse1 && have_mmxext) {
        vai.hwcaps  = VEX_HWCAPS_X86_MMXEXT;
        vai.hwcaps |= VEX_HWCAPS_X86_SSE1;
        vai.hwcaps |= VEX_HWCAPS_X86_SSE2;
        vai.hwcaps |= VEX_HWCAPS_X86_SSE3;
        if (have_lzcnt)
           vai.hwcaps |= VEX_HWCAPS_X86_LZCNT;
        VG_(machine_x86_have_mxcsr) = 1;
     } else if (have_sse2 && have_sse1 && have_mmxext) {
        vai.hwcaps  = VEX_HWCAPS_X86_MMXEXT;
        vai.hwcaps |= VEX_HWCAPS_X86_SSE1;
        vai.hwcaps |= VEX_HWCAPS_X86_SSE2;
        if (have_lzcnt)
           vai.hwcaps |= VEX_HWCAPS_X86_LZCNT;
        VG_(machine_x86_have_mxcsr) = 1;
     } else if (have_sse1 && have_mmxext) {
        vai.hwcaps  = VEX_HWCAPS_X86_MMXEXT;
        vai.hwcaps |= VEX_HWCAPS_X86_SSE1;
        VG_(machine_x86_have_mxcsr) = 1;
     } else if (have_mmxext) {
        vai.hwcaps  = VEX_HWCAPS_X86_MMXEXT; 
        VG_(machine_x86_have_mxcsr) = 0;
     } else {
       vai.hwcaps = 0; 
       VG_(machine_x86_have_mxcsr) = 0;
     }

     VG_(machine_get_cache_info)(&vai);

     return True;
   }

#elif defined(VGA_amd64)
   { Bool have_sse3, have_cx8, have_cx16;
     Bool have_lzcnt, have_avx, have_bmi, have_avx2;
     Bool have_rdtscp;
     UInt eax, ebx, ecx, edx, max_basic, max_extended;
     HChar vstr[13];
     vstr[0] = 0;

     if (!VG_(has_cpuid)())
        
        return False;

     VG_(cpuid)(0, 0, &eax, &ebx, &ecx, &edx);
     max_basic = eax;
     if (max_basic < 1)
        
        return False;

     VG_(memcpy)(&vstr[0], &ebx, 4);
     VG_(memcpy)(&vstr[4], &edx, 4);
     VG_(memcpy)(&vstr[8], &ecx, 4);
     vstr[12] = 0;

     VG_(cpuid)(0x80000000, 0, &eax, &ebx, &ecx, &edx);
     max_extended = eax;

     
     VG_(cpuid)(1, 0, &eax, &ebx, &ecx, &edx);

     
     have_sse3 = (ecx & (1<<0)) != 0;  
     
     
     

     
     
     
     have_avx = False;
     
     if ( (ecx & ((1<<27)|(1<<28))) == ((1<<27)|(1<<28)) ) {
        ULong w;
        __asm__ __volatile__("movq $0,%%rcx ; "
                             ".byte 0x0F,0x01,0xD0 ; " 
                             "movq %%rax,%0"
                             :"=r"(w) :
                             :"rdx","rcx");
        if ((w & 6) == 6) {
           
           have_avx = True;
           
        }
     }

     have_cx8 = (edx & (1<<8)) != 0; 
     if (!have_cx8)
        return False;

     
     have_cx16 = (ecx & (1<<13)) != 0; 

     
     have_lzcnt = False;
     if (max_extended >= 0x80000001) {
        VG_(cpuid)(0x80000001, 0, &eax, &ebx, &ecx, &edx);
        have_lzcnt = (ecx & (1<<5)) != 0; 
     }

     
     have_rdtscp = False;
     if (max_extended >= 0x80000001) {
        VG_(cpuid)(0x80000001, 0, &eax, &ebx, &ecx, &edx);
        have_rdtscp = (edx & (1<<27)) != 0; 
     }

     
     have_bmi = False;
     have_avx2 = False;
     if (have_avx && max_basic >= 7) {
        VG_(cpuid)(7, 0, &eax, &ebx, &ecx, &edx);
        have_bmi = (ebx & (1<<3)) != 0; 
        have_avx2 = (ebx & (1<<5)) != 0; 
     }

     va          = VexArchAMD64;
     vai.endness = VexEndnessLE;
     vai.hwcaps  = (have_sse3   ? VEX_HWCAPS_AMD64_SSE3   : 0)
                 | (have_cx16   ? VEX_HWCAPS_AMD64_CX16   : 0)
                 | (have_lzcnt  ? VEX_HWCAPS_AMD64_LZCNT  : 0)
                 | (have_avx    ? VEX_HWCAPS_AMD64_AVX    : 0)
                 | (have_bmi    ? VEX_HWCAPS_AMD64_BMI    : 0)
                 | (have_avx2   ? VEX_HWCAPS_AMD64_AVX2   : 0)
                 | (have_rdtscp ? VEX_HWCAPS_AMD64_RDTSCP : 0);

     VG_(machine_get_cache_info)(&vai);

     return True;
   }

#elif defined(VGA_ppc32)
   {
     vki_sigset_t          saved_set, tmp_set;
     vki_sigaction_fromK_t saved_sigill_act, saved_sigfpe_act;
     vki_sigaction_toK_t     tmp_sigill_act,   tmp_sigfpe_act;

     volatile Bool have_F, have_V, have_FX, have_GX, have_VX, have_DFP;
     volatile Bool have_isa_2_07;
     Int r;

     vg_assert(sizeof(vki_sigaction_fromK_t) == sizeof(vki_sigaction_toK_t));

     VG_(sigemptyset)(&tmp_set);
     VG_(sigaddset)(&tmp_set, VKI_SIGILL);
     VG_(sigaddset)(&tmp_set, VKI_SIGFPE);

     r = VG_(sigprocmask)(VKI_SIG_UNBLOCK, &tmp_set, &saved_set);
     vg_assert(r == 0);

     r = VG_(sigaction)(VKI_SIGILL, NULL, &saved_sigill_act);
     vg_assert(r == 0);
     tmp_sigill_act = saved_sigill_act;

     r = VG_(sigaction)(VKI_SIGFPE, NULL, &saved_sigfpe_act);
     vg_assert(r == 0);
     tmp_sigfpe_act = saved_sigfpe_act;

     tmp_sigill_act.sa_flags &= ~VKI_SA_RESETHAND;
     tmp_sigill_act.sa_flags &= ~VKI_SA_SIGINFO;
     tmp_sigill_act.sa_flags |=  VKI_SA_NODEFER;
     tmp_sigill_act.ksa_handler = handler_unsup_insn;
     r = VG_(sigaction)(VKI_SIGILL, &tmp_sigill_act, NULL);
     vg_assert(r == 0);

     tmp_sigfpe_act.sa_flags &= ~VKI_SA_RESETHAND;
     tmp_sigfpe_act.sa_flags &= ~VKI_SA_SIGINFO;
     tmp_sigfpe_act.sa_flags |=  VKI_SA_NODEFER;
     tmp_sigfpe_act.ksa_handler = handler_unsup_insn;
     r = VG_(sigaction)(VKI_SIGFPE, &tmp_sigfpe_act, NULL);
     vg_assert(r == 0);

     
     have_F = True;
     if (VG_MINIMAL_SETJMP(env_unsup_insn)) {
        have_F = False;
     } else {
        __asm__ __volatile__(".long 0xFC000090"); 
     }

     
     have_V = True;
     if (VG_MINIMAL_SETJMP(env_unsup_insn)) {
        have_V = False;
     } else {
        __asm__ __volatile__(".long 0x10000484"); 
     }

     
     have_FX = True;
     if (VG_MINIMAL_SETJMP(env_unsup_insn)) {
        have_FX = False;
     } else {
        __asm__ __volatile__(".long 0xFC00002C"); 
     }

     
     have_GX = True;
     if (VG_MINIMAL_SETJMP(env_unsup_insn)) {
        have_GX = False;
     } else {
        __asm__ __volatile__(".long 0xFC000034"); 
     }

     
     have_VX = True;
     if (VG_MINIMAL_SETJMP(env_unsup_insn)) {
        have_VX = False;
     } else {
        __asm__ __volatile__(".long 0xf0000564"); 
     }

     
     have_DFP = True;
     if (VG_MINIMAL_SETJMP(env_unsup_insn)) {
        have_DFP = False;
     } else {
        __asm__ __volatile__(".long 0xee4e8005"); 
     }

     
     have_isa_2_07 = True;
     if (VG_MINIMAL_SETJMP(env_unsup_insn)) {
        have_isa_2_07 = False;
     } else {
        __asm__ __volatile__(".long 0x7c000166"); 
     }

     find_ppc_dcbz_sz(&vai);

     r = VG_(sigaction)(VKI_SIGILL, &saved_sigill_act, NULL);
     vg_assert(r == 0);
     r = VG_(sigaction)(VKI_SIGFPE, &saved_sigfpe_act, NULL);
     vg_assert(r == 0);
     r = VG_(sigprocmask)(VKI_SIG_SETMASK, &saved_set, NULL);
     vg_assert(r == 0);
     VG_(debugLog)(1, "machine", "F %d V %d FX %d GX %d VX %d DFP %d ISA2.07 %d\n",
                    (Int)have_F, (Int)have_V, (Int)have_FX,
                    (Int)have_GX, (Int)have_VX, (Int)have_DFP,
                    (Int)have_isa_2_07);
     
     if (have_V && !have_F)
        have_V = False;
     if (have_FX && !have_F)
        have_FX = False;
     if (have_GX && !have_F)
        have_GX = False;

     VG_(machine_ppc32_has_FP)  = have_F ? 1 : 0;
     VG_(machine_ppc32_has_VMX) = have_V ? 1 : 0;

     va = VexArchPPC32;
     vai.endness = VexEndnessBE;

     vai.hwcaps = 0;
     if (have_F)  vai.hwcaps |= VEX_HWCAPS_PPC32_F;
     if (have_V)  vai.hwcaps |= VEX_HWCAPS_PPC32_V;
     if (have_FX) vai.hwcaps |= VEX_HWCAPS_PPC32_FX;
     if (have_GX) vai.hwcaps |= VEX_HWCAPS_PPC32_GX;
     if (have_VX) vai.hwcaps |= VEX_HWCAPS_PPC32_VX;
     if (have_DFP) vai.hwcaps |= VEX_HWCAPS_PPC32_DFP;
     if (have_isa_2_07) vai.hwcaps |= VEX_HWCAPS_PPC32_ISA2_07;

     VG_(machine_get_cache_info)(&vai);

     return True;
   }

#elif defined(VGA_ppc64be)|| defined(VGA_ppc64le)
   {
     
     vki_sigset_t          saved_set, tmp_set;
     vki_sigaction_fromK_t saved_sigill_act, saved_sigfpe_act;
     vki_sigaction_toK_t     tmp_sigill_act,   tmp_sigfpe_act;

     volatile Bool have_F, have_V, have_FX, have_GX, have_VX, have_DFP;
     volatile Bool have_isa_2_07;
     Int r;

     vg_assert(sizeof(vki_sigaction_fromK_t) == sizeof(vki_sigaction_toK_t));

     VG_(sigemptyset)(&tmp_set);
     VG_(sigaddset)(&tmp_set, VKI_SIGILL);
     VG_(sigaddset)(&tmp_set, VKI_SIGFPE);

     r = VG_(sigprocmask)(VKI_SIG_UNBLOCK, &tmp_set, &saved_set);
     vg_assert(r == 0);

     r = VG_(sigaction)(VKI_SIGILL, NULL, &saved_sigill_act);
     vg_assert(r == 0);
     tmp_sigill_act = saved_sigill_act;

     VG_(sigaction)(VKI_SIGFPE, NULL, &saved_sigfpe_act);
     tmp_sigfpe_act = saved_sigfpe_act;

     tmp_sigill_act.sa_flags &= ~VKI_SA_RESETHAND;
     tmp_sigill_act.sa_flags &= ~VKI_SA_SIGINFO;
     tmp_sigill_act.sa_flags |=  VKI_SA_NODEFER;
     tmp_sigill_act.ksa_handler = handler_unsup_insn;
     VG_(sigaction)(VKI_SIGILL, &tmp_sigill_act, NULL);

     tmp_sigfpe_act.sa_flags &= ~VKI_SA_RESETHAND;
     tmp_sigfpe_act.sa_flags &= ~VKI_SA_SIGINFO;
     tmp_sigfpe_act.sa_flags |=  VKI_SA_NODEFER;
     tmp_sigfpe_act.ksa_handler = handler_unsup_insn;
     VG_(sigaction)(VKI_SIGFPE, &tmp_sigfpe_act, NULL);

     
     have_F = True;
     if (VG_MINIMAL_SETJMP(env_unsup_insn)) {
        have_F = False;
     } else {
        __asm__ __volatile__("fmr 0,0");
     }

     
     have_V = True;
     if (VG_MINIMAL_SETJMP(env_unsup_insn)) {
        have_V = False;
     } else {
        __asm__ __volatile__(".long 0x10000484"); 
     }

     
     have_FX = True;
     if (VG_MINIMAL_SETJMP(env_unsup_insn)) {
        have_FX = False;
     } else {
        __asm__ __volatile__(".long 0xFC00002C"); 
     }

     
     have_GX = True;
     if (VG_MINIMAL_SETJMP(env_unsup_insn)) {
        have_GX = False;
     } else {
        __asm__ __volatile__(".long 0xFC000034"); 
     }

     
     have_VX = True;
     if (VG_MINIMAL_SETJMP(env_unsup_insn)) {
        have_VX = False;
     } else {
        __asm__ __volatile__(".long 0xf0000564"); 
     }

     
     have_DFP = True;
     if (VG_MINIMAL_SETJMP(env_unsup_insn)) {
        have_DFP = False;
     } else {
        __asm__ __volatile__(".long 0xee4e8005"); 
     }

     
     have_isa_2_07 = True;
     if (VG_MINIMAL_SETJMP(env_unsup_insn)) {
        have_isa_2_07 = False;
     } else {
        __asm__ __volatile__(".long 0x7c000166"); 
     }

     find_ppc_dcbz_sz(&vai);

     VG_(sigaction)(VKI_SIGILL, &saved_sigill_act, NULL);
     VG_(sigaction)(VKI_SIGFPE, &saved_sigfpe_act, NULL);
     VG_(sigprocmask)(VKI_SIG_SETMASK, &saved_set, NULL);
     VG_(debugLog)(1, "machine", "F %d V %d FX %d GX %d VX %d DFP %d ISA2.07 %d\n",
                    (Int)have_F, (Int)have_V, (Int)have_FX,
                    (Int)have_GX, (Int)have_VX, (Int)have_DFP,
                    (Int)have_isa_2_07);
     
     if (!have_F)
        return False;

     VG_(machine_ppc64_has_VMX) = have_V ? 1 : 0;

     va = VexArchPPC64;
#    if defined(VKI_LITTLE_ENDIAN)
     vai.endness = VexEndnessLE;
#    elif defined(VKI_BIG_ENDIAN)
     vai.endness = VexEndnessBE;
#    else
     vai.endness = VexEndness_INVALID;
#    endif

     vai.hwcaps = 0;
     if (have_V)  vai.hwcaps |= VEX_HWCAPS_PPC64_V;
     if (have_FX) vai.hwcaps |= VEX_HWCAPS_PPC64_FX;
     if (have_GX) vai.hwcaps |= VEX_HWCAPS_PPC64_GX;
     if (have_VX) vai.hwcaps |= VEX_HWCAPS_PPC64_VX;
     if (have_DFP) vai.hwcaps |= VEX_HWCAPS_PPC64_DFP;
     if (have_isa_2_07) vai.hwcaps |= VEX_HWCAPS_PPC64_ISA2_07;

     VG_(machine_get_cache_info)(&vai);

     return True;
   }

#elif defined(VGA_s390x)

#  include "libvex_s390x_common.h"

   {
     
     vki_sigset_t          saved_set, tmp_set;
     vki_sigaction_fromK_t saved_sigill_act;
     vki_sigaction_toK_t     tmp_sigill_act;

     volatile Bool have_LDISP, have_STFLE;
     Int i, r, model;

     model = VG_(get_machine_model)();

     
     VG_(sigemptyset)(&tmp_set);
     VG_(sigaddset)(&tmp_set, VKI_SIGILL);

     r = VG_(sigprocmask)(VKI_SIG_UNBLOCK, &tmp_set, &saved_set);
     vg_assert(r == 0);

     r = VG_(sigaction)(VKI_SIGILL, NULL, &saved_sigill_act);
     vg_assert(r == 0);
     tmp_sigill_act = saved_sigill_act;

     tmp_sigill_act.sa_flags &= ~VKI_SA_RESETHAND;
     tmp_sigill_act.sa_flags &= ~VKI_SA_SIGINFO;
     tmp_sigill_act.sa_flags |=  VKI_SA_NODEFER;
     tmp_sigill_act.ksa_handler = handler_unsup_insn;
     VG_(sigaction)(VKI_SIGILL, &tmp_sigill_act, NULL);


     have_LDISP = True;
     if (VG_MINIMAL_SETJMP(env_unsup_insn)) {
        have_LDISP = False;
     } else {
        __asm__ __volatile__("basr %%r1,%%r0\n\t"
                             ".long  0xe3001000\n\t"  
                             ".short 0x0057" : : : "r0", "r1", "cc", "memory");
     }

     ULong hoststfle[S390_NUM_FACILITY_DW];

     for (i = 0; i < S390_NUM_FACILITY_DW; ++i)
        hoststfle[i] = 0;

     have_STFLE = True;
     if (VG_MINIMAL_SETJMP(env_unsup_insn)) {
        have_STFLE = False;
     } else {
         register ULong reg0 asm("0") = S390_NUM_FACILITY_DW - 1;

         __asm__ __volatile__(" .insn s,0xb2b00000,%0\n"   
                              : "=m" (hoststfle), "+d"(reg0)
                              : : "cc", "memory");
     }

     
     r = VG_(sigaction)(VKI_SIGILL, &saved_sigill_act, NULL);
     vg_assert(r == 0);
     r = VG_(sigprocmask)(VKI_SIG_SETMASK, &saved_set, NULL);
     vg_assert(r == 0);
     va = VexArchS390X;
     vai.endness = VexEndnessBE;

     vai.hwcaps = model;
     if (have_STFLE) vai.hwcaps |= VEX_HWCAPS_S390X_STFLE;
     if (have_LDISP) {
        if (model >= VEX_S390X_MODEL_Z990)
           vai.hwcaps |= VEX_HWCAPS_S390X_LDISP;
     }

     struct fac_hwcaps_map {
        UInt installed;
        UInt facility_bit;
        UInt hwcaps_bit;
        const HChar name[6];   
     } fac_hwcaps[] = {
        { False, S390_FAC_EIMM,  VEX_HWCAPS_S390X_EIMM,  "EIMM"  },
        { False, S390_FAC_GIE,   VEX_HWCAPS_S390X_GIE,   "GIE"   },
        { False, S390_FAC_DFP,   VEX_HWCAPS_S390X_DFP,   "DFP"   },
        { False, S390_FAC_FPSE,  VEX_HWCAPS_S390X_FGX,   "FGX"   },
        { False, S390_FAC_ETF2,  VEX_HWCAPS_S390X_ETF2,  "ETF2"  },
        { False, S390_FAC_ETF3,  VEX_HWCAPS_S390X_ETF3,  "ETF3"  },
        { False, S390_FAC_STCKF, VEX_HWCAPS_S390X_STCKF, "STCKF" },
        { False, S390_FAC_FPEXT, VEX_HWCAPS_S390X_FPEXT, "FPEXT" },
        { False, S390_FAC_LSC,   VEX_HWCAPS_S390X_LSC,   "LSC"   },
        { False, S390_FAC_PFPO,  VEX_HWCAPS_S390X_PFPO,  "PFPO"  },
     };

     
     for (i=0; i < sizeof fac_hwcaps / sizeof fac_hwcaps[0]; ++i) {
        vg_assert(fac_hwcaps[i].facility_bit <= 63);  
        if (hoststfle[0] & (1ULL << (63 - fac_hwcaps[i].facility_bit))) {
           fac_hwcaps[i].installed = True;
           vai.hwcaps |= fac_hwcaps[i].hwcaps_bit;
        }
     }

     
     HChar fac_str[(sizeof fac_hwcaps / sizeof fac_hwcaps[0]) *
                   (sizeof fac_hwcaps[0].name + 3) + 
                   7 + 1 + 4 + 2  
                   + 1];  
     HChar *p = fac_str;
     p += VG_(sprintf)(p, "machine %4d  ", model);
     for (i=0; i < sizeof fac_hwcaps / sizeof fac_hwcaps[0]; ++i) {
        p += VG_(sprintf)(p, " %s %1d", fac_hwcaps[i].name,
                          fac_hwcaps[i].installed);
     }
     *p++ = '\0';

     VG_(debugLog)(1, "machine", "%s\n", fac_str);
     VG_(debugLog)(1, "machine", "hwcaps = 0x%x\n", vai.hwcaps);

     VG_(machine_get_cache_info)(&vai);

     return True;
   }

#elif defined(VGA_arm)
   {
     
     vki_sigset_t          saved_set, tmp_set;
     vki_sigaction_fromK_t saved_sigill_act, saved_sigfpe_act;
     vki_sigaction_toK_t     tmp_sigill_act,   tmp_sigfpe_act;

     volatile Bool have_VFP, have_VFP2, have_VFP3, have_NEON;
     volatile Int archlevel;
     Int r;

     vg_assert(sizeof(vki_sigaction_fromK_t) == sizeof(vki_sigaction_toK_t));

     VG_(sigemptyset)(&tmp_set);
     VG_(sigaddset)(&tmp_set, VKI_SIGILL);
     VG_(sigaddset)(&tmp_set, VKI_SIGFPE);

     r = VG_(sigprocmask)(VKI_SIG_UNBLOCK, &tmp_set, &saved_set);
     vg_assert(r == 0);

     r = VG_(sigaction)(VKI_SIGILL, NULL, &saved_sigill_act);
     vg_assert(r == 0);
     tmp_sigill_act = saved_sigill_act;

     VG_(sigaction)(VKI_SIGFPE, NULL, &saved_sigfpe_act);
     tmp_sigfpe_act = saved_sigfpe_act;

     tmp_sigill_act.sa_flags &= ~VKI_SA_RESETHAND;
     tmp_sigill_act.sa_flags &= ~VKI_SA_SIGINFO;
     tmp_sigill_act.sa_flags |=  VKI_SA_NODEFER;
     tmp_sigill_act.ksa_handler = handler_unsup_insn;
     VG_(sigaction)(VKI_SIGILL, &tmp_sigill_act, NULL);

     tmp_sigfpe_act.sa_flags &= ~VKI_SA_RESETHAND;
     tmp_sigfpe_act.sa_flags &= ~VKI_SA_SIGINFO;
     tmp_sigfpe_act.sa_flags |=  VKI_SA_NODEFER;
     tmp_sigfpe_act.ksa_handler = handler_unsup_insn;
     VG_(sigaction)(VKI_SIGFPE, &tmp_sigfpe_act, NULL);

     
     have_VFP = True;
     if (VG_MINIMAL_SETJMP(env_unsup_insn)) {
        have_VFP = False;
     } else {
        __asm__ __volatile__(".word 0xEEB02B42"); 
     }
     have_VFP2 = have_VFP;
     have_VFP3 = have_VFP;

     
     have_NEON = True;
     if (VG_MINIMAL_SETJMP(env_unsup_insn)) {
        have_NEON = False;
     } else {
        __asm__ __volatile__(".word 0xF2244154"); 
     }

     
     archlevel = 5; 
     if (archlevel < 7) {
        archlevel = 7;
        if (VG_MINIMAL_SETJMP(env_unsup_insn)) {
           archlevel = 5;
        } else {
           __asm__ __volatile__(".word 0xF45FF000"); 
        }
     }
     if (archlevel < 6) {
        archlevel = 6;
        if (VG_MINIMAL_SETJMP(env_unsup_insn)) {
           archlevel = 5;
        } else {
           __asm__ __volatile__(".word 0xE6822012"); 
        }
     }

     VG_(convert_sigaction_fromK_to_toK)(&saved_sigill_act, &tmp_sigill_act);
     VG_(convert_sigaction_fromK_to_toK)(&saved_sigfpe_act, &tmp_sigfpe_act);
     VG_(sigaction)(VKI_SIGILL, &tmp_sigill_act, NULL);
     VG_(sigaction)(VKI_SIGFPE, &tmp_sigfpe_act, NULL);
     VG_(sigprocmask)(VKI_SIG_SETMASK, &saved_set, NULL);

     VG_(debugLog)(1, "machine", "ARMv%d VFP %d VFP2 %d VFP3 %d NEON %d\n",
           archlevel, (Int)have_VFP, (Int)have_VFP2, (Int)have_VFP3,
           (Int)have_NEON);

     VG_(machine_arm_archlevel) = archlevel;

     va = VexArchARM;
     vai.endness = VexEndnessLE;

     vai.hwcaps = VEX_ARM_ARCHLEVEL(archlevel);
     if (have_VFP3) vai.hwcaps |= VEX_HWCAPS_ARM_VFP3;
     if (have_VFP2) vai.hwcaps |= VEX_HWCAPS_ARM_VFP2;
     if (have_VFP)  vai.hwcaps |= VEX_HWCAPS_ARM_VFP;
     if (have_NEON) vai.hwcaps |= VEX_HWCAPS_ARM_NEON;

     VG_(machine_get_cache_info)(&vai);

     return True;
   }

#elif defined(VGA_arm64)
   {
     va = VexArchARM64;
     vai.endness = VexEndnessLE;

     
     vai.hwcaps = 0;

     VG_(machine_get_cache_info)(&vai);

     vg_assert(vai.arm64_dMinLine_lg2_szB == 0);
     vg_assert(vai.arm64_iMinLine_lg2_szB == 0);
     ULong ctr_el0;
     __asm__ __volatile__("mrs %0, ctr_el0" : "=r"(ctr_el0));
     vai.arm64_dMinLine_lg2_szB = ((ctr_el0 >> 16) & 0xF) + 2;
     vai.arm64_iMinLine_lg2_szB = ((ctr_el0 >>  0) & 0xF) + 2;
     VG_(debugLog)(1, "machine", "ARM64: ctr_el0.dMinLine_szB = %d, "
                      "ctr_el0.iMinLine_szB = %d\n",
                   1 << vai.arm64_dMinLine_lg2_szB,
                   1 << vai.arm64_iMinLine_lg2_szB);

     return True;
   }

#elif defined(VGA_mips32)
   {
     
#    define FP64 22
     va = VexArchMIPS32;
     UInt model = VG_(get_machine_model)();
     if (model == -1)
         return False;

     vai.hwcaps = model;

#    if defined(VKI_LITTLE_ENDIAN)
     vai.endness = VexEndnessLE;
#    elif defined(VKI_BIG_ENDIAN)
     vai.endness = VexEndnessBE;
#    else
     vai.endness = VexEndness_INVALID;
#    endif

     
     vki_sigset_t          saved_set, tmp_set;
     vki_sigaction_fromK_t saved_sigill_act;
     vki_sigaction_toK_t   tmp_sigill_act;

     volatile Bool have_DSP, have_DSPr2;
     Int r;

     vg_assert(sizeof(vki_sigaction_fromK_t) == sizeof(vki_sigaction_toK_t));

     VG_(sigemptyset)(&tmp_set);
     VG_(sigaddset)(&tmp_set, VKI_SIGILL);

     r = VG_(sigprocmask)(VKI_SIG_UNBLOCK, &tmp_set, &saved_set);
     vg_assert(r == 0);

     r = VG_(sigaction)(VKI_SIGILL, NULL, &saved_sigill_act);
     vg_assert(r == 0);
     tmp_sigill_act = saved_sigill_act;

     tmp_sigill_act.sa_flags &= ~VKI_SA_RESETHAND;
     tmp_sigill_act.sa_flags &= ~VKI_SA_SIGINFO;
     tmp_sigill_act.sa_flags |=  VKI_SA_NODEFER;
     tmp_sigill_act.ksa_handler = handler_unsup_insn;
     VG_(sigaction)(VKI_SIGILL, &tmp_sigill_act, NULL);

     if (model == VEX_PRID_COMP_MIPS) {
        
        have_DSPr2 = True;
        if (VG_MINIMAL_SETJMP(env_unsup_insn)) {
           have_DSPr2 = False;
        } else {
           __asm__ __volatile__(".word 0x7d095351"); 
        }
        if (have_DSPr2) {
           
           vai.hwcaps |= VEX_PRID_IMP_74K;
        } else {
           
           have_DSP = True;
           if (VG_MINIMAL_SETJMP(env_unsup_insn)) {
              have_DSP = False;
           } else {
              __asm__ __volatile__(".word 0x7c3f44b8"); 
           }
           if (have_DSP) {
              
              vai.hwcaps |= VEX_PRID_IMP_34K;
           }
        }
     }

     
     int FIR = 0;
     __asm__ __volatile__(
        "cfc1 %0, $0"  "\n\t"
        : "=r" (FIR)
     );
     if (FIR & (1 << FP64)) {
        vai.hwcaps |= VEX_PRID_CPU_32FPR;
     }

     VG_(convert_sigaction_fromK_to_toK)(&saved_sigill_act, &tmp_sigill_act);
     VG_(sigaction)(VKI_SIGILL, &tmp_sigill_act, NULL);
     VG_(sigprocmask)(VKI_SIG_SETMASK, &saved_set, NULL);

     VG_(debugLog)(1, "machine", "hwcaps = 0x%x\n", vai.hwcaps);
     VG_(machine_get_cache_info)(&vai);

     return True;
   }

#elif defined(VGA_mips64)
   {
     va = VexArchMIPS64;
     UInt model = VG_(get_machine_model)();
     if (model == -1)
         return False;

     vai.hwcaps = model;

#    if defined(VKI_LITTLE_ENDIAN)
     vai.endness = VexEndnessLE;
#    elif defined(VKI_BIG_ENDIAN)
     vai.endness = VexEndnessBE;
#    else
     vai.endness = VexEndness_INVALID;
#    endif

     VG_(machine_get_cache_info)(&vai);

     return True;
   }

#elif defined(VGA_tilegx)
   {
     va = VexArchTILEGX;
     vai.hwcaps = VEX_HWCAPS_TILEGX_BASE;
     vai.endness = VexEndnessLE;

     VG_(machine_get_cache_info)(&vai);

     return True;
   }

#else
#  error "Unknown arch"
#endif
}

#if defined(VGA_ppc32)
void VG_(machine_ppc32_set_clszB)( Int szB )
{
   vg_assert(hwcaps_done);

   vg_assert(vai.ppc_icache_line_szB == 0
             || vai.ppc_icache_line_szB == szB);

   vg_assert(szB == 16 || szB == 32 || szB == 64 || szB == 128);
   vai.ppc_icache_line_szB = szB;
}
#endif


#if defined(VGA_ppc64be)|| defined(VGA_ppc64le)
void VG_(machine_ppc64_set_clszB)( Int szB )
{
   vg_assert(hwcaps_done);

   vg_assert(vai.ppc_icache_line_szB == 0
             || vai.ppc_icache_line_szB == szB);

   vg_assert(szB == 16 || szB == 32 || szB == 64 || szB == 128);
   vai.ppc_icache_line_szB = szB;
}
#endif


#if defined(VGA_arm)
void VG_(machine_arm_set_has_NEON)( Bool has_neon )
{
   vg_assert(hwcaps_done);
   

   if (has_neon) {
      vai.hwcaps |= VEX_HWCAPS_ARM_NEON;
   } else {
      vai.hwcaps &= ~VEX_HWCAPS_ARM_NEON;
   }
}
#endif


void VG_(machine_get_VexArchInfo)( VexArch* pVa,
                                   VexArchInfo* pVai )
{
   vg_assert(hwcaps_done);
   if (pVa)  *pVa  = va;
   if (pVai) *pVai = vai;
}


Int VG_(machine_get_size_of_largest_guest_register) ( void )
{
   vg_assert(hwcaps_done);

#  if defined(VGA_x86)
   vg_assert(va == VexArchX86);
   return 16;

#  elif defined(VGA_amd64)
   
   return (vai.hwcaps & VEX_HWCAPS_AMD64_AVX) ? 32 : 16;

#  elif defined(VGA_ppc32)
   
   if (vai.hwcaps & VEX_HWCAPS_PPC32_V) return 16;
   if (vai.hwcaps & VEX_HWCAPS_PPC32_VX) return 16;
   if (vai.hwcaps & VEX_HWCAPS_PPC32_DFP) return 16;
   return 8;

#  elif defined(VGA_ppc64be) || defined(VGA_ppc64le)
   
   if (vai.hwcaps & VEX_HWCAPS_PPC64_V) return 16;
   if (vai.hwcaps & VEX_HWCAPS_PPC64_VX) return 16;
   if (vai.hwcaps & VEX_HWCAPS_PPC64_DFP) return 16;
   return 8;

#  elif defined(VGA_s390x)
   return 8;

#  elif defined(VGA_arm)
   return 16;

#  elif defined(VGA_arm64)
   
   return 16;

#  elif defined(VGA_mips32)
   return 8;

#  elif defined(VGA_mips64)
   return 8;

#  elif defined(VGA_tilegx)
   return 8;

#  else
#    error "Unknown arch"
#  endif
}


void* VG_(fnptr_to_fnentry)( void* f )
{
#  if defined(VGP_x86_linux) || defined(VGP_amd64_linux)  \
      || defined(VGP_arm_linux) || defined(VGO_darwin)          \
      || defined(VGP_ppc32_linux) || defined(VGP_ppc64le_linux) \
      || defined(VGP_s390x_linux) || defined(VGP_mips32_linux) \
      || defined(VGP_mips64_linux) || defined(VGP_arm64_linux) \
      || defined(VGP_tilegx_linux)
   return f;
#  elif defined(VGP_ppc64be_linux)
   UWord* descr = (UWord*)f;
   return (void*)(descr[0]);
#  else
#    error "Unknown platform"
#  endif
}

