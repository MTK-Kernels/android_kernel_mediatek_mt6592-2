/* Implementation of ngettext(3) function.
   Copyright (C) 1995, 1997, 2000-2003 Free Software Foundation, Inc.

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

#ifdef _LIBC
# define __need_NULL
# include <stddef.h>
#else
# include <stdlib.h>		
#endif

#include "gettextP.h"
#ifdef _LIBC
# include <libintl.h>
#else
# include "libgnuintl.h"
#endif

#include <locale.h>


#ifdef _LIBC
# define NGETTEXT __ngettext
# define DCNGETTEXT __dcngettext
#else
# define NGETTEXT libintl_ngettext
# define DCNGETTEXT libintl_dcngettext
#endif

char *
NGETTEXT (const char *msgid1, const char *msgid2, unsigned long int n)
{
  return DCNGETTEXT (NULL, msgid1, msgid2, n, LC_MESSAGES);
}

#ifdef _LIBC
weak_alias (__ngettext, ngettext);
#endif
