

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


#if defined(VGP_s390x_linux)

#include "pub_core_basics.h"
#include "pub_core_vki.h"
#include "pub_core_vkiscnums.h"
#include "pub_core_threadstate.h"
#include "pub_core_aspacemgr.h"
#include "pub_core_debuglog.h"
#include "pub_core_libcbase.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcprint.h"
#include "pub_core_libcproc.h"
#include "pub_core_libcsignal.h"
#include "pub_core_mallocfree.h"
#include "pub_core_options.h"
#include "pub_core_scheduler.h"
#include "pub_core_sigframe.h"      
#include "pub_core_signals.h"
#include "pub_core_syscall.h"
#include "pub_core_syswrap.h"
#include "pub_core_tooliface.h"

#include "priv_types_n_macros.h"
#include "priv_syswrap-generic.h"    
#include "priv_syswrap-linux.h"      
#include "priv_syswrap-linux-variants.h" 
#include "priv_syswrap-main.h"



__attribute__((noreturn))
void ML_(call_on_new_stack_0_1) ( Addr stack,
                                  Addr retaddr,
                                  void (*f)(Word),
                                  Word arg1 );
asm(
    ".text\n"
    ".align 4\n"
    ".globl vgModuleLocal_call_on_new_stack_0_1\n"
    ".type vgModuleLocal_call_on_new_stack_0_1, @function\n"
    "vgModuleLocal_call_on_new_stack_0_1:\n"
    "   lgr %r15,%r2\n"     
    "   lgr %r14,%r3\n"     
    "   lgr %r2,%r5\n"      
    
    "   lghi  %r0,0\n"
    "   lghi  %r1,0\n"
    
    "   lghi  %r3,0\n"
    
    "   lghi  %r5,0\n"
    "   lghi  %r6,0\n"
    "   lghi  %r7,0\n"
    "   lghi  %r8,0\n"
    "   lghi  %r9,0\n"
    "   lghi  %r10,0\n"
    "   lghi  %r11,0\n"
    "   lghi  %r12,0\n"
    "   lghi  %r13,0\n"
    
    
    "   br  %r4\n"          
    ".previous\n"
    );

#define __NR_CLONE        VG_STRINGIFY(__NR_clone)
#define __NR_EXIT         VG_STRINGIFY(__NR_exit)

extern
ULong do_syscall_clone_s390x_linux ( void  *stack,
                                     ULong flags,
                                     Int   *parent_tid,
                                     Int   *child_tid,
                                     Addr  tlsaddr,
                                     Word (*fn)(void *),
                                     void  *arg);
asm(
   "   .text\n"
   "   .align  4\n"
   ".globl do_syscall_clone_s390x_linux\n"
   "do_syscall_clone_s390x_linux:\n"
   "   lg    %r1, 160(%r15)\n"   
   "   lg    %r0, 168(%r15)\n"   
   "   aghi  %r2, -160\n"        
   
   "   svc " __NR_CLONE"\n"        
   "   ltgr  %r2,%r2\n"           
   "   jne   1f\n"

   
   "   lgr   %r2, %r0\n"            
   "   basr  %r14,%r1\n"            

   
   "   svc " __NR_EXIT"\n"

   
   "   j +2\n"

   "1:\n"  
   "   br %r14\n"
   ".previous\n"
);

#undef __NR_CLONE
#undef __NR_EXIT

void VG_(cleanup_thread) ( ThreadArchState* arch )
{
  
}

static void setup_child (  ThreadArchState *child,
                     ThreadArchState *parent )
{
   
   child->vex = parent->vex;
   child->vex_shadow1 = parent->vex_shadow1;
   child->vex_shadow2 = parent->vex_shadow2;
}


