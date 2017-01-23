// Copyright (C) 2006-2010 Benoit Jacob <jacob.benoit.1@gmail.com>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_NUMTRAITS_H
#define EIGEN_NUMTRAITS_H

namespace Eigen {


template<typename T> struct GenericNumTraits
{
  enum {
    IsInteger = std::numeric_limits<T>::is_integer,
    IsSigned = std::numeric_limits<T>::is_signed,
    IsComplex = 0,
    RequireInitialization = internal::is_arithmetic<T>::value ? 0 : 1,
    ReadCost = 1,
    AddCost = 1,
    MulCost = 1
  };

  typedef T Real;
  typedef typename internal::conditional<
                     IsInteger,
                     typename internal::conditional<sizeof(T)<=2, float, double>::type,
                     T
                   >::type NonInteger;
  typedef T Nested;

  static inline Real epsilon() { return std::numeric_limits<T>::epsilon(); }
  static inline Real dummy_precision()
  {
    
    return Real(0);
  }
  static inline T highest() { return (std::numeric_limits<T>::max)(); }
  static inline T lowest()  { return IsInteger ? (std::numeric_limits<T>::min)() : (-(std::numeric_limits<T>::max)()); }
  
#ifdef EIGEN2_SUPPORT
  enum {
    HasFloatingPoint = !IsInteger
  };
  typedef NonInteger FloatingPoint;
#endif
};

template<typename T> struct NumTraits : GenericNumTraits<T>
{};

template<> struct NumTraits<float>
  : GenericNumTraits<float>
{
  static inline float dummy_precision() { return 1e-5f; }
};

template<> struct NumTraits<double> : GenericNumTraits<double>
{
  static inline double dummy_precision() { return 1e-12; }
};

template<> struct NumTraits<long double>
  : GenericNumTraits<long double>
{
  static inline long double dummy_precision() { return 1e-15l; }
};

template<typename _Real> struct NumTraits<std::complex<_Real> >
  : GenericNumTraits<std::complex<_Real> >
{
  typedef _Real Real;
  enum {
    IsComplex = 1,
    RequireInitialization = NumTraits<_Real>::RequireInitialization,
    ReadCost = 2 * NumTraits<_Real>::ReadCost,
    AddCost = 2 * NumTraits<Real>::AddCost,
    MulCost = 4 * NumTraits<Real>::MulCost + 2 * NumTraits<Real>::AddCost
  };

  static inline Real epsilon() { return NumTraits<Real>::epsilon(); }
  static inline Real dummy_precision() { return NumTraits<Real>::dummy_precision(); }
};

template<typename Scalar, int Rows, int Cols, int Options, int MaxRows, int MaxCols>
struct NumTraits<Array<Scalar, Rows, Cols, Options, MaxRows, MaxCols> >
{
  typedef Array<Scalar, Rows, Cols, Options, MaxRows, MaxCols> ArrayType;
  typedef typename NumTraits<Scalar>::Real RealScalar;
  typedef Array<RealScalar, Rows, Cols, Options, MaxRows, MaxCols> Real;
  typedef typename NumTraits<Scalar>::NonInteger NonIntegerScalar;
  typedef Array<NonIntegerScalar, Rows, Cols, Options, MaxRows, MaxCols> NonInteger;
  typedef ArrayType & Nested;
  
  enum {
    IsComplex = NumTraits<Scalar>::IsComplex,
    IsInteger = NumTraits<Scalar>::IsInteger,
    IsSigned  = NumTraits<Scalar>::IsSigned,
    RequireInitialization = 1,
    ReadCost = ArrayType::SizeAtCompileTime==Dynamic ? Dynamic : ArrayType::SizeAtCompileTime * NumTraits<Scalar>::ReadCost,
    AddCost  = ArrayType::SizeAtCompileTime==Dynamic ? Dynamic : ArrayType::SizeAtCompileTime * NumTraits<Scalar>::AddCost,
    MulCost  = ArrayType::SizeAtCompileTime==Dynamic ? Dynamic : ArrayType::SizeAtCompileTime * NumTraits<Scalar>::MulCost
  };
  
  static inline RealScalar epsilon() { return NumTraits<RealScalar>::epsilon(); }
  static inline RealScalar dummy_precision() { return NumTraits<RealScalar>::dummy_precision(); }
};

} 

#endif 
