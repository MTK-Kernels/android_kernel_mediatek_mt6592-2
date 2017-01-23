// Copyright (C) 2006-2010 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_DENSECOEFFSBASE_H
#define EIGEN_DENSECOEFFSBASE_H

namespace Eigen {

namespace internal {
template<typename T> struct add_const_on_value_type_if_arithmetic
{
  typedef typename conditional<is_arithmetic<T>::value, T, typename add_const_on_value_type<T>::type>::type type;
};
}

template<typename Derived>
class DenseCoeffsBase<Derived,ReadOnlyAccessors> : public EigenBase<Derived>
{
  public:
    typedef typename internal::traits<Derived>::StorageKind StorageKind;
    typedef typename internal::traits<Derived>::Index Index;
    typedef typename internal::traits<Derived>::Scalar Scalar;
    typedef typename internal::packet_traits<Scalar>::type PacketScalar;

    
    
    
    
    
    
    
    typedef typename internal::conditional<bool(internal::traits<Derived>::Flags&LvalueBit),
                         const Scalar&,
                         typename internal::conditional<internal::is_arithmetic<Scalar>::value, Scalar, const Scalar>::type
                     >::type CoeffReturnType;

    typedef typename internal::add_const_on_value_type_if_arithmetic<
                         typename internal::packet_traits<Scalar>::type
                     >::type PacketReturnType;

    typedef EigenBase<Derived> Base;
    using Base::rows;
    using Base::cols;
    using Base::size;
    using Base::derived;

    EIGEN_STRONG_INLINE Index rowIndexByOuterInner(Index outer, Index inner) const
    {
      return int(Derived::RowsAtCompileTime) == 1 ? 0
          : int(Derived::ColsAtCompileTime) == 1 ? inner
          : int(Derived::Flags)&RowMajorBit ? outer
          : inner;
    }

    EIGEN_STRONG_INLINE Index colIndexByOuterInner(Index outer, Index inner) const
    {
      return int(Derived::ColsAtCompileTime) == 1 ? 0
          : int(Derived::RowsAtCompileTime) == 1 ? inner
          : int(Derived::Flags)&RowMajorBit ? inner
          : outer;
    }

    EIGEN_STRONG_INLINE CoeffReturnType coeff(Index row, Index col) const
    {
      eigen_internal_assert(row >= 0 && row < rows()
                        && col >= 0 && col < cols());
      return derived().coeff(row, col);
    }

    EIGEN_STRONG_INLINE CoeffReturnType coeffByOuterInner(Index outer, Index inner) const
    {
      return coeff(rowIndexByOuterInner(outer, inner),
                   colIndexByOuterInner(outer, inner));
    }

    EIGEN_STRONG_INLINE CoeffReturnType operator()(Index row, Index col) const
    {
      eigen_assert(row >= 0 && row < rows()
          && col >= 0 && col < cols());
      return derived().coeff(row, col);
    }


    EIGEN_STRONG_INLINE CoeffReturnType
    coeff(Index index) const
    {
      eigen_internal_assert(index >= 0 && index < size());
      return derived().coeff(index);
    }



    EIGEN_STRONG_INLINE CoeffReturnType
    operator[](Index index) const
    {
      #ifndef EIGEN2_SUPPORT
      EIGEN_STATIC_ASSERT(Derived::IsVectorAtCompileTime,
                          THE_BRACKET_OPERATOR_IS_ONLY_FOR_VECTORS__USE_THE_PARENTHESIS_OPERATOR_INSTEAD)
      #endif
      eigen_assert(index >= 0 && index < size());
      return derived().coeff(index);
    }


    EIGEN_STRONG_INLINE CoeffReturnType
    operator()(Index index) const
    {
      eigen_assert(index >= 0 && index < size());
      return derived().coeff(index);
    }

    

    EIGEN_STRONG_INLINE CoeffReturnType
    x() const { return (*this)[0]; }

    

    EIGEN_STRONG_INLINE CoeffReturnType
    y() const { return (*this)[1]; }

    

    EIGEN_STRONG_INLINE CoeffReturnType
    z() const { return (*this)[2]; }

    

    EIGEN_STRONG_INLINE CoeffReturnType
    w() const { return (*this)[3]; }


    template<int LoadMode>
    EIGEN_STRONG_INLINE PacketReturnType packet(Index row, Index col) const
    {
      eigen_internal_assert(row >= 0 && row < rows()
                      && col >= 0 && col < cols());
      return derived().template packet<LoadMode>(row,col);
    }


    
    template<int LoadMode>
    EIGEN_STRONG_INLINE PacketReturnType packetByOuterInner(Index outer, Index inner) const
    {
      return packet<LoadMode>(rowIndexByOuterInner(outer, inner),
                              colIndexByOuterInner(outer, inner));
    }


    template<int LoadMode>
    EIGEN_STRONG_INLINE PacketReturnType packet(Index index) const
    {
      eigen_internal_assert(index >= 0 && index < size());
      return derived().template packet<LoadMode>(index);
    }

