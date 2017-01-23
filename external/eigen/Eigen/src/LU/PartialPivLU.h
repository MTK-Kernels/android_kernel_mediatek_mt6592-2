// Copyright (C) 2006-2009 Benoit Jacob <jacob.benoit.1@gmail.com>
// Copyright (C) 2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_PARTIALLU_H
#define EIGEN_PARTIALLU_H

namespace Eigen { 

template<typename _MatrixType> class PartialPivLU
{
  public:

    typedef _MatrixType MatrixType;
    enum {
      RowsAtCompileTime = MatrixType::RowsAtCompileTime,
      ColsAtCompileTime = MatrixType::ColsAtCompileTime,
      Options = MatrixType::Options,
      MaxRowsAtCompileTime = MatrixType::MaxRowsAtCompileTime,
      MaxColsAtCompileTime = MatrixType::MaxColsAtCompileTime
    };
    typedef typename MatrixType::Scalar Scalar;
    typedef typename NumTraits<typename MatrixType::Scalar>::Real RealScalar;
    typedef typename internal::traits<MatrixType>::StorageKind StorageKind;
    typedef typename MatrixType::Index Index;
    typedef PermutationMatrix<RowsAtCompileTime, MaxRowsAtCompileTime> PermutationType;
    typedef Transpositions<RowsAtCompileTime, MaxRowsAtCompileTime> TranspositionType;


    PartialPivLU();

    PartialPivLU(Index size);

    PartialPivLU(const MatrixType& matrix);

    PartialPivLU& compute(const MatrixType& matrix);

    inline const MatrixType& matrixLU() const
    {
      eigen_assert(m_isInitialized && "PartialPivLU is not initialized.");
      return m_lu;
    }

    inline const PermutationType& permutationP() const
    {
      eigen_assert(m_isInitialized && "PartialPivLU is not initialized.");
      return m_p;
    }

    template<typename Rhs>
    inline const internal::solve_retval<PartialPivLU, Rhs>
    solve(const MatrixBase<Rhs>& b) const
    {
      eigen_assert(m_isInitialized && "PartialPivLU is not initialized.");
      return internal::solve_retval<PartialPivLU, Rhs>(*this, b.derived());
    }

    inline const internal::solve_retval<PartialPivLU,typename MatrixType::IdentityReturnType> inverse() const
    {
      eigen_assert(m_isInitialized && "PartialPivLU is not initialized.");
      return internal::solve_retval<PartialPivLU,typename MatrixType::IdentityReturnType>
               (*this, MatrixType::Identity(m_lu.rows(), m_lu.cols()));
    }

    typename internal::traits<MatrixType>::Scalar determinant() const;

    MatrixType reconstructedMatrix() const;

    inline Index rows() const { return m_lu.rows(); }
    inline Index cols() const { return m_lu.cols(); }

  protected:
    MatrixType m_lu;
    PermutationType m_p;
    TranspositionType m_rowsTranspositions;
    Index m_det_p;
    bool m_isInitialized;
};

template<typename MatrixType>
PartialPivLU<MatrixType>::PartialPivLU()
  : m_lu(),
    m_p(),
    m_rowsTranspositions(),
    m_det_p(0),
    m_isInitialized(false)
{
}

template<typename MatrixType>
PartialPivLU<MatrixType>::PartialPivLU(Index size)
  : m_lu(size, size),
    m_p(size),
    m_rowsTranspositions(size),
    m_det_p(0),
    m_isInitialized(false)
{
}

template<typename MatrixType>
PartialPivLU<MatrixType>::PartialPivLU(const MatrixType& matrix)
  : m_lu(matrix.rows(), matrix.rows()),
    m_p(matrix.rows()),
    m_rowsTranspositions(matrix.rows()),
    m_det_p(0),
    m_isInitialized(false)
{
  compute(matrix);
}

namespace internal {

template<typename Scalar, int StorageOrder, typename PivIndex>
struct partial_lu_impl
{
  
  
  
  
  
