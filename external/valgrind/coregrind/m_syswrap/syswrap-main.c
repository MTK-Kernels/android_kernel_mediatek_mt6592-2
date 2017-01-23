

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

#include "libvex_guest_offsets.h"
#include "libvex_trc_values.h"
#include "pub_core_basics.h"
#include "pub_core_aspacemgr.h"
#include "pub_core_vki.h"
#include "pub_core_vkiscnums.h"
#include "pub_core_threadstate.h"
#include "pub_core_libcbase.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcprint.h"
#include "pub_core_libcproc.h"      
#include "pub_core_libcsignal.h"
#include "pub_core_scheduler.h"     
                                    
#include "pub_core_stacktrace.h"    
#include "pub_core_tooliface.h"
#include "pub_core_options.h"
#include "pub_core_signals.h"       
#include "pub_core_syscall.h"
#include "pub_core_machine.h"
#include "pub_core_mallocfree.h"
#include "pub_core_syswrap.h"

#include "priv_types_n_macros.h"
#include "priv_syswrap-main.h"

#if defined(VGO_darwin)
#include "priv_syswrap-darwin.h"
#endif



/* The main function is VG_(client_syscall).  The simulation calls it
   whenever a client thread wants to do a syscall.  The following is a
   sketch of what it does.

   * Ensures the root thread's stack is suitably mapped.  Tedious and
     arcane.  See big big comment in VG_(client_syscall).

   * First, it rounds up the syscall number and args (which is a
     platform dependent activity) and puts them in a struct ("args")
     and also a copy in "orig_args".

     The pre/post wrappers refer to these structs and so no longer
     need magic macros to access any specific registers.  This struct
     is stored in thread-specific storage.


   * The pre-wrapper is called, passing it a pointer to struct
     "args".


   * The pre-wrapper examines the args and pokes the tool
     appropriately.  It may modify the args; this is why "orig_args"
     is also stored.

     The pre-wrapper may choose to 'do' the syscall itself, and
     concludes one of three outcomes:

       Success(N)    -- syscall is already complete, with success;
                        result is N

       Fail(N)       -- syscall is already complete, with failure;
                        error code is N

       HandToKernel  -- (the usual case): this needs to be given to
                        the kernel to be done, using the values in
                        the possibly-modified "args" struct.

     In addition, the pre-wrapper may set some flags:

       MayBlock   -- only applicable when outcome==HandToKernel

       PostOnFail -- only applicable when outcome==HandToKernel or Fail


   * If the pre-outcome is HandToKernel, the syscall is duly handed
     off to the kernel (perhaps involving some thread switchery, but
     that's not important).  This reduces the possible set of outcomes
     to either Success(N) or Fail(N).


   * The outcome (Success(N) or Fail(N)) is written back to the guest
     register(s).  This is platform specific:

     x86:    Success(N) ==>  eax = N
             Fail(N)    ==>  eax = -N

     ditto amd64

     ppc32:  Success(N) ==>  r3 = N, CR0.SO = 0
             Fail(N) ==>     r3 = N, CR0.SO = 1

     Darwin:
     x86:    Success(N) ==>  edx:eax = N, cc = 0
             Fail(N)    ==>  edx:eax = N, cc = 1

     s390x:  Success(N) ==>  r2 = N
             Fail(N)    ==>  r2 = -N

   * The post wrapper is called if:

     - it exists, and
     - outcome==Success or (outcome==Fail and PostOnFail is set)

     The post wrapper is passed the adulterated syscall args (struct
     "args"), and the syscall outcome (viz, Success(N) or Fail(N)).

   There are several other complications, primarily to do with
   syscalls getting interrupted, explained in comments in the code.
*/




/* Perform a syscall on behalf of a client thread, using a specific
   signal mask.  On completion, the signal mask is set to restore_mask
   (which presumably blocks almost everything).  If a signal happens
   during the syscall, the handler should call
   VG_(fixup_guest_state_after_syscall_interrupted) to adjust the
   thread's context to do the right thing.

   The _WRK function is handwritten assembly, implemented per-platform
   in coregrind/m_syswrap/syscall-$PLAT.S.  It has some very magic
   properties.  See comments at the top of
   VG_(fixup_guest_state_after_syscall_interrupted) below for details.

   This function (these functions) are required to return zero in case
   of success (even if the syscall itself failed), and nonzero if the
   sigprocmask-swizzling calls failed.  We don't actually care about
   the failure values from sigprocmask, although most of the assembly
   implementations do attempt to return that, using the convention
   0 for success, or 0x8000 | error-code for failure.
*/
#if defined(VGO_linux)
extern
UWord ML_(do_syscall_for_client_WRK)( Word syscallno, 
                                      void* guest_state,
                                      const vki_sigset_t *syscall_mask,
                                      const vki_sigset_t *restore_mask,
                                      Word sigsetSzB );
#elif defined(VGO_darwin)
extern
UWord ML_(do_syscall_for_client_unix_WRK)( Word syscallno, 
                                           void* guest_state,
                                           const vki_sigset_t *syscall_mask,
                                           const vki_sigset_t *restore_mask,
                                           Word sigsetSzB ); 
extern
UWord ML_(do_syscall_for_client_mach_WRK)( Word syscallno, 
                                           void* guest_state,
                                           const vki_sigset_t *syscall_mask,
                                           const vki_sigset_t *restore_mask,
                                           Word sigsetSzB ); 
extern
UWord ML_(do_syscall_for_client_mdep_WRK)( Word syscallno, 
                                           void* guest_state,
                                           const vki_sigset_t *syscall_mask,
                                           const vki_sigset_t *restore_mask,
                                           Word sigsetSzB ); 
#else
#  error "Unknown OS"
#endif


static
void do_syscall_for_client ( Int syscallno,
                             ThreadState* tst,
                             const vki_sigset_t* syscall_mask )
{
   vki_sigset_t saved;
   UWord err;
#  if defined(VGO_linux)
   err = ML_(do_syscall_for_client_WRK)(
            syscallno, &tst->arch.vex, 
            syscall_mask, &saved, sizeof(vki_sigset_t)
         );
#  elif defined(VGO_darwin)
   switch (VG_DARWIN_SYSNO_CLASS(syscallno)) {
      case VG_DARWIN_SYSCALL_CLASS_UNIX:
         err = ML_(do_syscall_for_client_unix_WRK)(
                  VG_DARWIN_SYSNO_FOR_KERNEL(syscallno), &tst->arch.vex, 
                  syscall_mask, &saved, 0
               );
         break;
      case VG_DARWIN_SYSCALL_CLASS_MACH:
         err = ML_(do_syscall_for_client_mach_WRK)(
                  VG_DARWIN_SYSNO_FOR_KERNEL(syscallno), &tst->arch.vex, 
                  syscall_mask, &saved, 0
               );
         break;
      case VG_DARWIN_SYSCALL_CLASS_MDEP:
         err = ML_(do_syscall_for_client_mdep_WRK)(
                  VG_DARWIN_SYSNO_FOR_KERNEL(syscallno), &tst->arch.vex, 
                  syscall_mask, &saved, 0
               );
         break;
      default:
         vg_assert(0);
         
         break;
   }
#  else
#    error "Unknown OS"
#  endif
   vg_assert2(
      err == 0,
      "ML_(do_syscall_for_client_WRK): sigprocmask error %d",
      (Int)(err & 0xFFF)
   );
}



static
Bool eq_SyscallArgs ( SyscallArgs* a1, SyscallArgs* a2 )
{
   return a1->sysno == a2->sysno
          && a1->arg1 == a2->arg1
          && a1->arg2 == a2->arg2
          && a1->arg3 == a2->arg3
          && a1->arg4 == a2->arg4
          && a1->arg5 == a2->arg5
          && a1->arg6 == a2->arg6
          && a1->arg7 == a2->arg7
          && a1->arg8 == a2->arg8;
}

static
Bool eq_SyscallStatus ( SyscallStatus* s1, SyscallStatus* s2 )
{
   
   if (s1->what == s2->what && sr_EQ( s1->sres, s2->sres ))
      return True;
#  if defined(VGO_darwin)
   
   vg_assert(s1->what == s2->what);
   VG_(printf)("eq_SyscallStatus:\n");
   VG_(printf)("  {%lu %lu %u}\n", s1->sres._wLO, s1->sres._wHI, s1->sres._mode);
   VG_(printf)("  {%lu %lu %u}\n", s2->sres._wLO, s2->sres._wHI, s2->sres._mode);
   vg_assert(0);
#  endif
   return False;
}


static
SyscallStatus convert_SysRes_to_SyscallStatus ( SysRes res )
{
   SyscallStatus status;
   status.what = SsComplete;
   status.sres = res;
   return status;
}



