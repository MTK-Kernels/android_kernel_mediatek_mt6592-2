

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

#include "pub_core_basics.h"
#include "pub_core_debuglog.h"
#include "pub_core_hashtable.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcbase.h"
#include "pub_core_libcprint.h"
#include "pub_core_mallocfree.h"


#define CHAIN_NO(key,tbl) (((UWord)(key)) % tbl->n_chains)

struct _VgHashTable {
   UInt         n_chains;   
   UInt         n_elements;
   VgHashNode*  iterNode;   
   UInt         iterChain;  
   VgHashNode** chains;     
   Bool         iterOK;     
   const HChar* name;       
};

#define N_HASH_PRIMES 20

static const SizeT primes[N_HASH_PRIMES] = {
         769UL,         1543UL,         3079UL,          6151UL,
       12289UL,        24593UL,        49157UL,         98317UL,
      196613UL,       393241UL,       786433UL,       1572869UL,
     3145739UL,      6291469UL,     12582917UL,      25165843UL,
    50331653UL,    100663319UL,    201326611UL,     402653189UL
};


VgHashTable *VG_(HT_construct) ( const HChar* name )
{
   
   SizeT       n_chains = primes[0];
   SizeT       sz       = n_chains * sizeof(VgHashNode*);
   VgHashTable *table   = VG_(calloc)("hashtable.Hc.1",
                                      1, sizeof(struct _VgHashTable));
   table->chains        = VG_(calloc)("hashtable.Hc.2", 1, sz);
   table->n_chains      = n_chains;
   table->n_elements    = 0;
   table->iterOK        = True;
   table->name          = name;
   vg_assert(name);
   return table;
}

Int VG_(HT_count_nodes) ( const VgHashTable *table )
{
   return table->n_elements;
}

static void resize ( VgHashTable *table )
{
   Int          i;
   SizeT        sz;
   SizeT        old_chains = table->n_chains;
   SizeT        new_chains = old_chains + 1;
   VgHashNode** chains;
   VgHashNode * node;

   
   if (old_chains == primes[N_HASH_PRIMES-1])
      return;

   vg_assert(old_chains >= primes[0] 
             && old_chains < primes[N_HASH_PRIMES-1]);

   for (i = 0; i < N_HASH_PRIMES; i++) {
      if (primes[i] > new_chains) {
         new_chains = primes[i];
         break;
      }
   }

   vg_assert(new_chains > old_chains);
   vg_assert(new_chains > primes[0] 
             && new_chains <= primes[N_HASH_PRIMES-1]);

   VG_(debugLog)(
      1, "hashtable",
         "resizing table `%s' from %lu to %lu (total elems %lu)\n",
         table->name, (UWord)old_chains, (UWord)new_chains,
         (UWord)table->n_elements );

   table->n_chains = new_chains;
   sz = new_chains * sizeof(VgHashNode*);
   chains = VG_(calloc)("hashtable.resize.1", 1, sz);

   for (i = 0; i < old_chains; i++) {
      node = table->chains[i];
      while (node != NULL) {
         VgHashNode* next = node->next;
         UWord chain = CHAIN_NO(node->key, table);
         node->next = chains[chain];
         chains[chain] = node;
         node = next;
      }
   }

   VG_(free)(table->chains);
   table->chains = chains;
}

void VG_(HT_add_node) ( VgHashTable *table, void* vnode )
{
   VgHashNode* node     = (VgHashNode*)vnode;
   UWord chain          = CHAIN_NO(node->key, table);
   node->next           = table->chains[chain];
   table->chains[chain] = node;
   table->n_elements++;
   if ( (1 * (ULong)table->n_elements) > (1 * (ULong)table->n_chains) ) {
      resize(table);
   }

   
   table->iterOK = False;
}

void* VG_(HT_lookup) ( const VgHashTable *table, UWord key )
{
   VgHashNode* curr = table->chains[ CHAIN_NO(key, table) ];

   while (curr) {
      if (key == curr->key) {
         return curr;
      }
      curr = curr->next;
   }
   return NULL;
}

void* VG_(HT_gen_lookup) ( const VgHashTable *table, const void* node,
                           HT_Cmp_t cmp )
{
   const VgHashNode* hnode = node; 
   VgHashNode* curr = table->chains[ CHAIN_NO(hnode->key, table) ]; 

   while (curr) {
      if (hnode->key == curr->key && cmp (hnode, curr) == 0) { 
         return curr;
      }
      curr = curr->next;
   }
   return NULL;
}

void* VG_(HT_remove) ( VgHashTable *table, UWord key )
{
   UWord        chain         = CHAIN_NO(key, table);
   VgHashNode*  curr          =   table->chains[chain];
   VgHashNode** prev_next_ptr = &(table->chains[chain]);

   
   table->iterOK = False;

   while (curr) {
      if (key == curr->key) {
         *prev_next_ptr = curr->next;
         table->n_elements--;
         return curr;
      }
      prev_next_ptr = &(curr->next);
      curr = curr->next;
   }
   return NULL;
}

