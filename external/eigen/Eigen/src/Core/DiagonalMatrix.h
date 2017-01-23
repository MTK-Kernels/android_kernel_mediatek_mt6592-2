// Copyright (C) 2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2007-2009 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_DIAGONALMATRIX_H
#define EIGEN_DIAGONALMATRIX_H

namespace Eigen { 

#ifndef EIGEN_PARSED_BY_DOXYGEN
template<typename Derived>
class DiagonalBase : public EigenBase<Derived>
{
  public:
    typedef typename internal::traits<Derived>::DiagonalVectorType DiagonalVectorType;
    typedef typename DiagonalVectorType::Scalar Scalar;
    typedef typename DiagonalVectorType::RealScalar RealScalar;
    typedef typename internal::traits<Derived>::StorageKind StorageKind;
    typedef typename internal::traits<Derived>::Index Index;

    enum {
      RowsAtCompileTime = DiagonalVectorType::SizeAtCompileTime,
      ColsAtCompileTime = DiagonalVectorType::SizeAtCompileTime,
      MaxRowsAtCompileTime = DiagonalVectorType::MaxSizeAtCompileTime,
      MaxColsAtCompileTime = DiagonalVectorType::MaxSizeAtCompileTime,
      IsVectorAtCompileTime = 0,
      Flags = 0
    };

    typedef Matrix<Scalar, RowsAtCompileTime, ColsAtCompileTime, 0, MaxRowsAtCompileTime, MaxColsAtCompileTime> DenseMatrixType;
    typedef DenseMatrixType DenseType;
    typedef DiagonalMatrix<Scalar,DiagonalVectorType::SizeAtCompileTime,DiagonalVectorType::MaxSizeAtCompileTime> PlainObject;

    inline const Derived& derived() const { return *static_cast<const Derived*>(this); }
    inline Derived& derived() { return *static_cast<Derived*>(this); }

    DenseMatrixType toDenseMatrix() const { return derived(); }
    template<typename DenseDerived>
    void evalTo(MatrixBase<DenseDerived> &other) const;
    template<typename DenseDerived>
    void addTo(MatrixBase<DenseDerived> &other) const
    { other.diagonal() += diagonal(); }
    template<typename DenseDerived>
    void subTo(MatrixBase<DenseDerived> &other) const
    { other.diagonal() -= diagonal(); }

    inline const DiagonalVectorType& diagonal() const { return derived().diagonal(); }
    inline DiagonalVectorType& diagonal() { return derived().diagonal(); }

    inline Index rows() const { return diagonal().size(); }
    inline Index cols() const { return diagonal().size(); }

    template<typename MatrixDerived>
    const DiagonalProduct<MatrixDerived, Derived, OnTheLeft>
    operator*(const MatrixBase<MatrixDerived> &matrix) const
    {
      return DiagonalProduct<MatrixDerived, Derived, OnTheLeft>(matrix.derived(), derived());
    }

    inline const DiagonalWrapper<const CwiseUnaryOp<internal::scalar_inverse_op<Scalar>, const DiagonalVectorType> >
    inverse() const
    {
      return diagonal().cwiseInverse();
    }
    
    inline const DiagonalWrapper<const CwiseUnaryOp<internal::scalar_multiple_op<Scalar>, const DiagonalVectorType> >
    operator*(const Scalar& scalar) const
    {
      return diagonal() * scalar;
    }
    friend inline const DiagonalWrapper<const CwiseUnaryOp<internal::scalar_multiple_op<Scalar>, const DiagonalVectorType> >
    operator*(const Scalar& scalar, const DiagonalBase& other)
    {
      return other.diagonal() * scalar;
    }
    
