// Copyright (C) 2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_ARRAY_H
#define EIGEN_ARRAY_H

namespace Eigen {

namespace internal {
template<typename _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
struct traits<Array<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols> > : traits<Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols> >
{
  typedef ArrayXpr XprKind;
  typedef ArrayBase<Array<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols> > XprBase;
};
}

template<typename _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
class Array
  : public PlainObjectBase<Array<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols> >
{
  public:

    typedef PlainObjectBase<Array> Base;
    EIGEN_DENSE_PUBLIC_INTERFACE(Array)

    enum { Options = _Options };
    typedef typename Base::PlainObject PlainObject;

  protected:
    template <typename Derived, typename OtherDerived, bool IsVector>
    friend struct internal::conservative_resize_like_impl;

    using Base::m_storage;

  public:

    using Base::base;
    using Base::coeff;
    using Base::coeffRef;

    template<typename OtherDerived>
    EIGEN_STRONG_INLINE Array& operator=(const EigenBase<OtherDerived> &other)
    {
      return Base::operator=(other);
    }

    template<typename OtherDerived>
    EIGEN_STRONG_INLINE Array& operator=(const ArrayBase<OtherDerived>& other)
    {
      return Base::_set(other);
    }

    EIGEN_STRONG_INLINE Array& operator=(const Array& other)
    {
      return Base::_set(other);
    }

    EIGEN_STRONG_INLINE Array() : Base()
    {
      Base::_check_template_params();
      EIGEN_INITIALIZE_COEFFS_IF_THAT_OPTION_IS_ENABLED
    }

#ifndef EIGEN_PARSED_BY_DOXYGEN
    
    
    Array(internal::constructor_without_unaligned_array_assert)
      : Base(internal::constructor_without_unaligned_array_assert())
    {
      Base::_check_template_params();
      EIGEN_INITIALIZE_COEFFS_IF_THAT_OPTION_IS_ENABLED
    }
#endif

    EIGEN_STRONG_INLINE explicit Array(Index dim)
      : Base(dim, RowsAtCompileTime == 1 ? 1 : dim, ColsAtCompileTime == 1 ? 1 : dim)
    {
      Base::_check_template_params();
      EIGEN_STATIC_ASSERT_VECTOR_ONLY(Array)
      eigen_assert(dim >= 0);
      eigen_assert(SizeAtCompileTime == Dynamic || SizeAtCompileTime == dim);
      EIGEN_INITIALIZE_COEFFS_IF_THAT_OPTION_IS_ENABLED
    }

    #ifndef EIGEN_PARSED_BY_DOXYGEN
    template<typename T0, typename T1>
    EIGEN_STRONG_INLINE Array(const T0& val0, const T1& val1)
    {
      Base::_check_template_params();
      this->template _init2<T0,T1>(val0, val1);
    }
    #else
    Array(Index rows, Index cols);
    
    Array(const Scalar& val0, const Scalar& val1);
    #endif

    
    EIGEN_STRONG_INLINE Array(const Scalar& val0, const Scalar& val1, const Scalar& val2)
    {
      Base::_check_template_params();
      EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Array, 3)
      m_storage.data()[0] = val0;
      m_storage.data()[1] = val1;
      m_storage.data()[2] = val2;
    }
    
    EIGEN_STRONG_INLINE Array(const Scalar& val0, const Scalar& val1, const Scalar& val2, const Scalar& val3)
    {
      Base::_check_template_params();
      EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Array, 4)
      m_storage.data()[0] = val0;
      m_storage.data()[1] = val1;
      m_storage.data()[2] = val2;
      m_storage.data()[3] = val3;
    }

    explicit Array(const Scalar *data);

    
    template<typename OtherDerived>
    EIGEN_STRONG_INLINE Array(const ArrayBase<OtherDerived>& other)
             : Base(other.rows() * other.cols(), other.rows(), other.cols())
    {
      Base::_check_template_params();
      Base::_set_noalias(other);
    }
    
    EIGEN_STRONG_INLINE Array(const Array& other)
            : Base(other.rows() * other.cols(), other.rows(), other.cols())
    {
      Base::_check_template_params();
      Base::_set_noalias(other);
    }
    
    template<typename OtherDerived>
    EIGEN_STRONG_INLINE Array(const ReturnByValue<OtherDerived>& other)
    {
      Base::_check_template_params();
      Base::resize(other.rows(), other.cols());
      other.evalTo(*this);
    }

    
    template<typename OtherDerived>
    EIGEN_STRONG_INLINE Array(const EigenBase<OtherDerived> &other)
      : Base(other.derived().rows() * other.derived().cols(), other.derived().rows(), other.derived().cols())
    {
      Base::_check_template_params();
      Base::_resize_to_match(other);
      *this = other;
    }

    template<typename OtherDerived>
    void swap(ArrayBase<OtherDerived> const & other)
    { this->_swap(other.derived()); }

    inline Index innerStride() const { return 1; }
    inline Index outerStride() const { return this->innerSize(); }

    #ifdef EIGEN_ARRAY_PLUGIN
    #include EIGEN_ARRAY_PLUGIN
    #endif

  private:

    template<typename MatrixType, typename OtherDerived, bool SwapPointers>
    friend struct internal::matrix_swap_impl;
};


