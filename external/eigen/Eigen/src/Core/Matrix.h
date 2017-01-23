// Copyright (C) 2006-2010 Benoit Jacob <jacob.benoit.1@gmail.com>
// Copyright (C) 2008-2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_MATRIX_H
#define EIGEN_MATRIX_H

namespace Eigen {


namespace internal {
template<typename _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
struct traits<Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols> >
{
  typedef _Scalar Scalar;
  typedef Dense StorageKind;
  typedef DenseIndex Index;
  typedef MatrixXpr XprKind;
  enum {
    RowsAtCompileTime = _Rows,
    ColsAtCompileTime = _Cols,
    MaxRowsAtCompileTime = _MaxRows,
    MaxColsAtCompileTime = _MaxCols,
    Flags = compute_matrix_flags<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols>::ret,
    CoeffReadCost = NumTraits<Scalar>::ReadCost,
    Options = _Options,
    InnerStrideAtCompileTime = 1,
    OuterStrideAtCompileTime = (Options&RowMajor) ? ColsAtCompileTime : RowsAtCompileTime
  };
};
}

template<typename _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
class Matrix
  : public PlainObjectBase<Matrix<_Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols> >
{
  public:

    typedef PlainObjectBase<Matrix> Base;

    enum { Options = _Options };

    EIGEN_DENSE_PUBLIC_INTERFACE(Matrix)

    typedef typename Base::PlainObject PlainObject;

    using Base::base;
    using Base::coeffRef;

    EIGEN_STRONG_INLINE Matrix& operator=(const Matrix& other)
    {
      return Base::_set(other);
    }

    template<typename OtherDerived>
    EIGEN_STRONG_INLINE Matrix& operator=(const MatrixBase<OtherDerived>& other)
    {
      return Base::_set(other);
    }

    

    template<typename OtherDerived>
    EIGEN_STRONG_INLINE Matrix& operator=(const EigenBase<OtherDerived> &other)
    {
      return Base::operator=(other);
    }

    template<typename OtherDerived>
    EIGEN_STRONG_INLINE Matrix& operator=(const ReturnByValue<OtherDerived>& func)
    {
      return Base::operator=(func);
    }

    EIGEN_STRONG_INLINE Matrix() : Base()
    {
      Base::_check_template_params();
      EIGEN_INITIALIZE_COEFFS_IF_THAT_OPTION_IS_ENABLED
    }

    
    Matrix(internal::constructor_without_unaligned_array_assert)
      : Base(internal::constructor_without_unaligned_array_assert())
    { Base::_check_template_params(); EIGEN_INITIALIZE_COEFFS_IF_THAT_OPTION_IS_ENABLED }

    EIGEN_STRONG_INLINE explicit Matrix(Index dim)
      : Base(dim, RowsAtCompileTime == 1 ? 1 : dim, ColsAtCompileTime == 1 ? 1 : dim)
    {
      Base::_check_template_params();
      EIGEN_STATIC_ASSERT_VECTOR_ONLY(Matrix)
      eigen_assert(dim >= 0);
      eigen_assert(SizeAtCompileTime == Dynamic || SizeAtCompileTime == dim);
      EIGEN_INITIALIZE_COEFFS_IF_THAT_OPTION_IS_ENABLED
    }

    #ifndef EIGEN_PARSED_BY_DOXYGEN
    template<typename T0, typename T1>
    EIGEN_STRONG_INLINE Matrix(const T0& x, const T1& y)
    {
      Base::_check_template_params();
      Base::template _init2<T0,T1>(x, y);
    }
    #else
    Matrix(Index rows, Index cols);
    
    Matrix(const Scalar& x, const Scalar& y);
    #endif

    
    EIGEN_STRONG_INLINE Matrix(const Scalar& x, const Scalar& y, const Scalar& z)
    {
      Base::_check_template_params();
      EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Matrix, 3)
      m_storage.data()[0] = x;
      m_storage.data()[1] = y;
      m_storage.data()[2] = z;
    }
    