static 
void getSyscallArgsFromGuestState ( SyscallArgs*       canonical,
                                     VexGuestArchState* gst_vanilla, 
                                     UInt trc )
{
#if defined(VGP_x86_linux)
   VexGuestX86State* gst = (VexGuestX86State*)gst_vanilla;
   canonical->sysno = gst->guest_EAX;
   canonical->arg1  = gst->guest_EBX;
   canonical->arg2  = gst->guest_ECX;
   canonical->arg3  = gst->guest_EDX;
   canonical->arg4  = gst->guest_ESI;
   canonical->arg5  = gst->guest_EDI;
   canonical->arg6  = gst->guest_EBP;
   canonical->arg7  = 0;
   canonical->arg8  = 0;

#elif defined(VGP_amd64_linux)
   VexGuestAMD64State* gst = (VexGuestAMD64State*)gst_vanilla;
   canonical->sysno = gst->guest_RAX;
   canonical->arg1  = gst->guest_RDI;
   canonical->arg2  = gst->guest_RSI;
   canonical->arg3  = gst->guest_RDX;
   canonical->arg4  = gst->guest_R10;
   canonical->arg5  = gst->guest_R8;
   canonical->arg6  = gst->guest_R9;
   canonical->arg7  = 0;
   canonical->arg8  = 0;

#elif defined(VGP_ppc32_linux)
   VexGuestPPC32State* gst = (VexGuestPPC32State*)gst_vanilla;
   canonical->sysno = gst->guest_GPR0;
   canonical->arg1  = gst->guest_GPR3;
   canonical->arg2  = gst->guest_GPR4;
   canonical->arg3  = gst->guest_GPR5;
   canonical->arg4  = gst->guest_GPR6;
   canonical->arg5  = gst->guest_GPR7;
   canonical->arg6  = gst->guest_GPR8;
   canonical->arg7  = 0;
   canonical->arg8  = 0;

#elif defined(VGP_ppc64be_linux) || defined(VGP_ppc64le_linux)
   VexGuestPPC64State* gst = (VexGuestPPC64State*)gst_vanilla;
   canonical->sysno = gst->guest_GPR0;
   canonical->arg1  = gst->guest_GPR3;
   canonical->arg2  = gst->guest_GPR4;
   canonical->arg3  = gst->guest_GPR5;
   canonical->arg4  = gst->guest_GPR6;
   canonical->arg5  = gst->guest_GPR7;
   canonical->arg6  = gst->guest_GPR8;
   canonical->arg7  = 0;
   canonical->arg8  = 0;

#elif defined(VGP_arm_linux)
   VexGuestARMState* gst = (VexGuestARMState*)gst_vanilla;
   canonical->sysno = gst->guest_R7;
   canonical->arg1  = gst->guest_R0;
   canonical->arg2  = gst->guest_R1;
   canonical->arg3  = gst->guest_R2;
   canonical->arg4  = gst->guest_R3;
   canonical->arg5  = gst->guest_R4;
   canonical->arg6  = gst->guest_R5;
   canonical->arg7  = 0;
   canonical->arg8  = 0;

#elif defined(VGP_arm64_linux)
   VexGuestARM64State* gst = (VexGuestARM64State*)gst_vanilla;
   canonical->sysno = gst->guest_X8;
   canonical->arg1  = gst->guest_X0;
   canonical->arg2  = gst->guest_X1;
   canonical->arg3  = gst->guest_X2;
   canonical->arg4  = gst->guest_X3;
   canonical->arg5  = gst->guest_X4;
   canonical->arg6  = gst->guest_X5;
   canonical->arg7  = 0;
   canonical->arg8  = 0;

#elif defined(VGP_mips32_linux)
   VexGuestMIPS32State* gst = (VexGuestMIPS32State*)gst_vanilla;
   canonical->sysno = gst->guest_r2;    
   if (canonical->sysno == __NR_exit) {
      canonical->arg1 = gst->guest_r4;    
      canonical->arg2 = 0;
      canonical->arg3 = 0;
      canonical->arg4 = 0;
      canonical->arg5 = 0;
      canonical->arg6 = 0;
      canonical->arg8 = 0;
   } else if (canonical->sysno != __NR_syscall) {
      canonical->arg1  = gst->guest_r4;    
      canonical->arg2  = gst->guest_r5;    
      canonical->arg3  = gst->guest_r6;    
      canonical->arg4  = gst->guest_r7;    
      canonical->arg5  = *((UInt*) (gst->guest_r29 + 16));    
      canonical->arg6  = *((UInt*) (gst->guest_r29 + 20));    
      canonical->arg8 = 0;
   } else {
      
      canonical->sysno = gst->guest_r4;    
      canonical->arg1  = gst->guest_r5;    
      canonical->arg2  = gst->guest_r6;    
      canonical->arg3  = gst->guest_r7;    
      canonical->arg4  = *((UInt*) (gst->guest_r29 + 16));    
      canonical->arg5  = *((UInt*) (gst->guest_r29 + 20));    
      canonical->arg6  = *((UInt*) (gst->guest_r29 + 24));    
      canonical->arg8 = __NR_syscall;
   }

#elif defined(VGP_mips64_linux)
   VexGuestMIPS64State* gst = (VexGuestMIPS64State*)gst_vanilla;
   canonical->sysno = gst->guest_r2;    
   canonical->arg1  = gst->guest_r4;    
   canonical->arg2  = gst->guest_r5;    
   canonical->arg3  = gst->guest_r6;    
   canonical->arg4  = gst->guest_r7;    
   canonical->arg5  = gst->guest_r8;    
   canonical->arg6  = gst->guest_r9;    

#elif defined(VGP_x86_darwin)
   VexGuestX86State* gst = (VexGuestX86State*)gst_vanilla;
   UWord *stack = (UWord *)gst->guest_ESP;
   
   canonical->sysno = gst->guest_EAX;
   if (canonical->sysno != 0) {
      
      canonical->arg1  = stack[1];
      canonical->arg2  = stack[2];
      canonical->arg3  = stack[3];
      canonical->arg4  = stack[4];
      canonical->arg5  = stack[5];
      canonical->arg6  = stack[6];
      canonical->arg7  = stack[7];
      canonical->arg8  = stack[8];
   } else {
      
      
      
      
      
      
      canonical->sysno = stack[1];
      vg_assert(canonical->sysno != 0);
      canonical->arg1  = stack[2];
      canonical->arg2  = stack[3];
      canonical->arg3  = stack[4];
      canonical->arg4  = stack[5];
      canonical->arg5  = stack[6];
      canonical->arg6  = stack[7];
      canonical->arg7  = stack[8];
      canonical->arg8  = stack[9];
      
      PRINT("SYSCALL[%d,?](0) syscall(%s, ...); please stand by...\n",
            VG_(getpid)(), 
            VG_SYSNUM_STRING(canonical->sysno));
   }

   
   
   
   
   
   
   switch (trc) {
   case VEX_TRC_JMP_SYS_INT128:
      
      vg_assert(canonical->sysno >= 0);
      canonical->sysno = VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(canonical->sysno);
      break;
   case VEX_TRC_JMP_SYS_SYSENTER:
      
      
      if (canonical->sysno >= 0) {
         
         canonical->sysno = VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(canonical->sysno
                                                             & 0xffff);
      } else {
         canonical->sysno = VG_DARWIN_SYSCALL_CONSTRUCT_MACH(-canonical->sysno);
      }
      break;
   case VEX_TRC_JMP_SYS_INT129:
      
      vg_assert(canonical->sysno < 0);
      canonical->sysno = VG_DARWIN_SYSCALL_CONSTRUCT_MACH(-canonical->sysno);
      break;
   case VEX_TRC_JMP_SYS_INT130:
      
      vg_assert(canonical->sysno >= 0);
      canonical->sysno = VG_DARWIN_SYSCALL_CONSTRUCT_MDEP(canonical->sysno);
      break;
   default: 
      vg_assert(0);
      break;
   }
   
#elif defined(VGP_amd64_darwin)
   VexGuestAMD64State* gst = (VexGuestAMD64State*)gst_vanilla;
   UWord *stack = (UWord *)gst->guest_RSP;

   vg_assert(trc == VEX_TRC_JMP_SYS_SYSCALL);

   
   canonical->sysno = gst->guest_RAX;
   if (canonical->sysno != __NR_syscall) {
      
      canonical->arg1  = gst->guest_RDI;
      canonical->arg2  = gst->guest_RSI;
      canonical->arg3  = gst->guest_RDX;
      canonical->arg4  = gst->guest_R10;  
      canonical->arg5  = gst->guest_R8;
      canonical->arg6  = gst->guest_R9;
      canonical->arg7  = stack[1];
      canonical->arg8  = stack[2];
   } else {
      
      
      
      
      
      
      canonical->sysno = VG_DARWIN_SYSCALL_CONSTRUCT_UNIX(gst->guest_RDI);
      vg_assert(canonical->sysno != __NR_syscall);
      canonical->arg1  = gst->guest_RSI;
      canonical->arg2  = gst->guest_RDX;
      canonical->arg3  = gst->guest_R10;  
      canonical->arg4  = gst->guest_R8;
      canonical->arg5  = gst->guest_R9;
      canonical->arg6  = stack[1];
      canonical->arg7  = stack[2];
      canonical->arg8  = stack[3];
      
      PRINT("SYSCALL[%d,?](0) syscall(%s, ...); please stand by...\n",
            VG_(getpid)(), 
            VG_SYSNUM_STRING(canonical->sysno));
   }

   

#elif defined(VGP_s390x_linux)
   VexGuestS390XState* gst = (VexGuestS390XState*)gst_vanilla;
   canonical->sysno = gst->guest_SYSNO;
   canonical->arg1  = gst->guest_r2;
   canonical->arg2  = gst->guest_r3;
   canonical->arg3  = gst->guest_r4;
   canonical->arg4  = gst->guest_r5;
   canonical->arg5  = gst->guest_r6;
   canonical->arg6  = gst->guest_r7;
   canonical->arg7  = 0;
   canonical->arg8  = 0;

#elif defined(VGP_tilegx_linux)
   VexGuestTILEGXState* gst = (VexGuestTILEGXState*)gst_vanilla;
   canonical->sysno = gst->guest_r10;
   canonical->arg1  = gst->guest_r0;
   canonical->arg2  = gst->guest_r1;
   canonical->arg3  = gst->guest_r2;
   canonical->arg4  = gst->guest_r3;
   canonical->arg5  = gst->guest_r4;
   canonical->arg6  = gst->guest_r5;
   canonical->arg7  = 0;
   canonical->arg8  = 0;

#else
#  error "getSyscallArgsFromGuestState: unknown arch"
#endif
}

