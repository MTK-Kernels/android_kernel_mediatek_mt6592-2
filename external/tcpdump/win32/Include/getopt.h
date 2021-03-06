/* Declarations for getopt.
   Copyright (C) 1989, 1990, 1991, 1992, 1993 Free Software Foundation, Inc.

This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

#ifndef _GETOPT_H
#define _GETOPT_H 1

#ifdef	__cplusplus
extern "C" {
#endif


#ifndef WIN32
extern char *optarg;
#endif


#ifndef WIN32
extern int optind;
#endif


#ifndef WIN32
extern int opterr;
#endif


extern int optopt;


struct option
{
#if	__STDC__
  const char *name;
#else
  char *name;
#endif
  int has_arg;
  int *flag;
  int val;
};


#define	no_argument		0
#define required_argument	1
#define optional_argument	2

#if __STDC__
#if defined(__GNU_LIBRARY__)
extern int getopt (int argc, char *const *argv, const char *shortopts);
#else 
extern int getopt ();
#endif 
extern int getopt_long (int argc, char *const *argv, const char *shortopts,
		        const struct option *longopts, int *longind);
extern int getopt_long_only (int argc, char *const *argv,
			     const char *shortopts,
		             const struct option *longopts, int *longind);

extern int _getopt_internal (int argc, char *const *argv,
			     const char *shortopts,
		             const struct option *longopts, int *longind,
			     int long_only);
#else 
extern int getopt ();
extern int getopt_long ();
extern int getopt_long_only ();

extern int _getopt_internal ();
#endif 

#ifdef	__cplusplus
}
#endif

#endif 