#define EIGEN_MAKE_ARRAY_TYPEDEFS(Type, TypeSuffix, Size, SizeSuffix)   \
                                    \
typedef Array<Type, Size, Size> Array##SizeSuffix##SizeSuffix##TypeSuffix;  \
                                    \
typedef Array<Type, Size, 1>    Array##SizeSuffix##TypeSuffix;

#define EIGEN_MAKE_ARRAY_FIXED_TYPEDEFS(Type, TypeSuffix, Size)         \
                                    \
typedef Array<Type, Size, Dynamic> Array##Size##X##TypeSuffix;  \
                                    \
typedef Array<Type, Dynamic, Size> Array##X##Size##TypeSuffix;

#define EIGEN_MAKE_ARRAY_TYPEDEFS_ALL_SIZES(Type, TypeSuffix) \
EIGEN_MAKE_ARRAY_TYPEDEFS(Type, TypeSuffix, 2, 2) \
EIGEN_MAKE_ARRAY_TYPEDEFS(Type, TypeSuffix, 3, 3) \
EIGEN_MAKE_ARRAY_TYPEDEFS(Type, TypeSuffix, 4, 4) \
EIGEN_MAKE_ARRAY_TYPEDEFS(Type, TypeSuffix, Dynamic, X) \
EIGEN_MAKE_ARRAY_FIXED_TYPEDEFS(Type, TypeSuffix, 2) \
EIGEN_MAKE_ARRAY_FIXED_TYPEDEFS(Type, TypeSuffix, 3) \
EIGEN_MAKE_ARRAY_FIXED_TYPEDEFS(Type, TypeSuffix, 4)

EIGEN_MAKE_ARRAY_TYPEDEFS_ALL_SIZES(int,                  i)
EIGEN_MAKE_ARRAY_TYPEDEFS_ALL_SIZES(float,                f)
EIGEN_MAKE_ARRAY_TYPEDEFS_ALL_SIZES(double,               d)
EIGEN_MAKE_ARRAY_TYPEDEFS_ALL_SIZES(std::complex<float>,  cf)
EIGEN_MAKE_ARRAY_TYPEDEFS_ALL_SIZES(std::complex<double>, cd)

#undef EIGEN_MAKE_ARRAY_TYPEDEFS_ALL_SIZES
#undef EIGEN_MAKE_ARRAY_TYPEDEFS

#undef EIGEN_MAKE_ARRAY_TYPEDEFS_LARGE

#define EIGEN_USING_ARRAY_TYPEDEFS_FOR_TYPE_AND_SIZE(TypeSuffix, SizeSuffix) \
using Eigen::Matrix##SizeSuffix##TypeSuffix; \
using Eigen::Vector##SizeSuffix##TypeSuffix; \
using Eigen::RowVector##SizeSuffix##TypeSuffix;

#define EIGEN_USING_ARRAY_TYPEDEFS_FOR_TYPE(TypeSuffix) \
EIGEN_USING_ARRAY_TYPEDEFS_FOR_TYPE_AND_SIZE(TypeSuffix, 2) \
EIGEN_USING_ARRAY_TYPEDEFS_FOR_TYPE_AND_SIZE(TypeSuffix, 3) \
EIGEN_USING_ARRAY_TYPEDEFS_FOR_TYPE_AND_SIZE(TypeSuffix, 4) \
EIGEN_USING_ARRAY_TYPEDEFS_FOR_TYPE_AND_SIZE(TypeSuffix, X) \

#define EIGEN_USING_ARRAY_TYPEDEFS \
EIGEN_USING_ARRAY_TYPEDEFS_FOR_TYPE(i) \
EIGEN_USING_ARRAY_TYPEDEFS_FOR_TYPE(f) \
EIGEN_USING_ARRAY_TYPEDEFS_FOR_TYPE(d) \
EIGEN_USING_ARRAY_TYPEDEFS_FOR_TYPE(cf) \
EIGEN_USING_ARRAY_TYPEDEFS_FOR_TYPE(cd)

} 

#endif 