    EIGEN_STRONG_INLINE Matrix(const Scalar& x, const Scalar& y, const Scalar& z, const Scalar& w)
    {
      Base::_check_template_params();
      EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Matrix, 4)
      m_storage.data()[0] = x;
      m_storage.data()[1] = y;
      m_storage.data()[2] = z;
      m_storage.data()[3] = w;
    }

    explicit Matrix(const Scalar *data);

    
    template<typename OtherDerived>
    EIGEN_STRONG_INLINE Matrix(const MatrixBase<OtherDerived>& other)
             : Base(other.rows() * other.cols(), other.rows(), other.cols())
    {
      
      
      EIGEN_STATIC_ASSERT((internal::is_same<Scalar, typename OtherDerived::Scalar>::value),
        YOU_MIXED_DIFFERENT_NUMERIC_TYPES__YOU_NEED_TO_USE_THE_CAST_METHOD_OF_MATRIXBASE_TO_CAST_NUMERIC_TYPES_EXPLICITLY)

      Base::_check_template_params();
      Base::_set_noalias(other);
    }
    
    EIGEN_STRONG_INLINE Matrix(const Matrix& other)
            : Base(other.rows() * other.cols(), other.rows(), other.cols())
    {
      Base::_check_template_params();
      Base::_set_noalias(other);
    }
    
    template<typename OtherDerived>
    EIGEN_STRONG_INLINE Matrix(const ReturnByValue<OtherDerived>& other)
    {
      Base::_check_template_params();
      Base::resize(other.rows(), other.cols());
      other.evalTo(*this);
    }

    template<typename OtherDerived>
    EIGEN_STRONG_INLINE Matrix(const EigenBase<OtherDerived> &other)
      : Base(other.derived().rows() * other.derived().cols(), other.derived().rows(), other.derived().cols())
    {
      Base::_check_template_params();
      Base::_resize_to_match(other);
      
      
      *this = other;
    }

    template<typename OtherDerived>
    void swap(MatrixBase<OtherDerived> const & other)
    { this->_swap(other.derived()); }

    inline Index innerStride() const { return 1; }
    inline Index outerStride() const { return this->innerSize(); }

    

    template<typename OtherDerived>
    explicit Matrix(const RotationBase<OtherDerived,ColsAtCompileTime>& r);
    template<typename OtherDerived>
    Matrix& operator=(const RotationBase<OtherDerived,ColsAtCompileTime>& r);

    #ifdef EIGEN2_SUPPORT
    template<typename OtherDerived>
    explicit Matrix(const eigen2_RotationBase<OtherDerived,ColsAtCompileTime>& r);
    template<typename OtherDerived>
    Matrix& operator=(const eigen2_RotationBase<OtherDerived,ColsAtCompileTime>& r);
    #endif

    
    #ifdef EIGEN_MATRIX_PLUGIN
    #include EIGEN_MATRIX_PLUGIN
    #endif

  protected:
    template <typename Derived, typename OtherDerived, bool IsVector>
    friend struct internal::conservative_resize_like_impl;

    using Base::m_storage;
};


#define EIGEN_MAKE_TYPEDEFS(Type, TypeSuffix, Size, SizeSuffix)   \
                                    \
typedef Matrix<Type, Size, Size> Matrix##SizeSuffix##TypeSuffix;  \
                                    \
typedef Matrix<Type, Size, 1>    Vector##SizeSuffix##TypeSuffix;  \
                                    \
typedef Matrix<Type, 1, Size>    RowVector##SizeSuffix##TypeSuffix;

#define EIGEN_MAKE_FIXED_TYPEDEFS(Type, TypeSuffix, Size)         \
                                    \
typedef Matrix<Type, Size, Dynamic> Matrix##Size##X##TypeSuffix;  \
                                    \
typedef Matrix<Type, Dynamic, Size> Matrix##X##Size##TypeSuffix;

#define EIGEN_MAKE_TYPEDEFS_ALL_SIZES(Type, TypeSuffix) \
EIGEN_MAKE_TYPEDEFS(Type, TypeSuffix, 2, 2) \
EIGEN_MAKE_TYPEDEFS(Type, TypeSuffix, 3, 3) \
EIGEN_MAKE_TYPEDEFS(Type, TypeSuffix, 4, 4) \
EIGEN_MAKE_TYPEDEFS(Type, TypeSuffix, Dynamic, X) \
EIGEN_MAKE_FIXED_TYPEDEFS(Type, TypeSuffix, 2) \
EIGEN_MAKE_FIXED_TYPEDEFS(Type, TypeSuffix, 3) \
EIGEN_MAKE_FIXED_TYPEDEFS(Type, TypeSuffix, 4)

EIGEN_MAKE_TYPEDEFS_ALL_SIZES(int,                  i)
EIGEN_MAKE_TYPEDEFS_ALL_SIZES(float,                f)
EIGEN_MAKE_TYPEDEFS_ALL_SIZES(double,               d)
EIGEN_MAKE_TYPEDEFS_ALL_SIZES(std::complex<float>,  cf)
EIGEN_MAKE_TYPEDEFS_ALL_SIZES(std::complex<double>, cd)

#undef EIGEN_MAKE_TYPEDEFS_ALL_SIZES
#undef EIGEN_MAKE_TYPEDEFS
#undef EIGEN_MAKE_FIXED_TYPEDEFS

} 

#endif 
