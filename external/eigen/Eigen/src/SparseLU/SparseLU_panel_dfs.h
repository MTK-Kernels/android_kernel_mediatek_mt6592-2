// Copyright (C) 2012 Désiré Nuentsa-Wakam <desire.nuentsa_wakam@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

/* 
 
 * NOTE: This file is the modified version of [s,d,c,z]panel_dfs.c file in SuperLU 
 
 * -- SuperLU routine (version 2.0) --
 * Univ. of California Berkeley, Xerox Palo Alto Research Center,
 * and Lawrence Berkeley National Lab.
 * November 15, 1997
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
#ifndef SPARSELU_PANEL_DFS_H
#define SPARSELU_PANEL_DFS_H

namespace Eigen {

namespace internal {
  
template<typename IndexVector>
struct panel_dfs_traits
{
  typedef typename IndexVector::Scalar Index;
  panel_dfs_traits(Index jcol, Index* marker)
    : m_jcol(jcol), m_marker(marker)
  {}
  bool update_segrep(Index krep, Index jj)
  {
    if(m_marker[krep]<m_jcol)
    {
      m_marker[krep] = jj; 
      return true;
    }
    return false;
  }
  void mem_expand(IndexVector& , Index , Index ) {}
  enum { ExpandMem = false };
  Index m_jcol;
  Index* m_marker;
};


template <typename Scalar, typename Index>
template <typename Traits>
void SparseLUImpl<Scalar,Index>::dfs_kernel(const Index jj, IndexVector& perm_r,
                   Index& nseg, IndexVector& panel_lsub, IndexVector& segrep,
                   Ref<IndexVector> repfnz_col, IndexVector& xprune, Ref<IndexVector> marker, IndexVector& parent,
                   IndexVector& xplore, GlobalLU_t& glu,
                   Index& nextl_col, Index krow, Traits& traits
                  )
{
  
  Index kmark = marker(krow);
      
  
  marker(krow) = jj; 
  Index kperm = perm_r(krow); 
  if (kperm == emptyIdxLU ) {
    
    panel_lsub(nextl_col++) = krow;  
    
    traits.mem_expand(panel_lsub, nextl_col, kmark);
  }
  else 
  {
    
    
    
    Index krep = glu.xsup(glu.supno(kperm)+1) - 1; 
    
    Index myfnz = repfnz_col(krep); 
    
    if (myfnz != emptyIdxLU )
    {
      
      if (myfnz > kperm ) repfnz_col(krep) = kperm; 
      
    }
    else 
    {
      
      Index oldrep = emptyIdxLU; 
      parent(krep) = oldrep; 
      repfnz_col(krep) = kperm; 
      Index xdfs =  glu.xlsub(krep); 
      Index maxdfs = xprune(krep); 
      
      Index kpar;
      do 
      {
        
        while (xdfs < maxdfs) 
        {
          Index kchild = glu.lsub(xdfs); 
          xdfs++; 
          Index chmark = marker(kchild); 
          
          if (chmark != jj ) 
          {
            marker(kchild) = jj; 
            Index chperm = perm_r(kchild); 
            
            if (chperm == emptyIdxLU) 
            {
              
              panel_lsub(nextl_col++) = kchild;
              traits.mem_expand(panel_lsub, nextl_col, chmark);
            }
            else
            {
              
              
              
              Index chrep = glu.xsup(glu.supno(chperm)+1) - 1; 
              myfnz = repfnz_col(chrep); 
              
              if (myfnz != emptyIdxLU) 
              { 
                if (myfnz > chperm) 
                  repfnz_col(chrep) = chperm; 
              }
              else 
              { 
                xplore(krep) = xdfs; 
                oldrep = krep; 
                krep = chrep; 
                parent(krep) = oldrep; 
                repfnz_col(krep) = chperm; 
                xdfs = glu.xlsub(krep); 
                maxdfs = xprune(krep); 
                
              } 
            } 
                
          } 
        } 
        
        
        
        
        
        
        if(traits.update_segrep(krep,jj))
        
        {
          segrep(nseg) = krep; 
          ++nseg; 
          
        }
        
        kpar = parent(krep); 
        if (kpar == emptyIdxLU) 
          break; 
        krep = kpar; 
        xdfs = xplore(krep); 
        maxdfs = xprune(krep); 

      } while (kpar != emptyIdxLU); 
      
    } 

  } 
}


template <typename Scalar, typename Index>
void SparseLUImpl<Scalar,Index>::panel_dfs(const Index m, const Index w, const Index jcol, MatrixType& A, IndexVector& perm_r, Index& nseg, ScalarVector& dense, IndexVector& panel_lsub, IndexVector& segrep, IndexVector& repfnz, IndexVector& xprune, IndexVector& marker, IndexVector& parent, IndexVector& xplore, GlobalLU_t& glu)
{
  Index nextl_col; 
  
  
  VectorBlock<IndexVector> marker1(marker, m, m); 
  nseg = 0; 
  
  panel_dfs_traits<IndexVector> traits(jcol, marker1.data());
  
  
  for (Index jj = jcol; jj < jcol + w; jj++) 
  {
    nextl_col = (jj - jcol) * m; 
    
    VectorBlock<IndexVector> repfnz_col(repfnz, nextl_col, m); 
    VectorBlock<ScalarVector> dense_col(dense,nextl_col, m); 
    
    
    
    for (typename MatrixType::InnerIterator it(A, jj); it; ++it)
    {
      Index krow = it.row(); 
      dense_col(krow) = it.value();
      
      Index kmark = marker(krow); 
      if (kmark == jj) 
        continue; 
      
      dfs_kernel(jj, perm_r, nseg, panel_lsub, segrep, repfnz_col, xprune, marker, parent,
                   xplore, glu, nextl_col, krow, traits);
    }
    
  } 
}

} 
} 

#endif 
