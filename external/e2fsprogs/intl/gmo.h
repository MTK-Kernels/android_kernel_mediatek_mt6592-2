/* Description of GNU message catalog format: general file layout.
   Copyright (C) 1995, 1997, 2000-2002, 2004 Free Software Foundation, Inc.

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

#ifndef _GETTEXT_H
#define _GETTEXT_H 1

#include <limits.h>


#define _MAGIC 0x950412de
#define _MAGIC_SWAPPED 0xde120495

#define MO_REVISION_NUMBER 0
#define MO_REVISION_NUMBER_WITH_SYSDEP_I 1


#if __STDC__
# define UINT_MAX_32_BITS 4294967295U
#else
# define UINT_MAX_32_BITS 0xFFFFFFFF
#endif


#ifndef UINT_MAX
# define UINT_MAX UINT_MAX_32_BITS
#endif

#if UINT_MAX == UINT_MAX_32_BITS
typedef unsigned nls_uint32;
#else
# if USHRT_MAX == UINT_MAX_32_BITS
typedef unsigned short nls_uint32;
# else
#  if ULONG_MAX == UINT_MAX_32_BITS
typedef unsigned long nls_uint32;
#  else
  "Cannot determine unsigned 32-bit data type."
#  endif
# endif
#endif


struct mo_file_header
{
  
  nls_uint32 magic;
  
  nls_uint32 revision;

  

  
  nls_uint32 nstrings;
  
  nls_uint32 orig_tab_offset;
  
  nls_uint32 trans_tab_offset;
  
  nls_uint32 hash_tab_size;
  
  nls_uint32 hash_tab_offset;

  

  
  nls_uint32 n_sysdep_segments;
  
  nls_uint32 sysdep_segments_offset;
  
  nls_uint32 n_sysdep_strings;
  
  nls_uint32 orig_sysdep_tab_offset;
  
  nls_uint32 trans_sysdep_tab_offset;
};

struct string_desc
{
  
  nls_uint32 length;
  
  nls_uint32 offset;
};


struct sysdep_segment
{
  
  nls_uint32 length;
  
  nls_uint32 offset;
};

struct sysdep_string
{
  
  nls_uint32 offset;
  struct segment_pair
  {
    
    nls_uint32 segsize;
    
    nls_uint32 sysdepref;
  } segments[1];
};

#define SEGMENTS_END ((nls_uint32) ~0)


#endif	
