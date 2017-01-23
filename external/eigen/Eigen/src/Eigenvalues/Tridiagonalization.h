// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2010 Jitse Niesen <jitse@maths.leeds.ac.uk>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_TRIDIAGONALIZATION_H
#define EIGEN_TRIDIAGONALIZATION_H

namespace Eigen { 

namespace internal {
  
template<typename MatrixType> struct TridiagonalizationMatrixTReturnType;
template<typename MatrixType>
struct traits<TridiagonalizationMatrixTReturnType<MatrixType> >
{
  typedef typename MatrixType::PlainObject ReturnType;
};

template<typename MatrixType, typename CoeffVectorType>
void tridiagonalization_inplace(MatrixType& matA, CoeffVectorType& hCoeffs);
}

template<typename _MatrixType> class Tridiagonalization
{
  public:

    
    typedef _MatrixType MatrixType;

    typedef typename MatrixType::Scalar Scalar;
    typedef typename NumTraits<Scalar>::Real RealScalar;
    typedef typename MatrixType::Index Index;

    enum {
      Size = MatrixType::RowsAtCompileTime,
      SizeMinusOne = Size == Dynamic ? Dynamic : (Size > 1 ? Size - 1 : 1),
      Options = MatrixType::Options,
      MaxSize = MatrixType::MaxRowsAtCompileTime,
      MaxSizeMinusOne = MaxSize == Dynamic ? Dynamic : (MaxSize > 1 ? MaxSize - 1 : 1)
    };

    typedef Matrix<Scalar, SizeMinusOne, 1, Options & ~RowMajor, MaxSizeMinusOne, 1> CoeffVectorType;
    typedef typename internal::plain_col_type<MatrixType, RealScalar>::type DiagonalType;
    typedef Matrix<RealScalar, SizeMinusOne, 1, Options & ~RowMajor, MaxSizeMinusOne, 1> SubDiagonalType;
    typedef typename internal::remove_all<typename MatrixType::RealReturnType>::type MatrixTypeRealView;
    typedef internal::TridiagonalizationMatrixTReturnType<MatrixTypeRealView> MatrixTReturnType;

    typedef typename internal::conditional<NumTraits<Scalar>::IsComplex,
              typename internal::add_const_on_value_type<typename Diagonal<const MatrixType>::RealReturnType>::type,
              const Diagonal<const MatrixType>
            >::type DiagonalReturnType;

    typedef typename internal::conditional<NumTraits<Scalar>::IsComplex,
              typename internal::add_const_on_value_type<typename Diagonal<
                Block<const MatrixType,SizeMinusOne,SizeMinusOne> >::RealReturnType>::type,
              const Diagonal<
                Block<const MatrixType,SizeMinusOne,SizeMinusOne> >
            >::type SubDiagonalReturnType;

    
    typedef HouseholderSequence<MatrixType,typename internal::remove_all<typename CoeffVectorType::ConjugateReturnType>::type> HouseholderSequenceType;

    Tridiagonalization(Index size = Size==Dynamic ? 2 : Size)
      : m_matrix(size,size),
        m_hCoeffs(size > 1 ? size-1 : 1),
        m_isInitialized(false)
    {}

    Tridiagonalization(const MatrixType& matrix)
      : m_matrix(matrix),
        m_hCoeffs(matrix.cols() > 1 ? matrix.cols()-1 : 1),
        m_isInitialized(false)
    {
      internal::tridiagonalization_inplace(m_matrix, m_hCoeffs);
      m_isInitialized = true;
    }

    Tridiagonalization& compute(const MatrixType& matrix)
    {
      m_matrix = matrix;
      m_hCoeffs.resize(matrix.rows()-1, 1);
      internal::tridiagonalization_inplace(m_matrix, m_hCoeffs);
      m_isInitialized = true;
      return *this;
    }

    inline CoeffVectorType householderCoefficients() const
    {
      eigen_assert(m_isInitialized && "Tridiagonalization is not initialized.");
      return m_hCoeffs;
    }

    inline const MatrixType& packedMatrix() const
    {
      eigen_assert(m_isInitialized && "Tridiagonalization is not initialized.");
      return m_matrix;
    }

    HouseholderSequenceType matrixQ() const
    {
      eigen_assert(m_isInitialized && "Tridiagonalization is not initialized.");
      return HouseholderSequenceType(m_matrix, m_hCoeffs.conjugate())
             .setLength(m_matrix.rows() - 1)
             .setShift(1);
    }

    MatrixTReturnType matrixT() const
    {
      eigen_assert(m_isInitialized && "Tridiagonalization is not initialized.");
      return MatrixTReturnType(m_matrix.real());
    }

    DiagonalReturnType diagonal() const;

    SubDiagonalReturnType subDiagonal() const;

  protected:

    MatrixType m_matrix;
    CoeffVectorType m_hCoeffs;
    bool m_isInitialized;
};

template<typename MatrixType>
typename Tridiagonalization<MatrixType>::DiagonalReturnType
Tridiagonalization<MatrixType>::diagonal() const
{
  eigen_assert(m_isInitialized && "Tridiagonalization is not initialized.");
  return m_matrix.diagonal();
}

template<typename MatrixType>
typename Tridiagonalization<MatrixType>::SubDiagonalReturnType
Tridiagonalization<MatrixType>::subDiagonal() const
{
  eigen_assert(m_isInitialized && "Tridiagonalization is not initialized.");
  Index n = m_matrix.rows();
  return Block<const MatrixType,SizeMinusOne,SizeMinusOne>(m_matrix, 1, 0, n-1,n-1).diagonal();
}

namespace internal {

template<typename MatrixType, typename CoeffVectorType>
void tridiagonalization_inplace(MatrixType& matA, CoeffVectorType& hCoeffs)
{
  using numext::conj;
  typedef typename MatrixType::Index Index;
  typedef typename MatrixType::Scalar Scalar;
  typedef typename MatrixType::RealScalar RealScalar;
  Index n = matA.rows();
  eigen_assert(n==matA.cols());
  eigen_assert(n==hCoeffs.size()+1 || n==1);
  
  for (Index i = 0; i<n-1; ++i)
  {
    Index remainingSize = n-i-1;
    RealScalar beta;
    Scalar h;
    matA.col(i).tail(remainingSize).makeHouseholderInPlace(h, beta);

    
    
    matA.col(i).coeffRef(i+1) = 1;

    hCoeffs.tail(n-i-1).noalias() = (matA.bottomRightCorner(remainingSize,remainingSize).template selfadjointView<Lower>()
                                  * (conj(h) * matA.col(i).tail(remainingSize)));

    hCoeffs.tail(n-i-1) += (conj(h)*Scalar(-0.5)*(hCoeffs.tail(remainingSize).dot(matA.col(i).tail(remainingSize)))) * matA.col(i).tail(n-i-1);

    matA.bottomRightCorner(remainingSize, remainingSize).template selfadjointView<Lower>()
      .rankUpdate(matA.col(i).tail(remainingSize), hCoeffs.tail(remainingSize), -1);

    matA.col(i).coeffRef(i+1) = beta;
    hCoeffs.coeffRef(i) = h;
  }
}

template<typename MatrixType,
         int Size=MatrixType::ColsAtCompileTime,
         bool IsComplex=NumTraits<typename MatrixType::Scalar>::IsComplex>
struct tridiagonalization_inplace_selector;

template<typename MatrixType, typename DiagonalType, typename SubDiagonalType>
void tridiagonalization_inplace(MatrixType& mat, DiagonalType& diag, SubDiagonalType& subdiag, bool extractQ)
{
  eigen_assert(mat.cols()==mat.rows() && diag.size()==mat.rows() && subdiag.size()==mat.rows()-1);
  tridiagonalization_inplace_selector<MatrixType>::run(mat, diag, subdiag, extractQ);
}

template<typename MatrixType, int Size, bool IsComplex>
struct tridiagonalization_inplace_selector
{
  typedef typename Tridiagonalization<MatrixType>::CoeffVectorType CoeffVectorType;
  typedef typename Tridiagonalization<MatrixType>::HouseholderSequenceType HouseholderSequenceType;
  typedef typename MatrixType::Index Index;
  template<typename DiagonalType, typename SubDiagonalType>
  static void run(MatrixType& mat, DiagonalType& diag, SubDiagonalType& subdiag, bool extractQ)
  {
    CoeffVectorType hCoeffs(mat.cols()-1);
    tridiagonalization_inplace(mat,hCoeffs);
    diag = mat.diagonal().real();
    subdiag = mat.template diagonal<-1>().real();
    if(extractQ)
      mat = HouseholderSequenceType(mat, hCoeffs.conjugate())
            .setLength(mat.rows() - 1)
            .setShift(1);
  }
};

template<typename MatrixType>
struct tridiagonalization_inplace_selector<MatrixType,3,false>
{
  typedef typename MatrixType::Scalar Scalar;
  typedef typename MatrixType::RealScalar RealScalar;

