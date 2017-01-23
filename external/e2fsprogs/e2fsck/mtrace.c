/* More debugging hooks for `malloc'.
   Copyright (C) 1991, 1992 Free Software Foundation, Inc.
		 Written April 2, 1991 by John Gilmore of Cygnus Support.
		 Based on mcheck.c by Mike Haertel.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.

   The author may be reached (Email) at the address mike@ai.mit.edu,
   or (US mail) as Mike Haertel c/o Free Software Foundation.  */

#ifndef	_MALLOC_INTERNAL
#define	_MALLOC_INTERNAL
#include "./mtrace.h"
#endif

#include <stdio.h>

#ifndef	__GNU_LIBRARY__
extern char *getenv ();
#else
#include <stdlib.h>
#endif

static FILE *mallstream;
static char mallenv[]= "MALLOC_TRACE";
static char mallbuf[BUFSIZ];	

__ptr_t mallwatch;

static void (*tr_old_free_hook) __P ((__ptr_t ptr));
static __ptr_t (*tr_old_malloc_hook) __P ((size_t size));
static __ptr_t (*tr_old_realloc_hook) __P ((__ptr_t ptr, size_t size));

FILE *malloc_get_mallstream()
{
	return mallstream;
}


void tr_break __P ((void));
void
tr_break ()
{
}

static void tr_freehook __P ((__ptr_t));
static void
tr_freehook (ptr)
     __ptr_t ptr;
{
  fprintf (mallstream, "- %p\n", ptr);	
  if (ptr == mallwatch)
    tr_break ();
  __free_hook = tr_old_free_hook;
  free (ptr);
  __free_hook = tr_freehook;
}

static __ptr_t tr_mallochook __P ((size_t));
static __ptr_t
tr_mallochook (size)
     size_t size;
{
  __ptr_t hdr;

  __malloc_hook = tr_old_malloc_hook;
  hdr = (__ptr_t) malloc (size);
  __malloc_hook = tr_mallochook;

  
  fprintf (mallstream, "+ %p %d\n", hdr, size);

  if (hdr == mallwatch)
    tr_break ();

  return hdr;
}

static __ptr_t tr_reallochook __P ((__ptr_t, size_t));
static __ptr_t
tr_reallochook (ptr, size)
     __ptr_t ptr;
     size_t size;
{
  __ptr_t hdr;

  if (ptr == mallwatch)
    tr_break ();

  __free_hook = tr_old_free_hook;
  __malloc_hook = tr_old_malloc_hook;
  __realloc_hook = tr_old_realloc_hook;
  hdr = (__ptr_t) realloc (ptr, size);
  __free_hook = tr_freehook;
  __malloc_hook = tr_mallochook;
  __realloc_hook = tr_reallochook;
  if (hdr == NULL)
    
    fprintf (mallstream, "! %p %d\n", ptr, size);
  else
    fprintf (mallstream, "< %p\n> %p %d\n", ptr, hdr, size);

  if (hdr == mallwatch)
    tr_break ();

  return hdr;
}


void
mtrace ()
{
  char *mallfile;

  mallfile = getenv (mallenv);
  if (mallfile != NULL || mallwatch != NULL)
    {
      mallstream = fopen (mallfile != NULL ? mallfile : "/dev/null", "w");
      if (mallstream != NULL)
	{
	  
	  setbuf (mallstream, mallbuf);
	  fprintf (mallstream, "= Start\n");
	  tr_old_free_hook = __free_hook;
	  __free_hook = tr_freehook;
	  tr_old_malloc_hook = __malloc_hook;
	  __malloc_hook = tr_mallochook;
	  tr_old_realloc_hook = __realloc_hook;
	  __realloc_hook = tr_reallochook;
	}
    }
}
