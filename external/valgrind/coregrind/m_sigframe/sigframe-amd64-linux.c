

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2000-2013 Nicholas Nethercote
      njn@valgrind.org

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

#if defined(VGP_amd64_linux)

#include "pub_core_basics.h"
#include "pub_core_vki.h"
#include "pub_core_threadstate.h"
#include "pub_core_aspacemgr.h"
#include "pub_core_libcbase.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcprint.h"
#include "pub_core_machine.h"
#include "pub_core_options.h"
#include "pub_core_signals.h"
#include "pub_core_tooliface.h"
#include "pub_core_trampoline.h"
#include "pub_core_sigframe.h"   
#include "priv_sigframe.h"

/* This module creates and removes signal frames for signal deliveries
   on amd64-linux.

   Note, this file contains kernel-specific knowledge in the form of
   'struct rt_sigframe'.  How does that relate to the vki kernel
   interface stuff?

   A 'struct rtsigframe' is pushed onto the client's stack.  This
   contains a subsidiary vki_ucontext.  That holds the vcpu's state
   across the signal, so that the sighandler can mess with the vcpu
   state if it really wants.

   FIXME: sigcontexting is basically broken for the moment.  When
   delivering a signal, the integer registers and %rflags are
   correctly written into the sigcontext, however the FP and SSE state
   is not.  When returning from a signal, only the integer registers
   are restored from the sigcontext; the rest of the CPU state is
   restored to what it was before the signal.

   This will be fixed.
*/





struct vg_sigframe
{
   
   UInt magicPI;

   UInt handlerflags;	


   
   Int  sigNo_private;

   VexGuestAMD64State vex_shadow1;
   VexGuestAMD64State vex_shadow2;

   
   VexGuestAMD64State vex;
   

   
   vki_sigset_t	mask;

   UInt magicE;
};

struct rt_sigframe
{
   
   Addr retaddr;

   
   struct vki_ucontext uContext;

   
   vki_siginfo_t sigInfo;
   struct _vki_fpstate fpstate;

   struct vg_sigframe vg;
};





static 
void synth_ucontext(ThreadId tid, const vki_siginfo_t *si,
                    UWord trapno, UWord err, const vki_sigset_t *set, 
                    struct vki_ucontext *uc, struct _vki_fpstate *fpstate)
{
   ThreadState *tst = VG_(get_ThreadState)(tid);
   struct vki_sigcontext *sc = &uc->uc_mcontext;

   VG_(memset)(uc, 0, sizeof(*uc));

   uc->uc_flags = 0;
   uc->uc_link = 0;
   uc->uc_sigmask = *set;
   uc->uc_stack = tst->altstack;
   sc->fpstate = fpstate;

   

#  define SC2(reg,REG)  sc->reg = tst->arch.vex.guest_##REG
   SC2(r8,R8);
   SC2(r9,R9);
   SC2(r10,R10);
   SC2(r11,R11);
   SC2(r12,R12);
   SC2(r13,R13);
   SC2(r14,R14);
   SC2(r15,R15);
   SC2(rdi,RDI);
   SC2(rsi,RSI);
   SC2(rbp,RBP);
   SC2(rbx,RBX);
   SC2(rdx,RDX);
   SC2(rax,RAX);
   SC2(rcx,RCX);
   SC2(rsp,RSP);

   SC2(rip,RIP);
   sc->eflags = LibVEX_GuestAMD64_get_rflags(&tst->arch.vex);
   
   
   
   sc->trapno = trapno;
   sc->err = err;
#  undef SC2

   sc->cr2 = (UWord)si->_sifields._sigfault._addr;
}



