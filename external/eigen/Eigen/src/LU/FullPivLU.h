// Copyright (C) 2006-2009 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_LU_H
#define EIGEN_LU_H

namespace Eigen { 

template<typename _MatrixType> class FullPivLU
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
    typedef typename internal::plain_row_type<MatrixType, Index>::type IntRowVectorType;
    typedef typename internal::plain_col_type<MatrixType, Index>::type IntColVectorType;
    typedef PermutationMatrix<ColsAtCompileTime, MaxColsAtCompileTime> PermutationQType;
    typedef PermutationMatrix<RowsAtCompileTime, MaxRowsAtCompileTime> PermutationPType;

    FullPivLU();

    FullPivLU(Index rows, Index cols);

    FullPivLU(const MatrixType& matrix);

    FullPivLU& compute(const MatrixType& matrix);

    inline const MatrixType& matrixLU() const
    {
      eigen_assert(m_isInitialized && "LU is not initialized.");
      return m_lu;
    }

    inline Index nonzeroPivots() const
    {
      eigen_assert(m_isInitialized && "LU is not initialized.");
      return m_nonzero_pivots;
    }

    RealScalar maxPivot() const { return m_maxpivot; }

    inline const PermutationPType& permutationP() const
    {
      eigen_assert(m_isInitialized && "LU is not initialized.");
      return m_p;
    }

    inline const PermutationQType& permutationQ() const
    {
      eigen_assert(m_isInitialized && "LU is not initialized.");
      return m_q;
    }

    inline const internal::kernel_retval<FullPivLU> kernel() const
    {
      eigen_assert(m_isInitialized && "LU is not initialized.");
      return internal::kernel_retval<FullPivLU>(*this);
    }

    inline const internal::image_retval<FullPivLU>
      image(const MatrixType& originalMatrix) const
    {
      eigen_assert(m_isInitialized && "LU is not initialized.");
      return internal::image_retval<FullPivLU>(*this, originalMatrix);
    }

    template<typename Rhs>
    inline const internal::solve_retval<FullPivLU, Rhs>
    solve(const MatrixBase<Rhs>& b) const
    {
      eigen_assert(m_isInitialized && "LU is not initialized.");
      return internal::solve_retval<FullPivLU, Rhs>(*this, b.derived());
    }

    typename internal::traits<MatrixType>::Scalar determinant() const;

    FullPivLU& setThreshold(const RealScalar& threshold)
    {
      m_usePrescribedThreshold = true;
      m_prescribedThreshold = threshold;
      return *this;
    }

    FullPivLU& setThreshold(Default_t)
    {
      m_usePrescribedThreshold = false;
      return *this;
    }

    RealScalar threshold() const
    {
      eigen_assert(m_isInitialized || m_usePrescribedThreshold);
      return m_usePrescribedThreshold ? m_prescribedThreshold
      
      
                                      : NumTraits<Scalar>::epsilon() * m_lu.diagonalSize();
    }

    inline Index rank() const
    {
      using std::abs;
      eigen_assert(m_isInitialized && "LU is not initialized.");
      RealScalar premultiplied_threshold = abs(m_maxpivot) * threshold();
      Index result = 0;
      for(Index i = 0; i < m_nonzero_pivots; ++i)
        result += (abs(m_lu.coeff(i,i)) > premultiplied_threshold);
      return result;
    }

    inline Index dimensionOfKernel() const
    {
      eigen_assert(m_isInitialized && "LU is not initialized.");
      return cols() - rank();
    }

    inline bool isInjective() const
    {
      eigen_assert(m_isInitialized && "LU is not initialized.");
      return rank() == cols();
    }

    inline bool isSurjective() const
    {
      eigen_assert(m_isInitialized && "LU is not initialized.");
      return rank() == rows();
    }

    inline bool isInvertible() const
    {
      eigen_assert(m_isInitialized && "LU is not initialized.");
      return isInjective() && (m_lu.rows() == m_lu.cols());
    }