  protected:
    
    
    
    
    
    void coeffRef();
    void coeffRefByOuterInner();
    void writePacket();
    void writePacketByOuterInner();
    void copyCoeff();
    void copyCoeffByOuterInner();
    void copyPacket();
    void copyPacketByOuterInner();
    void stride();
    void innerStride();
    void outerStride();
    void rowStride();
    void colStride();
};

template<typename Derived>
class DenseCoeffsBase<Derived, WriteAccessors> : public DenseCoeffsBase<Derived, ReadOnlyAccessors>
{
  public:

    typedef DenseCoeffsBase<Derived, ReadOnlyAccessors> Base;

    typedef typename internal::traits<Derived>::StorageKind StorageKind;
    typedef typename internal::traits<Derived>::Index Index;
    typedef typename internal::traits<Derived>::Scalar Scalar;
    typedef typename internal::packet_traits<Scalar>::type PacketScalar;
    typedef typename NumTraits<Scalar>::Real RealScalar;

    using Base::coeff;
    using Base::rows;
    using Base::cols;
    using Base::size;
    using Base::derived;
    using Base::rowIndexByOuterInner;
    using Base::colIndexByOuterInner;
    using Base::operator[];
    using Base::operator();
    using Base::x;
    using Base::y;
    using Base::z;
    using Base::w;

    EIGEN_STRONG_INLINE Scalar& coeffRef(Index row, Index col)
    {
      eigen_internal_assert(row >= 0 && row < rows()
                        && col >= 0 && col < cols());
      return derived().coeffRef(row, col);
    }

    EIGEN_STRONG_INLINE Scalar&
    coeffRefByOuterInner(Index outer, Index inner)
    {
      return coeffRef(rowIndexByOuterInner(outer, inner),
                      colIndexByOuterInner(outer, inner));
    }


    EIGEN_STRONG_INLINE Scalar&
    operator()(Index row, Index col)
    {
      eigen_assert(row >= 0 && row < rows()
          && col >= 0 && col < cols());
      return derived().coeffRef(row, col);
    }



    EIGEN_STRONG_INLINE Scalar&
    coeffRef(Index index)
    {
      eigen_internal_assert(index >= 0 && index < size());
      return derived().coeffRef(index);
    }


    EIGEN_STRONG_INLINE Scalar&
    operator[](Index index)
    {
      #ifndef EIGEN2_SUPPORT
      EIGEN_STATIC_ASSERT(Derived::IsVectorAtCompileTime,
                          THE_BRACKET_OPERATOR_IS_ONLY_FOR_VECTORS__USE_THE_PARENTHESIS_OPERATOR_INSTEAD)
      #endif
      eigen_assert(index >= 0 && index < size());
      return derived().coeffRef(index);
    }


    EIGEN_STRONG_INLINE Scalar&
    operator()(Index index)
    {
      eigen_assert(index >= 0 && index < size());
      return derived().coeffRef(index);
    }

    

    EIGEN_STRONG_INLINE Scalar&
    x() { return (*this)[0]; }

    

    EIGEN_STRONG_INLINE Scalar&
    y() { return (*this)[1]; }

    

    EIGEN_STRONG_INLINE Scalar&
    z() { return (*this)[2]; }

    

    EIGEN_STRONG_INLINE Scalar&
    w() { return (*this)[3]; }


    template<int StoreMode>
    EIGEN_STRONG_INLINE void writePacket
    (Index row, Index col, const typename internal::packet_traits<Scalar>::type& val)
    {
      eigen_internal_assert(row >= 0 && row < rows()
                        && col >= 0 && col < cols());
      derived().template writePacket<StoreMode>(row,col,val);
    }


    
    template<int StoreMode>
    EIGEN_STRONG_INLINE void writePacketByOuterInner
    (Index outer, Index inner, const typename internal::packet_traits<Scalar>::type& val)
    {
      writePacket<StoreMode>(rowIndexByOuterInner(outer, inner),
                            colIndexByOuterInner(outer, inner),
                            val);
    }

    template<int StoreMode>
    EIGEN_STRONG_INLINE void writePacket
    (Index index, const typename internal::packet_traits<Scalar>::type& val)
    {
      eigen_internal_assert(index >= 0 && index < size());
      derived().template writePacket<StoreMode>(index,val);
    }

#ifndef EIGEN_PARSED_BY_DOXYGEN


    template<typename OtherDerived>
    EIGEN_STRONG_INLINE void copyCoeff(Index row, Index col, const DenseBase<OtherDerived>& other)
    {
      eigen_internal_assert(row >= 0 && row < rows()
                        && col >= 0 && col < cols());
      derived().coeffRef(row, col) = other.derived().coeff(row, col);
    }


    template<typename OtherDerived>
    EIGEN_STRONG_INLINE void copyCoeff(Index index, const DenseBase<OtherDerived>& other)
    {
      eigen_internal_assert(index >= 0 && index < size());
      derived().coeffRef(index) = other.derived().coeff(index);
    }