  typedef Map<Matrix<Scalar, Dynamic, Dynamic, StorageOrder> > MapLU;
  typedef Block<MapLU, Dynamic, Dynamic> MatrixType;
  typedef Block<MatrixType,Dynamic,Dynamic> BlockType;
  typedef typename MatrixType::RealScalar RealScalar;
  typedef typename MatrixType::Index Index;

  static Index unblocked_lu(MatrixType& lu, PivIndex* row_transpositions, PivIndex& nb_transpositions)
  {
    const Index rows = lu.rows();
    const Index cols = lu.cols();
    const Index size = (std::min)(rows,cols);
    nb_transpositions = 0;
    Index first_zero_pivot = -1;
    for(Index k = 0; k < size; ++k)
    {
      Index rrows = rows-k-1;
      Index rcols = cols-k-1;
        
      Index row_of_biggest_in_col;
      RealScalar biggest_in_corner
        = lu.col(k).tail(rows-k).cwiseAbs().maxCoeff(&row_of_biggest_in_col);
      row_of_biggest_in_col += k;

      row_transpositions[k] = PivIndex(row_of_biggest_in_col);

      if(biggest_in_corner != RealScalar(0))
      {
        if(k != row_of_biggest_in_col)
        {
          lu.row(k).swap(lu.row(row_of_biggest_in_col));
          ++nb_transpositions;
        }

        
        
        lu.col(k).tail(rrows) /= lu.coeff(k,k);
      }
      else if(first_zero_pivot==-1)
      {
        
        
        first_zero_pivot = k;
      }

      if(k<rows-1)
        lu.bottomRightCorner(rrows,rcols).noalias() -= lu.col(k).tail(rrows) * lu.row(k).tail(rcols);
    }
    return first_zero_pivot;
  }

