#define _GNU_SOURCE

#include "../../memcheck.h"
#include "scalar.h"
#include <unistd.h>
#include <sched.h>
#include <signal.h>
#include <linux/mman.h> 


int main(void)
{
   
   long* px  = malloc(sizeof(long));
   long  x0  = px[0];
   long  res;

   

   
   GO(__NR_restart_syscall, "n/a");
 

   
   GO(__NR_exit, "below");
   

   
   GO(__NR_fork, "other");
   

   
   
   GO(__NR_read, "1+3s 1m");
   SY(__NR_read+x0, x0, x0, x0+1); FAILx(EFAULT);

   
   GO(__NR_write, "3s 1m");
   SY(__NR_write, x0, x0, x0+1); FAIL;

   
   GO(__NR_open, "(2-args) 2s 1m");
   SY(__NR_open, x0, x0); FAIL;

   
   
   
   GO(__NR_open, "(3-args) 1s 0m");    
   SY(__NR_open, "scalar.c", O_CREAT|O_EXCL, x0); FAIL;

   
   GO(__NR_close, "1s 0m");
   SY(__NR_close, x0-1); FAIL;

   
   GO(__NR_waitpid, "3s 1m");
   SY(__NR_waitpid, x0, x0+1, x0); FAIL;

   
   GO(__NR_creat, "2s 1m");
   SY(__NR_creat, x0, x0); FAIL;

   
   GO(__NR_link, "2s 2m");
   SY(__NR_link, x0, x0); FAIL;

   
   GO(__NR_unlink, "1s 1m");
   SY(__NR_unlink, x0); FAIL;

   
   
   
   GO(__NR_execve, "3s 1m");
   SY(__NR_execve, x0, x0, x0); FAIL;

   
   GO(__NR_chdir, "1s 1m");
   SY(__NR_chdir, x0); FAIL;

   
   GO(__NR_time, "1s 1m");
   SY(__NR_time, x0+1); FAIL;

   
   GO(__NR_mknod, "3s 1m");
   SY(__NR_mknod, x0, x0, x0); FAIL;

   
   GO(__NR_chmod, "2s 1m");
   SY(__NR_chmod, x0, x0); FAIL;

   
   GO(__NR_lchown, "n/a");
 

   
   GO(__NR_break, "ni");
   SY(__NR_break); FAIL;

   
   GO(__NR_oldstat, "n/a");
   

   
   GO(__NR_lseek, "3s 0m");
   SY(__NR_lseek, x0-1, x0, x0); FAILx(EBADF);

   
   GO(__NR_getpid, "0s 0m");
   SY(__NR_getpid); SUCC;

   
   GO(__NR_mount, "5s 3m");
   SY(__NR_mount, x0, x0, x0, x0, x0); FAIL;
   
   
   GO(__NR_umount, "1s 1m");
   SY(__NR_umount, x0); FAIL;

   
   GO(__NR_setuid, "1s 0m");
   SY(__NR_setuid, x0); FAIL;

   
   GO(__NR_getuid, "0s 0m");
   SY(__NR_getuid); SUCC;

   
   GO(__NR_stime, "n/a");
 

   
   
   GO(__NR_ptrace, "4s 1m");
   SY(__NR_ptrace, x0+PTRACE_GETREGS, x0, x0, x0); FAIL;

   
   GO(__NR_alarm, "1s 0m");
   SY(__NR_alarm, x0); SUCC;

   
   GO(__NR_oldfstat, "n/a");
   

   
   GO(__NR_pause, "ignore");
   

   
   GO(__NR_utime, "2s 2m");
   SY(__NR_utime, x0, x0+1); FAIL;

   
   GO(__NR_stty, "ni");
   SY(__NR_stty); FAIL;

   
   GO(__NR_gtty, "ni");
   SY(__NR_gtty); FAIL;

   
   GO(__NR_access, "2s 1m");
   SY(__NR_access, x0, x0); FAIL;

   
   GO(__NR_nice, "1s 0m");
   SY(__NR_nice, x0); SUCC;

   
   GO(__NR_ftime, "ni");
   SY(__NR_ftime); FAIL;

   
   GO(__NR_sync, "0s 0m");
   SY(__NR_sync); SUCC;

   
   GO(__NR_kill, "2s 0m");
   SY(__NR_kill, x0, x0); SUCC;

   
   GO(__NR_rename, "2s 2m");
   SY(__NR_rename, x0, x0); FAIL;

   
   GO(__NR_mkdir, "2s 1m");
   SY(__NR_mkdir, x0, x0); FAIL;

   
   GO(__NR_rmdir, "1s 1m");
   SY(__NR_rmdir, x0); FAIL;

   
   GO(__NR_dup, "1s 0m");
   SY(__NR_dup, x0-1); FAIL;

   
   GO(__NR_pipe, "1s 1m");
   SY(__NR_pipe, x0); FAIL;

   
   GO(__NR_times, "1s 1m");
   SY(__NR_times, x0+1); FAIL;

   
   GO(__NR_prof, "ni");
   SY(__NR_prof); FAIL;

   
   GO(__NR_brk, "1s 0m");
   SY(__NR_brk, x0); SUCC;

   
   GO(__NR_setgid, "1s 0m");
   SY(__NR_setgid, x0); FAIL;

   
   GO(__NR_getgid, "0s 0m");
   SY(__NR_getgid); SUCC;

   
   GO(__NR_signal, "n/a");
 

   
   GO(__NR_geteuid, "0s 0m");
   SY(__NR_geteuid); SUCC;

   
   GO(__NR_getegid, "0s 0m");
   SY(__NR_getegid); SUCC;

   
   GO(__NR_acct, "1s 1m");
   SY(__NR_acct, x0); FAIL;

   
   GO(__NR_umount2, "2s 1m");
   SY(__NR_umount2, x0, x0); FAIL;

   
   GO(__NR_lock, "ni");
   SY(__NR_lock); FAIL;

   
   #include <asm/ioctls.h>
   GO(__NR_ioctl, "3s 1m");
   SY(__NR_ioctl, x0, x0+TCSETS, x0); FAIL;

   
   
   
   GO(__NR_fcntl, "(GETFD) 2s 0m");
   SY(__NR_fcntl, x0-1, x0+F_GETFD, x0); FAILx(EBADF);

   
   
   
   GO(__NR_fcntl, "(DUPFD) 1s 0m");
   SY(__NR_fcntl, -1, F_DUPFD, x0); FAILx(EBADF);

   
   
   
   GO(__NR_fcntl, "(GETLK) 1s 0m");
   SY(__NR_fcntl, -1, F_GETLK, x0); FAIL; 

   
   GO(__NR_mpx, "ni");
   SY(__NR_mpx); FAIL;

   
   GO(__NR_setpgid, "2s 0m");
   SY(__NR_setpgid, x0, x0-1); FAIL;

   
   GO(__NR_ulimit, "ni");
   SY(__NR_ulimit); FAIL;

   
   GO(__NR_oldolduname, "n/a");
   

   
   GO(__NR_umask, "1s 0m");
   SY(__NR_umask, x0+022); SUCC;

   
   GO(__NR_chroot, "1s 1m");
   SY(__NR_chroot, x0); FAIL;

   
   GO(__NR_ustat, "n/a");
   

   
   GO(__NR_dup2, "2s 0m");
   SY(__NR_dup2, x0-1, x0); FAIL;

   
   GO(__NR_getppid, "0s 0m");
   SY(__NR_getppid); SUCC;

   
   GO(__NR_getpgrp, "0s 0m");
   SY(__NR_getpgrp); SUCC;

   
   GO(__NR_setsid, "0s 0m");
   SY(__NR_setsid); SUCC_OR_FAIL;

   
   GO(__NR_sigaction, "3s 4m");
   SY(__NR_sigaction, x0, x0+&px[1], x0+&px[1]); FAIL;

   
   GO(__NR_sgetmask, "n/a");
 

   
   GO(__NR_ssetmask, "n/a");
 

   
   GO(__NR_setreuid, "2s 0m");
   SY(__NR_setreuid, x0, x0); FAIL;

   
   GO(__NR_setregid, "2s 0m");
   SY(__NR_setregid, x0, x0); FAIL;

   
   
   GO(__NR_sigsuspend, "ignore");
   

   
   GO(__NR_sigpending, "1s 1m");
   SY(__NR_sigpending, x0); FAIL;

   
   GO(__NR_sethostname, "n/a");
 

   
   GO(__NR_setrlimit, "2s 1m");
   SY(__NR_setrlimit, x0, x0); FAIL;

   
   GO(__NR_getrlimit, "2s 1m");
   SY(__NR_getrlimit, x0, x0); FAIL;

   
   GO(__NR_getrusage, "2s 1m");
   SY(__NR_getrusage, x0, x0); FAIL;

   
   GO(__NR_gettimeofday, "2s 2m");
   SY(__NR_gettimeofday, x0+1, x0+1); FAIL;

   
   GO(__NR_settimeofday, "2s 2m");
   SY(__NR_settimeofday, x0+1, x0+1); FAIL;

   
   GO(__NR_getgroups, "2s 1m");
   SY(__NR_getgroups, x0+1, x0+1); FAIL;

   
   GO(__NR_setgroups, "2s 1m");
   SY(__NR_setgroups, x0+1, x0+1); FAIL;

   
   {
      long args[5] = { x0+8, x0+0xffffffee, x0+1, x0+1, x0+1 };
      GO(__NR_select, "1s 5m");
      SY(__NR_select, args+x0); FAIL;
   }

   
   GO(__NR_symlink, "2s 2m");
   SY(__NR_symlink, x0, x0); FAIL;

   
   GO(__NR_oldlstat, "n/a");
   

   
   GO(__NR_readlink, "3s 2m");
   SY(__NR_readlink, x0+1, x0+1, x0+1); FAIL;

   
   GO(__NR_uselib, "n/a");
 

   
   GO(__NR_swapon, "n/a");
 

   
   GO(__NR_reboot, "n/a");
 

   
   GO(__NR_readdir, "n/a");
   

   
   {
      long args[6] = { x0, x0, x0, x0, x0-1, x0 };
      GO(__NR_mmap, "1s 1m");
      SY(__NR_mmap, args+x0); FAIL;
   }

   
   GO(__NR_munmap, "2s 0m");
   SY(__NR_munmap, x0, x0); FAIL;

   
   GO(__NR_truncate, "2s 1m");
   SY(__NR_truncate, x0, x0); FAIL;

   
   GO(__NR_ftruncate, "2s 0m");
   SY(__NR_ftruncate, x0, x0); FAIL;

   
   GO(__NR_fchmod, "2s 0m");
   SY(__NR_fchmod, x0-1, x0); FAIL;

   
   GO(__NR_fchown, "3s 0m");
   SY(__NR_fchown, x0, x0, x0); FAIL;

   
   GO(__NR_getpriority, "2s 0m");
   SY(__NR_getpriority, x0-1, x0); FAIL;

   
   GO(__NR_setpriority, "3s 0m");
   SY(__NR_setpriority, x0-1, x0, x0); FAIL;

   
   GO(__NR_profil, "ni");
   SY(__NR_profil); FAIL;

   
   GO(__NR_statfs, "2s 2m");
   SY(__NR_statfs, x0, x0); FAIL;

   
   GO(__NR_fstatfs, "2s 1m");
   SY(__NR_fstatfs, x0, x0); FAIL;

   
   GO(__NR_ioperm, "3s 0m");
   SY(__NR_ioperm, x0, x0, x0); FAIL;

   
   GO(__NR_socketcall, "XXX");
   

   
   GO(__NR_syslog, "3s 1m");
   SY(__NR_syslog, x0+2, x0, x0+1); FAIL;

   
   GO(__NR_setitimer, "3s 2m");
   SY(__NR_setitimer, x0, x0+1, x0+1); FAIL;

   
   GO(__NR_getitimer, "2s 1m");
   SY(__NR_getitimer, x0, x0, x0); FAIL;

   
   GO(__NR_stat, "2s 2m");
   SY(__NR_stat, x0, x0); FAIL;

   
   GO(__NR_lstat, "2s 2m");
   SY(__NR_lstat, x0, x0); FAIL;

   
   GO(__NR_fstat, "2s 1m");
   SY(__NR_fstat, x0, x0); FAIL;

   
   GO(__NR_olduname, "n/a");
   

   
   GO(__NR_iopl, "1s 0m");
   SY(__NR_iopl, x0+100); FAIL;

   
   GO(__NR_vhangup, "0s 0m");
   SY(__NR_vhangup); SUCC_OR_FAIL;  
   
   
   GO(__NR_idle, "ni");
   SY(__NR_idle); FAIL;

   
   GO(__NR_vm86old, "n/a");
   

   
   GO(__NR_wait4, "4s 2m");
   SY(__NR_wait4, x0, x0+1, x0, x0+1); FAIL;

   
   GO(__NR_swapoff, "n/a");
 

   
   GO(__NR_sysinfo, "1s 1m");
   SY(__NR_sysinfo, x0); FAIL;

   
   
   
   
   GO(__NR_ipc, "5s 0m");
   SY(__NR_ipc, x0+4, x0, x0, x0, x0, x0); FAIL;

   
   GO(__NR_fsync, "1s 0m");
   SY(__NR_fsync, x0-1); FAIL;

   
   GO(__NR_sigreturn, "n/a");
 

   
#ifndef CLONE_PARENT_SETTID
#define CLONE_PARENT_SETTID	0x00100000
#endif
   GO(__NR_clone, "5s 3m");
   SY(__NR_clone, x0|CLONE_PARENT_SETTID|CLONE_SETTLS|CLONE_CHILD_SETTID|SIGCHLD, x0, x0, x0, x0); FAIL;
   if (0 == res) {
      SY(__NR_exit, 0); FAIL;
   }

   
   GO(__NR_setdomainname, "n/a");
 

   
   GO(__NR_uname, "1s 1m");
   SY(__NR_uname, x0); FAIL;

   
   GO(__NR_modify_ldt, "3s 1m");
   SY(__NR_modify_ldt, x0+1, x0, x0+1); FAILx(EINVAL);

   
   
     GO(__NR_adjtimex, "XXX");

   
   GO(__NR_mprotect, "3s 0m");
   SY(__NR_mprotect, x0+1, x0, x0); FAILx(EINVAL);

   
   GO(__NR_sigprocmask, "3s 2m");
   SY(__NR_sigprocmask, x0, x0+&px[1], x0+&px[1]); SUCC;

   
   GO(__NR_create_module, "ni");
   SY(__NR_create_module); FAIL;

   
   GO(__NR_init_module, "3s 2m");
   SY(__NR_init_module, x0, x0+1, x0); FAIL;

   
   GO(__NR_delete_module, "n/a");
 

   
   GO(__NR_get_kernel_syms, "ni");
   SY(__NR_get_kernel_syms); FAIL;

   
   GO(__NR_quotactl, "4s 1m");
   SY(__NR_quotactl, x0, x0, x0, x0); FAIL;

   
   GO(__NR_getpgid, "1s 0m");
   SY(__NR_getpgid, x0-1); FAIL;

   
   GO(__NR_fchdir, "1s 0m");
   SY(__NR_fchdir, x0-1); FAIL;

   
   GO(__NR_bdflush, "n/a");
 

   
   GO(__NR_sysfs, "n/a");
 

   
   GO(__NR_personality, "1s 0m");
   SY(__NR_personality, x0+0xffffffff); SUCC;

   
   GO(__NR_afs_syscall, "ni");
   SY(__NR_afs_syscall); FAIL;

   
   GO(__NR_setfsuid, "1s 0m");
   SY(__NR_setfsuid, x0); SUCC;  

   
   GO(__NR_setfsgid, "1s 0m");
   SY(__NR_setfsgid, x0); SUCC;  

   
   GO(__NR__llseek, "5s 1m");
   SY(__NR__llseek, x0, x0, x0, x0, x0); FAIL;

   
   GO(__NR_getdents, "3s 1m");
   SY(__NR_getdents, x0, x0, x0+1); FAIL;

   
   GO(__NR__newselect, "5s 4m");
   SY(__NR__newselect, x0+8, x0+0xffffffff, x0+1, x0+1, x0+1); FAIL;

   
   GO(__NR_flock, "2s 0m");
   SY(__NR_flock, x0, x0); FAIL;

   
   GO(__NR_msync, "3s 1m");
   SY(__NR_msync, x0, x0+1, x0); FAIL;

   
   GO(__NR_readv, "3s 1m");
   SY(__NR_readv, x0, x0, x0+1); FAIL;

   
   GO(__NR_writev, "3s 1m");
   SY(__NR_writev, x0, x0, x0+1); FAIL;

   
   GO(__NR_getsid, "1s 0m");
   SY(__NR_getsid, x0-1); FAIL;

   
   GO(__NR_fdatasync, "1s 0m");
   SY(__NR_fdatasync, x0-1); FAIL;

   
   GO(__NR__sysctl, "1s 1m");
   SY(__NR__sysctl, x0); FAIL;

   
   GO(__NR_mlock, "2s 0m");
   SY(__NR_mlock, x0, x0+1); FAIL;

   
   GO(__NR_munlock, "2s 0m");
   SY(__NR_munlock, x0, x0+1); FAIL;

   
   GO(__NR_mlockall, "1s 0m");
   SY(__NR_mlockall, x0-1); FAIL;

   
   GO(__NR_munlockall, "0s 0m");
   SY(__NR_munlockall); SUCC_OR_FAILx(EPERM);

   
   GO(__NR_sched_setparam, "2s 1m");
   SY(__NR_sched_setparam, x0, x0); FAIL;

   
   GO(__NR_sched_getparam, "2s 1m");
   SY(__NR_sched_getparam, x0, x0); FAIL;

   
   GO(__NR_sched_setscheduler, "3s 1m");
   SY(__NR_sched_setscheduler, x0-1, x0, x0+1); FAIL;

   
   GO(__NR_sched_getscheduler, "1s 0m");
   SY(__NR_sched_getscheduler, x0-1); FAIL;

   
   GO(__NR_sched_yield, "0s 0m");
   SY(__NR_sched_yield); SUCC;

   
   GO(__NR_sched_get_priority_max, "1s 0m");
   SY(__NR_sched_get_priority_max, x0-1); FAIL;

   
   GO(__NR_sched_get_priority_min, "1s 0m");
   SY(__NR_sched_get_priority_min, x0-1); FAIL;

   
   GO(__NR_sched_rr_get_interval, "n/a");
 

   
   GO(__NR_nanosleep, "2s 2m");
   SY(__NR_nanosleep, x0, x0+1); FAIL;

   
   GO(__NR_mremap, "5s 0m");
   SY(__NR_mremap, x0+1, x0, x0, x0+MREMAP_FIXED, x0); FAILx(EINVAL);

   
   GO(__NR_setresuid, "3s 0m");
   SY(__NR_setresuid, x0, x0, x0); FAIL;

   
   GO(__NR_getresuid, "3s 3m");
   SY(__NR_getresuid, x0, x0, x0); FAIL;

   
   GO(__NR_vm86, "n/a");
   

   
   GO(__NR_query_module, "ni");
   SY(__NR_query_module); FAIL;

   
   GO(__NR_poll, "3s 1m");
   SY(__NR_poll, x0, x0+1, x0); FAIL;

   
   GO(__NR_nfsservctl, "n/a");
 

   
   GO(__NR_setresgid, "3s 0m");
   SY(__NR_setresgid, x0, x0, x0); FAIL;

   
   GO(__NR_getresgid, "3s 3m");
   SY(__NR_getresgid, x0, x0, x0); FAIL;

   
   GO(__NR_prctl, "5s 0m");
   SY(__NR_prctl, x0, x0, x0, x0, x0); FAIL;

   
   GO(__NR_rt_sigreturn, "n/a");
 

   
   GO(__NR_rt_sigaction, "4s 4m");
   SY(__NR_rt_sigaction, x0, x0+&px[2], x0+&px[2], x0); FAIL;

   
   GO(__NR_rt_sigprocmask, "4s 2m");
   SY(__NR_rt_sigprocmask, x0, x0+1, x0+1, x0); FAIL;

   
   GO(__NR_rt_sigpending, "2s 1m");
   SY(__NR_rt_sigpending, x0, x0+1); FAIL;

   
   GO(__NR_rt_sigtimedwait, "4s 3m");
   SY(__NR_rt_sigtimedwait, x0+1, x0+1, x0+1, x0); FAIL;

   
   GO(__NR_rt_sigqueueinfo, "3s 1m");
   SY(__NR_rt_sigqueueinfo, x0, x0+1, x0); FAIL;

   
   GO(__NR_rt_sigsuspend, "ignore");
   

   
   GO(__NR_pread64, "5s 1m");
   SY(__NR_pread64, x0, x0, x0+1, x0, x0); FAIL;

   
   GO(__NR_pwrite64, "5s 1m");
   SY(__NR_pwrite64, x0, x0, x0+1, x0, x0); FAIL;

   
   GO(__NR_chown, "3s 1m");
   SY(__NR_chown, x0, x0, x0); FAIL;

   
   GO(__NR_getcwd, "2s 1m");
   SY(__NR_getcwd, x0, x0+1); FAIL;

   
   GO(__NR_capget, "2s 2m");
   SY(__NR_capget, x0, x0+1); FAIL;

   
   GO(__NR_capset, "2s 2m");
   SY(__NR_capset, x0, x0); FAIL;

   
   {
      struct our_sigaltstack {
              void *ss_sp;
              int ss_flags;
              size_t ss_size;
      } ss;
      ss.ss_sp     = NULL;
      ss.ss_flags  = 0;
      ss.ss_size   = 0;
      VALGRIND_MAKE_MEM_NOACCESS(& ss, sizeof(struct our_sigaltstack));
      GO(__NR_sigaltstack, "2s 2m");
      SY(__NR_sigaltstack, x0+&ss, x0+&ss); SUCC;
   }

   
   GO(__NR_sendfile, "4s 1m");
   SY(__NR_sendfile, x0, x0, x0+1, x0); FAIL;

   
   
   
   GO(__NR_getpmsg, "5s 0m");
   SY(__NR_getpmsg, x0, x0, x0, x0); FAIL;

   
   
   
   GO(__NR_putpmsg, "5s 0m");
   SY(__NR_putpmsg, x0, x0, x0, x0, x0); FAIL;

   
   GO(__NR_vfork, "other");
   

   
   GO(__NR_ugetrlimit, "2s 1m");
   SY(__NR_ugetrlimit, x0, x0); FAIL;

   
   GO(__NR_mmap2, "6s 0m");
   SY(__NR_mmap2, x0, x0, x0, x0, x0-1, x0); FAIL;

   
   GO(__NR_truncate64, "3s 1m");
   SY(__NR_truncate64, x0, x0, x0); FAIL;

   
   GO(__NR_ftruncate64, "3s 0m");
   SY(__NR_ftruncate64, x0, x0, x0); FAIL;

   
   GO(__NR_stat64, "2s 2m");
   SY(__NR_stat64, x0, x0); FAIL;

   
   GO(__NR_lstat64, "2s 2m");
   SY(__NR_lstat64, x0, x0); FAIL;

   
   GO(__NR_fstat64, "2s 1m");
   SY(__NR_fstat64, x0, x0); FAIL;

   
   GO(__NR_lchown32, "3s 1m");
   SY(__NR_lchown32, x0, x0, x0); FAIL;

   
   GO(__NR_getuid32, "0s 0m");
   SY(__NR_getuid32); SUCC;

   
   GO(__NR_getgid32, "0s 0m");
   SY(__NR_getgid32); SUCC;

   
   GO(__NR_geteuid32, "0s 0m");
   SY(__NR_geteuid32); SUCC;

   
   GO(__NR_getegid32, "0s 0m");
   SY(__NR_getegid32); SUCC;

   
   GO(__NR_setreuid32, "2s 0m");
   SY(__NR_setreuid32, x0, x0); FAIL;

   
   GO(__NR_setregid32, "2s 0m");
   SY(__NR_setregid32, x0, x0); FAIL;

   
   GO(__NR_getgroups32, "2s 1m");
   SY(__NR_getgroups32, x0+1, x0+1); FAIL;

   
   GO(__NR_setgroups32, "2s 1m");
   SY(__NR_setgroups32, x0+1, x0+1); FAIL;

   
   GO(__NR_fchown32, "3s 0m");
   SY(__NR_fchown32, x0, x0, x0); FAIL;

   
   GO(__NR_setresuid32, "3s 0m");
   SY(__NR_setresuid32, x0, x0, x0); FAIL;

   
   GO(__NR_getresuid32, "3s 3m");
   SY(__NR_getresuid32, x0, x0, x0); FAIL;

   
   GO(__NR_setresgid32, "3s 0m");
   SY(__NR_setresgid32, x0, x0, x0); FAIL;

   
   GO(__NR_getresgid32, "3s 3m");
   SY(__NR_getresgid32, x0, x0, x0); FAIL;

   
   GO(__NR_chown32, "3s 1m");
   SY(__NR_chown32, x0, x0, x0); FAIL;

   
   GO(__NR_setuid32, "1s 0m");
   SY(__NR_setuid32, x0); FAIL;

   
   GO(__NR_setgid32, "1s 0m");
   SY(__NR_setgid32, x0); FAIL;

   
   GO(__NR_setfsuid32, "1s 0m");
   SY(__NR_setfsuid32, x0); SUCC;  

   
   GO(__NR_setfsgid32, "1s 0m");
   SY(__NR_setfsgid32, x0); SUCC;  

   
   GO(__NR_pivot_root, "n/a");
 

   
   GO(__NR_mincore, "3s 1m");
   SY(__NR_mincore, x0, x0+40960, x0); FAIL;

   
   GO(__NR_madvise, "3s 0m");
   SY(__NR_madvise, x0, x0+1, x0); FAILx(ENOMEM);

   
   GO(__NR_getdents64, "3s 1m");
   SY(__NR_getdents64, x0, x0, x0+1); FAIL;

   
   
   
   
   GO(__NR_fcntl64, "(GETFD) 2s 0m");
   SY(__NR_fcntl64, x0-1, x0+F_GETFD, x0); FAILx(EBADF);

   
   GO(__NR_fcntl64, "(DUPFD) 1s 0m");
   SY(__NR_fcntl64, -1, F_DUPFD, x0); FAILx(EBADF);

   
   
   
   GO(__NR_fcntl64, "(GETLK) 1s 0m"); 
   SY(__NR_fcntl64, -1, +F_GETLK, x0); FAIL; 

   
   GO(222, "ni");
   SY(222); FAIL;

   
   GO(223, "ni");
   SY(223); FAIL;

   
   GO(__NR_gettid, "n/a");
 

   
   GO(__NR_readahead, "n/a");
 

   
   GO(__NR_setxattr, "5s 3m");
   SY(__NR_setxattr, x0, x0, x0, x0+1, x0); FAIL;

   
   GO(__NR_lsetxattr, "5s 3m");
   SY(__NR_lsetxattr, x0, x0, x0, x0+1, x0); FAIL;

   
   GO(__NR_fsetxattr, "5s 2m");
   SY(__NR_fsetxattr, x0, x0, x0, x0+1, x0); FAIL;

   
   GO(__NR_getxattr, "4s 3m");
   SY(__NR_getxattr, x0, x0, x0, x0+1); FAIL;

   
   GO(__NR_lgetxattr, "4s 3m");
   SY(__NR_lgetxattr, x0, x0, x0, x0+1); FAIL;

   
   GO(__NR_fgetxattr, "4s 2m");
   SY(__NR_fgetxattr, x0, x0, x0, x0+1); FAIL;

   
   GO(__NR_listxattr, "3s 2m");
   SY(__NR_listxattr, x0, x0, x0+1); FAIL;

   
   GO(__NR_llistxattr, "3s 2m");
   SY(__NR_llistxattr, x0, x0, x0+1); FAIL;

   
   GO(__NR_flistxattr, "3s 1m");
   SY(__NR_flistxattr, x0-1, x0, x0+1); FAIL; 

   
   GO(__NR_removexattr, "2s 2m");
   SY(__NR_removexattr, x0, x0); FAIL;

   
   GO(__NR_lremovexattr, "2s 2m");
   SY(__NR_lremovexattr, x0, x0); FAIL;

   
   GO(__NR_fremovexattr, "2s 1m");
   SY(__NR_fremovexattr, x0, x0); FAIL;

   
   GO(__NR_tkill, "n/a");
 

   
   GO(__NR_sendfile64, "4s 1m");
   SY(__NR_sendfile64, x0, x0, x0+1, x0); FAIL;

   
   #ifndef FUTEX_WAIT
   #define FUTEX_WAIT   0
   #endif
   
   GO(__NR_futex, "5s 2m");
   SY(__NR_futex, x0+FUTEX_WAIT, x0, x0, x0+1, x0, x0); FAIL;

   
   GO(__NR_sched_setaffinity, "3s 1m");
   SY(__NR_sched_setaffinity, x0, x0+1, x0); FAIL;

   
   GO(__NR_sched_getaffinity, "3s 1m");
   SY(__NR_sched_getaffinity, x0, x0+1, x0); FAIL;

   
   GO(__NR_set_thread_area, "1s 1m");
   SY(__NR_set_thread_area, x0); FAILx(EFAULT);

   
   GO(__NR_get_thread_area, "1s 1m");
   SY(__NR_get_thread_area, x0); FAILx(EFAULT);

   
   GO(__NR_io_setup, "2s 1m");
   SY(__NR_io_setup, x0, x0); FAIL;

   
   {
      
      struct fake_aio_ring {   
        unsigned        id;     
        unsigned        nr;     
        
        
      } ring = { 0, 0 };
      struct fake_aio_ring* ringptr = &ring;
      GO(__NR_io_destroy, "1s 0m");
      SY(__NR_io_destroy, x0+&ringptr); FAIL;
   }

   
   GO(__NR_io_getevents, "5s 2m");
   SY(__NR_io_getevents, x0, x0, x0+1, x0, x0+1); FAIL;

   
   GO(__NR_io_submit, "3s 1m");
   SY(__NR_io_submit, x0, x0+1, x0); FAIL;

   
   GO(__NR_io_cancel, "3s 2m");
   SY(__NR_io_cancel, x0, x0, x0); FAIL;

   
   GO(__NR_fadvise64, "n/a");
 

   
   GO(251, "ni");
   SY(251); FAIL;

   
   GO(__NR_exit_group, "other");
   

   
   GO(__NR_lookup_dcookie, "4s 1m");
   SY(__NR_lookup_dcookie, x0, x0, x0, x0+1); FAIL;

   
   GO(__NR_epoll_create, "1s 0m");
   SY(__NR_epoll_create, x0); SUCC_OR_FAIL;

   
   GO(__NR_epoll_ctl, "4s 1m");
   SY(__NR_epoll_ctl, x0, x0, x0, x0); FAIL;

   
   GO(__NR_epoll_wait, "4s 1m");
   SY(__NR_epoll_wait, x0, x0, x0+1, x0); FAIL;

   
   GO(__NR_remap_file_pages, "n/a");
 

   
   GO(__NR_set_tid_address, "1s 0m");
   SY(__NR_set_tid_address, x0); SUCC_OR_FAILx(ENOSYS);

   
   GO(__NR_timer_create, "3s 2m");
   SY(__NR_timer_create, x0, x0+1, x0); FAIL;

   
   GO(__NR_timer_settime, "4s 2m");
   SY(__NR_timer_settime, x0, x0, x0, x0+1); FAIL;

   
   GO(__NR_timer_gettime, "2s 1m");
   SY(__NR_timer_gettime, x0, x0); FAIL;

   
   GO(__NR_timer_getoverrun, "1s 0m");
   SY(__NR_timer_getoverrun, x0); FAIL;

   
   GO(__NR_timer_delete, "1s 0m");
   SY(__NR_timer_delete, x0); FAIL;

   
   GO(__NR_clock_settime, "2s 1m");
   SY(__NR_clock_settime, x0, x0);  FAIL; FAIL;

   
   GO(__NR_clock_gettime, "2s 1m");
   SY(__NR_clock_gettime, x0, x0); FAIL;

   
   GO(__NR_clock_getres, "2s 1m");
   SY(__NR_clock_getres, x0+1, x0+1); FAIL; FAIL;

   
   GO(__NR_clock_nanosleep, "n/a");
 

   
   GO(__NR_statfs64, "3s 2m");
   SY(__NR_statfs64, x0, x0+1, x0); FAIL;

   
   GO(__NR_fstatfs64, "3s 1m");
   SY(__NR_fstatfs64, x0, x0+1, x0); FAIL;

   
   GO(__NR_tgkill, "n/a");
 

   
   GO(__NR_utimes, "2s 2m");
   SY(__NR_utimes, x0, x0+1); FAIL;

   
   GO(__NR_fadvise64_64, "n/a");
 

   
   GO(__NR_vserver, "ni");
   SY(__NR_vserver); FAIL;

   
   GO(__NR_mbind, "n/a");
 

   
   GO(__NR_get_mempolicy, "n/a");
 

   
   GO(__NR_set_mempolicy, "n/a");
 

   
   GO(__NR_mq_open, "4s 3m");
   SY(__NR_mq_open, x0, x0+O_CREAT, x0, x0+1); FAIL;

   
   GO(__NR_mq_unlink, "1s 1m");
   SY(__NR_mq_unlink, x0); FAIL;

   
   GO(__NR_mq_timedsend, "5s 2m");
   SY(__NR_mq_timedsend, x0, x0, x0+1, x0, x0+1); FAIL;

   
   GO(__NR_mq_timedreceive, "5s 3m");
   SY(__NR_mq_timedreceive, x0, x0, x0+1, x0+1, x0+1); FAIL;
  
   
   GO(__NR_mq_notify, "2s 1m");
   SY(__NR_mq_notify, x0, x0+1); FAIL;

   
   GO(__NR_mq_getsetattr, "3s 2m");
   SY(__NR_mq_getsetattr, x0, x0+1, x0+1); FAIL;
   
   
   GO(__NR_sys_kexec_load, "ni");
   SY(__NR_sys_kexec_load); FAIL;

   
   GO(__NR_epoll_create1, "1s 0m");
   SY(__NR_epoll_create1, x0); SUCC_OR_FAIL;

   
   GO(__NR_process_vm_readv, "6s 2m");
   SY(__NR_process_vm_readv, x0, x0, x0+1, x0, x0+1, x0); FAIL;

   
   GO(__NR_process_vm_writev, "6s 2m");
   SY(__NR_process_vm_writev, x0, x0, x0+1, x0, x0+1, x0); FAIL;

   
   GO(9999, "1e");
   SY(9999); FAIL;

   
   GO(__NR_exit, "1s 0m");
   SY(__NR_exit, x0); FAIL;

   assert(0);
}