  template<typename DiagonalType, typename SubDiagonalType>
  static void run(MatrixType& mat, DiagonalType& diag, SubDiagonalType& subdiag, bool extractQ)
  {
    using std::sqrt;
    diag[0] = mat(0,0);
    RealScalar v1norm2 = numext::abs2(mat(2,0));
    if(v1norm2 == RealScalar(0))
    {
      diag[1] = mat(1,1);
      diag[2] = mat(2,2);
      subdiag[0] = mat(1,0);
      subdiag[1] = mat(2,1);
      if (extractQ)
        mat.setIdentity();
    }
    else
    {
      RealScalar beta = sqrt(numext::abs2(mat(1,0)) + v1norm2);
      RealScalar invBeta = RealScalar(1)/beta;
      Scalar m01 = mat(1,0) * invBeta;
      Scalar m02 = mat(2,0) * invBeta;
      Scalar q = RealScalar(2)*m01*mat(2,1) + m02*(mat(2,2) - mat(1,1));
      diag[1] = mat(1,1) + m02*q;
      diag[2] = mat(2,2) - m02*q;
      subdiag[0] = beta;
      subdiag[1] = mat(2,1) - m01 * q;
      if (extractQ)
      {
        mat << 1,   0,    0,
               0, m01,  m02,
               0, m02, -m01;
      }
    }
  }
};

template<typename MatrixType, bool IsComplex>
struct tridiagonalization_inplace_selector<MatrixType,1,IsComplex>
{
  typedef typename MatrixType::Scalar Scalar;

  template<typename DiagonalType, typename SubDiagonalType>
  static void run(MatrixType& mat, DiagonalType& diag, SubDiagonalType&, bool extractQ)
  {
    diag(0,0) = numext::real(mat(0,0));
    if(extractQ)
      mat(0,0) = Scalar(1);
  }
};

template<typename MatrixType> struct TridiagonalizationMatrixTReturnType
: public ReturnByValue<TridiagonalizationMatrixTReturnType<MatrixType> >
{
    typedef typename MatrixType::Index Index;
  public:
    TridiagonalizationMatrixTReturnType(const MatrixType& mat) : m_matrix(mat) { }

    template <typename ResultType>
    inline void evalTo(ResultType& result) const
    {
      result.setZero();
      result.template diagonal<1>() = m_matrix.template diagonal<-1>().conjugate();
      result.diagonal() = m_matrix.diagonal();
      result.template diagonal<-1>() = m_matrix.template diagonal<-1>();
    }

    Index rows() const { return m_matrix.rows(); }
    Index cols() const { return m_matrix.cols(); }

  protected:
    typename MatrixType::Nested m_matrix;
};

} 

} 

#endif 
