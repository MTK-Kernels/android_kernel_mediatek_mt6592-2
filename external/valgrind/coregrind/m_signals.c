
 
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
#include "pub_core_vkiscnums.h"
#include "pub_core_debuglog.h"
#include "pub_core_threadstate.h"
#include "pub_core_xarray.h"
#include "pub_core_clientstate.h"
#include "pub_core_aspacemgr.h"
#include "pub_core_debugger.h"      
#include "pub_core_errormgr.h"
#include "pub_core_gdbserver.h"
#include "pub_core_libcbase.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcprint.h"
#include "pub_core_libcproc.h"
#include "pub_core_libcsignal.h"
#include "pub_core_machine.h"
#include "pub_core_mallocfree.h"
#include "pub_core_options.h"
#include "pub_core_scheduler.h"
#include "pub_core_signals.h"
#include "pub_core_sigframe.h"      
#include "pub_core_stacks.h"        
#include "pub_core_stacktrace.h"    
#include "pub_core_syscall.h"
#include "pub_core_syswrap.h"
#include "pub_core_tooliface.h"
#include "pub_core_coredump.h"



static void sync_signalhandler  ( Int sigNo, vki_siginfo_t *info,
                                             struct vki_ucontext * );
static void async_signalhandler ( Int sigNo, vki_siginfo_t *info,
                                             struct vki_ucontext * );
static void sigvgkill_handler	( Int sigNo, vki_siginfo_t *info,
                                             struct vki_ucontext * );

Int VG_(max_signal) = _VKI_NSIG;

#define N_QUEUED_SIGNALS	8

typedef struct SigQueue {
   Int	next;
   vki_siginfo_t sigs[N_QUEUED_SIGNALS];
} SigQueue;



#if defined(VGP_x86_linux)
#  define VG_UCONTEXT_INSTR_PTR(uc)       ((uc)->uc_mcontext.eip)
#  define VG_UCONTEXT_STACK_PTR(uc)       ((uc)->uc_mcontext.esp)
#  define VG_UCONTEXT_SYSCALL_SYSRES(uc)                        \
       \
      VG_(mk_SysRes_x86_linux)( (uc)->uc_mcontext.eax )
#  define VG_UCONTEXT_TO_UnwindStartRegs(srP, uc)        \
      { (srP)->r_pc = (ULong)((uc)->uc_mcontext.eip);    \
        (srP)->r_sp = (ULong)((uc)->uc_mcontext.esp);    \
        (srP)->misc.X86.r_ebp = (uc)->uc_mcontext.ebp;   \
      }

#elif defined(VGP_amd64_linux)
#  define VG_UCONTEXT_INSTR_PTR(uc)       ((uc)->uc_mcontext.rip)
#  define VG_UCONTEXT_STACK_PTR(uc)       ((uc)->uc_mcontext.rsp)
#  define VG_UCONTEXT_SYSCALL_SYSRES(uc)                        \
       \
      VG_(mk_SysRes_amd64_linux)( (uc)->uc_mcontext.rax )
#  define VG_UCONTEXT_TO_UnwindStartRegs(srP, uc)        \
      { (srP)->r_pc = (uc)->uc_mcontext.rip;             \
        (srP)->r_sp = (uc)->uc_mcontext.rsp;             \
        (srP)->misc.AMD64.r_rbp = (uc)->uc_mcontext.rbp; \
      }

#elif defined(VGP_ppc32_linux)
#  define VG_UCONTEXT_INSTR_PTR(uc)  ((uc)->uc_regs->mc_gregs[VKI_PT_NIP])
#  define VG_UCONTEXT_STACK_PTR(uc)  ((uc)->uc_regs->mc_gregs[VKI_PT_R1])
#  define VG_UCONTEXT_SYSCALL_SYSRES(uc)                            \
        \
      VG_(mk_SysRes_ppc32_linux)(                                   \
         (uc)->uc_regs->mc_gregs[VKI_PT_R3],                        \
         (((uc)->uc_regs->mc_gregs[VKI_PT_CCR] >> 28) & 1)          \
      )
#  define VG_UCONTEXT_TO_UnwindStartRegs(srP, uc)                     \
      { (srP)->r_pc = (ULong)((uc)->uc_regs->mc_gregs[VKI_PT_NIP]);   \
        (srP)->r_sp = (ULong)((uc)->uc_regs->mc_gregs[VKI_PT_R1]);    \
        (srP)->misc.PPC32.r_lr = (uc)->uc_regs->mc_gregs[VKI_PT_LNK]; \
      }

#elif defined(VGP_ppc64be_linux) || defined(VGP_ppc64le_linux)
#  define VG_UCONTEXT_INSTR_PTR(uc)  ((uc)->uc_mcontext.gp_regs[VKI_PT_NIP])
#  define VG_UCONTEXT_STACK_PTR(uc)  ((uc)->uc_mcontext.gp_regs[VKI_PT_R1])
   static inline SysRes VG_UCONTEXT_SYSCALL_SYSRES( struct vki_ucontext* uc )
   {
      ULong err = (uc->uc_mcontext.gp_regs[VKI_PT_CCR] >> 28) & 1;
      ULong r3  = uc->uc_mcontext.gp_regs[VKI_PT_R3];
      if (err) r3 &= 0xFF;
      return VG_(mk_SysRes_ppc64_linux)( r3, err );
   }
#  define VG_UCONTEXT_TO_UnwindStartRegs(srP, uc)                       \
      { (srP)->r_pc = (uc)->uc_mcontext.gp_regs[VKI_PT_NIP];            \
        (srP)->r_sp = (uc)->uc_mcontext.gp_regs[VKI_PT_R1];             \
        (srP)->misc.PPC64.r_lr = (uc)->uc_mcontext.gp_regs[VKI_PT_LNK]; \
      }

#elif defined(VGP_arm_linux)
#  define VG_UCONTEXT_INSTR_PTR(uc)       ((uc)->uc_mcontext.arm_pc)
#  define VG_UCONTEXT_STACK_PTR(uc)       ((uc)->uc_mcontext.arm_sp)
#  define VG_UCONTEXT_SYSCALL_SYSRES(uc)                        \
       \
      VG_(mk_SysRes_arm_linux)( (uc)->uc_mcontext.arm_r0 )
#  define VG_UCONTEXT_TO_UnwindStartRegs(srP, uc)       \
      { (srP)->r_pc = (uc)->uc_mcontext.arm_pc;         \
        (srP)->r_sp = (uc)->uc_mcontext.arm_sp;         \
        (srP)->misc.ARM.r14 = (uc)->uc_mcontext.arm_lr; \
        (srP)->misc.ARM.r12 = (uc)->uc_mcontext.arm_ip; \
        (srP)->misc.ARM.r11 = (uc)->uc_mcontext.arm_fp; \
        (srP)->misc.ARM.r7  = (uc)->uc_mcontext.arm_r7; \
      }

#elif defined(VGP_arm64_linux)
#  define VG_UCONTEXT_INSTR_PTR(uc)       ((UWord)((uc)->uc_mcontext.pc))
#  define VG_UCONTEXT_STACK_PTR(uc)       ((UWord)((uc)->uc_mcontext.sp))
#  define VG_UCONTEXT_SYSCALL_SYSRES(uc)                        \
       \
      VG_(mk_SysRes_arm64_linux)( (uc)->uc_mcontext.regs[0] )
#  define VG_UCONTEXT_TO_UnwindStartRegs(srP, uc)           \
      { (srP)->r_pc = (uc)->uc_mcontext.pc;                 \
        (srP)->r_sp = (uc)->uc_mcontext.sp;                 \
        (srP)->misc.ARM64.x29 = (uc)->uc_mcontext.regs[29]; \
        (srP)->misc.ARM64.x30 = (uc)->uc_mcontext.regs[30]; \
      }

#elif defined(VGP_x86_darwin)

   static inline Addr VG_UCONTEXT_INSTR_PTR( void* ucV ) {
      ucontext_t* uc = (ucontext_t*)ucV;
      struct __darwin_mcontext32* mc = uc->uc_mcontext;
      struct __darwin_i386_thread_state* ss = &mc->__ss;
      return ss->__eip;
   }
   static inline Addr VG_UCONTEXT_STACK_PTR( void* ucV ) {
      ucontext_t* uc = (ucontext_t*)ucV;
      struct __darwin_mcontext32* mc = uc->uc_mcontext;
      struct __darwin_i386_thread_state* ss = &mc->__ss;
      return ss->__esp;
   }
   static inline SysRes VG_UCONTEXT_SYSCALL_SYSRES( void* ucV,
                                                    UWord scclass ) {
      ucontext_t* uc = (ucontext_t*)ucV;
      struct __darwin_mcontext32* mc = uc->uc_mcontext;
      struct __darwin_i386_thread_state* ss = &mc->__ss;
      
      UInt carry = 1 & ss->__eflags;
      UInt err = 0;
      UInt wLO = 0;
      UInt wHI = 0;
      switch (scclass) {
         case VG_DARWIN_SYSCALL_CLASS_UNIX:
            err = carry;
            wLO = ss->__eax;
            wHI = ss->__edx;
            break;
         case VG_DARWIN_SYSCALL_CLASS_MACH:
            wLO = ss->__eax;
            break;
         case VG_DARWIN_SYSCALL_CLASS_MDEP:
            wLO = ss->__eax;
            break;
         default: 
            vg_assert(0);
            break;
      }
      return VG_(mk_SysRes_x86_darwin)( scclass, err ? True : False, 
                                        wHI, wLO );
   }
   static inline
   void VG_UCONTEXT_TO_UnwindStartRegs( UnwindStartRegs* srP,
                                        void* ucV ) {
      ucontext_t* uc = (ucontext_t*)(ucV);
      struct __darwin_mcontext32* mc = uc->uc_mcontext;
      struct __darwin_i386_thread_state* ss = &mc->__ss;
      srP->r_pc = (ULong)(ss->__eip);
      srP->r_sp = (ULong)(ss->__esp);
      srP->misc.X86.r_ebp = (UInt)(ss->__ebp);
   }

