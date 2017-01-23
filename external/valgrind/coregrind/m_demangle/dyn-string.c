/* An abstract string datatype.
   Copyright (C) 1998, 1999, 2000, 2002, 2004 Free Software Foundation, Inc.
   Contributed by Mark Mitchell (mark@markmitchell.com).

This file is part of GNU CC.
   
GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

In addition to the permissions in the GNU General Public License, the
Free Software Foundation gives you unlimited permission to link the
compiled version of this file into combinations with other programs,
and to distribute those combinations without any restriction coming
from the use of this file.  (The General Public License restrictions
do apply in other respects; for example, they cover modification of
the file, and distribution when not linked into a combined
executable.)

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 51 Franklin Street - Fifth Floor,
Boston, MA 02110-1301, USA.  */

#if 0 
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#endif 

#if 0 
#include <stdio.h>
#endif 

#if 0 
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#endif 

#if 0 
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#endif 

#if 0 
#include "libiberty.h"
#endif 

#include "vg_libciface.h"

#include "dyn-string.h"


int
dyn_string_init (struct dyn_string *ds_struct_ptr, int space)
{
  
  if (space == 0)
    space = 1;

#ifdef RETURN_ON_ALLOCATION_FAILURE
  ds_struct_ptr->s = (char *) malloc (space);
  if (ds_struct_ptr->s == NULL)
    return 0;
#else
  ds_struct_ptr->s = XNEWVEC (char, space);
#endif
  ds_struct_ptr->allocated = space;
  ds_struct_ptr->length = 0;
  ds_struct_ptr->s[0] = '\0';

  return 1;
}


dyn_string_t 
dyn_string_new (int space)
{
  dyn_string_t result;
#ifdef RETURN_ON_ALLOCATION_FAILURE
  result = (dyn_string_t) malloc (sizeof (struct dyn_string));
  if (result == NULL)
    return NULL;
  if (!dyn_string_init (result, space))
    {
      free (result);
      return NULL;
    }
#else
  result = XNEW (struct dyn_string);
  dyn_string_init (result, space);
#endif
  return result;
}


void 
dyn_string_delete (dyn_string_t ds)
{
  free (ds->s);
  free (ds);
}


char*
dyn_string_release (dyn_string_t ds)
{
  
  char* result = ds->s;
  
  ds->s = NULL;
  
  free (ds);
  
  return result;
}


dyn_string_t 
dyn_string_resize (dyn_string_t ds, int space)
{
  int new_allocated = ds->allocated;

  
  ++space;

  
  while (space > new_allocated)
    new_allocated *= 2;
    
  if (new_allocated != ds->allocated)
    {
      ds->allocated = new_allocated;
      
#ifdef RETURN_ON_ALLOCATION_FAILURE
      ds->s = (char *) realloc (ds->s, ds->allocated);
      if (ds->s == NULL)
	{
	  free (ds);
	  return NULL;
	}
#else
      ds->s = XRESIZEVEC (char, ds->s, ds->allocated);
#endif
    }

  return ds;
}


void
dyn_string_clear (dyn_string_t ds)
{
  
  ds->s[0] = '\0';
  ds->length = 0;
}


int
dyn_string_copy (dyn_string_t dest, dyn_string_t src)
{
  if (dest == src)
    abort ();

  
  if (dyn_string_resize (dest, src->length) == NULL)
    return 0;
  
  strcpy (dest->s, src->s);
  
  dest->length = src->length;
  return 1;
}


int
dyn_string_copy_cstr (dyn_string_t dest, const char *src)
{
  int length = strlen (src);
  
  if (dyn_string_resize (dest, length) == NULL)
    return 0;
  
  strcpy (dest->s, src);
  
  dest->length = length;
  return 1;
}


int
dyn_string_prepend (dyn_string_t dest, dyn_string_t src)
{
  return dyn_string_insert (dest, 0, src);
}


int
dyn_string_prepend_cstr (dyn_string_t dest, const char *src)
{
  return dyn_string_insert_cstr (dest, 0, src);
}


int
dyn_string_insert (dyn_string_t dest, int pos, dyn_string_t src)
{
  int i;

  if (src == dest)
    abort ();

  if (dyn_string_resize (dest, dest->length + src->length) == NULL)
    return 0;
  
  for (i = dest->length; i >= pos; --i)
    dest->s[i + src->length] = dest->s[i];
  
  strncpy (dest->s + pos, src->s, src->length);
  
  dest->length += src->length;
  return 1;
}


int
dyn_string_insert_cstr (dyn_string_t dest, int pos, const char *src)
{
  int i;
  int length = strlen (src);

  if (dyn_string_resize (dest, dest->length + length) == NULL)
    return 0;
  
  for (i = dest->length; i >= pos; --i)
    dest->s[i + length] = dest->s[i];
  
  strncpy (dest->s + pos, src, length);
  
  dest->length += length;
  return 1;
}


int
dyn_string_insert_char (dyn_string_t dest, int pos, int c)
{
  int i;

  if (dyn_string_resize (dest, dest->length + 1) == NULL)
    return 0;
  
  for (i = dest->length; i >= pos; --i)
    dest->s[i + 1] = dest->s[i];
  
  dest->s[pos] = c;
  
  ++dest->length;
  return 1;
}
     

int
dyn_string_append (dyn_string_t dest, dyn_string_t s)
{
  if (dyn_string_resize (dest, dest->length + s->length) == 0)
    return 0;
  strcpy (dest->s + dest->length, s->s);
  dest->length += s->length;
  return 1;
}


int
dyn_string_append_cstr (dyn_string_t dest, const char *s)
{
  int len = strlen (s);

  if (dyn_string_resize (dest, dest->length + len) == NULL)
    return 0;
  strcpy (dest->s + dest->length, s);
  dest->length += len;
  return 1;
}


int
dyn_string_append_char (dyn_string_t dest, int c)
{
  
  if (dyn_string_resize (dest, dest->length + 1) == NULL)
    return 0;
  
  dest->s[dest->length] = c;
  
  dest->s[dest->length + 1] = '\0';
  
  ++(dest->length);
  return 1;
}


int
dyn_string_substring (dyn_string_t dest, dyn_string_t src,
                      int start, int end)
{
  int i;
  int length = end - start;

  if (start > end || start > src->length || end > src->length)
    abort ();

  
  if (dyn_string_resize (dest, length) == NULL)
    return 0;
  
  for (i = length; --i >= 0; )
    dest->s[i] = src->s[start + i];
  
  dest->s[length] = '\0';
  
  dest->length = length;

  return 1;
}


int
dyn_string_eq (dyn_string_t ds1, dyn_string_t ds2)
{
  
  if (ds1->length != ds2->length)
    return 0;
  else
    return !strcmp (ds1->s, ds2->s);
}
