// Copyright (C) 2008-2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2009 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_FULLPIVOTINGHOUSEHOLDERQR_H
#define EIGEN_FULLPIVOTINGHOUSEHOLDERQR_H

namespace Eigen { 

namespace internal {

template<typename MatrixType> struct FullPivHouseholderQRMatrixQReturnType;

template<typename MatrixType>
struct traits<FullPivHouseholderQRMatrixQReturnType<MatrixType> >
{
  typedef typename MatrixType::PlainObject ReturnType;
};

}

template<typename _MatrixType> class FullPivHouseholderQR
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
    typedef internal::FullPivHouseholderQRMatrixQReturnType<MatrixType> MatrixQReturnType;
    typedef typename internal::plain_diag_type<MatrixType>::type HCoeffsType;
    typedef Matrix<Index, 1,
                   EIGEN_SIZE_MIN_PREFER_DYNAMIC(ColsAtCompileTime,RowsAtCompileTime), RowMajor, 1,
                   EIGEN_SIZE_MIN_PREFER_FIXED(MaxColsAtCompileTime,MaxRowsAtCompileTime)> IntDiagSizeVectorType;
    typedef PermutationMatrix<ColsAtCompileTime, MaxColsAtCompileTime> PermutationType;
    typedef typename internal::plain_row_type<MatrixType>::type RowVectorType;
    typedef typename internal::plain_col_type<MatrixType>::type ColVectorType;

    FullPivHouseholderQR()
      : m_qr(),
        m_hCoeffs(),
        m_rows_transpositions(),
        m_cols_transpositions(),
        m_cols_permutation(),
        m_temp(),
        m_isInitialized(false),
        m_usePrescribedThreshold(false) {}

    FullPivHouseholderQR(Index rows, Index cols)
      : m_qr(rows, cols),
        m_hCoeffs((std::min)(rows,cols)),
        m_rows_transpositions((std::min)(rows,cols)),
        m_cols_transpositions((std::min)(rows,cols)),
        m_cols_permutation(cols),
        m_temp(cols),
        m_isInitialized(false),
        m_usePrescribedThreshold(false) {}

    FullPivHouseholderQR(const MatrixType& matrix)
      : m_qr(matrix.rows(), matrix.cols()),
        m_hCoeffs((std::min)(matrix.rows(), matrix.cols())),
        m_rows_transpositions((std::min)(matrix.rows(), matrix.cols())),
        m_cols_transpositions((std::min)(matrix.rows(), matrix.cols())),
        m_cols_permutation(matrix.cols()),
        m_temp(matrix.cols()),
        m_isInitialized(false),
        m_usePrescribedThreshold(false)
    {
      compute(matrix);
    }

    template<typename Rhs>
    inline const internal::solve_retval<FullPivHouseholderQR, Rhs>
    solve(const MatrixBase<Rhs>& b) const
    {
      eigen_assert(m_isInitialized && "FullPivHouseholderQR is not initialized.");
      return internal::solve_retval<FullPivHouseholderQR, Rhs>(*this, b.derived());
    }

    MatrixQReturnType matrixQ(void) const;

    const MatrixType& matrixQR() const
    {
      eigen_assert(m_isInitialized && "FullPivHouseholderQR is not initialized.");
      return m_qr;
    }

    FullPivHouseholderQR& compute(const MatrixType& matrix);

    
    const PermutationType& colsPermutation() const
    {
      eigen_assert(m_isInitialized && "FullPivHouseholderQR is not initialized.");
      return m_cols_permutation;
    }

    
    const IntDiagSizeVectorType& rowsTranspositions() const
    {
      eigen_assert(m_isInitialized && "FullPivHouseholderQR is not initialized.");
      return m_rows_transpositions;
    }

    typename MatrixType::RealScalar absDeterminant() const;

    typename MatrixType::RealScalar logAbsDeterminant() const;

    inline Index rank() const
    {
      using std::abs;
      eigen_assert(m_isInitialized && "FullPivHouseholderQR is not initialized.");
      RealScalar premultiplied_threshold = abs(m_maxpivot) * threshold();
      Index result = 0;
      for(Index i = 0; i < m_nonzero_pivots; ++i)
        result += (abs(m_qr.coeff(i,i)) > premultiplied_threshold);
      return result;
    }

    inline Index dimensionOfKernel() const
    {
      eigen_assert(m_isInitialized && "FullPivHouseholderQR is not initialized.");
      return cols() - rank();
    }

    inline bool isInjective() const
    {
      eigen_assert(m_isInitialized && "FullPivHouseholderQR is not initialized.");
      return rank() == cols();
    }

    inline bool isSurjective() const
    {
      eigen_assert(m_isInitialized && "FullPivHouseholderQR is not initialized.");
      return rank() == rows();
    }

    inline bool isInvertible() const
    {
      eigen_assert(m_isInitialized && "FullPivHouseholderQR is not initialized.");
      return isInjective() && isSurjective();
    }

