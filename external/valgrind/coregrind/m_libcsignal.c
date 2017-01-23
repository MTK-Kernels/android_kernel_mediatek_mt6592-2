
 
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
#include "pub_core_debuglog.h"
#include "pub_core_vki.h"
#include "pub_core_vkiscnums.h"
#include "pub_core_libcbase.h"
#include "pub_core_libcassert.h"
#include "pub_core_syscall.h"
#include "pub_core_libcsignal.h"    



Int VG_(sigfillset)( vki_sigset_t* set )
{
   Int i;
   if (set == NULL)
      return -1;
   for (i = 0; i < _VKI_NSIG_WORDS; i++)
      set->sig[i] = ~0;
   return 0;
}

Int VG_(sigemptyset)( vki_sigset_t* set )
{
   Int i;
   if (set == NULL)
      return -1;
   for (i = 0; i < _VKI_NSIG_WORDS; i++)
      set->sig[i] = 0;
   return 0;
}

Bool VG_(isemptysigset)( const vki_sigset_t* set )
{
   Int i;
   vg_assert(set != NULL);
   for (i = 0; i < _VKI_NSIG_WORDS; i++)
      if (set->sig[i] != 0) return False;
   return True;
}

Bool VG_(isfullsigset)( const vki_sigset_t* set )
{
   Int i;
   vg_assert(set != NULL);
   for (i = 0; i < _VKI_NSIG_WORDS; i++)
      if (set->sig[i] != ~0) return False;
   return True;
}

Bool VG_(iseqsigset)( const vki_sigset_t* set1, const vki_sigset_t* set2 )
{
   Int i;
   vg_assert(set1 != NULL && set2 != NULL);
   for (i = 0; i < _VKI_NSIG_WORDS; i++)
      if (set1->sig[i] != set2->sig[i]) return False;
   return True;
}


Int VG_(sigaddset)( vki_sigset_t* set, Int signum )
{
   if (set == NULL)
      return -1;
   if (signum < 1 || signum > _VKI_NSIG)
      return -1;
   signum--;
   set->sig[signum / _VKI_NSIG_BPW] |= (1ULL << (signum % _VKI_NSIG_BPW));
   return 0;
}

Int VG_(sigdelset)( vki_sigset_t* set, Int signum )
{
   if (set == NULL)
      return -1;
   if (signum < 1 || signum > _VKI_NSIG)
      return -1;
   signum--;
   set->sig[signum / _VKI_NSIG_BPW] &= ~(1ULL << (signum % _VKI_NSIG_BPW));
   return 0;
}

Int VG_(sigismember) ( const vki_sigset_t* set, Int signum )
{
   if (set == NULL)
      return 0;
   if (signum < 1 || signum > _VKI_NSIG)
      return 0;
   signum--;
   if (1 & ((set->sig[signum / _VKI_NSIG_BPW]) >> (signum % _VKI_NSIG_BPW)))
      return 1;
   else
      return 0;
}

void VG_(sigaddset_from_set)( vki_sigset_t* dst, const vki_sigset_t* src )
{
   Int i;
   vg_assert(dst != NULL && src != NULL);
   for (i = 0; i < _VKI_NSIG_WORDS; i++)
      dst->sig[i] |= src->sig[i];
}

void VG_(sigdelset_from_set)( vki_sigset_t* dst, const vki_sigset_t* src )
{
   Int i;
   vg_assert(dst != NULL && src != NULL);
   for (i = 0; i < _VKI_NSIG_WORDS; i++)
      dst->sig[i] &= ~(src->sig[i]);
}

void VG_(sigintersectset)( vki_sigset_t* dst, const vki_sigset_t* src )
{
   Int i;
   vg_assert(dst != NULL && src != NULL);
   for (i = 0; i < _VKI_NSIG_WORDS; i++)
      dst->sig[i] &= src->sig[i];
}

void VG_(sigcomplementset)( vki_sigset_t* dst, const vki_sigset_t* src )
{
   Int i;
   vg_assert(dst != NULL && src != NULL);
   for (i = 0; i < _VKI_NSIG_WORDS; i++)
      dst->sig[i] = ~ src->sig[i];
}


