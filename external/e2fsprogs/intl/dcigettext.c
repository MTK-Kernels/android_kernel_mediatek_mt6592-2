/* Implementation of the internal dcigettext function.
   Copyright (C) 1995-1999, 2000-2003 Free Software Foundation, Inc.

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

#ifndef _GNU_SOURCE
# define _GNU_SOURCE	1
#endif

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/types.h>

#ifdef __GNUC__
# define alloca __builtin_alloca
# define HAVE_ALLOCA 1
#else
# ifdef _MSC_VER
#  include <malloc.h>
#  define alloca _alloca
# else
#  if defined HAVE_ALLOCA_H || defined _LIBC
#   include <alloca.h>
#  else
#   ifdef _AIX
 #pragma alloca
#   else
#    ifndef alloca
char *alloca ();
#    endif
#   endif
#  endif
# endif
#endif

#include <errno.h>
#ifndef errno
extern int errno;
#endif
#ifndef __set_errno
# define __set_errno(val) errno = (val)
#endif

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#if defined HAVE_UNISTD_H || defined _LIBC
# include <unistd.h>
#endif

#include <locale.h>

#ifdef _LIBC
# if defined __alpha__ || defined __arm__ || defined __i386__ \
     || defined __m68k__ || defined __s390__
#  define INTDIV0_RAISES_SIGFPE 1
# else
#  define INTDIV0_RAISES_SIGFPE 0
# endif
#endif
#if !INTDIV0_RAISES_SIGFPE
# include <signal.h>
#endif

#if defined HAVE_SYS_PARAM_H || defined _LIBC
# include <sys/param.h>
#endif

#include "gettextP.h"
#include "plural-exp.h"
#ifdef _LIBC
# include <libintl.h>
#else
# include "libgnuintl.h"
#endif
#include "hash-string.h"

#ifdef _LIBC
# include <bits/libc-lock.h>
#else
# define __libc_lock_define_initialized(CLASS, NAME)
# define __libc_lock_lock(NAME)
# define __libc_lock_unlock(NAME)
# define __libc_rwlock_define_initialized(CLASS, NAME)
# define __libc_rwlock_rdlock(NAME)
# define __libc_rwlock_unlock(NAME)
#endif

#if defined __GNUC__ && __GNUC__ >= 2
# define alignof(TYPE) __alignof__ (TYPE)
#else
# define alignof(TYPE) \
    ((int) &((struct { char dummy1; TYPE dummy2; } *) 0)->dummy2)
#endif

#if !defined _LIBC
# define _nl_default_default_domain libintl_nl_default_default_domain
# define _nl_current_default_domain libintl_nl_current_default_domain
# define _nl_default_dirname libintl_nl_default_dirname
# define _nl_domain_bindings libintl_nl_domain_bindings
#endif

#ifndef offsetof
# define offsetof(type,ident) ((size_t)&(((type*)0)->ident))
#endif


#ifdef _LIBC
# define getcwd __getcwd
# ifndef stpcpy
#  define stpcpy __stpcpy
# endif
# define tfind __tfind
#else
# if !defined HAVE_GETCWD
char *getwd ();
#  define getcwd(buf, max) getwd (buf)
# else
#  if VMS
#   define getcwd(buf, max) (getcwd) (buf, max, 0)
#  else
char *getcwd ();
#  endif
# endif
# ifndef HAVE_STPCPY
#define stpcpy(dest, src) my_stpcpy(dest, src)
static char *stpcpy (char *dest, const char *src);
# endif
# ifndef HAVE_MEMPCPY
static void *mempcpy (void *dest, const void *src, size_t n);
# endif
#endif

#define PATH_INCR 32

#if defined _POSIX_VERSION || (defined HAVE_LIMITS_H && !defined __GNUC__)
# include <limits.h>
#endif

#ifndef _POSIX_PATH_MAX
# define _POSIX_PATH_MAX 255
#endif

#if !defined PATH_MAX && defined _PC_PATH_MAX
# define PATH_MAX (pathconf ("/", _PC_PATH_MAX) < 1 ? 1024 : pathconf ("/", _PC_PATH_MAX))
#endif

#if defined HAVE_SYS_PARAM_H && !defined PATH_MAX && !defined MAXPATHLEN
# include <sys/param.h>
#endif

#if !defined PATH_MAX && defined MAXPATHLEN
# define PATH_MAX MAXPATHLEN
#endif

#ifndef PATH_MAX
# define PATH_MAX _POSIX_PATH_MAX
#endif

#if defined _WIN32 || defined __WIN32__ || defined __EMX__ || defined __DJGPP__
  
# define ISSLASH(C) ((C) == '/' || (C) == '\\')
# define HAS_DEVICE(P) \
    ((((P)[0] >= 'A' && (P)[0] <= 'Z') || ((P)[0] >= 'a' && (P)[0] <= 'z')) \
     && (P)[1] == ':')
# define IS_ABSOLUTE_PATH(P) (ISSLASH ((P)[0]) || HAS_DEVICE (P))
# define IS_PATH_WITH_DIR(P) \
    (strchr (P, '/') != NULL || strchr (P, '\\') != NULL || HAS_DEVICE (P))
#else
  
# define ISSLASH(C) ((C) == '/')
# define IS_ABSOLUTE_PATH(P) ISSLASH ((P)[0])
# define IS_PATH_WITH_DIR(P) (strchr (P, '/') != NULL)
#endif

struct known_translation_t
{
  
  char *domainname;

  
  int category;

  
  int counter;

  
  struct loaded_l10nfile *domain;

  
  const char *translation;
  size_t translation_length;

  
  char msgid[ZERO];
};

#if defined HAVE_TSEARCH || defined _LIBC
# include <search.h>

static void *root;

# ifdef _LIBC
#  define tsearch __tsearch
# endif

static int
transcmp (const void *p1, const void *p2)
{
  const struct known_translation_t *s1;
  const struct known_translation_t *s2;
  int result;

  s1 = (const struct known_translation_t *) p1;
  s2 = (const struct known_translation_t *) p2;

  result = strcmp (s1->msgid, s2->msgid);
  if (result == 0)
    {
      result = strcmp (s1->domainname, s2->domainname);
      if (result == 0)
	result = s1->category - s2->category;
    }

  return result;
}
#endif

#ifndef INTVARDEF
# define INTVARDEF(name)
#endif
#ifndef INTUSE
# define INTUSE(name) name
#endif

const char _nl_default_default_domain[] attribute_hidden = "messages";

const char *_nl_current_default_domain attribute_hidden
     = _nl_default_default_domain;

#if defined __EMX__
extern const char _nl_default_dirname[];
#else
const char _nl_default_dirname[] = LOCALEDIR;
INTVARDEF (_nl_default_dirname)
#endif

struct binding *_nl_domain_bindings;

static char *plural_lookup (struct loaded_l10nfile *domain,
			    unsigned long int n,
			    const char *translation, size_t translation_len)
     internal_function;
static const char *guess_category_value (int category,
					 const char *categoryname)
     internal_function;
#ifdef _LIBC
# include "../locale/localeinfo.h"
# define category_to_name(category)	_nl_category_names[category]
#else
static const char *category_to_name (int category) internal_function;
#endif


#ifdef HAVE_ALLOCA
# define freea(p) 
# define ADD_BLOCK(list, address) 
# define FREE_BLOCKS(list) 
#else
struct block_list
{
  void *address;
  struct block_list *next;
};
# define ADD_BLOCK(list, addr)						      \
  do {									      \
    struct block_list *newp = (struct block_list *) malloc (sizeof (*newp));  \
							      \
    if (newp != NULL) {							      \
      newp->address = (addr);						      \
      newp->next = (list);						      \
      (list) = newp;							      \
    }									      \
  } while (0)
# define FREE_BLOCKS(list)						      \
  do {									      \
    while (list != NULL) {						      \
      struct block_list *old = list;					      \
      list = list->next;						      \
      free (old->address);						      \
      free (old);							      \
    }									      \
  } while (0)
# undef alloca
# define alloca(size) (malloc (size))
# define freea(p) free (p)
#endif	


#ifdef _LIBC
typedef struct transmem_list
{
  struct transmem_list *next;
  char data[ZERO];
} transmem_block_t;
static struct transmem_list *transmem_list;
#else
typedef unsigned char transmem_block_t;
#endif


#ifdef _LIBC
# define DCIGETTEXT __dcigettext
#else
# define DCIGETTEXT libintl_dcigettext
#endif

#ifdef _LIBC
__libc_rwlock_define_initialized (, _nl_state_lock attribute_hidden)
#endif

#ifdef _LIBC
# define ENABLE_SECURE __libc_enable_secure
# define DETERMINE_SECURE
#else
# ifndef HAVE_GETUID
#  define getuid() 0
# endif
# ifndef HAVE_GETGID
#  define getgid() 0
# endif
# ifndef HAVE_GETEUID
#  define geteuid() getuid()
# endif
# ifndef HAVE_GETEGID
#  define getegid() getgid()
# endif
static int enable_secure;
# define ENABLE_SECURE (enable_secure == 1)
# define DETERMINE_SECURE \
  if (enable_secure == 0)						      \
    {									      \
      if (getuid () != geteuid () || getgid () != getegid ())		      \
	enable_secure = 1;						      \
      else								      \
	enable_secure = -1;						      \
    }
#endif

#include "eval-plural.h"

char *
DCIGETTEXT (const char *domainname, const char *msgid1, const char *msgid2,
	    int plural, unsigned long int n, int category)
{
#ifndef HAVE_ALLOCA
  struct block_list *block_list = NULL;
#endif
  struct loaded_l10nfile *domain;
  struct binding *binding;
  const char *categoryname;
  const char *categoryvalue;
  char *dirname, *xdomainname;
  char *single_locale;
  char *retval;
  size_t retlen;
  int saved_errno;
#if defined HAVE_TSEARCH || defined _LIBC
  struct known_translation_t *search;
  struct known_translation_t **foundp = NULL;
  size_t msgid_len;
#endif
  size_t domainname_len;

  
  if (msgid1 == NULL)
    return NULL;

#ifdef _LIBC
  if (category < 0 || category >= __LC_LAST || category == LC_ALL)
    
    return (plural == 0
	    ? (char *) msgid1
	    
	    : n == 1 ? (char *) msgid1 : (char *) msgid2);
#endif

  __libc_rwlock_rdlock (_nl_state_lock);

  if (domainname == NULL)
    domainname = _nl_current_default_domain;

  
#ifdef LC_MESSAGES_COMPAT
  if (category == LC_MESSAGES_COMPAT)
    category = LC_MESSAGES;
#endif

#if defined HAVE_TSEARCH || defined _LIBC
  msgid_len = strlen (msgid1) + 1;

  search = (struct known_translation_t *)
	   alloca (offsetof (struct known_translation_t, msgid) + msgid_len);
  memcpy (search->msgid, msgid1, msgid_len);
  search->domainname = (char *) domainname;
  search->category = category;

  foundp = (struct known_translation_t **) tfind (search, &root, transcmp);
  freea (search);
  if (foundp != NULL && (*foundp)->counter == _nl_msg_cat_cntr)
    {
      
      if (plural)
	retval = plural_lookup ((*foundp)->domain, n, (*foundp)->translation,
				(*foundp)->translation_length);
      else
	retval = (char *) (*foundp)->translation;

      __libc_rwlock_unlock (_nl_state_lock);
      return retval;
    }
#endif

  
  saved_errno = errno;

  
  DETERMINE_SECURE;

  
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

  if (binding == NULL)
    dirname = (char *) INTUSE(_nl_default_dirname);
  else if (IS_ABSOLUTE_PATH (binding->dirname))
    dirname = binding->dirname;
  else
    {
      
      size_t dirname_len = strlen (binding->dirname) + 1;
      size_t path_max;
      char *ret;

      path_max = (unsigned int) PATH_MAX;
      path_max += 2;		

      for (;;)
	{
	  dirname = (char *) alloca (path_max + dirname_len);
	  ADD_BLOCK (block_list, dirname);

	  __set_errno (0);
	  ret = getcwd (dirname, path_max);
	  if (ret != NULL || errno != ERANGE)
	    break;

	  path_max += path_max / 2;
	  path_max += PATH_INCR;
	}

      if (ret == NULL)
	goto return_untranslated;

      stpcpy (stpcpy (strchr (dirname, '\0'), "/"), binding->dirname);
    }

  
  categoryname = category_to_name (category);
  categoryvalue = guess_category_value (category, categoryname);

  domainname_len = strlen (domainname);
  xdomainname = (char *) alloca (strlen (categoryname)
				 + domainname_len + 5);
  ADD_BLOCK (block_list, xdomainname);

  stpcpy (mempcpy (stpcpy (stpcpy (xdomainname, categoryname), "/"),
		  domainname, domainname_len),
	  ".mo");

  
  single_locale = (char *) alloca (strlen (categoryvalue) + 1);
  ADD_BLOCK (block_list, single_locale);


  while (1)
    {
      
      while (categoryvalue[0] != '\0' && categoryvalue[0] == ':')
	++categoryvalue;
      if (categoryvalue[0] == '\0')
	{
	  single_locale[0] = 'C';
	  single_locale[1] = '\0';
	}
      else
	{
	  char *cp = single_locale;
	  while (categoryvalue[0] != '\0' && categoryvalue[0] != ':')
	    *cp++ = *categoryvalue++;
	  *cp = '\0';

	  if (ENABLE_SECURE && IS_PATH_WITH_DIR (single_locale))
	    
	    continue;
	}

      if (strcmp (single_locale, "C") == 0
	  || strcmp (single_locale, "POSIX") == 0)
	break;

      domain = _nl_find_domain (dirname, single_locale, xdomainname, binding);

      if (domain != NULL)
	{
	  retval = _nl_find_msg (domain, binding, msgid1, &retlen);

	  if (retval == NULL)
	    {
	      int cnt;

	      for (cnt = 0; domain->successor[cnt] != NULL; ++cnt)
		{
		  retval = _nl_find_msg (domain->successor[cnt], binding,
					 msgid1, &retlen);

		  if (retval != NULL)
		    {
		      domain = domain->successor[cnt];
		      break;
		    }
		}
	    }

	  if (retval != NULL)
	    {
	      FREE_BLOCKS (block_list);
#if defined HAVE_TSEARCH || defined _LIBC
	      if (foundp == NULL)
		{
		  
		  struct known_translation_t *newp;

		  newp = (struct known_translation_t *)
		    malloc (offsetof (struct known_translation_t, msgid)
			    + msgid_len + domainname_len + 1);
		  if (newp != NULL)
		    {
		      newp->domainname =
			mempcpy (newp->msgid, msgid1, msgid_len);
		      memcpy (newp->domainname, domainname, domainname_len + 1);
		      newp->category = category;
		      newp->counter = _nl_msg_cat_cntr;
		      newp->domain = domain;
		      newp->translation = retval;
		      newp->translation_length = retlen;

		      
		      foundp = (struct known_translation_t **)
			tsearch (newp, &root, transcmp);
		      if (foundp == NULL
			  || __builtin_expect (*foundp != newp, 0))
			
			free (newp);
		    }
		}
	      else
		{
		  
		  (*foundp)->counter = _nl_msg_cat_cntr;
		  (*foundp)->domain = domain;
		  (*foundp)->translation = retval;
		  (*foundp)->translation_length = retlen;
		}
#endif
	      __set_errno (saved_errno);

	      
	      if (plural)
		retval = plural_lookup (domain, n, retval, retlen);

	      __libc_rwlock_unlock (_nl_state_lock);
	      return retval;
	    }
	}
    }

 return_untranslated:
  
  FREE_BLOCKS (block_list);
  __libc_rwlock_unlock (_nl_state_lock);
#if 0				
#ifndef _LIBC
  if (!ENABLE_SECURE)
    {
      extern void _nl_log_untranslated (const char *logfilename,
					const char *domainname,
					const char *msgid1, const char *msgid2,
					int plural);
      const char *logfilename = getenv ("GETTEXT_LOG_UNTRANSLATED");

      if (logfilename != NULL && logfilename[0] != '\0')
	_nl_log_untranslated (logfilename, domainname, msgid1, msgid2, plural);
    }
#endif
#endif
  __set_errno (saved_errno);
  return (plural == 0
	  ? (char *) msgid1
	  
	  : n == 1 ? (char *) msgid1 : (char *) msgid2);
}


char *
internal_function
_nl_find_msg (struct loaded_l10nfile *domain_file,
	      struct binding *domainbinding, const char *msgid,
	      size_t *lengthp)
{
  struct loaded_domain *domain;
  nls_uint32 nstrings;
  size_t act;
  char *result;
  size_t resultlen;

  if (domain_file->decided == 0)
    _nl_load_domain (domain_file, domainbinding);

  if (domain_file->data == NULL)
    return NULL;

  domain = (struct loaded_domain *) domain_file->data;

  nstrings = domain->nstrings;

  
  if (domain->hash_tab != NULL)
    {
      
      nls_uint32 len = strlen (msgid);
      nls_uint32 hash_val = hash_string (msgid);
      nls_uint32 idx = hash_val % domain->hash_size;
      nls_uint32 incr = 1 + (hash_val % (domain->hash_size - 2));

      while (1)
	{
	  nls_uint32 nstr =
	    W (domain->must_swap_hash_tab, domain->hash_tab[idx]);

	  if (nstr == 0)
	    
	    return NULL;

	  nstr--;

	  if (nstr < nstrings
	      ? W (domain->must_swap, domain->orig_tab[nstr].length) >= len
		&& (strcmp (msgid,
			    domain->data + W (domain->must_swap,
					      domain->orig_tab[nstr].offset))
		    == 0)
	      : domain->orig_sysdep_tab[nstr - nstrings].length > len
		&& (strcmp (msgid,
			    domain->orig_sysdep_tab[nstr - nstrings].pointer)
		    == 0))
	    {
	      act = nstr;
	      goto found;
	    }

	  if (idx >= domain->hash_size - incr)
	    idx -= domain->hash_size - incr;
	  else
	    idx += incr;
	}
      
    }
  else
    {
      size_t top, bottom;

      bottom = 0;
      top = nstrings;
      while (bottom < top)
	{
	  int cmp_val;

	  act = (bottom + top) / 2;
	  cmp_val = strcmp (msgid, (domain->data
				    + W (domain->must_swap,
					 domain->orig_tab[act].offset)));
	  if (cmp_val < 0)
	    top = act;
	  else if (cmp_val > 0)
	    bottom = act + 1;
	  else
	    goto found;
	}
      
      return NULL;
    }

 found:
  if (act < nstrings)
    {
      result = (char *)
	(domain->data + W (domain->must_swap, domain->trans_tab[act].offset));
      resultlen = W (domain->must_swap, domain->trans_tab[act].length) + 1;
    }
  else
    {
      result = (char *) domain->trans_sysdep_tab[act - nstrings].pointer;
      resultlen = domain->trans_sysdep_tab[act - nstrings].length;
    }

#if defined _LIBC || HAVE_ICONV
  if (domain->codeset_cntr
      != (domainbinding != NULL ? domainbinding->codeset_cntr : 0))
    {
      _nl_free_domain_conv (domain);
      _nl_init_domain_conv (domain_file, domain, domainbinding);
    }

  if (
# ifdef _LIBC
      domain->conv != (__gconv_t) -1
# else
#  if HAVE_ICONV
      domain->conv != (iconv_t) -1
#  endif
# endif
      )
    {

      if (domain->conv_tab == NULL
	  && ((domain->conv_tab =
		 (char **) calloc (nstrings + domain->n_sysdep_strings,
				   sizeof (char *)))
	      == NULL))
	
	domain->conv_tab = (char **) -1;

      if (__builtin_expect (domain->conv_tab == (char **) -1, 0))
	
	goto converted;

      if (domain->conv_tab[act] == NULL)
	{
	  __libc_lock_define_initialized (static, lock)
# define INITIAL_BLOCK_SIZE	4080
	  static unsigned char *freemem;
	  static size_t freemem_size;

	  const unsigned char *inbuf;
	  unsigned char *outbuf;
	  int malloc_count;
# ifndef _LIBC
	  transmem_block_t *transmem_list = NULL;
# endif

	  __libc_lock_lock (lock);

	  inbuf = (const unsigned char *) result;
	  outbuf = freemem + sizeof (size_t);

	  malloc_count = 0;
	  while (1)
	    {
	      transmem_block_t *newmem;
# ifdef _LIBC
	      size_t non_reversible;
	      int res;

	      if (freemem_size < sizeof (size_t))
		goto resize_freemem;

	      res = __gconv (domain->conv,
			     &inbuf, inbuf + resultlen,
			     &outbuf,
			     outbuf + freemem_size - sizeof (size_t),
			     &non_reversible);

	      if (res == __GCONV_OK || res == __GCONV_EMPTY_INPUT)
		break;

	      if (res != __GCONV_FULL_OUTPUT)
		{
		  __libc_lock_unlock (lock);
		  goto converted;
		}

	      inbuf = result;
# else
#  if HAVE_ICONV
	      const char *inptr = (const char *) inbuf;
	      size_t inleft = resultlen;
	      char *outptr = (char *) outbuf;
	      size_t outleft;

	      if (freemem_size < sizeof (size_t))
		goto resize_freemem;

	      outleft = freemem_size - sizeof (size_t);
	      if (iconv (domain->conv,
			 (ICONV_CONST char **) &inptr, &inleft,
			 &outptr, &outleft)
		  != (size_t) (-1))
		{
		  outbuf = (unsigned char *) outptr;
		  break;
		}
	      if (errno != E2BIG)
		{
		  __libc_lock_unlock (lock);
		  goto converted;
		}
#  endif
# endif

	    resize_freemem:
	      
	      if (malloc_count > 0)
		{
		  ++malloc_count;
		  freemem_size = malloc_count * INITIAL_BLOCK_SIZE;
		  newmem = (transmem_block_t *) realloc (transmem_list,
							 freemem_size);
# ifdef _LIBC
		  if (newmem != NULL)
		    transmem_list = transmem_list->next;
		  else
		    {
		      struct transmem_list *old = transmem_list;

		      transmem_list = transmem_list->next;
		      free (old);
		    }
# endif
		}
	      else
		{
		  malloc_count = 1;
		  freemem_size = INITIAL_BLOCK_SIZE;
		  newmem = (transmem_block_t *) malloc (freemem_size);
		}
	      if (__builtin_expect (newmem == NULL, 0))
		{
		  freemem = NULL;
		  freemem_size = 0;
		  __libc_lock_unlock (lock);
		  goto converted;
		}

# ifdef _LIBC
	      newmem->next = transmem_list;
	      transmem_list = newmem;

	      freemem = newmem->data;
	      freemem_size -= offsetof (struct transmem_list, data);
# else
	      transmem_list = newmem;
	      freemem = newmem;
# endif

	      outbuf = freemem + sizeof (size_t);
	    }

	  *(size_t *) freemem = outbuf - freemem - sizeof (size_t);
	  domain->conv_tab[act] = (char *) freemem;
	  
	  freemem_size -= outbuf - freemem;
	  freemem = outbuf;
	  freemem += freemem_size & (alignof (size_t) - 1);
	  freemem_size = freemem_size & ~ (alignof (size_t) - 1);

	  __libc_lock_unlock (lock);
	}

      result = domain->conv_tab[act] + sizeof (size_t);
      resultlen = *(size_t *) domain->conv_tab[act];
    }

 converted:
  

#endif 

  *lengthp = resultlen;
  return result;
}


static char *
internal_function
plural_lookup (struct loaded_l10nfile *domain, unsigned long int n,
	       const char *translation, size_t translation_len)
{
  struct loaded_domain *domaindata = (struct loaded_domain *) domain->data;
  unsigned long int index;
  const char *p;

  index = plural_eval (domaindata->plural, n);
  if (index >= domaindata->nplurals)
    index = 0;

  
  p = translation;
  while (index-- > 0)
    {
#ifdef _LIBC
      p = __rawmemchr (p, '\0');
#else
      p = strchr (p, '\0');
#endif
      
      p++;

      if (p >= translation + translation_len)
	return (char *) translation;
    }
  return (char *) p;
}

#ifndef _LIBC
static const char *
internal_function
category_to_name (int category)
{
  const char *retval;

  switch (category)
  {
#ifdef LC_COLLATE
  case LC_COLLATE:
    retval = "LC_COLLATE";
    break;
#endif
#ifdef LC_CTYPE
  case LC_CTYPE:
    retval = "LC_CTYPE";
    break;
#endif
#ifdef LC_MONETARY
  case LC_MONETARY:
    retval = "LC_MONETARY";
    break;
#endif
#ifdef LC_NUMERIC
  case LC_NUMERIC:
    retval = "LC_NUMERIC";
    break;
#endif
#ifdef LC_TIME
  case LC_TIME:
    retval = "LC_TIME";
    break;
#endif
#ifdef LC_MESSAGES
  case LC_MESSAGES:
    retval = "LC_MESSAGES";
    break;
#endif
#ifdef LC_RESPONSE
  case LC_RESPONSE:
    retval = "LC_RESPONSE";
    break;
#endif
#ifdef LC_ALL
  case LC_ALL:
    retval = "LC_ALL";
    break;
#endif
  default:
    
    retval = "LC_XXX";
  }

  return retval;
}
#endif

static const char *
internal_function
guess_category_value (int category, const char *categoryname)
{
  const char *language;
  const char *retval;

  language = getenv ("LANGUAGE");
  if (language != NULL && language[0] == '\0')
    language = NULL;

#ifdef _LIBC
  retval = __current_locale_name (category);
#else
  retval = _nl_locale_name (category, categoryname);
#endif

  return language != NULL && strcmp (retval, "C") != 0 ? language : retval;
}


#if !_LIBC && !HAVE_STPCPY
static char *
stpcpy (char *dest, const char *src)
{
  while ((*dest++ = *src++) != '\0')
     ;
  return dest - 1;
}
#endif

#if !_LIBC && !HAVE_MEMPCPY
static void *
mempcpy (void *dest, const void *src, size_t n)
{
  return (void *) ((char *) memcpy (dest, src, n) + n);
}
#endif


#ifdef _LIBC
libc_freeres_fn (free_mem)
{
  void *old;

  while (_nl_domain_bindings != NULL)
    {
      struct binding *oldp = _nl_domain_bindings;
      _nl_domain_bindings = _nl_domain_bindings->next;
      if (oldp->dirname != INTUSE(_nl_default_dirname))
	
	free (oldp->dirname);
      free (oldp->codeset);
      free (oldp);
    }

  if (_nl_current_default_domain != _nl_default_default_domain)
    
    free ((char *) _nl_current_default_domain);

  
  __tdestroy (root, free);
  root = NULL;

  while (transmem_list != NULL)
    {
      old = transmem_list;
      transmem_list = transmem_list->next;
      free (old);
    }
}
#endif
