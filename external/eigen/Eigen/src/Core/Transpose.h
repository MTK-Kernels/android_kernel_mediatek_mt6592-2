// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
// Copyright (C) 2009-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_TRANSPOSE_H
#define EIGEN_TRANSPOSE_H

namespace Eigen { 


namespace internal {
template<typename MatrixType>
struct traits<Transpose<MatrixType> > : traits<MatrixType>
{
  typedef typename MatrixType::Scalar Scalar;
  typedef typename nested<MatrixType>::type MatrixTypeNested;
  typedef typename remove_reference<MatrixTypeNested>::type MatrixTypeNestedPlain;
  typedef typename traits<MatrixType>::StorageKind StorageKind;
  typedef typename traits<MatrixType>::XprKind XprKind;
  enum {
    RowsAtCompileTime = MatrixType::ColsAtCompileTime,
    ColsAtCompileTime = MatrixType::RowsAtCompileTime,
    MaxRowsAtCompileTime = MatrixType::MaxColsAtCompileTime,
    MaxColsAtCompileTime = MatrixType::MaxRowsAtCompileTime,
    FlagsLvalueBit = is_lvalue<MatrixType>::value ? LvalueBit : 0,
    Flags0 = MatrixTypeNestedPlain::Flags & ~(LvalueBit | NestByRefBit),
    Flags1 = Flags0 | FlagsLvalueBit,
    Flags = Flags1 ^ RowMajorBit,
    CoeffReadCost = MatrixTypeNestedPlain::CoeffReadCost,
    InnerStrideAtCompileTime = inner_stride_at_compile_time<MatrixType>::ret,
    OuterStrideAtCompileTime = outer_stride_at_compile_time<MatrixType>::ret
  };
};
}

template<typename MatrixType, typename StorageKind> class TransposeImpl;

template<typename MatrixType> class Transpose
  : public TransposeImpl<MatrixType,typename internal::traits<MatrixType>::StorageKind>
{
  public:

    typedef typename TransposeImpl<MatrixType,typename internal::traits<MatrixType>::StorageKind>::Base Base;
    EIGEN_GENERIC_PUBLIC_INTERFACE(Transpose)

    inline Transpose(MatrixType& a_matrix) : m_matrix(a_matrix) {}

    EIGEN_INHERIT_ASSIGNMENT_OPERATORS(Transpose)

    inline Index rows() const { return m_matrix.cols(); }
    inline Index cols() const { return m_matrix.rows(); }

    
    const typename internal::remove_all<typename MatrixType::Nested>::type&
    nestedExpression() const { return m_matrix; }

    
    typename internal::remove_all<typename MatrixType::Nested>::type&
    nestedExpression() { return m_matrix.const_cast_derived(); }

  protected:
    typename MatrixType::Nested m_matrix;
};

namespace internal {

template<typename MatrixType, bool HasDirectAccess = has_direct_access<MatrixType>::ret>
struct TransposeImpl_base
{
  typedef typename dense_xpr_base<Transpose<MatrixType> >::type type;
};

template<typename MatrixType>
struct TransposeImpl_base<MatrixType, false>
{
  typedef typename dense_xpr_base<Transpose<MatrixType> >::type type;
};

} 

template<typename MatrixType> class TransposeImpl<MatrixType,Dense>
  : public internal::TransposeImpl_base<MatrixType>::type
{
  public:

    typedef typename internal::TransposeImpl_base<MatrixType>::type Base;
    EIGEN_DENSE_PUBLIC_INTERFACE(Transpose<MatrixType>)
    EIGEN_INHERIT_ASSIGNMENT_OPERATORS(TransposeImpl)

    inline Index innerStride() const { return derived().nestedExpression().innerStride(); }
    inline Index outerStride() const { return derived().nestedExpression().outerStride(); }

    typedef typename internal::conditional<
                       internal::is_lvalue<MatrixType>::value,
                       Scalar,
                       const Scalar
                     >::type ScalarWithConstIfNotLvalue;

    inline ScalarWithConstIfNotLvalue* data() { return derived().nestedExpression().data(); }
    inline const Scalar* data() const { return derived().nestedExpression().data(); }

    inline ScalarWithConstIfNotLvalue& coeffRef(Index rowId, Index colId)
    {
      EIGEN_STATIC_ASSERT_LVALUE(MatrixType)
      return derived().nestedExpression().const_cast_derived().coeffRef(colId, rowId);
    }

    inline ScalarWithConstIfNotLvalue& coeffRef(Index index)
    {
      EIGEN_STATIC_ASSERT_LVALUE(MatrixType)
      return derived().nestedExpression().const_cast_derived().coeffRef(index);
    }

    inline const Scalar& coeffRef(Index rowId, Index colId) const
    {
      return derived().nestedExpression().coeffRef(colId, rowId);
    }

    inline const Scalar& coeffRef(Index index) const
    {
      return derived().nestedExpression().coeffRef(index);
    }

    inline CoeffReturnType coeff(Index rowId, Index colId) const
    {
      return derived().nestedExpression().coeff(colId, rowId);
    }

    inline CoeffReturnType coeff(Index index) const
    {
      return derived().nestedExpression().coeff(index);
    }

    template<int LoadMode>
    inline const PacketScalar packet(Index rowId, Index colId) const
    {
      return derived().nestedExpression().template packet<LoadMode>(colId, rowId);
    }

    template<int LoadMode>
    inline void writePacket(Index rowId, Index colId, const PacketScalar& x)
    {
      derived().nestedExpression().const_cast_derived().template writePacket<LoadMode>(colId, rowId, x);
    }

    template<int LoadMode>
    inline const PacketScalar packet(Index index) const
    {
      return derived().nestedExpression().template packet<LoadMode>(index);
    }

    template<int LoadMode>
    inline void writePacket(Index index, const PacketScalar& x)
    {
      derived().nestedExpression().const_cast_derived().template writePacket<LoadMode>(index, x);
    }
};

template<typename Derived>
inline Transpose<Derived>
DenseBase<Derived>::transpose()
{
  return derived();
}

template<typename Derived>
inline typename DenseBase<Derived>::ConstTransposeReturnType
DenseBase<Derived>::transpose() const
{
  return ConstTransposeReturnType(derived());
}

template<typename Derived>
inline const typename MatrixBase<Derived>::AdjointReturnType
MatrixBase<Derived>::adjoint() const
{
  return this->transpose(); 
                            
}


namespace internal {

template<typename MatrixType,
  bool IsSquare = (MatrixType::RowsAtCompileTime == MatrixType::ColsAtCompileTime) && MatrixType::RowsAtCompileTime!=Dynamic>
struct inplace_transpose_selector;

template<typename MatrixType>
struct inplace_transpose_selector<MatrixType,true> { 
  static void run(MatrixType& m) {
    m.matrix().template triangularView<StrictlyUpper>().swap(m.matrix().transpose());
  }
};

template<typename MatrixType>
struct inplace_transpose_selector<MatrixType,false> { 
  static void run(MatrixType& m) {
    if (m.rows()==m.cols())
      m.matrix().template triangularView<StrictlyUpper>().swap(m.matrix().transpose());
    else
      m = m.transpose().eval();
  }
};

} 

template<typename Derived>
inline void DenseBase<Derived>::transposeInPlace()
{
  eigen_assert((rows() == cols() || (RowsAtCompileTime == Dynamic && ColsAtCompileTime == Dynamic))
               && "transposeInPlace() called on a non-square non-resizable matrix");
  internal::inplace_transpose_selector<Derived>::run(derived());
}


template<typename Derived>
inline void MatrixBase<Derived>::adjointInPlace()
{
  derived() = adjoint().eval();
}

#ifndef EIGEN_NO_DEBUG


namespace internal {

template<typename BinOp,typename NestedXpr,typename Rhs>
struct blas_traits<SelfCwiseBinaryOp<BinOp,NestedXpr,Rhs> >
 : blas_traits<NestedXpr>
{
  typedef SelfCwiseBinaryOp<BinOp,NestedXpr,Rhs> XprType;
  static inline const XprType extract(const XprType& x) { return x; }
};

template<bool DestIsTransposed, typename OtherDerived>
struct check_transpose_aliasing_compile_time_selector
{
  enum { ret = bool(blas_traits<OtherDerived>::IsTransposed) != DestIsTransposed };
};

template<bool DestIsTransposed, typename BinOp, typename DerivedA, typename DerivedB>
struct check_transpose_aliasing_compile_time_selector<DestIsTransposed,CwiseBinaryOp<BinOp,DerivedA,DerivedB> >
{
  enum { ret =    bool(blas_traits<DerivedA>::IsTransposed) != DestIsTransposed
               || bool(blas_traits<DerivedB>::IsTransposed) != DestIsTransposed
  };
};

template<typename Scalar, bool DestIsTransposed, typename OtherDerived>
struct check_transpose_aliasing_run_time_selector
{
  static bool run(const Scalar* dest, const OtherDerived& src)
  {
    return (bool(blas_traits<OtherDerived>::IsTransposed) != DestIsTransposed) && (dest!=0 && dest==(const Scalar*)extract_data(src));
  }
};

template<typename Scalar, bool DestIsTransposed, typename BinOp, typename DerivedA, typename DerivedB>
struct check_transpose_aliasing_run_time_selector<Scalar,DestIsTransposed,CwiseBinaryOp<BinOp,DerivedA,DerivedB> >
{
  static bool run(const Scalar* dest, const CwiseBinaryOp<BinOp,DerivedA,DerivedB>& src)
  {
    return ((blas_traits<DerivedA>::IsTransposed != DestIsTransposed) && (dest!=0 && dest==(const Scalar*)extract_data(src.lhs())))
        || ((blas_traits<DerivedB>::IsTransposed != DestIsTransposed) && (dest!=0 && dest==(const Scalar*)extract_data(src.rhs())));
  }
};


template<typename Derived, typename OtherDerived,
         bool MightHaveTransposeAliasing
                 = check_transpose_aliasing_compile_time_selector
                     <blas_traits<Derived>::IsTransposed,OtherDerived>::ret
        >
struct checkTransposeAliasing_impl
{
    static void run(const Derived& dst, const OtherDerived& other)
    {
        eigen_assert((!check_transpose_aliasing_run_time_selector
                      <typename Derived::Scalar,blas_traits<Derived>::IsTransposed,OtherDerived>
                      ::run(extract_data(dst), other))
          && "aliasing detected during transposition, use transposeInPlace() "
             "or evaluate the rhs into a temporary using .eval()");

    }
};

template<typename Derived, typename OtherDerived>
struct checkTransposeAliasing_impl<Derived, OtherDerived, false>
{
    static void run(const Derived&, const OtherDerived&)
    {
    }
};

} 

template<typename Derived>
template<typename OtherDerived>
void DenseBase<Derived>::checkTransposeAliasing(const OtherDerived& other) const
{
    internal::checkTransposeAliasing_impl<Derived, OtherDerived>::run(derived(), other);
}
#endif

} 

#endif 
