/* General utility routines for the remote server for GDB.
   Copyright (C) 1986, 1989, 1993, 1995, 1996, 1997, 1999, 2000, 2002, 2003,
   2011
   Free Software Foundation, Inc.

   This file is part of GDB.
   It has been modified to integrate it in valgrind

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

#include "server.h"

void sr_perror (SysRes sr, const char *string,...)
{
   va_list args;
   if (sr_isError (sr))
      VG_(umsg) ("error %ld %s\n", sr_Err(sr), VG_(strerror) (sr_Err(sr)));
   else
      VG_(umsg) ("sr_perror called with no error!!!\n");
   va_start (args, string);
   VG_(vmessage) ( Vg_UserMsg, string, args );
   va_end (args);
}


void error (const char *string,...)
{
   va_list args;
   va_start (args, string);
   VG_(vmessage) ( Vg_UserMsg, string, args );
   va_end(args);
   VG_MINIMAL_LONGJMP(toplevel);
}


void fatal (const char *string,...)
{
   va_list args;
   va_start (args, string);
   VG_(vmessage) ( Vg_UserMsg, string, args );
   va_end (args);
   VG_(exit) (1);
}

void warning (const char *string,...)
{
   va_list args;
   va_start (args, string);
   VG_(vmessage) ( Vg_UserMsg, string, args );
   va_end (args);
}

#if 0
static
void dbgts(void)
{
   struct vki_timeval dbgtv;
   SysRes res;
   res = VG_(do_syscall2)(__NR_gettimeofday, (UWord)&dbgtv, (UWord)NULL);
   
   dlog(0, "%ld.%6ld ", dbgtv.tv_sec, dbgtv.tv_usec);
}
#endif