static SysRes do_clone ( ThreadId ptid,
                         Addr sp, ULong flags,
                         Int *parent_tidptr,
                         Int *child_tidptr,
                         Addr tlsaddr)
{
   static const Bool debug = False;

   ThreadId     ctid = VG_(alloc_ThreadState)();
   ThreadState* ptst = VG_(get_ThreadState)(ptid);
   ThreadState* ctst = VG_(get_ThreadState)(ctid);
   UWord*       stack;
   SysRes       res;
   ULong        r2;
   vki_sigset_t blockall, savedmask;

   VG_(sigfillset)(&blockall);

   vg_assert(VG_(is_running_thread)(ptid));
   vg_assert(VG_(is_valid_tid)(ctid));

   stack = (UWord*)ML_(allocstack)(ctid);
   if (stack == NULL) {
      res = VG_(mk_SysRes_Error)( VKI_ENOMEM );
      goto out;
   }

   setup_child( &ctst->arch, &ptst->arch );

   ctst->arch.vex.guest_r2 = 0;

   if (sp != 0)
      ctst->arch.vex.guest_SP = sp;

   ctst->os_state.parent = ptid;

   
   ctst->sig_mask = ptst->sig_mask;
   ctst->tmp_sig_mask = ptst->sig_mask;

   
   ctst->os_state.threadgroup = ptst->os_state.threadgroup;

   ML_(guess_and_register_stack) (sp, ctst);

   vg_assert(VG_(owns_BigLock_LL)(ptid));
   VG_TRACK ( pre_thread_ll_create, ptid, ctid );

   if (flags & VKI_CLONE_SETTLS) {
      if (debug)
	 VG_(printf)("clone child has SETTLS: tls at %#lx\n", tlsaddr);
      ctst->arch.vex.guest_a0 = (UInt) (tlsaddr >> 32);
      ctst->arch.vex.guest_a1 = (UInt) tlsaddr;
   }
   flags &= ~VKI_CLONE_SETTLS;

   
   VG_(sigprocmask)(VKI_SIG_SETMASK, &blockall, &savedmask);

   
   r2 = do_syscall_clone_s390x_linux(
            stack, flags, parent_tidptr, child_tidptr, tlsaddr,
            ML_(start_thread_NORETURN), &VG_(threads)[ctid]);

   res = VG_(mk_SysRes_s390x_linux)( r2 );

   VG_(sigprocmask)(VKI_SIG_SETMASK, &savedmask, NULL);

  out:
   if (sr_isError(res)) {
      
      ctst->status = VgTs_Empty;
      
      VG_TRACK( pre_thread_ll_exit, ctid );
   }

   return res;

}




#define PRE(name)       DEFN_PRE_TEMPLATE(s390x_linux, name)
#define POST(name)      DEFN_POST_TEMPLATE(s390x_linux, name)


DECL_TEMPLATE(s390x_linux, sys_ptrace);
DECL_TEMPLATE(s390x_linux, sys_mmap);
DECL_TEMPLATE(s390x_linux, sys_clone);
DECL_TEMPLATE(s390x_linux, sys_sigreturn);
DECL_TEMPLATE(s390x_linux, sys_rt_sigreturn);
DECL_TEMPLATE(s390x_linux, sys_fadvise64);


PRE(sys_ptrace)
{
   PRINT("sys_ptrace ( %ld, %ld, %#lx, %#lx )", ARG1,ARG2,ARG3,ARG4);
   PRE_REG_READ4(int, "ptrace",
                 long, request, long, pid, long, addr, long, data);
   switch (ARG1) {
   case VKI_PTRACE_PEEKTEXT:
   case VKI_PTRACE_PEEKDATA:
   case VKI_PTRACE_PEEKUSR:
      PRE_MEM_WRITE( "ptrace(peek)", ARG4,
		     sizeof (long));
      break;
   case VKI_PTRACE_GETEVENTMSG:
      PRE_MEM_WRITE( "ptrace(geteventmsg)", ARG4, sizeof(unsigned long));
      break;
   case VKI_PTRACE_GETSIGINFO:
      PRE_MEM_WRITE( "ptrace(getsiginfo)", ARG4, sizeof(vki_siginfo_t));
      break;
   case VKI_PTRACE_SETSIGINFO:
      PRE_MEM_READ( "ptrace(setsiginfo)", ARG4, sizeof(vki_siginfo_t));
      break;
   case VKI_PTRACE_PEEKUSR_AREA:
      {
         vki_ptrace_area *pa;

         
	 pa = (vki_ptrace_area *) ARG3;
         PRE_MEM_READ("ptrace(peekusrarea ptrace_area->len)",
                      (unsigned long) &pa->vki_len, sizeof(pa->vki_len));
         PRE_MEM_READ("ptrace(peekusrarea ptrace_area->kernel_addr)",
                      (unsigned long) &pa->vki_kernel_addr, sizeof(pa->vki_kernel_addr));
         PRE_MEM_READ("ptrace(peekusrarea ptrace_area->process_addr)",
                      (unsigned long) &pa->vki_process_addr, sizeof(pa->vki_process_addr));
         PRE_MEM_WRITE("ptrace(peekusrarea *(ptrace_area->process_addr))",
                       pa->vki_process_addr, pa->vki_len);
         break;
      }
   case VKI_PTRACE_POKEUSR_AREA:
      {
         vki_ptrace_area *pa;

         
	 pa = (vki_ptrace_area *) ARG3;
         PRE_MEM_READ("ptrace(pokeusrarea ptrace_area->len)",
                      (unsigned long) &pa->vki_len, sizeof(pa->vki_len));
         PRE_MEM_READ("ptrace(pokeusrarea ptrace_area->kernel_addr)",
                      (unsigned long) &pa->vki_kernel_addr,
                      sizeof(pa->vki_kernel_addr));
         PRE_MEM_READ("ptrace(pokeusrarea ptrace_area->process_addr)",
                      (unsigned long) &pa->vki_process_addr,
                      sizeof(pa->vki_process_addr));
         PRE_MEM_READ("ptrace(pokeusrarea *(ptrace_area->process_addr))",
                       pa->vki_process_addr, pa->vki_len);
         break;
      }
   case VKI_PTRACE_GETREGSET:
      ML_(linux_PRE_getregset)(tid, ARG3, ARG4);
      break;
   case VKI_PTRACE_SETREGSET:
      ML_(linux_PRE_setregset)(tid, ARG3, ARG4);
      break;
   default:
      break;
   }
}

