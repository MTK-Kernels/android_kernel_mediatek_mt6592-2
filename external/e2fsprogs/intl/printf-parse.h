/* Parse printf format string.
   Copyright (C) 1999, 2002-2003 Free Software Foundation, Inc.

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

#ifndef _PRINTF_PARSE_H
#define _PRINTF_PARSE_H

#include "printf-args.h"


#define FLAG_GROUP	 1	
#define FLAG_LEFT	 2	
#define FLAG_SHOWSIGN	 4	
#define FLAG_SPACE	 8	
#define FLAG_ALT	16	
#define FLAG_ZERO	32

#define ARG_NONE	(~(size_t)0)

typedef struct
{
  const char* dir_start;
  const char* dir_end;
  int flags;
  const char* width_start;
  const char* width_end;
  size_t width_arg_index;
  const char* precision_start;
  const char* precision_end;
  size_t precision_arg_index;
  char conversion; 
  size_t arg_index;
}
char_directive;

typedef struct
{
  size_t count;
  char_directive *dir;
  size_t max_width_length;
  size_t max_precision_length;
}
char_directives;


#ifdef STATIC
STATIC
#else
extern
#endif
int printf_parse (const char *format, char_directives *d, arguments *a);

#endif 
