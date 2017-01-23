/* Copyright (C) 2000-2010 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#include <stddef.h>



#ifdef ITERATE
# define FIRST(name)	name##_ent *first;
# define NEXT(name)	struct name##_ent *next;
#else
# define FIRST(name)
# define NEXT(name)
#endif

#ifndef HASHTYPE
# define HASHTYPE unsigned long int
#endif


extern size_t next_prime (size_t seed);


#define _DYNHASHENTTYPE(name) \
  typedef struct name##_ent						      \
  {									      \
    HASHTYPE hashval;							      \
    TYPE data;								      \
    NEXT (name)								      \
  } name##_ent
#define DYNHASHENTTYPE(name) _DYNHASHENTTYPE (name)
DYNHASHENTTYPE (NAME);


#define _DYNHASHTYPE(name) \
typedef struct								      \
{									      \
  size_t size;								      \
  size_t filled;							      \
  name##_ent *table;							      \
  FIRST	(name)								      \
} name
#define DYNHASHTYPE(name) _DYNHASHTYPE (name)
DYNHASHTYPE (NAME);



#define _FUNCTIONS(name) \
					      \
extern int name##_init (name *htab, size_t init_size);			      \
									      \
				      \
extern int name##_free (name *htab);					      \
									      \
						      \
extern int name##_insert (name *htab, HASHTYPE hval, TYPE data);	      \
									      \
			      \
extern int name##_overwrite (name *htab, HASHTYPE hval, TYPE data);	      \
									      \
					      \
extern TYPE name##_find (name *htab, HASHTYPE hval, TYPE val);
#define FUNCTIONS(name) _FUNCTIONS (name)
FUNCTIONS (NAME)


#ifdef ITERATE
# define _XFUNCTIONS(name) \
					      \
extern TYPE name##_iterate (name *htab, void **ptr);
# define XFUNCTIONS(name) _XFUNCTIONS (name)
XFUNCTIONS (NAME)
#endif

#ifndef NO_UNDEF
# undef DYNHASHENTTYPE
# undef DYNHASHTYPE
# undef FUNCTIONS
# undef _FUNCTIONS
# undef XFUNCTIONS
# undef _XFUNCTIONS
# undef NAME
# undef TYPE
# undef ITERATE
# undef COMPARE
# undef FIRST
# undef NEXT
#endif
