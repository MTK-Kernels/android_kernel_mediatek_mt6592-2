

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

#if defined(VGP_x86_linux)

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
   on x86-linux.

   Note, this file contains kernel-specific knowledge in the form of
   'struct sigframe' and 'struct rt_sigframe'.  How does that relate
   to the vki kernel interface stuff?

   Either a 'struct sigframe' or a 'struct rtsigframe' is pushed 
   onto the client's stack.  This contains a subsidiary
   vki_ucontext.  That holds the vcpu's state across the signal, 
   so that the sighandler can mess with the vcpu state if it
   really wants.

   FIXME: sigcontexting is basically broken for the moment.  When
   delivering a signal, the integer registers and %eflags are
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

   VexGuestX86State vex_shadow1;
   VexGuestX86State vex_shadow2;

   
   VexGuestX86State vex;
   

   
   vki_sigset_t	mask;

   UInt magicE;
};

struct sigframe
{
   
   Addr retaddr;
   Int  sigNo;

   struct vki_sigcontext sigContext;
   struct _vki_fpstate fpstate;

   struct vg_sigframe vg;
};

struct rt_sigframe
{
   
   Addr retaddr;
   Int  sigNo;

   
   Addr psigInfo;

   
   Addr puContext;
   
   vki_siginfo_t sigInfo;

   
   struct vki_ucontext uContext;
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
   SC2(gs,GS);
   SC2(fs,FS);
   SC2(es,ES);
   SC2(ds,DS);

   SC2(edi,EDI);
   SC2(esi,ESI);
   SC2(ebp,EBP);
   SC2(esp,ESP);
   SC2(ebx,EBX);
   SC2(edx,EDX);
   SC2(ecx,ECX);
   SC2(eax,EAX);

   SC2(eip,EIP);
   SC2(cs,CS);
   sc->eflags = LibVEX_GuestX86_get_eflags(&tst->arch.vex);
   SC2(ss,SS);
   
   sc->trapno = trapno;
   sc->err = err;
#  undef SC2

   sc->cr2 = (UInt)si->_sifields._sigfault._addr;
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
			   Addr esp_top_of_frame,
			   const vki_siginfo_t *siginfo,
                           const struct vki_ucontext *siguc,
			   UInt flags,
			   const vki_sigset_t *mask,
			   void *restorer)
{
   struct sigframe *frame;
   Addr esp = esp_top_of_frame;
   Int	sigNo = siginfo->si_signo;
   UWord trapno;
   UWord err;
   struct vki_ucontext uc;

   vg_assert((flags & VKI_SA_SIGINFO) == 0);

   esp -= sizeof(*frame);
   esp = VG_ROUNDDN(esp, 16);
   frame = (struct sigframe *)esp;

   if (! ML_(sf_maybe_extend_stack)(tst, esp, sizeof(*frame), flags))
      return esp_top_of_frame;

   /* retaddr, sigNo, siguContext fields are to be written */
   VG_TRACK( pre_mem_write, Vg_CoreSignal, tst->tid, "signal handler frame", 
	     esp, offsetof(struct sigframe, vg) );

   frame->sigNo = sigNo;

   if (flags & VKI_SA_RESTORER)
      frame->retaddr = (Addr)restorer;
   else
      frame->retaddr = (Addr)&VG_(x86_linux_SUBST_FOR_sigreturn);

   if (siguc) {
      trapno = siguc->uc_mcontext.trapno;
      err = siguc->uc_mcontext.err;
   } else {
      trapno = 0;
      err = 0;
   }

   synth_ucontext(tst->tid, siginfo, trapno, err, mask, &uc, &frame->fpstate);

   VG_(memcpy)(&frame->sigContext, &uc.uc_mcontext, 
	       sizeof(struct vki_sigcontext));
   frame->sigContext.oldmask = mask->sig[0];

   VG_TRACK( post_mem_write, Vg_CoreSignal, tst->tid, 
             esp, offsetof(struct sigframe, vg) );

   build_vg_sigframe(&frame->vg, tst, flags, sigNo);
   
   return esp;
}


