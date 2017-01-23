// Copyright (C) 2008-2011 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_SPARSESPARSEPRODUCTWITHPRUNING_H
#define EIGEN_SPARSESPARSEPRODUCTWITHPRUNING_H

namespace Eigen { 

namespace internal {


template<typename Lhs, typename Rhs, typename ResultType>
static void sparse_sparse_product_with_pruning_impl(const Lhs& lhs, const Rhs& rhs, ResultType& res, const typename ResultType::RealScalar& tolerance)
{
  

  typedef typename remove_all<Lhs>::type::Scalar Scalar;
  typedef typename remove_all<Lhs>::type::Index Index;

  
  Index rows = lhs.innerSize();
  Index cols = rhs.outerSize();
  
  eigen_assert(lhs.outerSize() == rhs.innerSize());

  
  AmbiVector<Scalar,Index> tempVector(rows);

  
  
  
  
  
  
  Index estimated_nnz_prod = lhs.nonZeros() + rhs.nonZeros();

  
  if(ResultType::IsRowMajor)
    res.resize(cols, rows);
  else
    res.resize(rows, cols);

  res.reserve(estimated_nnz_prod);
  double ratioColRes = double(estimated_nnz_prod)/double(lhs.rows()*rhs.cols());
  for (Index j=0; j<cols; ++j)
  {
    
    
    
    tempVector.init(ratioColRes);
    tempVector.setZero();
    for (typename Rhs::InnerIterator rhsIt(rhs, j); rhsIt; ++rhsIt)
    {
      // FIXME should be written like this: tmp += rhsIt.value() * lhs.col(rhsIt.index())
      tempVector.restart();
      Scalar x = rhsIt.value();
      for (typename Lhs::InnerIterator lhsIt(lhs, rhsIt.index()); lhsIt; ++lhsIt)
      {
        tempVector.coeffRef(lhsIt.index()) += lhsIt.value() * x;
      }
    }
    res.startVec(j);
    for (typename AmbiVector<Scalar,Index>::Iterator it(tempVector,tolerance); it; ++it)
      res.insertBackByOuterInner(j,it.index()) = it.value();
  }
  res.finalize();
}

template<typename Lhs, typename Rhs, typename ResultType,
  int LhsStorageOrder = traits<Lhs>::Flags&RowMajorBit,
  int RhsStorageOrder = traits<Rhs>::Flags&RowMajorBit,
  int ResStorageOrder = traits<ResultType>::Flags&RowMajorBit>
struct sparse_sparse_product_with_pruning_selector;

template<typename Lhs, typename Rhs, typename ResultType>
struct sparse_sparse_product_with_pruning_selector<Lhs,Rhs,ResultType,ColMajor,ColMajor,ColMajor>
{
  typedef typename traits<typename remove_all<Lhs>::type>::Scalar Scalar;
  typedef typename ResultType::RealScalar RealScalar;

  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res, const RealScalar& tolerance)
  {
    typename remove_all<ResultType>::type _res(res.rows(), res.cols());
    internal::sparse_sparse_product_with_pruning_impl<Lhs,Rhs,ResultType>(lhs, rhs, _res, tolerance);
    res.swap(_res);
  }
};

template<typename Lhs, typename Rhs, typename ResultType>
struct sparse_sparse_product_with_pruning_selector<Lhs,Rhs,ResultType,ColMajor,ColMajor,RowMajor>
{
  typedef typename ResultType::RealScalar RealScalar;
  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res, const RealScalar& tolerance)
  {
    
    typedef SparseMatrix<typename ResultType::Scalar,ColMajor,typename ResultType::Index> SparseTemporaryType;
    SparseTemporaryType _res(res.rows(), res.cols());
    internal::sparse_sparse_product_with_pruning_impl<Lhs,Rhs,SparseTemporaryType>(lhs, rhs, _res, tolerance);
    res = _res;
  }
};

template<typename Lhs, typename Rhs, typename ResultType>
struct sparse_sparse_product_with_pruning_selector<Lhs,Rhs,ResultType,RowMajor,RowMajor,RowMajor>
{
  typedef typename ResultType::RealScalar RealScalar;
  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res, const RealScalar& tolerance)
  {
    
    typename remove_all<ResultType>::type _res(res.rows(), res.cols());
    internal::sparse_sparse_product_with_pruning_impl<Rhs,Lhs,ResultType>(rhs, lhs, _res, tolerance);
    res.swap(_res);
  }
};

template<typename Lhs, typename Rhs, typename ResultType>
struct sparse_sparse_product_with_pruning_selector<Lhs,Rhs,ResultType,RowMajor,RowMajor,ColMajor>
{
  typedef typename ResultType::RealScalar RealScalar;
  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res, const RealScalar& tolerance)
  {
    typedef SparseMatrix<typename ResultType::Scalar,ColMajor,typename Lhs::Index> ColMajorMatrixLhs;
    typedef SparseMatrix<typename ResultType::Scalar,ColMajor,typename Lhs::Index> ColMajorMatrixRhs;
    ColMajorMatrixLhs colLhs(lhs);
    ColMajorMatrixRhs colRhs(rhs);
    internal::sparse_sparse_product_with_pruning_impl<ColMajorMatrixLhs,ColMajorMatrixRhs,ResultType>(colLhs, colRhs, res, tolerance);

    
  }
};


} 

} 

#endif 
