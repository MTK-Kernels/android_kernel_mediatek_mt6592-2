
 
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
#include "pub_core_machine.h"    
#include "pub_core_vki.h"
#include "pub_core_vkiscnums.h"
#include "pub_core_libcbase.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcprint.h"
#include "pub_core_libcproc.h"
#include "pub_core_libcsignal.h"
#include "pub_core_seqmatch.h"
#include "pub_core_mallocfree.h"
#include "pub_core_syscall.h"
#include "pub_core_xarray.h"
#include "pub_core_clientstate.h"

#if defined(VGO_darwin)
#include <mach/mach.h>   
#endif



HChar** VG_(client_envp) = NULL;

const HChar *VG_(libdir) = VG_LIBDIR;

const HChar *VG_(LD_PRELOAD_var_name) =
#if defined(VGO_linux)
   "LD_PRELOAD";
#elif defined(VGO_darwin)
   "DYLD_INSERT_LIBRARIES";
#else
#  error Unknown OS
#endif

HChar *VG_(getenv)(const HChar *varname)
{
   Int i, n;
   vg_assert( VG_(client_envp) );
   n = VG_(strlen)(varname);
   for (i = 0; VG_(client_envp)[i] != NULL; i++) {
      HChar* s = VG_(client_envp)[i];
      if (VG_(strncmp)(varname, s, n) == 0 && s[n] == '=') {
         return & s[n+1];
      }
   }
   return NULL;
}

void  VG_(env_unsetenv) ( HChar **env, const HChar *varname )
{
   HChar **from, **to;
   vg_assert(env);
   vg_assert(varname);
   to = NULL;
   Int len = VG_(strlen)(varname);

   for (from = to = env; from && *from; from++) {
      if (!(VG_(strncmp)(varname, *from, len) == 0 && (*from)[len] == '=')) {
	 *to = *from;
	 to++;
      }
   }
   *to = *from;
}

HChar **VG_(env_setenv) ( HChar ***envp, const HChar* varname,
                          const HChar *val )
{
   HChar **env = (*envp);
   HChar **cpp;
   Int len = VG_(strlen)(varname);
   HChar *valstr = VG_(malloc)("libcproc.es.1", len + VG_(strlen)(val) + 2);
   HChar **oldenv = NULL;

   VG_(sprintf)(valstr, "%s=%s", varname, val);

   for (cpp = env; cpp && *cpp; cpp++) {
      if (VG_(strncmp)(varname, *cpp, len) == 0 && (*cpp)[len] == '=') {
	 *cpp = valstr;
	 return oldenv;
      }
   }

   if (env == NULL) {
      env = VG_(malloc)("libcproc.es.2", sizeof(HChar *) * 2);
      env[0] = valstr;
      env[1] = NULL;

      *envp = env;

   }  else {
      Int envlen = (cpp-env) + 2;
      HChar **newenv = VG_(malloc)("libcproc.es.3", envlen * sizeof(HChar *));

      for (cpp = newenv; *env; )
	 *cpp++ = *env++;
      *cpp++ = valstr;
      *cpp++ = NULL;

      oldenv = *envp;

      *envp = newenv;
   }

   return oldenv;
}


static void mash_colon_env(HChar *varp, const HChar *remove_pattern)
{
   HChar *const start = varp;
   HChar *entry_start = varp;
   HChar *output = varp;

   if (varp == NULL)
      return;

   while(*varp) {
      if (*varp == ':') {
	 HChar prev;
	 Bool match;


	 prev = *output;
	 *output = '\0';

	 match = VG_(string_match)(remove_pattern, entry_start);

	 *output = prev;
	 
	 if (match) {
	    output = entry_start;
	    varp++;			
	 } else
	    entry_start = output+1;	
      }

      if (*varp)
         *output++ = *varp++;
   }

   
   *output = '\0';

   
   if (VG_(string_match)(remove_pattern, entry_start)) {
      output = entry_start;
      if (output > start) {
	 
	 output--;
	 vg_assert(*output == ':');
      }
   }	 

   
   while(output < varp)
      *output++ = '\0';
}