    inline const
    internal::solve_retval<FullPivHouseholderQR, typename MatrixType::IdentityReturnType>
    inverse() const
    {
      eigen_assert(m_isInitialized && "FullPivHouseholderQR is not initialized.");
      return internal::solve_retval<FullPivHouseholderQR,typename MatrixType::IdentityReturnType>
               (*this, MatrixType::Identity(m_qr.rows(), m_qr.cols()));
    }

    inline Index rows() const { return m_qr.rows(); }
    inline Index cols() const { return m_qr.cols(); }
    
    const HCoeffsType& hCoeffs() const { return m_hCoeffs; }

    FullPivHouseholderQR& setThreshold(const RealScalar& threshold)
    {
      m_usePrescribedThreshold = true;
      m_prescribedThreshold = threshold;
      return *this;
    }

    FullPivHouseholderQR& setThreshold(Default_t)
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
      eigen_assert(m_isInitialized && "LU is not initialized.");
      return m_nonzero_pivots;
    }

    RealScalar maxPivot() const { return m_maxpivot; }

  protected:
    MatrixType m_qr;
    HCoeffsType m_hCoeffs;
    IntDiagSizeVectorType m_rows_transpositions;
    IntDiagSizeVectorType m_cols_transpositions;
    PermutationType m_cols_permutation;
    RowVectorType m_temp;
    bool m_isInitialized, m_usePrescribedThreshold;
    RealScalar m_prescribedThreshold, m_maxpivot;
    Index m_nonzero_pivots;
    RealScalar m_precision;
    Index m_det_pq;
};

template<typename MatrixType>
typename MatrixType::RealScalar FullPivHouseholderQR<MatrixType>::absDeterminant() const
{
  using std::abs;
  eigen_assert(m_isInitialized && "FullPivHouseholderQR is not initialized.");
  eigen_assert(m_qr.rows() == m_qr.cols() && "You can't take the determinant of a non-square matrix!");
  return abs(m_qr.diagonal().prod());
}

template<typename MatrixType>
typename MatrixType::RealScalar FullPivHouseholderQR<MatrixType>::logAbsDeterminant() const
{
  eigen_assert(m_isInitialized && "FullPivHouseholderQR is not initialized.");
  eigen_assert(m_qr.rows() == m_qr.cols() && "You can't take the determinant of a non-square matrix!");
  return m_qr.diagonal().cwiseAbs().array().log().sum();
}

template<typename MatrixType>
FullPivHouseholderQR<MatrixType>& FullPivHouseholderQR<MatrixType>::compute(const MatrixType& matrix)
{
  using std::abs;
  Index rows = matrix.rows();
  Index cols = matrix.cols();
  Index size = (std::min)(rows,cols);

  m_qr = matrix;
  m_hCoeffs.resize(size);

  m_temp.resize(cols);

  m_precision = NumTraits<Scalar>::epsilon() * RealScalar(size);

  m_rows_transpositions.resize(size);
  m_cols_transpositions.resize(size);
  Index number_of_transpositions = 0;

  RealScalar biggest(0);

  m_nonzero_pivots = size; 
  m_maxpivot = RealScalar(0);

  for (Index k = 0; k < size; ++k)
  {
    Index row_of_biggest_in_corner, col_of_biggest_in_corner;
    RealScalar biggest_in_corner;

    biggest_in_corner = m_qr.bottomRightCorner(rows-k, cols-k)
                            .cwiseAbs()
                            .maxCoeff(&row_of_biggest_in_corner, &col_of_biggest_in_corner);
    row_of_biggest_in_corner += k;
    col_of_biggest_in_corner += k;
    if(k==0) biggest = biggest_in_corner;

    
    if(internal::isMuchSmallerThan(biggest_in_corner, biggest, m_precision))
    {
      m_nonzero_pivots = k;
      for(Index i = k; i < size; i++)
      {
        m_rows_transpositions.coeffRef(i) = i;
        m_cols_transpositions.coeffRef(i) = i;
        m_hCoeffs.coeffRef(i) = Scalar(0);
      }
      break;
    }

    m_rows_transpositions.coeffRef(k) = row_of_biggest_in_corner;
    m_cols_transpositions.coeffRef(k) = col_of_biggest_in_corner;
    if(k != row_of_biggest_in_corner) {
      m_qr.row(k).tail(cols-k).swap(m_qr.row(row_of_biggest_in_corner).tail(cols-k));
      ++number_of_transpositions;
    }
    if(k != col_of_biggest_in_corner) {
      m_qr.col(k).swap(m_qr.col(col_of_biggest_in_corner));
      ++number_of_transpositions;
    }

    RealScalar beta;
    m_qr.col(k).tail(rows-k).makeHouseholderInPlace(m_hCoeffs.coeffRef(k), beta);
    m_qr.coeffRef(k,k) = beta;

    
    if(abs(beta) > m_maxpivot) m_maxpivot = abs(beta);

    m_qr.bottomRightCorner(rows-k, cols-k-1)
        .applyHouseholderOnTheLeft(m_qr.col(k).tail(rows-k-1), m_hCoeffs.coeffRef(k), &m_temp.coeffRef(k+1));
  }

  m_cols_permutation.setIdentity(cols);
  for(Index k = 0; k < size; ++k)
    m_cols_permutation.applyTranspositionOnTheRight(k, m_cols_transpositions.coeff(k));

  m_det_pq = (number_of_transpositions%2) ? -1 : 1;
  m_isInitialized = true;

  return *this;
}