POST(sys_ptrace)
{
   switch (ARG1) {
   case VKI_PTRACE_PEEKTEXT:
   case VKI_PTRACE_PEEKDATA:
   case VKI_PTRACE_PEEKUSR:
      POST_MEM_WRITE( ARG4, sizeof (long));
      break;
   case VKI_PTRACE_GETEVENTMSG:
      POST_MEM_WRITE( ARG4, sizeof(unsigned long));
      break;
   case VKI_PTRACE_GETSIGINFO:
      POST_MEM_WRITE( ARG4, sizeof(vki_siginfo_t));
      break;
   case VKI_PTRACE_PEEKUSR_AREA:
      {
         vki_ptrace_area *pa;

	 pa = (vki_ptrace_area *) ARG3;
         POST_MEM_WRITE(pa->vki_process_addr, pa->vki_len);
	 break;
      }
   case VKI_PTRACE_GETREGSET:
      ML_(linux_POST_getregset)(tid, ARG3, ARG4);
      break;
   default:
      break;
   }
}

PRE(sys_mmap)
{
   UWord a0, a1, a2, a3, a4, a5;
   SysRes r;

   UWord* args = (UWord*)ARG1;
   PRE_REG_READ1(long, "sys_mmap", struct mmap_arg_struct *, args);
   PRE_MEM_READ( "sys_mmap(args)", (Addr) args, 6*sizeof(UWord) );

   a0 = args[0];
   a1 = args[1];
   a2 = args[2];
   a3 = args[3];
   a4 = args[4];
   a5 = args[5];

   PRINT("sys_mmap ( %#lx, %llu, %ld, %ld, %ld, %ld )",
         a0, (ULong)a1, a2, a3, a4, a5 );

   r = ML_(generic_PRE_sys_mmap)( tid, a0, a1, a2, a3, a4, (Off64T)a5 );
   SET_STATUS_from_SysRes(r);
}

PRE(sys_clone)
{
   UInt cloneflags;

   PRINT("sys_clone ( %lx, %#lx, %#lx, %#lx, %#lx )",ARG1,ARG2,ARG3,ARG4, ARG5);
   PRE_REG_READ2(int, "clone",
                 void *,        child_stack,
                 unsigned long, flags);

   if (ARG2 & VKI_CLONE_PARENT_SETTID) {
      if (VG_(tdict).track_pre_reg_read)
         PRA3("clone(parent_tidptr)", int *, parent_tidptr);
      PRE_MEM_WRITE("clone(parent_tidptr)", ARG3, sizeof(Int));
      if (!VG_(am_is_valid_for_client)(ARG3, sizeof(Int),
                                             VKI_PROT_WRITE)) {
         SET_STATUS_Failure( VKI_EFAULT );
         return;
      }
   }
   if (ARG2 & (VKI_CLONE_CHILD_SETTID | VKI_CLONE_CHILD_CLEARTID)) {
      if (VG_(tdict).track_pre_reg_read)
         PRA4("clone(child_tidptr)", int *, child_tidptr);
      PRE_MEM_WRITE("clone(child_tidptr)", ARG4, sizeof(Int));
      if (!VG_(am_is_valid_for_client)(ARG4, sizeof(Int),
                                             VKI_PROT_WRITE)) {
         SET_STATUS_Failure( VKI_EFAULT );
         return;
      }
   }

   
   if (ARG2 & VKI_CLONE_SETTLS) {
      if (VG_(tdict).track_pre_reg_read) {
         PRA5("clone", Addr, tlsinfo);
      }
   }

   cloneflags = ARG2;

   if (!ML_(client_signal_OK)(ARG2 & VKI_CSIGNAL)) {
      SET_STATUS_Failure( VKI_EINVAL );
      return;
   }

   
   switch (cloneflags & (VKI_CLONE_VM | VKI_CLONE_FS
                         | VKI_CLONE_FILES | VKI_CLONE_VFORK)) {
   case VKI_CLONE_VM | VKI_CLONE_FS | VKI_CLONE_FILES:
      
      SET_STATUS_from_SysRes(
         do_clone(tid,
                  (Addr)ARG1,   
                  ARG2,         
                  (Int *)ARG3,  
                  (Int *)ARG4, 
                  (Addr)ARG5)); 
      break;

   case VKI_CLONE_VFORK | VKI_CLONE_VM: 
      
      cloneflags &= ~(VKI_CLONE_VFORK | VKI_CLONE_VM);

   case 0: 
      SET_STATUS_from_SysRes(
         ML_(do_fork_clone)(tid,
                       cloneflags,      
                       (Int *)ARG3,     
                       (Int *)ARG4));   
      break;

   default:
      
      VG_(message)(Vg_UserMsg, "Unsupported clone() flags: 0x%lx\n", ARG2);
      VG_(message)(Vg_UserMsg, "\n");
      VG_(message)(Vg_UserMsg, "The only supported clone() uses are:\n");
      VG_(message)(Vg_UserMsg, " - via a threads library (NPTL)\n");
      VG_(message)(Vg_UserMsg, " - via the implementation of fork or vfork\n");
      VG_(unimplemented)
         ("Valgrind does not support general clone().");
   }

   if (SUCCESS) {
      if (ARG2 & VKI_CLONE_PARENT_SETTID)
         POST_MEM_WRITE(ARG3, sizeof(Int));
      if (ARG2 & (VKI_CLONE_CHILD_SETTID | VKI_CLONE_CHILD_CLEARTID))
         POST_MEM_WRITE(ARG4, sizeof(Int));

      *flags |= SfYieldAfter;
   }
}