static 
void putSyscallArgsIntoGuestState (  SyscallArgs*       canonical,
                                    VexGuestArchState* gst_vanilla )
{
#if defined(VGP_x86_linux)
   VexGuestX86State* gst = (VexGuestX86State*)gst_vanilla;
   gst->guest_EAX = canonical->sysno;
   gst->guest_EBX = canonical->arg1;
   gst->guest_ECX = canonical->arg2;
   gst->guest_EDX = canonical->arg3;
   gst->guest_ESI = canonical->arg4;
   gst->guest_EDI = canonical->arg5;
   gst->guest_EBP = canonical->arg6;

#elif defined(VGP_amd64_linux)
   VexGuestAMD64State* gst = (VexGuestAMD64State*)gst_vanilla;
   gst->guest_RAX = canonical->sysno;
   gst->guest_RDI = canonical->arg1;
   gst->guest_RSI = canonical->arg2;
   gst->guest_RDX = canonical->arg3;
   gst->guest_R10 = canonical->arg4;
   gst->guest_R8  = canonical->arg5;
   gst->guest_R9  = canonical->arg6;

#elif defined(VGP_ppc32_linux)
   VexGuestPPC32State* gst = (VexGuestPPC32State*)gst_vanilla;
   gst->guest_GPR0 = canonical->sysno;
   gst->guest_GPR3 = canonical->arg1;
   gst->guest_GPR4 = canonical->arg2;
   gst->guest_GPR5 = canonical->arg3;
   gst->guest_GPR6 = canonical->arg4;
   gst->guest_GPR7 = canonical->arg5;
   gst->guest_GPR8 = canonical->arg6;

#elif defined(VGP_ppc64be_linux) || defined(VGP_ppc64le_linux)
   VexGuestPPC64State* gst = (VexGuestPPC64State*)gst_vanilla;
   gst->guest_GPR0 = canonical->sysno;
   gst->guest_GPR3 = canonical->arg1;
   gst->guest_GPR4 = canonical->arg2;
   gst->guest_GPR5 = canonical->arg3;
   gst->guest_GPR6 = canonical->arg4;
   gst->guest_GPR7 = canonical->arg5;
   gst->guest_GPR8 = canonical->arg6;

#elif defined(VGP_arm_linux)
   VexGuestARMState* gst = (VexGuestARMState*)gst_vanilla;
   gst->guest_R7 = canonical->sysno;
   gst->guest_R0 = canonical->arg1;
   gst->guest_R1 = canonical->arg2;
   gst->guest_R2 = canonical->arg3;
   gst->guest_R3 = canonical->arg4;
   gst->guest_R4 = canonical->arg5;
   gst->guest_R5 = canonical->arg6;

#elif defined(VGP_arm64_linux)
   VexGuestARM64State* gst = (VexGuestARM64State*)gst_vanilla;
   gst->guest_X8 = canonical->sysno;
   gst->guest_X0 = canonical->arg1;
   gst->guest_X1 = canonical->arg2;
   gst->guest_X2 = canonical->arg3;
   gst->guest_X3 = canonical->arg4;
   gst->guest_X4 = canonical->arg5;
   gst->guest_X5 = canonical->arg6;

#elif defined(VGP_x86_darwin)
   VexGuestX86State* gst = (VexGuestX86State*)gst_vanilla;
   UWord *stack = (UWord *)gst->guest_ESP;

   gst->guest_EAX = VG_DARWIN_SYSNO_FOR_KERNEL(canonical->sysno);

   
   
   stack[1] = canonical->arg1;
   stack[2] = canonical->arg2;
   stack[3] = canonical->arg3;
   stack[4] = canonical->arg4;
   stack[5] = canonical->arg5;
   stack[6] = canonical->arg6;
   stack[7] = canonical->arg7;
   stack[8] = canonical->arg8;
   
#elif defined(VGP_amd64_darwin)
   VexGuestAMD64State* gst = (VexGuestAMD64State*)gst_vanilla;
   UWord *stack = (UWord *)gst->guest_RSP;

   gst->guest_RAX = VG_DARWIN_SYSNO_FOR_KERNEL(canonical->sysno);
   

   
   gst->guest_RDI = canonical->arg1;
   gst->guest_RSI = canonical->arg2;
   gst->guest_RDX = canonical->arg3;
   gst->guest_RCX = canonical->arg4;
   gst->guest_R8  = canonical->arg5;
   gst->guest_R9  = canonical->arg6;
   stack[1]       = canonical->arg7;
   stack[2]       = canonical->arg8;

#elif defined(VGP_s390x_linux)
   VexGuestS390XState* gst = (VexGuestS390XState*)gst_vanilla;
   gst->guest_SYSNO  = canonical->sysno;
   gst->guest_r2     = canonical->arg1;
   gst->guest_r3     = canonical->arg2;
   gst->guest_r4     = canonical->arg3;
   gst->guest_r5     = canonical->arg4;
   gst->guest_r6     = canonical->arg5;
   gst->guest_r7     = canonical->arg6;

#elif defined(VGP_mips32_linux)
   VexGuestMIPS32State* gst = (VexGuestMIPS32State*)gst_vanilla;
   if (canonical->arg8 != __NR_syscall) {
      gst->guest_r2 = canonical->sysno;
      gst->guest_r4 = canonical->arg1;
      gst->guest_r5 = canonical->arg2;
      gst->guest_r6 = canonical->arg3;
      gst->guest_r7 = canonical->arg4;
      *((UInt*) (gst->guest_r29 + 16)) = canonical->arg5; 
      *((UInt*) (gst->guest_r29 + 20)) = canonical->arg6; 
   } else {
      canonical->arg8 = 0;
      gst->guest_r2 = __NR_syscall;
      gst->guest_r4 = canonical->sysno;
      gst->guest_r5 = canonical->arg1;
      gst->guest_r6 = canonical->arg2;
      gst->guest_r7 = canonical->arg3;
      *((UInt*) (gst->guest_r29 + 16)) = canonical->arg4; 
      *((UInt*) (gst->guest_r29 + 20)) = canonical->arg5; 
      *((UInt*) (gst->guest_r29 + 24)) = canonical->arg6; 
   }

#elif defined(VGP_mips64_linux)
   VexGuestMIPS64State* gst = (VexGuestMIPS64State*)gst_vanilla;
   gst->guest_r2 = canonical->sysno;
   gst->guest_r4 = canonical->arg1;
   gst->guest_r5 = canonical->arg2;
   gst->guest_r6 = canonical->arg3;
   gst->guest_r7 = canonical->arg4;
   gst->guest_r8 = canonical->arg5;
   gst->guest_r9 = canonical->arg6;

#elif defined(VGP_tilegx_linux)
   VexGuestTILEGXState* gst = (VexGuestTILEGXState*)gst_vanilla;
   gst->guest_r10 = canonical->sysno;
   gst->guest_r0 = canonical->arg1;
   gst->guest_r1 = canonical->arg2;
   gst->guest_r2 = canonical->arg3;
   gst->guest_r3 = canonical->arg4;
   gst->guest_r4 = canonical->arg5;
   gst->guest_r5 = canonical->arg6;

#else
#  error "putSyscallArgsIntoGuestState: unknown arch"
#endif
}

