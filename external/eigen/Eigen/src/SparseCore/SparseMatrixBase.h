// Copyright (C) 2008-2011 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_SPARSEMATRIXBASE_H
#define EIGEN_SPARSEMATRIXBASE_H

namespace Eigen { 

template<typename Derived> class SparseMatrixBase : public EigenBase<Derived>
{
  public:

    typedef typename internal::traits<Derived>::Scalar Scalar;
    typedef typename internal::packet_traits<Scalar>::type PacketScalar;
    typedef typename internal::traits<Derived>::StorageKind StorageKind;
    typedef typename internal::traits<Derived>::Index Index;
    typedef typename internal::add_const_on_value_type_if_arithmetic<
                         typename internal::packet_traits<Scalar>::type
                     >::type PacketReturnType;

    typedef SparseMatrixBase StorageBaseType;
    typedef EigenBase<Derived> Base;
    
    template<typename OtherDerived>
    Derived& operator=(const EigenBase<OtherDerived> &other)
    {
      other.derived().evalTo(derived());
      return derived();
    }

    enum {

      RowsAtCompileTime = internal::traits<Derived>::RowsAtCompileTime,

      ColsAtCompileTime = internal::traits<Derived>::ColsAtCompileTime,


      SizeAtCompileTime = (internal::size_at_compile_time<internal::traits<Derived>::RowsAtCompileTime,
                                                   internal::traits<Derived>::ColsAtCompileTime>::ret),

      MaxRowsAtCompileTime = RowsAtCompileTime,
      MaxColsAtCompileTime = ColsAtCompileTime,

      MaxSizeAtCompileTime = (internal::size_at_compile_time<MaxRowsAtCompileTime,
                                                      MaxColsAtCompileTime>::ret),

      IsVectorAtCompileTime = RowsAtCompileTime == 1 || ColsAtCompileTime == 1,

      Flags = internal::traits<Derived>::Flags,

      CoeffReadCost = internal::traits<Derived>::CoeffReadCost,

      IsRowMajor = Flags&RowMajorBit ? 1 : 0,
      
      InnerSizeAtCompileTime = int(IsVectorAtCompileTime) ? int(SizeAtCompileTime)
                             : int(IsRowMajor) ? int(ColsAtCompileTime) : int(RowsAtCompileTime),

      #ifndef EIGEN_PARSED_BY_DOXYGEN
      _HasDirectAccess = (int(Flags)&DirectAccessBit) ? 1 : 0 
      #endif
    };

    
    typedef typename internal::conditional<NumTraits<Scalar>::IsComplex,
                        CwiseUnaryOp<internal::scalar_conjugate_op<Scalar>, Eigen::Transpose<const Derived> >,
                        Transpose<const Derived>
                     >::type AdjointReturnType;


    typedef SparseMatrix<Scalar, Flags&RowMajorBit ? RowMajor : ColMajor, Index> PlainObject;


#ifndef EIGEN_PARSED_BY_DOXYGEN
    typedef typename NumTraits<Scalar>::Real RealScalar;

    typedef typename internal::conditional<_HasDirectAccess, const Scalar&, Scalar>::type CoeffReturnType;

    
    typedef CwiseNullaryOp<internal::scalar_constant_op<Scalar>,Matrix<Scalar,Dynamic,Dynamic> > ConstantReturnType;

    
    typedef Matrix<Scalar,EIGEN_SIZE_MAX(RowsAtCompileTime,ColsAtCompileTime),
                          EIGEN_SIZE_MAX(RowsAtCompileTime,ColsAtCompileTime)> SquareMatrixType;

    inline const Derived& derived() const { return *static_cast<const Derived*>(this); }
    inline Derived& derived() { return *static_cast<Derived*>(this); }
    inline Derived& const_cast_derived() const
    { return *static_cast<Derived*>(const_cast<SparseMatrixBase*>(this)); }
#endif 

#define EIGEN_CURRENT_STORAGE_BASE_CLASS Eigen::SparseMatrixBase
#   include "../plugins/CommonCwiseUnaryOps.h"
#   include "../plugins/CommonCwiseBinaryOps.h"
#   include "../plugins/MatrixCwiseUnaryOps.h"
#   include "../plugins/MatrixCwiseBinaryOps.h"
#   include "../plugins/BlockMethods.h"
#   ifdef EIGEN_SPARSEMATRIXBASE_PLUGIN
#     include EIGEN_SPARSEMATRIXBASE_PLUGIN
#   endif
#   undef EIGEN_CURRENT_STORAGE_BASE_CLASS
#undef EIGEN_CURRENT_STORAGE_BASE_CLASS

    
    inline Index rows() const { return derived().rows(); }
    
    inline Index cols() const { return derived().cols(); }
    inline Index size() const { return rows() * cols(); }
    inline Index nonZeros() const { return derived().nonZeros(); }
    inline bool isVector() const { return rows()==1 || cols()==1; }
    Index outerSize() const { return (int(Flags)&RowMajorBit) ? this->rows() : this->cols(); }
    Index innerSize() const { return (int(Flags)&RowMajorBit) ? this->cols() : this->rows(); }

    bool isRValue() const { return m_isRValue; }
    Derived& markAsRValue() { m_isRValue = true; return derived(); }

    SparseMatrixBase() : m_isRValue(false) {  }

    
    template<typename OtherDerived>
    Derived& operator=(const ReturnByValue<OtherDerived>& other)
    {
      other.evalTo(derived());
      return derived();
    }


    template<typename OtherDerived>
    inline Derived& operator=(const SparseMatrixBase<OtherDerived>& other)
    {
      return assign(other.derived());
    }

    inline Derived& operator=(const Derived& other)
    {
      return assign(other.derived());
    }

  protected:

    template<typename OtherDerived>
    inline Derived& assign(const OtherDerived& other)
    {
      const bool transpose = (Flags & RowMajorBit) != (OtherDerived::Flags & RowMajorBit);
      const Index outerSize = (int(OtherDerived::Flags) & RowMajorBit) ? other.rows() : other.cols();
      if ((!transpose) && other.isRValue())
      {
        
        derived().resize(other.rows(), other.cols());
        derived().setZero();
        derived().reserve((std::max)(this->rows(),this->cols())*2);
        for (Index j=0; j<outerSize; ++j)
        {
          derived().startVec(j);
          for (typename OtherDerived::InnerIterator it(other, j); it; ++it)
          {
            Scalar v = it.value();
            derived().insertBackByOuterInner(j,it.index()) = v;
          }
        }
        derived().finalize();
      }
      else
      {
        assignGeneric(other);
      }
      return derived();
    }

    template<typename OtherDerived>
    inline void assignGeneric(const OtherDerived& other)
    {
      
      eigen_assert(( ((internal::traits<Derived>::SupportedAccessPatterns&OuterRandomAccessPattern)==OuterRandomAccessPattern) ||
                  (!((Flags & RowMajorBit) != (OtherDerived::Flags & RowMajorBit)))) &&
                  "the transpose operation is supposed to be handled in SparseMatrix::operator=");

      enum { Flip = (Flags & RowMajorBit) != (OtherDerived::Flags & RowMajorBit) };

      const Index outerSize = other.outerSize();
      
      
      Derived temp(other.rows(), other.cols());

      temp.reserve((std::max)(this->rows(),this->cols())*2);
      for (Index j=0; j<outerSize; ++j)
      {
        temp.startVec(j);
        for (typename OtherDerived::InnerIterator it(other.derived(), j); it; ++it)
        {
          Scalar v = it.value();
          temp.insertBackByOuterInner(Flip?it.index():j,Flip?j:it.index()) = v;
        }
      }
      temp.finalize();

      derived() = temp.markAsRValue();
    }

  public:

    template<typename Lhs, typename Rhs>
    inline Derived& operator=(const SparseSparseProduct<Lhs,Rhs>& product);

    friend std::ostream & operator << (std::ostream & s, const SparseMatrixBase& m)
    {
      typedef typename Derived::Nested Nested;
      typedef typename internal::remove_all<Nested>::type NestedCleaned;

      if (Flags&RowMajorBit)
      {
        const Nested nm(m.derived());
        for (Index row=0; row<nm.outerSize(); ++row)
        {
          Index col = 0;
          for (typename NestedCleaned::InnerIterator it(nm.derived(), row); it; ++it)
          {
            for ( ; col<it.index(); ++col)
              s << "0 ";
            s << it.value() << " ";
            ++col;
          }
          for ( ; col<m.cols(); ++col)
            s << "0 ";
          s << std::endl;
        }
      }
      else
      {
        const Nested nm(m.derived());
        if (m.cols() == 1) {
          Index row = 0;
          for (typename NestedCleaned::InnerIterator it(nm.derived(), 0); it; ++it)
          {
            for ( ; row<it.index(); ++row)
              s << "0" << std::endl;
            s << it.value() << std::endl;
            ++row;
          }
          for ( ; row<m.rows(); ++row)
            s << "0" << std::endl;
        }
        else
        {
          SparseMatrix<Scalar, RowMajorBit, Index> trans = m;
          s << static_cast<const SparseMatrixBase<SparseMatrix<Scalar, RowMajorBit, Index> >&>(trans);
        }
      }
      return s;
    }

    template<typename OtherDerived>
    Derived& operator+=(const SparseMatrixBase<OtherDerived>& other);
    template<typename OtherDerived>
    Derived& operator-=(const SparseMatrixBase<OtherDerived>& other);

    Derived& operator*=(const Scalar& other);
    Derived& operator/=(const Scalar& other);

    #define EIGEN_SPARSE_CWISE_PRODUCT_RETURN_TYPE \
      CwiseBinaryOp< \
        internal::scalar_product_op< \
          typename internal::scalar_product_traits< \
            typename internal::traits<Derived>::Scalar, \
            typename internal::traits<OtherDerived>::Scalar \
          >::ReturnType \
        >, \
        const Derived, \
        const OtherDerived \
      >

    template<typename OtherDerived>
    EIGEN_STRONG_INLINE const EIGEN_SPARSE_CWISE_PRODUCT_RETURN_TYPE
    cwiseProduct(const MatrixBase<OtherDerived> &other) const;

    
    template<typename OtherDerived>
    const typename SparseSparseProductReturnType<Derived,OtherDerived>::Type
    operator*(const SparseMatrixBase<OtherDerived> &other) const;

    
    template<typename OtherDerived>
    const SparseDiagonalProduct<Derived,OtherDerived>
    operator*(const DiagonalBase<OtherDerived> &other) const;

    
    template<typename OtherDerived> friend
    const SparseDiagonalProduct<OtherDerived,Derived>
    operator*(const DiagonalBase<OtherDerived> &lhs, const SparseMatrixBase& rhs)
    { return SparseDiagonalProduct<OtherDerived,Derived>(lhs.derived(), rhs.derived()); }

    
    template<typename OtherDerived> friend
    const typename DenseSparseProductReturnType<OtherDerived,Derived>::Type
    operator*(const MatrixBase<OtherDerived>& lhs, const Derived& rhs)
    { return typename DenseSparseProductReturnType<OtherDerived,Derived>::Type(lhs.derived(),rhs); }

    
    template<typename OtherDerived>
    const typename SparseDenseProductReturnType<Derived,OtherDerived>::Type
    operator*(const MatrixBase<OtherDerived> &other) const
    { return typename SparseDenseProductReturnType<Derived,OtherDerived>::Type(derived(), other.derived()); }
    
     
    SparseSymmetricPermutationProduct<Derived,Upper|Lower> twistedBy(const PermutationMatrix<Dynamic,Dynamic,Index>& perm) const
    {
      return SparseSymmetricPermutationProduct<Derived,Upper|Lower>(derived(), perm);
    }

    template<typename OtherDerived>
    Derived& operator*=(const SparseMatrixBase<OtherDerived>& other);

    #ifdef EIGEN2_SUPPORT
    
    template<typename OtherDerived>
    typename internal::plain_matrix_type_column_major<OtherDerived>::type
    solveTriangular(const MatrixBase<OtherDerived>& other) const;

    
    template<typename OtherDerived>
    void solveTriangularInPlace(MatrixBase<OtherDerived>& other) const;
    #endif 

    template<int Mode>
    inline const SparseTriangularView<Derived, Mode> triangularView() const;

    template<unsigned int UpLo> inline const SparseSelfAdjointView<Derived, UpLo> selfadjointView() const;
    template<unsigned int UpLo> inline SparseSelfAdjointView<Derived, UpLo> selfadjointView();

    template<typename OtherDerived> Scalar dot(const MatrixBase<OtherDerived>& other) const;
    template<typename OtherDerived> Scalar dot(const SparseMatrixBase<OtherDerived>& other) const;
    RealScalar squaredNorm() const;
    RealScalar norm()  const;
    RealScalar blueNorm() const;

    Transpose<Derived> transpose() { return derived(); }
    const Transpose<const Derived> transpose() const { return derived(); }
    const AdjointReturnType adjoint() const { return transpose(); }

    
    typedef Block<Derived,IsRowMajor?1:Dynamic,IsRowMajor?Dynamic:1,true>       InnerVectorReturnType;
    typedef Block<const Derived,IsRowMajor?1:Dynamic,IsRowMajor?Dynamic:1,true> ConstInnerVectorReturnType;
    InnerVectorReturnType innerVector(Index outer);
    const ConstInnerVectorReturnType innerVector(Index outer) const;

    
    Block<Derived,Dynamic,Dynamic,true> innerVectors(Index outerStart, Index outerSize);
    const Block<const Derived,Dynamic,Dynamic,true> innerVectors(Index outerStart, Index outerSize) const;

    
    template<typename DenseDerived>
    void evalTo(MatrixBase<DenseDerived>& dst) const
    {
      dst.setZero();
      for (Index j=0; j<outerSize(); ++j)
        for (typename Derived::InnerIterator i(derived(),j); i; ++i)
          dst.coeffRef(i.row(),i.col()) = i.value();
    }

    Matrix<Scalar,RowsAtCompileTime,ColsAtCompileTime> toDense() const
    {
      return derived();
    }

    template<typename OtherDerived>
    bool isApprox(const SparseMatrixBase<OtherDerived>& other,
                  const RealScalar& prec = NumTraits<Scalar>::dummy_precision()) const
    { return toDense().isApprox(other.toDense(),prec); }

    template<typename OtherDerived>
    bool isApprox(const MatrixBase<OtherDerived>& other,
                  const RealScalar& prec = NumTraits<Scalar>::dummy_precision()) const
    { return toDense().isApprox(other,prec); }

    inline const typename internal::eval<Derived>::type eval() const
    { return typename internal::eval<Derived>::type(derived()); }

    Scalar sum() const;

  protected:

    bool m_isRValue;
};

} 

#endif 