static Addr build_rt_sigframe(ThreadState *tst,
			      Addr esp_top_of_frame,
			      const vki_siginfo_t *siginfo,
                              const struct vki_ucontext *siguc,
			      UInt flags,
			      const vki_sigset_t *mask,
			      void *restorer)
{
   struct rt_sigframe *frame;
   Addr esp = esp_top_of_frame;
   Int	sigNo = siginfo->si_signo;
   UWord trapno;
   UWord err;

   vg_assert((flags & VKI_SA_SIGINFO) != 0);

   esp -= sizeof(*frame);
   esp = VG_ROUNDDN(esp, 16);
   frame = (struct rt_sigframe *)esp;

   if (! ML_(sf_maybe_extend_stack)(tst, esp, sizeof(*frame), flags))
      return esp_top_of_frame;

   /* retaddr, sigNo, pSiginfo, puContext fields are to be written */
   VG_TRACK( pre_mem_write, Vg_CoreSignal, tst->tid, "rt signal handler frame", 
	     esp, offsetof(struct rt_sigframe, vg) );

   frame->sigNo = sigNo;

   if (flags & VKI_SA_RESTORER)
      frame->retaddr = (Addr)restorer;
   else
      frame->retaddr = (Addr)&VG_(x86_linux_SUBST_FOR_rt_sigreturn);

   if (siguc) {
      trapno = siguc->uc_mcontext.trapno;
      err = siguc->uc_mcontext.err;
   } else {
      trapno = 0;
      err = 0;
   }

   frame->psigInfo = (Addr)&frame->sigInfo;
   frame->puContext = (Addr)&frame->uContext;
   VG_(memcpy)(&frame->sigInfo, siginfo, sizeof(vki_siginfo_t));

   
   if (sigNo == VKI_SIGILL && siginfo->si_code > 0)
      frame->sigInfo._sifields._sigfault._addr 
         = (void*)tst->arch.vex.guest_EIP;

   synth_ucontext(tst->tid, siginfo, trapno, err, mask,
                  &frame->uContext, &frame->fpstate);

   VG_TRACK( post_mem_write,  Vg_CoreSignal, tst->tid, 
             esp, offsetof(struct rt_sigframe, vg) );

   build_vg_sigframe(&frame->vg, tst, flags, sigNo);
   
   return esp;
}


void VG_(sigframe_create)( ThreadId tid, 
                           Addr esp_top_of_frame,
                           const vki_siginfo_t *siginfo,
                           const struct vki_ucontext *siguc,
                           void *handler, 
                           UInt flags,
                           const vki_sigset_t *mask,
		           void *restorer )
{
   Addr		esp;
   ThreadState* tst = VG_(get_ThreadState)(tid);

   if (flags & VKI_SA_SIGINFO)
      esp = build_rt_sigframe(tst, esp_top_of_frame, siginfo, siguc,
                                   flags, mask, restorer);
   else
      esp = build_sigframe(tst, esp_top_of_frame, siginfo, siguc,
                                flags, mask, restorer);

   
   
   VG_(set_SP)(tid, esp);
   VG_TRACK( post_reg_write, Vg_CoreSignal, tid, VG_O_STACK_PTR, sizeof(Addr));

   
   tst->arch.vex.guest_EIP = (Addr) handler;

   if (0)
      VG_(printf)("pushed signal frame; %%ESP now = %#lx, "
                  "next %%EIP = %#x, status=%d\n",
		  esp, tst->arch.vex.guest_EIP, tst->status);
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
   tst->arch.vex.guest_EAX     = sc->eax;
   tst->arch.vex.guest_ECX     = sc->ecx;
   tst->arch.vex.guest_EDX     = sc->edx;
   tst->arch.vex.guest_EBX     = sc->ebx;
   tst->arch.vex.guest_EBP     = sc->ebp; 
   tst->arch.vex.guest_ESP     = sc->esp;
   tst->arch.vex.guest_ESI     = sc->esi;
   tst->arch.vex.guest_EDI     = sc->edi;
   tst->arch.vex.guest_EIP     = sc->eip;
   tst->arch.vex.guest_CS      = sc->cs; 
   tst->arch.vex.guest_SS      = sc->ss;
   tst->arch.vex.guest_DS      = sc->ds;
   tst->arch.vex.guest_ES      = sc->es;
   tst->arch.vex.guest_FS      = sc->fs;
   tst->arch.vex.guest_GS      = sc->gs;

}


static 
SizeT restore_sigframe ( ThreadState *tst, 
                         struct sigframe *frame, Int *sigNo )
{
   if (restore_vg_sigframe(tst, &frame->vg, sigNo))
      restore_sigcontext(tst, &frame->sigContext, &frame->fpstate);

   return sizeof(*frame);
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
   Addr          esp;
   ThreadState*  tst;
   SizeT	 size;
   Int		 sigNo;

   tst = VG_(get_ThreadState)(tid);

   
   esp   = tst->arch.vex.guest_ESP;

   if (!isRT)
      size = restore_sigframe(tst, (struct sigframe *)esp, &sigNo);
   else
      size = restore_rt_sigframe(tst, (struct rt_sigframe *)esp, &sigNo);

   VG_TRACK( die_mem_stack_signal, esp - VG_STACK_REDZONE_SZB,
             size + VG_STACK_REDZONE_SZB );

   if (VG_(clo_trace_signals))
      VG_(message)(
         Vg_DebugMsg, 
         "VG_(signal_return) (thread %d): isRT=%d valid magic; EIP=%#x\n",
         tid, isRT, tst->arch.vex.guest_EIP);

   
   VG_TRACK( post_deliver_signal, tid, sigNo );
}

#endif 

