

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2005-2013 Apple Inc.
      Greg Parker  gparker@apple.com

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

#ifndef __PRIV_SYSWRAP_DARWIN_H
#define __PRIV_SYSWRAP_DARWIN_H

#include "pub_core_basics.h"         
#include "priv_types_n_macros.h"     

Addr allocstack ( ThreadId tid );
void find_stack_segment ( ThreadId tid, Addr sp );
void start_thread_NORETURN ( Word arg );
void assign_port_name(mach_port_t port, const char *name);
void record_named_port(ThreadId tid, mach_port_t port, mach_port_right_t right, const char *name);

extern const SyscallTableEntry ML_(mach_trap_table)[];
extern const SyscallTableEntry ML_(syscall_table)[];
extern const SyscallTableEntry ML_(mdep_trap_table)[];

extern const UInt ML_(syscall_table_size);
extern const UInt ML_(mach_trap_table_size);
extern const UInt ML_(mdep_trap_table_size);

void VG_(show_open_ports)(void);

Bool ML_(sync_mappings)(const HChar *when, const HChar *where, UWord num);

DECL_TEMPLATE(darwin, exit);                    
DECL_TEMPLATE(darwin, getfsstat);               
DECL_TEMPLATE(darwin, ptrace);                  
DECL_TEMPLATE(darwin, recvmsg);                 
DECL_TEMPLATE(darwin, sendmsg);                 
DECL_TEMPLATE(darwin, recvfrom);                
DECL_TEMPLATE(darwin, accept);                  
DECL_TEMPLATE(darwin, getpeername);             
DECL_TEMPLATE(darwin, getsockname);             
DECL_TEMPLATE(darwin, chflags);                 
DECL_TEMPLATE(darwin, fchflags);                
DECL_TEMPLATE(darwin, pipe);                    
DECL_TEMPLATE(darwin, sigaction);               
DECL_TEMPLATE(darwin, sigprocmask);             
DECL_TEMPLATE(darwin, getlogin);                
DECL_TEMPLATE(darwin, sigpending);              
DECL_TEMPLATE(darwin, ioctl);                   
DECL_TEMPLATE(darwin, getdtablesize);           
DECL_TEMPLATE(darwin, fcntl);                   
DECL_TEMPLATE(darwin, socket);                  
DECL_TEMPLATE(darwin, connect);                 
DECL_TEMPLATE(darwin, bind);                    
DECL_TEMPLATE(darwin, setsockopt);              
DECL_TEMPLATE(darwin, listen);                  
DECL_TEMPLATE(darwin, sigsuspend);              
DECL_TEMPLATE(darwin, getsockopt);              
DECL_TEMPLATE(darwin, mkfifo);                  
DECL_TEMPLATE(darwin, sendto);                  
DECL_TEMPLATE(darwin, shutdown);                
DECL_TEMPLATE(darwin, socketpair);              
DECL_TEMPLATE(darwin, futimes);                 
DECL_TEMPLATE(darwin, gethostuuid);             
DECL_TEMPLATE(darwin, mount);                   
DECL_TEMPLATE(darwin, csops);                   
DECL_TEMPLATE(darwin, kdebug_trace);            
DECL_TEMPLATE(darwin, setegid);                 
DECL_TEMPLATE(darwin, seteuid);                 
DECL_TEMPLATE(darwin, sigreturn);               
DECL_TEMPLATE(darwin, FAKE_SIGRETURN);
DECL_TEMPLATE(darwin, pathconf);            
DECL_TEMPLATE(darwin, fpathconf);           
DECL_TEMPLATE(darwin, getdirentries);       
DECL_TEMPLATE(darwin, mmap);                
DECL_TEMPLATE(darwin, lseek);               
DECL_TEMPLATE(darwin, __sysctl);                
DECL_TEMPLATE(darwin, getattrlist);             
DECL_TEMPLATE(darwin, setattrlist);             
DECL_TEMPLATE(darwin, getdirentriesattr);       
DECL_TEMPLATE(darwin, exchangedata);            
DECL_TEMPLATE(darwin, watchevent);              
DECL_TEMPLATE(darwin, waitevent);               
DECL_TEMPLATE(darwin, modwatch);                
DECL_TEMPLATE(darwin, getxattr);                
DECL_TEMPLATE(darwin, fgetxattr);               
DECL_TEMPLATE(darwin, setxattr);                
DECL_TEMPLATE(darwin, fsetxattr);               
DECL_TEMPLATE(darwin, removexattr);             
DECL_TEMPLATE(darwin, fremovexattr);            
DECL_TEMPLATE(darwin, listxattr);               
DECL_TEMPLATE(darwin, flistxattr);              
DECL_TEMPLATE(darwin, fsctl);                   
DECL_TEMPLATE(darwin, initgroups);              
DECL_TEMPLATE(darwin, posix_spawn);             
DECL_TEMPLATE(darwin, semctl);                  
DECL_TEMPLATE(darwin, semget);                  
DECL_TEMPLATE(darwin, semop);                   
DECL_TEMPLATE(darwin, shmat);                   
DECL_TEMPLATE(darwin, shmctl);                  
DECL_TEMPLATE(darwin, shmdt);                   
DECL_TEMPLATE(darwin, shmget);                  
DECL_TEMPLATE(darwin, shm_open);                
DECL_TEMPLATE(darwin, shm_unlink);              
DECL_TEMPLATE(darwin, sem_open);                
DECL_TEMPLATE(darwin, sem_close);               
DECL_TEMPLATE(darwin, sem_unlink);              
DECL_TEMPLATE(darwin, sem_wait);                
DECL_TEMPLATE(darwin, sem_trywait);             
DECL_TEMPLATE(darwin, sem_post);                