void* VG_(HT_gen_remove) ( VgHashTable *table, const void* node, HT_Cmp_t cmp  )
{
   const VgHashNode* hnode    = node; 
   UWord        chain         = CHAIN_NO(hnode->key, table); 
   VgHashNode*  curr          =   table->chains[chain];
   VgHashNode** prev_next_ptr = &(table->chains[chain]);

   
   table->iterOK = False;

   while (curr) {
      if (hnode->key == curr->key && cmp(hnode, curr) == 0) { 
         *prev_next_ptr = curr->next;
         table->n_elements--;
         return curr;
      }
      prev_next_ptr = &(curr->next);
      curr = curr->next;
   }
   return NULL;
}

void VG_(HT_print_stats) ( const VgHashTable *table, HT_Cmp_t cmp )
{
   #define MAXOCCUR 20
   UInt elt_occurences[MAXOCCUR+1];
   UInt key_occurences[MAXOCCUR+1];
   UInt cno_occurences[MAXOCCUR+1];
   #define INCOCCUR(occur,n) (n >= MAXOCCUR ? occur[MAXOCCUR]++ : occur[n]++)
   UInt i;
   UInt nkey, nelt, ncno;
   VgHashNode *cnode, *node;

   VG_(memset)(key_occurences, 0, sizeof(key_occurences));
   VG_(memset)(elt_occurences, 0, sizeof(elt_occurences));
   VG_(memset)(cno_occurences, 0, sizeof(cno_occurences));

   
   
   
   for (i = 0; i < table->n_chains; i++) {
      ncno = 0;
      for (cnode = table->chains[i]; cnode != NULL; cnode = cnode->next) {
         ncno++;

         nkey = 0;
         
         for (node = table->chains[i]; node != cnode; node = node->next) {
            if (node->key == cnode->key)
               nkey++;
         }
         
         if (nkey == 0) {
            for (node = cnode; node != NULL; node = node->next) {
               if (node->key == cnode->key)
                  nkey++;
            }
            INCOCCUR(key_occurences, nkey);
         }

         nelt = 0;
         
         for (node = table->chains[i]; node != cnode; node = node->next) {
            if (node->key == cnode->key
                && (cmp == NULL || cmp (node, cnode) == 0)) {
               nelt++;
            }
         }
         
         if (nelt == 0) {
            for (node = cnode; node != NULL; node = node->next) {
               if (node->key == cnode->key
                   && (cmp == NULL || cmp (node, cnode) == 0)) {
                  nelt++;
               }
            }
            INCOCCUR(elt_occurences, nelt);
         }
      }
      INCOCCUR(cno_occurences, ncno);
   }

   VG_(message)(Vg_DebugMsg, 
                "nr occurences of"
                " chains of len N,"
                " N-plicated keys,"
                " N-plicated elts\n");
   nkey = nelt = ncno = 0;
   for (i = 0; i <= MAXOCCUR; i++) {
      if (elt_occurences[i] > 0 
          || key_occurences[i] > 0 
          || cno_occurences[i] > 0)
         VG_(message)(Vg_DebugMsg,
                      "%s=%2d : nr chain %6d, nr keys %6d, nr elts %6d\n",
                      i == MAXOCCUR ? ">" : "N", i,
                      cno_occurences[i], key_occurences[i], elt_occurences[i]);
      nkey += key_occurences[i];
      nelt += elt_occurences[i];
      ncno += cno_occurences[i];
   }
   VG_(message)(Vg_DebugMsg, 
                "total nr of unique   slots: %6d, keys %6d, elts %6d."
                " Avg chain len %3.1f\n",
                ncno, nkey, nelt,
                (Double)nelt/(Double)(ncno == cno_occurences[0] ?
                                      1 : ncno - cno_occurences[0]));
}


VgHashNode** VG_(HT_to_array) (const VgHashTable *table,  UInt* n_elems)
{
   UInt       i, j;
   VgHashNode** arr;
   VgHashNode*  node;

   *n_elems = table->n_elements;
   if (*n_elems == 0)
      return NULL;

   arr = VG_(malloc)( "hashtable.Hta.1", *n_elems * sizeof(VgHashNode*) );

   j = 0;
   for (i = 0; i < table->n_chains; i++) {
      for (node = table->chains[i]; node != NULL; node = node->next) {
         arr[j++] = node;
      }
   }
   vg_assert(j == *n_elems);

   return arr;
}

void VG_(HT_ResetIter)(VgHashTable *table)
{
   vg_assert(table);
   table->iterNode  = NULL;
   table->iterChain = 0;
   table->iterOK    = True;
}

void* VG_(HT_Next)(VgHashTable *table)
{
   Int i;
   vg_assert(table);
   vg_assert(table->iterOK);

   if (table->iterNode && table->iterNode->next) {
      table->iterNode = table->iterNode->next;
      return table->iterNode;
   }

   for (i = table->iterChain; i < table->n_chains; i++) {
      if (table->chains[i]) {
         table->iterNode  = table->chains[i];
         table->iterChain = i + 1;  
         return table->iterNode;
      }
   }
   return NULL;
}

void VG_(HT_destruct)(VgHashTable *table, void(*freenode_fn)(void*))
{
   UInt       i;
   VgHashNode *node, *node_next;

   for (i = 0; i < table->n_chains; i++) {
      for (node = table->chains[i]; node != NULL; node = node_next) {
         node_next = node->next;
         freenode_fn(node);
      }
   }
   VG_(free)(table->chains);
   VG_(free)(table);
}