#elif defined(VGP_amd64_darwin)

   static inline Addr VG_UCONTEXT_INSTR_PTR( void* ucV ) {
      ucontext_t* uc = (ucontext_t*)ucV;
      struct __darwin_mcontext64* mc = uc->uc_mcontext;
      struct __darwin_x86_thread_state64* ss = &mc->__ss;
      return ss->__rip;
   }
   static inline Addr VG_UCONTEXT_STACK_PTR( void* ucV ) {
      ucontext_t* uc = (ucontext_t*)ucV;
      struct __darwin_mcontext64* mc = uc->uc_mcontext;
      struct __darwin_x86_thread_state64* ss = &mc->__ss;
      return ss->__rsp;
   }
   static inline SysRes VG_UCONTEXT_SYSCALL_SYSRES( void* ucV,
                                                    UWord scclass ) {
      ucontext_t* uc = (ucontext_t*)ucV;
      struct __darwin_mcontext64* mc = uc->uc_mcontext;
      struct __darwin_x86_thread_state64* ss = &mc->__ss;
      
      ULong carry = 1 & ss->__rflags;
      ULong err = 0;
      ULong wLO = 0;
      ULong wHI = 0;
      switch (scclass) {
         case VG_DARWIN_SYSCALL_CLASS_UNIX:
            err = carry;
            wLO = ss->__rax;
            wHI = ss->__rdx;
            break;
         case VG_DARWIN_SYSCALL_CLASS_MACH:
            wLO = ss->__rax;
            break;
         case VG_DARWIN_SYSCALL_CLASS_MDEP:
            wLO = ss->__rax;
            break;
         default: 
            vg_assert(0);
            break;
      }
      return VG_(mk_SysRes_amd64_darwin)( scclass, err ? True : False, 
					  wHI, wLO );
   }
   static inline
   void VG_UCONTEXT_TO_UnwindStartRegs( UnwindStartRegs* srP,
                                        void* ucV ) {
      ucontext_t* uc = (ucontext_t*)ucV;
      struct __darwin_mcontext64* mc = uc->uc_mcontext;
      struct __darwin_x86_thread_state64* ss = &mc->__ss;
      srP->r_pc = (ULong)(ss->__rip);
      srP->r_sp = (ULong)(ss->__rsp);
      srP->misc.AMD64.r_rbp = (ULong)(ss->__rbp);
   }

#elif defined(VGP_s390x_linux)

#  define VG_UCONTEXT_INSTR_PTR(uc)       ((uc)->uc_mcontext.regs.psw.addr)
#  define VG_UCONTEXT_STACK_PTR(uc)       ((uc)->uc_mcontext.regs.gprs[15])
#  define VG_UCONTEXT_FRAME_PTR(uc)       ((uc)->uc_mcontext.regs.gprs[11])
#  define VG_UCONTEXT_SYSCALL_SYSRES(uc)                        \
      VG_(mk_SysRes_s390x_linux)((uc)->uc_mcontext.regs.gprs[2])
#  define VG_UCONTEXT_LINK_REG(uc) ((uc)->uc_mcontext.regs.gprs[14])

#  define VG_UCONTEXT_TO_UnwindStartRegs(srP, uc)        \
      { (srP)->r_pc = (ULong)((uc)->uc_mcontext.regs.psw.addr);    \
        (srP)->r_sp = (ULong)((uc)->uc_mcontext.regs.gprs[15]);    \
        (srP)->misc.S390X.r_fp = (uc)->uc_mcontext.regs.gprs[11];  \
        (srP)->misc.S390X.r_lr = (uc)->uc_mcontext.regs.gprs[14];  \
      }

#elif defined(VGP_mips32_linux)
#  define VG_UCONTEXT_INSTR_PTR(uc)   ((UWord)(((uc)->uc_mcontext.sc_pc)))
#  define VG_UCONTEXT_STACK_PTR(uc)   ((UWord)((uc)->uc_mcontext.sc_regs[29]))
#  define VG_UCONTEXT_FRAME_PTR(uc)       ((uc)->uc_mcontext.sc_regs[30])
#  define VG_UCONTEXT_SYSCALL_NUM(uc)     ((uc)->uc_mcontext.sc_regs[2])
#  define VG_UCONTEXT_SYSCALL_SYSRES(uc)                         \
        \
      VG_(mk_SysRes_mips32_linux)( (uc)->uc_mcontext.sc_regs[2], \
                                   (uc)->uc_mcontext.sc_regs[3], \
                                   (uc)->uc_mcontext.sc_regs[7]) 
 
#  define VG_UCONTEXT_TO_UnwindStartRegs(srP, uc)              \
      { (srP)->r_pc = (uc)->uc_mcontext.sc_pc;                 \
        (srP)->r_sp = (uc)->uc_mcontext.sc_regs[29];           \
        (srP)->misc.MIPS32.r30 = (uc)->uc_mcontext.sc_regs[30]; \
        (srP)->misc.MIPS32.r31 = (uc)->uc_mcontext.sc_regs[31]; \
        (srP)->misc.MIPS32.r28 = (uc)->uc_mcontext.sc_regs[28]; \
      }

#elif defined(VGP_mips64_linux)
#  define VG_UCONTEXT_INSTR_PTR(uc)       (((uc)->uc_mcontext.sc_pc))
#  define VG_UCONTEXT_STACK_PTR(uc)       ((uc)->uc_mcontext.sc_regs[29])
#  define VG_UCONTEXT_FRAME_PTR(uc)       ((uc)->uc_mcontext.sc_regs[30])
#  define VG_UCONTEXT_SYSCALL_NUM(uc)     ((uc)->uc_mcontext.sc_regs[2])
#  define VG_UCONTEXT_SYSCALL_SYSRES(uc)                        \
       \
      VG_(mk_SysRes_mips64_linux)((uc)->uc_mcontext.sc_regs[2], \
                                  (uc)->uc_mcontext.sc_regs[3], \
                                  (uc)->uc_mcontext.sc_regs[7])

#  define VG_UCONTEXT_TO_UnwindStartRegs(srP, uc)               \
      { (srP)->r_pc = (uc)->uc_mcontext.sc_pc;                  \
        (srP)->r_sp = (uc)->uc_mcontext.sc_regs[29];            \
        (srP)->misc.MIPS64.r30 = (uc)->uc_mcontext.sc_regs[30]; \
        (srP)->misc.MIPS64.r31 = (uc)->uc_mcontext.sc_regs[31]; \
        (srP)->misc.MIPS64.r28 = (uc)->uc_mcontext.sc_regs[28]; \
      }
#elif defined(VGP_tilegx_linux)
#  define VG_UCONTEXT_INSTR_PTR(uc)       ((uc)->uc_mcontext.pc)
#  define VG_UCONTEXT_STACK_PTR(uc)       ((uc)->uc_mcontext.sp)
#  define VG_UCONTEXT_FRAME_PTR(uc)       ((uc)->uc_mcontext.gregs[52])
#  define VG_UCONTEXT_SYSCALL_NUM(uc)     ((uc)->uc_mcontext.gregs[10])
#  define VG_UCONTEXT_SYSCALL_SYSRES(uc)                            \
           \
      VG_(mk_SysRes_tilegx_linux)((uc)->uc_mcontext.gregs[0])
#  define VG_UCONTEXT_TO_UnwindStartRegs(srP, uc)              \
      { (srP)->r_pc = (uc)->uc_mcontext.pc;                    \
        (srP)->r_sp = (uc)->uc_mcontext.sp;                    \
        (srP)->misc.TILEGX.r52 = (uc)->uc_mcontext.gregs[52];  \
        (srP)->misc.TILEGX.r55 = (uc)->uc_mcontext.lr;         \
      }
#else
#  error Unknown platform
#endif



#if defined(VGO_linux)
#  define VKI_SIGINFO_si_addr  _sifields._sigfault._addr
#  define VKI_SIGINFO_si_pid   _sifields._kill._pid
#elif defined(VGO_darwin)
#  define VKI_SIGINFO_si_addr  si_addr
#  define VKI_SIGINFO_si_pid   si_pid
#else
#  error Unknown OS
#endif








typedef 
   struct {
      void* scss_handler;  
      UInt  scss_flags;
      vki_sigset_t scss_mask;
      void* scss_restorer; 
      void* scss_sa_tramp; 
   }
   SCSS_Per_Signal;

typedef 
   struct {
      
      SCSS_Per_Signal scss_per_sig[1+_VKI_NSIG];

      } 
      SCSS;

static SCSS scss;





typedef 
   struct {
      void* skss_handler;  
      UInt skss_flags;
      
   }
   SKSS_Per_Signal;

typedef 
   struct {
      SKSS_Per_Signal skss_per_sig[1+_VKI_NSIG];
   } 
   SKSS;

static SKSS skss;

static Bool is_sig_ign(vki_siginfo_t *info, ThreadId tid)
{
   vg_assert(info->si_signo >= 1 && info->si_signo <= _VKI_NSIG);

   return !VG_(gdbserver_report_signal) (info, tid)
      || scss.scss_per_sig[info->si_signo].scss_handler == VKI_SIG_IGN;
}


static 
void pp_SKSS ( void )
{
   Int sig;
   VG_(printf)("\n\nSKSS:\n");
   for (sig = 1; sig <= _VKI_NSIG; sig++) {
      VG_(printf)("sig %d:  handler %p,  flags 0x%x\n", sig,
                  skss.skss_per_sig[sig].skss_handler,
                  skss.skss_per_sig[sig].skss_flags );

   }
}

