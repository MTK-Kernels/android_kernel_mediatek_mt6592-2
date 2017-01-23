// Copyright (C) 2008-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_MACROS_H
#define EIGEN_MACROS_H

#define EIGEN_WORLD_VERSION 3
#define EIGEN_MAJOR_VERSION 2
#define EIGEN_MINOR_VERSION 4

#define EIGEN_VERSION_AT_LEAST(x,y,z) (EIGEN_WORLD_VERSION>x || (EIGEN_WORLD_VERSION>=x && \
                                      (EIGEN_MAJOR_VERSION>y || (EIGEN_MAJOR_VERSION>=y && \
                                                                 EIGEN_MINOR_VERSION>=z))))
#ifdef __GNUC__
  #define EIGEN_GNUC_AT_LEAST(x,y) ((__GNUC__==x && __GNUC_MINOR__>=y) || __GNUC__>x)
#else
  #define EIGEN_GNUC_AT_LEAST(x,y) 0
#endif
 
#ifdef __GNUC__
  #define EIGEN_GNUC_AT_MOST(x,y) ((__GNUC__==x && __GNUC_MINOR__<=y) || __GNUC__<x)
#else
  #define EIGEN_GNUC_AT_MOST(x,y) 0
#endif

#if EIGEN_GNUC_AT_MOST(4,3) && !defined(__clang__)
  
  #define EIGEN_SAFE_TO_USE_STANDARD_ASSERT_MACRO 0
#else
  #define EIGEN_SAFE_TO_USE_STANDARD_ASSERT_MACRO 1
#endif

#if defined(__GNUC__) && (__GNUC__ <= 3)
#define EIGEN_GCC3_OR_OLDER 1
#else
#define EIGEN_GCC3_OR_OLDER 0
#endif

#if defined(__GNUC__) && !(defined(__i386__) || defined(__x86_64__) || defined(__powerpc__) || defined(__ppc__) || defined(__ia64__))
#define EIGEN_GCC_AND_ARCH_DOESNT_WANT_STACK_ALIGNMENT 1
#else
#define EIGEN_GCC_AND_ARCH_DOESNT_WANT_STACK_ALIGNMENT 0
#endif

#if !EIGEN_GCC_AND_ARCH_DOESNT_WANT_STACK_ALIGNMENT \
 && !EIGEN_GCC3_OR_OLDER \
 && !defined(__SUNPRO_CC) \
 && !defined(__QNXNTO__)
  #define EIGEN_ARCH_WANTS_STACK_ALIGNMENT 1
#else
  #define EIGEN_ARCH_WANTS_STACK_ALIGNMENT 0
#endif

#ifdef EIGEN_DONT_ALIGN
  #ifndef EIGEN_DONT_ALIGN_STATICALLY
    #define EIGEN_DONT_ALIGN_STATICALLY
  #endif
  #define EIGEN_ALIGN 0
#else
  #define EIGEN_ALIGN 1
#endif

#if EIGEN_ARCH_WANTS_STACK_ALIGNMENT && !defined(EIGEN_DONT_ALIGN_STATICALLY)
  #define EIGEN_ALIGN_STATICALLY 1
#else
  #define EIGEN_ALIGN_STATICALLY 0
  #ifndef EIGEN_DISABLE_UNALIGNED_ARRAY_ASSERT
    #define EIGEN_DISABLE_UNALIGNED_ARRAY_ASSERT
  #endif
#endif

#ifdef EIGEN_DEFAULT_TO_ROW_MAJOR
#define EIGEN_DEFAULT_MATRIX_STORAGE_ORDER_OPTION RowMajor
#else
#define EIGEN_DEFAULT_MATRIX_STORAGE_ORDER_OPTION ColMajor
#endif

#ifndef EIGEN_DEFAULT_DENSE_INDEX_TYPE
#define EIGEN_DEFAULT_DENSE_INDEX_TYPE std::ptrdiff_t
#endif

#ifdef __has_builtin
#  define EIGEN_HAS_BUILTIN(x) __has_builtin(x)
#else
#  define EIGEN_HAS_BUILTIN(x) 0
#endif

#ifndef EIGEN_FAST_MATH
#define EIGEN_FAST_MATH 1
#endif

