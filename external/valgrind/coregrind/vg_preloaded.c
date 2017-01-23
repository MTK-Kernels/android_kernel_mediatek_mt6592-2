

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
#include "pub_core_clreq.h"
#include "pub_core_debuginfo.h"  
#include "pub_core_redir.h"      

#if defined(VGO_linux)


void VG_NOTIFY_ON_LOAD(freeres)( void );
void VG_NOTIFY_ON_LOAD(freeres)( void )
{
#  if !defined(__UCLIBC__) \
      && !defined(__ANDROID__)
   extern void __libc_freeres(void);
   __libc_freeres();
#  endif
   VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__LIBC_FREERES_DONE, 
                                   0, 0, 0, 0, 0);
   
   *(volatile int *)0 = 'x';
}


void * VG_NOTIFY_ON_LOAD(ifunc_wrapper) (void);
void * VG_NOTIFY_ON_LOAD(ifunc_wrapper) (void)
{
    OrigFn fn;
    Addr result = 0;
    Addr fnentry;

    
    VALGRIND_GET_ORIG_FN(fn);
    CALL_FN_W_v(result, fn);

#if defined(VGP_ppc64be_linux)
    fnentry = *(Addr*)result;
#else
    fnentry = result;
#endif

    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__ADD_IFUNC_TARGET,
                                    fn.nraddr, fnentry, 0, 0, 0);
    return (void*)result;
}

#elif defined(VGO_darwin)

#include "config.h" 


__private_extern__ const char *__crashreporter_info__ = "Instrumented by Valgrind " VERSION;


#include <string.h>
#include <crt_externs.h>

static void env_unsetenv ( HChar **env, const HChar *varname )
{
   HChar **from;
   HChar **to = NULL;
   Int len = strlen(varname);

   for (from = to = env; from && *from; from++) {
      if (!(strncmp(varname, *from, len) == 0 && (*from)[len] == '=')) {
	 *to = *from;
	 to++;
      }
   }
   *(to++) = *(from++);
   
   *(to++) = *(from++);
   *to = NULL;
}

static void vg_cleanup_env(void)  __attribute__((constructor));
static void vg_cleanup_env(void)
{
    HChar **envp = (HChar**)*_NSGetEnviron();
    env_unsetenv(envp, "VALGRIND_LAUNCHER");
    env_unsetenv(envp, "DYLD_SHARED_REGION");
    
    env_unsetenv(envp, "DYLD_INSERT_LIBRARIES");
}   


#include <fcntl.h>
#include <unistd.h>

int VG_REPLACE_FUNCTION_ZU(libSystemZdZaZddylib, arc4random)(void);
int VG_REPLACE_FUNCTION_ZU(libSystemZdZaZddylib, arc4random)(void)
{
    static int rnd = -1;
    int result;

    if (rnd < 0) rnd = open("/dev/random", O_RDONLY);

    read(rnd, &result, sizeof(result));
    return result;
}

void VG_REPLACE_FUNCTION_ZU(libSystemZdZaZddylib, arc4random_stir)(void);
void VG_REPLACE_FUNCTION_ZU(libSystemZdZaZddylib, arc4random_stir)(void)
{
    
}

void VG_REPLACE_FUNCTION_ZU(libSystemZdZaZddylib, arc4random_addrandom)(unsigned char *dat, int datlen);
void VG_REPLACE_FUNCTION_ZU(libSystemZdZaZddylib, arc4random_addrandom)(unsigned char *dat, int datlen)
{
    
    
    
}

#else

#  error Unknown OS
#endif