PRE(sys_sigreturn)
{
   ThreadState* tst;
   PRINT("sys_sigreturn ( )");

   vg_assert(VG_(is_valid_tid)(tid));
   vg_assert(tid >= 1 && tid < VG_N_THREADS);
   vg_assert(VG_(is_running_thread)(tid));

   tst = VG_(get_ThreadState)(tid);

   ML_(fixup_guest_state_to_restart_syscall)(&tst->arch);

   
   VG_(sigframe_destroy)(tid, False);

   *flags |= SfNoWriteResult;
   SET_STATUS_Success(0);

   
   *flags |= SfPollAfter;
}


PRE(sys_rt_sigreturn)
{

   ThreadState* tst;
   PRINT("sys_rt_sigreturn ( )");

   vg_assert(VG_(is_valid_tid)(tid));
   vg_assert(tid >= 1 && tid < VG_N_THREADS);
   vg_assert(VG_(is_running_thread)(tid));

   tst = VG_(get_ThreadState)(tid);

   ML_(fixup_guest_state_to_restart_syscall)(&tst->arch);

   
   VG_(sigframe_destroy)(tid, True);

   *flags |= SfNoWriteResult;
   SET_STATUS_Success(0);

   
   *flags |= SfPollAfter;
}

PRE(sys_fadvise64)
{
   PRINT("sys_fadvise64 ( %ld, %ld, %ld, %ld )", ARG1,ARG2,ARG3,ARG4);
   PRE_REG_READ4(long, "fadvise64",
                 int, fd, vki_loff_t, offset, vki_loff_t, len, int, advice);
}

#undef PRE
#undef POST


#define PLAX_(sysno, name)    WRAPPER_ENTRY_X_(s390x_linux, sysno, name)
#define PLAXY(sysno, name)    WRAPPER_ENTRY_XY(s390x_linux, sysno, name)