static
void calculate_SKSS_from_SCSS ( SKSS* dst )
{
   Int   sig;
   UInt  scss_flags;
   UInt  skss_flags;

   for (sig = 1; sig <= _VKI_NSIG; sig++) {
      void *skss_handler;
      void *scss_handler;
      
      scss_handler = scss.scss_per_sig[sig].scss_handler;
      scss_flags   = scss.scss_per_sig[sig].scss_flags;

      switch(sig) {
      case VKI_SIGSEGV:
      case VKI_SIGBUS:
      case VKI_SIGFPE:
      case VKI_SIGILL:
      case VKI_SIGTRAP:
	 skss_handler = sync_signalhandler;
	 break;

      case VKI_SIGCONT:
      case VKI_SIGCHLD:
      case VKI_SIGWINCH:
      case VKI_SIGURG:
	 if (scss.scss_per_sig[sig].scss_handler == VKI_SIG_DFL)
	    skss_handler = VKI_SIG_DFL;
	 else if (scss.scss_per_sig[sig].scss_handler == VKI_SIG_IGN)
	    skss_handler = VKI_SIG_IGN;
	 else
	    skss_handler = async_signalhandler;
	 break;

      default:
         
         
	 if (sig == VG_SIGVGKILL)
	    skss_handler = sigvgkill_handler;
	 else {
	    if (scss_handler == VKI_SIG_IGN)
	       skss_handler = VKI_SIG_IGN;
	    else 
	       skss_handler = async_signalhandler;
	 }
	 break;
      }

      

      skss_flags = 0;

      
      skss_flags |= scss_flags & (VKI_SA_NOCLDSTOP | VKI_SA_NOCLDWAIT);

      
      
      skss_flags |= VKI_SA_RESTART;

      

      
      

      
      skss_flags |= VKI_SA_SIGINFO;

      
      skss_flags |= VKI_SA_RESTORER;

      
      if (sig != VKI_SIGKILL && sig != VKI_SIGSTOP)
         dst->skss_per_sig[sig].skss_handler = skss_handler;
      else
         dst->skss_per_sig[sig].skss_handler = VKI_SIG_DFL;

      dst->skss_per_sig[sig].skss_flags   = skss_flags;
   }

   
   vg_assert(dst->skss_per_sig[VKI_SIGKILL].skss_handler == VKI_SIG_DFL);
   vg_assert(dst->skss_per_sig[VKI_SIGSTOP].skss_handler == VKI_SIG_DFL);

   if (0)
      pp_SKSS();
}



extern void my_sigreturn(void);

#if defined(VGP_x86_linux)
#  define _MY_SIGRETURN(name) \
   ".text\n" \
   ".globl my_sigreturn\n" \
   "my_sigreturn:\n" \
   "	movl	$" #name ", %eax\n" \
   "	int	$0x80\n" \
   ".previous\n"

#elif defined(VGP_amd64_linux)
#  define _MY_SIGRETURN(name) \
   ".text\n" \
   ".globl my_sigreturn\n" \
   "my_sigreturn:\n" \
   "	movq	$" #name ", %rax\n" \
   "	syscall\n" \
   ".previous\n"

#elif defined(VGP_ppc32_linux)
#  define _MY_SIGRETURN(name) \
   ".text\n" \
   ".globl my_sigreturn\n" \
   "my_sigreturn:\n" \
   "	li	0, " #name "\n" \
   "	sc\n" \
   ".previous\n"

#elif defined(VGP_ppc64be_linux)
#  define _MY_SIGRETURN(name) \
   ".align   2\n" \
   ".globl   my_sigreturn\n" \
   ".section \".opd\",\"aw\"\n" \
   ".align   3\n" \
   "my_sigreturn:\n" \
   ".quad    .my_sigreturn,.TOC.@tocbase,0\n" \
   ".previous\n" \
   ".type    .my_sigreturn,@function\n" \
   ".globl   .my_sigreturn\n" \
   ".my_sigreturn:\n" \
   "	li	0, " #name "\n" \
   "	sc\n"

#elif defined(VGP_ppc64le_linux)
#  define _MY_SIGRETURN(name) \
   ".align   2\n" \
   ".globl   my_sigreturn\n" \
   ".type    .my_sigreturn,@function\n" \
   "my_sigreturn:\n" \
   "#if _CALL_ELF == 2 \n" \
   "0: addis        2,12,.TOC.-0b@ha\n" \
   "   addi         2,2,.TOC.-0b@l\n" \
   "   .localentry my_sigreturn,.-my_sigreturn\n" \
   "#endif \n" \
   "   sc\n" \
   "   .size my_sigreturn,.-my_sigreturn\n"

#elif defined(VGP_arm_linux)
#  define _MY_SIGRETURN(name) \
   ".text\n" \
   ".globl my_sigreturn\n" \
   "my_sigreturn:\n\t" \
   "    mov  r7, #" #name "\n\t" \
   "    svc  0x00000000\n" \
   ".previous\n"

#elif defined(VGP_arm64_linux)
#  define _MY_SIGRETURN(name) \
   ".text\n" \
   ".globl my_sigreturn\n" \
   "my_sigreturn:\n\t" \
   "    mov  x8, #" #name "\n\t" \
   "    svc  0x0\n" \
   ".previous\n"

#elif defined(VGP_x86_darwin)
#  define _MY_SIGRETURN(name) \
   ".text\n" \
   ".globl my_sigreturn\n" \
   "my_sigreturn:\n" \
   "movl $" VG_STRINGIFY(__NR_DARWIN_FAKE_SIGRETURN) ",%eax\n" \
   "int $0x80"

#elif defined(VGP_amd64_darwin)
   
#  define _MY_SIGRETURN(name) \
   ".text\n" \
   ".globl my_sigreturn\n" \
   "my_sigreturn:\n" \
   "ud2\n"

#elif defined(VGP_s390x_linux)
#  define _MY_SIGRETURN(name) \
   ".text\n" \
   ".globl my_sigreturn\n" \
   "my_sigreturn:\n" \
   " svc " #name "\n" \
   ".previous\n"

#elif defined(VGP_mips32_linux)
#  define _MY_SIGRETURN(name) \
   ".text\n" \
   "my_sigreturn:\n" \
   "	li	$2, " #name "\n"  \
   "	syscall\n" \
   ".previous\n"

#elif defined(VGP_mips64_linux)
#  define _MY_SIGRETURN(name) \
   ".text\n" \
   "my_sigreturn:\n" \
   "   li $2, " #name "\n" \
   "   syscall\n" \
   ".previous\n"

#elif defined(VGP_tilegx_linux)
#  define _MY_SIGRETURN(name) \
   ".text\n" \
   "my_sigreturn:\n" \
   " moveli r10 ," #name "\n" \
   " swint1\n" \
   ".previous\n"

#else
#  error Unknown platform
#endif

#define MY_SIGRETURN(name)  _MY_SIGRETURN(name)
asm(
   MY_SIGRETURN(__NR_rt_sigreturn)
);


static void handle_SCSS_change ( Bool force_update )
{
   Int  res, sig;
   SKSS skss_old;
   vki_sigaction_toK_t   ksa;
   vki_sigaction_fromK_t ksa_old;

   
   skss_old = skss;
   calculate_SKSS_from_SCSS ( &skss );

   for (sig = 1; sig <= VG_(max_signal); sig++) {

      if (sig == VKI_SIGKILL || sig == VKI_SIGSTOP)
         continue;

      if (!force_update) {
         if ((skss_old.skss_per_sig[sig].skss_handler
              == skss.skss_per_sig[sig].skss_handler)
             && (skss_old.skss_per_sig[sig].skss_flags
                 == skss.skss_per_sig[sig].skss_flags))
            
            continue;
      }

      ksa.ksa_handler = skss.skss_per_sig[sig].skss_handler;
      ksa.sa_flags    = skss.skss_per_sig[sig].skss_flags;
#     if !defined(VGP_ppc32_linux) && \
         !defined(VGP_x86_darwin) && !defined(VGP_amd64_darwin) && \
         !defined(VGP_mips32_linux)
      ksa.sa_restorer = my_sigreturn;
#     endif

      
      VG_(sigfillset)( &ksa.sa_mask );
      VG_(sigdelset)( &ksa.sa_mask, VKI_SIGKILL );
      VG_(sigdelset)( &ksa.sa_mask, VKI_SIGSTOP );

      if (VG_(clo_trace_signals) && VG_(clo_verbosity) > 2)
         VG_(dmsg)("setting ksig %d to: hdlr %p, flags 0x%lx, "
                   "mask(msb..lsb) 0x%llx 0x%llx\n",
                   sig, ksa.ksa_handler,
                   (UWord)ksa.sa_flags,
                   _VKI_NSIG_WORDS > 1 ? (ULong)ksa.sa_mask.sig[1] : 0,
                   (ULong)ksa.sa_mask.sig[0]);

      res = VG_(sigaction)( sig, &ksa, &ksa_old );
      vg_assert(res == 0);

      if (!force_update) {
         vg_assert(ksa_old.ksa_handler 
                   == skss_old.skss_per_sig[sig].skss_handler);
         vg_assert(ksa_old.sa_flags 
                   == skss_old.skss_per_sig[sig].skss_flags);
#        if !defined(VGP_ppc32_linux) && \
            !defined(VGP_x86_darwin) && !defined(VGP_amd64_darwin) && \
            !defined(VGP_mips32_linux) && !defined(VGP_mips64_linux)
         vg_assert(ksa_old.sa_restorer == my_sigreturn);
#        endif
         VG_(sigaddset)( &ksa_old.sa_mask, VKI_SIGKILL );
         VG_(sigaddset)( &ksa_old.sa_mask, VKI_SIGSTOP );
         vg_assert(VG_(isfullsigset)( &ksa_old.sa_mask ));
      }
   }
}




static Bool on_sig_stack ( ThreadId tid, Addr m_SP )
{
   ThreadState *tst = VG_(get_ThreadState)(tid);

   return (m_SP - (Addr)tst->altstack.ss_sp < (Addr)tst->altstack.ss_size);
}

static Int sas_ss_flags ( ThreadId tid, Addr m_SP )
{
   ThreadState *tst = VG_(get_ThreadState)(tid);

   return (tst->altstack.ss_size == 0 
              ? VKI_SS_DISABLE
              : on_sig_stack(tid, m_SP) ? VKI_SS_ONSTACK : 0);
}