#if DARWIN_VERS < DARWIN_10_10
#elif DARWIN_VERS == DARWIN_10_10
DECL_TEMPLATE(darwin, sysctlbyname);            
#endif

DECL_TEMPLATE(darwin, sem_init);                
DECL_TEMPLATE(darwin, sem_destroy);             
DECL_TEMPLATE(darwin, open_extended)            
DECL_TEMPLATE(darwin, stat_extended);           
DECL_TEMPLATE(darwin, lstat_extended);          
DECL_TEMPLATE(darwin, fstat_extended);          
DECL_TEMPLATE(darwin, chmod_extended);          
DECL_TEMPLATE(darwin, fchmod_extended);         
DECL_TEMPLATE(darwin, access_extended);         
DECL_TEMPLATE(darwin, settid);                  
#if DARWIN_VERS >= DARWIN_10_7
DECL_TEMPLATE(darwin, gettid);                  
#endif
DECL_TEMPLATE(darwin, psynch_mutexwait);       
DECL_TEMPLATE(darwin, psynch_mutexdrop);       
DECL_TEMPLATE(darwin, psynch_cvbroad);         
DECL_TEMPLATE(darwin, psynch_cvsignal);        
DECL_TEMPLATE(darwin, psynch_cvwait);          
DECL_TEMPLATE(darwin, psynch_rw_rdlock);       
DECL_TEMPLATE(darwin, psynch_rw_wrlock);       
DECL_TEMPLATE(darwin, psynch_rw_unlock);       
DECL_TEMPLATE(darwin, psynch_cvclrprepost);    
DECL_TEMPLATE(darwin, aio_return);             
DECL_TEMPLATE(darwin, aio_suspend);            
DECL_TEMPLATE(darwin, aio_error);              
DECL_TEMPLATE(darwin, aio_read);               
DECL_TEMPLATE(darwin, aio_write);              
DECL_TEMPLATE(darwin, issetugid);               
DECL_TEMPLATE(darwin, __pthread_kill);          
DECL_TEMPLATE(darwin, __pthread_sigmask);       
DECL_TEMPLATE(darwin, __disable_threadsignal);  
DECL_TEMPLATE(darwin, __pthread_markcancel);    
DECL_TEMPLATE(darwin, __pthread_canceled);      
DECL_TEMPLATE(darwin, __semwait_signal);        
#if DARWIN_VERS >= DARWIN_10_6
DECL_TEMPLATE(darwin, proc_info);               
#endif
DECL_TEMPLATE(darwin, sendfile);                
DECL_TEMPLATE(darwin, stat64);                  
DECL_TEMPLATE(darwin, fstat64);                 
DECL_TEMPLATE(darwin, lstat64);                 
DECL_TEMPLATE(darwin, stat64_extended);         
DECL_TEMPLATE(darwin, lstat64_extended);        
DECL_TEMPLATE(darwin, fstat64_extended);        
DECL_TEMPLATE(darwin, getdirentries64);         
DECL_TEMPLATE(darwin, statfs64);                
DECL_TEMPLATE(darwin, fstatfs64);               
DECL_TEMPLATE(darwin, getfsstat64);             
DECL_TEMPLATE(darwin, __pthread_chdir);         
DECL_TEMPLATE(darwin, __pthread_fchdir);        
DECL_TEMPLATE(darwin, auditon);                 
#if DARWIN_VERS >= DARWIN_10_7
DECL_TEMPLATE(darwin, getaudit_addr)            
#endif
DECL_TEMPLATE(darwin, bsdthread_create);        
DECL_TEMPLATE(darwin, bsdthread_terminate);     
DECL_TEMPLATE(darwin, kqueue);                  
DECL_TEMPLATE(darwin, kevent);                  
DECL_TEMPLATE(darwin, bsdthread_register);      
DECL_TEMPLATE(darwin, workq_open);              
DECL_TEMPLATE(darwin, workq_ops);               
DECL_TEMPLATE(darwin, kevent64);                
DECL_TEMPLATE(darwin, __thread_selfid);         
DECL_TEMPLATE(darwin, __mac_syscall);           
DECL_TEMPLATE(darwin, fsgetpath);                
DECL_TEMPLATE(darwin, audit_session_self);       
DECL_TEMPLATE(darwin, fileport_makeport);        