static
void getSyscallStatusFromGuestState ( SyscallStatus*     canonical,
                                       VexGuestArchState* gst_vanilla )
{
#  if defined(VGP_x86_linux)
   VexGuestX86State* gst = (VexGuestX86State*)gst_vanilla;
   canonical->sres = VG_(mk_SysRes_x86_linux)( gst->guest_EAX );
   canonical->what = SsComplete;

#  elif defined(VGP_amd64_linux)
   VexGuestAMD64State* gst = (VexGuestAMD64State*)gst_vanilla;
   canonical->sres = VG_(mk_SysRes_amd64_linux)( gst->guest_RAX );
   canonical->what = SsComplete;

#  elif defined(VGP_ppc32_linux)
   VexGuestPPC32State* gst   = (VexGuestPPC32State*)gst_vanilla;
   UInt                cr    = LibVEX_GuestPPC32_get_CR( gst );
   UInt                cr0so = (cr >> 28) & 1;
   canonical->sres = VG_(mk_SysRes_ppc32_linux)( gst->guest_GPR3, cr0so );
   canonical->what = SsComplete;

#  elif defined(VGP_ppc64be_linux) || defined(VGP_ppc64le_linux)
   VexGuestPPC64State* gst   = (VexGuestPPC64State*)gst_vanilla;
   UInt                cr    = LibVEX_GuestPPC64_get_CR( gst );
   UInt                cr0so = (cr >> 28) & 1;
   canonical->sres = VG_(mk_SysRes_ppc64_linux)( gst->guest_GPR3, cr0so );
   canonical->what = SsComplete;

#  elif defined(VGP_arm_linux)
   VexGuestARMState* gst = (VexGuestARMState*)gst_vanilla;
   canonical->sres = VG_(mk_SysRes_arm_linux)( gst->guest_R0 );
   canonical->what = SsComplete;

#  elif defined(VGP_arm64_linux)
   VexGuestARM64State* gst = (VexGuestARM64State*)gst_vanilla;
   canonical->sres = VG_(mk_SysRes_arm64_linux)( gst->guest_X0 );
   canonical->what = SsComplete;

#  elif defined(VGP_mips32_linux)
   VexGuestMIPS32State* gst = (VexGuestMIPS32State*)gst_vanilla;
   UInt                v0 = gst->guest_r2;    
   UInt                v1 = gst->guest_r3;    
   UInt                a3 = gst->guest_r7;    
   canonical->sres = VG_(mk_SysRes_mips32_linux)( v0, v1, a3 );
   canonical->what = SsComplete;

#  elif defined(VGP_mips64_linux)
   VexGuestMIPS64State* gst = (VexGuestMIPS64State*)gst_vanilla;
   ULong                v0 = gst->guest_r2;    
   ULong                v1 = gst->guest_r3;    
   ULong                a3 = gst->guest_r7;    
   canonical->sres = VG_(mk_SysRes_mips64_linux)(v0, v1, a3);
   canonical->what = SsComplete;

#  elif defined(VGP_x86_darwin)
   
   VexGuestX86State* gst = (VexGuestX86State*)gst_vanilla;
   UInt carry = 1 & LibVEX_GuestX86_get_eflags(gst);
   UInt err = 0;
   UInt wLO = 0;
   UInt wHI = 0;
   switch (gst->guest_SC_CLASS) {
      case VG_DARWIN_SYSCALL_CLASS_UNIX:
         
         err = carry;
         wLO = gst->guest_EAX;
         wHI = gst->guest_EDX;
         break;
      case VG_DARWIN_SYSCALL_CLASS_MACH:
         
         wLO = gst->guest_EAX;
         break;
      case VG_DARWIN_SYSCALL_CLASS_MDEP:
         
         wLO = gst->guest_EAX;
         break;
      default: 
         vg_assert(0);
         break;
   }
   canonical->sres = VG_(mk_SysRes_x86_darwin)(
                        gst->guest_SC_CLASS, err ? True : False, 
                        wHI, wLO
                     );
   canonical->what = SsComplete;

#  elif defined(VGP_amd64_darwin)
   
   VexGuestAMD64State* gst = (VexGuestAMD64State*)gst_vanilla;
   ULong carry = 1 & LibVEX_GuestAMD64_get_rflags(gst);
   ULong err = 0;
   ULong wLO = 0;
   ULong wHI = 0;
   switch (gst->guest_SC_CLASS) {
      case VG_DARWIN_SYSCALL_CLASS_UNIX:
         
         err = carry;
         wLO = gst->guest_RAX;
         wHI = gst->guest_RDX;
         break;
      case VG_DARWIN_SYSCALL_CLASS_MACH:
         
         wLO = gst->guest_RAX;
         break;
      case VG_DARWIN_SYSCALL_CLASS_MDEP:
         
         wLO = gst->guest_RAX;
         break;
      default: 
         vg_assert(0);
         break;
   }
   canonical->sres = VG_(mk_SysRes_amd64_darwin)(
                        gst->guest_SC_CLASS, err ? True : False, 
                        wHI, wLO
                     );
   canonical->what = SsComplete;

#  elif defined(VGP_s390x_linux)
   VexGuestS390XState* gst   = (VexGuestS390XState*)gst_vanilla;
   canonical->sres = VG_(mk_SysRes_s390x_linux)( gst->guest_r2 );
   canonical->what = SsComplete;

#  elif defined(VGP_tilegx_linux)
   VexGuestTILEGXState* gst = (VexGuestTILEGXState*)gst_vanilla;
   canonical->sres = VG_(mk_SysRes_tilegx_linux)( gst->guest_r0 );
   canonical->what = SsComplete;

#  else
#    error "getSyscallStatusFromGuestState: unknown arch"
#  endif
}

static 
void putSyscallStatusIntoGuestState (  ThreadId tid, 
                                       SyscallStatus*     canonical,
                                      VexGuestArchState* gst_vanilla )
{
#  if defined(VGP_x86_linux)
   VexGuestX86State* gst = (VexGuestX86State*)gst_vanilla;
   vg_assert(canonical->what == SsComplete);
   if (sr_isError(canonical->sres)) {
      gst->guest_EAX = - (Int)sr_Err(canonical->sres);
   } else {
      gst->guest_EAX = sr_Res(canonical->sres);
   }
   VG_TRACK( post_reg_write, Vg_CoreSysCall, tid, 
             OFFSET_x86_EAX, sizeof(UWord) );

#  elif defined(VGP_amd64_linux)
   VexGuestAMD64State* gst = (VexGuestAMD64State*)gst_vanilla;
   vg_assert(canonical->what == SsComplete);
   if (sr_isError(canonical->sres)) {
      gst->guest_RAX = - (Long)sr_Err(canonical->sres);
   } else {
      gst->guest_RAX = sr_Res(canonical->sres);
   }
   VG_TRACK( post_reg_write, Vg_CoreSysCall, tid, 
             OFFSET_amd64_RAX, sizeof(UWord) );

#  elif defined(VGP_ppc32_linux)
   VexGuestPPC32State* gst = (VexGuestPPC32State*)gst_vanilla;
   UInt old_cr = LibVEX_GuestPPC32_get_CR(gst);
   vg_assert(canonical->what == SsComplete);
   if (sr_isError(canonical->sres)) {
      
      LibVEX_GuestPPC32_put_CR( old_cr | (1<<28), gst );
      gst->guest_GPR3 = sr_Err(canonical->sres);
   } else {
      
      LibVEX_GuestPPC32_put_CR( old_cr & ~(1<<28), gst );
      gst->guest_GPR3 = sr_Res(canonical->sres);
   }
   VG_TRACK( post_reg_write, Vg_CoreSysCall, tid, 
             OFFSET_ppc32_GPR3, sizeof(UWord) );
   VG_TRACK( post_reg_write, Vg_CoreSysCall, tid, 
             OFFSET_ppc32_CR0_0, sizeof(UChar) );

#  elif defined(VGP_ppc64be_linux) || defined(VGP_ppc64le_linux)
   VexGuestPPC64State* gst = (VexGuestPPC64State*)gst_vanilla;
   UInt old_cr = LibVEX_GuestPPC64_get_CR(gst);
   vg_assert(canonical->what == SsComplete);
   if (sr_isError(canonical->sres)) {
      
      LibVEX_GuestPPC64_put_CR( old_cr | (1<<28), gst );
      gst->guest_GPR3 = sr_Err(canonical->sres);
   } else {
      
      LibVEX_GuestPPC64_put_CR( old_cr & ~(1<<28), gst );
      gst->guest_GPR3 = sr_Res(canonical->sres);
   }
   VG_TRACK( post_reg_write, Vg_CoreSysCall, tid, 
             OFFSET_ppc64_GPR3, sizeof(UWord) );
   VG_TRACK( post_reg_write, Vg_CoreSysCall, tid, 
             OFFSET_ppc64_CR0_0, sizeof(UChar) );

#  elif defined(VGP_arm_linux)
   VexGuestARMState* gst = (VexGuestARMState*)gst_vanilla;
   vg_assert(canonical->what == SsComplete);
   if (sr_isError(canonical->sres)) {
      gst->guest_R0 = - (Int)sr_Err(canonical->sres);
   } else {
      gst->guest_R0 = sr_Res(canonical->sres);
   }
   VG_TRACK( post_reg_write, Vg_CoreSysCall, tid, 
             OFFSET_arm_R0, sizeof(UWord) );

#  elif defined(VGP_arm64_linux)
   VexGuestARM64State* gst = (VexGuestARM64State*)gst_vanilla;
   vg_assert(canonical->what == SsComplete);
   if (sr_isError(canonical->sres)) {
      gst->guest_X0 = - (Long)sr_Err(canonical->sres);
   } else {
      gst->guest_X0 = sr_Res(canonical->sres);
   }
   VG_TRACK( post_reg_write, Vg_CoreSysCall, tid, 
             OFFSET_arm64_X0, sizeof(UWord) );

#elif defined(VGP_x86_darwin)
   VexGuestX86State* gst = (VexGuestX86State*)gst_vanilla;
   SysRes sres = canonical->sres;
   vg_assert(canonical->what == SsComplete);
   switch (sres._mode) {
      case SysRes_MACH: 
      case SysRes_MDEP: 
         gst->guest_EAX = sres._wLO;
         VG_TRACK( post_reg_write, Vg_CoreSysCall, tid, 
                   OFFSET_x86_EAX, sizeof(UInt) );
         break;
      case SysRes_UNIX_OK:  
      case SysRes_UNIX_ERR: 
         gst->guest_EAX = sres._wLO;
         VG_TRACK( post_reg_write, Vg_CoreSysCall, tid, 
                   OFFSET_x86_EAX, sizeof(UInt) );
         gst->guest_EDX = sres._wHI;
         VG_TRACK( post_reg_write, Vg_CoreSysCall, tid, 
                   OFFSET_x86_EDX, sizeof(UInt) );
         LibVEX_GuestX86_put_eflag_c( sres._mode==SysRes_UNIX_ERR ? 1 : 0,
                                      gst );
         
         
         VG_TRACK( post_reg_write, Vg_CoreSysCall, tid, 
                   offsetof(VexGuestX86State, guest_CC_DEP1), sizeof(UInt) );
         break;
      default: 
         vg_assert(0);
         break;
   }
   
#elif defined(VGP_amd64_darwin)
   VexGuestAMD64State* gst = (VexGuestAMD64State*)gst_vanilla;
   SysRes sres = canonical->sres;
   vg_assert(canonical->what == SsComplete);
   switch (sres._mode) {
      case SysRes_MACH: 
      case SysRes_MDEP: 
         gst->guest_RAX = sres._wLO;
         VG_TRACK( post_reg_write, Vg_CoreSysCall, tid, 
                   OFFSET_amd64_RAX, sizeof(ULong) );
         break;
      case SysRes_UNIX_OK:  
      case SysRes_UNIX_ERR: 
         gst->guest_RAX = sres._wLO;
         VG_TRACK( post_reg_write, Vg_CoreSysCall, tid, 
                   OFFSET_amd64_RAX, sizeof(ULong) );
         gst->guest_RDX = sres._wHI;
         VG_TRACK( post_reg_write, Vg_CoreSysCall, tid, 
                   OFFSET_amd64_RDX, sizeof(ULong) );
         LibVEX_GuestAMD64_put_rflag_c( sres._mode==SysRes_UNIX_ERR ? 1 : 0,
                                        gst );
         
         
         VG_TRACK( post_reg_write, Vg_CoreSysCall, tid, 
                   offsetof(VexGuestAMD64State, guest_CC_DEP1), sizeof(ULong) );
         break;
      default: 
         vg_assert(0);
         break;
   }
   
#  elif defined(VGP_s390x_linux)
   VexGuestS390XState* gst = (VexGuestS390XState*)gst_vanilla;
   vg_assert(canonical->what == SsComplete);
   if (sr_isError(canonical->sres)) {
      gst->guest_r2 = - (Long)sr_Err(canonical->sres);
   } else {
      gst->guest_r2 = sr_Res(canonical->sres);
   }

#  elif defined(VGP_mips32_linux)
   VexGuestMIPS32State* gst = (VexGuestMIPS32State*)gst_vanilla;
   vg_assert(canonical->what == SsComplete);
   if (sr_isError(canonical->sres)) {
      gst->guest_r2 = (Int)sr_Err(canonical->sres);
      gst->guest_r7 = (Int)sr_Err(canonical->sres);
   } else {
      gst->guest_r2 = sr_Res(canonical->sres);
      gst->guest_r3 = sr_ResEx(canonical->sres);
      gst->guest_r7 = (Int)sr_Err(canonical->sres);
   }
   VG_TRACK( post_reg_write, Vg_CoreSysCall, tid,
             OFFSET_mips32_r2, sizeof(UWord) );
   VG_TRACK( post_reg_write, Vg_CoreSysCall, tid,
             OFFSET_mips32_r3, sizeof(UWord) );
   VG_TRACK( post_reg_write, Vg_CoreSysCall, tid,
             OFFSET_mips32_r7, sizeof(UWord) );

#  elif defined(VGP_mips64_linux)
   VexGuestMIPS64State* gst = (VexGuestMIPS64State*)gst_vanilla;
   vg_assert(canonical->what == SsComplete);
   if (sr_isError(canonical->sres)) {
      gst->guest_r2 = (Int)sr_Err(canonical->sres);
      gst->guest_r7 = (Int)sr_Err(canonical->sres);
   } else {
      gst->guest_r2 = sr_Res(canonical->sres);
      gst->guest_r3 = sr_ResEx(canonical->sres);
      gst->guest_r7 = (Int)sr_Err(canonical->sres);
   }
   VG_TRACK( post_reg_write, Vg_CoreSysCall, tid,
             OFFSET_mips64_r2, sizeof(UWord) );
   VG_TRACK( post_reg_write, Vg_CoreSysCall, tid,
             OFFSET_mips64_r3, sizeof(UWord) );
   VG_TRACK( post_reg_write, Vg_CoreSysCall, tid,
             OFFSET_mips64_r7, sizeof(UWord) );

#  elif defined(VGP_tilegx_linux)
   VexGuestTILEGXState* gst = (VexGuestTILEGXState*)gst_vanilla;
   vg_assert(canonical->what == SsComplete);
   if (sr_isError(canonical->sres)) {
      gst->guest_r0 = - (Long)sr_Err(canonical->sres);
      
      gst->guest_r1 = (Long)sr_Err(canonical->sres);
   } else {
      gst->guest_r0 = sr_Res(canonical->sres);
      gst->guest_r1 = 0;
   }

#  else
#    error "putSyscallStatusIntoGuestState: unknown arch"
#  endif
}