SysRes VG_(do_sys_sigaltstack) ( ThreadId tid, vki_stack_t* ss, vki_stack_t* oss )
{
   Addr m_SP;

   vg_assert(VG_(is_valid_tid)(tid));
   m_SP  = VG_(get_SP)(tid);

   if (VG_(clo_trace_signals))
      VG_(dmsg)("sys_sigaltstack: tid %d, "
                "ss %p{%p,sz=%llu,flags=0x%llx}, oss %p (current SP %p)\n",
                tid, (void*)ss, 
                ss ? ss->ss_sp : 0,
                (ULong)(ss ? ss->ss_size : 0),
                (ULong)(ss ? ss->ss_flags : 0),
                (void*)oss, (void*)m_SP);

   if (oss != NULL) {
      oss->ss_sp    = VG_(threads)[tid].altstack.ss_sp;
      oss->ss_size  = VG_(threads)[tid].altstack.ss_size;
      oss->ss_flags = VG_(threads)[tid].altstack.ss_flags
                      | sas_ss_flags(tid, m_SP);
   }

   if (ss != NULL) {
      if (on_sig_stack(tid, VG_(get_SP)(tid))) {
         return VG_(mk_SysRes_Error)( VKI_EPERM );
      }
      if (ss->ss_flags != VKI_SS_DISABLE 
          && ss->ss_flags != VKI_SS_ONSTACK 
          && ss->ss_flags != 0) {
         return VG_(mk_SysRes_Error)( VKI_EINVAL );
      }
      if (ss->ss_flags == VKI_SS_DISABLE) {
         VG_(threads)[tid].altstack.ss_flags = VKI_SS_DISABLE;
      } else {
         if (ss->ss_size < VKI_MINSIGSTKSZ) {
            return VG_(mk_SysRes_Error)( VKI_ENOMEM );
         }

	 VG_(threads)[tid].altstack.ss_sp    = ss->ss_sp;
	 VG_(threads)[tid].altstack.ss_size  = ss->ss_size;
	 VG_(threads)[tid].altstack.ss_flags = 0;
      }
   }
   return VG_(mk_SysRes_Success)( 0 );
}


SysRes VG_(do_sys_sigaction) ( Int signo, 
                               const vki_sigaction_toK_t* new_act, 
                               vki_sigaction_fromK_t* old_act )
{
   if (VG_(clo_trace_signals))
      VG_(dmsg)("sys_sigaction: sigNo %d, "
                "new %#lx, old %#lx, new flags 0x%llx\n",
                signo, (UWord)new_act, (UWord)old_act,
                (ULong)(new_act ? new_act->sa_flags : 0));


   
   if (signo < 1 || signo > VG_(max_signal)) goto bad_signo;

   
   if ( (signo > VG_SIGVGRTUSERMAX)
	&& new_act
	&& !(new_act->ksa_handler == VKI_SIG_DFL 
             || new_act->ksa_handler == VKI_SIG_IGN) )
      goto bad_signo_reserved;

   
   if ( (signo == VKI_SIGKILL || signo == VKI_SIGSTOP)
       && new_act
       && new_act->ksa_handler != VKI_SIG_DFL)
      goto bad_sigkill_or_sigstop;

   if (old_act) {
      old_act->ksa_handler = scss.scss_per_sig[signo].scss_handler;
      old_act->sa_flags    = scss.scss_per_sig[signo].scss_flags;
      old_act->sa_mask     = scss.scss_per_sig[signo].scss_mask;
#     if !defined(VGP_x86_darwin) && !defined(VGP_amd64_darwin)
      old_act->sa_restorer = scss.scss_per_sig[signo].scss_restorer;
#     endif
   }

   
   if (new_act) {
      scss.scss_per_sig[signo].scss_handler  = new_act->ksa_handler;
      scss.scss_per_sig[signo].scss_flags    = new_act->sa_flags;
      scss.scss_per_sig[signo].scss_mask     = new_act->sa_mask;

      scss.scss_per_sig[signo].scss_restorer = NULL;
#     if !defined(VGP_x86_darwin) && !defined(VGP_amd64_darwin)
      scss.scss_per_sig[signo].scss_restorer = new_act->sa_restorer;
#     endif

      scss.scss_per_sig[signo].scss_sa_tramp = NULL;
#     if defined(VGP_x86_darwin) || defined(VGP_amd64_darwin)
      scss.scss_per_sig[signo].scss_sa_tramp = new_act->sa_tramp;
#     endif

      VG_(sigdelset)(&scss.scss_per_sig[signo].scss_mask, VKI_SIGKILL);
      VG_(sigdelset)(&scss.scss_per_sig[signo].scss_mask, VKI_SIGSTOP);
   }

   
   if (new_act) {
      handle_SCSS_change( False  );
   }
   return VG_(mk_SysRes_Success)( 0 );

  bad_signo:
   if (VG_(showing_core_errors)() && !VG_(clo_xml)) {
      VG_(umsg)("Warning: bad signal number %d in sigaction()\n", signo);
   }
   return VG_(mk_SysRes_Error)( VKI_EINVAL );

  bad_signo_reserved:
   if (VG_(showing_core_errors)() && !VG_(clo_xml)) {
      VG_(umsg)("Warning: ignored attempt to set %s handler in sigaction();\n",
                VG_(signame)(signo));
      VG_(umsg)("         the %s signal is used internally by Valgrind\n", 
                VG_(signame)(signo));
   }
   return VG_(mk_SysRes_Error)( VKI_EINVAL );

  bad_sigkill_or_sigstop:
   if (VG_(showing_core_errors)() && !VG_(clo_xml)) {
      VG_(umsg)("Warning: ignored attempt to set %s handler in sigaction();\n",
                VG_(signame)(signo));
      VG_(umsg)("         the %s signal is uncatchable\n", 
                VG_(signame)(signo));
   }
   return VG_(mk_SysRes_Error)( VKI_EINVAL );
}


static
void do_sigprocmask_bitops ( Int vki_how, 
			     vki_sigset_t* orig_set,
			     vki_sigset_t* modifier )
{
   switch (vki_how) {
      case VKI_SIG_BLOCK: 
         VG_(sigaddset_from_set)( orig_set, modifier );
         break;
      case VKI_SIG_UNBLOCK:
         VG_(sigdelset_from_set)( orig_set, modifier );
         break;
      case VKI_SIG_SETMASK:
         *orig_set = *modifier;
         break;
      default:
         VG_(core_panic)("do_sigprocmask_bitops");
	 break;
   }
}

static
HChar* format_sigset ( const vki_sigset_t* set )
{
   static HChar buf[_VKI_NSIG_WORDS * 16 + 1];
   int w;

   VG_(strcpy)(buf, "");

   for (w = _VKI_NSIG_WORDS - 1; w >= 0; w--)
   {
#     if _VKI_NSIG_BPW == 32
      VG_(sprintf)(buf + VG_(strlen)(buf), "%08llx",
                   set ? (ULong)set->sig[w] : 0);
#     elif _VKI_NSIG_BPW == 64
      VG_(sprintf)(buf + VG_(strlen)(buf), "%16llx",
                   set ? (ULong)set->sig[w] : 0);
#     else
#       error "Unsupported value for _VKI_NSIG_BPW"
#     endif
   }

   return buf;
}

static
void do_setmask ( ThreadId tid,
                  Int how,
                  vki_sigset_t* newset,
		  vki_sigset_t* oldset )
{
   if (VG_(clo_trace_signals))
      VG_(dmsg)("do_setmask: tid = %d how = %d (%s), newset = %p (%s)\n", 
                tid, how,
                how==VKI_SIG_BLOCK ? "SIG_BLOCK" : (
                   how==VKI_SIG_UNBLOCK ? "SIG_UNBLOCK" : (
                      how==VKI_SIG_SETMASK ? "SIG_SETMASK" : "???")),
                newset, newset ? format_sigset(newset) : "NULL" );

   
   vg_assert(VG_(is_valid_tid)(tid));
   if (oldset) {
      *oldset = VG_(threads)[tid].sig_mask;
      if (VG_(clo_trace_signals))
         VG_(dmsg)("\toldset=%p %s\n", oldset, format_sigset(oldset));
   }
   if (newset) {
      do_sigprocmask_bitops (how, &VG_(threads)[tid].sig_mask, newset );
      VG_(sigdelset)(&VG_(threads)[tid].sig_mask, VKI_SIGKILL);
      VG_(sigdelset)(&VG_(threads)[tid].sig_mask, VKI_SIGSTOP);
      VG_(threads)[tid].tmp_sig_mask = VG_(threads)[tid].sig_mask;
   }
}


SysRes VG_(do_sys_sigprocmask) ( ThreadId tid,
                                 Int how, 
                                 vki_sigset_t* set,
                                 vki_sigset_t* oldset )
{
   switch(how) {
      case VKI_SIG_BLOCK:
      case VKI_SIG_UNBLOCK:
      case VKI_SIG_SETMASK:
         vg_assert(VG_(is_valid_tid)(tid));
         do_setmask ( tid, how, set, oldset );
         return VG_(mk_SysRes_Success)( 0 );

      default:
         VG_(dmsg)("sigprocmask: unknown 'how' field %d\n", how);
         return VG_(mk_SysRes_Error)( VKI_EINVAL );
   }
}




static void block_all_host_signals (  vki_sigset_t* saved_mask )
{
   Int           ret;
   vki_sigset_t block_procmask;
   VG_(sigfillset)(&block_procmask);
   ret = VG_(sigprocmask)
            (VKI_SIG_SETMASK, &block_procmask, saved_mask);
   vg_assert(ret == 0);
}

static void restore_all_host_signals (  vki_sigset_t* saved_mask )
{
   Int ret;
   ret = VG_(sigprocmask)(VKI_SIG_SETMASK, saved_mask, NULL);
   vg_assert(ret == 0);
}

void VG_(clear_out_queued_signals)( ThreadId tid, vki_sigset_t* saved_mask )
{
   block_all_host_signals(saved_mask);
   if (VG_(threads)[tid].sig_queue != NULL) {
      VG_(free)(VG_(threads)[tid].sig_queue);
      VG_(threads)[tid].sig_queue = NULL;
   }
   restore_all_host_signals(saved_mask);
}