    #ifdef EIGEN2_SUPPORT
    template<typename OtherDerived>
    bool isApprox(const DiagonalBase<OtherDerived>& other, typename NumTraits<Scalar>::Real precision = NumTraits<Scalar>::dummy_precision()) const
    {
      return diagonal().isApprox(other.diagonal(), precision);
    }
    template<typename OtherDerived>
    bool isApprox(const MatrixBase<OtherDerived>& other, typename NumTraits<Scalar>::Real precision = NumTraits<Scalar>::dummy_precision()) const
    {
      return toDenseMatrix().isApprox(other, precision);
    }
    #endif
};

template<typename Derived>
template<typename DenseDerived>
void DiagonalBase<Derived>::evalTo(MatrixBase<DenseDerived> &other) const
{
  other.setZero();
  other.diagonal() = diagonal();
}
#endif


namespace internal {
template<typename _Scalar, int SizeAtCompileTime, int MaxSizeAtCompileTime>
struct traits<DiagonalMatrix<_Scalar,SizeAtCompileTime,MaxSizeAtCompileTime> >
 : traits<Matrix<_Scalar,SizeAtCompileTime,SizeAtCompileTime,0,MaxSizeAtCompileTime,MaxSizeAtCompileTime> >
{
  typedef Matrix<_Scalar,SizeAtCompileTime,1,0,MaxSizeAtCompileTime,1> DiagonalVectorType;
  typedef Dense StorageKind;
  typedef DenseIndex Index;
  enum {
    Flags = LvalueBit
  };
};
}
template<typename _Scalar, int SizeAtCompileTime, int MaxSizeAtCompileTime>
class DiagonalMatrix
  : public DiagonalBase<DiagonalMatrix<_Scalar,SizeAtCompileTime,MaxSizeAtCompileTime> >
{
  public:
    #ifndef EIGEN_PARSED_BY_DOXYGEN
    typedef typename internal::traits<DiagonalMatrix>::DiagonalVectorType DiagonalVectorType;
    typedef const DiagonalMatrix& Nested;
    typedef _Scalar Scalar;
    typedef typename internal::traits<DiagonalMatrix>::StorageKind StorageKind;
    typedef typename internal::traits<DiagonalMatrix>::Index Index;
    #endif

  protected:

    DiagonalVectorType m_diagonal;

  public:

    
    inline const DiagonalVectorType& diagonal() const { return m_diagonal; }
    
    inline DiagonalVectorType& diagonal() { return m_diagonal; }

    
    inline DiagonalMatrix() {}

    
    inline DiagonalMatrix(Index dim) : m_diagonal(dim) {}

    
    inline DiagonalMatrix(const Scalar& x, const Scalar& y) : m_diagonal(x,y) {}

    
    inline DiagonalMatrix(const Scalar& x, const Scalar& y, const Scalar& z) : m_diagonal(x,y,z) {}

    
    template<typename OtherDerived>
    inline DiagonalMatrix(const DiagonalBase<OtherDerived>& other) : m_diagonal(other.diagonal()) {}

    #ifndef EIGEN_PARSED_BY_DOXYGEN
    
    inline DiagonalMatrix(const DiagonalMatrix& other) : m_diagonal(other.diagonal()) {}
    #endif

    
    template<typename OtherDerived>
    explicit inline DiagonalMatrix(const MatrixBase<OtherDerived>& other) : m_diagonal(other)
    {}

    
    template<typename OtherDerived>
    DiagonalMatrix& operator=(const DiagonalBase<OtherDerived>& other)
    {
      m_diagonal = other.diagonal();
      return *this;
    }

    #ifndef EIGEN_PARSED_BY_DOXYGEN
    DiagonalMatrix& operator=(const DiagonalMatrix& other)
    {
      m_diagonal = other.diagonal();
      return *this;
    }
    #endif

    
    inline void resize(Index size) { m_diagonal.resize(size); }
    
    inline void setZero() { m_diagonal.setZero(); }
    
    inline void setZero(Index size) { m_diagonal.setZero(size); }
    
    inline void setIdentity() { m_diagonal.setOnes(); }
    
    inline void setIdentity(Index size) { m_diagonal.setOnes(size); }
};


namespace internal {
template<typename _DiagonalVectorType>
struct traits<DiagonalWrapper<_DiagonalVectorType> >
{
  typedef _DiagonalVectorType DiagonalVectorType;
  typedef typename DiagonalVectorType::Scalar Scalar;
  typedef typename DiagonalVectorType::Index Index;
  typedef typename DiagonalVectorType::StorageKind StorageKind;
  enum {
    RowsAtCompileTime = DiagonalVectorType::SizeAtCompileTime,
    ColsAtCompileTime = DiagonalVectorType::SizeAtCompileTime,
    MaxRowsAtCompileTime = DiagonalVectorType::SizeAtCompileTime,
    MaxColsAtCompileTime = DiagonalVectorType::SizeAtCompileTime,
    Flags =  traits<DiagonalVectorType>::Flags & LvalueBit
  };
};
}

template<typename _DiagonalVectorType>
class DiagonalWrapper
  : public DiagonalBase<DiagonalWrapper<_DiagonalVectorType> >, internal::no_assignment_operator
{
  public:
    #ifndef EIGEN_PARSED_BY_DOXYGEN
    typedef _DiagonalVectorType DiagonalVectorType;
    typedef DiagonalWrapper Nested;
    #endif

    
    inline DiagonalWrapper(DiagonalVectorType& a_diagonal) : m_diagonal(a_diagonal) {}

    
    const DiagonalVectorType& diagonal() const { return m_diagonal; }

  protected:
    typename DiagonalVectorType::Nested m_diagonal;
};

template<typename Derived>
inline const DiagonalWrapper<const Derived>
MatrixBase<Derived>::asDiagonal() const
{
  return derived();
}

template<typename Derived>
bool MatrixBase<Derived>::isDiagonal(const RealScalar& prec) const
{
  using std::abs;
  if(cols() != rows()) return false;
  RealScalar maxAbsOnDiagonal = static_cast<RealScalar>(-1);
  for(Index j = 0; j < cols(); ++j)
  {
    RealScalar absOnDiagonal = abs(coeff(j,j));
    if(absOnDiagonal > maxAbsOnDiagonal) maxAbsOnDiagonal = absOnDiagonal;
  }
  for(Index j = 0; j < cols(); ++j)
    for(Index i = 0; i < j; ++i)
    {
      if(!internal::isMuchSmallerThan(coeff(i, j), maxAbsOnDiagonal, prec)) return false;
      if(!internal::isMuchSmallerThan(coeff(j, i), maxAbsOnDiagonal, prec)) return false;
    }
  return true;
}

} 

#endif 