static
void getSyscallArgLayout ( SyscallArgLayout* layout )
{
   VG_(bzero_inline)(layout, sizeof(*layout));

#if defined(VGP_x86_linux)
   layout->o_sysno  = OFFSET_x86_EAX;
   layout->o_arg1   = OFFSET_x86_EBX;
   layout->o_arg2   = OFFSET_x86_ECX;
   layout->o_arg3   = OFFSET_x86_EDX;
   layout->o_arg4   = OFFSET_x86_ESI;
   layout->o_arg5   = OFFSET_x86_EDI;
   layout->o_arg6   = OFFSET_x86_EBP;
   layout->uu_arg7  = -1; 
   layout->uu_arg8  = -1; 

#elif defined(VGP_amd64_linux)
   layout->o_sysno  = OFFSET_amd64_RAX;
   layout->o_arg1   = OFFSET_amd64_RDI;
   layout->o_arg2   = OFFSET_amd64_RSI;
   layout->o_arg3   = OFFSET_amd64_RDX;
   layout->o_arg4   = OFFSET_amd64_R10;
   layout->o_arg5   = OFFSET_amd64_R8;
   layout->o_arg6   = OFFSET_amd64_R9;
   layout->uu_arg7  = -1; 
   layout->uu_arg8  = -1; 

#elif defined(VGP_ppc32_linux)
   layout->o_sysno  = OFFSET_ppc32_GPR0;
   layout->o_arg1   = OFFSET_ppc32_GPR3;
   layout->o_arg2   = OFFSET_ppc32_GPR4;
   layout->o_arg3   = OFFSET_ppc32_GPR5;
   layout->o_arg4   = OFFSET_ppc32_GPR6;
   layout->o_arg5   = OFFSET_ppc32_GPR7;
   layout->o_arg6   = OFFSET_ppc32_GPR8;
   layout->uu_arg7  = -1; 
   layout->uu_arg8  = -1; 

#elif defined(VGP_ppc64be_linux) || defined(VGP_ppc64le_linux)
   layout->o_sysno  = OFFSET_ppc64_GPR0;
   layout->o_arg1   = OFFSET_ppc64_GPR3;
   layout->o_arg2   = OFFSET_ppc64_GPR4;
   layout->o_arg3   = OFFSET_ppc64_GPR5;
   layout->o_arg4   = OFFSET_ppc64_GPR6;
   layout->o_arg5   = OFFSET_ppc64_GPR7;
   layout->o_arg6   = OFFSET_ppc64_GPR8;
   layout->uu_arg7  = -1; 
   layout->uu_arg8  = -1; 

#elif defined(VGP_arm_linux)
   layout->o_sysno  = OFFSET_arm_R7;
   layout->o_arg1   = OFFSET_arm_R0;
   layout->o_arg2   = OFFSET_arm_R1;
   layout->o_arg3   = OFFSET_arm_R2;
   layout->o_arg4   = OFFSET_arm_R3;
   layout->o_arg5   = OFFSET_arm_R4;
   layout->o_arg6   = OFFSET_arm_R5;
   layout->uu_arg7  = -1; 
   layout->uu_arg8  = -1; 

#elif defined(VGP_arm64_linux)
   layout->o_sysno  = OFFSET_arm64_X8;
   layout->o_arg1   = OFFSET_arm64_X0;
   layout->o_arg2   = OFFSET_arm64_X1;
   layout->o_arg3   = OFFSET_arm64_X2;
   layout->o_arg4   = OFFSET_arm64_X3;
   layout->o_arg5   = OFFSET_arm64_X4;
   layout->o_arg6   = OFFSET_arm64_X5;
   layout->uu_arg7  = -1; 
   layout->uu_arg8  = -1; 

#elif defined(VGP_mips32_linux)
   layout->o_sysno  = OFFSET_mips32_r2;
   layout->o_arg1   = OFFSET_mips32_r4;
   layout->o_arg2   = OFFSET_mips32_r5;
   layout->o_arg3   = OFFSET_mips32_r6;
   layout->o_arg4   = OFFSET_mips32_r7;
   layout->s_arg5   = sizeof(UWord) * 4;
   layout->s_arg6   = sizeof(UWord) * 5;
   layout->uu_arg7  = -1; 
   layout->uu_arg8  = -1; 

#elif defined(VGP_mips64_linux)
   layout->o_sysno  = OFFSET_mips64_r2;
   layout->o_arg1   = OFFSET_mips64_r4;
   layout->o_arg2   = OFFSET_mips64_r5;
   layout->o_arg3   = OFFSET_mips64_r6;
   layout->o_arg4   = OFFSET_mips64_r7;
   layout->o_arg5   = OFFSET_mips64_r8;
   layout->o_arg6   = OFFSET_mips64_r9;
   layout->uu_arg7  = -1; 
   layout->uu_arg8  = -1; 

#elif defined(VGP_x86_darwin)
   layout->o_sysno  = OFFSET_x86_EAX;
   
   layout->s_arg1   = sizeof(UWord) * 1;
   layout->s_arg2   = sizeof(UWord) * 2;
   layout->s_arg3   = sizeof(UWord) * 3;
   layout->s_arg4   = sizeof(UWord) * 4;
   layout->s_arg5   = sizeof(UWord) * 5;
   layout->s_arg6   = sizeof(UWord) * 6;
   layout->s_arg7   = sizeof(UWord) * 7;
   layout->s_arg8   = sizeof(UWord) * 8;
   
#elif defined(VGP_amd64_darwin)
   layout->o_sysno  = OFFSET_amd64_RAX;
   layout->o_arg1   = OFFSET_amd64_RDI;
   layout->o_arg2   = OFFSET_amd64_RSI;
   layout->o_arg3   = OFFSET_amd64_RDX;
   layout->o_arg4   = OFFSET_amd64_RCX;
   layout->o_arg5   = OFFSET_amd64_R8;
   layout->o_arg6   = OFFSET_amd64_R9;
   layout->s_arg7   = sizeof(UWord) * 1;
   layout->s_arg8   = sizeof(UWord) * 2;

#elif defined(VGP_s390x_linux)
   layout->o_sysno  = OFFSET_s390x_SYSNO;
   layout->o_arg1   = OFFSET_s390x_r2;
   layout->o_arg2   = OFFSET_s390x_r3;
   layout->o_arg3   = OFFSET_s390x_r4;
   layout->o_arg4   = OFFSET_s390x_r5;
   layout->o_arg5   = OFFSET_s390x_r6;
   layout->o_arg6   = OFFSET_s390x_r7;
   layout->uu_arg7  = -1; 
   layout->uu_arg8  = -1; 
#elif defined(VGP_tilegx_linux)
   layout->o_sysno  = OFFSET_tilegx_r(10);
   layout->o_arg1   = OFFSET_tilegx_r(0);
   layout->o_arg2   = OFFSET_tilegx_r(1);
   layout->o_arg3   = OFFSET_tilegx_r(2);
   layout->o_arg4   = OFFSET_tilegx_r(3);
   layout->o_arg5   = OFFSET_tilegx_r(4);
   layout->o_arg6   = OFFSET_tilegx_r(5);
   layout->uu_arg7  = -1; 
   layout->uu_arg8  = -1; 

#else
#  error "getSyscallLayout: unknown arch"
#endif
}