void VG_(env_remove_valgrind_env_stuff)(HChar** envp)
{

#if defined(VGO_darwin)

   
   

#endif

   Int i;
   HChar* ld_preload_str = NULL;
   HChar* ld_library_path_str = NULL;
   HChar* dyld_insert_libraries_str = NULL;
   HChar* buf;

   
   
   
   
   
   for (i = 0; envp[i] != NULL; i++) {
      if (VG_(strncmp)(envp[i], "LD_PRELOAD=", 11) == 0) {
         envp[i] = VG_(strdup)("libcproc.erves.1", envp[i]);
         ld_preload_str = &envp[i][11];
      }
      if (VG_(strncmp)(envp[i], "LD_LIBRARY_PATH=", 16) == 0) {
         envp[i] = VG_(strdup)("libcproc.erves.2", envp[i]);
         ld_library_path_str = &envp[i][16];
      }
      if (VG_(strncmp)(envp[i], "DYLD_INSERT_LIBRARIES=", 22) == 0) {
         envp[i] = VG_(strdup)("libcproc.erves.3", envp[i]);
         dyld_insert_libraries_str = &envp[i][22];
      }
   }

   buf = VG_(malloc)("libcproc.erves.4", VG_(strlen)(VG_(libdir)) + 20);

   
   VG_(sprintf)(buf, "%s*/vgpreload_*.so", VG_(libdir));
   mash_colon_env(ld_preload_str, buf);
   mash_colon_env(dyld_insert_libraries_str, buf);
   VG_(sprintf)(buf, "%s*", VG_(libdir));
   mash_colon_env(ld_library_path_str, buf);

   
   VG_(env_unsetenv)(envp, VALGRIND_LAUNCHER);

   
   VG_(env_unsetenv)(envp, "DYLD_SHARED_REGION");

   

   VG_(free)(buf);
}


Int VG_(waitpid)(Int pid, Int *status, Int options)
{
#  if defined(VGO_linux)
   SysRes res = VG_(do_syscall4)(__NR_wait4,
                                 pid, (UWord)status, options, 0);
   return sr_isError(res) ? -1 : sr_Res(res);
#  elif defined(VGO_darwin)
   SysRes res = VG_(do_syscall4)(__NR_wait4_nocancel,
                                 pid, (UWord)status, options, 0);
   return sr_isError(res) ? -1 : sr_Res(res);
#  else
#    error Unknown OS
#  endif
}

HChar **VG_(env_clone) ( HChar **oldenv )
{
   HChar **oldenvp;
   HChar **newenvp;
   HChar **newenv;
   Int  envlen;

   vg_assert(oldenv);
   for (oldenvp = oldenv; oldenvp && *oldenvp; oldenvp++);

   envlen = oldenvp - oldenv + 1;
   
   newenv = VG_(malloc)("libcproc.ec.1", envlen * sizeof(HChar *));

   oldenvp = oldenv;
   newenvp = newenv;
   
   while (oldenvp && *oldenvp) {
      *newenvp++ = *oldenvp++;
   }
   
   *newenvp = *oldenvp;

   return newenv;
}

void VG_(execv) ( const HChar* filename, HChar** argv )
{
   HChar** envp;
   SysRes res;

   
   VG_(setrlimit)(VKI_RLIMIT_DATA, &VG_(client_rlimit_data));

   envp = VG_(env_clone)(VG_(client_envp));
   VG_(env_remove_valgrind_env_stuff)( envp );

   res = VG_(do_syscall3)(__NR_execve,
                          (UWord)filename, (UWord)argv, (UWord)envp);

   VG_(printf)("EXEC failed, errno = %lld\n", (Long)sr_Err(res));
}

