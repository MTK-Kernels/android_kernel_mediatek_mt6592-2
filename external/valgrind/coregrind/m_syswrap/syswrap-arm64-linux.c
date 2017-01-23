

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2013-2013 OpenWorks
      info@open-works.net

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

#if defined(VGP_arm64_linux)

#include "pub_core_basics.h"
#include "pub_core_vki.h"
#include "pub_core_vkiscnums.h"
#include "pub_core_threadstate.h"
#include "pub_core_aspacemgr.h"
#include "pub_core_libcbase.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcprint.h"
#include "pub_core_libcsignal.h"
#include "pub_core_options.h"
#include "pub_core_scheduler.h"
#include "pub_core_sigframe.h"      
#include "pub_core_syscall.h"
#include "pub_core_syswrap.h"
#include "pub_core_tooliface.h"

#include "priv_types_n_macros.h"
#include "priv_syswrap-generic.h"   
#include "priv_syswrap-linux.h"     



__attribute__((noreturn))
void ML_(call_on_new_stack_0_1) ( Addr stack,
                                  Addr retaddr,
                                  void (*f)(Word),
                                  Word arg1 );
asm(
".text\n"
".globl vgModuleLocal_call_on_new_stack_0_1\n"
"vgModuleLocal_call_on_new_stack_0_1:\n"
"   mov    sp, x0\n\t" 
"   mov    x30, x1\n\t" 
"   mov    x0, x3\n\t" 
"   mov    x9, x2\n\t" 
"   mov    x1, #0\n\t" 
"   mov    x2, #0\n\t"
"   mov    x3, #0\n\t"
"   mov    x4, #0\n\t"
"   mov    x5, #0\n\t"
"   mov    x6, #0\n\t"
"   mov    x7, #0\n\t"
"   mov    x8, #0\n\t"
"   mov    x10, #0\n\t"
"   mov    x11, #0\n\t"
"   mov    x12, #0\n\t"
"   mov    x13, #0\n\t"
"   mov    x14, #0\n\t"
"   mov    x15, #0\n\t"
"   mov    x16, #0\n\t"
"   mov    x17, #0\n\t"
"   mov    x18, #0\n\t"
"   mov    x19, #0\n\t"
"   mov    x20, #0\n\t"
"   mov    x21, #0\n\t"
"   mov    x22, #0\n\t"
"   mov    x23, #0\n\t"
"   mov    x24, #0\n\t"
"   mov    x25, #0\n\t"
"   mov    x26, #0\n\t"
"   mov    x27, #0\n\t"
"   mov    x28, #0\n\t"
"   mov    x29, sp\n\t" 
"   br     x9\n\t"
".previous\n"
);


#define __NR_CLONE        VG_STRINGIFY(__NR_clone)
#define __NR_EXIT         VG_STRINGIFY(__NR_exit)

extern
Long do_syscall_clone_arm64_linux ( Word (*fn)(void *), 
                                    void* child_stack, 
                                    Long  flags, 
                                    void* arg,
                                    Int*  child_tid,
                                    Int*  parent_tid,
                                    void* tls );
asm(
".text\n"
".globl do_syscall_clone_arm64_linux\n"
"do_syscall_clone_arm64_linux:\n"
        
"       sub    x1, x1, #16\n"       
"       str    x3, [x1, #8]\n"      
"       str    x0, [x1, #0]\n"      
        
        
"       mov    x8, #"__NR_CLONE"\n" 
"       mov    x0, x2\n"            
"       mov    x1, x1\n"            
"       mov    x2, x5\n"            
"       mov    x3, x6\n"            
"       mov    x4, x4\n"            

"       svc    0\n"                 

"       cmp    x0, #0\n"            
"       bne    1f\n"

        
"       ldr    x1, [sp, #0]\n"      
"       ldr    x0, [sp, #8]\n"      
"       add    sp, sp, #16\n"
"       blr    x1\n"                

        
"       mov    x0, x0\n"            
"       mov    x8, #"__NR_EXIT"\n"

"       svc    0\n"

        
"       .word 0xFFFFFFFF\n"

"1:\n"  
"       ret\n"
".previous\n"
);

