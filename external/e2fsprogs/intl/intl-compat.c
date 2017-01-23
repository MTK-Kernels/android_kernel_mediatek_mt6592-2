/* intl-compat.c - Stub functions to call gettext functions from GNU gettext
   Library.
   Copyright (C) 1995, 2000-2003 Software Foundation, Inc.

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

#include "gettextP.h"




#undef gettext
#undef dgettext
#undef dcgettext
#undef ngettext
#undef dngettext
#undef dcngettext
#undef textdomain
#undef bindtextdomain
#undef bind_textdomain_codeset


#if defined _MSC_VER && BUILDING_DLL
# define DLL_EXPORTED __declspec(dllexport)
#else
# define DLL_EXPORTED
#endif


DLL_EXPORTED
char *
gettext (const char *msgid)
{
  return libintl_gettext (msgid);
}


DLL_EXPORTED
char *
dgettext (const char *domainname, const char *msgid)
{
  return libintl_dgettext (domainname, msgid);
}


DLL_EXPORTED
char *
dcgettext (const char *domainname, const char *msgid, int category)
{
  return libintl_dcgettext (domainname, msgid, category);
}


DLL_EXPORTED
char *
ngettext (const char *msgid1, const char *msgid2, unsigned long int n)
{
  return libintl_ngettext (msgid1, msgid2, n);
}


DLL_EXPORTED
char *
dngettext (const char *domainname,
	   const char *msgid1, const char *msgid2, unsigned long int n)
{
  return libintl_dngettext (domainname, msgid1, msgid2, n);
}


DLL_EXPORTED
char *
dcngettext (const char *domainname,
	    const char *msgid1, const char *msgid2, unsigned long int n,
	    int category)
{
  return libintl_dcngettext (domainname, msgid1, msgid2, n, category);
}


DLL_EXPORTED
char *
textdomain (const char *domainname)
{
  return libintl_textdomain (domainname);
}


DLL_EXPORTED
char *
bindtextdomain (const char *domainname, const char *dirname)
{
  return libintl_bindtextdomain (domainname, dirname);
}


DLL_EXPORTED
char *
bind_textdomain_codeset (const char *domainname, const char *codeset)
{
  return libintl_bind_textdomain_codeset (domainname, codeset);
}