#define EIGEN_DEBUG_VAR(x) std::cerr << #x << " = " << x << std::endl;

#define EIGEN_CAT2(a,b) a ## b
#define EIGEN_CAT(a,b) EIGEN_CAT2(a,b)

#define EIGEN_MAKESTRING2(a) #a
#define EIGEN_MAKESTRING(a) EIGEN_MAKESTRING2(a)

#if (defined _MSC_VER) || (defined __INTEL_COMPILER)
#define EIGEN_STRONG_INLINE __forceinline
#else
#define EIGEN_STRONG_INLINE inline
#endif

#if EIGEN_GNUC_AT_LEAST(4,0)
#define EIGEN_ALWAYS_INLINE __attribute__((always_inline)) inline
#else
#define EIGEN_ALWAYS_INLINE EIGEN_STRONG_INLINE
#endif

#if (defined __GNUC__)
#define EIGEN_DONT_INLINE __attribute__((noinline))
#elif (defined _MSC_VER)
#define EIGEN_DONT_INLINE __declspec(noinline)
#else
#define EIGEN_DONT_INLINE
#endif

#if (defined __GNUC__)
#define EIGEN_PERMISSIVE_EXPR __extension__
#else
#define EIGEN_PERMISSIVE_EXPR
#endif

#define EIGEN_DECLARE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS
#define EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS inline

#ifdef NDEBUG
# ifndef EIGEN_NO_DEBUG
#  define EIGEN_NO_DEBUG
# endif
#endif

#ifdef EIGEN_NO_DEBUG
  #define eigen_plain_assert(x)
#else
  #if EIGEN_SAFE_TO_USE_STANDARD_ASSERT_MACRO
    namespace Eigen {
    namespace internal {
    inline bool copy_bool(bool b) { return b; }
    }
    }
    #define eigen_plain_assert(x) assert(x)
  #else
    
    #include <cstdlib>   
    #include <iostream>  

    namespace Eigen {
    namespace internal {
    
    
    namespace {
    EIGEN_DONT_INLINE bool copy_bool(bool b) { return b; }
    }
    inline void assert_fail(const char *condition, const char *function, const char *file, int line)
    {
      std::cerr << "assertion failed: " << condition << " in function " << function << " at " << file << ":" << line << std::endl;
      abort();
    }
    }
    }
    #define eigen_plain_assert(x) \
      do { \
        if(!Eigen::internal::copy_bool(x)) \
          Eigen::internal::assert_fail(EIGEN_MAKESTRING(x), __PRETTY_FUNCTION__, __FILE__, __LINE__); \
      } while(false)
  #endif
#endif

#ifndef eigen_assert
#define eigen_assert(x) eigen_plain_assert(x)
#endif

#ifdef EIGEN_INTERNAL_DEBUGGING
#define eigen_internal_assert(x) eigen_assert(x)
#else
#define eigen_internal_assert(x)
#endif

#ifdef EIGEN_NO_DEBUG
#define EIGEN_ONLY_USED_FOR_DEBUG(x) (void)x
#else
#define EIGEN_ONLY_USED_FOR_DEBUG(x)
#endif

#ifndef EIGEN_NO_DEPRECATED_WARNING
  #if (defined __GNUC__)
    #define EIGEN_DEPRECATED __attribute__((deprecated))
  #elif (defined _MSC_VER)
    #define EIGEN_DEPRECATED __declspec(deprecated)
  #else
    #define EIGEN_DEPRECATED
  #endif
#else
  #define EIGEN_DEPRECATED
#endif

#if (defined __GNUC__)
#define EIGEN_UNUSED __attribute__((unused))
#else
#define EIGEN_UNUSED
#endif

namespace Eigen {
  namespace internal {
    template<typename T> void ignore_unused_variable(const T&) {}
  }
}
#define EIGEN_UNUSED_VARIABLE(var) Eigen::internal::ignore_unused_variable(var);

#if !defined(EIGEN_ASM_COMMENT)
  #if (defined __GNUC__) && ( defined(__i386__) || defined(__x86_64__) )
    #define EIGEN_ASM_COMMENT(X)  __asm__("#" X)
  #else
    #define EIGEN_ASM_COMMENT(X)
  #endif