#undef __NR_CLONE
#undef __NR_EXIT

static void setup_child ( ThreadArchState*, ThreadArchState* );
static void assign_guest_tls(ThreadId ctid, Addr tlsptr);
            
static SysRes do_clone ( ThreadId ptid, 
                         ULong flags,
                         Addr  child_xsp, 
                         Int*  parent_tidptr, 
                         Int*  child_tidptr, 
                         Addr  child_tls )
{
   ThreadId     ctid = VG_(alloc_ThreadState)();
   ThreadState* ptst = VG_(get_ThreadState)(ptid);
   ThreadState* ctst = VG_(get_ThreadState)(ctid);
   UWord*       stack;
   SysRes       res;
   ULong        x0;
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

   ctst->arch.vex.guest_X0 = 0;

   if (child_xsp != 0)
      ctst->arch.vex.guest_XSP = child_xsp;

   ctst->os_state.parent = ptid;

   
   ctst->sig_mask = ptst->sig_mask;
   ctst->tmp_sig_mask = ptst->sig_mask;

   ctst->os_state.threadgroup = ptst->os_state.threadgroup;

   ML_(guess_and_register_stack)(child_xsp, ctst);

   vg_assert(VG_(owns_BigLock_LL)(ptid));
   VG_TRACK ( pre_thread_ll_create, ptid, ctid );

   if (flags & VKI_CLONE_SETTLS) {
      
      assign_guest_tls(ctid, child_tls);
   }
    
   flags &= ~VKI_CLONE_SETTLS;

   
   VG_(sigprocmask)(VKI_SIG_SETMASK, &blockall, &savedmask);

   x0 = do_syscall_clone_arm64_linux(
      ML_(start_thread_NORETURN), stack, flags, &VG_(threads)[ctid],
      child_tidptr, parent_tidptr, NULL
   );
    
   res = VG_(mk_SysRes_arm64_linux)( x0 );

   VG_(sigprocmask)(VKI_SIG_SETMASK, &savedmask, NULL);

  out:
   if (sr_isError(res)) {
      
      VG_(cleanup_thread)(&ctst->arch);
      ctst->status = VgTs_Empty;
      
      VG_TRACK( pre_thread_ll_exit, ctid );
   }

   return res;
}



void VG_(cleanup_thread) ( ThreadArchState* arch )
{
}  

void setup_child (  ThreadArchState *child,
                     ThreadArchState *parent )
{
   child->vex = parent->vex;
   child->vex_shadow1 = parent->vex_shadow1;
   child->vex_shadow2 = parent->vex_shadow2;
}

static void assign_guest_tls(ThreadId tid, Addr tlsptr)
{
   VG_(threads)[tid].arch.vex.guest_TPIDR_EL0 = tlsptr;
}



#define PRE(name)       DEFN_PRE_TEMPLATE(arm64_linux, name)
#define POST(name)      DEFN_POST_TEMPLATE(arm64_linux, name)


DECL_TEMPLATE(arm64_linux, sys_fadvise64);
DECL_TEMPLATE(arm64_linux, sys_mmap);
DECL_TEMPLATE(arm64_linux, sys_clone);
DECL_TEMPLATE(arm64_linux, sys_rt_sigreturn);


PRE(sys_fadvise64)
{
   PRINT("sys_fadvise64 ( %ld, %ld, %lu, %ld )", ARG1,ARG2,ARG3,ARG4);
   PRE_REG_READ4(long, "fadvise64",
                 int, fd, vki_loff_t, offset, vki_size_t, len, int, advice);
}

PRE(sys_mmap)
{
   SysRes r;

   PRINT("sys_mmap ( %#lx, %llu, %ld, %ld, %d, %ld )",
         ARG1, (ULong)ARG2, ARG3, ARG4, (Int)ARG5, ARG6 );
   PRE_REG_READ6(long, "mmap",
                 unsigned long, start, unsigned long, length,
                 unsigned long, prot,  unsigned long, flags,
                 unsigned long, fd,    unsigned long, offset);

   r = ML_(generic_PRE_sys_mmap)( tid, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6 );
   SET_STATUS_from_SysRes(r);
}