static
void push_signal_frame ( ThreadId tid, const vki_siginfo_t *siginfo,
                                       const struct vki_ucontext *uc )
{
   Addr         esp_top_of_frame;
   ThreadState* tst;
   Int		sigNo = siginfo->si_signo;

   vg_assert(sigNo >= 1 && sigNo <= VG_(max_signal));
   vg_assert(VG_(is_valid_tid)(tid));
   tst = & VG_(threads)[tid];

   if (VG_(clo_trace_signals)) {
      VG_(dmsg)("push_signal_frame (thread %d): signal %d\n", tid, sigNo);
      VG_(get_and_pp_StackTrace)(tid, 10);
   }

   if (
       (scss.scss_per_sig[sigNo].scss_flags & VKI_SA_ONSTACK )
       && 
          sas_ss_flags(tid, VG_(get_SP)(tid)) == 0
      ) {
      esp_top_of_frame 
         = (Addr)(tst->altstack.ss_sp) + tst->altstack.ss_size;
      if (VG_(clo_trace_signals))
         VG_(dmsg)("delivering signal %d (%s) to thread %d: "
                   "on ALT STACK (%p-%p; %ld bytes)\n",
                   sigNo, VG_(signame)(sigNo), tid, tst->altstack.ss_sp,
                   (UChar *)tst->altstack.ss_sp + tst->altstack.ss_size,
                   (Word)tst->altstack.ss_size );

      
      VG_TRACK( pre_deliver_signal, tid, sigNo, True );
      
   } else {
      esp_top_of_frame = VG_(get_SP)(tid) - VG_STACK_REDZONE_SZB;

      
      VG_TRACK( pre_deliver_signal, tid, sigNo, False );
   }

   vg_assert(scss.scss_per_sig[sigNo].scss_handler != VKI_SIG_IGN);
   vg_assert(scss.scss_per_sig[sigNo].scss_handler != VKI_SIG_DFL);

   VG_(sigframe_create) (tid, esp_top_of_frame, siginfo, uc,
                         scss.scss_per_sig[sigNo].scss_handler,
                         scss.scss_per_sig[sigNo].scss_flags,
                         &tst->sig_mask,
                         scss.scss_per_sig[sigNo].scss_restorer);
}


const HChar *VG_(signame)(Int sigNo)
{
   static HChar buf[20];  

   switch(sigNo) {
      case VKI_SIGHUP:    return "SIGHUP";
      case VKI_SIGINT:    return "SIGINT";
      case VKI_SIGQUIT:   return "SIGQUIT";
      case VKI_SIGILL:    return "SIGILL";
      case VKI_SIGTRAP:   return "SIGTRAP";
      case VKI_SIGABRT:   return "SIGABRT";
      case VKI_SIGBUS:    return "SIGBUS";
      case VKI_SIGFPE:    return "SIGFPE";
      case VKI_SIGKILL:   return "SIGKILL";
      case VKI_SIGUSR1:   return "SIGUSR1";
      case VKI_SIGUSR2:   return "SIGUSR2";
      case VKI_SIGSEGV:   return "SIGSEGV";
      case VKI_SIGPIPE:   return "SIGPIPE";
      case VKI_SIGALRM:   return "SIGALRM";
      case VKI_SIGTERM:   return "SIGTERM";
#     if defined(VKI_SIGSTKFLT)
      case VKI_SIGSTKFLT: return "SIGSTKFLT";
#     endif
      case VKI_SIGCHLD:   return "SIGCHLD";
      case VKI_SIGCONT:   return "SIGCONT";
      case VKI_SIGSTOP:   return "SIGSTOP";
      case VKI_SIGTSTP:   return "SIGTSTP";
      case VKI_SIGTTIN:   return "SIGTTIN";
      case VKI_SIGTTOU:   return "SIGTTOU";
      case VKI_SIGURG:    return "SIGURG";
      case VKI_SIGXCPU:   return "SIGXCPU";
      case VKI_SIGXFSZ:   return "SIGXFSZ";
      case VKI_SIGVTALRM: return "SIGVTALRM";
      case VKI_SIGPROF:   return "SIGPROF";
      case VKI_SIGWINCH:  return "SIGWINCH";
      case VKI_SIGIO:     return "SIGIO";
#     if defined(VKI_SIGPWR)
      case VKI_SIGPWR:    return "SIGPWR";
#     endif
#     if defined(VKI_SIGUNUSED)
      case VKI_SIGUNUSED: return "SIGUNUSED";
#     endif

#  if defined(VKI_SIGRTMIN) && defined(VKI_SIGRTMAX)
   case VKI_SIGRTMIN ... VKI_SIGRTMAX:
      VG_(sprintf)(buf, "SIGRT%d", sigNo-VKI_SIGRTMIN);
      return buf;
#  endif

   default:
      VG_(sprintf)(buf, "SIG%d", sigNo);
      return buf;
   }
}

void VG_(kill_self)(Int sigNo)
{
   Int r;
   vki_sigset_t	         mask, origmask;
   vki_sigaction_toK_t   sa, origsa2;
   vki_sigaction_fromK_t origsa;   

   sa.ksa_handler = VKI_SIG_DFL;
   sa.sa_flags = 0;
#  if !defined(VGP_x86_darwin) && !defined(VGP_amd64_darwin)
   sa.sa_restorer = 0;
#  endif
   VG_(sigemptyset)(&sa.sa_mask);
      
   VG_(sigaction)(sigNo, &sa, &origsa);

   VG_(sigemptyset)(&mask);
   VG_(sigaddset)(&mask, sigNo);
   VG_(sigprocmask)(VKI_SIG_UNBLOCK, &mask, &origmask);

   r = VG_(kill)(VG_(getpid)(), sigNo);
#  if defined(VGO_linux)
   
   vg_assert(r == 0);
#  endif

   VG_(convert_sigaction_fromK_to_toK)( &origsa, &origsa2 );
   VG_(sigaction)(sigNo, &origsa2, NULL);
   VG_(sigprocmask)(VKI_SIG_SETMASK, &origmask, NULL);
}

static Bool is_signal_from_kernel(ThreadId tid, int signum, int si_code)
{
#  if defined(VGO_linux)
   
   
   
   
   return ( si_code > VKI_SI_USER ? True : False );

#  elif defined(VGO_darwin)
   
   
   
   
   
   
   
   
   
   
   

   
   
   if (VG_(threads)[tid].status == VgTs_WaitSys) {
      return False;

   
   } else if (SIGSEGV == signum) {
      return ( si_code > 0 ? True : False );

   
   
   
   
   } else {
      return True;
   }
#  else
#    error Unknown OS
#  endif
}

#define VKI_SEGV_MADE_UP_GPF    0x80

