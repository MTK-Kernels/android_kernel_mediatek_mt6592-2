// Copyright (C) 2008-2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2009 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_COLPIVOTINGHOUSEHOLDERQR_H
#define EIGEN_COLPIVOTINGHOUSEHOLDERQR_H

namespace Eigen { 

template<typename _MatrixType> class ColPivHouseholderQR
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
    typedef typename MatrixType::RealScalar RealScalar;
    typedef typename MatrixType::Index Index;
    typedef Matrix<Scalar, RowsAtCompileTime, RowsAtCompileTime, Options, MaxRowsAtCompileTime, MaxRowsAtCompileTime> MatrixQType;
    typedef typename internal::plain_diag_type<MatrixType>::type HCoeffsType;
    typedef PermutationMatrix<ColsAtCompileTime, MaxColsAtCompileTime> PermutationType;
    typedef typename internal::plain_row_type<MatrixType, Index>::type IntRowVectorType;
    typedef typename internal::plain_row_type<MatrixType>::type RowVectorType;
    typedef typename internal::plain_row_type<MatrixType, RealScalar>::type RealRowVectorType;
    typedef HouseholderSequence<MatrixType,typename internal::remove_all<typename HCoeffsType::ConjugateReturnType>::type> HouseholderSequenceType;
    
  private:
    
    typedef typename PermutationType::Index PermIndexType;
    
  public:

    ColPivHouseholderQR()
      : m_qr(),
        m_hCoeffs(),
        m_colsPermutation(),
        m_colsTranspositions(),
        m_temp(),
        m_colSqNorms(),
        m_isInitialized(false),
        m_usePrescribedThreshold(false) {}

    ColPivHouseholderQR(Index rows, Index cols)
      : m_qr(rows, cols),
        m_hCoeffs((std::min)(rows,cols)),
        m_colsPermutation(PermIndexType(cols)),
        m_colsTranspositions(cols),
        m_temp(cols),
        m_colSqNorms(cols),
        m_isInitialized(false),
        m_usePrescribedThreshold(false) {}

    ColPivHouseholderQR(const MatrixType& matrix)
      : m_qr(matrix.rows(), matrix.cols()),
        m_hCoeffs((std::min)(matrix.rows(),matrix.cols())),
        m_colsPermutation(PermIndexType(matrix.cols())),
        m_colsTranspositions(matrix.cols()),
        m_temp(matrix.cols()),
        m_colSqNorms(matrix.cols()),
        m_isInitialized(false),
        m_usePrescribedThreshold(false)
    {
      compute(matrix);
    }

    template<typename Rhs>
    inline const internal::solve_retval<ColPivHouseholderQR, Rhs>
    solve(const MatrixBase<Rhs>& b) const
    {
      eigen_assert(m_isInitialized && "ColPivHouseholderQR is not initialized.");
      return internal::solve_retval<ColPivHouseholderQR, Rhs>(*this, b.derived());
    }

    HouseholderSequenceType householderQ(void) const;
    HouseholderSequenceType matrixQ(void) const
    {
      return householderQ(); 
    }

    const MatrixType& matrixQR() const
    {
      eigen_assert(m_isInitialized && "ColPivHouseholderQR is not initialized.");
      return m_qr;
    }
    
    const MatrixType& matrixR() const
    {
      eigen_assert(m_isInitialized && "ColPivHouseholderQR is not initialized.");
      return m_qr;
    }
    
    ColPivHouseholderQR& compute(const MatrixType& matrix);

    
    const PermutationType& colsPermutation() const
    {
      eigen_assert(m_isInitialized && "ColPivHouseholderQR is not initialized.");
      return m_colsPermutation;
    }

    typename MatrixType::RealScalar absDeterminant() const;

    typename MatrixType::RealScalar logAbsDeterminant() const;

    inline Index rank() const
    {
      using std::abs;
      eigen_assert(m_isInitialized && "ColPivHouseholderQR is not initialized.");
      RealScalar premultiplied_threshold = abs(m_maxpivot) * threshold();
      Index result = 0;
      for(Index i = 0; i < m_nonzero_pivots; ++i)
        result += (abs(m_qr.coeff(i,i)) > premultiplied_threshold);
      return result;
    }

    inline Index dimensionOfKernel() const
    {
      eigen_assert(m_isInitialized && "ColPivHouseholderQR is not initialized.");
      return cols() - rank();
    }

