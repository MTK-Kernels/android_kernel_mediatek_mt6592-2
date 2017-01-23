

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
#include "pub_core_libcbase.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcfile.h"
#include "pub_core_libcproc.h"
#include "pub_core_libcprint.h"
#include "pub_core_mallocfree.h"
#include "pub_core_initimg.h"         

#include "priv_initimg_pathscan.h"



static Bool scan_colsep(HChar *colsep, Bool (*func)(const HChar *))
{
   HChar *cp, *entry;
   int end;

   if (colsep == NULL ||
       *colsep == '\0')
      return False;

   entry = cp = colsep;

   do {
      end = (*cp == '\0');

      if (*cp == ':' || *cp == '\0') {
	 HChar save = *cp;

	 *cp = '\0';
	 if ((*func)(entry)) {
            *cp = save;
	    return True;
         }
	 *cp = save;
	 entry = cp+1;
      }
      cp++;
   } while(!end);

   return False;
}


static const HChar *executable_name_in;
static HChar *executable_name_out;

static Bool match_executable(const HChar *entry) 
{
   
   if (*entry == '\0')
      entry = ".";

   HChar buf[VG_(strlen)(entry) + 1 + VG_(strlen)(executable_name_in) + 1];

   VG_(sprintf)(buf, "%s/%s", entry, executable_name_in);

   
   if (VG_(is_dir)(buf))
      return False;

   
   
   
   if (VG_(access)(buf, True, False, True) == 0) {
      VG_(free)(executable_name_out);
      executable_name_out = VG_(strdup)("match_executable", buf);
      return True;      
   } else if (VG_(access)(buf, True, False, False) == 0 
              && executable_name_out == NULL)
   {
      executable_name_out = VG_(strdup)("match_executable", buf);
      return False;     
   } else { 
      return False;     
   }
}

const HChar* ML_(find_executable) ( const HChar* exec )
{
   vg_assert(NULL != exec);

   if (VG_(strchr)(exec, '/')) {
      
      
      
      
      return exec;
   }

   
   HChar* path = VG_(getenv)("PATH");

   VG_(free)(executable_name_out);

   executable_name_in  = exec;
   executable_name_out = NULL;
   scan_colsep(path, match_executable);

   return executable_name_out;
}

