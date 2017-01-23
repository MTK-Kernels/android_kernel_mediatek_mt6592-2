

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
*/


#include "pub_core_basics.h"
#include "pub_core_vki.h"
#include "pub_core_vkiscnums.h"
#include "pub_core_threadstate.h"
#include "pub_core_aspacemgr.h"
#include "pub_core_libcbase.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcprint.h"
#include "pub_core_machine.h"
#include "pub_core_options.h"
#include "pub_core_sigframe.h"
#include "pub_core_signals.h"
#include "pub_core_tooliface.h"
#include "pub_core_trampoline.h"
#include "priv_sigframe.h"

#if defined(VGA_s390x)


#define SET_SIGNAL_GPR(zztst, zzn, zzval)                    \
   do { zztst->arch.vex.guest_r##zzn = (unsigned long)(zzval);              \
      VG_TRACK( post_reg_write, Vg_CoreSignal, zztst->tid,     \
                offsetof(VexGuestS390XState,guest_r##zzn), \
                sizeof(UWord) );                             \
   } while (0)




struct vg_sigframe
{
   
   UInt magicPI;

   UInt handlerflags;	


   
   Int  sigNo_private;

   VexGuestS390XState vex_shadow1;
   VexGuestS390XState vex_shadow2;

   
   VexGuestS390XState vex;
   

   
   vki_sigset_t	mask;

   UInt magicE;
};

#define S390_SYSCALL_SIZE 2

struct sigframe
{
   UChar callee_used_stack[__VKI_SIGNAL_FRAMESIZE];
   struct vki_sigcontext sc;
   _vki_sigregs sregs;
   Int sigNo;
   UChar retcode[S390_SYSCALL_SIZE];

   struct vg_sigframe vg;
};

struct rt_sigframe
{
   UChar callee_used_stack[__VKI_SIGNAL_FRAMESIZE];
   UChar retcode[S390_SYSCALL_SIZE];
   struct vki_siginfo info;
   struct vki_ucontext uc;

   struct vg_sigframe vg;
};


static void save_sigregs(ThreadState *tst, _vki_sigregs *sigregs)
{
   sigregs->regs.gprs[0]  = tst->arch.vex.guest_r0;
   sigregs->regs.gprs[1]  = tst->arch.vex.guest_r1;
   sigregs->regs.gprs[2]  = tst->arch.vex.guest_r2;
   sigregs->regs.gprs[3]  = tst->arch.vex.guest_r3;
   sigregs->regs.gprs[4]  = tst->arch.vex.guest_r4;
   sigregs->regs.gprs[5]  = tst->arch.vex.guest_r5;
   sigregs->regs.gprs[6]  = tst->arch.vex.guest_r6;
   sigregs->regs.gprs[7]  = tst->arch.vex.guest_r7;
   sigregs->regs.gprs[8]  = tst->arch.vex.guest_r8;
   sigregs->regs.gprs[9]  = tst->arch.vex.guest_r9;
   sigregs->regs.gprs[10] = tst->arch.vex.guest_r10;
   sigregs->regs.gprs[11] = tst->arch.vex.guest_r11;
   sigregs->regs.gprs[12] = tst->arch.vex.guest_r12;
   sigregs->regs.gprs[13] = tst->arch.vex.guest_r13;
   sigregs->regs.gprs[14] = tst->arch.vex.guest_r14;
   sigregs->regs.gprs[15] = tst->arch.vex.guest_r15;

   sigregs->regs.acrs[0]  = tst->arch.vex.guest_a0;
   sigregs->regs.acrs[1]  = tst->arch.vex.guest_a1;
   sigregs->regs.acrs[2]  = tst->arch.vex.guest_a2;
   sigregs->regs.acrs[3]  = tst->arch.vex.guest_a3;
   sigregs->regs.acrs[4]  = tst->arch.vex.guest_a4;
   sigregs->regs.acrs[5]  = tst->arch.vex.guest_a5;
   sigregs->regs.acrs[6]  = tst->arch.vex.guest_a6;
   sigregs->regs.acrs[7]  = tst->arch.vex.guest_a7;
   sigregs->regs.acrs[8]  = tst->arch.vex.guest_a8;
   sigregs->regs.acrs[9]  = tst->arch.vex.guest_a9;
   sigregs->regs.acrs[10] = tst->arch.vex.guest_a10;
   sigregs->regs.acrs[11] = tst->arch.vex.guest_a11;
   sigregs->regs.acrs[12] = tst->arch.vex.guest_a12;
   sigregs->regs.acrs[13] = tst->arch.vex.guest_a13;
   sigregs->regs.acrs[14] = tst->arch.vex.guest_a14;
   sigregs->regs.acrs[15] = tst->arch.vex.guest_a15;

   sigregs->fpregs.fprs[0] = tst->arch.vex.guest_f0;
   sigregs->fpregs.fprs[1] = tst->arch.vex.guest_f1;
   sigregs->fpregs.fprs[2] = tst->arch.vex.guest_f2;
   sigregs->fpregs.fprs[3] = tst->arch.vex.guest_f3;
   sigregs->fpregs.fprs[4] = tst->arch.vex.guest_f4;
   sigregs->fpregs.fprs[5] = tst->arch.vex.guest_f5;
   sigregs->fpregs.fprs[6] = tst->arch.vex.guest_f6;
   sigregs->fpregs.fprs[7] = tst->arch.vex.guest_f7;
   sigregs->fpregs.fprs[8] = tst->arch.vex.guest_f8;
   sigregs->fpregs.fprs[9] = tst->arch.vex.guest_f9;
   sigregs->fpregs.fprs[10] = tst->arch.vex.guest_f10;
   sigregs->fpregs.fprs[11] = tst->arch.vex.guest_f11;
   sigregs->fpregs.fprs[12] = tst->arch.vex.guest_f12;
   sigregs->fpregs.fprs[13] = tst->arch.vex.guest_f13;
   sigregs->fpregs.fprs[14] = tst->arch.vex.guest_f14;
   sigregs->fpregs.fprs[15] = tst->arch.vex.guest_f15;
   sigregs->fpregs.fpc      = tst->arch.vex.guest_fpc;

   sigregs->regs.psw.addr = tst->arch.vex.guest_IA;
   
   sigregs->regs.psw.mask = 0x0705000180000000UL;
}

static void restore_sigregs(ThreadState *tst, _vki_sigregs *sigregs)
{
   tst->arch.vex.guest_r0  = sigregs->regs.gprs[0];
   tst->arch.vex.guest_r1  = sigregs->regs.gprs[1];
   tst->arch.vex.guest_r2  = sigregs->regs.gprs[2];
   tst->arch.vex.guest_r3  = sigregs->regs.gprs[3];
   tst->arch.vex.guest_r4  = sigregs->regs.gprs[4];
   tst->arch.vex.guest_r5  = sigregs->regs.gprs[5];
   tst->arch.vex.guest_r6  = sigregs->regs.gprs[6];
   tst->arch.vex.guest_r7  = sigregs->regs.gprs[7];
   tst->arch.vex.guest_r8  = sigregs->regs.gprs[8];
   tst->arch.vex.guest_r9  = sigregs->regs.gprs[9];
   tst->arch.vex.guest_r10 = sigregs->regs.gprs[10];
   tst->arch.vex.guest_r11 = sigregs->regs.gprs[11];
   tst->arch.vex.guest_r12 = sigregs->regs.gprs[12];
   tst->arch.vex.guest_r13 = sigregs->regs.gprs[13];
   tst->arch.vex.guest_r14 = sigregs->regs.gprs[14];
   tst->arch.vex.guest_r15 = sigregs->regs.gprs[15];

   tst->arch.vex.guest_a0  = sigregs->regs.acrs[0];
   tst->arch.vex.guest_a1  = sigregs->regs.acrs[1];
   tst->arch.vex.guest_a2  = sigregs->regs.acrs[2];
   tst->arch.vex.guest_a3  = sigregs->regs.acrs[3];
   tst->arch.vex.guest_a4  = sigregs->regs.acrs[4];
   tst->arch.vex.guest_a5  = sigregs->regs.acrs[5];
   tst->arch.vex.guest_a6  = sigregs->regs.acrs[6];
   tst->arch.vex.guest_a7  = sigregs->regs.acrs[7];
   tst->arch.vex.guest_a8  = sigregs->regs.acrs[8];
   tst->arch.vex.guest_a9  = sigregs->regs.acrs[9];
   tst->arch.vex.guest_a10 = sigregs->regs.acrs[10];
   tst->arch.vex.guest_a11 = sigregs->regs.acrs[11];
   tst->arch.vex.guest_a12 = sigregs->regs.acrs[12];
   tst->arch.vex.guest_a13 = sigregs->regs.acrs[13];
   tst->arch.vex.guest_a14 = sigregs->regs.acrs[14];
   tst->arch.vex.guest_a15 = sigregs->regs.acrs[15];

   tst->arch.vex.guest_f0  = sigregs->fpregs.fprs[0];
   tst->arch.vex.guest_f1  = sigregs->fpregs.fprs[1];
   tst->arch.vex.guest_f2  = sigregs->fpregs.fprs[2];
   tst->arch.vex.guest_f3  = sigregs->fpregs.fprs[3];
   tst->arch.vex.guest_f4  = sigregs->fpregs.fprs[4];
   tst->arch.vex.guest_f5  = sigregs->fpregs.fprs[5];
   tst->arch.vex.guest_f6  = sigregs->fpregs.fprs[6];
   tst->arch.vex.guest_f7  = sigregs->fpregs.fprs[7];
   tst->arch.vex.guest_f8  = sigregs->fpregs.fprs[8];
   tst->arch.vex.guest_f9  = sigregs->fpregs.fprs[9];
   tst->arch.vex.guest_f10 = sigregs->fpregs.fprs[10];
   tst->arch.vex.guest_f11 = sigregs->fpregs.fprs[11];
   tst->arch.vex.guest_f12 = sigregs->fpregs.fprs[12];
   tst->arch.vex.guest_f13 = sigregs->fpregs.fprs[13];
   tst->arch.vex.guest_f14 = sigregs->fpregs.fprs[14];
   tst->arch.vex.guest_f15 = sigregs->fpregs.fprs[15];
   tst->arch.vex.guest_fpc = sigregs->fpregs.fpc;

   tst->arch.vex.guest_IA = sigregs->regs.psw.addr;
}



static void build_vg_sigframe(struct vg_sigframe *frame,
			      ThreadState *tst,
			      UInt flags,
			      Int sigNo)
{
   frame->sigNo_private = sigNo;
   frame->magicPI       = 0x31415927;
   frame->vex_shadow1   = tst->arch.vex_shadow1;
   frame->vex_shadow2   = tst->arch.vex_shadow2;
   
   frame->vex           = tst->arch.vex;
   
   frame->mask          = tst->sig_mask;
   frame->handlerflags  = flags;
   frame->magicE        = 0x27182818;
}


static Addr build_sigframe(ThreadState *tst,
			   Addr sp_top_of_frame,
			   const vki_siginfo_t *siginfo,
			   const struct vki_ucontext *siguc,
			   UInt flags,
			   const vki_sigset_t *mask,
			   void *restorer)
{
   struct sigframe *frame;
   Addr sp = sp_top_of_frame;

   vg_assert((flags & VKI_SA_SIGINFO) == 0);
   vg_assert((sizeof(*frame) & 7) == 0);
   vg_assert((sp & 7) == 0);

   sp -= sizeof(*frame);
   frame = (struct sigframe *)sp;

   if (! ML_(sf_maybe_extend_stack)(tst, sp, sizeof(*frame), flags))
      return sp_top_of_frame;

   /* retcode, sigNo, sc, sregs fields are to be written */
   VG_TRACK( pre_mem_write, Vg_CoreSignal, tst->tid, "signal handler frame",
	     sp, offsetof(struct sigframe, vg) );

   save_sigregs(tst, &frame->sregs);

   frame->sigNo = siginfo->si_signo;
   frame->sc.sregs = &frame->sregs;
   VG_(memcpy)(frame->sc.oldmask, mask->sig, sizeof(frame->sc.oldmask));

   if (flags & VKI_SA_RESTORER) {
      SET_SIGNAL_GPR(tst, 14, restorer);
   } else {
      frame->retcode[0] = 0x0a;
      frame->retcode[1] = __NR_sigreturn;
      SET_SIGNAL_GPR(tst, 14, (Addr)&VG_(s390x_linux_SUBST_FOR_sigreturn));
   }

   SET_SIGNAL_GPR(tst, 2, siginfo->si_signo);
   SET_SIGNAL_GPR(tst, 3, &frame->sc);
   

   
   *((Addr *) sp) = sp_top_of_frame;

   VG_TRACK( post_mem_write, Vg_CoreSignal, tst->tid,
             sp, offsetof(struct sigframe, vg) );

   build_vg_sigframe(&frame->vg, tst, flags, siginfo->si_signo);

   return sp;
}

static Addr build_rt_sigframe(ThreadState *tst,
			      Addr sp_top_of_frame,
			      const vki_siginfo_t *siginfo,
			      const struct vki_ucontext *siguc,
			      UInt flags,
			      const vki_sigset_t *mask,
			      void *restorer)
{
   struct rt_sigframe *frame;
   Addr sp = sp_top_of_frame;
   Int sigNo = siginfo->si_signo;

   vg_assert((flags & VKI_SA_SIGINFO) != 0);
   vg_assert((sizeof(*frame) & 7) == 0);
   vg_assert((sp & 7) == 0);

   sp -= sizeof(*frame);
   frame = (struct rt_sigframe *)sp;

   if (! ML_(sf_maybe_extend_stack)(tst, sp, sizeof(*frame), flags))
      return sp_top_of_frame;

   /* retcode, sigNo, sc, sregs fields are to be written */
   VG_TRACK( pre_mem_write, Vg_CoreSignal, tst->tid, "signal handler frame",
	     sp, offsetof(struct rt_sigframe, vg) );

   save_sigregs(tst, &frame->uc.uc_mcontext);

   if (flags & VKI_SA_RESTORER) {
      frame->retcode[0] = 0;
      frame->retcode[1] = 0;
      SET_SIGNAL_GPR(tst, 14, restorer);
   } else {
      frame->retcode[0] = 0x0a;
      frame->retcode[1] = __NR_rt_sigreturn;
      SET_SIGNAL_GPR(tst, 14, (Addr)&VG_(s390x_linux_SUBST_FOR_rt_sigreturn));
   }

   VG_(memcpy)(&frame->info, siginfo, sizeof(vki_siginfo_t));
   frame->uc.uc_flags = 0;
   frame->uc.uc_link = 0;
   frame->uc.uc_sigmask = *mask;
   frame->uc.uc_stack = tst->altstack;

   SET_SIGNAL_GPR(tst, 2, siginfo->si_signo);
   SET_SIGNAL_GPR(tst, 3, &frame->info);
   SET_SIGNAL_GPR(tst, 4, &frame->uc);

   
   *((Addr *) sp) = sp_top_of_frame;

   VG_TRACK( post_mem_write, Vg_CoreSignal, tst->tid,
             sp, offsetof(struct rt_sigframe, vg) );

   build_vg_sigframe(&frame->vg, tst, flags, sigNo);
   return sp;
}

void VG_(sigframe_create)( ThreadId tid,
			   Addr sp_top_of_frame,
			   const vki_siginfo_t *siginfo,
			   const struct vki_ucontext *siguc,
			   void *handler,
			   UInt flags,
			   const vki_sigset_t *mask,
			   void *restorer )
{
   Addr sp;
   ThreadState* tst = VG_(get_ThreadState)(tid);

   if (flags & VKI_SA_SIGINFO)
      sp = build_rt_sigframe(tst, sp_top_of_frame, siginfo, siguc,
			     flags, mask, restorer);
   else
      sp = build_sigframe(tst, sp_top_of_frame, siginfo, siguc,
			  flags, mask, restorer);

   
   VG_(set_SP)(tid, sp);
   VG_TRACK( post_reg_write, Vg_CoreSignal, tid, VG_O_STACK_PTR, sizeof(Addr));

   tst->arch.vex.guest_IA = (Addr) handler;
   tst->arch.vex.guest_counter = 0;
}



static
Bool restore_vg_sigframe ( ThreadState *tst,
                           struct vg_sigframe *frame, Int *sigNo )
{
   if (frame->magicPI != 0x31415927 ||
       frame->magicE  != 0x27182818) {
      VG_(message)(Vg_UserMsg, "Thread %d return signal frame "
			       "corrupted.  Killing process.\n",
		   tst->tid);
      VG_(set_default_handler)(VKI_SIGSEGV);
      VG_(synth_fault)(tst->tid);
      *sigNo = VKI_SIGSEGV;
      return False;
   }
   tst->sig_mask         = frame->mask;
   tst->tmp_sig_mask     = frame->mask;
   tst->arch.vex_shadow1 = frame->vex_shadow1;
   tst->arch.vex_shadow2 = frame->vex_shadow2;
   
   tst->arch.vex         = frame->vex;
   
   *sigNo                = frame->sigNo_private;
   return True;
}

static
SizeT restore_sigframe ( ThreadState *tst,
                         struct sigframe *frame, Int *sigNo )
{
   if (restore_vg_sigframe(tst, &frame->vg, sigNo))
      restore_sigregs(tst, frame->sc.sregs);

   return sizeof(*frame);
}

static
SizeT restore_rt_sigframe ( ThreadState *tst,
                            struct rt_sigframe *frame, Int *sigNo )
{
   if (restore_vg_sigframe(tst, &frame->vg, sigNo)) {
      restore_sigregs(tst, &frame->uc.uc_mcontext);
   }
   return sizeof(*frame);
}


void VG_(sigframe_destroy)( ThreadId tid, Bool isRT )
{
   Addr          sp;
   ThreadState*  tst;
   SizeT         size;
   Int            sigNo;

   tst = VG_(get_ThreadState)(tid);

   
   sp   = tst->arch.vex.guest_SP;

   if (!isRT)
      size = restore_sigframe(tst, (struct sigframe *)sp, &sigNo);
   else
      size = restore_rt_sigframe(tst, (struct rt_sigframe *)sp, &sigNo);

   VG_TRACK( die_mem_stack_signal, sp - VG_STACK_REDZONE_SZB,
             size + VG_STACK_REDZONE_SZB );

   if (VG_(clo_trace_signals))
      VG_(message)(
         Vg_DebugMsg,
         "VG_(sigframe_destroy) (thread %d): isRT=%d valid magic; IP=%#llx\n",
         tid, isRT, tst->arch.vex.guest_IA);

   
   VG_TRACK( post_deliver_signal, tid, sigNo );
}

#endif 

