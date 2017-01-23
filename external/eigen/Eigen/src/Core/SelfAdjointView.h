// Copyright (C) 2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_SELFADJOINTMATRIX_H
#define EIGEN_SELFADJOINTMATRIX_H

namespace Eigen { 


namespace internal {
template<typename MatrixType, unsigned int UpLo>
struct traits<SelfAdjointView<MatrixType, UpLo> > : traits<MatrixType>
{
  typedef typename nested<MatrixType>::type MatrixTypeNested;
  typedef typename remove_all<MatrixTypeNested>::type MatrixTypeNestedCleaned;
  typedef MatrixType ExpressionType;
  typedef typename MatrixType::PlainObject DenseMatrixType;
  enum {
    Mode = UpLo | SelfAdjoint,
    Flags =  MatrixTypeNestedCleaned::Flags & (HereditaryBits)
           & (~(PacketAccessBit | DirectAccessBit | LinearAccessBit)), 
    CoeffReadCost = MatrixTypeNestedCleaned::CoeffReadCost
  };
};
}

template <typename Lhs, int LhsMode, bool LhsIsVector,
          typename Rhs, int RhsMode, bool RhsIsVector>
struct SelfadjointProductMatrix;

template<typename MatrixType, unsigned int UpLo> class SelfAdjointView
  : public TriangularBase<SelfAdjointView<MatrixType, UpLo> >
{
  public:

    typedef TriangularBase<SelfAdjointView> Base;
    typedef typename internal::traits<SelfAdjointView>::MatrixTypeNested MatrixTypeNested;
    typedef typename internal::traits<SelfAdjointView>::MatrixTypeNestedCleaned MatrixTypeNestedCleaned;

    
    typedef typename internal::traits<SelfAdjointView>::Scalar Scalar; 

    typedef typename MatrixType::Index Index;

    enum {
      Mode = internal::traits<SelfAdjointView>::Mode
    };
    typedef typename MatrixType::PlainObject PlainObject;

    inline SelfAdjointView(MatrixType& matrix) : m_matrix(matrix)
    {}

    inline Index rows() const { return m_matrix.rows(); }
    inline Index cols() const { return m_matrix.cols(); }
    inline Index outerStride() const { return m_matrix.outerStride(); }
    inline Index innerStride() const { return m_matrix.innerStride(); }

    inline Scalar coeff(Index row, Index col) const
    {
      Base::check_coordinates_internal(row, col);
      return m_matrix.coeff(row, col);
    }

    inline Scalar& coeffRef(Index row, Index col)
    {
      Base::check_coordinates_internal(row, col);
      return m_matrix.const_cast_derived().coeffRef(row, col);
    }

    
    const MatrixTypeNestedCleaned& _expression() const { return m_matrix; }

    const MatrixTypeNestedCleaned& nestedExpression() const { return m_matrix; }
    MatrixTypeNestedCleaned& nestedExpression() { return *const_cast<MatrixTypeNestedCleaned*>(&m_matrix); }

    
    template<typename OtherDerived>
    SelfadjointProductMatrix<MatrixType,Mode,false,OtherDerived,0,OtherDerived::IsVectorAtCompileTime>
    operator*(const MatrixBase<OtherDerived>& rhs) const
    {
      return SelfadjointProductMatrix
              <MatrixType,Mode,false,OtherDerived,0,OtherDerived::IsVectorAtCompileTime>
              (m_matrix, rhs.derived());
    }

    
    template<typename OtherDerived> friend
    SelfadjointProductMatrix<OtherDerived,0,OtherDerived::IsVectorAtCompileTime,MatrixType,Mode,false>
    operator*(const MatrixBase<OtherDerived>& lhs, const SelfAdjointView& rhs)
    {
      return SelfadjointProductMatrix
              <OtherDerived,0,OtherDerived::IsVectorAtCompileTime,MatrixType,Mode,false>
              (lhs.derived(),rhs.m_matrix);
    }

    template<typename DerivedU, typename DerivedV>
    SelfAdjointView& rankUpdate(const MatrixBase<DerivedU>& u, const MatrixBase<DerivedV>& v, const Scalar& alpha = Scalar(1));

    template<typename DerivedU>
    SelfAdjointView& rankUpdate(const MatrixBase<DerivedU>& u, const Scalar& alpha = Scalar(1));


    const LLT<PlainObject, UpLo> llt() const;
    const LDLT<PlainObject, UpLo> ldlt() const;


    
    typedef typename NumTraits<Scalar>::Real RealScalar;
    
    typedef Matrix<RealScalar, internal::traits<MatrixType>::ColsAtCompileTime, 1> EigenvaluesReturnType;

    EigenvaluesReturnType eigenvalues() const;
    RealScalar operatorNorm() const;
    
    #ifdef EIGEN2_SUPPORT
    template<typename OtherDerived>
    SelfAdjointView& operator=(const MatrixBase<OtherDerived>& other)
    {
      enum {
        OtherPart = UpLo == Upper ? StrictlyLower : StrictlyUpper
      };
      m_matrix.const_cast_derived().template triangularView<UpLo>() = other;
      m_matrix.const_cast_derived().template triangularView<OtherPart>() = other.adjoint();
      return *this;
    }
    template<typename OtherMatrixType, unsigned int OtherMode>
    SelfAdjointView& operator=(const TriangularView<OtherMatrixType, OtherMode>& other)
    {
      enum {
        OtherPart = UpLo == Upper ? StrictlyLower : StrictlyUpper
      };
      m_matrix.const_cast_derived().template triangularView<UpLo>() = other.toDenseMatrix();
      m_matrix.const_cast_derived().template triangularView<OtherPart>() = other.toDenseMatrix().adjoint();
      return *this;
    }
    #endif

  protected:
    MatrixTypeNested m_matrix;
};




namespace internal {

template<typename Derived1, typename Derived2, int UnrollCount, bool ClearOpposite>
struct triangular_assignment_selector<Derived1, Derived2, (SelfAdjoint|Upper), UnrollCount, ClearOpposite>
{
  enum {
    col = (UnrollCount-1) / Derived1::RowsAtCompileTime,
    row = (UnrollCount-1) % Derived1::RowsAtCompileTime
  };