    inline bool isInjective() const
    {
      eigen_assert(m_isInitialized && "ColPivHouseholderQR is not initialized.");
      return rank() == cols();
    }

    inline bool isSurjective() const
    {
      eigen_assert(m_isInitialized && "ColPivHouseholderQR is not initialized.");
      return rank() == rows();
    }

    inline bool isInvertible() const
    {
      eigen_assert(m_isInitialized && "ColPivHouseholderQR is not initialized.");
      return isInjective() && isSurjective();
    }

    inline const
    internal::solve_retval<ColPivHouseholderQR, typename MatrixType::IdentityReturnType>
    inverse() const
    {
      eigen_assert(m_isInitialized && "ColPivHouseholderQR is not initialized.");
      return internal::solve_retval<ColPivHouseholderQR,typename MatrixType::IdentityReturnType>
               (*this, MatrixType::Identity(m_qr.rows(), m_qr.cols()));
    }

    inline Index rows() const { return m_qr.rows(); }
    inline Index cols() const { return m_qr.cols(); }
    
    const HCoeffsType& hCoeffs() const { return m_hCoeffs; }

    ColPivHouseholderQR& setThreshold(const RealScalar& threshold)
    {
      m_usePrescribedThreshold = true;
      m_prescribedThreshold = threshold;
      return *this;
    }

    ColPivHouseholderQR& setThreshold(Default_t)
    {
      m_usePrescribedThreshold = false;
      return *this;
    }

    RealScalar threshold() const
    {
      eigen_assert(m_isInitialized || m_usePrescribedThreshold);
      return m_usePrescribedThreshold ? m_prescribedThreshold
      
      
                                      : NumTraits<Scalar>::epsilon() * RealScalar(m_qr.diagonalSize());
    }

    inline Index nonzeroPivots() const
    {
      eigen_assert(m_isInitialized && "ColPivHouseholderQR is not initialized.");
      return m_nonzero_pivots;
    }

    RealScalar maxPivot() const { return m_maxpivot; }
    
    ComputationInfo info() const
    {
      eigen_assert(m_isInitialized && "Decomposition is not initialized.");
      return Success;
    }

  protected:
    MatrixType m_qr;
    HCoeffsType m_hCoeffs;
    PermutationType m_colsPermutation;
    IntRowVectorType m_colsTranspositions;
    RowVectorType m_temp;
    RealRowVectorType m_colSqNorms;
    bool m_isInitialized, m_usePrescribedThreshold;
    RealScalar m_prescribedThreshold, m_maxpivot;
    Index m_nonzero_pivots;
    Index m_det_pq;
};

template<typename MatrixType>
typename MatrixType::RealScalar ColPivHouseholderQR<MatrixType>::absDeterminant() const
{
  using std::abs;
  eigen_assert(m_isInitialized && "ColPivHouseholderQR is not initialized.");
  eigen_assert(m_qr.rows() == m_qr.cols() && "You can't take the determinant of a non-square matrix!");
  return abs(m_qr.diagonal().prod());
}

template<typename MatrixType>
typename MatrixType::RealScalar ColPivHouseholderQR<MatrixType>::logAbsDeterminant() const
{
  eigen_assert(m_isInitialized && "ColPivHouseholderQR is not initialized.");
  eigen_assert(m_qr.rows() == m_qr.cols() && "You can't take the determinant of a non-square matrix!");
  return m_qr.diagonal().cwiseAbs().array().log().sum();
}