Int VG_(system) ( const HChar* cmd )
{
   Int pid;
   if (cmd == NULL)
      return 1;
   pid = VG_(fork)();
   if (pid < 0)
      return -1;
   if (pid == 0) {
      
      const HChar* argv[4] = { "/bin/sh", "-c", cmd, 0 };
      VG_(execv)(argv[0], CONST_CAST(HChar **,argv));

      
      VG_(exit)(1);
   } else {
      
      Int ir, zzz;
      vki_sigaction_toK_t sa, sa2;
      vki_sigaction_fromK_t saved_sa;
      VG_(memset)( &sa, 0, sizeof(sa) );
      VG_(sigemptyset)(&sa.sa_mask);
      sa.ksa_handler = VKI_SIG_DFL;
      sa.sa_flags    = 0;
      ir = VG_(sigaction)(VKI_SIGCHLD, &sa, &saved_sa);
      vg_assert(ir == 0);

      zzz = VG_(waitpid)(pid, NULL, 0);

      VG_(convert_sigaction_fromK_to_toK)( &saved_sa, &sa2 );
      ir = VG_(sigaction)(VKI_SIGCHLD, &sa2, NULL);
      vg_assert(ir == 0);
      return zzz == -1 ? -1 : 0;
   }
}

Int VG_(sysctl)(Int *name, UInt namelen, void *oldp, SizeT *oldlenp, void *newp, SizeT newlen)
{
   SysRes res;
#  if defined(VGO_darwin)
   res = VG_(do_syscall6)(__NR___sysctl,
                           name, namelen, oldp, oldlenp, newp, newlen);
#  else
   res = VG_(mk_SysRes_Error)(VKI_ENOSYS);
#  endif
   return sr_isError(res) ? -1 : sr_Res(res);
}


Int VG_(getrlimit) (Int resource, struct vki_rlimit *rlim)
{
   SysRes res = VG_(mk_SysRes_Error)(VKI_ENOSYS);
   
#  ifdef __NR_ugetrlimit
   res = VG_(do_syscall2)(__NR_ugetrlimit, resource, (UWord)rlim);
#  endif
   if (sr_isError(res) && sr_Err(res) == VKI_ENOSYS)
      res = VG_(do_syscall2)(__NR_getrlimit, resource, (UWord)rlim);
   return sr_isError(res) ? -1 : sr_Res(res);
}


Int VG_(setrlimit) (Int resource, const struct vki_rlimit *rlim)
{
   SysRes res;
   
   res = VG_(do_syscall2)(__NR_setrlimit, resource, (UWord)rlim);
   return sr_isError(res) ? -1 : sr_Res(res);
}

Int VG_(prctl) (Int option, 
                ULong arg2, ULong arg3, ULong arg4, ULong arg5)
{
   SysRes res = VG_(mk_SysRes_Error)(VKI_ENOSYS);
#  if defined(VGO_linux)
   
   res = VG_(do_syscall5)(__NR_prctl, (UWord) option,
                          (UWord) arg2, (UWord) arg3, (UWord) arg4,
                          (UWord) arg5);
#  endif

   return sr_isError(res) ? -1 : sr_Res(res);
}


Int VG_(gettid)(void)
{
#  if defined(VGO_linux)
   SysRes res = VG_(do_syscall0)(__NR_gettid);

   if (sr_isError(res) && sr_Res(res) == VKI_ENOSYS) {
      HChar pid[16];      

#     if defined(VGP_arm64_linux) || defined(VGP_tilegx_linux)
      res = VG_(do_syscall4)(__NR_readlinkat, VKI_AT_FDCWD,
                             (UWord)"/proc/self",
                             (UWord)pid, sizeof(pid));
#     else
      res = VG_(do_syscall3)(__NR_readlink, (UWord)"/proc/self",
                             (UWord)pid, sizeof(pid));
#     endif
      if (!sr_isError(res) && sr_Res(res) > 0) {
         HChar* s;
         pid[sr_Res(res)] = '\0';
         res = VG_(mk_SysRes_Success)(  VG_(strtoll10)(pid, &s) );
         if (*s != '\0') {
            VG_(message)(Vg_DebugMsg, 
               "Warning: invalid file name linked to by /proc/self: %s\n",
               pid);
         }
      }
   }

   return sr_Res(res);

#  elif defined(VGO_darwin)
   
   
   return mach_thread_self();

#  else
#    error "Unknown OS"
#  endif
}

Int VG_(getpid) ( void )
{
   
   return sr_Res( VG_(do_syscall0)(__NR_getpid) );
}