static void build_vg_sigframe(struct vg_sigframe *frame,
			      ThreadState *tst,
			      const vki_sigset_t *mask,
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


static Addr build_rt_sigframe(ThreadState *tst,
			      Addr rsp_top_of_frame,
			      const vki_siginfo_t *siginfo,
                              const struct vki_ucontext *siguc,
			      void *handler, UInt flags,
			      const vki_sigset_t *mask,
			      void *restorer)
{
   struct rt_sigframe *frame;
   Addr rsp = rsp_top_of_frame;
   Int	sigNo = siginfo->si_signo;
   UWord trapno;
   UWord err;

   rsp -= sizeof(*frame);
   rsp = VG_ROUNDDN(rsp, 16) - 8;
   frame = (struct rt_sigframe *)rsp;

   if (! ML_(sf_maybe_extend_stack)(tst, rsp, sizeof(*frame), flags))
      return rsp_top_of_frame;

   /* retaddr, siginfo, uContext fields are to be written */
   VG_TRACK( pre_mem_write, Vg_CoreSignal, tst->tid, "rt signal handler frame", 
	     rsp, offsetof(struct rt_sigframe, vg) );

   if (flags & VKI_SA_RESTORER)
      frame->retaddr = (Addr)restorer;
   else
      frame->retaddr = (Addr)&VG_(amd64_linux_SUBST_FOR_rt_sigreturn);

   if (siguc) {
      trapno = siguc->uc_mcontext.trapno;
      err = siguc->uc_mcontext.err;
   } else {
      trapno = 0;
      err = 0;
   }

   VG_(memcpy)(&frame->sigInfo, siginfo, sizeof(vki_siginfo_t));

   
   if (sigNo == VKI_SIGILL && siginfo->si_code > 0)
      frame->sigInfo._sifields._sigfault._addr 
         = (void*)tst->arch.vex.guest_RIP;

   synth_ucontext(tst->tid, siginfo, trapno, err, mask,
                  &frame->uContext, &frame->fpstate);

   VG_TRACK( post_mem_write,  Vg_CoreSignal, tst->tid, 
             rsp, offsetof(struct rt_sigframe, vg) );

   build_vg_sigframe(&frame->vg, tst, mask, flags, sigNo);
   
   return rsp;
}


void VG_(sigframe_create)( ThreadId tid, 
                            Addr rsp_top_of_frame,
                            const vki_siginfo_t *siginfo,
                            const struct vki_ucontext *siguc,
                            void *handler, 
                            UInt flags,
                            const vki_sigset_t *mask,
                            void *restorer )
{
   Addr rsp;
   struct rt_sigframe *frame;
   ThreadState* tst = VG_(get_ThreadState)(tid);

   rsp = build_rt_sigframe(tst, rsp_top_of_frame, siginfo, siguc,
                                handler, flags, mask, restorer);
   frame = (struct rt_sigframe *)rsp;

   
   
   VG_(set_SP)(tid, rsp);
   VG_TRACK( post_reg_write, Vg_CoreSignal, tid, VG_O_STACK_PTR, sizeof(Addr));

   
   tst->arch.vex.guest_RIP = (Addr) handler;
   tst->arch.vex.guest_RDI = (ULong) siginfo->si_signo;
   tst->arch.vex.guest_RSI = (Addr) &frame->sigInfo;
   tst->arch.vex.guest_RDX = (Addr) &frame->uContext;
   /* And tell the tool that these registers have been written. */
   VG_TRACK( post_reg_write, Vg_CoreSignal, tst->tid,
             offsetof(VexGuestAMD64State,guest_RIP), sizeof(UWord) );
   VG_TRACK( post_reg_write, Vg_CoreSignal, tst->tid,
             offsetof(VexGuestAMD64State,guest_RDI), sizeof(UWord) );
   VG_TRACK( post_reg_write, Vg_CoreSignal, tst->tid,
             offsetof(VexGuestAMD64State,guest_RSI), sizeof(UWord) );
   VG_TRACK( post_reg_write, Vg_CoreSignal, tst->tid,
             offsetof(VexGuestAMD64State,guest_RDX), sizeof(UWord) );


   if (0)
      VG_(printf)("pushed signal frame; %%RSP now = %#lx, "
                  "next %%RIP = %#llx, status=%d\n",
		  rsp, tst->arch.vex.guest_RIP, tst->status);
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
void restore_sigcontext( ThreadState *tst, 
                         struct vki_sigcontext *sc, 
                         struct _vki_fpstate *fpstate )
{
   tst->arch.vex.guest_RAX     = sc->rax;
   tst->arch.vex.guest_RCX     = sc->rcx;
   tst->arch.vex.guest_RDX     = sc->rdx;
   tst->arch.vex.guest_RBX     = sc->rbx;
   tst->arch.vex.guest_RBP     = sc->rbp; 
   tst->arch.vex.guest_RSP     = sc->rsp;
   tst->arch.vex.guest_RSI     = sc->rsi;
   tst->arch.vex.guest_RDI     = sc->rdi;
   tst->arch.vex.guest_R8      = sc->r8;
   tst->arch.vex.guest_R9      = sc->r9;
   tst->arch.vex.guest_R10     = sc->r10;
   tst->arch.vex.guest_R11     = sc->r11;
   tst->arch.vex.guest_R12     = sc->r12;
   tst->arch.vex.guest_R13     = sc->r13;
   tst->arch.vex.guest_R14     = sc->r14;
   tst->arch.vex.guest_R15     = sc->r15;
   tst->arch.vex.guest_RIP     = sc->rip;


}


static 
SizeT restore_rt_sigframe ( ThreadState *tst, 
                            struct rt_sigframe *frame, Int *sigNo )
{
   if (restore_vg_sigframe(tst, &frame->vg, sigNo))
      restore_sigcontext(tst, &frame->uContext.uc_mcontext, &frame->fpstate);

   return sizeof(*frame);
}


void VG_(sigframe_destroy)( ThreadId tid, Bool isRT )
{
   Addr          rsp;
   ThreadState*  tst;
   SizeT	 size;
   Int		 sigNo;

   vg_assert(isRT);

   tst = VG_(get_ThreadState)(tid);

   
   rsp   = tst->arch.vex.guest_RSP;

   size = restore_rt_sigframe(tst, (struct rt_sigframe *)rsp, &sigNo);

   VG_TRACK( die_mem_stack_signal, rsp - VG_STACK_REDZONE_SZB,
             size + VG_STACK_REDZONE_SZB );

   if (VG_(clo_trace_signals))
      VG_(message)(
         Vg_DebugMsg, 
         "VG_(signal_return) (thread %d): isRT=%d valid magic; RIP=%#llx\n",
         tid, isRT, tst->arch.vex.guest_RIP);

   
   VG_TRACK( post_deliver_signal, tid, sigNo );
}

#endif 