    template<typename OtherDerived>
    EIGEN_STRONG_INLINE void copyCoeffByOuterInner(Index outer, Index inner, const DenseBase<OtherDerived>& other)
    {
      const Index row = rowIndexByOuterInner(outer,inner);
      const Index col = colIndexByOuterInner(outer,inner);
      
      derived().copyCoeff(row, col, other);
    }


    template<typename OtherDerived, int StoreMode, int LoadMode>
    EIGEN_STRONG_INLINE void copyPacket(Index row, Index col, const DenseBase<OtherDerived>& other)
    {
      eigen_internal_assert(row >= 0 && row < rows()
                        && col >= 0 && col < cols());
      derived().template writePacket<StoreMode>(row, col,
        other.derived().template packet<LoadMode>(row, col));
    }


    template<typename OtherDerived, int StoreMode, int LoadMode>
    EIGEN_STRONG_INLINE void copyPacket(Index index, const DenseBase<OtherDerived>& other)
    {
      eigen_internal_assert(index >= 0 && index < size());
      derived().template writePacket<StoreMode>(index,
        other.derived().template packet<LoadMode>(index));
    }

    
    template<typename OtherDerived, int StoreMode, int LoadMode>
    EIGEN_STRONG_INLINE void copyPacketByOuterInner(Index outer, Index inner, const DenseBase<OtherDerived>& other)
    {
      const Index row = rowIndexByOuterInner(outer,inner);
      const Index col = colIndexByOuterInner(outer,inner);
      
      derived().template copyPacket< OtherDerived, StoreMode, LoadMode>(row, col, other);
    }
#endif

};

template<typename Derived>
class DenseCoeffsBase<Derived, DirectAccessors> : public DenseCoeffsBase<Derived, ReadOnlyAccessors>
{
  public:

    typedef DenseCoeffsBase<Derived, ReadOnlyAccessors> Base;
    typedef typename internal::traits<Derived>::Index Index;
    typedef typename internal::traits<Derived>::Scalar Scalar;
    typedef typename NumTraits<Scalar>::Real RealScalar;

    using Base::rows;
    using Base::cols;
    using Base::size;
    using Base::derived;

    inline Index innerStride() const
    {
      return derived().innerStride();
    }

    inline Index outerStride() const
    {
      return derived().outerStride();
    }

    
    inline Index stride() const
    {
      return Derived::IsVectorAtCompileTime ? innerStride() : outerStride();
    }

    inline Index rowStride() const
    {
      return Derived::IsRowMajor ? outerStride() : innerStride();
    }

    inline Index colStride() const
    {
      return Derived::IsRowMajor ? innerStride() : outerStride();
    }
};

template<typename Derived>
class DenseCoeffsBase<Derived, DirectWriteAccessors>
  : public DenseCoeffsBase<Derived, WriteAccessors>
{
  public:

    typedef DenseCoeffsBase<Derived, WriteAccessors> Base;
    typedef typename internal::traits<Derived>::Index Index;
    typedef typename internal::traits<Derived>::Scalar Scalar;
    typedef typename NumTraits<Scalar>::Real RealScalar;

    using Base::rows;
    using Base::cols;
    using Base::size;
    using Base::derived;

    inline Index innerStride() const
    {
      return derived().innerStride();
    }

    inline Index outerStride() const
    {
      return derived().outerStride();
    }

    
    inline Index stride() const
    {
      return Derived::IsVectorAtCompileTime ? innerStride() : outerStride();
    }

    inline Index rowStride() const
    {
      return Derived::IsRowMajor ? outerStride() : innerStride();
    }

    inline Index colStride() const
    {
      return Derived::IsRowMajor ? innerStride() : outerStride();
    }
};

namespace internal {

template<typename Derived, bool JustReturnZero>
struct first_aligned_impl
{
  static inline typename Derived::Index run(const Derived&)
  { return 0; }
};

template<typename Derived>
struct first_aligned_impl<Derived, false>
{
  static inline typename Derived::Index run(const Derived& m)
  {
    return internal::first_aligned(&m.const_cast_derived().coeffRef(0,0), m.size());
  }
};

template<typename Derived>
static inline typename Derived::Index first_aligned(const Derived& m)
{
  return first_aligned_impl
          <Derived, (Derived::Flags & AlignedBit) || !(Derived::Flags & DirectAccessBit)>
          ::run(m);
}

template<typename Derived, bool HasDirectAccess = has_direct_access<Derived>::ret>
struct inner_stride_at_compile_time
{
  enum { ret = traits<Derived>::InnerStrideAtCompileTime };
};

template<typename Derived>
struct inner_stride_at_compile_time<Derived, false>
{
  enum { ret = 0 };
};

template<typename Derived, bool HasDirectAccess = has_direct_access<Derived>::ret>
struct outer_stride_at_compile_time
{
  enum { ret = traits<Derived>::OuterStrideAtCompileTime };
};

template<typename Derived>
struct outer_stride_at_compile_time<Derived, false>
{
  enum { ret = 0 };
};

} 

} 

#endif 