Int VG_(getpgrp) ( void )
{
   
   return sr_Res( VG_(do_syscall0)(__NR_getpgrp) );
}

Int VG_(getppid) ( void )
{
   
   return sr_Res( VG_(do_syscall0)(__NR_getppid) );
}

Int VG_(geteuid) ( void )
{
   
#  if defined(__NR_geteuid32)
   
   
   return sr_Res( VG_(do_syscall0)(__NR_geteuid32) );
#  else
   return sr_Res( VG_(do_syscall0)(__NR_geteuid) );
#  endif
}

Int VG_(getegid) ( void )
{
   
#  if defined(__NR_getegid32)
   
   
   return sr_Res( VG_(do_syscall0)(__NR_getegid32) );
#  else
   return sr_Res( VG_(do_syscall0)(__NR_getegid) );
#  endif
}

/* Get supplementary groups into list[0 .. size-1].  Returns the
   number of groups written, or -1 if error.  Note that in order to be
   portable, the groups are 32-bit unsigned ints regardless of the
   platform. 
   As a special case, if size == 0 the function returns the number of
   groups leaving list untouched. */
Int VG_(getgroups)( Int size, UInt* list )
{
   if (size < 0) return -1;

#  if defined(VGP_x86_linux) || defined(VGP_ppc32_linux) \
      || defined(VGP_mips64_linux) || defined(VGP_tilegx_linux)
   Int    i;
   SysRes sres;
   UShort list16[size];
   sres = VG_(do_syscall2)(__NR_getgroups, size, (Addr)list16);
   if (sr_isError(sres))
      return -1;
   if (size != 0) {
      for (i = 0; i < sr_Res(sres); i++)
         list[i] = (UInt)list16[i];
   }
   return sr_Res(sres);

#  elif defined(VGP_amd64_linux) || defined(VGP_arm_linux) \
        || defined(VGP_ppc64be_linux) || defined(VGP_ppc64le_linux)  \
        || defined(VGO_darwin) || defined(VGP_s390x_linux)    \
        || defined(VGP_mips32_linux) || defined(VGP_arm64_linux)
   SysRes sres;
   sres = VG_(do_syscall2)(__NR_getgroups, size, (Addr)list);
   if (sr_isError(sres))
      return -1;
   return sr_Res(sres);

#  else
#     error "VG_(getgroups): needs implementation on this platform"
#  endif
}


Int VG_(ptrace) ( Int request, Int pid, void *addr, void *data )
{
   SysRes res;
   res = VG_(do_syscall4)(__NR_ptrace, request, pid, (UWord)addr, (UWord)data);
   if (sr_isError(res))
      return -1;
   return sr_Res(res);
}


Int VG_(fork) ( void )
{
#  if defined(VGP_arm64_linux) || defined(VGP_tilegx_linux)
   SysRes res;
   res = VG_(do_syscall5)(__NR_clone, VKI_SIGCHLD,
                          (UWord)NULL, (UWord)NULL, (UWord)NULL, (UWord)NULL);
   if (sr_isError(res))
      return -1;
   return sr_Res(res);

#  elif defined(VGO_linux)
   SysRes res;
   res = VG_(do_syscall0)(__NR_fork);
   if (sr_isError(res))
      return -1;
   return sr_Res(res);

#  elif defined(VGO_darwin)
   SysRes res;
   res = VG_(do_syscall0)(__NR_fork); 
   if (sr_isError(res))
      return -1;
   
   if (sr_ResHI(res) != 0) {
      return 0;  
   }
   return sr_Res(res);

#  else
#    error "Unknown OS"
#  endif
}