#if DARWIN_VERS == DARWIN_10_10
#endif 
DECL_TEMPLATE(darwin, guarded_open_np);          
DECL_TEMPLATE(darwin, guarded_close_np);         
DECL_TEMPLATE(darwin, guarded_kqueue_np);        
DECL_TEMPLATE(darwin, change_fdguard_np);        
DECL_TEMPLATE(darwin, connectx);                 
DECL_TEMPLATE(darwin, disconnectx);              
#if DARWIN_VERS == DARWIN_10_10
DECL_TEMPLATE(darwin, necp_match_policy);        
DECL_TEMPLATE(darwin, getattrlistbulk);          
DECL_TEMPLATE(darwin, readlinkat);               
DECL_TEMPLATE(darwin, bsdthread_ctl);            
DECL_TEMPLATE(darwin, guarded_open_dprotected_np);  
DECL_TEMPLATE(darwin, guarded_write_np);            
DECL_TEMPLATE(darwin, guarded_pwrite_np);           
DECL_TEMPLATE(darwin, guarded_writev_np);           
#endif 

DECL_TEMPLATE(darwin, mach_port_set_context);
DECL_TEMPLATE(darwin, host_info);
DECL_TEMPLATE(darwin, host_page_size);
DECL_TEMPLATE(darwin, host_get_io_master);
DECL_TEMPLATE(darwin, host_get_clock_service);
DECL_TEMPLATE(darwin, host_request_notification);
DECL_TEMPLATE(darwin, mach_port_type);
DECL_TEMPLATE(darwin, mach_port_extract_member);
DECL_TEMPLATE(darwin, mach_port_allocate);
DECL_TEMPLATE(darwin, mach_port_deallocate);
DECL_TEMPLATE(darwin, mach_port_get_refs);
DECL_TEMPLATE(darwin, mach_port_mod_refs);
DECL_TEMPLATE(darwin, mach_port_get_set_status);
DECL_TEMPLATE(darwin, mach_port_move_member);
DECL_TEMPLATE(darwin, mach_port_destroy);
DECL_TEMPLATE(darwin, mach_port_request_notification);
DECL_TEMPLATE(darwin, mach_port_insert_right);
DECL_TEMPLATE(darwin, mach_port_extract_right);
DECL_TEMPLATE(darwin, mach_port_get_attributes);
DECL_TEMPLATE(darwin, mach_port_set_attributes);
DECL_TEMPLATE(darwin, mach_port_insert_member);
DECL_TEMPLATE(darwin, task_get_special_port);
DECL_TEMPLATE(darwin, task_get_exception_ports);
DECL_TEMPLATE(darwin, semaphore_create);
DECL_TEMPLATE(darwin, semaphore_destroy);
DECL_TEMPLATE(darwin, task_policy_set);
DECL_TEMPLATE(darwin, mach_ports_register);
DECL_TEMPLATE(darwin, mach_ports_lookup);
DECL_TEMPLATE(darwin, task_info);
DECL_TEMPLATE(darwin, task_threads);
DECL_TEMPLATE(darwin, task_suspend);
DECL_TEMPLATE(darwin, task_resume);
DECL_TEMPLATE(darwin, vm_allocate);
DECL_TEMPLATE(darwin, vm_deallocate);
DECL_TEMPLATE(darwin, vm_protect);
DECL_TEMPLATE(darwin, vm_inherit);
DECL_TEMPLATE(darwin, vm_read);
DECL_TEMPLATE(darwin, mach_vm_read);
DECL_TEMPLATE(darwin, vm_copy);
DECL_TEMPLATE(darwin, vm_read_overwrite);
DECL_TEMPLATE(darwin, vm_map);
DECL_TEMPLATE(darwin, vm_remap);
DECL_TEMPLATE(darwin, mach_make_memory_entry_64);
DECL_TEMPLATE(darwin, vm_purgable_control);
DECL_TEMPLATE(darwin, mach_vm_purgable_control);
DECL_TEMPLATE(darwin, mach_vm_allocate);
DECL_TEMPLATE(darwin, mach_vm_deallocate);
DECL_TEMPLATE(darwin, mach_vm_protect);
DECL_TEMPLATE(darwin, mach_vm_copy);
DECL_TEMPLATE(darwin, mach_vm_read_overwrite);
DECL_TEMPLATE(darwin, mach_vm_inherit);
DECL_TEMPLATE(darwin, mach_vm_map);
DECL_TEMPLATE(darwin, mach_vm_remap);
DECL_TEMPLATE(darwin, mach_vm_region_recurse);
DECL_TEMPLATE(darwin, thread_terminate);
DECL_TEMPLATE(darwin, thread_create);
DECL_TEMPLATE(darwin, thread_create_running);
DECL_TEMPLATE(darwin, thread_suspend);
DECL_TEMPLATE(darwin, thread_resume);
DECL_TEMPLATE(darwin, thread_get_state);
DECL_TEMPLATE(darwin, thread_policy);
DECL_TEMPLATE(darwin, thread_policy_set);
DECL_TEMPLATE(darwin, thread_info);
DECL_TEMPLATE(darwin, bootstrap_register);
DECL_TEMPLATE(darwin, bootstrap_look_up);
DECL_TEMPLATE(darwin, mach_msg_receive);
DECL_TEMPLATE(darwin, mach_msg_bootstrap);
DECL_TEMPLATE(darwin, mach_msg_host);
DECL_TEMPLATE(darwin, mach_msg_task);
DECL_TEMPLATE(darwin, mach_msg_thread);

