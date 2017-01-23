

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2006-2013 OpenWorks LLP
      info@open-works.co.uk

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
#include "pub_core_libcassert.h"
#include "pub_core_vki.h"     




#if defined(VGP_ppc32_linux) || defined(VGP_ppc64be_linux) \
    || defined(VGP_ppc64le_linux) || defined(VGP_arm64_linux)
unsigned long VKI_PAGE_SHIFT = 12;
unsigned long VKI_PAGE_SIZE  = 1UL << 12;
#endif


void VG_(vki_do_initial_consistency_checks) ( void )
{
   

   vki_sigset_t set;
   
   vg_assert( 8 * sizeof(set) == _VKI_NSIG );
   
   vg_assert( 8 * sizeof(set.sig[0]) == _VKI_NSIG_BPW );
   
   vg_assert( _VKI_NSIG_BPW == 32 || _VKI_NSIG_BPW == 64 );

   

#  if defined(VGO_linux)
   
#  elif defined(VGP_x86_darwin) || defined(VGP_amd64_darwin)
   vg_assert(_VKI_NSIG == NSIG);
   vg_assert(_VKI_NSIG == 32);
   vg_assert(_VKI_NSIG_WORDS == 1);
   vg_assert(sizeof(sigset_t)  
             == sizeof(vki_sigset_t) );
#  else
#    error "Unknown plat"
#  endif

   

#  if defined(VGO_linux)
   
   vg_assert( sizeof(vki_sigaction_toK_t) 
              == sizeof(vki_sigaction_fromK_t) );
#  elif defined(VGO_darwin)
   vg_assert( sizeof(vki_sigaction_toK_t) 
              == sizeof(vki_sigaction_fromK_t) + sizeof(void*) );

   vg_assert(sizeof(struct sigaction) == sizeof(vki_sigaction_fromK_t));
   vg_assert(sizeof(struct __sigaction) == sizeof(vki_sigaction_toK_t));
   { struct __sigaction    t1;
     vki_sigaction_toK_t   t2;
     struct sigaction      f1;
     vki_sigaction_fromK_t f2;
     vg_assert(sizeof(t1.sa_handler) == sizeof(t2.ksa_handler));
     vg_assert(sizeof(t1.sa_tramp)   == sizeof(t2.sa_tramp));
     vg_assert(sizeof(t1.sa_mask)    == sizeof(t2.sa_mask));
     vg_assert(sizeof(t1.sa_flags)   == sizeof(t2.sa_flags));
     vg_assert(sizeof(f1.sa_handler) == sizeof(f2.ksa_handler));
     vg_assert(sizeof(f1.sa_mask)    == sizeof(f2.sa_mask));
     vg_assert(sizeof(f1.sa_flags)   == sizeof(f2.sa_flags));
#    if 0
     vg_assert(offsetof(t1,sa_handler) == offsetof(t2.ksa_handler));
     vg_assert(offsetof(t1.sa_tramp)   == offsetof(t2.sa_tramp));
     vg_assert(offsetof(t1.sa_mask)    == offsetof(t2.sa_mask));
     vg_assert(offsetof(t1.sa_flags)   == offsetof(t2.sa_flags));
     vg_assert(offsetof(f1.sa_handler) == offsetof(f2.ksa_handler));
     vg_assert(offsetof(f1.sa_mask)    == offsetof(f2.sa_mask));
     vg_assert(offsetof(f1.sa_flags)   == offsetof(f2.sa_flags));
#    endif
   }
   
   vg_assert(VKI_SIG_SETMASK == 3);

#  else
#     error "Unknown OS" 
#  endif
}


