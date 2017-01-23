// Copyright (C) 2008-2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_DENSESTORAGEBASE_H
#define EIGEN_DENSESTORAGEBASE_H

#if defined(EIGEN_INITIALIZE_MATRICES_BY_ZERO)
# define EIGEN_INITIALIZE_COEFFS
# define EIGEN_INITIALIZE_COEFFS_IF_THAT_OPTION_IS_ENABLED for(int i=0;i<base().size();++i) coeffRef(i)=Scalar(0);
#elif defined(EIGEN_INITIALIZE_MATRICES_BY_NAN)
# define EIGEN_INITIALIZE_COEFFS
# define EIGEN_INITIALIZE_COEFFS_IF_THAT_OPTION_IS_ENABLED for(int i=0;i<base().size();++i) coeffRef(i)=std::numeric_limits<Scalar>::quiet_NaN();
#else
# undef EIGEN_INITIALIZE_COEFFS
# define EIGEN_INITIALIZE_COEFFS_IF_THAT_OPTION_IS_ENABLED
#endif

namespace Eigen {

namespace internal {

template<int MaxSizeAtCompileTime> struct check_rows_cols_for_overflow {
  template<typename Index>
  static EIGEN_ALWAYS_INLINE void run(Index, Index)
  {
  }
};

template<> struct check_rows_cols_for_overflow<Dynamic> {
  template<typename Index>
  static EIGEN_ALWAYS_INLINE void run(Index rows, Index cols)
  {
    
    
    Index max_index = (size_t(1) << (8 * sizeof(Index) - 1)) - 1; 
    bool error = (rows == 0 || cols == 0) ? false
               : (rows > max_index / cols);
    if (error)
      throw_std_bad_alloc();
  }
};

template <typename Derived,
          typename OtherDerived = Derived,
          bool IsVector = bool(Derived::IsVectorAtCompileTime) && bool(OtherDerived::IsVectorAtCompileTime)>
struct conservative_resize_like_impl;

template<typename MatrixTypeA, typename MatrixTypeB, bool SwapPointers> struct matrix_swap_impl;

} 

#ifdef EIGEN_PARSED_BY_DOXYGEN
namespace internal {

template<typename Derived> struct dense_xpr_base_dispatcher_for_doxygen;
template<typename _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
struct dense_xpr_base_dispatcher_for_doxygen<Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols> >
    : public MatrixBase<Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols> > {};
template<typename _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
struct dense_xpr_base_dispatcher_for_doxygen<Array<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols> >
    : public ArrayBase<Array<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols> > {};

} 

template<typename Derived>
class PlainObjectBase : public internal::dense_xpr_base_dispatcher_for_doxygen<Derived>
#else
template<typename Derived>
class PlainObjectBase : public internal::dense_xpr_base<Derived>::type
#endif
{
  public:
    enum { Options = internal::traits<Derived>::Options };
    typedef typename internal::dense_xpr_base<Derived>::type Base;

    typedef typename internal::traits<Derived>::StorageKind StorageKind;
    typedef typename internal::traits<Derived>::Index Index;
    typedef typename internal::traits<Derived>::Scalar Scalar;
    typedef typename internal::packet_traits<Scalar>::type PacketScalar;
    typedef typename NumTraits<Scalar>::Real RealScalar;
    typedef Derived DenseType;

    using Base::RowsAtCompileTime;
    using Base::ColsAtCompileTime;
    using Base::SizeAtCompileTime;
    using Base::MaxRowsAtCompileTime;
    using Base::MaxColsAtCompileTime;
    using Base::MaxSizeAtCompileTime;
    using Base::IsVectorAtCompileTime;
    using Base::Flags;

    template<typename PlainObjectType, int MapOptions, typename StrideType> friend class Eigen::Map;
    friend  class Eigen::Map<Derived, Unaligned>;
    typedef Eigen::Map<Derived, Unaligned>  MapType;
    friend  class Eigen::Map<const Derived, Unaligned>;
    typedef const Eigen::Map<const Derived, Unaligned> ConstMapType;
    friend  class Eigen::Map<Derived, Aligned>;
    typedef Eigen::Map<Derived, Aligned> AlignedMapType;
    friend  class Eigen::Map<const Derived, Aligned>;
    typedef const Eigen::Map<const Derived, Aligned> ConstAlignedMapType;
    template<typename StrideType> struct StridedMapType { typedef Eigen::Map<Derived, Unaligned, StrideType> type; };
    template<typename StrideType> struct StridedConstMapType { typedef Eigen::Map<const Derived, Unaligned, StrideType> type; };
    template<typename StrideType> struct StridedAlignedMapType { typedef Eigen::Map<Derived, Aligned, StrideType> type; };
    template<typename StrideType> struct StridedConstAlignedMapType { typedef Eigen::Map<const Derived, Aligned, StrideType> type; };

  protected:
    DenseStorage<Scalar, Base::MaxSizeAtCompileTime, Base::RowsAtCompileTime, Base::ColsAtCompileTime, Options> m_storage;

  public:
    enum { NeedsToAlign = SizeAtCompileTime != Dynamic && (internal::traits<Derived>::Flags & AlignedBit) != 0 };
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF(NeedsToAlign)

    Base& base() { return *static_cast<Base*>(this); }
    const Base& base() const { return *static_cast<const Base*>(this); }

    EIGEN_STRONG_INLINE Index rows() const { return m_storage.rows(); }
    EIGEN_STRONG_INLINE Index cols() const { return m_storage.cols(); }

    EIGEN_STRONG_INLINE const Scalar& coeff(Index rowId, Index colId) const
    {
      if(Flags & RowMajorBit)
        return m_storage.data()[colId + rowId * m_storage.cols()];
      else 
        return m_storage.data()[rowId + colId * m_storage.rows()];
    }

    EIGEN_STRONG_INLINE const Scalar& coeff(Index index) const
    {
      return m_storage.data()[index];
    }

    EIGEN_STRONG_INLINE Scalar& coeffRef(Index rowId, Index colId)
    {
      if(Flags & RowMajorBit)
        return m_storage.data()[colId + rowId * m_storage.cols()];
      else 
        return m_storage.data()[rowId + colId * m_storage.rows()];
    }

    EIGEN_STRONG_INLINE Scalar& coeffRef(Index index)
    {
      return m_storage.data()[index];
    }

    EIGEN_STRONG_INLINE const Scalar& coeffRef(Index rowId, Index colId) const
    {
      if(Flags & RowMajorBit)
        return m_storage.data()[colId + rowId * m_storage.cols()];
      else 
        return m_storage.data()[rowId + colId * m_storage.rows()];
    }

    EIGEN_STRONG_INLINE const Scalar& coeffRef(Index index) const
    {
      return m_storage.data()[index];
    }

    
    template<int LoadMode>
    EIGEN_STRONG_INLINE PacketScalar packet(Index rowId, Index colId) const
    {
      return internal::ploadt<PacketScalar, LoadMode>
               (m_storage.data() + (Flags & RowMajorBit
                                   ? colId + rowId * m_storage.cols()
                                   : rowId + colId * m_storage.rows()));
    }

    
    template<int LoadMode>
    EIGEN_STRONG_INLINE PacketScalar packet(Index index) const
    {
      return internal::ploadt<PacketScalar, LoadMode>(m_storage.data() + index);
    }

    
    template<int StoreMode>
    EIGEN_STRONG_INLINE void writePacket(Index rowId, Index colId, const PacketScalar& val)
    {
      internal::pstoret<Scalar, PacketScalar, StoreMode>
              (m_storage.data() + (Flags & RowMajorBit
                                   ? colId + rowId * m_storage.cols()
                                   : rowId + colId * m_storage.rows()), val);
    }

    
    template<int StoreMode>
    EIGEN_STRONG_INLINE void writePacket(Index index, const PacketScalar& val)
    {
      internal::pstoret<Scalar, PacketScalar, StoreMode>(m_storage.data() + index, val);
    }

    
    EIGEN_STRONG_INLINE const Scalar *data() const
    { return m_storage.data(); }

    
    EIGEN_STRONG_INLINE Scalar *data()
    { return m_storage.data(); }

    EIGEN_STRONG_INLINE void resize(Index nbRows, Index nbCols)
    {
      eigen_assert(   EIGEN_IMPLIES(RowsAtCompileTime!=Dynamic,nbRows==RowsAtCompileTime)
                   && EIGEN_IMPLIES(ColsAtCompileTime!=Dynamic,nbCols==ColsAtCompileTime)
                   && EIGEN_IMPLIES(RowsAtCompileTime==Dynamic && MaxRowsAtCompileTime!=Dynamic,nbRows<=MaxRowsAtCompileTime)
                   && EIGEN_IMPLIES(ColsAtCompileTime==Dynamic && MaxColsAtCompileTime!=Dynamic,nbCols<=MaxColsAtCompileTime)
                   && nbRows>=0 && nbCols>=0 && "Invalid sizes when resizing a matrix or array.");
      internal::check_rows_cols_for_overflow<MaxSizeAtCompileTime>::run(nbRows, nbCols);
      #ifdef EIGEN_INITIALIZE_COEFFS
        Index size = nbRows*nbCols;
        bool size_changed = size != this->size();
        m_storage.resize(size, nbRows, nbCols);
        if(size_changed) EIGEN_INITIALIZE_COEFFS_IF_THAT_OPTION_IS_ENABLED
      #else
        internal::check_rows_cols_for_overflow<MaxSizeAtCompileTime>::run(nbRows, nbCols);
        m_storage.resize(nbRows*nbCols, nbRows, nbCols);
      #endif
    }

    inline void resize(Index size)
    {
      EIGEN_STATIC_ASSERT_VECTOR_ONLY(PlainObjectBase)
      eigen_assert(((SizeAtCompileTime == Dynamic && (MaxSizeAtCompileTime==Dynamic || size<=MaxSizeAtCompileTime)) || SizeAtCompileTime == size) && size>=0);
      #ifdef EIGEN_INITIALIZE_COEFFS
        bool size_changed = size != this->size();
      #endif
      if(RowsAtCompileTime == 1)
        m_storage.resize(size, 1, size);
      else
        m_storage.resize(size, size, 1);
      #ifdef EIGEN_INITIALIZE_COEFFS
        if(size_changed) EIGEN_INITIALIZE_COEFFS_IF_THAT_OPTION_IS_ENABLED
      #endif
    }

    inline void resize(NoChange_t, Index nbCols)
    {
      resize(rows(), nbCols);
    }

    inline void resize(Index nbRows, NoChange_t)
    {
      resize(nbRows, cols());
    }

    template<typename OtherDerived>
    EIGEN_STRONG_INLINE void resizeLike(const EigenBase<OtherDerived>& _other)
    {
      const OtherDerived& other = _other.derived();
      internal::check_rows_cols_for_overflow<MaxSizeAtCompileTime>::run(other.rows(), other.cols());
      const Index othersize = other.rows()*other.cols();
      if(RowsAtCompileTime == 1)
      {
        eigen_assert(other.rows() == 1 || other.cols() == 1);
        resize(1, othersize);
      }
      else if(ColsAtCompileTime == 1)
      {
        eigen_assert(other.rows() == 1 || other.cols() == 1);
        resize(othersize, 1);
      }
      else resize(other.rows(), other.cols());
    }

    EIGEN_STRONG_INLINE void conservativeResize(Index nbRows, Index nbCols)
    {
      internal::conservative_resize_like_impl<Derived>::run(*this, nbRows, nbCols);
    }

    EIGEN_STRONG_INLINE void conservativeResize(Index nbRows, NoChange_t)
    {
      
      conservativeResize(nbRows, cols());
    }

    EIGEN_STRONG_INLINE void conservativeResize(NoChange_t, Index nbCols)
    {
      
      conservativeResize(rows(), nbCols);
    }

    EIGEN_STRONG_INLINE void conservativeResize(Index size)
    {
      internal::conservative_resize_like_impl<Derived>::run(*this, size);
    }

    template<typename OtherDerived>
    EIGEN_STRONG_INLINE void conservativeResizeLike(const DenseBase<OtherDerived>& other)
    {
      internal::conservative_resize_like_impl<Derived,OtherDerived>::run(*this, other);
    }

    EIGEN_STRONG_INLINE Derived& operator=(const PlainObjectBase& other)
    {
      return _set(other);
    }

    
    template<typename OtherDerived>
    EIGEN_STRONG_INLINE Derived& lazyAssign(const DenseBase<OtherDerived>& other)
    {
      _resize_to_match(other);
      return Base::lazyAssign(other.derived());
    }

    template<typename OtherDerived>
    EIGEN_STRONG_INLINE Derived& operator=(const ReturnByValue<OtherDerived>& func)
    {
      resize(func.rows(), func.cols());
      return Base::operator=(func);
    }

    EIGEN_STRONG_INLINE PlainObjectBase() : m_storage()
    {
    }

#ifndef EIGEN_PARSED_BY_DOXYGEN
    
    
    PlainObjectBase(internal::constructor_without_unaligned_array_assert)
      : m_storage(internal::constructor_without_unaligned_array_assert())
    {
    }
#endif

    EIGEN_STRONG_INLINE PlainObjectBase(Index a_size, Index nbRows, Index nbCols)
      : m_storage(a_size, nbRows, nbCols)
    {
    }

    template<typename OtherDerived>
    EIGEN_STRONG_INLINE Derived& operator=(const EigenBase<OtherDerived> &other)
    {
      _resize_to_match(other);
      Base::operator=(other.derived());
      return this->derived();
    }

    
    template<typename OtherDerived>
    EIGEN_STRONG_INLINE PlainObjectBase(const EigenBase<OtherDerived> &other)
      : m_storage(other.derived().rows() * other.derived().cols(), other.derived().rows(), other.derived().cols())
    {
      _check_template_params();
      internal::check_rows_cols_for_overflow<MaxSizeAtCompileTime>::run(other.derived().rows(), other.derived().cols());
      Base::operator=(other.derived());
    }

    
    static inline ConstMapType Map(const Scalar* data)
    { return ConstMapType(data); }
    static inline MapType Map(Scalar* data)
    { return MapType(data); }
    static inline ConstMapType Map(const Scalar* data, Index size)
    { return ConstMapType(data, size); }
    static inline MapType Map(Scalar* data, Index size)
    { return MapType(data, size); }
    static inline ConstMapType Map(const Scalar* data, Index rows, Index cols)
    { return ConstMapType(data, rows, cols); }
    static inline MapType Map(Scalar* data, Index rows, Index cols)
    { return MapType(data, rows, cols); }

    static inline ConstAlignedMapType MapAligned(const Scalar* data)
    { return ConstAlignedMapType(data); }
    static inline AlignedMapType MapAligned(Scalar* data)
    { return AlignedMapType(data); }
    static inline ConstAlignedMapType MapAligned(const Scalar* data, Index size)
    { return ConstAlignedMapType(data, size); }
    static inline AlignedMapType MapAligned(Scalar* data, Index size)
    { return AlignedMapType(data, size); }
    static inline ConstAlignedMapType MapAligned(const Scalar* data, Index rows, Index cols)
    { return ConstAlignedMapType(data, rows, cols); }
    static inline AlignedMapType MapAligned(Scalar* data, Index rows, Index cols)
    { return AlignedMapType(data, rows, cols); }

    template<int Outer, int Inner>
    static inline typename StridedConstMapType<Stride<Outer, Inner> >::type Map(const Scalar* data, const Stride<Outer, Inner>& stride)
    { return typename StridedConstMapType<Stride<Outer, Inner> >::type(data, stride); }
    template<int Outer, int Inner>
    static inline typename StridedMapType<Stride<Outer, Inner> >::type Map(Scalar* data, const Stride<Outer, Inner>& stride)
    { return typename StridedMapType<Stride<Outer, Inner> >::type(data, stride); }
    template<int Outer, int Inner>
    static inline typename StridedConstMapType<Stride<Outer, Inner> >::type Map(const Scalar* data, Index size, const Stride<Outer, Inner>& stride)
    { return typename StridedConstMapType<Stride<Outer, Inner> >::type(data, size, stride); }
    template<int Outer, int Inner>
    static inline typename StridedMapType<Stride<Outer, Inner> >::type Map(Scalar* data, Index size, const Stride<Outer, Inner>& stride)
    { return typename StridedMapType<Stride<Outer, Inner> >::type(data, size, stride); }
    template<int Outer, int Inner>
    static inline typename StridedConstMapType<Stride<Outer, Inner> >::type Map(const Scalar* data, Index rows, Index cols, const Stride<Outer, Inner>& stride)
    { return typename StridedConstMapType<Stride<Outer, Inner> >::type(data, rows, cols, stride); }
    template<int Outer, int Inner>
    static inline typename StridedMapType<Stride<Outer, Inner> >::type Map(Scalar* data, Index rows, Index cols, const Stride<Outer, Inner>& stride)
    { return typename StridedMapType<Stride<Outer, Inner> >::type(data, rows, cols, stride); }

    template<int Outer, int Inner>
    static inline typename StridedConstAlignedMapType<Stride<Outer, Inner> >::type MapAligned(const Scalar* data, const Stride<Outer, Inner>& stride)
    { return typename StridedConstAlignedMapType<Stride<Outer, Inner> >::type(data, stride); }
    template<int Outer, int Inner>
    static inline typename StridedAlignedMapType<Stride<Outer, Inner> >::type MapAligned(Scalar* data, const Stride<Outer, Inner>& stride)
    { return typename StridedAlignedMapType<Stride<Outer, Inner> >::type(data, stride); }
    template<int Outer, int Inner>
    static inline typename StridedConstAlignedMapType<Stride<Outer, Inner> >::type MapAligned(const Scalar* data, Index size, const Stride<Outer, Inner>& stride)
    { return typename StridedConstAlignedMapType<Stride<Outer, Inner> >::type(data, size, stride); }
    template<int Outer, int Inner>
    static inline typename StridedAlignedMapType<Stride<Outer, Inner> >::type MapAligned(Scalar* data, Index size, const Stride<Outer, Inner>& stride)
    { return typename StridedAlignedMapType<Stride<Outer, Inner> >::type(data, size, stride); }
    template<int Outer, int Inner>
    static inline typename StridedConstAlignedMapType<Stride<Outer, Inner> >::type MapAligned(const Scalar* data, Index rows, Index cols, const Stride<Outer, Inner>& stride)
    { return typename StridedConstAlignedMapType<Stride<Outer, Inner> >::type(data, rows, cols, stride); }
    template<int Outer, int Inner>
    static inline typename StridedAlignedMapType<Stride<Outer, Inner> >::type MapAligned(Scalar* data, Index rows, Index cols, const Stride<Outer, Inner>& stride)
    { return typename StridedAlignedMapType<Stride<Outer, Inner> >::type(data, rows, cols, stride); }
    

    using Base::setConstant;
    Derived& setConstant(Index size, const Scalar& value);
    Derived& setConstant(Index rows, Index cols, const Scalar& value);

    using Base::setZero;
    Derived& setZero(Index size);
    Derived& setZero(Index rows, Index cols);

    using Base::setOnes;
    Derived& setOnes(Index size);
    Derived& setOnes(Index rows, Index cols);

    using Base::setRandom;
    Derived& setRandom(Index size);
    Derived& setRandom(Index rows, Index cols);

    #ifdef EIGEN_PLAINOBJECTBASE_PLUGIN
    #include EIGEN_PLAINOBJECTBASE_PLUGIN
    #endif

  protected:
    template<typename OtherDerived>
    EIGEN_STRONG_INLINE void _resize_to_match(const EigenBase<OtherDerived>& other)
    {
      #ifdef EIGEN_NO_AUTOMATIC_RESIZING
      eigen_assert((this->size()==0 || (IsVectorAtCompileTime ? (this->size() == other.size())
                 : (rows() == other.rows() && cols() == other.cols())))
        && "Size mismatch. Automatic resizing is disabled because EIGEN_NO_AUTOMATIC_RESIZING is defined");
      EIGEN_ONLY_USED_FOR_DEBUG(other);
      #else
      resizeLike(other);
      #endif
    }

    template<typename OtherDerived>
    EIGEN_STRONG_INLINE Derived& _set(const DenseBase<OtherDerived>& other)
    {
      _set_selector(other.derived(), typename internal::conditional<static_cast<bool>(int(OtherDerived::Flags) & EvalBeforeAssigningBit), internal::true_type, internal::false_type>::type());
      return this->derived();
    }

    template<typename OtherDerived>
    EIGEN_STRONG_INLINE void _set_selector(const OtherDerived& other, const internal::true_type&) { _set_noalias(other.eval()); }

    template<typename OtherDerived>
    EIGEN_STRONG_INLINE void _set_selector(const OtherDerived& other, const internal::false_type&) { _set_noalias(other); }

    template<typename OtherDerived>
    EIGEN_STRONG_INLINE Derived& _set_noalias(const DenseBase<OtherDerived>& other)
    {
      
      
      
      
      
      return internal::assign_selector<Derived,OtherDerived,false>::run(this->derived(), other.derived());
    }

    template<typename T0, typename T1>
    EIGEN_STRONG_INLINE void _init2(Index nbRows, Index nbCols, typename internal::enable_if<Base::SizeAtCompileTime!=2,T0>::type* = 0)
    {
      EIGEN_STATIC_ASSERT(bool(NumTraits<T0>::IsInteger) &&
                          bool(NumTraits<T1>::IsInteger),
                          FLOATING_POINT_ARGUMENT_PASSED__INTEGER_WAS_EXPECTED)
      resize(nbRows,nbCols);
    }
    template<typename T0, typename T1>
    EIGEN_STRONG_INLINE void _init2(const Scalar& val0, const Scalar& val1, typename internal::enable_if<Base::SizeAtCompileTime==2,T0>::type* = 0)
    {
      EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(PlainObjectBase, 2)
      m_storage.data()[0] = val0;
      m_storage.data()[1] = val1;
    }

    template<typename MatrixTypeA, typename MatrixTypeB, bool SwapPointers>
    friend struct internal::matrix_swap_impl;

    template<typename OtherDerived>
    void _swap(DenseBase<OtherDerived> const & other)
    {
      enum { SwapPointers = internal::is_same<Derived, OtherDerived>::value && Base::SizeAtCompileTime==Dynamic };
      internal::matrix_swap_impl<Derived, OtherDerived, bool(SwapPointers)>::run(this->derived(), other.const_cast_derived());
    }

  public:
#ifndef EIGEN_PARSED_BY_DOXYGEN
    static EIGEN_STRONG_INLINE void _check_template_params()
    {
      EIGEN_STATIC_ASSERT((EIGEN_IMPLIES(MaxRowsAtCompileTime==1 && MaxColsAtCompileTime!=1, (Options&RowMajor)==RowMajor)
                        && EIGEN_IMPLIES(MaxColsAtCompileTime==1 && MaxRowsAtCompileTime!=1, (Options&RowMajor)==0)
                        && ((RowsAtCompileTime == Dynamic) || (RowsAtCompileTime >= 0))
                        && ((ColsAtCompileTime == Dynamic) || (ColsAtCompileTime >= 0))
                        && ((MaxRowsAtCompileTime == Dynamic) || (MaxRowsAtCompileTime >= 0))
                        && ((MaxColsAtCompileTime == Dynamic) || (MaxColsAtCompileTime >= 0))
                        && (MaxRowsAtCompileTime == RowsAtCompileTime || RowsAtCompileTime==Dynamic)
                        && (MaxColsAtCompileTime == ColsAtCompileTime || ColsAtCompileTime==Dynamic)
                        && (Options & (DontAlign|RowMajor)) == Options),
        INVALID_MATRIX_TEMPLATE_PARAMETERS)
    }
#endif

private:
    enum { ThisConstantIsPrivateInPlainObjectBase };
};

namespace internal {

template <typename Derived, typename OtherDerived, bool IsVector>
struct conservative_resize_like_impl
{
  typedef typename Derived::Index Index;
  static void run(DenseBase<Derived>& _this, Index rows, Index cols)
  {
    if (_this.rows() == rows && _this.cols() == cols) return;
    EIGEN_STATIC_ASSERT_DYNAMIC_SIZE(Derived)

    if ( ( Derived::IsRowMajor && _this.cols() == cols) || 
         (!Derived::IsRowMajor && _this.rows() == rows) )  
    {
      internal::check_rows_cols_for_overflow<Derived::MaxSizeAtCompileTime>::run(rows, cols);
      _this.derived().m_storage.conservativeResize(rows*cols,rows,cols);
    }
    else
    {
      
      typename Derived::PlainObject tmp(rows,cols);
      const Index common_rows = (std::min)(rows, _this.rows());
      const Index common_cols = (std::min)(cols, _this.cols());
      tmp.block(0,0,common_rows,common_cols) = _this.block(0,0,common_rows,common_cols);
      _this.derived().swap(tmp);
    }
  }