    inline const internal::solve_retval<FullPivLU,typename MatrixType::IdentityReturnType> inverse() const
    {
      eigen_assert(m_isInitialized && "LU is not initialized.");
      eigen_assert(m_lu.rows() == m_lu.cols() && "You can't take the inverse of a non-square matrix!");
      return internal::solve_retval<FullPivLU,typename MatrixType::IdentityReturnType>
               (*this, MatrixType::Identity(m_lu.rows(), m_lu.cols()));
    }

    MatrixType reconstructedMatrix() const;

    inline Index rows() const { return m_lu.rows(); }
    inline Index cols() const { return m_lu.cols(); }

  protected:
    MatrixType m_lu;
    PermutationPType m_p;
    PermutationQType m_q;
    IntColVectorType m_rowsTranspositions;
    IntRowVectorType m_colsTranspositions;
    Index m_det_pq, m_nonzero_pivots;
    RealScalar m_maxpivot, m_prescribedThreshold;
    bool m_isInitialized, m_usePrescribedThreshold;
};

template<typename MatrixType>
FullPivLU<MatrixType>::FullPivLU()
  : m_isInitialized(false), m_usePrescribedThreshold(false)
{
}

template<typename MatrixType>
FullPivLU<MatrixType>::FullPivLU(Index rows, Index cols)
  : m_lu(rows, cols),
    m_p(rows),
    m_q(cols),
    m_rowsTranspositions(rows),
    m_colsTranspositions(cols),
    m_isInitialized(false),
    m_usePrescribedThreshold(false)
{
}

template<typename MatrixType>
FullPivLU<MatrixType>::FullPivLU(const MatrixType& matrix)
  : m_lu(matrix.rows(), matrix.cols()),
    m_p(matrix.rows()),
    m_q(matrix.cols()),
    m_rowsTranspositions(matrix.rows()),
    m_colsTranspositions(matrix.cols()),
    m_isInitialized(false),
    m_usePrescribedThreshold(false)
{
  compute(matrix);
}

template<typename MatrixType>
FullPivLU<MatrixType>& FullPivLU<MatrixType>::compute(const MatrixType& matrix)
{
  
  eigen_assert(matrix.rows()<=NumTraits<int>::highest() && matrix.cols()<=NumTraits<int>::highest());
  
  m_isInitialized = true;
  m_lu = matrix;

  const Index size = matrix.diagonalSize();
  const Index rows = matrix.rows();
  const Index cols = matrix.cols();

  
  
  m_rowsTranspositions.resize(matrix.rows());
  m_colsTranspositions.resize(matrix.cols());
  Index number_of_transpositions = 0; 

  m_nonzero_pivots = size; 
  m_maxpivot = RealScalar(0);

  for(Index k = 0; k < size; ++k)
  {
    

    
    Index row_of_biggest_in_corner, col_of_biggest_in_corner;
    RealScalar biggest_in_corner;
    biggest_in_corner = m_lu.bottomRightCorner(rows-k, cols-k)
                        .cwiseAbs()
                        .maxCoeff(&row_of_biggest_in_corner, &col_of_biggest_in_corner);
    row_of_biggest_in_corner += k; 
    col_of_biggest_in_corner += k; 

    if(biggest_in_corner==RealScalar(0))
    {
      
      
      m_nonzero_pivots = k;
      for(Index i = k; i < size; ++i)
      {
        m_rowsTranspositions.coeffRef(i) = i;
        m_colsTranspositions.coeffRef(i) = i;
      }
      break;
    }

    if(biggest_in_corner > m_maxpivot) m_maxpivot = biggest_in_corner;

    
    

    m_rowsTranspositions.coeffRef(k) = row_of_biggest_in_corner;
    m_colsTranspositions.coeffRef(k) = col_of_biggest_in_corner;
    if(k != row_of_biggest_in_corner) {
      m_lu.row(k).swap(m_lu.row(row_of_biggest_in_corner));
      ++number_of_transpositions;
    }
    if(k != col_of_biggest_in_corner) {
      m_lu.col(k).swap(m_lu.col(col_of_biggest_in_corner));
      ++number_of_transpositions;
    }

    
    

    if(k<rows-1)
      m_lu.col(k).tail(rows-k-1) /= m_lu.coeff(k,k);
    if(k<size-1)
      m_lu.block(k+1,k+1,rows-k-1,cols-k-1).noalias() -= m_lu.col(k).tail(rows-k-1) * m_lu.row(k).tail(cols-k-1);
  }

  
  

  m_p.setIdentity(rows);
  for(Index k = size-1; k >= 0; --k)
    m_p.applyTranspositionOnTheRight(k, m_rowsTranspositions.coeff(k));

  m_q.setIdentity(cols);
  for(Index k = 0; k < size; ++k)
    m_q.applyTranspositionOnTheRight(k, m_colsTranspositions.coeff(k));

  m_det_pq = (number_of_transpositions%2) ? -1 : 1;
  return *this;
}

