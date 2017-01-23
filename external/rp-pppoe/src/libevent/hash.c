/***********************************************************************
*
* hash.c
*
* Implementation of hash tables.  Each item inserted must include
* a hash_bucket member.
*
* Copyright (C) 2002 Roaring Penguin Software Inc.
*
* This software may be distributed under the terms of the GNU General
* Public License, Version 2 or (at your option) any later version.
*
* LIC: GPL
*
***********************************************************************/

static char const RCSID[] =
"$Id$";

#include "hash.h"

#include <limits.h>
#define BITS_IN_int     ( sizeof(int) * CHAR_BIT )
#define THREE_QUARTERS  ((int) ((BITS_IN_int * 3) / 4))
#define ONE_EIGHTH      ((int) (BITS_IN_int / 8))
#define HIGH_BITS       ( ~((unsigned int)(~0) >> ONE_EIGHTH ))

#define GET_BUCKET(tab, data) ((hash_bucket *) ((char *) (data) + (tab)->hash_offset))

#define GET_ITEM(tab, bucket) ((void *) (((char *) (void *) bucket) - (tab)->hash_offset))

static void *hash_next_cursor(hash_table *tab, hash_bucket *b);

void
hash_init(hash_table *tab,
	  size_t hash_offset,
	  unsigned int (*compute)(void *data),
	  int (*compare)(void *item1, void *item2))
{
    size_t i;

    tab->hash_offset = hash_offset;
    tab->compute_hash = compute;
    tab->compare = compare;
    for (i=0; i<HASHTAB_SIZE; i++) {
	tab->buckets[i] = NULL;
    }
    tab->num_entries = 0;
}

void
hash_insert(hash_table *tab,
	    void *item)
{
    hash_bucket *b = GET_BUCKET(tab, item);
    unsigned int val = tab->compute_hash(item);
    b->hashval = val;
    val %= HASHTAB_SIZE;
    b->prev = NULL;
    b->next = tab->buckets[val];
    if (b->next) {
	b->next->prev = b;
    }
    tab->buckets[val] = b;
    tab->num_entries++;
}

void
hash_remove(hash_table *tab,
	    void *item)
{
    hash_bucket *b = GET_BUCKET(tab, item);
    unsigned int val = b->hashval % HASHTAB_SIZE;

    if (b->prev) {
	b->prev->next = b->next;
    } else {
	tab->buckets[val] = b->next;
    }
    if (b->next) {
	b->next->prev = b->prev;
    }
    tab->num_entries--;
}

void *
hash_find(hash_table *tab,
	  void *item)
{
    unsigned int val = tab->compute_hash(item) % HASHTAB_SIZE;
    hash_bucket *b;
    for (b = tab->buckets[val]; b; b = b->next) {
	void *item2 = GET_ITEM(tab, b);
	if (!tab->compare(item, item2)) return item2;
    }
    return NULL;
}

void *
hash_find_next(hash_table *tab,
	       void *item)
{
    hash_bucket *b = GET_BUCKET(tab, item);
    for (b = b->next; b; b = b->next) {
	void *item2 = GET_ITEM(tab, b);
	if (!tab->compare(item, item2)) return item2;
    }
    return NULL;
}
void *
hash_start(hash_table *tab, void **cursor)
{
    int i;
    for (i=0; i<HASHTAB_SIZE; i++) {
	if (tab->buckets[i]) {
	    *cursor = hash_next_cursor(tab, tab->buckets[i]);
	    return GET_ITEM(tab, tab->buckets[i]);
	}
    }
    *cursor = NULL;
    return NULL;
}

void *
hash_next(hash_table *tab, void **cursor)
{
    hash_bucket *b;

    if (!*cursor) return NULL;

    b = (hash_bucket *) *cursor;
    *cursor = hash_next_cursor(tab, b);
    return GET_ITEM(tab, b);
}

static void *
hash_next_cursor(hash_table *tab, hash_bucket *b)
{
    unsigned int i;
    if (!b) return NULL;
    if (b->next) return b->next;

    i = b->hashval % HASHTAB_SIZE;
    for (++i; i<HASHTAB_SIZE; ++i) {
	if (tab->buckets[i]) return tab->buckets[i];
    }
    return NULL;
}

size_t
hash_num_entries(hash_table *tab)
{
    return tab->num_entries;
}

unsigned int
hash_pjw(char const * str)
{
    unsigned int hash_value, i;

    for (hash_value = 0; *str; ++str) {
        hash_value = ( hash_value << ONE_EIGHTH ) + *str;
        if (( i = hash_value & HIGH_BITS ) != 0 ) {
            hash_value =
                ( hash_value ^ ( i >> THREE_QUARTERS )) &
		~HIGH_BITS;
	}
    }
    return hash_value;
}