  static inline void run(Derived1 &dst, const Derived2 &src)
  {
    triangular_assignment_selector<Derived1, Derived2, (SelfAdjoint|Upper), UnrollCount-1, ClearOpposite>::run(dst, src);

    if(row == col)
      dst.coeffRef(row, col) = numext::real(src.coeff(row, col));
    else if(row < col)
      dst.coeffRef(col, row) = numext::conj(dst.coeffRef(row, col) = src.coeff(row, col));
  }
};

template<typename Derived1, typename Derived2, bool ClearOpposite>
struct triangular_assignment_selector<Derived1, Derived2, SelfAdjoint|Upper, 0, ClearOpposite>
{
  static inline void run(Derived1 &, const Derived2 &) {}
};

template<typename Derived1, typename Derived2, int UnrollCount, bool ClearOpposite>
struct triangular_assignment_selector<Derived1, Derived2, (SelfAdjoint|Lower), UnrollCount, ClearOpposite>
{
  enum {
    col = (UnrollCount-1) / Derived1::RowsAtCompileTime,
    row = (UnrollCount-1) % Derived1::RowsAtCompileTime
  };

  static inline void run(Derived1 &dst, const Derived2 &src)
  {
    triangular_assignment_selector<Derived1, Derived2, (SelfAdjoint|Lower), UnrollCount-1, ClearOpposite>::run(dst, src);

    if(row == col)
      dst.coeffRef(row, col) = numext::real(src.coeff(row, col));
    else if(row > col)
      dst.coeffRef(col, row) = numext::conj(dst.coeffRef(row, col) = src.coeff(row, col));
  }
};

template<typename Derived1, typename Derived2, bool ClearOpposite>
struct triangular_assignment_selector<Derived1, Derived2, SelfAdjoint|Lower, 0, ClearOpposite>
{
  static inline void run(Derived1 &, const Derived2 &) {}
};

template<typename Derived1, typename Derived2, bool ClearOpposite>
struct triangular_assignment_selector<Derived1, Derived2, SelfAdjoint|Upper, Dynamic, ClearOpposite>
{
  typedef typename Derived1::Index Index;
  static inline void run(Derived1 &dst, const Derived2 &src)
  {
    for(Index j = 0; j < dst.cols(); ++j)
    {
      for(Index i = 0; i < j; ++i)
      {
        dst.copyCoeff(i, j, src);
        dst.coeffRef(j,i) = numext::conj(dst.coeff(i,j));
      }
      dst.copyCoeff(j, j, src);
    }
  }
};

template<typename Derived1, typename Derived2, bool ClearOpposite>
struct triangular_assignment_selector<Derived1, Derived2, SelfAdjoint|Lower, Dynamic, ClearOpposite>
{
  static inline void run(Derived1 &dst, const Derived2 &src)
  {
  typedef typename Derived1::Index Index;
    for(Index i = 0; i < dst.rows(); ++i)
    {
      for(Index j = 0; j < i; ++j)
      {
        dst.copyCoeff(i, j, src);
        dst.coeffRef(j,i) = numext::conj(dst.coeff(i,j));
      }
      dst.copyCoeff(i, i, src);
    }
  }
};

} 


template<typename Derived>
template<unsigned int UpLo>
typename MatrixBase<Derived>::template ConstSelfAdjointViewReturnType<UpLo>::Type
MatrixBase<Derived>::selfadjointView() const
{
  return derived();
}

template<typename Derived>
template<unsigned int UpLo>
typename MatrixBase<Derived>::template SelfAdjointViewReturnType<UpLo>::Type
MatrixBase<Derived>::selfadjointView()
{
  return derived();
}

} 

#endif 