#if DARWIN_VERS >= DARWIN_10_8
DECL_TEMPLATE(darwin, kernelrpc_mach_vm_allocate_trap);
DECL_TEMPLATE(darwin, kernelrpc_mach_vm_deallocate_trap);
DECL_TEMPLATE(darwin, kernelrpc_mach_vm_protect_trap);
DECL_TEMPLATE(darwin, kernelrpc_mach_vm_map_trap);
DECL_TEMPLATE(darwin, kernelrpc_mach_port_allocate_trap);
DECL_TEMPLATE(darwin, kernelrpc_mach_port_destroy_trap);
DECL_TEMPLATE(darwin, kernelrpc_mach_port_deallocate_trap);
DECL_TEMPLATE(darwin, kernelrpc_mach_port_mod_refs_trap);
DECL_TEMPLATE(darwin, kernelrpc_mach_port_move_member_trap);
DECL_TEMPLATE(darwin, kernelrpc_mach_port_insert_right_trap);
DECL_TEMPLATE(darwin, kernelrpc_mach_port_insert_member_trap);
DECL_TEMPLATE(darwin, kernelrpc_mach_port_extract_member_trap);
DECL_TEMPLATE(darwin, kernelrpc_mach_port_construct_trap);
DECL_TEMPLATE(darwin, kernelrpc_mach_port_destruct_trap);
DECL_TEMPLATE(darwin, kernelrpc_mach_port_guard_trap);
DECL_TEMPLATE(darwin, kernelrpc_mach_port_unguard_trap);
DECL_TEMPLATE(darwin, iopolicysys);
DECL_TEMPLATE(darwin, process_policy);
#endif 
DECL_TEMPLATE(darwin, mach_msg_unhandled);
DECL_TEMPLATE(darwin, mach_msg_unhandled_check);
DECL_TEMPLATE(darwin, mach_msg);
DECL_TEMPLATE(darwin, mach_reply_port);
DECL_TEMPLATE(darwin, mach_thread_self);
DECL_TEMPLATE(darwin, mach_host_self);
DECL_TEMPLATE(darwin, mach_task_self);
DECL_TEMPLATE(darwin, syscall_thread_switch);
DECL_TEMPLATE(darwin, semaphore_signal);
DECL_TEMPLATE(darwin, semaphore_signal_all);
DECL_TEMPLATE(darwin, semaphore_signal_thread);
DECL_TEMPLATE(darwin, semaphore_wait);
DECL_TEMPLATE(darwin, semaphore_wait_signal);
DECL_TEMPLATE(darwin, semaphore_timedwait);
DECL_TEMPLATE(darwin, semaphore_timedwait_signal);
DECL_TEMPLATE(darwin, task_for_pid);
DECL_TEMPLATE(darwin, pid_for_task);
DECL_TEMPLATE(darwin, mach_timebase_info);
DECL_TEMPLATE(darwin, mach_wait_until);
DECL_TEMPLATE(darwin, mk_timer_create);
DECL_TEMPLATE(darwin, mk_timer_destroy);
DECL_TEMPLATE(darwin, mk_timer_arm);
DECL_TEMPLATE(darwin, mk_timer_cancel);
DECL_TEMPLATE(darwin, iokit_user_client_trap);
DECL_TEMPLATE(darwin, swtch);
DECL_TEMPLATE(darwin, swtch_pri);