template<typename MatrixType>
ColPivHouseholderQR<MatrixType>& ColPivHouseholderQR<MatrixType>::compute(const MatrixType& matrix)
{
  using std::abs;
  Index rows = matrix.rows();
  Index cols = matrix.cols();
  Index size = matrix.diagonalSize();
  
  
  eigen_assert(cols<=NumTraits<int>::highest());

  m_qr = matrix;
  m_hCoeffs.resize(size);

  m_temp.resize(cols);

  m_colsTranspositions.resize(matrix.cols());
  Index number_of_transpositions = 0;

  m_colSqNorms.resize(cols);
  for(Index k = 0; k < cols; ++k)
    m_colSqNorms.coeffRef(k) = m_qr.col(k).squaredNorm();

  RealScalar threshold_helper = m_colSqNorms.maxCoeff() * numext::abs2(NumTraits<Scalar>::epsilon()) / RealScalar(rows);

  m_nonzero_pivots = size; 
  m_maxpivot = RealScalar(0);

  for(Index k = 0; k < size; ++k)
  {
    
    Index biggest_col_index;
    RealScalar biggest_col_sq_norm = m_colSqNorms.tail(cols-k).maxCoeff(&biggest_col_index);
    biggest_col_index += k;

    
    
    
    
    biggest_col_sq_norm = m_qr.col(biggest_col_index).tail(rows-k).squaredNorm();

    
    m_colSqNorms.coeffRef(biggest_col_index) = biggest_col_sq_norm;

    
    
    
    
    
    if(biggest_col_sq_norm < threshold_helper * RealScalar(rows-k))
    {
      m_nonzero_pivots = k;
      m_hCoeffs.tail(size-k).setZero();
      m_qr.bottomRightCorner(rows-k,cols-k)
          .template triangularView<StrictlyLower>()
          .setZero();
      break;
    }

    
    m_colsTranspositions.coeffRef(k) = biggest_col_index;
    if(k != biggest_col_index) {
      m_qr.col(k).swap(m_qr.col(biggest_col_index));
      std::swap(m_colSqNorms.coeffRef(k), m_colSqNorms.coeffRef(biggest_col_index));
      ++number_of_transpositions;
    }

    
    RealScalar beta;
    m_qr.col(k).tail(rows-k).makeHouseholderInPlace(m_hCoeffs.coeffRef(k), beta);

    
    m_qr.coeffRef(k,k) = beta;

    
    if(abs(beta) > m_maxpivot) m_maxpivot = abs(beta);

    
    m_qr.bottomRightCorner(rows-k, cols-k-1)
        .applyHouseholderOnTheLeft(m_qr.col(k).tail(rows-k-1), m_hCoeffs.coeffRef(k), &m_temp.coeffRef(k+1));

    
    m_colSqNorms.tail(cols-k-1) -= m_qr.row(k).tail(cols-k-1).cwiseAbs2();
  }

  m_colsPermutation.setIdentity(PermIndexType(cols));
  for(PermIndexType k = 0; k < m_nonzero_pivots; ++k)
    m_colsPermutation.applyTranspositionOnTheRight(k, PermIndexType(m_colsTranspositions.coeff(k)));

  m_det_pq = (number_of_transpositions%2) ? -1 : 1;
  m_isInitialized = true;

  return *this;
}

namespace internal {

template<typename _MatrixType, typename Rhs>
struct solve_retval<ColPivHouseholderQR<_MatrixType>, Rhs>
  : solve_retval_base<ColPivHouseholderQR<_MatrixType>, Rhs>
{
  EIGEN_MAKE_SOLVE_HELPERS(ColPivHouseholderQR<_MatrixType>,Rhs)

  template<typename Dest> void evalTo(Dest& dst) const
  {
    eigen_assert(rhs().rows() == dec().rows());

    const Index cols = dec().cols(),
				nonzero_pivots = dec().nonzeroPivots();

    if(nonzero_pivots == 0)
    {
      dst.setZero();
      return;
    }

    typename Rhs::PlainObject c(rhs());

    
    c.applyOnTheLeft(householderSequence(dec().matrixQR(), dec().hCoeffs())
                     .setLength(dec().nonzeroPivots())
		     .transpose()
      );

    dec().matrixR()
       .topLeftCorner(nonzero_pivots, nonzero_pivots)
       .template triangularView<Upper>()
       .solveInPlace(c.topRows(nonzero_pivots));

    for(Index i = 0; i < nonzero_pivots; ++i) dst.row(dec().colsPermutation().indices().coeff(i)) = c.row(i);
    for(Index i = nonzero_pivots; i < cols; ++i) dst.row(dec().colsPermutation().indices().coeff(i)).setZero();
  }
};

} 

template<typename MatrixType>
typename ColPivHouseholderQR<MatrixType>::HouseholderSequenceType ColPivHouseholderQR<MatrixType>
  ::householderQ() const
{
  eigen_assert(m_isInitialized && "ColPivHouseholderQR is not initialized.");
  return HouseholderSequenceType(m_qr, m_hCoeffs.conjugate()).setLength(m_nonzero_pivots);
}

template<typename Derived>
const ColPivHouseholderQR<typename MatrixBase<Derived>::PlainObject>
MatrixBase<Derived>::colPivHouseholderQr() const
{
  return ColPivHouseholderQR<PlainObject>(eval());
}

} 

#endif 