UInt VG_(read_millisecond_timer) ( void )
{
   
   static ULong base = 0;
   ULong  now;

#  if defined(VGO_linux)
   { SysRes res;
     struct vki_timespec ts_now;
     res = VG_(do_syscall2)(__NR_clock_gettime, VKI_CLOCK_MONOTONIC,
                            (UWord)&ts_now);
     if (sr_isError(res) == 0) {
        now = ts_now.tv_sec * 1000000ULL + ts_now.tv_nsec / 1000;
     } else {
       struct vki_timeval tv_now;
       res = VG_(do_syscall2)(__NR_gettimeofday, (UWord)&tv_now, (UWord)NULL);
       vg_assert(! sr_isError(res));
       now = tv_now.tv_sec * 1000000ULL + tv_now.tv_usec;
     }
   }

#  elif defined(VGO_darwin)
   
   
   
   
   { SysRes res;
     struct vki_timeval tv_now = { 0, 0 };
     res = VG_(do_syscall2)(__NR_gettimeofday, (UWord)&tv_now, (UWord)NULL);
     vg_assert(! sr_isError(res));
     now = sr_Res(res) * 1000000ULL + sr_ResHI(res);
   }

#  else
#    error "Unknown OS"
#  endif

     
   if (base == 0)
      base = now;

   return (now - base) / 1000;
}

Int VG_(gettimeofday)(struct vki_timeval *tv, struct vki_timezone *tz)
{
   SysRes res;
   res = VG_(do_syscall2)(__NR_gettimeofday, (UWord)tv, (UWord)tz);

   if (! sr_isError(res)) return 0;

   
   if (tv) *tv = (struct vki_timeval){ .tv_sec = 0, .tv_usec = 0 };
   if (tz) *tz = (struct vki_timezone){ .tz_minuteswest = 0, .tz_dsttime = 0 };

   return -1;
}



struct atfork {
   vg_atfork_t  pre;
   vg_atfork_t  parent;
   vg_atfork_t  child;
};

#define VG_MAX_ATFORK 10

static struct atfork atforks[VG_MAX_ATFORK];
static Int n_atfork = 0;

void VG_(atfork)(vg_atfork_t pre, vg_atfork_t parent, vg_atfork_t child)
{
   Int i;

   for (i = 0; i < n_atfork; i++) {
      if (atforks[i].pre == pre &&
          atforks[i].parent == parent &&
          atforks[i].child == child)
         return;
   }

   if (n_atfork >= VG_MAX_ATFORK)
      VG_(core_panic)(
         "Too many VG_(atfork) handlers requested: raise VG_MAX_ATFORK");

   atforks[n_atfork].pre    = pre;
   atforks[n_atfork].parent = parent;
   atforks[n_atfork].child  = child;

   n_atfork++;
}

void VG_(do_atfork_pre)(ThreadId tid)
{
   Int i;

   for (i = 0; i < n_atfork; i++)
      if (atforks[i].pre != NULL)
         (*atforks[i].pre)(tid);
}

void VG_(do_atfork_parent)(ThreadId tid)
{
   Int i;

   for (i = 0; i < n_atfork; i++)
      if (atforks[i].parent != NULL)
         (*atforks[i].parent)(tid);
}

void VG_(do_atfork_child)(ThreadId tid)
{
   Int i;

   for (i = 0; i < n_atfork; i++)
      if (atforks[i].child != NULL)
         (*atforks[i].child)(tid);
}



