/* Implementation of the bindtextdomain(3) function
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

#include <stddef.h>
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
# define _nl_default_dirname libintl_nl_default_dirname
# define _nl_domain_bindings libintl_nl_domain_bindings
#endif

#ifndef offsetof
# define offsetof(type,ident) ((size_t)&(((type*)0)->ident))
#endif


extern const char _nl_default_dirname[];
#ifdef _LIBC
extern const char _nl_default_dirname_internal[] attribute_hidden;
#else
# define INTUSE(name) name
#endif

extern struct binding *_nl_domain_bindings;

__libc_rwlock_define (extern, _nl_state_lock attribute_hidden)


#ifdef _LIBC
# define BINDTEXTDOMAIN __bindtextdomain
# define BIND_TEXTDOMAIN_CODESET __bind_textdomain_codeset
# ifndef strdup
#  define strdup(str) __strdup (str)
# endif
#else
# define BINDTEXTDOMAIN libintl_bindtextdomain
# define BIND_TEXTDOMAIN_CODESET libintl_bind_textdomain_codeset
#endif

static void
set_binding_values (const char *domainname,
		    const char **dirnamep, const char **codesetp)
{
  struct binding *binding;
  int modified;

  
  if (domainname == NULL || domainname[0] == '\0')
    {
      if (dirnamep)
	*dirnamep = NULL;
      if (codesetp)
	*codesetp = NULL;
      return;
    }

  __libc_rwlock_wrlock (_nl_state_lock);

  modified = 0;

  for (binding = _nl_domain_bindings; binding != NULL; binding = binding->next)
    {
      int compare = strcmp (domainname, binding->domainname);
      if (compare == 0)
	
	break;
      if (compare < 0)
	{
	  
	  binding = NULL;
	  break;
	}
    }

  if (binding != NULL)
    {
      if (dirnamep)
	{
	  const char *dirname = *dirnamep;

	  if (dirname == NULL)
	    
	    *dirnamep = binding->dirname;
	  else
	    {
	      char *result = binding->dirname;
	      if (strcmp (dirname, result) != 0)
		{
		  if (strcmp (dirname, INTUSE(_nl_default_dirname)) == 0)
		    result = (char *) INTUSE(_nl_default_dirname);
		  else
		    {
#if defined _LIBC || defined HAVE_STRDUP
		      result = strdup (dirname);
#else
		      size_t len = strlen (dirname) + 1;
		      result = (char *) malloc (len);
		      if (__builtin_expect (result != NULL, 1))
			memcpy (result, dirname, len);
#endif
		    }

		  if (__builtin_expect (result != NULL, 1))
		    {
		      if (binding->dirname != INTUSE(_nl_default_dirname))
			free (binding->dirname);

		      binding->dirname = result;
		      modified = 1;
		    }
		}
	      *dirnamep = result;
	    }
	}

      if (codesetp)
	{
	  const char *codeset = *codesetp;

	  if (codeset == NULL)
	    
	    *codesetp = binding->codeset;
	  else
	    {
	      char *result = binding->codeset;
	      if (result == NULL || strcmp (codeset, result) != 0)
		{
#if defined _LIBC || defined HAVE_STRDUP
		  result = strdup (codeset);
#else
		  size_t len = strlen (codeset) + 1;
		  result = (char *) malloc (len);
		  if (__builtin_expect (result != NULL, 1))
		    memcpy (result, codeset, len);
#endif

		  if (__builtin_expect (result != NULL, 1))
		    {
		      free (binding->codeset);

		      binding->codeset = result;
		      binding->codeset_cntr++;
		      modified = 1;
		    }
		}
	      *codesetp = result;
	    }
	}
    }
  else if ((dirnamep == NULL || *dirnamep == NULL)
	   && (codesetp == NULL || *codesetp == NULL))
    {
      
      if (dirnamep)
	*dirnamep = INTUSE(_nl_default_dirname);
      if (codesetp)
	*codesetp = NULL;
    }
  else
    {
      
      size_t len = strlen (domainname) + 1;
      struct binding *new_binding =
	(struct binding *) malloc (offsetof (struct binding, domainname) + len);

      if (__builtin_expect (new_binding == NULL, 0))
	goto failed;

      memcpy (new_binding->domainname, domainname, len);

      if (dirnamep)
	{
	  const char *dirname = *dirnamep;

	  if (dirname == NULL)
	    
	    dirname = INTUSE(_nl_default_dirname);
	  else
	    {
	      if (strcmp (dirname, INTUSE(_nl_default_dirname)) == 0)
		dirname = INTUSE(_nl_default_dirname);
	      else
		{
		  char *result;
#if defined _LIBC || defined HAVE_STRDUP
		  result = strdup (dirname);
		  if (__builtin_expect (result == NULL, 0))
		    goto failed_dirname;
#else
		  size_t len = strlen (dirname) + 1;
		  result = (char *) malloc (len);
		  if (__builtin_expect (result == NULL, 0))
		    goto failed_dirname;
		  memcpy (result, dirname, len);
#endif
		  dirname = result;
		}
	    }
	  *dirnamep = dirname;
	  new_binding->dirname = (char *) dirname;
	}
      else
	
	new_binding->dirname = (char *) INTUSE(_nl_default_dirname);

      new_binding->codeset_cntr = 0;

      if (codesetp)
	{
	  const char *codeset = *codesetp;

	  if (codeset != NULL)
	    {
	      char *result;

#if defined _LIBC || defined HAVE_STRDUP
	      result = strdup (codeset);
	      if (__builtin_expect (result == NULL, 0))
		goto failed_codeset;
#else
	      size_t len = strlen (codeset) + 1;
	      result = (char *) malloc (len);
	      if (__builtin_expect (result == NULL, 0))
		goto failed_codeset;
	      memcpy (result, codeset, len);
#endif
	      codeset = result;
	      new_binding->codeset_cntr++;
	    }
	  *codesetp = codeset;
	  new_binding->codeset = (char *) codeset;
	}
      else
	new_binding->codeset = NULL;

      
      if (_nl_domain_bindings == NULL
	  || strcmp (domainname, _nl_domain_bindings->domainname) < 0)
	{
	  new_binding->next = _nl_domain_bindings;
	  _nl_domain_bindings = new_binding;
	}
      else
	{
	  binding = _nl_domain_bindings;
	  while (binding->next != NULL
		 && strcmp (domainname, binding->next->domainname) > 0)
	    binding = binding->next;

	  new_binding->next = binding->next;
	  binding->next = new_binding;
	}

      modified = 1;

      
      if (0)
	{
	failed_codeset:
	  if (new_binding->dirname != INTUSE(_nl_default_dirname))
	    free (new_binding->dirname);
	failed_dirname:
	  free (new_binding);
	failed:
	  if (dirnamep)
	    *dirnamep = NULL;
	  if (codesetp)
	    *codesetp = NULL;
	}
    }

  
  if (modified)
    ++_nl_msg_cat_cntr;

  __libc_rwlock_unlock (_nl_state_lock);
}

char *
BINDTEXTDOMAIN (const char *domainname, const char *dirname)
{
  set_binding_values (domainname, &dirname, NULL);
  return (char *) dirname;
}

char *
BIND_TEXTDOMAIN_CODESET (const char *domainname, const char *codeset)
{
  set_binding_values (domainname, NULL, &codeset);
  return (char *) codeset;
}

#ifdef _LIBC
weak_alias (__bindtextdomain, bindtextdomain);
weak_alias (__bind_textdomain_codeset, bind_textdomain_codeset);
#endif
