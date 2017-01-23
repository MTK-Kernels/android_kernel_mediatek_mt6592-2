// Copyright (C) 2008-2012 Gael Guennebaud <gael.guennebaud@inria.fr>

/*

NOTE: thes functions vave been adapted from the LDL library:

LDL Copyright (c) 2005 by Timothy A. Davis.  All Rights Reserved.

LDL License:

    Your use or distribution of LDL or any modified version of
    LDL implies that you agree to this License.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
    USA

    Permission is hereby granted to use or copy this program under the
    terms of the GNU LGPL, provided that the Copyright, this License,
    and the Availability of the original version is retained on all copies.
    User documentation of any code that uses this code or any modified
    version of this code must cite the Copyright, this License, the
    Availability note, and "Used by permission." Permission to modify
    the code and to distribute modified code is granted, provided the
    Copyright, this License, and the Availability note are retained,
    and a notice that the code was modified is included.
 */

#include "../Core/util/NonMPL2.h"

#ifndef EIGEN_SIMPLICIAL_CHOLESKY_IMPL_H
#define EIGEN_SIMPLICIAL_CHOLESKY_IMPL_H

namespace Eigen {

template<typename Derived>
void SimplicialCholeskyBase<Derived>::analyzePattern_preordered(const CholMatrixType& ap, bool doLDLT)
{
  const Index size = ap.rows();
  m_matrix.resize(size, size);
  m_parent.resize(size);
  m_nonZerosPerCol.resize(size);

  ei_declare_aligned_stack_constructed_variable(Index, tags, size, 0);

  for(Index k = 0; k < size; ++k)
  {
    
    m_parent[k] = -1;             
    tags[k] = k;                  
    m_nonZerosPerCol[k] = 0;      
    for(typename CholMatrixType::InnerIterator it(ap,k); it; ++it)
    {
      Index i = it.index();
      if(i < k)
      {
        
        for(; tags[i] != k; i = m_parent[i])
        {
          
          if (m_parent[i] == -1)
            m_parent[i] = k;
          m_nonZerosPerCol[i]++;        
          tags[i] = k;                  
        }
      }
    }
  }

  
  Index* Lp = m_matrix.outerIndexPtr();
  Lp[0] = 0;
  for(Index k = 0; k < size; ++k)
    Lp[k+1] = Lp[k] + m_nonZerosPerCol[k] + (doLDLT ? 0 : 1);

  m_matrix.resizeNonZeros(Lp[size]);

  m_isInitialized     = true;
  m_info              = Success;
  m_analysisIsOk      = true;
  m_factorizationIsOk = false;
}


template<typename Derived>
template<bool DoLDLT>
void SimplicialCholeskyBase<Derived>::factorize_preordered(const CholMatrixType& ap)
{
  using std::sqrt;

  eigen_assert(m_analysisIsOk && "You must first call analyzePattern()");
  eigen_assert(ap.rows()==ap.cols());
  const Index size = ap.rows();
  eigen_assert(m_parent.size()==size);
  eigen_assert(m_nonZerosPerCol.size()==size);

  const Index* Lp = m_matrix.outerIndexPtr();
  Index* Li = m_matrix.innerIndexPtr();
  Scalar* Lx = m_matrix.valuePtr();

  ei_declare_aligned_stack_constructed_variable(Scalar, y, size, 0);
  ei_declare_aligned_stack_constructed_variable(Index,  pattern, size, 0);
  ei_declare_aligned_stack_constructed_variable(Index,  tags, size, 0);

  bool ok = true;
  m_diag.resize(DoLDLT ? size : 0);

  for(Index k = 0; k < size; ++k)
  {
    
    y[k] = 0.0;                     
    Index top = size;               
    tags[k] = k;                    
    m_nonZerosPerCol[k] = 0;        
    for(typename MatrixType::InnerIterator it(ap,k); it; ++it)
    {
      Index i = it.index();
      if(i <= k)
      {
        y[i] += numext::conj(it.value());            
        Index len;
        for(len = 0; tags[i] != k; i = m_parent[i])
        {
          pattern[len++] = i;     
          tags[i] = k;            
        }
        while(len > 0)
          pattern[--top] = pattern[--len];
      }
    }

    

    RealScalar d = numext::real(y[k]) * m_shiftScale + m_shiftOffset;    
    y[k] = 0.0;
    for(; top < size; ++top)
    {
      Index i = pattern[top];       
      Scalar yi = y[i];             
      y[i] = 0.0;

      
      Scalar l_ki;
      if(DoLDLT)
        l_ki = yi / m_diag[i];
      else
        yi = l_ki = yi / Lx[Lp[i]];

      Index p2 = Lp[i] + m_nonZerosPerCol[i];
      Index p;
      for(p = Lp[i] + (DoLDLT ? 0 : 1); p < p2; ++p)
        y[Li[p]] -= numext::conj(Lx[p]) * yi;
      d -= numext::real(l_ki * numext::conj(yi));
      Li[p] = k;                          
      Lx[p] = l_ki;
      ++m_nonZerosPerCol[i];              
    }
    if(DoLDLT)
    {
      m_diag[k] = d;
      if(d == RealScalar(0))
      {
        ok = false;                         
        break;
      }
    }
    else
    {
      Index p = Lp[k] + m_nonZerosPerCol[k]++;
      Li[p] = k ;                
      if(d <= RealScalar(0)) {
        ok = false;              
        break;
      }
      Lx[p] = sqrt(d) ;
    }
  }

  m_info = ok ? Success : NumericalIssue;
  m_factorizationIsOk = true;
}

} 

#endif 
