/* Provide relocatable packages.
   Copyright (C) 2003 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2003.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU Library General Public License as published
   by the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
   USA.  */

#ifndef _RELOCATABLE_H
#define _RELOCATABLE_H

#ifdef __cplusplus
extern "C" {
#endif


#if ENABLE_RELOCATABLE

#if defined _MSC_VER && BUILDING_DLL
# define RELOCATABLE_DLL_EXPORTED __declspec(dllexport)
#else
# define RELOCATABLE_DLL_EXPORTED
#endif

extern RELOCATABLE_DLL_EXPORTED void
       set_relocation_prefix (const char *orig_prefix,
			      const char *curr_prefix);

extern const char * relocate (const char *pathname);


extern const char * compute_curr_prefix (const char *orig_installprefix,
					 const char *orig_installdir,
					 const char *curr_pathname);

#else

#define relocate(pathname) (pathname)

#endif


#ifdef __cplusplus
}
#endif

#endif 
