// Copyright (C) 2007-2010 Benoit Jacob <jacob.benoit.1@gmail.com>
// Copyright (C) 2008-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_DENSEBASE_H
#define EIGEN_DENSEBASE_H

namespace Eigen {

namespace internal {
  
static inline void check_DenseIndex_is_signed() {
  EIGEN_STATIC_ASSERT(NumTraits<DenseIndex>::IsSigned,THE_INDEX_TYPE_MUST_BE_A_SIGNED_TYPE); 
}

} 
  
template<typename Derived> class DenseBase
#ifndef EIGEN_PARSED_BY_DOXYGEN
  : public internal::special_scalar_op_base<Derived,typename internal::traits<Derived>::Scalar,
                                     typename NumTraits<typename internal::traits<Derived>::Scalar>::Real>
#else
  : public DenseCoeffsBase<Derived>
#endif 
{
  public:
    using internal::special_scalar_op_base<Derived,typename internal::traits<Derived>::Scalar,
                typename NumTraits<typename internal::traits<Derived>::Scalar>::Real>::operator*;

    class InnerIterator;

    typedef typename internal::traits<Derived>::StorageKind StorageKind;

    typedef typename internal::traits<Derived>::Index Index; 

    typedef typename internal::traits<Derived>::Scalar Scalar;
    typedef typename internal::packet_traits<Scalar>::type PacketScalar;
    typedef typename NumTraits<Scalar>::Real RealScalar;

    typedef DenseCoeffsBase<Derived> Base;
    using Base::derived;
    using Base::const_cast_derived;
    using Base::rows;
    using Base::cols;
    using Base::size;
    using Base::rowIndexByOuterInner;
    using Base::colIndexByOuterInner;
    using Base::coeff;
    using Base::coeffByOuterInner;
    using Base::packet;
    using Base::packetByOuterInner;
    using Base::writePacket;
    using Base::writePacketByOuterInner;
    using Base::coeffRef;
    using Base::coeffRefByOuterInner;
    using Base::copyCoeff;
    using Base::copyCoeffByOuterInner;
    using Base::copyPacket;
    using Base::copyPacketByOuterInner;
    using Base::operator();
    using Base::operator[];
    using Base::x;
    using Base::y;
    using Base::z;
    using Base::w;
    using Base::stride;
    using Base::innerStride;
    using Base::outerStride;
    using Base::rowStride;
    using Base::colStride;
    typedef typename Base::CoeffReturnType CoeffReturnType;

    enum {

      RowsAtCompileTime = internal::traits<Derived>::RowsAtCompileTime,

      ColsAtCompileTime = internal::traits<Derived>::ColsAtCompileTime,


      SizeAtCompileTime = (internal::size_at_compile_time<internal::traits<Derived>::RowsAtCompileTime,
                                                   internal::traits<Derived>::ColsAtCompileTime>::ret),

      MaxRowsAtCompileTime = internal::traits<Derived>::MaxRowsAtCompileTime,

      MaxColsAtCompileTime = internal::traits<Derived>::MaxColsAtCompileTime,

      MaxSizeAtCompileTime = (internal::size_at_compile_time<internal::traits<Derived>::MaxRowsAtCompileTime,
                                                      internal::traits<Derived>::MaxColsAtCompileTime>::ret),

      IsVectorAtCompileTime = internal::traits<Derived>::MaxRowsAtCompileTime == 1
                           || internal::traits<Derived>::MaxColsAtCompileTime == 1,

      Flags = internal::traits<Derived>::Flags,

      IsRowMajor = int(Flags) & RowMajorBit, 

      InnerSizeAtCompileTime = int(IsVectorAtCompileTime) ? int(SizeAtCompileTime)
                             : int(IsRowMajor) ? int(ColsAtCompileTime) : int(RowsAtCompileTime),

      CoeffReadCost = internal::traits<Derived>::CoeffReadCost,

      InnerStrideAtCompileTime = internal::inner_stride_at_compile_time<Derived>::ret,
      OuterStrideAtCompileTime = internal::outer_stride_at_compile_time<Derived>::ret
    };

    enum { ThisConstantIsPrivateInPlainObjectBase };

    inline Index nonZeros() const { return size(); }

    Index outerSize() const
    {
      return IsVectorAtCompileTime ? 1
           : int(IsRowMajor) ? this->rows() : this->cols();
    }

    Index innerSize() const
    {
      return IsVectorAtCompileTime ? this->size()
           : int(IsRowMajor) ? this->cols() : this->rows();
    }

    void resize(Index newSize)
    {
      EIGEN_ONLY_USED_FOR_DEBUG(newSize);
      eigen_assert(newSize == this->size()
                && "DenseBase::resize() does not actually allow to resize.");
    }
    void resize(Index nbRows, Index nbCols)
    {
      EIGEN_ONLY_USED_FOR_DEBUG(nbRows);
      EIGEN_ONLY_USED_FOR_DEBUG(nbCols);
      eigen_assert(nbRows == this->rows() && nbCols == this->cols()
                && "DenseBase::resize() does not actually allow to resize.");
    }

#ifndef EIGEN_PARSED_BY_DOXYGEN

    
    typedef CwiseNullaryOp<internal::scalar_constant_op<Scalar>,Derived> ConstantReturnType;
    
    typedef CwiseNullaryOp<internal::linspaced_op<Scalar,false>,Derived> SequentialLinSpacedReturnType;
    
    typedef CwiseNullaryOp<internal::linspaced_op<Scalar,true>,Derived> RandomAccessLinSpacedReturnType;
    
    typedef Matrix<typename NumTraits<typename internal::traits<Derived>::Scalar>::Real, internal::traits<Derived>::ColsAtCompileTime, 1> EigenvaluesReturnType;

#endif 

    
    template<typename OtherDerived>
    Derived& operator=(const DenseBase<OtherDerived>& other);

    Derived& operator=(const DenseBase& other);

    template<typename OtherDerived>
    Derived& operator=(const EigenBase<OtherDerived> &other);

    template<typename OtherDerived>
    Derived& operator+=(const EigenBase<OtherDerived> &other);

    template<typename OtherDerived>
    Derived& operator-=(const EigenBase<OtherDerived> &other);

    template<typename OtherDerived>
    Derived& operator=(const ReturnByValue<OtherDerived>& func);

#ifndef EIGEN_PARSED_BY_DOXYGEN
    
    template<typename OtherDerived>
    Derived& lazyAssign(const DenseBase<OtherDerived>& other);
#endif 

    CommaInitializer<Derived> operator<< (const Scalar& s);

    template<unsigned int Added,unsigned int Removed>
    const Flagged<Derived, Added, Removed> flagged() const;

    template<typename OtherDerived>
    CommaInitializer<Derived> operator<< (const DenseBase<OtherDerived>& other);

    Eigen::Transpose<Derived> transpose();
	typedef typename internal::add_const<Transpose<const Derived> >::type ConstTransposeReturnType;
    ConstTransposeReturnType transpose() const;
    void transposeInPlace();
#ifndef EIGEN_NO_DEBUG
  protected:
    template<typename OtherDerived>
    void checkTransposeAliasing(const OtherDerived& other) const;
  public:
#endif


    static const ConstantReturnType
    Constant(Index rows, Index cols, const Scalar& value);
    static const ConstantReturnType
    Constant(Index size, const Scalar& value);
    static const ConstantReturnType
    Constant(const Scalar& value);

    static const SequentialLinSpacedReturnType
    LinSpaced(Sequential_t, Index size, const Scalar& low, const Scalar& high);
    static const RandomAccessLinSpacedReturnType
    LinSpaced(Index size, const Scalar& low, const Scalar& high);
    static const SequentialLinSpacedReturnType
    LinSpaced(Sequential_t, const Scalar& low, const Scalar& high);
    static const RandomAccessLinSpacedReturnType
    LinSpaced(const Scalar& low, const Scalar& high);

    template<typename CustomNullaryOp>
    static const CwiseNullaryOp<CustomNullaryOp, Derived>
    NullaryExpr(Index rows, Index cols, const CustomNullaryOp& func);
    template<typename CustomNullaryOp>
    static const CwiseNullaryOp<CustomNullaryOp, Derived>
    NullaryExpr(Index size, const CustomNullaryOp& func);
    template<typename CustomNullaryOp>
    static const CwiseNullaryOp<CustomNullaryOp, Derived>
    NullaryExpr(const CustomNullaryOp& func);

    static const ConstantReturnType Zero(Index rows, Index cols);
    static const ConstantReturnType Zero(Index size);
    static const ConstantReturnType Zero();
    static const ConstantReturnType Ones(Index rows, Index cols);
    static const ConstantReturnType Ones(Index size);
    static const ConstantReturnType Ones();

    void fill(const Scalar& value);
    Derived& setConstant(const Scalar& value);
    Derived& setLinSpaced(Index size, const Scalar& low, const Scalar& high);
    Derived& setLinSpaced(const Scalar& low, const Scalar& high);
    Derived& setZero();
    Derived& setOnes();
    Derived& setRandom();

    template<typename OtherDerived>
    bool isApprox(const DenseBase<OtherDerived>& other,
                  const RealScalar& prec = NumTraits<Scalar>::dummy_precision()) const;
    bool isMuchSmallerThan(const RealScalar& other,
                           const RealScalar& prec = NumTraits<Scalar>::dummy_precision()) const;
    template<typename OtherDerived>
    bool isMuchSmallerThan(const DenseBase<OtherDerived>& other,
                           const RealScalar& prec = NumTraits<Scalar>::dummy_precision()) const;

    bool isApproxToConstant(const Scalar& value, const RealScalar& prec = NumTraits<Scalar>::dummy_precision()) const;
    bool isConstant(const Scalar& value, const RealScalar& prec = NumTraits<Scalar>::dummy_precision()) const;
    bool isZero(const RealScalar& prec = NumTraits<Scalar>::dummy_precision()) const;
    bool isOnes(const RealScalar& prec = NumTraits<Scalar>::dummy_precision()) const;
    
    inline bool hasNaN() const;
    inline bool allFinite() const;

    inline Derived& operator*=(const Scalar& other);
    inline Derived& operator/=(const Scalar& other);

    typedef typename internal::add_const_on_value_type<typename internal::eval<Derived>::type>::type EvalReturnType;
    EIGEN_STRONG_INLINE EvalReturnType eval() const
    {
      
      
      
      return typename internal::eval<Derived>::type(derived());
    }

    template<typename OtherDerived>
    void swap(const DenseBase<OtherDerived>& other,
              int = OtherDerived::ThisConstantIsPrivateInPlainObjectBase)
    {
      SwapWrapper<Derived>(derived()).lazyAssign(other.derived());
    }

    template<typename OtherDerived>
    void swap(PlainObjectBase<OtherDerived>& other)
    {
      SwapWrapper<Derived>(derived()).lazyAssign(other.derived());
    }


    inline const NestByValue<Derived> nestByValue() const;
    inline const ForceAlignedAccess<Derived> forceAlignedAccess() const;
    inline ForceAlignedAccess<Derived> forceAlignedAccess();
    template<bool Enable> inline const typename internal::conditional<Enable,ForceAlignedAccess<Derived>,Derived&>::type forceAlignedAccessIf() const;
    template<bool Enable> inline typename internal::conditional<Enable,ForceAlignedAccess<Derived>,Derived&>::type forceAlignedAccessIf();

    Scalar sum() const;
    Scalar mean() const;
    Scalar trace() const;

    Scalar prod() const;

    typename internal::traits<Derived>::Scalar minCoeff() const;
    typename internal::traits<Derived>::Scalar maxCoeff() const;

    template<typename IndexType>
    typename internal::traits<Derived>::Scalar minCoeff(IndexType* row, IndexType* col) const;
    template<typename IndexType>
    typename internal::traits<Derived>::Scalar maxCoeff(IndexType* row, IndexType* col) const;
    template<typename IndexType>
    typename internal::traits<Derived>::Scalar minCoeff(IndexType* index) const;
    template<typename IndexType>
    typename internal::traits<Derived>::Scalar maxCoeff(IndexType* index) const;

    template<typename BinaryOp>
    typename internal::result_of<BinaryOp(typename internal::traits<Derived>::Scalar)>::type
    redux(const BinaryOp& func) const;

    template<typename Visitor>
    void visit(Visitor& func) const;

    inline const WithFormat<Derived> format(const IOFormat& fmt) const;

    
    CoeffReturnType value() const
    {
      EIGEN_STATIC_ASSERT_SIZE_1x1(Derived)
      eigen_assert(this->rows() == 1 && this->cols() == 1);
      return derived().coeff(0,0);
    }

    bool all(void) const;
    bool any(void) const;
    Index count() const;

    typedef VectorwiseOp<Derived, Horizontal> RowwiseReturnType;
    typedef const VectorwiseOp<const Derived, Horizontal> ConstRowwiseReturnType;
    typedef VectorwiseOp<Derived, Vertical> ColwiseReturnType;
    typedef const VectorwiseOp<const Derived, Vertical> ConstColwiseReturnType;

    ConstRowwiseReturnType rowwise() const;
    RowwiseReturnType rowwise();
    ConstColwiseReturnType colwise() const;
    ColwiseReturnType colwise();

    static const CwiseNullaryOp<internal::scalar_random_op<Scalar>,Derived> Random(Index rows, Index cols);
    static const CwiseNullaryOp<internal::scalar_random_op<Scalar>,Derived> Random(Index size);
    static const CwiseNullaryOp<internal::scalar_random_op<Scalar>,Derived> Random();

    template<typename ThenDerived,typename ElseDerived>
    const Select<Derived,ThenDerived,ElseDerived>
    select(const DenseBase<ThenDerived>& thenMatrix,
           const DenseBase<ElseDerived>& elseMatrix) const;

    template<typename ThenDerived>
    inline const Select<Derived,ThenDerived, typename ThenDerived::ConstantReturnType>
    select(const DenseBase<ThenDerived>& thenMatrix, const typename ThenDerived::Scalar& elseScalar) const;

    template<typename ElseDerived>
    inline const Select<Derived, typename ElseDerived::ConstantReturnType, ElseDerived >
    select(const typename ElseDerived::Scalar& thenScalar, const DenseBase<ElseDerived>& elseMatrix) const;

    template<int p> RealScalar lpNorm() const;

    template<int RowFactor, int ColFactor>
    inline const Replicate<Derived,RowFactor,ColFactor> replicate() const;
    
    typedef Replicate<Derived,Dynamic,Dynamic> ReplicateReturnType;
    inline const ReplicateReturnType replicate(Index rowFacor,Index colFactor) const;

    typedef Reverse<Derived, BothDirections> ReverseReturnType;
    typedef const Reverse<const Derived, BothDirections> ConstReverseReturnType;
    ReverseReturnType reverse();
    ConstReverseReturnType reverse() const;
    void reverseInPlace();

#define EIGEN_CURRENT_STORAGE_BASE_CLASS Eigen::DenseBase
#   include "../plugins/BlockMethods.h"
#   ifdef EIGEN_DENSEBASE_PLUGIN
#     include EIGEN_DENSEBASE_PLUGIN
#   endif
#undef EIGEN_CURRENT_STORAGE_BASE_CLASS

#ifdef EIGEN2_SUPPORT

    Block<Derived> corner(CornerType type, Index cRows, Index cCols);
    const Block<Derived> corner(CornerType type, Index cRows, Index cCols) const;
    template<int CRows, int CCols>
    Block<Derived, CRows, CCols> corner(CornerType type);
    template<int CRows, int CCols>
    const Block<Derived, CRows, CCols> corner(CornerType type) const;

#endif 


    
    template<typename Dest> inline void evalTo(Dest& ) const
    {
      EIGEN_STATIC_ASSERT((internal::is_same<Dest,void>::value),THE_EVAL_EVALTO_FUNCTION_SHOULD_NEVER_BE_CALLED_FOR_DENSE_OBJECTS);
    }

  protected:
    
    DenseBase()
    {
#ifdef EIGEN_INTERNAL_DEBUGGING
      EIGEN_STATIC_ASSERT((EIGEN_IMPLIES(MaxRowsAtCompileTime==1 && MaxColsAtCompileTime!=1, int(IsRowMajor))
                        && EIGEN_IMPLIES(MaxColsAtCompileTime==1 && MaxRowsAtCompileTime!=1, int(!IsRowMajor))),
                          INVALID_STORAGE_ORDER_FOR_THIS_VECTOR_EXPRESSION)
#endif
    }

  private:
    explicit DenseBase(int);
    DenseBase(int,int);
    template<typename OtherDerived> explicit DenseBase(const DenseBase<OtherDerived>&);
};

} 

#endif 
