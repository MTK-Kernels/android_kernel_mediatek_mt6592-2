/* Determine a canonical name for the current locale's character encoding.

   Copyright (C) 2000-2003 Free Software Foundation, Inc.

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

/* Written by Bruno Haible <bruno@clisp.org>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "localcharset.h"

#if HAVE_STDDEF_H
# include <stddef.h>
#endif

#include <stdio.h>
#if HAVE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif
#if HAVE_STDLIB_H
# include <stdlib.h>
#endif

#if defined _WIN32 || defined __WIN32__
# undef WIN32   
# define WIN32
#endif

#if defined __EMX__
# define OS2
#endif

#if !defined WIN32
# if HAVE_LANGINFO_CODESET
#  include <langinfo.h>
# else
#  if HAVE_SETLOCALE
#   include <locale.h>
#  endif
# endif
#elif defined WIN32
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#endif
#if defined OS2
# define INCL_DOS
# include <os2.h>
#endif

#if ENABLE_RELOCATABLE
# include "relocatable.h"
#else
# define relocate(pathname) (pathname)
#endif

#if defined _WIN32 || defined __WIN32__ || defined __EMX__ || defined __DJGPP__
  
# define ISSLASH(C) ((C) == '/' || (C) == '\\')
#endif

#ifndef DIRECTORY_SEPARATOR
# define DIRECTORY_SEPARATOR '/'
#endif

#ifndef ISSLASH
# define ISSLASH(C) ((C) == DIRECTORY_SEPARATOR)
#endif

#if HAVE_DECL_GETC_UNLOCKED
# undef getc
# define getc getc_unlocked
#endif

#if __STDC__ != 1
# define volatile 
#endif
static const char * volatile charset_aliases;

static const char *
get_charset_aliases ()
{
  const char *cp;

  cp = charset_aliases;
  if (cp == NULL)
    {
#if !(defined VMS || defined WIN32)
      FILE *fp;
      const char *dir = relocate (LIBDIR);
      const char *base = "charset.alias";
      char *file_name;

      
      {
	size_t dir_len = strlen (dir);
	size_t base_len = strlen (base);
	int add_slash = (dir_len > 0 && !ISSLASH (dir[dir_len - 1]));
	file_name = (char *) malloc (dir_len + add_slash + base_len + 1);
	if (file_name != NULL)
	  {
	    memcpy (file_name, dir, dir_len);
	    if (add_slash)
	      file_name[dir_len] = DIRECTORY_SEPARATOR;
	    memcpy (file_name + dir_len + add_slash, base, base_len + 1);
	  }
      }

      if (file_name == NULL || (fp = fopen (file_name, "r")) == NULL)
	
	cp = "";
      else
	{
	  
	  int c;
	  char buf1[50+1];
	  char buf2[50+1];
	  char *res_ptr = NULL;
	  size_t res_size = 0;
	  size_t l1, l2;

	  for (;;)
	    {
	      c = getc (fp);
	      if (c == EOF)
		break;
	      if (c == '\n' || c == ' ' || c == '\t')
		continue;
	      if (c == '#')
		{
		  
		  do
		    c = getc (fp);
		  while (!(c == EOF || c == '\n'));
		  if (c == EOF)
		    break;
		  continue;
		}
	      ungetc (c, fp);
	      if (fscanf (fp, "%50s %50s", buf1, buf2) < 2)
		break;
	      l1 = strlen (buf1);
	      l2 = strlen (buf2);
	      if (res_size == 0)
		{
		  res_size = l1 + 1 + l2 + 1;
		  res_ptr = (char *) malloc (res_size + 1);
		}
	      else
		{
		  res_size += l1 + 1 + l2 + 1;
		  res_ptr = (char *) realloc (res_ptr, res_size + 1);
		}
	      if (res_ptr == NULL)
		{
		  
		  res_size = 0;
		  break;
		}
	      strcpy (res_ptr + res_size - (l2 + 1) - (l1 + 1), buf1);
	      strcpy (res_ptr + res_size - (l2 + 1), buf2);
	    }
	  fclose (fp);
	  if (res_size == 0)
	    cp = "";
	  else
	    {
	      *(res_ptr + res_size) = '\0';
	      cp = res_ptr;
	    }
	}

      free (file_name);

#else

# if defined VMS
      cp = "ISO8859-1" "\0" "ISO-8859-1" "\0"
	   "ISO8859-2" "\0" "ISO-8859-2" "\0"
	   "ISO8859-5" "\0" "ISO-8859-5" "\0"
	   "ISO8859-7" "\0" "ISO-8859-7" "\0"
	   "ISO8859-8" "\0" "ISO-8859-8" "\0"
	   "ISO8859-9" "\0" "ISO-8859-9" "\0"
	   
	   "eucJP" "\0" "EUC-JP" "\0"
	   "SJIS" "\0" "SHIFT_JIS" "\0"
	   "DECKANJI" "\0" "DEC-KANJI" "\0"
	   "SDECKANJI" "\0" "EUC-JP" "\0"
	   
	   "eucTW" "\0" "EUC-TW" "\0"
	   "DECHANYU" "\0" "DEC-HANYU" "\0"
	   "DECHANZI" "\0" "GB2312" "\0"
	   
	   "DECKOREAN" "\0" "EUC-KR" "\0";
# endif

# if defined WIN32

      cp = "CP936" "\0" "GBK" "\0"
	   "CP1361" "\0" "JOHAB" "\0"
	   "CP20127" "\0" "ASCII" "\0"
	   "CP20866" "\0" "KOI8-R" "\0"
	   "CP21866" "\0" "KOI8-RU" "\0"
	   "CP28591" "\0" "ISO-8859-1" "\0"
	   "CP28592" "\0" "ISO-8859-2" "\0"
	   "CP28593" "\0" "ISO-8859-3" "\0"
	   "CP28594" "\0" "ISO-8859-4" "\0"
	   "CP28595" "\0" "ISO-8859-5" "\0"
	   "CP28596" "\0" "ISO-8859-6" "\0"
	   "CP28597" "\0" "ISO-8859-7" "\0"
	   "CP28598" "\0" "ISO-8859-8" "\0"
	   "CP28599" "\0" "ISO-8859-9" "\0"
	   "CP28605" "\0" "ISO-8859-15" "\0";
# endif
#endif

      charset_aliases = cp;
    }

  return cp;
}


#ifdef STATIC
STATIC
#endif
const char *
locale_charset ()
{
  const char *codeset;
  const char *aliases;

#if !(defined WIN32 || defined OS2)

# if HAVE_LANGINFO_CODESET

  
  codeset = nl_langinfo (CODESET);

# else

  
  const char *locale = NULL;

#  if HAVE_SETLOCALE && 0
  locale = setlocale (LC_CTYPE, NULL);
#  endif
  if (locale == NULL || locale[0] == '\0')
    {
      locale = getenv ("LC_ALL");
      if (locale == NULL || locale[0] == '\0')
	{
	  locale = getenv ("LC_CTYPE");
	  if (locale == NULL || locale[0] == '\0')
	    locale = getenv ("LANG");
	}
    }

  codeset = locale;

# endif

#elif defined WIN32

  static char buf[2 + 10 + 1];

  
  sprintf (buf, "CP%u", GetACP ());
  codeset = buf;

#elif defined OS2

  const char *locale;
  static char buf[2 + 10 + 1];
  ULONG cp[3];
  ULONG cplen;

  locale = getenv ("LC_ALL");
  if (locale == NULL || locale[0] == '\0')
    {
      locale = getenv ("LC_CTYPE");
      if (locale == NULL || locale[0] == '\0')
	locale = getenv ("LANG");
    }
  if (locale != NULL && locale[0] != '\0')
    {
      
      const char *dot = strchr (locale, '.');

      if (dot != NULL)
	{
	  const char *modifier;

	  dot++;
	  
	  modifier = strchr (dot, '@');
	  if (modifier == NULL)
	    return dot;
	  if (modifier - dot < sizeof (buf))
	    {
	      memcpy (buf, dot, modifier - dot);
	      buf [modifier - dot] = '\0';
	      return buf;
	    }
	}

      
      codeset = locale;
    }
  else
    {
      
      if (DosQueryCp (sizeof (cp), cp, &cplen))
	codeset = "";
      else
	{
	  sprintf (buf, "CP%u", cp[0]);
	  codeset = buf;
	}
    }

#endif

  if (codeset == NULL)
    
    codeset = "";

  
  for (aliases = get_charset_aliases ();
       *aliases != '\0';
       aliases += strlen (aliases) + 1, aliases += strlen (aliases) + 1)
    if (strcmp (codeset, aliases) == 0
	|| (aliases[0] == '*' && aliases[1] == '\0'))
      {
	codeset = aliases + strlen (aliases) + 1;
	break;
      }

  if (codeset[0] == '\0')
    codeset = "ASCII";

  return codeset;
}