Int VG_(sigprocmask)( Int how, const vki_sigset_t* set, vki_sigset_t* oldset)
{
#  if defined(VGO_linux)
#  if defined(__NR_rt_sigprocmask)
   SysRes res = VG_(do_syscall4)(__NR_rt_sigprocmask, 
                                 how, (UWord)set, (UWord)oldset, 
                                 _VKI_NSIG_WORDS * sizeof(UWord));
#  else
   SysRes res = VG_(do_syscall3)(__NR_sigprocmask, 
                                 how, (UWord)set, (UWord)oldset);
#  endif

#  elif defined(VGO_darwin)
   SysRes res =  VG_(do_syscall3)(__NR___pthread_sigmask, 
                                  how, (UWord)set, (UWord)oldset);
#  else
#    error "Unknown OS"
#  endif
   return sr_isError(res) ? -1 : 0;
}


#if defined(VGO_darwin)
static 
void darwin_signal_demux(void* a1, UWord a2, UWord a3, void* a4, void* a5) {
   VG_(debugLog)(2, "libcsignal",
                    "PRE  demux sig, a2 = %lu, signo = %lu\n", a2, a3);
   if (a2 == 1)
      ((void(*)(int))a1) (a3);
   else
      ((void(*)(int,void*,void*))a1) (a3,a4,a5);
   VG_(debugLog)(2, "libcsignal",
                    "POST demux sig, a2 = %lu, signo = %lu\n", a2, a3);
   VG_(do_syscall2)(__NR_sigreturn, (UWord)a5, 0x1E);
   
   __asm__ __volatile__("ud2");
}
#endif

Int VG_(sigaction) ( Int signum, 
                     const vki_sigaction_toK_t* act,  
                     vki_sigaction_fromK_t* oldact)
{
#  if defined(VGO_linux)
   SysRes res = VG_(do_syscall4)(__NR_rt_sigaction,
                                 signum, (UWord)act, (UWord)oldact, 
                                 _VKI_NSIG_WORDS * sizeof(UWord));
   return sr_isError(res) ? -1 : 0;

#  elif defined(VGO_darwin)
   SysRes res;

   vki_sigaction_toK_t actCopy;
   struct {
     ULong before[2];
     vki_sigaction_fromK_t oa;
     ULong after[2];
   }
   oldactCopy;

   vki_sigaction_toK_t*   real_act;
   vki_sigaction_fromK_t* real_oldact;

   real_act    = act    ? &actCopy       : NULL;
   real_oldact = oldact ? &oldactCopy.oa : NULL;
   VG_(memset)(&oldactCopy, 0x55, sizeof(oldactCopy));
   if (real_act) {
      *real_act = *act;
      real_act->sa_tramp = (void*)&darwin_signal_demux;
   }
   res = VG_(do_syscall3)(__NR_sigaction, 
                          signum, (UWord)real_act, (UWord)real_oldact);
   if (real_oldact) {
      vg_assert(oldactCopy.before[0] == 0x5555555555555555ULL);
      vg_assert(oldactCopy.before[1] == 0x5555555555555555ULL);
      vg_assert(oldactCopy.after[0]  == 0x5555555555555555ULL);
      vg_assert(oldactCopy.after[1]  == 0x5555555555555555ULL);
      *oldact = *real_oldact;
   }
   return sr_isError(res) ? -1 : 0;

#  else
#    error "Unsupported OS"
#  endif
}


void 
VG_(convert_sigaction_fromK_to_toK)( const vki_sigaction_fromK_t* fromK,
                                     vki_sigaction_toK_t* toK )
{
#  if defined(VGO_linux)
   *toK = *fromK;
#  elif defined(VGO_darwin)
   toK->ksa_handler = fromK->ksa_handler;
   toK->sa_tramp    = NULL; 
   toK->sa_mask     = fromK->sa_mask;
   toK->sa_flags    = fromK->sa_flags;
#  else
#    error "Unsupported OS"
#  endif
}


Int VG_(kill)( Int pid, Int signo )
{
#  if defined(VGO_linux)
   SysRes res = VG_(do_syscall2)(__NR_kill, pid, signo);
#  elif defined(VGO_darwin)
   SysRes res = VG_(do_syscall3)(__NR_kill,
                                 pid, signo, 1);
#  else
#    error "Unsupported OS"
#  endif
   return sr_isError(res) ? -1 : 0;
}

