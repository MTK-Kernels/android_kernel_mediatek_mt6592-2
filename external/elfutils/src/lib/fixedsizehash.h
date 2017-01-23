/* Fixed size hash table with internal linking.
   Copyright (C) 2000, 2001, 2002, 2004, 2005 Red Hat, Inc.
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>
#include <sys/param.h>

#include <system.h>

#define CONCAT(t1,t2) __CONCAT (t1,t2)



extern size_t next_prime (size_t seed);


#ifndef HASHTYPE
# define HASHTYPE size_t
#endif

#ifndef CLASS
# define CLASS
#endif

#ifndef PREFIX
# define PREFIX
#endif


struct CONCAT(PREFIX,fshash)
{
  size_t nslots;
  struct CONCAT(PREFIX,fshashent)
  {
    HASHTYPE hval;
#ifdef STORE_POINTER
# define ENTRYP(el) (el).entry
    TYPE *entry;
#else
# define ENTRYP(el) &(el).entry
    TYPE entry;
#endif
  } table[0];
};


CLASS struct CONCAT(PREFIX,fshash) *
CONCAT(PREFIX,fshash_init) (size_t nelems)
{
  struct CONCAT(PREFIX,fshash) *result;
  const size_t max_size_t = ~((size_t) 0);

  if (nelems >= (max_size_t / 3) * 2)
    {
      errno = EINVAL;
      return NULL;
    }

  
  nelems = next_prime (MAX ((nelems * 3) / 2, 10));

  
  result = (struct CONCAT(PREFIX,fshash) *)
    xcalloc (sizeof (struct CONCAT(PREFIX,fshash))
	     + (nelems + 1) * sizeof (struct CONCAT(PREFIX,fshashent)), 1);
  if (result == NULL)
    return NULL;

  result->nslots = nelems;

  return result;
}


#ifndef NO_FINI_FCT
CLASS void
CONCAT(PREFIX,fshash_fini) (struct CONCAT(PREFIX,fshash) *htab)
{
  free (htab);
}
#endif


static struct CONCAT(PREFIX,fshashent) *
CONCAT(PREFIX,fshash_lookup) (struct CONCAT(PREFIX,fshash) *htab,
			      HASHTYPE hval, TYPE *data)
{
  size_t idx = 1 + hval % htab->nslots;

  if (htab->table[idx].hval != 0)
    {
      HASHTYPE hash;

      
      if (htab->table[idx].hval == hval
	  && COMPARE (data, ENTRYP (htab->table[idx])) == 0)
	return &htab->table[idx];

      
      hash = 1 + hval % (htab->nslots - 2);

      do
	{
	  if (idx <= hash)
	    idx = htab->nslots + idx - hash;
	  else
	    idx -= hash;

	  if (htab->table[idx].hval == hval
	      && COMPARE (data, ENTRYP(htab->table[idx])) == 0)
	    return &htab->table[idx];
	}
      while (htab->table[idx].hval != 0);
    }

  return &htab->table[idx];
}


CLASS int
__attribute__ ((unused))
CONCAT(PREFIX,fshash_insert) (struct CONCAT(PREFIX,fshash) *htab,
			      const char *str,
			      size_t len __attribute__ ((unused)), TYPE *data)
{
  HASHTYPE hval = HASHFCT (str, len ?: strlen (str));
  struct CONCAT(PREFIX,fshashent) *slot;

  slot = CONCAT(PREFIX,fshash_lookup) (htab, hval, data);
 if (slot->hval != 0)
    
    return -1;

  slot->hval = hval;
#ifdef STORE_POINTER
  slot->entry = data;
#else
  slot->entry = *data;
#endif

  return 0;
}


#ifdef INSERT_HASH
CLASS int
__attribute__ ((unused))
CONCAT(PREFIX,fshash_insert_hash) (struct CONCAT(PREFIX,fshash) *htab,
				   HASHTYPE hval, TYPE *data)
{
  struct CONCAT(PREFIX,fshashent) *slot;

  slot = CONCAT(PREFIX,fshash_lookup) (htab, hval, data);
  if (slot->hval != 0)
    
    return -1;

  slot->hval = hval;
#ifdef STORE_POINTER
  slot->entry = data;
#else
  slot->entry = *data;
#endif

  return 0;
}
#endif


CLASS int
__attribute__ ((unused))
CONCAT(PREFIX,fshash_overwrite) (struct CONCAT(PREFIX,fshash) *htab,
				 const char *str,
				 size_t len __attribute__ ((unused)),
				 TYPE *data)
{
  HASHTYPE hval = HASHFCT (str, len ?: strlen (str));
  struct CONCAT(PREFIX,fshashent) *slot;

  slot = CONCAT(PREFIX,fshash_lookup) (htab, hval, data);
  slot->hval = hval;
#ifdef STORE_POINTER
  slot->entry = data;
#else
  slot->entry = *data;
#endif

  return 0;
}


CLASS const TYPE *
CONCAT(PREFIX,fshash_find) (const struct CONCAT(PREFIX,fshash) *htab,
			    const char *str,
			    size_t len __attribute__ ((unused)), TYPE *data)
{
  HASHTYPE hval = HASHFCT (str, len ?: strlen (str));
  struct CONCAT(PREFIX,fshashent) *slot;

  slot = CONCAT(PREFIX,fshash_lookup) ((struct CONCAT(PREFIX,fshash) *) htab,
				       hval, data);
  if (slot->hval == 0)
    
    return NULL;

  return ENTRYP(*slot);
}


#undef TYPE
#undef HASHFCT
#undef HASHTYPE
#undef COMPARE
#undef CLASS
#undef PREFIX
#undef INSERT_HASH
#undef STORE_POINTER
#undef NO_FINI_FCT
