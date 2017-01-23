// Copyright (C) 2008-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2009 Benoit Jacob <jacob.benoit.1@gmail.com>
// Copyright (C) 2010 Vincent Lejeune
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_QR_H
#define EIGEN_QR_H

namespace Eigen { 

template<typename _MatrixType> class HouseholderQR
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
    typedef Matrix<Scalar, RowsAtCompileTime, RowsAtCompileTime, (MatrixType::Flags&RowMajorBit) ? RowMajor : ColMajor, MaxRowsAtCompileTime, MaxRowsAtCompileTime> MatrixQType;
    typedef typename internal::plain_diag_type<MatrixType>::type HCoeffsType;
    typedef typename internal::plain_row_type<MatrixType>::type RowVectorType;
    typedef HouseholderSequence<MatrixType,typename internal::remove_all<typename HCoeffsType::ConjugateReturnType>::type> HouseholderSequenceType;

    HouseholderQR() : m_qr(), m_hCoeffs(), m_temp(), m_isInitialized(false) {}

    HouseholderQR(Index rows, Index cols)
      : m_qr(rows, cols),
        m_hCoeffs((std::min)(rows,cols)),
        m_temp(cols),
        m_isInitialized(false) {}

    HouseholderQR(const MatrixType& matrix)
      : m_qr(matrix.rows(), matrix.cols()),
        m_hCoeffs((std::min)(matrix.rows(),matrix.cols())),
        m_temp(matrix.cols()),
        m_isInitialized(false)
    {
      compute(matrix);
    }

    template<typename Rhs>
    inline const internal::solve_retval<HouseholderQR, Rhs>
    solve(const MatrixBase<Rhs>& b) const
    {
      eigen_assert(m_isInitialized && "HouseholderQR is not initialized.");
      return internal::solve_retval<HouseholderQR, Rhs>(*this, b.derived());
    }

    HouseholderSequenceType householderQ() const
    {
      eigen_assert(m_isInitialized && "HouseholderQR is not initialized.");
      return HouseholderSequenceType(m_qr, m_hCoeffs.conjugate());
    }

    const MatrixType& matrixQR() const
    {
        eigen_assert(m_isInitialized && "HouseholderQR is not initialized.");
        return m_qr;
    }

    HouseholderQR& compute(const MatrixType& matrix);

    typename MatrixType::RealScalar absDeterminant() const;

    typename MatrixType::RealScalar logAbsDeterminant() const;

    inline Index rows() const { return m_qr.rows(); }
    inline Index cols() const { return m_qr.cols(); }
    
    const HCoeffsType& hCoeffs() const { return m_hCoeffs; }

  protected:
    MatrixType m_qr;
    HCoeffsType m_hCoeffs;
    RowVectorType m_temp;
    bool m_isInitialized;
};

template<typename MatrixType>
typename MatrixType::RealScalar HouseholderQR<MatrixType>::absDeterminant() const
{
  using std::abs;
  eigen_assert(m_isInitialized && "HouseholderQR is not initialized.");
  eigen_assert(m_qr.rows() == m_qr.cols() && "You can't take the determinant of a non-square matrix!");
  return abs(m_qr.diagonal().prod());
}

template<typename MatrixType>
typename MatrixType::RealScalar HouseholderQR<MatrixType>::logAbsDeterminant() const
{
  eigen_assert(m_isInitialized && "HouseholderQR is not initialized.");
  eigen_assert(m_qr.rows() == m_qr.cols() && "You can't take the determinant of a non-square matrix!");
  return m_qr.diagonal().cwiseAbs().array().log().sum();
}