Int VG_(tkill)( Int lwpid, Int signo )
{
#  if defined(__NR_tkill)
   SysRes res = VG_(mk_SysRes_Error)(VKI_ENOSYS);
   res = VG_(do_syscall2)(__NR_tkill, lwpid, signo);
   if (sr_isError(res) && sr_Err(res) == VKI_ENOSYS)
      res = VG_(do_syscall2)(__NR_kill, lwpid, signo);
   return sr_isError(res) ? -1 : 0;

#  elif defined(VGO_darwin)
   
   SysRes res;
   res = VG_(do_syscall2)(__NR___pthread_kill, lwpid, signo);
   return sr_isError(res) ? -1 : 0;

#  else
#    error "Unsupported plat"
#  endif
}




#if defined(VGO_linux)
Int VG_(sigtimedwait_zero)( const vki_sigset_t *set, 
                            vki_siginfo_t *info )
{
   static const struct vki_timespec zero = { 0, 0 };
   SysRes res = VG_(do_syscall4)(__NR_rt_sigtimedwait, (UWord)set, (UWord)info, 
                                 (UWord)&zero, sizeof(*set));
   return sr_isError(res) ? -1 : sr_Res(res);
}


#elif defined(VGO_darwin)



static void sigtimedwait_zero_handler ( Int sig ) 
{
   vg_assert(sig != VKI_SIGILL);
   vg_assert(sig != VKI_SIGSEGV);
   vg_assert(sig != VKI_SIGBUS);
   vg_assert(sig != VKI_SIGTRAP);
    
}

Int VG_(sigtimedwait_zero)( const vki_sigset_t *set, 
                            vki_siginfo_t *info )
{
  const Bool debug = False;
  Int    i, ir;
  SysRes sr;
  vki_sigset_t pending, blocked, allbutone;
  vki_sigaction_toK_t   sa, saved_sa2;
  vki_sigaction_fromK_t saved_sa;

  

  
  sr = VG_(do_syscall1)(__NR_sigpending, (UWord)&pending);
  vg_assert(!sr_isError(sr));

  
  
  VG_(sigintersectset)(&pending, (vki_sigset_t*)set);

  
  ir = VG_(sigprocmask)(VKI_SIG_SETMASK, NULL, &blocked);
  vg_assert(ir == 0);

  
  VG_(sigintersectset)(&pending, &blocked);

  
  for (i = 1; i < _VKI_NSIG; i++)
     if (VG_(sigismember)(&pending,i))
        break;

  if (i == _VKI_NSIG)
     return 0;

  if (debug)
     VG_(debugLog)(0, "libcsignal",
                      "sigtimedwait_zero: snarfing signal %d\n", i );

  
  VG_(sigfillset)(&sa.sa_mask);
  sa.ksa_handler = &sigtimedwait_zero_handler;
  sa.sa_flags    = 0;
  ir = VG_(sigaction)(i, &sa, &saved_sa);
  vg_assert(ir == 0);

  VG_(sigfillset)(&allbutone);
  VG_(sigdelset)(&allbutone, i);
  
  vg_assert(_VKI_NSIG_WORDS == 1);
  sr = VG_(do_syscall3)(__NR_sigsuspend_nocancel,
                        (UWord)allbutone.sig[0], 0,0);
  if (debug)
     VG_(debugLog)(0, "libcsignal",
                      "sigtimedwait_zero: sigsuspend got "
                      "res: %s %#lx\n", 
                      sr_isError(sr) ? "FAIL" : "SUCCESS",
                      sr_isError(sr) ? sr_Err(sr) : sr_Res(sr));
  vg_assert(sr_isError(sr));
  vg_assert(sr_Err(sr) == VKI_EINTR);

  
  VG_(convert_sigaction_fromK_to_toK)( &saved_sa, &saved_sa2 );
  ir = VG_(sigaction)(i, &saved_sa2, NULL);
  vg_assert(ir == 0);

  
  VG_(memset)( info, 0, sizeof(*info) );
  info->si_signo = i;

  return i;
}

#else
#  error "Unknown OS"
#endif