template<typename MatrixType>
typename internal::traits<MatrixType>::Scalar FullPivLU<MatrixType>::determinant() const
{
  eigen_assert(m_isInitialized && "LU is not initialized.");
  eigen_assert(m_lu.rows() == m_lu.cols() && "You can't take the determinant of a non-square matrix!");
  return Scalar(m_det_pq) * Scalar(m_lu.diagonal().prod());
}

template<typename MatrixType>
MatrixType FullPivLU<MatrixType>::reconstructedMatrix() const
{
  eigen_assert(m_isInitialized && "LU is not initialized.");
  const Index smalldim = (std::min)(m_lu.rows(), m_lu.cols());
  
  MatrixType res(m_lu.rows(),m_lu.cols());
  
  res = m_lu.leftCols(smalldim)
            .template triangularView<UnitLower>().toDenseMatrix()
      * m_lu.topRows(smalldim)
            .template triangularView<Upper>().toDenseMatrix();

  
  res = m_p.inverse() * res;

  
  res = res * m_q.inverse();

  return res;
}


namespace internal {
template<typename _MatrixType>
struct kernel_retval<FullPivLU<_MatrixType> >
  : kernel_retval_base<FullPivLU<_MatrixType> >
{
  EIGEN_MAKE_KERNEL_HELPERS(FullPivLU<_MatrixType>)

  enum { MaxSmallDimAtCompileTime = EIGEN_SIZE_MIN_PREFER_FIXED(
            MatrixType::MaxColsAtCompileTime,
            MatrixType::MaxRowsAtCompileTime)
  };

  template<typename Dest> void evalTo(Dest& dst) const
  {
    using std::abs;
    const Index cols = dec().matrixLU().cols(), dimker = cols - rank();
    if(dimker == 0)
    {
      
      
      
      dst.setZero();
      return;
    }



    Matrix<Index, Dynamic, 1, 0, MaxSmallDimAtCompileTime, 1> pivots(rank());
    RealScalar premultiplied_threshold = dec().maxPivot() * dec().threshold();
    Index p = 0;
    for(Index i = 0; i < dec().nonzeroPivots(); ++i)
      if(abs(dec().matrixLU().coeff(i,i)) > premultiplied_threshold)
        pivots.coeffRef(p++) = i;
    eigen_internal_assert(p == rank());

    
    
    
    
    Matrix<typename MatrixType::Scalar, Dynamic, Dynamic, MatrixType::Options,
           MaxSmallDimAtCompileTime, MatrixType::MaxColsAtCompileTime>
      m(dec().matrixLU().block(0, 0, rank(), cols));
    for(Index i = 0; i < rank(); ++i)
    {
      if(i) m.row(i).head(i).setZero();
      m.row(i).tail(cols-i) = dec().matrixLU().row(pivots.coeff(i)).tail(cols-i);
    }
    m.block(0, 0, rank(), rank());
    m.block(0, 0, rank(), rank()).template triangularView<StrictlyLower>().setZero();
    for(Index i = 0; i < rank(); ++i)
      m.col(i).swap(m.col(pivots.coeff(i)));

    
    
    
    m.topLeftCorner(rank(), rank())
     .template triangularView<Upper>().solveInPlace(
        m.topRightCorner(rank(), dimker)
      );

    
    for(Index i = rank()-1; i >= 0; --i)
      m.col(i).swap(m.col(pivots.coeff(i)));

    
    for(Index i = 0; i < rank(); ++i) dst.row(dec().permutationQ().indices().coeff(i)) = -m.row(i).tail(dimker);
    for(Index i = rank(); i < cols; ++i) dst.row(dec().permutationQ().indices().coeff(i)).setZero();
    for(Index k = 0; k < dimker; ++k) dst.coeffRef(dec().permutationQ().indices().coeff(rank()+k), k) = Scalar(1);
  }
};


template<typename _MatrixType>
struct image_retval<FullPivLU<_MatrixType> >
  : image_retval_base<FullPivLU<_MatrixType> >
{
  EIGEN_MAKE_IMAGE_HELPERS(FullPivLU<_MatrixType>)

  enum { MaxSmallDimAtCompileTime = EIGEN_SIZE_MIN_PREFER_FIXED(
            MatrixType::MaxColsAtCompileTime,
            MatrixType::MaxRowsAtCompileTime)
  };

  template<typename Dest> void evalTo(Dest& dst) const
  {
    using std::abs;
    if(rank() == 0)
    {
      
      
      
      dst.setZero();
      return;
    }

    Matrix<Index, Dynamic, 1, 0, MaxSmallDimAtCompileTime, 1> pivots(rank());
    RealScalar premultiplied_threshold = dec().maxPivot() * dec().threshold();
    Index p = 0;
    for(Index i = 0; i < dec().nonzeroPivots(); ++i)
      if(abs(dec().matrixLU().coeff(i,i)) > premultiplied_threshold)
        pivots.coeffRef(p++) = i;
    eigen_internal_assert(p == rank());

    for(Index i = 0; i < rank(); ++i)
      dst.col(i) = originalMatrix().col(dec().permutationQ().indices().coeff(pivots.coeff(i)));
  }
};


template<typename _MatrixType, typename Rhs>
struct solve_retval<FullPivLU<_MatrixType>, Rhs>
  : solve_retval_base<FullPivLU<_MatrixType>, Rhs>
{
  EIGEN_MAKE_SOLVE_HELPERS(FullPivLU<_MatrixType>,Rhs)

  template<typename Dest> void evalTo(Dest& dst) const
  {
    /* The decomposition PAQ = LU can be rewritten as A = P^{-1} L U Q^{-1}.
     * So we proceed as follows:
     * Step 1: compute c = P * rhs.
     * Step 2: replace c by the solution x to Lx = c. Exists because L is invertible.
     * Step 3: replace c by the solution x to Ux = c. May or may not exist.
     * Step 4: result = Q * c;
     */

    const Index rows = dec().rows(), cols = dec().cols(),
              nonzero_pivots = dec().nonzeroPivots();
    eigen_assert(rhs().rows() == rows);
    const Index smalldim = (std::min)(rows, cols);

    if(nonzero_pivots == 0)
    {
      dst.setZero();
      return;
    }

    typename Rhs::PlainObject c(rhs().rows(), rhs().cols());

    
    c = dec().permutationP() * rhs();

    
    dec().matrixLU()
        .topLeftCorner(smalldim,smalldim)
        .template triangularView<UnitLower>()
        .solveInPlace(c.topRows(smalldim));
    if(rows>cols)
    {
      c.bottomRows(rows-cols)
        -= dec().matrixLU().bottomRows(rows-cols)
         * c.topRows(cols);
    }

    
    dec().matrixLU()
        .topLeftCorner(nonzero_pivots, nonzero_pivots)
        .template triangularView<Upper>()
        .solveInPlace(c.topRows(nonzero_pivots));

    
    for(Index i = 0; i < nonzero_pivots; ++i)
      dst.row(dec().permutationQ().indices().coeff(i)) = c.row(i);
    for(Index i = nonzero_pivots; i < dec().matrixLU().cols(); ++i)
      dst.row(dec().permutationQ().indices().coeff(i)).setZero();
  }
};

} 


template<typename Derived>
inline const FullPivLU<typename MatrixBase<Derived>::PlainObject>
MatrixBase<Derived>::fullPivLu() const
{
  return FullPivLU<PlainObject>(eval());
}

} 

#endif 