#endif

#if (defined __GNUC__) || (defined __PGI) || (defined __IBMCPP__) || (defined __ARMCC_VERSION)
  #define EIGEN_ALIGN_TO_BOUNDARY(n) __attribute__((aligned(n)))
#elif (defined _MSC_VER)
  #define EIGEN_ALIGN_TO_BOUNDARY(n) __declspec(align(n))
#elif (defined __SUNPRO_CC)
  
  #define EIGEN_ALIGN_TO_BOUNDARY(n) __attribute__((aligned(n)))
#else
  #error Please tell me what is the equivalent of __attribute__((aligned(n))) for your compiler
#endif

#define EIGEN_ALIGN16 EIGEN_ALIGN_TO_BOUNDARY(16)

#if EIGEN_ALIGN_STATICALLY
#define EIGEN_USER_ALIGN_TO_BOUNDARY(n) EIGEN_ALIGN_TO_BOUNDARY(n)
#define EIGEN_USER_ALIGN16 EIGEN_ALIGN16
#else
#define EIGEN_USER_ALIGN_TO_BOUNDARY(n)
#define EIGEN_USER_ALIGN16
#endif

#ifdef EIGEN_DONT_USE_RESTRICT_KEYWORD
  #define EIGEN_RESTRICT
#endif
#ifndef EIGEN_RESTRICT
  #define EIGEN_RESTRICT __restrict
#endif

#ifndef EIGEN_STACK_ALLOCATION_LIMIT
#define EIGEN_STACK_ALLOCATION_LIMIT 131072
#endif

#ifndef EIGEN_DEFAULT_IO_FORMAT
#ifdef EIGEN_MAKING_DOCS
#define EIGEN_DEFAULT_IO_FORMAT Eigen::IOFormat(3, 0, " ", "\n", "", "")
#else
#define EIGEN_DEFAULT_IO_FORMAT Eigen::IOFormat()
#endif
#endif

#define EIGEN_EMPTY

#if defined(_MSC_VER) && (_MSC_VER < 1900) && (!defined(__INTEL_COMPILER))
#define EIGEN_INHERIT_ASSIGNMENT_EQUAL_OPERATOR(Derived) \
  using Base::operator =;
#elif defined(__clang__) 
#define EIGEN_INHERIT_ASSIGNMENT_EQUAL_OPERATOR(Derived) \
  using Base::operator =; \
  EIGEN_STRONG_INLINE Derived& operator=(const Derived& other) { Base::operator=(other); return *this; } \
  template <typename OtherDerived> \
  EIGEN_STRONG_INLINE Derived& operator=(const DenseBase<OtherDerived>& other) { Base::operator=(other.derived()); return *this; }
#else
#define EIGEN_INHERIT_ASSIGNMENT_EQUAL_OPERATOR(Derived) \
  using Base::operator =; \
  EIGEN_STRONG_INLINE Derived& operator=(const Derived& other) \
  { \
    Base::operator=(other); \
    return *this; \
  }
#endif

#define EIGEN_INHERIT_ASSIGNMENT_OPERATORS(Derived) \
  EIGEN_INHERIT_ASSIGNMENT_EQUAL_OPERATOR(Derived)


#define EIGEN_GENERIC_PUBLIC_INTERFACE(Derived) \
  typedef typename Eigen::internal::traits<Derived>::Scalar Scalar;  \
  typedef typename Eigen::NumTraits<Scalar>::Real RealScalar;  \
  typedef typename Base::CoeffReturnType CoeffReturnType;  \
  typedef typename Eigen::internal::nested<Derived>::type Nested; \
  typedef typename Eigen::internal::traits<Derived>::StorageKind StorageKind; \
  typedef typename Eigen::internal::traits<Derived>::Index Index; \
  enum { RowsAtCompileTime = Eigen::internal::traits<Derived>::RowsAtCompileTime, \
        ColsAtCompileTime = Eigen::internal::traits<Derived>::ColsAtCompileTime, \
        Flags = Eigen::internal::traits<Derived>::Flags, \
        CoeffReadCost = Eigen::internal::traits<Derived>::CoeffReadCost, \
        SizeAtCompileTime = Base::SizeAtCompileTime, \
        MaxSizeAtCompileTime = Base::MaxSizeAtCompileTime, \
        IsVectorAtCompileTime = Base::IsVectorAtCompileTime };


