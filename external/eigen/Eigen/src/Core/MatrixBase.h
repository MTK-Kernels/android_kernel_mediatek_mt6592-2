// Copyright (C) 2006-2009 Benoit Jacob <jacob.benoit.1@gmail.com>
// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_MATRIXBASE_H
#define EIGEN_MATRIXBASE_H

namespace Eigen {

template<typename Derived> class MatrixBase
  : public DenseBase<Derived>
{
  public:
#ifndef EIGEN_PARSED_BY_DOXYGEN
    typedef MatrixBase StorageBaseType;
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
    using Base::eval;
    using Base::operator+=;
    using Base::operator-=;
    using Base::operator*=;
    using Base::operator/=;

    typedef typename Base::CoeffReturnType CoeffReturnType;
    typedef typename Base::ConstTransposeReturnType ConstTransposeReturnType;
    typedef typename Base::RowXpr RowXpr;
    typedef typename Base::ColXpr ColXpr;
#endif 



#ifndef EIGEN_PARSED_BY_DOXYGEN
    
    typedef Matrix<Scalar,EIGEN_SIZE_MAX(RowsAtCompileTime,ColsAtCompileTime),
                          EIGEN_SIZE_MAX(RowsAtCompileTime,ColsAtCompileTime)> SquareMatrixType;
#endif 

    inline Index diagonalSize() const { return (std::min)(rows(),cols()); }

    typedef Matrix<typename internal::traits<Derived>::Scalar,
                internal::traits<Derived>::RowsAtCompileTime,
                internal::traits<Derived>::ColsAtCompileTime,
                AutoAlign | (internal::traits<Derived>::Flags&RowMajorBit ? RowMajor : ColMajor),
                internal::traits<Derived>::MaxRowsAtCompileTime,
                internal::traits<Derived>::MaxColsAtCompileTime
          > PlainObject;

#ifndef EIGEN_PARSED_BY_DOXYGEN
    
    typedef CwiseNullaryOp<internal::scalar_constant_op<Scalar>,Derived> ConstantReturnType;
    
    typedef typename internal::conditional<NumTraits<Scalar>::IsComplex,
                        CwiseUnaryOp<internal::scalar_conjugate_op<Scalar>, ConstTransposeReturnType>,
                        ConstTransposeReturnType
                     >::type AdjointReturnType;
    
    typedef Matrix<std::complex<RealScalar>, internal::traits<Derived>::ColsAtCompileTime, 1, ColMajor> EigenvaluesReturnType;
    
    typedef CwiseNullaryOp<internal::scalar_identity_op<Scalar>,Derived> IdentityReturnType;
    
    typedef Block<const CwiseNullaryOp<internal::scalar_identity_op<Scalar>, SquareMatrixType>,
                  internal::traits<Derived>::RowsAtCompileTime,
                  internal::traits<Derived>::ColsAtCompileTime> BasisReturnType;
#endif 

#define EIGEN_CURRENT_STORAGE_BASE_CLASS Eigen::MatrixBase
#   include "../plugins/CommonCwiseUnaryOps.h"
#   include "../plugins/CommonCwiseBinaryOps.h"
#   include "../plugins/MatrixCwiseUnaryOps.h"
#   include "../plugins/MatrixCwiseBinaryOps.h"
#   ifdef EIGEN_MATRIXBASE_PLUGIN
#     include EIGEN_MATRIXBASE_PLUGIN
#   endif
#undef EIGEN_CURRENT_STORAGE_BASE_CLASS

    Derived& operator=(const MatrixBase& other);

    
    

    template <typename OtherDerived>
    Derived& operator=(const DenseBase<OtherDerived>& other);

    template <typename OtherDerived>
    Derived& operator=(const EigenBase<OtherDerived>& other);

    template<typename OtherDerived>
    Derived& operator=(const ReturnByValue<OtherDerived>& other);

#ifndef EIGEN_PARSED_BY_DOXYGEN
    template<typename ProductDerived, typename Lhs, typename Rhs>
    Derived& lazyAssign(const ProductBase<ProductDerived, Lhs,Rhs>& other);

    template<typename MatrixPower, typename Lhs, typename Rhs>
    Derived& lazyAssign(const MatrixPowerProduct<MatrixPower, Lhs,Rhs>& other);
#endif 

    template<typename OtherDerived>
    Derived& operator+=(const MatrixBase<OtherDerived>& other);
    template<typename OtherDerived>
    Derived& operator-=(const MatrixBase<OtherDerived>& other);

    template<typename OtherDerived>
    const typename ProductReturnType<Derived,OtherDerived>::Type
    operator*(const MatrixBase<OtherDerived> &other) const;

    template<typename OtherDerived>
    const typename LazyProductReturnType<Derived,OtherDerived>::Type
    lazyProduct(const MatrixBase<OtherDerived> &other) const;

    template<typename OtherDerived>
    Derived& operator*=(const EigenBase<OtherDerived>& other);

    template<typename OtherDerived>
    void applyOnTheLeft(const EigenBase<OtherDerived>& other);

    template<typename OtherDerived>
    void applyOnTheRight(const EigenBase<OtherDerived>& other);

    template<typename DiagonalDerived>
    const DiagonalProduct<Derived, DiagonalDerived, OnTheRight>
    operator*(const DiagonalBase<DiagonalDerived> &diagonal) const;

    template<typename OtherDerived>
    typename internal::scalar_product_traits<typename internal::traits<Derived>::Scalar,typename internal::traits<OtherDerived>::Scalar>::ReturnType
    dot(const MatrixBase<OtherDerived>& other) const;

    #ifdef EIGEN2_SUPPORT
      template<typename OtherDerived>
      Scalar eigen2_dot(const MatrixBase<OtherDerived>& other) const;
    #endif

    RealScalar squaredNorm() const;
    RealScalar norm() const;
    RealScalar stableNorm() const;
    RealScalar blueNorm() const;
    RealScalar hypotNorm() const;
    const PlainObject normalized() const;
    void normalize();

    const AdjointReturnType adjoint() const;
    void adjointInPlace();

    typedef Diagonal<Derived> DiagonalReturnType;
    DiagonalReturnType diagonal();
    typedef typename internal::add_const<Diagonal<const Derived> >::type ConstDiagonalReturnType;
    ConstDiagonalReturnType diagonal() const;

    template<int Index> struct DiagonalIndexReturnType { typedef Diagonal<Derived,Index> Type; };
    template<int Index> struct ConstDiagonalIndexReturnType { typedef const Diagonal<const Derived,Index> Type; };

    template<int Index> typename DiagonalIndexReturnType<Index>::Type diagonal();
    template<int Index> typename ConstDiagonalIndexReturnType<Index>::Type diagonal() const;
    
    typedef Diagonal<Derived,DynamicIndex> DiagonalDynamicIndexReturnType;
    typedef typename internal::add_const<Diagonal<const Derived,DynamicIndex> >::type ConstDiagonalDynamicIndexReturnType;

    DiagonalDynamicIndexReturnType diagonal(Index index);
    ConstDiagonalDynamicIndexReturnType diagonal(Index index) const;

    #ifdef EIGEN2_SUPPORT
    template<unsigned int Mode> typename internal::eigen2_part_return_type<Derived, Mode>::type part();
    template<unsigned int Mode> const typename internal::eigen2_part_return_type<Derived, Mode>::type part() const;
    
    
    
    template<template<typename T, int N> class U>
    const DiagonalWrapper<ConstDiagonalReturnType> part() const
    { return diagonal().asDiagonal(); }
    #endif 

    template<unsigned int Mode> struct TriangularViewReturnType { typedef TriangularView<Derived, Mode> Type; };
    template<unsigned int Mode> struct ConstTriangularViewReturnType { typedef const TriangularView<const Derived, Mode> Type; };

    template<unsigned int Mode> typename TriangularViewReturnType<Mode>::Type triangularView();
    template<unsigned int Mode> typename ConstTriangularViewReturnType<Mode>::Type triangularView() const;

    template<unsigned int UpLo> struct SelfAdjointViewReturnType { typedef SelfAdjointView<Derived, UpLo> Type; };
    template<unsigned int UpLo> struct ConstSelfAdjointViewReturnType { typedef const SelfAdjointView<const Derived, UpLo> Type; };

    template<unsigned int UpLo> typename SelfAdjointViewReturnType<UpLo>::Type selfadjointView();
    template<unsigned int UpLo> typename ConstSelfAdjointViewReturnType<UpLo>::Type selfadjointView() const;

    const SparseView<Derived> sparseView(const Scalar& m_reference = Scalar(0),
                                         const typename NumTraits<Scalar>::Real& m_epsilon = NumTraits<Scalar>::dummy_precision()) const;
    static const IdentityReturnType Identity();
    static const IdentityReturnType Identity(Index rows, Index cols);
    static const BasisReturnType Unit(Index size, Index i);
    static const BasisReturnType Unit(Index i);
    static const BasisReturnType UnitX();
    static const BasisReturnType UnitY();
    static const BasisReturnType UnitZ();
    static const BasisReturnType UnitW();

    const DiagonalWrapper<const Derived> asDiagonal() const;
    const PermutationWrapper<const Derived> asPermutation() const;

    Derived& setIdentity();
    Derived& setIdentity(Index rows, Index cols);

    bool isIdentity(const RealScalar& prec = NumTraits<Scalar>::dummy_precision()) const;
    bool isDiagonal(const RealScalar& prec = NumTraits<Scalar>::dummy_precision()) const;

    bool isUpperTriangular(const RealScalar& prec = NumTraits<Scalar>::dummy_precision()) const;
    bool isLowerTriangular(const RealScalar& prec = NumTraits<Scalar>::dummy_precision()) const;

    template<typename OtherDerived>
    bool isOrthogonal(const MatrixBase<OtherDerived>& other,
                      const RealScalar& prec = NumTraits<Scalar>::dummy_precision()) const;
    bool isUnitary(const RealScalar& prec = NumTraits<Scalar>::dummy_precision()) const;

    template<typename OtherDerived>
    inline bool operator==(const MatrixBase<OtherDerived>& other) const
    { return cwiseEqual(other).all(); }

    template<typename OtherDerived>
    inline bool operator!=(const MatrixBase<OtherDerived>& other) const
    { return cwiseNotEqual(other).any(); }

    NoAlias<Derived,Eigen::MatrixBase > noalias();

    inline const ForceAlignedAccess<Derived> forceAlignedAccess() const;
    inline ForceAlignedAccess<Derived> forceAlignedAccess();
    template<bool Enable> inline typename internal::add_const_on_value_type<typename internal::conditional<Enable,ForceAlignedAccess<Derived>,Derived&>::type>::type forceAlignedAccessIf() const;
    template<bool Enable> inline typename internal::conditional<Enable,ForceAlignedAccess<Derived>,Derived&>::type forceAlignedAccessIf();

    Scalar trace() const;


    template<int p> RealScalar lpNorm() const;

    MatrixBase<Derived>& matrix() { return *this; }
    const MatrixBase<Derived>& matrix() const { return *this; }

    ArrayWrapper<Derived> array() { return derived(); }
    const ArrayWrapper<const Derived> array() const { return derived(); }


    const FullPivLU<PlainObject> fullPivLu() const;
    const PartialPivLU<PlainObject> partialPivLu() const;

    #if EIGEN2_SUPPORT_STAGE < STAGE20_RESOLVE_API_CONFLICTS
    const LU<PlainObject> lu() const;
    #endif

    #ifdef EIGEN2_SUPPORT
    const LU<PlainObject> eigen2_lu() const;
    #endif

    #if EIGEN2_SUPPORT_STAGE > STAGE20_RESOLVE_API_CONFLICTS
    const PartialPivLU<PlainObject> lu() const;
    #endif
    
    #ifdef EIGEN2_SUPPORT
    template<typename ResultType>
    void computeInverse(MatrixBase<ResultType> *result) const {
      *result = this->inverse();
    }
    #endif

    const internal::inverse_impl<Derived> inverse() const;
    template<typename ResultType>
    void computeInverseAndDetWithCheck(
      ResultType& inverse,
      typename ResultType::Scalar& determinant,
      bool& invertible,
      const RealScalar& absDeterminantThreshold = NumTraits<Scalar>::dummy_precision()
    ) const;
    template<typename ResultType>
    void computeInverseWithCheck(
      ResultType& inverse,
      bool& invertible,
      const RealScalar& absDeterminantThreshold = NumTraits<Scalar>::dummy_precision()
    ) const;
    Scalar determinant() const;


    const LLT<PlainObject>  llt() const;
    const LDLT<PlainObject> ldlt() const;


    const HouseholderQR<PlainObject> householderQr() const;
    const ColPivHouseholderQR<PlainObject> colPivHouseholderQr() const;
    const FullPivHouseholderQR<PlainObject> fullPivHouseholderQr() const;
    
    #ifdef EIGEN2_SUPPORT
    const QR<PlainObject> qr() const;
    #endif

    EigenvaluesReturnType eigenvalues() const;
    RealScalar operatorNorm() const;


    JacobiSVD<PlainObject> jacobiSvd(unsigned int computationOptions = 0) const;

    #ifdef EIGEN2_SUPPORT
    SVD<PlainObject> svd() const;
    #endif


    #ifndef EIGEN_PARSED_BY_DOXYGEN
    
    template<typename OtherDerived> struct cross_product_return_type {
      typedef typename internal::scalar_product_traits<typename internal::traits<Derived>::Scalar,typename internal::traits<OtherDerived>::Scalar>::ReturnType Scalar;
      typedef Matrix<Scalar,MatrixBase::RowsAtCompileTime,MatrixBase::ColsAtCompileTime> type;
    };
    #endif 
    template<typename OtherDerived>
    typename cross_product_return_type<OtherDerived>::type
    cross(const MatrixBase<OtherDerived>& other) const;
    template<typename OtherDerived>
    PlainObject cross3(const MatrixBase<OtherDerived>& other) const;
    PlainObject unitOrthogonal(void) const;
    Matrix<Scalar,3,1> eulerAngles(Index a0, Index a1, Index a2) const;
    
    #if EIGEN2_SUPPORT_STAGE > STAGE20_RESOLVE_API_CONFLICTS
    ScalarMultipleReturnType operator*(const UniformScaling<Scalar>& s) const;
    
    enum { HomogeneousReturnTypeDirection = ColsAtCompileTime==1?Vertical:Horizontal };
    typedef Homogeneous<Derived, HomogeneousReturnTypeDirection> HomogeneousReturnType;
    HomogeneousReturnType homogeneous() const;
    #endif
    
    enum {
      SizeMinusOne = SizeAtCompileTime==Dynamic ? Dynamic : SizeAtCompileTime-1
    };
    typedef Block<const Derived,
                  internal::traits<Derived>::ColsAtCompileTime==1 ? SizeMinusOne : 1,
                  internal::traits<Derived>::ColsAtCompileTime==1 ? 1 : SizeMinusOne> ConstStartMinusOne;
    typedef CwiseUnaryOp<internal::scalar_quotient1_op<typename internal::traits<Derived>::Scalar>,
                const ConstStartMinusOne > HNormalizedReturnType;

    const HNormalizedReturnType hnormalized() const;


    void makeHouseholderInPlace(Scalar& tau, RealScalar& beta);
    template<typename EssentialPart>
    void makeHouseholder(EssentialPart& essential,
                         Scalar& tau, RealScalar& beta) const;
    template<typename EssentialPart>
    void applyHouseholderOnTheLeft(const EssentialPart& essential,
                                   const Scalar& tau,
                                   Scalar* workspace);
    template<typename EssentialPart>
    void applyHouseholderOnTheRight(const EssentialPart& essential,
                                    const Scalar& tau,
                                    Scalar* workspace);


    template<typename OtherScalar>
    void applyOnTheLeft(Index p, Index q, const JacobiRotation<OtherScalar>& j);
    template<typename OtherScalar>
    void applyOnTheRight(Index p, Index q, const JacobiRotation<OtherScalar>& j);


    typedef typename internal::stem_function<Scalar>::type StemFunction;
    const MatrixExponentialReturnValue<Derived> exp() const;
    const MatrixFunctionReturnValue<Derived> matrixFunction(StemFunction f) const;
    const MatrixFunctionReturnValue<Derived> cosh() const;
    const MatrixFunctionReturnValue<Derived> sinh() const;
    const MatrixFunctionReturnValue<Derived> cos() const;
    const MatrixFunctionReturnValue<Derived> sin() const;
    const MatrixSquareRootReturnValue<Derived> sqrt() const;
    const MatrixLogarithmReturnValue<Derived> log() const;
    const MatrixPowerReturnValue<Derived> pow(const RealScalar& p) const;

#ifdef EIGEN2_SUPPORT
    template<typename ProductDerived, typename Lhs, typename Rhs>
    Derived& operator+=(const Flagged<ProductBase<ProductDerived, Lhs,Rhs>, 0,
                                      EvalBeforeAssigningBit>& other);

    template<typename ProductDerived, typename Lhs, typename Rhs>
    Derived& operator-=(const Flagged<ProductBase<ProductDerived, Lhs,Rhs>, 0,
                                      EvalBeforeAssigningBit>& other);

    template<typename OtherDerived>
    Derived& lazyAssign(const Flagged<OtherDerived, 0, EvalBeforeAssigningBit>& other)
    { return lazyAssign(other._expression()); }

    template<unsigned int Added>
    const Flagged<Derived, Added, 0> marked() const;
    const Flagged<Derived, 0, EvalBeforeAssigningBit> lazy() const;

    inline const Cwise<Derived> cwise() const;
    inline Cwise<Derived> cwise();

    VectorBlock<Derived> start(Index size);
    const VectorBlock<const Derived> start(Index size) const;
    VectorBlock<Derived> end(Index size);
    const VectorBlock<const Derived> end(Index size) const;
    template<int Size> VectorBlock<Derived,Size> start();
    template<int Size> const VectorBlock<const Derived,Size> start() const;
    template<int Size> VectorBlock<Derived,Size> end();
    template<int Size> const VectorBlock<const Derived,Size> end() const;

    Minor<Derived> minor(Index row, Index col);
    const Minor<Derived> minor(Index row, Index col) const;
#endif

  protected:
    MatrixBase() : Base() {}

  private:
    explicit MatrixBase(int);
    MatrixBase(int,int);
    template<typename OtherDerived> explicit MatrixBase(const MatrixBase<OtherDerived>&);
  protected:
    
    template<typename OtherDerived> Derived& operator+=(const ArrayBase<OtherDerived>& )
    {EIGEN_STATIC_ASSERT(std::ptrdiff_t(sizeof(typename OtherDerived::Scalar))==-1,YOU_CANNOT_MIX_ARRAYS_AND_MATRICES); return *this;}
    
    template<typename OtherDerived> Derived& operator-=(const ArrayBase<OtherDerived>& )
    {EIGEN_STATIC_ASSERT(std::ptrdiff_t(sizeof(typename OtherDerived::Scalar))==-1,YOU_CANNOT_MIX_ARRAYS_AND_MATRICES); return *this;}
};



template<typename Derived>
template<typename OtherDerived>
inline Derived&
MatrixBase<Derived>::operator*=(const EigenBase<OtherDerived> &other)
{
  other.derived().applyThisOnTheRight(derived());
  return derived();
}

template<typename Derived>
template<typename OtherDerived>
inline void MatrixBase<Derived>::applyOnTheRight(const EigenBase<OtherDerived> &other)
{
  other.derived().applyThisOnTheRight(derived());
}

template<typename Derived>
template<typename OtherDerived>
inline void MatrixBase<Derived>::applyOnTheLeft(const EigenBase<OtherDerived> &other)
{
  other.derived().applyThisOnTheLeft(derived());
}

} 

#endif 