PRE(sys_clone)
{
   UInt cloneflags;

   PRINT("sys_clone ( %lx, %#lx, %#lx, %#lx, %#lx )",ARG1,ARG2,ARG3,ARG4,ARG5);
   PRE_REG_READ5(int, "clone",
                 unsigned long, flags,
                 void *, child_stack,
                 int *, parent_tidptr,
                 void *, child_tls,
                 int *, child_tidptr);

   if (ARG1 & VKI_CLONE_PARENT_SETTID) {
      PRE_MEM_WRITE("clone(parent_tidptr)", ARG3, sizeof(Int));
      if (!VG_(am_is_valid_for_client)(ARG3, sizeof(Int), 
                                             VKI_PROT_WRITE)) {
         SET_STATUS_Failure( VKI_EFAULT );
         return;
      }
   }
   if (ARG1 & (VKI_CLONE_CHILD_SETTID | VKI_CLONE_CHILD_CLEARTID)) {
      PRE_MEM_WRITE("clone(child_tidptr)", ARG5, sizeof(Int));
      if (!VG_(am_is_valid_for_client)(ARG5, sizeof(Int), 
                                             VKI_PROT_WRITE)) {
         SET_STATUS_Failure( VKI_EFAULT );
         return;
      }
   }

   cloneflags = ARG1;

   if (!ML_(client_signal_OK)(ARG1 & VKI_CSIGNAL)) {
      SET_STATUS_Failure( VKI_EINVAL );
      return;
   }

   
   switch (cloneflags & (VKI_CLONE_VM | VKI_CLONE_FS 
                         | VKI_CLONE_FILES | VKI_CLONE_VFORK)) {
   case VKI_CLONE_VM | VKI_CLONE_FS | VKI_CLONE_FILES:
      
      SET_STATUS_from_SysRes(
         do_clone(tid,
                  ARG1,         
                  (Addr)ARG2,   
                  (Int*)ARG3,   
                  (Int*)ARG5,   
                  (Addr)ARG4)); 
      break;

   case VKI_CLONE_VFORK | VKI_CLONE_VM: 
      
      cloneflags &= ~(VKI_CLONE_VFORK | VKI_CLONE_VM);

   case 0: 
      SET_STATUS_from_SysRes(
         ML_(do_fork_clone)(tid,
                       cloneflags,     
                       (Int*)ARG3,     
                       (Int*)ARG5));   
      break;

   default:
      
      VG_(message)(Vg_UserMsg, "Unsupported clone() flags: 0x%lx\n", ARG1);
      VG_(message)(Vg_UserMsg, "\n");
      VG_(message)(Vg_UserMsg, "The only supported clone() uses are:\n");
      VG_(message)(Vg_UserMsg, " - via a threads library (LinuxThreads or NPTL)\n");
      VG_(message)(Vg_UserMsg, " - via the implementation of fork or vfork\n");
      VG_(message)(Vg_UserMsg, " - for the Quadrics Elan3 user-space driver\n");
      VG_(unimplemented)
         ("Valgrind does not support general clone().");
   }

   if (SUCCESS) {
      if (ARG1 & VKI_CLONE_PARENT_SETTID)
         POST_MEM_WRITE(ARG3, sizeof(Int));
      if (ARG1 & (VKI_CLONE_CHILD_SETTID | VKI_CLONE_CHILD_CLEARTID))
         POST_MEM_WRITE(ARG5, sizeof(Int));

      *flags |= SfYieldAfter;
   }
}


PRE(sys_rt_sigreturn)
{

   PRINT("rt_sigreturn ( )");

   vg_assert(VG_(is_valid_tid)(tid));
   vg_assert(tid >= 1 && tid < VG_N_THREADS);
   vg_assert(VG_(is_running_thread)(tid));

   
   VG_(sigframe_destroy)(tid, True);

   *flags |= SfNoWriteResult;
   SET_STATUS_Success(0);

   
   *flags |= SfPollAfter;
}




