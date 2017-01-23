// Copyright (C) 2008-2011 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_CONSERVATIVESPARSESPARSEPRODUCT_H
#define EIGEN_CONSERVATIVESPARSESPARSEPRODUCT_H

namespace Eigen { 

namespace internal {

template<typename Lhs, typename Rhs, typename ResultType>
static void conservative_sparse_sparse_product_impl(const Lhs& lhs, const Rhs& rhs, ResultType& res)
{
  typedef typename remove_all<Lhs>::type::Scalar Scalar;
  typedef typename remove_all<Lhs>::type::Index Index;

  
  Index rows = lhs.innerSize();
  Index cols = rhs.outerSize();
  eigen_assert(lhs.outerSize() == rhs.innerSize());

  std::vector<bool> mask(rows,false);
  Matrix<Scalar,Dynamic,1> values(rows);
  Matrix<Index,Dynamic,1>  indices(rows);

  
  
  
  
  
  
  Index estimated_nnz_prod = lhs.nonZeros() + rhs.nonZeros();

  res.setZero();
  res.reserve(Index(estimated_nnz_prod));
  
  for (Index j=0; j<cols; ++j)
  {

    res.startVec(j);
    Index nnz = 0;
    for (typename Rhs::InnerIterator rhsIt(rhs, j); rhsIt; ++rhsIt)
    {
      Scalar y = rhsIt.value();
      Index k = rhsIt.index();
      for (typename Lhs::InnerIterator lhsIt(lhs, k); lhsIt; ++lhsIt)
      {
        Index i = lhsIt.index();
        Scalar x = lhsIt.value();
        if(!mask[i])
        {
          mask[i] = true;
          values[i] = x * y;
          indices[nnz] = i;
          ++nnz;
        }
        else
          values[i] += x * y;
      }
    }

    
    for(Index k=0; k<nnz; ++k)
    {
      Index i = indices[k];
      res.insertBackByOuterInnerUnordered(j,i) = values[i];
      mask[i] = false;
    }

#if 0
    

    Index t200 = rows/(log2(200)*1.39);
    Index t = (rows*100)/139;

    
    
    
    
    
    
    
    
    if(true)
    {
      if(nnz>1) std::sort(indices.data(),indices.data()+nnz);
      for(Index k=0; k<nnz; ++k)
      {
        Index i = indices[k];
        res.insertBackByOuterInner(j,i) = values[i];
        mask[i] = false;
      }
    }
    else
    {
      
      for(Index i=0; i<rows; ++i)
      {
        if(mask[i])
        {
          mask[i] = false;
          res.insertBackByOuterInner(j,i) = values[i];
        }
      }
    }
#endif

  }
  res.finalize();
}


} 

namespace internal {

template<typename Lhs, typename Rhs, typename ResultType,
  int LhsStorageOrder = (traits<Lhs>::Flags&RowMajorBit) ? RowMajor : ColMajor,
  int RhsStorageOrder = (traits<Rhs>::Flags&RowMajorBit) ? RowMajor : ColMajor,
  int ResStorageOrder = (traits<ResultType>::Flags&RowMajorBit) ? RowMajor : ColMajor>
struct conservative_sparse_sparse_product_selector;

template<typename Lhs, typename Rhs, typename ResultType>
struct conservative_sparse_sparse_product_selector<Lhs,Rhs,ResultType,ColMajor,ColMajor,ColMajor>
{
  typedef typename remove_all<Lhs>::type LhsCleaned;
  typedef typename LhsCleaned::Scalar Scalar;

  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res)
  {
    typedef SparseMatrix<typename ResultType::Scalar,RowMajor,typename ResultType::Index> RowMajorMatrix;
    typedef SparseMatrix<typename ResultType::Scalar,ColMajor,typename ResultType::Index> ColMajorMatrix;
    ColMajorMatrix resCol(lhs.rows(),rhs.cols());
    internal::conservative_sparse_sparse_product_impl<Lhs,Rhs,ColMajorMatrix>(lhs, rhs, resCol);
    
    RowMajorMatrix resRow(resCol);
    res = resRow;
  }
};

template<typename Lhs, typename Rhs, typename ResultType>
struct conservative_sparse_sparse_product_selector<Lhs,Rhs,ResultType,RowMajor,ColMajor,ColMajor>
{
  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res)
  {
     typedef SparseMatrix<typename ResultType::Scalar,RowMajor,typename ResultType::Index> RowMajorMatrix;
     RowMajorMatrix rhsRow = rhs;
     RowMajorMatrix resRow(lhs.rows(), rhs.cols());
     internal::conservative_sparse_sparse_product_impl<RowMajorMatrix,Lhs,RowMajorMatrix>(rhsRow, lhs, resRow);
     res = resRow;
  }
};

