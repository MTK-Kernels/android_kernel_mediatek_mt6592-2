// Copyright (C) 2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_ARRAYBASE_H
#define EIGEN_ARRAYBASE_H

namespace Eigen { 

template<typename ExpressionType> class MatrixWrapper;

template<typename Derived> class ArrayBase
  : public DenseBase<Derived>
{
  public:
#ifndef EIGEN_PARSED_BY_DOXYGEN
    
    typedef ArrayBase StorageBaseType;

    typedef ArrayBase Eigen_BaseClassForSpecializationOfGlobalMathFuncImpl;

    using internal::special_scalar_op_base<Derived,typename internal::traits<Derived>::Scalar,
                typename NumTraits<typename internal::traits<Derived>::Scalar>::Real>::operator*;

    typedef typename internal::traits<Derived>::StorageKind StorageKind;
    typedef typename internal::traits<Derived>::Index Index;
    typedef typename internal::traits<Derived>::Scalar Scalar;
    typedef typename internal::packet_traits<Scalar>::type PacketScalar;
    typedef typename NumTraits<Scalar>::Real RealScalar;

    typedef DenseBase<Derived> Base;
    using Base::RowsAtCompileTime;
    using Base::ColsAtCompileTime;
    using Base::SizeAtCompileTime;
    using Base::MaxRowsAtCompileTime;
    using Base::MaxColsAtCompileTime;
    using Base::MaxSizeAtCompileTime;
    using Base::IsVectorAtCompileTime;
    using Base::Flags;
    using Base::CoeffReadCost;

    using Base::derived;
    using Base::const_cast_derived;
    using Base::rows;
    using Base::cols;
    using Base::size;
    using Base::coeff;
    using Base::coeffRef;
    using Base::lazyAssign;
    using Base::operator=;
    using Base::operator+=;
    using Base::operator-=;
    using Base::operator*=;
    using Base::operator/=;

    typedef typename Base::CoeffReturnType CoeffReturnType;

#endif 

#ifndef EIGEN_PARSED_BY_DOXYGEN
    typedef Array<typename internal::traits<Derived>::Scalar,
                internal::traits<Derived>::RowsAtCompileTime,
                internal::traits<Derived>::ColsAtCompileTime,
                AutoAlign | (internal::traits<Derived>::Flags&RowMajorBit ? RowMajor : ColMajor),
                internal::traits<Derived>::MaxRowsAtCompileTime,
                internal::traits<Derived>::MaxColsAtCompileTime
          > PlainObject;


    
    typedef CwiseNullaryOp<internal::scalar_constant_op<Scalar>,Derived> ConstantReturnType;
#endif 

#define EIGEN_CURRENT_STORAGE_BASE_CLASS Eigen::ArrayBase
#   include "../plugins/CommonCwiseUnaryOps.h"
#   include "../plugins/MatrixCwiseUnaryOps.h"
#   include "../plugins/ArrayCwiseUnaryOps.h"
#   include "../plugins/CommonCwiseBinaryOps.h"
#   include "../plugins/MatrixCwiseBinaryOps.h"
#   include "../plugins/ArrayCwiseBinaryOps.h"
#   ifdef EIGEN_ARRAYBASE_PLUGIN
#     include EIGEN_ARRAYBASE_PLUGIN
#   endif
#undef EIGEN_CURRENT_STORAGE_BASE_CLASS

    Derived& operator=(const ArrayBase& other)
    {
      return internal::assign_selector<Derived,Derived>::run(derived(), other.derived());
    }

    Derived& operator+=(const Scalar& scalar)
    { return *this = derived() + scalar; }
    Derived& operator-=(const Scalar& scalar)
    { return *this = derived() - scalar; }

    template<typename OtherDerived>
    Derived& operator+=(const ArrayBase<OtherDerived>& other);
    template<typename OtherDerived>
    Derived& operator-=(const ArrayBase<OtherDerived>& other);

    template<typename OtherDerived>
    Derived& operator*=(const ArrayBase<OtherDerived>& other);

    template<typename OtherDerived>
    Derived& operator/=(const ArrayBase<OtherDerived>& other);

  public:
    ArrayBase<Derived>& array() { return *this; }
    const ArrayBase<Derived>& array() const { return *this; }

    MatrixWrapper<Derived> matrix() { return derived(); }
    const MatrixWrapper<const Derived> matrix() const { return derived(); }


  protected:
    ArrayBase() : Base() {}

  private:
    explicit ArrayBase(Index);
    ArrayBase(Index,Index);
    template<typename OtherDerived> explicit ArrayBase(const ArrayBase<OtherDerived>&);
  protected:
    
    template<typename OtherDerived> Derived& operator+=(const MatrixBase<OtherDerived>& )
    {EIGEN_STATIC_ASSERT(std::ptrdiff_t(sizeof(typename OtherDerived::Scalar))==-1,YOU_CANNOT_MIX_ARRAYS_AND_MATRICES); return *this;}
    
    template<typename OtherDerived> Derived& operator-=(const MatrixBase<OtherDerived>& )
    {EIGEN_STATIC_ASSERT(std::ptrdiff_t(sizeof(typename OtherDerived::Scalar))==-1,YOU_CANNOT_MIX_ARRAYS_AND_MATRICES); return *this;}
};

template<typename Derived>
template<typename OtherDerived>
EIGEN_STRONG_INLINE Derived &
ArrayBase<Derived>::operator-=(const ArrayBase<OtherDerived> &other)
{
  SelfCwiseBinaryOp<internal::scalar_difference_op<Scalar>, Derived, OtherDerived> tmp(derived());
  tmp = other.derived();
  return derived();
}

template<typename Derived>
template<typename OtherDerived>
EIGEN_STRONG_INLINE Derived &
ArrayBase<Derived>::operator+=(const ArrayBase<OtherDerived>& other)
{
  SelfCwiseBinaryOp<internal::scalar_sum_op<Scalar>, Derived, OtherDerived> tmp(derived());
  tmp = other.derived();
  return derived();
}

template<typename Derived>
template<typename OtherDerived>
EIGEN_STRONG_INLINE Derived &
ArrayBase<Derived>::operator*=(const ArrayBase<OtherDerived>& other)
{
  SelfCwiseBinaryOp<internal::scalar_product_op<Scalar>, Derived, OtherDerived> tmp(derived());
  tmp = other.derived();
  return derived();
}

template<typename Derived>
template<typename OtherDerived>
EIGEN_STRONG_INLINE Derived &
ArrayBase<Derived>::operator/=(const ArrayBase<OtherDerived>& other)
{
  SelfCwiseBinaryOp<internal::scalar_quotient_op<Scalar>, Derived, OtherDerived> tmp(derived());
  tmp = other.derived();
  return derived();
}

} 

#endif 
