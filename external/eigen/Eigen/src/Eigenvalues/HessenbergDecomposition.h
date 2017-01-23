// Copyright (C) 2008-2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2010 Jitse Niesen <jitse@maths.leeds.ac.uk>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_HESSENBERGDECOMPOSITION_H
#define EIGEN_HESSENBERGDECOMPOSITION_H

namespace Eigen { 

namespace internal {
  
template<typename MatrixType> struct HessenbergDecompositionMatrixHReturnType;
template<typename MatrixType>
struct traits<HessenbergDecompositionMatrixHReturnType<MatrixType> >
{
  typedef MatrixType ReturnType;
};

}

template<typename _MatrixType> class HessenbergDecomposition
{
  public:

    
    typedef _MatrixType MatrixType;

    enum {
      Size = MatrixType::RowsAtCompileTime,
      SizeMinusOne = Size == Dynamic ? Dynamic : Size - 1,
      Options = MatrixType::Options,
      MaxSize = MatrixType::MaxRowsAtCompileTime,
      MaxSizeMinusOne = MaxSize == Dynamic ? Dynamic : MaxSize - 1
    };

    
    typedef typename MatrixType::Scalar Scalar;
    typedef typename MatrixType::Index Index;

    typedef Matrix<Scalar, SizeMinusOne, 1, Options & ~RowMajor, MaxSizeMinusOne, 1> CoeffVectorType;

    
    typedef HouseholderSequence<MatrixType,typename internal::remove_all<typename CoeffVectorType::ConjugateReturnType>::type> HouseholderSequenceType;
    
    typedef internal::HessenbergDecompositionMatrixHReturnType<MatrixType> MatrixHReturnType;

    HessenbergDecomposition(Index size = Size==Dynamic ? 2 : Size)
      : m_matrix(size,size),
        m_temp(size),
        m_isInitialized(false)
    {
      if(size>1)
        m_hCoeffs.resize(size-1);
    }

    HessenbergDecomposition(const MatrixType& matrix)
      : m_matrix(matrix),
        m_temp(matrix.rows()),
        m_isInitialized(false)
    {
      if(matrix.rows()<2)
      {
        m_isInitialized = true;
        return;
      }
      m_hCoeffs.resize(matrix.rows()-1,1);
      _compute(m_matrix, m_hCoeffs, m_temp);
      m_isInitialized = true;
    }

    HessenbergDecomposition& compute(const MatrixType& matrix)
    {
      m_matrix = matrix;
      if(matrix.rows()<2)
      {
        m_isInitialized = true;
        return *this;
      }
      m_hCoeffs.resize(matrix.rows()-1,1);
      _compute(m_matrix, m_hCoeffs, m_temp);
      m_isInitialized = true;
      return *this;
    }

    const CoeffVectorType& householderCoefficients() const
    {
      eigen_assert(m_isInitialized && "HessenbergDecomposition is not initialized.");
      return m_hCoeffs;
    }

    const MatrixType& packedMatrix() const
    {
      eigen_assert(m_isInitialized && "HessenbergDecomposition is not initialized.");
      return m_matrix;
    }

    HouseholderSequenceType matrixQ() const
    {
      eigen_assert(m_isInitialized && "HessenbergDecomposition is not initialized.");
      return HouseholderSequenceType(m_matrix, m_hCoeffs.conjugate())
             .setLength(m_matrix.rows() - 1)
             .setShift(1);
    }

    MatrixHReturnType matrixH() const
    {
      eigen_assert(m_isInitialized && "HessenbergDecomposition is not initialized.");
      return MatrixHReturnType(*this);
    }

  private:

    typedef Matrix<Scalar, 1, Size, Options | RowMajor, 1, MaxSize> VectorType;
    typedef typename NumTraits<Scalar>::Real RealScalar;
    static void _compute(MatrixType& matA, CoeffVectorType& hCoeffs, VectorType& temp);

  protected:
    MatrixType m_matrix;
    CoeffVectorType m_hCoeffs;
    VectorType m_temp;
    bool m_isInitialized;
};

/** \internal
  * Performs a tridiagonal decomposition of \a matA in place.
  *
  * \param matA the input selfadjoint matrix
  * \param hCoeffs returned Householder coefficients
  *
  * The result is written in the lower triangular part of \a matA.
  *
  * Implemented from Golub's "%Matrix Computations", algorithm 8.3.1.
  *
  * \sa packedMatrix()
  */
template<typename MatrixType>
void HessenbergDecomposition<MatrixType>::_compute(MatrixType& matA, CoeffVectorType& hCoeffs, VectorType& temp)
{
  eigen_assert(matA.rows()==matA.cols());
  Index n = matA.rows();
  temp.resize(n);
  for (Index i = 0; i<n-1; ++i)
  {
    
    Index remainingSize = n-i-1;
    RealScalar beta;
    Scalar h;
    matA.col(i).tail(remainingSize).makeHouseholderInPlace(h, beta);
    matA.col(i).coeffRef(i+1) = beta;
    hCoeffs.coeffRef(i) = h;

    
    

    
    matA.bottomRightCorner(remainingSize, remainingSize)
        .applyHouseholderOnTheLeft(matA.col(i).tail(remainingSize-1), h, &temp.coeffRef(0));

    
    matA.rightCols(remainingSize)
        .applyHouseholderOnTheRight(matA.col(i).tail(remainingSize-1).conjugate(), numext::conj(h), &temp.coeffRef(0));
  }
}

namespace internal {

template<typename MatrixType> struct HessenbergDecompositionMatrixHReturnType
: public ReturnByValue<HessenbergDecompositionMatrixHReturnType<MatrixType> >
{
    typedef typename MatrixType::Index Index;
  public:
    HessenbergDecompositionMatrixHReturnType(const HessenbergDecomposition<MatrixType>& hess) : m_hess(hess) { }

    template <typename ResultType>
    inline void evalTo(ResultType& result) const
    {
      result = m_hess.packedMatrix();
      Index n = result.rows();
      if (n>2)
        result.bottomLeftCorner(n-2, n-2).template triangularView<Lower>().setZero();
    }

    Index rows() const { return m_hess.packedMatrix().rows(); }
    Index cols() const { return m_hess.packedMatrix().cols(); }

  protected:
    const HessenbergDecomposition<MatrixType>& m_hess;
};

} 

} 

#endif 