  static void run(DenseBase<Derived>& _this, const DenseBase<OtherDerived>& other)
  {
    if (_this.rows() == other.rows() && _this.cols() == other.cols()) return;

    
    
    
    
    
    EIGEN_STATIC_ASSERT_DYNAMIC_SIZE(Derived)
    EIGEN_STATIC_ASSERT_DYNAMIC_SIZE(OtherDerived)

    if ( ( Derived::IsRowMajor && _this.cols() == other.cols()) || 
         (!Derived::IsRowMajor && _this.rows() == other.rows()) )  
    {
      const Index new_rows = other.rows() - _this.rows();
      const Index new_cols = other.cols() - _this.cols();
      _this.derived().m_storage.conservativeResize(other.size(),other.rows(),other.cols());
      if (new_rows>0)
        _this.bottomRightCorner(new_rows, other.cols()) = other.bottomRows(new_rows);
      else if (new_cols>0)
        _this.bottomRightCorner(other.rows(), new_cols) = other.rightCols(new_cols);
    }
    else
    {
      
      typename Derived::PlainObject tmp(other);
      const Index common_rows = (std::min)(tmp.rows(), _this.rows());
      const Index common_cols = (std::min)(tmp.cols(), _this.cols());
      tmp.block(0,0,common_rows,common_cols) = _this.block(0,0,common_rows,common_cols);
      _this.derived().swap(tmp);
    }
  }
};

template <typename Derived, typename OtherDerived>
struct conservative_resize_like_impl<Derived,OtherDerived,true>
  : conservative_resize_like_impl<Derived,OtherDerived,false>
{
  using conservative_resize_like_impl<Derived,OtherDerived,false>::run;
  
  typedef typename Derived::Index Index;
  static void run(DenseBase<Derived>& _this, Index size)
  {
    const Index new_rows = Derived::RowsAtCompileTime==1 ? 1 : size;
    const Index new_cols = Derived::RowsAtCompileTime==1 ? size : 1;
    _this.derived().m_storage.conservativeResize(size,new_rows,new_cols);
  }

  static void run(DenseBase<Derived>& _this, const DenseBase<OtherDerived>& other)
  {
    if (_this.rows() == other.rows() && _this.cols() == other.cols()) return;

    const Index num_new_elements = other.size() - _this.size();

    const Index new_rows = Derived::RowsAtCompileTime==1 ? 1 : other.rows();
    const Index new_cols = Derived::RowsAtCompileTime==1 ? other.cols() : 1;
    _this.derived().m_storage.conservativeResize(other.size(),new_rows,new_cols);

    if (num_new_elements > 0)
      _this.tail(num_new_elements) = other.tail(num_new_elements);
  }
};

template<typename MatrixTypeA, typename MatrixTypeB, bool SwapPointers>
struct matrix_swap_impl
{
  static inline void run(MatrixTypeA& a, MatrixTypeB& b)
  {
    a.base().swap(b);
  }
};

template<typename MatrixTypeA, typename MatrixTypeB>
struct matrix_swap_impl<MatrixTypeA, MatrixTypeB, true>
{
  static inline void run(MatrixTypeA& a, MatrixTypeB& b)
  {
    static_cast<typename MatrixTypeA::Base&>(a).m_storage.swap(static_cast<typename MatrixTypeB::Base&>(b).m_storage);
  }
};

} 

} 

#endif 
