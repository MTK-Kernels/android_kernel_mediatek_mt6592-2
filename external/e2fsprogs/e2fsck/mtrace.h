/* Declarations for `malloc' and friends.
   Copyright 1990, 1991, 1992 Free Software Foundation, Inc.
		  Written May 1989 by Mike Haertel.

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

#ifndef _MTRACE_H

#define _MTRACE_H	1

#ifdef	__cplusplus
extern "C"
{
#endif

#if defined (__cplusplus) || (defined (__STDC__) && __STDC__)
#undef	__P
#define	__P(args)	args
#undef	__ptr_t
#define	__ptr_t		void *
#else 
#undef	__P
#define	__P(args)	()
#undef	const
#define	const
#undef	__ptr_t
#define	__ptr_t		char *
#endif 

#ifndef	NULL
#define	NULL	0
#endif

#ifdef	__STDC__
#include <stddef.h>
#else
#undef	size_t
#define	size_t		unsigned int
#undef	ptrdiff_t
#define	ptrdiff_t	int
#endif


extern __ptr_t malloc __P ((size_t __size));
extern __ptr_t realloc __P ((__ptr_t __ptr, size_t __size));
extern __ptr_t calloc __P ((size_t __nmemb, size_t __size));
extern void free __P ((__ptr_t __ptr));

extern __ptr_t memalign __P ((size_t __alignment, size_t __size));

extern __ptr_t valloc __P ((size_t __size));


#ifdef _MALLOC_INTERNAL

#include <stdio.h>		

#if	defined(__GNU_LIBRARY__) || defined(STDC_HEADERS) || defined(USG)
#include <string.h>
#else
#ifndef memset
#define	memset(s, zero, n)	bzero ((s), (n))
#endif
#ifndef memcpy
#define	memcpy(d, s, n)		bcopy ((s), (d), (n))
#endif
#endif


#if	defined(__GNU_LIBRARY__) || defined(__STDC__)
#include <limits.h>
#else
#define	CHAR_BIT	8
#endif

#define INT_BIT		(CHAR_BIT * sizeof(int))
#define BLOCKLOG	(INT_BIT > 16 ? 12 : 9)
#define BLOCKSIZE	(1 << BLOCKLOG)
#define BLOCKIFY(SIZE)	(((SIZE) + BLOCKSIZE - 1) / BLOCKSIZE)

#define HEAP		(INT_BIT > 16 ? 4194304 : 65536)

#define FINAL_FREE_BLOCKS	8

typedef union
  {
    
    struct
      {
	int type;
	union
	  {
	    struct
	      {
		size_t nfree;	
		size_t first;	
	      } frag;
	    
	    size_t size;
	  } info;
      } busy;
    struct
      {
	size_t size;		
	size_t next;		
	size_t prev;		
      } free;
  } malloc_info;

extern char *_heapbase;

extern malloc_info *_heapinfo;

#define BLOCK(A)	(((char *) (A) - _heapbase) / BLOCKSIZE + 1)
#define ADDRESS(B)	((__ptr_t) (((B) - 1) * BLOCKSIZE + _heapbase))

extern size_t _heapindex;

extern size_t _heaplimit;

struct list
  {
    struct list *next;
    struct list *prev;
  };

extern struct list _fraghead[];

struct alignlist
  {
    struct alignlist *next;
    __ptr_t aligned;		
    __ptr_t exact;		
  };
extern struct alignlist *_aligned_blocks;

extern size_t _chunks_used;
extern size_t _bytes_used;
extern size_t _chunks_free;
extern size_t _bytes_free;

extern void _free_internal __P ((__ptr_t __ptr));

#endif 

extern __ptr_t (*__morecore) __P ((ptrdiff_t __size));

extern __ptr_t __default_morecore __P ((ptrdiff_t __size));

extern int __malloc_initialized;

extern void (*__free_hook) __P ((__ptr_t __ptr));
extern __ptr_t (*__malloc_hook) __P ((size_t __size));
extern __ptr_t (*__realloc_hook) __P ((__ptr_t __ptr, size_t __size));

extern void mcheck __P ((void (*__func) __P ((void))));

extern void mtrace __P ((void));

struct mstats
  {
    size_t bytes_total;		
    size_t chunks_used;		
    size_t bytes_used;		
    size_t chunks_free;		
    size_t bytes_free;		
  };

extern struct mstats mstats __P ((void));

#ifdef	__cplusplus
}
#endif

#endif 