DECL_TEMPLATE(darwin, thread_fast_set_cthread_self);

#include <mach/mach.h>
extern 
void thread_state_from_vex(thread_state_t mach_generic, 
                           thread_state_flavor_t flavor, 
                           mach_msg_type_number_t count, 
                           VexGuestArchState *vex_generic);
extern
void thread_state_to_vex(const thread_state_t mach_generic, 
                         thread_state_flavor_t flavor, 
                         mach_msg_type_number_t count, 
                         VexGuestArchState *vex_generic);
extern 
ThreadState *build_thread(const thread_state_t state, 
                          thread_state_flavor_t flavor, 
                          mach_msg_type_number_t count);
extern
void hijack_thread_state(thread_state_t mach_generic, 
                         thread_state_flavor_t flavor, 
                         mach_msg_type_number_t count, 
                         ThreadState *tst);
extern
__attribute__((noreturn))
void call_on_new_stack_0_1 ( Addr stack,
			     Addr retaddr,
			     void (*f)(Word),
                             Word arg1 );

extern void pthread_hijack_asm(void);
extern void pthread_hijack(Addr self, Addr kport, Addr func, Addr func_arg, 
                           Addr stacksize, Addr flags, Addr sp);
extern void wqthread_hijack_asm(void);
extern void wqthread_hijack(Addr self, Addr kport, Addr stackaddr, Addr workitem, Int reuse, Addr sp);

extern Addr pthread_starter;
extern Addr wqthread_starter;
extern SizeT pthread_structsize;


#endif