static void default_action(const vki_siginfo_t *info, ThreadId tid)
{
   Int  sigNo     = info->si_signo;
   Bool terminate = False;	
   Bool core      = False;	
   struct vki_rlimit corelim;
   Bool could_core;

   vg_assert(VG_(is_running_thread)(tid));
   
   switch(sigNo) {
      case VKI_SIGQUIT:	
      case VKI_SIGILL:	
      case VKI_SIGABRT:	
      case VKI_SIGFPE:	
      case VKI_SIGSEGV:	
      case VKI_SIGBUS:	
      case VKI_SIGTRAP:	
      case VKI_SIGXCPU:	
      case VKI_SIGXFSZ:	
         terminate = True;
         core = True;
         break;

      case VKI_SIGHUP:	
      case VKI_SIGINT:	
      case VKI_SIGKILL:	
      case VKI_SIGPIPE:	
      case VKI_SIGALRM:	
      case VKI_SIGTERM:	
      case VKI_SIGUSR1:	
      case VKI_SIGUSR2:	
      case VKI_SIGIO:	
#     if defined(VKI_SIGPWR)
      case VKI_SIGPWR:	
#     endif
      case VKI_SIGSYS:	
      case VKI_SIGPROF:	
      case VKI_SIGVTALRM:	
#     if defined(VKI_SIGRTMIN) && defined(VKI_SIGRTMAX)
      case VKI_SIGRTMIN ... VKI_SIGRTMAX: 
#     endif
         terminate = True;
         break;
   }

   vg_assert(!core || (core && terminate));

   if (VG_(clo_trace_signals))
      VG_(dmsg)("delivering %d (code %d) to default handler; action: %s%s\n",
                sigNo, info->si_code, terminate ? "terminate" : "ignore",
                core ? "+core" : "");

   if (!terminate)
      return;			

   could_core = core;

   if (core) {
	 
      VG_(getrlimit)(VKI_RLIMIT_CORE, &corelim);

      if (corelim.rlim_cur == 0)
	 core = False;
   }

   if ( (VG_(clo_verbosity) >= 1 ||
         (could_core && is_signal_from_kernel(tid, sigNo, info->si_code))
        ) &&
        !VG_(clo_xml) ) {
      VG_(umsg)(
         "\n"
         "Process terminating with default action of signal %d (%s)%s\n",
         sigNo, VG_(signame)(sigNo), core ? ": dumping core" : "");

      
      if (is_signal_from_kernel(tid, sigNo, info->si_code)) {
	 const HChar *event = NULL;
	 Bool haveaddr = True;

	 switch(sigNo) {
	 case VKI_SIGSEGV:
	    switch(info->si_code) {
	    case VKI_SEGV_MAPERR: event = "Access not within mapped region";
                                  break;
	    case VKI_SEGV_ACCERR: event = "Bad permissions for mapped region";
                                  break;
	    case VKI_SEGV_MADE_UP_GPF:
	       event = "General Protection Fault"; 
	       haveaddr = False;
	       break;
	    }
#if 0
            {
              HChar buf[50];  
              VG_(am_show_nsegments)(0,"post segfault");
              VG_(sprintf)(buf, "/bin/cat /proc/%d/maps", VG_(getpid)());
              VG_(system)(buf);
            }
#endif
	    break;

	 case VKI_SIGILL:
	    switch(info->si_code) {
	    case VKI_ILL_ILLOPC: event = "Illegal opcode"; break;
	    case VKI_ILL_ILLOPN: event = "Illegal operand"; break;
	    case VKI_ILL_ILLADR: event = "Illegal addressing mode"; break;
	    case VKI_ILL_ILLTRP: event = "Illegal trap"; break;
	    case VKI_ILL_PRVOPC: event = "Privileged opcode"; break;
	    case VKI_ILL_PRVREG: event = "Privileged register"; break;
	    case VKI_ILL_COPROC: event = "Coprocessor error"; break;
	    case VKI_ILL_BADSTK: event = "Internal stack error"; break;
	    }
	    break;

	 case VKI_SIGFPE:
	    switch (info->si_code) {
	    case VKI_FPE_INTDIV: event = "Integer divide by zero"; break;
	    case VKI_FPE_INTOVF: event = "Integer overflow"; break;
	    case VKI_FPE_FLTDIV: event = "FP divide by zero"; break;
	    case VKI_FPE_FLTOVF: event = "FP overflow"; break;
	    case VKI_FPE_FLTUND: event = "FP underflow"; break;
	    case VKI_FPE_FLTRES: event = "FP inexact"; break;
	    case VKI_FPE_FLTINV: event = "FP invalid operation"; break;
	    case VKI_FPE_FLTSUB: event = "FP subscript out of range"; break;
	    }
	    break;

	 case VKI_SIGBUS:
	    switch (info->si_code) {
	    case VKI_BUS_ADRALN: event = "Invalid address alignment"; break;
	    case VKI_BUS_ADRERR: event = "Non-existent physical address"; break;
	    case VKI_BUS_OBJERR: event = "Hardware error"; break;
	    }
	    break;
	 } 

	 if (event != NULL) {
	    if (haveaddr)
               VG_(umsg)(" %s at address %p\n",
                         event, info->VKI_SIGINFO_si_addr);
	    else
               VG_(umsg)(" %s\n", event);
	 }
      }
      if (VG_(is_valid_tid)(tid)) {
         Word first_ip_delta = 0;
#if defined(VGO_linux)
         if (tid == 1) {           
            Addr esp  = VG_(get_SP)(tid);
            Addr base = VG_PGROUNDDN(esp - VG_STACK_REDZONE_SZB);
            if (VG_(am_addr_is_in_extensible_client_stack)(base) &&
                VG_(extend_stack)(tid, base)) {
               if (VG_(clo_trace_signals))
                  VG_(dmsg)("       -> extended stack base to %#lx\n",
                            VG_PGROUNDDN(esp));
            }
         }
#endif
#if defined(VGA_s390x)
         if (sigNo == VKI_SIGILL) {
            Addr  addr = (Addr)info->VKI_SIGINFO_si_addr;
            UChar byte = ((UChar *)addr)[0];
            Int   insn_length = ((((byte >> 6) + 1) >> 1) + 1) << 1;

            first_ip_delta = -insn_length;
         }
#endif
         ExeContext* ec = VG_(am_is_valid_for_client)
                             (VG_(get_SP)(tid), sizeof(Addr), VKI_PROT_READ)
                        ? VG_(record_ExeContext)( tid, first_ip_delta )
                      : VG_(record_depth_1_ExeContext)( tid,
                                                        first_ip_delta );
         vg_assert(ec);
         VG_(pp_ExeContext)( ec );
      }
      if (sigNo == VKI_SIGSEGV 
          && is_signal_from_kernel(tid, sigNo, info->si_code)
          && info->si_code == VKI_SEGV_MAPERR) {
         VG_(umsg)(" If you believe this happened as a result of a stack\n" );
         VG_(umsg)(" overflow in your program's main thread (unlikely but\n");
         VG_(umsg)(" possible), you can try to increase the size of the\n"  );
         VG_(umsg)(" main thread stack using the --main-stacksize= flag.\n" );
         
         if (VG_(is_valid_tid)(1)) {
            VG_(umsg)(
               " The main thread stack size used in this run was %lu.\n",
               VG_(threads)[1].client_stack_szB);
         }
      }
   }

   if (VG_(clo_vgdb) != Vg_VgdbNo
       && VG_(dyn_vgdb_error) <= VG_(get_n_errs_shown)() + 1) {
      VG_(gdbserver_report_fatal_signal) (info, tid);
   }

   if (VG_(is_action_requested)( "Attach to debugger", & VG_(clo_db_attach) )) {
      VG_(start_debugger)( tid );
   }

   if (core) {
      static const struct vki_rlimit zero = { 0, 0 };

      VG_(make_coredump)(tid, info, corelim.rlim_cur);

      VG_(setrlimit)(VKI_RLIMIT_CORE, &zero);
   }

   
   
   

   
   VG_(nuke_all_threads_except)(tid, VgSrc_FatalSig);
   VG_(threads)[tid].exitreason = VgSrc_FatalSig;
   VG_(threads)[tid].os_state.fatalsig = sigNo;
}

static void deliver_signal ( ThreadId tid, const vki_siginfo_t *info,
                                           const struct vki_ucontext *uc )
{
   Int			sigNo = info->si_signo;
   SCSS_Per_Signal	*handler = &scss.scss_per_sig[sigNo];
   void			*handler_fn;
   ThreadState		*tst = VG_(get_ThreadState)(tid);

   if (VG_(clo_trace_signals))
      VG_(dmsg)("delivering signal %d (%s):%d to thread %d\n", 
                sigNo, VG_(signame)(sigNo), info->si_code, tid );

   if (sigNo == VG_SIGVGKILL) {
      vg_assert(VG_(is_exiting)(tid));
      return;
   }

   handler_fn = handler->scss_handler;
   if (handler_fn == VKI_SIG_IGN) 
      handler_fn = VKI_SIG_DFL;

   vg_assert(handler_fn != VKI_SIG_IGN);

   if (handler_fn == VKI_SIG_DFL) {
      default_action(info, tid);
   } else {
      vg_assert(VG_(is_valid_tid)(tid));

      push_signal_frame ( tid, info, uc );

      if (handler->scss_flags & VKI_SA_ONESHOT) {
	 
	 handler->scss_handler = VKI_SIG_DFL;

	 handle_SCSS_change( False  );
      }

      tst->sig_mask = tst->tmp_sig_mask;
      if (!(handler->scss_flags & VKI_SA_NOMASK)) {
	 VG_(sigaddset_from_set)(&tst->sig_mask, &handler->scss_mask);
	 VG_(sigaddset)(&tst->sig_mask, sigNo);
	 tst->tmp_sig_mask = tst->sig_mask;
      }
   }

   
}

static void resume_scheduler(ThreadId tid)
{
   ThreadState *tst = VG_(get_ThreadState)(tid);

   vg_assert(tst->os_state.lwpid == VG_(gettid)());

   if (tst->sched_jmpbuf_valid) {
      VG_MINIMAL_LONGJMP(tst->sched_jmpbuf);
   }
}

static void synth_fault_common(ThreadId tid, Addr addr, Int si_code)
{
   vki_siginfo_t info;

   vg_assert(VG_(threads)[tid].status == VgTs_Runnable);

   VG_(memset)(&info, 0, sizeof(info));
   info.si_signo = VKI_SIGSEGV;
   info.si_code = si_code;
   info.VKI_SIGINFO_si_addr = (void*)addr;

   (void) VG_(gdbserver_report_signal) (&info, tid);

   
   if (VG_(sigismember)(&VG_(threads)[tid].sig_mask, VKI_SIGSEGV))
      VG_(set_default_handler)(VKI_SIGSEGV);

   deliver_signal(tid, &info, NULL);
}

void VG_(synth_fault_perms)(ThreadId tid, Addr addr)
{
   synth_fault_common(tid, addr, VKI_SEGV_ACCERR);
}

void VG_(synth_fault_mapping)(ThreadId tid, Addr addr)
{
   synth_fault_common(tid, addr, VKI_SEGV_MAPERR);
}

void VG_(synth_fault)(ThreadId tid)
{
   synth_fault_common(tid, 0, VKI_SEGV_MADE_UP_GPF);
}

void VG_(synth_sigill)(ThreadId tid, Addr addr)
{
   vki_siginfo_t info;

   vg_assert(VG_(threads)[tid].status == VgTs_Runnable);

   VG_(memset)(&info, 0, sizeof(info));
   info.si_signo = VKI_SIGILL;
   info.si_code  = VKI_ILL_ILLOPC; 
   info.VKI_SIGINFO_si_addr = (void*)addr;

   if (VG_(gdbserver_report_signal) (&info, tid)) {
      resume_scheduler(tid);
      deliver_signal(tid, &info, NULL);
   }
   else
      resume_scheduler(tid);
}

void VG_(synth_sigbus)(ThreadId tid)
{
   vki_siginfo_t info;

   vg_assert(VG_(threads)[tid].status == VgTs_Runnable);

   VG_(memset)(&info, 0, sizeof(info));
   info.si_signo = VKI_SIGBUS;
   info.si_code  = VKI_BUS_ADRALN;
   

   if (VG_(gdbserver_report_signal) (&info, tid)) {
      resume_scheduler(tid);
      deliver_signal(tid, &info, NULL);
   }
   else
      resume_scheduler(tid);
}

void VG_(synth_sigtrap)(ThreadId tid)
{
   vki_siginfo_t info;
   struct vki_ucontext uc;
#  if defined(VGP_x86_darwin)
   struct __darwin_mcontext32 mc;
#  elif defined(VGP_amd64_darwin)
   struct __darwin_mcontext64 mc;
#  endif

   vg_assert(VG_(threads)[tid].status == VgTs_Runnable);

   VG_(memset)(&info, 0, sizeof(info));
   VG_(memset)(&uc,   0, sizeof(uc));
   info.si_signo = VKI_SIGTRAP;
   info.si_code = VKI_TRAP_BRKPT; 

#  if defined(VGP_x86_linux) || defined(VGP_amd64_linux)
   uc.uc_mcontext.trapno = 3;     
   uc.uc_mcontext.err = 0;        
#  elif defined(VGP_x86_darwin) || defined(VGP_amd64_darwin)
   
   VG_(memset)(&mc, 0, sizeof(mc));
   uc.uc_mcontext = &mc;
   uc.uc_mcontext->__es.__trapno = 3;
   uc.uc_mcontext->__es.__err = 0;
#  endif

   
   if (VG_(gdbserver_report_signal) (&info, tid)) {
      resume_scheduler(tid);
      deliver_signal(tid, &info, &uc);
   }
   else
      resume_scheduler(tid);
}

