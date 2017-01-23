// Copyright (C) 2009-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_CWISE_UNARY_VIEW_H
#define EIGEN_CWISE_UNARY_VIEW_H

namespace Eigen {


namespace internal {
template<typename ViewOp, typename MatrixType>
struct traits<CwiseUnaryView<ViewOp, MatrixType> >
 : traits<MatrixType>
{
  typedef typename result_of<
                     ViewOp(typename traits<MatrixType>::Scalar)
                   >::type Scalar;
  typedef typename MatrixType::Nested MatrixTypeNested;
  typedef typename remove_all<MatrixTypeNested>::type _MatrixTypeNested;
  enum {
    Flags = (traits<_MatrixTypeNested>::Flags & (HereditaryBits | LvalueBit | LinearAccessBit | DirectAccessBit)),
    CoeffReadCost = traits<_MatrixTypeNested>::CoeffReadCost + functor_traits<ViewOp>::Cost,
    MatrixTypeInnerStride =  inner_stride_at_compile_time<MatrixType>::ret,
    
    
    InnerStrideAtCompileTime = MatrixTypeInnerStride == Dynamic
                             ? int(Dynamic)
                             : int(MatrixTypeInnerStride) * int(sizeof(typename traits<MatrixType>::Scalar) / sizeof(Scalar)),
    OuterStrideAtCompileTime = outer_stride_at_compile_time<MatrixType>::ret == Dynamic
                             ? int(Dynamic)
                             : outer_stride_at_compile_time<MatrixType>::ret * int(sizeof(typename traits<MatrixType>::Scalar) / sizeof(Scalar))
  };
};
}

template<typename ViewOp, typename MatrixType, typename StorageKind>
class CwiseUnaryViewImpl;

template<typename ViewOp, typename MatrixType>
class CwiseUnaryView : public CwiseUnaryViewImpl<ViewOp, MatrixType, typename internal::traits<MatrixType>::StorageKind>
{
  public:

    typedef typename CwiseUnaryViewImpl<ViewOp, MatrixType,typename internal::traits<MatrixType>::StorageKind>::Base Base;
    EIGEN_GENERIC_PUBLIC_INTERFACE(CwiseUnaryView)

    inline CwiseUnaryView(const MatrixType& mat, const ViewOp& func = ViewOp())
      : m_matrix(mat), m_functor(func) {}

    EIGEN_INHERIT_ASSIGNMENT_OPERATORS(CwiseUnaryView)

    EIGEN_STRONG_INLINE Index rows() const { return m_matrix.rows(); }
    EIGEN_STRONG_INLINE Index cols() const { return m_matrix.cols(); }

    
    const ViewOp& functor() const { return m_functor; }

    
    const typename internal::remove_all<typename MatrixType::Nested>::type&
    nestedExpression() const { return m_matrix; }

    
    typename internal::remove_all<typename MatrixType::Nested>::type&
    nestedExpression() { return m_matrix.const_cast_derived(); }

  protected:
    
    typename internal::nested<MatrixType>::type m_matrix;
    ViewOp m_functor;
};

template<typename ViewOp, typename MatrixType>
class CwiseUnaryViewImpl<ViewOp,MatrixType,Dense>
  : public internal::dense_xpr_base< CwiseUnaryView<ViewOp, MatrixType> >::type
{
  public:

    typedef CwiseUnaryView<ViewOp, MatrixType> Derived;
    typedef typename internal::dense_xpr_base< CwiseUnaryView<ViewOp, MatrixType> >::type Base;

    EIGEN_DENSE_PUBLIC_INTERFACE(Derived)
    EIGEN_INHERIT_ASSIGNMENT_OPERATORS(CwiseUnaryViewImpl)
    
    inline Scalar* data() { return &coeffRef(0); }
    inline const Scalar* data() const { return &coeff(0); }

    inline Index innerStride() const
    {
      return derived().nestedExpression().innerStride() * sizeof(typename internal::traits<MatrixType>::Scalar) / sizeof(Scalar);
    }

    inline Index outerStride() const
    {
      return derived().nestedExpression().outerStride() * sizeof(typename internal::traits<MatrixType>::Scalar) / sizeof(Scalar);
    }

    EIGEN_STRONG_INLINE CoeffReturnType coeff(Index row, Index col) const
    {
      return derived().functor()(derived().nestedExpression().coeff(row, col));
    }

    EIGEN_STRONG_INLINE CoeffReturnType coeff(Index index) const
    {
      return derived().functor()(derived().nestedExpression().coeff(index));
    }

    EIGEN_STRONG_INLINE Scalar& coeffRef(Index row, Index col)
    {
      return derived().functor()(const_cast_derived().nestedExpression().coeffRef(row, col));
    }

    EIGEN_STRONG_INLINE Scalar& coeffRef(Index index)
    {
      return derived().functor()(const_cast_derived().nestedExpression().coeffRef(index));
    }
};

} 

#endif 