  static Index blocked_lu(Index rows, Index cols, Scalar* lu_data, Index luStride, PivIndex* row_transpositions, PivIndex& nb_transpositions, Index maxBlockSize=256)
  {
    MapLU lu1(lu_data,StorageOrder==RowMajor?rows:luStride,StorageOrder==RowMajor?luStride:cols);
    MatrixType lu(lu1,0,0,rows,cols);

    const Index size = (std::min)(rows,cols);

    
    if(size<=16)
    {
      return unblocked_lu(lu, row_transpositions, nb_transpositions);
    }

    
    
    Index blockSize;
    {
      blockSize = size/8;
      blockSize = (blockSize/16)*16;
      blockSize = (std::min)((std::max)(blockSize,Index(8)), maxBlockSize);
    }

    nb_transpositions = 0;
    Index first_zero_pivot = -1;
    for(Index k = 0; k < size; k+=blockSize)
    {
      Index bs = (std::min)(size-k,blockSize); 
      Index trows = rows - k - bs; 
      Index tsize = size - k - bs; 

      
      
      
      
      BlockType A_0(lu,0,0,rows,k);
      BlockType A_2(lu,0,k+bs,rows,tsize);
      BlockType A11(lu,k,k,bs,bs);
      BlockType A12(lu,k,k+bs,bs,tsize);
      BlockType A21(lu,k+bs,k,trows,bs);
      BlockType A22(lu,k+bs,k+bs,trows,tsize);

      PivIndex nb_transpositions_in_panel;
      
      
      Index ret = blocked_lu(trows+bs, bs, &lu.coeffRef(k,k), luStride,
                   row_transpositions+k, nb_transpositions_in_panel, 16);
      if(ret>=0 && first_zero_pivot==-1)
        first_zero_pivot = k+ret;

      nb_transpositions += nb_transpositions_in_panel;
      
      for(Index i=k; i<k+bs; ++i)
      {
        Index piv = (row_transpositions[i] += k);
        A_0.row(i).swap(A_0.row(piv));
      }

      if(trows)
      {
        
        for(Index i=k;i<k+bs; ++i)
          A_2.row(i).swap(A_2.row(row_transpositions[i]));

        
        A11.template triangularView<UnitLower>().solveInPlace(A12);

        A22.noalias() -= A21 * A12;
      }
    }
    return first_zero_pivot;
  }
};

template<typename MatrixType, typename TranspositionType>
void partial_lu_inplace(MatrixType& lu, TranspositionType& row_transpositions, typename TranspositionType::Index& nb_transpositions)
{
  eigen_assert(lu.cols() == row_transpositions.size());
  eigen_assert((&row_transpositions.coeffRef(1)-&row_transpositions.coeffRef(0)) == 1);

  partial_lu_impl
    <typename MatrixType::Scalar, MatrixType::Flags&RowMajorBit?RowMajor:ColMajor, typename TranspositionType::Index>
    ::blocked_lu(lu.rows(), lu.cols(), &lu.coeffRef(0,0), lu.outerStride(), &row_transpositions.coeffRef(0), nb_transpositions);
}

} 

template<typename MatrixType>
PartialPivLU<MatrixType>& PartialPivLU<MatrixType>::compute(const MatrixType& matrix)
{
  
  eigen_assert(matrix.rows()<NumTraits<int>::highest());
  
  m_lu = matrix;

  eigen_assert(matrix.rows() == matrix.cols() && "PartialPivLU is only for square (and moreover invertible) matrices");
  const Index size = matrix.rows();

  m_rowsTranspositions.resize(size);

  typename TranspositionType::Index nb_transpositions;
  internal::partial_lu_inplace(m_lu, m_rowsTranspositions, nb_transpositions);
  m_det_p = (nb_transpositions%2) ? -1 : 1;

  m_p = m_rowsTranspositions;

  m_isInitialized = true;
  return *this;
}

template<typename MatrixType>
typename internal::traits<MatrixType>::Scalar PartialPivLU<MatrixType>::determinant() const
{
  eigen_assert(m_isInitialized && "PartialPivLU is not initialized.");
  return Scalar(m_det_p) * m_lu.diagonal().prod();
}

template<typename MatrixType>
MatrixType PartialPivLU<MatrixType>::reconstructedMatrix() const
{
  eigen_assert(m_isInitialized && "LU is not initialized.");
  
  MatrixType res = m_lu.template triangularView<UnitLower>().toDenseMatrix()
                 * m_lu.template triangularView<Upper>();

  
  res = m_p.inverse() * res;

  return res;
}


namespace internal {

template<typename _MatrixType, typename Rhs>
struct solve_retval<PartialPivLU<_MatrixType>, Rhs>
  : solve_retval_base<PartialPivLU<_MatrixType>, Rhs>
{
  EIGEN_MAKE_SOLVE_HELPERS(PartialPivLU<_MatrixType>,Rhs)

  template<typename Dest> void evalTo(Dest& dst) const
  {
    /* The decomposition PA = LU can be rewritten as A = P^{-1} L U.
    * So we proceed as follows:
    * Step 1: compute c = Pb.
    * Step 2: replace c by the solution x to Lx = c.
    * Step 3: replace c by the solution x to Ux = c.
    */

    eigen_assert(rhs().rows() == dec().matrixLU().rows());

    
    dst = dec().permutationP() * rhs();

    
    dec().matrixLU().template triangularView<UnitLower>().solveInPlace(dst);

    
    dec().matrixLU().template triangularView<Upper>().solveInPlace(dst);
  }
};

} 


template<typename Derived>
inline const PartialPivLU<typename MatrixBase<Derived>::PlainObject>
MatrixBase<Derived>::partialPivLu() const
{
  return PartialPivLU<PlainObject>(eval());
}

#if EIGEN2_SUPPORT_STAGE > STAGE20_RESOLVE_API_CONFLICTS
template<typename Derived>
inline const PartialPivLU<typename MatrixBase<Derived>::PlainObject>
MatrixBase<Derived>::lu() const
{
  return PartialPivLU<PlainObject>(eval());
}
#endif

} 

#endif 