void VG_(synth_sigfpe)(ThreadId tid, UInt code)
{
#if !defined(VGA_mips32) && !defined(VGA_mips64)
   vg_assert(0);
#else
   vki_siginfo_t info;
   struct vki_ucontext uc;

   vg_assert(VG_(threads)[tid].status == VgTs_Runnable);

   VG_(memset)(&info, 0, sizeof(info));
   VG_(memset)(&uc,   0, sizeof(uc));
   info.si_signo = VKI_SIGFPE;
   info.si_code = code;

   if (VG_(gdbserver_report_signal) (VKI_SIGFPE, tid)) {
      resume_scheduler(tid);
      deliver_signal(tid, &info, &uc);
   }
   else
      resume_scheduler(tid);
#endif
}

static 
void queue_signal(ThreadId tid, const vki_siginfo_t *si)
{
   ThreadState *tst;
   SigQueue *sq;
   vki_sigset_t savedmask;

   tst = VG_(get_ThreadState)(tid);

   
   block_all_host_signals(&savedmask);

   if (tst->sig_queue == NULL) {
      tst->sig_queue = VG_(malloc)("signals.qs.1", sizeof(*tst->sig_queue));
      VG_(memset)(tst->sig_queue, 0, sizeof(*tst->sig_queue));
   }
   sq = tst->sig_queue;

   if (VG_(clo_trace_signals))
      VG_(dmsg)("Queueing signal %d (idx %d) to thread %d\n",
                si->si_signo, sq->next, tid);

   if (sq->sigs[sq->next].si_signo != 0)
      VG_(umsg)("Signal %d being dropped from thread %d's queue\n",
                sq->sigs[sq->next].si_signo, tid);

   sq->sigs[sq->next] = *si;
   sq->next = (sq->next+1) % N_QUEUED_SIGNALS;

   restore_all_host_signals(&savedmask);
}

static vki_siginfo_t *next_queued(ThreadId tid, const vki_sigset_t *set)
{
   ThreadState *tst = VG_(get_ThreadState)(tid);
   SigQueue *sq;
   Int idx;
   vki_siginfo_t *ret = NULL;

   sq = tst->sig_queue;
   if (sq == NULL)
      goto out;
   
   idx = sq->next;
   do {
      if (0)
	 VG_(printf)("idx=%d si_signo=%d inset=%d\n", idx,
		     sq->sigs[idx].si_signo,
                     VG_(sigismember)(set, sq->sigs[idx].si_signo));

      if (sq->sigs[idx].si_signo != 0 
          && VG_(sigismember)(set, sq->sigs[idx].si_signo)) {
	 if (VG_(clo_trace_signals))
            VG_(dmsg)("Returning queued signal %d (idx %d) for thread %d\n",
                      sq->sigs[idx].si_signo, idx, tid);
	 ret = &sq->sigs[idx];
	 goto out;
      }

      idx = (idx + 1) % N_QUEUED_SIGNALS;
   } while(idx != sq->next);
  out:   
   return ret;
}

static int sanitize_si_code(int si_code)
{
#if defined(VGO_linux)
   return (Short)si_code;
#elif defined(VGO_darwin)
   return si_code;
#else
#  error Unknown OS
#endif
}

static 
void async_signalhandler ( Int sigNo,
                           vki_siginfo_t *info, struct vki_ucontext *uc )
{
   ThreadId     tid = VG_(lwpid_to_vgtid)(VG_(gettid)());
   ThreadState* tst = VG_(get_ThreadState)(tid);
   SysRes       sres;

   
   vg_assert(tst->status == VgTs_WaitSys);
   VG_(acquire_BigLock)(tid, "async_signalhandler");

   info->si_code = sanitize_si_code(info->si_code);

   if (VG_(clo_trace_signals))
      VG_(dmsg)("async signal handler: signal=%d, tid=%d, si_code=%d\n",
                sigNo, tid, info->si_code);



#  if defined(VGO_darwin)
   sres = VG_UCONTEXT_SYSCALL_SYSRES(uc, tst->arch.vex.guest_SC_CLASS);
#  else
   sres = VG_UCONTEXT_SYSCALL_SYSRES(uc);
#  endif

   
   VG_(fixup_guest_state_after_syscall_interrupted)(
      tid, 
      VG_UCONTEXT_INSTR_PTR(uc), 
      sres,  
      !!(scss.scss_per_sig[sigNo].scss_flags & VKI_SA_RESTART)
   );

   
   
   if (!is_sig_ign(info, tid))
      deliver_signal(tid, info, uc);


   resume_scheduler(tid);

   VG_(core_panic)("async_signalhandler: got unexpected signal "
                   "while outside of scheduler");
}

Bool VG_(extend_stack)(ThreadId tid, Addr addr)
{
   SizeT udelta;

   
   const NSegment* seg = VG_(am_find_nsegment)(addr);
   vg_assert(seg != NULL);

   if (seg->kind == SkAnonC)
      
      return True;

   const NSegment* seg_next = VG_(am_next_nsegment)( seg, True );
   vg_assert(seg_next != NULL);

   udelta = VG_PGROUNDUP(seg_next->start - addr);

   VG_(debugLog)(1, "signals", 
                    "extending a stack base 0x%llx down by %lld\n",
                    (ULong)seg_next->start, (ULong)udelta);
   Bool overflow;
   if (! VG_(am_extend_into_adjacent_reservation_client)
       ( seg_next->start, -(SSizeT)udelta, &overflow )) {
      Addr new_stack_base = seg_next->start - udelta;
      if (overflow)
         VG_(umsg)("Stack overflow in thread #%d: can't grow stack to %#lx\n",
                   tid, new_stack_base);
      else
         VG_(umsg)("Cannot map memory to grow the stack for thread #%d "
                   "to %#lx\n", tid, new_stack_base);
      return False;
   }

   VG_(change_stack)(VG_(clstk_id), addr, VG_(clstk_end));

   if (VG_(clo_sanity_level) > 2)
      VG_(sanity_check_general)(False);

   return True;
}

static void (*fault_catcher)(Int sig, Addr addr) = NULL;

void VG_(set_fault_catcher)(void (*catcher)(Int, Addr))
{
   if (0)
      VG_(debugLog)(0, "signals", "set fault catcher to %p\n", catcher);
   vg_assert2(NULL == catcher || NULL == fault_catcher,
              "Fault catcher is already registered");

   fault_catcher = catcher;
}

static
void sync_signalhandler_from_user ( ThreadId tid,
         Int sigNo, vki_siginfo_t *info, struct vki_ucontext *uc )
{
   ThreadId qtid;


   if (VG_(threads)[tid].status == VgTs_WaitSys) {
      if (VG_(clo_trace_signals))
         VG_(dmsg)("Delivering user-sent sync signal %d as async signal\n",
                   sigNo);

      async_signalhandler(sigNo, info, uc);
      VG_(core_panic)("async_signalhandler returned!?\n");

   } else {
      if (VG_(clo_trace_signals))
         VG_(dmsg)("Routing user-sent sync signal %d via queue\n", sigNo);

#     if defined(VGO_linux)
      
      if (info->VKI_SIGINFO_si_pid == 0) {
         VG_(umsg)("Signal %d (%s) appears to have lost its siginfo; "
                   "I can't go on.\n", sigNo, VG_(signame)(sigNo));
         VG_(printf)(
"  This may be because one of your programs has consumed your ration of\n"
"  siginfo structures.  For more information, see:\n"
"    http://kerneltrap.org/mailarchive/1/message/25599/thread\n"
"  Basically, some program on your system is building up a large queue of\n"
"  pending signals, and this causes the siginfo data for other signals to\n"
"  be dropped because it's exceeding a system limit.  However, Valgrind\n"
"  absolutely needs siginfo for SIGSEGV.  A workaround is to track down the\n"
"  offending program and avoid running it while using Valgrind, but there\n"
"  is no easy way to do this.  Apparently the problem was fixed in kernel\n"
"  2.6.12.\n");

         
         VG_(set_default_handler)(sigNo);
         deliver_signal(tid, info, uc);
         resume_scheduler(tid);
         VG_(exit)(99);       
      }
#     endif

      qtid = 0;         
#     if defined(VGO_linux)
      if (info->si_code == VKI_SI_TKILL)
         qtid = tid;    
#     endif
      queue_signal(qtid, info);
   }
}

static Addr fault_mask(Addr in)
{
#  if defined(VGA_s390x)
   return VG_PGROUNDDN(in);
#  else
   return in;
#endif
}

static Bool extend_stack_if_appropriate(ThreadId tid, vki_siginfo_t* info)
{
   Addr fault;
   Addr esp;
   NSegment const *seg, *seg_next;

   if (info->si_signo != VKI_SIGSEGV)
      return False;

   fault    = (Addr)info->VKI_SIGINFO_si_addr;
   esp      = VG_(get_SP)(tid);
   seg      = VG_(am_find_nsegment)(fault);
   seg_next = seg ? VG_(am_next_nsegment)( seg, True )
                  : NULL;

   if (VG_(clo_trace_signals)) {
      if (seg == NULL)
         VG_(dmsg)("SIGSEGV: si_code=%d faultaddr=%#lx tid=%d ESP=%#lx "
                   "seg=NULL\n",
                   info->si_code, fault, tid, esp);
      else
         VG_(dmsg)("SIGSEGV: si_code=%d faultaddr=%#lx tid=%d ESP=%#lx "
                   "seg=%#lx-%#lx\n",
                   info->si_code, fault, tid, esp, seg->start, seg->end);
   }

   if (info->si_code == VKI_SEGV_MAPERR
       && seg
       && seg->kind == SkResvn
       && seg->smode == SmUpper
       && seg_next
       && seg_next->kind == SkAnonC
       && fault >= fault_mask(esp - VG_STACK_REDZONE_SZB)) {
      Addr base = VG_PGROUNDDN(esp - VG_STACK_REDZONE_SZB);
      if (VG_(am_addr_is_in_extensible_client_stack)(base) &&
          VG_(extend_stack)(tid, base)) {
         if (VG_(clo_trace_signals))
            VG_(dmsg)("       -> extended stack base to %#lx\n",
                      VG_PGROUNDDN(fault));
         return True;
      } else {
         return False;
      }
   } else {
      return False;
   }
}