template<typename Lhs, typename Rhs, typename ResultType>
struct conservative_sparse_sparse_product_selector<Lhs,Rhs,ResultType,ColMajor,RowMajor,ColMajor>
{
  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res)
  {
    typedef SparseMatrix<typename ResultType::Scalar,RowMajor,typename ResultType::Index> RowMajorMatrix;
    RowMajorMatrix lhsRow = lhs;
    RowMajorMatrix resRow(lhs.rows(), rhs.cols());
    internal::conservative_sparse_sparse_product_impl<Rhs,RowMajorMatrix,RowMajorMatrix>(rhs, lhsRow, resRow);
    res = resRow;
  }
};

template<typename Lhs, typename Rhs, typename ResultType>
struct conservative_sparse_sparse_product_selector<Lhs,Rhs,ResultType,RowMajor,RowMajor,ColMajor>
{
  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res)
  {
    typedef SparseMatrix<typename ResultType::Scalar,RowMajor,typename ResultType::Index> RowMajorMatrix;
    RowMajorMatrix resRow(lhs.rows(), rhs.cols());
    internal::conservative_sparse_sparse_product_impl<Rhs,Lhs,RowMajorMatrix>(rhs, lhs, resRow);
    res = resRow;
  }
};


template<typename Lhs, typename Rhs, typename ResultType>
struct conservative_sparse_sparse_product_selector<Lhs,Rhs,ResultType,ColMajor,ColMajor,RowMajor>
{
  typedef typename traits<typename remove_all<Lhs>::type>::Scalar Scalar;

  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res)
  {
    typedef SparseMatrix<typename ResultType::Scalar,ColMajor,typename ResultType::Index> ColMajorMatrix;
    ColMajorMatrix resCol(lhs.rows(), rhs.cols());
    internal::conservative_sparse_sparse_product_impl<Lhs,Rhs,ColMajorMatrix>(lhs, rhs, resCol);
    res = resCol;
  }
};

template<typename Lhs, typename Rhs, typename ResultType>
struct conservative_sparse_sparse_product_selector<Lhs,Rhs,ResultType,RowMajor,ColMajor,RowMajor>
{
  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res)
  {
    typedef SparseMatrix<typename ResultType::Scalar,ColMajor,typename ResultType::Index> ColMajorMatrix;
    ColMajorMatrix lhsCol = lhs;
    ColMajorMatrix resCol(lhs.rows(), rhs.cols());
    internal::conservative_sparse_sparse_product_impl<ColMajorMatrix,Rhs,ColMajorMatrix>(lhsCol, rhs, resCol);
    res = resCol;
  }
};

template<typename Lhs, typename Rhs, typename ResultType>
struct conservative_sparse_sparse_product_selector<Lhs,Rhs,ResultType,ColMajor,RowMajor,RowMajor>
{
  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res)
  {
    typedef SparseMatrix<typename ResultType::Scalar,ColMajor,typename ResultType::Index> ColMajorMatrix;
    ColMajorMatrix rhsCol = rhs;
    ColMajorMatrix resCol(lhs.rows(), rhs.cols());
    internal::conservative_sparse_sparse_product_impl<Lhs,ColMajorMatrix,ColMajorMatrix>(lhs, rhsCol, resCol);
    res = resCol;
  }
};

template<typename Lhs, typename Rhs, typename ResultType>
struct conservative_sparse_sparse_product_selector<Lhs,Rhs,ResultType,RowMajor,RowMajor,RowMajor>
{
  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res)
  {
    typedef SparseMatrix<typename ResultType::Scalar,RowMajor,typename ResultType::Index> RowMajorMatrix;
    typedef SparseMatrix<typename ResultType::Scalar,ColMajor,typename ResultType::Index> ColMajorMatrix;
    RowMajorMatrix resRow(lhs.rows(),rhs.cols());
    internal::conservative_sparse_sparse_product_impl<Rhs,Lhs,RowMajorMatrix>(rhs, lhs, resRow);
    
    ColMajorMatrix resCol(resRow);
    res = resCol;
  }
};

} 

} 

#endif 
