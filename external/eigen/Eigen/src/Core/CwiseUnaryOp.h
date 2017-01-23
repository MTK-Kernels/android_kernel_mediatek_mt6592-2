// Copyright (C) 2008-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_CWISE_UNARY_OP_H
#define EIGEN_CWISE_UNARY_OP_H

namespace Eigen { 


namespace internal {
template<typename UnaryOp, typename XprType>
struct traits<CwiseUnaryOp<UnaryOp, XprType> >
 : traits<XprType>
{
  typedef typename result_of<
                     UnaryOp(typename XprType::Scalar)
                   >::type Scalar;
  typedef typename XprType::Nested XprTypeNested;
  typedef typename remove_reference<XprTypeNested>::type _XprTypeNested;
  enum {
    Flags = _XprTypeNested::Flags & (
      HereditaryBits | LinearAccessBit | AlignedBit
      | (functor_traits<UnaryOp>::PacketAccess ? PacketAccessBit : 0)),
    CoeffReadCost = _XprTypeNested::CoeffReadCost + functor_traits<UnaryOp>::Cost
  };
};
}

template<typename UnaryOp, typename XprType, typename StorageKind>
class CwiseUnaryOpImpl;

template<typename UnaryOp, typename XprType>
class CwiseUnaryOp : internal::no_assignment_operator,
  public CwiseUnaryOpImpl<UnaryOp, XprType, typename internal::traits<XprType>::StorageKind>
{
  public:

    typedef typename CwiseUnaryOpImpl<UnaryOp, XprType,typename internal::traits<XprType>::StorageKind>::Base Base;
    EIGEN_GENERIC_PUBLIC_INTERFACE(CwiseUnaryOp)

    inline CwiseUnaryOp(const XprType& xpr, const UnaryOp& func = UnaryOp())
      : m_xpr(xpr), m_functor(func) {}

    EIGEN_STRONG_INLINE Index rows() const { return m_xpr.rows(); }
    EIGEN_STRONG_INLINE Index cols() const { return m_xpr.cols(); }

    
    const UnaryOp& functor() const { return m_functor; }

    
    const typename internal::remove_all<typename XprType::Nested>::type&
    nestedExpression() const { return m_xpr; }

    
    typename internal::remove_all<typename XprType::Nested>::type&
    nestedExpression() { return m_xpr.const_cast_derived(); }

  protected:
    typename XprType::Nested m_xpr;
    const UnaryOp m_functor;
};

template<typename UnaryOp, typename XprType>
class CwiseUnaryOpImpl<UnaryOp,XprType,Dense>
  : public internal::dense_xpr_base<CwiseUnaryOp<UnaryOp, XprType> >::type
{
  public:

    typedef CwiseUnaryOp<UnaryOp, XprType> Derived;
    typedef typename internal::dense_xpr_base<CwiseUnaryOp<UnaryOp, XprType> >::type Base;
    EIGEN_DENSE_PUBLIC_INTERFACE(Derived)

    EIGEN_STRONG_INLINE const Scalar coeff(Index rowId, Index colId) const
    {
      return derived().functor()(derived().nestedExpression().coeff(rowId, colId));
    }

    template<int LoadMode>
    EIGEN_STRONG_INLINE PacketScalar packet(Index rowId, Index colId) const
    {
      return derived().functor().packetOp(derived().nestedExpression().template packet<LoadMode>(rowId, colId));
    }

    EIGEN_STRONG_INLINE const Scalar coeff(Index index) const
    {
      return derived().functor()(derived().nestedExpression().coeff(index));
    }

    template<int LoadMode>
    EIGEN_STRONG_INLINE PacketScalar packet(Index index) const
    {
      return derived().functor().packetOp(derived().nestedExpression().template packet<LoadMode>(index));
    }
};

} 

#endif 