static
void sync_signalhandler_from_kernel ( ThreadId tid,
         Int sigNo, vki_siginfo_t *info, struct vki_ucontext *uc )
{
   if (fault_catcher) {
      vg_assert(VG_(in_generated_code) == False);

      (*fault_catcher)(sigNo, (Addr)info->VKI_SIGINFO_si_addr);
   }

   if (extend_stack_if_appropriate(tid, info)) {
   } else {
      ThreadState *tst = VG_(get_ThreadState)(tid);

      if (VG_(sigismember)(&tst->sig_mask, sigNo)) {
         
         VG_(set_default_handler)(sigNo);
      }

      if (VG_(in_generated_code)) {
         if (VG_(gdbserver_report_signal) (info, tid)
             || VG_(sigismember)(&tst->sig_mask, sigNo)) {
            deliver_signal(tid, info, uc);
            resume_scheduler(tid);
         }
         else
            resume_scheduler(tid);
      }

      VG_(dmsg)("VALGRIND INTERNAL ERROR: Valgrind received "
                "a signal %d (%s) - exiting\n",
                sigNo, VG_(signame)(sigNo));

      VG_(dmsg)("si_code=%x;  Faulting address: %p;  sp: %#lx\n",
                info->si_code, info->VKI_SIGINFO_si_addr,
                VG_UCONTEXT_STACK_PTR(uc));

      if (0)
         VG_(kill_self)(sigNo);  

      
      
      vg_assert(tid != 0);

      UnwindStartRegs startRegs;
      VG_(memset)(&startRegs, 0, sizeof(startRegs));

      VG_UCONTEXT_TO_UnwindStartRegs(&startRegs, uc);
      VG_(core_panic_at)("Killed by fatal signal", &startRegs);
   }
}

static
void sync_signalhandler ( Int sigNo,
                          vki_siginfo_t *info, struct vki_ucontext *uc )
{
   ThreadId tid = VG_(lwpid_to_vgtid)(VG_(gettid)());
   Bool from_user;

   if (0) 
      VG_(printf)("sync_sighandler(%d, %p, %p)\n", sigNo, info, uc);

   vg_assert(info != NULL);
   vg_assert(info->si_signo == sigNo);
   vg_assert(sigNo == VKI_SIGSEGV ||
	     sigNo == VKI_SIGBUS  ||
	     sigNo == VKI_SIGFPE  ||
	     sigNo == VKI_SIGILL  ||
	     sigNo == VKI_SIGTRAP);

   info->si_code = sanitize_si_code(info->si_code);

   from_user = !is_signal_from_kernel(tid, sigNo, info->si_code);

   if (VG_(clo_trace_signals)) {
      VG_(dmsg)("sync signal handler: "
                "signal=%d, si_code=%d, EIP=%#lx, eip=%#lx, from %s\n",
                sigNo, info->si_code, VG_(get_IP)(tid), 
                VG_UCONTEXT_INSTR_PTR(uc),
                ( from_user ? "user" : "kernel" ));
   }
   vg_assert(sigNo >= 1 && sigNo <= VG_(max_signal));


   if (from_user) {
      sync_signalhandler_from_user(  tid, sigNo, info, uc);
   } else {
      sync_signalhandler_from_kernel(tid, sigNo, info, uc);
   }
}


static void sigvgkill_handler(int signo, vki_siginfo_t *si,
                                         struct vki_ucontext *uc)
{
   ThreadId     tid = VG_(lwpid_to_vgtid)(VG_(gettid)());
   ThreadStatus at_signal = VG_(threads)[tid].status;

   if (VG_(clo_trace_signals))
      VG_(dmsg)("sigvgkill for lwp %d tid %d\n", VG_(gettid)(), tid);

   VG_(acquire_BigLock)(tid, "sigvgkill_handler");

   vg_assert(signo == VG_SIGVGKILL);
   vg_assert(si->si_signo == signo);

   if (at_signal == VgTs_WaitSys)
      VG_(post_syscall)(tid);
   

   resume_scheduler(tid);

   VG_(core_panic)("sigvgkill_handler couldn't return to the scheduler\n");
}

static __attribute((unused))
void pp_ksigaction ( vki_sigaction_toK_t* sa )
{
   Int i;
   VG_(printf)("pp_ksigaction: handler %p, flags 0x%x, restorer %p\n", 
               sa->ksa_handler, 
               (UInt)sa->sa_flags, 
#              if !defined(VGP_x86_darwin) && !defined(VGP_amd64_darwin)
                  sa->sa_restorer
#              else
                  (void*)0
#              endif
              );
   VG_(printf)("pp_ksigaction: { ");
   for (i = 1; i <= VG_(max_signal); i++)
      if (VG_(sigismember(&(sa->sa_mask),i)))
         VG_(printf)("%d ", i);
   VG_(printf)("}\n");
}

void VG_(set_default_handler)(Int signo)
{
   vki_sigaction_toK_t sa;   

   sa.ksa_handler = VKI_SIG_DFL;
   sa.sa_flags = 0;
#  if !defined(VGP_x86_darwin) && !defined(VGP_amd64_darwin)
   sa.sa_restorer = 0;
#  endif
   VG_(sigemptyset)(&sa.sa_mask);
      
   VG_(do_sys_sigaction)(signo, &sa, NULL);
}

void VG_(poll_signals)(ThreadId tid)
{
   vki_siginfo_t si, *sip;
   vki_sigset_t pollset;
   ThreadState *tst = VG_(get_ThreadState)(tid);
   vki_sigset_t saved_mask;

   
   
   VG_(sigcomplementset)( &pollset, &tst->sig_mask );

   block_all_host_signals(&saved_mask); 

   
   sip = next_queued(tid, &pollset); 

   if (sip == NULL)
      sip = next_queued(0, &pollset); 

   
   if (sip == NULL && VG_(sigtimedwait_zero)(&pollset, &si) > 0) {
      if (VG_(clo_trace_signals))
         VG_(dmsg)("poll_signals: got signal %d for thread %d\n",
                   si.si_signo, tid);
      sip = &si;
   }

   if (sip != NULL) {
      
      if (VG_(clo_trace_signals))
         VG_(dmsg)("Polling found signal %d for tid %d\n", sip->si_signo, tid);
      if (!is_sig_ign(sip, tid))
	 deliver_signal(tid, sip, NULL);
      else if (VG_(clo_trace_signals))
         VG_(dmsg)("   signal %d ignored\n", sip->si_signo);
	 
      sip->si_signo = 0;	
   }

   restore_all_host_signals(&saved_mask);
}

void VG_(sigstartup_actions) ( void )
{
   Int i, ret, vKI_SIGRTMIN;
   vki_sigset_t saved_procmask;
   vki_sigaction_fromK_t sa;

   VG_(memset)(&scss, 0, sizeof(scss));
   VG_(memset)(&skss, 0, sizeof(skss));

#  if defined(VKI_SIGRTMIN)
   vKI_SIGRTMIN = VKI_SIGRTMIN;
#  else
   vKI_SIGRTMIN = 0; 
#  endif

   
   block_all_host_signals( &saved_procmask );

   
   for (i = 1; i <= _VKI_NSIG; i++) {
      
      ret = VG_(sigaction)(i, NULL, &sa);

#     if defined(VGP_x86_darwin) || defined(VGP_amd64_darwin)
      if (ret != 0 && (i == VKI_SIGKILL || i == VKI_SIGSTOP))
         continue;
#     endif

      if (ret != 0)
	 break;

      if (vKI_SIGRTMIN > 0 
          && i >= vKI_SIGRTMIN) {
         vki_sigaction_toK_t tsa, sa2;

	 tsa.ksa_handler = (void *)sync_signalhandler;
	 tsa.sa_flags = VKI_SA_SIGINFO;
#        if !defined(VGP_x86_darwin) && !defined(VGP_amd64_darwin)
	 tsa.sa_restorer = 0;
#        endif
	 VG_(sigfillset)(&tsa.sa_mask);

	 
	 if (VG_(sigaction)(i, &tsa, NULL) != 0) {
	    
	    break;
	 }

         VG_(convert_sigaction_fromK_to_toK)( &sa, &sa2 );
	 ret = VG_(sigaction)(i, &sa2, NULL);
	 vg_assert(ret == 0);
      }

      VG_(max_signal) = i;

      if (VG_(clo_trace_signals) && VG_(clo_verbosity) > 2)
         VG_(printf)("snaffling handler 0x%lx for signal %d\n", 
                     (Addr)(sa.ksa_handler), i );

      scss.scss_per_sig[i].scss_handler  = sa.ksa_handler;
      scss.scss_per_sig[i].scss_flags    = sa.sa_flags;
      scss.scss_per_sig[i].scss_mask     = sa.sa_mask;

      scss.scss_per_sig[i].scss_restorer = NULL;
#     if !defined(VGP_x86_darwin) && !defined(VGP_amd64_darwin)
      scss.scss_per_sig[i].scss_restorer = sa.sa_restorer;
#     endif

      scss.scss_per_sig[i].scss_sa_tramp = NULL;
#     if defined(VGP_x86_darwin) || defined(VGP_amd64_darwin)
      scss.scss_per_sig[i].scss_sa_tramp = NULL;
      
#     endif
   }

   if (VG_(clo_trace_signals))
      VG_(dmsg)("Max kernel-supported signal is %d\n", VG_(max_signal));

   
   scss.scss_per_sig[VG_SIGVGKILL].scss_handler = VKI_SIG_IGN;
   scss.scss_per_sig[VG_SIGVGKILL].scss_flags   = VKI_SA_SIGINFO;
   VG_(sigfillset)(&scss.scss_per_sig[VG_SIGVGKILL].scss_mask);

   
   vg_assert(VG_(threads)[1].status == VgTs_Init);
   for (i = 2; i < VG_N_THREADS; i++)
      vg_assert(VG_(threads)[i].status == VgTs_Empty);

   VG_(threads)[1].sig_mask = saved_procmask;
   VG_(threads)[1].tmp_sig_mask = saved_procmask;

   handle_SCSS_change( True  );

}

