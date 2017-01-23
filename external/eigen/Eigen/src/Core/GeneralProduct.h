// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
// Copyright (C) 2008-2011 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_GENERAL_PRODUCT_H
#define EIGEN_GENERAL_PRODUCT_H

namespace Eigen { 

template<typename Lhs, typename Rhs, int ProductType = internal::product_type<Lhs,Rhs>::value>
class GeneralProduct;

enum {
  Large = 2,
  Small = 3
};

namespace internal {

template<int Rows, int Cols, int Depth> struct product_type_selector;

template<int Size, int MaxSize> struct product_size_category
{
  enum { is_large = MaxSize == Dynamic ||
                    Size >= EIGEN_CACHEFRIENDLY_PRODUCT_THRESHOLD,
         value = is_large  ? Large
               : Size == 1 ? 1
                           : Small
  };
};

template<typename Lhs, typename Rhs> struct product_type
{
  typedef typename remove_all<Lhs>::type _Lhs;
  typedef typename remove_all<Rhs>::type _Rhs;
  enum {
    MaxRows  = _Lhs::MaxRowsAtCompileTime,
    Rows  = _Lhs::RowsAtCompileTime,
    MaxCols  = _Rhs::MaxColsAtCompileTime,
    Cols  = _Rhs::ColsAtCompileTime,
    MaxDepth = EIGEN_SIZE_MIN_PREFER_FIXED(_Lhs::MaxColsAtCompileTime,
                                           _Rhs::MaxRowsAtCompileTime),
    Depth = EIGEN_SIZE_MIN_PREFER_FIXED(_Lhs::ColsAtCompileTime,
                                        _Rhs::RowsAtCompileTime),
    LargeThreshold = EIGEN_CACHEFRIENDLY_PRODUCT_THRESHOLD
  };

  
  
private:
  enum {
    rows_select = product_size_category<Rows,MaxRows>::value,
    cols_select = product_size_category<Cols,MaxCols>::value,
    depth_select = product_size_category<Depth,MaxDepth>::value
  };
  typedef product_type_selector<rows_select, cols_select, depth_select> selector;

public:
  enum {
    value = selector::ret
  };
#ifdef EIGEN_DEBUG_PRODUCT
  static void debug()
  {
      EIGEN_DEBUG_VAR(Rows);
      EIGEN_DEBUG_VAR(Cols);
      EIGEN_DEBUG_VAR(Depth);
      EIGEN_DEBUG_VAR(rows_select);
      EIGEN_DEBUG_VAR(cols_select);
      EIGEN_DEBUG_VAR(depth_select);
      EIGEN_DEBUG_VAR(value);
  }
#endif
};


template<int M, int N>  struct product_type_selector<M,N,1>              { enum { ret = OuterProduct }; };
template<int Depth>     struct product_type_selector<1,    1,    Depth>  { enum { ret = InnerProduct }; };
template<>              struct product_type_selector<1,    1,    1>      { enum { ret = InnerProduct }; };
template<>              struct product_type_selector<Small,1,    Small>  { enum { ret = CoeffBasedProductMode }; };
template<>              struct product_type_selector<1,    Small,Small>  { enum { ret = CoeffBasedProductMode }; };
template<>              struct product_type_selector<Small,Small,Small>  { enum { ret = CoeffBasedProductMode }; };
template<>              struct product_type_selector<Small, Small, 1>    { enum { ret = LazyCoeffBasedProductMode }; };
template<>              struct product_type_selector<Small, Large, 1>    { enum { ret = LazyCoeffBasedProductMode }; };
template<>              struct product_type_selector<Large, Small, 1>    { enum { ret = LazyCoeffBasedProductMode }; };
template<>              struct product_type_selector<1,    Large,Small>  { enum { ret = CoeffBasedProductMode }; };
template<>              struct product_type_selector<1,    Large,Large>  { enum { ret = GemvProduct }; };
template<>              struct product_type_selector<1,    Small,Large>  { enum { ret = CoeffBasedProductMode }; };
template<>              struct product_type_selector<Large,1,    Small>  { enum { ret = CoeffBasedProductMode }; };
template<>              struct product_type_selector<Large,1,    Large>  { enum { ret = GemvProduct }; };
template<>              struct product_type_selector<Small,1,    Large>  { enum { ret = CoeffBasedProductMode }; };
template<>              struct product_type_selector<Small,Small,Large>  { enum { ret = GemmProduct }; };
template<>              struct product_type_selector<Large,Small,Large>  { enum { ret = GemmProduct }; };
template<>              struct product_type_selector<Small,Large,Large>  { enum { ret = GemmProduct }; };
template<>              struct product_type_selector<Large,Large,Large>  { enum { ret = GemmProduct }; };
template<>              struct product_type_selector<Large,Small,Small>  { enum { ret = GemmProduct }; };
template<>              struct product_type_selector<Small,Large,Small>  { enum { ret = GemmProduct }; };
template<>              struct product_type_selector<Large,Large,Small>  { enum { ret = GemmProduct }; };

} 

template<typename Lhs, typename Rhs, int ProductType>
struct ProductReturnType
{
  

