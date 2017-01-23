/* Implementation of the textdomain(3) function.
   Copyright (C) 1995-1998, 2000-2003 Free Software Foundation, Inc.

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#ifdef _LIBC
# include <libintl.h>
#else
# include "libgnuintl.h"
#endif
#include "gettextP.h"

#ifdef _LIBC
# include <bits/libc-lock.h>
#else
# define __libc_rwlock_define(CLASS, NAME)
# define __libc_rwlock_wrlock(NAME)
# define __libc_rwlock_unlock(NAME)
#endif

#if !defined _LIBC
# define _nl_default_default_domain libintl_nl_default_default_domain
# define _nl_current_default_domain libintl_nl_current_default_domain
#endif


extern const char _nl_default_default_domain[] attribute_hidden;

extern const char *_nl_current_default_domain attribute_hidden;


#ifdef _LIBC
# define TEXTDOMAIN __textdomain
# ifndef strdup
#  define strdup(str) __strdup (str)
# endif
#else
# define TEXTDOMAIN libintl_textdomain
#endif

__libc_rwlock_define (extern, _nl_state_lock attribute_hidden)

char *
TEXTDOMAIN (const char *domainname)
{
  char *new_domain;
  char *old_domain;

  
  if (domainname == NULL)
    return (char *) _nl_current_default_domain;

  __libc_rwlock_wrlock (_nl_state_lock);

  old_domain = (char *) _nl_current_default_domain;

  
  if (domainname[0] == '\0'
      || strcmp (domainname, _nl_default_default_domain) == 0)
    {
      _nl_current_default_domain = _nl_default_default_domain;
      new_domain = (char *) _nl_current_default_domain;
    }
  else if (strcmp (domainname, old_domain) == 0)
    new_domain = old_domain;
  else
    {
#if defined _LIBC || defined HAVE_STRDUP
      new_domain = strdup (domainname);
#else
      size_t len = strlen (domainname) + 1;
      new_domain = (char *) malloc (len);
      if (new_domain != NULL)
	memcpy (new_domain, domainname, len);
#endif

      if (new_domain != NULL)
	_nl_current_default_domain = new_domain;
    }

  if (new_domain != NULL)
    {
      ++_nl_msg_cat_cntr;

      if (old_domain != new_domain && old_domain != _nl_default_default_domain)
	free (old_domain);
    }

  __libc_rwlock_unlock (_nl_state_lock);

  return new_domain;
}

#ifdef _LIBC
weak_alias (__textdomain, textdomain);
#endif
