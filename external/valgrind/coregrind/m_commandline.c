

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
#include "pub_core_vki.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcbase.h"
#include "pub_core_libcfile.h"
#include "pub_core_libcprint.h"
#include "pub_core_libcproc.h"
#include "pub_core_mallocfree.h"
#include "pub_core_xarray.h"
#include "pub_core_clientstate.h"
#include "pub_core_commandline.h" 



static void add_string ( XArray* xa, HChar* str )
{
   (void) VG_(addToXA)( xa, (void*)(&str) );
}



static HChar* read_dot_valgrindrc ( const HChar* dir )
{
   Int    n;
   SysRes fd;
   struct vg_stat stat_buf;
   HChar* f_clo = NULL;
   const  HChar dot_valgrindrc[] = ".valgrindrc";

   vg_assert(dir != NULL);

   HChar filename[VG_(strlen)(dir) + 1 + VG_(strlen)(dot_valgrindrc) + 1];
   VG_(sprintf)(filename, "%s/%s", dir, dot_valgrindrc);

   fd = VG_(open)(filename, 0, VKI_S_IRUSR);
   if ( !sr_isError(fd) ) {
      Int res = VG_(fstat)( sr_Res(fd), &stat_buf );
      
      if (!res && stat_buf.uid == VG_(geteuid)()
          && (!(stat_buf.mode & VKI_S_IWOTH))) {
         if ( stat_buf.size > 0 ) {
            f_clo = VG_(malloc)("commandline.rdv.1", stat_buf.size+1);
            n = VG_(read)(sr_Res(fd), f_clo, stat_buf.size);
            if (n == -1) n = 0;
            vg_assert(n >= 0 && n <= stat_buf.size+1);
            f_clo[n] = '\0';
         }
      }
      else
         VG_(message)(Vg_UserMsg,
               "%s was not read as it is either world writeable or not "
               "owned by the current user\n", filename);

      VG_(close)(sr_Res(fd));
   }
   return f_clo;
}



static void add_args_from_string ( HChar* s )
{
   HChar* tmp;
   HChar* cp = s;
   vg_assert(cp);
   while (True) {
      
      
      while (VG_(isspace)(*cp)) cp++;
      if (*cp == 0) break;
      tmp = cp;
      while ( !VG_(isspace)(*cp) && *cp != 0 ) cp++;
      if ( *cp != 0 ) *cp++ = '\0';       
      add_string( VG_(args_for_valgrind), tmp );
   }
}



void VG_(split_up_argv)( Int argc, HChar** argv )
{
          Int  i;
          Bool augment = True;
   static Bool already_called = False;

   XArray*  tmp_xarray;

   vg_assert(!already_called);
   already_called = True;

   tmp_xarray = VG_(newXA)( VG_(malloc), "commandline.sua.1",
                            VG_(free), sizeof(HChar*) );

   vg_assert( ! VG_(args_for_valgrind) );
   VG_(args_for_valgrind)
      = VG_(newXA)( VG_(malloc), "commandline.sua.2",
                    VG_(free), sizeof(HChar*) );

   vg_assert( ! VG_(args_for_client) );
   VG_(args_for_client)
      = VG_(newXA)( VG_(malloc), "commandline.sua.3",
                    VG_(free), sizeof(HChar*) );

   
   i = 1; 
   for (; i < argc; i++) {
      vg_assert(argv[i]);
      if (0 == VG_(strcmp)(argv[i], "--")) {
         i++;
         break;
      }
      if (0 == VG_(strcmp)(argv[i], "--command-line-only=yes"))
         augment = False;
      if (argv[i][0] != '-')
	break;
      add_string( tmp_xarray, argv[i] );
   }

   
   if (i < argc) {
      vg_assert(argv[i]);
      VG_(args_the_exename) = argv[i];
      i++;
   }

   
   for (; i < argc; i++) {
      vg_assert(argv[i]);
      add_string( VG_(args_for_client), argv[i] );
   }

   if (augment) {
      
      
      
      HChar* home    = VG_(getenv)("HOME");
      HChar* f1_clo  = home ? read_dot_valgrindrc( home ) : NULL;
      HChar* env_clo = VG_(strdup)( "commandline.sua.4",
                                    VG_(getenv)(VALGRIND_OPTS) );
      HChar* f2_clo  = NULL;

      
      
      if (home) {
         const HChar *cwd = VG_(get_startup_wd)();
         f2_clo = ( VG_STREQ(home, cwd)
                       ? NULL : read_dot_valgrindrc(".") );
      }

      if (f1_clo)  add_args_from_string( f1_clo );
      if (env_clo) add_args_from_string( env_clo );
      if (f2_clo)  add_args_from_string( f2_clo );
   }

   
   VG_(args_for_valgrind_noexecpass) 
      = VG_(sizeXA)( VG_(args_for_valgrind) );

   
   for (i = 0; i < VG_(sizeXA)( tmp_xarray ); i++)
      add_string( VG_(args_for_valgrind), 
                  * (HChar**)VG_(indexXA)( tmp_xarray, i ) );

   VG_(deleteXA)( tmp_xarray );
}