  typedef GeneralProduct<Lhs, Rhs, ProductType> Type;
};

template<typename Lhs, typename Rhs>
struct ProductReturnType<Lhs,Rhs,CoeffBasedProductMode>
{
  typedef typename internal::nested<Lhs, Rhs::ColsAtCompileTime, typename internal::plain_matrix_type<Lhs>::type >::type LhsNested;
  typedef typename internal::nested<Rhs, Lhs::RowsAtCompileTime, typename internal::plain_matrix_type<Rhs>::type >::type RhsNested;
  typedef CoeffBasedProduct<LhsNested, RhsNested, EvalBeforeAssigningBit | EvalBeforeNestingBit> Type;
};

template<typename Lhs, typename Rhs>
struct ProductReturnType<Lhs,Rhs,LazyCoeffBasedProductMode>
{
  typedef typename internal::nested<Lhs, Rhs::ColsAtCompileTime, typename internal::plain_matrix_type<Lhs>::type >::type LhsNested;
  typedef typename internal::nested<Rhs, Lhs::RowsAtCompileTime, typename internal::plain_matrix_type<Rhs>::type >::type RhsNested;
  typedef CoeffBasedProduct<LhsNested, RhsNested, NestByRefBit> Type;
};

template<typename Lhs, typename Rhs>
struct LazyProductReturnType : public ProductReturnType<Lhs,Rhs,LazyCoeffBasedProductMode>
{};



namespace internal {

template<typename Lhs, typename Rhs>
struct traits<GeneralProduct<Lhs,Rhs,InnerProduct> >
 : traits<Matrix<typename scalar_product_traits<typename Lhs::Scalar, typename Rhs::Scalar>::ReturnType,1,1> >
{};

}

template<typename Lhs, typename Rhs>
class GeneralProduct<Lhs, Rhs, InnerProduct>
  : internal::no_assignment_operator,
    public Matrix<typename internal::scalar_product_traits<typename Lhs::Scalar, typename Rhs::Scalar>::ReturnType,1,1>
{
    typedef Matrix<typename internal::scalar_product_traits<typename Lhs::Scalar, typename Rhs::Scalar>::ReturnType,1,1> Base;
  public:
    GeneralProduct(const Lhs& lhs, const Rhs& rhs)
    {
      EIGEN_STATIC_ASSERT((internal::is_same<typename Lhs::RealScalar, typename Rhs::RealScalar>::value),
        YOU_MIXED_DIFFERENT_NUMERIC_TYPES__YOU_NEED_TO_USE_THE_CAST_METHOD_OF_MATRIXBASE_TO_CAST_NUMERIC_TYPES_EXPLICITLY)

      Base::coeffRef(0,0) = (lhs.transpose().cwiseProduct(rhs)).sum();
    }

    
    operator const typename Base::Scalar() const {
      return Base::coeff(0,0);
    }
};


namespace internal {

template<typename ProductType, typename Dest, typename Func>
EIGEN_DONT_INLINE void outer_product_selector_run(const ProductType& prod, Dest& dest, const Func& func, const false_type&)
{
  typedef typename Dest::Index Index;
  
  
  const Index cols = dest.cols();
  for (Index j=0; j<cols; ++j)
    func(dest.col(j), prod.rhs().coeff(0,j) * prod.lhs());
}

template<typename ProductType, typename Dest, typename Func>
EIGEN_DONT_INLINE void outer_product_selector_run(const ProductType& prod, Dest& dest, const Func& func, const true_type&) {
  typedef typename Dest::Index Index;
  
  
  const Index rows = dest.rows();
  for (Index i=0; i<rows; ++i)
    func(dest.row(i), prod.lhs().coeff(i,0) * prod.rhs());
}

template<typename Lhs, typename Rhs>
struct traits<GeneralProduct<Lhs,Rhs,OuterProduct> >
 : traits<ProductBase<GeneralProduct<Lhs,Rhs,OuterProduct>, Lhs, Rhs> >
{};

}

template<typename Lhs, typename Rhs>
class GeneralProduct<Lhs, Rhs, OuterProduct>
  : public ProductBase<GeneralProduct<Lhs,Rhs,OuterProduct>, Lhs, Rhs>
{
    template<typename T> struct IsRowMajor : internal::conditional<(int(T::Flags)&RowMajorBit), internal::true_type, internal::false_type>::type {};
    
  public:
    EIGEN_PRODUCT_PUBLIC_INTERFACE(GeneralProduct)

    GeneralProduct(const Lhs& lhs, const Rhs& rhs) : Base(lhs,rhs)
    {
      EIGEN_STATIC_ASSERT((internal::is_same<typename Lhs::RealScalar, typename Rhs::RealScalar>::value),
        YOU_MIXED_DIFFERENT_NUMERIC_TYPES__YOU_NEED_TO_USE_THE_CAST_METHOD_OF_MATRIXBASE_TO_CAST_NUMERIC_TYPES_EXPLICITLY)
    }
    
    struct set  { template<typename Dst, typename Src> void operator()(const Dst& dst, const Src& src) const { dst.const_cast_derived()  = src; } };
    struct add  { template<typename Dst, typename Src> void operator()(const Dst& dst, const Src& src) const { dst.const_cast_derived() += src; } };
    struct sub  { template<typename Dst, typename Src> void operator()(const Dst& dst, const Src& src) const { dst.const_cast_derived() -= src; } };
    struct adds {
      Scalar m_scale;
      adds(const Scalar& s) : m_scale(s) {}
      template<typename Dst, typename Src> void operator()(const Dst& dst, const Src& src) const {
        dst.const_cast_derived() += m_scale * src;
      }
    };
    
    template<typename Dest>
    inline void evalTo(Dest& dest) const {
      internal::outer_product_selector_run(*this, dest, set(), IsRowMajor<Dest>());
    }
    
    template<typename Dest>
    inline void addTo(Dest& dest) const {
      internal::outer_product_selector_run(*this, dest, add(), IsRowMajor<Dest>());
    }

    template<typename Dest>
    inline void subTo(Dest& dest) const {
      internal::outer_product_selector_run(*this, dest, sub(), IsRowMajor<Dest>());
    }

    template<typename Dest> void scaleAndAddTo(Dest& dest, const Scalar& alpha) const
    {
      internal::outer_product_selector_run(*this, dest, adds(alpha), IsRowMajor<Dest>());
    }
};


namespace internal {

template<typename Lhs, typename Rhs>
struct traits<GeneralProduct<Lhs,Rhs,GemvProduct> >
 : traits<ProductBase<GeneralProduct<Lhs,Rhs,GemvProduct>, Lhs, Rhs> >
{};

template<int Side, int StorageOrder, bool BlasCompatible>
struct gemv_selector;

} 

template<typename Lhs, typename Rhs>
class GeneralProduct<Lhs, Rhs, GemvProduct>
  : public ProductBase<GeneralProduct<Lhs,Rhs,GemvProduct>, Lhs, Rhs>
{
  public:
    EIGEN_PRODUCT_PUBLIC_INTERFACE(GeneralProduct)

    typedef typename Lhs::Scalar LhsScalar;
    typedef typename Rhs::Scalar RhsScalar;

    GeneralProduct(const Lhs& a_lhs, const Rhs& a_rhs) : Base(a_lhs,a_rhs)
    {
    }

    enum { Side = Lhs::IsVectorAtCompileTime ? OnTheLeft : OnTheRight };
    typedef typename internal::conditional<int(Side)==OnTheRight,_LhsNested,_RhsNested>::type MatrixType;

    template<typename Dest> void scaleAndAddTo(Dest& dst, const Scalar& alpha) const
    {
      eigen_assert(m_lhs.rows() == dst.rows() && m_rhs.cols() == dst.cols());
      internal::gemv_selector<Side,(int(MatrixType::Flags)&RowMajorBit) ? RowMajor : ColMajor,
                       bool(internal::blas_traits<MatrixType>::HasUsableDirectAccess)>::run(*this, dst, alpha);
    }
};

namespace internal {

template<int StorageOrder, bool BlasCompatible>
struct gemv_selector<OnTheLeft,StorageOrder,BlasCompatible>
{
  template<typename ProductType, typename Dest>
  static void run(const ProductType& prod, Dest& dest, const typename ProductType::Scalar& alpha)
  {
    Transpose<Dest> destT(dest);
    enum { OtherStorageOrder = StorageOrder == RowMajor ? ColMajor : RowMajor };
    gemv_selector<OnTheRight,OtherStorageOrder,BlasCompatible>
      ::run(GeneralProduct<Transpose<const typename ProductType::_RhsNested>,Transpose<const typename ProductType::_LhsNested>, GemvProduct>
        (prod.rhs().transpose(), prod.lhs().transpose()), destT, alpha);
  }
};

template<typename Scalar,int Size,int MaxSize,bool Cond> struct gemv_static_vector_if;

template<typename Scalar,int Size,int MaxSize>
struct gemv_static_vector_if<Scalar,Size,MaxSize,false>
{
  EIGEN_STRONG_INLINE  Scalar* data() { eigen_internal_assert(false && "should never be called"); return 0; }
};

template<typename Scalar,int Size>
struct gemv_static_vector_if<Scalar,Size,Dynamic,true>
{
  EIGEN_STRONG_INLINE Scalar* data() { return 0; }
};

template<typename Scalar,int Size,int MaxSize>
struct gemv_static_vector_if<Scalar,Size,MaxSize,true>
{
  #if EIGEN_ALIGN_STATICALLY
  internal::plain_array<Scalar,EIGEN_SIZE_MIN_PREFER_FIXED(Size,MaxSize),0> m_data;
  EIGEN_STRONG_INLINE Scalar* data() { return m_data.array; }
  #else
  
  
  enum {
    ForceAlignment  = internal::packet_traits<Scalar>::Vectorizable,
    PacketSize      = internal::packet_traits<Scalar>::size
  };
  internal::plain_array<Scalar,EIGEN_SIZE_MIN_PREFER_FIXED(Size,MaxSize)+(ForceAlignment?PacketSize:0),0> m_data;
  EIGEN_STRONG_INLINE Scalar* data() {
    return ForceAlignment
            ? reinterpret_cast<Scalar*>((reinterpret_cast<size_t>(m_data.array) & ~(size_t(15))) + 16)
            : m_data.array;
  }
  #endif
};

template<> struct gemv_selector<OnTheRight,ColMajor,true>
{
  template<typename ProductType, typename Dest>
  static inline void run(const ProductType& prod, Dest& dest, const typename ProductType::Scalar& alpha)
  {
    typedef typename ProductType::Index Index;
    typedef typename ProductType::LhsScalar   LhsScalar;
    typedef typename ProductType::RhsScalar   RhsScalar;
    typedef typename ProductType::Scalar      ResScalar;
    typedef typename ProductType::RealScalar  RealScalar;
    typedef typename ProductType::ActualLhsType ActualLhsType;
    typedef typename ProductType::ActualRhsType ActualRhsType;
    typedef typename ProductType::LhsBlasTraits LhsBlasTraits;
    typedef typename ProductType::RhsBlasTraits RhsBlasTraits;
    typedef Map<Matrix<ResScalar,Dynamic,1>, Aligned> MappedDest;

    ActualLhsType actualLhs = LhsBlasTraits::extract(prod.lhs());
    ActualRhsType actualRhs = RhsBlasTraits::extract(prod.rhs());

    ResScalar actualAlpha = alpha * LhsBlasTraits::extractScalarFactor(prod.lhs())
                                  * RhsBlasTraits::extractScalarFactor(prod.rhs());

    enum {
      
      
      EvalToDestAtCompileTime = Dest::InnerStrideAtCompileTime==1,
      ComplexByReal = (NumTraits<LhsScalar>::IsComplex) && (!NumTraits<RhsScalar>::IsComplex),
      MightCannotUseDest = (Dest::InnerStrideAtCompileTime!=1) || ComplexByReal
    };

    gemv_static_vector_if<ResScalar,Dest::SizeAtCompileTime,Dest::MaxSizeAtCompileTime,MightCannotUseDest> static_dest;

    bool alphaIsCompatible = (!ComplexByReal) || (numext::imag(actualAlpha)==RealScalar(0));
    bool evalToDest = EvalToDestAtCompileTime && alphaIsCompatible;
    
    RhsScalar compatibleAlpha = get_factor<ResScalar,RhsScalar>::run(actualAlpha);

    ei_declare_aligned_stack_constructed_variable(ResScalar,actualDestPtr,dest.size(),
                                                  evalToDest ? dest.data() : static_dest.data());
    
    if(!evalToDest)
    {
      #ifdef EIGEN_DENSE_STORAGE_CTOR_PLUGIN
      int size = dest.size();
      EIGEN_DENSE_STORAGE_CTOR_PLUGIN
      #endif
      if(!alphaIsCompatible)
      {
        MappedDest(actualDestPtr, dest.size()).setZero();
        compatibleAlpha = RhsScalar(1);
      }
      else
        MappedDest(actualDestPtr, dest.size()) = dest;
    }

    general_matrix_vector_product
      <Index,LhsScalar,ColMajor,LhsBlasTraits::NeedToConjugate,RhsScalar,RhsBlasTraits::NeedToConjugate>::run(
        actualLhs.rows(), actualLhs.cols(),
        actualLhs.data(), actualLhs.outerStride(),
        actualRhs.data(), actualRhs.innerStride(),
        actualDestPtr, 1,
        compatibleAlpha);

    if (!evalToDest)
    {
      if(!alphaIsCompatible)
        dest += actualAlpha * MappedDest(actualDestPtr, dest.size());
      else
        dest = MappedDest(actualDestPtr, dest.size());
    }
  }
};

template<> struct gemv_selector<OnTheRight,RowMajor,true>
{
  template<typename ProductType, typename Dest>
  static void run(const ProductType& prod, Dest& dest, const typename ProductType::Scalar& alpha)
  {
    typedef typename ProductType::LhsScalar LhsScalar;
    typedef typename ProductType::RhsScalar RhsScalar;
    typedef typename ProductType::Scalar    ResScalar;
    typedef typename ProductType::Index Index;
    typedef typename ProductType::ActualLhsType ActualLhsType;
    typedef typename ProductType::ActualRhsType ActualRhsType;
    typedef typename ProductType::_ActualRhsType _ActualRhsType;
    typedef typename ProductType::LhsBlasTraits LhsBlasTraits;
    typedef typename ProductType::RhsBlasTraits RhsBlasTraits;

    typename add_const<ActualLhsType>::type actualLhs = LhsBlasTraits::extract(prod.lhs());
    typename add_const<ActualRhsType>::type actualRhs = RhsBlasTraits::extract(prod.rhs());

    ResScalar actualAlpha = alpha * LhsBlasTraits::extractScalarFactor(prod.lhs())
                                  * RhsBlasTraits::extractScalarFactor(prod.rhs());

    enum {
      
      
      DirectlyUseRhs = _ActualRhsType::InnerStrideAtCompileTime==1
    };

    gemv_static_vector_if<RhsScalar,_ActualRhsType::SizeAtCompileTime,_ActualRhsType::MaxSizeAtCompileTime,!DirectlyUseRhs> static_rhs;

    ei_declare_aligned_stack_constructed_variable(RhsScalar,actualRhsPtr,actualRhs.size(),
        DirectlyUseRhs ? const_cast<RhsScalar*>(actualRhs.data()) : static_rhs.data());

    if(!DirectlyUseRhs)
    {
      #ifdef EIGEN_DENSE_STORAGE_CTOR_PLUGIN
      int size = actualRhs.size();
      EIGEN_DENSE_STORAGE_CTOR_PLUGIN
      #endif
      Map<typename _ActualRhsType::PlainObject>(actualRhsPtr, actualRhs.size()) = actualRhs;
    }

    general_matrix_vector_product
      <Index,LhsScalar,RowMajor,LhsBlasTraits::NeedToConjugate,RhsScalar,RhsBlasTraits::NeedToConjugate>::run(
        actualLhs.rows(), actualLhs.cols(),
        actualLhs.data(), actualLhs.outerStride(),
        actualRhsPtr, 1,
        dest.data(), dest.innerStride(),
        actualAlpha);
  }
};

template<> struct gemv_selector<OnTheRight,ColMajor,false>
{
  template<typename ProductType, typename Dest>
  static void run(const ProductType& prod, Dest& dest, const typename ProductType::Scalar& alpha)
  {
    typedef typename Dest::Index Index;
    
    const Index size = prod.rhs().rows();
    for(Index k=0; k<size; ++k)
      dest += (alpha*prod.rhs().coeff(k)) * prod.lhs().col(k);
  }
};

template<> struct gemv_selector<OnTheRight,RowMajor,false>
{
  template<typename ProductType, typename Dest>
  static void run(const ProductType& prod, Dest& dest, const typename ProductType::Scalar& alpha)
  {
    typedef typename Dest::Index Index;
    
    const Index rows = prod.rows();
    for(Index i=0; i<rows; ++i)
      dest.coeffRef(i) += alpha * (prod.lhs().row(i).cwiseProduct(prod.rhs().transpose())).sum();
  }
};

} 


template<typename Derived>
template<typename OtherDerived>
inline const typename ProductReturnType<Derived, OtherDerived>::Type
MatrixBase<Derived>::operator*(const MatrixBase<OtherDerived> &other) const
{
  
  
  
  
  enum {
    ProductIsValid =  Derived::ColsAtCompileTime==Dynamic
                   || OtherDerived::RowsAtCompileTime==Dynamic
                   || int(Derived::ColsAtCompileTime)==int(OtherDerived::RowsAtCompileTime),
    AreVectors = Derived::IsVectorAtCompileTime && OtherDerived::IsVectorAtCompileTime,
    SameSizes = EIGEN_PREDICATE_SAME_MATRIX_SIZE(Derived,OtherDerived)
  };
  
  
  
  EIGEN_STATIC_ASSERT(ProductIsValid || !(AreVectors && SameSizes),
    INVALID_VECTOR_VECTOR_PRODUCT__IF_YOU_WANTED_A_DOT_OR_COEFF_WISE_PRODUCT_YOU_MUST_USE_THE_EXPLICIT_FUNCTIONS)
  EIGEN_STATIC_ASSERT(ProductIsValid || !(SameSizes && !AreVectors),
    INVALID_MATRIX_PRODUCT__IF_YOU_WANTED_A_COEFF_WISE_PRODUCT_YOU_MUST_USE_THE_EXPLICIT_FUNCTION)
  EIGEN_STATIC_ASSERT(ProductIsValid || SameSizes, INVALID_MATRIX_PRODUCT)
#ifdef EIGEN_DEBUG_PRODUCT
  internal::product_type<Derived,OtherDerived>::debug();
#endif
  return typename ProductReturnType<Derived,OtherDerived>::Type(derived(), other.derived());
}

template<typename Derived>
template<typename OtherDerived>
const typename LazyProductReturnType<Derived,OtherDerived>::Type
MatrixBase<Derived>::lazyProduct(const MatrixBase<OtherDerived> &other) const
{
  enum {
    ProductIsValid =  Derived::ColsAtCompileTime==Dynamic
                   || OtherDerived::RowsAtCompileTime==Dynamic
                   || int(Derived::ColsAtCompileTime)==int(OtherDerived::RowsAtCompileTime),
    AreVectors = Derived::IsVectorAtCompileTime && OtherDerived::IsVectorAtCompileTime,
    SameSizes = EIGEN_PREDICATE_SAME_MATRIX_SIZE(Derived,OtherDerived)
  };
  
  
  
  EIGEN_STATIC_ASSERT(ProductIsValid || !(AreVectors && SameSizes),
    INVALID_VECTOR_VECTOR_PRODUCT__IF_YOU_WANTED_A_DOT_OR_COEFF_WISE_PRODUCT_YOU_MUST_USE_THE_EXPLICIT_FUNCTIONS)
  EIGEN_STATIC_ASSERT(ProductIsValid || !(SameSizes && !AreVectors),
    INVALID_MATRIX_PRODUCT__IF_YOU_WANTED_A_COEFF_WISE_PRODUCT_YOU_MUST_USE_THE_EXPLICIT_FUNCTION)
  EIGEN_STATIC_ASSERT(ProductIsValid || SameSizes, INVALID_MATRIX_PRODUCT)

  return typename LazyProductReturnType<Derived,OtherDerived>::Type(derived(), other.derived());
}

} 

#endif 
