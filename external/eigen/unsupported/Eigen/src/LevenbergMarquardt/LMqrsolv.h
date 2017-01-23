// Copyright (C) 2009 Thomas Capricelli <orzel@freehackers.org>
// Copyright (C) 2012 Desire Nuentsa <desire.nuentsa_wakam@inria.fr>
// Copyright Jorge More - Argonne National Laboratory
// Copyright Burt Garbow - Argonne National Laboratory
// Copyright Ken Hillstrom - Argonne National Laboratory
// This Source Code Form is subject to the terms of the Minpack license
// (a BSD-like license) described in the campaigned CopyrightMINPACK.txt file.

#ifndef EIGEN_LMQRSOLV_H
#define EIGEN_LMQRSOLV_H

namespace Eigen { 

namespace internal {

template <typename Scalar,int Rows, int Cols, typename Index>
void lmqrsolv(
  Matrix<Scalar,Rows,Cols> &s,
  const PermutationMatrix<Dynamic,Dynamic,Index> &iPerm,
  const Matrix<Scalar,Dynamic,1> &diag,
  const Matrix<Scalar,Dynamic,1> &qtb,
  Matrix<Scalar,Dynamic,1> &x,
  Matrix<Scalar,Dynamic,1> &sdiag)
{

    
    Index i, j, k, l;
    Scalar temp;
    Index n = s.cols();
    Matrix<Scalar,Dynamic,1>  wa(n);
    JacobiRotation<Scalar> givens;

    
    
    

    
    
    x = s.diagonal();
    wa = qtb;
    
   
    s.topLeftCorner(n,n).template triangularView<StrictlyLower>() = s.topLeftCorner(n,n).transpose();
    
    for (j = 0; j < n; ++j) {

        
        
        l = iPerm.indices()(j);
        if (diag[l] == 0.)
            break;
        sdiag.tail(n-j).setZero();
        sdiag[j] = diag[l];

        
        
        
        Scalar qtbpj = 0.;
        for (k = j; k < n; ++k) {
            
            
            givens.makeGivens(-s(k,k), sdiag[k]);

            
            
            s(k,k) = givens.c() * s(k,k) + givens.s() * sdiag[k];
            temp = givens.c() * wa[k] + givens.s() * qtbpj;
            qtbpj = -givens.s() * wa[k] + givens.c() * qtbpj;
            wa[k] = temp;

            
            for (i = k+1; i<n; ++i) {
                temp = givens.c() * s(i,k) + givens.s() * sdiag[i];
                sdiag[i] = -givens.s() * s(i,k) + givens.c() * sdiag[i];
                s(i,k) = temp;
            }
        }
    }
  
    
    
    Index nsing;
    for(nsing=0; nsing<n && sdiag[nsing]!=0; nsing++) {}

    wa.tail(n-nsing).setZero();
    s.topLeftCorner(nsing, nsing).transpose().template triangularView<Upper>().solveInPlace(wa.head(nsing));
  
    
    sdiag = s.diagonal();
    s.diagonal() = x;

    
    x = iPerm * wa; 
}

template <typename Scalar, int _Options, typename Index>
void lmqrsolv(
  SparseMatrix<Scalar,_Options,Index> &s,
  const PermutationMatrix<Dynamic,Dynamic> &iPerm,
  const Matrix<Scalar,Dynamic,1> &diag,
  const Matrix<Scalar,Dynamic,1> &qtb,
  Matrix<Scalar,Dynamic,1> &x,
  Matrix<Scalar,Dynamic,1> &sdiag)
{
  
  typedef SparseMatrix<Scalar,RowMajor,Index> FactorType;
    Index i, j, k, l;
    Scalar temp;
    Index n = s.cols();
    Matrix<Scalar,Dynamic,1>  wa(n);
    JacobiRotation<Scalar> givens;

    
    
    

    
    wa = qtb;
    FactorType R(s);
    
    for (j = 0; j < n; ++j)
    {
      
      
      l = iPerm.indices()(j);
      if (diag(l) == Scalar(0)) 
        break; 
      sdiag.tail(n-j).setZero();
      sdiag[j] = diag[l];
      
      
      
      
      Scalar qtbpj = 0; 
      
      for (k = j; k < n; ++k)
      {
        typename FactorType::InnerIterator itk(R,k);
        for (; itk; ++itk){
          if (itk.index() < k) continue;
          else break;
        }
        
        
        
        givens.makeGivens(-itk.value(), sdiag(k));
        
        
        
        itk.valueRef() = givens.c() * itk.value() + givens.s() * sdiag(k);
        temp = givens.c() * wa(k) + givens.s() * qtbpj; 
        qtbpj = -givens.s() * wa(k) + givens.c() * qtbpj;
        wa(k) = temp;
        
        
        for (++itk; itk; ++itk)
        {
          i = itk.index();
          temp = givens.c() *  itk.value() + givens.s() * sdiag(i);
          sdiag(i) = -givens.s() * itk.value() + givens.c() * sdiag(i);
          itk.valueRef() = temp;
        }
      }
    }
    
    
    
    Index nsing;
    for(nsing = 0; nsing<n && sdiag(nsing) !=0; nsing++) {}
    
    wa.tail(n-nsing).setZero();
    wa.head(nsing) = R.topLeftCorner(nsing,nsing).template triangularView<Upper>().solve(wa.head(nsing));
    
    sdiag = R.diagonal();
    
    x = iPerm * wa; 
}
} 

} 

#endif 