static 
void bad_before ( ThreadId              tid,
                  SyscallArgLayout*     layout,
                  SyscallArgs*   args,
                  SyscallStatus* status,
                  UWord*         flags )
{
   VG_(dmsg)("WARNING: unhandled %s syscall: %s\n",
      VG_PLATFORM, VG_SYSNUM_STRING(args->sysno));
   if (VG_(clo_verbosity) > 1) {
      VG_(get_and_pp_StackTrace)(tid, VG_(clo_backtrace_size));
   }
   VG_(dmsg)("You may be able to write your own handler.\n");
   VG_(dmsg)("Read the file README_MISSING_SYSCALL_OR_IOCTL.\n");
   VG_(dmsg)("Nevertheless we consider this a bug.  Please report\n");
   VG_(dmsg)("it at http://valgrind.org/support/bug_reports.html.\n");

   SET_STATUS_Failure(VKI_ENOSYS);
}

static SyscallTableEntry bad_sys =
   { bad_before, NULL };

static const SyscallTableEntry* get_syscall_entry ( Int syscallno )
{
   const SyscallTableEntry* sys = NULL;

#  if defined(VGO_linux)
   sys = ML_(get_linux_syscall_entry)( syscallno );

#  elif defined(VGO_darwin)
   Int idx = VG_DARWIN_SYSNO_INDEX(syscallno);

   switch (VG_DARWIN_SYSNO_CLASS(syscallno)) {
   case VG_DARWIN_SYSCALL_CLASS_UNIX:
      if (idx >= 0 && idx < ML_(syscall_table_size) &&
          ML_(syscall_table)[idx].before != NULL)
         sys = &ML_(syscall_table)[idx];
         break;
   case VG_DARWIN_SYSCALL_CLASS_MACH:
      if (idx >= 0 && idx < ML_(mach_trap_table_size) &&
          ML_(mach_trap_table)[idx].before != NULL)
         sys = &ML_(mach_trap_table)[idx];
         break;
   case VG_DARWIN_SYSCALL_CLASS_MDEP:
      if (idx >= 0 && idx < ML_(mdep_trap_table_size) &&
          ML_(mdep_trap_table)[idx].before != NULL)
         sys = &ML_(mdep_trap_table)[idx];
         break;
   default: 
      vg_assert(0);
      break;
   }

#  else
#    error Unknown OS
#  endif

   return sys == NULL  ? &bad_sys  : sys;
}


static void sanitize_client_sigmask(vki_sigset_t *mask)
{
   VG_(sigdelset)(mask, VKI_SIGKILL);
   VG_(sigdelset)(mask, VKI_SIGSTOP);
   VG_(sigdelset)(mask, VG_SIGVGKILL); 
}

typedef
   struct {
      SyscallArgs   orig_args;
      SyscallArgs   args;
      SyscallStatus status;
      UWord         flags;
   }
   SyscallInfo;

SyscallInfo *syscallInfo;

void VG_(clear_syscallInfo) ( Int tid )
{
   vg_assert(syscallInfo);
   vg_assert(tid >= 0 && tid < VG_N_THREADS);
   VG_(memset)( & syscallInfo[tid], 0, sizeof( syscallInfo[tid] ));
   syscallInfo[tid].status.what = SsIdle;
}

Bool VG_(is_in_syscall) ( Int tid )
{
   vg_assert(tid >= 0 && tid < VG_N_THREADS);
   return (syscallInfo[tid].status.what != SsIdle);
}

static void ensure_initialised ( void )
{
   Int i;
   static Bool init_done = False;
   if (init_done) 
      return;
   init_done = True;

   syscallInfo = VG_(malloc)("scinfo", VG_N_THREADS * sizeof syscallInfo[0]);

   for (i = 0; i < VG_N_THREADS; i++) {
      VG_(clear_syscallInfo)( i );
   }
}


void VG_(client_syscall) ( ThreadId tid, UInt trc )
{
   Word                     sysno;
   ThreadState*             tst;
   const SyscallTableEntry* ent;
   SyscallArgLayout         layout;
   SyscallInfo*             sci;

   ensure_initialised();

   vg_assert(VG_(is_valid_tid)(tid));
   vg_assert(tid >= 1 && tid < VG_N_THREADS);
   vg_assert(VG_(is_running_thread)(tid));

#  if !defined(VGO_darwin)
   
   vg_assert(VG_(clo_resync_filter) == 0);
#  endif

   tst = VG_(get_ThreadState)(tid);

   
#  if defined(VGO_linux)
   if (tid == 1) {
      Addr     stackMin   = VG_(get_SP)(tid) - VG_STACK_REDZONE_SZB;

      if (VG_(am_addr_is_in_extensible_client_stack)(stackMin))
         VG_(extend_stack)( tid, stackMin );
   }
#  endif
   


   sci = & syscallInfo[tid];
   vg_assert(sci->status.what == SsIdle);

   getSyscallArgsFromGuestState( &sci->orig_args, &tst->arch.vex, trc );

   sci->args = sci->orig_args;

   sysno = sci->orig_args.sysno;

   if (0 && sysno == __NR_ioctl) {
      VG_(umsg)("\nioctl:\n");
      VG_(get_and_pp_StackTrace)(tid, 10);
      VG_(umsg)("\n");
   }

#  if defined(VGO_darwin)
   tst->arch.vex.guest_SC_CLASS = VG_DARWIN_SYSNO_CLASS(sysno);
#  endif

   sci->status.what = SsHandToKernel;
   sci->status.sres = VG_(mk_SysRes_Error)(0);
   sci->flags       = 0;

   ent = get_syscall_entry(sysno);

   getSyscallArgLayout( &layout );

   vg_assert(VG_(iseqsigset)(&tst->sig_mask, &tst->tmp_sig_mask));


   PRINT("SYSCALL[%d,%d](%s) ",
      VG_(getpid)(), tid, VG_SYSNUM_STRING(sysno));

   
   if (VG_(needs).syscall_wrapper) {
      UWord tmpv[8];
      tmpv[0] = sci->orig_args.arg1;
      tmpv[1] = sci->orig_args.arg2;
      tmpv[2] = sci->orig_args.arg3;
      tmpv[3] = sci->orig_args.arg4;
      tmpv[4] = sci->orig_args.arg5;
      tmpv[5] = sci->orig_args.arg6;
      tmpv[6] = sci->orig_args.arg7;
      tmpv[7] = sci->orig_args.arg8;
      VG_TDICT_CALL(tool_pre_syscall, tid, sysno,
                    &tmpv[0], sizeof(tmpv)/sizeof(tmpv[0]));
   }

   vg_assert(ent);
   vg_assert(ent->before);
   (ent->before)( tid,
                  &layout, 
                  &sci->args, &sci->status, &sci->flags );
   
   
   vg_assert(sci->status.what == SsHandToKernel
             || sci->status.what == SsComplete);
   vg_assert(sci->args.sysno == sci->orig_args.sysno);

   if (sci->status.what == SsComplete && !sr_isError(sci->status.sres)) {
      if (sci->flags & SfNoWriteResult) {
         PRINT(" --> [pre-success] NoWriteResult");
      } else {
         PRINT(" --> [pre-success] %s", VG_(sr_as_string)(sci->status.sres));
      }                                      
      vg_assert(0 == (sci->flags 
                      & ~(SfPollAfter | SfYieldAfter | SfNoWriteResult)));
      vg_assert(eq_SyscallArgs(&sci->args, &sci->orig_args));
   }

   else
   if (sci->status.what == SsComplete && sr_isError(sci->status.sres)) {
      
      PRINT(" --> [pre-fail] %s", VG_(sr_as_string)(sci->status.sres));
      vg_assert(0 == (sci->flags & ~(SfMayBlock | SfPostOnFail | SfPollAfter)));
      vg_assert(eq_SyscallArgs(&sci->args, &sci->orig_args));
   }

   else
   if (sci->status.what != SsHandToKernel) {
      
      vg_assert(0);
   }

   else  {

      vg_assert(0 == (sci->flags & ~(SfMayBlock | SfPostOnFail | SfPollAfter)));

      if (sci->flags & SfMayBlock) {

         
         vki_sigset_t mask;

         PRINT(" --> [async] ... \n");

         mask = tst->sig_mask;
         sanitize_client_sigmask(&mask);

         putSyscallArgsIntoGuestState( &sci->args, &tst->arch.vex );

         
         VG_(release_BigLock)(tid, VgTs_WaitSys, "VG_(client_syscall)[async]");

         do_syscall_for_client(sysno, tst, &mask);



         
         VG_(acquire_BigLock)(tid, "VG_(client_syscall)[async]");

         getSyscallStatusFromGuestState( &sci->status, &tst->arch.vex );
         vg_assert(sci->status.what == SsComplete);

         
         if (VG_(clo_trace_syscalls)) {
            PRINT("SYSCALL[%d,%d](%s) ... [async] --> %s",
                  VG_(getpid)(), tid, VG_SYSNUM_STRING(sysno),
                  VG_(sr_as_string)(sci->status.sres));
         }

      } else {

         
         SysRes sres 
            = VG_(do_syscall)(sysno, sci->args.arg1, sci->args.arg2, 
                                     sci->args.arg3, sci->args.arg4, 
                                     sci->args.arg5, sci->args.arg6,
                                     sci->args.arg7, sci->args.arg8 );
         sci->status = convert_SysRes_to_SyscallStatus(sres);

         
         if (VG_(clo_trace_syscalls)) {
           PRINT("[sync] --> %s", VG_(sr_as_string)(sci->status.sres));
         }
      }
   }

   vg_assert(sci->status.what == SsComplete);

   vg_assert(VG_(is_running_thread)(tid));

   if (!(sci->flags & SfNoWriteResult))
      putSyscallStatusIntoGuestState( tid, &sci->status, &tst->arch.vex );

   PRINT(" ");
   VG_(post_syscall)(tid);
   PRINT("\n");
}


