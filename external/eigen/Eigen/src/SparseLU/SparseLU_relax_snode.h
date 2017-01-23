// Copyright (C) 2012 Désiré Nuentsa-Wakam <desire.nuentsa_wakam@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

/* This file is a modified version of heap_relax_snode.c file in SuperLU
 * -- SuperLU routine (version 3.0) --
 * Univ. of California Berkeley, Xerox Palo Alto Research Center,
 * and Lawrence Berkeley National Lab.
 * October 15, 2003
 *
 * Copyright (c) 1994 by Xerox Corporation.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY
 * EXPRESSED OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program for any
 * purpose, provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is
 * granted, provided the above notices are retained, and a notice that
 * the code was modified is included with the above copyright notice.
 */

#ifndef SPARSELU_RELAX_SNODE_H
#define SPARSELU_RELAX_SNODE_H

namespace Eigen {

namespace internal {
 
template <typename Scalar, typename Index>
void SparseLUImpl<Scalar,Index>::relax_snode (const Index n, IndexVector& et, const Index relax_columns, IndexVector& descendants, IndexVector& relax_end)
{
  
  
  Index j, parent; 
  relax_end.setConstant(emptyIdxLU);
  descendants.setZero();
  for (j = 0; j < n; j++) 
  {
    parent = et(j);
    if (parent != n) 
      descendants(parent) += descendants(j) + 1;
  }
  
  Index snode_start; 
  for (j = 0; j < n; )
  {
    parent = et(j);
    snode_start = j; 
    while ( parent != n && descendants(parent) < relax_columns ) 
    {
      j = parent; 
      parent = et(j);
    }
    
    relax_end(snode_start) = j; 
    j++;
    
    while (descendants(j) != 0 && j < n) j++;
  } 
  
}

} 

} 
#endif