void VG_(invalidate_icache) ( void *ptr, SizeT nbytes )
{
   if (nbytes == 0) return;    

   
   VexArchInfo vai;
   VG_(machine_get_VexArchInfo)(NULL, &vai);

   
   if (vai.hwcache_info.icaches_maintain_coherence) return;

#  if defined(VGA_ppc32) || defined(VGA_ppc64be) || defined(VGA_ppc64le)
   Addr startaddr = (Addr) ptr;
   Addr endaddr   = startaddr + nbytes;
   Addr cls;
   Addr addr;

   cls = vai.ppc_icache_line_szB;

   
   vg_assert(cls == 16 || cls == 32 || cls == 64 || cls == 128);

   startaddr &= ~(cls - 1);
   for (addr = startaddr; addr < endaddr; addr += cls) {
      __asm__ __volatile__("dcbst 0,%0" : : "r" (addr));
   }
   __asm__ __volatile__("sync");
   for (addr = startaddr; addr < endaddr; addr += cls) {
      __asm__ __volatile__("icbi 0,%0" : : "r" (addr));
   }
   __asm__ __volatile__("sync; isync");

#  elif defined(VGP_arm_linux)
   
   Addr startaddr = (Addr) ptr;
   Addr endaddr   = startaddr + nbytes;
   VG_(do_syscall2)(__NR_ARM_cacheflush, startaddr, endaddr);

#  elif defined(VGP_arm64_linux)
   
   
   
   // which has the following copyright notice:
   /*
   Copyright 2013, ARM Limited
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:
   
   * Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   * Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
   * Neither the name of ARM Limited nor the names of its contributors may be
     used to endorse or promote products derived from this software without
     specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   */

   
   UInt cache_type_register;
   
   __asm__ __volatile__ ("mrs %[ctr], ctr_el0" 
                         : [ctr] "=r" (cache_type_register));

   const Int kDCacheLineSizeShift = 16;
   const Int kICacheLineSizeShift = 0;
   const UInt kDCacheLineSizeMask = 0xf << kDCacheLineSizeShift;
   const UInt kICacheLineSizeMask = 0xf << kICacheLineSizeShift;

   
   
   const UInt dcache_line_size_power_of_two =
       (cache_type_register & kDCacheLineSizeMask) >> kDCacheLineSizeShift;
   const UInt icache_line_size_power_of_two =
       (cache_type_register & kICacheLineSizeMask) >> kICacheLineSizeShift;

   const UInt dcache_line_size_ = 4 * (1 << dcache_line_size_power_of_two);
   const UInt icache_line_size_ = 4 * (1 << icache_line_size_power_of_two);

   Addr start = (Addr)ptr;
   
   Addr dsize = (Addr)dcache_line_size_;
   Addr isize = (Addr)icache_line_size_;

   
   Addr dstart = start & ~(dsize - 1);
   Addr istart = start & ~(isize - 1);
   Addr end    = start + nbytes;

   __asm__ __volatile__ (
     
     "0: \n\t"
     
     
     
     
     
     
     
     "dc cvau, %[dline] \n\t"
     "add %[dline], %[dline], %[dsize] \n\t"
     "cmp %[dline], %[end] \n\t"
     "b.lt 0b \n\t"
     
     
     
     
     
     
     
     
     
     "dsb ish \n\t"
     
     "1: \n\t"
     
     
     
     
     "ic ivau, %[iline] \n\t"
     "add %[iline], %[iline], %[isize] \n\t"
     "cmp %[iline], %[end] \n\t"
     "b.lt 1b \n\t"
     
     
     "dsb ish \n\t"
     
     
     
     "isb \n\t"
     : [dline] "+r" (dstart),
       [iline] "+r" (istart)
     : [dsize] "r" (dsize),
       [isize] "r" (isize),
       [end] "r" (end)
     
     
     : "cc", "memory"
   );

#  elif defined(VGA_mips32) || defined(VGA_mips64)
   SysRes sres = VG_(do_syscall3)(__NR_cacheflush, (UWord) ptr,
                                 (UWord) nbytes, (UWord) 3);
   vg_assert( sres._isError == 0 );

#  elif defined(VGA_tilegx)
   const HChar *start, *end;

   
   if (nbytes > 0x8000) nbytes = 0x8000;

   start = (const HChar *)((unsigned long)ptr & -64ULL);
   end = (const HChar*)ptr + nbytes - 1;

   __insn_mf();

   do {
     const HChar* p;
     for (p = start; p <= end; p += 64)
       __insn_icoh(p);
   } while (0);

   __insn_drain();

#  endif
}



void VG_(flush_dcache) ( void *ptr, SizeT nbytes )
{
   
#  if defined(VGA_arm64)
   Addr startaddr = (Addr) ptr;
   Addr endaddr   = startaddr + nbytes;
   Addr cls;
   Addr addr;

   ULong ctr_el0;
   __asm__ __volatile__ ("mrs %0, ctr_el0" : "=r"(ctr_el0));
   cls = 4 * (1ULL << (0xF & (ctr_el0 >> 16)));

   
   vg_assert(cls == 64);

   startaddr &= ~(cls - 1);
   for (addr = startaddr; addr < endaddr; addr += cls) {
      __asm__ __volatile__("dc cvau, %0" : : "r" (addr));
   }
   __asm__ __volatile__("dsb ish");
#  endif
}