#define PLAX_(sysno, name)    WRAPPER_ENTRY_X_(arm64_linux, sysno, name) 
#define PLAXY(sysno, name)    WRAPPER_ENTRY_XY(arm64_linux, sysno, name)


static SyscallTableEntry syscall_main_table[] = {
   LINXY(__NR_getxattr,          sys_getxattr),          
   LINXY(__NR_lgetxattr,         sys_lgetxattr),         
   GENXY(__NR_getcwd,            sys_getcwd),            
   LINXY(__NR_eventfd2,          sys_eventfd2),          
   LINXY(__NR_epoll_create1,     sys_epoll_create1),     
   LINX_(__NR_epoll_ctl,         sys_epoll_ctl),         
   LINXY(__NR_epoll_pwait,       sys_epoll_pwait),       
   GENXY(__NR_dup,               sys_dup),               
   LINXY(__NR_dup3,              sys_dup3),              

   
   LINXY(__NR3264_fcntl,         sys_fcntl),             

   LINXY(__NR_inotify_init1,     sys_inotify_init1),     
   LINX_(__NR_inotify_add_watch, sys_inotify_add_watch), 
   LINX_(__NR_inotify_rm_watch,  sys_inotify_rm_watch),  
   LINXY(__NR_ioctl,             sys_ioctl),             
   GENX_(__NR_flock,             sys_flock),             
   LINX_(__NR_mknodat,           sys_mknodat),           
   LINX_(__NR_mkdirat,           sys_mkdirat),           
   LINX_(__NR_unlinkat,          sys_unlinkat),          
   LINX_(__NR_symlinkat,         sys_symlinkat),         
   LINX_(__NR_linkat,            sys_linkat),            
   LINX_(__NR_renameat,		 sys_renameat),          

   LINX_(__NR_umount2,            sys_umount),           
   LINX_(__NR_mount,              sys_mount),            

   
   GENXY(__NR3264_statfs,        sys_statfs),            
   GENXY(__NR3264_fstatfs,       sys_fstatfs),           

   
   GENX_(__NR3264_ftruncate,     sys_ftruncate),         

   LINX_(__NR_fallocate,         sys_fallocate),         
   LINX_(__NR_faccessat,         sys_faccessat),         
   GENX_(__NR_chdir,             sys_chdir),             
   GENX_(__NR_fchdir,            sys_fchdir),            
   GENX_(__NR_chroot,            sys_chroot),            
   GENX_(__NR_fchmod,            sys_fchmod),            
   LINX_(__NR_fchmodat,          sys_fchmodat),          
   LINX_(__NR_fchownat,          sys_fchownat),          
   GENX_(__NR_fchown,            sys_fchown),            
   LINXY(__NR_openat,            sys_openat),            
   GENXY(__NR_close,             sys_close),             
   LINXY(__NR_pipe2,             sys_pipe2),             
   LINX_(__NR_quotactl,          sys_quotactl),          
   GENXY(__NR_getdents64,        sys_getdents64),        

   
   LINX_(__NR3264_lseek,         sys_lseek),             

   GENXY(__NR_read,              sys_read),              
   GENX_(__NR_write,             sys_write),             
   GENXY(__NR_readv,             sys_readv),             
   GENX_(__NR_writev,            sys_writev),            
   GENXY(__NR_pread64,           sys_pread64),           
   GENX_(__NR_pwrite64,          sys_pwrite64),          
   LINX_(__NR_pselect6,          sys_pselect6),          
   LINXY(__NR_ppoll,             sys_ppoll),             
   LINXY(__NR_signalfd4,         sys_signalfd4),         
   LINX_(__NR_readlinkat,        sys_readlinkat),        

   
   LINXY(__NR3264_fstatat,       sys_newfstatat),        
   GENXY(__NR3264_fstat,         sys_newfstat),          

   LINX_(__NR_utimensat,         sys_utimensat),         
   GENX_(__NR_fsync,             sys_fsync),             
   GENX_(__NR_fdatasync,         sys_fdatasync),         
   LINXY(__NR_timerfd_create,    sys_timerfd_create),    
   LINXY(__NR_timerfd_settime,   sys_timerfd_settime),   
   LINXY(__NR_timerfd_gettime,   sys_timerfd_gettime),   
   LINXY(__NR_capget,            sys_capget),            
   GENX_(__NR_exit,              sys_exit),              
   LINX_(__NR_exit_group,        sys_exit_group),        
   LINX_(__NR_set_tid_address,   sys_set_tid_address),   
   LINXY(__NR_futex,             sys_futex),             
   LINX_(__NR_set_robust_list,   sys_set_robust_list),   
   GENXY(__NR_nanosleep,         sys_nanosleep),         
   GENXY(__NR_setitimer,         sys_setitimer),         
   LINXY(__NR_clock_gettime,     sys_clock_gettime),     
   LINXY(__NR_clock_getres,      sys_clock_getres),      
   LINXY(__NR_syslog,            sys_syslog),            
   LINX_(__NR_sched_setaffinity, sys_sched_setaffinity), 
   LINXY(__NR_sched_getaffinity, sys_sched_getaffinity), 
   LINX_(__NR_sched_yield,       sys_sched_yield),       
   GENX_(__NR_kill,              sys_kill),              
   LINX_(__NR_tgkill,            sys_tgkill),            
   GENXY(__NR_sigaltstack,       sys_sigaltstack),       
   LINX_(__NR_rt_sigsuspend,     sys_rt_sigsuspend),     
   LINXY(__NR_rt_sigaction,      sys_rt_sigaction),      
   LINXY(__NR_rt_sigprocmask,    sys_rt_sigprocmask),    
   LINXY(__NR_rt_sigtimedwait,   sys_rt_sigtimedwait),   
   LINXY(__NR_rt_sigqueueinfo,   sys_rt_sigqueueinfo),   
   PLAX_(__NR_rt_sigreturn,      sys_rt_sigreturn),      
   GENX_(__NR_setpriority,       sys_setpriority),       
   GENX_(__NR_getpriority,       sys_getpriority),       
   GENX_(__NR_setregid,          sys_setregid),          
   GENX_(__NR_setgid,            sys_setgid),            
   GENX_(__NR_setreuid,          sys_setreuid),          
   LINX_(__NR_setresuid,         sys_setresuid),         
   LINXY(__NR_getresuid,         sys_getresuid),         
   LINXY(__NR_getresgid,         sys_getresgid),         
   GENXY(__NR_times,             sys_times),             
   GENX_(__NR_setpgid,           sys_setpgid),           
   GENX_(__NR_getpgid,           sys_getpgid),           
   GENX_(__NR_getsid,            sys_getsid),            
   GENX_(__NR_setsid,            sys_setsid),            
   GENXY(__NR_getgroups,         sys_getgroups),         
   GENX_(__NR_setgroups,         sys_setgroups),         
   GENXY(__NR_uname,             sys_newuname),          
   GENXY(__NR_getrlimit,         sys_old_getrlimit),     
   GENX_(__NR_setrlimit,         sys_setrlimit),         
   GENXY(__NR_getrusage,         sys_getrusage),         
   GENX_(__NR_umask,             sys_umask),             
   LINXY(__NR_prctl,             sys_prctl),             
   GENXY(__NR_gettimeofday,      sys_gettimeofday),      
   GENX_(__NR_getpid,            sys_getpid),            
   GENX_(__NR_getppid,           sys_getppid),           
   GENX_(__NR_getuid,            sys_getuid),            
   GENX_(__NR_geteuid,           sys_geteuid),           
   GENX_(__NR_getgid,            sys_getgid),            
   GENX_(__NR_getegid,           sys_getegid),           
   LINX_(__NR_gettid,            sys_gettid),            
   LINXY(__NR_sysinfo,           sys_sysinfo),           
   LINXY(__NR_mq_open,           sys_mq_open),           
   LINX_(__NR_mq_unlink,         sys_mq_unlink),         
   LINX_(__NR_mq_timedsend,      sys_mq_timedsend),      
   LINXY(__NR_mq_timedreceive,   sys_mq_timedreceive),   
   LINX_(__NR_mq_notify,         sys_mq_notify),         
   LINXY(__NR_mq_getsetattr,     sys_mq_getsetattr),     
   LINX_(__NR_msgget,            sys_msgget),            
   LINXY(__NR_msgctl,            sys_msgctl),            
   LINXY(__NR_msgrcv,            sys_msgrcv),            
   LINX_(__NR_msgsnd,            sys_msgsnd),            
   LINX_(__NR_semget,            sys_semget),            
   LINXY(__NR_semctl,            sys_semctl),            
   LINX_(__NR_semtimedop,        sys_semtimedop),        
   LINX_(__NR_semop,             sys_semop),             
   LINX_(__NR_shmget,            sys_shmget),            
   LINXY(__NR_shmctl,            sys_shmctl),            
   LINXY(__NR_shmat,             wrap_sys_shmat),        
   LINXY(__NR_shmdt,             sys_shmdt),             
   LINXY(__NR_socket,            sys_socket),            
   LINXY(__NR_socketpair,        sys_socketpair),        
   LINX_(__NR_bind,              sys_bind),              
   LINX_(__NR_listen,            sys_listen),            
   LINXY(__NR_accept,            sys_accept),            
   LINX_(__NR_connect,           sys_connect),           
   LINXY(__NR_getsockname,       sys_getsockname),       
   LINXY(__NR_getpeername,       sys_getpeername),       
   LINX_(__NR_sendto,            sys_sendto),            
   LINXY(__NR_recvfrom,          sys_recvfrom),          
   LINX_(__NR_setsockopt,        sys_setsockopt),        
   LINXY(__NR_getsockopt,        sys_getsockopt),        
   LINX_(__NR_shutdown,          sys_shutdown),          
   LINX_(__NR_sendmsg,           sys_sendmsg),           
   LINXY(__NR_recvmsg,           sys_recvmsg),           
   LINX_(__NR_readahead,         sys_readahead),         
   GENX_(__NR_brk,               sys_brk),               
   GENXY(__NR_munmap,            sys_munmap),            
   GENX_(__NR_mremap,            sys_mremap),            
   LINX_(__NR_add_key,           sys_add_key),           
   LINXY(__NR_keyctl,            sys_keyctl),            
   PLAX_(__NR_clone,             sys_clone),             
   GENX_(__NR_execve,            sys_execve),            

   
   PLAX_(__NR3264_mmap,          sys_mmap),              
   PLAX_(__NR3264_fadvise64,     sys_fadvise64),         

   GENXY(__NR_mprotect,          sys_mprotect),          
   GENX_(__NR_msync,             sys_msync),             
   GENX_(__NR_mlock,             sys_mlock),             
   GENX_(__NR_mlockall,          sys_mlockall),          
   GENX_(__NR_madvise,           sys_madvise),           
   LINX_(__NR_mbind,             sys_mbind),             
   LINXY(__NR_get_mempolicy,     sys_get_mempolicy),     
   LINX_(__NR_set_mempolicy,     sys_set_mempolicy),     

   LINXY(__NR_recvmmsg,          sys_recvmmsg),          
   LINXY(__NR_accept4,           sys_accept4),           

   GENXY(__NR_wait4,             sys_wait4),             

   LINX_(__NR_syncfs,            sys_syncfs),            

   LINXY(__NR_sendmmsg,          sys_sendmmsg),          
   LINXY(__NR_process_vm_readv,  sys_process_vm_readv),  
   LINX_(__NR_process_vm_writev, sys_process_vm_writev), 
   LINXY(__NR_getrandom,         sys_getrandom),         
   LINXY(__NR_memfd_create,      sys_memfd_create),      



};



SyscallTableEntry* ML_(get_linux_syscall_entry) ( UInt sysno )
{
   const UInt syscall_main_table_size
      = sizeof(syscall_main_table) / sizeof(syscall_main_table[0]);

   
   if (sysno < syscall_main_table_size) {
      SyscallTableEntry* sys = &syscall_main_table[sysno];
      if (sys->before == NULL)
         return NULL; 
      else
         return sys;
   }


   
   return NULL;
}

#endif 

