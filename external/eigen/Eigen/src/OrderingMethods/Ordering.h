 
// Copyright (C) 2012  Désiré Nuentsa-Wakam <desire.nuentsa_wakam@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_ORDERING_H
#define EIGEN_ORDERING_H

namespace Eigen {
  
#include "Eigen_Colamd.h"

namespace internal {
    
template<typename MatrixType> 
void ordering_helper_at_plus_a(const MatrixType& mat, MatrixType& symmat)
{
  MatrixType C;
  C = mat.transpose(); 
  for (int i = 0; i < C.rows(); i++) 
  {
      for (typename MatrixType::InnerIterator it(C, i); it; ++it)
        it.valueRef() = 0.0;
  }
  symmat = C + mat; 
}
    
}

#ifndef EIGEN_MPL2_ONLY

template <typename Index>
class AMDOrdering
{
  public:
    typedef PermutationMatrix<Dynamic, Dynamic, Index> PermutationType;
    
    template <typename MatrixType>
    void operator()(const MatrixType& mat, PermutationType& perm)
    {
      
      SparseMatrix<typename MatrixType::Scalar, ColMajor, Index> symm;
      internal::ordering_helper_at_plus_a(mat,symm); 
    
      
      
      internal::minimum_degree_ordering(symm, perm);
    }
    
    
    template <typename SrcType, unsigned int SrcUpLo> 
    void operator()(const SparseSelfAdjointView<SrcType, SrcUpLo>& mat, PermutationType& perm)
    { 
      SparseMatrix<typename SrcType::Scalar, ColMajor, Index> C; C = mat;
      
      
      
      internal::minimum_degree_ordering(C, perm);
    }
};

#endif 

template <typename Index>
class NaturalOrdering
{
  public:
    typedef PermutationMatrix<Dynamic, Dynamic, Index> PermutationType;
    
    
    template <typename MatrixType>
    void operator()(const MatrixType& , PermutationType& perm)
    {
      perm.resize(0); 
    }
    
};

template<typename Index>
class COLAMDOrdering
{
  public:
    typedef PermutationMatrix<Dynamic, Dynamic, Index> PermutationType; 
    typedef Matrix<Index, Dynamic, 1> IndexVector;
    
    template <typename MatrixType>
    void operator() (const MatrixType& mat, PermutationType& perm)
    {
      eigen_assert(mat.isCompressed() && "COLAMDOrdering requires a sparse matrix in compressed mode. Call .makeCompressed() before passing it to COLAMDOrdering");
      
      Index m = mat.rows();
      Index n = mat.cols();
      Index nnz = mat.nonZeros();
      
      Index Alen = internal::colamd_recommended(nnz, m, n); 
      
      double knobs [COLAMD_KNOBS]; 
      Index stats [COLAMD_STATS];
      internal::colamd_set_defaults(knobs);
      
      IndexVector p(n+1), A(Alen); 
      for(Index i=0; i <= n; i++)   p(i) = mat.outerIndexPtr()[i];
      for(Index i=0; i < nnz; i++)  A(i) = mat.innerIndexPtr()[i];
      
      Index info = internal::colamd(m, n, Alen, A.data(), p.data(), knobs, stats); 
      EIGEN_UNUSED_VARIABLE(info);
      eigen_assert( info && "COLAMD failed " );
      
      perm.resize(n);
      for (Index i = 0; i < n; i++) perm.indices()(p(i)) = i;
    }
};

} 

#endif
