// Copyright (C) 2008-2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_ORTHOMETHODS_H
#define EIGEN_ORTHOMETHODS_H

namespace Eigen { 

template<typename Derived>
template<typename OtherDerived>
inline typename MatrixBase<Derived>::template cross_product_return_type<OtherDerived>::type
MatrixBase<Derived>::cross(const MatrixBase<OtherDerived>& other) const
{
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Derived,3)
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(OtherDerived,3)

  
  
  typename internal::nested<Derived,2>::type lhs(derived());
  typename internal::nested<OtherDerived,2>::type rhs(other.derived());
  return typename cross_product_return_type<OtherDerived>::type(
    numext::conj(lhs.coeff(1) * rhs.coeff(2) - lhs.coeff(2) * rhs.coeff(1)),
    numext::conj(lhs.coeff(2) * rhs.coeff(0) - lhs.coeff(0) * rhs.coeff(2)),
    numext::conj(lhs.coeff(0) * rhs.coeff(1) - lhs.coeff(1) * rhs.coeff(0))
  );
}

namespace internal {

template< int Arch,typename VectorLhs,typename VectorRhs,
          typename Scalar = typename VectorLhs::Scalar,
          bool Vectorizable = bool((VectorLhs::Flags&VectorRhs::Flags)&PacketAccessBit)>
struct cross3_impl {
  static inline typename internal::plain_matrix_type<VectorLhs>::type
  run(const VectorLhs& lhs, const VectorRhs& rhs)
  {
    return typename internal::plain_matrix_type<VectorLhs>::type(
      numext::conj(lhs.coeff(1) * rhs.coeff(2) - lhs.coeff(2) * rhs.coeff(1)),
      numext::conj(lhs.coeff(2) * rhs.coeff(0) - lhs.coeff(0) * rhs.coeff(2)),
      numext::conj(lhs.coeff(0) * rhs.coeff(1) - lhs.coeff(1) * rhs.coeff(0)),
      0
    );
  }
};

}

template<typename Derived>
template<typename OtherDerived>
inline typename MatrixBase<Derived>::PlainObject
MatrixBase<Derived>::cross3(const MatrixBase<OtherDerived>& other) const
{
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Derived,4)
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(OtherDerived,4)

  typedef typename internal::nested<Derived,2>::type DerivedNested;
  typedef typename internal::nested<OtherDerived,2>::type OtherDerivedNested;
  DerivedNested lhs(derived());
  OtherDerivedNested rhs(other.derived());

  return internal::cross3_impl<Architecture::Target,
                        typename internal::remove_all<DerivedNested>::type,
                        typename internal::remove_all<OtherDerivedNested>::type>::run(lhs,rhs);
}

template<typename ExpressionType, int Direction>
template<typename OtherDerived>
const typename VectorwiseOp<ExpressionType,Direction>::CrossReturnType
VectorwiseOp<ExpressionType,Direction>::cross(const MatrixBase<OtherDerived>& other) const
{
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(OtherDerived,3)
  EIGEN_STATIC_ASSERT((internal::is_same<Scalar, typename OtherDerived::Scalar>::value),
    YOU_MIXED_DIFFERENT_NUMERIC_TYPES__YOU_NEED_TO_USE_THE_CAST_METHOD_OF_MATRIXBASE_TO_CAST_NUMERIC_TYPES_EXPLICITLY)

  CrossReturnType res(_expression().rows(),_expression().cols());
  if(Direction==Vertical)
  {
    eigen_assert(CrossReturnType::RowsAtCompileTime==3 && "the matrix must have exactly 3 rows");
    res.row(0) = (_expression().row(1) * other.coeff(2) - _expression().row(2) * other.coeff(1)).conjugate();
    res.row(1) = (_expression().row(2) * other.coeff(0) - _expression().row(0) * other.coeff(2)).conjugate();
    res.row(2) = (_expression().row(0) * other.coeff(1) - _expression().row(1) * other.coeff(0)).conjugate();
  }
  else
  {
    eigen_assert(CrossReturnType::ColsAtCompileTime==3 && "the matrix must have exactly 3 columns");
    res.col(0) = (_expression().col(1) * other.coeff(2) - _expression().col(2) * other.coeff(1)).conjugate();
    res.col(1) = (_expression().col(2) * other.coeff(0) - _expression().col(0) * other.coeff(2)).conjugate();
    res.col(2) = (_expression().col(0) * other.coeff(1) - _expression().col(1) * other.coeff(0)).conjugate();
  }
  return res;
}

namespace internal {

template<typename Derived, int Size = Derived::SizeAtCompileTime>
struct unitOrthogonal_selector
{
  typedef typename plain_matrix_type<Derived>::type VectorType;
  typedef typename traits<Derived>::Scalar Scalar;
  typedef typename NumTraits<Scalar>::Real RealScalar;
  typedef typename Derived::Index Index;
  typedef Matrix<Scalar,2,1> Vector2;
  static inline VectorType run(const Derived& src)
  {
    VectorType perp = VectorType::Zero(src.size());
    Index maxi = 0;
    Index sndi = 0;
    src.cwiseAbs().maxCoeff(&maxi);
    if (maxi==0)
      sndi = 1;
    RealScalar invnm = RealScalar(1)/(Vector2() << src.coeff(sndi),src.coeff(maxi)).finished().norm();
    perp.coeffRef(maxi) = -numext::conj(src.coeff(sndi)) * invnm;
    perp.coeffRef(sndi) =  numext::conj(src.coeff(maxi)) * invnm;

    return perp;
   }
};

template<typename Derived>
struct unitOrthogonal_selector<Derived,3>
{
  typedef typename plain_matrix_type<Derived>::type VectorType;
  typedef typename traits<Derived>::Scalar Scalar;
  typedef typename NumTraits<Scalar>::Real RealScalar;
  static inline VectorType run(const Derived& src)
  {
    VectorType perp;

    if((!isMuchSmallerThan(src.x(), src.z()))
    || (!isMuchSmallerThan(src.y(), src.z())))
    {
      RealScalar invnm = RealScalar(1)/src.template head<2>().norm();
      perp.coeffRef(0) = -numext::conj(src.y())*invnm;
      perp.coeffRef(1) = numext::conj(src.x())*invnm;
      perp.coeffRef(2) = 0;
    }
    else
    {
      RealScalar invnm = RealScalar(1)/src.template tail<2>().norm();
      perp.coeffRef(0) = 0;
      perp.coeffRef(1) = -numext::conj(src.z())*invnm;
      perp.coeffRef(2) = numext::conj(src.y())*invnm;
    }

    return perp;
   }
};

template<typename Derived>
struct unitOrthogonal_selector<Derived,2>
{
  typedef typename plain_matrix_type<Derived>::type VectorType;
  static inline VectorType run(const Derived& src)
  { return VectorType(-numext::conj(src.y()), numext::conj(src.x())).normalized(); }
};

} 

template<typename Derived>
typename MatrixBase<Derived>::PlainObject
MatrixBase<Derived>::unitOrthogonal() const
{
  EIGEN_STATIC_ASSERT_VECTOR_ONLY(Derived)
  return internal::unitOrthogonal_selector<Derived>::run(derived());
}

} 

#endif 
