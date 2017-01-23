// Copyright (C) 2009-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_ARRAYWRAPPER_H
#define EIGEN_ARRAYWRAPPER_H

namespace Eigen { 


namespace internal {
template<typename ExpressionType>
struct traits<ArrayWrapper<ExpressionType> >
  : public traits<typename remove_all<typename ExpressionType::Nested>::type >
{
  typedef ArrayXpr XprKind;
  
  enum {
    Flags0 = traits<typename remove_all<typename ExpressionType::Nested>::type >::Flags,
    Flags = Flags0 & ~NestByRefBit
  };
};
}

template<typename ExpressionType>
class ArrayWrapper : public ArrayBase<ArrayWrapper<ExpressionType> >
{
  public:
    typedef ArrayBase<ArrayWrapper> Base;
    EIGEN_DENSE_PUBLIC_INTERFACE(ArrayWrapper)
    EIGEN_INHERIT_ASSIGNMENT_OPERATORS(ArrayWrapper)

    typedef typename internal::conditional<
                       internal::is_lvalue<ExpressionType>::value,
                       Scalar,
                       const Scalar
                     >::type ScalarWithConstIfNotLvalue;

    typedef typename internal::nested<ExpressionType>::type NestedExpressionType;

    inline ArrayWrapper(ExpressionType& matrix) : m_expression(matrix) {}

    inline Index rows() const { return m_expression.rows(); }
    inline Index cols() const { return m_expression.cols(); }
    inline Index outerStride() const { return m_expression.outerStride(); }
    inline Index innerStride() const { return m_expression.innerStride(); }

    inline ScalarWithConstIfNotLvalue* data() { return m_expression.const_cast_derived().data(); }
    inline const Scalar* data() const { return m_expression.data(); }

    inline CoeffReturnType coeff(Index rowId, Index colId) const
    {
      return m_expression.coeff(rowId, colId);
    }

    inline Scalar& coeffRef(Index rowId, Index colId)
    {
      return m_expression.const_cast_derived().coeffRef(rowId, colId);
    }

    inline const Scalar& coeffRef(Index rowId, Index colId) const
    {
      return m_expression.const_cast_derived().coeffRef(rowId, colId);
    }

    inline CoeffReturnType coeff(Index index) const
    {
      return m_expression.coeff(index);
    }

    inline Scalar& coeffRef(Index index)
    {
      return m_expression.const_cast_derived().coeffRef(index);
    }

    inline const Scalar& coeffRef(Index index) const
    {
      return m_expression.const_cast_derived().coeffRef(index);
    }

    template<int LoadMode>
    inline const PacketScalar packet(Index rowId, Index colId) const
    {
      return m_expression.template packet<LoadMode>(rowId, colId);
    }

    template<int LoadMode>
    inline void writePacket(Index rowId, Index colId, const PacketScalar& val)
    {
      m_expression.const_cast_derived().template writePacket<LoadMode>(rowId, colId, val);
    }

    template<int LoadMode>
    inline const PacketScalar packet(Index index) const
    {
      return m_expression.template packet<LoadMode>(index);
    }

    template<int LoadMode>
    inline void writePacket(Index index, const PacketScalar& val)
    {
      m_expression.const_cast_derived().template writePacket<LoadMode>(index, val);
    }

    template<typename Dest>
    inline void evalTo(Dest& dst) const { dst = m_expression; }

    const typename internal::remove_all<NestedExpressionType>::type& 
    nestedExpression() const 
    {
      return m_expression;
    }

    void resize(Index newSize) { m_expression.const_cast_derived().resize(newSize); }
    void resize(Index nbRows, Index nbCols) { m_expression.const_cast_derived().resize(nbRows,nbCols); }

  protected:
    NestedExpressionType m_expression;
};


namespace internal {
template<typename ExpressionType>
struct traits<MatrixWrapper<ExpressionType> >
 : public traits<typename remove_all<typename ExpressionType::Nested>::type >
{
  typedef MatrixXpr XprKind;
  
  enum {
    Flags0 = traits<typename remove_all<typename ExpressionType::Nested>::type >::Flags,
    Flags = Flags0 & ~NestByRefBit
  };
};
}

template<typename ExpressionType>
class MatrixWrapper : public MatrixBase<MatrixWrapper<ExpressionType> >
{
  public:
    typedef MatrixBase<MatrixWrapper<ExpressionType> > Base;
    EIGEN_DENSE_PUBLIC_INTERFACE(MatrixWrapper)
    EIGEN_INHERIT_ASSIGNMENT_OPERATORS(MatrixWrapper)

    typedef typename internal::conditional<
                       internal::is_lvalue<ExpressionType>::value,
                       Scalar,
                       const Scalar
                     >::type ScalarWithConstIfNotLvalue;

    typedef typename internal::nested<ExpressionType>::type NestedExpressionType;

    inline MatrixWrapper(ExpressionType& a_matrix) : m_expression(a_matrix) {}

    inline Index rows() const { return m_expression.rows(); }
    inline Index cols() const { return m_expression.cols(); }
    inline Index outerStride() const { return m_expression.outerStride(); }
    inline Index innerStride() const { return m_expression.innerStride(); }

    inline ScalarWithConstIfNotLvalue* data() { return m_expression.const_cast_derived().data(); }
    inline const Scalar* data() const { return m_expression.data(); }

    inline CoeffReturnType coeff(Index rowId, Index colId) const
    {
      return m_expression.coeff(rowId, colId);
    }

    inline Scalar& coeffRef(Index rowId, Index colId)
    {
      return m_expression.const_cast_derived().coeffRef(rowId, colId);
    }

    inline const Scalar& coeffRef(Index rowId, Index colId) const
    {
      return m_expression.derived().coeffRef(rowId, colId);
    }

    inline CoeffReturnType coeff(Index index) const
    {
      return m_expression.coeff(index);
    }

    inline Scalar& coeffRef(Index index)
    {
      return m_expression.const_cast_derived().coeffRef(index);
    }

    inline const Scalar& coeffRef(Index index) const
    {
      return m_expression.const_cast_derived().coeffRef(index);
    }

    template<int LoadMode>
    inline const PacketScalar packet(Index rowId, Index colId) const
    {
      return m_expression.template packet<LoadMode>(rowId, colId);
    }

    template<int LoadMode>
    inline void writePacket(Index rowId, Index colId, const PacketScalar& val)
    {
      m_expression.const_cast_derived().template writePacket<LoadMode>(rowId, colId, val);
    }

    template<int LoadMode>
    inline const PacketScalar packet(Index index) const
    {
      return m_expression.template packet<LoadMode>(index);
    }

    template<int LoadMode>
    inline void writePacket(Index index, const PacketScalar& val)
    {
      m_expression.const_cast_derived().template writePacket<LoadMode>(index, val);
    }

    const typename internal::remove_all<NestedExpressionType>::type& 
    nestedExpression() const 
    {
      return m_expression;
    }

    void resize(Index newSize) { m_expression.const_cast_derived().resize(newSize); }
    void resize(Index nbRows, Index nbCols) { m_expression.const_cast_derived().resize(nbRows,nbCols); }

  protected:
    NestedExpressionType m_expression;
};

} 

#endif 