void VG_(post_syscall) (ThreadId tid)
{
   SyscallInfo*             sci;
   const SyscallTableEntry* ent;
   SyscallStatus            test_status;
   ThreadState*             tst;
   Word sysno;

   
   vg_assert(VG_(is_valid_tid)(tid));
   vg_assert(tid >= 1 && tid < VG_N_THREADS);
   vg_assert(VG_(is_running_thread)(tid));

   tst = VG_(get_ThreadState)(tid);
   sci = & syscallInfo[tid];

   if (sci->status.what == SsIdle || sci->status.what == SsHandToKernel) {
      sci->status.what = SsIdle;
      return;
   }

   /* Validate current syscallInfo entry.  In particular we require
      that the current .status matches what's actually in the guest
      state.  At least in the normal case where we have actually
      previously written the result into the guest state. */
   vg_assert(sci->status.what == SsComplete);

   getSyscallStatusFromGuestState( &test_status, &tst->arch.vex );
   if (!(sci->flags & SfNoWriteResult))
      vg_assert(eq_SyscallStatus( &sci->status, &test_status ));
   

   vg_assert(sci->args.sysno == sci->orig_args.sysno);
   sysno = sci->args.sysno;
   ent = get_syscall_entry(sysno);

   
   if (ent->after
       && ((!sr_isError(sci->status.sres))
           || (sr_isError(sci->status.sres)
               && (sci->flags & SfPostOnFail) ))) {

      (ent->after)( tid, &sci->args, &sci->status );
   }

   if (!(sci->flags & SfNoWriteResult))
      putSyscallStatusIntoGuestState( tid, &sci->status, &tst->arch.vex );

   
   if (VG_(needs).syscall_wrapper) {
      UWord tmpv[8];
      tmpv[0] = sci->orig_args.arg1;
      tmpv[1] = sci->orig_args.arg2;
      tmpv[2] = sci->orig_args.arg3;
      tmpv[3] = sci->orig_args.arg4;
      tmpv[4] = sci->orig_args.arg5;
      tmpv[5] = sci->orig_args.arg6;
      tmpv[6] = sci->orig_args.arg7;
      tmpv[7] = sci->orig_args.arg8;
      VG_TDICT_CALL(tool_post_syscall, tid, 
                    sysno,
                    &tmpv[0], sizeof(tmpv)/sizeof(tmpv[0]),
                    sci->status.sres);
   }

   
   vg_assert(sci->status.what == SsComplete);
   sci->status.what = SsIdle;

   if (sci->flags & SfPollAfter)
      VG_(poll_signals)(tid);

   if (sci->flags & SfYieldAfter)
      VG_(vg_yield)();
}





#if defined(VGO_linux)
  extern const Addr ML_(blksys_setup);
  extern const Addr ML_(blksys_restart);
  extern const Addr ML_(blksys_complete);
  extern const Addr ML_(blksys_committed);
  extern const Addr ML_(blksys_finished);
#elif defined(VGO_darwin)
  
  extern const Addr ML_(blksys_setup_MACH);
  extern const Addr ML_(blksys_restart_MACH);
  extern const Addr ML_(blksys_complete_MACH);
  extern const Addr ML_(blksys_committed_MACH);
  extern const Addr ML_(blksys_finished_MACH);
  extern const Addr ML_(blksys_setup_MDEP);
  extern const Addr ML_(blksys_restart_MDEP);
  extern const Addr ML_(blksys_complete_MDEP);
  extern const Addr ML_(blksys_committed_MDEP);
  extern const Addr ML_(blksys_finished_MDEP);
  extern const Addr ML_(blksys_setup_UNIX);
  extern const Addr ML_(blksys_restart_UNIX);
  extern const Addr ML_(blksys_complete_UNIX);
  extern const Addr ML_(blksys_committed_UNIX);
  extern const Addr ML_(blksys_finished_UNIX);
#else
# error "Unknown OS"
#endif



void ML_(fixup_guest_state_to_restart_syscall) ( ThreadArchState* arch )
{
#if defined(VGP_x86_linux)
   arch->vex.guest_EIP -= 2;             

   {
      UChar *p = (UChar *)arch->vex.guest_EIP;
      
      if (p[0] != 0xcd || p[1] != 0x80)
         VG_(message)(Vg_DebugMsg,
                      "?! restarting over syscall at %#x %02x %02x\n",
                      arch->vex.guest_EIP, p[0], p[1]); 

      vg_assert(p[0] == 0xcd && p[1] == 0x80);
   }

#elif defined(VGP_amd64_linux)
   arch->vex.guest_RIP -= 2;             

   {
      UChar *p = (UChar *)arch->vex.guest_RIP;
      
      if (p[0] != 0x0F || p[1] != 0x05)
         VG_(message)(Vg_DebugMsg,
                      "?! restarting over syscall at %#llx %02x %02x\n",
                      arch->vex.guest_RIP, p[0], p[1]); 

      vg_assert(p[0] == 0x0F && p[1] == 0x05);
   }

#elif defined(VGP_ppc32_linux) || defined(VGP_ppc64be_linux)
   arch->vex.guest_CIA -= 4;             

   {
      UChar *p = (UChar *)arch->vex.guest_CIA;

      if (p[0] != 0x44 || p[1] != 0x0 || p[2] != 0x0 || p[3] != 0x02)
         VG_(message)(Vg_DebugMsg,
                      "?! restarting over syscall at %#llx %02x %02x %02x %02x\n",
                      arch->vex.guest_CIA + 0ULL, p[0], p[1], p[2], p[3]);

      vg_assert(p[0] == 0x44 && p[1] == 0x0 && p[2] == 0x0 && p[3] == 0x2);
   }

#elif defined(VGP_ppc64le_linux)
   arch->vex.guest_CIA -= 4;             

   {
      UChar *p = (UChar *)arch->vex.guest_CIA;

      if (p[3] != 0x44 || p[2] != 0x0 || p[1] != 0x0 || p[0] != 0x02)
         VG_(message)(Vg_DebugMsg,
                      "?! restarting over syscall at %#llx %02x %02x %02x %02x\n",
                      arch->vex.guest_CIA + 0ULL, p[3], p[2], p[1], p[0]);

      vg_assert(p[3] == 0x44 && p[2] == 0x0 && p[1] == 0x0 && p[0] == 0x2);
   }

#elif defined(VGP_arm_linux)
   if (arch->vex.guest_R15T & 1) {
      
      
      
      arch->vex.guest_R15T -= 2;   
      UChar* p     = (UChar*)(arch->vex.guest_R15T - 1);
      Bool   valid = p[0] == 0 && p[1] == 0xDF;
      if (!valid) {
         VG_(message)(Vg_DebugMsg,
                      "?! restarting over (Thumb) syscall that is not syscall "
                      "at %#llx %02x %02x\n",
                      arch->vex.guest_R15T - 1ULL, p[0], p[1]);
      }
      vg_assert(valid);
      
      
      
      
      
      
      vg_assert(arch->vex.guest_ITSTATE == 0);
   } else {
      
      
      
      arch->vex.guest_R15T -= 4;   
      UChar* p     = (UChar*)arch->vex.guest_R15T;
      Bool   valid = p[0] == 0 && p[1] == 0 && p[2] == 0
                     && (p[3] & 0xF) == 0xF;
      if (!valid) {
         VG_(message)(Vg_DebugMsg,
                      "?! restarting over (ARM) syscall that is not syscall "
                      "at %#llx %02x %02x %02x %02x\n",
                      arch->vex.guest_R15T + 0ULL, p[0], p[1], p[2], p[3]);
      }
      vg_assert(valid);
   }

#elif defined(VGP_arm64_linux)
   arch->vex.guest_PC -= 4;             

   {
      UChar *p = (UChar *)arch->vex.guest_PC;

      if (p[0] != 0x01 || p[1] != 0x00 || p[2] != 0x00 || p[3] != 0xD4)
         VG_(message)(
            Vg_DebugMsg,
            "?! restarting over syscall at %#llx %02x %02x %02x %02x\n",
            arch->vex.guest_PC + 0ULL, p[0], p[1], p[2], p[3]
          );

      vg_assert(p[0] == 0x01 && p[1] == 0x00 && p[2] == 0x00 && p[3] == 0xD4);
   }

#elif defined(VGP_x86_darwin)
   arch->vex.guest_EIP = arch->vex.guest_IP_AT_SYSCALL; 

   {
       UChar *p = (UChar *)arch->vex.guest_EIP;
       Bool  ok = (p[0] == 0xCD && p[1] == 0x80) 
                  || (p[0] == 0xCD && p[1] == 0x81)
                  || (p[0] == 0xCD && p[1] == 0x82)  
                  || (p[0] == 0x0F && p[1] == 0x34);
       if (!ok)
           VG_(message)(Vg_DebugMsg,
                        "?! restarting over syscall at %#x %02x %02x\n",
                        arch->vex.guest_EIP, p[0], p[1]);
       vg_assert(ok);
   }
   
#elif defined(VGP_amd64_darwin)
   
   vg_assert(0);
   
#elif defined(VGP_s390x_linux)
   arch->vex.guest_IA -= 2;             

   {
      UChar *p = (UChar *)arch->vex.guest_IA;
      if (p[0] != 0x0A)
         VG_(message)(Vg_DebugMsg,
                      "?! restarting over syscall at %#llx %02x %02x\n",
                      arch->vex.guest_IA, p[0], p[1]);

      vg_assert(p[0] == 0x0A);
   }

#elif defined(VGP_mips32_linux) || defined(VGP_mips64_linux)

   arch->vex.guest_PC -= 4;             

   {
      UChar *p = (UChar *)(arch->vex.guest_PC);
#     if defined (VG_LITTLEENDIAN)
      if (p[0] != 0x0c || p[1] != 0x00 || p[2] != 0x00 || p[3] != 0x00)
         VG_(message)(Vg_DebugMsg,
                      "?! restarting over syscall at %#llx %02x %02x %02x %02x\n",
                      (ULong)arch->vex.guest_PC, p[0], p[1], p[2], p[3]);

      vg_assert(p[0] == 0x0c && p[1] == 0x00 && p[2] == 0x00 && p[3] == 0x00);
#     elif defined (VG_BIGENDIAN)
      if (p[0] != 0x00 || p[1] != 0x00 || p[2] != 0x00 || p[3] != 0x0c)
         VG_(message)(Vg_DebugMsg,
                      "?! restarting over syscall at %#llx %02x %02x %02x %02x\n",
                      (ULong)arch->vex.guest_PC, p[0], p[1], p[2], p[3]);

      vg_assert(p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x00 && p[3] == 0x0c);
#     else
#        error "Unknown endianness"
#     endif
   }
#elif defined(VGP_tilegx_linux)
   arch->vex.guest_pc -= 8;             

   {
      unsigned long *p = (unsigned long *)arch->vex.guest_pc;

      if (p[0] != 0x286b180051485000ULL )  
         VG_(message)(Vg_DebugMsg,
                      "?! restarting over syscall at 0x%lx %lx\n",
                      arch->vex.guest_pc, p[0]);
      vg_assert(p[0] == 0x286b180051485000ULL);
   }

#else
#  error "ML_(fixup_guest_state_to_restart_syscall): unknown plat"
#endif
}