namespace internal {

template<typename _MatrixType, typename Rhs>
struct solve_retval<FullPivHouseholderQR<_MatrixType>, Rhs>
  : solve_retval_base<FullPivHouseholderQR<_MatrixType>, Rhs>
{
  EIGEN_MAKE_SOLVE_HELPERS(FullPivHouseholderQR<_MatrixType>,Rhs)

  template<typename Dest> void evalTo(Dest& dst) const
  {
    const Index rows = dec().rows(), cols = dec().cols();
    eigen_assert(rhs().rows() == rows);

    
    
    if(dec().rank()==0)
    {
      dst.setZero();
      return;
    }

    typename Rhs::PlainObject c(rhs());

    Matrix<Scalar,1,Rhs::ColsAtCompileTime> temp(rhs().cols());
    for (Index k = 0; k < dec().rank(); ++k)
    {
      Index remainingSize = rows-k;
      c.row(k).swap(c.row(dec().rowsTranspositions().coeff(k)));
      c.bottomRightCorner(remainingSize, rhs().cols())
       .applyHouseholderOnTheLeft(dec().matrixQR().col(k).tail(remainingSize-1),
                                  dec().hCoeffs().coeff(k), &temp.coeffRef(0));
    }

    dec().matrixQR()
       .topLeftCorner(dec().rank(), dec().rank())
       .template triangularView<Upper>()
       .solveInPlace(c.topRows(dec().rank()));

    for(Index i = 0; i < dec().rank(); ++i) dst.row(dec().colsPermutation().indices().coeff(i)) = c.row(i);
    for(Index i = dec().rank(); i < cols; ++i) dst.row(dec().colsPermutation().indices().coeff(i)).setZero();
  }
};

template<typename MatrixType> struct FullPivHouseholderQRMatrixQReturnType
  : public ReturnByValue<FullPivHouseholderQRMatrixQReturnType<MatrixType> >
{
public:
  typedef typename MatrixType::Index Index;
  typedef typename FullPivHouseholderQR<MatrixType>::IntDiagSizeVectorType IntDiagSizeVectorType;
  typedef typename internal::plain_diag_type<MatrixType>::type HCoeffsType;
  typedef Matrix<typename MatrixType::Scalar, 1, MatrixType::RowsAtCompileTime, RowMajor, 1,
                 MatrixType::MaxRowsAtCompileTime> WorkVectorType;

  FullPivHouseholderQRMatrixQReturnType(const MatrixType&       qr,
                                        const HCoeffsType&      hCoeffs,
                                        const IntDiagSizeVectorType& rowsTranspositions)
    : m_qr(qr),
      m_hCoeffs(hCoeffs),
      m_rowsTranspositions(rowsTranspositions)
      {}

  template <typename ResultType>
  void evalTo(ResultType& result) const
  {
    const Index rows = m_qr.rows();
    WorkVectorType workspace(rows);
    evalTo(result, workspace);
  }

  template <typename ResultType>
  void evalTo(ResultType& result, WorkVectorType& workspace) const
  {
    using numext::conj;
    
    
    
    const Index rows = m_qr.rows();
    const Index cols = m_qr.cols();
    const Index size = (std::min)(rows, cols);
    workspace.resize(rows);
    result.setIdentity(rows, rows);
    for (Index k = size-1; k >= 0; k--)
    {
      result.block(k, k, rows-k, rows-k)
            .applyHouseholderOnTheLeft(m_qr.col(k).tail(rows-k-1), conj(m_hCoeffs.coeff(k)), &workspace.coeffRef(k));
      result.row(k).swap(result.row(m_rowsTranspositions.coeff(k)));
    }
  }

    Index rows() const { return m_qr.rows(); }
    Index cols() const { return m_qr.rows(); }

protected:
  typename MatrixType::Nested m_qr;
  typename HCoeffsType::Nested m_hCoeffs;
  typename IntDiagSizeVectorType::Nested m_rowsTranspositions;
};

} 

template<typename MatrixType>
inline typename FullPivHouseholderQR<MatrixType>::MatrixQReturnType FullPivHouseholderQR<MatrixType>::matrixQ() const
{
  eigen_assert(m_isInitialized && "FullPivHouseholderQR is not initialized.");
  return MatrixQReturnType(m_qr, m_hCoeffs, m_rows_transpositions);
}

template<typename Derived>
const FullPivHouseholderQR<typename MatrixBase<Derived>::PlainObject>
MatrixBase<Derived>::fullPivHouseholderQr() const
{
  return FullPivHouseholderQR<PlainObject>(eval());
}

} 

#endif 
