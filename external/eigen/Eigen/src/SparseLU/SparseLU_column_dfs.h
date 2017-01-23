// Copyright (C) 2012 Désiré Nuentsa-Wakam <desire.nuentsa_wakam@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

/* 
 
 * NOTE: This file is the modified version of [s,d,c,z]column_dfs.c file in SuperLU 
 
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
#ifndef SPARSELU_COLUMN_DFS_H
#define SPARSELU_COLUMN_DFS_H

template <typename Scalar, typename Index> class SparseLUImpl;
namespace Eigen {

namespace internal {

template<typename IndexVector, typename ScalarVector>
struct column_dfs_traits : no_assignment_operator
{
  typedef typename ScalarVector::Scalar Scalar;
  typedef typename IndexVector::Scalar Index;
  column_dfs_traits(Index jcol, Index& jsuper, typename SparseLUImpl<Scalar, Index>::GlobalLU_t& glu, SparseLUImpl<Scalar, Index>& luImpl)
   : m_jcol(jcol), m_jsuper_ref(jsuper), m_glu(glu), m_luImpl(luImpl)
 {}
  bool update_segrep(Index , Index )
  {
    return true;
  }
  void mem_expand(IndexVector& lsub, Index& nextl, Index chmark)
  {
    if (nextl >= m_glu.nzlmax)
      m_luImpl.memXpand(lsub, m_glu.nzlmax, nextl, LSUB, m_glu.num_expansions); 
    if (chmark != (m_jcol-1)) m_jsuper_ref = emptyIdxLU;
  }
  enum { ExpandMem = true };
  
  Index m_jcol;
  Index& m_jsuper_ref;
  typename SparseLUImpl<Scalar, Index>::GlobalLU_t& m_glu;
  SparseLUImpl<Scalar, Index>& m_luImpl;
};


template <typename Scalar, typename Index>
Index SparseLUImpl<Scalar,Index>::column_dfs(const Index m, const Index jcol, IndexVector& perm_r, Index maxsuper, Index& nseg,  BlockIndexVector lsub_col, IndexVector& segrep, BlockIndexVector repfnz, IndexVector& xprune, IndexVector& marker, IndexVector& parent, IndexVector& xplore, GlobalLU_t& glu)
{
  
  Index jsuper = glu.supno(jcol); 
  Index nextl = glu.xlsub(jcol); 
  VectorBlock<IndexVector> marker2(marker, 2*m, m); 
  
  
  column_dfs_traits<IndexVector, ScalarVector> traits(jcol, jsuper, glu, *this);
  
  
  for (Index k = 0; ((k < m) ? lsub_col[k] != emptyIdxLU : false) ; k++)
  {
    Index krow = lsub_col(k); 
    lsub_col(k) = emptyIdxLU; 
    Index kmark = marker2(krow); 
    
    
    if (kmark == jcol) continue;
    
    dfs_kernel(jcol, perm_r, nseg, glu.lsub, segrep, repfnz, xprune, marker2, parent,
                   xplore, glu, nextl, krow, traits);
  } 
  
  Index fsupc, jptr, jm1ptr, ito, ifrom, istop;
  Index nsuper = glu.supno(jcol);
  Index jcolp1 = jcol + 1;
  Index jcolm1 = jcol - 1;
  
  
  if ( jcol == 0 )
  { 
    nsuper = glu.supno(0) = 0 ;
  }
  else 
  {
    fsupc = glu.xsup(nsuper); 
    jptr = glu.xlsub(jcol); 
    jm1ptr = glu.xlsub(jcolm1); 
    
    
    if ( (nextl-jptr != jptr-jm1ptr-1) ) jsuper = emptyIdxLU;
    
    
    
    if ( (jcol - fsupc) >= maxsuper) jsuper = emptyIdxLU; 
    
    if (jsuper == emptyIdxLU)
    { 
      if ( (fsupc < jcolm1-1) ) 
      { 
        ito = glu.xlsub(fsupc+1);
        glu.xlsub(jcolm1) = ito; 
        istop = ito + jptr - jm1ptr; 
        xprune(jcolm1) = istop; 
        glu.xlsub(jcol) = istop; 
        
        for (ifrom = jm1ptr; ifrom < nextl; ++ifrom, ++ito)
          glu.lsub(ito) = glu.lsub(ifrom); 
        nextl = ito;  
      }
      nsuper++; 
      glu.supno(jcol) = nsuper; 
    } 
  } 
  
  
  glu.xsup(nsuper+1) = jcolp1; 
  glu.supno(jcolp1) = nsuper; 
  xprune(jcol) = nextl;  
  glu.xlsub(jcolp1) = nextl; 
  
  return 0; 
}

} 

} 

#endif
