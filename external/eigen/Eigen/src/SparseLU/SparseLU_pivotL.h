// Copyright (C) 2012 Désiré Nuentsa-Wakam <desire.nuentsa_wakam@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

/* 
 
 * NOTE: This file is the modified version of xpivotL.c file in SuperLU 
 
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
#ifndef SPARSELU_PIVOTL_H
#define SPARSELU_PIVOTL_H

namespace Eigen {
namespace internal {
  
template <typename Scalar, typename Index>
Index SparseLUImpl<Scalar,Index>::pivotL(const Index jcol, const RealScalar& diagpivotthresh, IndexVector& perm_r, IndexVector& iperm_c, Index& pivrow, GlobalLU_t& glu)
{
  
  Index fsupc = (glu.xsup)((glu.supno)(jcol)); 
  Index nsupc = jcol - fsupc; 
  Index lptr = glu.xlsub(fsupc); 
  Index nsupr = glu.xlsub(fsupc+1) - lptr; 
  Index lda = glu.xlusup(fsupc+1) - glu.xlusup(fsupc); 
  Scalar* lu_sup_ptr = &(glu.lusup.data()[glu.xlusup(fsupc)]); 
  Scalar* lu_col_ptr = &(glu.lusup.data()[glu.xlusup(jcol)]); 
  Index* lsub_ptr = &(glu.lsub.data()[lptr]); 
  
  
  Index diagind = iperm_c(jcol); 
  RealScalar pivmax = 0.0; 
  Index pivptr = nsupc; 
  Index diag = emptyIdxLU; 
  RealScalar rtemp;
  Index isub, icol, itemp, k; 
  for (isub = nsupc; isub < nsupr; ++isub) {
    rtemp = std::abs(lu_col_ptr[isub]);
    if (rtemp > pivmax) {
      pivmax = rtemp; 
      pivptr = isub;
    } 
    if (lsub_ptr[isub] == diagind) diag = isub;
  }
  
  
  if ( pivmax == 0.0 ) {
    pivrow = lsub_ptr[pivptr];
    perm_r(pivrow) = jcol;
    return (jcol+1);
  }
  
  RealScalar thresh = diagpivotthresh * pivmax; 
  
  
  
  {
    
    if (diag >= 0 ) 
    {
      
      rtemp = std::abs(lu_col_ptr[diag]);
      if (rtemp != 0.0 && rtemp >= thresh) pivptr = diag;
    }
    pivrow = lsub_ptr[pivptr];
  }
  
  
  perm_r(pivrow) = jcol; 
  
  if (pivptr != nsupc )
  {
    std::swap( lsub_ptr[pivptr], lsub_ptr[nsupc] );
    
    
    for (icol = 0; icol <= nsupc; icol++)
    {
      itemp = pivptr + icol * lda; 
      std::swap(lu_sup_ptr[itemp], lu_sup_ptr[nsupc + icol * lda]);
    }
  }
  
  Scalar temp = Scalar(1.0) / lu_col_ptr[nsupc];
  for (k = nsupc+1; k < nsupr; k++)
    lu_col_ptr[k] *= temp; 
  return 0;
}

} 
} 

#endif 
