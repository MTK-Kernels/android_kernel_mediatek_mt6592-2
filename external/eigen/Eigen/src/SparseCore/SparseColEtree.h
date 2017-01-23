// Copyright (C) 2012 Désiré Nuentsa-Wakam <desire.nuentsa_wakam@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed


/* 
 
 * NOTE: This file is the modified version of sp_coletree.c file in SuperLU 
 
 * -- SuperLU routine (version 3.1) --
 * Univ. of California Berkeley, Xerox Palo Alto Research Center,
 * and Lawrence Berkeley National Lab.
 * August 1, 2008
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
#ifndef SPARSE_COLETREE_H
#define SPARSE_COLETREE_H

namespace Eigen {

namespace internal {

 
template<typename Index, typename IndexVector>
Index etree_find (Index i, IndexVector& pp)
{
  Index p = pp(i); 
  Index gp = pp(p); 
  while (gp != p) 
  {
    pp(i) = gp; 
    i = gp; 
    p = pp(i);
    gp = pp(p);
  }
  return p; 
}

template <typename MatrixType, typename IndexVector>
int coletree(const MatrixType& mat, IndexVector& parent, IndexVector& firstRowElt, typename MatrixType::Index *perm=0)
{
  typedef typename MatrixType::Index Index;
  Index nc = mat.cols(); 
  Index m = mat.rows();
  Index diagSize = (std::min)(nc,m);
  IndexVector root(nc); 
  root.setZero();
  IndexVector pp(nc); 
  pp.setZero(); 
  parent.resize(mat.cols());
  
  Index row,col; 
  firstRowElt.resize(m);
  firstRowElt.setConstant(nc);
  firstRowElt.segment(0, diagSize).setLinSpaced(diagSize, 0, diagSize-1);
  bool found_diag;
  for (col = 0; col < nc; col++)
  {
    Index pcol = col;
    if(perm) pcol  = perm[col];
    for (typename MatrixType::InnerIterator it(mat, pcol); it; ++it)
    { 
      row = it.row();
      firstRowElt(row) = (std::min)(firstRowElt(row), col);
    }
  }
  Index rset, cset, rroot; 
  for (col = 0; col < nc; col++) 
  {
    found_diag = col>=m;
    pp(col) = col; 
    cset = col; 
    root(cset) = col; 
    parent(col) = nc; 
 
    Index pcol = col;
    if(perm) pcol  = perm[col];
    for (typename MatrixType::InnerIterator it(mat, pcol); it||!found_diag; ++it)
    { 
      Index i = col;
      if(it) i = it.index();
      if (i == col) found_diag = true;
      
      row = firstRowElt(i);
      if (row >= col) continue; 
      rset = internal::etree_find(row, pp); 
      rroot = root(rset);
      if (rroot != col) 
      {
        parent(rroot) = col; 
        pp(cset) = rset; 
        cset = rset; 
        root(cset) = col; 
      }
    }
  }
  return 0;  
}

template <typename Index, typename IndexVector>
void nr_etdfs (Index n, IndexVector& parent, IndexVector& first_kid, IndexVector& next_kid, IndexVector& post, Index postnum)
{
  Index current = n, first, next;
  while (postnum != n) 
  {
    
    first = first_kid(current);
    
    
    if (first == -1) 
    {
      
      post(current) = postnum++;
      
      
      next = next_kid(current); 
      while (next == -1) 
      {
        
        current = parent(current); 
        
        post(current) = postnum++;
        
        
        next = next_kid(current); 
      }
      
      if (postnum == n+1) return; 
      
      
      current = next; 
    }
    else 
    {
      current = first; 
    }
  }
}


template <typename Index, typename IndexVector>
void treePostorder(Index n, IndexVector& parent, IndexVector& post)
{
  IndexVector first_kid, next_kid; 
  Index postnum; 
  
  first_kid.resize(n+1); 
  next_kid.setZero(n+1);
  post.setZero(n+1);
  
  
  Index v, dad; 
  first_kid.setConstant(-1); 
  for (v = n-1; v >= 0; v--) 
  {
    dad = parent(v);
    next_kid(v) = first_kid(dad); 
    first_kid(dad) = v; 
  }
  
  
  postnum = 0; 
  internal::nr_etdfs(n, parent, first_kid, next_kid, post, postnum);
}

} 

} 

#endif 