static SyscallTableEntry syscall_table[] = {
   GENX_(0, sys_ni_syscall),       
   GENX_(__NR_exit,  sys_exit),                                       
   GENX_(__NR_fork,  sys_fork),                                       
   GENXY(__NR_read,  sys_read),                                       
   GENX_(__NR_write,  sys_write),                                     

   GENXY(__NR_open,  sys_open),                                       
   GENXY(__NR_close,  sys_close),                                     
   GENXY(__NR_creat,  sys_creat),                                     
   GENX_(__NR_link,  sys_link),                                       

   GENX_(__NR_unlink,  sys_unlink),                                   
   GENX_(__NR_execve,  sys_execve),                                   
   GENX_(__NR_chdir,  sys_chdir),                                     
   GENX_(13, sys_ni_syscall),      
   GENX_(__NR_mknod,  sys_mknod),                                     

   GENX_(__NR_chmod,  sys_chmod),                                     
   GENX_(16, sys_ni_syscall),      
   GENX_(17, sys_ni_syscall),      
   GENX_(18, sys_ni_syscall),      
   LINX_(__NR_lseek,  sys_lseek),                                     

   GENX_(__NR_getpid,  sys_getpid),                                   
   LINX_(__NR_mount,  sys_mount),                                     
   LINX_(__NR_umount, sys_oldumount),                                 
   GENX_(23, sys_ni_syscall),      
   GENX_(24, sys_ni_syscall),      

   GENX_(25, sys_ni_syscall),      
   PLAXY(__NR_ptrace, sys_ptrace),                                    
   GENX_(__NR_alarm,  sys_alarm),                                     
   GENX_(28, sys_ni_syscall),      
   GENX_(__NR_pause,  sys_pause),                                     

   LINX_(__NR_utime,  sys_utime),                                     
   GENX_(31, sys_ni_syscall),      
   GENX_(32, sys_ni_syscall),      
   GENX_(__NR_access,  sys_access),                                   
   GENX_(__NR_nice, sys_nice),                                        

   GENX_(35, sys_ni_syscall),      
   GENX_(__NR_sync, sys_sync),                                        
   GENX_(__NR_kill,  sys_kill),                                       
   GENX_(__NR_rename,  sys_rename),                                   
   GENX_(__NR_mkdir,  sys_mkdir),                                     

   GENX_(__NR_rmdir, sys_rmdir),                                      
   GENXY(__NR_dup,  sys_dup),                                         
   LINXY(__NR_pipe,  sys_pipe),                                       
   GENXY(__NR_times,  sys_times),                                     
   GENX_(44, sys_ni_syscall),      

   GENX_(__NR_brk,  sys_brk),                                         
   GENX_(46, sys_ni_syscall),      
   GENX_(47, sys_ni_syscall),      
   GENX_(49, sys_ni_syscall),      

   GENX_(50, sys_ni_syscall),      
   GENX_(__NR_acct, sys_acct),                                        
   LINX_(__NR_umount2, sys_umount),                                   
   GENX_(53, sys_ni_syscall),      
   LINXY(__NR_ioctl,  sys_ioctl),                                     

   LINXY(__NR_fcntl,  sys_fcntl),                                     
   GENX_(56, sys_ni_syscall),      
   GENX_(__NR_setpgid,  sys_setpgid),                                 
   GENX_(58, sys_ni_syscall),      
   GENX_(59, sys_ni_syscall),      

   GENX_(__NR_umask,  sys_umask),                                     
   GENX_(__NR_chroot,  sys_chroot),                                   
   GENXY(__NR_dup2,  sys_dup2),                                       
   GENX_(__NR_getppid,  sys_getppid),                                 

   GENX_(__NR_getpgrp,  sys_getpgrp),                                 
   GENX_(__NR_setsid,  sys_setsid),                                   
   GENX_(68, sys_ni_syscall),      
   GENX_(69, sys_ni_syscall),      

   GENX_(70, sys_ni_syscall),      
   GENX_(71, sys_ni_syscall),      

   GENX_(__NR_setrlimit,  sys_setrlimit),                             
   GENXY(76,  sys_getrlimit),                       
   GENXY(__NR_getrusage,  sys_getrusage),                             
   GENXY(__NR_gettimeofday,  sys_gettimeofday),                       
   GENX_(__NR_settimeofday, sys_settimeofday),                        

   GENX_(80, sys_ni_syscall),      
   GENX_(81, sys_ni_syscall),      
   GENX_(82, sys_ni_syscall),      
   GENX_(__NR_symlink,  sys_symlink),                                 
   GENX_(84, sys_ni_syscall),      

   GENX_(__NR_readlink,  sys_readlink),                               
   GENX_(89, sys_ni_syscall),      

   PLAX_(__NR_mmap, sys_mmap ),                                       
   GENXY(__NR_munmap,  sys_munmap),                                   
   GENX_(__NR_truncate,  sys_truncate),                               
   GENX_(__NR_ftruncate,  sys_ftruncate),                             
   GENX_(__NR_fchmod,  sys_fchmod),                                   

   GENX_(95, sys_ni_syscall),      
   GENX_(__NR_getpriority, sys_getpriority),                          
   GENX_(__NR_setpriority, sys_setpriority),                          
   GENX_(98, sys_ni_syscall),      
   GENXY(__NR_statfs,  sys_statfs),                                   

   GENXY(__NR_fstatfs,  sys_fstatfs),                                 
   GENX_(101, sys_ni_syscall),     
   LINXY(__NR_socketcall, sys_socketcall),                            
   LINXY(__NR_syslog,  sys_syslog),                                   
   GENXY(__NR_setitimer,  sys_setitimer),                             

   GENXY(__NR_getitimer,  sys_getitimer),                             
   GENXY(__NR_stat, sys_newstat),                                     
   GENXY(__NR_lstat, sys_newlstat),                                   
   GENXY(__NR_fstat, sys_newfstat),                                   
   GENX_(109, sys_ni_syscall),     

   LINXY(__NR_lookup_dcookie, sys_lookup_dcookie),                    
   LINX_(__NR_vhangup, sys_vhangup),                                  
   GENX_(112, sys_ni_syscall),     
   GENX_(113, sys_ni_syscall),     
   GENXY(__NR_wait4,  sys_wait4),                                     

   LINXY(__NR_sysinfo,  sys_sysinfo),                                 
   LINXY(__NR_ipc, sys_ipc),                                          
   GENX_(__NR_fsync,  sys_fsync),                                     
   PLAX_(__NR_sigreturn, sys_sigreturn),                              

   PLAX_(__NR_clone,  sys_clone),                                     
   GENXY(__NR_uname, sys_newuname),                                   
   GENX_(123, sys_ni_syscall),     

   GENXY(__NR_mprotect,  sys_mprotect),                               
   GENX_(127, sys_ni_syscall),     
   LINX_(__NR_init_module,  sys_init_module),                         
   LINX_(__NR_delete_module,  sys_delete_module),                     

   GENX_(130, sys_ni_syscall),     
   LINX_(__NR_quotactl, sys_quotactl),                                
   GENX_(__NR_getpgid,  sys_getpgid),                                 
   GENX_(__NR_fchdir,  sys_fchdir),                                   

   LINX_(__NR_personality, sys_personality),                          
   GENX_(137, sys_ni_syscall),     
   GENX_(138, sys_ni_syscall),     
   GENX_(139, sys_ni_syscall),     

   GENXY(__NR_getdents,  sys_getdents),                               
   GENX_(__NR_select, sys_select),                                    
   GENX_(__NR_flock,  sys_flock),                                     
   GENX_(__NR_msync,  sys_msync),                                     

   GENXY(__NR_readv,  sys_readv),                                     
   GENX_(__NR_writev,  sys_writev),                                   
   GENX_(__NR_getsid, sys_getsid),                                    
   GENX_(__NR_fdatasync,  sys_fdatasync),                             
   LINXY(__NR__sysctl, sys_sysctl),                                   

   GENX_(__NR_mlock,  sys_mlock),                                     
   GENX_(__NR_munlock,  sys_munlock),                                 
   GENX_(__NR_mlockall,  sys_mlockall),                               
   LINX_(__NR_munlockall,  sys_munlockall),                           
   LINXY(__NR_sched_setparam,  sys_sched_setparam),                   

   LINXY(__NR_sched_getparam,  sys_sched_getparam),                   
   LINX_(__NR_sched_setscheduler,  sys_sched_setscheduler),           
   LINX_(__NR_sched_getscheduler,  sys_sched_getscheduler),           
   LINX_(__NR_sched_yield,  sys_sched_yield),                         
   LINX_(__NR_sched_get_priority_max,  sys_sched_get_priority_max),   

   LINX_(__NR_sched_get_priority_min,  sys_sched_get_priority_min),   
   LINXY(__NR_sched_rr_get_interval, sys_sched_rr_get_interval),      
   GENXY(__NR_nanosleep,  sys_nanosleep),                             
   GENX_(__NR_mremap,  sys_mremap),                                   
   GENX_(164, sys_ni_syscall),     

   GENX_(165, sys_ni_syscall),     
   GENX_(166, sys_ni_syscall),     
   GENX_(167, sys_ni_syscall),     
   GENXY(__NR_poll,  sys_poll),                                       

   GENX_(170, sys_ni_syscall),     
   GENX_(171, sys_ni_syscall),     
   LINXY(__NR_prctl, sys_prctl),                                      
   PLAX_(__NR_rt_sigreturn,  sys_rt_sigreturn),                       
   LINXY(__NR_rt_sigaction,  sys_rt_sigaction),                       

   LINXY(__NR_rt_sigprocmask,  sys_rt_sigprocmask),                   
   LINXY(__NR_rt_sigpending, sys_rt_sigpending),                      
   LINXY(__NR_rt_sigtimedwait,  sys_rt_sigtimedwait),                 
   LINXY(__NR_rt_sigqueueinfo, sys_rt_sigqueueinfo),                  
   LINX_(__NR_rt_sigsuspend, sys_rt_sigsuspend),                      

   GENXY(__NR_pread64,  sys_pread64),                                 
   GENX_(__NR_pwrite64, sys_pwrite64),                                
   GENX_(182, sys_ni_syscall),     
   GENXY(__NR_getcwd,  sys_getcwd),                                   
   LINXY(__NR_capget,  sys_capget),                                   

   LINX_(__NR_capset,  sys_capset),                                   
   GENXY(__NR_sigaltstack,  sys_sigaltstack),                         
   LINXY(__NR_sendfile, sys_sendfile),                                
   GENX_(188, sys_ni_syscall),     
   GENX_(189, sys_ni_syscall),     

   GENX_(__NR_vfork,  sys_fork),                                      
   GENXY(__NR_getrlimit,  sys_getrlimit),                             
   GENX_(192, sys_ni_syscall),              
   GENX_(193, sys_ni_syscall),     
   GENX_(194, sys_ni_syscall),     

   GENX_(195, sys_ni_syscall),     
   GENX_(196, sys_ni_syscall),     
   GENX_(197, sys_ni_syscall),     
   GENX_(__NR_lchown, sys_lchown),                                    
   GENX_(__NR_getuid, sys_getuid),                                    

   GENX_(__NR_getgid, sys_getgid),                                    
   GENX_(__NR_geteuid, sys_geteuid),                                  
   GENX_(__NR_getegid, sys_getegid),                                  
   GENX_(__NR_setreuid, sys_setreuid),                                
   GENX_(__NR_setregid, sys_setregid),                                

   GENXY(__NR_getgroups, sys_getgroups),                              
   GENX_(__NR_setgroups, sys_setgroups),                              
   GENX_(__NR_fchown, sys_fchown),                                    
   LINX_(__NR_setresuid, sys_setresuid),                              
   LINXY(__NR_getresuid, sys_getresuid),                              

   LINX_(__NR_setresgid, sys_setresgid),                              
   LINXY(__NR_getresgid, sys_getresgid),                              
   GENX_(__NR_chown, sys_chown),                                      
   GENX_(__NR_setuid, sys_setuid),                                    
   GENX_(__NR_setgid, sys_setgid),                                    

   LINX_(__NR_setfsuid, sys_setfsuid),                                
   LINX_(__NR_setfsgid, sys_setfsgid),                                
   LINX_(__NR_pivot_root, sys_pivot_root),                            
   GENXY(__NR_mincore, sys_mincore),                                  
   GENX_(__NR_madvise,  sys_madvise),                                 

   GENXY(__NR_getdents64,  sys_getdents64),                           
   GENX_(221, sys_ni_syscall),     
   LINX_(__NR_readahead, sys_readahead),                              
   GENX_(223, sys_ni_syscall),     
   LINX_(__NR_setxattr, sys_setxattr),                                

   LINX_(__NR_lsetxattr, sys_lsetxattr),                              
   LINX_(__NR_fsetxattr, sys_fsetxattr),                              
   LINXY(__NR_getxattr,  sys_getxattr),                               
   LINXY(__NR_lgetxattr,  sys_lgetxattr),                             
   LINXY(__NR_fgetxattr,  sys_fgetxattr),                             

   LINXY(__NR_listxattr,  sys_listxattr),                             
   LINXY(__NR_llistxattr,  sys_llistxattr),                           
   LINXY(__NR_flistxattr,  sys_flistxattr),                           
   LINX_(__NR_removexattr,  sys_removexattr),                         
   LINX_(__NR_lremovexattr,  sys_lremovexattr),                       

   LINX_(__NR_fremovexattr,  sys_fremovexattr),                       
   LINX_(__NR_gettid,  sys_gettid),                                   
   LINXY(__NR_tkill, sys_tkill),                                      
   LINXY(__NR_futex,  sys_futex),                                     
   LINX_(__NR_sched_setaffinity,  sys_sched_setaffinity),             

   LINXY(__NR_sched_getaffinity,  sys_sched_getaffinity),             
   LINXY(__NR_tgkill, sys_tgkill),                                    
   GENX_(242, sys_ni_syscall),     
   LINXY(__NR_io_setup, sys_io_setup),                                
   LINX_(__NR_io_destroy,  sys_io_destroy),                           

   LINXY(__NR_io_getevents,  sys_io_getevents),                       
   LINX_(__NR_io_submit,  sys_io_submit),                             
   LINXY(__NR_io_cancel,  sys_io_cancel),                             
   LINX_(__NR_exit_group,  sys_exit_group),                           
   LINXY(__NR_epoll_create,  sys_epoll_create),                       

   LINX_(__NR_epoll_ctl,  sys_epoll_ctl),                             
   LINXY(__NR_epoll_wait,  sys_epoll_wait),                           
   LINX_(__NR_set_tid_address,  sys_set_tid_address),                 
   PLAX_(__NR_fadvise64, sys_fadvise64),                              
   LINXY(__NR_timer_create,  sys_timer_create),                       

   LINXY(__NR_timer_settime,  sys_timer_settime),                     
   LINXY(__NR_timer_gettime,  sys_timer_gettime),                     
   LINX_(__NR_timer_getoverrun,  sys_timer_getoverrun),               
   LINX_(__NR_timer_delete,  sys_timer_delete),                       
   LINX_(__NR_clock_settime,  sys_clock_settime),                     

   LINXY(__NR_clock_gettime,  sys_clock_gettime),                     
   LINXY(__NR_clock_getres,  sys_clock_getres),                       
   LINXY(__NR_clock_nanosleep,  sys_clock_nanosleep),                 
   GENX_(263, sys_ni_syscall),     
   GENX_(264, sys_ni_syscall),     

   GENXY(__NR_statfs64, sys_statfs64),                                
   GENXY(__NR_fstatfs64, sys_fstatfs64),                              
   GENX_(268, sys_ni_syscall),     
   GENX_(269, sys_ni_syscall),     

   GENX_(270, sys_ni_syscall),     
   LINXY(__NR_mq_open,  sys_mq_open),                                 
   LINX_(__NR_mq_unlink,  sys_mq_unlink),                             
   LINX_(__NR_mq_timedsend,  sys_mq_timedsend),                       
   LINXY(__NR_mq_timedreceive, sys_mq_timedreceive),                  

   LINX_(__NR_mq_notify,  sys_mq_notify),                             
   LINXY(__NR_mq_getsetattr,  sys_mq_getsetattr),                     
   LINX_(__NR_add_key,  sys_add_key),                                 
   LINX_(__NR_request_key,  sys_request_key),                         

   LINXY(__NR_keyctl,  sys_keyctl),                                   
   LINXY(__NR_waitid, sys_waitid),                                    
   LINX_(__NR_ioprio_set,  sys_ioprio_set),                           
   LINX_(__NR_ioprio_get,  sys_ioprio_get),                           
   LINX_(__NR_inotify_init,  sys_inotify_init),                       

   LINX_(__NR_inotify_add_watch,  sys_inotify_add_watch),             
   LINX_(__NR_inotify_rm_watch,  sys_inotify_rm_watch),               
   GENX_(287, sys_ni_syscall),     
   LINXY(__NR_openat,  sys_openat),                                   
   LINX_(__NR_mkdirat,  sys_mkdirat),                                 

   LINX_(__NR_mknodat,  sys_mknodat),                                 
   LINX_(__NR_fchownat,  sys_fchownat),                               
   LINX_(__NR_futimesat,  sys_futimesat),                             
   LINXY(__NR_newfstatat, sys_newfstatat),                            
   LINX_(__NR_unlinkat,  sys_unlinkat),                               

   LINX_(__NR_renameat,  sys_renameat),                               
   LINX_(__NR_linkat,  sys_linkat),                                   
   LINX_(__NR_symlinkat,  sys_symlinkat),                             
   LINX_(__NR_readlinkat,  sys_readlinkat),                           
   LINX_(__NR_fchmodat,  sys_fchmodat),                               

   LINX_(__NR_faccessat,  sys_faccessat),                             
   LINX_(__NR_pselect6, sys_pselect6),                                
   LINXY(__NR_ppoll, sys_ppoll),                                      
   LINX_(__NR_unshare, sys_unshare),                                  
   LINX_(__NR_set_robust_list,  sys_set_robust_list),                 

   LINXY(__NR_get_robust_list,  sys_get_robust_list),                 
   LINX_(__NR_splice, sys_splice),                                    
   LINX_(__NR_sync_file_range, sys_sync_file_range),                  
   LINX_(__NR_tee, sys_tee),                                          
   LINXY(__NR_vmsplice, sys_vmsplice),                                

   GENX_(310, sys_ni_syscall),     
   LINXY(__NR_getcpu, sys_getcpu),                                    
   LINXY(__NR_epoll_pwait,  sys_epoll_pwait),                         
   GENX_(__NR_utimes, sys_utimes),                                    
   LINX_(__NR_fallocate, sys_fallocate),                              

   LINX_(__NR_utimensat,  sys_utimensat),                             
   LINXY(__NR_signalfd,  sys_signalfd),                               
   GENX_(317, sys_ni_syscall),     
   LINXY(__NR_eventfd,  sys_eventfd),                                 
   LINXY(__NR_timerfd_create,  sys_timerfd_create),                   

   LINXY(__NR_timerfd_settime,  sys_timerfd_settime),                 
   LINXY(__NR_timerfd_gettime,  sys_timerfd_gettime),                 
   LINXY(__NR_signalfd4,  sys_signalfd4),                             
   LINXY(__NR_eventfd2,  sys_eventfd2),                               
   LINXY(__NR_inotify_init1,  sys_inotify_init1),                     

   LINXY(__NR_pipe2,  sys_pipe2),                                     
   LINXY(__NR_dup3,  sys_dup3),                                       
   LINXY(__NR_epoll_create1,  sys_epoll_create1),                     
   LINXY(__NR_preadv, sys_preadv),                                    
   LINX_(__NR_pwritev, sys_pwritev),                                  

   LINXY(__NR_rt_tgsigqueueinfo, sys_rt_tgsigqueueinfo),              
   LINXY(__NR_perf_event_open, sys_perf_event_open),                  
   LINXY(__NR_fanotify_init, sys_fanotify_init),                      
   LINX_(__NR_fanotify_mark, sys_fanotify_mark),                      
   LINXY(__NR_prlimit64, sys_prlimit64),                              

   LINXY(__NR_name_to_handle_at, sys_name_to_handle_at),              
   LINXY(__NR_open_by_handle_at, sys_open_by_handle_at),              
   LINXY(__NR_clock_adjtime, sys_clock_adjtime),                      
   LINX_(__NR_syncfs, sys_syncfs),                                    

   LINXY(__NR_process_vm_readv, sys_process_vm_readv),                
   LINX_(__NR_process_vm_writev, sys_process_vm_writev),              
   LINX_(__NR_kcmp, sys_kcmp),                                        

   LINXY(__NR_getrandom, sys_getrandom),                              

   LINXY(__NR_memfd_create, sys_memfd_create)                         
};

SyscallTableEntry* ML_(get_linux_syscall_entry) ( UInt sysno )
{
   const UInt syscall_table_size
      = sizeof(syscall_table) / sizeof(syscall_table[0]);

   
   if (sysno < syscall_table_size) {
      SyscallTableEntry* sys = &syscall_table[sysno];
      if (sys->before == NULL)
         return NULL; 
      else
         return sys;
   }

   
   return NULL;
}

#endif