namespace internal {

template<typename MatrixQR, typename HCoeffs>
void householder_qr_inplace_unblocked(MatrixQR& mat, HCoeffs& hCoeffs, typename MatrixQR::Scalar* tempData = 0)
{
  typedef typename MatrixQR::Index Index;
  typedef typename MatrixQR::Scalar Scalar;
  typedef typename MatrixQR::RealScalar RealScalar;
  Index rows = mat.rows();
  Index cols = mat.cols();
  Index size = (std::min)(rows,cols);

  eigen_assert(hCoeffs.size() == size);

  typedef Matrix<Scalar,MatrixQR::ColsAtCompileTime,1> TempType;
  TempType tempVector;
  if(tempData==0)
  {
    tempVector.resize(cols);
    tempData = tempVector.data();
  }

  for(Index k = 0; k < size; ++k)
  {
    Index remainingRows = rows - k;
    Index remainingCols = cols - k - 1;

    RealScalar beta;
    mat.col(k).tail(remainingRows).makeHouseholderInPlace(hCoeffs.coeffRef(k), beta);
    mat.coeffRef(k,k) = beta;

    
    mat.bottomRightCorner(remainingRows, remainingCols)
        .applyHouseholderOnTheLeft(mat.col(k).tail(remainingRows-1), hCoeffs.coeffRef(k), tempData+k+1);
  }
}

template<typename MatrixQR, typename HCoeffs>
void householder_qr_inplace_blocked(MatrixQR& mat, HCoeffs& hCoeffs,
                                       typename MatrixQR::Index maxBlockSize=32,
                                       typename MatrixQR::Scalar* tempData = 0)
{
  typedef typename MatrixQR::Index Index;
  typedef typename MatrixQR::Scalar Scalar;
  typedef Block<MatrixQR,Dynamic,Dynamic> BlockType;

  Index rows = mat.rows();
  Index cols = mat.cols();
  Index size = (std::min)(rows, cols);

  typedef Matrix<Scalar,Dynamic,1,ColMajor,MatrixQR::MaxColsAtCompileTime,1> TempType;
  TempType tempVector;
  if(tempData==0)
  {
    tempVector.resize(cols);
    tempData = tempVector.data();
  }

  Index blockSize = (std::min)(maxBlockSize,size);

  Index k = 0;
  for (k = 0; k < size; k += blockSize)
  {
    Index bs = (std::min)(size-k,blockSize);  
    Index tcols = cols - k - bs;            
    Index brows = rows-k;                   

    
    
    
    
    
    
    

    BlockType A11_21 = mat.block(k,k,brows,bs);
    Block<HCoeffs,Dynamic,1> hCoeffsSegment = hCoeffs.segment(k,bs);

    householder_qr_inplace_unblocked(A11_21, hCoeffsSegment, tempData);

    if(tcols)
    {
      BlockType A21_22 = mat.block(k,k+bs,brows,tcols);
      apply_block_householder_on_the_left(A21_22,A11_21,hCoeffsSegment.adjoint());
    }
  }
}

template<typename _MatrixType, typename Rhs>
struct solve_retval<HouseholderQR<_MatrixType>, Rhs>
  : solve_retval_base<HouseholderQR<_MatrixType>, Rhs>
{
  EIGEN_MAKE_SOLVE_HELPERS(HouseholderQR<_MatrixType>,Rhs)

  template<typename Dest> void evalTo(Dest& dst) const
  {
    const Index rows = dec().rows(), cols = dec().cols();
    const Index rank = (std::min)(rows, cols);
    eigen_assert(rhs().rows() == rows);

    typename Rhs::PlainObject c(rhs());

    
    c.applyOnTheLeft(householderSequence(
      dec().matrixQR().leftCols(rank),
      dec().hCoeffs().head(rank)).transpose()
    );

    dec().matrixQR()
       .topLeftCorner(rank, rank)
       .template triangularView<Upper>()
       .solveInPlace(c.topRows(rank));

    dst.topRows(rank) = c.topRows(rank);
    dst.bottomRows(cols-rank).setZero();
  }
};

} 

template<typename MatrixType>
HouseholderQR<MatrixType>& HouseholderQR<MatrixType>::compute(const MatrixType& matrix)
{
  Index rows = matrix.rows();
  Index cols = matrix.cols();
  Index size = (std::min)(rows,cols);

  m_qr = matrix;
  m_hCoeffs.resize(size);

  m_temp.resize(cols);

  internal::householder_qr_inplace_blocked(m_qr, m_hCoeffs, 48, m_temp.data());

  m_isInitialized = true;
  return *this;
}

template<typename Derived>
const HouseholderQR<typename MatrixBase<Derived>::PlainObject>
MatrixBase<Derived>::householderQr() const
{
  return HouseholderQR<PlainObject>(eval());
}

} 

#endif 
