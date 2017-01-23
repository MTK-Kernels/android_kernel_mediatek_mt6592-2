// Copyright (C) 2012 Désiré Nuentsa-Wakam <desire.nuentsa_wakam@inria.fr>
// Copyright (C) 2012 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

/* 
 
 * NOTE: This file is the modified version of [s,d,c,z]panel_bmod.c file in SuperLU 
 
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
#ifndef SPARSELU_PANEL_BMOD_H
#define SPARSELU_PANEL_BMOD_H

namespace Eigen {
namespace internal {

template <typename Scalar, typename Index>
void SparseLUImpl<Scalar,Index>::panel_bmod(const Index m, const Index w, const Index jcol, 
                                            const Index nseg, ScalarVector& dense, ScalarVector& tempv,
                                            IndexVector& segrep, IndexVector& repfnz, GlobalLU_t& glu)
{
  
  Index ksub,jj,nextl_col; 
  Index fsupc, nsupc, nsupr, nrow; 
  Index krep, kfnz; 
  Index lptr; 
  Index luptr; 
  Index segsize,no_zeros ; 
  
  Index k = nseg - 1; 
  const Index PacketSize = internal::packet_traits<Scalar>::size;
  
  for (ksub = 0; ksub < nseg; ksub++)
  { 
    krep = segrep(k); k--; 
    fsupc = glu.xsup(glu.supno(krep)); 
    nsupc = krep - fsupc + 1; 
    nsupr = glu.xlsub(fsupc+1) - glu.xlsub(fsupc); 
    nrow = nsupr - nsupc; 
    lptr = glu.xlsub(fsupc); 
    
    
    Index u_rows = 0;
    Index u_cols = 0;
    for (jj = jcol; jj < jcol + w; jj++)
    {
      nextl_col = (jj-jcol) * m; 
      VectorBlock<IndexVector> repfnz_col(repfnz, nextl_col, m); 
      
      kfnz = repfnz_col(krep); 
      if ( kfnz == emptyIdxLU ) 
        continue; 
      
      segsize = krep - kfnz + 1;
      u_cols++;
      u_rows = (std::max)(segsize,u_rows);
    }
    
    if(nsupc >= 2)
    { 
      Index ldu = internal::first_multiple<Index>(u_rows, PacketSize);
      Map<Matrix<Scalar,Dynamic,Dynamic>, Aligned,  OuterStride<> > U(tempv.data(), u_rows, u_cols, OuterStride<>(ldu));
      
      
      Index u_col = 0;
      for (jj = jcol; jj < jcol + w; jj++)
      {
        nextl_col = (jj-jcol) * m; 
        VectorBlock<IndexVector> repfnz_col(repfnz, nextl_col, m); 
        VectorBlock<ScalarVector> dense_col(dense, nextl_col, m); 
        
        kfnz = repfnz_col(krep); 
        if ( kfnz == emptyIdxLU ) 
          continue; 
        
        segsize = krep - kfnz + 1;
        luptr = glu.xlusup(fsupc);    
        no_zeros = kfnz - fsupc; 
        
        Index isub = lptr + no_zeros;
        Index off = u_rows-segsize;
        for (Index i = 0; i < off; i++) U(i,u_col) = 0;
        for (Index i = 0; i < segsize; i++)
        {
          Index irow = glu.lsub(isub); 
          U(i+off,u_col) = dense_col(irow); 
          ++isub; 
        }
        u_col++;
      }
      
      luptr = glu.xlusup(fsupc);
      Index lda = glu.xlusup(fsupc+1) - glu.xlusup(fsupc);
      no_zeros = (krep - u_rows + 1) - fsupc;
      luptr += lda * no_zeros + no_zeros;
      Map<Matrix<Scalar,Dynamic,Dynamic>, 0, OuterStride<> > A(glu.lusup.data()+luptr, u_rows, u_rows, OuterStride<>(lda) );
      U = A.template triangularView<UnitLower>().solve(U);
      
      
      luptr += u_rows;
      Map<Matrix<Scalar,Dynamic,Dynamic>, 0, OuterStride<> > B(glu.lusup.data()+luptr, nrow, u_rows, OuterStride<>(lda) );
      eigen_assert(tempv.size()>w*ldu + nrow*w + 1);
      
      Index ldl = internal::first_multiple<Index>(nrow, PacketSize);
      Index offset = (PacketSize-internal::first_aligned(B.data(), PacketSize)) % PacketSize;
      Map<Matrix<Scalar,Dynamic,Dynamic>, 0, OuterStride<> > L(tempv.data()+w*ldu+offset, nrow, u_cols, OuterStride<>(ldl));
      
      L.setZero();
      internal::sparselu_gemm<Scalar>(L.rows(), L.cols(), B.cols(), B.data(), B.outerStride(), U.data(), U.outerStride(), L.data(), L.outerStride());
      
      
      u_col = 0;
      for (jj = jcol; jj < jcol + w; jj++)
      {
        nextl_col = (jj-jcol) * m; 
        VectorBlock<IndexVector> repfnz_col(repfnz, nextl_col, m); 
        VectorBlock<ScalarVector> dense_col(dense, nextl_col, m); 
        
        kfnz = repfnz_col(krep); 
        if ( kfnz == emptyIdxLU ) 
          continue; 
        
        segsize = krep - kfnz + 1;
        no_zeros = kfnz - fsupc; 
        Index isub = lptr + no_zeros;
        
        Index off = u_rows-segsize;
        for (Index i = 0; i < segsize; i++)
        {
          Index irow = glu.lsub(isub++); 
          dense_col(irow) = U.coeff(i+off,u_col);
          U.coeffRef(i+off,u_col) = 0;
        }
        
        
        for (Index i = 0; i < nrow; i++)
        {
          Index irow = glu.lsub(isub++); 
          dense_col(irow) -= L.coeff(i,u_col);
          L.coeffRef(i,u_col) = 0;
        }
        u_col++;
      }
    }
    else 
    {
      
      for (jj = jcol; jj < jcol + w; jj++)
      {
        nextl_col = (jj-jcol) * m; 
        VectorBlock<IndexVector> repfnz_col(repfnz, nextl_col, m); 
        VectorBlock<ScalarVector> dense_col(dense, nextl_col, m); 
        
        kfnz = repfnz_col(krep); 
        if ( kfnz == emptyIdxLU ) 
          continue; 
        
        segsize = krep - kfnz + 1;
        luptr = glu.xlusup(fsupc);
        
        Index lda = glu.xlusup(fsupc+1)-glu.xlusup(fsupc);
        
        
        
        no_zeros = kfnz - fsupc; 
              if(segsize==1)  LU_kernel_bmod<1>::run(segsize, dense_col, tempv, glu.lusup, luptr, lda, nrow, glu.lsub, lptr, no_zeros);
        else  if(segsize==2)  LU_kernel_bmod<2>::run(segsize, dense_col, tempv, glu.lusup, luptr, lda, nrow, glu.lsub, lptr, no_zeros);
        else  if(segsize==3)  LU_kernel_bmod<3>::run(segsize, dense_col, tempv, glu.lusup, luptr, lda, nrow, glu.lsub, lptr, no_zeros);
        else                  LU_kernel_bmod<Dynamic>::run(segsize, dense_col, tempv, glu.lusup, luptr, lda, nrow, glu.lsub, lptr, no_zeros); 
      } 
    }
    
  } 
} 

} 

} 

#endif 