#define EIGEN_DENSE_PUBLIC_INTERFACE(Derived) \
  typedef typename Eigen::internal::traits<Derived>::Scalar Scalar;  \
  typedef typename Eigen::NumTraits<Scalar>::Real RealScalar;  \
  typedef typename Base::PacketScalar PacketScalar; \
  typedef typename Base::CoeffReturnType CoeffReturnType;  \
  typedef typename Eigen::internal::nested<Derived>::type Nested; \
  typedef typename Eigen::internal::traits<Derived>::StorageKind StorageKind; \
  typedef typename Eigen::internal::traits<Derived>::Index Index; \
  enum { RowsAtCompileTime = Eigen::internal::traits<Derived>::RowsAtCompileTime, \
        ColsAtCompileTime = Eigen::internal::traits<Derived>::ColsAtCompileTime, \
        MaxRowsAtCompileTime = Eigen::internal::traits<Derived>::MaxRowsAtCompileTime, \
        MaxColsAtCompileTime = Eigen::internal::traits<Derived>::MaxColsAtCompileTime, \
        Flags = Eigen::internal::traits<Derived>::Flags, \
        CoeffReadCost = Eigen::internal::traits<Derived>::CoeffReadCost, \
        SizeAtCompileTime = Base::SizeAtCompileTime, \
        MaxSizeAtCompileTime = Base::MaxSizeAtCompileTime, \
        IsVectorAtCompileTime = Base::IsVectorAtCompileTime }; \
  using Base::derived; \
  using Base::const_cast_derived;


#define EIGEN_PLAIN_ENUM_MIN(a,b) (((int)a <= (int)b) ? (int)a : (int)b)
#define EIGEN_PLAIN_ENUM_MAX(a,b) (((int)a >= (int)b) ? (int)a : (int)b)

#define EIGEN_SIZE_MIN_PREFER_DYNAMIC(a,b) (((int)a == 0 || (int)b == 0) ? 0 \
                           : ((int)a == 1 || (int)b == 1) ? 1 \
                           : ((int)a == Dynamic || (int)b == Dynamic) ? Dynamic \
                           : ((int)a <= (int)b) ? (int)a : (int)b)

#define EIGEN_SIZE_MIN_PREFER_FIXED(a,b)  (((int)a == 0 || (int)b == 0) ? 0 \
                           : ((int)a == 1 || (int)b == 1) ? 1 \
                           : ((int)a == Dynamic && (int)b == Dynamic) ? Dynamic \
                           : ((int)a == Dynamic) ? (int)b \
                           : ((int)b == Dynamic) ? (int)a \
                           : ((int)a <= (int)b) ? (int)a : (int)b)

#define EIGEN_SIZE_MAX(a,b) (((int)a == Dynamic || (int)b == Dynamic) ? Dynamic \
                           : ((int)a >= (int)b) ? (int)a : (int)b)

#define EIGEN_LOGICAL_XOR(a,b) (((a) || (b)) && !((a) && (b)))

#define EIGEN_IMPLIES(a,b) (!(a) || (b))

#define EIGEN_MAKE_CWISE_BINARY_OP(METHOD,FUNCTOR) \
  template<typename OtherDerived> \
  EIGEN_STRONG_INLINE const CwiseBinaryOp<FUNCTOR<Scalar>, const Derived, const OtherDerived> \
  (METHOD)(const EIGEN_CURRENT_STORAGE_BASE_CLASS<OtherDerived> &other) const \
  { \
    return CwiseBinaryOp<FUNCTOR<Scalar>, const Derived, const OtherDerived>(derived(), other.derived()); \
  }

#define EIGEN_CWISE_PRODUCT_RETURN_TYPE(LHS,RHS) \
    CwiseBinaryOp< \
      internal::scalar_product_op< \
          typename internal::traits<LHS>::Scalar, \
          typename internal::traits<RHS>::Scalar \
      >, \
      const LHS, \
      const RHS \
    >

#endif 
