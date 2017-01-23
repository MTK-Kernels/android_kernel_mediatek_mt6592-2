

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2005-2013 Nicholas Nethercote
      njn@valgrind.org

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
*/

#ifndef __PUB_TOOL_HASHTABLE_H
#define __PUB_TOOL_HASHTABLE_H

#include "pub_tool_basics.h"   



typedef
   struct _VgHashNode {
      struct _VgHashNode * next;
      UWord              key;
   }
   VgHashNode;

typedef struct _VgHashTable VgHashTable;

extern VgHashTable *VG_(HT_construct) ( const HChar* name );

extern Int VG_(HT_count_nodes) ( const VgHashTable *table );

extern void VG_(HT_add_node) ( VgHashTable *t, void* node );

extern void* VG_(HT_lookup) ( const VgHashTable *table, UWord key );

extern void* VG_(HT_remove) ( VgHashTable *table, UWord key );

typedef Word  (*HT_Cmp_t) ( const void* node1, const void* node2 );

extern void* VG_(HT_gen_lookup) ( const VgHashTable *table, const void* node,
                                  HT_Cmp_t cmp );
extern void* VG_(HT_gen_remove) ( VgHashTable *table, const void* node,
                                  HT_Cmp_t cmp );

extern void VG_(HT_print_stats) ( const VgHashTable *table, HT_Cmp_t cmp );

extern VgHashNode** VG_(HT_to_array) ( const VgHashTable *table,
                                        UInt* n_elems );

extern void VG_(HT_ResetIter) ( VgHashTable *table );

extern void* VG_(HT_Next) ( VgHashTable *table );

extern void VG_(HT_destruct) ( VgHashTable *table, void(*freenode_fn)(void*) );


#endif   

