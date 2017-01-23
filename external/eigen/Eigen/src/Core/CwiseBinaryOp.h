// Copyright (C) 2008-2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_CWISE_BINARY_OP_H
#define EIGEN_CWISE_BINARY_OP_H

namespace Eigen {


namespace internal {
template<typename BinaryOp, typename Lhs, typename Rhs>
struct traits<CwiseBinaryOp<BinaryOp, Lhs, Rhs> >
{
  
  
  typedef typename remove_all<Lhs>::type Ancestor;
  typedef typename traits<Ancestor>::XprKind XprKind;
  enum {
    RowsAtCompileTime = traits<Ancestor>::RowsAtCompileTime,
    ColsAtCompileTime = traits<Ancestor>::ColsAtCompileTime,
    MaxRowsAtCompileTime = traits<Ancestor>::MaxRowsAtCompileTime,
    MaxColsAtCompileTime = traits<Ancestor>::MaxColsAtCompileTime
  };

  
  
  typedef typename result_of<
                     BinaryOp(
                       typename Lhs::Scalar,
                       typename Rhs::Scalar
                     )
                   >::type Scalar;
  typedef typename promote_storage_type<typename traits<Lhs>::StorageKind,
                                           typename traits<Rhs>::StorageKind>::ret StorageKind;
  typedef typename promote_index_type<typename traits<Lhs>::Index,
                                         typename traits<Rhs>::Index>::type Index;
  typedef typename Lhs::Nested LhsNested;
  typedef typename Rhs::Nested RhsNested;
  typedef typename remove_reference<LhsNested>::type _LhsNested;
  typedef typename remove_reference<RhsNested>::type _RhsNested;
  enum {
    LhsCoeffReadCost = _LhsNested::CoeffReadCost,
    RhsCoeffReadCost = _RhsNested::CoeffReadCost,
    LhsFlags = _LhsNested::Flags,
    RhsFlags = _RhsNested::Flags,
    SameType = is_same<typename _LhsNested::Scalar,typename _RhsNested::Scalar>::value,
    StorageOrdersAgree = (int(Lhs::Flags)&RowMajorBit)==(int(Rhs::Flags)&RowMajorBit),
    Flags0 = (int(LhsFlags) | int(RhsFlags)) & (
        HereditaryBits
      | (int(LhsFlags) & int(RhsFlags) &
           ( AlignedBit
           | (StorageOrdersAgree ? LinearAccessBit : 0)
           | (functor_traits<BinaryOp>::PacketAccess && StorageOrdersAgree && SameType ? PacketAccessBit : 0)
           )
        )
     ),
    Flags = (Flags0 & ~RowMajorBit) | (LhsFlags & RowMajorBit),
    CoeffReadCost = LhsCoeffReadCost + RhsCoeffReadCost + functor_traits<BinaryOp>::Cost
  };
};
} 

#define EIGEN_CHECK_BINARY_COMPATIBILIY(BINOP,LHS,RHS) \
  EIGEN_STATIC_ASSERT((internal::functor_is_product_like<BINOP>::ret \
                        ? int(internal::scalar_product_traits<LHS, RHS>::Defined) \
                        : int(internal::is_same<LHS, RHS>::value)), \
    YOU_MIXED_DIFFERENT_NUMERIC_TYPES__YOU_NEED_TO_USE_THE_CAST_METHOD_OF_MATRIXBASE_TO_CAST_NUMERIC_TYPES_EXPLICITLY)

template<typename BinaryOp, typename Lhs, typename Rhs, typename StorageKind>
class CwiseBinaryOpImpl;

template<typename BinaryOp, typename Lhs, typename Rhs>
class CwiseBinaryOp : internal::no_assignment_operator,
  public CwiseBinaryOpImpl<
          BinaryOp, Lhs, Rhs,
          typename internal::promote_storage_type<typename internal::traits<Lhs>::StorageKind,
                                           typename internal::traits<Rhs>::StorageKind>::ret>
{
  public:

    typedef typename CwiseBinaryOpImpl<
        BinaryOp, Lhs, Rhs,
        typename internal::promote_storage_type<typename internal::traits<Lhs>::StorageKind,
                                         typename internal::traits<Rhs>::StorageKind>::ret>::Base Base;
    EIGEN_GENERIC_PUBLIC_INTERFACE(CwiseBinaryOp)

    typedef typename internal::nested<Lhs>::type LhsNested;
    typedef typename internal::nested<Rhs>::type RhsNested;
    typedef typename internal::remove_reference<LhsNested>::type _LhsNested;
    typedef typename internal::remove_reference<RhsNested>::type _RhsNested;

    EIGEN_STRONG_INLINE CwiseBinaryOp(const Lhs& aLhs, const Rhs& aRhs, const BinaryOp& func = BinaryOp())
      : m_lhs(aLhs), m_rhs(aRhs), m_functor(func)
    {
      EIGEN_CHECK_BINARY_COMPATIBILIY(BinaryOp,typename Lhs::Scalar,typename Rhs::Scalar);
      
      EIGEN_STATIC_ASSERT_SAME_MATRIX_SIZE(Lhs, Rhs)
      eigen_assert(aLhs.rows() == aRhs.rows() && aLhs.cols() == aRhs.cols());
    }

    EIGEN_STRONG_INLINE Index rows() const {
      
      if (internal::traits<typename internal::remove_all<LhsNested>::type>::RowsAtCompileTime==Dynamic)
        return m_rhs.rows();
      else
        return m_lhs.rows();
    }
    EIGEN_STRONG_INLINE Index cols() const {
      
      if (internal::traits<typename internal::remove_all<LhsNested>::type>::ColsAtCompileTime==Dynamic)
        return m_rhs.cols();
      else
        return m_lhs.cols();
    }

    
    const _LhsNested& lhs() const { return m_lhs; }
    
    const _RhsNested& rhs() const { return m_rhs; }
    
    const BinaryOp& functor() const { return m_functor; }

  protected:
    LhsNested m_lhs;
    RhsNested m_rhs;
    const BinaryOp m_functor;
};

template<typename BinaryOp, typename Lhs, typename Rhs>
class CwiseBinaryOpImpl<BinaryOp, Lhs, Rhs, Dense>
  : public internal::dense_xpr_base<CwiseBinaryOp<BinaryOp, Lhs, Rhs> >::type
{
    typedef CwiseBinaryOp<BinaryOp, Lhs, Rhs> Derived;
  public:

    typedef typename internal::dense_xpr_base<CwiseBinaryOp<BinaryOp, Lhs, Rhs> >::type Base;
    EIGEN_DENSE_PUBLIC_INTERFACE( Derived )

    EIGEN_STRONG_INLINE const Scalar coeff(Index rowId, Index colId) const
    {
      return derived().functor()(derived().lhs().coeff(rowId, colId),
                                 derived().rhs().coeff(rowId, colId));
    }

    template<int LoadMode>
    EIGEN_STRONG_INLINE PacketScalar packet(Index rowId, Index colId) const
    {
      return derived().functor().packetOp(derived().lhs().template packet<LoadMode>(rowId, colId),
                                          derived().rhs().template packet<LoadMode>(rowId, colId));
    }

    EIGEN_STRONG_INLINE const Scalar coeff(Index index) const
    {
      return derived().functor()(derived().lhs().coeff(index),
                                 derived().rhs().coeff(index));
    }

    template<int LoadMode>
    EIGEN_STRONG_INLINE PacketScalar packet(Index index) const
    {
      return derived().functor().packetOp(derived().lhs().template packet<LoadMode>(index),
                                          derived().rhs().template packet<LoadMode>(index));
    }
};

template<typename Derived>
template<typename OtherDerived>
EIGEN_STRONG_INLINE Derived &
MatrixBase<Derived>::operator-=(const MatrixBase<OtherDerived> &other)
{
  SelfCwiseBinaryOp<internal::scalar_difference_op<Scalar>, Derived, OtherDerived> tmp(derived());
  tmp = other.derived();
  return derived();
}

template<typename Derived>
template<typename OtherDerived>
EIGEN_STRONG_INLINE Derived &
MatrixBase<Derived>::operator+=(const MatrixBase<OtherDerived>& other)
{
  SelfCwiseBinaryOp<internal::scalar_sum_op<Scalar>, Derived, OtherDerived> tmp(derived());
  tmp = other.derived();
  return derived();
}

} 

#endif 