void 
VG_(fixup_guest_state_after_syscall_interrupted)( ThreadId tid, 
                                                  Addr     ip, 
                                                  SysRes   sres,
                                                  Bool     restart)
{

   ThreadState*     tst;
   SyscallStatus    canonical;
   ThreadArchState* th_regs;
   SyscallInfo*     sci;

   
   Bool outside_range, 
        in_setup_to_restart,      
        at_restart,               
        in_complete_to_committed, 
        in_committed_to_finished; 

#  if defined(VGO_linux)
   outside_range
      = ip < ML_(blksys_setup) || ip >= ML_(blksys_finished);
   in_setup_to_restart
      = ip >= ML_(blksys_setup) && ip < ML_(blksys_restart); 
   at_restart
      = ip == ML_(blksys_restart); 
   in_complete_to_committed
      = ip >= ML_(blksys_complete) && ip < ML_(blksys_committed); 
   in_committed_to_finished
      = ip >= ML_(blksys_committed) && ip < ML_(blksys_finished);
#  elif defined(VGO_darwin)
   outside_range
      =  (ip < ML_(blksys_setup_MACH) || ip >= ML_(blksys_finished_MACH))
      && (ip < ML_(blksys_setup_MDEP) || ip >= ML_(blksys_finished_MDEP))
      && (ip < ML_(blksys_setup_UNIX) || ip >= ML_(blksys_finished_UNIX));
   in_setup_to_restart
      =  (ip >= ML_(blksys_setup_MACH) && ip < ML_(blksys_restart_MACH))
      || (ip >= ML_(blksys_setup_MDEP) && ip < ML_(blksys_restart_MDEP))
      || (ip >= ML_(blksys_setup_UNIX) && ip < ML_(blksys_restart_UNIX));
   at_restart
      =  (ip == ML_(blksys_restart_MACH))
      || (ip == ML_(blksys_restart_MDEP))
      || (ip == ML_(blksys_restart_UNIX));
   in_complete_to_committed
      =  (ip >= ML_(blksys_complete_MACH) && ip < ML_(blksys_committed_MACH))
      || (ip >= ML_(blksys_complete_MDEP) && ip < ML_(blksys_committed_MDEP))
      || (ip >= ML_(blksys_complete_UNIX) && ip < ML_(blksys_committed_UNIX));
   in_committed_to_finished
      =  (ip >= ML_(blksys_committed_MACH) && ip < ML_(blksys_finished_MACH))
      || (ip >= ML_(blksys_committed_MDEP) && ip < ML_(blksys_finished_MDEP))
      || (ip >= ML_(blksys_committed_UNIX) && ip < ML_(blksys_finished_UNIX));
   
#  else
#    error "Unknown OS"
#  endif

   if (VG_(clo_trace_signals))
      VG_(message)( Vg_DebugMsg,
                    "interrupted_syscall: tid=%d, ip=0x%llx, "
                    "restart=%s, sres.isErr=%s, sres.val=%lld\n", 
                    (Int)tid,
                    (ULong)ip, 
                    restart ? "True" : "False", 
                    sr_isError(sres) ? "True" : "False",
                    (Long)(sr_isError(sres) ? sr_Err(sres) : sr_Res(sres)) );

   vg_assert(VG_(is_valid_tid)(tid));
   vg_assert(tid >= 1 && tid < VG_N_THREADS);
   vg_assert(VG_(is_running_thread)(tid));

   tst     = VG_(get_ThreadState)(tid);
   th_regs = &tst->arch;
   sci     = & syscallInfo[tid];

   if (outside_range) {
      if (VG_(clo_trace_signals))
         VG_(message)( Vg_DebugMsg,
                       "  not in syscall at all: hmm, very suspicious\n" );
      
      vg_assert(sci->status.what != SsIdle);
      return;
   }

   vg_assert(sci->status.what != SsIdle);


   if (in_setup_to_restart) {
      
      if (VG_(clo_trace_signals))
         VG_(message)( Vg_DebugMsg, "  not started: restarting\n");
      vg_assert(sci->status.what == SsHandToKernel);
      ML_(fixup_guest_state_to_restart_syscall)(th_regs);
   } 

   else 
   if (at_restart) {
      if (restart) {
         if (VG_(clo_trace_signals))
            VG_(message)( Vg_DebugMsg, "  at syscall instr: restarting\n");
         ML_(fixup_guest_state_to_restart_syscall)(th_regs);
      } else {
         if (VG_(clo_trace_signals))
            VG_(message)( Vg_DebugMsg, "  at syscall instr: returning EINTR\n");
         canonical = convert_SysRes_to_SyscallStatus( 
                        VG_(mk_SysRes_Error)( VKI_EINTR ) 
                     );
         if (!(sci->flags & SfNoWriteResult))
            putSyscallStatusIntoGuestState( tid, &canonical, &th_regs->vex );
         sci->status = canonical;
         VG_(post_syscall)(tid);
      }
   }

   else 
   if (in_complete_to_committed) {
      /* Syscall complete, but result hasn't been written back yet.
         Write the SysRes we were supplied with back to the guest
         state. */
      if (VG_(clo_trace_signals))
         VG_(message)( Vg_DebugMsg,
                       "  completed, but uncommitted: committing\n");
      canonical = convert_SysRes_to_SyscallStatus( sres );
      if (!(sci->flags & SfNoWriteResult))
         putSyscallStatusIntoGuestState( tid, &canonical, &th_regs->vex );
      sci->status = canonical;
      VG_(post_syscall)(tid);
   } 

   else  
   if (in_committed_to_finished) {
      if (VG_(clo_trace_signals))
         VG_(message)( Vg_DebugMsg,
                       "  completed and committed: nothing to do\n");
      getSyscallStatusFromGuestState( &sci->status, &th_regs->vex );
      vg_assert(sci->status.what == SsComplete);
      VG_(post_syscall)(tid);
   } 

   else
      VG_(core_panic)("?? strange syscall interrupt state?");

   sci->status.what = SsIdle;
}


#if defined(VGO_darwin)
void ML_(wqthread_continue_NORETURN)(ThreadId tid)
{
   ThreadState*     tst;
   SyscallInfo*     sci;

   VG_(acquire_BigLock)(tid, "wqthread_continue_NORETURN");

   PRINT("SYSCALL[%d,%d](%s) workq_ops() starting new workqueue item\n", 
         VG_(getpid)(), tid, VG_SYSNUM_STRING(__NR_workq_ops));

   vg_assert(VG_(is_valid_tid)(tid));
   vg_assert(tid >= 1 && tid < VG_N_THREADS);
   vg_assert(VG_(is_running_thread)(tid));

   tst     = VG_(get_ThreadState)(tid);
   sci     = & syscallInfo[tid];
   vg_assert(sci->status.what != SsIdle);
   vg_assert(tst->os_state.wq_jmpbuf_valid);  

   
   sci->status = convert_SysRes_to_SyscallStatus( VG_(mk_SysRes_Success)(0) );
   sci->flags |= SfNoWriteResult;
   VG_(post_syscall)(tid);

   ML_(sync_mappings)("in", "ML_(wqthread_continue_NORETURN)", 0);

   sci->status.what = SsIdle;

   vg_assert(tst->sched_jmpbuf_valid);
   VG_MINIMAL_LONGJMP(tst->sched_jmpbuf);

   
   vg_assert(0);
}
#endif



void (* VG_(address_of_m_main_shutdown_actions_NORETURN) )
       (ThreadId,VgSchedReturnCode)
   = NULL;

