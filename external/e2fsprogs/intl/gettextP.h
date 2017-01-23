/* Header describing internals of libintl library.
   Copyright (C) 1995-1999, 2000-2003 Free Software Foundation, Inc.
   Written by Ulrich Drepper <drepper@cygnus.com>, 1995.

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

#ifndef _GETTEXTP_H
#define _GETTEXTP_H

#include <stddef.h>		

#ifdef _LIBC
# include "../iconv/gconv_int.h"
#else
# if HAVE_ICONV
#  include <iconv.h>
# endif
#endif

#include "loadinfo.h"

#include "gmo.h"		


#ifndef internal_function
# define internal_function
#endif

#ifndef attribute_hidden
# define attribute_hidden
#endif

#ifndef HAVE_BUILTIN_EXPECT
# define __builtin_expect(expr, val) (expr)
#endif

#ifndef W
# define W(flag, data) ((flag) ? SWAP (data) : (data))
#endif


#ifdef _LIBC
# include <byteswap.h>
# define SWAP(i) bswap_32 (i)
#else
static inline nls_uint32
SWAP (i)
     nls_uint32 i;
{
  return (i << 24) | ((i & 0xff00) << 8) | ((i >> 8) & 0xff00) | (i >> 24);
}
#endif


struct sysdep_string_desc
{
  
  size_t length;
  
  const char *pointer;
};

struct loaded_domain
{
  
  const char *data;
  
  int use_mmap;
  
  size_t mmap_size;
  
  int must_swap;
  
  void *malloced;

  
  nls_uint32 nstrings;
  
  const struct string_desc *orig_tab;
  
  const struct string_desc *trans_tab;

  
  nls_uint32 n_sysdep_strings;
  
  const struct sysdep_string_desc *orig_sysdep_tab;
  
  const struct sysdep_string_desc *trans_sysdep_tab;

  
  nls_uint32 hash_size;
  
  const nls_uint32 *hash_tab;
  
  int must_swap_hash_tab;

  int codeset_cntr;
#ifdef _LIBC
  __gconv_t conv;
#else
# if HAVE_ICONV
  iconv_t conv;
# endif
#endif
  char **conv_tab;

  struct expression *plural;
  unsigned long int nplurals;
};

#ifdef __GNUC__
# define ZERO 0
#else
# define ZERO 1
#endif

struct binding
{
  struct binding *next;
  char *dirname;
  int codeset_cntr;	
  char *codeset;
  char domainname[ZERO];
};

extern int _nl_msg_cat_cntr;

#ifndef _LIBC
const char *_nl_locale_name (int category, const char *categoryname);
#endif

struct loaded_l10nfile *_nl_find_domain (const char *__dirname, char *__locale,
					 const char *__domainname,
					 struct binding *__domainbinding)
     internal_function;
void _nl_load_domain (struct loaded_l10nfile *__domain,
		      struct binding *__domainbinding)
     internal_function;
void _nl_unload_domain (struct loaded_domain *__domain)
     internal_function;
const char *_nl_init_domain_conv (struct loaded_l10nfile *__domain_file,
				  struct loaded_domain *__domain,
				  struct binding *__domainbinding)
     internal_function;
void _nl_free_domain_conv (struct loaded_domain *__domain)
     internal_function;

char *_nl_find_msg (struct loaded_l10nfile *domain_file,
		    struct binding *domainbinding, const char *msgid,
		    size_t *lengthp)
     internal_function;

#ifdef _LIBC
extern char *__gettext (const char *__msgid);
extern char *__dgettext (const char *__domainname, const char *__msgid);
extern char *__dcgettext (const char *__domainname, const char *__msgid,
			  int __category);
extern char *__ngettext (const char *__msgid1, const char *__msgid2,
			 unsigned long int __n);
extern char *__dngettext (const char *__domainname,
			  const char *__msgid1, const char *__msgid2,
			  unsigned long int n);
extern char *__dcngettext (const char *__domainname,
			   const char *__msgid1, const char *__msgid2,
			   unsigned long int __n, int __category);
extern char *__dcigettext (const char *__domainname,
			   const char *__msgid1, const char *__msgid2,
			   int __plural, unsigned long int __n,
			   int __category);
extern char *__textdomain (const char *__domainname);
extern char *__bindtextdomain (const char *__domainname,
			       const char *__dirname);
extern char *__bind_textdomain_codeset (const char *__domainname,
					const char *__codeset);
#else
# undef _INTL_REDIRECT_INLINE
# undef _INTL_REDIRECT_MACROS
# define _INTL_REDIRECT_MACROS
# include "libgnuintl.h"
extern char *libintl_dcigettext (const char *__domainname,
				 const char *__msgid1, const char *__msgid2,
				 int __plural, unsigned long int __n,
				 int __category);
#endif


#endif 
